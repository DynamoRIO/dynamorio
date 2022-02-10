/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
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
 * vmareas.h - virtual memory areas
 */

#ifndef _VMAREAS_H_
#define _VMAREAS_H_ 1

/* case 9750: use these constants to specify empty and all-encompassing address
 * space regions for flushing, etc.
 */
#define EMPTY_REGION_BASE ((app_pc)PTR_UINT_1) /* avoid [0,0) == wrapped universal */
#define EMPTY_REGION_SIZE ((size_t)0U)
#define UNIVERSAL_REGION_BASE ((app_pc)NULL)
/* really open-ended should have this 1 larger */
#define UNIVERSAL_REGION_SIZE POINTER_MAX
/* really open-ended would make this wrap around to 0 */
#define UNIVERSAL_REGION_END ((app_pc)POINTER_MAX)

/* opaque struct */
struct vm_area_t;

enum {
    /* these are bitmask flags */
    VECTOR_SHARED = 0x0001,
    VECTOR_FRAGMENT_LIST = 0x0002, /* for vmareas.c-only use */
    /* never merge adjacent regions */
    VECTOR_NEVER_MERGE_ADJACENT = 0x0004,
    /* results in an assert if a new region overlaps existing */
    VECTOR_NEVER_OVERLAP = 0x0008,
    /* case 10335: if a higher-level lock is being used, set this
     * flag to avoid the redundant vector-level lock
     */
    VECTOR_NO_LOCK = 0x0010,
};

#define VECTOR_NEVER_MERGE (VECTOR_NEVER_MERGE_ADJACENT | VECTOR_NEVER_OVERLAP)

/* This vector data structure is only exposed here for quick length checks.
 * For external (non-vmareas.c) users, the vmvector_* interface is the
 * preferred way of manipulating vectors.
 *
 * Each vector is kept sorted by area.  Since there are no overlaps allowed
 * among areas in the same vector (they're merged to preserve that), sorting
 * by start_pc or by end_pc produce identical results.
 */
struct vm_area_vector_t {
    struct vm_area_t *buf;
    int size; /* capacity */
    int length;
    uint flags; /* VECTOR_* flags */
    /* often thread-shared, so needs a lock
     * read-write lock for performance, and to allow a high-level writer
     * to perform a read (don't need full recursive lock)
     */
    read_write_lock_t lock;

    /* Callbacks to support payloads */
    /* Frees a payload */
    void (*free_payload_func)(void *);
    /* Returns the payload to use for a new region split from the given data's
     * region.
     */
    void *(*split_payload_func)(void *);
    /* Should adjacent/overlapping regions w/ given payloads be merged?
     * If it returns false, adjacent regions are not merged, and
     * a new overlapping region is split (the split_payload_func is
     * called) and only nonoverlapping pieces are added.
     * If NULL, it is assumed to return true for adjacent but false
     * for overlapping.
     * The VECTOR_NEVER_MERGE_ADJACENT flag takes precedence over this function.
     */
    bool (*should_merge_func)(bool adjacent, void *, void *);
    /* Merge adjacent or overlapping regions: dst is first arg.
     * If NULL, the free_payload_func will be called for src.
     * If non-NULL, the free_payload_func will NOT be called.
     */
    void *(*merge_payload_func)(void *dst, void *src);
}; /* typedef-ed in globals.h */

/* vm_area_vectors should NOT be declared statically if their locks need to be
 * accessed on a regular basis.  Instead, allocate them on the heap with this macro:
 */
#define VMVECTOR_ALLOC_VECTOR(v, dc, flags, lockname)             \
    do {                                                          \
        v = vmvector_create_vector((dc), (flags));                \
        if (!TEST(VECTOR_NO_LOCK, (flags)))                       \
            ASSIGN_INIT_READWRITE_LOCK_FREE((v)->lock, lockname); \
    } while (0);

/* iterator over a vm_area_vector_t */
typedef struct vmvector_iterator_t {
    vm_area_vector_t *vector;
    int index;
} vmvector_iterator_t;

/* rather than exporting specialized routines for just these vectors we
 * export the vectors and general routines
 */
