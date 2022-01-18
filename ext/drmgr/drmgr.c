/* **********************************************************
 * Copyright (c) 2010-2021 Google, Inc.   All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DynamoRIO Multi-Instrumentation Manager Extension: a mediator for
 * combining and coordinating multiple instrumentation passes
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drmgr_priv.h"
#include "hashtable.h"
#include "../ext_utils.h"
#ifdef UNIX
#    include <string.h>
#endif
#include <stddef.h> /* offsetof */
#include <stdint.h> /* intptr */

#undef dr_register_bb_event
#undef dr_unregister_bb_event
#undef dr_get_tls_field
#undef dr_set_tls_field
#undef dr_insert_read_tls_field
#undef dr_insert_write_tls_field
#undef dr_register_thread_init_event
#undef dr_unregister_thread_init_event
#undef dr_register_thread_exit_event
#undef dr_unregister_thread_exit_event
#undef dr_register_pre_syscall_event
#undef dr_unregister_pre_syscall_event
#undef dr_register_post_syscall_event
#undef dr_unregister_post_syscall_event
#undef dr_register_module_load_event
#undef dr_unregister_module_load_event
#undef dr_register_module_unload_event
#undef dr_unregister_module_unload_event
#undef dr_register_kernel_xfer_event
#undef dr_unregister_kernel_xfer_event
#undef dr_register_signal_event
#undef dr_unregister_signal_event
#undef dr_register_exception_event
#undef dr_unregister_exception_event
#undef dr_register_restore_state_ex_event
#undef dr_unregister_restore_state_ex_event
#undef dr_register_low_on_memory_event
#undef dr_unregister_low_on_memory_event

/* currently using asserts on internal logic sanity checks (never on
 * input from user) but perhaps we shouldn't since this is a library
 */
#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define IF_DEBUG(x) x
#else
#    define ASSERT(x, msg) /* nothing */
#    define IF_DEBUG(x)    /* nothing */
#endif

/***************************************************************************
 * TYPES
 */

/* priority list entry base struct */
typedef struct _priority_event_entry_t {
    bool valid; /* is whole entry valid (not just priority) */
    /* Given as input by the user. Enables reorder of cb lists. */
    drmgr_priority_t in_priority;
} priority_event_entry_t;

/* bb event list entry */
typedef struct _cb_entry_t {
    priority_event_entry_t pri;
    void *registration_user_data; /* user data passed at registration time */
    bool has_quintet;
    bool has_pair;
    bool is_opcode_insertion;
    union {
        drmgr_xform_cb_t xform_cb;
        struct {
            drmgr_analysis_cb_t analysis_cb;
            drmgr_insertion_cb_t insertion_cb;
        } pair;

        /* quintet */
        drmgr_app2app_ex_cb_t app2app_ex_cb;
        struct {
            drmgr_ilist_ex_cb_t analysis_ex_cb;
            drmgr_insertion_cb_t insertion_ex_cb;
        } pair_ex;
        drmgr_ilist_ex_cb_t instru2instru_ex_cb;
        drmgr_opcode_insertion_cb_t opcode_insertion_cb;
        drmgr_ilist_ex_cb_t meta_instru_ex_cb;
    } cb;
} cb_entry_t;

/* generic event list entry */
typedef struct _generic_event_entry_t {
    priority_event_entry_t pri;
    bool is_ex;
    bool is_using_user_data;
    void *user_data;
    union {
        void (*generic_cb)(void);
        union {
            void (*cb_no_user_data)(void *);
            void (*cb_user_data)(void *, void *);
        } thread_cb;
        void (*cls_cb)(void *, bool);
        union {
            bool (*cb_no_user_data)(void *, int);
            bool (*cb_user_data)(void *, int, void *);
        } presys_cb;
        union {
            void (*cb_no_user_data)(void *, int);
            void (*cb_user_data)(void *, int, void *);
        } postsys_cb;
        union {
            void (*cb_no_user_data)(void *, const module_data_t *, bool);
            void (*cb_user_data)(void *, const module_data_t *, bool, void *);
        } modload_cb;
        union {
            void (*cb_no_user_data)(void *, const module_data_t *);
            void (*cb_user_data)(void *, const module_data_t *, void *);
        } modunload_cb;
        union {
            void (*cb_no_user_data)();
            void (*cb_user_data)(void *);
        } low_on_memory_cb;
        void (*kernel_xfer_cb)(void *, const dr_kernel_xfer_info_t *);
#ifdef UNIX
        union {
            dr_signal_action_t (*cb_no_user_data)(void *, dr_siginfo_t *);
            dr_signal_action_t (*cb_user_data)(void *, dr_siginfo_t *, void *);
        } signal_cb;
#endif
#ifdef WINDOWS
        bool (*exception_cb)(void *, dr_exception_t *);
#endif
        void (*fault_cb)(void *, void *, dr_mcontext_t *, bool, bool);
        bool (*fault_ex_cb)(void *, bool, dr_restore_state_info_t *);
    } cb;
} generic_event_entry_t;

/* array of events */
/* XXX: what we want is drvector_t with inlined elements */
typedef struct _cb_list_t {
    union { /* array of callbacks */
        cb_entry_t *bb;
        generic_event_entry_t *generic;
        byte *array;
    } cbs;
    size_t entry_sz;  /* size of each entry */
    size_t num_def;   /* defined (may not all be valid) entries in array */
    size_t num_valid; /* valid entries in array */
    size_t capacity;  /* allocated entries in array */
    /* We support only keeping events when a user has requested them.
     * This helps with things like DR's assert about a filter event (i#2991).
     */
    void (*lazy_register)(void);
    void (*lazy_unregister)(void);
} cb_list_t;

#define EVENTS_INITIAL_SZ 10

/* Denotes the number of cb entries that are stored on the stack as
 * used by a local_cb_info_t value. This is primarily used as an
 * optimization to avoid head allocation usage. As a result,
 * a local_cb_info_t value takes a lot of space on the stack, which
 * could lead to a "__chkstk" compile time error on Windows. If such an
 * error is being encountered, reducing EVENTS_STACK_SZ might help
 * circumvent the issue.
 */
#define EVENTS_STACK_SZ 10

/* Our own TLS data */
typedef struct _per_thread_t {
    drmgr_bb_phase_t cur_phase;
    instr_t *first_instr;
    instr_t *first_nonlabel_instr;
    instr_t *last_instr;
    emulated_instr_t emulation_info;
    bool in_emulation_region;
    /* This field stores the current insertion event instruction so that
     * separate query APIs don't need to take in the instruction.
     */
    instr_t *insertion_instr;
} per_thread_t;

/* Emulation note types */
enum {
    DRMGR_NOTE_EMUL_START,
    DRMGR_NOTE_EMUL_STOP,
    DRMGR_NOTE_EMUL_COUNT,
};

/* Used to store temporary local information when handling drmgr's bb event in order
 * to avoid holding a lock during the instrumentation process.
 */
typedef struct _local_ctx_t {
    cb_list_t iter_app2app;
    cb_list_t iter_insert;
    cb_list_t iter_instru;
    cb_list_t iter_meta_instru;
    /* used as stack storage by the cb if large enough: */
    cb_entry_t app2app[EVENTS_STACK_SZ];
    cb_entry_t insert[EVENTS_STACK_SZ];
    cb_entry_t instru[EVENTS_STACK_SZ];
    cb_entry_t meta_instru[EVENTS_STACK_SZ];
    /* for opcode instrumentation events: */
    cb_list_t *iter_opcode_insert;
    bool was_opcode_instrum_registered;
    /* for user-data: */
    int pair_count;
    int quintet_count;
    /* for bbdup events: */
    bool is_bbdup_enabled;
    drmgr_bbdup_duplicate_bb_cb_t bbdup_duplicate_cb;
    drmgr_bbdup_extract_cb_t bbdup_extract_cb;
    drmgr_bbdup_stitch_cb_t bbdup_stitch_cb;
    drmgr_bbdup_insert_encoding_cb_t bbdup_insert_encoding_cb;
    cb_list_t iter_pre_bbdup;
    cb_entry_t pre_bbdup[EVENTS_STACK_SZ];
} local_cb_info_t;

/***************************************************************************
 * GLOBALS
 */

/* Using read-write locks to protect counts and lists to allow concurrent
 * bb events and only require mutual exclusion when a cb is registered
 * or unregistered, which should be rare.
 *
 * We also use this lock for opcode specific instrumentation.
 */
static void *bb_cb_lock;

/* To know whether we need any DR events; protected by bb_cb_lock */
static uint bb_event_count;

/* Lists sorted by priority and protected by bb_cb_lock */
static cb_list_t cblist_app2app;
static cb_list_t cblist_instrumentation;
static cb_list_t cblist_instru2instru;
static cb_list_t cblist_meta_instru;

/* For opcode specific instrumentation. */
static hashtable_t global_opcode_instrum_table; /* maps opcodes to cb lists */
static void *opcode_table_lock; /* Denotes whether opcode event was ever registered. */
/* A flag indicating whether opcode instrum was ever registered. It is protected
 * by bb_cb_lock.
 */
static bool was_opcode_instrum_registered;

/* Count of callbacks needing user_data, protected by bb_cb_lock */
static uint pair_count;
static uint quintet_count;

/* Priority used for non-_ex events */
static const drmgr_priority_t default_priority = { sizeof(default_priority),
                                                   "__DEFAULT__", NULL, NULL, 0 };

static int our_tls_idx;

static dr_emit_flags_t
drmgr_bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating);

static void
drmgr_bb_init(void);

static void
drmgr_bb_exit(void);

/* Reserve space for emulation specific note values */
static ptr_uint_t note_base_emul;
static void
drmgr_emulation_init(void);

/* Size of tls/cls arrays.  In order to support slot access from the
 * code cache, this number cannot be changed dynamically.  We could
 * make it a runtime parameter, but that would add another level of
 * indirection (and thus a performance hit); plus, there should only
 * be one client (maybe split into a few components) and a handful of
 * libraries, so we leave it as static.
 */
#define MAX_NUM_TLS 64

/* Strategy: each cb level clones tls[] and makes a new cls[].
 * We could share tls[] but that would cost a level of indirection.
 */
typedef struct _tls_array_t {
    void *tls[MAX_NUM_TLS];
    void *cls[MAX_NUM_TLS];
    struct _tls_array_t *prev;
    struct _tls_array_t *next;
} tls_array_t;

/* Whether each slot is reserved.  Protected by tls_lock. */
static bool tls_taken[MAX_NUM_TLS];
static bool cls_taken[MAX_NUM_TLS];
static void *tls_lock;

static void *note_lock;

/* Thread event cbs and rwlock */
static cb_list_t cb_list_thread_init;
static cb_list_t cb_list_thread_exit;
static void *thread_event_lock;

static cb_list_t cblist_cls_init;
static cb_list_t cblist_cls_exit;
static void *cls_event_lock;

/* Yet another event we must wrap to ensure we go last */
static cb_list_t cblist_presys;
static void *presys_event_lock;

static cb_list_t cblist_postsys;
static void *postsys_event_lock;

static cb_list_t cblist_modload;
static void *modload_event_lock;

static cb_list_t cblist_modunload;
static void *modunload_event_lock;

static cb_list_t cblist_low_on_memory;
static void *low_on_memory_event_lock;

static cb_list_t cblist_kernel_xfer;
static void *kernel_xfer_event_lock;

/* For drbbdup integration. Protected by bb_cb_lock. */
static drmgr_bbdup_duplicate_bb_cb_t bbdup_duplicate_cb;
static drmgr_bbdup_insert_encoding_cb_t bbdup_insert_encoding_cb;
static drmgr_bbdup_extract_cb_t bbdup_extract_cb;
static drmgr_bbdup_stitch_cb_t bbdup_stitch_cb;
static cb_list_t cblist_pre_bbdup;

#ifdef UNIX
static cb_list_t cblist_signal;
static void *signal_event_lock;
#endif

#ifdef WINDOWS
static cb_list_t cblist_exception;
static void *exception_event_lock;
#endif

static cb_list_t cblist_fault;
static void *fault_event_lock;
static bool registered_fault; /* for lazy registration */

static int
priority_event_add(cb_list_t *list, drmgr_priority_t *new_pri);

static void
drmgr_init_opcode_hashtable(hashtable_t *opcode_instrum_table);

static void
drmgr_thread_init_event(void *drcontext);

static void
drmgr_thread_exit_event(void *drcontext);

static bool
drmgr_cls_stack_init(void *drcontext);

static bool
drmgr_cls_stack_exit(void *drcontext);

static void
drmgr_event_init(void);

static void
drmgr_event_exit(void);

static bool
drmgr_presyscall_event(void *drcontext, int sysnum);

static void
drmgr_postsyscall_event(void *drcontext, int sysnum);

static void
drmgr_modload_event(void *drcontext, const module_data_t *info, bool loaded);

static void
drmgr_modunload_event(void *drcontext, const module_data_t *info);

static void
drmgr_low_on_memory_event();

static void
drmgr_kernel_xfer_event(void *drcontext, const dr_kernel_xfer_info_t *info);

#ifdef UNIX
static dr_signal_action_t
drmgr_signal_event(void *drcontext, dr_siginfo_t *siginfo);
#endif

#ifdef WINDOWS
static bool
drmgr_exception_event(void *drcontext, dr_exception_t *excpt);
#endif

