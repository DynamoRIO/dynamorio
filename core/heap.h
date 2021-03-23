/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
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

#ifndef _HEAP_H_
#define _HEAP_H_ 1

#ifdef HEAP_ACCOUNTING
typedef enum {
    ACCT_FRAGMENT = 0,
    ACCT_COARSE_LINK,
    ACCT_FRAG_FUTURE,
    ACCT_FRAG_TABLE,
    ACCT_IBLTABLE,
    ACCT_TRACE,
    ACCT_FCACHE_EMPTY,
    ACCT_VMAREA_MULTI,
    ACCT_IR,
    ACCT_AFTER_CALL,
    ACCT_VMAREAS,
    ACCT_SYMBOLS,
#    ifdef SIDELINE
    ACCT_SIDELINE,
#    endif
    ACCT_THCOUNTER,
    ACCT_TOMBSTONE, /* N.B.: leaks in this category are not reported;
                     * not currently used */

    ACCT_HOT_PATCHING,
    ACCT_THREAD_MGT,
    ACCT_MEM_MGT,
    ACCT_STATS,
    ACCT_SPECIAL,
    ACCT_CLIENT,
    ACCT_LIBDUP, /* private copies of system libs => may leak */
    ACCT_CLEANCALL,
    /* NOTE: Also update the whichheap_name in heap.c when adding here */
    ACCT_OTHER,
    ACCT_LAST
} which_heap_t;

#    define HEAPACCT(x) , x
#    define IF_HEAPACCT_ELSE(x, y) x
#else
#    define HEAPACCT(x)
#    define IF_HEAPACCT_ELSE(x, y) y
#endif

typedef enum {
    MAP_FILE_COPY_ON_WRITE = 0x0001,
    MAP_FILE_IMAGE = 0x0002,      /* Windows-only */
    MAP_FILE_FIXED = 0x0004,      /* Linux-only */
    MAP_FILE_REACHABLE = 0x0008,  /* Map at location reachable from vmcode */
    MAP_FILE_VMM_COMMIT = 0x0010, /* Map address is pre-reserved inside VMM. */
    MAP_FILE_APP = 0x0020,        /* Mapping is for the app, not DR/client. */
} map_flags_t;

typedef byte *heap_pc;
#define HEAP_ALIGNMENT sizeof(heap_pc *)
extern vm_area_vector_t *landing_pad_areas;

#ifdef X64
/* Request that the supplied region be 32bit offset reachable from the DR heap.  Should
 * be called before vmm_heap_init() so we can place the DR heap to meet these constraints.
 * Can also be called post vmm_heap_init() but at that point acts as an assert that the
 * supplied region is reachable since the heap is already reserved. */
void
request_region_be_heap_reachable(byte *start, size_t size);

void
vmcode_get_reachable_region(byte **region_start OUT, byte **region_end OUT);
#endif

/* Virtual memory types.  These are bitmask values. */
typedef enum {
    /* Mutually-exclusive core types. */
    VMM_HEAP = 0x0001,
    VMM_CACHE = 0x0002,
    VMM_STACK = 0x0004,
    VMM_SPECIAL_HEAP = 0x0008,
    VMM_SPECIAL_MMAP = 0x0010,
    /* These modify the core types. */
    VMM_REACHABLE = 0x0020,
    /* This is used to decide whether to add guard pages for -per_thread_guard_pages.
     * It is not required that all thread-private allocs use this, nor that this
     * never end up labeling a thread-shared alloc.  It is also not required that
     * this flag be present at incremental commits: only at reserve and unreserve calls.
     */
    VMM_PER_THREAD = 0x0040,
} which_vmm_t;

void
vmm_heap_init(void);
void
vmm_heap_exit(void);
#ifdef UNIX
void
vmm_heap_fork_pre(dcontext_t *dcontext);
void
vmm_heap_fork_post(dcontext_t *dcontext, bool parent);
void
vmm_heap_fork_init(dcontext_t *dcontext);
#endif
void
print_vmm_heap_data(file_t outf);
byte *
vmcode_get_start();
byte *
vmcode_get_end();
void
iterate_vmm_regions(void (*cb)(byte *region_start, byte *region_end, void *user_data),
                    void *user_data);
byte *
vmcode_unreachable_pc();
byte *
vmcode_get_writable_addr(byte *exec_addr);
byte *
vmcode_get_executable_addr(byte *write_addr);