extern vm_area_vector_t *emulate_write_areas;

extern vm_area_vector_t *patch_proof_areas;

/* IAT or GOT areas of all mapped DLLs except for those in
 * native_exec_areas - note the exact regions are added here */
extern vm_area_vector_t *IAT_areas;

extern mutex_t shared_delete_lock;

/* operations on the opaque vector struct */
void
vmvector_set_callbacks(vm_area_vector_t *v, void (*free_func)(void *),
                       void *(*split_func)(void *),
                       bool (*should_merge_func)(bool, void *, void *),
                       void *(*merge_func)(void *, void *));

void
vmvector_print(vm_area_vector_t *v, file_t outf);

void
vmvector_add(vm_area_vector_t *v, app_pc start, app_pc end, void *data);

/* Returns the old data, or NULL if no prior area [start,end) */
void *
vmvector_add_replace(vm_area_vector_t *v, app_pc start, app_pc end, void *data);

bool
vmvector_remove(vm_area_vector_t *v, app_pc start, app_pc end);

bool
vmvector_remove_containing_area(vm_area_vector_t *v, app_pc pc,
                                app_pc *area_start /* OUT optional */,
                                app_pc *area_end /* OUT optional */);

static inline bool
vmvector_empty(vm_area_vector_t *v)
{
    ASSERT(v != NULL);
    if (v == NULL) /* defensive programming */
        return true;
    else
        return (v->length == 0);
}

bool
vmvector_overlap(vm_area_vector_t *v, app_pc start, app_pc end);

void *
vmvector_lookup(vm_area_vector_t *v, app_pc pc);

/* this routine does NOT initialize the rw lock!  use VMVECTOR_INITIALIZE_VECTOR */
void
vmvector_init_vector(vm_area_vector_t *v, uint flags);

/* this routine does NOT initialize the rw lock!  use VMVECTOR_ALLOC_VECTOR instead */
vm_area_vector_t *
vmvector_create_vector(dcontext_t *dcontext, uint flags);

bool
vmvector_lookup_data(vm_area_vector_t *v, app_pc pc, app_pc *start, app_pc *end,
                     void **data);

/* Returns false if pc is in a vmarea in v.  Otherwise, returns the bounds of the
 * vmarea prior to pc in [prev_start,prev_end) (both NULL if none) and the bounds of
 * the vmarea after pc in [next_start,next_end) (both POINTER_MAX if none).
 */
bool
vmvector_lookup_prev_next(vm_area_vector_t *v, app_pc pc, OUT app_pc *prev_start,
                          OUT app_pc *prev_end, OUT app_pc *next_start,
                          OUT app_pc *next_end);

bool
vmvector_modify_data(vm_area_vector_t *v, app_pc start, app_pc end, void *data);

void
vmvector_delete_vector(dcontext_t *dcontext, vm_area_vector_t *v);

void
vmvector_reset_vector(dcontext_t *dcontext, vm_area_vector_t *v);

/* vmvector iterator.  Initialized with vmvector_iterator_start(), and
 * destroyed with vmvector_iterator_stop() to release vector lock.
 * Accessor vmvector_iterator_next() should be called only when
 * predicate vmvector_iterator_hasnext() is true.  No add/remove
 * mutations are allowed, although only shared vectors will detect that.
 */
void
vmvector_iterator_start(vm_area_vector_t *v, vmvector_iterator_t *vmvi);
/* Remove mutations can be used with this routine when removing everything */
void
vmvector_iterator_startover(vmvector_iterator_t *vmvi);
bool
vmvector_iterator_hasnext(vmvector_iterator_t *vmvi);
/* does not increment the iterator */
void *
vmvector_iterator_peek(vmvector_iterator_t *vmvi, /* IN/OUT */
                       app_pc *area_start /* OUT */, app_pc *area_end /* OUT */);
void *
vmvector_iterator_next(vmvector_iterator_t *vmvi, /* IN/OUT */
                       app_pc *area_start /* OUT */, app_pc *area_end /* OUT */);
void
vmvector_iterator_stop(vmvector_iterator_t *vmvi);