static bool
drmgr_restore_state_event(void *drcontext, bool restore_memory,
                          dr_restore_state_info_t *info);

static void
our_thread_init_event(void *drcontext);

static void
our_thread_exit_event(void *drcontext);

static bool
is_bbdup_enabled();

/***************************************************************************
 * INIT
 */

static int drmgr_init_count;

DR_EXPORT
bool
drmgr_init(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drmgr_init_count, 1);
    if (count > 1)
        return true;

    note_lock = dr_mutex_create();

    bb_cb_lock = dr_rwlock_create();
    thread_event_lock = dr_rwlock_create();
    tls_lock = dr_mutex_create();
    cls_event_lock = dr_rwlock_create();
    presys_event_lock = dr_rwlock_create();
    postsys_event_lock = dr_rwlock_create();
    modload_event_lock = dr_rwlock_create();
    modunload_event_lock = dr_rwlock_create();
    low_on_memory_event_lock = dr_rwlock_create();
    kernel_xfer_event_lock = dr_rwlock_create();
    opcode_table_lock = dr_rwlock_create();

#ifdef UNIX
    signal_event_lock = dr_rwlock_create();
#endif
#ifdef WINDOWS
    exception_event_lock = dr_rwlock_create();
#endif
    fault_event_lock = dr_rwlock_create();

    dr_register_thread_init_event(drmgr_thread_init_event);
    dr_register_thread_exit_event(drmgr_thread_exit_event);
    dr_register_module_load_event(drmgr_modload_event);
    dr_register_module_unload_event(drmgr_modunload_event);
    dr_register_low_on_memory_event(drmgr_low_on_memory_event);
    dr_register_kernel_xfer_event(drmgr_kernel_xfer_event);
#ifdef UNIX
    dr_register_signal_event(drmgr_signal_event);
#endif
#ifdef WINDOWS
    dr_register_exception_event(drmgr_exception_event);
#endif

    drmgr_bb_init();
    drmgr_event_init();
    drmgr_emulation_init();
    drmgr_init_opcode_hashtable(&global_opcode_instrum_table);

    our_tls_idx = drmgr_register_tls_field();
    if (!drmgr_register_thread_init_event(our_thread_init_event) ||
        !drmgr_register_thread_exit_event(our_thread_exit_event))
        return false;

    return true;
}

DR_EXPORT
void
drmgr_exit(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drmgr_init_count, -1);
    if (count != 0)
        return;

    drmgr_unregister_tls_field(our_tls_idx);
    drmgr_unregister_thread_init_event(our_thread_init_event);
    drmgr_unregister_thread_exit_event(our_thread_exit_event);

    hashtable_delete(&global_opcode_instrum_table);
    drmgr_bb_exit();
    drmgr_event_exit();

    dr_unregister_thread_init_event(drmgr_thread_init_event);
    dr_unregister_thread_exit_event(drmgr_thread_exit_event);

    /* We blindly unregister even if we never (lazily) registered. */
    dr_unregister_pre_syscall_event(drmgr_presyscall_event);
    dr_unregister_post_syscall_event(drmgr_postsyscall_event);

    dr_unregister_module_load_event(drmgr_modload_event);
    dr_unregister_module_unload_event(drmgr_modunload_event);
    dr_unregister_low_on_memory_event(drmgr_low_on_memory_event);
    dr_unregister_kernel_xfer_event(drmgr_kernel_xfer_event);
#ifdef UNIX
    dr_unregister_signal_event(drmgr_signal_event);
#endif
#ifdef WINDOWS
    dr_unregister_exception_event(drmgr_exception_event);
#endif

    if (bb_event_count > 0)
        dr_unregister_bb_event(drmgr_bb_event);

    if (registered_fault) {
        dr_unregister_restore_state_ex_event(drmgr_restore_state_event);
        registered_fault = false;
    }

    dr_rwlock_destroy(fault_event_lock);
#ifdef UNIX
    dr_rwlock_destroy(signal_event_lock);
#endif
#ifdef WINDOWS
    dr_rwlock_destroy(exception_event_lock);
#endif
    dr_rwlock_destroy(opcode_table_lock);
    dr_rwlock_destroy(kernel_xfer_event_lock);
    dr_rwlock_destroy(low_on_memory_event_lock);
    dr_rwlock_destroy(modunload_event_lock);
    dr_rwlock_destroy(modload_event_lock);
    dr_rwlock_destroy(postsys_event_lock);
    dr_rwlock_destroy(presys_event_lock);
    dr_rwlock_destroy(cls_event_lock);
    dr_mutex_destroy(tls_lock);
    dr_rwlock_destroy(thread_event_lock);
    dr_rwlock_destroy(bb_cb_lock);

    dr_mutex_destroy(note_lock);

    /* Reset global state to handle reattaches with statically linked DR
     * (no natural reset of global state), but only in case of detaching to
     * avoid the overhead when there is no chance of re-attaching.
     */
    if (dr_is_detaching()) {
        memset(tls_taken, 0, sizeof(tls_taken));
        memset(cls_taken, 0, sizeof(cls_taken));
        bb_event_count = 0;
        pair_count = 0;
        quintet_count = 0;
        was_opcode_instrum_registered = false;
        /* for drbbdup: */
        bbdup_duplicate_cb = NULL;
        bbdup_insert_encoding_cb = NULL;
        bbdup_extract_cb = NULL;
        bbdup_stitch_cb = NULL;
    }
}

/***************************************************************************
 * DYNAMIC ARAY OF CALLBACKS
 */

/* We want an array for a simple temporary copy to support unregistering
 * while delivering events.  Even though we sort our callbacks, we expect
 * orders of magnitude more event delivery than even register/unregister
 * calls.
 *
 * We do not shift on removal: we simply mark invalid.  We try to use
 * invalid slots when we can, but with the sorting that's rare.
 * We assume there won't be enough unregister calls to cause
 * fragmentation.
 */

static void
cblist_init(cb_list_t *l, size_t per_entry)
{
    l->entry_sz = per_entry;
    l->num_def = 0;
    l->num_valid = 0;
    l->capacity = EVENTS_INITIAL_SZ;
    l->cbs.array = dr_global_alloc(l->capacity * l->entry_sz);
    l->lazy_register = NULL;
    l->lazy_unregister = NULL;
}

static void
cblist_delete(cb_list_t *l)
{
    dr_global_free(l->cbs.array, l->capacity * l->entry_sz);
}

static inline priority_event_entry_t *
cblist_get_pri(cb_list_t *l, uint idx)
{
    return (priority_event_entry_t *)(l->cbs.array + idx * l->entry_sz);
}

/* Caller must hold write lock.
 * Increments l->num_def but not l->num_valid.
 * Returns -1 on error.
 */
static int
cblist_shift_and_resize(cb_list_t *l, uint insert_at)
{
    size_t orig_num = l->num_def;
    if (insert_at > l->num_def)
        return -1;
    /* check for invalid slots we can easily use */
    if (insert_at < l->num_def && !cblist_get_pri(l, insert_at)->valid)
        return insert_at;
    else if (insert_at > 0 && !cblist_get_pri(l, insert_at - 1)->valid)
        return insert_at - 1;
    l->num_def++;
    if (l->num_def >= l->capacity) {
        /* do the shift implicitly while resizing */
        size_t new_cap = l->capacity * 2;
        byte *new_array = dr_global_alloc(new_cap * l->entry_sz);
        memcpy(new_array, l->cbs.array, insert_at * l->entry_sz);
        memcpy(new_array + (insert_at + 1) * l->entry_sz,
               l->cbs.array + insert_at * l->entry_sz,
               (orig_num - insert_at) * l->entry_sz);
        dr_global_free(l->cbs.array, l->capacity * l->entry_sz);
        l->cbs.array = new_array;
        l->capacity = new_cap;
    } else {
        if (insert_at == orig_num)
            return insert_at;
        memmove(l->cbs.array + (insert_at + 1) * l->entry_sz,
                l->cbs.array + insert_at * l->entry_sz,
                (l->num_def - insert_at) * l->entry_sz);
    }
    return insert_at;
}

static void
cblist_insert_other(cb_list_t *l, cb_list_t *l_to_copy)
{
    drmgr_priority_t *pri = NULL;
    ASSERT(l->entry_sz == l_to_copy->entry_sz, "must be of the same size");
    size_t i;
    for (i = 0; i < l_to_copy->num_def; i++) {
        cb_entry_t *e = &l_to_copy->cbs.bb[i];
        if (!e->pri.valid)
            continue;

        pri = &e->pri.in_priority;
        if (strcmp(pri->name, default_priority.name) == 0) {
            /* Nullify here so that we do not bypass user validation done by
             * priority_event_add. */
            pri = NULL;
        }

        int idx = priority_event_add(l, pri);
        if (idx >= 0) {
            cb_entry_t *new_e = &l->cbs.bb[idx];
            *new_e = *e;
        }
    }
}

/* A helper function to help with the copying of cb list data for
 * cblist_create_local() and cblist_create_global().
 */
static void
cblist_copy(cb_list_t *src, cb_list_t *dst)
{
    ASSERT(src->num_def <= dst->capacity, "dst must have large enough capacity");
    dst->entry_sz = src->entry_sz;
    dst->num_def = src->num_def;
    dst->num_valid = src->num_valid;
    memcpy(dst->cbs.array, src->cbs.array, src->num_def * src->entry_sz);
}

/* Creates a temporary local copy, using local if it's big enough but
 * otherwise allocating new space on the heap. It allocates just enough space
 * for the list's array, using num_def instead of capacity for its size.
 * Caller must hold read lock. Use cblist_delete_local() to destroy.
 */
static void
cblist_create_local(void *drcontext, cb_list_t *src, cb_list_t *dst, byte *local,
                    size_t local_num)
{
    dst->capacity = src->num_def;
    if (src->num_def > local_num) {
        dst->cbs.array = dr_thread_alloc(drcontext, src->num_def * src->entry_sz);
    } else {
        dst->cbs.array = local;
    }
    cblist_copy(src, dst);
}

static void
cblist_delete_local(void *drcontext, cb_list_t *l, size_t local_num)
{
    if (l->num_def > local_num) {
        dr_thread_free(drcontext, l->cbs.array, l->num_def * l->entry_sz);
    } /* else nothing to do */
}

/* Creates a global copy of a cb list. Use cblist_delete() to destroy. */
static void
cblist_create_global(cb_list_t *src, cb_list_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->capacity = src->capacity;
    dst->cbs.array = dr_global_alloc(src->capacity * src->entry_sz);
    cblist_copy(src, dst);
}

/***************************************************************************
 * OPCODE INSTRUM HASHTABLE
 */

static void
drmgr_destroy_opcode_cb_list(void *opcode_cb_list_opaque)
{
    cb_list_t *opcode_cb_list = (cb_list_t *)opcode_cb_list_opaque;
    cblist_delete(opcode_cb_list);
    dr_global_free(opcode_cb_list, sizeof(cb_list_t));
}

static void
drmgr_init_opcode_hashtable(hashtable_t *opcode_instrum_table)
{
    hashtable_init_ex(opcode_instrum_table, 8, HASH_INTPTR, false, false,
                      drmgr_destroy_opcode_cb_list, NULL, NULL);
}

/* Returns false if opcode instrumentation is not applicable, i.e., no registration.
 */
static bool
drmgr_set_up_local_opcode_table(IN instrlist_t *bb, IN cb_list_t *insert_list,
                                INOUT hashtable_t *local_opcode_instrum_table)
{
    instr_t *inst, *next_inst;
    int opcode;
    bool is_opcode_instrum_applicable = false;

    dr_rwlock_read_lock(opcode_table_lock);

    /* A high-level check to avoid expensive allocation and locking bb_cb_lock.
     * In particular, we iterate over the bb and check whether any applicable opcode
     * event is registered.
     */
    for (inst = instrlist_first(bb); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        if (!instr_opcode_valid(inst))
            continue;
        opcode = instr_get_opcode(inst);
        cb_list_t *opcode_cb_list =
            hashtable_lookup(&global_opcode_instrum_table, (void *)(intptr_t)opcode);
        if (opcode_cb_list != NULL) {
            is_opcode_instrum_applicable = true;
            break;
        }
    }

    /* Just return if no opcode event is registered for any instruction in the BB. */
    if (!is_opcode_instrum_applicable) {
        dr_rwlock_read_unlock(opcode_table_lock);
        return false;
    }

    /* Since both opcode and insert events are handled during stage 3, they need to be
     * jointly organized according to their priorities. This is done now.
     *
     * An alternative approach is to do the merging upon registration of opcode
     * events, and updating all mapped cb lists of opcodes when further bb insert events
     * are registered/unregistered. However, in order to avoid races, this would require
     * the dr bb event processed by drmgr to create a local copy of the entire opcode
     * table every time.
     */
    dr_rwlock_read_lock(bb_cb_lock);
    for (inst = instrlist_first(bb); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        if (!instr_opcode_valid(inst))
            continue;

        opcode = instr_get_opcode(inst);
        cb_list_t *opcode_cb_list =
            hashtable_lookup(&global_opcode_instrum_table, (void *)(intptr_t)opcode);
        if (opcode_cb_list != NULL && opcode_cb_list->num_valid != 0) {
            cb_list_t *local_opcode_cb_list =
                hashtable_lookup(local_opcode_instrum_table, (void *)(intptr_t)opcode);
            if (local_opcode_cb_list == NULL) {
                local_opcode_cb_list = dr_global_alloc(sizeof(cb_list_t));
                cblist_create_global(insert_list, local_opcode_cb_list);
                cblist_insert_other(local_opcode_cb_list, opcode_cb_list);
                hashtable_add(local_opcode_instrum_table, (void *)(intptr_t)opcode,
                              local_opcode_cb_list);
            }
        }
    }
    dr_rwlock_read_unlock(bb_cb_lock);
    dr_rwlock_read_unlock(opcode_table_lock);
    return true;
}

