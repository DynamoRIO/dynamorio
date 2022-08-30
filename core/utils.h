/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
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
 * utils.h - miscellenaous utilities
 */

#ifndef _UTILS_H_
#define _UTILS_H_ 1

#ifdef assert
#    undef assert
#endif
/* avoid mistake of lower-case assert */
#define assert assert_no_good_use_ASSERT_instead

/* we provide a less-than-default level of checking */
#define CHKLVL_ASSERTS 1
#define CHKLVL_DEFAULT 2
#if defined(DEBUG) && !defined(NOT_DYNAMORIO_CORE_PROPER) && \
    !defined(NOT_DYNAMORIO_CORE) && !defined(STANDALONE_DECODER)
/* we can't use DYNAMO_OPTION() b/c that has an assert inside it */
#    define DEBUG_CHECKS(level) (dynamo_options.checklevel >= (level))
#else
#    define DEBUG_CHECKS(level) true
#endif

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
#    ifdef INTERNAL
/* cast to void to avoid gcc warning "statement with no effect" when used as
 * a statement and x is a compile-time false
 * FIXME: cl debug build allocates a local for each ?:, so if this gets
 * unrolled in some optionsx or other expansion we could have stack problems!
 */
#        define ASSERT(x)                                                 \
            ((void)((DEBUG_CHECKS(CHKLVL_ASSERTS) && !(x))                \
                        ? (d_r_internal_error(__FILE__, __LINE__, #x), 0) \
                        : 0))
/* make type void to avoid gcc 4.1 warnings about "value computed is not used"
 * (case 7851).  can also use statement expr ({;}) but only w/ gcc, not w/ cl.
 */
#        define ASSERT_MESSAGE(level, msg, x)                                     \
            ((DEBUG_CHECKS(level) && !(x))                                        \
                 ? (d_r_internal_error(msg " @" __FILE__, __LINE__, #x), (void)0) \
                 : (void)0)
#        define REPORT_CURIOSITY(x)                                                   \
            do {                                                                      \
                if (!ignore_assert(__FILE__ ":" STRINGIFY(__LINE__),                  \
                                   "curiosity : " #x)) {                              \
                    report_dynamorio_problem(NULL, DUMPCORE_CURIOSITY, NULL, NULL,    \
                                             "CURIOSITY : %s in file %s line %d", #x, \
                                             __FILE__, __LINE__);                     \
                }                                                                     \
            } while (0)
#        define ASSERT_CURIOSITY(x)                         \
            do {                                            \
                if (DEBUG_CHECKS(CHKLVL_ASSERTS) && !(x)) { \
                    REPORT_CURIOSITY(x);                    \
                }                                           \
            } while (0)
#        define ASSERT_CURIOSITY_ONCE(x)                    \
            do {                                            \
                if (DEBUG_CHECKS(CHKLVL_ASSERTS) && !(x)) { \
                    DO_ONCE({ REPORT_CURIOSITY(x); });      \
                }                                           \
            } while (0)
#    else
/* cast to void to avoid gcc warning "statement with no effect" */
#        define ASSERT(x)                                                 \
            ((void)((DEBUG_CHECKS(CHKLVL_ASSERTS) && !(x))                \
                        ? (d_r_internal_error(__FILE__, __LINE__, ""), 0) \
                        : 0))
#        define ASSERT_MESSAGE(level, msg, x)                             \
            ((void)((DEBUG_CHECKS(level) && !(x))                         \
                        ? (d_r_internal_error(__FILE__, __LINE__, ""), 0) \
                        : 0))
#        define ASSERT_CURIOSITY(x) ((void)0)
#        define ASSERT_CURIOSITY_ONCE(x) ((void)0)
#    endif /* INTERNAL */
#    define ASSERT_NOT_TESTED() \
        SYSLOG_INTERNAL_WARNING_ONCE("Not tested @%s:%d", __FILE__, __LINE__)
#else
#    define ASSERT(x) ((void)0)
#    define ASSERT_MESSAGE(level, msg, x) ASSERT(x)
#    define ASSERT_NOT_TESTED() ASSERT(true)
#    define ASSERT_CURIOSITY(x) ASSERT(true)
#    define ASSERT_CURIOSITY_ONCE(x) ASSERT(true)
#endif /* DEBUG */

#define ASSERT_NOT_REACHED() ASSERT(false)
#define ASSERT_BUG_NUM(num, x) ASSERT_MESSAGE(CHKLVL_ASSERTS, "Bug #" #num, x)
#define ASSERT_NOT_IMPLEMENTED(x) ASSERT_MESSAGE(CHKLVL_ASSERTS, "Not implemented", x)
#define EXEMPT_TEST(tests) check_filter(tests, get_short_name(get_application_name()))

#if defined(INTERNAL) || defined(DEBUG)
void
d_r_internal_error(const char *file, int line, const char *expr);
bool
ignore_assert(const char *assert_file_line, const char *expr);
#endif

/* we now use apicheck as a SYSLOG + abort even for non-API builds */
#define apicheck(x, msg)                                       \
    ((void)((DEBUG_CHECKS(CHKLVL_ASSERTS) && !(x))             \
                ? (external_error(__FILE__, __LINE__, msg), 0) \
                : 0))
void
external_error(const char *file, int line, const char *msg);

#ifdef DEBUG
#    define CLIENT_ASSERT(x, msg) apicheck(x, msg)
#else
#    define CLIENT_ASSERT(x, msg) /* PR 215261: nothing in release builds */
#endif

#ifdef DR_APP_EXPORTS
#    define APP_EXPORT_ASSERT(x, msg) apicheck(x, msg)
#else
#    define APP_EXPORT_ASSERT(x, msg) ASSERT_MESSAGE(CHKLVL_ASSERTS, msg, x)
#endif

/* truncation assert - should be used wherever addressing cl warning 4244 */
/* an explicit type cast to lower precision should match the type used here
 * Later compiler versions cl.13 have an option /RCTc that will make make all
 * explicit casts have that extra check, so these ASSERTs can then be removed.
 */
/* Assumption: int64 is our largest signed type (so casting to it never loses precision).
 * Assumption C99 promotion rules (if same signess promote to size of larger, if
 * different signess promote to size and signess of larger, if different signess and
 * same size promote to unsigned). */
#define CHECK_TRUNCATE_TYPE_byte(val) ((val) >= 0 && (val) <= UCHAR_MAX)
#define CHECK_TRUNCATE_TYPE_sbyte(val) ((val) <= SCHAR_MAX && ((int64)(val)) >= SCHAR_MIN)
#define CHECK_TRUNCATE_TYPE_ushort(val) ((val) >= 0 && (val) <= USHRT_MAX)
#define CHECK_TRUNCATE_TYPE_short(val) ((val) <= SHRT_MAX && ((int64)(val)) >= SHRT_MIN)
#define CHECK_TRUNCATE_TYPE_uint(val) ((val) >= 0 && (val) <= UINT_MAX)
#ifdef UNIX
/* The check causes a gcc warning that can't be disabled in gcc 4.2 and earlier
 * (in gcc 4.3 it was moved to -Wextra under -Wtype-limits).
 * The warning is on ((int64)(val)) >= INT_MIN
 * if val has type uint that I can't seem to cast around and is impossible to ignore -
 * "comparison is always true due to limited range of data type".
 * See http://gcc.gnu.org/ml/gcc/2006-01/msg00784.html and note that the suggested
 * workaround option doesn't exist as far as I can tell. We are potentially in trouble
 * if val has type int64, is negative, and too big to fit in an int.
 */
#    ifdef HAVE_TYPELIMITS_CONTROL
#        define CHECK_TRUNCATE_TYPE_int(val) \
            ((val) <= INT_MAX && ((int64)(val)) >= INT_MIN)
#    else
#        define CHECK_TRUNCATE_TYPE_int(val) \
            ((val) <= INT_MAX && (-(int64)(val)) <= ((int64)INT_MAX) + 1)
#    endif
#else
#    define CHECK_TRUNCATE_TYPE_int(val) ((val) <= INT_MAX && ((int64)(val)) >= INT_MIN)
#endif
#ifdef X64
#    define CHECK_TRUNCATE_TYPE_size_t(val) ((val) >= 0)
/* avoid gcc warning: always true anyway since stats_int_t == int64 */
#    define CHECK_TRUNCATE_TYPE_stats_int_t(val) (true)
#else
#    define CHECK_TRUNCATE_TYPE_size_t CHECK_TRUNCATE_TYPE_uint
#    define CHECK_TRUNCATE_TYPE_stats_int_t CHECK_TRUNCATE_TYPE_int
#endif

/* FIXME: too bad typeof is a GCC only extension - so need to pass both var and type */

/* var = (type) val; should always be preceded by a call to ASSERT_TRUNCATE  */
/* note that it is also OK to use ASSERT_TRUNCATE(type, type, val) for return values */
#define ASSERT_TRUNCATE(var, type, val)                                      \
    ASSERT(sizeof(type) == sizeof(var) && CHECK_TRUNCATE_TYPE_##type(val) && \
           "truncating " #var " to " #type)
#define CURIOSITY_TRUNCATE(var, type, val)                                             \
    ASSERT_CURIOSITY(sizeof(type) == sizeof(var) && CHECK_TRUNCATE_TYPE_##type(val) && \
                     "truncating " #var " to " #type)
#define CLIENT_ASSERT_TRUNCATE(var, type, val, msg) \
    CLIENT_ASSERT(sizeof(type) == sizeof(var) && CHECK_TRUNCATE_TYPE_##type(val), msg)

/* assumes val is unsigned and width<32 */
#define ASSERT_BITFIELD_TRUNCATE(width, val) \
    ASSERT((val) < (1 << ((width) + 1)) && "truncating to " #width " bits");
#define CLIENT_ASSERT_BITFIELD_TRUNCATE(width, val, msg) \
    CLIENT_ASSERT((val) < (1 << ((width) + 1)), msg);

/* alignment helpers, alignment must be power of 2 */
#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)
#define ALIGN_FORWARD(x, alignment) \
    ((((ptr_uint_t)x) + ((alignment)-1)) & (~((ptr_uint_t)(alignment)-1)))
#define ALIGN_FORWARD_UINT(x, alignment) \
    ((((uint)x) + ((alignment)-1)) & (~((alignment)-1)))
#define ALIGN_BACKWARD(x, alignment) (((ptr_uint_t)x) & (~((ptr_uint_t)(alignment)-1)))
#define PAD(length, alignment) (ALIGN_FORWARD((length), (alignment)) - (length))
#define ALIGN_MOD(addr, size, alignment) \
    ((((ptr_uint_t)addr) + (size)-1) & ((alignment)-1))
#define CROSSES_ALIGNMENT(addr, size, alignment) \
    (ALIGN_MOD(addr, size, alignment) < (size)-1)
/* number of bytes you need to shift addr forward so that it's !CROSSES_ALIGNMENT */
#define ALIGN_SHIFT_SIZE(addr, size, alignment)          \
    (CROSSES_ALIGNMENT(addr, size, alignment)            \
         ? ((size)-1 - ALIGN_MOD(addr, size, alignment)) \
         : 0)

/****************************************************************************
 * Synchronization
 */

#include "atomic_exports.h"

#define LOCK_FREE_STATE -1                   /* allows a quick >=0 test for contention */
#define LOCK_SET_STATE (LOCK_FREE_STATE + 1) /* set when requested by a single thread */
/* any value greater than LOCK_SET_STATE means multiple threads requested the lock */
#define LOCK_CONTENDED_STATE (LOCK_SET_STATE + 1)

#define SPINLOCK_FREE_STATE                                    \
    0 /* for initstack_mutex is a spin lock with values 0 or 1 \
       */

/* We want lazy init of the contended event (which can avoid creating
 * dozens of kernel objects), but to initialize it atomically, we need
 * either a pointer to separately-initialized memory or an inlined
 * kernel handle.  We can't allocate heap for locks b/c they're used
 * too early and late, and it seems ugly to use a static array, so we
 * end up having to expose KSYNCH_TYPE for Mac, resulting in a
 * non-uniform initialization of the field.
 * We can't keep this in os_exports.h due to header ordering constraints.
 */
#ifdef WINDOWS
#    define KSYNCH_TYPE HANDLE
#    define KSYNCH_TYPE_STATIC_INIT NULL
#elif defined(LINUX)
#    define KSYNCH_TYPE volatile int
#    define KSYNCH_TYPE_STATIC_INIT -1
#elif defined(MACOS)
#    include <mach/semaphore.h>
typedef struct _mac_synch_t {
    semaphore_t sem;
    volatile int value;
} mac_synch_t;
#    define KSYNCH_TYPE mac_synch_t
#    define KSYNCH_TYPE_STATIC_INIT \
        {                           \
            0, 0                    \
        }
#else
#    error Unknown operating system
#endif

typedef KSYNCH_TYPE contention_event_t;

typedef struct _mutex_t {
    volatile int lock_requests; /* number of threads requesting this lock minus 1 */
    /* a value greater than LOCK_FREE_STATE means the lock has been requested */
    contention_event_t contended_event; /* event object to wait on when contended */
#ifdef DEADLOCK_AVOIDANCE
    /* These fields are initialized with the INIT_LOCK_NO_TYPE macro */
    const char *name; /* set to variable lock name and location */
    /* We flag as a violation if a lock with rank numerically smaller
     * or equal to the rank of a lock already held by the owning thread is acquired
     */
    uint rank;         /* sets rank order in which this lock can be set */
    thread_id_t owner; /* TID of owner (reusable, not avail before initialization) */
    /* Above here is explicitly set w/ INIT_LOCK_NO_TYPE macro so update
     * it if changing them.  Below here is filled with 0's.
     */

    dcontext_t *owning_dcontext; /* dcxt responsible (reusable, multiple per thread) */
    struct _mutex_t *prev_owned_lock; /* linked list of thread owned locks */
    uint count_times_acquired;        /* count total times this lock was acquired */
    uint count_times_contended;       /* count total times this lock was contended upon */
    uint count_times_spin_pause; /* count total times contended in a spin pause loop */
    uint max_contended_requests; /* max number of simultaneous requests when contended */
    /* count times contended but grabbed after spinning without yielding */
    uint count_times_spin_only;
    /* We need to register all locks in the process to be able to dump
     * regular statistics.
     * Linked list of all live locks (for all threads), another ad hoc
     * double linked circular list:
     */
    struct _mutex_t *prev_process_lock;
    struct _mutex_t *next_process_lock;
    /* TODO: we should also add cycles spent while holding the lock, KSTATS-like */
#    ifdef MUTEX_CALLSTACK
    /* FIXME: case 5378: to avoid allocating memory in mutexes use a
     * static array that will get too fat if larger than 4
     */
#        define MAX_MUTEX_CALLSTACK 4
    byte *callstack[MAX_MUTEX_CALLSTACK];
    /* keep as last item so not initialized in INIT_LOCK_NO_TYPE */
    /* i#779: support DR locks used as app locks */
    bool app_lock;
#    else
#        define MAX_MUTEX_CALLSTACK 0 /* cannot use */
#    endif                            /* MUTEX_CALLSTACK */
    bool deleted;                     /* this lock has been deleted at least once */
#endif                                /* DEADLOCK_AVOIDANCE */
    /* Any new field needs to be initialized with INIT_LOCK_NO_TYPE */
} /* XXX i#5383: 8-align is needed for MacM1 or for aarch64 in general? */
IF_MACOS(IF_AARCH64(__attribute__((aligned(8))))) mutex_t;

/* A spin_mutex_t is the same thing as a mutex_t (and all utils.c users use it as
 * such).  It exists only to enforce type separation outside of utils.c which
 * is why we can't simply do a typedef mutex_t spin_mutex_t. */
typedef struct _spin_mutex_t {
    mutex_t lock;
} spin_mutex_t;

/* perhaps for DEBUG all locks should record owner? */
typedef struct _recursive_lock_t {
    mutex_t lock;
    /* Requirement: reading owner field is atomic!
     * Thus must allocate this statically (compiler should 4-byte-align
     * this field, which is good enough) or align it manually!
     * XXX: Provide creation routine that does that for non-static locks?
     */
    thread_id_t owner;
    uint count;
} recursive_lock_t;

typedef struct _read_write_lock_t {
    mutex_t lock;
    /* FIXME: could be merged w/ lock->state if want to get more sophisticated...
     * we could use the lock->state as a 32-bit counter, incremented
     * by readers, and with the MSB bit (sign) set by writers
     */
    volatile int num_readers;
    /* we store the writer so that writers can be readers */
    thread_id_t writer;
    volatile int num_pending_readers; /* readers that have contended with a writer */
    contention_event_t writer_waiting_readers; /* event object for writer to wait on */
    contention_event_t readers_waiting_writer; /* event object for readers to wait on */
    /* make sure to update the two INIT_READWRITE_LOCK cases if you add new fields  */
} read_write_lock_t;

#ifdef DEADLOCK_AVOIDANCE
#    define LOCK_RANK(lock) lock##_rank
/* This should be the single place where all ranks are declared */
/* Your lock should preferably take the last possible rank in this
 * list, usually at the location marked as ADD HERE  */

enum {
    LOCK_RANK(outermost_lock), /* pseudo lock, sentinel of thread owned locks list */
    LOCK_RANK(thread_in_DR_exclusion), /* outermost */
    LOCK_RANK(all_threads_synch_lock), /* < thread_initexit_lock */

    LOCK_RANK(trace_building_lock), /* < bb_building_lock, < table_rwlock */

    /* decode exception -> check if should_intercept requires all_threads
     * FIXME: any other locks that could be interrupted by exception that
     * could be app's fault?
     */
    LOCK_RANK(thread_initexit_lock), /* < all_threads_lock, < snapshot_lock */

    LOCK_RANK(bb_building_lock), /* < change_linking_lock + all vm and heap locks */

#    ifdef WINDOWS
    LOCK_RANK(exception_stack_lock), /* < all_threads_lock */
#    endif
    /* FIXME: grabbed on an exception, which could happen anywhere!
     * possible deadlock if already held */
    LOCK_RANK(all_threads_lock), /* < global_alloc_lock */

    LOCK_RANK(linking_lock), /* < dynamo_areas < global_alloc_lock */

#    ifdef SHARING_STUDY
    LOCK_RANK(shared_blocks_lock), /* < global_alloc_lock */
    LOCK_RANK(shared_traces_lock), /* < global_alloc_lock */
#    endif

    LOCK_RANK(synch_lock), /* per thread, < protect_info */

    LOCK_RANK(protect_info), /* < cache and heap traversal locks */

    LOCK_RANK(sideline_mutex),

    LOCK_RANK(shared_cache_flush_lock), /* < shared_cache_count_lock,
                                           < shared_delete_lock,
                                           < change_linking_lock */
    LOCK_RANK(shared_delete_lock),      /* < change_linking_lock < shared_vm_areas */
    LOCK_RANK(lazy_delete_lock),        /* > shared_delete_lock, < shared_cache_lock */

    LOCK_RANK(shared_cache_lock), /* < dynamo_areas, < allunits_lock,
                                   * < table_rwlock for shared cache regen/replace,
                                   * < shared_vm_areas for cache unit flush,
                                   * < change_linking_lock for add_to_free_list */

    LOCK_RANK(change_linking_lock), /* < shared_vm_areas, < all heap locks */

    LOCK_RANK(shared_vm_areas), /* > change_linking_lock, < executable_areas  */
    LOCK_RANK(shared_cache_count_lock),

    LOCK_RANK(fragment_delete_mutex), /* > shared_vm_areas */

    LOCK_RANK(tracedump_mutex), /* > fragment_delete_mutex, > shared_vm_areas */

    LOCK_RANK(emulate_write_lock), /* in future may be < emulate_write_areas */

    LOCK_RANK(unit_flush_lock), /* > shared_delete_lock */

#    ifdef LINUX
    LOCK_RANK(maps_iter_buf_lock), /* < executable_areas, < module_data_lock,
                                    * < hotp_vul_table_lock */
#    endif

#    ifdef HOT_PATCHING_INTERFACE
    /* This lock's rank needs to be after bb_building_lock because
     * build_bb_ilist() is where injection takes place, which means the
     * bb lock has been acquired before any hot patching related work is done
     * on a bb.
     */
    LOCK_RANK(hotp_vul_table_lock), /* > bb_building_lock,
                                     * < dynamo_areas, < heap_unit_lock. */
#    endif
    LOCK_RANK(coarse_info_lock), /* < special_heap_lock, < global_alloc_lock,
                                  * > change_linking_lock */

    LOCK_RANK(executable_areas), /* < dynamo_areas < global_alloc_lock
                                  * < process_module_vector_lock (diagnostics)
                                  */
#    ifdef RCT_IND_BRANCH
    LOCK_RANK(rct_module_lock), /* > coarse_info_lock, > executable_areas,
                                 * < module_data_lock, < heap allocation */
#    endif
#    ifdef RETURN_AFTER_CALL
    LOCK_RANK(after_call_lock), /* < table_rwlock, > bb_building_lock,
                                 * > coarse_info_lock, > executable_areas,
                                 * < module_data_lock */
#    endif
    LOCK_RANK(written_areas), /* > executable_areas, < module_data_lock,
                               * < dynamo_areas < global_alloc_lock */
#    ifdef LINUX
    LOCK_RANK(rseq_trigger_lock), /* < rseq_areas, < module_data_lock */
#    endif
    LOCK_RANK(module_data_lock), /* < loaded_module_areas, < special_heap_lock,
                                  * > executable_areas */
#    ifdef LINUX
    LOCK_RANK(rseq_areas), /* < dynamo_areas < global_alloc_lock, > module_data_lock */
#    endif
    LOCK_RANK(special_units_list_lock),   /* < special_heap_lock */
    LOCK_RANK(special_heap_lock),         /* > bb_building_lock, > hotp_vul_table_lock
                                           * < dynamo_areas, < heap_unit_lock */
    LOCK_RANK(coarse_info_incoming_lock), /* < coarse_table_rwlock
                                           * > special_heap_lock, > coarse_info_lock,
                                           * > change_linking_lock */

    /* (We don't technically need a coarse_table_rwlock separate from table_rwlock
     * anymore but having it gives us flexibility so I'm leaving it)
     */
    LOCK_RANK(coarse_table_rwlock), /* < global_alloc_lock, < coarse_th_table_rwlock */
    /* We make the pc table separate (we write it while holding main table lock) */
    LOCK_RANK(coarse_pclookup_table_rwlock), /* < global_alloc_lock,
                                              * < coarse_th_table_rwlock */
    /* We make the th table separate (we look in it while holding main table lock) */
    LOCK_RANK(coarse_th_table_rwlock), /* < global_alloc_lock */

    LOCK_RANK(process_module_vector_lock), /* < snapshot_lock > all_threads_synch_lock */
    /* For Loglevel 1 and higher, with LOG_MEMSTATS, the snapshot lock is
     * grabbed on an exception, possible deadlock if already held FIXME */
    LOCK_RANK(snapshot_lock), /* < dynamo_areas */
#    ifdef PROGRAM_SHEPHERDING
    LOCK_RANK(futureexec_areas), /* > executable_areas
                                  * < dynamo_areas < global_alloc_lock */
#        ifdef WINDOWS
    LOCK_RANK(app_flushed_areas), /* < dynamo_areas < global_alloc_lock */
#        endif
#    endif
    LOCK_RANK(pretend_writable_areas), /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(patch_proof_areas),      /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(emulate_write_areas),    /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(IAT_areas),              /* < dynamo_areas < global_alloc_lock */
    /* PR 198871: this same label is used for all client locks */
    LOCK_RANK(dr_client_mutex),            /* > module_data_lock */
    LOCK_RANK(client_thread_count_lock),   /* > dr_client_mutex */
    LOCK_RANK(client_flush_request_lock),  /* > dr_client_mutex */
    LOCK_RANK(low_on_memory_pending_lock), /* < callback_registration_lock <
                                              global_alloc_lock */
    LOCK_RANK(callback_registration_lock), /* > dr_client_mutex */
    LOCK_RANK(client_tls_lock),            /* > dr_client_mutex */
    LOCK_RANK(intercept_hook_lock),        /* < table_rwlock */
    LOCK_RANK(privload_lock),              /* < modlist_areas, < table_rwlock */
#    ifdef LINUX
    LOCK_RANK(sigfdtable_lock), /* < table_rwlock */
#    endif
    LOCK_RANK(table_rwlock),        /* > dr_client_mutex */
    LOCK_RANK(loaded_module_areas), /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(aslr_areas),          /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(aslr_pad_areas),      /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(native_exec_areas),   /* < dynamo_areas < global_alloc_lock */
    LOCK_RANK(thread_vm_areas),     /* currently never used */

    LOCK_RANK(app_pc_table_rwlock), /* > after_call_lock, > rct_module_lock,
                                     * > module_data_lock */

    LOCK_RANK(dead_tables_lock), /* < heap_unit_lock */
    LOCK_RANK(aslr_lock),

#    ifdef HOT_PATCHING_INTERFACE
    LOCK_RANK(hotp_only_tramp_areas_lock),  /* > hotp_vul_table_lock,
                                             * < global_alloc_lock */
    LOCK_RANK(hotp_patch_point_areas_lock), /* > hotp_vul_table_lock,
                                             * < global_alloc_lock */
#    endif
#    ifdef CALL_PROFILE
    LOCK_RANK(profile_callers_lock), /* < global_alloc_lock */
#    endif
    LOCK_RANK(coarse_stub_areas), /* < global_alloc_lock */
    LOCK_RANK(moduledb_lock),     /* < global heap allocation */
    LOCK_RANK(pcache_dir_check_lock),
#    ifdef UNIX
    LOCK_RANK(suspend_lock),
    LOCK_RANK(sighand_lock), /* < sigmask_lock */
    LOCK_RANK(sigmask_lock), /* > sighand_lock */
#    endif
    LOCK_RANK(modlist_areas), /* < dynamo_areas < global_alloc_lock */
#    ifdef WINDOWS
    LOCK_RANK(drwinapi_localheap_lock), /* < global_alloc_lock */
#    endif
    LOCK_RANK(client_aux_libs),
#    ifdef WINDOWS
    LOCK_RANK(client_aux_lib64_lock),
#    endif
#    ifdef WINDOWS
    LOCK_RANK(alt_tls_lock),
#    endif
#    ifdef UNIX
    LOCK_RANK(detached_sigact_lock),
#    endif
    /* ADD HERE a lock around section that may allocate memory */

    /* N.B.: the order of allunits < global_alloc < heap_unit is relied on
     * in the {fcache,heap}_low_on_memory routines.  IMPORTANT - any locks
     * added between the allunits_lock and heap_unit_lock must have special
     * handling in the fcache_low_on_memory() routine.
     */
    LOCK_RANK(allunits_lock),                    /* < global_alloc_lock */
    LOCK_RANK(fcache_unit_areas),                /* > allunits_lock,
                                                    < dynamo_areas, < global_alloc_lock */
    IF_NO_MEMQUERY_(LOCK_RANK(all_memory_areas)) /* < dynamo_areas */
    IF_UNIX_(LOCK_RANK(set_thread_area_lock))    /* no constraints */
    LOCK_RANK(landing_pad_areas_lock),           /* < global_alloc_lock, < dynamo_areas */
    LOCK_RANK(dynamo_areas),                     /* < global_alloc_lock */
    LOCK_RANK(map_intercept_pc_lock),            /* < global_alloc_lock */
    LOCK_RANK(global_alloc_lock),                /* < heap_unit_lock */
    LOCK_RANK(heap_unit_lock),                   /* recursive */
    LOCK_RANK(vmh_lock),                         /* lowest level */
    LOCK_RANK(last_deallocated_lock),
/*---- no one below here can be held at a memory allocation site ----*/

#    ifdef UNIX
    LOCK_RANK(tls_lock), /* if used for get_thread_private_dcontext() may
                          * need to be even lower: as it is, only used for set */
#    endif
    LOCK_RANK(reset_pending_lock), /* > heap_unit_lock */

    LOCK_RANK(initstack_mutex), /* FIXME: NOT TESTED */

    LOCK_RANK(event_lock),          /* FIXME: NOT TESTED */
    LOCK_RANK(do_threshold_mutex),  /* FIXME: NOT TESTED */
    LOCK_RANK(threads_killed_lock), /* FIXME: NOT TESTED */
    LOCK_RANK(child_lock),          /* FIXME: NOT TESTED */

#    ifdef SIDELINE
    LOCK_RANK(sideline_lock),       /* FIXME: NOT TESTED */
    LOCK_RANK(do_not_delete_lock),  /* FIXME: NOT TESTED */
    LOCK_RANK(remember_lock),       /* FIXME: NOT TESTED */
    LOCK_RANK(sideline_table_lock), /* FIXME: NOT TESTED */
#    endif
#    ifdef SIMULATE_ATTACK
    LOCK_RANK(simulate_lock),
#    endif
#    ifdef KSTATS
    LOCK_RANK(process_kstats_lock),
#    endif
#    ifdef X64
    LOCK_RANK(request_region_be_heap_reachable_lock), /* > heap_unit_lock, vmh_lock
                                                       * < report_buf_lock (for assert) */
#    endif
    LOCK_RANK(report_buf_lock),
/* FIXME: if we crash while holding the all_threads_lock, snapshot_lock
 * (for loglevel 1+, logmask LOG_MEMSTATS), or any lock below this
 * line (except the profile_dump_lock, and possibly others depending on
 * options) we will deadlock
 */
#    ifdef LINUX
    LOCK_RANK(memory_info_buf_lock),
#    elif defined(MACOS)
    LOCK_RANK(memquery_backing_lock),
#    endif
#    ifdef WINDOWS
    LOCK_RANK(dump_core_lock),
#    endif

    LOCK_RANK(logdir_mutex), /* recursive */
    LOCK_RANK(diagnost_reg_mutex),
#    ifdef WINDOWS_PC_SAMPLE
    LOCK_RANK(profile_dump_lock),
#    endif

    LOCK_RANK(prng_lock),
    /* ---------------------------------------------------------- */
    /* No new locks below this line, reserved for innermost ASSERT,
     * SYSLOG and STATS facilities */
    LOCK_RANK(options_lock),
#    ifdef WINDOWS
    LOCK_RANK(debugbox_lock),
#    endif
    LOCK_RANK(eventlog_mutex), /* < datasec_selfprot_lock only for hello_message */
    LOCK_RANK(datasec_selfprot_lock),
    LOCK_RANK(thread_stats_lock),
#    ifdef UNIX
    /* shared_itimer_lock is used in timer signal handling, which could happen at
     * anytime, so we put it at the innermost.
     */
    LOCK_RANK(shared_itimer_lock),
#    endif
    LOCK_RANK(innermost_lock), /* innermost internal lock, head of all locks list */
};

struct _thread_locks_t;
typedef struct _thread_locks_t thread_locks_t;

extern mutex_t outermost_lock;
extern mutex_t innermost_lock;

void
locks_thread_init(dcontext_t *dcontext);
void
locks_thread_exit(dcontext_t *dcontext);
uint
locks_not_closed(void);
bool
thread_owns_no_locks(dcontext_t *dcontext);
bool
thread_owns_one_lock(dcontext_t *dcontext, mutex_t *lock);
bool
thread_owns_two_locks(dcontext_t *dcontext, mutex_t *lock1, mutex_t *lock2);
bool
thread_owns_first_or_both_locks_only(dcontext_t *dcontext, mutex_t *lock1,
                                     mutex_t *lock2);

/* We need the (mutex_t) type specifier for direct initialization,
   but not when defining compound structures, hence NO_TYPE */
#    define INIT_LOCK_NO_TYPE(name, rank)                                            \
        {                                                                            \
            LOCK_FREE_STATE, KSYNCH_TYPE_STATIC_INIT, name, rank, INVALID_THREAD_ID, \
        }
#else
/* Ignore the arguments */
#    define INIT_LOCK_NO_TYPE(name, rank)            \
        {                                            \
            LOCK_FREE_STATE, KSYNCH_TYPE_STATIC_INIT \
        }
#endif /* DEADLOCK_AVOIDANCE */

/* Structure assignments and initialization don't work the same in gcc and cl
 * in gcc it is ok to have assignment {gcc; var = (mutex_t) {0}; }
 * and so it helped to have the type specifier.
 * Windows however doesn't like that at all even for initialization,
 * so I have to go and add explicit initialized temporaries to be assigned to a variable
 * i.e. {cl; {mutex_t temp = {0}; var = temp; }; }
 */
#ifdef WINDOWS
#    define STRUCTURE_TYPE(x)
#else
/* FIXME: gcc 4.1.1 complains "initializer element is not constant"
 * if we have our old (x) here; presumably some older gcc needed it;
 * once sure nobody does let's remove this define altogether.
 */
#    define STRUCTURE_TYPE(x)
#endif

#define INIT_LOCK_FREE(lock)                                      \
    STRUCTURE_TYPE(mutex_t)                                       \
    INIT_LOCK_NO_TYPE(#lock "(mutex)"                             \
                            "@" __FILE__ ":" STRINGIFY(__LINE__), \
                      LOCK_RANK(lock))

#define ASSIGN_INIT_LOCK_FREE(var, lock)                                  \
    do {                                                                  \
        mutex_t initializer_##lock = STRUCTURE_TYPE(mutex_t)              \
            INIT_LOCK_NO_TYPE(#lock "(mutex)"                             \
                                    "@" __FILE__ ":" STRINGIFY(__LINE__), \
                              LOCK_RANK(lock));                           \
        var = initializer_##lock;                                         \
    } while (0)

#define ASSIGN_INIT_SPINMUTEX_FREE(var, spinmutex) \
    ASSIGN_INIT_LOCK_FREE((var).lock, spinmutex)

#define INIT_RECURSIVE_LOCK(lock)                                     \
    STRUCTURE_TYPE(recursive_lock_t)                                  \
    {                                                                 \
        INIT_LOCK_NO_TYPE(#lock "(recursive)"                         \
                                "@" __FILE__ ":" STRINGIFY(__LINE__), \
                          LOCK_RANK(lock)),                           \
            INVALID_THREAD_ID, 0                                      \
    }

#define INIT_READWRITE_LOCK(lock)                                                     \
    STRUCTURE_TYPE(read_write_lock_t)                                                 \
    {                                                                                 \
        INIT_LOCK_NO_TYPE(#lock "(readwrite)"                                         \
                                "@" __FILE__ ":" STRINGIFY(__LINE__),                 \
                          LOCK_RANK(lock)),                                           \
            0, INVALID_THREAD_ID, 0, KSYNCH_TYPE_STATIC_INIT, KSYNCH_TYPE_STATIC_INIT \
    }

#define ASSIGN_INIT_READWRITE_LOCK_FREE(var, lock)                                 \
    do {                                                                           \
        read_write_lock_t initializer_##lock = STRUCTURE_TYPE(read_write_lock_t) { \
            INIT_LOCK_NO_TYPE(#lock "(readwrite)"                                  \
                                    "@" __FILE__ ":" STRINGIFY(__LINE__),          \
                              LOCK_RANK(lock)),                                    \
            0,                                                                     \
            INVALID_THREAD_ID,                                                     \
            0,                                                                     \
            KSYNCH_TYPE_STATIC_INIT,                                               \
            KSYNCH_TYPE_STATIC_INIT                                                \
        };                                                                         \
        var = initializer_##lock;                                                  \
    } while (0)

#define ASSIGN_INIT_RECURSIVE_LOCK_FREE(var, lock)                               \
    do {                                                                         \
        recursive_lock_t initializer_##lock = STRUCTURE_TYPE(recursive_lock_t) { \
            INIT_LOCK_NO_TYPE(#lock "(recursive)"                                \
                                    "@" __FILE__ ":" STRINGIFY(__LINE__),        \
                              LOCK_RANK(lock)),                                  \
            INVALID_THREAD_ID, 0                                                 \
        };                                                                       \
        var = initializer_##lock;                                                \
    } while (0)

#define INIT_SPINLOCK_FREE(lock) \
    STRUCTURE_TYPE(mutex_t)      \
    {                            \
        SPINLOCK_FREE_STATE,     \
    }

/* in order to use parallel names to the above INIT_*LOCK routines */
#define DELETE_LOCK(lock) d_r_mutex_delete(&lock)
#define DELETE_SPINMUTEX(spinmutex) spinmutex_delete(&spinmutex)
#define DELETE_RECURSIVE_LOCK(rec_lock) d_r_mutex_delete(&(rec_lock).lock)
#define DELETE_READWRITE_LOCK(rwlock) d_r_mutex_delete(&(rwlock).lock)
/* mutexes need to release any kernel objects that were created */
void
d_r_mutex_delete(mutex_t *lock);

/* basic synchronization functions */
void
d_r_mutex_lock(mutex_t *mutex);
bool
d_r_mutex_trylock(mutex_t *mutex);
void
d_r_mutex_unlock(mutex_t *mutex);
#ifdef UNIX
void
d_r_mutex_fork_reset(mutex_t *mutex);
#endif
void
d_r_mutex_mark_as_app(mutex_t *lock);
/* Use this version of 'lock' when obtaining a lock in an app context. In the
 * case that there is contention on this lock, this thread will be marked safe
 * to be relocated and even detached. The current thread's mcontext may be
 * clobbered with the provided value even if the thread is not suspended.
 */
void
d_r_mutex_lock_app(mutex_t *mutex, priv_mcontext_t *mc);

/* spinmutex synchronization */
bool
spinmutex_trylock(spin_mutex_t *spin_lock);
void
spinmutex_lock(spin_mutex_t *spin_lock);
void
spinmutex_lock_no_yield(spin_mutex_t *spin_lock);
void
spinmutex_unlock(spin_mutex_t *spin_lock);
void
spinmutex_delete(spin_mutex_t *spin_lock);

/* tests if a lock is held, but doesn't grab it */
/* note that this is not a synchronizing function, its intended uses are :
 * 1. for synch code to guarantee that a thread it has suspened isn't holding
 * a lock (note that a return of true doesn't mean that the suspended thread
 * is holding the lock, could be some other thread)
 * 2. for when you want to assert that you hold a lock, while you can't
 * actually do that, you can assert with this function that the lock is held
 * by someone
 * 3. read_{un,}lock use this function to check the state of write lock mutex
 */
static inline bool
mutex_testlock(mutex_t *lock)
{
    return atomic_aligned_read_int(&lock->lock_requests) > LOCK_FREE_STATE;
}

static inline bool
spinmutex_testlock(spin_mutex_t *spin_lock)
{
    return mutex_testlock(&spin_lock->lock);
}

/* A recursive lock can be taken more than once by the owning thread */
void
acquire_recursive_lock(recursive_lock_t *lock);
bool
try_recursive_lock(recursive_lock_t *lock);
void
release_recursive_lock(recursive_lock_t *lock);
bool
self_owns_recursive_lock(recursive_lock_t *lock);
/* Use this version of 'lock' when obtaining a lock in an app context. In the
 * case that there is contention on this lock, this thread will be marked safe
 * to be relocated and even detached. The current thread's mcontext may be
 * clobbered with the provided value even if the thread is not suspended.
 */
void
acquire_recursive_app_lock(recursive_lock_t *mutex, priv_mcontext_t *mc);

/* A read write lock allows multiple readers or alternatively a single writer */
void
d_r_read_lock(read_write_lock_t *rw);
void
d_r_write_lock(read_write_lock_t *rw);
bool
d_r_write_trylock(read_write_lock_t *rw);
void
d_r_read_unlock(read_write_lock_t *rw);
void
d_r_write_unlock(read_write_lock_t *rw);
bool
self_owns_write_lock(read_write_lock_t *rw);

#if defined(MACOS) || (defined(X64) && defined(WINDOWS))
#    define ATOMIC_READ_THREAD_ID(id) \
        ((thread_id_t)atomic_aligned_read_int64((volatile int64 *)(id)))
#else
#    define ATOMIC_READ_THREAD_ID(id) \
        ((thread_id_t)atomic_aligned_read_int((volatile int *)(id)))
#endif

/* test whether locks are held at all */
#define WRITE_LOCK_HELD(rw) \
    (mutex_testlock(&(rw)->lock) && atomic_aligned_read_int(&(rw)->num_readers) == 0)
#define READ_LOCK_HELD(rw) (atomic_aligned_read_int(&(rw)->num_readers) > 0)

/* test whether current thread owns locks
 * for non-DEADLOCK_AVOIDANCE, cannot tell who owns it, so we bundle
 * into asserts to make sure not used in a way that counts on it
 * knowing for sure.
 */
#ifdef DEADLOCK_AVOIDANCE
#    define OWN_MUTEX(m) (ATOMIC_READ_THREAD_ID(&(m)->owner) == d_r_get_thread_id())
#    define ASSERT_OWN_MUTEX(pred, m) ASSERT(!(pred) || OWN_MUTEX(m))
#    define ASSERT_DO_NOT_OWN_MUTEX(pred, m) ASSERT(!(pred) || !OWN_MUTEX(m))
#    define OWN_NO_LOCKS(dc) thread_owns_no_locks(dc)
#    define ASSERT_OWN_NO_LOCKS()                                        \
        do {                                                             \
            dcontext_t *dc = get_thread_private_dcontext();              \
            ASSERT(dc == NULL /* no way to tell */ || OWN_NO_LOCKS(dc)); \
        } while (0)
#else
/* don't know for sure: imprecise in a conservative direction */
#    define OWN_MUTEX(m) (mutex_testlock(m))
#    define ASSERT_OWN_MUTEX(pred, m) ASSERT(!(pred) || OWN_MUTEX(m))
#    define ASSERT_DO_NOT_OWN_MUTEX(pred, m) ASSERT(!(pred) || true /*no way to tell*/)
#    define OWN_NO_LOCKS(dc) true /* no way to tell */
#    define ASSERT_OWN_NO_LOCKS() /* no way to tell */
#endif
#define ASSERT_OWN_WRITE_LOCK(pred, rw) ASSERT(!(pred) || self_owns_write_lock(rw))
#define ASSERT_DO_NOT_OWN_WRITE_LOCK(pred, rw) \
    ASSERT(!(pred) || !self_owns_write_lock(rw))
/* FIXME: no way to tell if current thread is one of the readers */
#define ASSERT_OWN_READ_LOCK(pred, rw) ASSERT(!(pred) || READ_LOCK_HELD(rw))
#define READWRITE_LOCK_HELD(rw) (READ_LOCK_HELD(rw) || self_owns_write_lock(rw))
#define ASSERT_OWN_READWRITE_LOCK(pred, rw) ASSERT(!(pred) || READWRITE_LOCK_HELD(rw))
#define ASSERT_OWN_RECURSIVE_LOCK(pred, l) ASSERT(!(pred) || self_owns_recursive_lock(l))

/* in machine-specific assembly file: */
int
atomic_swap(volatile int *addr, int value);

#define SHARED_MUTEX(operation, lock)                                            \
    do {                                                                         \
        if (SHARED_FRAGMENTS_ENABLED() && !INTERNAL_OPTION(single_thread_in_DR)) \
            mutex_##operation(&(lock));                                          \
    } while (0)
#define SHARED_RECURSIVE_LOCK(operation, lock)                                   \
    do {                                                                         \
        if (SHARED_FRAGMENTS_ENABLED() && !INTERNAL_OPTION(single_thread_in_DR)) \
            operation##_recursive_lock(&(lock));                                 \
    } while (0)
/* internal use only */
/* We need to serialize bbs for thread-private for first-execution module load
 * events (i#884).
 */
extern bool
dr_modload_hook_exists(void); /* hard to include instrument.h here */
#define USE_BB_BUILDING_LOCK_STEADY_STATE()                                  \
    ((DYNAMO_OPTION(shared_bbs) && !INTERNAL_OPTION(single_thread_in_DR)) || \
     dr_modload_hook_exists())
/* anyone guarding the bb_building_lock with this must use SHARED_BB_{UN,}LOCK */
#define USE_BB_BUILDING_LOCK() (USE_BB_BUILDING_LOCK_STEADY_STATE() && bb_lock_start)
#define SHARED_BB_LOCK()                         \
    do {                                         \
        if (USE_BB_BUILDING_LOCK())              \
            d_r_mutex_lock(&(bb_building_lock)); \
    } while (0)
/* We explicitly check the lock_requests to handle a thread from appearing
 * suddenly and causing USE_BB_BUILDING_LOCK() to return true while we're
 * about to unlock it.
 * We'll still have a race where the original thread and the new thread
 * add to the cache simultaneously, and the original thread can do the
 * unlock (with the 2nd thread's unlock then being a nop), but it should
 * only happen in extreme corner cases.  In debug it could raise an
 * error about the non-owner releasing the mutex.
 */
#define SHARED_BB_UNLOCK()                                                              \
    do {                                                                                \
        if (USE_BB_BUILDING_LOCK() && bb_building_lock.lock_requests > LOCK_FREE_STATE) \
            d_r_mutex_unlock(&(bb_building_lock));                                      \
    } while (0)
/* we assume dynamo_resetting is only done w/ all threads suspended */
#define NEED_SHARED_LOCK(flags)                                             \
    (TEST(FRAG_SHARED, (flags)) && !INTERNAL_OPTION(single_thread_in_DR) && \
     !dynamo_exited && !dynamo_resetting)
#define SHARED_FLAGS_MUTEX(flags, operation, lock) \
    do {                                           \
        if (NEED_SHARED_LOCK((flags)))             \
            mutex_##operation(&(lock));            \
    } while (0)
#define SHARED_FLAGS_RECURSIVE_LOCK(flags, operation, lock) \
    do {                                                    \
        if (NEED_SHARED_LOCK((flags)))                      \
            operation##_recursive_lock(&(lock));            \
    } while (0)

/****************************************************************************/

/* Our parameters (option string, logdir, etc.) are configured
 * through files and secondarily environment variables.
 * On Windows we also support using the registry.
 * The routines below are used to check both of those places.
 * value is a buffer allocated by the caller to hold the
 * resulting value.
 */
#include "config.h"

/****************************************************************************
 * hashing functions
 */
/* bits=entries: 8=256, 12=4096, 13=8192, 14=16384, 16=65536 */

/* Our default hash function is a bitwise & with a mask (HASH_FUNC_BITS)
 * to select index bits, and maybe offset to ignore least significant bits.
 * (as we're dealing with pcs here that is generally enough, but for larger
 *  table sizes a more complicated function is sometimes needed so the
 *  HASH_FUNC macro supports other hashing functions)
 * These macros would be typed as follows:
 *   extern uint HASH_FUNC(uint val, table* table);
 *      where table is any struct that contains
 *      hash_func, hash_mask and hash_offset elements
 *   extern uint HASH_FUNC_BITS(uint val, int num_bits);
 *   extern uint HASH_MASK(int num_bits);
 *   extern uint HASHTABLE_SIZE(int num_bits);
 */

/* FIXME - xref 8139 which is best 2654435769U 2654435761U or 0x9e379e37
 * FIXME PR 212574 (==8139): would we want the 32-bit one for smaller index values?
 */
#define PHI_2_32 2654435769U           /* (sqrt(5)-1)/2 * (2^32) */
#define PHI_2_64 11400714819323198485U /* (sqrt(5)-1)/2 * (2^64) */
#ifdef X64
#    define HASH_PHI PHI_2_64
#    define HASH_TAG_BITS 64
#else
#    define HASH_PHI PHI_2_32
#    define HASH_TAG_BITS 32
#endif

/* bitmask selecting num_bits least significant bits */
#define HASH_MASK(num_bits) ((~PTR_UINT_0) >> (HASH_TAG_BITS - (num_bits)))

/* evaluate hash function and select index bits.  Although bit
 * selection and shifting could be done in reverse, better assembly
 * code can be emitted when hash_mask selects the index bits */
#define HASH_FUNC(val, table)                                            \
    ((uint)((HASH_VALUE_FOR_TABLE(val, table) & ((table)->hash_mask)) >> \
            (table)->hash_mask_offset))

#ifdef X86
/* no instruction alignment -> use the lsb!
 * for 64-bit we assume the mask is taking out everything beyond uint range
 */
#    define HASH_FUNC_BITS(val, num_bits) ((uint)((val) & (HASH_MASK(num_bits))))
#else
/* do not use the lsb (alignment!) */
/* FIXME: this function in product builds is in fact not used on
 * addresses so ignoring the LSB is not helping.
 * Better use the more generic HASH_FUNC that allows for hash_offset other than 1
 */
#    define HASH_FUNC_BITS(val, num_bits) (((val) & (HASH_MASK(num_bits))) >> 1)
#endif

/* FIXME - xref 8139, what's the best shift for multiply phi? In theory for
 * m bit table should take the middle m bits of the qword result. We currently
 * take the top m bits of the lower dword which is prob. almost as good, but
 * could experiment (however, I suspect probing strategy might make more of a
 * difference at that point). You're not allowed to touch this code without
 * first reading Knuth vol 3 sec 6.4 */
#define HASH_VALUE_FOR_TABLE(val, table) \
    ((table)->hash_func == HASH_FUNCTION_NONE ?                         \
         (val)                                :                         \
         ((table)->hash_func == HASH_FUNCTION_MULTIPLY_PHI ?            \
             /* All ibl tables use NONE so don't need to worry about    \
              * the later hash_mask_offset shift. FIXME - structure all \
              * these macros a little clearly/efficiently. */           \
             /* case 8457: keep in sync w/ hash_value()'s calc */       \
             (((val) * HASH_PHI) >> (HASH_TAG_BITS - (table)->hash_bits)) : \
             hash_value((val), (table)->hash_func, (table)->hash_mask,  \
                        (table)->hash_bits)))

#define HASHTABLE_SIZE(num_bits) (1U << (num_bits))

/* Note that 1 will be the default */
typedef enum {
    HASH_FUNCTION_NONE = 0,
    HASH_FUNCTION_MULTIPLY_PHI = 1,
#ifdef INTERNAL
    HASH_FUNCTION_LOWER_BSWAP = 2,
    HASH_FUNCTION_BSWAP_XOR = 3,
    HASH_FUNCTION_SWAP_12TO15 = 4,
    HASH_FUNCTION_SWAP_12TO15_AND_NONE = 5,
    HASH_FUNCTION_SHIFT_XOR = 6,
#endif
    HASH_FUNCTION_STRING = 7,
    HASH_FUNCTION_STRING_NOCASE = 8,
    HASH_FUNCTION_ENUM_MAX,
} hash_function_t;

ptr_uint_t
hash_value(ptr_uint_t val, hash_function_t func, ptr_uint_t mask, uint bits);
uint
hashtable_num_bits(uint size);

/****************************************************************************/

/* Reachability helpers */
/* Given a region, returns the start of the enclosing region that can reached by a
 * 32bit displacement from everywhere in the supplied region. Checks for underflow. If the
 * supplied region is too large then returned value may be > reachable_region_start
 * (caller should check) as the constraint may not be satisfiable. */
/* i#14: gcc 4.3.0 treats "ptr - const < ptr" as always true,
 * so we work around that here.  could adapt POINTER_OVERFLOW_ON_ADD
 * or cast to ptr_uint_t like it does, instead.
 */
#define REACHABLE_32BIT_START(reachable_region_start, reachable_region_end) \
    (((reachable_region_end) > ((byte *)(ptr_uint_t)(uint)(INT_MIN)))       \
         ? (reachable_region_end) + INT_MIN                                 \
         : (byte *)PTR_UINT_0)
/* Given a region, returns the end of the enclosing region that can reached by a
 * 32bit displacement from everywhere in the supplied region. Checks for overflow. If the
 * supplied region is too large then returned value may be < reachable_region_end
 * (caller should check) as the constraint may not be satisfiable. */
#define REACHABLE_32BIT_END(reachable_region_start, reachable_region_end) \
    (((reachable_region_start) < ((byte *)POINTER_MAX) - INT_MAX)         \
         ? (reachable_region_start) + INT_MAX                             \
         : (byte *)POINTER_MAX)

#define MAX_LOW_2GB ((byte *)(ptr_uint_t)INT_MAX)

#define IS_POWER_OF_2(x) ((x) == 0 || ((x) & ((x)-1)) == 0)

/* C standard has pointer overflow as undefined so cast to unsigned
 * (i#14 and drmem i#302)
 */
#define POINTER_OVERFLOW_ON_ADD(ptr, add) \
    (((ptr_uint_t)(ptr)) + (add) < ((ptr_uint_t)(ptr)))
#define POINTER_UNDERFLOW_ON_SUB(ptr, sub) \
    (((ptr_uint_t)(ptr)) - (sub) > ((ptr_uint_t)(ptr)))

/* bitmap_t operations */

/* Current implementation uses integers representing 32 bits */
typedef uint bitmap_element_t;
typedef bitmap_element_t bitmap_t[];

/* Note that we have some bitmap operations in unix/signal.c for
 *  kernel version of sigset_t as well as in
 *  win32/ntdll.c:tls_{alloc,free} which could use some of these
 *  facilities, but for now we leave those as more OS specific
 */

#define BITMAP_DENSITY 32
#define BITMAP_MASK(i) (1 << ((i) % BITMAP_DENSITY))
#define BITMAP_INDEX(i) ((i) / BITMAP_DENSITY)
#define BITMAP_NOT_FOUND ((uint)-1)

/* bitmap_t primitives */

/* TODO: could use BT for bit test, BTS/BTR for set/clear, and BSF for
 *  bit scan forward.  See /usr/src/linux-2.4/include/asm/bitops.h for
 *  an all assembly implementation.  Here we stick to plain C.
 */

/* returns non-zero value if bit is set */
static inline bool
bitmap_test(bitmap_t b, uint i)
{
    return ((b[BITMAP_INDEX(i)] & BITMAP_MASK(i)) != 0);
}

static inline void
bitmap_set(bitmap_t b, uint i)
{
    b[BITMAP_INDEX(i)] |= BITMAP_MASK(i);
}

static inline void
bitmap_clear(bitmap_t b, uint i)
{
    b[BITMAP_INDEX(i)] &= ~BITMAP_MASK(i);
}

/* bitmap_size is number of bits in the bitmap_t */
void
bitmap_initialize_free(bitmap_t b, uint bitmap_size);
uint
bitmap_allocate_blocks(bitmap_t b, uint bitmap_size, uint request_blocks,
                       uint start_block);
void
bitmap_free_blocks(bitmap_t b, uint bitmap_size, uint first_block, uint num_free);

#ifdef DEBUG
/* used only for ASSERTs */
bool
bitmap_are_reserved_blocks(bitmap_t b, uint bitmap_size, uint first_block,
                           uint num_blocks);
bool
bitmap_check_consistency(bitmap_t b, uint bitmap_size, uint expect_free);
#endif /* DEBUG */

/* logging functions */
/* use the following three defines to control the logging directory format */
#define LOGDIR_MAX_NUM 1000
#define LOGDIR_FORMAT_STRING "%s.%03d"
#define LOGDIR_FORMAT_ARGS(num) "dynamorio", num
/* longest message we would put in a log or messagebox
 * 512 is too short for internal exception w/ app + options + callstack
 */
/* We define MAX_LOG_LENGTH_MINUS_ONE for splitting long buffers.
 * It must be a raw numeric constant as we STRINGIFY it.
 */
#ifdef PARAMS_IN_REGISTRY
#    define MAX_LOG_LENGTH IF_X64_ELSE(1280, 768)
#    define MAX_LOG_LENGTH_MINUS_ONE IF_X64_ELSE(1279, 767)
#else
/* need more space for printing out longer option strings */
/* For client we have a larger stack and 2048 option length so go bigger
 * so clients don't have dr_printf truncated as often.
 */
#    define MAX_LOG_LENGTH 2048
#    define MAX_LOG_LENGTH_MINUS_ONE 2047
#endif

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
#    define LOG(file, mask, level, ...)                                \
        do {                                                           \
            if (d_r_stats != NULL && d_r_stats->loglevel >= (level) && \
                (d_r_stats->logmask & (mask)) != 0)                    \
                d_r_print_log(file, mask, level, __VA_ARGS__);         \
        } while (0)
/* use DOELOG for customer visible logging. statement can be a {} block */
#    define DOELOG(level, mask, statement)                             \
        do {                                                           \
            if (d_r_stats != NULL && d_r_stats->loglevel >= (level) && \
                (d_r_stats->logmask & (mask)) != 0)                    \
                statement                                              \
        } while (0)
/* not using DYNAMO_OPTION b/c it contains ASSERT */
#    define DOCHECK(level, statement) \
        do {                          \
            if (DEBUG_CHECKS(level))  \
                statement             \
        } while (0)
#    ifdef INTERNAL
#        define DOLOG DOELOG
#        define LOG_DECLARE(declaration) declaration
#    else
/* XXX: this means LOG_DECLARE and LOG are different for non-INTERNAL */
#        define DOLOG(level, mask, statement)
#        define LOG_DECLARE(declaration)
#    endif /* INTERNAL */
#    define THREAD          \
        ((dcontext == NULL) \
             ? main_logfile \
             : ((dcontext == GLOBAL_DCONTEXT) ? main_logfile : dcontext->logfile))
#    define THREAD_GET get_thread_private_logfile()
#    define GLOBAL main_logfile
#else /* !DEBUG */
/* make use of gcc macro varargs, LOG's args may be ifdef DEBUG */
/* the macro is actually ,fmt... but C99 requires one+ argument which we just strip */
#    define LOG(file, mask, level, ...)
#    define DOLOG(level, mask, statement)
#    define DOELOG DOLOG
#    define LOG_DECLARE(declaration)
#    define DOCHECK(level, statement) /* nothing */
#endif
void
d_r_print_log(file_t logfile, uint mask, uint level, const char *fmt, ...);
void
print_file(file_t f, const char *fmt, ...);

/* For repeated appending to a buffer.  The "sofar" var should be set
 * to 0 by the caller before the first call to print_to_buffer.
 */
bool
print_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, const char *fmt, ...);

const char *
memprot_string(uint prot);

char *
double_strchr(char *string, char c1, char c2);
#ifndef WINDOWS
const char *
double_strrchr(const char *string, char c1, char c2);
#else
/* double_strrchr() defined in win32/inject_shared.h */
/* wcsnlen is provided in ntdll only from win7 onward */
size_t
our_wcsnlen(const wchar_t *str, size_t max);
#    define wcsnlen our_wcsnlen
#endif
bool
str_case_prefix(const char *str, const char *pfx);

bool
is_region_memset_to_char(byte *addr, size_t size, byte val);

/* calculates intersection of two regions defined as open ended intervals
 * [region1_start, region1_start + region1_len) \intersect
 * [region2_start, region2_start + region2_len)
 *
 * intersection_len is set to 0 if the regions do not overlap
 * otherwise returns the intersecting region as
 * [intersection_start, intersection_start + intersection_len)
 */
void
region_intersection(app_pc *intersection_start /* OUT */,
                    size_t *intersection_len /* OUT */, const app_pc region1_start,
                    size_t region1_len, const app_pc region2_start, size_t region2_len);

bool
check_filter(const char *filter, const char *short_name);
bool
check_filter_with_wildcards(const char *filter, const char *short_name);

typedef enum {
    BASE_DIR,   /* Only creates directory specified in env */
    PROCESS_DIR /* Creates a process subdir off of base (e.g. dynamorio.000) */
} log_dir_t;
void
enable_new_log_dir(void); /* enable creating a new base logdir (for a fork, e.g.) */
void
create_log_dir(int dir_type);
bool
get_log_dir(log_dir_t dir_type, char *buffer, uint *buffer_length);

/* must use close_log_file() to close */
file_t
open_log_file(const char *basename, char *finalname_with_path, uint maxlen);
void
close_log_file(file_t f);

file_t
get_thread_private_logfile(void);
bool
get_unique_logfile(const char *file_type, char *filename_buffer, uint maxlen,
                   bool open_directory, file_t *file);
const char *
get_app_name_for_path(void);
const char *
get_short_name(const char *exename);

/* Self-protection: we can't use pragmas in local scopes so no convenient way to
 * place the do_once var elsewhere than .data.  Since it's only written once we
 * go ahead and unprotect here.  Even if we have dozens of these (there aren't
 * that many in release builds currently) it shouldn't hurt us.
 * FIXME: this means that if the protection routines call a routine that has
 * a do-once, we have a deadlock!  Could switch to a recursive lock.
 */
extern int do_once_generation; /* for possible re-attach */
#define DO_ONCE(statement)                                          \
    {                                                               \
        /* no mutual exclusion, should be used only with logging */ \
        static int do_once = 0;                                     \
        if (do_once < do_once_generation) {                         \
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);            \
            do_once++;                                              \
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);              \
            statement;                                              \
        }                                                           \
    }

/* This is more heavy-weight and includes its own static mutex
 * The counter is only incremented if it is less than the threshold
 */
/* Self-protection case 8075: can't use pragma at local scope.  We put
 * the burden on the caller to make the do_threshold_cur in .data
 * writable.  For do_threshold_mutex, even if it's writable at the
 * macro site, {add,remove}_process_lock will crash on adjacent
 * entries in the lock list (and an attempt there to unprot .data will
 * deadlock as the datasec lock is acquired and hits the same
 * unprot!).  So we use a single global mutex in debug builds.  There
 * aren't currently any uses of this macro that will be hurt by this
 * serialization so we could also do it in release builds.
 */
#ifdef DEADLOCK_AVOIDANCE
extern mutex_t do_threshold_mutex;
#    define DECLARE_THRESHOLD_LOCK(section) /* nothing */
#else
#    define DECLARE_THRESHOLD_LOCK(section)                         \
        static mutex_t do_threshold_mutex VAR_IN_SECTION(section) = \
            INIT_LOCK_FREE(do_threshold_mutex);
#endif
/* The section argument is our support for the user wrapping entire
 * function in a separate section, which for gcc also requires
 * annotating each var declaration.
 */
#define DO_THRESHOLD_SAFE(threshold, section, statement_below, statement_after) \
    {                                                                           \
        DECLARE_THRESHOLD_LOCK(section)                                         \
        static uint do_threshold_cur VAR_IN_SECTION(section) = 0;               \
        d_r_mutex_lock(&do_threshold_mutex);                                    \
        if (do_threshold_cur < threshold) {                                     \
            do_threshold_cur++;                                                 \
            d_r_mutex_unlock(&do_threshold_mutex);                              \
            statement_below;                                                    \
        } else {                                                                \
            d_r_mutex_unlock(&do_threshold_mutex);                              \
            statement_after; /* or at */                                        \
        }                                                                       \
    }

/* TRY/EXCEPT and TRY/FINALLY
 * usage notes:
 * any automatic variables that you want to use in the except block
 * should be declared as volatile - see case 5891
 *
 * Note we do not have language support - do not use return within a
 * TRY block!  otherwise we can't rollback or execute FINALLY.
 *
 * Note we do not support filters in EXCEPT statements so the
 * innermost handler will be called.  Also finally blocks are not
 * implemented (FIXME: we would have to unwind all nested finally blocks
 * before the EXCEPT block).
 *
 * Note no locks should be grabbed within a TRY/EXCEPT block (FIXME:
 * until we have FINALLY support to release them).
 *
 * (tip: compile your TRY blocks first outside of this macro for
 * easier line matching and debugging)
 */
/* This form allows GLOBAL_DCONTEXT or NULL dcontext if !dynamo_initialized.
 * In release build we'll run w/o crashing if dcontext is NULL and we're
 * post-dynamo_initialized and so can't use global_try_except w/o a race,
 * but we don't want to do this and we assert on it.  It should only
 * happen during late thread exit and currently there are no instances of it.
 */
#define TRY_EXCEPT_ALLOW_NO_DCONTEXT(dcontext, try_statement, except_statement) \
    do {                                                                        \
        try_except_t *try__except = NULL;                                       \
        dcontext_t *dc__local = dcontext;                                       \
        if ((dc__local == NULL || dc__local == GLOBAL_DCONTEXT) &&              \
            !dynamo_initialized) {                                              \
            try__except = &global_try_except;                                   \
            IF_UNIX(global_try_tid = get_sys_thread_id());                      \
        } else {                                                                \
            if (dc__local == GLOBAL_DCONTEXT)                                   \
                dc__local = get_thread_private_dcontext();                      \
            if (dc__local != NULL)                                              \
                try__except = &dc__local->try_except;                           \
        }                                                                       \
        ASSERT(try__except != NULL);                                            \
        TRY(try__except, try_statement, EXCEPT(try__except, except_statement)); \
        IF_UNIX(global_try_tid = INVALID_THREAD_ID);                            \
    } while (0)

/* these use do..while w/ a local to avoid double-eval of dcontext */
#define TRY_EXCEPT(dcontext, try_statement, except_statement)                   \
    do {                                                                        \
        try_except_t *try__except = &(dcontext)->try_except;                    \
        ASSERT((dcontext) != NULL && (dcontext) != GLOBAL_DCONTEXT);            \
        TRY(try__except, try_statement, EXCEPT(try__except, except_statement)); \
    } while (0)

#define TRY_FINALLY(dcontext, try_statement, finally_statement)                  \
    do {                                                                         \
        try_except_t *try__except = &(dcontext)->try_except;                     \
        ASSERT((dcontext) != NULL && (dcontext) != GLOBAL_DCONTEXT);             \
        TRY(try__except, try_statement, FINALLY(try__except, except_statement)); \
    } while (0)

/* internal versions */
#define TRY(try_pointer, try_statement, except_or_finally)                             \
    do {                                                                               \
        try_except_context_t try__state;                                               \
        /* must be current thread -> where we'll fault */                              \
        /* We allow NULL solely to avoid duplicating try_statement in                  \
         * TRY_EXCEPT_ALLOW_NO_DCONTEXT.                                               \
         */                                                                            \
        ASSERT(                                                                        \
            (try_pointer) == &global_try_except || (try_pointer) == NULL ||            \
            (get_thread_private_dcontext() !=                                          \
                 NULL && /* Note that the following statement does not dereference the \
                          * result of get_thread_private_dcontext() (because we need   \
                          * just the offset of a data member). Still, when the null    \
                          * sanitizer is enabled, it performs the is-null check, which \
                          * can fail if the returned value is NULL. So, we need this   \
                          * is-null check of our own.                                  \
                          */                                                           \
             (try_pointer) == &get_thread_private_dcontext()->try_except) ||           \
            (try_pointer) == /* A currently-native thread: */                          \
                &thread_lookup(IF_UNIX_ELSE(get_sys_thread_id(), d_r_get_thread_id())) \
                     ->dcontext->try_except);                                          \
        if ((try_pointer) != NULL) {                                                   \
            try__state.prev_context = (try_pointer)->try_except_state;                 \
            (try_pointer)->try_except_state = &try__state;                             \
        }                                                                              \
        if ((try_pointer) == NULL || DR_SETJMP(&try__state.context) == 0) {            \
            try_statement /* TRY block */ /* make sure there is no return in           \
                                             try_statement */                          \
                if ((try_pointer) != NULL)                                             \
            {                                                                          \
                POP_TRY_BLOCK(try_pointer, try__state);                                \
            }                                                                          \
        }                                                                              \
        except_or_finally                                                              \
        /* EXCEPT or FINALLY will POP_TRY_BLOCK on exception */                        \
    } while (0)

/* implementation notes: */
/* FIXME: it is more secure yet not as flexible to use a scheme
 * like the Exception Tables in the Linux kernel where a static
 * mapping from a faulting PC to a fixup code (in
 * exception_table_entry) can be kept in read-only memory.  That
 * scheme works really well for tight assembly, and the fast path
 * is somewhat faster than the 10 instructions in dr_setjmp().
 * Note however, that a return address on our thread stack is just
 * as vulnerable, so the security advantage is minor.
 * The tighter scheme also makes it hard to cover up faults that are
 * at unexpected instructions in a block.
 */

/* no filters
 * FIXME: we may want filters in debug builds to make sure we can
 * detect the proper EXCEPT condition (need to register in the TRY).
 * Alternatively, should match against a list of instructions that are
 * the only ones known to possibly fail.
 *
 * Note we also don't provide any access to the exception context,
 * since we don't plan on recovering at the fault point (which a
 * filter may recommend).
 */

/* Only called within a TRY block that contains the proper try__state */
#define EXCEPT(try_pointer, statement)                           \
    else                                                         \
    { /* EXCEPT */                                               \
        /* a failure in the EXCEPT should be thrown higher up */ \
        /* rollback first */                                     \
        POP_TRY_BLOCK(try_pointer, try__state);                  \
        statement;                                               \
        /* FIXME: stop unwinding */                              \
    }

/* FIXME: should be called only nested within another TRY/EXCEPT
 * block.  (We don't support __leave so there is no other use).  If it
 * was called not nested in an EXCEPT handler, we can't just hide
 * there was an exception at all, otherwise this will change behavior
 * if it is ever nested.
 */
/* Only called within a TRY block */
/* NYI */
#define FINALLY(try_pointer, statement) /* ALWAYS */                  \
    {                                                                 \
        ASSERT_NOT_IMPLEMENTED(false);                                \
        ASSERT((try_pointer) != NULL);                                \
        if ((try_pointer)->unwinding_exception) {                     \
            /* only on exception we have to POP here */               \
            /* normal execution would have already POPped */          \
                                                                      \
            /* pop before executing finally statement */              \
            /* so an exception in it is delivered to the */           \
            /* previous handler */                                    \
            /* only parent TRY block has proper try__state */         \
            POP_TRY_BLOCK(try_pointer, try__state);                   \
        }                                                             \
        ASSERT((try_pointer)->try_except_state != NULL &&             \
               "try/finally should be nested in try/except");         \
        /* executed for both normal execution, or exception */        \
        statement;                                                    \
        if ((try_pointer)->unwinding_exception) {                     \
            /* FIXME: on nested exception must keep UNWINDing */      \
            /* and give control to the previous nested handler */     \
            /* until an EXCEPT handler resumes to normal execution */ \
            /* we don't keep any exception context */                 \
            ASSERT_NOT_IMPLEMENTED(false);                            \
        }                                                             \
    }

/* internal helper */
#define POP_TRY_BLOCK(try_pointer, state)                \
    ASSERT((try_pointer) != NULL);                       \
    ASSERT((try_pointer)->try_except_state == &(state)); \
    (try_pointer)->try_except_state = (try_pointer)->try_except_state->prev_context;

enum { LONGJMP_EXCEPTION = 1 };
/* the return value of setjmp() returned on exception (or unwinding) */

/* volatile to ensure the compiler doesn't completely skip a READ */
#define PROBE_READ_PC(pc) ((*(volatile char *)(pc)))
#define PROBE_WRITE_PC(pc) ATOMIC_ADD_PTR(volatile char *, (*(volatile char *)(pc)), 0)
/* FIXME: while handling a read exception thread stack expansion in
 * other threads may lose its guard page.  Since current thread won't
 * know if it is ok to expand, therefore the stacks won't grow any
 * further.  See MSDN: IsBadReadPtr(), We may want to mark back any
 * PAGE_GUARD faults before we handle them in our EXCEPT block. For
 * most purposes it is unlikely to be an issue that is not already an
 * app bug causing us to touch these.  Our LOCKed ADD is somewhat
 * better than IsBadWritePtr() but is best not to have to use it.
 */

#include "stats.h"

#ifdef UNIX
#    define UNUSED __attribute__((unused))
#else
#    define UNUSED
#endif

/* Use to shut up the compiler about an unused variable when the
 * alternative is a painful modification of more src code. Our
 * standard is to use this macro just after the variable is declared
 * and to use it judiciously.
 */
#define UNUSED_VARIABLE(pv)                  \
    {                                        \
        void *unused_pv UNUSED = (void *)pv; \
        unused_pv = NULL;                    \
    }

/* Both release and debug builds share these common stats macros */
/* If -no_global_rstats, all values will be 0, so user does not have to
 * use DO_GLOBAL_STATS or check runtime option.
 */
#define GLOBAL_STAT(stat) d_r_stats->stat##_pair.value
/* explicit macro for addr so no assumptions on GLOBAL_STAT being lvalue */
#define GLOBAL_STAT_ADDR(stat) &(d_r_stats->stat##_pair.value)
#define DO_GLOBAL_STATS(statement) \
    do {                           \
        if (GLOBAL_STATS_ON()) {   \
            statement;             \
        }                          \
    } while (0)

#ifdef X64
#    define XSTATS_ATOMIC_INC(var) ATOMIC_INC(int64, var)
#    define XSTATS_ATOMIC_DEC(var) ATOMIC_DEC(int64, var)
#    define XSTATS_ATOMIC_ADD(var, val) ATOMIC_ADD(int64, var, val)
#    define XSTATS_ATOMIC_MAX(max, cur) ATOMIC_MAX(int64, max, cur)
#    define XSTATS_ATOMIC_ADD_EXCHANGE(var, val) atomic_add_exchange_int64(var, val)
#else
#    define XSTATS_ATOMIC_INC(var) ATOMIC_INC(int, var)
#    define XSTATS_ATOMIC_DEC(var) ATOMIC_DEC(int, var)
#    define XSTATS_ATOMIC_ADD(var, val) ATOMIC_ADD(int, var, val)
#    define XSTATS_ATOMIC_MAX(max, cur) ATOMIC_MAX(int, max, cur)
#    define XSTATS_ATOMIC_ADD_EXCHANGE(var, val) atomic_add_exchange_int(var, val)
#endif

/* XSTAT_* macros are pointed at by either STATS_* or RSTATS_*
 * XSTAT_* should not be called directly outside this file
 */
#define XSTATS_INC_DC(dcontext, stat)                                \
    do {                                                             \
        DO_THREAD_STATS(dcontext, THREAD_STAT(dcontext, stat) += 1); \
        DO_GLOBAL_STATS(XSTATS_ATOMIC_INC(GLOBAL_STAT(stat)));       \
    } while (0)
#define XSTATS_INC(stat) \
    XSTATS_WITH_DC(stats_inc__dcontext, XSTATS_INC_DC(stats_inc__dcontext, stat))

#define XSTATS_DEC_DC(dcontext, stat)                                \
    do {                                                             \
        DO_THREAD_STATS(dcontext, THREAD_STAT(dcontext, stat) -= 1); \
        DO_GLOBAL_STATS(XSTATS_ATOMIC_DEC(GLOBAL_STAT(stat)));       \
    } while (0)
#define XSTATS_DEC(stat) \
    XSTATS_WITH_DC(stats_dec__dcontext, XSTATS_DEC_DC(stats_dec__dcontext, stat))

#define XSTATS_ADD_DC(dcontext, stat, value)                                           \
    do {                                                                               \
        stats_int_t stats_add_dc__value = (stats_int_t)(value);                        \
        CURIOSITY_TRUNCATE(stats_add_dc__value, stats_int_t, value);                   \
        DO_THREAD_STATS(dcontext, THREAD_STAT(dcontext, stat) += stats_add_dc__value); \
        DO_GLOBAL_STATS(XSTATS_ATOMIC_ADD(GLOBAL_STAT(stat), stats_add_dc__value));    \
    } while (0)
#define XSTATS_ADD(stat, value) \
    XSTATS_WITH_DC(stats_add__dcontext, XSTATS_ADD_DC(stats_add__dcontext, stat, value))
#define XSTATS_SUB(stat, value) XSTATS_ADD(stat, -(stats_int_t)(value))
#define XSTATS_ADD_ASSIGN_DC(dcontext, stat, var, value)                                \
    do {                                                                                \
        stats_int_t stats_add_assign_dc__value = (stats_int_t)(value);                  \
        CURIOSITY_TRUNCATE(stats_add_assign_dc__value, stats_int_t, value);             \
        DO_THREAD_STATS(dcontext,                                                       \
                        THREAD_STAT(dcontext, stat) += stats_add_assign_dc__value);     \
        /* would normally DO_GLOBAL_STATS(), but need to assign var */                  \
        var =                                                                           \
            XSTATS_ATOMIC_ADD_EXCHANGE(&GLOBAL_STAT(stat), stats_add_assign_dc__value); \
    } while (0)
#define XSTATS_INC_ASSIGN_DC(dcontext, stat, var) \
    XSTATS_ADD_ASSIGN_DC(dcontext, stats, var, 1)
#define XSTATS_ADD_ASSIGN(stat, var, value)    \
    XSTATS_WITH_DC(stats_add_assign__dcontext, \
                   XSTATS_ADD_ASSIGN_DC(stats_add_assign__dcontext, stat, var, value))
#define XSTATS_INC_ASSIGN(stat, var) XSTATS_ADD_ASSIGN(stat, var, 1)

#define XSTATS_MAX_HELPER(dcontext, stat, global_val, thread_val)               \
    do {                                                                        \
        DO_THREAD_STATS(dcontext, {                                             \
            stats_int_t stats_max_helper__value;                                \
            stats_max_helper__value = (thread_val);                             \
            if (THREAD_STAT(dcontext, stat) < stats_max_helper__value)          \
                THREAD_STAT(dcontext, stat) = stats_max_helper__value;          \
        });                                                                     \
        DO_GLOBAL_STATS({ XSTATS_ATOMIC_MAX(GLOBAL_STAT(stat), global_val); }); \
    } while (0)
#define XSTATS_MAX_DC(dcontext, stat_max, stat_cur)              \
    XSTATS_MAX_HELPER(dcontext, stat_max, GLOBAL_STAT(stat_cur), \
                      THREAD_STAT(dcontext, stat_cur))
#define XSTATS_PEAK_DC(dcontext, stat) XSTATS_MAX_DC(dcontext, peak_##stat, stat)
#define XSTATS_MAX(stat_max, stat_cur)  \
    XSTATS_WITH_DC(stats_max__dcontext, \
                   XSTATS_MAX_DC(stats_max__dcontext, stat_max, stat_cur))
#define XSTATS_TRACK_MAX(stats_track_max, val)                                       \
    do {                                                                             \
        stats_int_t stats_track_max__value = (stats_int_t)(val);                     \
        CURIOSITY_TRUNCATE(stats_track_max__value, stats_int_t, val);                \
        XSTATS_WITH_DC(stats_track_max__dcontext,                                    \
                       XSTATS_MAX_HELPER(stats_track_max__dcontext, stats_track_max, \
                                         stats_track_max__value,                     \
                                         stats_track_max__value));                   \
    } while (0)
#define XSTATS_PEAK(stat) \
    XSTATS_WITH_DC(stats_peak__dcontext, XSTATS_PEAK_DC(stats_peak__dcontext, stat))

#define XSTATS_ADD_MAX_DC(dcontext, stat_max, stat_cur, value)                \
    do {                                                                      \
        stats_int_t stats_add_max__temp;                                      \
        XSTATS_ADD_ASSIGN_DC(dcontext, stat_cur, stats_add_max__temp, value); \
        XSTATS_MAX_HELPER(dcontext, stat_max, stats_add_max__temp,            \
                          THREAD_STAT(dcontext, stat_cur));                   \
    } while (0)
#define XSTATS_ADD_MAX(stat_max, stat_cur, value) \
    XSTATS_WITH_DC(                               \
        stats_add_max__dcontext,                  \
        XSTATS_ADD_MAX_DC(stats_add_max__dcontext, stat_max, stat_cur, value))
#define XSTATS_ADD_PEAK_DC(dcontext, stat, value) \
    XSTATS_ADD_MAX_DC(dcontext, peak_##stat, stat, value)
#define XSTATS_ADD_PEAK(stat, value)         \
    XSTATS_WITH_DC(stats_add_peak__dcontext, \
                   XSTATS_ADD_PEAK_DC(stats_add_peak__dcontext, stat, value))

#define XSTATS_RESET_DC(dcontext, stat)                             \
    do {                                                            \
        DO_THREAD_STATS(dcontext, THREAD_STAT(dcontext, stat) = 0); \
        DO_GLOBAL_STATS(GLOBAL_STAT(stat) = 0);                     \
    } while (0)
#define XSTATS_RESET(stat) \
    XSTATS_WITH_DC(stats_reset__dcontext, XSTATS_RESET_DC(stats_reset__dcontext, stat))

/* common to both release and debug build */
#define RSTATS_INC XSTATS_INC
#define RSTATS_DEC XSTATS_DEC
#define RSTATS_ADD XSTATS_ADD
#define RSTATS_SUB XSTATS_SUB
#define RSTATS_ADD_PEAK XSTATS_ADD_PEAK

#if defined(DEBUG) && defined(INTERNAL)
#    define DODEBUGINT DODEBUG
#    define DOCHECKINT DOCHECK
#else
#    define DODEBUGINT(statement)        /* nothing */
#    define DOCHECKINT(level, statement) /* nothing */
#endif

/* For use in CLIENT_ASSERT or elsewhere that exists even if
 * STANDALONE_DECODER is defined.
 */
#ifdef DEBUG
#    define DEBUG_EXT_DECLARE(declaration) declaration
#else
#    define DEBUG_EXT_DECLARE(declaration)
#endif

#if defined(DEBUG) && !defined(STANDALONE_DECODER)
#    define DODEBUG(statement) \
        do {                   \
            statement          \
        } while (0)
#    define DEBUG_DECLARE(declaration) declaration
#    define DOSTATS(statement) \
        do {                   \
            statement          \
        } while (0)
/* FIXME: move to stats.h */
/* Note : stats macros are called in places where it is not safe to hold any lock
 * (such as special_heap_create_unit, others?), if ever go back to using a mutex
 * to protect the stats need to update such places
 */
/* global and thread local stats, can be used as lvalues,
 * not used if not DEBUG */
/* We assume below that all stats are aligned and thus reading and writing
 * stats are atomic operations on Intel x86 */
/* In general should prob. be using stats_add_[peak, max] instead of
 * stats_[peak/max] since they tie the adjustment of the stat to the
 * setting of the max, otherwise you're open to race conditions involving
 * multiple threads adjusting the same stats and setting peak/max FIXME
 */
#    define GLOBAL_STATS_ON() (d_r_stats != NULL && INTERNAL_OPTION(global_stats))
#    define THREAD_STAT(dcontext, stat) (dcontext->thread_stats)->stat##_thread
#    define THREAD_STATS_ON(dcontext)                         \
        (dcontext != NULL && INTERNAL_OPTION(thread_stats) && \
         dcontext != GLOBAL_DCONTEXT && dcontext->thread_stats != NULL)
#    define DO_THREAD_STATS(dcontext, statement) \
        do {                                     \
            if (THREAD_STATS_ON(dcontext)) {     \
                statement;                       \
            }                                    \
        } while (0)
#    define XSTATS_WITH_DC(var, statement)           \
        do {                                         \
            dcontext_t *var = NULL;                  \
            if (INTERNAL_OPTION(thread_stats))       \
                var = get_thread_private_dcontext(); \
            statement;                               \
        } while (0)

#    define STATS_INC XSTATS_INC
/* we'll expose more *_DC as we need them */
#    define STATS_INC_DC XSTATS_INC_DC
#    define STATS_DEC XSTATS_DEC
#    define STATS_ADD XSTATS_ADD
#    define STATS_SUB XSTATS_SUB
#    define STATS_INC_ASSIGN XSTATS_INC_ASSIGN
#    define STATS_ADD_ASSIGN XSTATS_ADD_ASSIGN
#    define STATS_MAX XSTATS_MAX
#    define STATS_TRACK_MAX XSTATS_TRACK_MAX
#    define STATS_PEAK XSTATS_PEAK
#    define STATS_ADD_MAX XSTATS_ADD_MAX
#    define STATS_ADD_PEAK XSTATS_ADD_PEAK
#    define STATS_RESET XSTATS_RESET

#else
#    define DODEBUG(statement)
#    define DEBUG_DECLARE(declaration)
#    define DOSTATS(statement)
#    define THREAD_STATS_ON(dcontext) false
#    define XSTATS_WITH_DC(var, statement) statement
#    define DO_THREAD_STATS(dcontext, statement) /* nothing */
#    define GLOBAL_STATS_ON() (d_r_stats != NULL && DYNAMO_OPTION(global_rstats))

/* Would be nice to catch incorrect usage of STATS_INC on a release-build
 * stat: if rename release vars, have to use separate GLOBAL_RSTAT though.
 */
#    define STATS_INC(stat)                          /* nothing */
#    define STATS_INC_DC(dcontext, stat)             /* nothing */
#    define STATS_DEC(stat)                          /* nothing */
#    define STATS_ADD(stat, value)                   /* nothing */
#    define STATS_SUB(stat, value)                   /* nothing */
#    define STATS_INC_ASSIGN(stat, var)              /* nothing */
#    define STATS_ADD_ASSIGN(stat, var, value)       /* nothing */
#    define STATS_MAX(stat_max, stat_cur)            /* nothing */
#    define STATS_TRACK_MAX(stats_track_max, val)    /* nothing */
#    define STATS_PEAK(stat)                         /* nothing */
#    define STATS_ADD_MAX(stat_max, stat_cur, value) /* nothing */
#    define STATS_ADD_PEAK(stat, value)              /* nothing */
#    define STATS_RESET(stat)                        /* nothing */
#endif                                               /* DEBUG */

#ifdef KSTATS
#    define DOKSTATS(statement) \
        do {                    \
            statement           \
        } while (0)

/* The proper use is most commonly KSTART(name)/KSTOP(name), or
 * occasionally KSTART(name)/KSWITCH(better_name)/KSTOP(name), and in
 * ignorable cases KSTART(name)/KSTOP_NOT_PROPAGATED(name)
 */
/* starts a timer */
#    define KSTART(name) KSTAT_THREAD(name, kstat_start_var(ks, pv))

/* makes sure we're matching start/stop */
#    define KSTOP(name) KSTAT_THREAD(name, kstat_stop_matching_var(ks, pv))

/* modifies the variable against which this path should be counted */
#    define KSWITCH(name) KSTAT_THREAD(name, kstat_switch_var(ks, pv))

/* allow mismatched start/stop - for use with KSWITCH */
#    define KSTOP_NOT_MATCHING(name)                                              \
        KSTAT_THREAD_NO_PV_START(get_thread_private_dcontext())                   \
        ASSERT(ks->depth > 2 && "stop_not_matching not allowed to clear kstack"); \
        kstat_stop_not_matching_var(ks, ignored);                                 \
        KSTAT_THREAD_NO_PV_END()

/* rewind the callstack exiting multiple entries - for exception cases */
#    define KSTOP_REWIND(name) KSTAT_THREAD(name, kstat_stop_rewind_var(ks, pv))
#    define KSTOP_REWIND_UNTIL(name) KSTAT_THREAD(name, kstat_stop_longjmp_var(ks, pv))

/* simultanously switch to a path and stop timer */
#    define KSWITCH_STOP(name)                        \
        KSTAT_THREAD(name, {                          \
            kstat_switch_var(ks, pv);                 \
            kstat_stop_not_matching_var(ks, ignored); \
        })

/* simultanously switch to a path and stop timer w/o propagating to parent */
#    define KSWITCH_STOP_NOT_PROPAGATED(name)                        \
        KSTAT_THREAD(name, {                                         \
            timestamp_t ignore_cum;                                  \
            kstat_switch_var(ks, pv);                                \
            kstat_stop_not_propagated_var(ks, ignored, &ignore_cum); \
        })

/* do not propagate subpath time to parent */
#    define KSTOP_NOT_MATCHING_NOT_PROPAGATED(name)                                      \
        KSTAT_THREAD(name, {                                                             \
            timestamp_t ignore_cum;                                                      \
            ASSERT(ks->depth > 2 && "stop_not_matching_np not allowed to clear kstack"); \
            kstat_stop_not_propagated_var(ks, pv, &ignore_cum);                          \
        })

/* do not propagate subpath time to parent */
#    define KSTOP_NOT_PROPAGATED(name)                                            \
        KSTAT_THREAD(name, {                                                      \
            timestamp_t ignore_cum;                                               \
            DODEBUG({                                                             \
                if (ks->node[ks->depth - 1].var != pv)                            \
                    kstats_dump_stack(cur_dcontext);                              \
            });                                                                   \
            ASSERT(ks->node[ks->depth - 1].var == pv && "stop not matching TOS"); \
            kstat_stop_not_propagated_var(ks, pv, &ignore_cum);                   \
        })

/* in some cases we do need to pass a dcontext for another thread */
/* since get_thread_private_dcontext() may be expensive we can pass a dcontext
 * to this version of the macro, however we should then use this everywhere
 * to have comparable overheads
 */
#    define KSTART_DC(dc, name) KSTAT_OTHER_THREAD(dc, name, kstat_start_var(ks, pv))
#    define KSTOP_DC(dc, name) \
        KSTAT_OTHER_THREAD(dc, name, kstat_stop_matching_var(ks, pv))
#    define KSTOP_NOT_MATCHING_DC(dc, name)       \
        KSTAT_THREAD_NO_PV_START(dc)              \
        kstat_stop_not_matching_var(ks, ignored); \
        KSTAT_THREAD_NO_PV_END()
#    define KSTOP_REWIND_DC(dc, name) \
        KSTAT_OTHER_THREAD(dc, name, kstat_stop_rewind_var(ks, pv))
#else                                               /* !KSTATS */
#    define DOKSTATS(statement)                     /* nothing */
#    define KSTART(name)                            /* nothing */
#    define KSWITCH(name)                           /* nothing */
#    define KSWITCH_STOP(name)                      /* nothing */
#    define KSWITCH_STOP_NOT_PROPAGATED(name)       /* nothing */
#    define KSTOP_NOT_MATCHING_NOT_PROPAGATED(name) /* nothing */
#    define KSTOP_NOT_PROPAGATED(name)              /* nothing */
#    define KSTOP_NOT_MATCHING(name)                /* nothing */
#    define KSTOP(name)                             /* nothing */
#    define KSTOP_REWIND(name)                      /* nothing */
#    define KSTOP_REWIND_UNTIL(name)                /* nothing */

#    define KSTART_DC(dc, name)             /* nothing */
#    define KSTOP_DC(dc, name)              /* nothing */
#    define KSTOP_NOT_MATCHING_DC(dc, name) /* nothing */
#    define KSTOP_REWIND_DC(dc, name)       /* nothing */
#endif                                      /* KSTATS */

#ifdef INTERNAL
#    define DODEBUG_ONCE(statement) DODEBUG(DO_ONCE(statement))
#    define DOLOG_ONCE(level, mask, statement) DOLOG(level, mask, DO_ONCE(statement))
#else
#    define DODEBUG_ONCE(statement)            /* nothing */
#    define DOLOG_ONCE(level, mask, statement) /* nothing */
#endif                                         /* INTERNAL */

#define MAX_FP_STATE_SIZE (512 + 16) /* maximum buffer size plus alignment */
/* for convenice when want to save floating point state around a statement
 * that contains ifdefs, need to be used at same nesting depth */
/* fpstate_junk is used so that this macro can be used before, or in the
 * middle of a list of declerations without bothering the compiler or creating
 * a new nesting block */

/* keep in mind that each use of PRESERVE_FLOATING_POINT_STATE takes
 * 512 bytes - avoid nesting uses
 */
/* We call dr_fpu_exception_init() to avoid the app clearing float and xmm
 * exception flags and messing up our code (i#1213).
 */

#define PRESERVE_FLOATING_POINT_STATE_START()                    \
    {                                                            \
        byte fpstate_buf[MAX_FP_STATE_SIZE];                     \
        byte *fpstate = (byte *)ALIGN_FORWARD(fpstate_buf, 16);  \
        size_t fpstate_junk UNUSED = proc_save_fpstate(fpstate); \
        dr_fpu_exception_init()

#define PRESERVE_FLOATING_POINT_STATE_END() \
    proc_restore_fpstate(fpstate);          \
    fpstate_junk = 0xdead;                  \
    }

#define PRESERVE_FLOATING_POINT_STATE(statement) \
    do {                                         \
        PRESERVE_FLOATING_POINT_STATE_START();   \
        statement;                               \
        PRESERVE_FLOATING_POINT_STATE_END();     \
    } while (0)

/* argument counting, http://gcc.gnu.org/ml/gcc/2000-09/msg00604.html */
#define ARGUMENT_COUNTER(...) _ARGUMENT_COUNT1(, ##__VA_ARGS__)
#define _ARGUMENT_COUNT1(...) _ARGUMENT_COUNT2(__VA_ARGS__, 5, 4, 3, 2, 1, 0)
#define _ARGUMENT_COUNT2(_, x0, x1, x2, x3, x4, argc, ...) argc

/* write to the system event log facilities - using syslogd or EventLog */
/* For true portability we have to get the message strings as well as
   the argument count out of the .mc files.  It will be nice to get them
   compile time type checked by way of having the format strings as
   static arguments for LOGging.  (As for the explicit number of substitions,
   we can easily count args with macros, but for now we'll at least have to
   lookup the number of arguments in events.mc.)
*/
#include "event_strings.h"
#ifdef WINDOWS
#    include "events.h"
#endif

extern const char *exception_label_core;
extern const char *exception_label_client;
/* These should be the same size for our report_exception_skip_prefix() */
#define CRASH_NAME "internal crash"
#define STACK_OVERFLOW_NAME "stack overflow"

/* pass NULL to use defaults */
void
set_exception_strings(const char *override_label, const char *override_url);

void
set_display_version(const char *ver);

void
report_dynamorio_problem(dcontext_t *dcontext, uint dumpcore_flag, app_pc exception_addr,
                         app_pc report_ebp, const char *fmt, ...);

void
report_app_problem(dcontext_t *dcontext, uint appfault_flag, app_pc pc, app_pc report_ebp,
                   const char *fmt, ...);

void
d_r_notify(syslog_event_type_t priority, bool internal, bool synch,
           IF_WINDOWS_(uint message_id) uint substitution_nam, const char *prefix,
           const char *fmt, ...);

#define SYSLOG_COMMON(synch, type, id, sub, ...)                                        \
    d_r_notify(type, false, synch, IF_WINDOWS_(MSG_##id) sub, #type, MSG_##id##_STRING, \
               __VA_ARGS__)

#define SYSLOG_INTERNAL_COMMON(synch, type, ...) \
    d_r_notify(type, true, synch, IF_WINDOWS_(MSG_INTERNAL_##type) 0, #type, __VA_ARGS__)

/* For security messages use passed in fmt string instead of eventlog fmt
 * string for LOG/stderr/msgbox to avoid breaking our regression suite,
 * NOTE assumes actual id passed, not name of id less MSG_ (so can use array
 * of id's in vmareas.c, another reason need separate fmt string)
 * FIXME : use events.mc string instead (SYSLOG_COMMON), breaks regression
 * This is now used for out-of-memory as well, for the same reason --
 * we should have a mechanism to strip the application name & pid prefix,
 * then we could use the eventlog string.
 */
#define SYSLOG_CUSTOM_NOTIFY(type, id, sub, ...) \
    d_r_notify(type, false, true, IF_WINDOWS_(id) sub, #type, __VA_ARGS__)

#define SYSLOG(type, id, sub, ...) SYSLOG_COMMON(true, type, id, sub, __VA_ARGS__)
#define SYSLOG_NO_OPTION_SYNCH(type, id, sub, ...) \
    SYSLOG_COMMON(false, type, id, sub, __VA_ARGS__)

#if defined(INTERNAL) && !defined(STANDALONE_DECODER)
#    define SYSLOG_INTERNAL(type, ...) SYSLOG_INTERNAL_COMMON(true, type, __VA_ARGS__)
#    define SYSLOG_INTERNAL_NO_OPTION_SYNCH(type, ...) \
        SYSLOG_INTERNAL_COMMON(false, type, __VA_ARGS__)
#else
#    define SYSLOG_INTERNAL(...)
#    define SYSLOG_INTERNAL_NO_OPTION_SYNCH(...)
#endif /* INTERNAL */

/* for convenience */
#define SYSLOG_INTERNAL_INFO(...) SYSLOG_INTERNAL(SYSLOG_INFORMATION, __VA_ARGS__)
#define SYSLOG_INTERNAL_WARNING(...) SYSLOG_INTERNAL(SYSLOG_WARNING, __VA_ARGS__)
#define SYSLOG_INTERNAL_ERROR(...) SYSLOG_INTERNAL(SYSLOG_ERROR, __VA_ARGS__)
#define SYSLOG_INTERNAL_CRITICAL(...) SYSLOG_INTERNAL(SYSLOG_CRITICAL, __VA_ARGS__)

#define SYSLOG_INTERNAL_INFO_ONCE(...) DODEBUG_ONCE(SYSLOG_INTERNAL_INFO(__VA_ARGS__))
#define SYSLOG_INTERNAL_WARNING_ONCE(...) \
    DODEBUG_ONCE(SYSLOG_INTERNAL_WARNING(__VA_ARGS__))
#define SYSLOG_INTERNAL_ERROR_ONCE(...) DODEBUG_ONCE(SYSLOG_INTERNAL_ERROR(__VA_ARGS__))
#define SYSLOG_INTERNAL_CRITICAL_ONCE(...) \
    DODEBUG_ONCE(SYSLOG_INTERNAL_CRITICAL(__VA_ARGS__))

#define FATAL_ERROR_EXIT_CODE 40

#define REPORT_FATAL_ERROR_AND_EXIT(msg_id, arg_count, ...)                     \
    do {                                                                        \
        /* Right now we just print an error message. In the future              \
         * it may make sense to generate a core dump too.                       \
         */                                                                     \
        SYSLOG_COMMON(false, SYSLOG_CRITICAL, msg_id, arg_count, __VA_ARGS__);  \
        /* We hard-code NULL for the dcontext because we know it isn't used     \
         * with TERMINATE_PROCESS or our specific error code.                   \
         */                                                                     \
        os_terminate_with_code(NULL, TERMINATE_PROCESS, FATAL_ERROR_EXIT_CODE); \
        ASSERT_NOT_REACHED();                                                   \
    } while (0)

/* FIXME, eventually want usage_error to also be external (may also eventually
 * need non dynamic option synch form as well for usage errors while updating
 * dynamic options), but lot of work to get all in eventlog and currently only
 * really triggered by internal options */
/* FIXME : could leave out the asserts, this is a recoverable error */
#define USAGE_ERROR(...)                    \
    do {                                    \
        SYSLOG_INTERNAL_ERROR(__VA_ARGS__); \
        ASSERT_NOT_REACHED();               \
    } while (0)
#define FATAL_USAGE_ERROR(id, sub, ...)                                                \
    do {                                                                               \
        /* synchronize dynamic options for dumpcore_mask */                            \
        synchronize_dynamic_options();                                                 \
        if (TEST(DUMPCORE_FATAL_USAGE_ERROR, DYNAMO_OPTION_NOT_STRING(dumpcore_mask))) \
            os_dump_core("fatal usage error");                                         \
        SYSLOG(SYSLOG_CRITICAL, id, sub, __VA_ARGS__);                                 \
        os_terminate(NULL, TERMINATE_PROCESS);                                         \
    } while (0)
#define OPTION_PARSE_ERROR(id, sub, ...)                            \
    do {                                                            \
        SYSLOG_NO_OPTION_SYNCH(SYSLOG_ERROR, id, sub, __VA_ARGS__); \
        DODEBUG(os_terminate(NULL, TERMINATE_PROCESS););            \
    } while (0)

#ifdef DEBUG
/* only for temporary tracing - do not leave in source, use make ugly to remind you */
#    define TRACELOG(level) LOG(GLOBAL, LOG_TOP, level, "%s:%d ", __FILE__, __LINE__)
#else
#    define TRACELOG(level)
#endif

/* what-to-log bitmask values */
/* N.B.: if these constants are changed, win32gui must also be changed!
 * They are also duplicated in instrument.h -- too hard to get them to
 * automatically show up in right place in header files for release.
 */
#define LOG_NONE 0x00000000
#define LOG_STATS 0x00000001
#define LOG_TOP 0x00000002
#define LOG_THREADS 0x00000004
#define LOG_SYSCALLS 0x00000008
#define LOG_ASYNCH 0x00000010
#define LOG_INTERP 0x00000020
#define LOG_EMIT 0x00000040
#define LOG_LINKS 0x00000080
#define LOG_CACHE 0x00000100
#define LOG_FRAGMENT 0x00000200
#define LOG_DISPATCH 0x00000400
#define LOG_MONITOR 0x00000800
#define LOG_HEAP 0x00001000
#define LOG_VMAREAS 0x00002000
#define LOG_SYNCH 0x00004000
#define LOG_MEMSTATS 0x00008000
#define LOG_OPTS 0x00010000
#define LOG_SIDELINE 0x00020000
#define LOG_SYMBOLS 0x00040000
#define LOG_RCT 0x00080000
#define LOG_NT 0x00100000
#define LOG_HOT_PATCHING 0x00200000
#define LOG_HTABLE 0x00400000
#define LOG_MODULEDB 0x00800000
#define LOG_LOADER 0x01000000
#define LOG_CLEANCALL 0x02000000
#define LOG_ANNOTATIONS 0x04000000
#define LOG_VIA_ANNOTATIONS 0x08000000

#define LOG_ALL_RELEASE 0x0fe0ffff
#define LOG_ALL 0x0fffffff

#ifdef WINDOWS_PC_SAMPLE
#    define LOG_PROFILE LOG_ALL
#endif

/* buffer size supposed to handle undecorated names like
 * "kernel32!CreateFile" or "kernel32.dll!CreateProcess" or ranges as
 * needed by print_symbolic_address() */
#define MAXIMUM_SYMBOL_LENGTH 80

#ifdef DEBUG
#    define PRINT_TIMESTAMP_MAX_LENGTH 32

/* given an array of size size of integers, computes and prints the
 * min, max, mean, and stddev
 */
void
print_statistics(int *data, int size);
/* global and thread local STATS */
void
dump_global_stats(bool raw);
void
dump_thread_stats(dcontext_t *dcontext, bool raw);
void
stats_thread_init(dcontext_t *dcontext);
void
stats_thread_exit(dcontext_t *dcontext);
uint
print_timestamp_to_buffer(char *buffer, size_t len);
uint
d_r_print_timestamp(file_t logfile);

/* prints a symbolic name, or best guess of it into a caller provided buffer */
void
print_symbolic_address(app_pc tag, char *buf, int max_chars, bool exact_only);

#endif /* DEBUG */

void
dump_global_rstats_to_stderr(void);

bool
under_internal_exception(void);

enum {
    DUMP_NO_QUOTING = 0x01000,   // no quoting for C string replay
    DUMP_OCTAL = 0x02000,        // hex otherwise
    DUMP_NO_CHARS = 0x04000,     // no printable characters
    DUMP_RAW = 0x08000,          // do not keep as a string
    DUMP_DWORD = 0x10000,        // dump as 4-byte chunks
    DUMP_ADDRESS = 0x20000,      // prepend address before each line of output
    DUMP_APPEND_ASCII = 0x40000, // append printable ASCII after each line
    DUMP_PER_LINE = 0x000ff      // mask for bytes per line flag
};
#define DUMP_PER_LINE_DEFAULT 16

/* dump buffer in a desired hex/octal/char combination */
void
dump_buffer_as_bytes(file_t logfile, void *buf, size_t len, int flags);

bool
is_readable_without_exception_try(byte *pc, size_t size);
/* Note: this is OS independent version
 * cf. is_readable_without_exception() and is_readable_without_exception_query_os()
 */

/* NOTE - uses try/except when possible, is_readable_without_exception otherwise. NOTE -
 * like our other is_readable_without_exception routines this routine offers no guarantee
 * that the string will remain valid after the call returns.  FIXME - may be worthwhile
 * to extend this routine to copy the string into a buffer while in the try/using
 * safe_read instead of making the caller do it. */
bool
is_string_readable_without_exception(char *str, size_t *str_length /* OPTIONAL OUT */);

bool
safe_write_try_except(void *base, size_t size, const void *in_buf, size_t *bytes_written);

#ifdef DEBUG
bool
is_valid_xml_char(char c);
#endif

/* prints str, escaping out char or char seq. for a xml CDATA block */
void
print_xml_cdata(file_t f, const char *str);

/* Reads the full file, returns it in a buffer and sets buf_len to the size
 * of the buffer allocated on the specified heap, if successful.  Returns NULL
 * and sets buf_len to 0 on failure.
 */
char *
read_entire_file(const char *file,
                 size_t *buf_len /* OUT */
                     HEAPACCT(which_heap_t heap));

/* returns false if we are too low on disk to create a file of desired size */
bool
check_low_disk_threshold(file_t f, uint64 new_file_size);

/* MD5 */
/* Note: MD5 is only 16 bytes in length, but it is usually used as a
 * string, so each byte will result in 2 chars being used.
 */
#define MD5_BLOCK_LENGTH 64
#define MD5_RAW_BYTES 16
#define MD5_STRING_LENGTH (2 * MD5_RAW_BYTES)

/* To compute the message digest of several chunks of bytes, declare
 * an MD5Context structure, pass it to d_r_md5_init, call d_r_md5_update as
 * needed on buffers full of bytes, and then call d_r_md5_final, which will
 * fill a supplied 16-byte array with the digest.
 */
struct MD5Context {
    uint32 state[4];                        /* state */
    uint64 count;                           /* number of bits, mod 2^64 */
    unsigned char buffer[MD5_BLOCK_LENGTH]; /* input buffer */
};

void
d_r_md5_init(struct MD5Context *ctx);
void
d_r_md5_update(struct MD5Context *ctx, const unsigned char *buf, size_t len);
void
d_r_md5_final(unsigned char digest[16], struct MD5Context *ctx);

#ifdef PROCESS_CONTROL
bool
get_md5_for_file(const char *file, char *hash_buf /* OUT */);
#endif
const char *
get_application_md5(void);
void
get_md5_for_region(const byte *region_start, uint len,
                   unsigned char digest[MD5_RAW_BYTES] /* OUT */);
bool
md5_digests_equal(const byte digest1[MD5_RAW_BYTES], const byte digest2[MD5_RAW_BYTES]);

void
print_version_and_app_info(file_t file);

/* returns a pseudo random number in [0, max_offset)
 * Not crypto strong, and not thread safe!
 */
size_t
get_random_offset(size_t max_offset);

void
d_r_set_random_seed(uint seed);

uint
d_r_get_random_seed(void);

void
convert_millis_to_date(uint64 millis, dr_time_t *time OUT);

void
convert_date_to_millis(const dr_time_t *dr_time, uint64 *millis OUT);

uint
d_r_crc32(const char *buf, const uint len);
void
utils_init(void);
void
utils_exit(void);

#ifdef WINDOWS
#    ifdef isprint
#        undef isprint
bool
isprint(int c);
#    endif

#    ifdef isdigit
#        undef isdigit
bool
isdigit(int c);
#    endif
#endif

#define isprint_fast(c) (((c) >= 0x20) && ((c) < 0x7f))
#define isdigit_fast(c) (((c) >= '0') && ((c) <= '9'))

#ifdef UNIX
/* PR 251709 / PR 257565: avoid __ctype_b linking issues for standalone
 * and start/stop clients.  We simply avoid linking with the locale code
 * altogether.
 */
#    ifdef isprint
#        undef isprint
#    endif
#    ifdef isdigit
#        undef isdigit
#    endif
#    define isprint isprint_HAS_LINK_ERRORS_USE_isprint_fast_INSTEAD
#    define isdigit isdigit_HAS_LINK_ERRORS_USE_isdigit_fast_INSTEAD
/* to avoid calling isprint()/isdigit() and localization tables
 * xref PR 251709 / PR 257565
 */
#endif

/* a little utiliy for printing a float that is formed by dividing 2 ints,
 * gives back high and low parts for printing, also supports percentages
 * Usage : given a, b (uint[64]); d_int()==divide_uint64_print();
 *         uint c, d tmp; parameterized on precision p and width w
 * note that %f is eqv. to %.6f, 3rd ex. is a percentage ex.
 * "%.pf", a/(float)b => d_int(a, b, false, p, &c, &d);  "%u.%.pu", c,d
 * "%w.pf", a/(float)b => d_int(a, b, false, p, &c, &d); "%(w-p)u.%.pu", c,d
 * "%.pf%%", 100*(a/(float)b) => d_int(a, b, true, p, &c, &d); "%u.%.pu%%", c,d
 */
void
divide_uint64_print(uint64 numerator, uint64 denominator, bool percentage, uint precision,
                    uint *top, uint *bottom);

/* For printing a float.
 * NOTE: You must preserve x87 floating point state to call this function, unless
 * you can prove the compiler will never use x87 state for float operations.
 * Usage : given double/float a; uint c, d and char *s tmp; dp==double_print
 *         parameterized on precision p width w
 * note that %f is eqv. to %.6f
 * "%.pf", a => dp(a, p, &c, &d, &s) "%s%u.%.pu", s, c, d
 * "%w.pf", a => dp(a, p, &c, &d, &s) "%s%(w-p)u.%.pu", s, c, d
 */
void
double_print(double val, uint precision, uint *top, uint *bottom, const char **sign);

#ifdef CALL_PROFILE
/* Max depth of call stack to maintain.
 * We actually maintain DYNAMO_OPTION(prof_caller) depth.
 */
#    define MAX_CALL_PROFILE_DEPTH 8
void
profile_callers(void);
void
profile_callers_exit(void);
#endif

#ifdef STANDALONE_UNIT_TEST

/* an ASSERT replacement for use in unit tests */
#    define FAIL() EXPECT(true, false)
#    define EXPECT(expr, expected)                                     \
        do {                                                           \
            ptr_uint_t value_once = (ptr_uint_t)(expr);                \
            EXPECT_RELATION_INTERNAL(#expr, value_once, ==, expected); \
        } while (0)

#    define EXPECT_EQ(expr, expected)                                  \
        do {                                                           \
            ptr_uint_t value_once = (ptr_uint_t)(expr);                \
            EXPECT_RELATION_INTERNAL(#expr, value_once, ==, expected); \
        } while (0)

#    define EXPECT_NE(expr, expected)                                  \
        do {                                                           \
            ptr_uint_t value_once = (ptr_uint_t)(expr);                \
            EXPECT_RELATION_INTERNAL(#expr, value_once, !=, expected); \
        } while (0)

#    define EXPECT_RELATION(expr, REL, expected)                        \
        do {                                                            \
            ptr_uint_t value_once = (ptr_uint_t)(expr);                 \
            EXPECT_RELATION_INTERNAL(#expr, value_once, REL, expected); \
        } while (0)

#    define EXPECT_RELATION_INTERNAL(exprstr, value, REL, expected)                     \
        do {                                                                            \
            LOG(GLOBAL, LOG_ALL, 1, "%s = %d [expected " #REL " %d] %s\n", exprstr,     \
                value_once, expected,                                                   \
                value_once REL(ptr_uint_t) expected ? "good" : "BAD");                  \
            /* Avoid ASSERT to support a release build. */                              \
            if (!(value REL(ptr_uint_t) expected)) {                                    \
                print_file(STDERR, "EXPECT failed at %s:%d in test %s: %s\n", __FILE__, \
                           __LINE__, __FUNCTION__, exprstr);                            \
                os_terminate(NULL, TERMINATE_PROCESS);                                  \
            }                                                                           \
        } while (0)

#    define EXPECT_STR(expr, expected, n)                                               \
        do {                                                                            \
            const char *value_once = expr;                                              \
            bool ok = strncmp(value_once, expected, n) == 0;                            \
            LOG(GLOBAL, LOG_ALL, 1, #expr " = %s [expected == %s] %s\n", value_once,    \
                expected, ok ? "good" : "BAD");                                         \
            /* Avoid ASSERT to support a release build. */                              \
            if (!ok) {                                                                  \
                print_file(STDERR, "EXPECT failed at %s:%d in test %s: %s\n", __FILE__, \
                           __LINE__, __FUNCTION__, #expr);                              \
                os_terminate(NULL, TERMINATE_PROCESS);                                  \
            }                                                                           \
        } while (0)

#    define TESTRUN(test)                                             \
        do {                                                          \
            test_number++;                                            \
            print_file(STDERR, "Test %d: " #test ":\n", test_number); \
            test;                                                     \
            print_file(STDERR, "\t" #test " [OK]\n");                 \
        } while (0)

/* define this on top of each unit test, and of course unit_main()
 * has to be declared.
 * Note that unit_main() is
 * responsible for calling standalone_init() and any other
 * initialization routines as needed
 */
#    define UNIT_TEST_MAIN                                 \
        uint test_number = 0;                              \
        static int unit_main(void);                        \
        int main()                                         \
        {                                                  \
            print_file(STDERR, "%s:\n", __FILE__);         \
            unit_main();                                   \
            print_file(STDERR, "%d tests\n", test_number); \
            print_file(STDERR, "%s: SUCCESS\n", __FILE__); \
            return 0;                                      \
        }

#endif /* STANDALONE_UNIT_TEST */

/* internal strdup */
char *
dr_strdup(const char *str HEAPACCT(which_heap_t which));

#ifdef WINDOWS
/* Allocates a new char* (NOT a new wchar_t*) from a wchar_t* */
char *
dr_wstrdup(const wchar_t *str HEAPACCT(which_heap_t which));
#endif

/* Frees a char* string (NOT a wchar_t*) allocated via dr_strdup or
 * dr_wstrdup that has not been modified since being copied!
 */
void
dr_strfree(const char *str HEAPACCT(which_heap_t which));

/* Merges two arrays, treating their elements as type void*.
 * Allocates a new array on dcontext's heap for the result
 * and returns it and its size.
 */
void
array_merge(dcontext_t *dcontext, bool intersect /* else union */, void **src1,
            uint src1_num, void **src2, uint src2_num,
            /*OUT*/ void ***dst, /*OUT*/ uint *dst_num HEAPACCT(which_heap_t which));

/* Implementation for dr_get_stats() */
bool
stats_get_snapshot(dr_stats_t *drstats);

#endif /* _UTILS_H_ */