/* called earlier than vm_areas_init() to allow add_dynamo_vm_area to be called */
void
dynamo_vm_areas_init(void);

void
dynamo_vm_areas_exit(void);

/* initialize per process */
int
vm_areas_init(void);

/* cleanup and print statistics per process */
int
vm_areas_exit(void);

/* post cleanup to support reattach */
void
vm_areas_post_exit(void);

/* thread-shared initialization that should be repeated after a reset */
void
vm_areas_reset_init(void);

/* Free all thread-shared state not critical to forward progress;
 * vm_areas_reset_init() will be called before continuing.
 */
void
vm_areas_reset_free(void);

void
vm_area_coarse_units_reset_free(void);

void
vm_area_coarse_units_freeze(bool in_place);

/* print list of all currently allowed executable areas */
void
print_executable_areas(file_t outf);

#ifdef PROGRAM_SHEPHERDING
/* print list of all future-exec areas */
void
print_futureexec_areas(file_t outf);
#endif

/* print list of all current dynamo areas */
void
print_dynamo_areas(file_t outf);

/* initialize per thread */
void
vm_areas_thread_init(dcontext_t *dcontext);

/* cleanup per thread */
void
vm_areas_thread_exit(dcontext_t *dcontext);

/* re-initializes non-persistent memory */
void
vm_areas_thread_reset_init(dcontext_t *dcontext);

/* frees all non-persistent memory */
void
vm_areas_thread_reset_free(dcontext_t *dcontext);

/* lookup an address against the per-process executable map */
bool
is_executable_address(app_pc addr);

/* if addr is an executable area, returns true and returns in *flags
 *   any FRAG_ flags associated with addr's vm area
 * returns false if area not found
 */
bool
get_executable_area_flags(app_pc addr, uint *frag_flags);

/* returns any VM_ flags associated with addr's vm area
 * returns 0 if no area is found
 */
bool
get_executable_area_vm_flags(app_pc addr, uint *vm_flags);

/* For coarse-grain operation, we use a separate cache and htable per region.
 * See coarse_info_t notes on synchronization model.
 */
coarse_info_t *
get_executable_area_coarse_info(app_pc addr);

/* Marks the executable_areas region corresponding to info as frozen. */
void
mark_executable_area_coarse_frozen(coarse_info_t *info);

/* returns true if addr is on a page that was marked writable by the application
 * but that we marked RO b/c it contains executable code
 * does NOT check if addr is executable, only that something on its page is!
 */
bool
is_executable_area_writable(app_pc addr);

app_pc
is_executable_area_writable_overlap(app_pc start, app_pc end);

/* combines is_executable_area_writable and is_pretend_writable_address */
bool
is_pretend_or_executable_writable(app_pc addr);

bool
was_executable_area_writable(app_pc addr);

/* returns false if addr is not in an executable area that
 * contains self-modifying code */
bool
is_executable_area_selfmod(app_pc addr);

#ifdef DGC_DIAGNOSTICS
/* returns false if addr is not in an executable area marked as dyngen */
bool
is_executable_area_dyngen(app_pc addr);
#endif

#ifdef PROGRAM_SHEPHERDING
/* returns true if addr is in the dyngen syscall area of XP+/RH9+ */
bool
is_dyngen_vsyscall(app_pc addr);

/* returns true if addr is a DGC,
   Note either executed or would be allowed in the future */
bool
is_dyngen_code(app_pc addr);

bool
is_in_futureexec_area(app_pc addr);
#endif

/* lookup an address against the per-process map
 * used to distinguish security violations from other segmentation faults
 */
bool
is_valid_address(app_pc addr);

/* Used for DR heap area changes as circular dependences prevent
 * directly adding or removing DR vm areas.
 * Must hold the DR areas lock across the combination of calling this and
 * modifying the heap lists.
 * Used to determine when we need to do another heap walk to keep
 * dynamo vm areas up to date (can't do it incrementally b/c of
 * circular dependencies)
 */
void
mark_dynamo_vm_areas_stale(void);

bool
are_dynamo_vm_areas_stale(void);