/***************************************************************************
 * BB EVENTS
 */

/* To support multiple non-meta ctis in app2app phase, we mark them meta
 * before handing to DR to satisfy its bb constraints
 */
static void
drmgr_fix_app_ctis(void *drcontext, instrlist_t *bb)
{
    instr_t *inst;
    for (inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        /* Any CTI with an instr target must have an intra-bb target and thus
         * we assume it should not be mangled.  We mark it meta.
         */
        if (instr_is_app(inst) && instr_opcode_valid(inst) &&
            /* For -fast_client_decode we can have level 0 instrs so check
             * to ensure this is an single instr with valid opcode.
             */
            instr_is_cti(inst) && opnd_is_instr(instr_get_target(inst))) {
            instr_set_meta(inst);
            /* instrumentation passes should set the translation field
             * so other passes can see what app pc these app instrs
             * correspond to: but DR complains if there's a meta instr
             * w/ a translation but no restore_state event
             */
            instr_set_translation(inst, NULL);
        }
    }
}

/* Triggers the appropriate insert cb during stage 3. */
static dr_emit_flags_t
drmgr_bb_event_do_insertion_per_instr(void *drcontext, void *tag, instrlist_t *bb,
                                      instr_t *inst, bool for_trace, bool translating,
                                      cb_list_t *iter_insert, void **pair_data,
                                      void **quintet_data)
{
    uint i;
    cb_entry_t *e;
    uint pair_idx, quintet_idx;
    dr_emit_flags_t res = DR_EMIT_DEFAULT;

    for (quintet_idx = 0, pair_idx = 0, i = 0; i < iter_insert->num_def; i++) {
        e = &iter_insert->cbs.bb[i];
        if (!e->pri.valid)
            continue;
        /* Most client instrumentation wants to be predicated to match the app
         * instruction, so we do it by default (i#1723). Clients may opt-out
         * by calling drmgr_disable_auto_predication() at the start of the
         * insertion bb event.
         */
        instrlist_set_auto_predicate(bb, instr_get_predicate(inst));
        if (e->is_opcode_insertion) {
            res |= (*e->cb.opcode_insertion_cb)(drcontext, tag, bb, inst, for_trace,
                                                translating, e->registration_user_data);
        } else if (e->has_quintet) {
            res |=
                (*e->cb.pair_ex.insertion_ex_cb)(drcontext, tag, bb, inst, for_trace,
                                                 translating, quintet_data[quintet_idx]);
            quintet_idx++;
        } else {
            ASSERT(e->has_pair, "internal pair-vs-quintet state is wrong");
            if (e->cb.pair.insertion_cb != NULL) {
                res |= (*e->cb.pair.insertion_cb)(drcontext, tag, bb, inst, for_trace,
                                                  translating, pair_data[pair_idx]);
            }
            pair_idx++;
        }
        instrlist_set_auto_predicate(bb, DR_PRED_NONE);
        /* XXX: add checks that cb followed the rules */
    }
    return res;
}

static dr_emit_flags_t
drmgr_bb_event_do_instrum_phases(void *drcontext, void *tag, instrlist_t *bb,
                                 bool for_trace, bool translating, per_thread_t *pt,
                                 local_cb_info_t *local_info, void **pair_data,
                                 void **quintet_data)
{
    uint i;
    cb_entry_t *e;
    dr_emit_flags_t res = DR_EMIT_DEFAULT;
    instr_t *inst, *next_inst;
    uint pair_idx, quintet_idx;
    int opcode;
    /* denotes whether opcode instrumentation is applicable for this bb */
    bool is_opcode_instrum_applicable = false;
    hashtable_t local_opcode_instrum_table;

    /* Pass 1: app2app */
    /* XXX: better to avoid all this set_tls overhead and assume DR is globally
     * synchronizing bb building anyway and use a global var + mutex?
     */
    pt->cur_phase = DRMGR_PHASE_APP2APP;
    for (quintet_idx = 0, i = 0; i < local_info->iter_app2app.num_def; i++) {
        e = &local_info->iter_app2app.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quintet) {
            res |= (*e->cb.app2app_ex_cb)(drcontext, tag, bb, for_trace, translating,
                                          &quintet_data[quintet_idx]);
            quintet_idx++;
        } else
            res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    /* Pass 2: analysis */
    pt->cur_phase = DRMGR_PHASE_ANALYSIS;
    for (quintet_idx = 0, pair_idx = 0, i = 0; i < local_info->iter_insert.num_def; i++) {
        e = &local_info->iter_insert.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quintet) {
            res |= (*e->cb.pair_ex.analysis_ex_cb)(
                drcontext, tag, bb, for_trace, translating, quintet_data[quintet_idx]);
            quintet_idx++;
        } else {
            ASSERT(e->has_pair, "internal pair-vs-quintet state is wrong");
            if (e->cb.pair.analysis_cb == NULL) {
                pair_data[pair_idx] = NULL;
            } else {
                res |= (*e->cb.pair.analysis_cb)(drcontext, tag, bb, for_trace,
                                                 translating, &pair_data[pair_idx]);
            }
            pair_idx++;
        }
        /* XXX: add checks that cb followed the rules */
    }

    /* Pass 3: instru, per instr */
    pt->cur_phase = DRMGR_PHASE_INSERTION;
    pt->first_instr = instrlist_first(bb);
    pt->first_nonlabel_instr = instrlist_first_nonlabel(bb);
    pt->last_instr = instrlist_last(bb);
    pt->in_emulation_region = false; /* Just to be safe. */

    /* For opcode instrumentation:
     * We need to create a local copy of the opcode map.
     * This is done at this point because instructions inserted
     * in prior stages also need to be looked-up (including meta instructions) when
     * iterating over the basic block.
     *
     * Initialise table only if opcode events were ever registered by the user.
     */
    if (local_info->was_opcode_instrum_registered) {
        drmgr_init_opcode_hashtable(&local_opcode_instrum_table);
        is_opcode_instrum_applicable = drmgr_set_up_local_opcode_table(
            bb, &local_info->iter_insert, &local_opcode_instrum_table);
    }

    /* Main pass for instrumentation. */
    for (inst = instrlist_first(bb); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        pt->insertion_instr = inst;
        if (!pt->in_emulation_region && drmgr_is_emulation_start(inst)) {
            IF_DEBUG(bool ok =) drmgr_get_emulated_instr_data(inst, &pt->emulation_info);
            ASSERT(ok, "should be at emulation start label");
            pt->in_emulation_region = true;
            pt->emulation_info.flags |= DR_EMULATE_IS_FIRST_INSTR;
        }
        if (is_opcode_instrum_applicable && instr_opcode_valid(inst)) {
            opcode = instr_get_opcode(inst);
            local_info->iter_opcode_insert =
                hashtable_lookup(&local_opcode_instrum_table, (void *)(intptr_t)opcode);
            if (local_info->iter_opcode_insert != NULL) {
                res |= drmgr_bb_event_do_insertion_per_instr(
                    drcontext, tag, bb, inst, for_trace, translating,
                    local_info->iter_opcode_insert, pair_data, quintet_data);
                continue;
            }
        }
        res |= drmgr_bb_event_do_insertion_per_instr(
            drcontext, tag, bb, inst, for_trace, translating, &local_info->iter_insert,
            pair_data, quintet_data);
        if (pt->in_emulation_region) {
            pt->emulation_info.flags &= ~DR_EMULATE_IS_FIRST_INSTR;
            if (drmgr_is_emulation_end(inst) ||
                (TEST(DR_EMULATE_REST_OF_BLOCK, pt->emulation_info.flags) &&
                 drmgr_is_last_instr(drcontext, inst)))
                pt->in_emulation_region = false;
        }
    }

    /* Pass 4: instru optimizations */
    pt->cur_phase = DRMGR_PHASE_INSTRU2INSTRU;
    for (quintet_idx = 0, i = 0; i < local_info->iter_instru.num_def; i++) {
        e = &local_info->iter_instru.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quintet) {
            res |= (*e->cb.instru2instru_ex_cb)(drcontext, tag, bb, for_trace,
                                                translating, quintet_data[quintet_idx]);
            quintet_idx++;
        } else
            res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    /* Pass 5: meta-instrumentation (final) */
    pt->cur_phase = DRMGR_PHASE_META_INSTRU;
    for (quintet_idx = 0, i = 0; i < local_info->iter_meta_instru.num_def; i++) {
        e = &local_info->iter_meta_instru.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quintet) {
            res |= (*e->cb.meta_instru_ex_cb)(drcontext, tag, bb, for_trace, translating,
                                              quintet_data[quintet_idx]);
            quintet_idx++;
        } else
            res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    pt->cur_phase = DRMGR_PHASE_NONE;

    /* Delete table only if opcode events were ever registered by the user. */
    if (local_info->was_opcode_instrum_registered)
        hashtable_delete(&local_opcode_instrum_table);

    return res;
}

static bool
drmgr_bb_event_instrument_dups(void *drcontext, void *tag, instrlist_t *bb,
                               bool for_trace, bool translating, dr_emit_flags_t *res,
                               per_thread_t *pt, local_cb_info_t *local_info,
                               void **pair_data, void **quintet_data)
{
    uint i;
    cb_entry_t *e;

    /* Pass pre bbdup: */
    for (i = 0; i < local_info->iter_pre_bbdup.num_def; i++) {
        e = &local_info->iter_pre_bbdup.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        *res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    void *local_dup_info;
    /* Do the dups. */
    bool is_dups = local_info->bbdup_duplicate_cb(drcontext, tag, bb, for_trace,
                                                  translating, &local_dup_info);
    if (is_dups) {
        instrlist_t *case_bb = local_info->bbdup_extract_cb(drcontext, tag, bb, for_trace,
                                                            translating, local_dup_info);
        while (case_bb != NULL) {
            /* Do an instrumentation pass for the case bb. */
            *res |= drmgr_bb_event_do_instrum_phases(drcontext, tag, case_bb, for_trace,
                                                     translating, pt, local_info,
                                                     pair_data, quintet_data);
            /* Stitch case bb back to the main bb. */
            local_info->bbdup_stitch_cb(drcontext, tag, bb, case_bb, for_trace,
                                        translating, local_dup_info);
            /* Extract next duplicated bb. Returns NULL if
             * no more dups are pending.
             */
            case_bb = local_info->bbdup_extract_cb(drcontext, tag, bb, for_trace,
                                                   translating, local_dup_info);
        }
        /* Insert encoding at start and finalise. */
        local_info->bbdup_insert_encoding_cb(drcontext, tag, bb, for_trace, translating,
                                             local_dup_info);
    }
    return is_dups;
}

static void
drmgr_bb_event_set_local_cb_info(void *drcontext, OUT local_cb_info_t *local_info)
{
    dr_rwlock_read_lock(bb_cb_lock);
    /* We use arrays to more easily support unregistering while in an event (i#1356).
     * With arrays we can make a temporary copy and avoid holding a lock while
     * delivering events.
     */
    cblist_create_local(drcontext, &cblist_app2app, &local_info->iter_app2app,
                        (byte *)local_info->app2app,
                        BUFFER_SIZE_ELEMENTS(local_info->app2app));
    cblist_create_local(drcontext, &cblist_instrumentation, &local_info->iter_insert,
                        (byte *)local_info->insert,
                        BUFFER_SIZE_ELEMENTS(local_info->insert));
    cblist_create_local(drcontext, &cblist_instru2instru, &local_info->iter_instru,
                        (byte *)local_info->instru,
                        BUFFER_SIZE_ELEMENTS(local_info->instru));
    cblist_create_local(drcontext, &cblist_meta_instru, &local_info->iter_meta_instru,
                        (byte *)local_info->meta_instru,
                        BUFFER_SIZE_ELEMENTS(local_info->meta_instru));
    local_info->pair_count = pair_count;
    local_info->quintet_count = quintet_count;
    local_info->was_opcode_instrum_registered = was_opcode_instrum_registered;
    /* We do not make a complete local copy of the opcode hashtable as this can be
     * expensive. Instead, we create a scoped table later on that only maps the cb lists
     * of those opcodes required by this specific bb.
     */

    /* Copy bbdup callbacks. */
    local_info->is_bbdup_enabled = is_bbdup_enabled();
    if (local_info->is_bbdup_enabled) {
        ASSERT(bbdup_duplicate_cb != NULL, "should not be NULL");
        ASSERT(bbdup_insert_encoding_cb != NULL, "should not be NULL");
        ASSERT(bbdup_extract_cb != NULL, "should not be NULL");
        ASSERT(bbdup_stitch_cb != NULL, "should not be NULL");

        local_info->bbdup_duplicate_cb = bbdup_duplicate_cb;
        local_info->bbdup_insert_encoding_cb = bbdup_insert_encoding_cb;
        local_info->bbdup_extract_cb = bbdup_extract_cb;
        local_info->bbdup_stitch_cb = bbdup_stitch_cb;

        cblist_create_local(drcontext, &cblist_pre_bbdup, &local_info->iter_pre_bbdup,
                            (byte *)local_info->pre_bbdup,
                            BUFFER_SIZE_ELEMENTS(local_info->pre_bbdup));
    }
    dr_rwlock_read_unlock(bb_cb_lock);
}

static void
drmgr_bb_event_delete_local_cb_info(void *drcontext, IN local_cb_info_t *local_info)
{
    cblist_delete_local(drcontext, &local_info->iter_app2app,
                        BUFFER_SIZE_ELEMENTS(local_info->app2app));
    cblist_delete_local(drcontext, &local_info->iter_insert,
                        BUFFER_SIZE_ELEMENTS(local_info->insert));
    cblist_delete_local(drcontext, &local_info->iter_instru,
                        BUFFER_SIZE_ELEMENTS(local_info->instru));

    if (local_info->is_bbdup_enabled) {
        cblist_delete_local(drcontext, &local_info->iter_pre_bbdup,
                            BUFFER_SIZE_ELEMENTS(local_info->pre_bbdup));
    }
}

static dr_emit_flags_t
drmgr_bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
               bool translating)
{
    dr_emit_flags_t res = DR_EMIT_DEFAULT;
    local_cb_info_t local_info;
    void **pair_data = NULL, **quintet_data = NULL;
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);

    drmgr_bb_event_set_local_cb_info(drcontext, &local_info);

    /* We need per-thread user_data */
    if (local_info.pair_count > 0) {
        pair_data =
            (void **)dr_thread_alloc(drcontext, sizeof(void *) * local_info.pair_count);
    }
    if (local_info.quintet_count > 0) {
        quintet_data = (void **)dr_thread_alloc(
            drcontext, sizeof(void *) * local_info.quintet_count);
    }

    bool is_dups = false;
    /* Only true if drbbdup is in use. */
    if (local_info.is_bbdup_enabled) {
        is_dups = drmgr_bb_event_instrument_dups(drcontext, tag, bb, for_trace,
                                                 translating, &res, pt, &local_info,
                                                 pair_data, quintet_data);
    }

    if (!is_dups) {
        res = drmgr_bb_event_do_instrum_phases(drcontext, tag, bb, for_trace, translating,
                                               pt, &local_info, pair_data, quintet_data);
    }

    /* Do final fix passes: */
    /* Pass 5: our private pass to support multiple non-meta ctis in app2app phase */
    drmgr_fix_app_ctis(drcontext, bb);