void
vmm_heap_handle_pending_low_on_memory_event_trigger();

bool
heap_check_option_compatibility(void);

bool
is_vmm_reserved_address(byte *pc, size_t size, OUT byte **region_start,
                        OUT byte **region_end);
/* Returns whether "target" is reachable from all future vmcode allocations. */
bool
rel32_reachable_from_vmcode(byte *target);
/* Returns whether "target" is reachable from the current vmcode.  It may
 * not be reachable in the future.  This should normally only be used when
 * "target" is going to be added to the must-be-reachable region, ensuring
 * future reachability.
 */
bool
rel32_reachable_from_current_vmcode(byte *target);

/* heap management */
void
d_r_heap_init(void);
void
d_r_heap_exit(void);
void
heap_post_exit(void); /* post exit to support reattach */
void
heap_reset_init(void);
void
heap_reset_free(void);
void
heap_thread_init(dcontext_t *dcontext);
void
heap_thread_exit(dcontext_t *dcontext);

/* re-initializes non-persistent memory */
void
heap_thread_reset_init(dcontext_t *dcontext);
/* frees all non-persistent memory */
void
heap_thread_reset_free(dcontext_t *dcontext);

/* these functions use the global heap instead of a thread's heap: */
void *
global_heap_alloc(size_t size HEAPACCT(which_heap_t which));
void
global_heap_free(void *p, size_t size HEAPACCT(which_heap_t which));
void *
global_heap_realloc(void *ptr, size_t old_num, size_t new_num,
                    size_t element_size HEAPACCT(which_heap_t which));

bool
lockwise_safe_to_allocate_memory(void);

/* use heap_mmap* to allocate large chunks of memory */
void *
heap_mmap(size_t size, uint prot, which_vmm_t which);
void
heap_munmap(void *p, size_t size, which_vmm_t which);
void *
heap_mmap_reserve(size_t reserve_size, size_t commit_size, uint prot, which_vmm_t which);

void *
heap_mmap_ex(size_t reserve_size, size_t commit_size, uint prot, bool guarded,
             which_vmm_t which);
void
heap_munmap_ex(void *p, size_t size, bool guarded, which_vmm_t which);

byte *
heap_reserve_for_external_mapping(byte *preferred, size_t size, which_vmm_t which);

bool
heap_unreserve_for_external_mapping(byte *p, size_t size, which_vmm_t which);

/* updates dynamo_areas and calls the os_ versions */
byte *
d_r_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot,
             map_flags_t map_flags);
bool
d_r_unmap_file(byte *map, size_t size);

/* Allocates executable memory in the same allocation region as this thread's
 * stack, to save address space (case 9474).
 */
void *
heap_mmap_reserve_post_stack(dcontext_t *dcontext, size_t reserve_size,
                             size_t commit_size, uint prot, which_vmm_t which);
void
heap_munmap_post_stack(dcontext_t *dcontext, void *p, size_t reserve_size,
                       which_vmm_t which);

/* It is up to the caller to ensure commit_size is a page size multiple,
 * and that it does not extend beyond the initial reservation.
 */
void
heap_mmap_extend_commitment(void *p, size_t commit_size, which_vmm_t which);

void
heap_mmap_retract_commitment(void *retract_start, size_t decommit_size,
                             which_vmm_t which);

/* use stack_alloc to build a stack -- it returns TOS */
void *
stack_alloc(size_t size, byte *min_addr);
void
stack_free(void *p, size_t size);

/* checks if pc is in guard page on stack */
bool
is_stack_overflow(dcontext_t *dcontext, byte *sp);

/* these are for thread-local allocs
 * passing dcontext == GLOBAL_DCONTEXT will end up calling global_heap_{alloc,free}
 */
void *
heap_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which));
void
heap_free(dcontext_t *dcontext, void *p, size_t size HEAPACCT(which_heap_t which));

#ifdef HEAP_ACCOUNTING
void
print_heap_statistics(void);
#endif

/* FIXME: persistence is yet another dimension here
 * let's clean all these up and have a single alloc routine?
 */
/* i#1791: nonpersistent heap cannot be used for IR or other client allocations */
void *
nonpersistent_heap_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which));
void
nonpersistent_heap_free(dcontext_t *dcontext, void *p,
                        size_t size HEAPACCT(which_heap_t which));