void
dynamo_vm_areas_lock(void);

void
dynamo_vm_areas_unlock(void);

bool
self_owns_dynamo_vm_area_lock(void);

/* adds dynamo area but doesn't grab lock -- only use this when called to
 * update heap areas!
 */
bool
add_dynamo_heap_vm_area(app_pc start, app_pc end, bool writable,
                        bool from_unmod_image _IF_DEBUG(const char *comment));

/* assumes caller holds a private vmareas lock --
 * only for use by heap_vmareas_synch_units()
 */
void
remove_dynamo_heap_areas(void);

/* used by detach to fixup the permissions of any app writable regions that
 * dynamorio has marked read only for its cache consistency effort */
void
revert_memory_regions(void);

/* returns true if address is inside some dynamo-internal vm area
 * note that this includes not only dynamo library executable regions but
 * also all memory allocated by us, including the code cache!
 */
bool
is_dynamo_address(app_pc addr);

/* check if any features that need pretend writable areas are enabled,
 * in default product configuration should always be true
 */
#define USING_PRETEND_WRITABLE()                            \
    (DYNAMO_OPTION(handle_DR_modify) == DR_MODIFY_NOP ||    \
     DYNAMO_OPTION(handle_ntdll_modify) == DR_MODIFY_NOP || \
     !IS_STRING_OPTION_EMPTY(patch_proof_list) ||           \
     !IS_STRING_OPTION_EMPTY(patch_proof_default_list))

/* returns true iff address is an address that the app thinks is writable
 * but really is not, as it overlaps DR memory
 */
bool
is_pretend_writable_address(app_pc addr);

#ifdef DEBUG
/* returns comment for addr, if there is one, else NULL
 */
char *
get_address_comment(app_pc addr);
#endif

/* returns true if the passed in area overlaps any known executable areas */
bool
executable_vm_area_overlap(app_pc start, app_pc end, bool have_writelock);

/* Returns true if region [start, end) overlaps any regions that are
 * marked as FRAG_COARSE_GRAIN.
 */
bool
executable_vm_area_coarse_overlap(app_pc start, app_pc end);

/* Returns true if region [start, end) overlaps any regions that are
 * marked as VM_PERSISTED_CACHE.
 */
bool
executable_vm_area_persisted_overlap(app_pc start, app_pc end);

/* Returns true if any part of region [start, end) has ever been executed from
 * (including having ibl targets loaded from a persistent cache)
 */
bool
executable_vm_area_executed_from(app_pc start, app_pc end);

/* If there is no overlap between executable_areas and [start,end), returns false.
 * Else, returns true and sets [overlap_start,overlap_end) as the bounds of the first
 * and last executable_area regions that overlap [start,end); i.e.,
 *   overlap_start starts the first area that overlaps [start,end);
 *   overlap_end ends the last area that overlaps [start,end).
 * Thus, overlap_start may be > start and overlap_end may be < end.
 *
 * If frag_flags != 0, the region described above is expanded such that the regions
 * before and after [overlap_start,overlap_end) do NOT match
 * [overlap_start,overlap_end) in TESTALL of frag_flags, but only considering
 * non-contiguous regions if !contig.
 */
bool
executable_area_overlap_bounds(app_pc start, app_pc end, app_pc *overlap_start /*OUT*/,
                               app_pc *overlap_end /*OUT*/, uint frag_flags, bool contig);

/* Coarse unit iterator with support for vm area start and end bounds */
void
vm_area_coarse_iter_start(vmvector_iterator_t *vmvi, app_pc start);
bool
vm_area_coarse_iter_hasnext(vmvector_iterator_t *vmvi, app_pc end);
/* Note that NULL is a valid return value for intermediate iterations
 * if that coarse region has not yet been executed from, due to our
 * new lazy init scheme (case 8640).
 * FIXME: may need to return region bounds or something if have other callers.
 */
coarse_info_t *
vm_area_coarse_iter_next(vmvector_iterator_t *vmvi, app_pc end);
void
vm_area_coarse_iter_stop(vmvector_iterator_t *vmvi);