#ifdef ARM
    /* Pass 6: private pass to legalize conditional Thumb instrs.
     * Xref various discussions about removing IT instrs earlier, but there's a
     * conflict w/ tools who want to see the original instr stream and it's not
     * clear *when* to remove them.  Thus, we live w/ an inconsistent state
     * until this point.
     */
    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_THUMB) {
        dr_remove_it_instrs(drcontext, bb);
        dr_insert_it_instrs(drcontext, bb);
    }
#endif

    if (local_info.pair_count > 0)
        dr_thread_free(drcontext, pair_data, sizeof(void *) * local_info.pair_count);
    if (local_info.quintet_count > 0) {
        dr_thread_free(drcontext, quintet_data,
                       sizeof(void *) * local_info.quintet_count);
    }

    drmgr_bb_event_delete_local_cb_info(drcontext, &local_info);

    return res;
}

/* Caller must hold write lock.
 * priority can be NULL in which case default_priority is used.
 * Returns -1 on error, else index of new entry.
 */
static int
priority_event_add(cb_list_t *list, drmgr_priority_t *new_pri)
{
    int i;
    bool past_after;   /* are we past the "after" constraint */
    bool found_before; /* did we find the "before" constraint */
    priority_event_entry_t *pri;

    if (new_pri == NULL)
        new_pri = (drmgr_priority_t *)&default_priority;
    if (new_pri->name == NULL)
        return -1; /* must have a name */

    /* if we add fields in the future this is where we decide which to use */
    if (new_pri->struct_size < sizeof(drmgr_priority_t))
        return -1; /* incorrect struct */

    /* check for duplicate names.
     * not expecting a very long list, so simpler to do full walk
     * here rather than more complex logic to fold into walk below.
     */
    if (new_pri != &default_priority) {
        for (i = 0; i < (int)list->num_def; i++) {
            pri = cblist_get_pri(list, i);
            if (pri->valid && strcmp(new_pri->name, pri->in_priority.name) == 0)
                return -1; /* duplicate name */
        }
    }

    /* Keep the list sorted by numeric priority.
     * Callback priorities are not re-sorted dynamically as callbacks are registered,
     * so callbacks intending to be named in before or after requests should
     * use non-zero numeric priorities to ensure proper ordering.
     * Xref the dynamic proposal in i#1762.
     */
    past_after = (new_pri->after == NULL);
    found_before = (new_pri->before == NULL);
    for (i = 0; i < (int)list->num_def; i++) {
        pri = cblist_get_pri(list, i);
        if (!pri->valid)
            continue;
        /* Primary sort: numeric priority.  Tie goes to 1st to register. */
        if (pri->in_priority.priority > new_pri->priority)
            break;
        /* Secondary constraint #1: must be before "before" */
        if (new_pri->before != NULL &&
            strcmp(new_pri->before, pri->in_priority.name) == 0) {
            found_before = true;
            if (pri->in_priority.priority < new_pri->priority) {
                /* cannot satisfy both before and numeric */
                return -1;
            }
            break;
        }
        /* Secondary constraint #2: must be after "after" */
        else if (!past_after) {
            ASSERT(new_pri->after != NULL, "past_after should be true");
            if (strcmp(new_pri->after, pri->in_priority.name) == 0) {
                past_after = true;
            }
        }
    }
    if (!past_after) {
        /* Cannot satisfy both the before and after requests, or both
         * the after and numeric requests.
         */
        return -1;
    }
    if (!found_before) {
        /* We require the before to already be registered (i#1762 covers
         * switching to a dynamic model).
         */
        int j;
        ASSERT(new_pri->before != NULL, "found_before should be true");
        for (j = i; j < (int)list->num_def; j++) {
            pri = cblist_get_pri(list, j);
            if (pri->valid && strcmp(new_pri->before, pri->in_priority.name) == 0) {
                found_before = true;
                break;
            }
        }
        if (!found_before)
            return -1;
    }
    /* insert at index i */
    i = cblist_shift_and_resize(list, i);
    if (i < 0)
        return -1;
    pri = cblist_get_pri(list, i);
    pri->valid = true;
    pri->in_priority = *new_pri;
    list->num_valid++;
    if (list->num_valid == 1 && list->lazy_register != NULL)
        (*list->lazy_register)();
    return (int)i;
}

static bool
drmgr_bb_cb_add(cb_list_t *list, void *func1, void *func2, drmgr_priority_t *priority,
                void *user_data, /*passed at registration */
                void (*set_cb_fields)(cb_entry_t *e, void *f1, void *f2))
{
    int idx;
    bool res = false;
    ASSERT(list != NULL && (func1 != NULL || func2 != NULL) && set_cb_fields != NULL,
           "invalid internal params");

    dr_rwlock_write_lock(bb_cb_lock);
    idx = priority_event_add(list, priority);
    if (idx >= 0) {
        cb_entry_t *new_e = &list->cbs.bb[idx];
        new_e->registration_user_data = user_data;
        new_e->has_quintet = false;
        new_e->has_pair = false;
        new_e->is_opcode_insertion = false;
        set_cb_fields(new_e, func1, func2);
        if (bb_event_count == 0)
            dr_register_bb_event(drmgr_bb_event);
        bb_event_count++;
        if (new_e->has_quintet)
            quintet_count++;
        else if (new_e->has_pair)
            pair_count++;
        res = true;
    }
    dr_rwlock_write_unlock(bb_cb_lock);
    return res;
}

static void
cb_entry_set_fields_xform(cb_entry_t *new_e, void *func1, void *func2)
{
    ASSERT(func2 == NULL, "invalid internal params");
    new_e->cb.xform_cb = (drmgr_xform_cb_t)func1;
}

DR_EXPORT
bool
drmgr_register_bb_app2app_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_app2app, (void *)func, NULL, priority,
                           NULL /* no user data */, cb_entry_set_fields_xform);
}

static void
cb_entry_set_fields_instrumentation(cb_entry_t *new_e, void *func1, void *func2)
{
    new_e->has_pair = true;
    new_e->cb.pair.analysis_cb = (drmgr_analysis_cb_t)func1;
    new_e->cb.pair.insertion_cb = (drmgr_insertion_cb_t)func2;
}

DR_EXPORT
bool
drmgr_register_bb_instrumentation_event(drmgr_analysis_cb_t analysis_func,
                                        drmgr_insertion_cb_t insertion_func,
                                        drmgr_priority_t *priority)
{
    if (analysis_func == NULL && insertion_func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_instrumentation, (void *)analysis_func,
                           (void *)insertion_func, priority, NULL /* no user data */,
                           cb_entry_set_fields_instrumentation);
}

DR_EXPORT
bool
drmgr_register_bb_instru2instru_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_instru2instru, (void *)func, NULL, priority,
                           NULL /* no user data */, cb_entry_set_fields_xform);
}

static void
cb_entry_set_fields_app2app_ex(cb_entry_t *new_e, void *func1, void *func2)
{
    ASSERT(func2 == NULL, "invalid internal params");
    new_e->has_quintet = true;
    new_e->cb.app2app_ex_cb = (drmgr_app2app_ex_cb_t)func1;
}

static void
cb_entry_set_fields_insertion_ex(cb_entry_t *new_e, void *func1, void *func2)
{
    new_e->has_quintet = true;
    new_e->cb.pair_ex.analysis_ex_cb = (drmgr_ilist_ex_cb_t)func1;
    new_e->cb.pair_ex.insertion_ex_cb = (drmgr_insertion_cb_t)func2;
}

static void
cb_entry_set_fields_instru2instru_ex(cb_entry_t *new_e, void *func1, void *func2)
{
    ASSERT(func2 == NULL, "invalid internal params");
    new_e->has_quintet = true;
    new_e->cb.instru2instru_ex_cb = (drmgr_ilist_ex_cb_t)func1;
}

static void
cb_entry_set_fields_meta_instru_ex(cb_entry_t *new_e, void *func1, void *func2)
{
    ASSERT(func2 == NULL, "invalid internal params");
    new_e->has_quintet = true;
    new_e->cb.meta_instru_ex_cb = (drmgr_ilist_ex_cb_t)func1;
}

DR_EXPORT
bool
drmgr_register_bb_instrumentation_ex_event(drmgr_app2app_ex_cb_t app2app_func,
                                           drmgr_ilist_ex_cb_t analysis_func,
                                           drmgr_insertion_cb_t insertion_func,
                                           drmgr_ilist_ex_cb_t instru2instru_func,
                                           drmgr_priority_t *priority)
{
    bool ok = true;
    if ((app2app_func == NULL && analysis_func == NULL && insertion_func == NULL &&
         instru2instru_func == NULL) ||
        /* can't have insertion but not analysis here b/c of unreg constraints */
        (analysis_func == NULL && insertion_func != NULL))
        return false; /* invalid params */
    if (app2app_func != NULL) {
        ok = drmgr_bb_cb_add(&cblist_app2app, (void *)app2app_func, NULL, priority,
                             NULL /* no user data */, cb_entry_set_fields_app2app_ex) &&
            ok;
    }
    if (analysis_func != NULL) {
        ok = drmgr_bb_cb_add(&cblist_instrumentation, (void *)analysis_func,
                             (void *)insertion_func, priority, NULL /* no user data */,
                             cb_entry_set_fields_insertion_ex) &&
            ok;
    }
    if (instru2instru_func != NULL) {
        ok = drmgr_bb_cb_add(&cblist_instru2instru, (void *)instru2instru_func, NULL,
                             priority, NULL /* no user data */,
                             cb_entry_set_fields_instru2instru_ex) &&
            ok;
    }
    return ok;
}

static bool
drmgr_bb_cb_remove(cb_list_t *list, void *func,
                   bool (*matches_func)(cb_entry_t *e, void *f))
{
    bool res = false;
    uint i;
    ASSERT(list != NULL && func != NULL && matches_func != NULL,
           "invalid internal params");

    dr_rwlock_write_lock(bb_cb_lock);
    for (i = 0; i < list->num_def; i++) {
        cb_entry_t *e = &list->cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (matches_func(e, func)) {
            res = true;
            e->pri.valid = false;
            ASSERT(list->num_valid > 0, "invalid num_valid");
            list->num_valid--;
            if (list->num_valid == 0 && list->lazy_unregister != NULL)
                (*list->lazy_unregister)();
            if (i == list->num_def - 1)
                list->num_def--;
            if (e->has_quintet)
                quintet_count--;
            else if (e->has_pair)
                pair_count--;
            bb_event_count--;
            if (bb_event_count == 0)
                dr_unregister_bb_event(drmgr_bb_event);
            break;
        }
    }
    dr_rwlock_write_unlock(bb_cb_lock);
    return res;
}

