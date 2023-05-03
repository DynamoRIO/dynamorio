/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
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

/*
 * vmareas.c - virtual memory executable areas
 */

#include "globals.h"

/* all of this for selfmod handling */
#include "fragment.h"
#include "instr.h"
#include "decode.h"
#include "decode_fast.h"

#include "link.h"
#include "disassemble.h"
#include "fcache.h"
#include "hotpatch.h"
#include "moduledb.h"
#include "module_shared.h"
#include "perscache.h"
#include "translate.h"
#include "jit_opt.h"

#ifdef WINDOWS
#    include "events.h" /* event log messages - not supported yet on Linux  */
#endif

#include "instrument.h"

#ifdef DEBUG
#    include "synch.h" /* all_threads_synch_lock */
#endif

enum {
    /* VM_ flags to distinguish region types
     * We also use some FRAG_ flags (but in a separate field so no value space overlap)
     * Adjacent regions w/ different flags are never merged.
     */
    VM_WRITABLE = 0x0001, /* app memory writable? */
    /* UNMOD_IMAGE means the region was mmapped in and has been read-only since then
     * this excludes even loader modifications (IAT update, relocate, etc.) on win32!
     */
    VM_UNMOD_IMAGE = 0x0002,
    VM_DELETE_ME = 0x0004, /* on delete queue -- for thread-local only */
    /* NOTE : if a new area is added that overlaps an existing area with a
     * different VM_WAS_FUTURE flag, the areas will be merged with the flag
     * taken from the new area, see FIXME in add_vm_area */
    VM_WAS_FUTURE = 0x0008, /* moved from future list to exec list */
    VM_DR_HEAP = 0x0010,    /* DR heap area */
    VM_ONCE_ONLY = 0x0020,  /* on future list but should be removed on
                             * first exec */
    /* FIXME case 7877, 3744: need to properly merge pageprot regions with
     * existing selfmod regions before we can truly separate this.  For now we
     * continue to treat selfmod as pageprot.
     * Once we separate, we should update DR_MADE_READONLY.
     */
    VM_MADE_READONLY = VM_WRITABLE /* FIXME: should be 0x0040 -- see above */,
    /* DR has marked this region read
     * only for consistency, should only be used
     * in conjunction with VM_WRITABLE */
    VM_DELAY_READONLY = 0x0080, /* dr has not yet marked this region read
                                 * only for consistency, should only be used
                                 * in conjunction with VM_WRITABLE */
#ifdef PROGRAM_SHEPHERDING
    /* re-verify this region for code origins policies every time it is
     * encountered.  only used with selfmod regions that are only allowed if
     * they match patterns, to prevent other threads from writing non-pattern
     * code though and executing after the region has been approved.
     * xref case 4020.  can remove once we split code origins list from
     * cache consistency list (case 3744).
     */
    VM_PATTERN_REVERIFY = 0x0100,
#endif

    VM_DRIVER_ADDRESS = 0x0200,
    /* a driver hooker area, needed for case 9022.  Note we can
     * normally read properties only of user mode addresses, so we
     * have to probe addresses in this area.  Also note that we're
     * still executing all of this code in user mode e.g. there is no
     * mode switch, no conforming segments, etc.
     */

    /* Does this region contain a persisted cache?
     * Must also be FRAG_COARSE_GRAIN of course.
     * This is a shortcut to reading custom.client->persisted.
     * This is not guaranteed to be set on shared_data: only on executable_areas.
     */
    VM_PERSISTED_CACHE = 0x0400,

    /* Case 10584: avoid flush synch when no code has been executed */
    VM_EXECUTED_FROM = 0x0800,

    /* A workaround for lock rank issues: we delay adding loaded persisted
     * units to shared_data until first asked about.
     * This flags is NOT propagated on vmarea splits.
     */
    VM_ADD_TO_SHARED_DATA = 0x1000,

    /* i#1114: for areas containing JIT code flushed via annotation or inference */
    VM_JIT_MANAGED = 0x2000,
};

/* simple way to disable sandboxing */
#define SANDBOX_FLAG() \
    (INTERNAL_OPTION(hw_cache_consistency) ? FRAG_SELFMOD_SANDBOXED : 0)

/* Because VM_MADE_READONLY == VM_WRITABLE it's not sufficient on its own */
#define DR_MADE_READONLY(flags) \
    (INTERNAL_OPTION(hw_cache_consistency) && TEST(VM_MADE_READONLY, flags))

/* Fields only used for written_areas */
typedef struct _ro_vs_sandbox_data_t {
    /* written_count only used for written_areas vector.
     * if > 0, areas will NOT be merged, so we can keep separate
     * counts by page (hopefully not making the list too long).
     */
    uint written_count;
    /* Used only for -sandbox2ro_threshold.  It's only in the
     * written_areas vector b/c executable_areas has its regions removed
     * on a flush while threads could still be accessing counters in
     * selfmod fragments in the cache.  We lose some granularity here but
     * it's not a big deal.
     * We could make these both ushorts, but it'd be more of a pain
     * to increment this counter from the cache then, worrying about overflows.
     */
    uint selfmod_execs;
#ifdef DEBUG
    uint ro2s_xfers;
    uint s2ro_xfers;
#endif
} ro_vs_sandbox_data_t;

/* Our executable area list has three types of areas.  Each type can be merged
 * with adjacent areas of the same type but not with any of the other types!
 * 1) originally RO code   == we leave alone
 * 2) originally RW code   == we mark RO
 * 3) originally RW code, written to from within itself  == we leave RW and sandbox
 * We keep all three types in the same list b/c any particular address interval
 * can only be of one type at any one time, and all three are executable, meaning
 * code cache code was copied from there.
 */
typedef struct vm_area_t {
    app_pc start;
    app_pc end; /* open end interval */
    /* We have two different flags fields to allow easy use of the FRAG_ flags.
     * The two combined are used to distinguish different regions.
     * Adjacent regions w/ different flags are never merged.
     */
    /* Flags that start with VM_ */
    uint vm_flags;
    /* Flags that start with FRAG_
     * In use now are FRAG_SELFMOD_SANDBOXED and FRAG_DYNGEN.
     */
    uint frag_flags;
#ifdef DEBUG
    char *comment;
#endif
    /********************
     * custom fields not used in all vectors
     * FIXME: separate into separately-allocated piece?  or have a struct
     * extension (poor man's subclass, like trace_t, etc.) and make our vector
     * iterators handle it?
     * once we have a generic interval data structure (case 6208) this
     * hardcoding of individual uses will go away.
     */
    union {
        /* Used in per-thread and shared vectors, not in main area lists.
         * We identify vectors using this via VECTOR_FRAGMENT_LIST, needed
         * b/c {add,remove}_vm_area have special behavior for frags.
         */
        fragment_t *frags;
        /* for clients' custom use via vmvector interfaces */
        void *client;
    } custom;
} vm_area_t;

/* for each thread we record all executable areas, to make it faster
 * to decide whether we need to flush any fragments on an munmap
 */
typedef struct thread_data_t {
    vm_area_vector_t areas;
    /* cached pointer to last area encountered by thread */
    vm_area_t *last_area;
    /* FIXME: for locality would be nice to have per-thread last_shared_area
     * (cannot put shared in private last_area, that would void its usefulness
     *  since couldn't tell if area really in shared list or not)
     * but then have to update all other threads whenever change shared
     * vmarea vector, so for now we use a global last_area
     */
    /* cached pointer of a PC in the last page decoded by thread -- set only
     * in thread-private structures, not in shared structures like shared_data */
    app_pc last_decode_area_page_pc;
    bool last_decode_area_valid; /* since no sentinel exists */
#ifdef PROGRAM_SHEPHERDING
    uint thrown_exceptions; /* number of responses to execution violations */
#endif
} thread_data_t;

#define SHOULD_LOCK_VECTOR(v)                                                \
    (TEST(VECTOR_SHARED, (v)->flags) && !TEST(VECTOR_NO_LOCK, (v)->flags) && \
     !self_owns_write_lock(&(v)->lock))

#define LOCK_VECTOR(v, release_lock, RW) \
    do {                                 \
        if (SHOULD_LOCK_VECTOR(v)) {     \
            (release_lock) = true;       \
            d_r_##RW##_lock(&(v)->lock); \
        } else                           \
            (release_lock) = false;      \
    } while (0);

#define UNLOCK_VECTOR(v, release_lock, RW)               \
    do {                                                 \
        if ((release_lock)) {                            \
            ASSERT(TEST(VECTOR_SHARED, (v)->flags));     \
            ASSERT(!TEST(VECTOR_NO_LOCK, (v)->flags));   \
            ASSERT_OWN_READWRITE_LOCK(true, &(v)->lock); \
            d_r_##RW##_unlock(&v->lock);                 \
        }                                                \
    } while (0);

/* these two global vectors store all executable areas and all dynamo
 * areas (executable or otherwise).
 * executable_areas' custom field is used to store coarse unit info.
 * for a FRAG_COARSE_GRAIN region, an info struct is always present, even
 * if not yet executed from (initially, or after a flush).
 */
static vm_area_vector_t *executable_areas;
static vm_area_vector_t *dynamo_areas;

/* Protected by executable_areas lock; used only to delete coarse_info_t
 * while holding executable_areas lock during execute-less flushes
 * (case 10995).  Extra layer of indirection to get on heap and avoid .data
 * unprotection.
 */
static coarse_info_t **coarse_to_delete;

/* used for DYNAMO_OPTION(handle_DR_modify),
 * DYNAMO_OPTION(handle_ntdll_modify) == DR_MODIFY_NOP or
 * DYNAMO_OPTION(patch_proof_list)
 */
static vm_area_vector_t *pretend_writable_areas;

/* used for DYNAMO_OPTION(patch_proof_list) areas to watch */
vm_area_vector_t *patch_proof_areas;

/* used for DYNAMO_OPTION(emulate_IAT_writes), though in future may be
 * expanded, so not just ifdef WINDOWS or ifdef PROGRAM_SHEPHERDING
 */
vm_area_vector_t *emulate_write_areas;

/* used for DYNAMO_OPTION(IAT_convert)
 * IAT or GOT areas of all mapped DLLs - note the exact regions are added here.
 * While the IATs for modules in native_exec_areas are not added here -
 * note that any module's IAT may still be importing native modules.
 */
vm_area_vector_t *IAT_areas;

/* Keeps persistent written-to and execution counts for switching back and
 * forth from page prot to sandboxing.
 */
static vm_area_vector_t *written_areas;

static void
free_written_area(void *data);

#ifdef PROGRAM_SHEPHERDING
/* For executable_if_flush and executable_if_alloc, we need a future list, so
 * their regions are considered executable until de-allocated -- even if written to!
 */
static vm_area_vector_t *futureexec_areas;
#    ifdef WINDOWS
/* FIXME: for -xdata_rct we only need start pc called on, so htable would do,
 * once we have reusable htable for storing single pc
 */
static vm_area_vector_t *app_flushed_areas;

#    endif
#endif

/* tamper resistant region see tamper_resistant_region_add() for current use.
 * If needed this should be turned into a vm_area_vector_t as well.
 */
static app_pc tamper_resistant_region_start, tamper_resistant_region_end;

/* shared_data is synchronized via either single_thread_in_DR or
 * the vector lock (cannot use bb_building_lock b/c both trace building
 * and pc translation need read access and neither can/should grab
 * the bb building lock, plus it's cleaner to not depend on it, and now
 * with -shared_traces it's not sufficient).
 * N.B.: the vector lock is used to protect not just the vector, but also
 * the whole thread_data_t struct (including last_area) and sequences
 * of vector operations.
 * Kept on the heap for selfprot (case 7957).
 */
static thread_data_t *shared_data; /* set in vm_areas_reset_init() */

typedef struct _pending_delete_t {
#ifdef DEBUG
    /* record bounds of original deleted region, for debugging only */
    app_pc start;
    app_pc end;
#endif
    /* list of unlinked fragments that are waiting to be deleted */
    fragment_t *frags;
    /* ref count and timestamp to determine when it's safe to delete them */
    uint ref_count;
    uint flushtime_deleted;
    /* we use a simple linked list of entries */
    struct _pending_delete_t *next;
} pending_delete_t;

/* We keep these list pointers on the heap for selfprot (case 8074). */
typedef struct _deletion_lists_t {
    /* Unlike private vm lists, we cannot simply mark shared_data vm areas as
     * deleted since new fragments come in concurrently, so we have to have a
     * separate list of flushed-but-not-yet-deleted areas.  We can't use a
     * vm_area_vector_t b/c newly flushed fragments spoil our ref count by resetting
     * it, so we keep a linked list of fragment lists.
     */
    pending_delete_t *shared_delete;
    /* We maintain the tail solely for fcache_free_pending_units() */
    pending_delete_t *shared_delete_tail;
    /* count used for reset threshold */
    uint shared_delete_count;

    /* shared lazy deletion: a list of fragment_t chained via next_vmarea that
     * are pending deletion, but are only freed when a shared deletion event
     * shows that it is safe to do so.
     */
    fragment_t *lazy_delete_list;
    /* stores the end of the list, for appending */
    fragment_t *lazy_delete_tail;
    /* stores the length of the lazy list */
    uint lazy_delete_count;
    /* ensure only one thread tries to move to pending deletion list */
    bool move_pending;
} deletion_lists_t;

static deletion_lists_t *todelete;

typedef struct _last_deallocated_t {
    /* case 9330 - we want to detect races during DLL unloads, and to
     * silence a reported violation during unload.  At least DLLs are
     * expected to be already serialized by the loader so keeping only
     * one is sufficient (note Win2K3 doesn't hold lock only during
     * process initialization).  We'll also keep references to the
     * last DLL that was unloaded for diagnostics.  Although, that is
     * not reliable enough when multiple DLLs are involved - case 6061
     * should be used for better tracking after unload.
     */
    /* Yet loss of integrity is tolerable, as long as detected.  Since
     * we currently mark all mappings they are not necessarily
     * serialized (and potentially other apps can directly map, so
     * can't really count on the loader lock for integrity).  We
     * should make sure that we do not set unload_in_progress unless
     * [last_unload_base, last_unload_size) is really still the
     * current module.
     */
    bool unload_in_progress;
    app_pc last_unload_base;
    size_t last_unload_size;
    /* FIXME: we may want to overload the above or add different
     * fields for non image (MEM_MAPPED) unmaps, and DGC (MEM_PRIVATE)
     * frees.  Note that we avoid keeping lists of active unloads, or
     * even to deal with case 9371 we would need intersection of
     * overlapping app syscalls.  If we serialize app syscalls as
     * proposed case 545 a single one will be sufficient.
     */
} last_deallocated_t;
static last_deallocated_t *last_deallocated;
/* synchronization currently used only for the contents of
 * last_deallocated: last_unload_base and last_unload_size
 */
DECLARE_CXTSWPROT_VAR(static mutex_t last_deallocated_lock,
                      INIT_LOCK_FREE(last_deallocated_lock));

/* synchronization for shared_delete, not a rw lock since readers usually write */
DECLARE_CXTSWPROT_VAR(mutex_t shared_delete_lock, INIT_LOCK_FREE(shared_delete_lock));
/* synchronization for the lazy deletion list */
DECLARE_CXTSWPROT_VAR(static mutex_t lazy_delete_lock, INIT_LOCK_FREE(lazy_delete_lock));

/* multi_entry_t allocation is either global or local heap */
#define MULTI_ALLOC_DC(dc, flags) FRAGMENT_ALLOC_DC(dc, flags)
#define GET_DATA(dc, flags)                                  \
    (((dc) == GLOBAL_DCONTEXT || TEST(FRAG_SHARED, (flags))) \
         ? shared_data                                       \
         : (thread_data_t *)(dc)->vm_areas_field)
#define GET_VECTOR(dc, flags)                                             \
    (((dc) == GLOBAL_DCONTEXT || TEST(FRAG_SHARED, (flags)))              \
         ? (TEST(FRAG_WAS_DELETED, (flags)) ? NULL : &shared_data->areas) \
         : (&((thread_data_t *)(dc)->vm_areas_field)->areas))
#define SHARED_VECTOR_RWLOCK(v, rw, op)         \
    do {                                        \
        if (TEST(VECTOR_SHARED, (v)->flags)) {  \
            ASSERT(SHARED_FRAGMENTS_ENABLED()); \
            d_r_##rw##_##op(&(v)->lock);        \
        }                                       \
    } while (0)
#define ASSERT_VMAREA_DATA_PROTECTED(data, RW)                          \
    ASSERT_OWN_##RW##_LOCK(                                             \
        (data == shared_data && !INTERNAL_OPTION(single_thread_in_DR)), \
        &shared_data->areas.lock)

/* FIXME: find a way to assert that an area by itself is synchronized if
 * it points into a vector for the routines that take in only areas
 */
#ifdef DEBUG
#    define ASSERT_VMAREA_VECTOR_PROTECTED(v, RW)                                        \
        do {                                                                             \
            ASSERT_OWN_##RW##_LOCK(SHOULD_LOCK_VECTOR(v) && !dynamo_exited, &(v)->lock); \
            if ((v) == dynamo_areas) {                                                   \
                ASSERT(dynamo_areas_uptodate || dynamo_areas_synching);                  \
            }                                                                            \
        } while (0);
#else
#    define ASSERT_VMAREA_VECTOR_PROTECTED(v, RW) /* nothing */
#endif

/* size of security violation string - must be at least 16 */
#define MAXIMUM_VIOLATION_NAME_LENGTH 16

#define VMVECTOR_INITIALIZE_VECTOR(v, flags, lockname)        \
    do {                                                      \
        vmvector_init_vector((v), (flags));                   \
        ASSIGN_INIT_READWRITE_LOCK_FREE((v)->lock, lockname); \
    } while (0);

/* forward declarations */
static void
vmvector_free_vector(dcontext_t *dcontext, vm_area_vector_t *v);

static void
vm_area_clean_fraglist(dcontext_t *dcontext, vm_area_t *area);
static bool
lookup_addr(vm_area_vector_t *v, app_pc addr, vm_area_t **area);
#if defined(DEBUG) && defined(INTERNAL)
static void
print_fraglist(dcontext_t *dcontext, vm_area_t *area, const char *prefix);
static void
print_written_areas(file_t outf);
#endif
#ifdef DEBUG
static void
exec_area_bounds_match(dcontext_t *dcontext, thread_data_t *data);
#endif

static void
update_dynamo_vm_areas(bool have_writelock);
static void
dynamo_vm_areas_start_reading(void);
static void
dynamo_vm_areas_done_reading(void);

#ifdef PROGRAM_SHEPHERDING
static bool
remove_futureexec_vm_area(app_pc start, app_pc end);

DECLARE_CXTSWPROT_VAR(static mutex_t threads_killed_lock,
                      INIT_LOCK_FREE(threads_killed_lock));
void
mark_unload_future_added(app_pc module_base, size_t size);
#endif

static void
vm_area_coarse_region_freeze(dcontext_t *dcontext, coarse_info_t *info, vm_area_t *area,
                             bool in_place);

#ifdef SIMULATE_ATTACK
/* synch simulate_at string parsing */
DECLARE_CXTSWPROT_VAR(static mutex_t simulate_lock, INIT_LOCK_FREE(simulate_lock));
#endif

/* used to determine when we need to do another heap walk to keep
 * dynamo vm areas up to date (can't do it incrementally b/c of
 * circular dependencies).
 * protected for both read and write by dynamo_areas->lock
 */
/* Case 3045: areas inside the vmheap reservation are not added to the list,
 * so the vector is considered uptodate until we run out of reservation
 */
DECLARE_FREQPROT_VAR(static bool dynamo_areas_uptodate, true);

#ifdef DEBUG
/* used for debugging to tell when uptodate can be false.
 * protected for both read and write by dynamo_areas->lock
 */
DECLARE_FREQPROT_VAR(static bool dynamo_areas_synching, false);
#endif

/* HACK to make dynamo_areas->lock recursive
 * protected for both read and write by dynamo_areas->lock
 * FIXME: provide general rwlock w/ write portion recursive
 */
DECLARE_CXTSWPROT_VAR(uint dynamo_areas_recursion, 0);

/* used for DR area debugging */
bool vm_areas_exited = false;

/***************************************************
 * flushing by walking entire hashtable is too slow, so we keep a list of
 * all fragments in each region.
 * to save memory, we use the fragment_t struct as the linked list entry
 * for these lists.  However, some fragments are on multiple lists due to
 * crossing boundaries (usually traces).  For those, the other entries are
 * pointed to by an "also" field, and the entries themselves use this struct,
 * which plays games (similar to fcache's empty_slot_t) to be able to be used
 * like a fragment_t struct in the lists.
 *
 * this is better than the old fragment_t->app_{min,max}_pc performance wise,
 * and granularity-wise for blocks that bounce over regions, but worse
 * granularity-wise since if want to flush singe page in text
 * section, will end up flushing entire region.  especially scary in face of
 * merges of adjacent regions, but merges are rare for images since
 * they usually have more than just text, so texts aren't adjacent.
 *
 * FIXME: better way, now that fcache supports multiple units, is to have
 * a separate unit for each source vmarea.  common case will be a flush to
 * an un-merged or clipped area, so just toss whole unit.
 */
typedef struct _multi_entry_t {
    fragment_t *f; /* backpointer */
    /* flags MUST be at same location as fragment_t->flags
     * we set flags==FRAG_IS_EXTRA_VMAREA to indicate a multi_entry_t
     * we also use FRAG_SHARED to indicate that a multi_entry_t is on global heap
     */
    uint flags;
    /* officially all list entries are fragment_t *, really some are multi_entry_t */
    fragment_t *next_vmarea;
    fragment_t *prev_vmarea;
    fragment_t *also_vmarea; /* if in multiple areas */
    /* need to be able to look up vmarea: area not stored since vmareas
     * shift and merge, so we store original pc */
    app_pc pc;
} multi_entry_t;

/* macros to make dealing with both fragment_t and multi_entry_t easier */
#define FRAG_MULTI(f) (TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags))

#define FRAG_MULTI_INIT(f) \
    (TESTALL((FRAG_IS_EXTRA_VMAREA | FRAG_IS_EXTRA_VMAREA_INIT), (f)->flags))

#define FRAG_NEXT(f)                                                                \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags)) ? ((multi_entry_t *)(f))->next_vmarea \
                                              : (f)->next_vmarea)

#define FRAG_NEXT_ASSIGN(f, val)                         \
    do {                                                 \
        if (TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags))      \
            ((multi_entry_t *)(f))->next_vmarea = (val); \
        else                                             \
            (f)->next_vmarea = (val);                    \
    } while (0)

#define FRAG_PREV(f)                                                                \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags)) ? ((multi_entry_t *)(f))->prev_vmarea \
                                              : (f)->prev_vmarea)

#define FRAG_PREV_ASSIGN(f, val)                         \
    do {                                                 \
        if (TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags))      \
            ((multi_entry_t *)(f))->prev_vmarea = (val); \
        else                                             \
            (f)->prev_vmarea = (val);                    \
    } while (0)

/* Case 8419: also_vmarea is invalid once we 1st-stage-delete a fragment */
#define FRAG_ALSO(f)                           \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags))  \
         ? ((multi_entry_t *)(f))->also_vmarea \
         : (ASSERT(!TEST(FRAG_WAS_DELETED, (f)->flags)), (f)->also.also_vmarea))
/* Only call this one to avoid the assert when you know it's safe */
#define FRAG_ALSO_DEL_OK(f)                                                         \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags)) ? ((multi_entry_t *)(f))->also_vmarea \
                                              : (f)->also.also_vmarea)

#define FRAG_ALSO_ASSIGN(f, val)                         \
    do {                                                 \
        if (TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags))      \
            ((multi_entry_t *)(f))->also_vmarea = (val); \
        else {                                           \
            ASSERT(!TEST(FRAG_WAS_DELETED, (f)->flags)); \
            (f)->also.also_vmarea = (val);               \
        }                                                \
    } while (0)

/* assumption: if multiple units, fragment_t is on list of region owning tag */
#define FRAG_PC(f) \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags)) ? ((multi_entry_t *)(f))->pc : (f)->tag)

#define FRAG_PC_ASSIGN(f, val)                      \
    do {                                            \
        if (TEST(FRAG_IS_EXTRA_VMAREA, (f)->flags)) \
            ((multi_entry_t *)(f))->pc = (val);     \
        else                                        \
            ASSERT_NOT_REACHED();                   \
    } while (0)

#define FRAG_FRAG(fr) \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (fr)->flags)) ? ((multi_entry_t *)(fr))->f : (fr))

#define FRAG_FRAG_ASSIGN(fr, val)                    \
    do {                                             \
        if (TEST(FRAG_IS_EXTRA_VMAREA, (fr)->flags)) \
            ((multi_entry_t *)(fr))->f = (val);      \
        else                                         \
            ASSERT_NOT_REACHED();                    \
    } while (0)

#define FRAG_ID(fr)                                                             \
    ((TEST(FRAG_IS_EXTRA_VMAREA, (fr)->flags)) ? ((multi_entry_t *)(fr))->f->id \
                                               : (fr)->id)

/***************************************************/

/* FIXME : is problematic to page align subpage regions */
static void
vm_make_writable(byte *pc, size_t size)
{
    byte *start_pc = (byte *)ALIGN_BACKWARD(pc, PAGE_SIZE);
    size_t final_size = ALIGN_FORWARD(size + (pc - start_pc), PAGE_SIZE);
    DEBUG_DECLARE(bool ok =)
    make_writable(start_pc, final_size);
    ASSERT(ok);
    ASSERT(INTERNAL_OPTION(hw_cache_consistency));
}

static void
vm_make_unwritable(byte *pc, size_t size)
{
    byte *start_pc = (byte *)ALIGN_BACKWARD(pc, PAGE_SIZE);
    size_t final_size = ALIGN_FORWARD(size + (pc - start_pc), PAGE_SIZE);
    ASSERT(INTERNAL_OPTION(hw_cache_consistency));
    make_unwritable(start_pc, final_size);

    /* case 8308: We should never call vm_make_unwritable if
     * -sandbox_writable is on, or if -sandbox_non_text is on and this
     * is a non-text region.
     */
    ASSERT(!DYNAMO_OPTION(sandbox_writable));
    DOCHECK(1, {
        if (DYNAMO_OPTION(sandbox_non_text)) {
            app_pc modbase = get_module_base(pc);
            ASSERT(modbase != NULL &&
                   is_range_in_code_section(modbase, pc, pc + size, NULL, NULL));
        }
    });
}

/* since dynamorio changes some readwrite memory regions to read only,
 * this changes all regions memory permissions back to what they should be,
 * since dynamorio uses this mechanism to ensure code cache coherency,
 * once this method is called stale code could be executed out of the
 * code cache */
void
revert_memory_regions()
{
    int i;

    /* executable_areas doesn't exist in thin_client mode. */
    ASSERT(!DYNAMO_OPTION(thin_client));

    d_r_read_lock(&executable_areas->lock);
    for (i = 0; i < executable_areas->length; i++) {
        if (DR_MADE_READONLY(executable_areas->buf[i].vm_flags)) {
            /* this is a region that dynamorio has marked read only, fix */
            LOG(GLOBAL, LOG_VMAREAS, 1,
                " fixing permissions for RW executable area " PFX "-" PFX " %s\n",
                executable_areas->buf[i].start, executable_areas->buf[i].end,
                executable_areas->buf[i].comment);
            vm_make_writable(executable_areas->buf[i].start,
                             executable_areas->buf[i].end -
                                 executable_areas->buf[i].start);
        }
    }
    d_r_read_unlock(&executable_areas->lock);
}

static void
print_vm_flags(uint vm_flags, uint frag_flags, file_t outf)
{
    print_file(outf, " %s%s%s%s", (vm_flags & VM_WRITABLE) != 0 ? "W" : "-",
               (vm_flags & VM_WAS_FUTURE) != 0 ? "F" : "-",
               (frag_flags & FRAG_SELFMOD_SANDBOXED) != 0 ? "S" : "-",
               TEST(FRAG_COARSE_GRAIN, frag_flags) ? "C" : "-");
#ifdef PROGRAM_SHEPHERDING
    print_file(outf, "%s%s", TEST(VM_PATTERN_REVERIFY, vm_flags) ? "P" : "-",
               (frag_flags & FRAG_DYNGEN) != 0 ? "D" : "-");
#endif
}

/* ok to pass NULL for v, only used to identify use of custom field */
static void
print_vm_area(vm_area_vector_t *v, vm_area_t *area, file_t outf, const char *prefix)
{
    print_file(outf, "%s" PFX "-" PFX, prefix, area->start, area->end);
    print_vm_flags(area->vm_flags, area->frag_flags, outf);
    if (v == executable_areas && TEST(FRAG_COARSE_GRAIN, area->frag_flags)) {
        coarse_info_t *info = (coarse_info_t *)area->custom.client;
        if (info != NULL) {
            if (info->persisted)
                print_file(outf, "R");
            else if (info->frozen)
                print_file(outf, "Z");
            else
                print_file(outf, "-");
        }
    }
#ifdef DEBUG
    print_file(outf, " %s", area->comment);
    DOLOG(1, LOG_VMAREAS, {
        IF_NO_MEMQUERY(extern vm_area_vector_t * all_memory_areas;)
        extern vm_area_vector_t *fcache_unit_areas;   /* fcache.c */
        extern vm_area_vector_t *loaded_module_areas; /* module_list.c */
        app_pc modbase =
            /* avoid rank order violation */
            IF_NO_MEMQUERY(v == all_memory_areas ? NULL :)
            /* i#1649: avoid rank order for dynamo_areas and for other vectors. */
            ((v == dynamo_areas || v == fcache_unit_areas || v == loaded_module_areas ||
              v == modlist_areas IF_LINUX(|| v == d_r_rseq_areas))
                 ? NULL
                 : get_module_base(area->start));
        if (modbase != NULL &&
            /* avoid rank order violations */
            v != dynamo_areas && v != written_areas &&
            /* we free module list before vmareas */
            !dynamo_exited_and_cleaned &&
            is_mapped_as_image(modbase) /*avoid asserts in getting name */) {
            const char *name;
            os_get_module_info_lock();
            os_get_module_name(modbase, &name);
            print_file(outf, " %s", name == NULL ? "" : name);
            os_get_module_info_unlock();
        }
    });
#endif
    if (v == written_areas) {
        ro_vs_sandbox_data_t *ro2s = (ro_vs_sandbox_data_t *)area->custom.client;
#ifdef DEBUG
        if (ro2s != NULL) { /* can be null if in middle of adding */
            uint tot_w = ro2s->ro2s_xfers * DYNAMO_OPTION(ro2sandbox_threshold);
            uint tot_s = ro2s->s2ro_xfers * DYNAMO_OPTION(sandbox2ro_threshold);
            print_file(outf, " w %3d, %3d tot; x %3d, %5d tot; ro2s %d, s2ro %d",
                       ro2s->written_count, tot_w, ro2s->selfmod_execs, tot_s,
                       ro2s->ro2s_xfers, ro2s->s2ro_xfers);
        }
#else
        print_file(outf, " written %3d, exec %5d", ro2s->written_count,
                   ro2s->selfmod_execs);
#endif
    }
    print_file(outf, "\n");
}

/* Assumes caller holds v->lock for coherency */
static void
print_vm_areas(vm_area_vector_t *v, file_t outf)
{
    int i;
    ASSERT_VMAREA_VECTOR_PROTECTED(v, READWRITE);
    for (i = 0; i < v->length; i++) {
        print_vm_area(v, &v->buf[i], outf, "  ");
    }
}

#if defined(DEBUG) && defined(INTERNAL)
static void
print_contig_vm_areas(vm_area_vector_t *v, app_pc start, app_pc end, file_t outf,
                      const char *prefix)
{
    vm_area_t *new_area;
    app_pc pc = start;
    do {
        lookup_addr(v, pc, &new_area);
        if (new_area == NULL)
            break;
        print_vm_area(v, new_area, outf, prefix);
        pc = new_area->end + 1;
    } while (new_area->end < end);
}
#endif

#if defined(DEBUG) && defined(INTERNAL)
static void
print_pending_list(file_t outf)
{
    pending_delete_t *pend;
    int i;
    ASSERT_OWN_MUTEX(true, &shared_delete_lock);
    for (i = 0, pend = todelete->shared_delete; pend != NULL; i++, pend = pend->next) {
        print_file(outf, "%d: " PFX "-" PFX " ref=%d, stamp=%d\n", i, pend->start,
                   pend->end, pend->ref_count, pend->flushtime_deleted);
    }
}
#endif

/* If v requires a lock and the calling thread does not hold that lock,
 * this routine acquires the lock and returns true; else it returns false.
 */
static bool
writelock_if_not_already(vm_area_vector_t *v)
{
    if (TEST(VECTOR_SHARED, v->flags) && !self_owns_write_lock(&v->lock)) {
        SHARED_VECTOR_RWLOCK(v, write, lock);
        return true;
    }
    return false;
}

static void
vm_area_vector_check_size(vm_area_vector_t *v)
{
    /* only called by add_vm_area which does the assert that the vector is
     * protected */
    /* check if at capacity */
    if (v->size == v->length) {
        if (v->length == 0) {
            v->size = INTERNAL_OPTION(vmarea_initial_size);
            v->buf = (vm_area_t *)global_heap_alloc(
                v->size * sizeof(struct vm_area_t) HEAPACCT(ACCT_VMAREAS));
        } else {
            /* FIXME: case 4471 we should be doubling size here */
            int new_size = (INTERNAL_OPTION(vmarea_increment_size) + v->length);
            STATS_INC(num_vmareas_resized);
            v->buf = global_heap_realloc(v->buf, v->size, new_size,
                                         sizeof(struct vm_area_t) HEAPACCT(ACCT_VMAREAS));
            v->size = new_size;
        }
        ASSERT(v->buf != NULL);
    }
}

static void
vm_area_merge_fraglists(vm_area_t *dst, vm_area_t *src)
{
    /* caller must hold write lock for vector of course: FIXME: assert that here */
    LOG(THREAD_GET, LOG_VMAREAS, 2,
        "\tmerging frag lists for " PFX "-" PFX " and " PFX "-" PFX "\n", src->start,
        src->end, dst->start, dst->end);
    if (dst->custom.frags == NULL)
        dst->custom.frags = src->custom.frags;
    else if (src->custom.frags == NULL)
        return;
    else {
        /* put src's frags at end of dst's frags */
        fragment_t *top1 = dst->custom.frags;
        fragment_t *top2 = src->custom.frags;
        fragment_t *tmp = FRAG_PREV(top1);
        FRAG_NEXT_ASSIGN(tmp, top2);
        FRAG_PREV_ASSIGN(top1, FRAG_PREV(top2));
        FRAG_PREV_ASSIGN(top2, tmp);
        DOLOG(4, LOG_VMAREAS, {
            print_fraglist(get_thread_private_dcontext(), dst,
                           "after merging fraglists:");
        });
    }
}

/* Assumes caller holds v->lock, if necessary.
 * Does not return the area added since it may be merged or split depending
 * on existing areas->
 * If a last_area points into this vector, the caller must make sure to
 *   clear or update the last_area pointer.
 *   FIXME: make it easier to keep them in synch -- too easy to add_vm_area
 *   somewhere to a thread vector and forget to clear last_area.
 * Adds a new area to v, merging it with adjacent areas of the same type.
 * A new area is only allowed to overlap an old area of a different type if it
 *   meets certain criteria (see asserts below).  For VM_WAS_FUTURE and
 *   VM_ONCE_ONLY we may clear the flag from an existing region if the new
 *   region doesn't have the flag and overlaps the existing region.  Otherwise
 *   the new area is split such that the overlapping portion remains part of
 *   the old area.  This tries to keep entire new area from becoming selfmod
 *   for instance. FIXME : for VM_WAS_FUTURE and VM_ONCE_ONLY may want to split
 *   region if only paritally overlapping
 *
 * FIXME: change add_vm_area to return NULL when merged, and otherwise
 * return the new complete area, so callers don't have to do a separate lookup
 * to access the added area.
 */
static void
add_vm_area(vm_area_vector_t *v, app_pc start, app_pc end, uint vm_flags, uint frag_flags,
            void *data _IF_DEBUG(const char *comment))
{
    int i, j, diff;
    /* if we have overlap, we extend an existing area -- else we add a new area */
    int overlap_start = -1, overlap_end = -1;
    DEBUG_DECLARE(uint flagignore;)
    IF_UNIX(IF_DEBUG(IF_NO_MEMQUERY(extern vm_area_vector_t * all_memory_areas;)))

    ASSERT(start < end);

    ASSERT_VMAREA_VECTOR_PROTECTED(v, WRITE);
    LOG(GLOBAL, LOG_VMAREAS, 4, "in add_vm_area%s " PFX " " PFX " %s\n",
        (v == executable_areas ? " executable_areas"
                               : (v == IF_LINUX_ELSE(all_memory_areas, NULL)
                                      ? " all_memory_areas"
                                      : (v == dynamo_areas ? " dynamo_areas" : ""))),
        start, end, comment);
    /* N.B.: new area could span multiple existing areas! */
    for (i = 0; i < v->length; i++) {
        /* look for overlap, or adjacency of same type (including all flags, and never
         * merge adjacent if keeping write counts)
         */
        if ((start < v->buf[i].end && end > v->buf[i].start) ||
            (start <= v->buf[i].end && end >= v->buf[i].start &&
             vm_flags == v->buf[i].vm_flags && frag_flags == v->buf[i].frag_flags &&
             /* never merge coarse-grain */
             !TEST(FRAG_COARSE_GRAIN, v->buf[i].frag_flags) &&
             !TEST(VECTOR_NEVER_MERGE_ADJACENT, v->flags) &&
             (v->should_merge_func == NULL ||
              v->should_merge_func(true /*adjacent*/, data, v->buf[i].custom.client)))) {
            ASSERT(!(start < v->buf[i].end && end > v->buf[i].start) ||
                   !TEST(VECTOR_NEVER_OVERLAP, v->flags));
            if (overlap_start == -1) {
                /* assume we'll simply expand an existing area rather than
                 * add a new one -- we'll reset this if we hit merge conflicts */
                overlap_start = i;
            }
            /* overlapping regions of different properties are often
             * problematic so we add a lot of debugging output
             */
            DOLOG(4, LOG_VMAREAS, {
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "==================================================\n"
                    "add_vm_area " PFX "-" PFX " %s %x-%x overlaps " PFX "-" PFX
                    " %s %x-%x\n",
                    start, end, comment, vm_flags, frag_flags, v->buf[i].start,
                    v->buf[i].end, v->buf[i].comment, v->buf[i].vm_flags,
                    v->buf[i].frag_flags);
                print_vm_areas(v, GLOBAL);
                /* rank order problem if holding heap_unit_lock, so only print
                 * if not holding a lock for v right now, though ok to print
                 * for shared vm areas since its lock is higher than the lock
                 * for executable/written areas
                 */
                if (v != dynamo_areas &&
                    (!TEST(VECTOR_SHARED, v->flags) || v == &shared_data->areas)) {
                    LOG(GLOBAL, LOG_VMAREAS, 1, "\nexecutable areas:\n");
                    print_executable_areas(GLOBAL);
                    LOG(GLOBAL, LOG_VMAREAS, 1, "\nwritten areas:\n");
                    print_written_areas(GLOBAL);
                }
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "==================================================\n\n");
            });

            /* we have some restrictions on overlapping regions with
             * different flags */

            /* no restrictions on WAS_FUTURE flag, but if new region is
             * not was future and old region is then should drop from old
             * region FIXME : partial overlap? we don't really care about
             * this flag anyways */
            if (TEST(VM_WAS_FUTURE, v->buf[i].vm_flags) &&
                !TEST(VM_WAS_FUTURE, vm_flags)) {
                v->buf[i].vm_flags &= ~VM_WAS_FUTURE;
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "Warning : removing was_future flag from area " PFX "-" PFX
                    " %s that overlaps new area " PFX "-" PFX " %s\n",
                    v->buf[i].start, v->buf[i].end, v->buf[i].comment, start, end,
                    comment);
            }
            /* no restrictions on ONCE_ONLY flag, but if new region is not
             * should drop fom existing region FIXME : partial overlap? is
             * not much of an additional security risk */
            if (TEST(VM_ONCE_ONLY, v->buf[i].vm_flags) && !TEST(VM_ONCE_ONLY, vm_flags)) {
                v->buf[i].vm_flags &= ~VM_ONCE_ONLY;
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "Warning : removing once_only flag from area " PFX "-" PFX
                    " %s that overlaps new area " PFX "-" PFX " %s\n",
                    v->buf[i].start, v->buf[i].end, v->buf[i].comment, start, end,
                    comment);
            }
            /* shouldn't be adding unmod image over existing not unmod image,
             * reverse could happen with os region merging though */
            ASSERT(TEST(VM_UNMOD_IMAGE, v->buf[i].vm_flags) ||
                   !TEST(VM_UNMOD_IMAGE, vm_flags));
            /* for VM_WRITABLE only allow new region to not be writable and
             * existing region to be writable to handle cases of os region
             * merging due to our consistency protection changes */
            ASSERT(TEST(VM_WRITABLE, v->buf[i].vm_flags) ||
                   !TEST(VM_WRITABLE, vm_flags) ||
                   !INTERNAL_OPTION(hw_cache_consistency));
            /* FIXME: case 7877: if new is VM_MADE_READONLY and old is not, we
             * must mark old overlapping portion as VM_MADE_READONLY.  Things only
             * worked now b/c VM_MADE_READONLY==VM_WRITABLE, so we can add
             * pageprot regions that overlap w/ selfmod.
             */
#ifdef PROGRAM_SHEPHERDING
            /* !VM_PATTERN_REVERIFY trumps having the flag on, so for new having
             * the flag and old not, we're fine, but when old has it we'd like
             * to remove it from the overlap portion: FIXME: need better merging
             * control, also see all the partial overlap fixmes above.
             * for this flag not a big deal, just a possible perf hit as we
             * re-check every time.
             */
#endif
            /* disallow any other vm_flag differences */
            DODEBUG({
                flagignore = VM_UNMOD_IMAGE | VM_WAS_FUTURE | VM_ONCE_ONLY | VM_WRITABLE;
            });
#ifdef PROGRAM_SHEPHERDING
            DODEBUG({ flagignore = flagignore | VM_PATTERN_REVERIFY; });
#endif
            ASSERT((v->buf[i].vm_flags & ~flagignore) == (vm_flags & ~flagignore));

            /* new region must be more innocent with respect to selfmod */
            ASSERT(TEST(FRAG_SELFMOD_SANDBOXED, v->buf[i].frag_flags) ||
                   !TEST(FRAG_SELFMOD_SANDBOXED, frag_flags));
            /* disallow other frag_flag differences */
#ifndef PROGRAM_SHEPHERDING
            ASSERT((v->buf[i].frag_flags & ~FRAG_SELFMOD_SANDBOXED) ==
                   (frag_flags & ~FRAG_SELFMOD_SANDBOXED));
#else
#    ifdef DGC_DIAGNOSTICS
            /* FIXME : no restrictions on differing FRAG_DYNGEN_RESTRICTED
             * flags? */
            ASSERT((v->buf[i].frag_flags &
                    ~(FRAG_SELFMOD_SANDBOXED | FRAG_DYNGEN | FRAG_DYNGEN_RESTRICTED)) ==
                   (frag_flags &
                    ~(FRAG_SELFMOD_SANDBOXED | FRAG_DYNGEN | FRAG_DYNGEN_RESTRICTED)));
#    else
            ASSERT((v->buf[i].frag_flags & ~(FRAG_SELFMOD_SANDBOXED | FRAG_DYNGEN)) ==
                   (frag_flags & ~(FRAG_SELFMOD_SANDBOXED | FRAG_DYNGEN)));
#    endif
            /* shouldn't add non-dyngen overlapping existing dyngen, FIXME
             * is the reverse possible? right now we allow it */
            ASSERT(TEST(FRAG_DYNGEN, frag_flags) ||
                   !TEST(FRAG_DYNGEN, v->buf[i].frag_flags));
#endif
            /* Never split FRAG_COARSE_GRAIN */
            ASSERT(TEST(FRAG_COARSE_GRAIN, frag_flags) ||
                   !TEST(FRAG_COARSE_GRAIN, v->buf[i].frag_flags));

            /* for overlapping region: must overlap same type -- else split */
            if ((vm_flags != v->buf[i].vm_flags || frag_flags != v->buf[i].frag_flags) &&
                (v->should_merge_func == NULL ||
                 !v->should_merge_func(false /*not adjacent*/, data,
                                       v->buf[i].custom.client))) {
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "add_vm_area " PFX "-" PFX " %s vm_flags=0x%08x "
                    "frag_flags=0x%08x\n  overlaps diff type " PFX "-" PFX " %s"
                    "vm_flags=0x%08x frag_flags=0x%08x\n  in vect at " PFX "\n",
                    start, end, comment, vm_flags, frag_flags, v->buf[i].start,
                    v->buf[i].end, v->buf[i].comment, v->buf[i].vm_flags,
                    v->buf[i].frag_flags, v);
                LOG(GLOBAL, LOG_VMAREAS, 3,
                    "before splitting b/c adding " PFX "-" PFX ":\n", start, end);
                DOLOG(3, LOG_VMAREAS, { print_vm_areas(v, GLOBAL); });

                /* split off the overlapping part from the new region
                 * reasoning: old regions get marked selfmod, then see new code,
                 * its region overlaps old selfmod -- don't make new all selfmod,
                 * split off the part that hasn't been proved selfmod yet.
                 * since we never split the old region, we don't need to worry
                 * about splitting its frags list.
                 */
                if (start < v->buf[i].start) {
                    if (end > v->buf[i].end) {
                        void *add_data = data;
                        /* need two areas, one for either side */
                        LOG(GLOBAL, LOG_VMAREAS, 3,
                            "=> will add " PFX "-" PFX " after i\n", v->buf[i].end, end);
                        /* safe to recurse here, new area will be after the area
                         * we are currently looking at in the vector */
                        if (v->split_payload_func != NULL)
                            add_data = v->split_payload_func(data);
                        add_vm_area(v, v->buf[i].end, end, vm_flags, frag_flags,
                                    add_data _IF_DEBUG(comment));
                    }
                    /* if had been merging, let this routine finish that off -- else,
                     * need to add a new area
                     */
                    end = v->buf[i].start;
                    if (overlap_start == i) {
                        /* no merging */
                        overlap_start = -1;
                    }
                    LOG(GLOBAL, LOG_VMAREAS, 3,
                        "=> will add/merge " PFX "-" PFX " before i\n", start, end);
                    overlap_end = i;
                    break;
                } else if (end > v->buf[i].end) {
                    /* shift area of consideration to end of i, and keep going,
                     * can't act now since don't know areas overlapping beyond i
                     */
                    LOG(GLOBAL, LOG_VMAREAS, 3,
                        "=> ignoring " PFX "-" PFX ", only adding " PFX "-" PFX "\n",
                        start, v->buf[i].end, v->buf[i].end, end);
                    start = v->buf[i].end;
                    /* reset overlap vars */
                    ASSERT(overlap_start <= i);
                    overlap_start = -1;
                } else {
                    /* completely inside -- ok, we'll leave it that way and won't split */
                    LOG(GLOBAL, LOG_VMAREAS, 3,
                        "=> ignoring " PFX "-" PFX ", forcing to be part of " PFX "-" PFX
                        "\n",
                        start, end, v->buf[i].start, v->buf[i].end);
                }
                ASSERT(end > start);
            }
        } else if (overlap_start > -1) {
            overlap_end = i; /* not inclusive */
            break;
        } else if (end <= v->buf[i].start)
            break;
    }

    if (overlap_start == -1) {
        /* brand-new area, goes before v->buf[i] */
        struct vm_area_t new_area = { start, end, vm_flags, frag_flags, /* rest 0 */ };
#ifdef DEBUG
        /* get comment */
        size_t len = strlen(comment);
        ASSERT(len < 1024);
        new_area.comment = (char *)global_heap_alloc(len + 1 HEAPACCT(ACCT_VMAREAS));
        strncpy(new_area.comment, comment, len);
        new_area.comment[len] = '\0'; /* if max no null */
#endif
        new_area.custom.client = data;
        LOG(GLOBAL, LOG_VMAREAS, 3, "=> adding " PFX "-" PFX "\n", start, end);
        vm_area_vector_check_size(v);
        /* shift subsequent entries */
        for (j = v->length; j > i; j--)
            v->buf[j] = v->buf[j - 1];
        v->buf[i] = new_area;
        /* assumption: no overlaps between areas in list! */
#ifdef DEBUG
        if (!((i == 0 || v->buf[i - 1].end <= v->buf[i].start) &&
              (i == v->length || v->buf[i].end <= v->buf[i + 1].start))) {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "ERROR: add_vm_area illegal overlap " PFX " " PFX " %s\n", start, end,
                comment);
            print_vm_areas(v, GLOBAL);
        }
#endif
        ASSERT((i == 0 || v->buf[i - 1].end <= v->buf[i].start) &&
               (i == v->length || v->buf[i].end <= v->buf[i + 1].start));
        v->length++;
        STATS_TRACK_MAX(max_vmareas_length, v->length);
        DOSTATS({
            if (v == dynamo_areas)
                STATS_TRACK_MAX(max_DRareas_length, v->length);
            else if (v == executable_areas)
                STATS_TRACK_MAX(max_execareas_length, v->length);
        });
#ifdef WINDOWS
        DOSTATS({
            extern vm_area_vector_t *loaded_module_areas;
            if (v == loaded_module_areas)
                STATS_TRACK_MAX(max_modareas_length, v->length);
        });
#endif
    } else {
        /* overlaps one or more areas, modify first to equal entire range,
         * delete rest
         */
        if (overlap_end == -1)
            overlap_end = v->length;
        LOG(GLOBAL, LOG_VMAREAS, 3, "=> changing " PFX "-" PFX,
            v->buf[overlap_start].start, v->buf[overlap_start].end);
        if (start < v->buf[overlap_start].start)
            v->buf[overlap_start].start = start;
        if (end > v->buf[overlap_end - 1].end)
            v->buf[overlap_start].end = end;
        else
            v->buf[overlap_start].end = v->buf[overlap_end - 1].end;
        if (v->merge_payload_func != NULL) {
            v->buf[overlap_start].custom.client =
                v->merge_payload_func(data, v->buf[overlap_start].custom.client);
        } else if (v->free_payload_func != NULL) {
            /* if a merge exists we assume it will free if necessary */
            v->free_payload_func(v->buf[overlap_start].custom.client);
        }
        LOG(GLOBAL, LOG_VMAREAS, 3, " to " PFX "-" PFX "\n", v->buf[overlap_start].start,
            v->buf[overlap_start].end);
        /* when merge, use which comment?  could combine them all
         * FIXME
         */
        /* now delete */
        for (i = overlap_start + 1; i < overlap_end; i++) {
            LOG(GLOBAL, LOG_VMAREAS, 3, "=> completely removing " PFX "-" PFX " %s\n",
                v->buf[i].start, v->buf[i].end, v->buf[i].comment);
#ifdef DEBUG
            global_heap_free(v->buf[i].comment,
                             strlen(v->buf[i].comment) + 1 HEAPACCT(ACCT_VMAREAS));
#endif
            if (v->merge_payload_func != NULL) {
                v->buf[overlap_start].custom.client = v->merge_payload_func(
                    v->buf[overlap_start].custom.client, v->buf[i].custom.client);
            } else if (v->free_payload_func != NULL) {
                /* if a merge exists we assume it will free if necessary */
                v->free_payload_func(v->buf[i].custom.client);
            }
            /* See the XXX comment in remove_vm_area about using a free_payload_func.
             * Here we have to handle ld.so using an initial +rx map which triggers
             * loading a persisted unit for the true-x segment and a new coarse unit
             * for the temp-x-later-data segment.  But we then add the full +rx for
             * the whole region, which comes here where we need to remove the
             * just-created data segment coarse unit.  (Yes, better to avoid the temp
             * creation, but that is likely more complex: delay coarse loading until
             * first-execution or something.)
             */
            if (v == executable_areas) {
                coarse_info_t *info = (coarse_info_t *)v->buf[i].custom.client;
                if (info != NULL) {
                    /* Should be un-executed from, and thus requires no reset and
                     * thus no complex delayed deletion via coarse_to_delete.
                     */
                    ASSERT(info->cache == NULL && info->stubs == NULL &&
                           info->non_frozen == NULL);
                    coarse_unit_free(GLOBAL_DCONTEXT, info);
                }
            }
            /* merge frags lists */
            /* FIXME: switch this to a merge_payload_func.  It won't be able
             * to print out the bounds, and it will have to do the work of
             * vm_area_clean_fraglist() on each merge, but we could then get
             * rid of VECTOR_FRAGMENT_LIST.
             */
            if (TEST(VECTOR_FRAGMENT_LIST, v->flags) && v->buf[i].custom.frags != NULL)
                vm_area_merge_fraglists(&v->buf[overlap_start], &v->buf[i]);
        }
        diff = overlap_end - (overlap_start + 1);
        for (i = overlap_start + 1; i < v->length - diff; i++)
            v->buf[i] = v->buf[i + diff];
        v->length -= diff;
        i = overlap_start; /* for return value */
        if (TEST(VECTOR_FRAGMENT_LIST, v->flags) && v->buf[i].custom.frags != NULL) {
            dcontext_t *dcontext = get_thread_private_dcontext();
            ASSERT(dcontext != NULL);
            /* have to remove all alsos that are now in same area as frag */
            vm_area_clean_fraglist(dcontext, &v->buf[i]);
        }
    }
    DOLOG(5, LOG_VMAREAS, { print_vm_areas(v, GLOBAL); });
}

static void
adjust_coarse_unit_bounds(vm_area_t *area, bool if_invalid)
{
    coarse_info_t *info = (coarse_info_t *)area->custom.client;
    ASSERT(TEST(FRAG_COARSE_GRAIN, area->frag_flags));
    ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
    ASSERT(info != NULL);
    if (info == NULL) /* be paranoid */
        return;
    /* FIXME: we'd like to grab info->lock but we have a rank order w/
     * exec_areas lock -- so instead we rely on all-thread-synch flushing
     * being the only reason to get here; an empty flush won't have synchall,
     * but we won't be able to get_executable_area_coarse_info w/o the
     * exec areas write lock so we're ok there.
     */
    ASSERT(dynamo_all_threads_synched ||
           (!TEST(VM_EXECUTED_FROM, area->vm_flags) &&
            READWRITE_LOCK_HELD(&executable_areas->lock)));
    if (!if_invalid && TEST(PERSCACHE_CODE_INVALID, info->flags)) {
        /* Don't change bounds of primary or secondary; we expect vm_area_t to
         * be merged back to this size post-rebind; if not, we'll throw out this
         * pcache at validation time due to not matching the vm_area_t.
         */
        return;
    }
    LOG(THREAD_GET, LOG_VMAREAS, 3, "%s: " PFX "-" PFX " vs area " PFX "-" PFX "\n",
        __FUNCTION__, info->base_pc, info->end_pc, area->start, area->end);
    while (info != NULL) { /* loop over primary and secondary unit */
        /* We should have reset this coarse info when flushing */
        ASSERT((info->cache == NULL && !info->frozen && !info->persisted) ||
               /* i#1652: if nothing was flushed a pcache may remain */
               (info->base_pc == area->start && info->end_pc == area->end));
        /* No longer covers the removed region */
        if (info->base_pc < area->start)
            info->base_pc = area->start;
        if (info->end_pc > area->end)
            info->end_pc = area->end;
        ASSERT(info->frozen || info->non_frozen == NULL);
        info = info->non_frozen;
        ASSERT(info == NULL || !info->frozen);
    }
}

/* Assumes caller holds v->lock, if necessary
 * Returns false if no area contains start..end
 * Ignores type of area -- removes all within start..end
 * Caller should probably clear last_area as well
 */
static bool
remove_vm_area(vm_area_vector_t *v, app_pc start, app_pc end, bool restore_prot)
{
    int i, diff;
    int overlap_start = -1, overlap_end = -1;
    bool add_new_area = false;
    vm_area_t new_area = { 0 }; /* used only when add_new_area, wimpy compiler */
    /* FIXME: cleaner test? shared_data copies flags, but uses
     * custom.frags and not custom.client
     */
    bool official_coarse_vector = (v == executable_areas);

    ASSERT_VMAREA_VECTOR_PROTECTED(v, WRITE);
    LOG(GLOBAL, LOG_VMAREAS, 4, "in remove_vm_area " PFX " " PFX "\n", start, end);
    /* N.B.: removed area could span multiple areas! */
    for (i = 0; i < v->length; i++) {
        /* look for overlap */
        if (start < v->buf[i].end && end > v->buf[i].start) {
            if (overlap_start == -1)
                overlap_start = i;
        } else if (overlap_start > -1) {
            overlap_end = i; /* not inclusive */
            break;
        } else if (end <= v->buf[i].start)
            break;
    }
    if (overlap_start == -1)
        return false;
    if (overlap_end == -1)
        overlap_end = v->length;
    /* since it's sorted and there are no overlaps, we do not have to re-sort.
     * we just delete entire intervals affected, and shorten non-entire
     */
    if (start > v->buf[overlap_start].start) {
        /* need to split? */
        if (overlap_start == overlap_end - 1 && end < v->buf[overlap_start].end) {
            /* don't call add_vm_area now, that will mess up our vector */
            new_area = v->buf[overlap_start]; /* make a copy */
            new_area.start = end;
            /* rest of fields are correct */
            add_new_area = true;
        }
        /* move ending bound backward */
        LOG(GLOBAL, LOG_VMAREAS, 3, "\tchanging " PFX "-" PFX " to " PFX "-" PFX "\n",
            v->buf[overlap_start].start, v->buf[overlap_start].end,
            v->buf[overlap_start].start, start);
        if (restore_prot && DR_MADE_READONLY(v->buf[overlap_start].vm_flags)) {
            vm_make_writable(start, end - start);
        }
        v->buf[overlap_start].end = start;
        /* FIXME: add a vmvector callback function for changing bounds? */
        if (TEST(FRAG_COARSE_GRAIN, v->buf[overlap_start].frag_flags) &&
            official_coarse_vector) {
            adjust_coarse_unit_bounds(&v->buf[overlap_start], false /*leave invalid*/);
        }
        overlap_start++; /* don't delete me */
    }
    if (end < v->buf[overlap_end - 1].end) {
        /* move starting bound forward */
        LOG(GLOBAL, LOG_VMAREAS, 3, "\tchanging " PFX "-" PFX " to " PFX "-" PFX "\n",
            v->buf[overlap_end - 1].start, v->buf[overlap_end - 1].end, end,
            v->buf[overlap_end - 1].end);
        if (restore_prot && DR_MADE_READONLY(v->buf[overlap_end - 1].vm_flags)) {
            vm_make_writable(v->buf[overlap_end - 1].start,
                             end - v->buf[overlap_end - 1].start);
        }
        v->buf[overlap_end - 1].start = end;
        /* FIXME: add a vmvector callback function for changing bounds? */
        if (TEST(FRAG_COARSE_GRAIN, v->buf[overlap_end - 1].frag_flags) &&
            official_coarse_vector) {
            adjust_coarse_unit_bounds(&v->buf[overlap_end - 1], false /*leave invalid*/);
        }
        overlap_end--; /* don't delete me */
    }
    /* now delete */
    if (overlap_start < overlap_end) {
        for (i = overlap_start; i < overlap_end; i++) {
            LOG(GLOBAL, LOG_VMAREAS, 3, "\tcompletely removing " PFX "-" PFX " %s\n",
                v->buf[i].start, v->buf[i].end, v->buf[i].comment);
            if (restore_prot && DR_MADE_READONLY(v->buf[i].vm_flags)) {
                vm_make_writable(v->buf[i].start, v->buf[i].end - v->buf[i].start);
            }
            /* XXX: Better to use a free_payload_func instead of this custom
             * code.  But then we couldn't assert on the bounds and on
             * VM_EXECUTED_FROM.  Could add bounds to callback params, but
             * vm_flags are not exposed to vmvector interface...
             */
            if (TEST(FRAG_COARSE_GRAIN, v->buf[i].frag_flags) && official_coarse_vector) {
                coarse_info_t *info = (coarse_info_t *)v->buf[i].custom.client;
                coarse_info_t *next_info;
                ASSERT(info != NULL);
                ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
                while (info != NULL) { /* loop over primary and secondary unit */
                    ASSERT(info->base_pc >= v->buf[i].start &&
                           info->end_pc <= v->buf[i].end);
                    ASSERT(info->frozen || info->non_frozen == NULL);
                    /* Should have already freed fields (unless we flushed a region
                     * that has not been executed from (case 10995)).
                     * We must delay as we cannot grab change_linking_lock or
                     * special_heap_lock or info->lock while holding exec_areas lock.
                     */
                    if (info->cache != NULL) {
                        ASSERT(info->persisted);
                        ASSERT(!TEST(VM_EXECUTED_FROM, v->buf[i].vm_flags));
                        ASSERT(info->non_frozen != NULL);
                        ASSERT(coarse_to_delete != NULL);
                        /* Both primary and secondary must be un-executed */
                        info->non_frozen->non_frozen = *coarse_to_delete;
                        *coarse_to_delete = info;
                        info = NULL;
                    } else {
                        ASSERT(info->cache == NULL && info->stubs == NULL);
                        next_info = info->non_frozen;
                        coarse_unit_free(GLOBAL_DCONTEXT, info);
                        info = next_info;
                        ASSERT(info == NULL || !info->frozen);
                    }
                }
                v->buf[i].custom.client = NULL;
            }
            if (v->free_payload_func != NULL) {
                v->free_payload_func(v->buf[i].custom.client);
            }
#ifdef DEBUG
            global_heap_free(v->buf[i].comment,
                             strlen(v->buf[i].comment) + 1 HEAPACCT(ACCT_VMAREAS));
#endif
            /* frags list should always be null here (flush should have happened,
             * etc.) */
            ASSERT(!TEST(VECTOR_FRAGMENT_LIST, v->flags) ||
                   v->buf[i].custom.frags == NULL);
        }
        diff = overlap_end - overlap_start;
        for (i = overlap_start; i < v->length - diff; i++)
            v->buf[i] = v->buf[i + diff];
#ifdef DEBUG
        memset(v->buf + v->length - diff, 0, diff * sizeof(vm_area_t));
#endif
        v->length -= diff;
    }
    if (add_new_area) {
        /* Case 8640: Do not propagate coarse-grain-ness to split-off region,
         * for now only for simplicity.  FIXME: come up with better policy.  We
         * do keep it on original part of split region.  FIXME: assert that
         * there the unit is fully flushed.  Better to remove in
         * vm_area_allsynch_flush_fragments() and then re-add if warranted?
         */
        new_area.frag_flags &= ~FRAG_COARSE_GRAIN;
        /* With flush of partial module region w/o remove (e.g., from
         * -unsafe_ignore_IAT_writes) we can have VM_ADD_TO_SHARED_DATA set
         */
        new_area.vm_flags &= ~VM_ADD_TO_SHARED_DATA;
        LOG(GLOBAL, LOG_VMAREAS, 3, "\tadding " PFX "-" PFX "\n", new_area.start,
            new_area.end);
        /* we copied v->buf[overlap_start] above and so already have a copy
         * of the client field
         */
        if (v->split_payload_func != NULL) {
            new_area.custom.client = v->split_payload_func(new_area.custom.client);
        } /* else, just keep the copy */
        add_vm_area(v, new_area.start, new_area.end, new_area.vm_flags,
                    new_area.frag_flags,
                    new_area.custom.client _IF_DEBUG(new_area.comment));
    }
    DOLOG(5, LOG_VMAREAS, { print_vm_areas(v, GLOBAL); });
    return true;
}

/* Returns true if start..end overlaps any area in v.
 * If end==NULL, assumes that end is very top of address space (wraparound).
 * If area!=NULL, sets *area to an overlapping area in v
 *   If index!=NULL, sets *index to the vector index of area; if no match
 *   is found, sets *index to the index before [start,end) (may be -1).
 *   If first, makes sure *area is the 1st overlapping area
 * Assumes caller holds v->lock, if necessary
 * N.B.: the pointer returned by this routine is volatile!  Only use it while
 * you have exclusive control over the vector v, either by holding its lock
 * or by being its owning thread if it has no lock.
 */
static bool
binary_search(vm_area_vector_t *v, app_pc start, app_pc end, vm_area_t **area /*OUT*/,
              int *index /*OUT*/, bool first)
{
    /* BINARY SEARCH -- assumes the vector is kept sorted by add & remove! */
    int min = 0;
    int max = v->length - 1;

    /* We support an empty range start==end in general but we do
     * complain about 0..0 to catch bugs like i#4097.
     */
    ASSERT(start != NULL || end != NULL);
    ASSERT(start <= end || end == NULL /* wraparound */);

    ASSERT_VMAREA_VECTOR_PROTECTED(v, READWRITE);
    LOG(GLOBAL, LOG_VMAREAS, 7, "Binary search for " PFX "-" PFX " on this vector:\n",
        start, end);
    DOLOG(7, LOG_VMAREAS, { print_vm_areas(v, GLOBAL); });
    /* binary search */
    while (max >= min) {
        int i = (min + max) / 2;
        if (end != NULL && end <= v->buf[i].start)
            max = i - 1;
        else if (start >= v->buf[i].end || start == end)
            min = i + 1;
        else {
            if (area != NULL || index != NULL) {
                if (first) {
                    /* caller wants 1st matching area */
                    for (; i >= 1 && v->buf[i - 1].end > start; i--)
                        ;
                }
                /* returning pointer to volatile array dangerous -- see comment above */
                if (area != NULL)
                    *area = &(v->buf[i]);
                if (index != NULL)
                    *index = i;
            }
            LOG(GLOBAL, LOG_VMAREAS, 7,
                "\tfound " PFX "-" PFX " in area " PFX "-" PFX "\n", start, end,
                v->buf[i].start, v->buf[i].end);
            return true;
        }
    }
    /* now max < min */
    LOG(GLOBAL, LOG_VMAREAS, 7, "\tdid not find " PFX "-" PFX "!\n", start, end);
    if (index != NULL) {
        ASSERT((max < 0 || v->buf[max].end <= start || start == end) &&
               (min > v->length - 1 || v->buf[min].start >= end));
        *index = max;
    }
    return false;
}

/* lookup an addr in the current area
 * RETURN true if address area is found, false otherwise
 * if area is non NULL it is set to the area found
 * Assumes caller holds v->lock, if necessary
 * N.B.: the pointer returned by this routine is volatile!  Only use it while
 * you have exclusive control over the vector v, either by holding its lock
 * or by being its owning thread if it has no lock.
 */
/* FIXME: change lookup_addr to two routines, one for readers which
 * returns a copy, and the other for writers who must hold a lock
 * across all uses of the pointer
 */
static bool
lookup_addr(vm_area_vector_t *v, app_pc addr, vm_area_t **area)
{
    /* binary search asserts v is protected */
    return binary_search(v, addr, addr + 1 /*open end*/, area, NULL, false);
}

/* returns true if the passed in area overlaps any known executable areas
 * Assumes caller holds v->lock, if necessary
 */
static bool
vm_area_overlap(vm_area_vector_t *v, app_pc start, app_pc end)
{
    /* binary search asserts v is protected */
    return binary_search(v, start, end, NULL, NULL, false);
}

/*********************** EXPORTED ROUTINES **********************/

/* thread-shared initialization that should be repeated after a reset */
void
vm_areas_reset_init(void)
{
    memset(shared_data, 0, sizeof(*shared_data));
    VMVECTOR_INITIALIZE_VECTOR(&shared_data->areas, VECTOR_SHARED | VECTOR_FRAGMENT_LIST,
                               shared_vm_areas);
}

void
dynamo_vm_areas_init()
{
    VMVECTOR_ALLOC_VECTOR(dynamo_areas, GLOBAL_DCONTEXT, VECTOR_SHARED, dynamo_areas);
}

void
dynamo_vm_areas_exit()
{
    vmvector_delete_vector(GLOBAL_DCONTEXT, dynamo_areas);
    dynamo_areas = NULL;
}

/* calls find_executable_vm_areas to get per-process map
 * N.B.: add_dynamo_vm_area can be called before this init routine!
 * N.B.: this is called after vm_areas_thread_init()
 */
int
vm_areas_init()
{
    int areas;

    /* Case 7957: we allocate all vm vectors on the heap for self-prot reasons.
     * We're already paying the indirection cost by passing their addresses
     * to generic routines, after all.
     */
    VMVECTOR_ALLOC_VECTOR(executable_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          executable_areas);
    VMVECTOR_ALLOC_VECTOR(pretend_writable_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          pretend_writable_areas);
    VMVECTOR_ALLOC_VECTOR(patch_proof_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          patch_proof_areas);
    VMVECTOR_ALLOC_VECTOR(emulate_write_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          emulate_write_areas);
    VMVECTOR_ALLOC_VECTOR(IAT_areas, GLOBAL_DCONTEXT, VECTOR_SHARED, IAT_areas);
    VMVECTOR_ALLOC_VECTOR(written_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE, written_areas);
    vmvector_set_callbacks(written_areas, free_written_area, NULL, NULL, NULL);
#ifdef PROGRAM_SHEPHERDING
    VMVECTOR_ALLOC_VECTOR(futureexec_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          futureexec_areas);
#    ifdef WINDOWS
    VMVECTOR_ALLOC_VECTOR(app_flushed_areas, GLOBAL_DCONTEXT, VECTOR_SHARED,
                          app_flushed_areas);
#    endif
#endif

    shared_data =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, thread_data_t, ACCT_VMAREAS, PROTECTED);

    todelete =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, deletion_lists_t, ACCT_VMAREAS, PROTECTED);
    memset(todelete, 0, sizeof(*todelete));

    coarse_to_delete =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, coarse_info_t *, ACCT_VMAREAS, PROTECTED);
    *coarse_to_delete = NULL;

    if (DYNAMO_OPTION(unloaded_target_exception)) {
        last_deallocated =
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, last_deallocated_t, ACCT_VMAREAS, PROTECTED);
        memset(last_deallocated, 0, sizeof(*last_deallocated));
    } else
        ASSERT(last_deallocated == NULL);

    vm_areas_reset_init();

    /* initialize dynamo list first */
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "\n------------------------------------------------------------------------\n");
    dynamo_vm_areas_lock();
    areas = find_dynamo_library_vm_areas();
    dynamo_vm_areas_unlock();

    /* initialize executable list
     * this routine calls app_memory_allocation() w/ dcontext==NULL and so we
     * won't go adding rwx regions, like the linux stack, to our list, even w/
     * -executable_if_alloc
     */
    areas = find_executable_vm_areas();
    DOLOG(1, LOG_VMAREAS, {
        if (areas > 0) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "\nExecution is allowed in %d areas\n", areas);
            print_executable_areas(GLOBAL);
        }
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "------------------------------------------------------------------------\n");
    });

    return areas;
}

static void
vm_areas_statistics()
{
#ifdef PROGRAM_SHEPHERDING
    DOLOG(1, LOG_VMAREAS | LOG_STATS, {
        uint top;
        uint bottom;
        divide_uint64_print(GLOBAL_STAT(looked_up_in_last_area),
                            GLOBAL_STAT(checked_addresses), true, 2, &top, &bottom);
        LOG(GLOBAL, LOG_VMAREAS | LOG_STATS, 1,
            "Code Origin: %d address lookups, %d in last area, hit ratio %u.%.2u\n",
            GLOBAL_STAT(checked_addresses), GLOBAL_STAT(looked_up_in_last_area), top,
            bottom);
    });
#endif /* PROGRAM_SHEPHERDING */
    DOLOG(1, LOG_VMAREAS, {
        LOG(GLOBAL, LOG_VMAREAS, 1, "\nexecutable_areas at exit:\n");
        print_executable_areas(GLOBAL);
    });
}

/* Free all thread-shared state not critical to forward progress;
 * vm_areas_reset_init() will be called before continuing.
 */
void
vm_areas_reset_free(void)
{
    if (SHARED_FRAGMENTS_ENABLED()) {
        /* all deletion entries should be removed in fragment_exit(),
         * else we'd have to free the frags lists and entries here
         */
        ASSERT(todelete->shared_delete == NULL);
        ASSERT(todelete->shared_delete_tail == NULL);
        /* FIXME: don't free lock so init has less work */
        vmvector_free_vector(GLOBAL_DCONTEXT, &shared_data->areas);
    }
    /* vm_area_coarse_units_reset_free() is called in fragment_reset_free() */
}

int
vm_areas_exit()
{
    vm_areas_exited = true;
    vm_areas_statistics();

    if (DYNAMO_OPTION(thin_client)) {
        vmvector_delete_vector(GLOBAL_DCONTEXT, dynamo_areas);
        dynamo_areas = NULL;
        /* For thin_client none of the following areas should have been
         * initialized because they aren't used.
         * FIXME: wonder if I can do something like this for -client and see
         * what I am using unnecessarily.
         */
        ASSERT(shared_data == NULL);
        ASSERT(todelete == NULL);
        ASSERT(executable_areas == NULL);
        ASSERT(pretend_writable_areas == NULL);
        ASSERT(patch_proof_areas == NULL);
        ASSERT(emulate_write_areas == NULL);
        ASSERT(written_areas == NULL);
#ifdef PROGRAM_SHEPHERDING
        ASSERT(futureexec_areas == NULL);
        IF_WINDOWS(ASSERT(app_flushed_areas == NULL);)
#endif
        ASSERT(IAT_areas == NULL);
        return 0;
    }

    vm_areas_reset_free();
    DELETE_LOCK(shared_delete_lock);
    DELETE_LOCK(lazy_delete_lock);
    ASSERT(todelete->lazy_delete_count == 0);
    ASSERT(!todelete->move_pending);

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, shared_data, thread_data_t, ACCT_VMAREAS, PROTECTED);
    shared_data = NULL;

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, todelete, deletion_lists_t, ACCT_VMAREAS, PROTECTED);
    todelete = NULL;

    ASSERT(coarse_to_delete != NULL);
    /* should be freed immediately after each use, during a no-exec flush */
    ASSERT(*coarse_to_delete == NULL);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, coarse_to_delete, coarse_info_t *, ACCT_VMAREAS,
                   PROTECTED);

    if (DYNAMO_OPTION(unloaded_target_exception)) {
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, last_deallocated, last_deallocated_t,
                       ACCT_VMAREAS, PROTECTED);
        last_deallocated = NULL;
    } else
        ASSERT(last_deallocated == NULL);
    DELETE_LOCK(last_deallocated_lock);

    vmvector_delete_vector(GLOBAL_DCONTEXT, executable_areas);
    executable_areas = NULL;
    DOLOG(1, LOG_VMAREAS, {
        if (dynamo_areas->buf != NULL) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "DR regions at exit are:\n");
            print_dynamo_areas(GLOBAL);
            LOG(GLOBAL, LOG_VMAREAS, 1, "\n");
        }
    });
    dynamo_vm_areas_exit();
    DOLOG(1, LOG_VMAREAS, {
        if (written_areas->buf != NULL) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "Code write and selfmod exec counts:\n");
            print_written_areas(GLOBAL);
            LOG(GLOBAL, LOG_VMAREAS, 1, "\n");
        }
    });
    vmvector_delete_vector(GLOBAL_DCONTEXT, pretend_writable_areas);
    pretend_writable_areas = NULL;
    vmvector_delete_vector(GLOBAL_DCONTEXT, patch_proof_areas);
    patch_proof_areas = NULL;
    vmvector_delete_vector(GLOBAL_DCONTEXT, emulate_write_areas);
    emulate_write_areas = NULL;

    vmvector_delete_vector(GLOBAL_DCONTEXT, written_areas);
    written_areas = NULL;

#ifdef PROGRAM_SHEPHERDING
    DOLOG(1, LOG_VMAREAS, {
        if (futureexec_areas->buf != NULL) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "futureexec %d regions at exit are:\n",
                futureexec_areas->length);
        }
        print_futureexec_areas(GLOBAL);
    });
    vmvector_delete_vector(GLOBAL_DCONTEXT, futureexec_areas);
    futureexec_areas = NULL;
    DELETE_LOCK(threads_killed_lock);
#    ifdef WINDOWS
    ASSERT(DYNAMO_OPTION(xdata_rct) || vmvector_empty(app_flushed_areas));
    vmvector_delete_vector(GLOBAL_DCONTEXT, app_flushed_areas);
    app_flushed_areas = NULL;
#    endif
#endif
#ifdef SIMULATE_ATTACK
    DELETE_LOCK(simulate_lock);
#endif
    vmvector_delete_vector(GLOBAL_DCONTEXT, IAT_areas);
    IAT_areas = NULL;

    tamper_resistant_region_start = NULL;
    tamper_resistant_region_end = NULL;

    return 0;
}

void
vm_areas_post_exit()
{
    vm_areas_exited = false;
}

void
vm_areas_thread_reset_init(dcontext_t *dcontext)
{
    thread_data_t *data = (thread_data_t *)dcontext->vm_areas_field;
    memset(dcontext->vm_areas_field, 0, sizeof(thread_data_t));
    VMVECTOR_INITIALIZE_VECTOR(&data->areas, VECTOR_FRAGMENT_LIST, thread_vm_areas);
    /* data->areas.lock is never used, but we may want to grab it one day,
       e.g. to print other thread areas */
}

/* N.B.: this is called before vm_areas_init() */
void
vm_areas_thread_init(dcontext_t *dcontext)
{
    thread_data_t *data = HEAP_TYPE_ALLOC(dcontext, thread_data_t, ACCT_OTHER, PROTECTED);
    dcontext->vm_areas_field = data;
    vm_areas_thread_reset_init(dcontext);
}

void
vm_areas_thread_reset_free(dcontext_t *dcontext)
{
    /* we free the local areas vector so it will match fragments post-reset
     * FIXME: put it in nonpersistent heap
     */
    thread_data_t *data = (thread_data_t *)dcontext->vm_areas_field;
    /* yes, we end up using global heap for the thread-local area
     * vector...not a big deal, but FIXME sometime
     */
    vmvector_free_vector(GLOBAL_DCONTEXT, &data->areas);
}

void
vm_areas_thread_exit(dcontext_t *dcontext)
{
    vm_areas_thread_reset_free(dcontext);
#ifdef DEBUG
    /* for non-debug we do fast exit path and don't free local heap */
    HEAP_TYPE_FREE(dcontext, dcontext->vm_areas_field, thread_data_t, ACCT_OTHER,
                   PROTECTED);
#endif
}

/****************************************************************************
 * external interface to vm_area_vector_t
 *
 * FIXME: add user data field to vector and to add routine
 * FIXME: have init and destroy routines so don't have to expose
 *        vm_area_vector_t struct or declare vector in this file
 */

void
vmvector_set_callbacks(vm_area_vector_t *v, void (*free_func)(void *),
                       void *(*split_func)(void *),
                       bool (*should_merge_func)(bool, void *, void *),
                       void *(*merge_func)(void *, void *))
{
    bool release_lock; /* 'true' means this routine needs to unlock */
    ASSERT(v != NULL);
    LOCK_VECTOR(v, release_lock, read);
    v->free_payload_func = free_func;
    v->split_payload_func = split_func;
    v->should_merge_func = should_merge_func;
    v->merge_payload_func = merge_func;
    UNLOCK_VECTOR(v, release_lock, read);
}

void
vmvector_print(vm_area_vector_t *v, file_t outf)
{
    bool release_lock; /* 'true' means this routine needs to unlock */
    LOCK_VECTOR(v, release_lock, read);
    print_vm_areas(v, outf);
    UNLOCK_VECTOR(v, release_lock, read);
}

void
vmvector_add(vm_area_vector_t *v, app_pc start, app_pc end, void *data)
{
    bool release_lock; /* 'true' means this routine needs to unlock */
    LOCK_VECTOR(v, release_lock, write);
    ASSERT_OWN_WRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    add_vm_area(v, start, end, 0, 0, data _IF_DEBUG(""));
    UNLOCK_VECTOR(v, release_lock, write);
}

void *
vmvector_add_replace(vm_area_vector_t *v, app_pc start, app_pc end, void *data)
{
    bool overlap;
    vm_area_t *area = NULL;
    void *old_data = NULL;
    bool release_lock; /* 'true' means this routine needs to unlock */

    LOCK_VECTOR(v, release_lock, write);
    ASSERT_OWN_WRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    overlap = lookup_addr(v, start, &area);
    if (overlap && start == area->start && end == area->end) {
        old_data = area->custom.client;
        area->custom.client = data;
    } else
        add_vm_area(v, start, end, 0, 0, data _IF_DEBUG(""));
    UNLOCK_VECTOR(v, release_lock, write);
    return old_data;
}

bool
vmvector_remove(vm_area_vector_t *v, app_pc start, app_pc end)
{
    bool ok;
    bool release_lock; /* 'true' means this routine needs to unlock */
    LOCK_VECTOR(v, release_lock, write);
    ASSERT_OWN_WRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    ok = remove_vm_area(v, start, end, false);
    UNLOCK_VECTOR(v, release_lock, write);
    return ok;
}

/* Looks up area encapsulating target pc and removes.
 * returns true if found and removed, and optional area boundaries are set
 * returns false if not found
 */
bool
vmvector_remove_containing_area(vm_area_vector_t *v, app_pc pc,
                                app_pc *area_start /* OUT optional */,
                                app_pc *area_end /* OUT optional */)
{
    vm_area_t *a;
    bool ok;
    bool release_lock; /* 'true' means this routine needs to unlock */

    /* common path should be to find one, and would need write lock to
     * remove */
    LOCK_VECTOR(v, release_lock, write);
    ASSERT_OWN_WRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    ok = lookup_addr(v, pc, &a);
    if (ok) {
        if (area_start != NULL)
            *area_start = a->start;
        if (area_end != NULL)
            *area_end = a->end;
        remove_vm_area(v, a->start, a->end, false);
    }
    UNLOCK_VECTOR(v, release_lock, write);
    return ok;
}

bool
vmvector_overlap(vm_area_vector_t *v, app_pc start, app_pc end)
{
    bool overlap;
    bool release_lock; /* 'true' means this routine needs to unlock */
    if (vmvector_empty(v))
        return false;
    LOCK_VECTOR(v, release_lock, read);
    ASSERT_OWN_READWRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    overlap = vm_area_overlap(v, start, end);
    UNLOCK_VECTOR(v, release_lock, read);
    return overlap;
}

/* returns custom data field, or NULL if not found.  NOTE: Access to
 * custom data needs explicit synchronization in addition to
 * vm_area_vector_t's locks!
 */
void *
vmvector_lookup(vm_area_vector_t *v, app_pc pc)
{
    void *data = NULL;
    vmvector_lookup_data(v, pc, NULL, NULL, &data);
    return data;
}

/* Looks up if pc is in a vmarea and optionally returns the areas's bounds
 * and any custom data. NOTE: Access to custom data needs explicit
 * synchronization in addition to vm_area_vector_t's locks!
 */
bool
vmvector_lookup_data(vm_area_vector_t *v, app_pc pc, app_pc *start /* OUT */,
                     app_pc *end /* OUT */, void **data /* OUT */)
{
    bool overlap;
    vm_area_t *area = NULL;
    bool release_lock; /* 'true' means this routine needs to unlock */

    LOCK_VECTOR(v, release_lock, read);
    ASSERT_OWN_READWRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    overlap = lookup_addr(v, pc, &area);
    if (overlap) {
        if (start != NULL)
            *start = area->start;
        if (end != NULL)
            *end = area->end;
        if (data != NULL)
            *data = area->custom.client;
    }
    UNLOCK_VECTOR(v, release_lock, read);
    return overlap;
}

/* Returns false if pc is in a vmarea in v.
 * Otherwise, returns the start pc of the vmarea prior to pc in prev and
 * the start pc of the vmarea after pc in next.
 * FIXME: most callers will call this and vmvector_lookup_data():
 * should this routine do both to avoid an extra binary search?
 */
bool
vmvector_lookup_prev_next(vm_area_vector_t *v, app_pc pc, OUT app_pc *prev_start,
                          OUT app_pc *prev_end, OUT app_pc *next_start,
                          OUT app_pc *next_end)
{
    bool success;
    int index;
    bool release_lock; /* 'true' means this routine needs to unlock */

    LOCK_VECTOR(v, release_lock, read);
    ASSERT_OWN_READWRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    success = !binary_search(v, pc, pc + 1, NULL, &index, false);
    if (success) {
        if (index == -1) {
            if (prev_start != NULL)
                *prev_start = NULL;
            if (prev_end != NULL)
                *prev_end = NULL;
        } else {
            if (prev_start != NULL)
                *prev_start = v->buf[index].start;
            if (prev_end != NULL)
                *prev_end = v->buf[index].end;
        }
        if (index >= v->length - 1) {
            if (next_start != NULL)
                *next_start = (app_pc)POINTER_MAX;
            if (next_end != NULL)
                *next_end = (app_pc)POINTER_MAX;
        } else {
            if (next_start != NULL)
                *next_start = v->buf[index + 1].start;
            if (next_end != NULL)
                *next_end = v->buf[index + 1].end;
        }
    }
    UNLOCK_VECTOR(v, release_lock, read);
    return success;
}

/* Sets custom data field if a vmarea is present. Returns true if found,
 * false if not found.  NOTE: Access to custom data needs explicit
 * synchronization in addition to vm_area_vector_t's locks!
 */
bool
vmvector_modify_data(vm_area_vector_t *v, app_pc start, app_pc end, void *data)
{
    bool overlap;
    vm_area_t *area = NULL;
    bool release_lock; /* 'true' means this routine needs to unlock */

    LOCK_VECTOR(v, release_lock, write);
    ASSERT_OWN_WRITE_LOCK(SHOULD_LOCK_VECTOR(v), &v->lock);
    overlap = lookup_addr(v, start, &area);
    if (overlap && start == area->start && end == area->end)
        area->custom.client = data;
    UNLOCK_VECTOR(v, release_lock, write);
    return overlap;
}

/* this routine does NOT initialize the rw lock!  use VMVECTOR_INITIALIZE_VECTOR */
void
vmvector_init_vector(vm_area_vector_t *v, uint flags)
{
    memset(v, 0, sizeof(*v));
    v->flags = flags;
}

/* this routine does NOT initialize the rw lock!  use VMVECTOR_ALLOC_VECTOR instead */
vm_area_vector_t *
vmvector_create_vector(dcontext_t *dcontext, uint flags)
{
    vm_area_vector_t *v =
        HEAP_TYPE_ALLOC(dcontext, vm_area_vector_t, ACCT_VMAREAS, PROTECTED);
    vmvector_init_vector(v, flags);
    return v;
}

/* frees the fields of vm_area_vector_t v (not v itself) */
void
vmvector_reset_vector(dcontext_t *dcontext, vm_area_vector_t *v)
{
    DODEBUG({
        int i;
        /* walk areas and delete coarse info and comments */
        for (i = 0; i < v->length; i++) {
            /* FIXME: this code is duplicated in remove_vm_area() */
            if (TEST(FRAG_COARSE_GRAIN, v->buf[i].frag_flags) &&
                /* FIXME: cleaner test? shared_data copies flags, but uses
                 * custom.frags and not custom.client
                 */
                v == executable_areas) {
                coarse_info_t *info = (coarse_info_t *)v->buf[i].custom.client;
                coarse_info_t *next_info;
                ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
                ASSERT(info != NULL);
                while (info != NULL) { /* loop over primary and secondary unit */
                    next_info = info->non_frozen;
                    ASSERT(info->frozen || info->non_frozen == NULL);
                    coarse_unit_free(GLOBAL_DCONTEXT, info);
                    info = next_info;
                    ASSERT(info == NULL || !info->frozen);
                }
                v->buf[i].custom.client = NULL;
            }
            global_heap_free(v->buf[i].comment,
                             strlen(v->buf[i].comment) + 1 HEAPACCT(ACCT_VMAREAS));
        }
    });
    /* with thread shared cache it is in fact possible to have no thread local vmareas */
    if (v->buf != NULL) {
        if (v->free_payload_func != NULL) {
            int i;
            for (i = 0; i < v->length; i++) {
                v->free_payload_func(v->buf[i].custom.client);
            }
        }
        /* FIXME: walk through and make sure frags lists are all freed */
        global_heap_free(v->buf,
                         v->size * sizeof(struct vm_area_t) HEAPACCT(ACCT_VMAREAS));
        v->size = 0;
        v->length = 0;
        v->buf = NULL;
    } else
        ASSERT(v->size == 0 && v->length == 0);
}

static void
vmvector_free_vector(dcontext_t *dcontext, vm_area_vector_t *v)
{
    vmvector_reset_vector(dcontext, v);
    if (!TEST(VECTOR_NO_LOCK, v->flags))
        DELETE_READWRITE_LOCK(v->lock);
}

/* frees the vm_area_vector_t v and its associated memory */
void
vmvector_delete_vector(dcontext_t *dcontext, vm_area_vector_t *v)
{
    vmvector_free_vector(dcontext, v);
    HEAP_TYPE_FREE(dcontext, v, vm_area_vector_t, ACCT_VMAREAS, PROTECTED);
}

/* vmvector iterator */
/* initialize an iterator, has to be released with
 * vmvector_iterator_stop.  The iterator doesn't support mutations.
 * In fact shared vectors should detect a deadlock
 * if vmvector_add() and vmvector_remove() is erroneously called.
 */
void
vmvector_iterator_start(vm_area_vector_t *v, vmvector_iterator_t *vmvi)
{
    ASSERT(v != NULL);
    ASSERT(vmvi != NULL);
    if (SHOULD_LOCK_VECTOR(v))
        d_r_read_lock(&v->lock);
    vmvi->vector = v;
    vmvi->index = -1;
}

bool
vmvector_iterator_hasnext(vmvector_iterator_t *vmvi)
{
    ASSERT_VMAREA_VECTOR_PROTECTED(vmvi->vector, READWRITE);
    return (vmvi->index + 1) < vmvi->vector->length;
}

void
vmvector_iterator_startover(vmvector_iterator_t *vmvi)
{
    ASSERT_VMAREA_VECTOR_PROTECTED(vmvi->vector, READWRITE);
    vmvi->index = -1;
}

/* iterator accessor
 * has to be initialized with vmvector_iterator_start, and should be
 * called only when vmvector_iterator_hasnext() is true
 *
 * returns custom data and
 * sets the area boundaries in area_start and area_end
 *
 * does not increment the iterator
 */
void *
vmvector_iterator_peek(vmvector_iterator_t *vmvi, /* IN/OUT */
                       app_pc *area_start /* OUT */, app_pc *area_end /* OUT */)
{
    int idx = vmvi->index + 1;
    ASSERT(vmvector_iterator_hasnext(vmvi));
    ASSERT_VMAREA_VECTOR_PROTECTED(vmvi->vector, READWRITE);
    ASSERT(idx < vmvi->vector->length);
    if (area_start != NULL)
        *area_start = vmvi->vector->buf[idx].start;
    if (area_end != NULL)
        *area_end = vmvi->vector->buf[idx].end;
    return vmvi->vector->buf[idx].custom.client;
}

/* iterator accessor
 * has to be initialized with vmvector_iterator_start, and should be
 * called only when vmvector_iterator_hasnext() is true
 *
 * returns custom data and
 * sets the area boundaries in area_start and area_end
 */
void *
vmvector_iterator_next(vmvector_iterator_t *vmvi, /* IN/OUT */
                       app_pc *area_start /* OUT */, app_pc *area_end /* OUT */)
{
    void *res = vmvector_iterator_peek(vmvi, area_start, area_end);
    vmvi->index++;
    return res;
}

void
vmvector_iterator_stop(vmvector_iterator_t *vmvi)
{
    ASSERT_VMAREA_VECTOR_PROTECTED(vmvi->vector, READWRITE);
    if (SHOULD_LOCK_VECTOR(vmvi->vector))
        d_r_read_unlock(&vmvi->vector->lock);
    DODEBUG({
        vmvi->vector = NULL; /* crash incorrect reuse */
        vmvi->index = -1;
    });
}

/****************************************************************************
 * routines specific to our own vectors
 */

void
print_executable_areas(file_t outf)
{
    vmvector_print(executable_areas, outf);
}

void
print_dynamo_areas(file_t outf)
{
    dynamo_vm_areas_start_reading();
    print_vm_areas(dynamo_areas, outf);
    dynamo_vm_areas_done_reading();
}

#ifdef PROGRAM_SHEPHERDING
void
print_futureexec_areas(file_t outf)
{
    vmvector_print(futureexec_areas, outf);
}
#endif

#if defined(DEBUG) && defined(INTERNAL)
static void
print_written_areas(file_t outf)
{
    vmvector_print(written_areas, outf);
}
#endif

static void
free_written_area(void *data)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, (ro_vs_sandbox_data_t *)data, ro_vs_sandbox_data_t,
                   ACCT_VMAREAS, UNPROTECTED);
}

/* Functions as a lookup routine if an entry is already present.
 * Returns true if an entry was already present, false if not, in which
 * case an entry containing tag with suggested bounds of [start, end)
 * (actual bounds may be smaller to avoid overlap) is added.
 */
static bool
add_written_area(vm_area_vector_t *v, app_pc tag, app_pc start, app_pc end,
                 vm_area_t **area)
{
    vm_area_t *a = NULL;
    bool already;
    DEBUG_DECLARE(bool ok;)
    /* currently only one vector */
    ASSERT(v == written_areas);
    ASSERT_OWN_WRITE_LOCK(true, &v->lock);
    ASSERT(tag >= start && tag < end);
    /* re-adding fails for written_areas since no merging, so lookup first */
    already = lookup_addr(v, tag, &a);
    if (!already) {
        app_pc prev_start = NULL, prev_end = NULL, next_start = NULL;
        LOG(GLOBAL, LOG_VMAREAS, 2, "new written executable vm area: " PFX "-" PFX "\n",
            start, end);
        /* case 9179: With no flags, any overlap (in non-tag portion of [start,
         * end)) will result in a merge: so we'll inherit and share counts from
         * any adjacent region(s): maybe better to split?  Rare in any case and
         * not critical.  In case of simultaneous overlap, we take counter from
         * first region, since that's how add_vm_area does the merge.
         */
        /* we can't merge b/c we have hardcoded counter pointers in code
         * in the cache, so we make sure to only add the non-overlap
         */
        DEBUG_DECLARE(ok =)
        vmvector_lookup_prev_next(v, tag, &prev_start, &prev_end, &next_start, NULL);
        ASSERT(ok); /* else already should be true */
        if (prev_start != NULL) {
            if (prev_end > start)
                start = prev_end;
        }
        if (next_start < (app_pc)POINTER_MAX && end > next_start)
            end = next_start;
        add_vm_area(v, start, end, /* no flags */ 0, 0, NULL _IF_DEBUG(""));
        DEBUG_DECLARE(ok =) lookup_addr(v, tag, &a);
        ASSERT(ok && a != NULL);
        /* If we merged, we already have an ro2s struct */
        /* FIXME: now that we have merge callback support, should just pass
         * a struct into add_vm_area and avoid this post-lookup
         */
        if (a->custom.client == NULL) {
            /* Since selfmod_execs is written from the cache this must be
             * unprotected.  Attacker changing selfmod_execs or written_count
             * shouldn't be able to cause problems.
             */
            ro_vs_sandbox_data_t *ro2s = HEAP_TYPE_ALLOC(
                GLOBAL_DCONTEXT, ro_vs_sandbox_data_t, ACCT_VMAREAS, UNPROTECTED);
            /* selfmod_execs is inc-ed from the cache, and if it crosses a cache
             * line we could have a problem with large thresholds.  We assert on
             * 32-bit alignment here, which our heap alloc currently provides, to
             * ensure no cache line is crossed.
             */
            ASSERT(ALIGNED(ro2s, sizeof(uint)));
            memset(ro2s, 0, sizeof(*ro2s));
            a->custom.client = (void *)ro2s;
        }
    } else {
        LOG(GLOBAL, LOG_VMAREAS, 3,
            "request for written area " PFX "-" PFX " vs existing " PFX "-" PFX "\n",
            start, end, a->start, a->end);
    }
    ASSERT(a != NULL);
    if (area != NULL)
        *area = a;
    return already;
}

#ifdef WINDOWS
/* Adjusts a new executable area with respect to the IAT.
 * Returns whether it should remain coarse or not.
 */
static bool
add_executable_vm_area_check_IAT(app_pc *start /*IN/OUT*/, app_pc *end /*IN/OUT*/,
                                 uint vm_flags, vm_area_t **existing_area /*OUT*/,
                                 coarse_info_t **info_out /*OUT*/,
                                 coarse_info_t **tofree /*OUT*/,
                                 app_pc *delay_start /*OUT*/, app_pc *delay_end /*OUT*/)
{
    bool keep_coarse = false;
    app_pc IAT_start = NULL, IAT_end = NULL;
    app_pc orig_start = *start, orig_end = *end;
    ASSERT(existing_area != NULL && info_out != NULL && tofree != NULL);
    ASSERT(delay_start != NULL && delay_end != NULL);
    if (DYNAMO_OPTION(coarse_merge_iat) && get_module_base(*start) != NULL &&
        get_IAT_section_bounds(get_module_base(*start), &IAT_start, &IAT_end) &&
        /* case 1094{5,7}: to match the assumptions of case 10600 we adjust
         * to post-IAT even if the IAT is in the middle, if it's toward the front
         */
        (*start >= IAT_start || (IAT_start - *start < *end - IAT_end)) &&
        *start<IAT_end &&
               /* be paranoid: multi-page IAT where hooker fooled our loader matching
                * could add just 1st page of IAT? */
               * end>
            IAT_end /* for == avoid an empty region */) {
        /* If a pre-IAT region exists, split if off separately (case 10945).
         * We want to keep as coarse, but we need the post-IAT region to be the
         * primary coarse and the one we try to load a pcache for: so we delay
         * the add.
         * FIXME: should we do a general split around the IAT and make both sides
         * coarse with larger the primary instead of assuming pre-IAT is smaller?
         */
        if (orig_start < IAT_start) {
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "splitting pre-IAT " PFX "-" PFX " off from exec area " PFX "-" PFX "\n",
                orig_start, IAT_start, orig_start, orig_end);
            *delay_start = orig_start;
            *delay_end = IAT_start;
            DOCHECK(1, {
                /* When IAT is in the middle of +rx region we expect .orpc */
                app_pc orpc_start = NULL;
                app_pc orpc_end = NULL;
                get_named_section_bounds(get_module_base(orig_start), ".orpc",
                                         &orpc_start, &orpc_end);
                ASSERT_CURIOSITY(orpc_start == orig_start && orpc_end == IAT_start);
            });
        }
        /* Just abandon [*start, IAT_end) */
        *start = IAT_end;
        ASSERT(*end > *start);
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "adjusting exec area " PFX "-" PFX " to post-IAT " PFX "-" PFX "\n",
            orig_start, *end, *start, *end);
    } else {
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "NOT adjusting exec area " PFX "-" PFX " vs IAT " PFX "-" PFX "\n",
            orig_start, *end, IAT_start, IAT_end);
    }
    if (TEST(VM_UNMOD_IMAGE, vm_flags))
        keep_coarse = true;
    else {
        /* Keep the coarse-grain flag for modified pages only if IAT pages.
         * We want to avoid repeated coarse flushes, so we are
         * very conservative about marking if_rx_text regions coarse: we count on
         * our IAT loader check to make this a do-once.
         * FIXME: Should extend this to also merge on left with .orpc:
         * .orpc at page 1, IAT on page 2, and .text continuing on
         */
        ASSERT(ALIGNED(*end, PAGE_SIZE));
        if (DYNAMO_OPTION(coarse_merge_iat) && vm_flags == 0 /* no other flags */ &&
            /* FIXME: used our stored bounds */
            is_IAT(orig_start, orig_end, true /*page-align*/, NULL, NULL) &&
            is_module_patch_region(GLOBAL_DCONTEXT, orig_start, orig_end,
                                   true /*be conservative*/) &&
            /* We stored the IAT code at +rw time */
            os_module_cmp_IAT_code(orig_start)) {
            vm_area_t *area = NULL;
            bool all_new =
                !executable_vm_area_overlap(orig_start, orig_end - 1, true /*wlock*/);
            ASSERT(IAT_start != NULL); /* should have found bounds above */
            if (all_new &&             /* elseif assumes next call happened */
                lookup_addr(executable_areas, *end, &area) &&
                TEST(FRAG_COARSE_GRAIN, area->frag_flags) &&
                /* Only merge if no execution has yet occurred: else this
                 * must not be normal rebinding */
                !TEST(VM_EXECUTED_FROM, area->vm_flags) &&
                /* Should be marked invalid; else no loader +rw => not rebinding */
                area->custom.client != NULL &&
                TEST(PERSCACHE_CODE_INVALID,
                     ((coarse_info_t *)area->custom.client)->flags)) {
                /* Case 8640: merge IAT page back in to coarse area.
                 * Easier to merge here than in add_vm_area.
                 */
                coarse_info_t *info = (coarse_info_t *)area->custom.client;
                keep_coarse = true;
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "merging post-IAT (" PFX "-" PFX ") with " PFX "-" PFX "\n", IAT_end,
                    orig_end, area->start, area->end);
                ASSERT(area != NULL);
                ASSERT(area->start == *end);
                ASSERT(IAT_end > orig_start && IAT_end < area->start);
                ASSERT(*start == IAT_end); /* set up above */
                *end = area->end;
                area->start = *start;
                *existing_area = area;
                STATS_INC(coarse_merge_IAT);
                /* If info was loaded prior to rebinding just use it.
                 * Else, we need a fresh coarse_info_t if persisted, so rather than
                 * adjust_coarse_unit_bounds on info we must free it.
                 * Due to lock constraints we can't do that while holding
                 * exec areas lock.
                 */
                /* Bounds should match exactly, since we did not adjust them
                 * on the flush; if they don't, don't use the pcache. */
                if (info->base_pc == area->start && info->end_pc == area->end) {
                    info->flags &= ~PERSCACHE_CODE_INVALID;
                    *info_out = info;
                    STATS_INC(coarse_marked_valid);
                    LOG(GLOBAL, LOG_VMAREAS, 2,
                        "\tkeeping now-valid info %s " PFX "-" PFX "\n", info->module,
                        info->base_pc, info->end_pc);
                } else {
                    /* Go ahead and merge, but don't use this pcache */
                    ASSERT_CURIOSITY(false && "post-rebind pcache bounds mismatch");
                    *tofree = info;
                    area->custom.client = NULL;
                    /* FIXME: we'll try to load again: prevent that?  We
                     * know the image hasn't been modified so no real danger. */
                    STATS_INC(perscache_rebind_load);
                }
            } else if (all_new && area == NULL /*nothing following*/) {
                /* Code section is a single page, so was completely flushed
                 * We'll try to re-load the pcache.
                 * FIXME: we already merged the persisted rct tables into
                 * the live tables when we flushed the pcache: so now
                 * we'll have redundancy, and if we flush again we'll waste
                 * time tryingn to re-add (we do check for dups).
                 */
                ASSERT(!lookup_addr(executable_areas, *start, NULL));
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "marking IAT/code region (" PFX "-" PFX " vs " PFX "-" PFX
                    ") as coarse\n",
                    IAT_start, IAT_end, orig_start, orig_end);
                keep_coarse = true;
                STATS_INC(coarse_merge_IAT); /* we use same stat */
            } else {
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "NOT merging IAT-containing " PFX "-" PFX ": abuts non-inv-coarse\n",
                    orig_start, orig_end);
                DOCHECK(1, {
                    if (all_new && area != NULL &&
                        TEST(FRAG_COARSE_GRAIN, area->frag_flags) &&
                        TEST(VM_EXECUTED_FROM, area->vm_flags)) {
                        coarse_info_t *info = (coarse_info_t *)area->custom.client;
                        ASSERT(!info->persisted);
                        ASSERT(!TEST(PERSCACHE_CODE_INVALID, info->flags));
                    }
                });
            }
        } else {
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "NOT merging .text " PFX "-" PFX " vs IAT " PFX "-" PFX
                " %d %d %d %d %d\n",
                orig_start, orig_end, IAT_start, IAT_end, DYNAMO_OPTION(coarse_merge_iat),
                vm_flags == 0, is_IAT(orig_start, *end, true /*page-align*/, NULL, NULL),
                is_module_patch_region(GLOBAL_DCONTEXT, orig_start, orig_end,
                                       true /*be conservative*/),
                os_module_cmp_IAT_code(orig_start));
        }
    }
    return keep_coarse;
}
#endif

static void
add_executable_vm_area_helper(app_pc start, app_pc end, uint vm_flags, uint frag_flags,
                              coarse_info_t *info _IF_DEBUG(const char *comment))
{
    ASSERT_OWN_WRITE_LOCK(true, &executable_areas->lock);

    add_vm_area(executable_areas, start, end, vm_flags, frag_flags,
                NULL _IF_DEBUG(comment));

    if (TEST(VM_WRITABLE, vm_flags)) {
        /* N.B.: the writable flag indicates the natural state of the memory,
         * not what we have made it be -- we make it read-only before adding
         * to the executable list!
         * FIXME: win32 callback's intercept_call code appears in fragments
         * and is writable...would like to fix that, and coalesce that memory
         * with the generated routines or something
         */
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "WARNING: new executable vm area is writable: " PFX "-" PFX " %s\n", start,
            end, comment);
#if 0
        /* this syslog causes services.exe to hang (ref case 666) once case 666
         * is fixed re-enable if desired FIXME */
        SYSLOG_INTERNAL_WARNING_ONCE("new executable vm area is writable.");
#endif
    }
#ifdef PROGRAM_SHEPHERDING
    if (!DYNAMO_OPTION(selfmod_futureexec) && TEST(FRAG_SELFMOD_SANDBOXED, frag_flags)) {
        /* We do not need future entries for selfmod regions.  We mark
         * the futures as once-only when they are selfmod at future add time, and
         * here we catch those who weren't selfmod then but are now.
         */
        remove_futureexec_vm_area(start, end);
    }
#endif
    if (TEST(FRAG_COARSE_GRAIN, frag_flags)) {
        vm_area_t *area = NULL;
        DEBUG_DECLARE(bool found =)
        lookup_addr(executable_areas, start, &area);
        ASSERT(found && area != NULL);
        if (info == NULL) {
            /* May have been created already, by app_memory_pre_alloc(). */
            info = (coarse_info_t *)area->custom.client;
        }
        /* case 9521: always have one non-frozen coarse unit per coarse region */
        if (info == NULL || (info->frozen && info->non_frozen == NULL)) {
            coarse_info_t *new_info =
                coarse_unit_create(start, end, (info == NULL) ? NULL : &info->module_md5,
                                   true /*for execution*/);
            LOG(GLOBAL, LOG_VMAREAS, 1, "new %scoarse unit %s " PFX "-" PFX "\n",
                info == NULL ? "" : "secondary ", new_info->module, start, end);
            if (info == NULL)
                info = new_info;
            else
                info->non_frozen = new_info;
        }
        area->custom.client = (void *)info;
    }
    DOLOG(2, LOG_VMAREAS, {
        /* new area could have been split into multiple */
        print_contig_vm_areas(executable_areas, start, end, GLOBAL,
                              "new executable vm area: ");
    });
}

static coarse_info_t *
vm_area_load_coarse_unit(app_pc *start INOUT, app_pc *end INOUT, uint vm_flags,
                         uint frag_flags, bool delayed _IF_DEBUG(const char *comment))
{
    coarse_info_t *info;
    /* We load persisted cache files at mmap time primarily for RCT
     * tables; but to avoid duplicated code, and for simplicity, we do
     * so if -use_persisted even if not -use_persisted_rct.
     */
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT_OWN_WRITE_LOCK(true, &executable_areas->lock);
    /* FIXME: we're called before 1st thread is set up.  Only a problem
     * right now for rac_entries_resurrect() w/ private after-call
     * which won't happen w/ -coarse_units that requires shared bbs.
     */
    info = coarse_unit_load(dcontext == NULL ? GLOBAL_DCONTEXT : dcontext, *start, *end,
                            true /*for execution*/);
    if (info != NULL) {
        ASSERT(info->base_pc >= *start && info->end_pc <= *end);
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "using persisted coarse unit %s " PFX "-" PFX " for " PFX "-" PFX "\n",
            info->module, info->base_pc, info->end_pc, *start, *end);
        /* Case 8640/9653/8639: adjust region bounds so that a
         * cache consistency event outside the persisted region
         * does not invalidate it (mainly targeting loader rebinding).
         * We count on FRAG_COARSE_GRAIN preventing any merging of regions.
         * We could delay this until code validation, as RCT tables don't care,
         * and then we could avoid splitting the region in case validation
         * fails: but our plan for lazy per-page validation (case 10601)
         * means we can fail post-split even that way.  So we go ahead and split
         * up front here.  For 4.4 we should move this to 1st exec.
         */
        if (delayed && (info->base_pc > *start || info->end_pc < *end)) {
            /* we already added a region for the whole range earlier */
            remove_vm_area(executable_areas, *start, *end, false /*leave writability*/);
            add_executable_vm_area_helper(info->base_pc, info->end_pc, vm_flags,
                                          frag_flags, info _IF_DEBUG(comment));
        }
        if (info->base_pc > *start) {
            add_executable_vm_area_helper(*start, info->base_pc, vm_flags, frag_flags,
                                          NULL _IF_DEBUG(comment));
            *start = info->base_pc;
        }
        if (info->end_pc < *end) {
            add_executable_vm_area_helper(info->end_pc, *end, vm_flags, frag_flags,
                                          NULL _IF_DEBUG(comment));
            *end = info->end_pc;
        }
        /* if !delayed we'll add the region for the unit in caller */
        ASSERT(info->frozen && info->persisted);
        vm_flags |= VM_PERSISTED_CACHE;
        /* For 4.4 we would mark as PERSCACHE_CODE_INVALID here and
         * mark valid only at 1st execution when we do md5 checks;
         * for 4.3 we're valid until a rebind action.
         */
        ASSERT(!TEST(PERSCACHE_CODE_INVALID, info->flags));
        /* We must add to shared_data, but we cannot here due to lock
         * rank issues (shared_vm_areas lock is higher rank than
         * executable_areas, and we have callers doing flushes and
         * already holding executable_areas), so we delay.
         */
        vm_flags |= VM_ADD_TO_SHARED_DATA;
    }
    return info;
}

/* NOTE : caller is responsible for ensuring that consistency conditions are
 * met, thus if the region is writable the caller must either mark it read
 * only or pass in the VM_DELAY_READONLY flag in which case
 * check_thread_vm_area will mark it read only when a thread goes to build a
 * block from the region */
static bool
add_executable_vm_area(app_pc start, app_pc end, uint vm_flags, uint frag_flags,
                       bool have_writelock _IF_DEBUG(const char *comment))
{
    vm_area_t *existing_area = NULL;
    coarse_info_t *info = NULL;
    coarse_info_t *tofree = NULL;
    app_pc delay_start = NULL, delay_end = NULL;
    /* only expect to see the *_READONLY flags on WRITABLE regions */
    ASSERT(!TEST(VM_DELAY_READONLY, vm_flags) || TEST(VM_WRITABLE, vm_flags));
    ASSERT(!TEST(VM_MADE_READONLY, vm_flags) || TEST(VM_WRITABLE, vm_flags));
#ifdef DEBUG /* can't use DODEBUG b/c of ifdef inside */
    {
        /* we only expect certain flags */
        uint expect = VM_WRITABLE | VM_UNMOD_IMAGE | VM_MADE_READONLY |
            VM_DELAY_READONLY | VM_WAS_FUTURE | VM_EXECUTED_FROM | VM_DRIVER_ADDRESS;
#    ifdef PROGRAM_SHEPHERDING
        expect |= VM_PATTERN_REVERIFY;
#    endif
        ASSERT(!TESTANY(~expect, vm_flags));
    }
#endif /* DEBUG */
    if (!have_writelock) {
#ifdef HOT_PATCHING_INTERFACE
        /* case 9970: need to check hotp vs perscache; rank order hotp < exec_areas */
        if (DYNAMO_OPTION(hot_patching))
            d_r_read_lock(hotp_get_lock());
#endif
        d_r_write_lock(&executable_areas->lock);
    }
    ASSERT_OWN_WRITE_LOCK(true, &executable_areas->lock);
    /* FIXME: rather than change all callers who already hold exec_areas lock
     * to first grab hotp lock, we don't support perscache in those cases.
     * We expect to only be adding a coarse-grain area for module loads.
     */
    ASSERT(!TEST(FRAG_COARSE_GRAIN, frag_flags) || !have_writelock);
    if (TEST(FRAG_COARSE_GRAIN, frag_flags) && !have_writelock) {
#ifdef WINDOWS
        if (!add_executable_vm_area_check_IAT(&start, &end, vm_flags, &existing_area,
                                              &info, &tofree, &delay_start, &delay_end))
            frag_flags &= ~FRAG_COARSE_GRAIN;
#else
        ASSERT(TEST(VM_UNMOD_IMAGE, vm_flags));
#endif
        ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
        if (TEST(FRAG_COARSE_GRAIN, frag_flags) && DYNAMO_OPTION(use_persisted) &&
            info == NULL
            /* if clients are present, don't load until after they're initialized
             */
            && (dynamo_initialized || !CLIENTS_EXIST())) {
            vm_area_t *area;
            if (lookup_addr(executable_areas, start, &area))
                info = (coarse_info_t *)area->custom.client;
            if (info == NULL) {
                info = vm_area_load_coarse_unit(&start, &end, vm_flags, frag_flags,
                                                false _IF_DEBUG(comment));
            }
        }
    }
    if (!DYNAMO_OPTION(coarse_units))
        frag_flags &= ~FRAG_COARSE_GRAIN;

    if (existing_area == NULL) {
        add_executable_vm_area_helper(start, end, vm_flags, frag_flags,
                                      info _IF_DEBUG(comment));
    } else {
        /* we shouldn't need the other parts of _helper() */
        ASSERT(!TEST(VM_WRITABLE, vm_flags));
#ifdef PROGRAM_SHEPHERDING
        ASSERT(DYNAMO_OPTION(selfmod_futureexec) ||
               !TEST(FRAG_SELFMOD_SANDBOXED, frag_flags));
#endif
    }

    if (delay_start != NULL) {
        ASSERT(delay_end > delay_start);
        add_executable_vm_area_helper(delay_start, delay_end, vm_flags, frag_flags,
                                      NULL _IF_DEBUG(comment));
    }

    DOLOG(2, LOG_VMAREAS, {
        /* new area could have been split into multiple */
        print_contig_vm_areas(executable_areas, start, end, GLOBAL,
                              "new executable vm area: ");
    });

    if (!have_writelock) {
        d_r_write_unlock(&executable_areas->lock);
#ifdef HOT_PATCHING_INTERFACE
        if (DYNAMO_OPTION(hot_patching))
            d_r_read_unlock(hotp_get_lock());
#endif
    }
    if (tofree != NULL) {
        /* Since change_linking_lock and info->lock are higher rank than exec areas we
         * must free down here.  FIXME: this should move to 1st exec for 4.4.
         */
        ASSERT(tofree->non_frozen == NULL);
        coarse_unit_reset_free(GLOBAL_DCONTEXT, tofree, false /*no locks*/,
                               true /*unlink*/, true /*give up primary*/);
        coarse_unit_free(GLOBAL_DCONTEXT, tofree);
    }
    return true;
}

/* Used to add dr allocated memory regions that may execute out of the cache */
/* NOTE : region is assumed to not be writable, caller is responsible for
 * ensuring this (see fixme in signal.c adding sigreturn code)
 */
bool
add_executable_region(app_pc start, size_t size _IF_DEBUG(const char *comment))
{
    return add_executable_vm_area(start, start + size, 0, 0,
                                  false /*no lock*/
                                  _IF_DEBUG(comment));
}

/* remove an executable area from the area list
 * the caller is responsible for ensuring that all threads' local vm lists
 * are updated by calling flush_fragments_and_remove_region (can't just
 * remove local vm areas and leave existing fragments hanging...)
 */
static bool
remove_executable_vm_area(app_pc start, app_pc end, bool have_writelock)
{
    bool ok;
    LOG(GLOBAL, LOG_VMAREAS, 2, "removing executable vm area: " PFX "-" PFX "\n", start,
        end);
    if (!have_writelock)
        d_r_write_lock(&executable_areas->lock);
    ok = remove_vm_area(executable_areas, start, end, true /*restore writability!*/);
    if (!have_writelock)
        d_r_write_unlock(&executable_areas->lock);
    return ok;
}

/* removes a region from the executable list */
/* NOTE :the caller is responsible for ensuring that all threads' local
 * vm lists are updated by calling flush_fragments_and_remove_region
 */
bool
remove_executable_region(app_pc start, size_t size, bool have_writelock)
{
    return remove_executable_vm_area(start, start + size, have_writelock);
}

/* To give clients a chance to process pcaches as we load them, we
 * delay the loading until we've initialized the clients.
 */
void
vm_area_delay_load_coarse_units(void)
{
    int i;
    ASSERT(!dynamo_initialized);
    if (!DYNAMO_OPTION(use_persisted) ||
        /* we already loaded if there's no client */
        !CLIENTS_EXIST())
        return;
    d_r_write_lock(&executable_areas->lock);
    for (i = 0; i < executable_areas->length; i++) {
        if (TEST(FRAG_COARSE_GRAIN, executable_areas->buf[i].frag_flags)) {
            vm_area_t *a = &executable_areas->buf[i];
            /* store cur_info b/c a might be blown away */
            coarse_info_t *cur_info = (coarse_info_t *)a->custom.client;
            if (cur_info == NULL || !cur_info->frozen) {
                app_pc start = a->start, end = a->end;
                coarse_info_t *info = vm_area_load_coarse_unit(
                    &start, &end, a->vm_flags, a->frag_flags, true _IF_DEBUG(a->comment));
                if (info != NULL) {
                    /* re-acquire a and i */
                    DEBUG_DECLARE(bool ok =)
                    binary_search(executable_areas, info->base_pc,
                                  info->base_pc + 1 /*open end*/, &a, &i, false);
                    ASSERT(ok);
                    if (cur_info != NULL)
                        info->non_frozen = cur_info;
                    a->custom.client = (void *)info;
                }
            } else
                ASSERT_NOT_REACHED(); /* shouldn't have been loaded already */
        }
    }
    d_r_write_unlock(&executable_areas->lock);
}

/* case 10995: we have to delay freeing un-executed coarse units until
 * we can release the exec areas lock when we flush an un-executed region.
 * This routine frees the queued-up coarse units, and releases the
 * executable areas lock, which the caller must hold.
 */
bool
free_nonexec_coarse_and_unlock()
{
    bool freed_any = false;
    coarse_info_t *info = NULL;
    coarse_info_t *next_info;
    /* We must hold the exec areas lock while traversing the to-delete list,
     * yet we cannot delete while holding it, so we use a temp var
     */
    ASSERT_OWN_WRITE_LOCK(true, &executable_areas->lock);
    ASSERT(coarse_to_delete != NULL);
    if (coarse_to_delete != NULL /*paranoid*/ && *coarse_to_delete != NULL) {
        freed_any = true;
        info = *coarse_to_delete;
        *coarse_to_delete = NULL;
    }
    /* Now we can unlock, and then it's safe to delete */
    executable_areas_unlock();
    if (freed_any) {
        /* units are chained by non_frozen field */
        while (info != NULL) {
            next_info = info->non_frozen;
            if (info->cache != NULL) {
                ASSERT(info->persisted);
                /* We shouldn't need to unlink since no execution has occurred
                 * (lazy linking)
                 */
                ASSERT(info->incoming == NULL);
                ASSERT(!coarse_unit_outgoing_linked(GLOBAL_DCONTEXT, info));
            }
            coarse_unit_reset_free(GLOBAL_DCONTEXT, info, false /*no locks*/,
                                   false /*!unlink*/, true /*give up primary*/);
            coarse_unit_free(GLOBAL_DCONTEXT, info);
            info = next_info;
        }
    }
    return freed_any;
}

#ifdef PROGRAM_SHEPHERDING
/* add a "future executable area" (e.g., mapped EW) to the future list
 *
 * FIXME: now that this is vmareas.c-internal we should change it to
 * take in direct VM_ flags, and make separate flags for each future-adding
 * code origins policy.  Then we can have policy-specific removal from future list.
 */
static bool
add_futureexec_vm_area(app_pc start, app_pc end,
                       bool once_only _IF_DEBUG(const char *comment))
{
    /* FIXME: don't add portions that overlap w/ exec areas */
    LOG(GLOBAL, LOG_VMAREAS, 2, "new FUTURE executable vm area: " PFX "-" PFX " %s%s\n",
        start, end, (once_only ? "ONCE " : ""), comment);

    if (DYNAMO_OPTION(unloaded_target_exception)) {
        /* case 9371 - to avoid possible misclassification in a tight race
         * between NtUnmapViewOfSection and a consecutive future area
         * allocated in the same place, we clear our the unload in progress flag
         */
        mark_unload_future_added(start, end - start);
    }

    d_r_write_lock(&futureexec_areas->lock);
    add_vm_area(futureexec_areas, start, end, (once_only ? VM_ONCE_ONLY : 0),
                0 /* frag_flags */, NULL _IF_DEBUG(comment));
    d_r_write_unlock(&futureexec_areas->lock);
    return true;
}

/* remove a "future executable area" from the future list */
static bool
remove_futureexec_vm_area(app_pc start, app_pc end)
{
    bool ok;
    LOG(GLOBAL, LOG_VMAREAS, 2, "removing FUTURE executable vm area: " PFX "-" PFX "\n",
        start, end);
    d_r_write_lock(&futureexec_areas->lock);
    ok = remove_vm_area(futureexec_areas, start, end, false);
    d_r_write_unlock(&futureexec_areas->lock);
    return ok;
}

/* returns true if the passed in area overlaps any known future executable areas */
static bool
futureexec_vm_area_overlap(app_pc start, app_pc end)
{
    bool overlap;
    d_r_read_lock(&futureexec_areas->lock);
    overlap = vm_area_overlap(futureexec_areas, start, end);
    d_r_read_unlock(&futureexec_areas->lock);
    return overlap;
}
#endif /* PROGRAM_SHEPHERDING */

/* lookup against the per-process executable addresses map */
bool
is_executable_address(app_pc addr)
{
    bool found;
    d_r_read_lock(&executable_areas->lock);
    found = lookup_addr(executable_areas, addr, NULL);
    d_r_read_unlock(&executable_areas->lock);
    return found;
}

/* returns any VM_ flags associated with addr's vm area
 * returns 0 if no area is found
 * cf. get_executable_area_flags() for FRAG_ flags
 */
bool
get_executable_area_vm_flags(app_pc addr, uint *vm_flags)
{
    bool found = false;
    vm_area_t *area;
    d_r_read_lock(&executable_areas->lock);
    if (lookup_addr(executable_areas, addr, &area)) {
        *vm_flags = area->vm_flags;
        found = true;
    }
    d_r_read_unlock(&executable_areas->lock);
    return found;
}

/* if addr is an executable area, returns true and returns in *flags
 *   any FRAG_ flags associated with addr's vm area
 * returns false if area not found
 *
 * cf. get_executable_area_vm_flags() for VM_ flags
 */
bool
get_executable_area_flags(app_pc addr, uint *frag_flags)
{
    bool found = false;
    vm_area_t *area;
    d_r_read_lock(&executable_areas->lock);
    if (lookup_addr(executable_areas, addr, &area)) {
        *frag_flags = area->frag_flags;
        found = true;
    }
    d_r_read_unlock(&executable_areas->lock);
    return found;
}

/* For coarse-grain operation, we use a separate cache and htable per region.
 * See coarse_info_t notes on synchronization model.
 * Returns NULL when region is not coarse.
 * Assumption: this routine is called prior to the first execution from a
 * coarse vm area region.
 */
static coarse_info_t *
get_coarse_info_internal(app_pc addr, bool init, bool have_shvm_lock)
{
    coarse_info_t *coarse = NULL;
    vm_area_t *area = NULL;
    vm_area_t area_copy = {
        0,
    };
    bool add_to_shared = false;
    bool reset_unit = false;
    /* FIXME perf opt: have a last_area */
    /* FIXME: could use vmvector_lookup_data() but I need area->{vm,frag}_flags */
    d_r_read_lock(&executable_areas->lock);
    if (lookup_addr(executable_areas, addr, &area)) {
        ASSERT(area != NULL);
        /* The custom field is initialized to 0 in add_vm_area */
        coarse = (coarse_info_t *)area->custom.client;
        DEBUG_DECLARE(bool is_coarse = TEST(FRAG_COARSE_GRAIN, area->frag_flags);)
        /* We always create coarse_info_t up front in add_executable_vm_area */
        ASSERT((is_coarse && coarse != NULL) || (!is_coarse && coarse == NULL));
        if (init && coarse != NULL && TEST(PERSCACHE_CODE_INVALID, coarse->flags)) {
            /* Reset the unit as the validating event did not occur
             * (can't do it here due to lock rank order vs exec areas lock) */
            reset_unit = true;
            /* We do need to adjust coarse unit bounds for 4.3 when we don't see
             * the rebind +rx event */
            adjust_coarse_unit_bounds(area, true /*even if invalid*/);
            STATS_INC(coarse_executed_invalid);
            /* FIXME for 4.4: validation won't happen post-rebind like 4.3, so we
             * will always get here marked as invalid.  Here we'll do full md5
             * modulo rebasing check (split into per-page via read-only as opt).
             */
        }
        /* We cannot add to shared_data when we load in a persisted unit
         * due to lock rank issues, so we delay until first asked about.
         */
        if (init && TEST(VM_ADD_TO_SHARED_DATA, area->vm_flags)) {
            add_to_shared = true;
            area->vm_flags &= ~VM_ADD_TO_SHARED_DATA;
            area->vm_flags |= VM_EXECUTED_FROM;
            area_copy = *area;
        } else {
            DODEBUG({ area_copy = *area; }); /* for ASSERT below */
        }
    }
    d_r_read_unlock(&executable_areas->lock);

    if (coarse != NULL && init) {
        /* For 4.3, bounds check is done at post-rebind validation;
         * FIXME: in 4.4, we need to do it here and adjust bounds or invalidate
         * pcache if not a superset (we'll allow any if_rx_text to merge into coarse).
         */
        ASSERT(coarse->base_pc == area_copy.start && coarse->end_pc == area_copy.end);
        if (reset_unit) {
            coarse_unit_reset_free(get_thread_private_dcontext(), coarse,
                                   false /*no locks*/, true /*unlink*/,
                                   true /*give up primary*/);
        }
        if (add_to_shared) {
            if (!have_shvm_lock)
                SHARED_VECTOR_RWLOCK(&shared_data->areas, write, lock);
            ASSERT_VMAREA_VECTOR_PROTECTED(&shared_data->areas, WRITE);
            /* avoid double-add from a race */
            if (!lookup_addr(&shared_data->areas, coarse->base_pc, NULL)) {
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "adding coarse region " PFX "-" PFX " to shared vm areas\n",
                    area_copy.start, area_copy.end);
                add_vm_area(&shared_data->areas, area_copy.start, area_copy.end,
                            area_copy.vm_flags, area_copy.frag_flags,
                            NULL _IF_DEBUG(area_copy.comment));
            }
            if (!have_shvm_lock)
                SHARED_VECTOR_RWLOCK(&shared_data->areas, write, unlock);
        }
    } else
        ASSERT(!add_to_shared && !reset_unit);

    return coarse;
}

coarse_info_t *
get_executable_area_coarse_info(app_pc addr)
{
    return get_coarse_info_internal(addr, true /*init*/, false /*no lock*/);
}

/* Ensures there is a non-frozen coarse unit for the executable_areas region
 * corresponding to "frozen", which is now frozen.
 */
void
mark_executable_area_coarse_frozen(coarse_info_t *frozen)
{
    vm_area_t *area = NULL;
    coarse_info_t *info;
    ASSERT(frozen->frozen);                  /* caller should mark */
    d_r_write_lock(&executable_areas->lock); /* since writing flags */
    if (lookup_addr(executable_areas, frozen->base_pc, &area)) {
        ASSERT(area != NULL);
        /* The custom field is initialized to 0 in add_vm_area */
        if (area->custom.client != NULL) {
            ASSERT(TEST(FRAG_COARSE_GRAIN, area->frag_flags));
            info = (coarse_info_t *)area->custom.client;
            ASSERT(info == frozen && frozen->non_frozen == NULL);
            info = coarse_unit_create(frozen->base_pc, frozen->end_pc,
                                      &frozen->module_md5, true /*for execution*/);
            LOG(GLOBAL, LOG_VMAREAS, 1, "new secondary coarse unit %s " PFX "-" PFX "\n",
                info->module, frozen->base_pc, frozen->end_pc);
            frozen->non_frozen = info;
        } else
            ASSERT(!TEST(FRAG_COARSE_GRAIN, area->frag_flags));
    }
    d_r_write_unlock(&executable_areas->lock);
}

/* iterates through all executable areas overlapping the pages touched
 * by the region addr_[start,end)
 * if are_all_matching is false
 *    returns true if any overlapping region has matching vm_flags and frag_flags;
 *    false otherwise
 * if are_all_matching is true
 *    returns true only if all overlapping regions have matching vm_flags
 *    and matching frag_flags, or if there are no overlapping regions;
 *    false otherwise
 * a match of 0 matches all
 */
static bool
executable_areas_match_flags(app_pc addr_start, app_pc addr_end, bool *found_area,
                             /* first_match_start is only set for !are_all_matching */
                             app_pc *first_match_start,
                             bool are_all_matching /* ALL when true, EXISTS when false */,
                             uint match_vm_flags, uint match_frag_flags)
{
    /* binary search below will assure that we hold an executable_areas lock */
    app_pc page_start = (app_pc)ALIGN_BACKWARD(addr_start, PAGE_SIZE);
    app_pc page_end = (app_pc)ALIGN_FORWARD(addr_end, PAGE_SIZE);
    vm_area_t *area;
    if (found_area != NULL)
        *found_area = false;
    /* For flushing the whole address space make sure we don't pass 0..0. */
    if (page_end == NULL && page_start == NULL)
        page_start = (app_pc)1UL;
    ASSERT(page_start < page_end || page_end == NULL); /* wraparound */
    /* We have subpage regions from some of our rules, we should return true
     * if any area on the list that overlaps the pages enclosing the addr_[start,end)
     * region is writable */
    while (binary_search(executable_areas, page_start, page_end, &area, NULL, true)) {
        if (found_area != NULL)
            *found_area = true;
        /* TESTALL will return true for a match of 0 */
        if (are_all_matching) {
            if (!TESTALL(match_vm_flags, area->vm_flags) ||
                !TESTALL(match_frag_flags, area->frag_flags))
                return false;
        } else {
            if (TESTALL(match_vm_flags, area->vm_flags) &&
                TESTALL(match_frag_flags, area->frag_flags)) {
                if (first_match_start != NULL)
                    *first_match_start = area->start;
                return true;
            }
        }
        if (area->end < page_end || page_end == NULL)
            page_start = area->end;
        else
            break;
    }
    return are_all_matching; /* false for EXISTS, true for ALL */
}

/* returns true if addr is on a page that was marked writable by the
 * application but that we marked RO b/c it contains executable code
 * does NOT check if addr is executable, only that something on its page is!
 */
bool
is_executable_area_writable(app_pc addr)
{
    bool writable;
    d_r_read_lock(&executable_areas->lock);
    writable = executable_areas_match_flags(addr, addr + 1 /* open ended */, NULL, NULL,
                                            false /* EXISTS */, VM_MADE_READONLY, 0);
    d_r_read_unlock(&executable_areas->lock);
    return writable;
}

app_pc
is_executable_area_writable_overlap(app_pc start, app_pc end)
{
    app_pc match_start = NULL;
    d_r_read_lock(&executable_areas->lock);
    executable_areas_match_flags(start, end, NULL, &match_start, false /* EXISTS */,
                                 VM_MADE_READONLY, 0);
    d_r_read_unlock(&executable_areas->lock);
    return match_start;
}

#if defined(DEBUG) /* since only used for a stat right now */
/* returns true if region [start, end) overlaps pages that match match_vm_flags
 * e.g. VM_WRITABLE is set when all pages marked writable by the
 *      application but that we marked RO b/c they contain executable code.
 *
 * Does NOT check if region is executable, only that something
 * overlapping its pages is!  are_all_matching determines
 * whether all regions need to match flags, or whether a matching
 * region exists.
 */
bool
is_executable_area_overlap(app_pc start, app_pc end, bool are_all_matching,
                           uint match_vm_flags)
{
    bool writable;
    d_r_read_lock(&executable_areas->lock);
    writable = executable_areas_match_flags(start, end, NULL, NULL, are_all_matching,
                                            match_vm_flags, 0);
    d_r_read_unlock(&executable_areas->lock);
    return writable;
}
#endif

bool
is_pretend_or_executable_writable(app_pc addr)
{
    /* see if asking about an executable area we made read-only */
    return (!standalone_library &&
            (is_executable_area_writable(addr) ||
             (USING_PRETEND_WRITABLE() && is_pretend_writable_address(addr))));
}

/* Returns true if region [start, end) overlaps any regions that are
 * marked as FRAG_COARSE_GRAIN.
 */
bool
executable_vm_area_coarse_overlap(app_pc start, app_pc end)
{
    bool match;
    d_r_read_lock(&executable_areas->lock);
    match = executable_areas_match_flags(start, end, NULL, NULL,
                                         false /*exists, not all*/, 0, FRAG_COARSE_GRAIN);
    d_r_read_unlock(&executable_areas->lock);
    return match;
}

/* Returns true if region [start, end) overlaps any regions that are
 * marked as VM_PERSISTED_CACHE.
 */
bool
executable_vm_area_persisted_overlap(app_pc start, app_pc end)
{
    bool match;
    d_r_read_lock(&executable_areas->lock);
    match = executable_areas_match_flags(
        start, end, NULL, NULL, false /*exists, not all*/, VM_PERSISTED_CACHE, 0);
    d_r_read_unlock(&executable_areas->lock);
    return match;
}

/* Returns true if any part of region [start, end) has ever been executed from */
bool
executable_vm_area_executed_from(app_pc start, app_pc end)
{
    bool match;
    d_r_read_lock(&executable_areas->lock);
    match = executable_areas_match_flags(start, end, NULL, NULL,
                                         false /*exists, not all*/, VM_EXECUTED_FROM, 0);
    d_r_read_unlock(&executable_areas->lock);
    return match;
}

/* If there is no overlap between executable_areas and [start,end), returns false.
 * Else, returns true and sets [overlap_start,overlap_end) as the bounds of the first
 * and last executable_area regions that overlap [start,end); i.e.,
 *   overlap_start starts the first area that overlaps [start,end);
 *   overlap_end ends the last area that overlaps [start,end).
 * Note that overlap_start may be > start and overlap_end may be < end.
 *
 * If frag_flags != 0, the region described above is expanded such that the regions
 * before and after [overlap_start,overlap_end) do NOT match
 * [overlap_start,overlap_end) in TESTALL of frag_flags, but only considering
 * non-contiguous regions if !contig.
 * For example, we pass in FRAG_COARSE_GRAIN and contig=true; then, if
 * the overlap_start region is FRAG_COARSE_GRAIN and it has a contiguous region
 * to its left that is also FRAG_COARSE_GRAIN, but beyond that there is no
 * contiguous region, we will return the start of the region to the left rather than
 * the regular overlap_start.
 */
bool
executable_area_overlap_bounds(app_pc start, app_pc end, app_pc *overlap_start /*OUT*/,
                               app_pc *overlap_end /*OUT*/, uint frag_flags, bool contig)
{
    int start_index, end_index; /* must be signed */
    int i;                      /* must be signed */
    ASSERT(overlap_start != NULL && overlap_end != NULL);
    d_r_read_lock(&executable_areas->lock);

    /* Find first overlapping region */
    if (!binary_search(executable_areas, start, end, NULL, &start_index,
                       true /*first*/)) {
        d_r_read_unlock(&executable_areas->lock);
        return false;
    }
    ASSERT(start_index >= 0);
    if (frag_flags != 0) {
        for (i = start_index - 1; i >= 0; i--) {
            if ((contig &&
                 executable_areas->buf[i].end != executable_areas->buf[i + 1].start) ||
                (TESTALL(frag_flags, executable_areas->buf[i].frag_flags) !=
                 TESTALL(frag_flags, executable_areas->buf[start_index].frag_flags)))
                break;
        }
        ASSERT(i + 1 >= 0);
        *overlap_start = executable_areas->buf[i + 1].start;
    } else
        *overlap_start = executable_areas->buf[start_index].start;

    /* Now find region just at or before end */
    binary_search(executable_areas, end - 1, end, NULL, &end_index, true /*first*/);
    ASSERT(end_index >= 0); /* else 1st binary search would have failed */
    ASSERT(end_index >= start_index);
    if (end_index < executable_areas->length - 1 && frag_flags != 0) {
        for (i = end_index + 1; i < executable_areas->length; i++) {
            if ((contig &&
                 executable_areas->buf[i].start != executable_areas->buf[i - 1].end) ||
                (TESTALL(frag_flags, executable_areas->buf[i].frag_flags) !=
                 TESTALL(frag_flags, executable_areas->buf[end_index].frag_flags)))
                break;
        }
        ASSERT(i - 1 < executable_areas->length);
        *overlap_end = executable_areas->buf[i - 1].end;
    } else /* no extension asked for, or nowhere to extend to */
        *overlap_end = executable_areas->buf[end_index].end;

    d_r_read_unlock(&executable_areas->lock);
    return true;
}

/***************************************************
 * Iterator over coarse units in executable_areas that overlap [start,end) */
void
vm_area_coarse_iter_start(vmvector_iterator_t *vmvi, app_pc start)
{
    int start_index; /* must be signed */
    ASSERT(vmvi != NULL);
    vmvector_iterator_start(executable_areas, vmvi);
    ASSERT_OWN_READ_LOCK(true, &executable_areas->lock);
    /* Find first overlapping region */
    if (start != NULL &&
        binary_search(executable_areas, start, start + 1, NULL, &start_index,
                      true /*first*/)) {
        ASSERT(start_index >= 0);
        vmvi->index = start_index - 1 /*since next is +1*/;
    }
}
static bool
vm_area_coarse_iter_find_next(vmvector_iterator_t *vmvi, app_pc end, bool mutate,
                              coarse_info_t **info_out /*OUT*/)
{
    int forw;
    ASSERT_VMAREA_VECTOR_PROTECTED(vmvi->vector, READWRITE);
    ASSERT(vmvi->vector == executable_areas);
    for (forw = 1; vmvi->index + forw < vmvi->vector->length; forw++) {
        if (end != NULL && executable_areas->buf[vmvi->index + forw].start >= end)
            break;
        if (TEST(FRAG_COARSE_GRAIN,
                 executable_areas->buf[vmvi->index + forw].frag_flags)) {
            coarse_info_t *info = executable_areas->buf[vmvi->index + forw].custom.client;
            if (mutate)
                vmvi->index = vmvi->index + forw;
            ASSERT(info != NULL); /* we always allocate up front */
            if (info_out != NULL)
                *info_out = info;
            return true;
        }
    }
    return false;
}
bool
vm_area_coarse_iter_hasnext(vmvector_iterator_t *vmvi, app_pc end)
{
    return vm_area_coarse_iter_find_next(vmvi, end, false /*no mutate*/, NULL);
}
/* May want to return region bounds if have callers who care about that. */
coarse_info_t *
vm_area_coarse_iter_next(vmvector_iterator_t *vmvi, app_pc end)
{
    coarse_info_t *info = NULL;
    vm_area_coarse_iter_find_next(vmvi, end, true /*mutate*/, &info);
    return info;
}
void
vm_area_coarse_iter_stop(vmvector_iterator_t *vmvi)
{
    ASSERT(vmvi->vector == executable_areas);
    vmvector_iterator_stop(vmvi);
}
/***************************************************/

/* returns true if addr is on a page that contains at least one selfmod
 * region and no non-selfmod regions.
 */
static bool
is_executable_area_on_all_selfmod_pages(app_pc start, app_pc end)
{
    bool all_selfmod;
    bool found;
    d_r_read_lock(&executable_areas->lock);
    all_selfmod = executable_areas_match_flags(start, end, &found, NULL, true /* ALL */,
                                               0, FRAG_SELFMOD_SANDBOXED);
    d_r_read_unlock(&executable_areas->lock);
    /* we require at least one area to be present */
    return all_selfmod && found;
}

/* Meant to be called from a seg fault handler.
 * Returns true if addr is on a page that was marked writable by the
 * application but that we marked RO b/c it contains executable code, OR if
 * addr is on a writable page (since another thread could have removed addr
 * from exec list before seg fault handler was scheduled).
 * does NOT check if addr is executable, only that something on its page is!
 */
bool
was_executable_area_writable(app_pc addr)
{
    bool found_area = false, was_writable = false;
    d_r_read_lock(&executable_areas->lock);
    was_writable = executable_areas_match_flags(addr, addr + 1, &found_area, NULL,
                                                false /* EXISTS */, VM_MADE_READONLY, 0);
    /* seg fault could have happened, then area was made writable before
     * thread w/ exception was scheduled.
     * we assume that area was writable at time of seg fault if it's
     * exec writable now (above) OR no area was found and it's writable now
     * and not on DR area list (below).
     * Need to check DR area list since a write to protected DR area from code
     * cache can end up here, as DR area may be made writable once in fault handler
     * due to self-protection un-protection for entering DR!
     * FIXME: checking for threads_ever_created==1 could further rule out other
     * causes for some apps.
     * Keep readlock to avoid races.
     */
    if (!found_area) {
        uint prot;
        if (get_memory_info(addr, NULL, NULL, &prot))
            was_writable = TEST(MEMPROT_WRITE, prot) && !is_dynamo_address(addr);
    }
    d_r_read_unlock(&executable_areas->lock);
    return was_writable;
}

/* returns true if addr is in an executable area that contains
 * self-modifying code, and so should be sandboxed
 */
bool
is_executable_area_selfmod(app_pc addr)
{
    uint flags;
    if (get_executable_area_flags(addr, &flags))
        return TEST(FRAG_SELFMOD_SANDBOXED, flags);
    else
        return false;
}

#ifdef DGC_DIAGNOSTICS
/* returns false if addr is not in an executable area marked as dyngen */
bool
is_executable_area_dyngen(app_pc addr)
{
    uint flags;
    if (get_executable_area_flags(addr, &flags))
        return TEST(FRAG_DYNGEN, flags);
    else
        return false;
}
#endif

/* lookup against the per-process addresses map */
bool
is_valid_address(app_pc addr)
{
    ASSERT_NOT_IMPLEMENTED(false && "is_valid_address not implemented");
    return false;
}

/* Due to circular dependencies bet vmareas and global heap, we cannot
 * incrementally keep dynamo_areas up to date.
 * Instead, we wait until people ask about it, when we do a complete
 * walk through the heap units and add them all (yes, re-adding
 * ones we've seen).
 */
static void
update_dynamo_vm_areas(bool have_writelock)
{
    if (dynamo_areas_uptodate)
        return;
    if (!have_writelock)
        dynamo_vm_areas_lock();
    ASSERT(dynamo_areas != NULL);
    ASSERT_OWN_WRITE_LOCK(true, &dynamo_areas->lock);
    /* avoid uptodate asserts from heap needed inside add_vm_area */
    DODEBUG({ dynamo_areas_synching = true; });
    /* check again with lock, and repeat until done since
     * could require more memory in the middle for vm area vector
     */
    while (!dynamo_areas_uptodate) {
        dynamo_areas_uptodate = true;
        heap_vmareas_synch_units();
        LOG(GLOBAL, LOG_VMAREAS, 3, "after updating dynamo vm areas:\n");
        DOLOG(3, LOG_VMAREAS, { print_vm_areas(dynamo_areas, GLOBAL); });
    }
    DODEBUG({ dynamo_areas_synching = false; });
    if (!have_writelock)
        dynamo_vm_areas_unlock();
}

bool
are_dynamo_vm_areas_stale(void)
{
    return !dynamo_areas_uptodate;
}

/* Used for DR heap area changes as circular dependences prevent
 * directly adding or removing DR vm areas->
 * Must hold the DR areas lock across the combination of calling this and
 * modifying the heap lists.
 */
void
mark_dynamo_vm_areas_stale()
{
    /* ok to ask for locks or mark stale before dynamo_areas is allocated */
    ASSERT(
        (dynamo_areas == NULL && d_r_get_num_threads() <= 1 /*must be only DR thread*/) ||
        self_owns_write_lock(&dynamo_areas->lock));
    dynamo_areas_uptodate = false;
}

/* HACK to get recursive write lock for internal and external use */
void
dynamo_vm_areas_lock()
{
    all_memory_areas_lock();
    /* ok to ask for locks or mark stale before dynamo_areas is allocated,
     * during heap init and before we can allocate it.  no lock needed then.
     */
    ASSERT(dynamo_areas != NULL || d_r_get_num_threads() <= 1 /*must be only DR thread*/);
    if (dynamo_areas == NULL)
        return;
    if (self_owns_write_lock(&dynamo_areas->lock)) {
        dynamo_areas_recursion++;
        /* we have a 5-deep path:
         *   global_heap_alloc | heap_create_unit | get_guarded_real_memory |
         *   heap_low_on_memory | release_guarded_real_memory
         */
        ASSERT_CURIOSITY(dynamo_areas_recursion <= 4);
    } else
        d_r_write_lock(&dynamo_areas->lock);
}

void
dynamo_vm_areas_unlock()
{
    /* ok to ask for locks or mark stale before dynamo_areas is allocated,
     * during heap init and before we can allocate it.  no lock needed then.
     */
    ASSERT(dynamo_areas != NULL || d_r_get_num_threads() <= 1 /*must be only DR thread*/);
    if (dynamo_areas == NULL)
        return;
    if (dynamo_areas_recursion > 0) {
        ASSERT_OWN_WRITE_LOCK(true, &dynamo_areas->lock);
        dynamo_areas_recursion--;
    } else
        d_r_write_unlock(&dynamo_areas->lock);
    all_memory_areas_unlock();
}

bool
self_owns_dynamo_vm_area_lock()
{
    /* heap inits before dynamo_areas (which now needs heap to init) so
     * we ignore the lock prior to dynamo_areas init, assuming single-DR-thread.
     */
    ASSERT(dynamo_areas != NULL || d_r_get_num_threads() <= 1 /*must be only DR thread*/);
    return dynamo_areas == NULL || self_owns_write_lock(&dynamo_areas->lock);
}

/* grabs read lock and checks for update -- when it returns it guarantees
 * to hold read lock with no updates pending
 */
static void
dynamo_vm_areas_start_reading()
{
    d_r_read_lock(&dynamo_areas->lock);
    while (!dynamo_areas_uptodate) {
        /* switch to write lock
         * cannot rely on uptodate value prior to a lock so must
         * grab read and then check it, and back out if necessary
         * as we have no reader->writer transition
         */
        d_r_read_unlock(&dynamo_areas->lock);
        dynamo_vm_areas_lock();
        update_dynamo_vm_areas(true);
        /* FIXME: more efficient if we could safely drop from write to read
         * lock -- could simply reverse order here and then while becomes if,
         * but a little fragile in that properly nested rwlocks may be assumed
         * elsewhere
         */
        dynamo_vm_areas_unlock();
        d_r_read_lock(&dynamo_areas->lock);
    }
}

static void
dynamo_vm_areas_done_reading()
{
    d_r_read_unlock(&dynamo_areas->lock);
}

/* add dynamo-internal area to the dynamo-internal area list
 * this should be atomic wrt the memory being allocated to avoid races
 * w/ the app executing from it -- thus caller must hold DR areas write lock!
 */
bool
add_dynamo_vm_area(app_pc start, app_pc end, uint prot,
                   bool unmod_image _IF_DEBUG(const char *comment))
{
    uint vm_flags = (TEST(MEMPROT_WRITE, prot) ? VM_WRITABLE : 0) |
        (unmod_image ? VM_UNMOD_IMAGE : 0);
    /* case 3045: areas inside the vmheap reservation are not added to the list */
    ASSERT(!is_vmm_reserved_address(start, end - start, NULL, NULL));
    LOG(GLOBAL, LOG_VMAREAS, 2, "new dynamo vm area: " PFX "-" PFX " %s\n", start, end,
        comment);
    ASSERT(dynamo_areas != NULL);
    ASSERT_OWN_WRITE_LOCK(true, &dynamo_areas->lock);
    if (!dynamo_areas_uptodate)
        update_dynamo_vm_areas(true);
    ASSERT(!vm_area_overlap(dynamo_areas, start, end));
    add_vm_area(dynamo_areas, start, end, vm_flags, 0 /* frag_flags */,
                NULL _IF_DEBUG(comment));
    update_all_memory_areas(start, end, prot,
                            unmod_image ? DR_MEMTYPE_IMAGE : DR_MEMTYPE_DATA);
    return true;
}

/* remove dynamo-internal area from the dynamo-internal area list
 * this should be atomic wrt the memory being freed to avoid races
 * w/ it being re-used and problems w/ the app executing from it --
 * thus caller must hold DR areas write lock!
 */
bool
remove_dynamo_vm_area(app_pc start, app_pc end)
{
    bool ok;
    DEBUG_DECLARE(bool removed);
    LOG(GLOBAL, LOG_VMAREAS, 2, "removing dynamo vm area: " PFX "-" PFX "\n", start, end);
    ASSERT(dynamo_areas != NULL);
    ASSERT_OWN_WRITE_LOCK(true, &dynamo_areas->lock);
    if (!dynamo_areas_uptodate)
        update_dynamo_vm_areas(true);
    ok = remove_vm_area(dynamo_areas, start, end, false);
    DEBUG_DECLARE(removed =)
    remove_from_all_memory_areas(start, end);
    ASSERT(removed);
    return ok;
}

/* adds dynamo-internal area to the dynamo-internal area list, but
 * doesn't grab the dynamo areas lock.  intended to be only used for
 * heap walk updates, where the lock is grabbed prior to the walk and held
 * throughout the entire walk.
 */
bool
add_dynamo_heap_vm_area(app_pc start, app_pc end, bool writable,
                        bool unmod_image _IF_DEBUG(const char *comment))
{
    LOG(GLOBAL, LOG_VMAREAS, 2, "new dynamo vm area: " PFX "-" PFX " %s\n", start, end,
        comment);
    ASSERT(!vm_area_overlap(dynamo_areas, start, end));
    /* case 3045: areas inside the vmheap reservation are not added to the list */
    ASSERT(!is_vmm_reserved_address(start, end - start, NULL, NULL));
    /* add_vm_area will assert that write lock is held */
    add_vm_area(dynamo_areas, start, end,
                VM_DR_HEAP | (writable ? VM_WRITABLE : 0) |
                    (unmod_image ? VM_UNMOD_IMAGE : 0),
                0 /* frag_flags */, NULL _IF_DEBUG(comment));
    return true;
}

/* breaking most abstractions here we return whether current vmarea
 * vector starts at given heap_pc.  The price of circular dependency
 * is that abstractions can no longer be safely used.  case 4196
 */
bool
is_dynamo_area_buffer(byte *heap_unit_start_pc)
{
    return (void *)heap_unit_start_pc == dynamo_areas->buf;
}

/* assumes caller holds dynamo_areas->lock */
void
remove_dynamo_heap_areas()
{
    int i;
    /* remove_vm_area will assert that write lock is held, but let's make
     * sure we're holding it as we walk the vector, even if make no removals
     */
    ASSERT_VMAREA_VECTOR_PROTECTED(dynamo_areas, WRITE);
    LOG(GLOBAL, LOG_VMAREAS, 4, "remove_dynamo_heap_areas:\n");
    /* walk backwards to avoid O(n^2) */
    for (i = dynamo_areas->length - 1; i >= 0; i--) {
        if (TEST(VM_DR_HEAP, dynamo_areas->buf[i].vm_flags)) {
            app_pc start = dynamo_areas->buf[i].start;
            app_pc end = dynamo_areas->buf[i].end;
            /* ASSUMPTION: remove_vm_area, given exact bounds, simply shifts later
             * areas down in vector!
             */
            LOG(GLOBAL, LOG_VMAREAS, 4, "Before removing vm area:\n");
            DOLOG(3, LOG_VMAREAS, { print_vm_areas(dynamo_areas, GLOBAL); });
            remove_vm_area(dynamo_areas, start, end, false);
            LOG(GLOBAL, LOG_VMAREAS, 4, "After removing vm area:\n");
            DOLOG(3, LOG_VMAREAS, { print_vm_areas(dynamo_areas, GLOBAL); });
            remove_from_all_memory_areas(start, end);
        }
    }
}

bool
is_dynamo_address(app_pc addr)
{
    bool found;
    /* case 3045: areas inside the vmheap reservation are not added to the list */
    if (is_vmm_reserved_address(addr, 1, NULL, NULL))
        return true;
    dynamo_vm_areas_start_reading();
    found = lookup_addr(dynamo_areas, addr, NULL);
    dynamo_vm_areas_done_reading();
    return found;
}

/* returns true iff address is an address that the app thinks is writable
 * but really is not, as it overlaps DR memory (or did at the prot time);
 * or we're preventing function patching in specified application modules.
 */
bool
is_pretend_writable_address(app_pc addr)
{
    bool found;
    ASSERT(DYNAMO_OPTION(handle_DR_modify) == DR_MODIFY_NOP ||
           DYNAMO_OPTION(handle_ntdll_modify) == DR_MODIFY_NOP ||
           !IS_STRING_OPTION_EMPTY(patch_proof_list) ||
           !IS_STRING_OPTION_EMPTY(patch_proof_default_list));
    d_r_read_lock(&pretend_writable_areas->lock);
    found = lookup_addr(pretend_writable_areas, addr, NULL);
    d_r_read_unlock(&pretend_writable_areas->lock);
    return found;
}

/* returns true if the passed in area overlaps any known pretend writable areas */
static bool
pretend_writable_vm_area_overlap(app_pc start, app_pc end)
{
    bool overlap;
    d_r_read_lock(&pretend_writable_areas->lock);
    overlap = vm_area_overlap(pretend_writable_areas, start, end);
    d_r_read_unlock(&pretend_writable_areas->lock);
    return overlap;
}

#ifdef DEBUG
/* returns comment for addr, if there is one, else NULL
 */
char *
get_address_comment(app_pc addr)
{
    char *res = NULL;
    vm_area_t *area;
    bool ok;
    d_r_read_lock(&executable_areas->lock);
    ok = lookup_addr(executable_areas, addr, &area);
    if (ok)
        res = area->comment;
    d_r_read_unlock(&executable_areas->lock);
    if (!ok) {
        d_r_read_lock(&dynamo_areas->lock);
        ok = lookup_addr(dynamo_areas, addr, &area);
        if (ok)
            res = area->comment;
        d_r_read_unlock(&dynamo_areas->lock);
    }
    return res;
}
#endif

/* returns true if the passed in area overlaps any known executable areas
 * if !have_writelock, acquires the executable_areas read lock
 */
bool
executable_vm_area_overlap(app_pc start, app_pc end, bool have_writelock)
{
    bool overlap;
    if (!have_writelock)
        d_r_read_lock(&executable_areas->lock);
    overlap = vm_area_overlap(executable_areas, start, end);
    if (!have_writelock)
        d_r_read_unlock(&executable_areas->lock);
    return overlap;
}

void
executable_areas_lock()
{
    d_r_write_lock(&executable_areas->lock);
}

void
executable_areas_unlock()
{
    ASSERT_OWN_WRITE_LOCK(true, &executable_areas->lock);
    d_r_write_unlock(&executable_areas->lock);
}

/* returns true if the passed in area overlaps any dynamo areas */
bool
dynamo_vm_area_overlap(app_pc start, app_pc end)
{
    bool overlap;
    /* case 3045: areas inside the vmheap reservation are not added to the list */
    if (is_vmm_reserved_address(start, end - start, NULL, NULL))
        return true;
    dynamo_vm_areas_start_reading();
    overlap = vm_area_overlap(dynamo_areas, start, end);
    dynamo_vm_areas_done_reading();
    return overlap;
}

/* Checks to see if pc is on the stack
 * If pc has already been resolved into an area, pass that in.
 */
static bool
is_on_stack(dcontext_t *dcontext, app_pc pc, vm_area_t *area)
{
    byte *stack_base, *stack_top; /* "official" stack */
    byte *esp = (byte *)get_mcontext(dcontext)->xsp;
    byte *esp_base;
    size_t size;
    bool ok, query_esp = true;
    /* First check the area if we're supplied one. */
    if (area != NULL) {
        LOG(THREAD, LOG_VMAREAS, 3,
            "stack vs " PFX ": area " PFX ".." PFX ", esp " PFX "\n", pc, area->start,
            area->end, esp);
        ASSERT(pc >= area->start && pc < area->end);
        if (esp >= area->start && esp < area->end)
            return true;
    }
    /* Now check the "official" stack bounds.  These are cached so cheap to
     * look up.  Xref case 8180, these might not always be available,
     * get_stack_bounds() takes care of any asserts on availability. */
    ok = get_stack_bounds(dcontext, &stack_base, &stack_top);
    if (ok) {
        LOG(THREAD, LOG_VMAREAS, 3,
            "stack vs " PFX ": official " PFX ".." PFX ", esp " PFX "\n", pc, stack_base,
            stack_top, esp);
        ASSERT(stack_base < stack_top);
        if (pc >= stack_base && pc < stack_top)
            return true;
        /* We optimize away the expensive query of esp region bounds if esp
         * is in within the "official" stack cached allocation bounds. */
        if (esp >= stack_base && esp < stack_top)
            query_esp = false;
    }
    if (query_esp) {
        ok = get_memory_info(esp, &esp_base, &size, NULL);
        if (!ok) {
            /* This can happen with dr_prepopulate_cache(). */
            ASSERT(!dynamo_started);
            return false;
        }
        LOG(THREAD, LOG_VMAREAS, 3,
            "stack vs " PFX ": region " PFX ".." PFX ", esp " PFX "\n", pc, esp_base,
            esp_base + size, esp);
        /* FIXME - stack could be split into multiple os regions by prot
         * differences, could check alloc base equivalence. */
        if (pc >= esp_base && pc < esp_base + size)
            return true;
    }
    return false;
}

bool
is_address_on_stack(dcontext_t *dcontext, app_pc address)
{
    return is_on_stack(dcontext, address, NULL);
}

/* returns true if an executable area exists with VM_DRIVER_ADDRESS,
 * not a strict opposite of is_user_address() */
bool
is_driver_address(app_pc addr)
{
    uint vm_flags;
    if (get_executable_area_vm_flags(addr, &vm_flags)) {
        return TEST(VM_DRIVER_ADDRESS, vm_flags);
    }
    return false;
}

#ifdef PROGRAM_SHEPHERDING /********************************************/

/* forward declaration */
static int
check_origins_bb_pattern(dcontext_t *dcontext, app_pc addr, app_pc *base, size_t *size,
                         uint *vm_flags, uint *frag_flags);

/* The following two arrays need to be in synch with enum action_type_t defined in
 * vmareas.h.
 */
#    define MESSAGE_EXEC_VIOLATION "Execution security violation was intercepted!\n"
#    define MESSAGE_CONTACT_VENDOR \
        "Contact your vendor for a security vulnerability fix.\n"
const char *const action_message[] = {
    /* no trailing newlines for SYSLOG_INTERNAL */
    MESSAGE_EXEC_VIOLATION MESSAGE_CONTACT_VENDOR "Program terminated.",
    MESSAGE_EXEC_VIOLATION MESSAGE_CONTACT_VENDOR "Program continuing!",
    MESSAGE_EXEC_VIOLATION MESSAGE_CONTACT_VENDOR
    "Program continuing after terminating thread.",
    MESSAGE_EXEC_VIOLATION MESSAGE_CONTACT_VENDOR
    "Program continuing after throwing an exception."
};

/* event log message IDs */
#    ifdef WINDOWS
const uint action_event_id[] = {
    MSG_SEC_VIOLATION_TERMINATED, MSG_SEC_VIOLATION_CONTINUE,
    MSG_SEC_VIOLATION_THREAD,     MSG_SEC_VIOLATION_EXCEPTION,
#        ifdef HOT_PATCHING_INTERFACE
    MSG_HOT_PATCH_VIOLATION,
#        endif
};
#    endif

/* fills the target component of a threat ID */
static void
fill_security_violation_target(char name[MAXIMUM_VIOLATION_NAME_LENGTH],
                               const byte target_contents[4])
{
    int i;
    for (i = 0; i < 4; i++)
        name[i + 5] = (char)((target_contents[i] % 10) + '0');
}

static void
get_security_violation_name(dcontext_t *dcontext, app_pc addr, char *name,
                            int name_length, security_violation_t violation_type,
                            const char *threat_id)
{
    ptr_uint_t addr_as_int;
    app_pc name_addr = NULL;
    int i;

    ASSERT(name_length >= MAXIMUM_VIOLATION_NAME_LENGTH);

    /* Hot patches & process_control use their own threat IDs. */
    if (IF_HOTP(violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
                violation_type == HOT_PATCH_PROTECTOR_VIOLATION ||)
            IF_PROC_CTL(violation_type == PROCESS_CONTROL_VIOLATION ||) false) {
        ASSERT(threat_id != NULL);
        strncpy(name, threat_id, MAXIMUM_VIOLATION_NAME_LENGTH);
    } else {
        bool unreadable_addr = false;
        byte target_contents[4];   /* 4 instruction bytes read from target */
        ASSERT(threat_id == NULL); /* Supplied only for hot patch violations.*/

        /* First four characters are alphabetics calculated from the address
           of the beginning of the basic block from which the violating
           contol transfer instruction originated.  Ideally we would use the
           exact CTI address rather than the beginning of its block, but
           we don't want to translate it back to an app address to reduce
           possible failure points on this critical path. */
        name_addr = dcontext->last_fragment->tag;
#    ifdef WINDOWS
        /* Move PC relative to preferred base for consistent naming */
        name_addr += get_module_preferred_base_delta(name_addr);
#    endif
        addr_as_int = (ptr_uint_t)name_addr;
        for (i = 0; i < 4; i++) {
            name[i] = (char)((addr_as_int % 26) + 'A');
            addr_as_int /= 256;
        }

        /* Fifth character is a '.' */
        name[4] = '.';

        unreadable_addr = !d_r_safe_read(addr, sizeof(target_contents), &target_contents);

        /* if at unreadable memory see if an ASLR preferred address can be used */
        if (unreadable_addr) {
            app_pc likely_target_pc = aslr_possible_preferred_address(addr);
            if (likely_target_pc != NULL) {
                unreadable_addr = !d_r_safe_read(
                    likely_target_pc, sizeof(target_contents), &target_contents);
            } else {
                unreadable_addr = true;
            }
        }

        /* Next four characters are decimal numerics from the target code */
        if (unreadable_addr) {
            for (i = 0; i < 4; i++)
                name[i + 5] = 'X';
        } else {
            fill_security_violation_target(name, target_contents);
        }
    }

    /* Tenth character is a '.' */
    name[9] = '.';

    /* Next character indicates the security violation type;
     * sequential letter choices used rather than semantic ones to
     * obfuscate meaning.   */
    switch (violation_type) {
    case STACK_EXECUTION_VIOLATION: name[10] = 'A'; break;
    case HEAP_EXECUTION_VIOLATION: name[10] = 'B'; break;
    case RETURN_TARGET_VIOLATION: name[10] = 'C'; break;
    case RETURN_DIRECT_RCT_VIOLATION:
        name[10] = 'D';
        ASSERT_NOT_IMPLEMENTED(false);
        break;
    case INDIRECT_CALL_RCT_VIOLATION: name[10] = 'E'; break;
    case INDIRECT_JUMP_RCT_VIOLATION: name[10] = 'F'; break;
#    ifdef HOT_PATCHING_INTERFACE
    case HOT_PATCH_DETECTOR_VIOLATION: name[10] = 'H'; break;
    case HOT_PATCH_PROTECTOR_VIOLATION: name[10] = 'P'; break;
#    endif
#    ifdef PROCESS_CONTROL
    case PROCESS_CONTROL_VIOLATION: name[10] = 'K'; break;
#    endif
#    ifdef GBOP
    case GBOP_SOURCE_VIOLATION: name[10] = 'O'; break;
#    endif
    case ASLR_TARGET_VIOLATION: name[10] = 'R'; break;
    case ATTACK_SIM_NUDGE_VIOLATION: /* share w/ normal attack sim */
    case ATTACK_SIMULATION_VIOLATION: name[10] = 'S'; break;
    case APC_THREAD_SHELLCODE_VIOLATION:
        /* injected shellcode threat names are custom generated */
        ASSERT_NOT_REACHED();
        name[10] = 'B';
        break;
    default: name[10] = 'X'; ASSERT_NOT_REACHED();
    }

    /* Null-terminate */
    name[11] = '\0';

    LOG(GLOBAL, LOG_ALL, 1, "Security violation name: %s\n", name);
}

bool
is_exempt_threat_name(const char *name)
{
    if (DYNAMO_OPTION(exempt_threat) && !IS_STRING_OPTION_EMPTY(exempt_threat_list)) {
        bool onlist;
        string_option_read_lock();
        onlist = check_filter_with_wildcards(DYNAMO_OPTION(exempt_threat_list), name);
        string_option_read_unlock();
        if (onlist) {
            LOG(THREAD_GET, LOG_INTERP | LOG_VMAREAS, 1,
                "WARNING: threat %s is on exempt list, suppressing violation\n", name);
            SYSLOG_INTERNAL_WARNING_ONCE("threat %s exempt", name);
            STATS_INC(num_exempt_threat);
            return true;
        }
    }
    return false;
}

/***************************************************************************
 * Case 8075: we don't want to unprotect .data during violation reporting, so we
 * place all the local-scope static vars (from DO_THRESHOLD) into .fspdata.
 */
START_DATA_SECTION(FREQ_PROTECTED_SECTION, "w");

/* Report security violation to all outputs - syslog, diagnostics, and interactive
 * returns false if violation was not reported
 */
static bool
security_violation_report(app_pc addr, security_violation_t violation_type,
                          const char *name, action_type_t action)
{
    bool dump_forensics = true;
    /* shouldn't report anything if on silent_block_threat_list */
    if (!IS_STRING_OPTION_EMPTY(silent_block_threat_list)) {
        bool onlist;
        string_option_read_lock();
        onlist =
            check_filter_with_wildcards(DYNAMO_OPTION(silent_block_threat_list), name);
        string_option_read_unlock();
        if (onlist) {
            LOG(THREAD_GET, LOG_INTERP | LOG_VMAREAS, 1,
                "WARNING: threat %s is on silent block list, suppressing reporting\n",
                name);
            SYSLOG_INTERNAL_WARNING_ONCE("threat %s silently blocked", name);
            STATS_INC(num_silently_blocked_threat);
            return false;
        }
    }

    if (dynamo_options.report_max) {
        /* need bool since ctr only inc-ed when < threshold, so no way
         * to tell 1st instance beyond threshold from subsequent
         */
        static bool reached_max = false;
        /* do not report in any way if report threshold is reached */
        DO_THRESHOLD_SAFE(dynamo_options.report_max, FREQ_PROTECTED_SECTION,
                          { /* < report_max */ }, {
                              /* >= report_max */
                              if (!reached_max) {
                                  reached_max = true;
                                  SYSLOG(SYSLOG_WARNING, WARNING_REPORT_THRESHOLD, 2,
                                         get_application_name(), get_application_pid());
                              }
                              return false;
                          });
    }

    /* options already synchronized by security_violation() */
    if ((TEST(DUMPCORE_SECURITY_VIOLATION, DYNAMO_OPTION(dumpcore_mask))
#    ifdef HOT_PATCHING_INTERFACE /* Part of fix for 5367. */
         && violation_type != HOT_PATCH_DETECTOR_VIOLATION &&
         violation_type != HOT_PATCH_PROTECTOR_VIOLATION
#    endif
         )
#    ifdef HOT_PATCHING_INTERFACE /* Part of fix for 5367. */
        /* Dump core if violation was for hot patch detector/protector and
         * the corresponding dumpcore_mask flag was set.
         */
        || (TEST(DUMPCORE_HOTP_DETECTION, DYNAMO_OPTION(dumpcore_mask)) &&
            violation_type == HOT_PATCH_DETECTOR_VIOLATION) ||
        (TEST(DUMPCORE_HOTP_PROTECTION, DYNAMO_OPTION(dumpcore_mask)) &&
         violation_type == HOT_PATCH_PROTECTOR_VIOLATION)
#    endif
    ) {
        DO_THRESHOLD_SAFE(DYNAMO_OPTION(dumpcore_violation_threshold),
                          FREQ_PROTECTED_SECTION, os_dump_core(name) /* < threshold */, );
    }

#    ifdef HOT_PATCHING_INTERFACE
    if (violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
        violation_type == HOT_PATCH_PROTECTOR_VIOLATION) {
        SYSLOG_CUSTOM_NOTIFY(SYSLOG_ERROR, IF_WINDOWS_ELSE_0(MSG_HOT_PATCH_VIOLATION), 3,
                             (char *)action_message[action], get_application_name(),
                             get_application_pid(), name);
    } else
#    endif
        SYSLOG_CUSTOM_NOTIFY(SYSLOG_ERROR, IF_WINDOWS_ELSE_0(action_event_id[action]), 3,
                             (char *)action_message[action], get_application_name(),
                             get_application_pid(), name);

#    ifdef HOT_PATCHING_INTERFACE
    /* Part of fix for 5367.  For hot patches core dumps and forensics should
     * be generated only if needed, which is not the case for other violations.
     */
    if (!(DYNAMO_OPTION(hotp_diagnostics)) &&
        (violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
         violation_type == HOT_PATCH_PROTECTOR_VIOLATION))
        dump_forensics = false;
#    endif
#    ifdef PROCESS_CONTROL
    if (!DYNAMO_OPTION(pc_diagnostics) && /* Case 11023. */
        violation_type == PROCESS_CONTROL_VIOLATION)
        dump_forensics = false;
#    endif
    /* report_max (above) will limit the number of files created */
    if (dump_forensics)
        report_diagnostics(action_message[action], name, violation_type);

    return true;
}

/* Attack handling: reports violation, decides on action, possibly terminates the process.
 * N.B.: we make assumptions about whether the callers of this routine hold
 * various locks, so be careful when adding new callers.
 *
 * type_handling prescribes per-type handling and is combined with
 * global options.  It can be used to specify whether to take an
 * action (and may request specific alternative handling with
 * OPTION_HANDLING), and whether to report.
 *
 * The optional out value result_type can differ from the passed-in violation_type
 * for exemptions.
 * Returns an action, with the caller responsible for calling
 * security_violation_action() if action != ACTION_CONTINUE
 */
static action_type_t
security_violation_internal_main(dcontext_t *dcontext, app_pc addr,
                                 security_violation_t violation_type,
                                 security_option_t type_handling, const char *threat_id,
                                 const action_type_t desired_action,
                                 read_write_lock_t *lock,
                                 security_violation_t *result_type /*OUT*/)
{
    /* All violations except hot patch ones will request the safest solution, i.e.,
     * to terminate the process.  Based on the options used, different ones may be
     * selected in this function.  However, hot patches can request specific actions
     * as specified by the hot patch writer.
     */
    action_type_t action = desired_action;
    /* probably best to simply use the default TERMINATE_PROCESS */
    char name[MAXIMUM_VIOLATION_NAME_LENGTH];
    bool action_selected = false;
    bool found_unsupported = false;
#    ifdef HOT_PATCHING_INTERFACE
    /* Passing the hotp lock as an argument is ugly, but it is the cleanest way
     * to release the hotp lock for case 7988, otherwise, will have to release
     * it in hotp_event_notify and re-acquire it after reporting - really ugly.
     * Anyway, cleaning up the interface to security_violation is in plan
     * for Marlin, a FIXME, case 8079.
     */
    ASSERT((DYNAMO_OPTION(hot_patching) && lock == hotp_get_lock()) || lock == NULL);
#    else
    ASSERT(lock == NULL);
#    endif
    /* though ASLR handling is currently not using this routine */
    ASSERT(violation_type != ASLR_TARGET_VIOLATION);

    DOLOG(2, LOG_ALL, {
        SYSLOG_INTERNAL_INFO("security_violation(" PFX ", %d)", addr, violation_type);
        LOG(THREAD, LOG_VMAREAS, 2, "executable areas are:\n");
        print_executable_areas(THREAD);
        LOG(THREAD, LOG_VMAREAS, 2, "future executable areas are:\n");
        d_r_read_lock(&futureexec_areas->lock);
        print_vm_areas(futureexec_areas, THREAD);
        d_r_read_unlock(&futureexec_areas->lock);
    });

    /* case 8075: we no longer unprot .data on the violation path */
    ASSERT(check_should_be_protected(DATASEC_RARELY_PROT));

    /* CHECK: all options for attack handling and reporting are dynamic,
     * synchronized only once.
     */
    synchronize_dynamic_options();

#    ifdef HOT_PATCHING_INTERFACE
    if (violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
        violation_type == HOT_PATCH_PROTECTOR_VIOLATION) {
        /* For hot patches, the action is provided by the hot patch writer;
         * nothing should be selected here.
         */
        action_selected = true;
    }
#    endif
#    ifdef PROCESS_CONTROL
    /* A process control violation (which can only happen if process control is
     * turned on) results in the process being killed unless it is running in
     * detect mode.
     */
    if (violation_type == PROCESS_CONTROL_VIOLATION) {
        ASSERT(IS_PROCESS_CONTROL_ON());
        ASSERT((action == ACTION_TERMINATE_PROCESS && !DYNAMO_OPTION(pc_detect_mode)) ||
               (action == ACTION_CONTINUE && DYNAMO_OPTION(pc_detect_mode)));
        action_selected = true;
    }
#    endif
    /* one last chance to avoid a violation */
    get_security_violation_name(dcontext, addr, name, MAXIMUM_VIOLATION_NAME_LENGTH,
                                violation_type, threat_id);
    if (!IS_STRING_OPTION_EMPTY(exempt_threat_list)) {
        if (is_exempt_threat_name(name)) {
            if (result_type != NULL)
                *result_type = ALLOWING_BAD;
            mark_module_exempted(addr);
            return ACTION_CONTINUE;
        }
    }

    /* FIXME: if we reinstate case 6141 where we acquire the thread_initexit_lock
     * we'll need to release our locks!
     * See ifdef FORENSICS_ACQUIRES_INITEXIT_LOCK in the Attic.
     * FIXME: even worse, we'll crash w/ case 9381 if we get a flush
     * while we're nolinking due to init-extra-vmareas on the frags list!
     */

    /* diagnose_violation_mode says to check if would have allowed if were
     * allowing patterns.
     */
    if (dynamo_options.diagnose_violation_mode &&
        !dynamo_options.executable_if_trampoline) {
        size_t junk;
        if (check_origins_bb_pattern(dcontext, addr, (app_pc *)&junk, &junk,
                                     (uint *)&junk, (uint *)&junk) == ALLOWING_OK) {
            /* FIXME: change later user-visible message to indicate this may be
             * a false positive
             */
            SYSLOG_INTERNAL_WARNING_ONCE("would have allowed pattern DGC.");
        }
    }
#    ifdef DGC_DIAGNOSTICS
    LOG(GLOBAL, LOG_VMAREAS, 1, "violating basic block target:\n");
    DOLOG(1, LOG_VMAREAS, { disassemble_app_bb(dcontext, addr, GLOBAL); });
#    endif
    /* for non-debug build, give some info on violating block */
    DODEBUG({
        if (is_readable_without_exception(addr, 12)) {
            SYSLOG_INTERNAL_WARNING("violating basic block target @" PFX ": "
                                    "%x %x %x %x %x %x %x %x %x %x %x %x",
                                    addr, *addr, *(addr + 1), *(addr + 2), *(addr + 3),
                                    *(addr + 4), *(addr + 5), *(addr + 6), *(addr + 7),
                                    *(addr + 8), *(addr + 9), *(addr + 10), *(addr + 11));
        } else
            SYSLOG_INTERNAL_WARNING(
                "violating basic block target @" PFX ": not readable!", addr);
    });

    if (DYNAMO_OPTION(detect_mode) &&
        !TEST(OPTION_BLOCK_IGNORE_DETECT, type_handling)
        /* As of today, detect mode for hot patches is set using modes files. */
        IF_HOTP(&&violation_type != HOT_PATCH_DETECTOR_VIOLATION &&
                violation_type != HOT_PATCH_PROTECTOR_VIOLATION)) {
        bool allow = true;
        /* would be nice to keep the count going when no max, so if dynamically impose
         * one later all the previous ones count toward it, but then have to worry about
         * overflow of counter, etc. -- so we ignore count while there's no max
         */
        if (DYNAMO_OPTION(detect_mode_max) > 0) {
            /* global counter for violations in all threads */
            DO_THRESHOLD_SAFE(
                DYNAMO_OPTION(detect_mode_max), FREQ_PROTECTED_SECTION,
                { /* < max */
                  LOG(GLOBAL, LOG_ALL, 1,
                      "security_violation: allowing violation #%d "
                      "[max %d], tid=" TIDFMT "\n",
                      do_threshold_cur, DYNAMO_OPTION(detect_mode_max),
                      d_r_get_thread_id());
                },
                { /* >= max */
                  allow = false;
                  LOG(GLOBAL, LOG_ALL, 1,
                      "security_violation: reached maximum allowed %d, "
                      "tid=" TIDFMT "\n",
                      DYNAMO_OPTION(detect_mode_max), d_r_get_thread_id());
                });
        } else {
            LOG(GLOBAL, LOG_ALL, 1,
                "security_violation: allowing violation, no max, tid=%d\n",
                d_r_get_thread_id());
        }
        if (allow) {
            /* we have priority over other handling options */
            action = ACTION_CONTINUE;
            action_selected = true;
            mark_module_exempted(addr);
        }
    }

    /* FIXME: case 2144 we need to TEST(OPTION_BLOCK early on so that
     * we do not impact the counters, in addition we need to
     * TEST(OPTION_HANDLING to specify an alternative attack handling
     * (e.g. -throw_exception if default is -kill_thread)
     * FIXME: We may also want a different message to allow 'staging' events to be
     * considered differently, maybe with a DO_ONCE semantics...
     */

    /* decide on specific attack handling action if not continuing */
    if (!action_selected && DYNAMO_OPTION(throw_exception)) {
        thread_data_t *thread_local = (thread_data_t *)dcontext->vm_areas_field;
        /* maintain a thread local counter to bail out and avoid infinite exceptions */
        if (thread_local->thrown_exceptions <
            DYNAMO_OPTION(throw_exception_max_per_thread)) {
#    ifdef WINDOWS
            /* If can't verify consistent SEH chain should fall through to kill path */
            /* UnhandledExceptionFilter is always installed. */
            /* There is no point in throwing an exception if no other
               handlers are installed to unwind.
               We may still get there when our exception is not handled,
               but at least cleanup code will be given a chance.
            */
            enum {
                MIN_SEH_DEPTH = 1
                /* doesn't seem to deserve a separate option */
            };
            int seh_chain_depth = exception_frame_chain_depth(dcontext);
            if (seh_chain_depth > MIN_SEH_DEPTH) {
                /* note the check is best effort,
                   e.g. attacked handler can still point to valid RET */
                bool global_max_reached = true;
                /* check global counter as well */
                DO_THRESHOLD_SAFE(
                    DYNAMO_OPTION(throw_exception_max), FREQ_PROTECTED_SECTION,
                    { global_max_reached = false; }, { global_max_reached = true; });
                if (!global_max_reached) {
                    thread_local->thrown_exceptions++;
                    LOG(GLOBAL, LOG_ALL, 1,
                        "security_violation: throwing exception %d for this thread "
                        "[max pt %d] [global max %d]\n",
                        thread_local->thrown_exceptions,
                        dynamo_options.throw_exception_max_per_thread,
                        dynamo_options.throw_exception_max);
                    action = ACTION_THROW_EXCEPTION;
                    action_selected = true;
                }
            } else {
                LOG(GLOBAL, LOG_ALL, 1,
                    "security_violation: SEH chain invalid [%d], better kill\n",
                    seh_chain_depth);
            }
#    else
            ASSERT_NOT_IMPLEMENTED(false);
#    endif /* WINDOWS */
        } else {
            LOG(GLOBAL, LOG_ALL, 1,
                "security_violation: reached maximum exception count, kill now\n");
        }
    }

    /* kill process or maybe thread */
    if (!action_selected) {
        ASSERT(action == ACTION_TERMINATE_PROCESS);
        if (DYNAMO_OPTION(kill_thread)) {
            /* check global counter as well */
            DO_THRESHOLD_SAFE(
                DYNAMO_OPTION(kill_thread_max), FREQ_PROTECTED_SECTION,
                { /* < max */
                  LOG(GLOBAL, LOG_ALL, 1,
                      "security_violation: \t killing thread #%d [max %d], tid=%d\n",
                      do_threshold_cur, DYNAMO_OPTION(kill_thread_max),
                      d_r_get_thread_id());
                  /* FIXME: can't check if d_r_get_num_threads()==1 then say we're
                   * killing process because it is possible that another
                   * thread has not been scheduled yet and we wouldn't have
                   * seen it.  Still, only our message will be wrong if we end
                   * up killing the process, when we terminate the last thread
                   */
                  action = ACTION_TERMINATE_THREAD;
                  action_selected = true;
                },
                { /* >= max */
                  LOG(GLOBAL, LOG_ALL, 1,
                      "security_violation: reached maximum thread kill, "
                      "kill process now\n");
                  action = ACTION_TERMINATE_PROCESS;
                  action_selected = true;
                });
        } else {
            action = ACTION_TERMINATE_PROCESS;
            action_selected = true;
        }
    }
    ASSERT(action_selected);

    /* Case 9712: Inform the client of the security violation and
     * give it a chance to modify the action.
     */
    if (CLIENTS_EXIST()) {
        instrument_security_violation(dcontext, addr, violation_type, &action);
    }

    /* now we know what is the chosen action and we can report */
    if (TEST(OPTION_REPORT, type_handling))
        security_violation_report(addr, violation_type, name, action);

    /* FIXME: walking the loader data structures at arbitrary
     * points is dangerous due to data races with other threads
     * -- see is_module_being_initialized and get_module_name */
    if (check_for_unsupported_modules()) {
        /* found an unsupported module */
        action = ACTION_TERMINATE_PROCESS;
        found_unsupported = true;
        /* NOTE that because of the violation_threshold this
         * check isn't actually sufficient to ensure we get a dump file
         * (if for instance already got several violations) but it's good
         * enough */
        if (TEST(DUMPCORE_UNSUPPORTED_APP, DYNAMO_OPTION(dumpcore_mask)) &&
            !TEST(DUMPCORE_SECURITY_VIOLATION, DYNAMO_OPTION(dumpcore_mask))) {
            os_dump_core("unsupported module");
        }
    }

#    ifdef WINDOWS
    if (ACTION_TERMINATE_PROCESS == action &&
        (TEST(DETACH_UNHANDLED_VIOLATION, DYNAMO_OPTION(internal_detach_mask)) ||
         (found_unsupported &&
          TEST(DETACH_UNSUPPORTED_MODULE, DYNAMO_OPTION(internal_detach_mask))))) {
        /* set pc to right value and detach */
        get_mcontext(dcontext)->pc = addr;
        /* FIXME - currently detach_internal creates a new thread to do the
         * detach (case 3312) and if we hold an app lock used by the init apc
         * such as the loader lock (case 4486) we could livelock the process
         * if we used a synchronous detach.  Instead, we set detach in motion
         * disable all future violations and continue.
         */
        detach_internal();
        options_make_writable();
        /* make sure synchronizes won't clobber the changes here */
        dynamo_options.dynamic_options = false;
        dynamo_options.detect_mode = true;
        dynamo_options.detect_mode_max = 0; /* no limit on detections */
        dynamo_options.report_max = 1;      /* don't report any more */
        options_restore_readonly();
        action = ACTION_CONTINUE;
    }
#    endif

    /* FIXME: move this into hotp code like we've done for bb building so we
     * don't need to pass the lock in anymore
     */
#    ifdef HOT_PATCHING_INTERFACE
    /* Fix for case 7988.  Release the hotp lock when the remediation action
     * is to terminate the {thread,process} or to throw an exception, otherwise
     * we will deadlock trying to access the hotp_vul_table in another thread.
     */
    if (lock != NULL &&
        (action == ACTION_TERMINATE_THREAD || action == ACTION_TERMINATE_PROCESS ||
         action == ACTION_THROW_EXCEPTION)) {
#        ifdef GBOP
        ASSERT(violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
               violation_type == HOT_PATCH_PROTECTOR_VIOLATION ||
               violation_type == GBOP_SOURCE_VIOLATION);
#        else
        ASSERT(violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
               violation_type == HOT_PATCH_PROTECTOR_VIOLATION);
#        endif
        ASSERT_OWN_READ_LOCK(true, lock);
        d_r_read_unlock(lock);
    }
#    endif

    if (result_type != NULL)
        *result_type = violation_type;
    return action;
}

/* Meant to be called after security_violation_internal_main().
 * Caller should only call for action!=ACTION_CONTINUE.
 */
void
security_violation_action(dcontext_t *dcontext, action_type_t action, app_pc addr)
{
    ASSERT(action != ACTION_CONTINUE);
    if (action == ACTION_CONTINUE)
        return;

    /* timeout before we take an action */
    if (dynamo_options.timeout) {
        /* For now assuming only current thread sleeps.
           FIXME: If we are about the kill the process anyways,
           it may be safer to stop_the_world,
           so attacks in this time window do not get through.

           TODO: On the other hand sleeping in one thread,
           while the rest are preparing for controlled shutdown sounds better,
           yet we have no way of telling them that process death is pending.
        */
        /* FIXME: shouldn't we suspend all other threads for the messagebox too? */

        /* For services you can get a similar effect to -timeout on kill process
           by settings in Services\service properties\Recovery.
           Restart service after x minutes.
           0 is very useful - then you get your app back immediately.
           1 minute however may be too much in some circumstances,
           Our option is then useful for finer control, e.g. -timeout 10s
        */
        os_timeout(dynamo_options.timeout);
    }

    if (ACTION_THROW_EXCEPTION == action) {
        os_forge_exception(addr, UNREADABLE_MEMORY_EXECUTION_EXCEPTION);
        ASSERT_NOT_REACHED();
    }
    if (ACTION_CONTINUE != action) {
        uint terminate_flags_t = TERMINATE_PROCESS;
        if (is_self_couldbelinking()) {
            /* must be nolinking for terminate cleanup to avoid deadlock w/ flush */
            enter_nolinking(dcontext, NULL, false /*not a real cache transition*/);
        }
        if (action == ACTION_TERMINATE_THREAD) {
            terminate_flags_t = TERMINATE_THREAD;
            /* clean up when terminating a thread */
            terminate_flags_t |= TERMINATE_CLEANUP;
        } else {
            ASSERT(action == ACTION_TERMINATE_PROCESS &&
                   terminate_flags_t == TERMINATE_PROCESS);
        }
#    ifdef HOT_PATCHING_INTERFACE
        ASSERT(!DYNAMO_OPTION(hot_patching) ||
               !READ_LOCK_HELD(hotp_get_lock())); /* See case 7998. */
#    endif
        os_terminate(dcontext, terminate_flags_t);
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
}

/* Caller must call security_violation_action() if return != ACTION_CONTINUE */
static action_type_t
security_violation_main(dcontext_t *dcontext, app_pc addr,
                        security_violation_t violation_type,
                        security_option_t type_handling)
{
    return security_violation_internal_main(dcontext, addr, violation_type, type_handling,
                                            NULL, ACTION_TERMINATE_PROCESS, NULL, NULL);
}

/* See security_violation_internal_main() for further comments.
 *
 * Returns ALLOWING_BAD if on exempt_threat_list, or if in detect mode
 * returns the passed violation_type (a negative value)
 * Does not return if protection action is taken.
 */
security_violation_t
security_violation_internal(dcontext_t *dcontext, app_pc addr,
                            security_violation_t violation_type,
                            security_option_t type_handling, const char *threat_id,
                            const action_type_t desired_action, read_write_lock_t *lock)
{
    security_violation_t result_type;
    action_type_t action =
        security_violation_internal_main(dcontext, addr, violation_type, type_handling,
                                         threat_id, desired_action, lock, &result_type);
    DOKSTATS(if (ACTION_CONTINUE != action) { KSTOP_REWIND_UNTIL(dispatch_num_exits); });
    if (action != ACTION_CONTINUE)
        security_violation_action(dcontext, action, addr);
    return result_type;
}

/* security_violation_internal() is the real function.  This a wrapper exists
 * for two reasons; one, hot patching needs to send extra arguments for event
 * notification and two, existing calls to security_violation() in the code
 * shouldn't have to change the interface.
 */
security_violation_t
security_violation(dcontext_t *dcontext, app_pc addr, security_violation_t violation_type,
                   security_option_t type_handling)
{
    return security_violation_internal(dcontext, addr, violation_type, type_handling,
                                       NULL, ACTION_TERMINATE_PROCESS, NULL);
}

/* back to normal section */
END_DATA_SECTION()
/****************************************************************************/

bool
is_dyngen_vsyscall(app_pc addr)
{
    /* FIXME: on win32, should we only allow portion of page? */
    /* CHECK: likely to be true on all Linux versions by the time we ship */
    /* if vsyscall_page_start == 0, then this exception doesn't apply */
    /* Note vsyscall_page_start is a global defined in the corresponding os.c files */
    if (vsyscall_page_start == 0)
        return false;
    return (addr >= (app_pc)vsyscall_page_start &&
            addr < (app_pc)(vsyscall_page_start + PAGE_SIZE));
}

bool
is_in_futureexec_area(app_pc addr)
{
    bool future;
    d_r_read_lock(&futureexec_areas->lock);
    future = lookup_addr(futureexec_areas, addr, NULL);
    d_r_read_unlock(&futureexec_areas->lock);
    return future;
}

bool
is_dyngen_code(app_pc addr)
{
    uint flags;
    if (get_executable_area_flags(addr, &flags)) {
        /* assuming only true DGC is marked DYNGEN  */
        return TEST(FRAG_DYNGEN, flags);
    }

    return is_in_futureexec_area(addr);
}

/* Returns true if in is a direct jmp targeting a known piece of non-DGC code
 */
static bool
is_direct_jmp_to_image(dcontext_t *dcontext, instr_t *in)
{
    bool ok = false;
    if (instr_get_opcode(in) == OP_jmp && /* no short jmps */
        opnd_is_near_pc(instr_get_target(in))) {
        app_pc target = opnd_get_pc(instr_get_target(in));
        uint flags;
        if (get_executable_area_flags(target, &flags)) {
            /* we could test for UNMOD_IMAGE but that would ruin windows
             * loader touch-ups, which can happen for any dll!
             * so we test FRAG_DYNGEN instead
             */
            ok = !TEST(FRAG_DYNGEN, flags);
        }
    }
    return ok;
}

/* allow original code displaced by a hook, seen for Citrix 4.0 (case 6615):
 *   <zero or more non-cti and non-syscall instrs whose length < 5>
 *   <one more such instr, making length sum X>
 *   jmp <dll:Y>, where <dll:Y-X> contains a jmp to this page
 */
static bool
check_trampoline_displaced_code(dcontext_t *dcontext, app_pc addr, bool on_stack,
                                instrlist_t *ilist, size_t *len)
{
    uint size = 0;
    bool match = false;
    instr_t *in, *last = instrlist_last(ilist);
    ASSERT(DYNAMO_OPTION(trampoline_displaced_code));
    if (on_stack || !is_direct_jmp_to_image(dcontext, last))
        return false;
    ASSERT(instr_length(dcontext, last) == JMP_LONG_LENGTH);
    for (in = instrlist_first(ilist); in != NULL /*sanity*/ && in != last;
         in = instr_get_next(in)) {
        /* build_app_bb_ilist should fully decode everything */
        ASSERT(instr_opcode_valid(in));
        if (instr_is_cti(in) || instr_is_syscall(in) || instr_is_interrupt(in))
            break;
        size += instr_length(dcontext, in);
        if (instr_get_next(in) == last) {
            if (size < JMP_LONG_LENGTH)
                break;
        } else {
            if (size >= JMP_LONG_LENGTH)
                break;
        }
    }
    ASSERT(in != NULL);
    if (in == last) {
        app_pc target;
        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
            "check_trampoline_displaced_code @" PFX ": size=%d\n", addr, size);
        DOLOG(3, LOG_INTERP | LOG_VMAREAS,
              { instrlist_disassemble(dcontext, addr, ilist, THREAD); });
        /* is_direct_jmp_to_image should have checked for us */
        ASSERT(opnd_is_near_pc(instr_get_target(last)));
        target = opnd_get_pc(instr_get_target(last));
        if (is_readable_without_exception(target - size, JMP_LONG_LENGTH)) {
            instr_t *tramp = instr_create(dcontext);
            /* Ensure a racy unmap causing a decode crash is passed to the app */
            set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(target - size));
            target = decode_cti(dcontext, target - size, tramp);
            if (target != NULL && instr_opcode_valid(tramp) && instr_is_ubr(tramp) &&
                opnd_is_near_pc(instr_get_target(tramp))) {
                app_pc hook = opnd_get_pc(instr_get_target(tramp));
                /* FIXME: could be tighter by ensuring that hook targets a jmp
                 * or call right before addr but that may be too specific.
                 * FIXME: if the pattern crosses a page we could fail to match.
                 * we could check for being inside region instead.
                 */
                if (PAGE_START(hook) == PAGE_START(addr)) {
                    *len = size + JMP_LONG_LENGTH;
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "WARNING: allowing hook-displaced code " PFX " -> " PFX " -> " PFX
                        "\n",
                        addr, target, hook);
                    SYSLOG_INTERNAL_WARNING_ONCE("hook-displaced code allowed.");
                    STATS_INC(trampolines_displaced_code);
                    match = true;
                }
            }
            instr_destroy(dcontext, tramp);
        }
    }
    return match;
}

/* other than JITed code, we allow a small set of specific patterns of DGC such
 * as function closure trampolines, which this routine checks for.
 * returns ALLOWING_OK if bb matches, else returns ALLOWING_BAD
 */
static int
check_origins_bb_pattern(dcontext_t *dcontext, app_pc addr, app_pc *base, size_t *size,
                         uint *vm_flags, uint *frag_flags)
{
    /* we assume this is not a cti target (flag diffs will prevent direct cti here)
     * we only check for the bb beginning at addr
     */
    instrlist_t *ilist;
    instr_t *in, *first;
    opnd_t op;
    size_t len = 0;
    int res = ALLOWING_BAD; /* signal to caller not a match */
    bool on_stack = is_on_stack(dcontext, addr, NULL);

    /* FIXME: verify bb memory is readable prior to decoding it
     * we shouldn't get here if addr is unreadable, but rest of bb could be
     * note that may end up looking at win32 GUARD page -- don't need to do
     * anything special since that will look unreadable
     */
    /* FIXME bug 9376: if unreadable check_thread_vm_area() will
     * assert vmlist!=NULL and throw an exception, which is ok
     */
    ilist = build_app_bb_ilist(dcontext, addr, INVALID_FILE);
    first = instrlist_first(ilist);
    if (first == NULL) /* empty bb: perhaps invalid instr */
        goto check_origins_bb_pattern_exit;

    LOG(GLOBAL, LOG_VMAREAS, 3, "check_origins_bb_pattern:\n");
    DOLOG(3, LOG_VMAREAS, { instrlist_disassemble(dcontext, addr, ilist, GLOBAL); });

#    ifndef X86
    /* FIXME: move the x86-specific analysis to an arch/ file! */
    ASSERT_NOT_IMPLEMENTED(false);
#    endif

#    ifdef UNIX
    /* is this a sigreturn pattern placed by kernel on the stack or vsyscall page? */
    if (is_signal_restorer_code(addr, &len)) {
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
            "found signal restorer code @" PFX ", allowing it\n", addr);
        SYSLOG_INTERNAL_WARNING_ONCE("signal restorer code allowed.");
        res = ALLOWING_OK;
        goto check_origins_bb_pattern_exit;
    }
#    endif

    /* is this a closure trampoline that looks like this:
     *   mov immed -> 0x4(esp)             (put frame ptr directly in slot)
     *   jmp known-non-DGC-address
     * or like this (gcc-style, also seen in dfrgui):
     *   mov immed -> %ecx                 (put frame ptr in ecx, callee puts in slot)
     *   jmp known-non-DGC-address
     * OR, is this some sort of C++ exception chaining (seen in soffice):
     *   mov immed -> %eax                 (put try index in eax)
     *   jmp known-non-DGC-address
     * these can be on the stack or on the heap, except the soffice one, which must
     * be on the heap (simply b/c we've never seen it on the stack)
     * all of these must be targeted by a call
     */
    if (instr_get_opcode(first) == OP_mov_imm ||
        /* funny case where store of immed is mov_st -- see ir/decode_table.c */
        (instr_get_opcode(first) == OP_mov_st &&
         opnd_is_immed(instr_get_src(first, 0)))) {
        bool ok = false;
        LOG(GLOBAL, LOG_VMAREAS, 3, "testing for mov immed pattern\n");
        /* mov_imm always has immed src, just check dst */
        op = instr_get_dst(first, 0);
        ok = (opnd_is_near_base_disp(op) && opnd_get_base(op) == REG_XSP &&
              opnd_get_disp(op) == 4 && opnd_get_scale(op) == REG_NULL);

        if (!ok && opnd_is_reg(op) && opnd_get_size(instr_get_src(first, 0)) == OPSZ_4) {
            uint immed = (uint)opnd_get_immed_int(instr_get_src(first, 0));
            /* require immed be addr for ecx, non-addr plus on heap for eax */
            /* FIXME: PAGE_SIZE is arbitrary restriction, assuming eax values
             * are small indices, and it's a nice way to distinguish pointers
             */
            IF_X64(ASSERT_NOT_TESTED()); /* on x64 will these become rcx & rax? */
            ok = (opnd_get_reg(op) == REG_ECX && immed > PAGE_SIZE) ||
                (opnd_get_reg(op) == REG_EAX && immed < PAGE_SIZE && !on_stack);
        }

        if (ok) {
            /* check 2nd instr */
            ok = false;
            len += instr_length(dcontext, first);
            in = instr_get_next(first);
            if (instr_get_next(in) == NULL && /* only 2 instrs in this bb */
                is_direct_jmp_to_image(dcontext, in)) {
                len += instr_length(dcontext, in);
                ok = true;
            } else
                LOG(GLOBAL, LOG_VMAREAS, 3, "2nd instr not jmp to good code!\n");
        } else
            LOG(GLOBAL, LOG_VMAREAS, 3, "immed bad!\n");

        if (ok) {
            /* require source to be known and to be a call
             * cases where source is unknown are fairly pathological
             * (another thread flushing and deleting the fragment, etc.)
             */
            ok = EXIT_IS_CALL(dcontext->last_exit->flags);
        }
        if (ok) {
            LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
                "WARNING: found trampoline block @" PFX ", allowing it\n", addr);
            SYSLOG_INTERNAL_WARNING_ONCE("trampoline DGC allowed.");
            res = ALLOWING_OK;
            goto check_origins_bb_pattern_exit;
        }
    }

    /* is this a PLT-type push/jmp, where the push uses its own address
     * (this is seen in soffice):
     *   push own-address
     *   jmp known-non-DGC-address
     */
    if (instr_get_opcode(first) == OP_push_imm &&
        opnd_get_size(instr_get_src(first, 0)) == OPSZ_4) {
        ptr_uint_t immed = opnd_get_immed_int(instr_get_src(first, 0));
        LOG(GLOBAL, LOG_VMAREAS, 3, "testing for push immed pattern\n");
        if ((app_pc)immed == addr) {
            len += instr_length(dcontext, first);
            in = instr_get_next(first);
            if (instr_get_next(in) == NULL && /* only 2 instrs in this bb */
                is_direct_jmp_to_image(dcontext, in)) {
                len += instr_length(dcontext, in);
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
                    "WARNING: found push/jmp block @" PFX ", allowing it\n", addr);
                SYSLOG_INTERNAL_WARNING_ONCE("push/jmp DGC allowed.");
                res = ALLOWING_OK;
                goto check_origins_bb_pattern_exit;
            }
        }
    }

    /* look for the DGC ret on the stack that office xp uses, beyond TOS!
     * it varies between having no arg or having an immed arg -- my guess
     * is they use it to handle varargs with stdcall: callee must
     * clean up args but has to deal w/ dynamically varying #args, so
     * they use DGC ret, only alternative is jmp* and no ret
     */
    if (instr_is_return(first) && on_stack &&
        addr < (app_pc)get_mcontext(dcontext)->xsp) { /* beyond TOS */
        ASSERT(instr_get_next(first) == NULL);        /* bb should have only ret in it */
        len = instr_length(dcontext, first);
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
            "WARNING: found ret-beyond-TOS @" PFX ", allowing it\n", addr);
        SYSLOG_INTERNAL_WARNING_ONCE("ret-beyond-TOS DGC allowed.");
        res = ALLOWING_OK;
        goto check_origins_bb_pattern_exit;
    }

    if (DYNAMO_OPTION(trampoline_dirjmp) && !on_stack &&
        is_direct_jmp_to_image(dcontext, first)) {
        /* should be a lone jmp */
        ASSERT(instr_get_next(first) == NULL);
        len = instr_length(dcontext, first);
        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
            "WARNING: allowing targeted direct jmp @" PFX "\n", addr);
        SYSLOG_INTERNAL_WARNING_ONCE("trampoline direct jmp allowed.");
        STATS_INC(trampolines_direct_jmps);
        res = ALLOWING_OK;
        goto check_origins_bb_pattern_exit;
    }

    /* allow a .NET COM method table: a lone direct call on the heap, and a
     * ret immediately preceding it (see case 3558 and case 3564)
     */
    if (DYNAMO_OPTION(trampoline_dircall) && !on_stack && instr_is_call_direct(first)) {
        len = instr_length(dcontext, first);
        /* ignore rest of ilist -- may or may not follow call for real bb, as
         * will have separate calls to check_thread_vm_area() and thus
         * separate code origins checks being applied to the target, making this
         * not really a security hole at all as attack could have sent control
         * directly to target
         */
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
            "WARNING: allowing targeted direct call @" PFX "\n", addr);
        SYSLOG_INTERNAL_WARNING_ONCE("trampoline direct call allowed.");
        STATS_INC(trampolines_direct_calls);
        res = ALLOWING_OK;
        goto check_origins_bb_pattern_exit;
    }
    if (DYNAMO_OPTION(trampoline_com_ret) && !on_stack && instr_is_return(first)) {
        app_pc nxt_pc = addr + instr_length(dcontext, first);
        if (is_readable_without_exception(nxt_pc, MAX_INSTR_LENGTH)) {
            instr_t *nxt = instr_create(dcontext);
            /* WARNING: until our decoding is more robust, as this is AFTER a
             * ret this could fire a decode assert if not actually code there,
             * so we avoid any more decoding than we have to do w/ decode_cti.
             */
            /* A racy unmap could cause a fault here so we track the page
             * that's being decoded. */
            set_thread_decode_page_start(dcontext, (app_pc)PAGE_START(nxt_pc));
            nxt_pc = decode_cti(dcontext, nxt_pc, nxt);
            if (nxt_pc != NULL && instr_opcode_valid(nxt) && instr_is_call_direct(nxt)) {
                /* actually we don't get here w/ current native_exec early-gateway
                 * design since we go native at the PREVIOUS call to this ret's call
                 */
                ASSERT_NOT_TESTED();
                instr_destroy(dcontext, nxt);
                len = instr_length(dcontext, first);
                LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
                    "WARNING: allowing .NET COM ret in method table @" PFX "\n", addr);
                SYSLOG_INTERNAL_WARNING_ONCE(".NET COM method table ret allowed.");
                STATS_INC(trampolines_com_rets);
                res = ALLOWING_OK;
                goto check_origins_bb_pattern_exit;
            }
            instr_destroy(dcontext, nxt);
        }
    }

    if (DYNAMO_OPTION(trampoline_displaced_code) &&
        check_trampoline_displaced_code(dcontext, addr, on_stack, ilist, &len)) {
        res = ALLOWING_OK;
        goto check_origins_bb_pattern_exit;
    }

check_origins_bb_pattern_exit:
    if (res == ALLOWING_OK) {
        /* bb matches pattern, let's allow it, but only this block, not entire region! */
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
            "Trimming exec area " PFX "-" PFX " to match pattern bb " PFX "-" PFX "\n",
            *base, *base + *size, addr, addr + len);
        *base = addr;
        ASSERT(len > 0);
        *size = len;
        /* Since this is a sub-page region that shouldn't be frequently
         * executed, it's best to use sandboxing.
         */
        *frag_flags |= SANDBOX_FLAG();
        /* ensure another thread is not able to use this memory region for
         * a non-pattern-matching code sequence
         */
        *vm_flags |= VM_PATTERN_REVERIFY;
        STATS_INC(num_selfmod_vm_areas);
    }
    instrlist_clear_and_destroy(dcontext, ilist);
    return res;
}

/* trims [base, base+size) to its intersection with [start, end)
 * NOTE - regions are required to intersect */
static void
check_origins_trim_region_helper(app_pc *base /*INOUT*/, size_t *size /*INOUT*/,
                                 app_pc start, app_pc end)
{
    app_pc original_base = *base;
    ASSERT(!POINTER_OVERFLOW_ON_ADD(*base, *size)); /* shouldn't overflow */
    ASSERT(start < end); /* [start, end) should be an actual region */
    ASSERT(*base + *size > start && *base < end); /* region must intersect */
    LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
        "Trimming exec area " PFX "-" PFX " to intersect area " PFX "-" PFX "\n", *base,
        *base + *size, start, end);
    *base = MAX(*base, start);
    /* don't use new base here! (case 8152) */
    *size = MIN(original_base + *size, end) - *base;
}

/* Checks if the given PC is trusted and to what level
 * if execution for the referenced area is not allowed program execution
 * should be aborted
 */
static INLINE_ONCE security_violation_t
check_origins_helper(dcontext_t *dcontext, app_pc addr, app_pc *base, size_t *size,
                     uint prot, uint *vm_flags, uint *frag_flags, const char *modname)
{
    vm_area_t *fut_area;

    if (is_dyngen_vsyscall(addr) && *size == PAGE_SIZE && (prot & MEMPROT_WRITE) == 0) {
        /* FIXME: don't allow anyone to make this region writable? */
        LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
            PFX " is the vsyscall page, ok to execute\n", addr);
        return ALLOWING_OK;
    }
#    if 0
    /* this syslog causes services.exe to hang (ref case 666) once case 666
     * is fixed re-enable if desired FIXME */
    SYSLOG_INTERNAL_WARNING_ONCE("executing region at "PFX" not on executable list.",
                                 addr);
#    else
    LOG(GLOBAL, LOG_VMAREAS, 1,
        "executing region at " PFX " not on executable list. Thread %d\n", addr,
        dcontext->owning_thread);
#    endif

    if (USING_FUTURE_EXEC_LIST) {
        bool ok;
        bool once_only;
        d_r_read_lock(&futureexec_areas->lock);
        ok = lookup_addr(futureexec_areas, addr, &fut_area);
        if (!ok)
            d_r_read_unlock(&futureexec_areas->lock);
        else {
            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                "WARNING: pc = " PFX " is future executable, allowing\n", addr);
            LOG(GLOBAL, LOG_INTERP | LOG_VMAREAS, 2,
                "WARNING: pc = " PFX " is future executable, allowing\n", addr);
#    if 0
            /* this syslog causes services.exe to hang (ref case 666)
             * once case 666 is fixed re-enable if desired FIXME */
            SYSLOG_INTERNAL_WARNING_ONCE("future executable region allowed.");
#    else
            DODEBUG_ONCE(LOG(GLOBAL, LOG_ALL, 1, "future executable region allowed."));
#    endif
            if (*base < fut_area->start || *base + *size > fut_area->end) {
                check_origins_trim_region_helper(base, size, fut_area->start,
                                                 fut_area->end);
            }
            once_only = TEST(VM_ONCE_ONLY, fut_area->vm_flags);
            /* now done w/ fut_area */
            d_r_read_unlock(&futureexec_areas->lock);
            fut_area = NULL;
            if (is_on_stack(dcontext, addr, NULL)) {
                /* normally futureexec regions are persistent, to allow app to
                 * repeatedly write and then execute (yes this happens a lot).
                 * we don't want to do that for the stack, b/c it amounts to
                 * permanently allowing a certain piece of stack to be executed!
                 * besides, we don't see the write-exec iter scheme for the stack.
                 */
                STATS_INC(num_exec_future_stack);
                LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                    "future exec " PFX "-" PFX
                    " is on stack, removing from future list\n",
                    *base, *base + *size);
                ok = remove_futureexec_vm_area(*base, *base + *size);
                ASSERT(ok);
            } else {
                STATS_INC(num_exec_future_heap);
                if (!DYNAMO_OPTION(selfmod_futureexec)) {
                    /* if on all-selfmod pages, then we shouldn't need to keep it on
                     * the futureexec list
                     */
                    if (is_executable_area_on_all_selfmod_pages(*base, *base + *size))
                        once_only = true;
                }
                if (once_only) {
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "future exec " PFX "-" PFX " is once-only, removing from "
                        "future list\n",
                        *base, *base + *size);
                    ok = remove_futureexec_vm_area(*base, *base + *size);
                    ASSERT(ok);
                    STATS_INC(num_exec_future_once);
                }
            }
            *vm_flags |= VM_WAS_FUTURE;
            return ALLOWING_OK;
        }
    }

    if (DYNAMO_OPTION(executable_if_text) || DYNAMO_OPTION(executable_if_rx_text) ||
        (DYNAMO_OPTION(exempt_text) || !IS_STRING_OPTION_EMPTY(exempt_text_list))) {
        app_pc modbase = get_module_base(addr);
        if (modbase != NULL) { /* PE, and is readable */
            /* note that it could still be a PRIVATE mapping */
            /* don't expand region to match actual text section bounds -- if we split
             * let's keep this region smaller.
             */
            app_pc sec_start = NULL, sec_end = NULL;
            if (is_in_code_section(modbase, addr, &sec_start, &sec_end)) {
                bool allow = false;
                if (DYNAMO_OPTION(executable_if_text)) {
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "exec region is in code section of module @" PFX " (%s), "
                        "allowing\n",
                        modbase, modname == NULL ? "<invalid name>" : modname);
                    STATS_INC(num_text);
                    mark_module_exempted(addr);
                    allow = true;
                } else {
                    uint memprot = 0;
                    list_default_or_append_t deflist = LIST_NO_MATCH;
                    /* Xref case 10526, in the common case app_mem_prot_change() adds
                     * this region, however it can miss -> rx transitions if they
                     * overlapped more then one section (fixing it to do so would
                     * require signifigant restructuring of that routine, see comments
                     * there) so we also check here. */
                    if (DYNAMO_OPTION(executable_if_rx_text) &&
                        get_memory_info(addr, NULL, NULL, &memprot) &&
                        (TEST(MEMPROT_EXEC, memprot) && !TEST(MEMPROT_WRITE, memprot))) {
                        /* matches -executable_if_rx_text */
                        /* case 9799: we don't mark exempted for default-on options */
                        allow = true;
                        SYSLOG_INTERNAL_WARNING_ONCE("allowable rx text section not "
                                                     "found till check_origins");
                    }
                    if (!allow && modname != NULL) {
                        bool onlist;
                        string_option_read_lock();
                        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
                            "exec region is in code section of module %s, vs list %s\n",
                            modname, DYNAMO_OPTION(exempt_text_list));
                        onlist = check_filter(DYNAMO_OPTION(exempt_text_list), modname);
                        string_option_read_unlock();
                        if (onlist) {
                            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                                "module %s is on text list, allowing execution\n",
                                modname);
                            STATS_INC(num_text_list);
                            SYSLOG_INTERNAL_WARNING_ONCE("code origins: module %s text "
                                                         "section exempt",
                                                         modname);
                            mark_module_exempted(addr);
                            allow = true;
                        }
                    }

                    if (!allow && modname != NULL) {
                        deflist = check_list_default_and_append(
                            dynamo_options.exempt_mapped_image_text_default_list,
                            dynamo_options.exempt_mapped_image_text_list, modname);
                    }
                    if (deflist != LIST_NO_MATCH) {
                        bool image_mapping = is_mapped_as_image(modbase);
                        if (image_mapping) {
                            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                                "module %s is on text list, of a mapped IMAGE"
                                " allowing execution\n",
                                modname);
                            STATS_INC(num_image_text_list);
                            SYSLOG_INTERNAL_WARNING_ONCE("code origins: module %s IMAGE "
                                                         "text section exempt",
                                                         modname);
                            if (deflist == LIST_ON_APPEND) /* case 9799: not default */
                                mark_module_exempted(addr);
                            allow = true;
                        } else {
                            ASSERT_NOT_TESTED();
                            SYSLOG_INTERNAL_WARNING_ONCE("code origins: module %s text "
                                                         "not IMAGE, attack!",
                                                         modname);
                        }
                    }
                }
                if (allow) {
                    /* trim exec area to allowed bounds */
                    check_origins_trim_region_helper(base, size, sec_start, sec_end);
                    return ALLOWING_OK;
                }
            }
        }
    }

    if (DYNAMO_OPTION(executable_if_dot_data) ||
        DYNAMO_OPTION(executable_if_dot_data_x) ||
        (DYNAMO_OPTION(exempt_dot_data) &&
         !IS_STRING_OPTION_EMPTY(exempt_dot_data_list)) ||
        (DYNAMO_OPTION(exempt_dot_data_x) &&
         !IS_STRING_OPTION_EMPTY(exempt_dot_data_x_list))) {
        /* FIXME: get_module_base() is called all over in this function.
         *        This function could do with some refactoring. */
        app_pc modbase = get_module_base(addr);
        if (modbase != NULL) {
            /* A loaded module exists for addr; now see if addr is in .data. */
            app_pc sec_start = NULL, sec_end = NULL;
            if (is_in_dot_data_section(modbase, addr, &sec_start, &sec_end)) {
                bool allow = false;
                bool onlist = false;
                uint memprot = 0;
                if (!DYNAMO_OPTION(executable_if_dot_data) &&
                    DYNAMO_OPTION(exempt_dot_data) &&
                    !IS_STRING_OPTION_EMPTY(exempt_dot_data_list)) {
                    if (modname != NULL) {
                        string_option_read_lock();
                        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
                            "exec region is in data of module %s, vs list %s\n", modname,
                            DYNAMO_OPTION(exempt_dot_data_list));
                        onlist =
                            check_filter(DYNAMO_OPTION(exempt_dot_data_list), modname);
                        string_option_read_unlock();
                        DOSTATS({
                            if (onlist)
                                STATS_INC(num_dot_data_list);
                        });
                    }
                }
                DOSTATS({
                    if (DYNAMO_OPTION(executable_if_dot_data))
                        STATS_INC(num_dot_data);
                });
                if (onlist || DYNAMO_OPTION(executable_if_dot_data)) {
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "exec region is in .data section of module %s\n",
                        modname == NULL ? "<invalid name>" : modname);
                    SYSLOG_INTERNAL_WARNING_ONCE(
                        "code origins: .data section of module %s exempt",
                        modname == NULL ? "<invalid name>" : modname);
                    /* case 9799: FIXME: we don't want to mark as exempted for the
                     * default modules on the list: should split into a separate
                     * default list so we can tell!  Those modules will have private
                     * pcaches if in a process w/ ANY exemption options */
                    mark_module_exempted(addr);
                    allow = true;
                    ;
                }
                if (!allow && get_memory_info(addr, NULL, NULL, &memprot) &&
                    TEST(MEMPROT_EXEC, memprot)) {
                    /* check the _x versions */
                    if (!DYNAMO_OPTION(executable_if_dot_data_x) &&
                        DYNAMO_OPTION(exempt_dot_data_x) &&
                        !IS_STRING_OPTION_EMPTY(exempt_dot_data_x_list)) {
                        if (modname != NULL) {
                            string_option_read_lock();
                            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
                                "exec region is in x data of module %s, vs list %s\n",
                                modname, DYNAMO_OPTION(exempt_dot_data_x_list));
                            onlist = check_filter_with_wildcards(
                                DYNAMO_OPTION(exempt_dot_data_x_list), modname);
                            string_option_read_unlock();
                            DOSTATS({
                                if (onlist)
                                    STATS_INC(num_dot_data_x_list);
                            });
                        }
                        DOSTATS({
                            if (DYNAMO_OPTION(executable_if_dot_data_x))
                                STATS_INC(num_dot_data_x);
                        });
                    }
                    if (DYNAMO_OPTION(executable_if_dot_data_x) || onlist) {
                        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                            "exec region is in x .data section of module %s\n",
                            modname == NULL ? "<invalid name>" : modname);
                        SYSLOG_INTERNAL_WARNING_ONCE(
                            "code origins: .data section of module %s exempt",
                            modname == NULL ? "<invalid name>" : modname);
                        /* case 9799: FIXME: we don't want to mark as exempted for
                         * the default modules on the list: should split into a
                         * separate default list so we can tell!  Those modules will
                         * have private pcaches if in a process w/ ANY exemption
                         * options */
                        mark_module_exempted(addr);
                        allow = true;
                    }
                }
                if (allow) {
                    /* trim exec area to allowed bounds */
                    check_origins_trim_region_helper(base, size, sec_start, sec_end);
                    return ALLOWING_OK;
                }
            }
        }
    }

    if (DYNAMO_OPTION(executable_if_image) ||
        (DYNAMO_OPTION(exempt_image) && !IS_STRING_OPTION_EMPTY(exempt_image_list)) ||
        !moduledb_exempt_list_empty(MODULEDB_EXEMPT_IMAGE)) {
        app_pc modbase = get_module_base(addr);
        if (modbase != NULL) {
            /* A loaded module exists for addr; we allow the module (xref 10526 we
             * used to limit to just certain sections).  FIXME - we could use the
             * relaxed is_in_any_section here, but other relaxations (such as dll2heap)
             * exclude the entire module so need to match that to prevent there being
             * non exemptable areas. */
            bool onlist = false;
            bool mark_exempted = true;
            if (!DYNAMO_OPTION(executable_if_image)) {
                if (modname != NULL) {
                    string_option_read_lock();
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
                        "exec region is in image of module %s, vs list %s\n", modname,
                        DYNAMO_OPTION(exempt_image_list));
                    onlist = check_filter(DYNAMO_OPTION(exempt_image_list), modname);
                    string_option_read_unlock();
                    DOSTATS({
                        if (onlist)
                            STATS_INC(num_exempt_image_list);
                    });
                    if (!onlist && !moduledb_exempt_list_empty(MODULEDB_EXEMPT_IMAGE)) {
                        onlist =
                            moduledb_check_exempt_list(MODULEDB_EXEMPT_IMAGE, modname);
                        DOSTATS({
                            if (onlist)
                                STATS_INC(num_moduledb_exempt_image);
                        });
                        /* FIXME - could be that a later policy would
                         * allow this in which case we shouldn't report,
                         * however from layout this is should be the last
                         * place that could allow this target. */
                        if (onlist) {
                            /* Case 9799: We don't want to set this for
                             * default-on options like moduledb to avoid
                             * non-shared pcaches when other exemption options
                             * are turned on in the process.
                             */
                            mark_exempted = false;
                            moduledb_report_exemption("Moduledb image exemption"
                                                      " " PFX " to " PFX " from "
                                                      "module %s",
                                                      *base, *base + *size, modname);
                        }
                    }
                }
            } else {
                STATS_INC(num_exempt_image);
            }
            if (onlist || DYNAMO_OPTION(executable_if_image)) {
                LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                    "exec region is in the loaded image of module %s\n",
                    modname == NULL ? "<invalid name>" : modname);
                SYSLOG_INTERNAL_WARNING_ONCE("code origins: loaded image of module %s"
                                             "exempt",
                                             modname == NULL ? "<invalid name>"
                                                             : modname);
                if (mark_exempted)
                    mark_module_exempted(addr);
                return ALLOWING_OK;
            }
        }
    }

    if (((DYNAMO_OPTION(exempt_dll2heap) &&
          !IS_STRING_OPTION_EMPTY(exempt_dll2heap_list)) ||
         !moduledb_exempt_list_empty(MODULEDB_EXEMPT_DLL2HEAP) ||
         (DYNAMO_OPTION(exempt_dll2stack) &&
          !IS_STRING_OPTION_EMPTY(exempt_dll2stack_list)) ||
         !moduledb_exempt_list_empty(MODULEDB_EXEMPT_DLL2STACK)) &&
        /* FIXME: any way to find module info for deleted source? */
        !LINKSTUB_FAKE(dcontext->last_exit)) {
        /* no cutting corners here -- find exact module that exit cti is from */
        app_pc modbase;
        app_pc translated_pc = recreate_app_pc(
            dcontext, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
            dcontext->last_fragment);
        ASSERT(translated_pc != NULL);
        modbase = get_module_base(translated_pc);
        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
            "check_origins: dll2heap and dll2stack for " PFX ": cache " PFX " => app " PFX
            " == mod " PFX "\n",
            addr, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
            translated_pc, modbase);
        if (modbase != NULL) { /* PE, and is readable */
            if (modname != NULL) {
                bool onheaplist = false, onstacklist = false;
                bool on_moddb_heaplist = false, on_moddb_stacklist = false;
                string_option_read_lock();
                LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
                    "source region is in module %s\n", modname);
                if (DYNAMO_OPTION(exempt_dll2heap)) {
                    onheaplist =
                        check_filter(DYNAMO_OPTION(exempt_dll2heap_list), modname);
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3, "exempt heap list: %s\n",
                        DYNAMO_OPTION(exempt_dll2heap_list));
                }
                if (DYNAMO_OPTION(exempt_dll2stack)) {
                    onstacklist =
                        check_filter(DYNAMO_OPTION(exempt_dll2stack_list), modname);
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3, "exempt stack list: %s\n",
                        DYNAMO_OPTION(exempt_dll2stack_list));
                }
                string_option_read_unlock();
                if (!onheaplist) {
                    on_moddb_heaplist =
                        moduledb_check_exempt_list(MODULEDB_EXEMPT_DLL2HEAP, modname);
                }
                if (!onstacklist) {
                    on_moddb_stacklist =
                        moduledb_check_exempt_list(MODULEDB_EXEMPT_DLL2STACK, modname);
                }

                /* make sure targeting non-stack, non-module memory */
                if ((onheaplist || on_moddb_heaplist) &&
                    !is_on_stack(dcontext, addr, NULL) && get_module_base(addr) == NULL) {
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "source module %s is on exempt list, target is heap => allowing "
                        "execution\n",
                        modname);
                    if (on_moddb_heaplist) {
                        STATS_INC(num_moduledb_exempt_dll2heap);
                        moduledb_report_exemption("Moduledb dll2heap exemption " PFX " to"
                                                  " " PFX " from module %s",
                                                  translated_pc, addr, modname);
                    } else {
                        STATS_INC(num_exempt_dll2heap);
                        SYSLOG_INTERNAL_WARNING_ONCE("code origins: dll2heap from %s "
                                                     "exempt",
                                                     modname);
                    }
                    return ALLOWING_OK;
                }
                if ((onstacklist || on_moddb_stacklist) &&
                    is_on_stack(dcontext, addr, NULL)) {
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "source module %s is on exempt list, target is stack => allowing"
                        "execution\n",
                        modname);
                    if (on_moddb_stacklist) {
                        STATS_INC(num_moduledb_exempt_dll2stack);
                        moduledb_report_exemption("Moduledb dll2stack exemption " PFX " "
                                                  "to " PFX " from module %s",
                                                  translated_pc, addr, modname);
                    } else {
                        SYSLOG_INTERNAL_WARNING_ONCE("code origins: dll2stack from %s is"
                                                     " exempt",
                                                     modname);
                        STATS_INC(num_exempt_dll2stack);
                    }
                    return ALLOWING_OK;
                }
            }
        }
    }

    if (dynamo_options.executable_if_trampoline) {
        /* check for specific bb patterns we allow */
        if (check_origins_bb_pattern(dcontext, addr, base, size, vm_flags, frag_flags) ==
            ALLOWING_OK) {
            DOSTATS({
                if (is_on_stack(dcontext, addr, NULL)) {
                    STATS_INC(num_trampolines_stack);
                } else {
                    STATS_INC(num_trampolines_heap);
                }
            });
            return ALLOWING_OK;
        }
    }

    if (DYNAMO_OPTION(executable_if_driver)) {
        if (TEST(VM_DRIVER_ADDRESS, *vm_flags)) {
            ASSERT(*size == PAGE_SIZE);
            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                "check origins: pc = " PFX " is in a new driver area\n", addr);
            STATS_INC(num_driver_areas);
            return ALLOWING_OK;
        }
    }

    if (is_on_stack(dcontext, addr, NULL)) {
        /* WARNING: stack check not bulletproof since attackers control esp */
        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
            "check origins: pc = " PFX " is on the stack\n", addr);
        STATS_INC(num_stack_violations);
        if (!dynamo_options.executable_stack) {
            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 1,
                "ERROR: Address " PFX " on the stack is not executable!\n", addr);
            return STACK_EXECUTION_VIOLATION;
        } else {
            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 1,
                "WARNING: Execution violation @ stack address " PFX " detected. "
                "Continuing...\n",
                addr);
            return ALLOWING_BAD;
        }
    } else {
        STATS_INC(num_heap_violations);
        if (!dynamo_options.executable_heap) {
            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 1,
                "ERROR: Address " PFX " on the heap is not executable!\n", addr);
            SYSLOG_INTERNAL_WARNING_ONCE("Address " PFX " on the heap is not executable",
                                         addr);
            return HEAP_EXECUTION_VIOLATION;
        } else {
            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 1,
                "WARNING: Execution violation @ heap address " PFX " detected. "
                "Continuing...\n",
                addr);
            return ALLOWING_BAD;
        }
    }

    /* CHECK: why did we get here? */
    ASSERT_NOT_REACHED();
}

/* It is up to the caller to raise a violation if return value is < 0 */
static INLINE_ONCE int
check_origins(dcontext_t *dcontext, app_pc addr, app_pc *base, size_t *size, uint prot,
              uint *vm_flags, uint *frag_flags, bool xfer)
{
    security_violation_t res;
    /* Many exemptions need to know the module name, so we obtain here */
    char modname_buf[MAX_MODNAME_INTERNAL];
    const char *modname = os_get_module_name_buf_strdup(
        addr, modname_buf, BUFFER_SIZE_ELEMENTS(modname_buf) HEAPACCT(ACCT_VMAREAS));

    ASSERT(DYNAMO_OPTION(code_origins));
    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3, "check origins: pc = " PFX "\n", addr);
    res = check_origins_helper(dcontext, addr, base, size, prot, vm_flags, frag_flags,
                               modname);
#    ifdef DGC_DIAGNOSTICS
    if (res != ALLOWING_OK) {
        /* set flag so we can call this area BAD in the future */
        *frag_flags |= FRAG_DYNGEN_RESTRICTED;
    }
#    endif
    if (res < 0) {
        /* if_x shouldn't have to check here, should catch all regions marked x
         * at DR init time or app allocation time
         */
        /* FIXME: turn these into a SYSLOG_INTERNAL_WARNING_ONCE(in case an
         * external agent has added that code)
         * and then we'd need to add them now.
         * FIXME: xref case 3742
         */
        ASSERT_BUG_NUM(3742,
                       !DYNAMO_OPTION(executable_if_x) || !TEST(MEMPROT_EXEC, prot));
        ASSERT(!DYNAMO_OPTION(executable_if_rx) || !TEST(MEMPROT_EXEC, prot) ||
               TEST(MEMPROT_WRITE, prot));
    }
    if (modname != NULL && modname != modname_buf)
        dr_strfree(modname HEAPACCT(ACCT_VMAREAS));
    return res;
}

/* returns whether it ended up deleting the self-writing fragment
 * by flushing the region
 */
bool
vm_area_fragment_self_write(dcontext_t *dcontext, app_pc tag)
{
    if (!dynamo_options.executable_stack && is_on_stack(dcontext, tag, NULL)) {
        /* stack code is NOT persistently executable, nor is it allowed to be
         * written, period!  however, in keeping with our philosophy of only
         * interfering with the program when it executes, we don't stop it
         * at the write here, we simply remove the code from the executable
         * list and remove its sandboxing.  after all, the code on the stack
         * may be finished with, and now the stack is just being used as data!
         *
         * FIXME: there is a hole here due to selfmod fragments
         * being private: a second thread can write to a stack region and then
         * execute from the changed region w/o kicking it off the executable
         * list.  case 4020 fixed this for pattern-matched regions.
         */
        bool ok;
        vm_area_t *area = NULL;
        app_pc start, end;
        d_r_read_lock(&executable_areas->lock);
        ok = lookup_addr(executable_areas, tag, &area);
        ASSERT(ok);
        /* grab fields since can't hold lock entire time */
        start = area->start;
        end = area->end;
        d_r_read_unlock(&executable_areas->lock);
        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 1,
            "WARNING: code on stack " PFX "-" PFX " @tag " PFX " written to\n", start,
            end, tag);
        SYSLOG_INTERNAL_WARNING_ONCE("executable code on stack written to.");
        /* FIXME: fragment could extend into multiple areas, we should flush
         * them all to cover the written-to region (which we don't know)
         */
        flush_fragments_and_remove_region(dcontext, start, end - start,
                                          false /* don't own initexit_lock */,
                                          false /* keep futures */);
        return true;
    }
    return false;
}
#endif /* PROGRAM_SHEPHERDING ******************************************/

#ifdef SIMULATE_ATTACK

enum {
    SIMULATE_INIT = 0,
    SIMULATE_GENERIC = 1,
    SIMULATE_AT_ADDR = 2,
    SIMULATE_AT_FRAGNUM = 4,
    SIMULATE_WIPE_STACK = 8,
    SIMULATE_OVER = 0x1000,
};

/* attack simulation list */
/* comma separated list of simulate points.
     @fragnum
        fragment number available only in DEBUG builds
     0xfragpc
        will test addr only whenever check_thread_vm_area is called
        start of bb, pc at end of direct cti instr, target of direct cti,
        pc at end of final instr in bb
     s: prefix wipes the stack
   Ex: -simulate_at @100,s:@150,0x77e9e8d6,s:0x77e9e8f0,@777,@2000,s:@19999,@29999
*/
/* simulate_at is modified in place, hence caller needs to synchronize
   and should be 0 after the first call just like strtok */
int
next_simulate_at_fragment(char **tokpos /* OUT */, int *action /* OUT */)
{
    char *fragnum;

    // assumes sscanf won't get confused with the ,s
    for (fragnum = *tokpos; fragnum; fragnum = *tokpos) {
        int num;
        *tokpos = strchr(fragnum, ','); /* next ptr */
        if (*tokpos)
            (*tokpos)++;

        if (sscanf(fragnum, PIFX, &num) == 1) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "next_simulate_at_fragment: %s=" PIFX " addr\n",
                fragnum, num);
            *action = SIMULATE_AT_ADDR;
            return num;
        } else if (sscanf(fragnum, "s:" PIFX, &num) == 1) {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "next_simulate_at_fragment: wipe stack %s=" PIFX "\n", fragnum, num);
            *action = SIMULATE_WIPE_STACK | SIMULATE_AT_ADDR;
            return num;
        }
#    ifdef DEBUG /* for fragment count */
        else if (sscanf(fragnum, "s:@%d", &num) == 1) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "next_simulate_at_fragment: wipe stack %s=%d\n",
                fragnum, num);
            *action = SIMULATE_WIPE_STACK | SIMULATE_AT_FRAGNUM;
            return num;
        } else if (sscanf(fragnum, "@%d", &num) == 1) {
            LOG(GLOBAL, LOG_VMAREAS, 1, "next_simulate_at_fragment: %s=%d num\n", fragnum,
                num);
            *action = SIMULATE_AT_FRAGNUM;
            return num;
        }
#    endif
        else {
            LOG(GLOBAL, LOG_VMAREAS, 1, "next_simulate_at_fragment: frg=%s ignored\n",
                fragnum);
        }
    }

    *action = SIMULATE_OVER;
    LOG(GLOBAL, LOG_VMAREAS, 1, "next_simulate_at_fragment: simulate attack over\n");

    return 0;
}

void
simulate_attack(dcontext_t *dcontext, app_pc pc)
{
    static char *tokpos;
    static int next_frag = 0; /* number or address */
    static int action = SIMULATE_INIT;

    bool attack = false;

    if (TEST(SIMULATE_AT_FRAGNUM, action)) {
        attack = GLOBAL_STAT(num_fragments) > next_frag;
    }
    if (TEST(SIMULATE_AT_ADDR, action)) {
        if (pc == (app_pc)next_frag)
            attack = true;
    }

    if (attack) {
        LOG(GLOBAL, LOG_VMAREAS, 1, "SIMULATE ATTACK for " PFX " @%d frags\n", pc,
            GLOBAL_STAT(num_fragments));

        if (TEST(SIMULATE_WIPE_STACK, action)) {
            reg_t esp = get_mcontext(dcontext)->xsp;
            uint overflow_size = 1024;
            LOG(THREAD_GET, LOG_VMAREAS, 1,
                "simulate_attack: wipe stack " PFX "-" PFX "\n", esp,
                esp + overflow_size - 1);

            /* wipe out a good portion of the app stack */
            memset((void *)esp, 0xbf, overflow_size); /* LOOK for 0xbf in the log */
            LOG(THREAD_GET, LOG_VMAREAS, 1,
                "simulate_attack: wiped stack " PFX "-" PFX "\n", esp,
                esp + overflow_size - 1);

            /* FIXME: we may want to just wipe the stack and return to app */
        }
    }

    /* prepare for what to do next */
    if (attack || action == SIMULATE_INIT) {
        d_r_mutex_lock(&simulate_lock);
        string_option_read_lock();
        tokpos = dynamo_options.simulate_at;
        if (action == SIMULATE_INIT) {
            if ('\0' == *tokpos)
                tokpos = NULL;
        }
        next_frag = next_simulate_at_fragment(&tokpos, &action);
        /* dynamic changes to the string may have truncated it in front of original */
        ASSERT(tokpos < strchr(dynamo_options.simulate_at, '\0'));
        string_option_read_unlock();
        /* FIXME: tokpos ptr is kept beyond release of lock! */
        d_r_mutex_unlock(&simulate_lock);
    }

    if (attack) {
        security_violation(dcontext, pc, ATTACK_SIMULATION_VIOLATION,
                           OPTION_BLOCK | OPTION_REPORT);
    }
}
#endif /* SIMULATE_ATTACK */

#if defined(DEBUG) && defined(INTERNAL)
static void
print_entry(dcontext_t *dcontext, fragment_t *entry, const char *prefix)
{
    if (entry == NULL)
        LOG(THREAD, LOG_VMAREAS, 1, "%s<NULL>\n", prefix);
    else if (FRAG_MULTI(entry)) {
        if (FRAG_MULTI_INIT(entry)) {
            LOG(THREAD, LOG_VMAREAS, 1, "%s" PFX " <init: tag=" PFX "> pc=" PFX "\n",
                prefix, entry, FRAG_FRAG(entry), FRAG_PC(entry));
        } else {
            LOG(THREAD, LOG_VMAREAS, 1, "%s" PFX " F=" PFX " pc=" PFX "\n", prefix, entry,
                FRAG_FRAG(entry), FRAG_PC(entry));
        }
    } else {
        fragment_t *f = (fragment_t *)entry;
        LOG(THREAD, LOG_VMAREAS, 1, "%s" PFX " F%d tag=" PFX "\n", prefix, f, f->id,
            f->tag);
    }
}

static void
print_fraglist(dcontext_t *dcontext, vm_area_t *area, const char *prefix)
{
    fragment_t *entry, *last;
    LOG(THREAD, LOG_VMAREAS, 1, "%sFragments for area (" PFX ") " PFX ".." PFX "\n",
        prefix, area, area->start, area->end);
    for (entry = area->custom.frags, last = NULL; entry != NULL;
         last = entry, entry = FRAG_NEXT(entry)) {
        print_entry(dcontext, entry, "\t");
        DOLOG(7, LOG_VMAREAS, {
            print_entry(dcontext, FRAG_PREV(entry), "\t    <=");
            print_entry(dcontext, FRAG_NEXT(entry), "\t    =>");
        });
        if (FRAG_ALSO(entry) != NULL) {
            fragment_t *also = FRAG_ALSO(entry);
            print_entry(dcontext, FRAG_ALSO(entry), "\t    also =>");

            /* check for also in same area == inconsistency in data structs */
            if (FRAG_PC(also) >= area->start && FRAG_PC(also) < area->end) {
                if (FRAG_MULTI_INIT(also)) {
                    LOG(THREAD, LOG_VMAREAS, 1, "WARNING: self-also frag tag " PFX "\n",
                        FRAG_FRAG(also));
                } else {
                    fragment_t *f = FRAG_FRAG(also);
                    LOG(THREAD, LOG_VMAREAS, 1,
                        "WARNING: self-also frag F%d(" PFX ")%s\n", f->id, f->tag,
                        TEST(FRAG_IS_TRACE, f->flags) ? " trace" : "");
                }
                /* not an assertion b/c we sometimes print prior to cleaning */
            }
        }

        ASSERT(last == NULL || last == FRAG_PREV(entry));
    }
    ASSERT(area->custom.frags == NULL || FRAG_PREV(area->custom.frags) == last);
}

static void
print_fraglists(dcontext_t *dcontext)
{
    thread_data_t *data = GET_DATA(dcontext, 0);
    int i;
    ASSERT_VMAREA_DATA_PROTECTED(data, READWRITE);
    LOG(THREAD, LOG_VMAREAS, 1, "\nFragment lists for ALL AREAS:\n");
    for (i = 0; i < data->areas.length; i++) {
        print_fraglist(dcontext, &(data->areas.buf[i]), "");
    }
    LOG(THREAD, LOG_VMAREAS, 1, "\n");
}

static void
print_frag_arealist(dcontext_t *dcontext, fragment_t *f)
{
    fragment_t *entry;
    if (FRAG_MULTI(f)) {
        LOG(THREAD, LOG_VMAREAS, 1, "Areas for F=" PFX " (" PFX ")\n", FRAG_FRAG(f),
            FRAG_PC(f));
    } else
        LOG(THREAD, LOG_VMAREAS, 1, "Areas for F%d (" PFX ")\n", f->id, f->tag);
    for (entry = f; entry != NULL; entry = FRAG_ALSO(entry)) {
        print_entry(dcontext, entry, "\t");
    }
}
#endif /* DEBUG && INTERNAL */

#ifdef DEBUG
static bool
area_contains_frag_pc(vm_area_t *area, fragment_t *f)
{
    app_pc pc = FRAG_PC(f);
    if (area == NULL)
        return true;
    return (pc >= area->start && pc < area->end);
}
#endif /* DEBUG */

/* adds entry to front of area's frags list
 * caller must synchronize modification of area
 * FIXME: how assert that caller has done that w/o asking for whole vector
 * to be passed in, or having backpointer from area?
 * See general FIXME of same flavor at top of file.
 */
static void
prepend_entry_to_fraglist(vm_area_t *area, fragment_t *entry)
{
    /* Can't assert area_contains_frag_pc() because vm_area_unlink_fragments
     * moves all also entries onto the area fraglist that's being flushed.
     */
    LOG(THREAD_GET, LOG_VMAREAS, 4,
        "%s: putting fragment @" PFX " (%s) on vmarea " PFX "-" PFX "\n",
        /* i#1215: FRAG_ID(entry) can crash if entry->f hold tag temporarily */
        __FUNCTION__, FRAG_PC(entry),
        TEST(FRAG_SHARED, entry->flags) ? "shared" : "private", area->start, area->end);
    FRAG_NEXT_ASSIGN(entry, area->custom.frags);
    /* prev wraps around, but not next */
    if (area->custom.frags != NULL) {
        FRAG_PREV_ASSIGN(entry, FRAG_PREV(area->custom.frags));
        FRAG_PREV_ASSIGN(area->custom.frags, entry);
    } else
        FRAG_PREV_ASSIGN(entry, entry);
    area->custom.frags = entry;
}

/* adds a multi_entry_t to the list of fragments for area.
 * cross-links with prev if prev != NULL.
 * sticks tag in for f (will be fixed in vm_area_add_fragment, once f is created)
 */
static fragment_t *
prepend_fraglist(dcontext_t *dcontext, vm_area_t *area, app_pc entry_pc, app_pc tag,
                 fragment_t *prev)
{
    multi_entry_t *e = (multi_entry_t *)nonpersistent_heap_alloc(
        dcontext, sizeof(multi_entry_t) HEAPACCT(ACCT_VMAREA_MULTI));
    fragment_t *entry = (fragment_t *)e;
    e->flags = FRAG_FAKE | FRAG_IS_EXTRA_VMAREA | /* distinguish from fragment_t */
        FRAG_IS_EXTRA_VMAREA_INIT;   /* indicate f field is a tag, not a fragment_t yet */
    if (dcontext == GLOBAL_DCONTEXT) /* shared */
        e->flags |= FRAG_SHARED;
    e->f = (fragment_t *)tag; /* placeholder */
    e->pc = entry_pc;
    if (prev != NULL)
        FRAG_ALSO_ASSIGN(prev, entry);
    FRAG_ALSO_ASSIGN(entry, NULL);
    ASSERT(area_contains_frag_pc(area, entry));
    prepend_entry_to_fraglist(area, entry);
    DOLOG(7, LOG_VMAREAS,
          { print_fraglist(dcontext, area, "after prepend_fraglist, "); });
    return entry;
}

#ifdef DGC_DIAGNOSTICS
void
dyngen_diagnostics(dcontext_t *dcontext, app_pc pc, app_pc base_pc, size_t size,
                   uint prot)
{
    bool future, stack;
    char buf[MAXIMUM_SYMBOL_LENGTH];
    app_pc translated_pc;

    d_r_read_lock(&futureexec_areas->lock);
    future = lookup_addr(futureexec_areas, pc, NULL);
    d_r_read_unlock(&futureexec_areas->lock);
    stack = is_on_stack(dcontext, pc, NULL);

    if (!future)
        future = is_dyngen_vsyscall(pc);

    print_symbolic_address(pc, buf, sizeof(buf), false);
    LOG(GLOBAL, LOG_VMAREAS, 1,
        "DYNGEN in %d: target=" PFX " => " PFX "-" PFX " %s%s%s%s%s %s\n",
        dcontext->owning_thread, pc, base_pc, base_pc + size,
        ((prot & MEMPROT_READ) != 0) ? "R" : "", ((prot & MEMPROT_WRITE) != 0) ? "W" : "",
        ((prot & MEMPROT_EXEC) != 0) ? "E" : "", future ? " future" : " BAD",
        stack ? " stack" : "", buf);

    if (LINKSTUB_FAKE(dcontext->last_exit)) {
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "source=!!! fake last_exit, must have been flushed?\n");
        return;
    }

    /* FIXME: risky if last fragment is deleted -- should check for that
     * here and instead just print type from last_exit, since recreate
     * may fail
     */
    translated_pc = recreate_app_pc(
        dcontext, EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit),
        dcontext->last_fragment);
    if (translated_pc != NULL) {
        print_symbolic_address(translated_pc, buf, sizeof(buf), false);
        LOG(GLOBAL, LOG_VMAREAS, 1, "source=F%d(" PFX ") @" PFX " \"%s\"\n",
            dcontext->last_fragment->id, dcontext->last_fragment->tag,
            EXIT_CTI_PC(dcontext->last_fragment, dcontext->last_exit), buf);
        disassemble_with_bytes(dcontext, translated_pc, main_logfile);
    }
    DOLOG(4, LOG_VMAREAS,
          { disassemble_fragment(dcontext, dcontext->last_fragment, false); });
}
#endif

/***************************************************************************
 * APPLICATION MEMORY STATE TRACKING
 */

/* Internal version with a flag for whether to only check or also update vmareas. */
static uint
app_memory_protection_change_internal(dcontext_t *dcontext, bool update_areas,
                                      app_pc base, size_t size,
                                      uint prot, /* platform independent MEMPROT_ */
                                      uint *new_memprot, /* OUT */
                                      uint *old_memprot /* OPTIONAL OUT*/, bool image);

/* Checks whether a requested allocation at a particular base will change
 * the protection bits of any code.  Returns whether or not to allow
 * the operation to go through.  The change_executable parameter is passed
 * through to app_memory_protection_change() on existing areas inside
 * [base, base+size).
 */
bool
app_memory_pre_alloc(dcontext_t *dcontext, byte *base, size_t size, uint prot, bool hint,
                     bool update_areas, bool image)
{
    byte *pb = base;
    dr_mem_info_t info;
    while (pb < base + size &&
           /* i#1462: getting the true bounds on Windows is expensive so we get just
            * the cur base first.  This can result in an extra syscall in some cases,
            * but in large-region cases it saves huge number of syscalls.
            */
           query_memory_cur_base(pb, &info)) {
        /* We can't also check for "info.prot != prot" for update_areas, b/c this is
         * delayed to post-syscall and we have to process changes after the fact.
         */
        if (info.type != DR_MEMTYPE_FREE && info.type != DR_MEMTYPE_RESERVED &&
            (update_areas || prot != info.prot)) {
            size_t change_sz;
            uint subset_memprot;
            uint res;
            /* We need the real base */
            if (!query_memory_ex(pb, &info))
                break;
            change_sz = MIN(info.base_pc + info.size - pb, base + size - pb);
            if (hint) {
                /* Just have caller remove the hint, before we go through
                 * -handle_dr_modify handling.
                 */
                return false;
            }
            LOG(GLOBAL, LOG_VMAREAS, 3,
                "%s: app alloc may be changing " PFX "-" PFX " %x\n", __FUNCTION__,
                info.base_pc, info.base_pc + info.size, info.prot);
            res = app_memory_protection_change_internal(dcontext, update_areas, pb,
                                                        change_sz, prot, &subset_memprot,
                                                        NULL, image);
            if (res != DO_APP_MEM_PROT_CHANGE) {
                if (res == FAIL_APP_MEM_PROT_CHANGE) {
                    return false;
                } else if (res == PRETEND_APP_MEM_PROT_CHANGE ||
                           res == SUBSET_APP_MEM_PROT_CHANGE) {
                    /* This gets complicated to handle.  If the syscall is
                     * changing a few existing pages and then allocating new
                     * pages beyond them, we could adjust the base: but there
                     * are many corner cases.  Thus we fail the syscall, which
                     * is the right thing for cases we've seen like i#1178
                     * where the app tries to commit to a random address!
                     */
                    SYSLOG_INTERNAL_WARNING_ONCE("Failing app alloc w/ suspect overlap");
                    return false;
                }
            }
        }
        if (POINTER_OVERFLOW_ON_ADD(info.base_pc, info.size))
            break;
        pb = info.base_pc + info.size;
    }
    return true;
}

/* Newly allocated or mapped in memory region. Returns true if added to exec list.
 * OK to pass in NULL for dcontext -- in fact, assumes dcontext is NULL at initialization.
 */
bool
app_memory_allocation(dcontext_t *dcontext, app_pc base, size_t size, uint prot,
                      bool image _IF_DEBUG(const char *comment))
{
    /* First handle overlap with existing areas.  Callers try to additionally do this
     * pre-syscall to catch cases we want to block, but we can't do everything there
     * b/c we don't know all "image" cases.
     * We skip this until the app is doing sthg, to avoid the extra memory queries
     * during os_walk_address_space().
     */
    if (dynamo_initialized &&
        !app_memory_pre_alloc(dcontext, base, size, prot, false /*!hint*/,
                              true /*update*/, image)) {
        /* XXX: We should do better by telling app_memory_protection_change() we
         * can't fail so it should try to handle.  We do not expect this to happen
         * except with a pathological race.
         */
        SYSLOG_INTERNAL_WARNING_ONCE(
            "Protection change already happened but should have been blocked");
    }
#ifdef PROGRAM_SHEPHERDING
    DODEBUG({
        /* case 4175 - reallocations will overlap with no easy way to
         * enforce this
         */
        if (futureexec_vm_area_overlap(base, base + size)) {
            SYSLOG_INTERNAL_WARNING_ONCE(
                "existing future area overlapping [" PFX ", " PFX ")", base, base + size);
        }
    });
#endif

    /* no current policies allow non-x code at allocation time onto exec list */
    if (!TEST(MEMPROT_EXEC, prot))
        return false;

    /* Do not add our own code cache and other data structures
     * to executable list -- but do add our code segment
     * FIXME: checking base only is good enough?
     */
    if (dynamo_vm_area_overlap(base, base + size)) {
        LOG(GLOBAL, LOG_VMAREAS, 2, "\t<dynamorio region>\n");
        /* assumption: preload/preinject library is not on DR area list since unloaded */
        if (!is_in_dynamo_dll(base) /* our own text section is ok */
            /* client lib text section is ok (xref i#487) */
            && !is_in_client_lib(base))
            return false;
    }

    LOG(GLOBAL, LOG_VMAREAS, 1, "New +x app memory region: " PFX "-" PFX " %s\n", base,
        base + size, memprot_string(prot));

    if (!TEST(MEMPROT_WRITE, prot)) {
        uint frag_flags = 0;
        if (DYNAMO_OPTION(coarse_units) && image && !RUNNING_WITHOUT_CODE_CACHE()) {
            /* all images start out with coarse-grain management */
            frag_flags |= FRAG_COARSE_GRAIN;
        }
        add_executable_vm_area(base, base + size, image ? VM_UNMOD_IMAGE : 0, frag_flags,
                               false /*no lock*/ _IF_DEBUG(comment));
        return true;
    } else if (dcontext == NULL ||
               /* i#626: we skip is_no_stack because of no mcontext at init time,
                * we also assume that no alloc overlaps w/ stack at init time.
                */
               (dynamo_initialized && !is_on_stack(dcontext, base, NULL))) {
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "WARNING: " PFX "-" PFX " is writable, NOT adding to executable list\n", base,
            base + size);

#ifdef PROGRAM_SHEPHERDING
        if (DYNAMO_OPTION(executable_if_x)) {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "app_memory_allocation: New future exec region b/c x: " PFX "-" PFX
                " %s\n",
                base, base + size, memprot_string(prot));
            STATS_INC(num_mark_if_x);
            add_futureexec_vm_area(base, base + size,
                                   false /*permanent*/
                                   _IF_DEBUG("alloc executable_if_x"));
            mark_module_exempted(base);
        } else if (DYNAMO_OPTION(executable_if_alloc)) {
            bool future = false;
            /* rwx regions are not added at init time unless in images */
#    ifdef WINDOWS
            if (image) {
                /* anything marked rwx in an image is added to future list
                 * otherwise it is not added -- must be separately allocated,
                 * not just be present at init or in a mapped non-image file
                 */
                future = true;
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "New future exec region b/c x from image: " PFX "-" PFX " %s\n", base,
                    base + size, memprot_string(prot));
            } else if (dcontext != NULL && dcontext->alloc_no_reserve) {
                /* we only add a region marked rwx at allocation time to the
                 * future list if it is allocated and reserved at the same time
                 * (to distinguish from the rwx heap on 2003)
                 */
                future = true;
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "New future exec region b/c x @alloc & no reserve: " PFX "-" PFX
                    " %s\n",
                    base, base + size, memprot_string(prot));
            }
#    else
            if (dcontext != NULL || image) {
                /* XXX: can't distinguish stack -- saved at init time since we don't
                 * add rwx then, but what about stacks whose creation we see?
                 */
                future = true;
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "New future exec region b/c x @alloc: " PFX "-" PFX " %s\n", base,
                    base + size, memprot_string(prot));
            }
#    endif
            if (future) {
                STATS_INC(num_alloc_exec);
                add_futureexec_vm_area(base, base + size,
                                       false /*permanent*/
                                       _IF_DEBUG("alloc x"));
            }
        }
#endif /* PROGRAM_SHEPHERDING */
    }
    return false;
}

/* de-allocated or un-mapped memory region */
void
app_memory_deallocation(dcontext_t *dcontext, app_pc base, size_t size,
                        bool own_initexit_lock, bool image)
{
    ASSERT(!dynamo_vm_area_overlap(base, base + size));
    /* we check for overlap regardless of memory protections, to allow flexible
     * policies that are independent of rwx bits -- if any overlap we remove,
     * no shortcuts
     */
    if (executable_vm_area_overlap(base, base + size, false /*have no lock*/)) {
        /* ok for overlap to have changed in between, flush checks again */
        flush_fragments_and_remove_region(dcontext, base, size, own_initexit_lock,
                                          true /*free futures*/);

#ifdef RETURN_AFTER_CALL
        if (DYNAMO_OPTION(ret_after_call) && !image && !DYNAMO_OPTION(rac_dgc_sticky)) {
            /* we can have after call targets in DGC in addition to DLLs */
            /* Note IMAGE mappings are handled in process_image() on
             * Windows, so that they can be handled more efficiently
             * as a single region. FIXME: case 4983 on Linux
             */
            /* only freeing if we have ever interp/executed from this area */

            /* FIXME: note that on app_memory_protection_change() we
             * do NOT want to free these entries, therefore we'd have
             * a leak if a portion gets marked writable and is thus no
             * longer on our list.  Note we can't flush the areas on
             * memory protection because the likelihood of introducing
             * a false positives in doing so is vastly greater than
             * the security risk of not flushing.  (Many valid after
             * call locations may still be active, and our vmarea
             * boundaries can not precisely capture the application
             * intent.)  Note that we would not leak on DLLs even if
             * they are made writable, because we treat separately.
             */
            /* FIXME: see proposal in case 2236 about using a
             * heuristic that removes only when too numerous, if that
             * works well as a heuristic that DGC is being reused, and
             * unlikely that it will be so densely filled.
             */

            /* FIXME: [perf] case 9331 this is not so good on all
             * deallocations, if we can't tell whether we have
             * executed from it.  On every module LOAD, before mapping
             * it as MEM_IMAGE the loader first maps a DLL as
             * MEM_MAPPED, and on each of the corresponding unmaps
             * during LoadLibrary(), we'd be walking the cumulative
             * hashtable.  Although there shouldn't be that many valid
             * AC entries at process startup, maybe best to leave the
             * DGC leak for now if this will potentially hurt startup
             * time in say svchost.exe.  Currently rac_dgc_sticky is
             * on by default so we don't reach this code.
             */
            /* case 9331: should find out if there was any true
             * execution in any thread here before we go through a
             * linear walk of the hashtable.  More directly we need a
             * vmvector matching all vmareas that had a .C added for
             * them, considering the common case should be that this
             * is an app memory deallocation that has nothing to do
             * with us.
             *
             * FIXME: for now just checking if base is declared DGC,
             * and ignoring any others possible vm_areas for the same
             * OS region, so we may still have a leak.
             */
            if (is_dyngen_code(base)) {
                ASSERT_NOT_TESTED();
                invalidate_after_call_target_range(dcontext, base, base + size);
            }
        }
#endif /* RETURN_AFTER_CALL */
    }

#ifdef PROGRAM_SHEPHERDING
    if (USING_FUTURE_EXEC_LIST && futureexec_vm_area_overlap(base, base + size)) {
        remove_futureexec_vm_area(base, base + size);
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "removing future exec " PFX "-" PFX " since now freed\n", base, base + size);
    }
#endif
}

/* A convenience routine that starts the two-phase flushing protocol */
/* Note this is not flush_fragments_and_remove_region */
static bool
flush_and_remove_executable_vm_area(dcontext_t *dcontext, app_pc base, size_t size)
{
    DEBUG_DECLARE(bool res;)
    flush_fragments_in_region_start(
        dcontext, base, size, false /* don't own initexit_lock */,
        false /* case 2236: keep futures */, true /* exec invalid */,
        false /* don't force synchall */
        _IF_DGCDIAG(NULL));
    DEBUG_DECLARE(res =)
    remove_executable_vm_area(base, base + size, true /*have lock*/);
    DODEBUG(if (!res) {
        /* area doesn't have to be executable in fact when called
         * on executable_if_hook path
         */
        LOG(THREAD, LOG_VMAREAS, 2,
            "\tregion was in fact not on executable_areas, so nothing to remove\n");
    });
    /* making sure there is no overlap now */
    ASSERT(!executable_vm_area_overlap(base, base + size, true /* holding lock */));

    return true;
}

void
tamper_resistant_region_add(app_pc start, app_pc end)
{
    /* For now assuming a single area for specially protected areas
     * that is looked up in addition to dynamo_vm_areas.  Assuming
     * modifications to any location is ntdll.dll is always
     * interesting to us, instead of only those pieces we trampoline
     * this should be sufficient.
     *
     * FIXME: we could add a new vm_area_vector_t for protected possibly
     * subpage regions that we later turn into pretend_writable_areas
     *
     * Note that ntdll doesn't have an IAT section so we only worry
     * about function patching
     */
    ASSERT(tamper_resistant_region_start == NULL);
    tamper_resistant_region_start = start;
    tamper_resistant_region_end = end;
}

/* returns true if [start, end) overlaps with a tamper_resistant region
 * as needed for DYNAMO_OPTION(handle_ntdll_modify)
 */
bool
tamper_resistant_region_overlap(app_pc start, app_pc end)
{
    return (end > tamper_resistant_region_start && start < tamper_resistant_region_end);
}

bool
is_jit_managed_area(app_pc addr)
{
    uint vm_flags;
    if (get_executable_area_vm_flags(addr, &vm_flags))
        return TEST(VM_JIT_MANAGED, vm_flags);
    else
        return false;
}

void
set_region_jit_managed(app_pc start, size_t len)
{
    vm_area_t *region;

    ASSERT(DYNAMO_OPTION(opt_jit));
    d_r_write_lock(&executable_areas->lock);
    if (lookup_addr(executable_areas, start, &region)) {
        LOG(GLOBAL, LOG_VMAREAS, 1, "set_region_jit_managed(" PFX " +0x%x)\n", start,
            len);
        ASSERT(region->start == start && region->end == (start + len));
        if (!TEST(VM_JIT_MANAGED, region->vm_flags)) {
            if (TEST(VM_MADE_READONLY, region->vm_flags))
                vm_make_writable(region->start, region->end - region->start);
            region->vm_flags |= VM_JIT_MANAGED;
            region->vm_flags &= ~(VM_MADE_READONLY | VM_DELAY_READONLY);
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "Region (" PFX " +0x%x) no longer 'made readonly'\n", start, len);
        }
    } else {
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "Generating new jit-managed vmarea: " PFX "-" PFX "\n", start, start + len);

        add_vm_area(executable_areas, start, start + len, VM_JIT_MANAGED, 0,
                    NULL _IF_DEBUG("jit-managed"));
    }
    d_r_write_unlock(&executable_areas->lock);
}

/* Called when memory region base:base+size is about to have privileges prot.
 * Returns a value from the enum in vmareas->h about whether to perform the
 * system call or not and if not what the return code to the app should be.
 * If update_areas is true and the syscall should go through, updates
 * the executable areas; else it is up to the caller to change executable areas.
 */
/* FIXME : This is called before the system call that will change
 * the memory permission which could be race condition prone! If another
 * thread executes from a region added by this function before the system call
 * goes through we could get a disconnect on what the memory premissions of the
 * region really are vs what vmareas expects for consistency, see bug 2833
 */
/* N.B.: be careful about leaving code read-only and returning
 * PRETEND_APP_MEM_PROT_CHANGE or SUBSET_APP_MEM_PROT_CHANGE, or other
 * cases where mixed with native execution we may have incorrect page settings -
 * e.g. make sure all pages that need to be executable are executable!
 *
 * Note new_memprot is set only for SUBSET_APP_MEM_PROT_CHANGE,
 * and old_memprot is set for PRETEND_APP_MEM_PROT_CHANGE or SUBSET_APP_MEM_PROT_CHANGE.
 */
/* Note: hotp_only_mem_prot_change() relies on executable_areas to find out
 * previous state, so eliminating it should be carefully; see case 6669.
 */
static uint
app_memory_protection_change_internal(dcontext_t *dcontext, bool update_areas,
                                      app_pc base, size_t size,
                                      uint prot, /* platform independent MEMPROT_ */
                                      uint *new_memprot, /* OUT */
                                      uint *old_memprot /* OPTIONAL OUT*/, bool image)
{
    /* FIXME: look up whether image, etc. here?
     * but could overlap multiple regions!
     */
    bool is_executable;

    bool should_finish_flushing = false;

    bool dr_overlap = DYNAMO_OPTION(handle_DR_modify) != DR_MODIFY_OFF /* we don't care */
        && dynamo_vm_area_overlap(base, base + size);

    bool system_overlap =
        DYNAMO_OPTION(handle_ntdll_modify) != DR_MODIFY_OFF /* we don't care */
        && tamper_resistant_region_overlap(base, base + size);

    bool patch_proof_overlap = false;
#ifdef WINDOWS
    uint frag_flags;
#endif
    ASSERT(new_memprot != NULL);
    /* old_memprot is optional */

#if defined(PROGRAM_SHEPHERDING) && defined(WINDOWS)
    patch_proof_overlap = (!IS_STRING_OPTION_EMPTY(patch_proof_default_list) ||
                           !IS_STRING_OPTION_EMPTY(patch_proof_list)) &&
        vmvector_overlap(patch_proof_areas, base, base + size);
    /* FIXME: [minor perf] all the above tests can be combined into a
     * single vmarea lookup when this feature default on, case 6632 */
    ASSERT(base != NULL);
    if (patch_proof_overlap) {
        app_pc modbase = get_module_base(base);
        bool loader = is_module_patch_region(dcontext, base, base + size,
                                             false /*be liberal: don't miss loader*/);
        bool patching_code =
            is_range_in_code_section(modbase, base, base + size, NULL, NULL);
        bool patching_IAT = is_IAT(base, base + size, true /*page-align*/, NULL, NULL);
        /* FIXME: [perf] could add CODE sections, not modules, to patch_proof_areas */
        /* FIXME: [minor perf] is_module_patch_region already collected these */
        /* FIXME: [minor perf] same check is done later for IATs for emulate_IAT_writes */

        bool patch_proof_IAT = false; /* NYI - case 6622 */
        /* FIXME: case 6622 IAT hooker protection for some modules is
         * expected to conflict with emulate_IAT_writes, need to make
         * sure emulate_write_areas will not overlap with this
         */
        ASSERT_NOT_IMPLEMENTED(!patch_proof_IAT);

        patch_proof_overlap = !loader && patching_code &&
            /* even if it is not the loader we protect IAT sections only */
            (!patching_IAT || patch_proof_IAT);

        LOG(THREAD, LOG_VMAREAS, 1,
            "patch proof module " PFX "-" PFX " modified %s, by %s,%s=>%s\n", base,
            base + size, patching_code ? "code!" : "data --ok",
            loader              ? "loader --ok"
                : patching_code ? "hooker!"
                                : "loader or hooker",
            patching_IAT ? "IAT hooker" : "patching!",
            patch_proof_overlap ? "SQUASH" : "allow");
        /* curiosly the loader modifies the .reloc section of Dell\QuickSet\dadkeyb.dll */
    }
#endif /* defined(PROGRAM_SHEPHERDING) && defined(WINDOWS) */

    /* FIXME: case 6622 IAT hooking should be controlled separately,
     * note that when it is not protecting all IAT areas - exemptions
     * tracked by module name there may have to handle two different
     * cases.  If making sure a particular DLL is always using the
     * real exports current implementation above will work.  Yet in
     * the use case of avoiding a particular IAT hooker replacing
     * imports from kernel32, _all_ modules will have to be pretend
     * writable.  xref case 1948 for tracking read/written values
     */

    if (dr_overlap || system_overlap || patch_proof_overlap) {
        uint how_handle;
        const char *target_area_name;
        /* FIXME: separate this in a function */
        if (dr_overlap) {
            how_handle = DYNAMO_OPTION(handle_DR_modify);
            STATS_INC(app_modify_DR_prot);
            target_area_name = PRODUCT_NAME;
        } else if (system_overlap) {
            ASSERT(system_overlap);
            how_handle = DYNAMO_OPTION(handle_ntdll_modify);
            STATS_INC(app_modify_ntdll_prot);
            target_area_name = "system";
        } else {
            ASSERT(patch_proof_overlap);
            target_area_name = "module";
            how_handle = DR_MODIFY_NOP; /* use pretend writable */
            STATS_INC(app_modify_module_prot);
        }

        /* we can't be both pretend writable and emulate write */
        ASSERT(!vmvector_overlap(emulate_write_areas, base, base + size));

        if (how_handle == DR_MODIFY_HALT) {
            /* Until we've fixed our DR area list problems and gotten shim.dll to work,
             * we will issue an unrecoverable error
             */
            report_dynamorio_problem(dcontext, DUMPCORE_SECURITY_VIOLATION, NULL, NULL,
                                     "Application changing protections of "
                                     "%s memory @" PFX "-" PFX,
                                     target_area_name, base, base + size);
            /* FIXME: walking the loader data structures at arbitrary
             * points is dangerous due to data races with other threads
             * -- see is_module_being_initialized and get_module_name
             */
            check_for_unsupported_modules();
            os_terminate(dcontext, TERMINATE_PROCESS);
            ASSERT_NOT_REACHED();
        } else {
            /* On Win10 this happens in every run so we do not syslog. */
            LOG(THREAD, LOG_VMAREAS, 1,
                "Application changing protections of %s memory (" PFX "-" PFX ")",
                target_area_name, base, base + size);
            if (how_handle == DR_MODIFY_NOP) {
                /* we use a separate list, rather than a flag on DR areas, as the
                 * affected region could include non-DR memory
                 */
                /* FIXME: note that we do not intersect with a concrete
                 * region that we want to protect - considering Win32
                 * protection changes allowed only separately
                 * allocated regions this may be ok.  If we want to
                 * have subpage regions then it becomes an issue:
                 * we'd have to be able to emulate a write on a
                 * page that has pretend writable regions.
                 * For now we ensure pretend_writable_areas is always page-aligned.
                 */
                app_pc page_base;
                size_t page_size;
                ASSERT_CURIOSITY(ALIGNED(base, PAGE_SIZE));
                ASSERT_CURIOSITY(ALIGNED(size, PAGE_SIZE));
                page_base = (app_pc)PAGE_START(base);
                page_size = ALIGN_FORWARD(base + size, PAGE_SIZE) - (size_t)page_base;
                d_r_write_lock(&pretend_writable_areas->lock);
                if (TEST(MEMPROT_WRITE, prot)) {
                    LOG(THREAD, LOG_VMAREAS, 2,
                        "adding pretend-writable region " PFX "-" PFX "\n", page_base,
                        page_base + page_size);
                    add_vm_area(pretend_writable_areas, page_base, page_base + page_size,
                                true, 0, NULL _IF_DEBUG("DR_MODIFY_NOP"));
                } else {
                    LOG(THREAD, LOG_VMAREAS, 2,
                        "removing pretend-writable region " PFX "-" PFX "\n", page_base,
                        page_base + page_size);
                    remove_vm_area(pretend_writable_areas, page_base,
                                   page_base + page_size, false);
                }
                d_r_write_unlock(&pretend_writable_areas->lock);
                LOG(THREAD, LOG_VMAREAS, 2, "turning system call into a nop\n");

                if (old_memprot != NULL) {
                    /* FIXME: case 10437 we should keep track of any previous values */
                    if (!get_memory_info(base, NULL, NULL, old_memprot)) {
                        /* FIXME: should we fail instead of feigning success? */
                        ASSERT_CURIOSITY(false && "prot change nop should fail");
                        *old_memprot = MEMPROT_NONE;
                    }
                }
                return PRETEND_APP_MEM_PROT_CHANGE; /* have syscall be a nop! */
            } else if (how_handle == DR_MODIFY_FAIL) {
                /* not the default b/c hooks that target our DLL often ignore the return
                 * code of the syscall and blindly write, failing on the write fault.
                 */
                LOG(THREAD, LOG_VMAREAS, 2, "turning system call into a failure\n");
                return FAIL_APP_MEM_PROT_CHANGE; /* have syscall fail! */
            } else if (how_handle == DR_MODIFY_ALLOW) {
                LOG(THREAD, LOG_VMAREAS, 2, "ALLOWING system call!\n");
                /* continue down below */
            }
        }
        ASSERT(how_handle == DR_MODIFY_ALLOW);
    }

    /* DR areas may have changed, but we still have to remove from pretend list */
    if (USING_PRETEND_WRITABLE() && !TEST(MEMPROT_WRITE, prot) &&
        pretend_writable_vm_area_overlap(base, base + size)) {
        ASSERT_NOT_TESTED();
        /* FIXME: again we have the race -- if we could go from read to write
         * it would be a simple fix, else have to grab write up front, or check again
         */
        d_r_write_lock(&pretend_writable_areas->lock);
        LOG(THREAD, LOG_VMAREAS, 2, "removing pretend-writable region " PFX "-" PFX "\n",
            base, base + size);
        remove_vm_area(pretend_writable_areas, base, base + size, false);
        d_r_write_unlock(&pretend_writable_areas->lock);
    }

#ifdef PROGRAM_SHEPHERDING
    if (USING_FUTURE_EXEC_LIST && futureexec_vm_area_overlap(base, base + size)) {
        /* something changed */
        if (!TEST(MEMPROT_EXEC, prot)) {
            /* we DO remove future regions just b/c they're now marked non-x
             * but we may want to re-consider this -- some hooks briefly go to rw, e.g.
             * although we MUST do this for executable_if_exec
             * we should add flags to future areas indicating which policy put it here
             * (have to not merge different policies, I guess -- problematic for
             * sub-page flush combined w/ other policies?)
             */
            DEBUG_DECLARE(bool ok =) remove_futureexec_vm_area(base, base + size);
            ASSERT(ok);
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "future region " PFX "-" PFX " is being made non-x, removing\n", base,
                base + size);
        } else {
            /* Maybe nothing is changed in fact. */
            /* In fact this happens when a protection size larger than
             * necessary for a hook leaves some pages on the
             * futureexec_vm_area_overlap region (case 2871 for a two
             * page hooker).  There is nothing to do here,
             * executable_if_hook should re-add the pages.
             */
            /* case 3279 - probably similar behaviour -- when a second
             * NOP memory protection change happens to a region
             * already on the future list - we'd need to power it up
             * again
             */
            /* xref case 3102 - where we don't care about VM_WRITABLE */
#    if 0 /* this syslog may causes services.exe to hang (ref case 666)  */
            SYSLOG_INTERNAL_WARNING("future executable area overlapping with "PFX"-"
                                    PFX" made %s",
                                    base, base + size, memprot_string(prot));
#    endif
        }
    }
#endif

#if defined(PROGRAM_SHEPHERDING) && defined(WINDOWS)
    /* Just remove up front if changing anything about an emulation region.
     * Should certainly remove if becoming -w, but should also remove if
     * being added to exec list -- current usage expects to be removed on
     * next protection change (hooker restoring IAT privileges).
     * FIXME: should make the ->rx restoration syscall a NOP for performance
     */
    if (DYNAMO_OPTION(emulate_IAT_writes) && !vmvector_empty(emulate_write_areas) &&
        vmvector_overlap(emulate_write_areas, base, base + size)) {
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2,
            "removing emulation region " PFX "-" PFX "\n", base, base + size);
        vmvector_remove(emulate_write_areas, base, base + size);
    }
#endif

#ifndef PROGRAM_SHEPHERDING
    if (!INTERNAL_OPTION(hw_cache_consistency))
        return DO_APP_MEM_PROT_CHANGE; /* let syscall go through */
#endif
    if (!update_areas)
        return DO_APP_MEM_PROT_CHANGE; /* let syscall go through */

    /* look for calls making code writable!
     * cache is_executable here w/o holding lock -- if decide to perform state
     * change via flushing, we'll re-check overlap there and all will be atomic
     * at that point, no reason to try and make atomic from here, will hit
     * deadlock issues w/ thread_initexit_lock
     */
    is_executable = executable_vm_area_overlap(base, base + size, false /*have no lock*/);
    if (is_executable && TEST(MEMPROT_WRITE, prot) && !TEST(MEMPROT_EXEC, prot) &&
        INTERNAL_OPTION(hw_cache_consistency)) {
#ifdef WINDOWS
        app_pc IAT_start, IAT_end;
        /* Could not page-align and ask for original params but some hookers
         * page-align even when targeting only IAT */
        bool is_iat =
            is_IAT(base, base + size, true /*page-align*/, &IAT_start, &IAT_end);
        bool is_patch =
            is_module_patch_region(dcontext, base, base + size, true /*be conservative*/);
        DOSTATS({
            if (is_iat && is_patch)
                STATS_INC(num_app_rebinds);
        });
#    ifdef PROGRAM_SHEPHERDING
        /* This potentially unsafe option is superseded by -coarse_merge_iat
         * FIXME: this should be available for !PROGRAM_SHEPHERDING
         */
        if (DYNAMO_OPTION(unsafe_ignore_IAT_writes) && is_iat && is_patch) {
            /* do nothing: let go writable and then come back */
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "WARNING: letting IAT be written w/o flushing: potentially unsafe\n");
            return DO_APP_MEM_PROT_CHANGE; /* let syscall go through */
        }
#    endif
        /* Case 11072: must match these conditions w/ the assert on freeing */
        if (DYNAMO_OPTION(coarse_units) && DYNAMO_OPTION(coarse_merge_iat) &&
#    ifdef PROGRAM_SHEPHERDING
            /* Ensure we'll re-mark as valid */
            (DYNAMO_OPTION(executable_if_rx_text) ||
             DYNAMO_OPTION(executable_after_load)) &&
#    endif
            is_iat && is_patch && !executable_vm_area_executed_from(IAT_start, IAT_end) &&
            /* case 10830/11072: ensure currently marked coarse-grain to avoid
             * blessing the IAT region as coarse when it was in fact made non-coarse
             * due to a rebase (or anything else) prior to a rebind.  check the end,
             * since we may have adjusted the exec area bounds to be post-IAT.
             */
            get_executable_area_flags(base + size - 1, &frag_flags) &&
            TEST(FRAG_COARSE_GRAIN, frag_flags)) {
            coarse_info_t *info =
                get_coarse_info_internal(IAT_end, false /*no init*/, false /*no lock*/);
            /* loader rebinding
             * We cmp and free the stored code at +rx time; if that doesn't happen,
             * we free at module unload time.
             */
            DEBUG_DECLARE(bool success =)
            os_module_store_IAT_code(base);
            ASSERT(success);
            ASSERT(!RUNNING_WITHOUT_CODE_CACHE()); /* FRAG_COARSE_GRAIN excludes */
            LOG(GLOBAL, LOG_VMAREAS, 2, "storing IAT code for " PFX "-" PFX "\n",
                IAT_start, IAT_end);
            if (info != NULL) {
                /* Only expect to do this for empty or persisted units */
                ASSERT(info->cache == NULL ||
                       (info->persisted && info->non_frozen != NULL &&
                        info->non_frozen->cache == NULL));
                /* Do not reset/free during flush as we hope to see a validating
                 * event soon.
                 */
                ASSERT(!TEST(PERSCACHE_CODE_INVALID, info->flags));
                info->flags |= PERSCACHE_CODE_INVALID;
                STATS_INC(coarse_marked_invalid);
            }
        }
#    ifdef PROGRAM_SHEPHERDING
        if (DYNAMO_OPTION(emulate_IAT_writes) && is_iat &&
            /* We do NOT want to emulate hundreds of writes by the loader -- we
             * assume no other thread will execute in the module until it's
             * initialized.  We only need our emulation for hookers who come in
             * after initialization when another thread may be in there.
             */
            !is_patch) {
            /* To avoid having the IAT page (which often includes the start of the
             * text section) off the exec areas list, we only remove the IAT itself,
             * and emulate writes to it.
             * FIXME: perhaps this should become an IAT-only vector, and be used
             * for when we have the IAT read-only to protect it security-wise.
             */
            /* unfortunately we have to flush to be conservative */
            should_finish_flushing = flush_and_remove_executable_vm_area(
                dcontext, IAT_start, IAT_end - IAT_start);
            /* a write to IAT gets emulated, but to elsewhere on page is a code mod */
            vmvector_add(emulate_write_areas, IAT_start, IAT_end, NULL);
            /* must release the exec areas lock, even if expect no flush */
            if (should_finish_flushing) {
                flush_fragments_in_region_finish(dcontext,
                                                 false /*don't keep initexit_lock*/);
            }
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "executable region == IAT so not marking %s, emulating writes\n",
                memprot_string(prot));
            /* now leave as read-only.
             * we do not record what other flags they're using here -- we assume
             * they're going to restore IAT back to what it was
             */
            /* FIXME: case 10437 we should keep track of any previous values */
            if (old_memprot != NULL) {
                if (!get_memory_info(base, NULL, NULL, old_memprot)) {
                    /* FIXME: should we fail instead of feigning success? */
                    ASSERT_CURIOSITY(false && "prot change nop should fail");
                    *old_memprot = MEMPROT_NONE;
                }
            }
            return PRETEND_APP_MEM_PROT_CHANGE;
        }
#    endif /* PROGRAM_SHEPHERDING */
#endif     /* WINDOWS */
        /* being made writable but non-executable!
         * kill all current fragments in the region (since a
         * non-executable region is ignored by flush routine)
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "WARNING: executable region being made writable and non-executable\n");
        flush_fragments_and_remove_region(dcontext, base, size,
                                          false /* don't own initexit_lock */,
                                          false /* case 2236: keep futures */);
#ifdef HOT_PATCHING_INTERFACE
        if (DYNAMO_OPTION(hotp_only))
            hotp_only_mem_prot_change(base, size, true, false);
#endif
    } else if (is_executable && TESTALL(MEMPROT_WRITE | MEMPROT_EXEC, prot) &&
               INTERNAL_OPTION(hw_cache_consistency)) {
        /* Need to flush all fragments in [base, base+size), unless
         * they are ALL already writable
         */
        DOSTATS({
            /* If all the overlapping executable areas are VM_WRITABLE|
             * VM_DELAY_READONLY then we could optimize away the flush since
             * we haven't made any portion of this region read only for
             * consistency purposes.  We haven't implemented this optimization
             * as it's quite rare (though does happen xref case 8104) and
             * previous implementations of this optimization proved buggy. */
            if (is_executable_area_overlap(base, base + size, true /* ALL regions are: */,
                                           VM_WRITABLE | VM_DELAY_READONLY)) {
                STATS_INC(num_possible_app_to_rwx_skip_flush);
            }
        });
        /* executable region being made writable
         * flush all current fragments, and mark as non-executable
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "WARNING: executable region " PFX "-" PFX " is being made writable!\n"
            "\tRemoving from executable list\n",
            base, base + size);
        /* use two-part flush to make futureexec & exec changes atomic w/ flush */
        should_finish_flushing =
            flush_and_remove_executable_vm_area(dcontext, base, size);
        /* we flush_fragments_finish after security checks to keep them atomic */
    } else if (is_executable && is_executable_area_writable(base) &&
               !TEST(MEMPROT_WRITE, prot) && TEST(MEMPROT_EXEC, prot) &&
               INTERNAL_OPTION(hw_cache_consistency)) {
        /* executable & writable region being made read-only
         * make sure any future write faults are given to app, not us
         */
        LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "executable writable region " PFX "-" PFX " => read-only!\n", base,
            base + size);
        /* remove writable exec area, then add read-only exec area */
        /* use two-part flush to make futureexec & exec changes atomic w/ flush */
        should_finish_flushing =
            flush_and_remove_executable_vm_area(dcontext, base, size);
        /* FIXME: this is wrong -- this will make all pieces in the middle executable,
         * which is not what we want -- we want all pieces ON THE EXEC LIST to
         * change from rw to r.  thus this should be like the change-to-selfmod case
         * in handle_modified_code => add new vector routine?  (case 3570)
         */
        add_executable_vm_area(base, base + size, image ? VM_UNMOD_IMAGE : 0, 0,
                               should_finish_flushing /* own lock if flushed */
                                   _IF_DEBUG("protection change"));
    }
    /* also look for calls making data executable
     * FIXME: perhaps should do a write_keep for this is_executable, to bind
     * to the subsequent exec areas changes -- though case 2833 would still be there
     */
    else if (!is_executable && TEST(MEMPROT_EXEC, prot) &&
             INTERNAL_OPTION(hw_cache_consistency)) {
        if (TEST(MEMPROT_WRITE, prot)) {
            /* do NOT add to executable list if writable */
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "WARNING: data region " PFX "-" PFX " made executable and "
                "writable, not adding to exec list\n",
                base, base + size);
        } else {
            bool add_to_exec_list = false;
#ifdef WINDOWS
            bool check_iat = false;
            bool free_iat = false;
#endif
            uint frag_flags_pfx = 0;
            DEBUG_DECLARE(const char *comment = "";)
            LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "WARNING: data region " PFX "-" PFX " is being made executable\n", base,
                base + size);
#ifdef PROGRAM_SHEPHERDING
            /* if on future, no reason to add to exec list now
             * if once-only, no reason to add to exec list and remove from future
             * wait until actually executed!
             */
            /* none of our policies allow this on the stack */
            if (is_address_on_stack(dcontext, base)) {
                LOG(THREAD, LOG_VMAREAS, 2, "not allowing data->x for stack region\n");
#    ifdef WINDOWS
            } else if (DYNAMO_OPTION(executable_after_load) &&
                       is_module_patch_region(dcontext, base, base + size,
                                              false /*be liberal: can't miss loader*/)) {
                STATS_INC(num_mark_after_load);
                add_to_exec_list = true;
                check_iat = true;
                DODEBUG({ comment = "if_after_load"; });
                LOG(THREAD, LOG_VMAREAS, 2,
                    "module is being initialized, adding region to executable list\n");

#    endif
            } else if (DYNAMO_OPTION(executable_if_rx_text)) {
                /* FIXME: this should be moved out of the if (!executable) branch?
                 * to where executable_if_x is handled
                 */
                /* NOTE - xref case 10526, the check here is insufficient to implement
                 * this policy because [*base, *base+*size) could overlap multiple
                 * sections (some of which might not be code) which would cause this
                 * check to fail.  Fixing this here would require us to find the
                 * intersection of this region and any code section(s) and add the
                 * resulting region(s) (there could be more then one). Instead we leave
                 * this check here to catch the common case but extend
                 * check_origins_helper to catch anything unusual. */
                app_pc modbase = get_module_base(base);
                if (modbase != NULL &&
                    is_range_in_code_section(modbase, base, base + size, NULL, NULL)) {
                    STATS_INC(num_2rx_text);
                    add_to_exec_list = true;
                    IF_WINDOWS(check_iat = true;)
                    DODEBUG({ comment = "if_rx_text"; });
                    LOG(THREAD, LOG_VMAREAS, 2,
                        "adding code region being marked rx to executable list\n");
                }
            } /* Don't use an  else if here, the else if for
               * -executable_if_rx_text if doesn't check all its
               * conditionals in the first if */

            if (DYNAMO_OPTION(executable_if_rx)) {
                STATS_INC(num_mark_if_rx);
                add_to_exec_list = true;
                mark_module_exempted(base);
                DODEBUG({ comment = "if_rx"; });
                LOG(THREAD, LOG_VMAREAS, 2,
                    "adding region marked only rx "
                    "to executable list\n");
            }
#else
            add_to_exec_list = true;
            IF_WINDOWS(check_iat = true;)
#endif
#ifdef WINDOWS
            if (check_iat) {
                if (DYNAMO_OPTION(coarse_units) && DYNAMO_OPTION(coarse_merge_iat) &&
                    is_IAT(base, base + size, true /*page-align*/, NULL, NULL))
                    free_iat = true;
                LOG(THREAD, LOG_VMAREAS, 2,
                    ".text or IAT is being made rx again " PFX "-" PFX "\n", base,
                    base + size);
                if (!RUNNING_WITHOUT_CODE_CACHE()) {
                    /* case 8640: let add_executable_vm_area() decide whether to
                     * keep the coarse-grain flag
                     */
                    frag_flags_pfx |= FRAG_COARSE_GRAIN;
                } else {
                    free_iat = false;
                    ASSERT(!os_module_free_IAT_code(base));
                }
            }
#endif
            if (add_to_exec_list) {
                if (DYNAMO_OPTION(coarse_units) && image &&
                    !RUNNING_WITHOUT_CODE_CACHE()) {
                    /* all images start out with coarse-grain management */
                    frag_flags_pfx |= FRAG_COARSE_GRAIN;
                }
                /* FIXME : see note at top of function about bug 2833 */
                ASSERT(!TEST(MEMPROT_WRITE, prot)); /* sanity check */
                add_executable_vm_area(base, base + size, image ? VM_UNMOD_IMAGE : 0,
                                       frag_flags_pfx,
                                       false /*no lock*/ _IF_DEBUG(comment));
            }
#ifdef WINDOWS
            if (free_iat) {
                DEBUG_DECLARE(bool had_iat =)
                os_module_free_IAT_code(base);
                DEBUG_DECLARE(app_pc text_start;)
                DEBUG_DECLARE(app_pc text_end;)
                DEBUG_DECLARE(app_pc iat_start = NULL;)
                DEBUG_DECLARE(app_pc iat_end = NULL;)
                /* calculate IAT bounds */
                ASSERT(
                    is_IAT(base, base + size, true /*page-align*/, &iat_start, &iat_end));
                ASSERT(had_iat ||
                       /* duplicate the reasons we wouldn't have stored the IAT: */
                       !is_module_patch_region(dcontext, base, base + size,
                                               true /*be conservative*/) ||
                       executable_vm_area_executed_from(iat_start, iat_end) ||
                       /* case 11072: rebase prior to rebind prevents IAT storage */
                       (get_module_preferred_base_delta(base) != 0 &&
                        is_in_code_section(get_module_base(base), base, &text_start,
                                           &text_end) &&
                        iat_start >= text_start && iat_end <= text_end));
            }
#endif
#ifdef HOT_PATCHING_INTERFACE
            if (DYNAMO_OPTION(hotp_only))
                hotp_only_mem_prot_change(base, size, false, true);
#endif
        }
    }

#ifdef PROGRAM_SHEPHERDING
    /* These policies do not depend on a transition taking place. */
    /* Make sure weaker policies are considered first, so that
     * the region is kept on the futureexec list with the least restrictions
     */
    if (DYNAMO_OPTION(executable_if_x) && TEST(MEMPROT_EXEC, prot)) {
        /* The executable_if_x policy considers all code marked ..x to be executable */

        /* Note that executable_if_rx may have added a region directly
         * to the executable_areas, while here we only add to the futureexec_areas
         * FIXME: move executable_if_rx checks as an 'else if' following this if.
         */
        LOG(GLOBAL, LOG_VMAREAS, 1,
            "New future region b/c x, " PFX "-" PFX " %s, was %sexecutable\n", base,
            base + size, memprot_string(prot), is_executable ? "" : "not ");
        STATS_INC(num_mark_if_x);
        add_futureexec_vm_area(base, base + size,
                               false /*permanent*/
                               _IF_DEBUG(TEST(MEMPROT_WRITE, prot)
                                             ? "executable_if_x protect exec .wx"
                                             : "executable_if_x protect exec .-x"));
        mark_module_exempted(base);
    } else if (DYNAMO_OPTION(executable_if_hook) &&
               TESTALL(MEMPROT_WRITE | MEMPROT_EXEC, prot)) {
        /* Note here we're strict in requesting a .WX setting by the
         * hooker, won't be surprising if some don't do even this
         */
        /* FIXME: could restrict to sub-page piece of text section,
         * since should only be targeting 4 or 5 byte area
         */
        app_pc modbase = get_module_base(base);
        if (modbase != NULL) { /* PE, and is readable */
            /* FIXME - xref case 10526, if the base - base+size overlaps more than
             * one section then this policy won't apply, though not clear if we'd want
             * it to for such an unusual hooker. */
            if (is_range_in_code_section(modbase, base, base + size, NULL, NULL)) {
                uint vm_flags;
                DOLOG(2, LOG_INTERP | LOG_VMAREAS, {
                    char modname[MAX_MODNAME_INTERNAL];
                    os_get_module_name_buf(modbase, modname,
                                           BUFFER_SIZE_ELEMENTS(modname));
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "adding hook to future list: " PFX "-" PFX " in code of "
                        "module @" PFX " == %s made rwx\n",
                        base, base + size, modbase,
                        modname == NULL ? "<invalid name>" : modname);
                });
                STATS_INC(num_hook);

                /* add as a once-only future area */
                add_futureexec_vm_area(base, base + size,
                                       true /*once-only*/
                                       _IF_DEBUG(memprot_string(prot)));
                /* This is text section, leave area on executable list
                 * so app can execute here, write, and then execute
                 * again (via future list) to handle cases of hooking
                 * kernel32 functions ref case 2803 and case 3097 */

                if (!should_finish_flushing) {
                    /* FIXME: as a quick fix we flush the existing area
                     * just in case anyways, so that we don't think about
                     * merging properly the FRAG_DYNGEN
                     */
                    should_finish_flushing =
                        flush_and_remove_executable_vm_area(dcontext, base, size);
                }

                /* FIXME: we could optimize away the VM_DELAY_READONLY
                 * path if we actually knew that the current real
                 * protection flag is not writable.  Yet we've removed
                 * any internal data about it, so we need
                 * restructuring or an extra system call here vs the
                 * safe one at make_unwritable().
                 *
                 * case 8308: Don't mark as DELAY_READONLY if -sandbox_writable
                 * is on.  We don't need to check for -sandbox_non_text here
                 * since we know we're in a text region here.
                 */
                vm_flags = VM_WRITABLE;
                if (!DYNAMO_OPTION(sandbox_writable))
                    vm_flags |= VM_DELAY_READONLY;

                add_executable_vm_area(
                    base, base + size, vm_flags, 0,
                    should_finish_flushing /* own the lock if
                                              we have flushed */
                        _IF_DEBUG("prot chg txt rx->rwx not yet written"));
                /* leave read only since we are leaving on exec list */
                if (should_finish_flushing) {
                    flush_fragments_in_region_finish(dcontext,
                                                     false /*don't keep initexit_lock*/);
                }

                if (old_memprot != NULL) {
                    /* FIXME: case 10437 we should keep track of any previous values */
                    if (!get_memory_info(base, NULL, NULL, old_memprot)) {
                        /* FIXME: should we fail instead of feigning success? */
                        ASSERT_CURIOSITY(false && "prot change nop should fail");
                        *old_memprot = MEMPROT_NONE;
                    }
                }
                /* case 10387 initial fix - on a DEP machine to
                 * support properly native execution we must set the X
                 * bit: most needed for -hotp_only when we provide our
                 * code origins policies for GBOP enforcement, but
                 * similar need in native_exec or other possible mixed
                 * modes.
                 */

                /* We really should be setting everything according to
                 * app request except for writability.  Hopefully we
                 * don't have sophisticated hookers using PAGE_GUARD
                 * so ok to use only the memprot supported flags.
                 */
                prot &= ~MEMPROT_WRITE;
                ASSERT_CURIOSITY(TESTALL(MEMPROT_READ | MEMPROT_EXEC, prot));

                *new_memprot = prot;
                return SUBSET_APP_MEM_PROT_CHANGE;
            }
        }
    }
#endif /* PROGRAM_SHEPHERDING */
    if (should_finish_flushing) {
        flush_fragments_in_region_finish(dcontext, false /*don't keep initexit_lock*/);

        if (DYNAMO_OPTION(opt_jit) && is_jit_managed_area(base))
            jitopt_clear_span(base, base + size);
    }
    return DO_APP_MEM_PROT_CHANGE; /* let syscall go through */
}

uint
app_memory_protection_change(dcontext_t *dcontext, app_pc base, size_t size,
                             uint prot,         /* platform independent MEMPROT_ */
                             uint *new_memprot, /* OUT */
                             uint *old_memprot /* OPTIONAL OUT*/, bool image)
{
    return app_memory_protection_change_internal(dcontext, true /*update*/, base, size,
                                                 prot, new_memprot, old_memprot, image);
}

#ifdef WINDOWS
/* memory region base:base+size was flushed from hardware icache by app */
void
app_memory_flush(dcontext_t *dcontext, app_pc base, size_t size, uint prot)
{
#    ifdef PROGRAM_SHEPHERDING
    if (DYNAMO_OPTION(executable_if_flush)) {
        /* We want to ignore the loader calling flush, since our current
         * impl makes a flush region permanently executable.
         * The loader always follows the order "rw, rx, flush", but we have
         * seen real DGC marking rx before flushing as well, so we use
         * our module-being-loaded test:
         */
        if (!is_module_patch_region(dcontext, base, base + size,
                                    false /*be liberal: don't miss loader*/)) {
            /* FIXME case 280: we'd like to always be once-only, but writes
             * to data on the same page make it hard to do that.
             */
            bool onceonly = false;
            /* we do NOT go to page boundaries, instead we put sub-page
             * regions on our future list
             */
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "New future exec region b/c flushed: " PFX "-" PFX " %s\n", base,
                base + size, memprot_string(prot));
            if (!DYNAMO_OPTION(selfmod_futureexec) &&
                is_executable_area_on_all_selfmod_pages(base, base + size)) {
                /* for selfmod we can be onceonly, as writes to data on the
                 * same page won't kick us off the executable list
                 */
                onceonly = true;
            }
            add_futureexec_vm_area(base, base + size,
                                   onceonly _IF_DEBUG("NtFlushInstructionCache"));
            if (DYNAMO_OPTION(xdata_rct)) {
                /* FIXME: for now we only care about start pc */
                vmvector_add(app_flushed_areas, base, base + 1, NULL);
                /* FIXME: remove when region de-allocated? */
            }
            DOSTATS({
                if (is_executable_area_writable(base))
                    STATS_INC(num_NT_flush_w2r); /* pretend writable (we made RO) */
                if (TEST(MEMPROT_WRITE, prot))
                    STATS_INC(num_NT_flush_w);
                else
                    STATS_INC(num_NT_flush_r);
                if (is_address_on_stack(dcontext, base)) {
                    STATS_INC(num_NT_flush_stack);
                } else {
                    STATS_INC(num_NT_flush_heap);
                }
            });
        } else {
            LOG(THREAD, LOG_VMAREAS, 1, "module is being loaded, ignoring flush\n");
            STATS_INC(num_NT_flush_loader);
        }
    }
#    else
    /* NOP */
#    endif /* PROGRAM_SHEPHERDING */
}

#    ifdef PROGRAM_SHEPHERDING
bool
was_address_flush_start(dcontext_t *dcontext, app_pc pc)
{
    ASSERT(DYNAMO_OPTION(xdata_rct));
    /* FIXME: once we have flags marking where each futureexec region
     * came from we can distinguish NtFlush, but for now we need our own list,
     * which as FIXME above says could be simply htable since we only care about
     * start_pc (for now).
     * We assume we only add start pcs to the vector.
     */
    return vmvector_overlap(app_flushed_areas, pc, pc + 1);
}
#    endif
#endif

/****************************************************************************/

/* a helper function for check_thread_vm_area
 * assumes caller owns executable_areas write lock */
static void
handle_delay_readonly(dcontext_t *dcontext, app_pc pc, vm_area_t *area)
{
    ASSERT_OWN_WRITE_LOCK(true, &executable_areas->lock);
    ASSERT(TESTALL(VM_DELAY_READONLY | VM_WRITABLE, area->vm_flags));
    /* should never get a selfmod region here, to be marked selfmod
     * would already have had to execute (to get faulting write)
     * so region would already have had to go through here */
    ASSERT(!TEST(FRAG_SELFMOD_SANDBOXED, area->frag_flags));
    if (!is_on_stack(dcontext, pc, NULL) && INTERNAL_OPTION(hw_cache_consistency)) {
        vm_make_unwritable(area->start, area->end - area->start);
        area->vm_flags |= VM_MADE_READONLY;
    } else {
        /* this could happen if app changed mem protection on its
         * stack that triggered us adding a delay_readonly writable
         * region to the executable list in
         * app_memory_protection_change() */
        ASSERT_CURIOSITY(false);
        area->frag_flags |= FRAG_SELFMOD_SANDBOXED;
    }
    area->vm_flags &= ~VM_DELAY_READONLY;
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "\tMarking existing wx vm_area_t ro for consistency, "
        "area " PFX " - " PFX ", target pc " PFX "\n",
        area->start, area->end, pc);
    STATS_INC(num_delayed_rw2r);
}

/* Frees resources acquired in check_thread_vm_area().
 * data and vmlist need to match those used in check_thread_vm_area().
 * abort indicates that we are forging and exception or killing a thread
 * or some other drastic action that will not return to the caller
 * of check_thread_vm_area.
 * own_execareas_writelock indicates whether the executable_areas
 * write lock is currently held, while caller_execareas_writelock
 * indicates whether the caller held that lock and thus we should not
 * free it unless we're aborting.
 * If both clean_bb and abort are true, calls bb_build_abort.
 */
static void
check_thread_vm_area_cleanup(dcontext_t *dcontext, bool abort, bool clean_bb,
                             thread_data_t *data, void **vmlist,
                             bool own_execareas_writelock,
                             bool caller_execareas_writelock)
{
    if (own_execareas_writelock && (!caller_execareas_writelock || abort)) {
        ASSERT(self_owns_write_lock(&executable_areas->lock));
        d_r_write_unlock(&executable_areas->lock);
#ifdef HOT_PATCHING_INTERFACE
        if (DYNAMO_OPTION(hot_patching)) {
            ASSERT(self_owns_write_lock(hotp_get_lock()));
            d_r_write_unlock(hotp_get_lock());
        }
#endif
    }
    ASSERT(!caller_execareas_writelock || self_owns_write_lock(&executable_areas->lock));
    /* FIXME: could we have multiply-nested vmlist==NULL where we'd need to
     * release read lock more than once? */
    if (vmlist == NULL)
        SHARED_VECTOR_RWLOCK(&data->areas, read, unlock);
    if (self_owns_write_lock(&data->areas.lock) && (vmlist != NULL || abort)) {
        /* Case 9376: we can forge an exception for vmlist==NULL, in which case
         * we must release the write lock from the prior layer;
         * we can also have a decode fault with vmlist!=NULL but w/o holding
         * the vm areas lock.
         */
        SHARED_VECTOR_RWLOCK(&data->areas, write, unlock);
    } /* we need to not unlock vmareas for nested check_thread_vm_area() call */
    if (abort) {
        if (vmlist != NULL && *vmlist != NULL) {
            vm_area_destroy_list(dcontext, *vmlist);
        }
        if (clean_bb) {
            /* clean up bb_building_lock and IR */
            bb_build_abort(dcontext, false /*don't call back*/, true /*unlock*/);
        }
    }
}

/* Releases any held locks.  Up to caller to free vmlist.
 * Flags are reverse logic, just like for check_thread_vm_area()
 */
void
check_thread_vm_area_abort(dcontext_t *dcontext, void **vmlist, uint flags)
{
    thread_data_t *data;
    if (DYNAMO_OPTION(shared_bbs) &&
        !TEST(FRAG_SHARED, flags)) { /* yes, reverse logic, see comment above */
        data = shared_data;
    } else {
        data = (thread_data_t *)dcontext->vm_areas_field;
    }
    check_thread_vm_area_cleanup(dcontext, true, false /*caller takes care of bb*/, data,
                                 vmlist, self_owns_write_lock(&executable_areas->lock),
                                 self_owns_write_lock(&data->areas.lock));
}

static bool
allow_xfer_for_frag_flags(dcontext_t *dcontext, app_pc pc, uint src_flags, uint tgt_flags)
{
    /* the flags we don't allow a direct cti to bridge if different */
    const uint frag_flags_cmp = FRAG_SELFMOD_SANDBOXED | FRAG_COARSE_GRAIN
#ifdef PROGRAM_SHEPHERDING
        | FRAG_DYNGEN
#endif
        ;
    uint src_cmp = src_flags & frag_flags_cmp;
    uint tgt_cmp = tgt_flags & frag_flags_cmp;
    bool allow = (src_cmp == tgt_cmp) ||
        /* Case 8917: hack to allow elision of call* to vsyscall-in-ntdll,
         * while still ruling out fine fragments coming in to coarse regions
         * (where we'd rather stop the fine and build a (cheaper) coarse bb).
         * Use == instead of TEST to rule out any other funny flags.
         */
        (src_cmp == 0 /* we removed FRAG_COARSE_GRAIN to make this fine */
         && tgt_cmp == FRAG_COARSE_GRAIN /* still in coarse region though */
         && TEST(FRAG_HAS_SYSCALL, src_flags));
    if (TEST(FRAG_COARSE_GRAIN, src_flags)) {
        /* FIXME case 8606: we can allow intra-module xfers but we have no
         * way of checking here -- would have to check in
         * interp.c:check_new_page_jmp().  So for now we disallow all xfers.
         * If our regions match modules exactly we shouldn't see any
         * intra-module direct xfers anyway.
         */
        /* N.B.: ibl entry removal (case 9636) assumes coarse fragments
         * stay bounded within contiguous FRAG_COARSE_GRAIN regions
         */
        allow = false;
    }
    if (!allow) {
        LOG(THREAD, LOG_VMAREAS, 3,
            "change in vm area flags (0x%08x vs. 0x%08x %d): "
            "stopping at " PFX "\n",
            src_flags, tgt_flags, TEST(FRAG_COARSE_GRAIN, src_flags), pc);
        DOSTATS({
            if (TEST(FRAG_COARSE_GRAIN, tgt_flags))
                STATS_INC(elisions_prevented_for_coarse);
        });
    }
    return allow;
}

/* check origins of code for several purposes:
 * 1) we need list of areas where this thread's fragments come
 *    from, for faster flushing on munmaps
 * 2) also for faster flushing, each vmarea has a list of fragments
 * 3) we need to mark as read-only any writable region that
 *    has a fragment come from it, to handle self-modifying code
 * 4) for PROGRAM_SHEPHERDING for security
 *
 * We keep a list of vm areas per thread, to make flushing fragments
 * due to memory unmaps faster
 * This routine adds the page containing start to the thread's list.
 * Adds any FRAG_ flags relevant for a fragment overlapping start's page.
 * If xfer and encounters change in vmareas flags, returns false and does NOT
 * add the new page to the list for this fragment -- assumes caller will NOT add
 * it to the current bb.  This allows for selectively not following direct ctis.
 * Assumes only building a real app bb if vmlist!=NULL -- assumes that otherwise
 * caller is reconstructing an app bb or some other secondary bb walk.
 * If returns true, returns in the optional stop OUT parameter the final pc of
 * this region (open-ended).
 */
bool
check_thread_vm_area(dcontext_t *dcontext, app_pc pc, app_pc tag, void **vmlist,
                     uint *flags, app_pc *stop, bool xfer)
{
    bool result;
    thread_data_t *data;
    bool in_last = false;
    uint frag_flags = 0;
    uint vm_flags = 0;
    bool ok;
#ifdef PROGRAM_SHEPHERDING
    bool shared_to_private = false;
#endif
    /* used for new area */
    app_pc base_pc = 0;
    size_t size = 0; /* set only for unknown areas */
    uint prot = 0;   /* set only for unknown areas */
    /* both area and local_area either point to thread-local vector, for which
     * we do not need a lock, or to a shared area, for which we hold
     * a read or a write lock (either is sufficient) the entire time
     */
    vm_area_t *area = NULL;
    vm_area_t *local_area = NULL; /* entry for this thread */
    vm_area_t area_copy;          /* local copy, so can let go of lock */
    /* we can be recursively called (check_origins() calling build_app_bb_ilist())
     * so make sure we don't re-try to get a lock we already hold
     */
    bool caller_execareas_writelock = self_owns_write_lock(&executable_areas->lock);
    bool own_execareas_writelock = caller_execareas_writelock;
    DEBUG_DECLARE(const char *new_area_prefix;)

    /* deadlock issues if write lock is held already for vmlist!=NULL case */
    ASSERT(vmlist == NULL || !caller_execareas_writelock);
#ifdef HOT_PATCHING_INTERFACE
    /* hotp_vul_table_lock goes hand in hand w/ executable_areas lock here */
    ASSERT(!DYNAMO_OPTION(hot_patching) ||
           (own_execareas_writelock && self_owns_write_lock(hotp_get_lock())) ||
           (!own_execareas_writelock && !self_owns_write_lock(hotp_get_lock())));
#endif

    ASSERT(flags != NULL);

    /* don't know yet whether this bb will be shared, but a good chance,
     * so we guess shared and will rectify later.
     * later, to add to local instead, we call again, and to tell the difference
     * we perversely pass FRAG_SHARED
     */
    if (DYNAMO_OPTION(shared_bbs) &&
        /* for TEMP_PRIVATE we make private up front */
        !TEST(FRAG_TEMP_PRIVATE, *flags) &&
        !TEST(FRAG_SHARED, *flags)) { /* yes, reverse logic, see comment above */
        data = shared_data;
        DODEBUG({ new_area_prefix = "new shared vm area: "; });
        if (vmlist == NULL) { /* not making any state changes to vm lists */
            /* need read access only, for lookup and holding ptr into vector */
            SHARED_VECTOR_RWLOCK(&data->areas, read, lock);
        } else { /* building a bb */
            /* need write access later, and want our lookup to be bundled
             * with our writes so we don't rely on the bb building lock,
             * so we grab the write lock for the whole routine
             */
            SHARED_VECTOR_RWLOCK(&data->areas, write, lock);
        }
    } else {
        DODEBUG({ new_area_prefix = "new vm area for thread: "; });
        data = (thread_data_t *)dcontext->vm_areas_field;
#ifdef PROGRAM_SHEPHERDING
        if (DYNAMO_OPTION(shared_bbs) && TEST(FRAG_SHARED, *flags))
            shared_to_private = true;
#endif
    }

    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 4, "check_thread_vm_area: pc = " PFX "\n", pc);

    /* no lock on data->areas needed if thread-local,
     * if shared we grabbed either read or write lock above
     */
    /* check cached last area first to avoid lookup cost */
    if (data->last_area != NULL)
        in_last = (pc < data->last_area->end && data->last_area->start <= pc);

    DOSTATS({
        STATS_INC(checked_addresses);
        if (in_last)
            STATS_INC(looked_up_in_last_area);
    });

    if (in_last) {
        local_area = data->last_area;
        area = local_area;
    } else if (lookup_addr(&data->areas, pc, &local_area)) {
        /* ok to hold onto pointer since it's this thread's */
        area = local_area;
    } else {
        bool is_allocated_mem;
        /* not in this thread's current executable list
         * try the global executable area list
         */
#ifdef LINUX
        /* i#1760: an app module loaded by custom loader (e.g., bionic libc)
         * might not be detected by DynamoRIO in process_mmap, so we check
         * whether it is an unseen module here.
         */
        os_check_new_app_module(dcontext, pc);
#endif
        /* i#884: module load event is now on first execution */
        instrument_module_load_trigger(pc);
        if (!own_execareas_writelock)
            d_r_read_lock(&executable_areas->lock);
        ok = lookup_addr(executable_areas, pc, &area);
        if (ok && TEST(VM_DELAY_READONLY, area->vm_flags)) {
            /* need to mark region read only for consistency
             * need to upgrade to write lock, have to release lock first
             * then recheck conditions after grabbing hotp + write lock */
            if (!own_execareas_writelock) {
                d_r_read_unlock(&executable_areas->lock);
#ifdef HOT_PATCHING_INTERFACE
                /* Case 8780: due to lock rank issues we must grab the hotp lock
                 * prior to the exec areas lock, as the hotp lock may be needed
                 * for pc recreation in check_origins().  We assume this will
                 * not cause noticeable lock contention.
                 */
                if (DYNAMO_OPTION(hot_patching))
                    d_r_write_lock(hotp_get_lock());
#endif
                d_r_write_lock(&executable_areas->lock);
                own_execareas_writelock = true;
                ok = lookup_addr(executable_areas, pc, &area);
            }
            if (ok && TEST(VM_DELAY_READONLY, area->vm_flags))
                handle_delay_readonly(dcontext, pc, area);
        }
        if ((!ok || (ok && vmlist != NULL && !TEST(VM_EXECUTED_FROM, area->vm_flags))) &&
            !own_execareas_writelock) {
            /* we must hold the write lock until we add the new region, as we
             * may want to give it selfmod or other properties that will not mix
             * well if we have a race and another thread adds an overlapping
             * region with different properties!
             * or if never executed from, we need to mark the area as such
             * (if we didn't support thread-private, we would just grab write
             * lock up front and not bother with read lock).
             */
            d_r_read_unlock(&executable_areas->lock);
#ifdef HOT_PATCHING_INTERFACE
            if (DYNAMO_OPTION(hot_patching))
                d_r_write_lock(hotp_get_lock()); /* case 8780 -- see comments above */
#endif
            d_r_write_lock(&executable_areas->lock);
            own_execareas_writelock = true;
            ok = lookup_addr(executable_areas, pc, &area);
        }
        if (ok) {
            if (vmlist != NULL && !TEST(VM_EXECUTED_FROM, area->vm_flags)) {
                ASSERT(self_owns_write_lock(&executable_areas->lock));
                area->vm_flags |= VM_EXECUTED_FROM;
            }
            area_copy = *area;
            area = &area_copy;
            /* if we already have an area, we do not need to hold an execareas
             * lock, as there is no race within this routine.  any removal of
             * the area must go through the flush synch and so cannot be
             * concurrent to this routine.
             */
            if (own_execareas_writelock) {
                if (!caller_execareas_writelock) {
                    d_r_write_unlock(&executable_areas->lock);
#ifdef HOT_PATCHING_INTERFACE
                    if (DYNAMO_OPTION(hot_patching))
                        d_r_write_unlock(hotp_get_lock()); /* case 8780 -- see above */
#endif
                    own_execareas_writelock = false;
                }
            } else
                d_r_read_unlock(&executable_areas->lock);
        }
        /* if ok we should not own the readlock but we can't assert on that */
        ASSERT(
            ok ||
            (self_owns_write_lock(&executable_areas->lock) &&
             own_execareas_writelock IF_HOTP(&&(!DYNAMO_OPTION(hot_patching) ||
                                                self_owns_write_lock(hotp_get_lock())))));
        ASSERT(!ok || area != NULL);
        is_allocated_mem = get_memory_info(pc, &base_pc, &size, &prot);
        /* i#2135 : it can be a guard page if either ok or not ok
         * so we have to get protection value right now
         */
#ifdef WINDOWS
        if (TEST(DR_MEMPROT_GUARD, prot)) {
            /* remove protection so as to go on */
            if (unmark_page_as_guard(pc, prot)) {
                /* We test that there was still the guard protection to remove.
                 * Otherwise, there could be a race condition with
                 * two threads trying to execute from the guarded page
                 * and we would raise two exceptions instead of one.
                 */
                SYSLOG_INTERNAL_WARNING("Application tried to execute "
                                        "from guard memory " PFX ".\n",
                                        pc);
                check_thread_vm_area_cleanup(dcontext, true /*abort*/, true /*clean bb*/,
                                             data, vmlist, own_execareas_writelock,
                                             caller_execareas_writelock);
                os_forge_exception(pc, GUARD_PAGE_EXCEPTION);
                ASSERT_NOT_REACHED();
            }
        }
#endif

        if (!ok) {
            /* we no longer allow execution from arbitrary dr mem, our dll is
             * on the executable list and we specifically add the callback
             * interception code */
            bool is_in_dr = is_dynamo_address(pc);
            /* this is an unknown or dr area
             * we may need to return false, if flags change or if pc is
             * unreadable (and so we don't want to follow a direct cti there
             * until the app actually does)
             */
            bool is_being_unloaded = false;

            /* Clients are allowed to use DR-allocated memory as app code:
             * we give up some robustness by allowing any DR-allocated memory
             * outside of the code cache that is marked as +x (we do not allow
             * -x to avoid a wild jump targeting our own heap and our own cache
             * cons policy making the heap read-only and causing a DR crash:
             * xref DrM#1820).
             * XXX i#852: should we instead have some dr_appcode_alloc() or
             * dr_appcode_mark() API?
             */
            if (is_in_dr && INTERNAL_OPTION(code_api) && TEST(MEMPROT_EXEC, prot) &&
                !in_fcache(pc))
                is_in_dr = false; /* allow it */

            if (!is_allocated_mem) {
                /* case 9022 - Kaspersky sports JMPs to a driver in
                 * kernel address space e.g. jmp f7ab7d67
                 * and system call queries refuse to provide any information.
                 * We need to just try reading from that address.
                 */

                /* we first compare to
                 * SYSTEM_BASIC_INFORMATION.HighestUserAddress (2GB or
                 * 3GB) to know for sure we're testing a kernel
                 * address, and not dealing with a race instead.
                 */

                if (!is_user_address(pc) && is_readable_without_exception_try(pc, 1)) {
                    SYSLOG_INTERNAL_WARNING_ONCE(
                        "Readable kernel address space memory at " PFX ".\n"
                        "case 9022 seen with Kaspersky AV",
                        pc);
                    /* FIXME: we're constructing these flags with the
                     * intent to allow this region, any other
                     * characteristics are hard to validate
                     */
                    is_allocated_mem = true;
                    base_pc = (app_pc)ALIGN_BACKWARD(pc, PAGE_SIZE);
                    size = PAGE_SIZE;
                    prot = MEMPROT_READ | MEMPROT_EXEC;

                    /* FIXME: note we could also test for
                     * MEMPROT_WRITE, note that explicitly turn on
                     * SANDBOX_FLAG() anyways.  Luckily, the one known
                     * case where this is needed doesn't leave its
                     * driver space writable.
                     */

                    vm_flags |= VM_DRIVER_ADDRESS;
                    /* we mark so that we can add to executable_areas
                     * list later, and as in the only current example
                     * so that we can allow execution.  FIXME: Note
                     * we'll never remove this area.  We could check
                     * on a future access whether such an address is
                     * still readable, and then we can remove it if
                     * the address stops being readable.  Note that we
                     * can never tell if this area has disappeared -
                     * since we won't get notified on memory changes.
                     * So we may be more likely to get a decode fault
                     * if these ever happen.
                     */
                    /* FIXME: we don't support this on Linux where
                     * we'd have to also add to all_memory_areas */

                    /* Note it is better to ALWAYS turn on
                     * SANDBOX_FLAG for these fragments since it is
                     * not clear that we can control any writes to
                     * them from kernel space FIXME: may be
                     * unnecessary in the case of Kaspersky.
                     * insert_selfmod_sandbox() will suppress
                     * sandbox2ro_threshold for VM_DRIVER_ADDRESS areas
                     */
                    frag_flags |= SANDBOX_FLAG();
                    /* FIXME: could do this under an option */
                } else {
                    /* just a bad address in kernel space - like 0xdeadbeef */
                }
            } else {
                /* check for race where DLL is still present, but no
                 * longer on our list.
                 */
                is_being_unloaded = is_unreadable_or_currently_unloaded_region(pc);
                /* note here we'll forge an exception to the app,
                 * even if the address is practically still readable
                 */
                if (is_being_unloaded) {
                    STATS_INC(num_unloaded_race_code_origins);
                    SYSLOG_INTERNAL_WARNING_ONCE("Application executing from unloaded "
                                                 "address " PFX "\n",
                                                 pc);
                }
            }

            /* if target unreadable, app will die, so make sure we don't die
             * instead, NOTE we treat dr memory as unreadable because of app
             * races (see bug 2574) and the fact that we don't yet expect
             * targeted attacks against dr */
            /* case 9330 tracks a violation while we are unloading,
             * but address shouldn't be on a new futureexec_area (case 9371)
             */
#ifdef WINDOWS
            if (in_private_library(pc)) {
                /* Privately-loaded libs are put on the DR list, and if the app
                 * ends up executing from them they can come here.  We assert
                 * in debug build but let it go in release.  But, we first
                 * have to swap to native execution of FLS callbacks, which
                 * we cannot use our do-not-inline on b/c they're call* targets.
                 */
                if (private_lib_handle_cb(dcontext, pc)) {
                    /* Did the native call and set up to interpret at retaddr */
                    check_thread_vm_area_cleanup(
                        dcontext, true /*redirecting*/, true /*clean bb*/, data, vmlist,
                        own_execareas_writelock, caller_execareas_writelock);
                    /* avoid assert in dispatch_enter_dynamorio() */
                    dcontext->whereami = DR_WHERE_TRAMPOLINE;
                    set_last_exit(
                        dcontext,
                        (linkstub_t *)get_ibl_sourceless_linkstub(LINK_RETURN, 0));
                    if (is_couldbelinking(dcontext))
                        enter_nolinking(dcontext, NULL, false);
                    KSTART(fcache_default);
                    transfer_to_dispatch(dcontext, get_mcontext(dcontext),
                                         true /*full_DR_state*/);
                    ASSERT_NOT_REACHED();
                }
                CLIENT_ASSERT(false,
                              "privately-loaded library executed by app: "
                              "please report this transparency violation");
            }
#endif
            if ((is_in_dr IF_WINDOWS(&&!in_private_library(pc))) || !is_allocated_mem ||
                prot == 0 /*no access flags*/ || is_being_unloaded) {
                if (xfer) {
                    /* don't follow cti, wait for app to get there and then
                     * handle this (might be pathological case where cti is
                     * never really followed)
                     */

                    /* Note for case 9330 that for direct xfer we want
                     * to be able to recreate the scenario after we
                     * stop.  Even though is_being_unloaded is a
                     * transient property, since we treat unreadable
                     * the same way, next time we get here we'll be
                     * ok.  We already have to make sure we don't
                     * missclassify futureexec_areas so can't really
                     * get here.  Normal module unloads would have
                     * flushed all other bb's.
                     */

                    LOG(THREAD, LOG_VMAREAS, 3,
                        "cti targets %s " PFX ", stopping bb here\n",
                        is_in_dr ? "dr" : "unreadable", pc);
                    result = false;
                    goto check_thread_return;
                } else {
                    /* generate sigsegv as though target application
                     * instruction being decoded generated it
                     */
                    /* FIXME : might be pathalogical selfmod case where
                     * app in fact jumps out of block before reaching the
                     * unreadable memory */
                    if (vmlist == NULL) {
                        /* Case 9376: check_origins_bb_pattern() can get here
                         * w/ vmlist==NULL.  We have to be careful to free
                         * resources of the prior vmlist and the vmarea write lock.
                         */
                        SYSLOG_INTERNAL_INFO("non-bb-build app decode found "
                                             "unreadable memory");
                    }
                    LOG(GLOBAL, LOG_VMAREAS, 1,
                        "application tried to execute from %s " PFX
                        " is_allocated_mem=%d prot=0x%x\n",
                        is_in_dr ? "dr" : "unreadable", pc, is_allocated_mem, prot);
                    LOG(THREAD, LOG_VMAREAS, 1,
                        "application tried to execute from %s " PFX
                        " is_allocated_mem=%d prot=0x%x\n",
                        is_in_dr ? "dr" : "unreadable", pc, is_allocated_mem, prot);
                    DOLOG(1, LOG_VMAREAS, {
                        dump_callstack(pc,
                                       (app_pc)get_mcontext_frame_ptr(
                                           dcontext, get_mcontext(dcontext)),
                                       THREAD, DUMP_NOT_XML);
                    });

                    /* FIXME: what if the app masks it with an exception
                     * handler? */
                    SYSLOG_INTERNAL_WARNING_ONCE(
                        "Application tried to execute from %s memory " PFX ".\n"
                        "This may be a result of an unsuccessful attack or a potential "
                        "application vulnerability.",
                        is_in_dr ? "dr" : "unreadable", pc);
                    /* Not logged as a security violation, but still an
                     * external warning, We don't want to take blame for all
                     * program bugs that overwrite EIP with invalid addresses,
                     * yet it may help discovering new security holes.
                     * [Although, watching for crashes of 0x41414141 can't read
                     *  0x41414141 helps.]
                     *It may also be a failing attack..
                     */

                    check_thread_vm_area_cleanup(
                        dcontext, true /*abort*/, true /*clean bb*/, data, vmlist,
                        own_execareas_writelock, caller_execareas_writelock);

                    /* Create an exception record for this failure */
                    if (TEST(DUMPCORE_FORGE_UNREAD_EXEC, DYNAMO_OPTION(dumpcore_mask))) {
                        os_dump_core("Warning: App trying to execute from unreadable "
                                     "memory");
                    }
                    os_forge_exception(pc, UNREADABLE_MEMORY_EXECUTION_EXCEPTION);
                    ASSERT_NOT_REACHED();
                }
            }

            /* set all flags that don't intermix now */
#ifdef PROGRAM_SHEPHERDING
#    ifdef WINDOWS
            /* Don't classify the vsyscall code page as DGC for our purposes,
             * since we permit execution from that region. This is needed
             * for Windows XP/2003 pre-SP2 on which the code page is not
             * part of ntdll.
             * FIXME What about SP1?
             * FIXME A better soln is to add the region to the exec list
             * during os init and remove this specialized check.
             */
            if (!is_dyngen_vsyscall(pc))
#    endif
                frag_flags |= FRAG_DYNGEN;
#endif
#ifdef WINDOWS
            if ((prot & MEMPROT_WRITE) != 0 && is_on_stack(dcontext, pc, NULL)) {
                /* On win32, kernel kills process if esp is bad,
                 * doesn't even call KiUserExceptionDispatcher entry point!
                 * Thus we cannot make this region read-only.
                 * We must treat it as self-modifying code, and sandbox
                 * the whole thing, to guarantee cache consistency.
                 * FIXME: esp can point anywhere, so other regions we make
                 * read-only may end up becoming "stack", and then we'll
                 * just silently fail on a write there!!!
                 */
                frag_flags |= SANDBOX_FLAG();
                STATS_INC(num_selfmod_vm_areas);
            }
#endif
        }
    }
    if (area != NULL) {
        ASSERT_CURIOSITY(vmlist == NULL || !TEST(VM_DELETE_ME, area->vm_flags));
        if (vmlist != NULL && TEST(FRAG_COARSE_GRAIN, area->frag_flags)) {
            /* We assume get_executable_area_coarse_info() is called prior to
             * execution in a coarse region.  We go ahead and initialize here
             * though we could wait if a xfer since the bb will not cross.
             */
            DEBUG_DECLARE(coarse_info_t *info =)
            get_coarse_info_internal(pc, true /*init*/, true /*have shvm lock*/);
            ASSERT(info != NULL);
        }
        ASSERT(!TEST(FRAG_COARSE_GRAIN, area->frag_flags) ||
               get_coarse_info_internal(pc, false /*no init*/, false /*no lock*/) !=
                   NULL);
        frag_flags |= area->frag_flags;

#ifdef PROGRAM_SHEPHERDING
        if (vmlist != NULL && /* only for bb building */
            TEST(VM_PATTERN_REVERIFY, area->vm_flags) &&
            !shared_to_private /* ignore shared-to-private conversion */) {
            /* case 8168: sandbox2ro_threshold can turn into a non-sandboxed region,
             * and our re-verify won't change that as the region is already on the
             * executable list.  It will all work fine though.
             */
            ASSERT(DYNAMO_OPTION(sandbox2ro_threshold) > 0 ||
                   TEST(FRAG_SELFMOD_SANDBOXED, area->frag_flags));
            /* Re-verify the code origins policies, unless we are ensuring that
             * the end of the pattern is ok.  This fixes case 4020 where another
             * thread can use a pattern region for non-pattern code.
             */
            area = NULL; /* clear to force a re-verify */
            /* Ensure we have prot */
            get_memory_info(pc, &base_pc, &size, &prot);
            /* satisfy lock asumptions when area == NULL */
            if (!own_execareas_writelock) {
#    ifdef HOT_PATCHING_INTERFACE
                if (DYNAMO_OPTION(hot_patching))
                    d_r_write_lock(hotp_get_lock()); /* case 8780 -- see comments above */
#    endif
                d_r_write_lock(&executable_areas->lock);
                own_execareas_writelock = true;
            }
        }
#endif
    }

    /* Ensure we looked up the mem attributes, if a new area */
    ASSERT(area != NULL || size > 0);
    /* FIXME: fits nicely down below as alternative to marking read-only,
     * but must be here for vm==NULL so will stop bb at cti -- although
     * here it gets executed multiple times until actually switch to sandboxing
     */
    if (area == NULL && DYNAMO_OPTION(ro2sandbox_threshold) > 0 &&
        TEST(MEMPROT_WRITE, prot) && !TEST(FRAG_SELFMOD_SANDBOXED, frag_flags)) {
        vm_area_t *w_area; /* can't clobber area here */
        ro_vs_sandbox_data_t *ro2s = NULL;
        /* even though area==NULL this can still be an exec-writable area
         * if area is sub-page!  we can't change to sandboxing w/ sub-page
         * regions on the same page, so we wait until come here the 1st time
         * after a flush (which will flush the whole os region).  thus, the
         * threshold is really just a lower bound.  FIXME: add stats on this case!
         */
        ASSERT(own_execareas_writelock);
#ifdef HOT_PATCHING_INTERFACE
        ASSERT(!DYNAMO_OPTION(hot_patching) || self_owns_write_lock(hotp_get_lock()));
#endif
        ASSERT(self_owns_write_lock(&executable_areas->lock));
        if (!is_executable_area_writable(pc)) { /* ok to read as a writer */
            /* see whether this region has been cycling on and off the list due
             * to being written to -- if so, switch to sandboxing
             */
            d_r_read_lock(&written_areas->lock);
            ok = lookup_addr(written_areas, pc, &w_area);
            if (ok)
                ro2s = (ro_vs_sandbox_data_t *)w_area->custom.client;
            if (ok && ro2s->written_count >= DYNAMO_OPTION(ro2sandbox_threshold)) {
                LOG(GLOBAL, LOG_VMAREAS, 1,
                    "new executable area " PFX "-" PFX " written >= %dX => "
                    "switch to sandboxing\n",
                    base_pc, base_pc + size, DYNAMO_OPTION(ro2sandbox_threshold));
                DOSTATS({
                    if (vmlist != NULL) /* don't count non-build calls */
                        STATS_INC(num_ro2sandbox);
                });
                /* TODO FOR PERFORMANCE:
                 * -- if app appending to area of jitted code, make threshold big enough
                 * so will get off page
                 * -- modern jit shouldn't really have data on same page: all jitted
                 * code should be combined
                 * -- we're using OS regions b/c we merge ours, but if writer and writee
                 * are on sep pages but in same OS region, we'll keep in cycle when we
                 * could simply split region!  even if peel off written-to pages here,
                 * (can't at flush time as must flush whole vm region)
                 * if exec even once from target page, will add entire since we
                 * merge, and will flush entire since flush bounds suggested by
                 * OS regions (and must flush entire merged vmarea since that's
                 * granularity of frags list).  still, worth splitting, even if
                 * will merge back, to not lose perf if writee is on
                 * never-executed page!  to impl, want another vm vector in
                 * which, at flush time, we store bounds for next exec.
                 */
                frag_flags |= SANDBOX_FLAG();
                /* for sandboxing best to stay at single-page regions */
                base_pc = (app_pc)PAGE_START(pc);
                size = PAGE_SIZE;
                /* We do not clear the written count as we're only doing one page
                 * here.  We want the next exec in the same region to also be
                 * over the threshold.
                 */
                DODEBUG({ ro2s->ro2s_xfers++; });
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "\tsandboxing just the page " PFX "-" PFX "\n", base_pc,
                    base_pc + size);
            }
            d_r_read_unlock(&written_areas->lock);
        } else
            STATS_INC(num_ro2sandbox_other_sub);
    }

    /* now that we know about new area, decide whether it's compatible to be
     * in the same bb as previous areas, as dictated by old flags
     * N.B.: we only care about FRAG_ flags here, not VM_ flags
     */
    if (xfer && !allow_xfer_for_frag_flags(dcontext, pc, *flags, frag_flags)) {
        result = false;
        goto check_thread_return;
    }

    /* Normally we return the union of flags from all vmarea regions touched.
     * But if one region is coarse and another fine, we do NOT want the union,
     * but rather we want the whole thing to be fine.  FIXME: We could also try
     * to put in functionality to truncate at the region boundary.
     * Case 9932: in fact we cannot allow touching two adjacent coarse regions.
     */
    /* N.B.: ibl entry removal (case 9636) assumes coarse fragments
     * stay bounded within a single FRAG_COARSE_GRAIN region
     */
    if (TEST(FRAG_COARSE_GRAIN, frag_flags) && pc != tag /*don't cmp to nothing*/ &&
        ((*flags & FRAG_COARSE_GRAIN) != (frag_flags & FRAG_COARSE_GRAIN) ||
         area == NULL || area->start > tag)) {
        *flags &= ~FRAG_COARSE_GRAIN;
        frag_flags &= ~FRAG_COARSE_GRAIN; /* else we'll re-add below */
        DOSTATS({
            if (vmlist != NULL)
                STATS_INC(coarse_overlap_with_fine);
        });
    }

    if (vmlist == NULL) {
        /* caller only cared about whether to follow direct cti, so exit now, don't
         * make any persistent state changes
         */
        *flags |= frag_flags;
        if (stop != NULL) {
            if (area == NULL)
                *stop = base_pc + size;
            else
                *stop = area->end;
        }
        ASSERT(*stop != NULL);
        result = true;
        goto check_thread_return;
    }
    /* once reach this point we're building a real bb */

#ifdef SIMULATE_ATTACK
    simulate_attack(dcontext, pc);
#endif /* SIMULATE_ATTACK */

    if (area == NULL /* unknown area */) {
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "WARNING: " PFX " -> " PFX "-" PFX
            " %s%s is not on executable list (thread " TIDFMT ")\n",
            pc, base_pc, base_pc + size, ((prot & MEMPROT_WRITE) != 0) ? "W" : "",
            ((prot & MEMPROT_EXEC) != 0) ? "E" : "", dcontext->owning_thread);
        DOLOG(3, LOG_VMAREAS, { print_executable_areas(GLOBAL); });
        DODEBUG({
            if (is_on_stack(dcontext, pc, NULL)) {
                SYSLOG_INTERNAL_WARNING_ONCE("executing region with pc " PFX " on "
                                             "the stack.",
                                             pc);
            }
        });
#ifdef DGC_DIAGNOSTICS
        dyngen_diagnostics(dcontext, pc, base_pc, size, prot);
#endif

#ifdef PROGRAM_SHEPHERDING
        /* give origins checker a chance to change region
         * N.B.: security violation reports in detect_mode assume that at
         * this point we aren't holding pointers into vectors, since the
         * shared vm write lock is released briefly for the diagnostic report.
         */
        if (DYNAMO_OPTION(code_origins) &&
            !shared_to_private) { /* don't check for shared-to-private conversion */
            int res = check_origins(dcontext, pc, &base_pc, &size, prot, &vm_flags,
                                    &frag_flags, xfer);
            if (res < 0) {
                if (!xfer) {
                    action_type_t action = security_violation_main(
                        dcontext, pc, res, OPTION_BLOCK | OPTION_REPORT);
                    if (action != ACTION_CONTINUE) {
                        check_thread_vm_area_cleanup(
                            dcontext, true /*abort*/, true /*clean bb*/, data, vmlist,
                            own_execareas_writelock, caller_execareas_writelock);
                        security_violation_action(dcontext, action, pc);
                        ASSERT_NOT_REACHED();
                    }
                } else {
                    /* if xfer, we simply don't follow the xfer */
                    LOG(THREAD, LOG_VMAREAS, 3,
                        "xfer to " PFX " => violation, so stopping at " PFX "\n", base_pc,
                        pc);
                    result = false;
                    goto check_thread_return;
                }
            }
        }
#endif

        /* make sure code is either read-only or selfmod sandboxed */
        /* making unwritable and adding to exec areas must be atomic
         * (another thread could get what would look like app seg fault in between!)
         * and selfmod flag additions, etc. have restrictions, so we must have
         * held the write lock the whole time
         */
        ASSERT(own_execareas_writelock);
        ok = lookup_addr(executable_areas, pc, &area);
        if (ok) {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "\tNew executable region is on page already added!\n");
#ifdef FORENSICS_ACQUIRES_INITEXIT_LOCK
            /* disabled until case 6141 is resolved: no lock release needed for now */
            /* if we release the exec areas lock to emit forensic info, then
             * someone else could have added the region since we checked above.
             * see if we need to handle the DELAY_READONLY flag.
             */
            if (TEST(VM_DELAY_READONLY, area->vm_flags))
                handle_delay_readonly(dcontext, pc, area);
            else {
#endif
#ifdef PROGRAM_SHEPHERDING
                /* else, this can only happen for pattern reverification: no races! */
                ASSERT(TEST(VM_PATTERN_REVERIFY, area->vm_flags) &&
                       TEST(FRAG_SELFMOD_SANDBOXED, area->frag_flags));
#else
            ASSERT_NOT_REACHED();
#endif
#ifdef FORENSICS_ACQUIRES_INITEXIT_LOCK
            }
#endif
        } else {
            /* need to add the region */
            if (TEST(MEMPROT_WRITE, prot)) {
                vm_flags |= VM_WRITABLE;
                STATS_INC(num_writable_code_regions);
                /* Now that new area bounds are finalized, see if it should be
                 * selfmod.  Mainly this is a problem with a subpage region on the
                 * same page as an existing subpage selfmod region.  We want the new
                 * region to be selfmod to avoid forcing the old to switch to page
                 * protection.  We won't have to do this once we separate the
                 * consistency region list from the code origins list (case 3744):
                 * then we'd have the whole page as selfmod on the consistency list,
                 * with only the valid subpage on the origins list.  We don't mark
                 * pieces of a large region, for simplicity.
                 */
                if (is_executable_area_on_all_selfmod_pages(base_pc, base_pc + size)) {
                    frag_flags |= SANDBOX_FLAG();
                }
                /* case 8308: We've added options to force certain regions to
                 * use selfmod instead of RO.  -sandbox_writable causes all writable
                 * regions to be selfmod.  -sandbox_non_text causes all non-text
                 * writable regions to be selfmod.
                 */
                else if (DYNAMO_OPTION(sandbox_writable)) {
                    frag_flags |= SANDBOX_FLAG();
                } else if (DYNAMO_OPTION(sandbox_non_text)) {
                    app_pc modbase = get_module_base(base_pc);
                    if (modbase == NULL ||
                        !is_range_in_code_section(modbase, base_pc, base_pc + size, NULL,
                                                  NULL)) {
                        frag_flags |= SANDBOX_FLAG();
                    }
                }

                if (TEST(FRAG_SELFMOD_SANDBOXED, frag_flags)) {
                    LOG(GLOBAL, LOG_VMAREAS, 2,
                        "\tNew executable region " PFX "-" PFX
                        " is writable, but selfmod, "
                        "so leaving as writable\n",
                        base_pc, base_pc + size);
                } else if (INTERNAL_OPTION(hw_cache_consistency)) {
                    /* Make entire region read-only
                     * If that's too big, i.e., it contains some data, the
                     * region size will be corrected when we get a write
                     * fault in the region
                     */
                    LOG(GLOBAL, LOG_VMAREAS, 2,
                        "\tNew executable region " PFX "-" PFX " is writable, "
                        "making it read-only\n",
                        base_pc, base_pc + size);
#if 0
                    /* this syslog causes services.exe to hang
                     * (ref case 666) once case 666 is fixed re-enable if
                     * desired FIXME */
                    SYSLOG_INTERNAL_WARNING_ONCE("new executable vm area is writable.");
#endif
                    vm_make_unwritable(base_pc, size);
                    vm_flags |= VM_MADE_READONLY;
                    STATS_INC(num_rw2r_code_regions);
                }
            }
            /* now add the new region to the global list */
            ASSERT(!TEST(FRAG_COARSE_GRAIN, frag_flags)); /* else no pre-exec query */
            add_executable_vm_area(base_pc, base_pc + size, vm_flags | VM_EXECUTED_FROM,
                                   frag_flags,
                                   true /*own lock*/
                                   _IF_DEBUG("unexpected vm area"));
            ok = lookup_addr(executable_areas, pc, &area);
            ASSERT(ok);
            DOLOG(2, LOG_VMAREAS, {
                /* new area could have been split into multiple */
                print_contig_vm_areas(executable_areas, base_pc, base_pc + size, GLOBAL,
                                      "new executable vm area: ");
            });
        }
        ASSERT(area != NULL);
        area_copy = *area;
        area = &area_copy;

        if (xfer && !allow_xfer_for_frag_flags(dcontext, pc, *flags, frag_flags)) {
            result = false;
            goto check_thread_return;
        }
    }
    if (local_area == NULL) {
        /* new area for this thread */
        ASSERT(TEST(VM_EXECUTED_FROM, area->vm_flags)); /* marked above */
#ifdef DGC_DIAGNOSTICS
        if (!TESTANY(VM_UNMOD_IMAGE | VM_WAS_FUTURE, area->vm_flags)) {
            LOG(GLOBAL, LOG_VMAREAS, 1,
                "DYNGEN in %d: non-unmod-image exec area " PFX "-" PFX " %s\n",
                d_r_get_thread_id(), area->start, area->end, area->comment);
        }
#endif
#ifdef PROGRAM_SHEPHERDING
        DOSTATS({
            if (!TEST(VM_UNMOD_IMAGE, area->vm_flags) &&
                TEST(VM_WAS_FUTURE, area->vm_flags)) {
                /* increment for other threads (1st thread will be inc-ed in
                 * check_origins_helper)
                 */
                if (is_on_stack(dcontext, area->start, area)) {
                    STATS_INC(num_exec_future_stack);
                } else {
                    STATS_INC(num_exec_future_heap);
                }
            }
        });
#    ifdef WINDOWS
        DOSTATS({
            if (!TEST(VM_UNMOD_IMAGE, area->vm_flags) &&
                !TEST(VM_WAS_FUTURE, area->vm_flags))
                STATS_INC(num_exec_after_load);
        });
#    endif
#endif

        add_vm_area(&data->areas, area->start, area->end, area->vm_flags,
                    area->frag_flags, NULL _IF_DEBUG(area->comment));
        /* get area for actual pc (new area could have been split up) */
        ok = lookup_addr(&data->areas, pc, &local_area);
        ASSERT(ok);
        DOLOG(2, LOG_VMAREAS,
              { print_vm_area(&data->areas, local_area, THREAD, new_area_prefix); });
        DOLOG(5, LOG_VMAREAS, { print_vm_areas(&data->areas, THREAD); });
        DOCHECK(CHKLVL_ASSERTS, {
            LOG(THREAD, LOG_VMAREAS, 1,
                "checking thread vmareas against executable_areas\n");
            exec_area_bounds_match(dcontext, data);
        });
    }

    ASSERT(local_area != NULL);
    data->last_area = local_area;

    /* for adding new bbs to frag lists */
    if (tag != NULL) {
        bool already = false;
        fragment_t *entry, *prev;
        /* see if this frag is already on this area's list.
         * prev entry may not be first on list due to area merging or due to
         * trace building that requires bb creation in middle.
         */
        /* vmlist has to point to front, so must walk every time
         * along the way check to see if existing entry points to this area
         */
        for (entry = (fragment_t *)*vmlist, prev = NULL; entry != NULL;
             prev = entry, entry = FRAG_ALSO(entry)) {
            if (FRAG_PC(entry) >= local_area->start && FRAG_PC(entry) < local_area->end) {
                already = true;
                break;
            }
        }
        if (!already) {
            /* always allocate global, will re-allocate later if not shared */
            prev = prepend_fraglist(
                MULTI_ALLOC_DC(dcontext, (data == shared_data) ? FRAG_SHARED : 0),
                local_area, pc, tag, prev);
            ASSERT(FRAG_PREV(prev) != NULL);
            if (*vmlist == NULL) {
                /* write back first */
                *vmlist = (void *)prev;
            }
        }
        DOLOG(6, LOG_VMAREAS,
              { print_fraglist(dcontext, local_area, "after check_thread_vm_area, "); });
        DOLOG(7, LOG_VMAREAS, { print_fraglists(dcontext); });
    }

    result = true;
    *flags |= frag_flags;
    if (stop != NULL) {
        *stop = area->end;
        ASSERT(*stop != NULL);
#ifdef LINUX
        if (!vmvector_empty(d_r_rseq_areas)) {
            /* XXX i#3798: While for core operation we do not need to end a block at
             * an rseq endpoint, we need clients to treat the endpoint as a barrier and
             * restore app state (which we do have DR_NOTE_REG_BARRIER for) and we
             * prefer to simplify the block as much as we can.
             * Similarly, we don't really need to not have a block span the start
             * of an rseq region.  But, we need to save app values at the start, which
             * is best done prior to drreg storing them elsewhere; plus, it makes it
             * easier to turn on full_decode for simpler mangling.
             */
            bool entered_rseq = false, exited_rseq = false;
            app_pc rseq_start, next_boundary = NULL;
            if (vmvector_lookup_data(d_r_rseq_areas, pc, &rseq_start, &next_boundary,
                                     NULL)) {
                if (rseq_start > tag)
                    entered_rseq = true;
                else if (tag == rseq_start)
                    *flags |= FRAG_STARTS_RSEQ_REGION;
            } else {
                app_pc prev_end;
                if (vmvector_lookup_prev_next(d_r_rseq_areas, pc, NULL, &prev_end,
                                              &next_boundary, NULL)) {
                    if (tag < prev_end) {
                        /* Avoiding instructions after the rseq endpoint simplifies
                         * drmemtrace and other clients when the native rseq execution
                         * aborts, and shrinks the block with the large native rseq
                         * mangling.
                         */
                        exited_rseq = true;
                    }
                    if (prev_end == pc)
                        next_boundary = prev_end;
                }
            }
            if (next_boundary != NULL && next_boundary < *stop) {
                /* Ensure we check again before we hit a boundary. */
                *stop = next_boundary;
            }
            if (xfer && (entered_rseq || exited_rseq || pc == next_boundary)) {
                LOG(THREAD, LOG_VMAREAS | LOG_INTERP, 3,
                    "Stopping bb at rseq boundary " PFX "\n", pc);
                if (exited_rseq)
                    *flags |= FRAG_HAS_RSEQ_ENDPOINT;
                result = false;
            }
        }
#endif
        LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 4,
            "check_thread_vm_area: check_stop = " PFX "\n", *stop);
    }

    /* we are building a real bb, assert consistency checks */
    /* XXX i#4257: These memqueries are surprisingly slow on Mac64 and AArch64.
     * Investigation is needed.  For now we avoid them in default debug runs.
     */
    DOCHECK(IF_MACOS64_ELSE(3, IF_AARCH64_ELSE(3, 1)), {
        uint prot2;
        ok = get_memory_info(pc, NULL, NULL, &prot2);
        ASSERT(!ok || !TEST(MEMPROT_WRITE, prot2) ||
               TEST(FRAG_SELFMOD_SANDBOXED, *flags) ||
               !INTERNAL_OPTION(hw_cache_consistency));
        ASSERT(is_readable_without_exception_try(pc, 1));
    });

check_thread_return:
    check_thread_vm_area_cleanup(dcontext, false /*not aborting*/, false /*leave bb*/,
                                 data, vmlist, own_execareas_writelock,
                                 caller_execareas_writelock);
    return result;
}

static void
remove_fraglist_entry(dcontext_t *dcontext, fragment_t *entry, vm_area_t *area);

/* page_pc must be aligned to the start of a page */
void
set_thread_decode_page_start(dcontext_t *dcontext, app_pc page_pc)
{
    thread_data_t *data;
    /* Regardless of the dcontext that's passed in, we want to track the
     * page_pc for the thread so get a real dcontext. */
#ifdef UNIX
    /* FIXME On Linux, fetching a context requires a syscall, which is a
     * relatively costly operation, so we don't even try. Note that this can
     * be misleading when the dcontext that's passed in isn't the one for
     * the executing thread (such as in case 5388 on Windows).
     */
    if (dcontext == GLOBAL_DCONTEXT) {
        ASSERT_CURIOSITY(dynamo_exited);
        return;
    }
#else
    dcontext = get_thread_private_dcontext();
    if (dcontext == NULL) {
        ASSERT_CURIOSITY(dynamo_exited);
        return;
    }
#endif
    data = (thread_data_t *)dcontext->vm_areas_field;
    ASSERT(page_pc == (app_pc)PAGE_START(page_pc));
    data->last_decode_area_page_pc = page_pc;
    data->last_decode_area_valid = true;
}

/* Check if address is in the last area that passed the check_thread_vm_area tests.
 * Used for testing for an application race condition (case 845),
 * where code executed by one thread is unmapped by another.
 * The last decoded application pc should always be in the thread's last area.
 */
bool
check_in_last_thread_vm_area(dcontext_t *dcontext, app_pc pc)
{
    thread_data_t *data = NULL;
    bool in_last = false;
    app_pc last_decode_area_page_pc;
    /* extra paranoia since called by intercept_exception */
    if (is_readable_without_exception((app_pc)&dcontext->vm_areas_field, 4))
        data = (thread_data_t *)dcontext->vm_areas_field;
    /* note that if data is NULL &data->last_area will not be readable either */
    if (is_readable_without_exception((app_pc)&data->last_area, 4) &&
        is_readable_without_exception((app_pc)&data->last_area->end, 4) &&
        is_readable_without_exception((app_pc)&data->last_area->start, 4)) {
        /* we can walk off to the next page */
        in_last = (pc < data->last_area->end + MAX_INSTR_LENGTH &&
                   data->last_area->start <= pc);
    }
    /* last decoded app pc may be in last shared area instead */
    if (!in_last && DYNAMO_OPTION(shared_bbs)) {
        /* We avoid the high-ranked shared_vm_areas lock which can easily cause
         * rank order violations (i#3346).  We're trying to catch the scenario
         * where a shared bb is being built and we fault decoding it.  There,
         * the bb building lock will prevent another thread from changing the
         * shared last_area, so we should hit when reading w/o the lock.
         * The risk of falsely matching with a half-updated or mismatched
         * racily read last_area bounds seems lower than the risk of problems
         * if we grab the lock.
         */
        if (is_readable_without_exception((app_pc)&shared_data->last_area->end, 4) &&
            is_readable_without_exception((app_pc)&shared_data->last_area->start, 4)) {
            /* we can walk off to the next page */
            in_last = (pc < shared_data->last_area->end + MAX_INSTR_LENGTH &&
                       shared_data->last_area->start <= pc);
        }
    }
    /* the last decoded app pc may be in the last decoded page or the page after
     * if the instr crosses a page boundary. This can help us more gracefully
     * handle a race during the origins pattern check between a thread unmapping
     * a region and another thread decoding in that region (xref case 7103).
     */
    if (!in_last && data != NULL &&
        d_r_safe_read(&data->last_decode_area_page_pc, sizeof(last_decode_area_page_pc),
                      &last_decode_area_page_pc) &&
        /* I think the above "safety" checks are ridiculous so not doing them here */
        data->last_decode_area_valid) {
        /* Check the last decoded pc's current page and the page after. */
        app_pc last_decode_page_end = last_decode_area_page_pc + 2 * PAGE_SIZE;
        in_last = ((POINTER_OVERFLOW_ON_ADD(last_decode_area_page_pc, 2 * PAGE_SIZE) ||
                    pc < last_decode_page_end) &&
                   last_decode_area_page_pc <= pc);
    }
    return in_last;
}

/* Removes vmlist entries added to the global vmarea list for f.
 * If new_vmlist != NULL, adds locally in addition to removing globally, and
 * removes the global area itself if empty.
 */
static void
remove_shared_vmlist(dcontext_t *dcontext, void *vmlist, fragment_t *f,
                     void **local_vmlist)
{
    vm_area_t *area = NULL;
    fragment_t *entry = (fragment_t *)vmlist;
    fragment_t *next;
    bool remove;
    uint check_flags = 0;
    app_pc pc;
    LOG(THREAD, LOG_VMAREAS, 4, "\tremoving shared vm data for F%d(" PFX ")\n", f->id,
        f->tag);
    SHARED_VECTOR_RWLOCK(&shared_data->areas, write, lock);
    while (entry != NULL) {
        ASSERT(FRAG_MULTI_INIT(entry));
        ASSERT(FRAG_FRAG(entry) == (fragment_t *)f->tag); /* for this frag */
        /* If area will become empty, remove it, since it was only added for
         * this bb that is not actually shared.
         * Case 8906: do NOT remove the area for coarse fragments, as they are
         * still shared!  We need the area, just not the fragment on the frags
         * list(s).
         */
        remove = (local_vmlist != NULL && FRAG_PREV(entry) == entry &&
                  !TEST(FRAG_COARSE_GRAIN, f->flags));
        if (remove) {
            DEBUG_DECLARE(bool ok =)
            lookup_addr(&shared_data->areas, FRAG_PC(entry), &area);
            ASSERT(ok && area != NULL);
            if (TEST(FRAG_COARSE_GRAIN, area->frag_flags)) {
                /* Case 9806: do NOT remove the coarse area even if this
                 * particular fragment is fine-grained.  We also test f->flags
                 * up front to avoid the lookup cost as an optimization.
                 */
                remove = false;
            } else {
                LOG(THREAD, LOG_VMAREAS, 4,
                    "sole fragment in added shared area, removing\n");
            }
        } else
            area = NULL;
        next = FRAG_ALSO(entry);
        pc = FRAG_PC(entry);
        remove_fraglist_entry(GLOBAL_DCONTEXT, entry, area /* ok to be NULL */);
        if (remove) {
            /* FIXME case 8629: lots of churn if frequent removals (e.g., coarse grain) */
            remove_vm_area(&shared_data->areas, area->start, area->end, false);
            shared_data->last_area = NULL;
        }
        if (local_vmlist != NULL) {
            /* add area to local and add local heap also entry */
            if (DYNAMO_OPTION(shared_bbs))
                check_flags = f->flags | FRAG_SHARED; /*indicator to NOT use global*/
            DEBUG_DECLARE(bool ok =)
            check_thread_vm_area(dcontext, pc, f->tag, local_vmlist, &check_flags, NULL,
                                 false /*xfer should not matter now*/);
            ASSERT(ok);
        }
        entry = next;
    }
    SHARED_VECTOR_RWLOCK(&shared_data->areas, write, unlock);
}

void
vm_area_add_fragment(dcontext_t *dcontext, fragment_t *f, void *vmlist)
{
    thread_data_t *data;
    vm_area_t *area = NULL;
    fragment_t *entry = (fragment_t *)vmlist;
    fragment_t *prev = NULL;

    LOG(THREAD, LOG_VMAREAS, 4, "vm_area_add_fragment for F%d(" PFX ")\n", f->id, f->tag);

    if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* We went ahead and built up vmlist since we might decide later to not
         * make a fragment coarse-grain.  If it is emitted as coarse-grain,
         * we need to clean up the vmlist as it is not needed.
         */
        remove_shared_vmlist(dcontext, vmlist, f, NULL /*do not add local*/);
        return;
    }

    if (TEST(FRAG_SHARED, f->flags)) {
        data = shared_data;
        /* need write lock since writing area->frags */
        SHARED_VECTOR_RWLOCK(&shared_data->areas, write, lock);
    } else if (!DYNAMO_OPTION(shared_bbs) ||
               /* should already be in private vmareas */
               TESTANY(FRAG_IS_TRACE | FRAG_TEMP_PRIVATE, f->flags))
        data = (thread_data_t *)dcontext->vm_areas_field;
    else {
        void *local_vmlist = NULL;
        /* turns out bb isn't shared, so we have to transfer also entries
         * to local heap and vector.  we do that by removing from global
         * and then calling check_thread_vm_area, telling it to add local.
         */
        ASSERT(dcontext != GLOBAL_DCONTEXT);
        /* only bbs do we build shared and then switch to private */
        ASSERT(!TEST(FRAG_IS_TRACE, f->flags));
        data = (thread_data_t *)dcontext->vm_areas_field;
        LOG(THREAD, LOG_VMAREAS, 4,
            "\tbb not shared, shifting vm data to thread-local\n");
        remove_shared_vmlist(dcontext, vmlist, f, &local_vmlist);
        /* now proceed as though everything were local to begin with */
        vmlist = local_vmlist;
        entry = (fragment_t *)vmlist;
    }

    /* swap f for the first multi_entry_t (the one in region of f->tag) */
    ASSERT(entry != NULL);
    FRAG_NEXT_ASSIGN(f, FRAG_NEXT(entry));
    FRAG_PREV_ASSIGN(f, FRAG_PREV(entry));
    FRAG_ALSO_ASSIGN(f, FRAG_ALSO(entry));
    prev = FRAG_PREV(f);
    ASSERT(prev != NULL); /* prev is never null */
    if (FRAG_NEXT(prev) == NULL) {
        DEBUG_DECLARE(bool ok =)
        /* need to know area */
        lookup_addr(&data->areas, FRAG_PC(entry), &area);
        ASSERT(ok);
        /* remember: prev wraps around, next does not */
        ASSERT(area->custom.frags == entry);
        area->custom.frags = f;
        /* if single entry will be circular */
        if (prev == entry)
            FRAG_PREV_ASSIGN(f, f);
    } else
        FRAG_NEXT_ASSIGN(prev, f);
    if (FRAG_NEXT(f) == NULL) {
        if (area == NULL) {
            DEBUG_DECLARE(bool ok =)
            /* need to know area for area->frags */
            lookup_addr(&data->areas, FRAG_PC(entry), &area);
            ASSERT(ok);
        }
        if (area->custom.frags == f) {
            ASSERT(FRAG_PREV(area->custom.frags) == f);
        } else {
            ASSERT(FRAG_PREV(area->custom.frags) == entry);
            FRAG_PREV_ASSIGN(area->custom.frags, f);
        }
    } else {
        prev = FRAG_NEXT(f);
        FRAG_PREV_ASSIGN(prev, f);
    }

    ASSERT(area_contains_frag_pc(area, entry));

    prev = FRAG_ALSO(entry);
    nonpersistent_heap_free(MULTI_ALLOC_DC(dcontext, entry->flags), entry,
                            sizeof(multi_entry_t) HEAPACCT(ACCT_VMAREA_MULTI));
    entry = prev;

    DOSTATS({
        if (entry != NULL)
            STATS_INC(num_bb_also_vmarea);
    });

    /* now put backpointers in */
    while (entry != NULL) {
        ASSERT(FRAG_MULTI_INIT(entry));
        ASSERT(FRAG_FRAG(entry) == (fragment_t *)f->tag); /* for this frag */
        DOLOG(4, LOG_VMAREAS, { print_entry(dcontext, entry, "\talso "); });
        FRAG_FRAG_ASSIGN(entry, f);
        /* remove the init flag now that the real fragment_t is in the f field
         * The vector lock protects this non-atomic flag change.
         */
        entry->flags &= ~FRAG_IS_EXTRA_VMAREA_INIT;
        entry = FRAG_ALSO(entry);
    }

    DOLOG(6, LOG_VMAREAS, { print_frag_arealist(dcontext, f); });
    DOLOG(7, LOG_VMAREAS, { print_fraglists(dcontext); });

    /* can't release lock once done w/ prev/next values since alsos can
     * be changed as well by vm_area_clean_fraglist()!
     */
    SHARED_VECTOR_RWLOCK(&data->areas, write, unlock);
}

void
acquire_vm_areas_lock(dcontext_t *dcontext, uint flags)
{
    thread_data_t *data = GET_DATA(dcontext, flags);
    SHARED_VECTOR_RWLOCK(&data->areas, write, lock);
}

bool
acquire_vm_areas_lock_if_not_already(dcontext_t *dcontext, uint flags)
{
    thread_data_t *data = GET_DATA(dcontext, flags);
    return writelock_if_not_already(&data->areas);
}

void
release_vm_areas_lock(dcontext_t *dcontext, uint flags)
{
    thread_data_t *data = GET_DATA(dcontext, flags);
    SHARED_VECTOR_RWLOCK(&data->areas, write, unlock);
}

#ifdef DEBUG
/* i#942: Check that each also_vmarea entry in a multi-area fragment is in its
 * own vmarea.  If a fragment is on a vmarea fragment list twice, we can end up
 * deleting that fragment twice while flushing.
 */
static bool
frag_also_list_areas_unique(dcontext_t *dcontext, thread_data_t *tgt_data, void **vmlist)
{
    fragment_t *entry;
    fragment_t *already;
    vm_area_t *entry_area;
    vm_area_t *already_area;
    bool ok;
    for (entry = (fragment_t *)*vmlist; entry != NULL; entry = FRAG_ALSO(entry)) {
        ASSERT(FRAG_MULTI(entry));
        ok = lookup_addr(&tgt_data->areas, FRAG_PC(entry), &entry_area);
        ASSERT(ok);
        /* Iterate the previous also entries and make sure they don't have the
         * same vmarea.
         * XXX: This is O(n^2) in the also list length, but these lists are
         * short and the O(n) impl would require a hashtable.
         */
        for (already = (fragment_t *)*vmlist; already != entry;
             already = FRAG_ALSO(already)) {
            ASSERT(FRAG_MULTI(already));
            ok = lookup_addr(&tgt_data->areas, FRAG_PC(already), &already_area);
            ASSERT(ok);
            if (entry_area == already_area)
                return false;
        }
    }
    return true;
}

/* i#942: Check that the per-thread list of executed areas doesn't cross any
 * executable_area boundaries.  If this happens, we start adding fragments to the
 * wrong vmarea fragment lists.  This check should be roughly O(n log n) in the
 * number of exec areas, so not too slow to run at the assertion check level.
 */
static void
exec_area_bounds_match(dcontext_t *dcontext, thread_data_t *data)
{
    vm_area_vector_t *v = &data->areas;
    int i;
    d_r_read_lock(&executable_areas->lock);
    for (i = 0; i < v->length; i++) {
        vm_area_t *thread_area = &v->buf[i];
        vm_area_t *exec_area;
        bool ok = lookup_addr(executable_areas, thread_area->start, &exec_area);
        ASSERT(ok);
        /* It's OK if thread areas are more fragmented than executable_areas.
         */
        if (!(thread_area->start >= exec_area->start &&
              thread_area->end <= exec_area->end)) {
            DOLOG(1, LOG_VMAREAS, {
                LOG(THREAD, LOG_VMAREAS, 1, "%s: bounds mismatch on %s vmvector\n",
                    __FUNCTION__, (TEST(VECTOR_SHARED, v->flags) ? "shared" : "private"));
                print_vm_area(v, thread_area, THREAD, "thread area: ");
                print_vm_area(v, exec_area, THREAD, "exec area: ");
                LOG(THREAD, 1, LOG_VMAREAS, "executable_areas:\n");
                print_vm_areas(executable_areas, THREAD);
                LOG(THREAD, 1, LOG_VMAREAS, "thread areas:\n");
                print_vm_areas(v, THREAD);
                ASSERT(false && "vmvector does not match exec area bounds");
            });
        }
    }
    d_r_read_unlock(&executable_areas->lock);
}
#endif /* DEBUG */

/* Creates a list of also entries for each vmarea touched by f and prepends it
 * to vmlist.
 *
 * Case 8419: this routine will fail and return false if f is marked as
 * FRAG_WAS_DELETED, since that means f's also entries have been deleted!
 * Caller can make an atomic no-fail region by holding f's vm area lock
 * and the change_linking_lock and passing true for have_locks.
 */
bool
vm_area_add_to_list(dcontext_t *dcontext, app_pc tag, void **vmlist, uint list_flags,
                    fragment_t *f, bool have_locks)
{
    thread_data_t *src_data = GET_DATA(dcontext, f->flags);
    thread_data_t *tgt_data = GET_DATA(dcontext, list_flags);
    vm_area_t *area = NULL;
    bool ok;
    fragment_t *prev = (fragment_t *)*vmlist;
    fragment_t *already;
    fragment_t *entry = f;
    bool success = true;
    bool lock;
    if (!have_locks)
        SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
    else {
        ASSERT((!TEST(VECTOR_SHARED, tgt_data->areas.flags) &&
                !TEST(VECTOR_SHARED, src_data->areas.flags)) ||
               self_owns_recursive_lock(&change_linking_lock));
    }
    /* support caller already owning write lock */
    lock = writelock_if_not_already(&src_data->areas);
    if (src_data != tgt_data) {
        /* we assume only one of the two is shared, or that they are both the same,
         * and we thus grab only one lock in this routine:
         * otherwise we need to do more work to avoid deadlocks here!
         */
        ASSERT(!TEST(VECTOR_SHARED, tgt_data->areas.flags) ||
               !TEST(VECTOR_SHARED, src_data->areas.flags));
        if (TEST(VECTOR_SHARED, tgt_data->areas.flags)) {
            ASSERT(!lock);
            lock = writelock_if_not_already(&tgt_data->areas);
        }
    }
    ASSERT((lock && !have_locks) || (!lock && have_locks) ||
           (!TEST(VECTOR_SHARED, tgt_data->areas.flags) &&
            !TEST(VECTOR_SHARED, src_data->areas.flags)));
    DOCHECK(CHKLVL_ASSERTS, {
        LOG(THREAD, 1, LOG_VMAREAS, "checking src_data\n");
        exec_area_bounds_match(dcontext, src_data);
        LOG(THREAD, 1, LOG_VMAREAS, "checking tgt_data\n");
        exec_area_bounds_match(dcontext, tgt_data);
    });
    /* If deleted, the also field is invalid and we cannot handle that! */
    if (TEST(FRAG_WAS_DELETED, f->flags)) {
        success = false;
        goto vm_area_add_to_list_done;
    }
    /* vmlist has to point to front, so must walk every time to find end */
    while (prev != NULL && FRAG_ALSO(prev) != NULL)
        prev = FRAG_ALSO(prev);
    /* walk f's areas */
    while (entry != NULL) {
        /* see if each of f's areas is already on trace's list */
        ok = lookup_addr(&src_data->areas, FRAG_PC(entry), &area);
        ASSERT(ok);
        ok = false; /* whether found existing entry in area or not */
        for (already = (fragment_t *)*vmlist; already != NULL;
             already = FRAG_ALSO(already)) {
            ASSERT(FRAG_MULTI(already));
            if (FRAG_PC(already) >= area->start && FRAG_PC(already) < area->end) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            /* found new area that trace is on */
            /* src may be shared bb, its area may not be on tgt list (e.g.,
             * private trace)
             */
            if (src_data != tgt_data) { /* else, have area already */
                vm_area_t *tgt_area = NULL;
                if (lookup_addr(&tgt_data->areas, FRAG_PC(entry), &tgt_area)) {
                    /* check target area for existing entry */
                    for (already = (fragment_t *)*vmlist; already != NULL;
                         already = FRAG_ALSO(already)) {
                        ASSERT(FRAG_MULTI(already));
                        if (FRAG_PC(already) >= tgt_area->start &&
                            FRAG_PC(already) < tgt_area->end) {
                            ok = true;
                            break;
                        }
                    }
                    if (ok)
                        break;
                } else {
                    add_vm_area(&tgt_data->areas, area->start, area->end, area->vm_flags,
                                area->frag_flags, NULL _IF_DEBUG(area->comment));
                    ok = lookup_addr(&tgt_data->areas, FRAG_PC(entry), &tgt_area);
                    ASSERT(ok);
                    /* modified vector, must clear last_area */
                    tgt_data->last_area = NULL;
                    DOLOG(2, LOG_VMAREAS, {
                        print_vm_area(&tgt_data->areas, tgt_area, THREAD,
                                      "new vm area for thread: ");
                    });
                    DOLOG(5, LOG_VMAREAS, { print_vm_areas(&tgt_data->areas, THREAD); });
                }
                area = tgt_area;
            }
            ASSERT(area != NULL);
            prev = prepend_fraglist(MULTI_ALLOC_DC(dcontext, list_flags), area,
                                    FRAG_PC(entry), tag, prev);
            if (*vmlist == NULL) {
                /* write back first */
                *vmlist = (void *)prev;
            }
        }
        entry = FRAG_ALSO(entry);
    }
    ASSERT_MESSAGE(CHKLVL_DEFAULT, "fragment also list has duplicate entries",
                   frag_also_list_areas_unique(dcontext, tgt_data, vmlist));
    DOLOG(6, LOG_VMAREAS, { print_frag_arealist(dcontext, (fragment_t *)*vmlist); });
    DOLOG(7, LOG_VMAREAS, { print_fraglists(dcontext); });
vm_area_add_to_list_done:
    if (lock) {
        if (src_data != tgt_data)
            SHARED_VECTOR_RWLOCK(&tgt_data->areas, write, unlock);
        SHARED_VECTOR_RWLOCK(&src_data->areas, write, unlock);
    }
    if (!have_locks)
        SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
    return success;
}

/* Frees storage for any multi-entries in the list (NOT for any fragment_t).
 * FIXME: this is now used on bb abort, where we may want to remove a vmarea
 * that was added only for a unreadable region (if decode fault will have been
 * added already)!  Yet we don't know whether any coarse fragments in area,
 * etc., so we go ahead and leave there: cached in last_area will lead to decode
 * fault rather than explicit detection in check_thread_vm_area but that's ok.
 * If we do want to remove should share code between this routine and
 * remove_shared_vmlist().
 */
void
vm_area_destroy_list(dcontext_t *dcontext, void *vmlist)
{
    if (vmlist != NULL)
        vm_area_remove_fragment(dcontext, (fragment_t *)vmlist);
}

bool
vm_list_overlaps(dcontext_t *dcontext, void *vmlist, app_pc start, app_pc end)
{
    vm_area_vector_t *v = GET_VECTOR(dcontext, ((fragment_t *)vmlist)->flags);
    fragment_t *entry;
    bool ok;
    vm_area_t *area;
    bool result = false;
    LOG(THREAD, LOG_VMAREAS, 4, "vm_list_overlaps " PFX " vs " PFX "-" PFX "\n", vmlist,
        start, end);
    /* don't assert if can't find anything -- see usage in handle_modified_code() */
    if (v == NULL)
        return false;
    SHARED_VECTOR_RWLOCK(v, read, lock);
    for (entry = vmlist; entry != NULL; entry = FRAG_ALSO(entry)) {
        ok = lookup_addr(v, FRAG_PC(entry), &area);
        if (!ok)
            break;
        if (start < area->end && end > area->start) {
            result = true;
            break;
        }
    }
    SHARED_VECTOR_RWLOCK(v, read, unlock);
    return result;
}

/* Removes an entry from the fraglist of area.
 * If area is NULL, looks it up based on dcontext->vm_areas_field->areas,
 * or the shared areas, depending on entry.
 * That lookup may need to be synchronized: this routine checks if the
 * caller holds the write lock before grabbing it.
 * If entry is a multi_entry_t, frees its heap
 * DOES NOT update the also chain!
 */
static void
remove_fraglist_entry(dcontext_t *dcontext, fragment_t *entry, vm_area_t *area)
{
    thread_data_t *data = GET_DATA(dcontext, entry->flags);
    fragment_t *prev;
    vm_area_vector_t *vector = &data->areas;
    /* need write lock since may modify area->frags */
    bool lock = writelock_if_not_already(vector);
    /* entry is only in shared vector if still live -- if not
     * we shouldn't get here
     */
    ASSERT(!TEST(VECTOR_SHARED, vector->flags) || !TEST(FRAG_WAS_DELETED, entry->flags));
    ASSERT(area_contains_frag_pc(area, entry));

    prev = FRAG_PREV(entry);
    if (FRAG_NEXT(prev) == NULL || FRAG_NEXT(entry) == NULL) {
        /* need to know area */
        DEBUG_DECLARE(bool ok =)
        lookup_addr(vector, FRAG_PC(entry), &area);
        ASSERT(ok);
        ASSERT(area != NULL);
    }

    /* remember: prev wraps around, next does not */
    if (FRAG_NEXT(prev) == NULL) {
        ASSERT(area->custom.frags == entry);
        area->custom.frags = FRAG_NEXT(entry);
    } else {
        FRAG_NEXT_ASSIGN(prev, FRAG_NEXT(entry));
    }
    if (FRAG_NEXT(entry) == NULL) {
        if (area->custom.frags != NULL) {
            ASSERT(FRAG_PREV(area->custom.frags) == entry);
            FRAG_PREV_ASSIGN(area->custom.frags, FRAG_PREV(entry));
        }
    } else {
        fragment_t *next = FRAG_NEXT(entry);
        FRAG_PREV_ASSIGN(next, FRAG_PREV(entry));
    }
    /* next MUST be NULL-ed for fragment_remove_shared_no_flush() */
    FRAG_NEXT_ASSIGN(entry, NULL);
    DODEBUG({
        FRAG_PREV_ASSIGN(entry, NULL);
        FRAG_ALSO_ASSIGN(entry, NULL);
    });
    if (FRAG_MULTI(entry)) {
        nonpersistent_heap_free(MULTI_ALLOC_DC(dcontext, entry->flags), entry,
                                sizeof(multi_entry_t) HEAPACCT(ACCT_VMAREA_MULTI));
    }
    if (lock)
        SHARED_VECTOR_RWLOCK(vector, write, unlock);
}

#ifdef DEBUG
/* For every multi_entry_t fragment in the fraglist, make sure that neither the
 * real fragment nor any of the other also entries are in the same fraglist.
 * This should only ever happen after a merger, at which point we call
 * vm_area_clean_fraglist() to fix it.  Any other occurrence is a bug.
 */
static void
vm_area_check_clean_fraglist(vm_area_t *area)
{
    fragment_t *entry;
    for (entry = area->custom.frags; entry != NULL; entry = FRAG_NEXT(entry)) {
        /* All entries and fragments should be from this area. */
        ASSERT(area_contains_frag_pc(area, entry));
        if (FRAG_MULTI(entry)) {
            fragment_t *f = FRAG_FRAG(entry);
            /* Ideally we'd take FRAG_ALSO(f) to start also iteration, but that
             * pointer isn't valid during bb building.
             */
            fragment_t *also = FRAG_ALSO(entry);
            ASSERT(f != FRAG_NEXT(entry));
            /* Iterate the also list.  All elements should be outside the
             * current area, or they should be the multi_entry_t that we're
             * currently looking at.
             */
            while (also != NULL) {
                ASSERT(FRAG_MULTI(also));
                ASSERT(also == entry || !area_contains_frag_pc(area, also));
                also = FRAG_ALSO(also);
            }
            /* This is a multi area entry, so the real fragment shouldn't start
             * in this area and therefore shouldn't be on this list.
             */
            ASSERT(FRAG_MULTI_INIT(entry) ||
                   !(f->tag >= area->start && f->tag < area->end));
        }
    }
}
#endif /* DEBUG */

/* Removes redundant also entries in area's frags list
 * (viz., those also entries that are now in same area as frag)
 * Meant to be called after merging areas
 */
static void
vm_area_clean_fraglist(dcontext_t *dcontext, vm_area_t *area)
{
    fragment_t *entry, *next, *f;
    fragment_t *also, *also_prev, *also_next;
    LOG(THREAD, LOG_VMAREAS, 4, "vm_area_clean_fraglist for " PFX "-" PFX "\n",
        area->start, area->end);
    DOLOG(6, LOG_VMAREAS, { print_fraglist(dcontext, area, "before cleaning "); });
    /* FIXME: would like to assert we hold write lock but only have area ptr */
    for (entry = area->custom.frags; entry != NULL; entry = next) {
        next = FRAG_NEXT(entry); /* might delete entry */
        /* Strategy: look at each multi, see if its fragment_t is here or if the next
         * multi in also chain is here.
         * This cleaning doesn't happen very often so this shouldn't be perf critical.
         */
        if (FRAG_MULTI(entry)) {
            f = FRAG_FRAG(entry);
            ASSERT(f != next);
            /* Remove later also entries first */
            also = FRAG_ALSO(entry);
            also_prev = entry;
            while (also != NULL) {
                app_pc pc = FRAG_PC(also);
                also_next = FRAG_ALSO(also);
                if (pc >= area->start && pc < area->end) {
                    ASSERT(FRAG_FRAG(also) == f);
                    DOLOG(5, LOG_VMAREAS,
                          { print_entry(dcontext, also, "\tremoving "); });
                    /* we have to remove from also chain ourselves */
                    FRAG_ALSO_ASSIGN(also_prev, also_next);
                    /* now remove from area frags list */
                    remove_fraglist_entry(dcontext, also, area);
                } else
                    also_prev = also;
                also = also_next;
            }
            /* fragment_t itself is always in area of its tag */
            if (!FRAG_MULTI_INIT(entry) && f->tag >= area->start && f->tag < area->end) {
                /* Remove this multi entry */
                DOLOG(5, LOG_VMAREAS, { print_entry(dcontext, entry, "\tremoving "); });
                /* we have to remove from also chain ourselves */
                for (also_prev = f; FRAG_ALSO(also_prev) != entry;
                     also_prev = FRAG_ALSO(also_prev))
                    ;
                FRAG_ALSO_ASSIGN(also_prev, FRAG_ALSO(entry));
                /* now remove from area frags list */
                remove_fraglist_entry(dcontext, entry, area);
            }
        }
    }
    DOCHECK(CHKLVL_DEFAULT, { vm_area_check_clean_fraglist(area); });
    DOLOG(6, LOG_VMAREAS, { print_fraglist(dcontext, area, "after cleaning "); });
}

void
vm_area_remove_fragment(dcontext_t *dcontext, fragment_t *f)
{
    fragment_t *entry, *next;
    DEBUG_DECLARE(fragment_t * match;)
    /* must grab lock across whole thing since alsos can be changed
     * by vm_area_clean_fraglist()
     */
    vm_area_vector_t *vector = &(GET_DATA(dcontext, f->flags))->areas;
    bool multi = FRAG_MULTI(f);
    bool lock = writelock_if_not_already(vector);

    if (!multi) {
        LOG(THREAD, LOG_VMAREAS, 4, "vm_area_remove_fragment: F%d tag=" PFX "\n", f->id,
            f->tag);
        DODEBUG(match = f;);
    } else {
        /* we do get called for multi-entries from vm_area_destroy_list */
        LOG(THREAD, LOG_VMAREAS, 4, "vm_area_remove_fragment: entry " PFX "\n", f);
        DODEBUG(match = FRAG_FRAG(f););
    }
    ASSERT(FRAG_PREV(f) != NULL); /* prev wraps around, should never be null */

    entry = f;
    while (entry != NULL) {
        DOLOG(5, LOG_VMAREAS, { print_entry(dcontext, entry, "\tremoving "); });
        /* from vm_area_destroy_list we can end up deleting a multi-init */
        ASSERT(FRAG_FRAG(entry) == match);
        next = FRAG_ALSO(entry);
        remove_fraglist_entry(dcontext, entry, NULL);
        entry = next;
    }
    if (!multi) /* else f may have been freed */
        FRAG_ALSO_ASSIGN(f, NULL);

    DOLOG(7, LOG_VMAREAS, { print_fraglists(dcontext); });

    /* f may no longer exist if it is FRAG_MULTI */
    if (lock)
        SHARED_VECTOR_RWLOCK(vector, write, unlock);
}

/* adds the fragment list chained by next_vmarea starting at f to a new
 * pending deletion entry
 */
static void
add_to_pending_list(dcontext_t *dcontext, fragment_t *f, uint refcount,
                    uint flushtime _IF_DEBUG(app_pc start) _IF_DEBUG(app_pc end))
{
    pending_delete_t *pend;
    ASSERT_OWN_MUTEX(true, &shared_delete_lock);
    pend = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, pending_delete_t, ACCT_VMAREAS, PROTECTED);
    DODEBUG({
        pend->start = start;
        pend->end = end;
    });
    pend->frags = f;
    if (DYNAMO_OPTION(shared_deletion)) {
        /* Set up ref count and timestamp for delayed deletion */
        pend->ref_count = refcount;
        pend->flushtime_deleted = flushtime;
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "deleted area ref count=%d timestamp=%u start=" PFX " end=" PFX "\n",
            pend->ref_count, pend->flushtime_deleted, start, end);
    }
    /* add to front of list */
    pend->next = todelete->shared_delete;
    todelete->shared_delete = pend;
    todelete->shared_delete_count++;
    if (pend->next == NULL) {
        ASSERT(todelete->shared_delete_tail == NULL);
        todelete->shared_delete_tail = pend;
    }

    if (DYNAMO_OPTION(reset_every_nth_pending) > 0 &&
        DYNAMO_OPTION(reset_every_nth_pending) == todelete->shared_delete_count) {
        /* if too many pending entries are piling up, suspend all threads
         * in order to free them immediately.
         * we can get here multiple times before we actually do the reset
         * (can dec and then re-inc shared_delete_count),
         * but that's not a problem, except we have to move our stats inc
         * into the reset routine itself.
         */
        schedule_reset(RESET_PENDING_DELETION/*NYI: currently this is ignored and we
                                              * do a full reset*/);
    }

    STATS_INC(num_shared_flush_regions);
    LOG(GLOBAL, LOG_VMAREAS, 3, "Pending list after adding deleted vm area:\n");
    DOLOG(3, LOG_VMAREAS, { print_pending_list(GLOBAL); });
}

#if defined(DEBUG) && defined(INTERNAL)
static void
print_lazy_deletion_list(dcontext_t *dcontext, const char *msg)
{
    uint i = 0;
    fragment_t *f;
    ASSERT_OWN_MUTEX(true, &lazy_delete_lock);
    LOG(THREAD, LOG_VMAREAS, 1, "%s", msg);
    for (f = todelete->lazy_delete_list; f != NULL; f = f->next_vmarea) {
        LOG(THREAD, LOG_VMAREAS, 1, "\t%d: F%d (" PFX ")\n", i, f->id, f->tag);
        i++;
    }
}
#endif

#ifdef DEBUG
static void
check_lazy_deletion_list_consistency()
{
    uint i = 0;
    fragment_t *f;
    ASSERT_OWN_MUTEX(true, &lazy_delete_lock);
    for (f = todelete->lazy_delete_list; f != NULL; f = f->next_vmarea) {
        i++;
    }
    ASSERT(i == todelete->lazy_delete_count);
}
#endif

bool
remove_from_lazy_deletion_list(dcontext_t *dcontext, fragment_t *remove)
{
    fragment_t *f, *prev_f = NULL;
    d_r_mutex_lock(&lazy_delete_lock);
    /* FIXME: start using prev_vmarea?!? (case 7165) */
    for (f = todelete->lazy_delete_list; f != NULL; prev_f = f, f = f->next_vmarea) {
        if (f == remove) {
            if (prev_f == NULL)
                todelete->lazy_delete_list = f->next_vmarea;
            else
                prev_f->next_vmarea = f->next_vmarea;
            if (f == todelete->lazy_delete_tail)
                todelete->lazy_delete_tail = prev_f;
            todelete->lazy_delete_count--;
            d_r_mutex_unlock(&lazy_delete_lock);
            return true;
        }
    }
    d_r_mutex_unlock(&lazy_delete_lock);
    return false;
}

/* Moves all lazy list entries into a real pending deletion entry.
 * Can only be called when !couldbelinking.
 */
static void
move_lazy_list_to_pending_delete(dcontext_t *dcontext)
{
    ASSERT_OWN_NO_LOCKS();
    ASSERT(is_self_couldbelinking());
    /* to properly set up ref count we MUST get a flushtime synched with a
     * thread count (otherwise we may have too many threads decrementing
     * the ref count, or vice versa, causing either premature or
     * never-occurring freeing), so we must grab thread_initexit_lock,
     * meaning we must be nolinking, meaning the caller must accept loss
     * of locals.
     * FIXME: should switch to a flag-triggered addition in d_r_dispatch
     * to avoid this nolinking trouble.
     */
    enter_nolinking(dcontext, NULL, false /*not a cache transition*/);
    d_r_mutex_lock(&thread_initexit_lock);
    /* to ensure no deletion queue checks happen in the middle of our update */
    d_r_mutex_lock(&shared_cache_flush_lock);
    d_r_mutex_lock(&shared_delete_lock);
    d_r_mutex_lock(&lazy_delete_lock);
    if (todelete->move_pending) {
        /* it's possible for remove_from_lazy_deletion_list to drop the count */
#ifdef X86
        DODEBUG({
            fragment_t *f; /* Raise SIGILL if a deleted fragment gets executed again */
            for (f = todelete->lazy_delete_list; f != NULL; f = f->next_vmarea) {
                *(ushort *)vmcode_get_writable_addr(f->start_pc) = RAW_OPCODE_SIGILL;
            }
        });
#endif
        DODEBUG({
            if (todelete->lazy_delete_count <= DYNAMO_OPTION(lazy_deletion_max_pending)) {
                SYSLOG_INTERNAL_WARNING_ONCE("lazy_delete_count dropped below "
                                             "threshold before move to pending");
            }
        });
        LOG(THREAD, LOG_VMAREAS, 3, "moving lazy list to a pending deletion entry\n");
        STATS_INC(num_lazy_del_to_pending);
        STATS_ADD(num_lazy_del_frags_to_pending, todelete->lazy_delete_count);
        /* ensure all threads in ref count will actually check the queue */
        increment_global_flushtime();
        add_to_pending_list(dcontext, todelete->lazy_delete_list,
                            /* we do count this thread, as we aren't checking the
                             * pending list here or inc-ing our flushtime
                             */
                            d_r_get_num_threads(),
                            flushtime_global _IF_DEBUG(NULL) _IF_DEBUG(NULL));
        todelete->lazy_delete_list = NULL;
        todelete->lazy_delete_tail = NULL;
        todelete->lazy_delete_count = 0;
        todelete->move_pending = false;
    } else /* should not happen */
        ASSERT(false && "race in move_lazy_list_to_pending_delete");
    DODEBUG({ check_lazy_deletion_list_consistency(); });
    d_r_mutex_unlock(&lazy_delete_lock);
    d_r_mutex_unlock(&shared_delete_lock);
    d_r_mutex_unlock(&shared_cache_flush_lock);
    d_r_mutex_unlock(&thread_initexit_lock);
    enter_couldbelinking(dcontext, NULL, false /*not a cache transition*/);
}

/* adds the list of fragments beginning with f and chained by {next,prev}_vmarea
 * to a new pending-lazy-deletion entry.
 * This routine may become nolinking, meaning that fragments may be freed
 * before this routine returns, so the caller should invalidate all pointers.
 * It also means that no locks may be held by the caller!
 */
void
add_to_lazy_deletion_list(dcontext_t *dcontext, fragment_t *f)
{
    /* rather than allocate memory for a pending operation to save memory,
     * we re-use f->incoming_stubs's slot (via a union), which is no longer needed
     * (caller should have already called incoming_remove_fragment()), to store our
     * timestamp, and next_vmarea to chain.
     */
    fragment_t *tail, *prev = NULL;
    uint flushtime;
    bool perform_move = false;
    ASSERT_OWN_NO_LOCKS();
    ASSERT(is_self_couldbelinking());
    d_r_mutex_lock(&shared_cache_flush_lock); /* for consistent flushtime */
    d_r_mutex_lock(&lazy_delete_lock);
    /* We need a flushtime as we are compared to shared deletion pending
     * entries, but we don't need to inc flushtime_global.  We need a value
     * larger than any thread has already signed off on, and thus larger than
     * the current flushtime_global.  We hold shared_cache_flush_lock to ensure
     * our flushtime retains that property until the lazy list is updated.
     *
     * (Optimization to allow lazy adds to proceed concurrently with deletion
     * list checks: don't grab the shared_cache_flush_lock.  Since we're
     * couldbelinking, the flusher won't inc flushtime until we're done here,
     * and the lazy lock prevents other lazy adders from incing flushtime global
     * for a shift to pending deletion list (in code below).  Then non-flusher
     * must hold lazy lock in general to inc flushtime.).
     */
    ASSERT(flushtime_global < UINT_MAX);
    /* currently we reset if flushtime hits a threshold -- in which case we
     * may never reach this flushtime, but the reset if we hit threshold again,
     * moving lazy entries to pending delete (below), and -reset_every_nth_pending
     * combined should ensure we delete these fragments
     */
    flushtime = flushtime_global + 1;
    /* we support adding a string of fragments at once
     * FIXME: if a string is common, move to a data structure w/ a
     * single timestamp for a group of fragments -- though lazy_deletion_max_pending
     * sort of does that for us.
     */
    /* must append to keep the list reverse-sorted by flushtime */
    if (todelete->lazy_delete_list == NULL) {
        ASSERT(todelete->lazy_delete_tail == NULL);
        todelete->lazy_delete_list = f;
    } else {
        ASSERT(todelete->lazy_delete_tail->next_vmarea == NULL);
        todelete->lazy_delete_tail->next_vmarea = f;
    }
    for (tail = f; tail != NULL; prev = tail, tail = tail->next_vmarea) {
        ASSERT(tail->also.also_vmarea == NULL);
        ASSERT(TEST(FRAG_SHARED, tail->flags));
        tail->also.flushtime = flushtime;
        todelete->lazy_delete_count++;
    }
    todelete->lazy_delete_tail = prev;
    ASSERT(todelete->lazy_delete_tail != NULL);
    LOG(THREAD, LOG_VMAREAS, 3, "adding F%d to lazy deletion list @ timestamp %u\n",
        f->id, flushtime);
    STATS_INC(num_lazy_deletion_appends);
    DOLOG(5, LOG_VMAREAS, {
        print_lazy_deletion_list(dcontext,
                                 "Lazy deletion list after adding deleted fragment:\n");
    });
    DODEBUG({ check_lazy_deletion_list_consistency(); });
    /* case 9115: ensure only one thread calls move_lazy_list_to_pending_delete,
     * to reduce thread_initexit_lock contention and subsequent synch_with_all_threads
     * performance issues
     */
    if (!todelete->move_pending &&
        todelete->lazy_delete_count > DYNAMO_OPTION(lazy_deletion_max_pending)) {
        perform_move = true;
        todelete->move_pending = true;
    }
    d_r_mutex_unlock(&lazy_delete_lock);
    d_r_mutex_unlock(&shared_cache_flush_lock);
    if (perform_move) {
        /* hit threshold -- move to real pending deletion entry */
        /* had to release lazy_delete_lock and re-grab for proper rank order */
        move_lazy_list_to_pending_delete(dcontext);
    }
}

/* frees all fragments on the lazy list with flushtimes less than flushtime
 */
static void
check_lazy_deletion_list(dcontext_t *dcontext, uint flushtime)
{
    fragment_t *f, *next_f;
    d_r_mutex_lock(&lazy_delete_lock);
    LOG(THREAD, LOG_VMAREAS, 3, "checking lazy list @ timestamp %u\n", flushtime);
    for (f = todelete->lazy_delete_list; f != NULL; f = next_f) {
        next_f = f->next_vmarea; /* may be freed so cache now */
        LOG(THREAD, LOG_VMAREAS, 4, "\tf->id %u vs %u\n", f->id, f->also.flushtime,
            flushtime);
        if (f->also.flushtime <= flushtime) {
            /* it is safe to free! */
            LOG(THREAD, LOG_VMAREAS, 3,
                "freeing F%d on lazy deletion list @ timestamp %u\n", f->id, flushtime);
            DOSTATS({
                if (dcontext == GLOBAL_DCONTEXT) /* at exit */
                    STATS_INC(num_lazy_deletion_frees_atexit);
                else
                    STATS_INC(num_lazy_deletion_frees);
            });
            /* FIXME: separate stats for frees at exit time */
            ASSERT(TEST(FRAG_SHARED, f->flags));
            /* we assume we're freeing the entire head of the list */
            todelete->lazy_delete_count--;
            todelete->lazy_delete_list = next_f;
            if (f == todelete->lazy_delete_tail) {
                ASSERT(todelete->lazy_delete_list == NULL);
                todelete->lazy_delete_tail = NULL;
            }
#ifdef X86
            DODEBUG({ /* Raise SIGILL if a deleted fragment gets executed again */
                      *(ushort *)vmcode_get_writable_addr(f->start_pc) =
                          RAW_OPCODE_SIGILL;
            });
#endif
            fragment_delete(dcontext, f,
                            FRAGDEL_NO_OUTPUT | FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE |
                                FRAGDEL_NO_VMAREA);
        } else {
            /* the lazy list is appended to and thus reverse-sorted, so
             * we can stop now as the oldest items are at the front
             */
            break;
        }
    }
    DOLOG(5, LOG_VMAREAS, {
        print_lazy_deletion_list(dcontext,
                                 "Lazy deletion list after freeing fragments:\n");
    });
    DODEBUG({ check_lazy_deletion_list_consistency(); });
    d_r_mutex_unlock(&lazy_delete_lock);
}

/* Prepares a list of shared fragments for deletion..
 * Caller should have already called vm_area_remove_fragment() on
 * each and chained them together via next_vmarea.
 * Caller must hold the shared_cache_flush_lock.
 * Returns the number of fragments unlinked
 */
int
unlink_fragments_for_deletion(dcontext_t *dcontext, fragment_t *list,
                              int pending_delete_threads)
{
    fragment_t *f, *next;
    uint num = 0;
    /* only applies to lists of shared fragments -- we check the head now */
    ASSERT(TEST(FRAG_SHARED, list->flags));
    /* for shared_deletion we have to protect this whole walk w/ a lock so
     * that the flushtime_global value remains higher than any thread's
     * flushtime.
     */
    ASSERT_OWN_MUTEX(DYNAMO_OPTION(shared_deletion), &shared_cache_flush_lock);

    acquire_recursive_lock(&change_linking_lock);
    for (f = list; f != NULL; f = next) {
        ASSERT(!FRAG_MULTI(f));
        next = f->next_vmarea;
        if (SHARED_IB_TARGETS()) {
            /* Invalidate shared targets from all threads' ibl tables
             * (if private) or from shared ibl tables.  Right now this
             * routine is only called mid-flush so it's safe to do
             * this here.
             */
            flush_invalidate_ibl_shared_target(dcontext, f);
        }
        fragment_unlink_for_deletion(dcontext, f);
        num++;
    }
    release_recursive_lock(&change_linking_lock);

    d_r_mutex_lock(&shared_delete_lock);
    /* add area's fragments as a new entry in the pending deletion list */
    add_to_pending_list(dcontext, list, pending_delete_threads,
                        flushtime_global _IF_DEBUG(NULL) _IF_DEBUG(NULL));
    d_r_mutex_unlock(&shared_delete_lock);
    STATS_ADD(list_entries_unlinked_for_deletion, num);
    return num;
}

/* returns the number of fragments unlinked */
int
vm_area_unlink_fragments(dcontext_t *dcontext, app_pc start, app_pc end,
                         int pending_delete_threads _IF_DGCDIAG(app_pc written_pc))
{
    /* dcontext is for another thread, so don't use THREAD to log.  Cache the
     * logfile instead of repeatedly calling THREAD_GET.
     */
    LOG_DECLARE(file_t thread_log = get_thread_private_logfile();)
    thread_data_t *data = GET_DATA(dcontext, 0);
    fragment_t *entry, *next;
    int num = 0, i;
    if (data == shared_data) {
        /* we also need to add to the deletion list */
        d_r_mutex_lock(&shared_delete_lock);

        acquire_recursive_lock(&change_linking_lock);

        /* we do not need the bb building lock, only the vm lock and
         * the fragment hashtable write lock, which is grabbed by fragment_remove
         */
        SHARED_VECTOR_RWLOCK(&data->areas, write, lock);

        /* clear shared last_area now, don't want a new bb in flushed area
         * thought to be ok b/c of a last_area hit
         */
        shared_data->last_area = NULL;

        /* for shared_deletion we have to protect this whole walk w/ a lock so
         * that the flushtime_global value remains higher than any thread's
         * flushtime.
         */
        ASSERT_OWN_MUTEX(DYNAMO_OPTION(shared_deletion), &shared_cache_flush_lock);
    }

    LOG(thread_log, LOG_FRAGMENT | LOG_VMAREAS, 2,
        "vm_area_unlink_fragments " PFX ".." PFX "\n", start, end);

    /* walk backwards to avoid O(n^2)
     * FIXME case 9819: could use executable_area_overlap_bounds() to avoid linear walk
     */
    for (i = data->areas.length - 1; i >= 0; i--) {
        /* look for overlap */
        if (start < data->areas.buf[i].end && end > data->areas.buf[i].start) {
            LOG(thread_log, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\tmarking region " PFX ".." PFX
                " for deletion & unlinking all its frags\n",
                data->areas.buf[i].start, data->areas.buf[i].end);
            data->areas.buf[i].vm_flags |= VM_DELETE_ME;
            if (data->areas.buf[i].start < start || data->areas.buf[i].end > end) {
                /* FIXME: best to only delete within asked-for flush area
                 * however, checking every fragment's bounds is way too expensive
                 * (surprisingly).  we've gone through several different schemes,
                 * including keeping a min_page and max_page in fragment_t, or
                 * various multi-page flags, to make checking every fragment faster,
                 * but keeping vm area lists is the most efficient.
                 * HOWEVER, deleting outside the flush bounds can cause problems
                 * if the caller holds fragment_t pointers and expects them not
                 * to be flushed (e.g., a faulting write on a read-only code region).
                 */
                LOG(thread_log, LOG_FRAGMENT | LOG_VMAREAS, 2,
                    "\tWARNING: region " PFX ".." PFX " is larger than "
                    "flush area " PFX ".." PFX "\n",
                    data->areas.buf[i].start, data->areas.buf[i].end, start, end);
            }
            /* i#942: We can't flush a fragment list with multiple also entries
             * from the same fragment on it, or our iteration gets derailed.
             */
            DOCHECK(CHKLVL_DEFAULT,
                    { vm_area_check_clean_fraglist(&data->areas.buf[i]); });
            ASSERT(!TEST(FRAG_COARSE_GRAIN, data->areas.buf[i].frag_flags));
            for (entry = data->areas.buf[i].custom.frags; entry != NULL; entry = next) {
                fragment_t *f = FRAG_FRAG(entry);
                next = FRAG_NEXT(entry);
                ASSERT(f != next && "i#942: changing f's fraglist derails iteration");
                /* case 9381: this shouldn't happen but we handle it to avoid crash */
                if (FRAG_MULTI_INIT(entry)) {
                    ASSERT(false && "stale multi-init entry on frags list");
                    /* stale init entry, just remove it */
                    vm_area_remove_fragment(dcontext, entry);
                    continue;
                }
                /* case 9118: call fragment_unlink_for_deletion() even if fragment
                 * is already unlinked
                 */
                if (!TEST(FRAG_WAS_DELETED, f->flags) || data == shared_data) {
                    LOG(thread_log, LOG_FRAGMENT | LOG_VMAREAS, 5,
                        "\tunlinking " PFX "%s F%d(" PFX ")\n", entry,
                        FRAG_MULTI(entry) ? " multi" : "", FRAG_ID(entry),
                        FRAG_PC(entry));
                    /* need to remove also entries from other vm lists
                     * thread-private doesn't have to do this b/c only unlinking,
                     * so ok if encounter an also in same flush, except we
                     * now do incoming_remove_fragment() for thread-private for
                     * use of fragment_t.incoming_stubs as a union.  so we
                     * do this for all fragments.
                     */
                    if (FRAG_ALSO(entry) != NULL || FRAG_MULTI(entry)) {
                        if (FRAG_MULTI(entry)) {
                            vm_area_remove_fragment(dcontext, f);
                            /* move to this area's frags list so will get
                             * transferred to deletion list if shared, or
                             * freed from this marked-vmarea if private
                             */
                            prepend_entry_to_fraglist(&data->areas.buf[i], f);
                        } else {
                            /* entry is the fragment, remove all its alsos */
                            vm_area_remove_fragment(dcontext, FRAG_ALSO(entry));
                        }
                        FRAG_ALSO_ASSIGN(f, NULL);
                    }
                    if (data == shared_data && SHARED_IB_TARGETS()) {
                        /* Invalidate shared targets from all threads' ibl
                         * tables (if private) or from shared ibl tables
                         */
                        flush_invalidate_ibl_shared_target(dcontext, f);
                    }
                    fragment_unlink_for_deletion(dcontext, f);
#ifdef DGC_DIAGNOSTICS
                    /* try to find out exactly which fragment contained written_pc */
                    if (written_pc != NULL) {
                        app_pc bb;
                        DOLOG(2, LOG_VMAREAS, {
                            LOG(thread_log, LOG_VMAREAS, 1, "Flushing F%d " PFX ":\n",
                                FRAG_ID(entry), FRAG_PC(entry));
                            disassemble_fragment(dcontext, entry, false);
                            LOG(thread_log, LOG_VMAREAS, 1, "First app bb for frag:\n");
                            disassemble_app_bb(dcontext, FRAG_PC(entry), thread_log);
                        });
                        if (fragment_overlaps(dcontext, entry, written_pc, written_pc + 1,
                                              false, NULL, &bb)) {
                            LOG(thread_log, LOG_VMAREAS, 1,
                                "Write target is actually inside app bb @" PFX ":\n",
                                written_pc);
                            disassemble_app_bb(dcontext, bb, thread_log);
                        }
                    }
#endif
                    num++;
                } else {
                    LOG(thread_log, LOG_FRAGMENT | LOG_VMAREAS, 5,
                        "\tnot unlinking " PFX "%s F%d(" PFX ") (already unlinked)\n",
                        entry, FRAG_MULTI(entry) ? " multi" : "", FRAG_ID(entry),
                        FRAG_PC(entry));
                }
                /* let recreate_fragment_ilist() know that this fragment
                 * is pending deletion and might no longer match the app's
                 * state. note that if we called fragment_unlink_for_deletion()
                 * then we already set this flag above. */
                f->flags |= FRAG_WAS_DELETED;
            }
            DOLOG(6, LOG_VMAREAS, {
                print_fraglist(dcontext, &data->areas.buf[i],
                               "Fragments after unlinking\n");
            });
            if (data == shared_data) {
                if (data->areas.buf[i].custom.frags != NULL) {
                    /* add area's fragments as a new entry in the pending deletion list */
                    add_to_pending_list(
                        dcontext, data->areas.buf[i].custom.frags, pending_delete_threads,
                        flushtime_global _IF_DEBUG(data->areas.buf[i].start)
                            _IF_DEBUG(data->areas.buf[i].end));
                    /* frags are moved over completely */
                    data->areas.buf[i].custom.frags = NULL;
                    STATS_INC(num_shared_flush_regions);
                }

                /* ASSUMPTION: remove_vm_area, given exact bounds, simply shifts later
                 * areas down in vector!
                 */
                LOG(thread_log, LOG_VMAREAS, 3, "Before removing vm area:\n");
                DOLOG(3, LOG_VMAREAS, { print_vm_areas(&data->areas, thread_log); });
                LOG(thread_log, LOG_VMAREAS, 2,
                    "Removing shared vm area " PFX "-" PFX "\n", data->areas.buf[i].start,
                    data->areas.buf[i].end);
                remove_vm_area(&data->areas, data->areas.buf[i].start,
                               data->areas.buf[i].end, false);
                LOG(thread_log, LOG_VMAREAS, 3, "After removing vm area:\n");
                DOLOG(3, LOG_VMAREAS, { print_vm_areas(&data->areas, thread_log); });
            }
        }
    }

    if (data == shared_data) {
        SHARED_VECTOR_RWLOCK(&data->areas, write, unlock);
        release_recursive_lock(&change_linking_lock);
        d_r_mutex_unlock(&shared_delete_lock);
    }

    LOG(thread_log, LOG_FRAGMENT | LOG_VMAREAS, 2, "  Unlinked %d frags\n", num);
    return num;
}

/* removes incoming links for all private fragments in the dcontext
 * thread that contain 'pc'
 */
void
vm_area_unlink_incoming(dcontext_t *dcontext, app_pc pc)
{
    int i;
    thread_data_t *data;

    ASSERT(dcontext != GLOBAL_DCONTEXT);
    data = GET_DATA(dcontext, 0);

    for (i = data->areas.length - 1; i >= 0; i--) {
        if (pc >= data->areas.buf[i].start && pc < data->areas.buf[i].end) {

            fragment_t *entry;
            for (entry = data->areas.buf[i].custom.frags; entry != NULL;
                 entry = FRAG_NEXT(entry)) {
                fragment_t *f = FRAG_FRAG(entry);
                ASSERT(!TEST(FRAG_SHARED, f->flags));

                /* Note that we aren't unlinking or ibl-invalidating
                 * (i.e., making unreachable) any fragments in other
                 * threads containing pc.
                 */
                if ((f->flags & FRAG_LINKED_INCOMING) != 0)
                    unlink_fragment_incoming(dcontext, f);
                fragment_remove_from_ibt_tables(dcontext, f, false);
            }
        }
    }
}

/* Decrements ref counts for thread-shared pending-deletion fragments,
 * and deletes those whose count has reached 0.
 * If dcontext==GLOBAL_DCONTEXT, does NOT check the ref counts and assumes it's
 * safe to free EVERYTHING.
 * Returns false iff was_I_flushed has been flushed (not necessarily
 * fully freed yet though, but may be at any time after this call
 * returns, so caller should drop its ref to it).
 */
bool
vm_area_check_shared_pending(dcontext_t *dcontext, fragment_t *was_I_flushed)
{
    pending_delete_t *pend;
    pending_delete_t *pend_prev = NULL;
    pending_delete_t *pend_nxt;
    /* a local list used to arrange in reverse order of flushtime */
    pending_delete_t *tofree = NULL;
    fragment_t *entry, *next;
    int num = 0;
    DEBUG_DECLARE(int i = 0;)
    bool not_flushed = true;
    ASSERT(DYNAMO_OPTION(shared_deletion) || dynamo_exited);
    /* must pass in real dcontext, unless exiting or resetting */
    ASSERT(dcontext != GLOBAL_DCONTEXT || dynamo_exited || dynamo_resetting);

    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
        "thread " TIDFMT " (flushtime %d) walking pending deletion list "
        "(was_I_flushed==F%d)\n",
        d_r_get_thread_id(),
        dcontext == GLOBAL_DCONTEXT ? flushtime_global
                                    : get_flushtime_last_update(dcontext),
        (was_I_flushed == NULL) ? -1 : was_I_flushed->id);
    STATS_INC(num_shared_flush_walks);

    /* synch w/ anyone incrementing flushtime_global and using its
     * value when adding to the shared deletion list (currently flushers
     * and lazy list transfers).
     */
    d_r_mutex_lock(&shared_cache_flush_lock);

    /* check if was_I_flushed has been flushed, prior to dec ref count and
     * allowing anyone to be fully freed
     */
    if (was_I_flushed != NULL &&
        TESTALL(FRAG_SHARED | FRAG_WAS_DELETED, was_I_flushed->flags)) {
        not_flushed = false;
        if (was_I_flushed == dcontext->last_fragment)
            last_exit_deleted(dcontext);
    }
    /* we can hit check points before we re-enter the cache, so we cannot
     * rely on the enter_couldbelinking of exiting the cache for invalidating
     * last_fragment -- we must check here as well (case 7453) (and case 7666,
     * where a non-null was_I_flushed prevented this check from executing).
     */
    if (dcontext != GLOBAL_DCONTEXT && dcontext->last_fragment != NULL &&
        TESTALL(FRAG_SHARED | FRAG_WAS_DELETED, dcontext->last_fragment->flags)) {
        last_exit_deleted(dcontext);
    }

    d_r_mutex_lock(&shared_delete_lock);
    for (pend = todelete->shared_delete; pend != NULL; pend = pend_nxt) {
        bool delete_area = false;
        pend_nxt = pend->next;
        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
            "  Considering #%d: " PFX ".." PFX " flushtime %d\n", i, pend->start,
            pend->end, pend->flushtime_deleted);
        if (dcontext == GLOBAL_DCONTEXT) {
            /* indication that it's safe to free everything */
            delete_area = true;
            if (dynamo_exited)
                STATS_INC(num_shared_flush_atexit);
            else
                STATS_INC(num_shared_flush_atreset);
        } else if (get_flushtime_last_update(dcontext) < pend->flushtime_deleted) {
            ASSERT(pend->ref_count > 0);
            pend->ref_count--;
            STATS_INC(num_shared_flush_refdec);
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\tdec => ref_count is now %d, flushtime diff is %d\n", pend->ref_count,
                flushtime_global - pend->flushtime_deleted);
            delete_area = (pend->ref_count == 0);
            DODEBUG({
                if (INTERNAL_OPTION(detect_dangling_fcache) && delete_area) {
                    /* don't actually free fragments until exit so we can catch any
                     * lingering links or ibt entries
                     */
                    delete_area = false;
                    for (entry = pend->frags; entry != NULL; entry = FRAG_NEXT(entry)) {
                        /* we do have to notify caller of flushing held ptrs */
                        if (FRAG_FRAG(entry) == was_I_flushed)
                            ASSERT(!not_flushed); /* should have been caught up top */
                        /* catch any links or ibt entries allowing access to deleted
                         * fragments by filling w/ int3 instead of reusing the cache
                         * space.  this will show up as a pc translation assert,
                         * typically.
                         */
                        /* should only get fragment_t here */
                        ASSERT(!FRAG_MULTI(entry));
                        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 4,
                            "\tfilling F%d " PFX "-" PFX " with 0x%x\n", entry->id,
                            entry->start_pc, entry->start_pc + entry->size,
                            DEBUGGER_INTERRUPT_BYTE);
                        memset(entry->start_pc, DEBUGGER_INTERRUPT_BYTE, entry->size);
                    }
                }
            });
            DOSTATS({
                if (delete_area)
                    STATS_INC(num_shared_flush_refzero);
            });
        } else {
            /* optimization: since we always pre-pend, can skip all the rest, as
             * they are guaranteed to have been ok-ed by us already
             */
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\t(aborting now since rest have already been ok-ed)\n");
            break;
        }

        if (delete_area) {
            /* we want to delete in increasing order of flushtime so that
             * fcache unit flushing will not occur before all lazily deleted
             * fragments in a unit are freed
             */
            if (pend_prev == NULL)
                todelete->shared_delete = pend->next;
            else
                pend_prev->next = pend->next;
            if (pend == todelete->shared_delete_tail) {
                ASSERT(pend->next == NULL);
                todelete->shared_delete_tail = pend_prev;
            }
            pend->next = tofree;
            tofree = pend;
        } else
            pend_prev = pend;
        DODEBUG({ i++; });
    }

    for (pend = tofree; pend != NULL; pend = pend_nxt) {
        pend_nxt = pend->next;

        /* we now know that any objects unlinked at or before this entry's
         * timestamp are safe to be freed (although not all earlier objects have
         * yet been freed, so containers cannot necessarily be freed: case 8242).
         * free these before this entry's fragments as they are older
         * (fcache unit flushing relies on this order).
         */
        check_lazy_deletion_list(dcontext, pend->flushtime_deleted);

        STATS_TRACK_MAX(num_shared_flush_maxdiff,
                        flushtime_global - pend->flushtime_deleted);
        DOSTATS({
            /* metric: # times flushtime diff is > #threads */
            if (flushtime_global - pend->flushtime_deleted > (uint)d_r_get_num_threads())
                STATS_INC(num_shared_flush_diffthreads);
        });
        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
            "\tdeleting all fragments in region " PFX ".." PFX " flushtime %u\n",
            pend->start, pend->end, pend->flushtime_deleted);
        ASSERT(pend->frags != NULL);
        for (entry = pend->frags; entry != NULL; entry = next) {
            next = FRAG_NEXT(entry);
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 5,
                "\tremoving " PFX "%s F%d(" PFX ")\n", entry,
                FRAG_MULTI(entry) ? " multi" : "", FRAG_ID(entry), FRAG_PC(entry));
            if (FRAG_FRAG(entry) == was_I_flushed)
                ASSERT(!not_flushed); /* should have been caught up top */
            /* vm_area_unlink_fragments should have removed all multis/alsos */
            ASSERT(!FRAG_MULTI(entry));
            /* FRAG_ALSO is used by lazy list so it may not be NULL */
            ASSERT(TEST(FRAG_WAS_DELETED, FRAG_FRAG(entry)->flags));
            /* do NOT call vm_area_remove_fragment, as it will freak out trying
             * to look up the area this fragment is in
             */
            fragment_delete(dcontext, FRAG_FRAG(entry),
                            FRAGDEL_NO_OUTPUT | FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE |
                                FRAGDEL_NO_VMAREA);
            STATS_INC(num_fragments_deleted_consistency);
            num++;
        }

        ASSERT(todelete->shared_delete_count > 0);
        todelete->shared_delete_count--;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, pend, pending_delete_t, ACCT_VMAREAS, PROTECTED);
    }

    if (tofree != NULL) { /* if we freed something (careful: tofree is dangling) */
        /* case 8242: due to -syscalls_synch_flush, a later entry can
         * reach refcount 0 before an earlier entry, so we cannot free
         * units until all earlier entries have been freed.
         */
        if (todelete->shared_delete_tail == NULL)
            fcache_free_pending_units(dcontext, flushtime_global);
        else {
            fcache_free_pending_units(
                dcontext, todelete->shared_delete_tail->flushtime_deleted - 1);
        }
    }

    if (dcontext == GLOBAL_DCONTEXT) { /* need to free everything */
        check_lazy_deletion_list(dcontext, flushtime_global + 1);
        fcache_free_pending_units(dcontext, flushtime_global + 1);
        /* reset_every_nth_pending relies on this */
        ASSERT(todelete->shared_delete_count == 0);
    }
    d_r_mutex_unlock(&shared_delete_lock);
    STATS_TRACK_MAX(num_shared_flush_maxpending, i);

    /* last_area cleared in vm_area_unlink_fragments */
    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
        "thread " TIDFMT " done walking pending list @flushtime %d\n",
        d_r_get_thread_id(), flushtime_global);
    if (dcontext != GLOBAL_DCONTEXT) {
        /* update thread timestamp */
        set_flushtime_last_update(dcontext, flushtime_global);
    }
    d_r_mutex_unlock(&shared_cache_flush_lock);

    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2, "  Flushed %d frags\n", num);
    return not_flushed;
}

/* Deletes all pending-delete thread-private vm areas belonging to dcontext.
 * Returns false iff was_I_flushed ends up being deleted.
 */
bool
vm_area_flush_fragments(dcontext_t *dcontext, fragment_t *was_I_flushed)
{
    thread_data_t *data = GET_DATA(dcontext, 0);
    vm_area_vector_t *v = &data->areas;
    fragment_t *entry, *next;
    int i, num = 0;
    bool not_flushed = true;
    /* should call vm_area_check_shared_pending for shared flushing */
    ASSERT(data != shared_data);

    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2, "vm_area_flush_fragments\n");
    /* walk backwards to avoid O(n^2) */
    for (i = v->length - 1; i >= 0; i--) {
        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
            "  Considering %d == " PFX ".." PFX "\n", i, v->buf[i].start, v->buf[i].end);
        if (TEST(VM_DELETE_ME, v->buf[i].vm_flags)) {
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\tdeleting all fragments in region " PFX ".." PFX "\n", v->buf[i].start,
                v->buf[i].end);
            for (entry = v->buf[i].custom.frags; entry != NULL; entry = next) {
                next = FRAG_NEXT(entry);
                LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 5,
                    "\tremoving " PFX "%s F%d(" PFX ")\n", entry,
                    FRAG_MULTI(entry) ? " multi" : "", FRAG_ID(entry), FRAG_PC(entry));
                if (FRAG_FRAG(entry) == was_I_flushed) {
                    not_flushed = false;
                    if (was_I_flushed == dcontext->last_fragment)
                        last_exit_deleted(dcontext);
                }
                ASSERT(TEST(FRAG_WAS_DELETED, FRAG_FRAG(entry)->flags));
                ASSERT(FRAG_ALSO_DEL_OK(entry) == NULL);
                fragment_delete(dcontext, FRAG_FRAG(entry),
                                /* We used to leave link, vmarea, and htable removal
                                 * until here for private fragments, but for case
                                 * 3559 we wanted link removal at unlink time, and
                                 * the 3 of them must go together, so we now do all 3
                                 * at unlink time just like for shared fragments.
                                 */
                                FRAGDEL_NO_OUTPUT | FRAGDEL_NO_UNLINK |
                                    FRAGDEL_NO_HTABLE | FRAGDEL_NO_VMAREA);
                STATS_INC(num_fragments_deleted_consistency);
                num++;
            }
            v->buf[i].custom.frags = NULL;
            /* could just remove flush region...but we flushed entire vm region
             * ASSUMPTION: remove_vm_area, given exact bounds, simply shifts later
             * areas down in vector!
             */
            LOG(THREAD, LOG_VMAREAS, 3, "Before removing vm area:\n");
            DOLOG(3, LOG_VMAREAS, { print_vm_areas(v, THREAD); });
            remove_vm_area(v, v->buf[i].start, v->buf[i].end, false);
            LOG(THREAD, LOG_VMAREAS, 3, "After removing vm area:\n");
            DOLOG(3, LOG_VMAREAS, { print_vm_areas(v, THREAD); });
        }
    }

#ifdef WINDOWS
    /* The relink needs a real thread dcontext, so don't pass a GLOBAL_DCONTEXT
     * in. This can occur when flushing shared fragments. Functionally, this is
     * fine since only private fragments are routed thru shared syscall, and
     * flush requests for such fragments are provided with a real thread
     * context.
     */
    if (DYNAMO_OPTION(shared_syscalls) && dcontext != GLOBAL_DCONTEXT &&
        !IS_SHARED_SYSCALL_THREAD_SHARED) {
        /* re-link shared syscall */
        link_shared_syscall(dcontext);
    }
#endif

    /* i#849: re-link private xfer */
    if (dcontext != GLOBAL_DCONTEXT && special_ibl_xfer_is_thread_private())
        link_special_ibl_xfer(dcontext);

    data->last_area = NULL;

    DOSTATS({
        if (num == 0)
            STATS_INC(num_flushq_actually_empty);
    });
    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2, "  Flushed %d frags\n", num);
    DOLOG(7, LOG_VMAREAS, {
        SHARED_VECTOR_RWLOCK(&data->areas, read, lock);
        print_fraglists(dcontext);
        SHARED_VECTOR_RWLOCK(&data->areas, read, unlock);
    });

    return not_flushed;
}

/* Flushes all units grouped with info.
 * Caller must hold change_linking_lock, read lock hotp_get_lock(), and
 * executable_areas lock.
 */
static void
vm_area_flush_coarse_unit(dcontext_t *dcontext, coarse_info_t *info_in, vm_area_t *area,
                          bool all_synched, bool entire)
{
    coarse_info_t *info = info_in, *next_info;
    ASSERT(info != NULL);
    ASSERT_OWN_RECURSIVE_LOCK(true, &change_linking_lock);
#ifdef HOT_PATCHING_INTERFACE
    ASSERT_OWN_READWRITE_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
#endif
    ASSERT(READ_LOCK_HELD(&executable_areas->lock));
    /* Need a real dcontext for persisting rac */
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();
    if (DYNAMO_OPTION(coarse_freeze_at_unload)) {
        /* we do not try to freeze if we've failed to suspend the world */
        if (all_synched) {
            /* in-place builds a separate unit anyway so no savings that way */
            vm_area_coarse_region_freeze(dcontext, info, area, false /*!in place*/);
            STATS_INC(persist_unload_try);
        } else {
            SYSLOG_INTERNAL_WARNING_ONCE("not freezing due to synch failure");
            STATS_INC(persist_unload_suspend_failure);
        }
    }
    while (info != NULL) { /* loop over primary and secondary unit */
        next_info = info->non_frozen;
        ASSERT(info->frozen || info->non_frozen == NULL);
        if (!entire && TEST(PERSCACHE_CODE_INVALID, info->flags)) {
            /* Do not reset yet as it may become valid again.
             * Assumption: if !entire, we will leave this info there.
             */
            /* Should only mark invalid if no or empty secondary unit */
            ASSERT(next_info == NULL || next_info->cache == NULL);
            break;
        }
        DOSTATS({
            if (info->persisted) {
                STATS_INC(flush_persisted_units);
                if (os_module_get_flag(info->base_pc, MODULE_BEING_UNLOADED))
                    STATS_INC(flush_persisted_unload);
            }
            STATS_INC(flush_coarse_units);
        });
        coarse_unit_reset_free(dcontext, info, false /*no locks*/, true /*unlink*/,
                               true /*give up primary*/);
        /* We only want one non-frozen unit per region; we keep the 1st unit */
        if (info != info_in) {
            coarse_unit_free(GLOBAL_DCONTEXT, info);
            info = NULL;
        } else
            coarse_unit_mark_in_use(info); /* still in-use if re-used */
        /* The remaining info itself is freed from exec list in remove_vm_area,
         * though may remain if only part of this region is removed
         * and will be lazily re-initialized if we execute from there again.
         * FIXME: case 8640: better to remove it all here?
         */
        info = next_info;
        ASSERT(info == NULL || !info->frozen);
    }
}

/* Assumes that all threads are suspended at safe synch points.
 * Flushes fragments in the region [start, end) in the vmarea
 * list for del_dcontext.
 * If dcontext == del_dcontext == GLOBAL_DCONTEXT,
 *   removes shared fine fragments and coarse units in the region.
 * If dcontext == thread and del_dcontext == GLOBAL_DCONTEXT,
 *   removes any ibl table entries for shared fragments in the region.
 *   WARNING: this routine will not remove coarse ibl entries!
 * Else (both dcontexts are the local thread's), deletes private fragments
 *   in the region.
 * FIXME: share code w/ vm_area_unlink_fragments() and vm_area_flush_fragments()!
 * all_synched is ignored unless dcontext == GLOBAL_DCONTEXT
 */
void
vm_area_allsynch_flush_fragments(dcontext_t *dcontext, dcontext_t *del_dcontext,
                                 app_pc start, app_pc end, bool exec_invalid,
                                 bool all_synched)
{
    thread_data_t *data = GET_DATA(del_dcontext, 0);
    vm_area_vector_t *v = &data->areas;
    fragment_t *entry, *next;
    int i;
    bool remove_shared_vm_area = true;
    DEBUG_DECLARE(int num_fine = 0;)
    DEBUG_DECLARE(int num_coarse = 0;)

    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
        "vm_area_allsynch_flush_fragments " PFX " " PFX "\n", dcontext, del_dcontext);
    ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock));
    ASSERT(is_self_allsynch_flushing());

    /* change_linking_lock is higher ranked than shared_vm_areas lock and is
     * acquired for fragment_delete()'s unlinking as well as fcache removal to
     * add to free list, so we must grab it up front.
     * coarse_unit_persist and coarse_unit_freeze also require it to be held.
     */
    acquire_recursive_lock(&change_linking_lock);

    if (dcontext == GLOBAL_DCONTEXT && del_dcontext == GLOBAL_DCONTEXT) {
        /* We can't add persisted units to shared vector at load time due to
         * lock rank orders, so we normally add on first access -- but we can
         * flush before any access, so we must walk exec areas here.
         * While we're at it we do our coarse unit freeing here, so don't have
         * to do lookups in exec areas while walking shared vmarea vector below.
         */
#ifdef HOT_PATCHING_INTERFACE
        if (DYNAMO_OPTION(hot_patching))
            d_r_read_lock(hotp_get_lock()); /* case 9970: rank hotp < exec_areas */
#endif
        d_r_read_lock(&executable_areas->lock); /* no need to write */
        for (i = 0; i < executable_areas->length; i++) {
            if (TEST(FRAG_COARSE_GRAIN, executable_areas->buf[i].frag_flags) &&
                start < executable_areas->buf[i].end &&
                end > executable_areas->buf[i].start) {
                coarse_info_t *coarse =
                    (coarse_info_t *)executable_areas->buf[i].custom.client;
                bool do_flush = (coarse != NULL);
#ifdef HOT_PATCHING_INTERFACE
                /* Case 9995: do not flush for 1-byte (mostly hotp) regions that are
                 * still valid execution regions and that are recorded as not being
                 * present in persistent caches.
                 */
                if (do_flush && !exec_invalid && start + 1 == end &&
                    coarse->hotp_ppoint_vec != NULL) {
                    app_pc modbase = get_module_base(coarse->base_pc);
                    ASSERT(modbase <= start);
                    /* Only persisted units store vec, though we could store for
                     * frozen but not persisted if we had frequent nudges throwing
                     * them out. */
                    ASSERT(coarse->persisted);
                    if (hotp_ppoint_on_list((app_rva_t)(start - modbase),
                                            coarse->hotp_ppoint_vec,
                                            coarse->hotp_ppoint_vec_num)) {
                        do_flush = false;
                        STATS_INC(perscache_hotp_flush_avoided);
                        remove_shared_vm_area = false;
                    }
                }
#endif
                if (do_flush) {
                    vm_area_flush_coarse_unit(dcontext, coarse, &executable_areas->buf[i],
                                              all_synched,
                                              start <= executable_areas->buf[i].start &&
                                                  end >= executable_areas->buf[i].end);
                    DODEBUG({ num_coarse++; });
                    if (TEST(VM_ADD_TO_SHARED_DATA, executable_areas->buf[i].vm_flags)) {
                        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                            "\tdeleting coarse unit not yet in shared vector " PFX
                            ".." PFX "\n",
                            executable_areas->buf[i].start, executable_areas->buf[i].end);
                        /* This flag is only relevant for persisted units, so we clear it
                         * here since this same coarse_info_t may be re-used
                         */
                        executable_areas->buf[i].vm_flags &= ~VM_ADD_TO_SHARED_DATA;
                    }
                }
            }
        }
        d_r_read_unlock(&executable_areas->lock);
#ifdef HOT_PATCHING_INTERFACE
        if (DYNAMO_OPTION(hot_patching))
            d_r_read_unlock(hotp_get_lock());
#endif
    }

    SHARED_VECTOR_RWLOCK(v, write, lock);
    /* walk backwards to avoid O(n^2)
     * FIXME case 9819: could use executable_area_overlap_bounds() to avoid linear walk
     */
    for (i = v->length - 1; i >= 0; i--) {
        if (start < v->buf[i].end && end > v->buf[i].start) {
            if (v->buf[i].start < start || v->buf[i].end > end) {
                /* see comments in vm_area_unlink_fragments() */
                LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                    "\tWARNING: region " PFX ".." PFX " is larger than flush area"
                    " " PFX ".." PFX "\n",
                    v->buf[i].start, v->buf[i].end, start, end);
            }
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\tdeleting all fragments in region " PFX ".." PFX "\n", v->buf[i].start,
                v->buf[i].end);
            /* We flush coarse units in executable_areas walk down below */
            /* We can have fine fragments here as well */
            if (v->buf[i].custom.frags != NULL) {
                for (entry = v->buf[i].custom.frags; entry != NULL; entry = next) {
                    next = FRAG_NEXT(entry);
                    if (dcontext == del_dcontext) {
                        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 5,
                            "\tremoving " PFX "%s F%d(" PFX ")\n", entry,
                            FRAG_MULTI(entry) ? " multi" : "", FRAG_ID(entry),
                            FRAG_PC(entry));
                        if (SHARED_IBT_TABLES_ENABLED()) {
                            /* fragment_remove() won't remove from shared ibt tables,
                             * b/c assuming we didn't do the synch for it, so we
                             * have to explicitly remove
                             */
                            fragment_remove_from_ibt_tables(dcontext, FRAG_FRAG(entry),
                                                            true /*rm from shared*/);
                        }
                        fragment_delete(dcontext, FRAG_FRAG(entry), FRAGDEL_ALL);
                        STATS_INC(num_fragments_deleted_consistency);
                        DODEBUG({ num_fine++; });
                    } else {
                        ASSERT(dcontext != GLOBAL_DCONTEXT &&
                               del_dcontext == GLOBAL_DCONTEXT);
                        fragment_remove_from_ibt_tables(dcontext, FRAG_FRAG(entry),
                                                        false /*shouldn't be in shared*/);
                    }
                }
                if (dcontext == del_dcontext)
                    v->buf[i].custom.frags = NULL;
            }
            if (dcontext == del_dcontext && remove_shared_vm_area) {
                /* could just remove flush region...but we flushed entire vm region
                 * ASSUMPTION: remove_vm_area, given exact bounds, simply shifts later
                 * areas down in vector!
                 */
                LOG(THREAD, LOG_VMAREAS, 3, "Before removing vm area:\n");
                DOLOG(3, LOG_VMAREAS, { print_vm_areas(v, THREAD); });
                remove_vm_area(v, v->buf[i].start, v->buf[i].end, false);
                LOG(THREAD, LOG_VMAREAS, 3, "After removing vm area:\n");
                DOLOG(3, LOG_VMAREAS, { print_vm_areas(v, THREAD); });
            } else {
                ASSERT(dcontext != del_dcontext ||
                       /* should only not flush for special hotp case 9995 */
                       start + 1 == end);
            }
        }
    }

    if (dcontext == del_dcontext)
        data->last_area = NULL;
    SHARED_VECTOR_RWLOCK(v, write, unlock);
    release_recursive_lock(&change_linking_lock);

    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
        "  Flushed %d fine frags & %d coarse units\n", num_fine, num_coarse);
    DOLOG(7, LOG_VMAREAS, {
        SHARED_VECTOR_RWLOCK(v, read, lock);
        print_fraglists(dcontext);
        SHARED_VECTOR_RWLOCK(v, read, unlock);
    });
}

/* Deletes all coarse units */
void
vm_area_coarse_units_reset_free()
{
    vm_area_vector_t *v = executable_areas;
    int i;
    ASSERT(DYNAMO_OPTION(coarse_units));
    LOG(GLOBAL, LOG_FRAGMENT | LOG_VMAREAS, 2, "vm_area_coarse_units_reset_free\n");
    ASSERT(dynamo_exited || dynamo_resetting);
    DOLOG(1, LOG_VMAREAS, {
        LOG(GLOBAL, LOG_VMAREAS, 1, "\nexecutable_areas before reset:\n");
        print_executable_areas(GLOBAL);
    });
    /* We would grab executable_areas_lock but coarse_unit_reset_free() grabs
     * change_linking_lock and coarse_info_lock, both of higher rank.  We could
     * grab change_linking_lock first here and raise executable_areas_lock above
     * coarse_info_lock's rank, but executable_areas_lock can be acquired during
     * coarse_unit_unlink after special_heap_lock -- so the best solution is to
     * not grab executable_areas_lock here and rely on reset synch.
     */
    for (i = 0; i < v->length; i++) {
        if (TEST(FRAG_COARSE_GRAIN, v->buf[i].frag_flags)) {
            coarse_info_t *info_start = (coarse_info_t *)v->buf[i].custom.client;
            coarse_info_t *info = info_start, *next_info;
            ASSERT(info != NULL);
            while (info != NULL) { /* loop over primary and secondary unit */
                next_info = info->non_frozen;
                ASSERT(info->frozen || info->non_frozen == NULL);
                LOG(GLOBAL, LOG_FRAGMENT | LOG_VMAREAS, 2,
                    "\tdeleting all fragments in region " PFX ".." PFX "\n",
                    v->buf[i].start, v->buf[i].end);
                coarse_unit_reset_free(GLOBAL_DCONTEXT, info, false /*no locks*/,
                                       true /*unlink*/, true /*give up primary*/);
                /* We only want one non-frozen unit per region; we keep the 1st one */
                if (info != info_start) {
                    coarse_unit_free(GLOBAL_DCONTEXT, info);
                    info = NULL;
                } else
                    coarse_unit_mark_in_use(info); /* still in-use if re-used */
                /* The start info itself is freed in remove_vm_area, if exiting */
                /* XXX i#1051: should re-load persisted caches after reset */
                info = next_info;
                ASSERT(info == NULL || !info->frozen);
            }
        }
    }
}

/* Returns true if info && info->non_frozen meet the size requirements
 * for persisting.
 */
static bool
coarse_region_should_persist(dcontext_t *dcontext, coarse_info_t *info)
{
    bool cache_large_enough = false;
    size_t cache_size = 0;
    /* Must hold lock to get size but ok for size to change afterward;
     * normal usage has all threads synched */
    if (!info->persisted) {
        d_r_mutex_lock(&info->lock);
        cache_size += coarse_frozen_cache_size(dcontext, info);
        d_r_mutex_unlock(&info->lock);
    }
    if (info->non_frozen != NULL) {
        d_r_mutex_lock(&info->non_frozen->lock);
        cache_size += coarse_frozen_cache_size(dcontext, info->non_frozen);
        d_r_mutex_unlock(&info->non_frozen->lock);
    }
    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
        "\tconsidering persisting coarse unit %s with cache size %d\n", info->module,
        cache_size);
    /* case 10107: check for disk space before freezing, if persisting.
     * A crude estimate is all we need up front (we'll do a precise check at file
     * write time): estimate that hashtables, stubs, etc. double cache size.
     */
    if (!coarse_unit_check_persist_space(INVALID_FILE, cache_size * 2)) {
        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2, "\tnot enough disk space for %s\n",
            info->module);
        STATS_INC(coarse_units_persist_nospace);
        return false;
    }
    cache_large_enough =
        (cache_size > DYNAMO_OPTION(coarse_freeze_min_size) ||
         (info->persisted &&
          /* FIXME: should use append size if merging only w/ disk as well */
          cache_size > DYNAMO_OPTION(coarse_freeze_append_size)));
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    /* Real cost is in pages touched while walking reloc, which is
     * typically 80% of module.
     */
    if (rct_module_live_entries(dcontext, info->base_pc, RCT_RCT) >
        DYNAMO_OPTION(coarse_freeze_rct_min)) {
        DOSTATS({
            if (!cache_large_enough)
                STATS_INC(persist_code_small);
        });
        LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
            "\tRCT entries are over threshold so persisting %s\n", info->module);
        return true;
    }
#endif /* defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH) */
    DOSTATS({
        if (!cache_large_enough) {
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\tnot persisting %s since too small\n", info->module);
            STATS_INC(persist_too_small);
        }
    });
    return cache_large_enough;
}

/* FIXME case 9975: we should provide separate control over persistence
 * (today we assume !in_place==persist) so we can persist and use in_place
 * rather than having to wait until next run to get the benefit.
 */
/* FIXME: if we map in a newly persisted unit we need to set
 * VM_PERSISTED_CACHE, but we only care about it in executable_areas.
 */
/* Caller must hold change_linking_lock, read lock hotp_get_lock(), and
 * either executable_areas lock or dynamo_all_threads_synched.
 */
static void
vm_area_coarse_region_freeze(dcontext_t *dcontext, coarse_info_t *info, vm_area_t *area,
                             bool in_place)
{
    coarse_info_t *frozen_info = NULL;   /* the already-frozen info */
    coarse_info_t *unfrozen_info = NULL; /* the un-frozen info */
    if (!DYNAMO_OPTION(coarse_enable_freeze) || RUNNING_WITHOUT_CODE_CACHE())
        return;
    ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
    ASSERT(info != NULL);
    ASSERT_OWN_RECURSIVE_LOCK(true, &change_linking_lock);
#ifdef HOT_PATCHING_INTERFACE
    ASSERT_OWN_READWRITE_LOCK(DYNAMO_OPTION(hot_patching), hotp_get_lock());
#endif
    ASSERT(READ_LOCK_HELD(&executable_areas->lock) || dynamo_all_threads_synched);
    /* Note that freezing in place will call mark_executable_area_coarse_frozen and
     * add a new unit, so next_info should not be traversed after freezing.
     */
    if (info->frozen) {
        frozen_info = info;
        unfrozen_info = info->non_frozen;
    } else {
        unfrozen_info = info;
        ASSERT(info->non_frozen == NULL);
    }
    if (unfrozen_info != NULL && unfrozen_info->cache != NULL /*skip empty units*/ &&
        !TEST(PERSCACHE_CODE_INVALID, unfrozen_info->flags) &&
        /* we only freeze a unit in presence of a frozen unit if we're merging
         * (we don't support side-by-side frozen units) */
        (DYNAMO_OPTION(coarse_freeze_merge) || frozen_info == NULL)) {
        if (in_place || coarse_region_should_persist(dcontext, info)) {
            coarse_info_t *frozen;
            coarse_info_t *premerge;
            LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2,
                "\tfreezing coarse unit for region " PFX ".." PFX " %s\n", info->base_pc,
                info->end_pc, info->module);
            if (frozen_info != NULL && in_place) {
                /* We're freezing unfrozen_info, merging frozen_info into it, and
                 * then deleting frozen_info, so we need to replace it with just
                 * unfrozen_info (soon to be frozen); we do it this way since
                 * mark_executable_area_coarse_frozen assumes being-frozen info is
                 * the 1st info.
                 */
                area->custom.client = (void *)unfrozen_info;
            }
            frozen = coarse_unit_freeze(dcontext, unfrozen_info, in_place);
            ASSERT(frozen != NULL && frozen->frozen);
            /* mark_executable_area_coarse_frozen creates new non_frozen for in_place */
            ASSERT(!in_place || frozen->non_frozen != NULL);
            premerge = frozen;
            if (frozen_info != NULL) {
                ASSERT(DYNAMO_OPTION(coarse_freeze_merge));
                /* case 9701: more efficient to merge while freezing, but
                 * this way we share code w/ offline merger
                 */
                /* I would put most-likely-larger unit as first source since more
                 * efficient to merge into, but we need frozen first in case
                 * we are in_place.
                 */
                frozen = coarse_unit_merge(dcontext, frozen, frozen_info, in_place);
                ASSERT(frozen != NULL);
                ASSERT(!in_place || frozen->non_frozen != NULL);
                if (frozen == NULL && in_place) {
                    /* Shouldn't happen w/ online units; if it does we end up
                     * tossing frozen_info w/o merging it
                     */
                    frozen = premerge;
                }
                /* for !in_place we free premerge after persisting, so clients don't
                 * get deletion events that remove data from hashtables too early
                 * (xref https://github.com/DynamoRIO/drmemory/issues/869)
                 */
                if (in_place) {
                    coarse_unit_reset_free(dcontext, frozen_info, false /*no locks*/,
                                           true /*need to unlink*/,
                                           false /*keep primary*/);
                    coarse_unit_free(dcontext, frozen_info);
                    frozen_info = NULL;
                }
            }
            if (!in_place && frozen != NULL) {
                coarse_unit_persist(dcontext, frozen);
                coarse_unit_reset_free(dcontext, frozen, false /*no locks*/,
                                       false /*already unlinked*/,
                                       false /*not in use anyway*/);
                coarse_unit_free(dcontext, frozen);
                frozen = NULL;
            } else
                ASSERT(frozen == unfrozen_info);
            if (frozen_info != NULL && !in_place && premerge != NULL) {
                /* see comment above: delayed until after persist */
                coarse_unit_reset_free(dcontext, premerge, false /*no locks*/,
                                       false /*already unlinked*/,
                                       false /*not in use anyway*/);
                ASSERT(frozen != premerge);
                coarse_unit_free(dcontext, premerge);
                premerge = NULL;
            }
        }
    } else if (frozen_info != NULL && frozen_info->cache != NULL && !in_place &&
               !frozen_info->persisted) {
        ASSERT(!TEST(PERSCACHE_CODE_INVALID, frozen_info->flags));
        if (coarse_region_should_persist(dcontext, frozen_info))
            coarse_unit_persist(dcontext, frozen_info);
    }
}

/* FIXME: could create iterator and move this and vm_area_coarse_units_reset_free()
 * into callers
 * If !in_place this routine freezes (if not already) and persists.
 */
void
vm_area_coarse_units_freeze(bool in_place)
{
    vm_area_vector_t *v = executable_areas;
    int i;
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (!DYNAMO_OPTION(coarse_units) || !DYNAMO_OPTION(coarse_enable_freeze) ||
        RUNNING_WITHOUT_CODE_CACHE())
        return;
    ASSERT(!RUNNING_WITHOUT_CODE_CACHE());
    ASSERT(dcontext != NULL);
    LOG(THREAD, LOG_FRAGMENT | LOG_VMAREAS, 2, "vm_area_coarse_units_freeze\n");
    ASSERT(dynamo_all_threads_synched);
    acquire_recursive_lock(&change_linking_lock);
#ifdef HOT_PATCHING_INTERFACE
    if (DYNAMO_OPTION(hot_patching))
        d_r_read_lock(hotp_get_lock());
#endif
    /* We would grab executable_areas_lock but coarse_unit_freeze() grabs
     * change_linking_lock and coarse_info_lock, both of higher rank.  We could
     * grab change_linking_lock first here and raise executable_areas_lock above
     * coarse_info_lock's rank, but executable_areas_lock can be acquired during
     * coarse_unit_unlink after special_heap_lock -- so the best solution is to
     * not grab executable_areas_lock here and rely on all_threads_synched.
     * Could make executable_areas_lock recursive and grab all locks here?
     */
    for (i = 0; i < v->length; i++) {
        if (TEST(FRAG_COARSE_GRAIN, v->buf[i].frag_flags)) {
            coarse_info_t *info = (coarse_info_t *)v->buf[i].custom.client;
            ASSERT(info != NULL);
            if (info != NULL)
                vm_area_coarse_region_freeze(dcontext, info, &v->buf[i], in_place);
        }
    }
#ifdef HOT_PATCHING_INTERFACE
    if (DYNAMO_OPTION(hot_patching))
        d_r_read_unlock(hotp_get_lock());
#endif
    release_recursive_lock(&change_linking_lock);
}

#if 0 /* not used */
/* remove a thread's vm area */
static bool
remove_thread_vm_area(dcontext_t *dcontext, app_pc start, app_pc end)
{
    thread_data_t *data = GET_DATA(dcontext, 0);
    bool ok;
    LOG(THREAD, LOG_VMAREAS, 2, "removing thread "TIDFMT" vm area: "PFX"-"PFX"\n",
        dcontext->owning_thread, start, end);
    /* no lock needed, this is thread-private */
    ok = remove_vm_area(&data->areas, start, end, false);
    /* due to re-sorting, areas move around...not worth trying to shift,
     * just clear the cache area */
    data->last_area = NULL;
    return ok;
}
#endif

/* returns true if the passed in area overlaps any thread executable areas */
bool
thread_vm_area_overlap(dcontext_t *dcontext, app_pc start, app_pc end)
{
    thread_data_t *data = GET_DATA(dcontext, 0);
    bool res;
    if (data == shared_data) {
        ASSERT(!self_owns_write_lock(&shared_data->areas.lock));
        SHARED_VECTOR_RWLOCK(&data->areas, write, lock);
    }
    res = vm_area_overlap(&data->areas, start, end);
    if (data == shared_data) {
        SHARED_VECTOR_RWLOCK(&data->areas, write, unlock);
    }
    return res;
}

/* Returns NULL if should re-execute the faulting write
 * Else returns the target pc for a new basic block -- caller should
 * return to d_r_dispatch rather than the code cache
 * If instr_cache_pc==NULL, assumes the cache is unavailable (due to reset).
 */
app_pc
handle_modified_code(dcontext_t *dcontext, cache_pc instr_cache_pc, app_pc instr_app_pc,
                     app_pc target, fragment_t *f)
{
    /* FIXME: for Linux, this is all happening inside signal handler...
     * flushing could take a while, and signals are blocked the entire time!
     */
    app_pc base_pc, flush_start = NULL;
    app_pc instr_size_pc;
    size_t size, flush_size = 0;
    uint opnd_size = 0;
    uint prot;
    overlap_info_t info = { 0, /* init to 0 so info.overlap is false */ };
    app_pc bb_start = NULL;
    app_pc bb_end = NULL;
    app_pc bb_pstart = NULL, bb_pend = NULL; /* pages occupied by instr's bb */
    vm_area_t *a = NULL;
    fragment_t wrapper;
    /* get the "region" size (don't use exec list, it merges regions),
     * the os merges regions too, and we might have changed the protections
     * on the region and caused it do so, so below we take the intersection
     * with the enclosing executable_areas region if it exists */
    bool ok = get_memory_info(target, &base_pc, &size, &prot);
    if (f == NULL && instr_cache_pc != NULL)
        f = fragment_pclookup(dcontext, instr_cache_pc, &wrapper);
    /* FIXME: what if seg fault is b/c target is unreadable?  then should have
     * app die, not us trigger assertion!
     */
    /* In the absence of reset, f MUST still be in the cache since we're still
     * nolinking, and pclookup will find it even if it's no longer in htables.
     * But, a reset can result in not having the fragment available at all.  In
     * that case we just flush the whole region and hope that in the future
     * we'll eventually identify the writer, but there's a possibility of no
     * forward progress if another thread keeps flushing writing fragment
     * (ro2sandbox_threshold would alleviate that).
     */
    DOLOG(1, LOG_VMAREAS, {
        if (instr_cache_pc == NULL) {
            LOG(THREAD, LOG_VMAREAS, 1,
                "WARNING: cache unavailable for processing code mod @ app pc " PFX "\n",
                instr_app_pc);
        } else if (f == NULL) {
            LOG(THREAD, LOG_VMAREAS, 1,
                "WARNING: cannot find fragment @ writer pc " PFX " -- was deleted, "
                "or native\n",
                instr_cache_pc);
        }
    });
    ASSERT(ok);
    SYSLOG_INTERNAL_WARNING_ONCE("writing to executable region.");
    STATS_INC(num_write_faults);
    d_r_read_lock(&executable_areas->lock);
    lookup_addr(executable_areas, (app_pc)target, &a);
    if (a == NULL) {
        LOG(THREAD, LOG_VMAREAS, 1,
            "\tRegion for " PFX " not exec, probably data on same page\n", target);
        DOLOG(2, LOG_VMAREAS, { print_vm_areas(executable_areas, THREAD); });
    } else {
        /* The os may have merged regions  because we made a region read
         * only! (ref case 2803), thus we should take the intersection of
         * the region on our list and the os region */
        /* make sure to handle sub-page regions, pad to page boundary */
        app_pc a_pstart = (app_pc)ALIGN_BACKWARD(a->start, PAGE_SIZE);
        app_pc a_pend = (app_pc)ALIGN_FORWARD(a->end, PAGE_SIZE);
        if (a_pstart > base_pc) {
            size -= a_pstart - base_pc;
            base_pc = a_pstart;
        }
        if (a_pend < base_pc + size) {
            size = a_pend - base_pc;
        }
        LOG(THREAD, LOG_VMAREAS, 1,
            "WARNING: Exec " PFX "-" PFX " %s%s written @" PFX " by " PFX " == app " PFX
            "\n",
            base_pc, base_pc + size, ((a->vm_flags & VM_WRITABLE) != 0) ? "W" : "",
            ((prot & MEMPROT_EXEC) != 0) ? "E" : "", target, instr_cache_pc,
            instr_app_pc);
    }
    d_r_read_unlock(&executable_areas->lock);
#ifdef DGC_DIAGNOSTICS
    DOLOG(1, LOG_VMAREAS, {
        /* it's hard to locate frag owning an app pc in the cache, so we wait until
         * we flush and only check the flushed frags
         */
        char buf[MAXIMUM_SYMBOL_LENGTH];
        print_symbolic_address(instr_app_pc, buf, sizeof(buf), false);
        LOG(THREAD, LOG_VMAREAS, 1, "code written by app pc " PFX " from bb %s:\n",
            instr_app_pc, buf);
        disassemble_app_bb(dcontext, instr_app_pc, THREAD);
    });
#endif
    if (TEST(MEMPROT_WRITE, prot)) {
        LOG(THREAD, LOG_VMAREAS, 1,
            "\tWARNING: region now writable: assuming another thread already flushed it\n"
            "\tgoing to flush again just to make sure\n");
        /* we could just bail here, but could have no forward progress if repeated
         * races between selfmod writer and out-of-region writer
         */
        STATS_INC(num_write_fault_races);
    }

    /* see if writer is inside our region
     * need instr size and opnd size to check for page boundary overlaps!
     * For reset when the cache is not present, we decode from the app code,
     * though that's racy!  solution is to have reset store a copy of the app instr
     * (FIXME case 7393).
     */
    instr_size_pc = (instr_cache_pc == NULL) ? instr_app_pc : instr_cache_pc;
    DEBUG_DECLARE(app_pc next_pc =)
    decode_memory_reference_size(dcontext, instr_size_pc, &opnd_size);
    ASSERT(next_pc != NULL);
    ASSERT(opnd_size != 0);
    DEBUG_DECLARE(size_t instr_size = next_pc - instr_size_pc;)
    /* FIXME case 7492: if write crosses page boundary, the reported faulting
     * target for win32 will be in the middle of the instr's target (win32
     * reports the first unwritable byte).  (On Linux we're fine as we calculate
     * the target ourselves.)
     */
    if (target + opnd_size > base_pc + size) {
        /* must expand to cover entire target, even if crosses OS regions */
        app_pc t_pend = (app_pc)ALIGN_FORWARD(target + opnd_size, PAGE_SIZE);
        size = t_pend - base_pc;
    }
    /* see if instr's bb is in region
     * not good enough to only check instr!
     * will end up in infinite loop if any part of bb overlaps the executable
     * region removed!
     * if f was deleted, we threw away its also info, so we have to do a full
     * overlaps lookup.  f cannot have been removed completely since we
     * count as being in the shared cache and could be inside f.
     */
    if (f != NULL &&
        /* faster check up front if frag not deleted -- BUT, we are in
         * a race w/ any flusher marking as deleted!
         * so, we make vm_list_overlaps not assert on a not-there fragment,
         * and only if it finds it and it's STILL not marked do we trust the
         * return value.
         */
        (vm_list_overlaps(dcontext, (void *)f, base_pc, base_pc + size) ||
         TEST(FRAG_WAS_DELETED, f->flags))) {
        fragment_overlaps(dcontext, f, instr_app_pc, instr_app_pc + 1,
                          false /* fine-grain! */, &info, &bb_start);
        /* if did fast check and it said overlap, slow check should too */
        ASSERT(TEST(FRAG_WAS_DELETED, f->flags) || info.overlap);
    }
    if (info.overlap) {
        /* instr_t may be in region, but could also be from a different region
         * included in a trace.  Determine if instr bb overlaps with target
         * region.
         * Move to page boundaries, with inclusive end pages.
         * We must look at entire bb containing instr, not just instr
         * itself (can't isolate write from its bb -- will always
         * enter from top of bb, even across direct cti)
         */
        ASSERT(info.overlap && bb_start != NULL);
        if (info.contiguous)
            bb_end = info.bb_end;
        else {
            /* FIXME: could be smart and have info include list of all pages,
             * handle situations like start outside of region and jmp/call in,
             * but this is going to be rare -- let's just take min and max of
             * entire bb, even if that includes huge area (in which case we'll
             * consider it self-modifying code, even if jumped over middle)
             */
            bb_start = info.min_pc;
            bb_end = info.max_pc;
            ASSERT(bb_start != NULL && bb_end != NULL);
        }
        bb_pstart = (app_pc)PAGE_START(bb_start);
        bb_pend = (app_pc)PAGE_START(bb_end);
        ASSERT(instr_app_pc >= bb_pstart &&
               instr_app_pc + instr_size <= bb_pend + PAGE_SIZE);
        ASSERT(f != NULL); /* else info.overlap should not be set */
    }
    /* Now we can check if source bb overlaps target region. */
    if (info.overlap && base_pc < (bb_pend + PAGE_SIZE) && (base_pc + size) > bb_pstart) {
        /* bb pages overlap target region -
         * We want to split up region to keep instr exec but target writable.
         * All pages touched by target will become writable.
         * All pages in instr's bb must remain executable (can't isolate
         * write from its bb -- will always enter from top of bb)
         */
        /* pages occupied by target */
        app_pc tgt_pstart = (app_pc)PAGE_START(target);
        app_pc tgt_pend = (app_pc)PAGE_START(target + opnd_size);

        DOSTATS({
            /* race condition case of another thread flushing 1st */
            if (TEST(MEMPROT_WRITE, prot))
                STATS_INC(num_write_fault_races_selfmod);
        });

        LOG(THREAD, LOG_VMAREAS, 2, "Write instr is inside F%d " PFX "\n", f->id, f->tag);

        LOG(THREAD, LOG_VMAREAS, 1,
            "\tinstr's bb src " PFX "-" PFX " overlaps target " PFX "-" PFX "\n",
            bb_start, bb_end, target, target + opnd_size);

        /* look for selfmod overlap */
        if (bb_pstart <= tgt_pend && bb_pend >= tgt_pstart) {
            vm_area_t *execarea;
            app_pc nxt_on_page;
            LOG(THREAD, LOG_VMAREAS, 1,
                "WARNING: self-modifying code: instr @" PFX " (in bb " PFX "-" PFX ")\n"
                "\twrote to " PFX "-" PFX "\n",
                instr_app_pc, bb_start, bb_end, target, target + opnd_size);
            SYSLOG_INTERNAL_WARNING_ONCE("self-modifying code.");
            /* can leave non-intersection part of instr pages as executable,
             * no need to flush them
             */
            /* DGC_DIAGNOSTICS: have flusher pass target to
             * vm_area_unlink_fragments to check if code was actually overwritten
             */
            flush_fragments_in_region_start(
                dcontext, (app_pc)tgt_pstart, (tgt_pend + PAGE_SIZE - tgt_pstart),
                false /* don't own initexit_lock */, false /* keep futures */,
                true /* exec invalid */,
                false /* don't force synchall */
                _IF_DGCDIAG(target));
            /* flush_* grabbed exec areas lock for us, to make following
             * sequence atomic.
             * Need to change all exec areas on these pages to be selfmod.
             */
            for (ok = true, nxt_on_page = (app_pc)tgt_pstart;
                 ok && nxt_on_page < (app_pc)tgt_pend + PAGE_SIZE;) {
                ok = binary_search(executable_areas, nxt_on_page,
                                   (app_pc)tgt_pend + PAGE_SIZE, &execarea, NULL,
                                   true /* want 1st match! */);
                if (ok) {
                    nxt_on_page = execarea->end;
                    if (TESTANY(FRAG_SELFMOD_SANDBOXED, execarea->frag_flags)) {
                        /* not calling remove_vm_area so we have to vm_make_writable
                         * FIXME: why do we have to do anything if already selfmod?
                         */
                        if (DR_MADE_READONLY(execarea->vm_flags)) {
                            vm_make_writable(execarea->start,
                                             execarea->end - execarea->start);
                        }
                        continue;
                    }
                    if (execarea->start < (app_pc)tgt_pstart ||
                        execarea->end > (app_pc)tgt_pend + PAGE_SIZE) {
                        /* this area sticks out from our target area, so we split it
                         * by removing and then re-adding (as selfmod) the overlap portion
                         */
                        uint old_vmf = execarea->vm_flags;
                        uint old_ff = execarea->frag_flags;
                        app_pc old_start =
                            (execarea->start < tgt_pstart) ? tgt_pstart : execarea->start;
                        app_pc old_end = (execarea->end > tgt_pend + PAGE_SIZE)
                            ? tgt_pend + PAGE_SIZE
                            : execarea->end;
                        LOG(GLOBAL, LOG_VMAREAS, 2,
                            "removing executable vm area to mark selfmod: " PFX "-" PFX
                            "\n",
                            old_start, old_end);
                        remove_vm_area(executable_areas, old_start, old_end, true);
                        /* now re-add */
                        add_executable_vm_area(old_start, old_end, old_vmf,
                                               old_ff | FRAG_SELFMOD_SANDBOXED,
                                               true /*own lock */
                                               _IF_DEBUG("selfmod replacement"));
                        STATS_INC(num_selfmod_vm_areas);
                        /* this won't hurt our iteration since it's stateless except for
                         * nxt_on_page
                         */
                    } else {
                        LOG(THREAD, LOG_VMAREAS, 2,
                            "\tmarking " PFX "-" PFX " as selfmod\n", execarea->start,
                            execarea->end);
                        execarea->frag_flags |= SANDBOX_FLAG();
                        STATS_INC(num_selfmod_vm_areas);
                        /* not calling remove_vm_area so we have to vm_make_writable */
                        if (DR_MADE_READONLY(execarea->vm_flags)) {
                            vm_make_writable(execarea->start,
                                             execarea->end - execarea->start);
                        }
                    }
                }
            }
            LOG(GLOBAL, LOG_VMAREAS, 3,
                "After marking all areas in " PFX "-" PFX " as selfmod:\n", tgt_pstart,
                tgt_pend + PAGE_SIZE);
            DOLOG(3, LOG_VMAREAS, { print_vm_areas(executable_areas, GLOBAL); });
            flush_fragments_in_region_finish(dcontext,
                                             false /*don't keep initexit_lock*/);
            if (DYNAMO_OPTION(opt_jit) && !TEST(MEMPROT_WRITE, prot) &&
                is_jit_managed_area((app_pc)tgt_pstart)) {
                jitopt_clear_span((app_pc)tgt_pstart,
                                  (app_pc)(tgt_pend + PAGE_SIZE - tgt_pstart));
            }
            /* must execute instr_app_pc next, even though that new bb will be
             * useless afterward (will most likely re-enter from bb_start)
             */
            return instr_app_pc;
        } else {
            /* Not selfmod, but target and bb region may still overlap -
             * heuristic: split the region up -- assume will keep writing
             * to higher addresses and keep executing at higher addresses.
             */
            if (tgt_pend < bb_pstart) {
                /* make all pages from tgt_pstart up to bb_pstart or
                 * region end (which ever is first) non-exec */
                /* FIXME - CHECK - should we really be starting at
                 * base_pc instead? Not clear why we shouldn't start at
                 * region start (like we would if we didn't have an
                 * overlap). */
                flush_start = tgt_pstart;
                ASSERT(bb_pstart < (base_pc + size) && bb_pstart > tgt_pstart);
                flush_size = bb_pstart - tgt_pstart;
            } else if (tgt_pstart > bb_pend) {
                /* make all pages from tgt_pstart to end of region non-exec */
                flush_start = tgt_pstart;
                flush_size = (base_pc + size) - tgt_pstart;
            } else {
                /* should never get here -- all cases covered above */
                ASSERT_NOT_REACHED();
            }
            LOG(THREAD, LOG_VMAREAS, 2,
                "splitting region up, flushing just " PFX "-" PFX "\n", flush_start,
                flush_start + flush_size);
        }
    } else {
        ASSERT(!info.overlap || (f != NULL && TEST(FRAG_IS_TRACE, f->flags)));
        /* instr not in region, so move entire region off the executable list */
        flush_start = base_pc;
        flush_size = size;
        LOG(THREAD, LOG_VMAREAS, 2,
            "instr not in region, flushing entire " PFX "-" PFX "\n", flush_start,
            flush_start + flush_size);
    }

    /* DGC_DIAGNOSTICS: have flusher pass target to
     * vm_area_unlink_fragments to check if code was actually overwritten
     */
    flush_fragments_in_region_start(dcontext, flush_start, flush_size,
                                    false /* don't own initexit_lock */,
                                    false /* keep futures */, true /* exec invalid */,
                                    false /* don't force synchall */
                                    _IF_DGCDIAG(target));
    f = NULL; /* after the flush we don't know if it's safe to deref f */

    if (DYNAMO_OPTION(ro2sandbox_threshold) > 0) {
        /* add removed region to written list to track # of times this has happened
         * actually, we only track by the written-to page
         * FIXME case 8161: should we add more than just the page?
         * we'll keep adding the whole region until it hits the ro2sandbox threshold,
         * at which point we'll just add the page
         */
        ro_vs_sandbox_data_t *ro2s;
        d_r_write_lock(&written_areas->lock);
        /* use the add routine to lookup if present, add if not */
        add_written_area(written_areas, target, (app_pc)PAGE_START(target),
                         (app_pc)PAGE_START(target + opnd_size) + PAGE_SIZE, &a);
        ASSERT(a != NULL);
        ro2s = (ro_vs_sandbox_data_t *)a->custom.client;
        ro2s->written_count++;
        LOG(GLOBAL, LOG_VMAREAS, 2, "written area " PFX "-" PFX " now written %d X\n",
            a->start, a->end, ro2s->written_count);
        DOLOG(3, LOG_VMAREAS, {
            LOG(GLOBAL, LOG_VMAREAS, 2, "\nwritten areas:\n");
            print_vm_areas(written_areas, GLOBAL);
        });
        d_r_write_unlock(&written_areas->lock);
    }

    if (
#ifdef PROGRAM_SHEPHERDING
        !DYNAMO_OPTION(selfmod_futureexec) &&
#endif
        is_executable_area_on_all_selfmod_pages(target, target + opnd_size)) {
        /* We can be in various races with another thread in handling write
         * faults to this same region.  We check at the start of this routine,
         * but in practice (case 7911) I've seen the race more often show up
         * here, after the flush synch.  If another thread has already switched
         * the target region to selfmod, then we shouldn't remove it from
         * executable_areas here.  In fact if we were to remove it we would foil
         * the selfmod->remove future optimizations (case 280) (once-only at
         * NtFlush, selfmod when used to validate exec area, and remove
         * overlapping futures w/ new selfmod exec area).
         */
        /* FIXME: is it worth checking this selfmod overlap in earlier places,
         * like the start of this routine, or at the start of the flush synch,
         * which could save some synch work and perhaps avoid the flush
         * altogether?
         */
        STATS_INC(flush_selfmod_race_no_remove);
        LOG(THREAD, LOG_VMAREAS, 2,
            "Target " PFX " is already selfmod, race, no reason to remove\n", target);
    } else {
        /* flush_* grabbed exec areas lock for us, to make vm_make_writable,
         * remove global vm area, and lookup an atomic sequence
         */
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "removing executable vm area since written: " PFX "-" PFX "\n", flush_start,
            flush_start + flush_size);
        /* FIXME : are we removing regions that might not get re-added here?
         * what about things that came from once only future or mem prot changes,
         * the region removed here can be much larger then just the page written
         */
        /* FIXME (part of case 3744): should remove only non-selfmod regions here!
         * Then can eliminate the if above.   Could pass filter flag to remove_vm_area,
         * but better to just split code origins from consistency and not have
         * sub-page regions on the consistency list (case 3744).
         */
        remove_vm_area(executable_areas, flush_start, flush_start + flush_size,
                       true /*restore writability!*/);
        LOG(THREAD, LOG_VMAREAS, 2,
            "Removed " PFX "-" PFX " from exec list, continuing @ write\n", flush_start,
            flush_start + flush_size);
    }
    DOLOG(3, LOG_VMAREAS, {
        thread_data_t *data = GET_DATA(dcontext, 0);
        LOG(THREAD, LOG_VMAREAS, 2, "\nexecutable areas:\n");
        print_vm_areas(executable_areas, THREAD);
        LOG(THREAD, LOG_VMAREAS, 2, "\nthread areas:\n");
        print_vm_areas(&data->areas, THREAD);
    });

    /* There is no good way to tell if we flushed f or not, so need to start
     * interpreting at instr_app_pc. If f was a trace could overlap flushed
     * region even if the src bb didn't and anyways flushing can end up
     * flushing outside the requested region (entire vm_area_t). If we could tell
     * we could return NULL instead (which is a special flag that says redo the
     * write instead of going to d_r_dispatch) if f wasn't flushed.
     * FIXME - Redoing the write would be more efficient then going back to
     * d_r_dispatch and should be the common case. */
    flush_fragments_in_region_finish(dcontext, false /*don't keep initexit_lock*/);
    if (DYNAMO_OPTION(opt_jit) && !TEST(MEMPROT_WRITE, prot) &&
        is_jit_managed_area(flush_start))
        jitopt_clear_span(flush_start, flush_start + flush_size);
    return instr_app_pc;
}

/* Returns the counter a selfmod fragment should execute for -sandbox2ro_threshold */
uint *
get_selfmod_exec_counter(app_pc tag)
{
    vm_area_t *area = NULL;
    ro_vs_sandbox_data_t *ro2s;
    uint *counter;
    bool ok;
    d_r_read_lock(&written_areas->lock);
    ok = lookup_addr(written_areas, tag, &area);
    if (!ok) {
        d_r_read_unlock(&written_areas->lock);
        d_r_read_lock(&executable_areas->lock);
        d_r_write_lock(&written_areas->lock);
        ok = lookup_addr(executable_areas, tag, &area);
        ASSERT(ok && area != NULL);
        /* FIXME: do this addition whenever add new exec area marked as
         * selfmod?
         * FIXME case 8161: add only add one page?  since never split written_areas?
         * For now we add the whole region, reasoning that as a selfmod
         * region it's probably not very big anyway.
         * In Sun's JVM 1.4.2 we actually never get here b/c we always
         * have an executable region already present before we make it selfmod,
         * so we're only adding to written_areas when we get a write fault,
         * at which point we only use the surrounding page.
         */
        STATS_INC(num_sandbox_before_ro);
        add_written_area(written_areas, tag, area->start, area->end, &area);
        ASSERT(area != NULL);
        ro2s = (ro_vs_sandbox_data_t *)area->custom.client;
        counter = &ro2s->selfmod_execs;
        /* Inc of selfmod_execs from cache can have problems if it crosses a
         * cache line, so we assert on the 32-bit alignment we should get from
         * the heap.  add_written_area already asserts but we double-check here.
         */
        ASSERT(ALIGNED(counter, sizeof(uint)));
        d_r_write_unlock(&written_areas->lock);
        d_r_read_unlock(&executable_areas->lock);
    } else {
        ASSERT(ok && area != NULL);
        ro2s = (ro_vs_sandbox_data_t *)area->custom.client;
        counter = &ro2s->selfmod_execs;
        d_r_read_unlock(&written_areas->lock);
    }
    /* ref to counter will be accessed in-cache w/o read lock but
     * written_areas is never merged and counter won't be freed until
     * exit time.
     */
    return counter;
}

/* Returns true if f has been flushed */
bool
vm_area_selfmod_check_clear_exec_count(dcontext_t *dcontext, fragment_t *f)
{
    ro_vs_sandbox_data_t *ro2s = NULL;
    vm_area_t *exec_area = NULL, *written_area;
    app_pc start, end;
    bool ok;
    bool convert_s2ro = true;
    if (DYNAMO_OPTION(sandbox2ro_threshold) == 0)
        return false;

    /* NOTE - we could only grab the readlock here.  Even though we're going to
     * write to selfmod_execs count, it's not really protected by the written_areas
     * lock since we read and write to it from the cache.  Should change to read lock
     * if contention ever becomes an issue.  Note that we would then have to later
     * grab the write lock if we need to write to ro2s->written_count below. */
    d_r_write_lock(&written_areas->lock);

    ok = lookup_addr(written_areas, f->tag, &written_area);
    if (ok) {
        ro2s = (ro_vs_sandbox_data_t *)written_area->custom.client;
    } else {
        /* never had instrumentation */
        d_r_write_unlock(&written_areas->lock);
        return false;
    }
    if (ro2s->selfmod_execs < DYNAMO_OPTION(sandbox2ro_threshold)) {
        /* must be a real fragment modification, reset the selfmod_execs count
         * xref case 9908 */
        LOG(THREAD, LOG_VMAREAS, 3,
            "Fragment " PFX " self-write -> " PFX "-" PFX
            " selfmod exec counter reset, old"
            " count=%d\n",
            f->tag, written_area->start, written_area->end, ro2s->selfmod_execs);
        /* Write must be atomic since we access this field from the cache, an aligned
         * 4 byte write is atomic on the architectures we support. */
        ASSERT(sizeof(ro2s->selfmod_execs) == 4 && ALIGNED(&(ro2s->selfmod_execs), 4));
        ro2s->selfmod_execs = 0;
        d_r_write_unlock(&written_areas->lock);
        return false;
    }

    LOG(THREAD, LOG_VMAREAS, 1,
        "Fragment " PFX " caused " PFX "-" PFX
        " to cross sandbox2ro threshold %d vs %d\n",
        f->tag, written_area->start, written_area->end, ro2s->selfmod_execs,
        DYNAMO_OPTION(sandbox2ro_threshold));
    start = written_area->start;
    end = written_area->end;
    /* reset to avoid immediate re-trigger */
    ro2s->selfmod_execs = 0;

    if (is_on_stack(dcontext, f->tag, NULL)) {
        /* Naturally we cannot make the stack ro.  We checked when we built f,
         * but esp must now point elsewhere.  We go ahead and flush and assume
         * that when we rebuild f we won't put the instrumentation in. */
        convert_s2ro = false;
        STATS_INC(num_sandbox2ro_onstack);
        LOG(THREAD, LOG_VMAREAS, 1, "Fragment " PFX " is on stack now!\n", f->tag);
        ASSERT_CURIOSITY(false && "on-stack selfmod bb w/ counter inc");
    }

    if (convert_s2ro && DYNAMO_OPTION(ro2sandbox_threshold) > 0) {
        /* We'll listen to -sandbox2ro_threshold even if a selfmod region
         * didn't become that way via -ro2sandbox_threshold, to avoid perf
         * problems w/ other code in the same region, and to take advantage of
         * patterns of write at init time and then never selfmod again.
         * FIXME: have a different threshold for regions made selfmod for actual
         * self-writes versus -ro2sandbox_threshold regions?
         * If there is a written_count, we reset it so it can trigger again.
         * We reset here rather than when ro2sandbox_threshold is triggered as
         * ro2sandbox only does a page at a time and if keeping a count for
         * multiple pages doesn't want to clear that count too early.
         */
        LOG(THREAD, LOG_VMAREAS, 2,
            "re-setting written executable vm area: " PFX "-" PFX " written %d X\n",
            written_area->start, written_area->end, ro2s->written_count);
        ro2s->written_count = 0;
    }
    DOLOG(3, LOG_VMAREAS, {
        LOG(THREAD, LOG_VMAREAS, 2, "\nwritten areas:\n");
        print_vm_areas(written_areas, THREAD);
    });

    d_r_write_unlock(&written_areas->lock);

    /* Convert the selfmod region to a ro region.
     * FIXME case 8161: should we flush and make ro the executable area,
     * or the written area?  Written area may only be a page if made
     * selfmod due to a code write, but then it should match the executable area
     * in the common case, though written area may be larger if executable area
     * is from a tiny NtFlush.  If we make a sub-piece of the executable area ro,
     * the rest will remain selfmod and will eventually come here anyway.
     */
    flush_fragments_in_region_start(dcontext, start, end - start,
                                    false /* don't own initexit_lock */,
                                    false /* keep futures */, true /* exec invalid */,
                                    false /* don't force synchall */
                                    _IF_DGCDIAG(NULL));
    if (convert_s2ro) {
        DODEBUG(ro2s->s2ro_xfers++;);
        /* flush_* grabbed executable_areas lock for us */
        ok = lookup_addr(executable_areas, f->tag, &exec_area);
        if (ok) {
            if (TEST(FRAG_SELFMOD_SANDBOXED, exec_area->frag_flags)) {
                /* FIXME: if exec area is larger than flush area, it's
                 * ok since marking fragments in a ro region as selfmod
                 * is not a correctness problem.  Current flush impl, though,
                 * will flush whole region.
                 */
                vm_area_t area_copy = *exec_area; /* copy since we remove it */
                exec_area = &area_copy;
                LOG(THREAD, LOG_VMAREAS, 1,
                    "\tconverting " PFX "-" PFX " from sandbox to ro\n", exec_area->start,
                    exec_area->end);
                exec_area->frag_flags &= ~FRAG_SELFMOD_SANDBOXED;
                /* can't ASSERT(!TEST(VM_MADE_READONLY, area->vm_flags)) (case 7877) */
                vm_make_unwritable(exec_area->start, exec_area->end - exec_area->start);
                exec_area->vm_flags |= VM_MADE_READONLY;
                /* i#942: Remove the sandboxed area and re-add it to merge it
                 * back with any areas it used to be a part of.
                 */
                remove_vm_area(executable_areas, exec_area->start, exec_area->end,
                               false /* !restore_prot */);
                ok = add_executable_vm_area(exec_area->start, exec_area->end,
                                            exec_area->vm_flags, exec_area->frag_flags,
                                            true /*own lock */
                                            _IF_DEBUG("selfmod replacement"));
                ASSERT(ok);
                /* Re-do the lookup in case of merger. */
                ok = lookup_addr(executable_areas, f->tag, &exec_area);
                ASSERT(ok);
                LOG(THREAD, LOG_VMAREAS, 3,
                    "After marking " PFX "-" PFX " as NOT selfmod:\n", exec_area->start,
                    exec_area->end);
                DOLOG(3, LOG_VMAREAS, { print_vm_areas(executable_areas, THREAD); });
                STATS_INC(num_sandbox2ro);
            } else {
                /* must be a race! */
                LOG(THREAD, LOG_VMAREAS, 3,
                    "Area " PFX "-" PFX " is ALREADY not selfmod!\n", exec_area->start,
                    exec_area->end);
                STATS_INC(num_sandbox2ro_race);
            }
        } else {
            /* must be a flushing race */
            LOG(THREAD, LOG_VMAREAS, 3, "Area " PFX "-" PFX " is no longer there!\n",
                start, end);
            STATS_INC(num_sandbox2ro_flush_race);
        }
    }

    ASSERT(exec_area == NULL || /* never looked up */
           (start < exec_area->end && end > exec_area->start));

    flush_fragments_in_region_finish(dcontext, false /*don't keep initexit_lock*/);
    if (DYNAMO_OPTION(opt_jit) && is_jit_managed_area(start))
        jitopt_clear_span(start, end);
    return true;
}

void
mark_unload_start(app_pc module_base, size_t module_size)
{
    /* in thin client mode we don't allocate this,
     * but we do track unloads in -client mode
     */
    if (last_deallocated == NULL)
        return;
    ASSERT(DYNAMO_OPTION(unloaded_target_exception));
    ASSERT_CURIOSITY(!last_deallocated->unload_in_progress);
    /* we may have a race, or a thread killed during unload syscall,
     * either way we just mark our last region on top of the old one
     */
    d_r_mutex_lock(&last_deallocated_lock);
    last_deallocated->last_unload_base = module_base;
    last_deallocated->last_unload_size = module_size;
    last_deallocated->unload_in_progress = true;
    d_r_mutex_unlock(&last_deallocated_lock);
}

void
mark_unload_future_added(app_pc module_base, size_t size)
{
    /* case 9371: if a thread gets preempted before returning from
     * unmapviewofsection and in the mean time another has a _future_ exec
     * area allocated at the same place and executes from it, we should not
     * throw exception mistakenly if the area would have been allowed
     */
    if (last_deallocated == NULL)
        return;
    ASSERT(DYNAMO_OPTION(unloaded_target_exception));

    ASSERT_CURIOSITY(!last_deallocated->unload_in_progress && "future while unload");

    /* FIXME: more preciselly we should only remove our intersection
     * with the last module, otherwise don't need to, but it is never
     * expected to happen, so not optimizing at all
     */
    last_deallocated->unload_in_progress = false;
}

void
mark_unload_end(app_pc module_base)
{
    if (last_deallocated == NULL)
        return;
    ASSERT(DYNAMO_OPTION(unloaded_target_exception));

    /* We're trying to avoid a spurious security violation while we
     * are flushing our security policies, but before the address is
     * actually fully unloaded.  So if we don't have an entry in our
     * executable_areas or RAC or RCT policies then we should either
     * find the address unreadable with query_virtual_memory(), or we
     * should make sure that we find it as is_currently_unloaded_region().
     */

    /* The fact that we have reached this routine already guarantees
     * that the memory was made unreadable (whether the memory is
     * still unreadable is not guaranteed, see below).  Yet if we do
     * checks in proper order is_currently_unloaded_region() _before_
     * is_readable_without_exception(), as we do in the convenience
     * routine is_unreadable_or_currently_unloaded_region(), we can
     * get away without a barrier here.
     */

    /* FIXME: Otherwise we'd need a barrier, such that until a security
     * policy reader is done, we cannot mark the module as unloaded,
     * and if they start doing their check after this - then they
     * should get a policy consistent with the memory already being
     * unreadable.  (For example, we can synchronize with
     * check_thread_vm_area() via
     * {executable_areas_lock();executable_areas_unlock()} but since
     * all other policies have sufficient information from unreadable
     * memory, we're OK with a DLL being completely unloaded.
     */

    /* FIXME: note we may want to grab the appropriate policy locks so
     * that we can thus delay our declaring we're no longer unloading
     * a module until the policy processing is done, e.g. if one has
     * started querying a security policy while we are unloading, we
     * should preserve the marker until they are done.
     * for .B we hold a writable executable_areas_lock(),
     * watch out here if for case 9371 we want to also mark_unload_end()
     * on any new allocations
     * FIXME: the RCT policies however we don't hold a lock.
     */

    /* FIXME: case 9372 Note that we may still have a problem primarily if a DLL
     * gets subsequently reloaded at the same location, (so we have
     * lost our flag) so after a time in which we make our checks
     * whether the target is unreadable, the new version will show up
     * and may not yet be fully processed in postsys_MapViewOfSection
     * (and even if it is, we may have already checked our
     * policies). I assume this should be less frequent than the
     * unload side (although it still shows up in our
     * win32/reload-race.c).  At least not a problem if the DLL gets
     * reloaded at a different address, like case 9121 or with -aslr 1
     */

    /* note grabbing this lock is only useful for the ASSERTs, setting
     * the flag is atomic even without it.
     * is_unreadable_or_currently_unloaded_region() when used in
     * proper order doesn't need to synchronize with this lock either */
    d_r_mutex_lock(&last_deallocated_lock);

    /* note, we mark_unload_start on MEM_IMAGE but mark_unload_end on
     * MEM_MAPPED as well.  Note base doesn't have to match as long as
     * it is within the module */
    ASSERT_CURIOSITY(!last_deallocated->unload_in_progress ||
                     ((last_deallocated->last_unload_base <= module_base &&
                       module_base < (last_deallocated->last_unload_base +
                                      last_deallocated->last_unload_size)) &&
                      "race - multiple unmaps"));
    DOLOG(1, LOG_VMAREAS, {
        /* there are a few cases where DLLs aren't unloaded by real
         * base uxtheme.dll, but I haven't seen them */
        ASSERT_CURIOSITY(
            !last_deallocated->unload_in_progress ||
            (last_deallocated->last_unload_base == module_base && "not base"));
    });

    /* multiple racy unmaps can't be handled simultaneously anyways */
    last_deallocated->unload_in_progress = false;
    d_r_mutex_unlock(&last_deallocated_lock);
}

bool
is_in_last_unloaded_region(app_pc pc)
{
    bool in_last = true;
    if (last_deallocated == NULL)
        return false;
    ASSERT(DYNAMO_OPTION(unloaded_target_exception));

    d_r_mutex_lock(&last_deallocated_lock);
    /* if we are in such a tight race that we're no longer
     * last_deallocated->unload_in_progress we can still use the
     * already unloaded module
     */
    if ((pc < last_deallocated->last_unload_base) ||
        (pc >= (last_deallocated->last_unload_base + last_deallocated->last_unload_size)))
        in_last = false;
    d_r_mutex_unlock(&last_deallocated_lock);
    return in_last;
}

static bool
is_currently_unloaded_region(app_pc pc)
{
    if (last_deallocated == NULL)
        return false;
    ASSERT(DYNAMO_OPTION(unloaded_target_exception));

    if (!last_deallocated->unload_in_progress)
        return false;

    return is_in_last_unloaded_region(pc);
}

bool
is_unreadable_or_currently_unloaded_region(app_pc pc)
{
    /* we want one atomic query - so if we are before the completion
     * of the UnMap system call we should be
     * is_currently_unloaded_region(), but afterwards the address
     * should be !is_readable_without_exception
     */
    /* order of execution is important - so that we don't have to grab
     * a lock to synchronize with mark_unload_end().
     */

    if (is_currently_unloaded_region(pc)) {
        STATS_INC(num_unloaded_race);
        return true;
    }
    /* If we are not in a currently unloaded module then target is
     * either not being unloaded or we are beyond system call.
     */
    if (!is_readable_without_exception(pc, 1)) {
        return true;
    }
    return false;
}

void
print_last_deallocated(file_t outf)
{
    if (last_deallocated == NULL)
        return;

    ASSERT(DYNAMO_OPTION(unloaded_target_exception));
    if (last_deallocated->last_unload_base == NULL) {
        print_file(outf, "never unloaded\n");
        return;
    }

    print_file(outf, "last unload: " PFX "-" PFX "%s\n",
               last_deallocated->last_unload_base,
               last_deallocated->last_unload_base + last_deallocated->last_unload_size,
               last_deallocated->unload_in_progress ? " being unloaded" : "");
}

#ifdef PROGRAM_SHEPHERDING
/* Note that rerouting an APC to this target should safely popup the arguments
 * and continue.
 *
 * Since ThreadProc and APCProc have the same signature, we handle a
 * remote thread in a similar way, instead of letting attack handling
 * decide its fate - which may be an exception instead of killing the
 * thread.
 *
 * FIXME: we're interpreting dynamorio.dll code here
 */
/* FIXME clean up: safe_apc_or_thread_target, apc_thread_policy_helper and
 * aslr_report_violation should all be ifdef WINDOWS, and may be in a
 * different file
 */
/* could do naked to get a single RET 4 emitted with no prologue */
void APC_API
safe_apc_or_thread_target(reg_t arg)
{
    /* NOTHING */
}
/* FIXME: case 9023: this is WRONG for NATIVE APCs!
 * kernel32!BaseDispatchAPC+0x33:
 * 7c82c13a c20c00           ret     0xc
 * FIXME: add safe_native_apc(PVOID context, PAPCFUNC func, reg_t arg)
 */

/* a helper procedure for DYNAMO_OPTION(apc_policy) or DYNAMO_OPTION(thread_policy)
 *
 * FIXME: currently relevant only on WINDOWS
 */
void
apc_thread_policy_helper(app_pc *apc_target_location, /* IN/OUT */
                         security_option_t target_policy, apc_thread_type_t target_type)
{
    bool is_apc =
        (target_type == APC_TARGET_NATIVE) || (target_type == APC_TARGET_WINDOWS);
    /* if is_win32api we're evaluating the Win32 API targets of
     * QueueUserAPC/CreateThreadEx, otherwise it is the native
     * NtQueueApcThread/NtCreateThreadEx targets
     */
    bool is_win32api =
        (target_type == THREAD_TARGET_WINDOWS) || (target_type == APC_TARGET_WINDOWS);

    bool match = false;
    /* FIXME: note taking the risk here of reading from either the
     * word on the stack, or from a Cxt.  While the app would fail in
     * either case this should be safer.  I don't want the extra
     * is_readable_without_exception() here though.
     */
    app_pc injected_target = *apc_target_location;
    uint injected_code = 0; /* first bytes of shellcode */

    /* match PIC shellcode header, for example
     * 0013004c 53               push    ebx
     * 0013004d e800000000       call    00130052
     */
    enum { PIC_SHELLCODE_MATCH = 0x0000e853 };

    /* Now we quickly check a stipped down code origins policy instead
     * of letting the bb builder do this.  ALTERNATIVE design: We could save
     * the target and have this extra work done only after a code
     * origins violations.  Then we would not modify application state
     * unnecessarily.  The problem however is that we need to make
     * sure we do that only _immediately_ after an APC.
     */

    /* using only executable area - assuming areas added by
     * -executable_if_x are only added to futureexec_areas, so that
     * this test can be done and acted upon independently of us
     * running in NX compatibility
     */
    if (is_executable_address(injected_target)) {
        return; /* not a match */
    }

    if (d_r_safe_read(injected_target, sizeof(injected_code), &injected_code)) {
        LOG(GLOBAL, LOG_ASYNCH, 2,
            "ASYNCH intercepted APC: APC pc=" PFX ", APC code=" PFX " %s\n",
            injected_target, injected_code,
            injected_code == PIC_SHELLCODE_MATCH ? "MATCH" : "");
    } else {
        ASSERT_NOT_TESTED();
    }

    /* target is a non-executable area, but we may want to be more specific */
    if (TEST(OPTION_CUSTOM, target_policy)) {
        match = true; /* no matter what is in the shellcode */
    } else {
        if (injected_code == PIC_SHELLCODE_MATCH)
            match = true;
    }

    if (match) {
        bool squashed = false;
        char injected_threat_buf[MAXIMUM_VIOLATION_NAME_LENGTH] = "APCS.XXXX.B";
        const char *name = injected_threat_buf;

        bool block = TEST(OPTION_BLOCK, target_policy);

        /* we need the constructed name before deciding to really
         * block, in case we exempt by ID */
        if (TEST(OPTION_REPORT, target_policy)) {
            /* mangle injected_code into a name */
            if (injected_code == PIC_SHELLCODE_MATCH) {
                /* keeping the well known hardcoded ones for VSE */
                name = is_apc ? "VVPP.3200.B" : "YCRP.3200.B";
            } else {
                /* FIXME: nativs vs non-native could get a different prefix as well */
                if (!is_apc) {
                    const char *INJT = "INJT";
                    /* (injected) shellcode thread */
                    ASSERT_NOT_TESTED();
                    /* gcc warns if we use the string "INJT" directly */
                    strncpy(injected_threat_buf, INJT, 4);
                }
                fill_security_violation_target(injected_threat_buf,
                                               (const byte *)&injected_code);
            }

            /* we allow -exempt_threat_list to override our action */
            if (!IS_STRING_OPTION_EMPTY(exempt_threat_list)) {
                if (is_exempt_threat_name(name)) {
                    /* we want to ALLOW unconditionally so we don't
                     * immediately get a regular .B violation after we
                     * let it through the APC check
                     */
                    block = false;
                }
                /* FIXME: we don't have a good way to express allow
                 * everyone except for the ones on this list while we
                 * could say block = !block that doesn't match the
                 * general meaning of exempt_threat_list
                 */
            }
        }

        if (block) {
            /* always using custom attack handling */
            /* We cannot let default attack handling take care
             * of this because a main thread may get affected very early.
             *
             * It is also hard to reuse security_violation() call here
             * (since we are not under d_r_dispatch()).  If we want to see a
             * code origins failure, we can just disable this policy.
             */
            ASSERT(!TEST(OPTION_HANDLING, target_policy) &&
                   "handling cannot be modified");

            SYSLOG_INTERNAL_WARNING(
                "squashed %s %s at bad target pc=" PFX " %s", is_apc ? "APC" : "thread",
                is_win32api ? "win32" : "native", injected_target, name);

            /* FIXME: case 9023 : should squash appropriately native
             * vs non-native since the number of arguments may be
             * different, hence stdcall RET size
             */
            *apc_target_location = is_win32api ? (app_pc)safe_apc_or_thread_target
                                               : (app_pc)safe_apc_or_thread_target;

            squashed = true;
        } else {
            /* allow */
            app_pc base = (app_pc)PAGE_START(injected_target);
            SYSLOG_INTERNAL_WARNING(
                "allowing %s %s at bad target pc=" PFX " %s", is_apc ? "APC" : "thread",
                is_win32api ? "win32" : "native", injected_target, name);

            /* FIXME: for HIGH mode, unfortunately the target code
             * may be selfmod, so adding a hook-style policy is hard.
             */
            /* FIXME: It looks like in VirusScan (case 2871) they
             * eventually free this memory, so not that bad hole.
             * Although I haven't found how would they properly
             * synchronize that entapi.dll is loaded. */

            /* we can't safely determine a subpage region so adding whole page */
            add_futureexec_vm_area(base, base + PAGE_SIZE,
                                   false /*permanent*/
                                   _IF_DEBUG(is_apc ? "apc_helper" : "thread_policy"));
        }

        if (TEST(OPTION_REPORT, target_policy)) {
            /* report a violation adjusted for appropriate action */
            /* FIXME: should come up with a new name for this
             * violation otherwise it is pretty inconsistent to say
             * running in detect mode and -B policies
             */
            /* note that we may not actually report if silent_block_threat_list */
            security_violation_report(
                injected_target, APC_THREAD_SHELLCODE_VIOLATION, name,
                squashed ? ACTION_TERMINATE_THREAD : ACTION_CONTINUE);
        }

        DOSTATS({
            if (is_apc)
                STATS_INC(num_used_apc_policy);
            else
                STATS_INC(num_used_thread_policy);
        });
    }
}

/* a helper procedure for reporting ASLR violations */
void
aslr_report_violation(app_pc execution_fault_pc, security_option_t handling_policy)
{
    STATS_INC(aslr_wouldbe_exec);

    /* note OPTION_BLOCK has to be set since there is nothing we can
     * do to not block the attack, there is no detect mode here, yet
     * we let the original exception be passed.  For default
     * applications where ASLR can be hit natively, the attack
     * handling policy is to throw an exception.
     */
    ASSERT(TEST(OPTION_BLOCK, handling_policy));

    /* FIXME: yet we should have a choice whether to override the
     * exception that would normally be delivered to the application,
     * with a -kill_thread or -kill_process in case the SEH chain is
     * corrupt, and to allow the attack handling thresholds to take
     * effect.
     */
    ASSERT(!TEST(OPTION_HANDLING, handling_policy));
    /* FIXME: if using report security_violation() to provide attack
     * handling decisions should make sure it prefers exceptions,
     * FIXME: make sure not trying to release locks, FIXME: also clean
     * kstats (currently hotp_only is already broken)
     */

    ASSERT(!TEST(OPTION_CUSTOM, handling_policy));

    if (TEST(OPTION_REPORT, handling_policy)) {
        /* report a violation, adjusted for appropriate action */
        char aslr_threat_id[MAXIMUM_VIOLATION_NAME_LENGTH];

        /* in -hotp_only mode cannot have the regular distinction
         * between stack and heap targets (usually marked as .A and
         * .B), instead marking all as the same .R violation.
         */
        security_violation_t aslr_violation_type = ASLR_TARGET_VIOLATION;

        /* source cannot be obtained */
        /* FIXME: case 8160 on possibly setting the source to something useful */

        /* FIXME: target is currently unreadable, forensic and Threat
         * ID generation will adjust to a likely current mapping to
         * print its contents
         */
        dcontext_t *dcontext = get_thread_private_dcontext();

        /* should be in hotp_only */
        ASSERT(dcontext != NULL && dcontext->last_fragment != NULL &&
               dcontext->last_fragment->tag == NULL);

        /* note we clobber next_tag here, not bothering to preserve */
        /* report_dcontext_info() uses next_tag for target (and
         * preferred target) diagnostics */
        dcontext->next_tag = execution_fault_pc;

        /* if likely_target_pc is unreadable (and it should be)
         * get_security_violation_name will use as target the contents
         * of a likely would be target */
        get_security_violation_name(dcontext, execution_fault_pc, aslr_threat_id,
                                    MAXIMUM_VIOLATION_NAME_LENGTH, aslr_violation_type,
                                    NULL);
        security_violation_report(execution_fault_pc, aslr_violation_type, aslr_threat_id,
                                  ACTION_THROW_EXCEPTION);
    }
}
#endif /* PROGRAM_SHEPHERDING */

#ifdef STANDALONE_UNIT_TEST
#    define INT_TO_PC(x) ((app_pc)(ptr_uint_t)(x))

static void
print_vector_msg(vm_area_vector_t *v, file_t f, const char *msg)
{
    print_file(f, "%s:\n", msg);
    print_vm_areas(v, f);
}

static void
check_vec(vm_area_vector_t *v, int i, app_pc start, app_pc end, uint vm_flags,
          uint frag_flags, void *data)
{
    ASSERT(i < v->length);
    ASSERT(v->buf[i].start == start);
    ASSERT(v->buf[i].end == end);
    ASSERT(v->buf[i].vm_flags == vm_flags);
    ASSERT(v->buf[i].frag_flags == frag_flags);
    ASSERT(v->buf[i].custom.client == data);
}

void
vmvector_tests()
{
    vm_area_vector_t v = { 0, 0, 0, VECTOR_SHARED | VECTOR_NEVER_MERGE,
                           INIT_READWRITE_LOCK(thread_vm_areas) };
    bool res;
    app_pc start = NULL, end = NULL;
    print_file(STDERR, "\nvm_area_vector_t tests\n");
    /* FIXME: not tested */
    vmvector_add(&v, INT_TO_PC(0x100), INT_TO_PC(0x103), NULL);
    vmvector_add(&v, INT_TO_PC(0x200), INT_TO_PC(0x203), NULL);
    vmvector_print(&v, STDERR);
#    if 0 /* this raises no-merge assert: no mechanism to test that it fires though */
    vmvector_add(&v, INT_TO_PC(0x202), INT_TO_PC(0x210), NULL); /* should complain */
#    endif
    vmvector_add(&v, INT_TO_PC(0x203), INT_TO_PC(0x221), NULL);
    vmvector_print(&v, STDERR);
    check_vec(&v, 2, INT_TO_PC(0x203), INT_TO_PC(0x221), 0, 0, NULL);

    res = vmvector_remove_containing_area(&v, INT_TO_PC(0x103), NULL, NULL); /* not in */
    EXPECT(res, false);
    check_vec(&v, 0, INT_TO_PC(0x100), INT_TO_PC(0x103), 0, 0, NULL);
    res = vmvector_remove_containing_area(&v, INT_TO_PC(0x100), NULL, &end);
    EXPECT(end, 0x103);
    EXPECT(res, true);
    vmvector_print(&v, STDERR);
    check_vec(&v, 0, INT_TO_PC(0x200), INT_TO_PC(0x203), 0, 0, NULL);
    res = vmvector_remove_containing_area(&v, INT_TO_PC(0x100), NULL, NULL); /* not in */
    EXPECT(res, false);
    vmvector_print(&v, STDERR);
    res = vmvector_remove_containing_area(&v, INT_TO_PC(0x202), &start, NULL);
    EXPECT(res, true);
    EXPECT(start, 0x200);
    vmvector_print(&v, STDERR);
    res =
        vmvector_remove(&v, INT_TO_PC(0x20), INT_TO_PC(0x210)); /* truncation allowed? */
    EXPECT(res, true);
    vmvector_print(&v, STDERR);
}

/* initial vector tests
 * FIXME: should add a lot more, esp. wrt other flags -- these only
 * test no flags or interactions w/ selfmod flag
 */
void
unit_test_vmareas(void)
{
    vm_area_vector_t v = { 0, 0, 0, false };
    /* not needed yet: dcontext_t *dcontext = */
    ASSIGN_INIT_READWRITE_LOCK_FREE(v.lock, thread_vm_areas);

    /* TEST 1: merge a bunch of areas
     */
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(3), 0, 0, NULL _IF_DEBUG("A"));
    add_vm_area(&v, INT_TO_PC(5), INT_TO_PC(7), 0, 0, NULL _IF_DEBUG("B"));
    add_vm_area(&v, INT_TO_PC(9), INT_TO_PC(11), 0, 0, NULL _IF_DEBUG("C"));
    print_vector_msg(&v, STDERR, "after adding areas");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(3), 0, 0, NULL);
    check_vec(&v, 1, INT_TO_PC(5), INT_TO_PC(7), 0, 0, NULL);
    check_vec(&v, 2, INT_TO_PC(9), INT_TO_PC(11), 0, 0, NULL);

    add_vm_area(&v, INT_TO_PC(0), INT_TO_PC(12), 0, 0, NULL _IF_DEBUG("D"));
    print_vector_msg(&v, STDERR, "after merging with D");
    check_vec(&v, 0, INT_TO_PC(0), INT_TO_PC(12), 0, 0, NULL);

    /* clear for next test */
    remove_vm_area(&v, INT_TO_PC(0), UNIVERSAL_REGION_END, false);
    print_file(STDERR, "\n");

    /* TEST 2: add an area that covers several smaller ones, including one
     * that cannot be merged
     */
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(3), 0, 0, NULL _IF_DEBUG("A"));
    add_vm_area(&v, INT_TO_PC(5), INT_TO_PC(7), 0, FRAG_SELFMOD_SANDBOXED,
                NULL _IF_DEBUG("B"));
    add_vm_area(&v, INT_TO_PC(9), INT_TO_PC(11), 0, 0, NULL _IF_DEBUG("C"));
    print_vector_msg(&v, STDERR, "after adding areas");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(3), 0, 0, NULL);
    check_vec(&v, 1, INT_TO_PC(5), INT_TO_PC(7), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 2, INT_TO_PC(9), INT_TO_PC(11), 0, 0, NULL);

    add_vm_area(&v, INT_TO_PC(2), INT_TO_PC(10), 0, 0, NULL _IF_DEBUG("D"));
    print_vector_msg(&v, STDERR, "after merging with D");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(5), 0, 0, NULL);
    check_vec(&v, 1, INT_TO_PC(5), INT_TO_PC(7), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 2, INT_TO_PC(7), INT_TO_PC(11), 0, 0, NULL);

    remove_vm_area(&v, INT_TO_PC(6), INT_TO_PC(8), false);
    print_vector_msg(&v, STDERR, "after removing 6-8");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(5), 0, 0, NULL);
    check_vec(&v, 1, INT_TO_PC(5), INT_TO_PC(6), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 2, INT_TO_PC(8), INT_TO_PC(11), 0, 0, NULL);

    /* clear for next test */
    remove_vm_area(&v, INT_TO_PC(0), UNIVERSAL_REGION_END, false);
    print_file(STDERR, "\n");

    /* TEST 3: add an area that covers several smaller ones, including two
     * that cannot be merged
     */
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(3), 0, FRAG_SELFMOD_SANDBOXED,
                NULL _IF_DEBUG("A"));
    add_vm_area(&v, INT_TO_PC(5), INT_TO_PC(7), 0, FRAG_SELFMOD_SANDBOXED,
                NULL _IF_DEBUG("B"));
    add_vm_area(&v, INT_TO_PC(9), INT_TO_PC(11), 0, 0, NULL _IF_DEBUG("C"));
    print_vector_msg(&v, STDERR, "after adding areas");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(3), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 1, INT_TO_PC(5), INT_TO_PC(7), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 2, INT_TO_PC(9), INT_TO_PC(11), 0, 0, NULL);

    add_vm_area(&v, INT_TO_PC(2), INT_TO_PC(12), 0, 0, NULL _IF_DEBUG("D"));
    print_vector_msg(&v, STDERR, "after merging with D");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(3), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 1, INT_TO_PC(3), INT_TO_PC(5), 0, 0, NULL);
    check_vec(&v, 2, INT_TO_PC(5), INT_TO_PC(7), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 3, INT_TO_PC(7), INT_TO_PC(12), 0, 0, NULL);

    remove_vm_area(&v, INT_TO_PC(2), INT_TO_PC(11), false);
    print_vector_msg(&v, STDERR, "after removing 2-11");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(2), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 1, INT_TO_PC(11), INT_TO_PC(12), 0, 0, NULL);

    /* FIXME: would be nice to be able to test that an assert is generated...
     * say, for this:
     * add_vm_area(&v, INT_TO_PC(7), INT_TO_PC(12), 0, FRAG_SELFMOD_SANDBOXED, NULL
     *             _IF_DEBUG("E"));
     */

    /* clear for next test */
    remove_vm_area(&v, INT_TO_PC(0), UNIVERSAL_REGION_END, false);
    print_file(STDERR, "\n");

    /* TEST 4: add an area completely inside one that cannot be merged
     */
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(5), 0, FRAG_SELFMOD_SANDBOXED,
                NULL _IF_DEBUG("A"));
    print_vector_msg(&v, STDERR, "after adding areas");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(5), 0, FRAG_SELFMOD_SANDBOXED, NULL);

    add_vm_area(&v, INT_TO_PC(3), INT_TO_PC(4), 0, 0, NULL _IF_DEBUG("B"));
    print_vector_msg(&v, STDERR, "after merging with B");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(5), 0, FRAG_SELFMOD_SANDBOXED, NULL);

    /* clear for next test */
    remove_vm_area(&v, INT_TO_PC(0), UNIVERSAL_REGION_END, false);
    print_file(STDERR, "\n");

    /* TEST 5: Test merging adjacent areas.
     */
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(2), 0, 0, NULL _IF_DEBUG("A"));
    add_vm_area(&v, INT_TO_PC(2), INT_TO_PC(3), 0, 0, NULL _IF_DEBUG("B"));
    add_vm_area(&v, INT_TO_PC(3), INT_TO_PC(4), 0, 0, NULL _IF_DEBUG("C"));
    print_vector_msg(&v, STDERR, "do areas merge");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(4), 0, 0, NULL);

    remove_vm_area(&v, INT_TO_PC(1), INT_TO_PC(4), false);
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(2), 0, 0, NULL _IF_DEBUG("A"));
    add_vm_area(&v, INT_TO_PC(2), INT_TO_PC(3), 0, FRAG_SELFMOD_SANDBOXED,
                NULL _IF_DEBUG("B"));
    add_vm_area(&v, INT_TO_PC(3), INT_TO_PC(4), 0, 0, NULL _IF_DEBUG("C"));
    print_vector_msg(&v, STDERR, "do areas merge with flags");
    check_vec(&v, 0, INT_TO_PC(1), INT_TO_PC(2), 0, 0, NULL);
    check_vec(&v, 1, INT_TO_PC(2), INT_TO_PC(3), 0, FRAG_SELFMOD_SANDBOXED, NULL);
    check_vec(&v, 2, INT_TO_PC(3), INT_TO_PC(4), 0, 0, NULL);
    remove_vm_area(&v, INT_TO_PC(0), UNIVERSAL_REGION_END, false);

    /* TEST 6: Binary search.
     */
    add_vm_area(&v, INT_TO_PC(1), INT_TO_PC(3), 0, 0, NULL _IF_DEBUG("A"));
    add_vm_area(&v, INT_TO_PC(4), INT_TO_PC(5), 0, 0, NULL _IF_DEBUG("B"));
    add_vm_area(&v, INT_TO_PC(7), INT_TO_PC(9), 0, 0, NULL _IF_DEBUG("C"));
    vm_area_t *container = NULL;
    int index = -1;
    bool found = binary_search(&v, INT_TO_PC(2), INT_TO_PC(3), &container, &index, true);
    EXPECT(found, true);
    EXPECT(container->start, 1);
    EXPECT(container->end, 3);
    EXPECT(index, 0);
    found = binary_search(&v, INT_TO_PC(6), INT_TO_PC(7), &container, &index, true);
    EXPECT(found, false);
    EXPECT(index, 1);
    /* Test start==end. */
    found = binary_search(&v, INT_TO_PC(8), INT_TO_PC(8), &container, &index, true);
    EXPECT(found, false);
    EXPECT(index, 2);
    /* Test wraparound searching to NULL (i#4097). */
    found = binary_search(&v, INT_TO_PC(1), INT_TO_PC(0), &container, &index, true);
    EXPECT(found, true);
    EXPECT(index, 0);
    found = binary_search(&v, container->end, INT_TO_PC(0), &container, &index, true);
    EXPECT(found, true);
    EXPECT(index, 1);
    found = binary_search(&v, container->end, INT_TO_PC(0), &container, &index, true);
    EXPECT(found, true);
    EXPECT(index, 2);
    found = binary_search(&v, container->end, INT_TO_PC(0), &container, &index, true);
    EXPECT(found, false);
    EXPECT(index, 2);

    vmvector_tests();
}
#endif /* STANDALONE_UNIT_TEST */