/* To give clients a chance to process pcaches as we load them, we
 * delay the loading until we've initialized the clients.
 */
void
vm_area_delay_load_coarse_units(void);

void
executable_areas_lock(void);

void
executable_areas_unlock(void);

/* returns true if the passed in area overlaps any dynamo areas */
bool
dynamo_vm_area_overlap(app_pc start, app_pc end);

/* Used to add dr allocated memory regions that may execute out of the cache */
/* NOTE : region is assumed to not be writable, caller is responsible for
 * ensuring this (see fixme in signal.c adding sigreturn code)
 */
bool
add_executable_region(app_pc start, size_t size _IF_DEBUG(const char *comment));

/* removes a region from the executable list */
/* NOTE :the caller is responsible for ensuring that all threads' local
 * vm lists are updated by calling flush_fragments_and_remove_region
 */
bool
remove_executable_region(app_pc start, size_t size, bool have_writelock);

bool
free_nonexec_coarse_and_unlock(void);

/* add dynamo-internal area to the dynamo-internal area list */
bool
add_dynamo_vm_area(app_pc start, app_pc end, uint prot,
                   bool from_unmod_image _IF_DEBUG(const char *comment));
bool
is_dynamo_area_buffer(byte *heap_pc);

/* remove dynamo-internal area from the dynamo-internal area list */
bool
remove_dynamo_vm_area(app_pc start, app_pc end);

#ifdef PROGRAM_SHEPHERDING /* Fix for case 5061.  See case 5075. */
extern const char *const action_message[];

/* be sure to keep this enum and the two arrays, action_message[] &
 * action_event_id[] located in vmareas.c, in synch
 */
typedef enum {
    ACTION_TERMINATE_PROCESS,
    ACTION_CONTINUE, /* detect mode */
    ACTION_TERMINATE_THREAD,
    ACTION_THROW_EXCEPTION,
} action_type_t;

/* Wrapper for security_violation_internal. */
security_violation_t
security_violation(dcontext_t *dcontext, app_pc addr, security_violation_t violation_type,
                   security_option_t type_handling);

/* attack handling - reports violation and possibly terminates the process */
security_violation_t
security_violation_internal(dcontext_t *dcontext, app_pc addr,
                            security_violation_t violation_type,
                            security_option_t type_handling, const char *threat_id,
                            const action_type_t desired_action, read_write_lock_t *lock);

/* returns whether it ended up deleting the self-writing fragment
 * by flushing the region
 */
bool
vm_area_fragment_self_write(dcontext_t *dcontext, app_pc tag);

#    ifdef WINDOWS
#        define USING_FUTURE_EXEC_LIST                                               \
            (dynamo_options.executable_if_alloc || dynamo_options.executable_if_x || \
             dynamo_options.executable_if_flush || dynamo_options.executable_if_hook)
#    else
#        define USING_FUTURE_EXEC_LIST \
            (dynamo_options.executable_if_alloc || dynamo_options.executable_if_x)
#    endif

#endif

/* newly allocated or mapped in memory region */
bool
app_memory_allocation(dcontext_t *dcontext, app_pc base, size_t size, uint prot,
                      bool image _IF_DEBUG(const char *comment));

/* de-allocated or un-mapped memory region */
void
app_memory_deallocation(dcontext_t *dcontext, app_pc base, size_t size,
                        bool own_initexit_lock, bool image);

/* memory region base:base+size now has privileges prot
 * returns one of the following codes
 */
enum {
    DO_APP_MEM_PROT_CHANGE,      /* Let the system call execute normally */
    FAIL_APP_MEM_PROT_CHANGE,    /* skip the system call and return a
                                  * failure code to the app */
    PRETEND_APP_MEM_PROT_CHANGE, /* skip the system call but return success */
    SUBSET_APP_MEM_PROT_CHANGE,  /* make a system call with modified protection,
                                  * expect to return success,
                                  */
};

/* values taken by the option handle_DR_modify and handle_ntdll_modify
 * specifies how to handle app attempts to modify DR memory protection:
 * either halt with an error, return failure to the app, or turn into a nop
 */