static void
drmgr_bb_init(void)
{
    cblist_init(&cblist_app2app, sizeof(cb_entry_t));
    cblist_init(&cblist_instrumentation, sizeof(cb_entry_t));
    cblist_init(&cblist_instru2instru, sizeof(cb_entry_t));
    cblist_init(&cblist_meta_instru, sizeof(cb_entry_t));
}

static void
drmgr_bb_exit(void)
{
    /* i#1317: we don't do grab the write rwlock to support exiting
     * mid-event.  drmgr_exit() is already ensuring we're only
     * called by one thread.
     */
    cblist_delete(&cblist_app2app);
    cblist_delete(&cblist_instrumentation);
    cblist_delete(&cblist_instru2instru);
    cblist_delete(&cblist_meta_instru);
}

static bool
cb_entry_matches_xform(cb_entry_t *e, void *func)
{
    return e->cb.xform_cb == func;
}

DR_EXPORT
bool
drmgr_unregister_bb_app2app_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_app2app, (void *)func, cb_entry_matches_xform);
}

static bool
cb_entry_matches_analysis(cb_entry_t *e, void *func)
{
    return e->cb.pair.analysis_cb == func;
}

DR_EXPORT
bool
drmgr_unregister_bb_instrumentation_event(drmgr_analysis_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instrumentation, (void *)func,
                              cb_entry_matches_analysis);
}

static bool
cb_entry_matches_insertion(cb_entry_t *e, void *func)
{
    return e->cb.pair.insertion_cb == func;
}

DR_EXPORT
bool
drmgr_unregister_bb_insertion_event(drmgr_insertion_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instrumentation, (void *)func,
                              cb_entry_matches_insertion);
}

DR_EXPORT
bool
drmgr_unregister_bb_instru2instru_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instru2instru, (void *)func,
                              cb_entry_matches_xform);
}

static bool
cb_entry_matches_app2app_ex(cb_entry_t *e, void *func)
{
    return e->cb.app2app_ex_cb == func;
}

static bool
cb_entry_matches_analysis_ex(cb_entry_t *e, void *func)
{
    return e->cb.pair_ex.analysis_ex_cb == func;
}

static bool
cb_entry_matches_instru2instru_ex(cb_entry_t *e, void *func)
{
    return e->cb.instru2instru_ex_cb == func;
}

static bool
cb_entry_matches_meta_instru_ex(cb_entry_t *e, void *func)
{
    return e->cb.meta_instru_ex_cb == func;
}

DR_EXPORT
bool
drmgr_unregister_bb_instrumentation_ex_event(drmgr_app2app_ex_cb_t app2app_func,
                                             drmgr_ilist_ex_cb_t analysis_func,
                                             drmgr_insertion_cb_t insertion_func,
                                             drmgr_ilist_ex_cb_t instru2instru_func)
{
    bool ok = true;
    if ((app2app_func == NULL && analysis_func == NULL && insertion_func == NULL &&
         instru2instru_func == NULL) ||
        /* can't have insertion but not analysis here b/c of unreg constraints */
        (analysis_func == NULL && insertion_func != NULL))
        return false; /* invalid params */
    if (app2app_func != NULL) {
        ok = drmgr_bb_cb_remove(&cblist_app2app, (void *)app2app_func,
                                cb_entry_matches_app2app_ex) &&
            ok;
    }
    if (analysis_func != NULL) {
        /* Although analysis_func and insertion_func are registered together in
         * drmgr_register_bb_instrumentation_ex_event, drmgr_bb_cb_remove only
         * checks one, so we only look for analysis_func.
         */
        ok = drmgr_bb_cb_remove(&cblist_instrumentation, (void *)analysis_func,
                                cb_entry_matches_analysis_ex) &&
            ok;
    }
    if (instru2instru_func != NULL) {
        ok = drmgr_bb_cb_remove(&cblist_instru2instru, (void *)instru2instru_func,
                                cb_entry_matches_instru2instru_ex) &&
            ok;
    }
    return ok;
}

static void
cb_entry_set_fields_opcode(cb_entry_t *new_e, void *func1, void *func2)
{
    ASSERT(func2 == NULL, "invalid internal params");
    new_e->is_opcode_insertion = true;
    new_e->cb.opcode_insertion_cb = (drmgr_opcode_insertion_cb_t)func1;
    /* set the flag b/c we encountered the registration of an opcode event */
    was_opcode_instrum_registered = true;
}

DR_EXPORT
bool
drmgr_register_opcode_instrumentation_event(drmgr_opcode_insertion_cb_t func, int opcode,
                                            drmgr_priority_t *priority, void *user_data)
{
    if (func == NULL)
        return false;

    dr_rwlock_write_lock(opcode_table_lock);
    cb_list_t *opcode_cb_list =
        hashtable_lookup(&global_opcode_instrum_table, (void *)(intptr_t)opcode);
    if (opcode_cb_list == NULL) {
        /* We need to add a new mapping. */
        opcode_cb_list = dr_global_alloc(sizeof(cb_list_t));
        cblist_init(opcode_cb_list, sizeof(cb_entry_t));
        hashtable_add(&global_opcode_instrum_table, (void *)(intptr_t)opcode,
                      opcode_cb_list);
    }
    dr_rwlock_write_unlock(opcode_table_lock);

    return drmgr_bb_cb_add(opcode_cb_list, (void *)func, NULL, priority, user_data,
                           cb_entry_set_fields_opcode);
}

static bool
cb_entry_matches_opcode(cb_entry_t *e, void *func)
{
    return e->cb.opcode_insertion_cb == func;
}

DR_EXPORT
bool
drmgr_unregister_opcode_instrumentation_event(drmgr_opcode_insertion_cb_t func,
                                              int opcode)
{
    if (func == NULL)
        return false;

    dr_rwlock_write_lock(opcode_table_lock);
    cb_list_t *opcode_cb_list =
        hashtable_lookup(&global_opcode_instrum_table, (void *)(intptr_t)opcode);
    if (opcode_cb_list == NULL)
        return false; /* there should be a cb list present in the table */
    dr_rwlock_write_unlock(opcode_table_lock);

    return drmgr_bb_cb_remove(opcode_cb_list, (void *)func, cb_entry_matches_opcode);
}

DR_EXPORT
bool
drmgr_register_bb_meta_instru_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_meta_instru, (void *)func, NULL, priority,
                           NULL /* no user data */, cb_entry_set_fields_xform);
}

DR_EXPORT
bool
drmgr_unregister_bb_meta_instru_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_meta_instru, (void *)func, cb_entry_matches_xform);
}

DR_EXPORT
bool
drmgr_register_bb_instrumentation_all_events(drmgr_instru_events_t *events,
                                             drmgr_priority_t *priority)
{
    if (events->struct_size < offsetof(drmgr_instru_events_t, instru2instru_func))
        return false;

    if (!drmgr_register_bb_instrumentation_ex_event(
            events->app2app_func, events->analysis_func, events->insertion_func,
            events->instru2instru_func, priority))
        return false;

    if (events->struct_size >= offsetof(drmgr_instru_events_t, meta_instru_func) &&
        events->meta_instru_func != NULL) {
        if (!drmgr_bb_cb_add(&cblist_meta_instru, (void *)events->meta_instru_func, NULL,
                             priority, NULL /* no user data */,
                             cb_entry_set_fields_meta_instru_ex))
            return false;
    }

    return true;
}

DR_EXPORT
bool
drmgr_unregister_bb_instrumentation_all_events(drmgr_instru_events_t *events)
{
    if (events->struct_size < offsetof(drmgr_instru_events_t, instru2instru_func))
        return false;

    if (!drmgr_unregister_bb_instrumentation_ex_event(
            events->app2app_func, events->analysis_func, events->insertion_func,
            events->instru2instru_func))
        return false;

    if (events->struct_size >= offsetof(drmgr_instru_events_t, meta_instru_func) &&
        events->meta_instru_func != NULL) {
        if (!drmgr_bb_cb_remove(&cblist_meta_instru, (void *)events->meta_instru_func,
                                cb_entry_matches_meta_instru_ex))
            return false;
    }
    return true;
}

DR_EXPORT
drmgr_bb_phase_t
drmgr_current_bb_phase(void *drcontext)
{
    per_thread_t *pt;
    /* Support being called w/o being set up, for detection of whether under drmgr */
    if (drmgr_init_count == 0)
        return DRMGR_PHASE_NONE;
    pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    /* Support being called during process init (i#2910). */
    if (pt == NULL)
        return DRMGR_PHASE_NONE;
    return pt->cur_phase;
}

DR_EXPORT
bool
drmgr_is_first_instr(void *drcontext, instr_t *instr)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    return instr == pt->first_instr;
}

DR_EXPORT
bool
drmgr_is_first_nonlabel_instr(void *drcontext, instr_t *instr)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    return instr == pt->first_nonlabel_instr;
}

DR_EXPORT
bool
drmgr_is_last_instr(void *drcontext, instr_t *instr)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    return instr == pt->last_instr;
}

static void
our_thread_init_event(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    pt->emulation_info.size = sizeof(pt->emulation_info);
    drmgr_set_tls_field(drcontext, our_tls_idx, (void *)pt);
}