/* Passing dcontext == GLOBAL_DCONTEXT allocates from a global pool.
 * Important note: within the W^X scheme (-satisfy_w_xor_x), this will return an address
 * of the writeable view. vmcode_get_executable_addr() needs to be called in order to get
 * the reachable address.
 */
void *
heap_reachable_alloc(dcontext_t *dcontext, size_t size HEAPACCT(which_heap_t which));
void
heap_reachable_free(dcontext_t *dcontext, void *p,
                    size_t size HEAPACCT(which_heap_t which));

bool
local_heap_protected(dcontext_t *dcontext);
void
protect_local_heap(dcontext_t *dcontext, bool writable);
void
protect_global_heap(bool writable);
void *
global_unprotected_heap_alloc(size_t size HEAPACCT(which_heap_t which));
void
global_unprotected_heap_free(void *p, size_t size HEAPACCT(which_heap_t which));

#define UNPROTECTED_LOCAL_ALLOC(dc, ...) global_unprotected_heap_alloc(__VA_ARGS__)
#define UNPROTECTED_LOCAL_FREE(dc, ...) global_unprotected_heap_free(__VA_ARGS__)
#define UNPROTECTED_GLOBAL_ALLOC global_unprotected_heap_alloc
#define UNPROTECTED_GLOBAL_FREE global_unprotected_heap_free

/* use global heap for shared fragments and their related data structures */
#define FRAGMENT_ALLOC_DC(dc, flags) (TEST(FRAG_SHARED, (flags)) ? GLOBAL_DCONTEXT : (dc))
#define FRAGMENT_TABLE_ALLOC_DC(dc, flags) \
    (TEST(HASHTABLE_SHARED, (flags)) ? GLOBAL_DCONTEXT : (dc))

/* convenience for allocating a single type: does cast, sizeof, and HEAPACCT
 * for you, and takes param for whether protected or not
 */
#define PROTECTED true
#define UNPROTECTED false
#define HEAP_ARRAY_ALLOC(dc, type, num, which, protected)              \
    ((protected)                                                       \
         ? (type *)heap_alloc(dc, sizeof(type) * (num)HEAPACCT(which)) \
         : (type *)UNPROTECTED_LOCAL_ALLOC(dc, sizeof(type) * (num)HEAPACCT(which)))
#define HEAP_TYPE_ALLOC(dc, type, which, protected) \
    HEAP_ARRAY_ALLOC(dc, type, 1, which, protected)
#define HEAP_ARRAY_ALLOC_MEMSET(dc, type, num, which, protected, val) \
    memset(HEAP_ARRAY_ALLOC(dc, type, num, which, protected), (val),  \
           sizeof(type) * (num));

#define HEAP_ARRAY_FREE(dc, p, type, num, which, protected)              \
    ((protected)                                                         \
         ? heap_free(dc, (type *)p, sizeof(type) * (num)HEAPACCT(which)) \
         : UNPROTECTED_LOCAL_FREE(dc, (type *)p, sizeof(type) * (num)HEAPACCT(which)))
#define HEAP_TYPE_FREE(dc, p, type, which, protected) \
    HEAP_ARRAY_FREE(dc, p, type, 1, which, protected)

/* nonpersistent heap is assumed to be protected */
/* i#1791: nonpersistent heap cannot be used for IR or other client allocations */
#define NONPERSISTENT_HEAP_ARRAY_ALLOC(dc, type, num, which) \
    (type *)nonpersistent_heap_alloc(dc, sizeof(type) * (num)HEAPACCT(which))
#define NONPERSISTENT_HEAP_TYPE_ALLOC(dc, type, which) \
    NONPERSISTENT_HEAP_ARRAY_ALLOC(dc, type, 1, which)
#define NONPERSISTENT_HEAP_ARRAY_FREE(dc, p, type, num, which) \
    nonpersistent_heap_free(dc, (type *)p, sizeof(type) * (num)HEAPACCT(which))
#define NONPERSISTENT_HEAP_TYPE_FREE(dc, p, type, which) \
    NONPERSISTENT_HEAP_ARRAY_FREE(dc, p, type, 1, which)

#define MIN_VMM_BLOCK_SIZE (4U * 1024)

/* special heap of same-sized blocks that avoids global locks */
void *
special_heap_init(uint block_size, bool use_lock, bool executable, bool persistent);
void *
special_heap_init_aligned(uint block_size, uint alignment, bool use_lock, bool executable,
                          bool persistent, size_t initial_unit_size);