enum {
    /* these are mutually exclusive */
    DR_MODIFY_HALT = 0,  /* throw an internal error at the mem prot attempt */
    DR_MODIFY_NOP = 1,   /* turn the mem prot and later write faults into nops */
    DR_MODIFY_FAIL = 2,  /* have the mem prot fail */
    DR_MODIFY_ALLOW = 3, /* let the app muck with us -- WARNING: use at own risk */
    DR_MODIFY_OFF = 4,   /* we don't even check for attempts; WARNING: use at own risk */
};

bool
app_memory_pre_alloc(dcontext_t *dcontext, byte *base, size_t size, uint prot, bool hint,
                     bool update_areas, bool image);

uint
app_memory_protection_change(dcontext_t *dcontext, app_pc base, size_t size,
                             uint prot,         /* platform independent MEMPROT_ */
                             uint *new_memprot, /* OUT */
                             uint *old_memprot /* OPTIONAL OUT*/, bool image);

#ifdef WINDOWS
/* memory region base:base+size was flushed from hardware icache by app */
void
app_memory_flush(dcontext_t *dcontext, app_pc base, size_t size, uint prot);
#    ifdef PROGRAM_SHEPHERDING
/* was pc ever the start pc arg to a flush request? */
bool
was_address_flush_start(dcontext_t *dcontext, app_pc pc);
#    endif
#endif

/* we keep a list of vm areas per thread, to make flushing fragments
 * due to memory unmaps faster
 * This routine adds the page containing start to the thread's list.
 * Adds any FRAG_ flags relevant for a fragment overlapping start's page.
 * If xfer and encounters change in vmareas flags, returns false and does NOT
 * add the new page to the list for this fragment -- assumes caller will NOT add
 * it to the current bb.  This allows for selectively not following direct ctis.
 */
bool
check_thread_vm_area(dcontext_t *dcontext, app_pc start, app_pc tag, void **vmlist,
                     uint *flags, app_pc *stop, bool xfer);

void
check_thread_vm_area_abort(dcontext_t *dcontext, void **vmlist, uint flags);

/* Indicate the PC of the page in which instructions are about to be decoded. */
void
set_thread_decode_page_start(dcontext_t *dcontext, app_pc page_start);

bool
check_in_last_thread_vm_area(dcontext_t *dcontext, app_pc pc);

void
vm_area_add_fragment(dcontext_t *dcontext, fragment_t *f, void *vmlist);

void
acquire_vm_areas_lock(dcontext_t *dcontext, uint fragment_flags);

bool
acquire_vm_areas_lock_if_not_already(dcontext_t *dcontext, uint flags);

void
release_vm_areas_lock(dcontext_t *dcontext, uint fragment_flags);

bool
vm_area_add_to_list(dcontext_t *dcontext, app_pc tag, void **vmlist, uint list_flags,
                    fragment_t *f, bool have_locks);

void
vm_area_destroy_list(dcontext_t *dcontext, void *vmlist);

bool
vm_list_overlaps(dcontext_t *dcontext, void *vmlist, app_pc start, app_pc end);

void
vm_area_remove_fragment(dcontext_t *dcontext, fragment_t *f);

/* Prepares a list of shared fragments for deletion..
 * Caller should have already called vm_area_remove_fragment() on
 * each and chained them together via next_vmarea.
 * Caller must hold the shared_cache_flush_lock.
 * Returns the number of fragments unlinked
 */
int
unlink_fragments_for_deletion(dcontext_t *dcontext, fragment_t *list,
                              int pending_delete_threads);

/* returns the number of fragments unlinked */
int
vm_area_unlink_fragments(dcontext_t *dcontext, app_pc start, app_pc end,
                         int pending_delete_threads _IF_DGCDIAG(app_pc written_pc));

/* removes incoming links for all private fragments in the dcontext
 * thread that contain 'pc'
 */
void
vm_area_unlink_incoming(dcontext_t *dcontext, app_pc pc);

/* Flushes thread-private pending-deletion fragments.
 * Returns false iff was_I_flushed ends up being deleted.
 */
bool
vm_area_flush_fragments(dcontext_t *dcontext, fragment_t *was_I_flushed);