static void
our_thread_exit_event(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

/***************************************************************************
 * WRAPPED EVENTS
 */

/* We must go first on thread init and last on thread exit, and DR
 * doesn't provide any priority scheme to guarantee that, so we must
 * wrap the thread events.
 */

static bool
drmgr_generic_event_add_ex(cb_list_t *list, void *rwlock, void (*func)(void),
                           drmgr_priority_t *priority, bool is_using_user_data,
                           void *user_data, bool is_ex)
{
    int idx;
    generic_event_entry_t *e;
    bool res = false;
    if (func == NULL)
        return false;
    dr_rwlock_write_lock(rwlock);
    idx = priority_event_add(list, priority);
    if (idx >= 0) {
        res = true;
        e = &list->cbs.generic[idx];
        e->is_ex = is_ex;
        e->cb.generic_cb = func;
        e->is_using_user_data = is_using_user_data;
        e->user_data = user_data;
    }
    dr_rwlock_write_unlock(rwlock);
    return res;
}

static bool
drmgr_generic_event_add(cb_list_t *list, void *rwlock, void (*func)(void),
                        drmgr_priority_t *priority, bool is_using_user_data,
                        void *user_data)
{
    return drmgr_generic_event_add_ex(list, rwlock, func, priority, is_using_user_data,
                                      user_data, false);
}

static bool
drmgr_generic_event_remove(cb_list_t *list, void *rwlock, void (*func)(void))
{
    bool res = false;
    uint i;
    generic_event_entry_t *e;
    if (func == NULL)
        return false;
    dr_rwlock_write_lock(rwlock);
    for (i = 0; i < list->num_def; i++) {
        e = &list->cbs.generic[i];
        if (!e->pri.valid)
            continue;
        if (e->cb.generic_cb == func) {
            res = true;
            e->pri.valid = false;
            ASSERT(list->num_valid > 0, "invalid num_valid");
            list->num_valid--;
            if (list->num_valid == 0 && list->lazy_unregister != NULL)
                (*list->lazy_unregister)();
            break;
        }
    }
    dr_rwlock_write_unlock(rwlock);
    return res;
}

/* We delay registering syscall events until a client does, to avoid triggering DR's
 * assert about having a syscall event and no filter event (we can't register our own
 * filter and provide the warning ourselves unless we replace DR's filter).  Today we
 * have no action of our own on pre or post syscall so this works out.
 */
static void
drmgr_lazy_register_presys(void)
{
    dr_register_pre_syscall_event(drmgr_presyscall_event);
}
static void
drmgr_lazy_register_postsys(void)
{
    dr_register_post_syscall_event(drmgr_postsyscall_event);
}
static void
drmgr_lazy_unregister_presys(void)
{
    dr_unregister_pre_syscall_event(drmgr_presyscall_event);
}
static void
drmgr_lazy_unregister_postsys(void)
{
    dr_unregister_post_syscall_event(drmgr_postsyscall_event);
}

static void
drmgr_event_init(void)
{
    cblist_init(&cb_list_thread_init, sizeof(generic_event_entry_t));
    cblist_init(&cb_list_thread_exit, sizeof(generic_event_entry_t));
    cblist_init(&cblist_cls_init, sizeof(generic_event_entry_t));
    cblist_init(&cblist_cls_exit, sizeof(generic_event_entry_t));

    cblist_init(&cblist_presys, sizeof(generic_event_entry_t));
    cblist_presys.lazy_register = drmgr_lazy_register_presys;
    cblist_presys.lazy_unregister = drmgr_lazy_unregister_presys;
    cblist_init(&cblist_postsys, sizeof(generic_event_entry_t));
    cblist_postsys.lazy_register = drmgr_lazy_register_postsys;
    cblist_postsys.lazy_unregister = drmgr_lazy_unregister_postsys;

    cblist_init(&cblist_modload, sizeof(generic_event_entry_t));
    cblist_init(&cblist_modunload, sizeof(generic_event_entry_t));
    cblist_init(&cblist_low_on_memory, sizeof(generic_event_entry_t));
    cblist_init(&cblist_kernel_xfer, sizeof(generic_event_entry_t));
#ifdef UNIX
    cblist_init(&cblist_signal, sizeof(generic_event_entry_t));
#endif
#ifdef WINDOWS
    cblist_init(&cblist_exception, sizeof(generic_event_entry_t));
#endif
    cblist_init(&cblist_fault, sizeof(generic_event_entry_t));
}

static void
drmgr_event_exit(void)
{
    /* i#1317: we don't grab the write rwlock to support exiting
     * mid-event.  drmgr_exit() is already ensuring we're only
     * called by one thread.
     */
    cblist_delete(&cb_list_thread_init);
    cblist_delete(&cb_list_thread_exit);
    cblist_delete(&cblist_cls_init);
    cblist_delete(&cblist_cls_exit);
    cblist_delete(&cblist_presys);
    cblist_delete(&cblist_postsys);
    cblist_delete(&cblist_modload);
    cblist_delete(&cblist_modunload);
    cblist_delete(&cblist_low_on_memory);
    cblist_delete(&cblist_kernel_xfer);
#ifdef UNIX
    cblist_delete(&cblist_signal);
#endif
#ifdef WINDOWS
    cblist_delete(&cblist_exception);
#endif
    cblist_delete(&cblist_fault);
}

DR_EXPORT
bool
drmgr_register_thread_init_event(void (*func)(void *drcontext))
{
    return drmgr_register_thread_init_event_ex(func, NULL);
}

DR_EXPORT
bool
drmgr_register_thread_init_event_ex(void (*func)(void *drcontext),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cb_list_thread_init, thread_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_thread_init_event_user_data(void (*func)(void *drcontext, void *user_data),
                                           drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cb_list_thread_init, thread_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_thread_init_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_remove(&cb_list_thread_init, thread_event_lock,
                                      (void (*)(void))func);
}

bool
drmgr_unregister_thread_init_event_user_data(void (*func)(void *drcontext,
                                                          void *user_data))
{
    return drmgr_generic_event_remove(&cb_list_thread_init, thread_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_register_thread_exit_event(void (*func)(void *drcontext))
{
    return drmgr_register_thread_exit_event_ex(func, NULL);
}

DR_EXPORT
bool
drmgr_register_thread_exit_event_ex(void (*func)(void *drcontext),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cb_list_thread_exit, thread_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_thread_exit_event_user_data(void (*func)(void *drcontext, void *user_data),
                                           drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cb_list_thread_exit, thread_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_thread_exit_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_remove(&cb_list_thread_exit, thread_event_lock,
                                      (void (*)(void))func);
}

bool
drmgr_unregister_thread_exit_event_user_data(void (*func)(void *drcontext,
                                                          void *user_data))
{
    return drmgr_generic_event_remove(&cb_list_thread_exit, thread_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_register_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_add(&cblist_presys, presys_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_pre_syscall_event_ex(bool (*func)(void *drcontext, int sysnum),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_presys, presys_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_pre_syscall_event_user_data(bool (*func)(void *drcontext, int sysnum,
                                                        void *user_data),
                                           drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cblist_presys, presys_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_remove(&cblist_presys, presys_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_unregister_pre_syscall_event_user_data(bool (*func)(void *drcontext, int sysnum,
                                                          void *user_data))
{
    return drmgr_generic_event_remove(&cblist_presys, presys_event_lock,
                                      (void (*)(void))func);
}

static bool
drmgr_presyscall_event(void *drcontext, int sysnum)
{
    bool execute = true;
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(presys_event_lock);
    cblist_create_local(drcontext, &cblist_presys, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(presys_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false) {
            execute =
                (*iter.cbs.generic[i].cb.presys_cb.cb_no_user_data)(drcontext, sysnum) &&
                execute;
        } else {
            execute = (*iter.cbs.generic[i].cb.presys_cb.cb_user_data)(drcontext, sysnum,
                                                                       user_data) &&
                execute;
        }
    }

    /* We used to track NtCallbackReturn for CLS (before DR provided the kernel xfer
     * event) and had to handle it last here.  Now we have nothing ourselves to
     * do here.  If we do add something we'll need to redo the lazy
     * drmgr_lazy_register_presys(), etc.
     */

    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));

    return execute;
}

DR_EXPORT
bool
drmgr_register_post_syscall_event(void (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_add(&cblist_postsys, postsys_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_post_syscall_event_ex(void (*func)(void *drcontext, int sysnum),
                                     drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_postsys, postsys_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_post_syscall_event_user_data(void (*func)(void *drcontext, int sysnum,
                                                         void *user_data),
                                            drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cblist_postsys, postsys_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_post_syscall_event(void (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_remove(&cblist_postsys, postsys_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_unregister_post_syscall_event_user_data(void (*func)(void *drcontext, int sysnum,
                                                           void *user_data))
{
    return drmgr_generic_event_remove(&cblist_postsys, postsys_event_lock,
                                      (void (*)(void))func);
}

static void
drmgr_postsyscall_event(void *drcontext, int sysnum)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(postsys_event_lock);
    cblist_create_local(drcontext, &cblist_postsys, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(postsys_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false)
            (*iter.cbs.generic[i].cb.postsys_cb.cb_no_user_data)(drcontext, sysnum);
        else {
            (*iter.cbs.generic[i].cb.postsys_cb.cb_user_data)(drcontext, sysnum,
                                                              user_data);
        }
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

/***************************************************************************
 * WRAPPED MODULE EVENTS
 */

DR_EXPORT
bool
drmgr_register_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                              bool loaded))
{
    return drmgr_generic_event_add(&cblist_modload, modload_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_module_load_event_ex(void (*func)(void *drcontext,
                                                 const module_data_t *info, bool loaded),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_modload, modload_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_module_load_event_user_data(void (*func)(void *drcontext,
                                                        const module_data_t *info,
                                                        bool loaded, void *user_data),
                                           drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cblist_modload, modload_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_module_load_event(void (*func)(void *drcontext,
                                                const module_data_t *info, bool loaded))
{
    return drmgr_generic_event_remove(&cblist_modload, modload_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_unregister_module_load_event_user_data(void (*func)(void *drcontext,
                                                          const module_data_t *info,
                                                          bool loaded, void *user_data))
{
    return drmgr_generic_event_remove(&cblist_modload, modload_event_lock,
                                      (void (*)(void))func);
}

static void
drmgr_modload_event(void *drcontext, const module_data_t *info, bool loaded)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(modload_event_lock);
    cblist_create_local(drcontext, &cblist_modload, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(modload_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false) {
            (*iter.cbs.generic[i].cb.modload_cb.cb_no_user_data)(drcontext, info, loaded);
        } else {
            (*iter.cbs.generic[i].cb.modload_cb.cb_user_data)(drcontext, info, loaded,
                                                              user_data);
        }
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

DR_EXPORT
bool
drmgr_register_module_unload_event(void (*func)(void *drcontext,
                                                const module_data_t *info))
{
    return drmgr_generic_event_add(&cblist_modunload, modunload_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_module_unload_event_ex(void (*func)(void *drcontext,
                                                   const module_data_t *info),
                                      drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_modunload, modunload_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_module_unload_event_user_data(void (*func)(void *drcontext,
                                                          const module_data_t *info,
                                                          void *user_data),
                                             drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cblist_modunload, modunload_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_module_unload_event(void (*func)(void *drcontext,
                                                  const module_data_t *info))
{
    return drmgr_generic_event_remove(&cblist_modunload, modunload_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_unregister_module_unload_event_user_data(void (*func)(void *drcontext,
                                                            const module_data_t *info,
                                                            void *user_data))
{
    return drmgr_generic_event_remove(&cblist_modunload, modunload_event_lock,
                                      (void (*)(void))func);
}

static void
drmgr_modunload_event(void *drcontext, const module_data_t *info)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(modunload_event_lock);
    cblist_create_local(drcontext, &cblist_modunload, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(modunload_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false)
            (*iter.cbs.generic[i].cb.modunload_cb.cb_no_user_data)(drcontext, info);
        else {
            (*iter.cbs.generic[i].cb.modunload_cb.cb_user_data)(drcontext, info,
                                                                user_data);
        }
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

/***************************************************************************
 * WRAPPED FAULT EVENTS
 */

#ifdef UNIX
DR_EXPORT
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_register_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                       dr_siginfo_t *siginfo))
/* clang-format on */
{
    return drmgr_generic_event_add(&cblist_signal, signal_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_signal_event_ex(dr_signal_action_t (*func)(void *drcontext,
                                                          dr_siginfo_t *siginfo),
                               drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_signal, signal_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_signal_event_user_data(dr_signal_action_t (*func)(void *drcontext,
                                                                 dr_siginfo_t *siginfo,
                                                                 void *user_data),
                                      drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cblist_signal, signal_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}
DR_EXPORT
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_unregister_signal_event(dr_signal_action_t (*func)
                              (void *drcontext, dr_siginfo_t *siginfo))
/* clang-format on */
{
    return drmgr_generic_event_remove(&cblist_signal, signal_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
bool
drmgr_unregister_signal_event_user_data(dr_signal_action_t (*func)(void *drcontext,
                                                                   dr_siginfo_t *siginfo,
                                                                   void *user_data))
/* clang-format on */
{
    return drmgr_generic_event_remove(&cblist_signal, signal_event_lock,
                                      (void (*)(void))func);
}

static dr_signal_action_t
drmgr_signal_event(void *drcontext, dr_siginfo_t *siginfo)
{
    dr_signal_action_t res = DR_SIGNAL_DELIVER;
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(signal_event_lock);
    cblist_create_local(drcontext, &cblist_signal, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(signal_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;

        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        /* follow DR semantics: short-circuit on first handler to "own" the signal */
        if (is_using_user_data == false) {
            res = (*iter.cbs.generic[i].cb.signal_cb.cb_no_user_data)(drcontext, siginfo);
        } else {
            res = (*iter.cbs.generic[i].cb.signal_cb.cb_user_data)(drcontext, siginfo,
                                                                   user_data);
        }
        if (res != DR_SIGNAL_DELIVER)
            break;
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
    return res;
}
#endif /* UNIX */

#ifdef WINDOWS
DR_EXPORT
bool
drmgr_register_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt))
{
    return drmgr_generic_event_add(&cblist_exception, exception_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_exception_event_ex(bool (*func)(void *drcontext, dr_exception_t *excpt),
                                  drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_exception, exception_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_unregister_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt))
{
    return drmgr_generic_event_remove(&cblist_exception, exception_event_lock,
                                      (void (*)(void))func);
}

static bool
drmgr_exception_event(void *drcontext, dr_exception_t *excpt)
{
    bool res = true; /* deliver to app */
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(exception_event_lock);
    cblist_create_local(drcontext, &cblist_exception, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(exception_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        /* follow DR semantics: short-circuit on first handler to "own" the fault */
        res = (*iter.cbs.generic[i].cb.exception_cb)(drcontext, excpt);
        if (!res)
            break;
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
    return res;
}
#endif /* WINDOWS */

static void
drmgr_register_fault_event(void)
{
    if (!registered_fault) {
        dr_rwlock_write_lock(fault_event_lock);
        /* we lazily register so dr_xl8_hook_exists() is useful */
        if (!registered_fault) {
            dr_register_restore_state_ex_event(drmgr_restore_state_event);
            registered_fault = true;
        }
        dr_rwlock_write_unlock(fault_event_lock);
    }
}

DR_EXPORT
bool
drmgr_register_restore_state_event(void (*func)(void *drcontext, void *tag,
                                                dr_mcontext_t *mcontext,
                                                bool restore_memory,
                                                bool app_code_consistent))
{
    drmgr_register_fault_event();
    return drmgr_generic_event_add_ex(&cblist_fault, fault_event_lock,
                                      (void (*)(void))func, NULL, false, NULL,
                                      false /*!ex*/);
}

DR_EXPORT
bool
drmgr_register_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                   dr_restore_state_info_t *info))
{
    drmgr_register_fault_event();
    return drmgr_generic_event_add_ex(&cblist_fault, fault_event_lock,
                                      (void (*)(void))func, NULL, false, NULL,
                                      true /*ex*/);
}

DR_EXPORT
bool
drmgr_register_restore_state_ex_event_ex(bool (*func)(void *drcontext,
                                                      bool restore_memory,
                                                      dr_restore_state_info_t *info),
                                         drmgr_priority_t *priority)
{
    drmgr_register_fault_event();
    return drmgr_generic_event_add_ex(&cblist_fault, fault_event_lock,
                                      (void (*)(void))func, priority, false, NULL,
                                      true /*ex*/);
}

DR_EXPORT
bool
drmgr_unregister_restore_state_event(void (*func)(void *drcontext, void *tag,
                                                  dr_mcontext_t *mcontext,
                                                  bool restore_memory,
                                                  bool app_code_consistent))
{
    /* we never unregister our event once registered */
    return drmgr_generic_event_remove(&cblist_fault, fault_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_unregister_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                     dr_restore_state_info_t *info))
{
    return drmgr_generic_event_remove(&cblist_fault, fault_event_lock,
                                      (void (*)(void))func);
}

static bool
drmgr_restore_state_event(void *drcontext, bool restore_memory,
                          dr_restore_state_info_t *info)
{
    bool res = true; /* deliver to app */
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(fault_event_lock);
    cblist_create_local(drcontext, &cblist_fault, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(fault_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        /* follow DR semantics: short-circuit on first handler to "own" the fault */
        if (iter.cbs.generic[i].is_ex) {
            res = (*iter.cbs.generic[i].cb.fault_ex_cb)(drcontext, restore_memory, info);
        } else {
            (*iter.cbs.generic[i].cb.fault_cb)(drcontext, info->fragment_info.tag,
                                               info->mcontext, restore_memory,
                                               info->fragment_info.app_code_consistent);
        }
        if (!res)
            break;
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
    return res;
}

/***************************************************************************
 * TLS
 */

static void
drmgr_thread_init_event(void *drcontext)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    tls_array_t *tls = dr_thread_alloc(drcontext, sizeof(*tls));
    memset(tls, 0, sizeof(*tls));
    dr_set_tls_field(drcontext, (void *)tls);

    dr_rwlock_read_lock(thread_event_lock);
    cblist_create_local(drcontext, &cb_list_thread_init, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(thread_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false)
            (*iter.cbs.generic[i].cb.thread_cb.cb_no_user_data)(drcontext);
        else
            (*iter.cbs.generic[i].cb.thread_cb.cb_user_data)(drcontext, user_data);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));

    drmgr_cls_stack_init(drcontext);
}

static void
drmgr_thread_exit_event(void *drcontext)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(thread_event_lock);
    cblist_create_local(drcontext, &cb_list_thread_exit, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(thread_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false)
            (*iter.cbs.generic[i].cb.thread_cb.cb_no_user_data)(drcontext);
        else
            (*iter.cbs.generic[i].cb.thread_cb.cb_user_data)(drcontext, user_data);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));

    drmgr_cls_stack_exit(drcontext);
}

/* shared by tls and cls */
static int
drmgr_reserve_tls_cls_field(bool *taken)
{
    int i;
    dr_mutex_lock(tls_lock);
    for (i = 0; i < MAX_NUM_TLS; i++) {
        if (!taken[i]) {
            taken[i] = true;
            break;
        }
    }
    dr_mutex_unlock(tls_lock);
    if (i < MAX_NUM_TLS)
        return i;
    else
        return -1;
}

static bool
drmgr_unreserve_tls_cls_field(bool *taken, int idx)
{
    bool res = false;
    if (idx < 0 || idx > MAX_NUM_TLS)
        return false;
    dr_mutex_lock(tls_lock);
    if (taken[idx]) {
        res = true;
        taken[idx] = false;
    } else
        res = false;
    dr_mutex_unlock(tls_lock);
    return res;
}

DR_EXPORT
int
drmgr_register_tls_field(void)
{
    return drmgr_reserve_tls_cls_field(tls_taken);
}

DR_EXPORT
bool
drmgr_unregister_tls_field(int idx)
{
    return drmgr_unreserve_tls_cls_field(tls_taken, idx);
}

DR_EXPORT
void *
drmgr_get_tls_field(void *drcontext, int idx)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    /* no need to check for tls_taken since would return NULL anyway (i#484) */
    if (idx < 0 || idx > MAX_NUM_TLS || tls == NULL)
        return NULL;
    return tls->tls[idx];
}

DR_EXPORT
bool
drmgr_set_tls_field(void *drcontext, int idx, void *value)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || tls == NULL)
        return false;
    /* going DR's traditional route of efficiency over safety: making this
     * a debug-only check to avoid cost in release build
     */
    ASSERT(tls_taken[idx], "usage error: setting tls index that is not reserved");
    tls->tls[idx] = value;
    return true;
}

DR_EXPORT
bool
drmgr_insert_read_tls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                            reg_id_t reg)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !tls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, reg);
    instrlist_meta_preinsert(
        ilist, where,
        XINST_CREATE_load(
            drcontext, opnd_create_reg(reg),
            OPND_CREATE_MEMPTR(reg, offsetof(tls_array_t, tls) + idx * sizeof(void *))));
    return true;
}

DR_EXPORT
bool
drmgr_insert_write_tls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                             reg_id_t reg, reg_id_t scratch)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !tls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg) || !reg_is_gpr(scratch) ||
        !reg_is_pointer_sized(scratch))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, scratch);
    instrlist_meta_preinsert(
        ilist, where,
        XINST_CREATE_store(
            drcontext,
            OPND_CREATE_MEMPTR(scratch,
                               offsetof(tls_array_t, tls) + idx * sizeof(void *)),
            opnd_create_reg(reg)));
    return true;
}

/***************************************************************************
 * CLS
 */

static bool
drmgr_cls_stack_push_event(void *drcontext, bool new_depth)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    /* let client initialize cls slots (and allocate new ones if new_depth) */
    dr_rwlock_read_lock(cls_event_lock);
    cblist_create_local(drcontext, &cblist_cls_init, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(cls_event_lock);
    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.cls_cb)(drcontext, new_depth);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
    return true;
}

static bool
drmgr_cls_stack_init(void *drcontext)
{
    return drmgr_cls_stack_push_event(drcontext, true /*new_depth*/);
}

static bool
drmgr_cls_stack_push(void)
{
    void *drcontext = dr_get_current_drcontext();
    tls_array_t *tls_parent = (tls_array_t *)dr_get_tls_field(drcontext);
    tls_array_t *tls_child;
    bool new_depth = false;
    if (tls_parent == NULL) {
        ASSERT(false, "internal error");
        return false;
    }

    tls_child = tls_parent->next;
    /* we re-use to avoid churn */
    if (tls_child == NULL) {
        tls_child = dr_thread_alloc(drcontext, sizeof(*tls_child));
        memset(tls_child, 0, sizeof(*tls_child));
        tls_parent->next = tls_child;
        tls_child->prev = tls_parent;
        tls_child->next = NULL;
        new_depth = true;
    } else
        ASSERT(tls_child->prev == tls_parent, "cls stack corrupted");

    /* share the tls slots */
    memcpy(tls_child->tls, tls_parent->tls, sizeof(*tls_child->tls) * MAX_NUM_TLS);
    /* swap in as the current structure */
    dr_set_tls_field(drcontext, (void *)tls_child);

    return drmgr_cls_stack_push_event(drcontext, new_depth);
}

static bool
drmgr_cls_stack_pop(void)
{
    /* Our callback enter is AFTER DR's, but our callback exit is
     * BEFORE. */
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    void *drcontext = dr_get_current_drcontext();
    tls_array_t *tls_child = (tls_array_t *)dr_get_tls_field(drcontext);
    tls_array_t *tls_parent;
    if (tls_child == NULL) {
        ASSERT(false, "internal error");
        return false;
    }

    tls_parent = tls_child->prev;
    if (tls_parent == NULL) {
        /* DR took over in the middle of a callback: ignore */
        return true;
    }

    /* let client know, though normally no action is needed */
    dr_rwlock_read_lock(cls_event_lock);
    cblist_create_local(drcontext, &cblist_cls_exit, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(cls_event_lock);
    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.cls_cb)(drcontext, false /*!thread_exit*/);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));

    /* update tls w/ any changes while in child context */
    memcpy(tls_parent->tls, tls_child->tls, sizeof(*tls_child->tls) * MAX_NUM_TLS);
    /* swap in as the current structure */
    dr_set_tls_field(drcontext, (void *)tls_parent);

    return true;
}

static bool
drmgr_cls_stack_exit(void *drcontext)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    tls_array_t *nxt, *tmp;
    if (tls == NULL)
        return false;

    dr_rwlock_read_lock(cls_event_lock);
    cblist_create_local(drcontext, &cblist_cls_exit, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(cls_event_lock);

    for (nxt = tls; nxt->prev != NULL; nxt = nxt->prev)
        ; /* nothing */
    while (nxt != NULL) {
        tmp = nxt;
        nxt = nxt->next;
        /* set the field in case client queries */
        dr_set_tls_field(drcontext, (void *)tmp);
        for (i = 0; i < iter.num_def; i++) {
            if (!iter.cbs.generic[i].pri.valid)
                continue;
            (*iter.cbs.generic[i].cb.cls_cb)(drcontext, true /*thread_exit*/);
        }
        dr_thread_free(drcontext, tmp, sizeof(*tmp));
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
    dr_set_tls_field(drcontext, NULL);
    return true;
}

#ifdef WINDOWS
/* Determines the syscall from its Nt* wrapper.
 * Returns -1 on error.
 * FIXME: does not handle somebody hooking the wrapper.
 */
/* XXX: exporting this so drwrap can use it but I might prefer to
 * have this in drutil or the upcoming drsys, especially since
 * drmgr no longer uses this now that DR provides a kernel xfer
 * event.
 */
DR_EXPORT
int
drmgr_decode_sysnum_from_wrapper(app_pc entry)
{
    void *drcontext = dr_get_current_drcontext();
    int num = -1;
    byte *pc = entry;
    uint opc;
    instr_t instr;
    instr_init(drcontext, &instr);
    do {
        instr_reset(drcontext, &instr);
        pc = decode(drcontext, pc, &instr);
        if (!instr_valid(&instr))
            break; /* unknown system call sequence */
        opc = instr_get_opcode(&instr);
        /* sanity check: wrapper should be short */
        if (pc - entry > 20)
            break; /* unknown system call sequence */
        if (opc == OP_mov_imm && opnd_is_reg(instr_get_dst(&instr, 0)) &&
            opnd_get_reg(instr_get_dst(&instr, 0)) == DR_REG_EAX &&
            opnd_is_immed_int(instr_get_src(&instr, 0))) {
            num = (int)opnd_get_immed_int(instr_get_src(&instr, 0));
            break; /* success */
        }
        /* stop at call to vsyscall (wow64) or at int itself */
    } while (opc != OP_call_ind && opc != OP_int && opc != OP_sysenter &&
             opc != OP_syscall && opc != OP_ret);
    instr_free(drcontext, &instr);
    return num;
}
#endif /* WINDOWS */

static void
drmgr_kernel_xfer_event(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    /* We used to watch KiUserCallbackDispatcher, identify
     * NtCallbackReturn's number, and watch int 0x2b ourselves, but
     * now DR provides us with an event that operates at the last
     * moment before the kernel action, making our lives much
     * easier: we just have to order all other xfer events vs ours.
     */
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(kernel_xfer_event_lock);
    cblist_create_local(drcontext, &cblist_kernel_xfer, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(kernel_xfer_event_lock);

    if (info->type == DR_XFER_CALLBACK_DISPATCHER) {
        /* We want to go first for callback entry so clients have a
         * new context. */
        drmgr_cls_stack_push();
    }

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.kernel_xfer_cb)(drcontext, info);
    }

    if (info->type == DR_XFER_CALLBACK_RETURN) {
        /* We want to go last for cbret to swap contexts at the
         * last possible moment, to ensure there are no references
         * to cls data before we swap it.
         */
        drmgr_cls_stack_pop();
    }

    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

DR_EXPORT
bool
drmgr_register_kernel_xfer_event(void (*func)(void *drcontext,
                                              const dr_kernel_xfer_info_t *info))
{
    return drmgr_generic_event_add(&cblist_kernel_xfer, kernel_xfer_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_kernel_xfer_event_ex(void (*func)(void *drcontext,
                                                 const dr_kernel_xfer_info_t *info),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_kernel_xfer, kernel_xfer_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_unregister_kernel_xfer_event(void (*func)(void *drcontext,
                                                const dr_kernel_xfer_info_t *info))
{
    return drmgr_generic_event_remove(&cblist_kernel_xfer, kernel_xfer_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
int
drmgr_register_cls_field(void (*cb_init_func)(void *drcontext, bool new_depth),
                         void (*cb_exit_func)(void *drcontext, bool thread_exit))
{
    if (cb_init_func == NULL || cb_exit_func == NULL)
        return -1;
    if (!drmgr_generic_event_add(&cblist_cls_init, cls_event_lock,
                                 (void (*)(void))cb_init_func, NULL, false, NULL))
        return -1;
    if (!drmgr_generic_event_add(&cblist_cls_exit, cls_event_lock,
                                 (void (*)(void))cb_exit_func, NULL, false, NULL))
        return -1;
    return drmgr_reserve_tls_cls_field(cls_taken);
}

DR_EXPORT
bool
drmgr_unregister_cls_field(void (*cb_init_func)(void *drcontext, bool new_depth),
                           void (*cb_exit_func)(void *drcontext, bool thread_exit),
                           int idx)
{
    bool res = drmgr_generic_event_remove(&cblist_cls_init, cls_event_lock,
                                          (void (*)(void))cb_init_func);
    res = drmgr_generic_event_remove(&cblist_cls_exit, cls_event_lock,
                                     (void (*)(void))cb_exit_func) &&
        res;
    res = drmgr_unreserve_tls_cls_field(cls_taken, idx) && res;
    return res;
}

DR_EXPORT
void *
drmgr_get_cls_field(void *drcontext, int idx)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return NULL;
    return tls->cls[idx];
}

DR_EXPORT
bool
drmgr_set_cls_field(void *drcontext, int idx, void *value)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return false;
    tls->cls[idx] = value;
    return true;
}

DR_EXPORT
void *
drmgr_get_parent_cls_field(void *drcontext, int idx)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return NULL;
    if (tls->prev != NULL)
        return tls->prev->cls[idx];
    return NULL;
}

DR_EXPORT
bool
drmgr_insert_read_cls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                            reg_id_t reg)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, reg);
    instrlist_meta_preinsert(
        ilist, where,
        XINST_CREATE_load(
            drcontext, opnd_create_reg(reg),
            OPND_CREATE_MEMPTR(reg, offsetof(tls_array_t, cls) + idx * sizeof(void *))));
    return true;
}

DR_EXPORT
bool
drmgr_insert_write_cls_field(void *drcontext, int idx, instrlist_t *ilist, instr_t *where,
                             reg_id_t reg, reg_id_t scratch)
{
    tls_array_t *tls = (tls_array_t *)dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg) || !reg_is_gpr(scratch) ||
        !reg_is_pointer_sized(scratch))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, scratch);
    instrlist_meta_preinsert(
        ilist, where,
        XINST_CREATE_store(
            drcontext,
            OPND_CREATE_MEMPTR(scratch,
                               offsetof(tls_array_t, cls) + idx * sizeof(void *)),
            opnd_create_reg(reg)));
    return true;
}

DR_EXPORT
bool
drmgr_push_cls(void *drcontext)
{
    return drmgr_cls_stack_push();
}

DR_EXPORT
bool
drmgr_pop_cls(void *drcontext)
{
    return drmgr_cls_stack_pop();
}

/***************************************************************************
 * Low-on-memory
 */

DR_EXPORT
bool
drmgr_register_low_on_memory_event(void (*func)())
{
    return drmgr_generic_event_add(&cblist_low_on_memory, low_on_memory_event_lock,
                                   (void (*)(void))func, NULL, false, NULL);
}

DR_EXPORT
bool
drmgr_register_low_on_memory_event_ex(void (*func)(), drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_low_on_memory, low_on_memory_event_lock,
                                   (void (*)(void))func, priority, false, NULL);
}

DR_EXPORT
bool
drmgr_register_low_on_memory_event_user_data(void (*func)(void *user_data),
                                             drmgr_priority_t *priority, void *user_data)
{
    return drmgr_generic_event_add(&cblist_low_on_memory, low_on_memory_event_lock,
                                   (void (*)(void))func, priority, true, user_data);
}

DR_EXPORT
bool
drmgr_unregister_low_on_memory_event(void (*func)())
{
    return drmgr_generic_event_remove(&cblist_low_on_memory, low_on_memory_event_lock,
                                      (void (*)(void))func);
}

DR_EXPORT
bool
drmgr_unregister_low_on_memory_event_user_data(void (*func)(void *user_data))
{
    return drmgr_generic_event_remove(&cblist_low_on_memory, low_on_memory_event_lock,
                                      (void (*)(void))func);
}

static void
drmgr_low_on_memory_event()
{
    void *drcontext = dr_get_current_drcontext();
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(low_on_memory_event_lock);
    cblist_create_local(drcontext, &cblist_low_on_memory, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(low_on_memory_event_lock);

    for (i = 0; i < iter.num_def; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;

        bool is_using_user_data = iter.cbs.generic[i].is_using_user_data;
        void *user_data = iter.cbs.generic[i].user_data;
        if (is_using_user_data == false) {
            (*iter.cbs.generic[i].cb.low_on_memory_cb.cb_no_user_data)();
        } else {
            (*iter.cbs.generic[i].cb.low_on_memory_cb.cb_user_data)(user_data);
        }
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

/***************************************************************************
 * INSTRUCTION NOTE FIELD
 */

enum {
    /* if drmgr itself needed note values we'd put them here */
    DRMGR_NOTE_FIRST_FREE = DRMGR_NOTE_NONE + 1,
};

static ptr_uint_t note_next = DRMGR_NOTE_FIRST_FREE;

/* un-reserving is not supported (would require interval tree to
 * impl) */
DR_EXPORT
ptr_uint_t
drmgr_reserve_note_range(size_t size)
{
    ptr_uint_t res;
    if (size == 0)
        return DRMGR_NOTE_NONE;
    dr_mutex_lock(note_lock);
    if (note_next + size < DR_NOTE_FIRST_RESERVED) {
        res = note_next;
        note_next += size;
    } else
        res = DRMGR_NOTE_NONE;
    dr_mutex_unlock(note_lock);
    return res;
}

DR_EXPORT
bool
drmgr_disable_auto_predication(void *drcontext, instrlist_t *ilist)
{
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION)
        return false;
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
    return true;
}

/***************************************************************************
 * EMULATION
 *
 * XXX i#4947: Should we add new instrumentation insertion events that
 * hide expansions and emulations for a simpler model than the current
 * query interfaces below?
 */

/*
 * Constants used when accessing emulated instruction data with the
 * drmgr_get_emulated_instr_data() function. Each constant refers
 * to an element of the emulated_instr_t struct.
 */
typedef enum {
    DRMGR_EMUL_INSTR_PC, /* The PC address of the emulated
                            instruction. */
    DRMGR_EMUL_INSTR,    /* The emulated instruction. */
    DRMGR_EMUL_FLAGS,    /* Flags controlling emulation. */
} emulated_instr_data_t;

/* Reserve space for emulation note values */
static void
drmgr_emulation_init(void)
{
    ASSERT(sizeof(emulated_instr_t) <= sizeof(dr_instr_label_data_t),
           "label data area is not large enough to store "
           "emulation data");

    note_base_emul = drmgr_reserve_note_range(DRMGR_NOTE_EMUL_COUNT);
    ASSERT(note_base_emul != DRMGR_NOTE_NONE, "failed to reserve emulation note space");
}

/* Get note values based on emulation specific enums. */
static inline ptr_int_t
get_emul_note_val(int enote_val)
{
    return (ptr_int_t)(note_base_emul + enote_val);
}

/* Write emulation data to a label's data area. */
static void
set_emul_label_data(instr_t *label, int type, ptr_uint_t data)
{
    dr_instr_label_data_t *label_data = instr_get_label_data_area(label);
    ASSERT(label_data != NULL, "failed to find label's data area");

    ASSERT(type >= DRMGR_EMUL_INSTR_PC && type <= DRMGR_EMUL_FLAGS,
           "type is invalid, should be an emulated_instr_data_t");
    label_data->data[type] = data;
}

/* Read emulation data from a label's data area. */
static ptr_uint_t
get_emul_label_data(instr_t *label, int type)
{
    dr_instr_label_data_t *label_data = instr_get_label_data_area(label);
    ASSERT(label_data != NULL, "failed to find label's data area");

    ASSERT(type >= DRMGR_EMUL_INSTR_PC && type <= DRMGR_EMUL_FLAGS,
           "type is invalid, should be an emulated_instr_data_t");
    return label_data->data[type];
}

/* A callback function to free the emulated instruction represented
 * by the label. This will be called when the label is freed.
 */
static void
free_einstr(void *drcontext, instr_t *label)
{
    instr_t *einstr = (instr_t *)get_emul_label_data(label, DRMGR_EMUL_INSTR);
    instr_destroy(drcontext, einstr);
}

/* Set the start emulation label and emulated instruction data */
DR_EXPORT
bool
drmgr_insert_emulation_start(void *drcontext, instrlist_t *ilist, instr_t *where,
                             emulated_instr_t *einstr)
{
    if (einstr->size < sizeof(emulated_instr_t)) {
        return false;
    }
    instr_t *start_emul_label = INSTR_CREATE_label(drcontext);
    instr_set_meta(start_emul_label);
    instr_set_note(start_emul_label, (void *)get_emul_note_val(DRMGR_NOTE_EMUL_START));

    set_emul_label_data(start_emul_label, DRMGR_EMUL_INSTR_PC, (ptr_uint_t)einstr->pc);
    set_emul_label_data(start_emul_label, DRMGR_EMUL_INSTR, (ptr_uint_t)einstr->instr);
    set_emul_label_data(start_emul_label, DRMGR_EMUL_FLAGS, (ptr_uint_t)einstr->flags);

    instr_set_label_callback(start_emul_label, free_einstr);
    instrlist_meta_preinsert(ilist, where, start_emul_label);

    return true;
}

/* Set the end emulation label */
DR_EXPORT
void
drmgr_insert_emulation_end(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    instr_t *stop_emul_label = INSTR_CREATE_label(drcontext);
    instr_set_meta(stop_emul_label);
    instr_set_note(stop_emul_label, (void *)get_emul_note_val(DRMGR_NOTE_EMUL_STOP));
    instrlist_meta_preinsert(ilist, where, stop_emul_label);
}

DR_EXPORT
bool
drmgr_is_emulation_start(instr_t *instr)
{
    return instr_is_label(instr) &&
        ((ptr_int_t)instr_get_note(instr) == get_emul_note_val(DRMGR_NOTE_EMUL_START));
}

DR_EXPORT
bool
drmgr_is_emulation_end(instr_t *instr)
{
    return instr_is_label(instr) &&
        ((ptr_int_t)instr_get_note(instr) == get_emul_note_val(DRMGR_NOTE_EMUL_STOP));
}

DR_EXPORT
bool
drmgr_get_emulated_instr_data(instr_t *instr, emulated_instr_t *emulated)
{
    ASSERT(instr_is_label(instr), "emulation instruction does not have a label");
    ASSERT(drmgr_is_emulation_start(instr), "instruction is not a start emulation label");

    if (emulated->size < offsetof(emulated_instr_t, flags))
        return false;

    emulated->pc = (app_pc)get_emul_label_data(instr, DRMGR_EMUL_INSTR_PC);
    emulated->instr = (instr_t *)get_emul_label_data(instr, DRMGR_EMUL_INSTR);
    if (emulated->size > offsetof(emulated_instr_t, flags)) {
        emulated->flags =
            (dr_emulate_options_t)get_emul_label_data(instr, DRMGR_EMUL_FLAGS);
    }

    return true;
}

DR_EXPORT
bool
drmgr_in_emulation_region(void *drcontext, OUT const emulated_instr_t **emulation_info)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION)
        return false;
    if (!pt->in_emulation_region)
        return false;
    if (emulation_info != NULL)
        *emulation_info = &pt->emulation_info;
    return true;
}

DR_EXPORT
instr_t *
drmgr_orig_app_instr_for_fetch(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION)
        return NULL;
    const emulated_instr_t *emulation;
    if (drmgr_in_emulation_region(drcontext, &emulation)) {
        if (TEST(DR_EMULATE_IS_FIRST_INSTR, emulation->flags)) {
            return emulation->instr;
        } // Else skip further instr fetches until outside emulation region.
    } else if (instr_is_app(pt->insertion_instr)) {
        return pt->insertion_instr;
    }
    return NULL;
}

DR_EXPORT
instr_t *
drmgr_orig_app_instr_for_operands(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, our_tls_idx);
    if (drmgr_current_bb_phase(drcontext) != DRMGR_PHASE_INSERTION)
        return NULL;
    const emulated_instr_t *emulation;
    if (drmgr_in_emulation_region(drcontext, &emulation)) {
        if (TEST(DR_EMULATE_IS_FIRST_INSTR, emulation->flags) &&
            !TEST(DR_EMULATE_INSTR_ONLY, emulation->flags))
            return emulation->instr;
        if (instr_is_app(pt->insertion_instr) &&
            TEST(DR_EMULATE_INSTR_ONLY, emulation->flags))
            return pt->insertion_instr;
    } else if (instr_is_app(pt->insertion_instr)) {
        return pt->insertion_instr;
    }
    return NULL;
}

/***************************************************************************
 * DRBBDUP
 */

static bool
is_bbdup_enabled()
{
    return bbdup_duplicate_cb != NULL;
}

bool
drmgr_register_bbdup_event(drmgr_bbdup_duplicate_bb_cb_t bb_dup_func,
                           drmgr_bbdup_insert_encoding_cb_t insert_encoding,
                           drmgr_bbdup_extract_cb_t extract_func,
                           drmgr_bbdup_stitch_cb_t stitch_func)
{
    bool succ = false;

    /* None of the cbs can be NULL. */
    if (bb_dup_func == NULL || insert_encoding == NULL || extract_func == NULL ||
        stitch_func == NULL)
        return succ;

    dr_rwlock_write_lock(bb_cb_lock);
    if (!is_bbdup_enabled()) {
        bbdup_duplicate_cb = bb_dup_func;
        bbdup_insert_encoding_cb = insert_encoding;
        bbdup_extract_cb = extract_func;
        bbdup_stitch_cb = stitch_func;
        cblist_init(&cblist_pre_bbdup, sizeof(cb_entry_t));
        succ = true;
    }
    dr_rwlock_write_unlock(bb_cb_lock);
    return succ;
}

bool
drmgr_unregister_bbdup_event()
{
    bool succ = false;
    dr_rwlock_write_lock(bb_cb_lock);
    if (is_bbdup_enabled()) {
        bbdup_duplicate_cb = NULL;
        bbdup_insert_encoding_cb = NULL;
        bbdup_extract_cb = NULL;
        bbdup_stitch_cb = NULL;
        cblist_delete(&cblist_pre_bbdup);
        ASSERT(!is_bbdup_enabled(), "should be disabled after unregistration");
        succ = true;
    }
    dr_rwlock_write_unlock(bb_cb_lock);
    return succ;
}

bool
drmgr_register_bbdup_pre_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (!is_bbdup_enabled())
        return false;

    if (func == NULL)
        return false; /* invalid params */

    return drmgr_bb_cb_add(&cblist_pre_bbdup, (void *)func, NULL, priority, NULL,
                           cb_entry_set_fields_xform);
}

bool
drmgr_unregister_bbdup_pre_event(drmgr_xform_cb_t func)
{
    if (!is_bbdup_enabled())
        return false;

    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_pre_bbdup, (void *)func, cb_entry_matches_xform);
}