void
special_heap_exit(void *special);
void *
special_heap_alloc(void *special);
void *
special_heap_calloc(void *special, uint num);
void
special_heap_free(void *special, void *p);
void
special_heap_cfree(void *special, void *p, uint num);
/* return true if the requested chunk would be fulfilled by special_heap_calloc()
 * without allocating additional heap units
 */
bool
special_heap_can_calloc(void *special, uint num);
#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
void
special_heap_profile_exit(void);
#endif

/* iterator over units in a special heap */
typedef struct _special_heap_iterator {
    /* opaque types, exposed to support stack-alloc iterator */
    void *heap;
    void *next_unit;
} special_heap_iterator_t;

/* Special heap iterator.  Initialized with special_heap_iterator_start(), and
 * destroyed with special_heap_iterator_stop() to release the lock.
 * If the special heap uses no lock for alloc, it is up to the caller
 * to prevent race conditions causing problems.
 * Accessor special_heap_iterator_next() should be called only when
 * predicate special_heap_iterator_hasnext() is true.
 * Any mutation of the heap while iterating will result in a deadlock
 * for heaps that use locks for alloc.
 */
void
special_heap_iterator_start(void *heap, special_heap_iterator_t *shi);
bool
special_heap_iterator_hasnext(special_heap_iterator_t *shi);
void
special_heap_iterator_next(special_heap_iterator_t *shi /* IN/OUT */,
                           app_pc *heap_start /* OUT */, app_pc *heap_end /* OUT */);
void
special_heap_iterator_stop(special_heap_iterator_t *shi);

/* Special heap w/ a vector for lookups */
void *
special_heap_pclookup_init(uint block_size, bool use_lock, bool executable,
                           bool persistent, vm_area_vector_t *vector, void *vector_data,
                           byte *heap_region, size_t heap_size, bool unit_full);

void
special_heap_set_vector_data(void *special, void *vector_data);

/* Returns false if the special heap has more than one unit or has a
 * non-externally-allocated unit.
 * Sets the cur pc for the only unit to end_pc.
 */
bool
special_heap_set_unit_end(void *special, byte *end_pc);

void
heap_vmareas_synch_units(void);

/* Utility routine to delete special heap locks; needed for case 9593. */
void
special_heap_delete_lock(void *special);

#ifdef DEBUG_MEMORY
#    define HEAP_TO_BYTE_EX(hex) 0x##hex
#    define HEAP_TO_BYTE(hex) HEAP_TO_BYTE_EX(hex)
#    define HEAP_TO_UINT_EX(hex) 0x##hex##hex##hex##hex
#    define HEAP_TO_UINT(hex) HEAP_TO_UINT_EX(hex)
#    ifdef X64
#        define HEAP_TO_PTR_UINT_EX(hex) 0x##hex##hex##hex##hex##hex##hex##hex##hex
#    else
#        define HEAP_TO_PTR_UINT_EX(hex) 0x##hex##hex##hex##hex
#    endif
#    define HEAP_TO_PTR_UINT(hex) HEAP_TO_PTR_UINT_EX(hex)

#    define HEAP_UNALLOCATED cd
#    define HEAP_UNALLOCATED_BYTE HEAP_TO_BYTE(HEAP_UNALLOCATED)
#    define HEAP_UNALLOCATED_UINT HEAP_TO_UINT(HEAP_UNALLOCATED)
#    define HEAP_UNALLOCATED_PTR_UINT HEAP_TO_PTR_UINT(HEAP_UNALLOCATED)

#    define HEAP_ALLOCATED ab
#    define HEAP_ALLOCATED_BYTE HEAP_TO_BYTE(HEAP_ALLOCATED)
#    define HEAP_ALLOCATED_UINT HEAP_TO_UINT(HEAP_ALLOCATED)
#    define HEAP_ALLOCATED_PTR_UINT HEAP_TO_PTR_UINT(HEAP_ALLOCATED)

#    define HEAP_PAD bc
#    define HEAP_PAD_BYTE HEAP_TO_BYTE(HEAP_PAD)
#    define HEAP_PAD_UINT HEAP_TO_UINT(HEAP_PAD)
#    define HEAP_PAD_PTR_UINT HEAP_TO_PTR_UINT(HEAP_PAD)
#endif

#endif /* _HEAP_H_ */