/* Decrements ref counts for thread-shared pending-deletion fragments,
 * and deletes those whose count has reached 0.
 * Returns false iff was_I_flushed has been flushed (not necessarily
 * fully freed yet though, but may be at any time after this call
 * returns, so caller should drop its ref to it).
 */
bool
vm_area_check_shared_pending(dcontext_t *dcontext, fragment_t *was_I_flushed);

/* Assumes that all threads are suspended at safe synch points.
 * Deletes all fine fragments and coarse units in [start, end).
 */
void
vm_area_allsynch_flush_fragments(dcontext_t *dcontext, dcontext_t *del_dcontext,
                                 app_pc start, app_pc end, bool exec_invalid,
                                 bool all_synched);

/* adds the list of fragments beginning with f and chained by {next,prev}_vmarea
 * to a new pending-lazy-deletion entry
 */
void
add_to_lazy_deletion_list(dcontext_t *dcontext, fragment_t *f);

bool
remove_from_lazy_deletion_list(dcontext_t *dcontext, fragment_t *remove);

/* returns true if the passed in area overlaps any vm area of given thread */
bool
thread_vm_area_overlap(dcontext_t *dcontext, app_pc start, app_pc end);

/* Returns NULL if should re-execute the faulting write
 * Else returns the target pc for a new basic block -- caller should
 * return to d_r_dispatch rather than the code cache.
 * Pass in the fragment containing instr_cache_pc if known: else pass NULL.
 */
app_pc
handle_modified_code(dcontext_t *dcontext, cache_pc instr_cache_pc, app_pc instr_app_pc,
                     app_pc target, fragment_t *f);

/* Returns the counter a selfmod fragment should execute for -sandbox2ro_threshold */
uint *
get_selfmod_exec_counter(app_pc tag);

/* Returns true if f has been flushed */
bool
vm_area_selfmod_check_clear_exec_count(dcontext_t *dcontext, fragment_t *f);

/* return true if the address is determined to be on the application's stack
 * otherwise return false
 */
bool
is_address_on_stack(dcontext_t *dcontext, app_pc address);

/* returns true if an executable area exists with VM_DRIVER_ADDRESS,
 * not a strict opposite of is_user_address() */
bool
is_driver_address(app_pc addr);

/* FIXME clean up: safe_apc_or_thread_target, apc_thread_policy_helper and
 * aslr_report_violation should all be ifdef WINDOWS, and may be in a
 * different file
 */
#ifdef PROGRAM_SHEPHERDING
#    ifdef WINDOWS
#        define APC_API __stdcall
#    else /* !WINDOWS */
#        define APC_API
#    endif
void APC_API
safe_apc_or_thread_target(reg_t arg);
#endif /* PROGRAM_SHEPHERDING */

/* a helper procedure for DYNAMO_OPTION(apc_policy) or
 * DYNAMO_OPTION(thread_policy)
 */
typedef enum {
    APC_TARGET_NATIVE,
    APC_TARGET_WINDOWS,
    THREAD_TARGET_NATIVE,
    THREAD_TARGET_WINDOWS
} apc_thread_type_t;

void
apc_thread_policy_helper(app_pc *apc_target_location, /* IN/OUT */
                         security_option_t target_policy, apc_thread_type_t target_type);

/* a helper procedure for reporting ASLR violations */
void
aslr_report_violation(app_pc execution_fault_pc, security_option_t handling_policy);

/* tamper resistant areas used for handle_ntdll_modify */
void
tamper_resistant_region_add(app_pc start, app_pc end);

bool
tamper_resistant_region_overlap(app_pc start, app_pc end);

bool
is_jit_managed_area(app_pc addr);

void
set_region_jit_managed(app_pc start, size_t len);

void
mark_unload_start(app_pc module_base, size_t module_size);
void
mark_unload_end(app_pc module_base);
void
print_last_deallocated(file_t outf);
bool
is_unreadable_or_currently_unloaded_region(app_pc pc);
bool
is_in_last_unloaded_region(app_pc pc);

#endif /* _VMAREAS_H_ */
