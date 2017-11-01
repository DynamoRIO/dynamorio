/* **********************************************************
 * Copyright (c) 2010-2017 Google, Inc.   All rights reserved.
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
#include "../ext_utils.h"
#ifdef UNIX
# include <string.h>
#endif
#include <stddef.h> /* offsetof */

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
#undef dr_register_signal_event
#undef dr_unregister_signal_event
#undef dr_register_exception_event
#undef dr_unregister_exception_event
#undef dr_register_restore_state_ex_event
#undef dr_unregister_restore_state_ex_event

/* currently using asserts on internal logic sanity checks (never on
 * input from user) but perhaps we shouldn't since this is a library
 */
#ifdef DEBUG
# define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
# define ASSERT(x, msg) /* nothing */
#endif

/***************************************************************************
 * TYPES
 */

/* priority list entry base struct */
typedef struct _priority_event_entry_t {
    bool valid; /* is whole entry valid (not just priority) */
    int priority;
    const char *name;
} priority_event_entry_t;


/* bb event list entry */
typedef struct _cb_entry_t {
    priority_event_entry_t pri;
    bool has_quartet;
    union {
        drmgr_xform_cb_t xform_cb;
        struct {
            drmgr_analysis_cb_t analysis_cb;
            drmgr_insertion_cb_t insertion_cb;
        } pair;

        /* quartet */
        drmgr_app2app_ex_cb_t app2app_ex_cb;
        struct {
            drmgr_ilist_ex_cb_t analysis_ex_cb;
            drmgr_insertion_cb_t insertion_ex_cb;
        } pair_ex;
        drmgr_ilist_ex_cb_t instru2instru_ex_cb;
    } cb;
} cb_entry_t;

/* generic event list entry */
typedef struct _generic_event_entry_t {
    priority_event_entry_t pri;
    bool is_ex;
    union {
        void (*generic_cb)(void);
        void (*thread_cb)(void *);
        void (*cls_cb)(void *, bool);
        bool (*presys_cb)(void *, int);
        void (*postsys_cb)(void *, int);
        void (*modload_cb)(void *, const module_data_t *, bool);
        void (*modunload_cb)(void *, const module_data_t *);
#ifdef UNIX
        dr_signal_action_t (*signal_cb)(void *, dr_siginfo_t *);
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
    size_t entry_sz; /* size of each entry */
    size_t num;      /* valid entries in array */
    size_t capacity; /* allocated entries in array */
} cb_list_t;

#define EVENTS_INITIAL_SZ 10
#define EVENTS_STACK_SZ 16

/* Our own TLS data */
typedef struct _per_thread_t {
    drmgr_bb_phase_t cur_phase;
    instr_t *first_app;
    instr_t *last_app;
} per_thread_t;

/***************************************************************************
 * GLOBALS
 */

/* Using read-write locks to protect counts and lists to allow concurrent
 * bb events and only require mutual exclusion when a cb is registered
 * or unregistered, which should be rare.
 */
static void *bb_cb_lock;

/* To know whether we need any DR events; protected by bb_cb_lock */
static uint bb_event_count;

/* Lists sorted by priority and protected by bb_cb_lock */
static cb_list_t cblist_app2app;
static cb_list_t cblist_instrumentation;
static cb_list_t cblist_instru2instru;

/* Count of callbacks needing user_data, protected by bb_cb_lock */
static uint pair_count;
static uint quartet_count;

/* Priority used for non-_ex events */
static const drmgr_priority_t default_priority = {
    sizeof(default_priority), "__DEFAULT__", NULL, NULL, 0
};

static int our_tls_idx;

static dr_emit_flags_t
drmgr_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating);

static void
drmgr_bb_init(void);

static void
drmgr_bb_exit(void);

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

#ifdef WINDOWS
static byte *addr_KiCallback;
static int sysnum_NtCallbackReturn;
# define CBRET_INTERRUPT_NUM 0x2b
#endif

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
drmgr_modload_event(void *drcontext, const module_data_t *info,
                    bool loaded);

static void
drmgr_modunload_event(void *drcontext, const module_data_t *info);

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

static bool
drmgr_cls_presys_event(void *drcontext, int sysnum);

#ifdef WINDOWS
static void
drmgr_cls_exit(void);
#endif

static void
our_thread_init_event(void *drcontext);

static void
our_thread_exit_event(void *drcontext);

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
#ifdef UNIX
    signal_event_lock = dr_rwlock_create();
#endif
#ifdef WINDOWS
    exception_event_lock = dr_rwlock_create();
#endif
    fault_event_lock = dr_rwlock_create();

    dr_register_thread_init_event(drmgr_thread_init_event);
    dr_register_thread_exit_event(drmgr_thread_exit_event);
    dr_register_pre_syscall_event(drmgr_presyscall_event);
    dr_register_post_syscall_event(drmgr_postsyscall_event);
    dr_register_module_load_event(drmgr_modload_event);
    dr_register_module_unload_event(drmgr_modunload_event);
#ifdef UNIX
    dr_register_signal_event(drmgr_signal_event);
#endif
#ifdef WINDOWS
    dr_register_exception_event(drmgr_exception_event);
#endif

    drmgr_bb_init();
    drmgr_event_init();

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

    drmgr_bb_exit();
    drmgr_event_exit();

    dr_unregister_thread_init_event(drmgr_thread_init_event);
    dr_unregister_thread_exit_event(drmgr_thread_exit_event);
    dr_unregister_pre_syscall_event(drmgr_presyscall_event);
    dr_unregister_post_syscall_event(drmgr_postsyscall_event);
    dr_unregister_module_load_event(drmgr_modload_event);
    dr_unregister_module_unload_event(drmgr_modunload_event);
#ifdef UNIX
    dr_unregister_signal_event(drmgr_signal_event);
#endif
#ifdef WINDOWS
    dr_unregister_exception_event(drmgr_exception_event);
#endif

    if (bb_event_count > 0)
        dr_unregister_bb_event(drmgr_bb_event);
    if (registered_fault)
        dr_unregister_restore_state_ex_event(drmgr_restore_state_event);
#ifdef WINDOWS
    drmgr_cls_exit();
#endif

    dr_rwlock_destroy(fault_event_lock);
#ifdef UNIX
    dr_rwlock_destroy(signal_event_lock);
#endif
#ifdef WINDOWS
    dr_rwlock_destroy(exception_event_lock);
#endif
    dr_rwlock_destroy(modunload_event_lock);
    dr_rwlock_destroy(modload_event_lock);
    dr_rwlock_destroy(postsys_event_lock);
    dr_rwlock_destroy(presys_event_lock);
    dr_rwlock_destroy(cls_event_lock);
    dr_mutex_destroy(tls_lock);
    dr_rwlock_destroy(thread_event_lock);
    dr_rwlock_destroy(bb_cb_lock);

    dr_mutex_destroy(note_lock);
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
    l->num = 0;
    l->capacity = EVENTS_INITIAL_SZ;
    l->cbs.array = dr_global_alloc(l->capacity * l->entry_sz);
}

static void
cblist_delete(cb_list_t *l)
{
    dr_global_free(l->cbs.array, l->capacity * l->entry_sz);
}

static inline priority_event_entry_t *
cblist_get_pri(cb_list_t *l, uint idx)
{
    return (priority_event_entry_t *)(l->cbs.array + idx*l->entry_sz);
}

/* Caller must hold write lock.
 * Returns -1 on error.
 */
static int
cblist_shift_and_resize(cb_list_t *l, uint insert_at)
{
    size_t orig_num = l->num;
    if (insert_at > l->num)
        return -1;
    /* check for invalid slots we can easily use */
    if (insert_at < l->num && !cblist_get_pri(l, insert_at)->valid)
        return insert_at;
    else if (insert_at > 0 && !cblist_get_pri(l, insert_at - 1)->valid)
        return insert_at - 1;
    l->num++;
    if (l->num >= l->capacity) {
        /* do the shift implicitly while resizing */
        size_t new_cap = l->capacity * 2;
        byte *new_array = dr_global_alloc(new_cap * l->entry_sz);
        memcpy(new_array, l->cbs.array, insert_at*l->entry_sz);
        memcpy(new_array + (insert_at+1)*l->entry_sz,
               l->cbs.array + insert_at*l->entry_sz,
               (orig_num - insert_at)*l->entry_sz);
        dr_global_free(l->cbs.array, l->capacity * l->entry_sz);
        l->cbs.array = new_array;
        l->capacity = new_cap;
    } else {
        if (insert_at == orig_num)
            return insert_at;
        memmove(l->cbs.array + (insert_at+1)*l->entry_sz,
                l->cbs.array + insert_at*l->entry_sz,
                (l->num - insert_at)*l->entry_sz);
    }
    return insert_at;
}

/* Creates a temporary local copy, using local if it's big enough but
 * otherwise allocating new space on the heap.
 * Caller must hold read lock.
 */
static void
cblist_create_local(void *drcontext, cb_list_t *src, cb_list_t *dst,
                    byte *local, size_t local_num)
{
    dst->entry_sz = src->entry_sz;
    dst->num = src->num;
    dst->capacity = src->num;
    if (src->num > local_num) {
        dst->cbs.array = dr_thread_alloc(drcontext, src->num*src->entry_sz);
    } else {
        dst->cbs.array = local;
    }
    memcpy(dst->cbs.array, src->cbs.array, src->num*src->entry_sz);
}

static void
cblist_delete_local(void *drcontext, cb_list_t *l, size_t local_num)
{
    if (l->num > local_num) {
        dr_thread_free(drcontext, l->cbs.array, l->num*l->entry_sz);
    } /* else nothing to do */
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
        if (instr_is_app(inst) &&
            instr_opcode_valid(inst) &&
            /* For -fast_client_decode we can have level 0 instrs so check
             * to ensure this is an single instr with valid opcode.
             */
            instr_is_cti(inst) &&
            opnd_is_instr(instr_get_target(inst))) {
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

static dr_emit_flags_t
drmgr_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating)
{
    uint i;
    cb_entry_t *e;
    dr_emit_flags_t res = DR_EMIT_DEFAULT;
    instr_t *inst, *next_inst;
    void **pair_data = NULL, **quartet_data = NULL;
    uint pair_idx, quartet_idx;
    cb_entry_t local_app2app[EVENTS_STACK_SZ];
    cb_entry_t local_insert[EVENTS_STACK_SZ];
    cb_entry_t local_instru[EVENTS_STACK_SZ];
    cb_list_t iter_app2app;
    cb_list_t iter_insert;
    cb_list_t iter_instru;
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, our_tls_idx);

    dr_rwlock_read_lock(bb_cb_lock);
    /* We use arrays to more easily support unregistering while in an event (i#1356).
     * With arrays we can make a temporary copy and avoid holding a lock while
     * delivering events.
     */
    cblist_create_local(drcontext, &cblist_app2app, &iter_app2app,
                        (byte *)local_app2app, BUFFER_SIZE_ELEMENTS(local_app2app));
    cblist_create_local(drcontext, &cblist_instrumentation, &iter_insert,
                        (byte *)local_insert, BUFFER_SIZE_ELEMENTS(local_insert));
    cblist_create_local(drcontext, &cblist_instru2instru, &iter_instru,
                        (byte *)local_instru, BUFFER_SIZE_ELEMENTS(local_instru));
    dr_rwlock_read_unlock(bb_cb_lock);

    /* We need per-thread user_data */
    if (pair_count > 0)
        pair_data = (void **) dr_thread_alloc(drcontext, sizeof(void*)*pair_count);
    if (quartet_count > 0)
        quartet_data = (void **) dr_thread_alloc(drcontext, sizeof(void*)*quartet_count);

    /* Pass 1: app2app */
    /* XXX: better to avoid all this set_tls overhead and assume DR is globally
     * synchronizing bb building anyway and use a global var + mutex?
     */
    pt->cur_phase = DRMGR_PHASE_APP2APP;
    for (quartet_idx = 0, i = 0; i < iter_app2app.num; i++) {
        e = &iter_app2app.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quartet) {
            res |= (*e->cb.app2app_ex_cb)
                (drcontext, tag, bb, for_trace, translating, &quartet_data[quartet_idx]);
            quartet_idx++;
        } else
            res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    /* Pass 2: analysis */
    pt->cur_phase = DRMGR_PHASE_ANALYSIS;
    for (quartet_idx = 0, pair_idx = 0, i = 0; i < iter_insert.num; i++) {
        e = &iter_insert.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quartet) {
            res |= (*e->cb.pair_ex.analysis_ex_cb)
                (drcontext, tag, bb, for_trace, translating, quartet_data[quartet_idx]);
            quartet_idx++;
        } else {
            if (e->cb.pair.analysis_cb == NULL) {
                pair_data[pair_idx] = NULL;
            } else {
                res |= (*e->cb.pair.analysis_cb)
                    (drcontext, tag, bb, for_trace, translating, &pair_data[pair_idx]);
            }
            pair_idx++;
        }
        /* XXX: add checks that cb followed the rules */
    }

    /* Pass 3: instru, per instr */
    pt->cur_phase = DRMGR_PHASE_INSERTION;
    pt->first_app = instrlist_first(bb);
    pt->last_app = instrlist_last(bb);
    for (inst = instrlist_first(bb); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        for (quartet_idx = 0, pair_idx = 0, i = 0; i < iter_insert.num; i++) {
            e = &iter_insert.cbs.bb[i];
            if (!e->pri.valid)
                continue;
            /* Most client instrumentation wants to be predicated to match the app
             * instruction, so we do it by default (i#1723). Clients may opt-out
             * by calling drmgr_disable_auto_predication() at the start of the
             * insertion bb event.
             */
            instrlist_set_auto_predicate(bb, instr_get_predicate(inst));
            if (e->has_quartet) {
                res |= (*e->cb.pair_ex.insertion_ex_cb)
                    (drcontext, tag, bb, inst, for_trace, translating,
                     quartet_data[quartet_idx]);
                quartet_idx++;
            } else {
                if (e->cb.pair.insertion_cb != NULL) {
                    res |= (*e->cb.pair.insertion_cb)
                        (drcontext, tag, bb, inst, for_trace, translating,
                         pair_data[pair_idx]);
                }
                pair_idx++;
            }
            instrlist_set_auto_predicate(bb, DR_PRED_NONE);
            /* XXX: add checks that cb followed the rules */
        }
    }

    /* Pass 4: final */
    pt->cur_phase = DRMGR_PHASE_INSTRU2INSTRU;
    for (quartet_idx = 0, i = 0; i < iter_instru.num; i++) {
        e = &iter_instru.cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if (e->has_quartet) {
            res |= (*e->cb.instru2instru_ex_cb)
                (drcontext, tag, bb, for_trace, translating, quartet_data[quartet_idx]);
            quartet_idx++;
        } else
            res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

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

    pt->cur_phase = DRMGR_PHASE_NONE;

    if (pair_count > 0)
        dr_thread_free(drcontext, pair_data, sizeof(void*)*pair_count);
    if (quartet_count > 0)
        dr_thread_free(drcontext, quartet_data, sizeof(void*)*quartet_count);

    cblist_delete_local(drcontext, &iter_app2app, BUFFER_SIZE_ELEMENTS(local_app2app));
    cblist_delete_local(drcontext, &iter_insert, BUFFER_SIZE_ELEMENTS(local_insert));
    cblist_delete_local(drcontext, &iter_instru, BUFFER_SIZE_ELEMENTS(local_instru));

    return res;
}

/* Caller must hold write lock.
 * priority can be NULL in which case default_priority is used.
 * Returns -1 on error, else index of new entry.
 */
static int
priority_event_add(cb_list_t *list,
                   drmgr_priority_t *new_pri)
{
    int i;
    bool past_after; /* are we past the "after" constraint */
    bool found_before; /* did we find the "before" constraint */
    priority_event_entry_t *pri;

    if (new_pri == NULL)
        new_pri = (drmgr_priority_t *) &default_priority;
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
        for (i = 0; i < (int)list->num; i++) {
            pri = cblist_get_pri(list, i);
            if (pri->valid && strcmp(new_pri->name, pri->name) == 0)
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
    for (i = 0; i < (int)list->num; i++) {
        pri = cblist_get_pri(list, i);
        if (!pri->valid)
            continue;
        /* Primary sort: numeric priority.  Tie goes to 1st to register. */
        if (pri->priority > new_pri->priority)
            break;
        /* Secondary constraint #1: must be before "before" */
        if (new_pri->before != NULL && strcmp(new_pri->before, pri->name) == 0) {
            found_before = true;
            if (pri->priority < new_pri->priority) {
                /* cannot satisfy both before and numeric */
                return -1;
            }
            break;
        }
        /* Secondary constraint #2: must be after "after" */
        else if (!past_after) {
            ASSERT(new_pri->after != NULL, "past_after should be true");
            if (strcmp(new_pri->after, pri->name) == 0)
                past_after = true;
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
        for (j = i; j < (int)list->num; j++) {
            pri = cblist_get_pri(list, j);
            if (pri->valid && strcmp(new_pri->before, pri->name) == 0) {
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
    pri->name = new_pri->name;
    pri->priority = new_pri->priority;
    return (int) i;
}

static bool
drmgr_bb_cb_add(cb_list_t *list,
                drmgr_xform_cb_t xform_func,
                drmgr_analysis_cb_t analysis_func,
                drmgr_insertion_cb_t insertion_func,
                /* for quartet (also uses insertion_func) */
                drmgr_app2app_ex_cb_t app2app_ex_func,
                drmgr_ilist_ex_cb_t analysis_ex_func,
                drmgr_ilist_ex_cb_t instru2instru_ex_func,
                drmgr_priority_t *priority)
{
    int idx;
    bool res = false;
    ASSERT(list != NULL, "invalid internal params");
    ASSERT(((xform_func != NULL && analysis_func == NULL && insertion_func == NULL &&
             app2app_ex_func == NULL && analysis_ex_func == NULL &&
             instru2instru_ex_func == NULL) ||
            (xform_func == NULL && (analysis_func != NULL || insertion_func != NULL) &&
             app2app_ex_func == NULL && analysis_ex_func == NULL &&
             instru2instru_ex_func == NULL) ||
            (xform_func == NULL && analysis_func == NULL && insertion_func == NULL &&
             app2app_ex_func != NULL && analysis_ex_func == NULL &&
             instru2instru_ex_func == NULL) ||
            (xform_func == NULL && analysis_func == NULL && insertion_func != NULL &&
             app2app_ex_func == NULL && analysis_ex_func != NULL &&
             instru2instru_ex_func == NULL) ||
            (xform_func == NULL && analysis_func == NULL && insertion_func == NULL &&
             app2app_ex_func == NULL && analysis_ex_func == NULL &&
             instru2instru_ex_func != NULL)),
           "invalid internal params");

    dr_rwlock_write_lock(bb_cb_lock);
    idx = priority_event_add(list, priority);
    if (idx >= 0) {
        cb_entry_t *new_e = &list->cbs.bb[idx];
        if (app2app_ex_func != NULL) {
            new_e->has_quartet = true;
            new_e->cb.app2app_ex_cb = app2app_ex_func;
        } else if (analysis_ex_func != NULL) {
            new_e->has_quartet = true;
            new_e->cb.pair_ex.analysis_ex_cb = analysis_ex_func;
            new_e->cb.pair_ex.insertion_ex_cb = insertion_func;
        } else if (instru2instru_ex_func != NULL) {
            new_e->has_quartet = true;
            new_e->cb.instru2instru_ex_cb = instru2instru_ex_func;
        } else {
            new_e->has_quartet = false;
            if (xform_func != NULL) {
                new_e->cb.xform_cb = xform_func;
            } else {
                new_e->cb.pair.analysis_cb = analysis_func;
                new_e->cb.pair.insertion_cb = insertion_func;
            }
        }
        if (bb_event_count == 0)
            dr_register_bb_event(drmgr_bb_event);
        bb_event_count++;
        if (new_e->has_quartet)
            quartet_count++;
        else if (xform_func == NULL)
            pair_count++;
        res = true;
    }
    dr_rwlock_write_unlock(bb_cb_lock);
    return res;
}

DR_EXPORT
bool
drmgr_register_bb_app2app_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_app2app, func, NULL, NULL,
                           NULL, NULL, NULL, priority);
}

DR_EXPORT
bool
drmgr_register_bb_instrumentation_event(drmgr_analysis_cb_t analysis_func,
                                        drmgr_insertion_cb_t insertion_func,
                                        drmgr_priority_t *priority)
{
    if (analysis_func == NULL && insertion_func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_instrumentation, NULL, analysis_func,
                           insertion_func, NULL, NULL, NULL, priority);
}

DR_EXPORT
bool
drmgr_register_bb_instru2instru_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_instru2instru, func, NULL, NULL,
                           NULL, NULL, NULL, priority);
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
        ok = drmgr_bb_cb_add(&cblist_app2app, NULL, NULL, NULL, app2app_func,
                             NULL, NULL, priority) && ok;
    }
    if (analysis_func != NULL) {
        ok = drmgr_bb_cb_add(&cblist_instrumentation, NULL, NULL, insertion_func,
                             NULL, analysis_func, NULL, priority) && ok;
    }
    if (instru2instru_func != NULL) {
        ok = drmgr_bb_cb_add(&cblist_instru2instru, NULL, NULL, NULL,
                             NULL, NULL, instru2instru_func, priority) && ok;
    }
    return ok;
}

static bool
drmgr_bb_cb_remove(cb_list_t *list,
                   drmgr_xform_cb_t xform_func,
                   drmgr_analysis_cb_t analysis_func,
                   drmgr_insertion_cb_t insertion_func,
                   /* for quartet */
                   drmgr_app2app_ex_cb_t app2app_ex_func,
                   drmgr_ilist_ex_cb_t analysis_ex_func,
                   drmgr_ilist_ex_cb_t instru2instru_ex_func)
{
    bool res = false;
    uint i;
    ASSERT(list != NULL, "invalid internal params");
    ASSERT((xform_func != NULL && analysis_func == NULL) ||
           (xform_func == NULL && analysis_func != NULL) ||
           (xform_func == NULL && analysis_func == NULL &&
            insertion_func != NULL) ||
           (xform_func == NULL && analysis_func == NULL &&
            (app2app_ex_func != NULL ||
             analysis_ex_func != NULL ||
             instru2instru_ex_func != NULL)), "invalid internal params");

    dr_rwlock_write_lock(bb_cb_lock);
    for (i = 0; i < list->num; i++) {
        cb_entry_t *e = &list->cbs.bb[i];
        if (!e->pri.valid)
            continue;
        if ((xform_func != NULL && xform_func == e->cb.xform_cb) ||
            (analysis_func != NULL && analysis_func == e->cb.pair.analysis_cb) ||
            (insertion_func != NULL && insertion_func == e->cb.pair.insertion_cb) ||
            (app2app_ex_func != NULL && app2app_ex_func == e->cb.app2app_ex_cb) ||
            (analysis_ex_func != NULL &&
             analysis_ex_func == e->cb.pair_ex.analysis_ex_cb) ||
            (instru2instru_ex_func != NULL &&
             instru2instru_ex_func == e->cb.instru2instru_ex_cb)) {
            res = true;
            e->pri.valid = false;
            if (i == list->num - 1)
                list->num--;
            if (e->has_quartet)
                quartet_count--;
            else if (xform_func == NULL)
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
}

DR_EXPORT
bool
drmgr_unregister_bb_app2app_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_app2app, func, NULL, NULL, NULL, NULL, NULL);
}

DR_EXPORT
bool
drmgr_unregister_bb_instrumentation_event(drmgr_analysis_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instrumentation, NULL, func, NULL, NULL,
                              NULL, NULL);
}

DR_EXPORT
bool
drmgr_unregister_bb_insertion_event(drmgr_insertion_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instrumentation, NULL, NULL, func, NULL, NULL,
                              NULL);
}

DR_EXPORT
bool
drmgr_unregister_bb_instru2instru_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instru2instru, func, NULL, NULL, NULL, NULL, NULL);
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
        ok = drmgr_bb_cb_remove(&cblist_app2app, NULL, NULL, NULL, app2app_func,
                                NULL, NULL) && ok;
    }
    if (analysis_func != NULL) {
        /* Although analysis_func and insertion_func are registered together in
         * drmgr_register_bb_instrumentation_ex_event, drmgr_bb_cb_remove only
         * checks analysis_func, so we pass NULL instead of insertion_func here.
         */
        ok = drmgr_bb_cb_remove(&cblist_instrumentation, NULL, NULL, NULL, NULL,
                                analysis_func, NULL) && ok;
    }
    if (instru2instru_func != NULL) {
        ok = drmgr_bb_cb_remove(&cblist_instru2instru, NULL, NULL, NULL, NULL,
                                NULL, instru2instru_func) && ok;
    }
    return ok;
}

DR_EXPORT
drmgr_bb_phase_t
drmgr_current_bb_phase(void *drcontext)
{
    per_thread_t *pt;
    /* Support being called w/o being set up, for detection of whether under drmgr */
    if (drmgr_init_count == 0)
        return DRMGR_PHASE_NONE;
    pt = (per_thread_t *) drmgr_get_tls_field(drcontext, our_tls_idx);
    return pt->cur_phase;
}

DR_EXPORT
bool
drmgr_is_first_instr(void *drcontext, instr_t *instr)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, our_tls_idx);
    return instr == pt->first_app;
}

DR_EXPORT
bool
drmgr_is_last_instr(void *drcontext, instr_t *instr)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, our_tls_idx);
    return instr == pt->last_app;
}

static void
our_thread_init_event(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    drmgr_set_tls_field(drcontext, our_tls_idx, (void *) pt);
}

static void
our_thread_exit_event(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, our_tls_idx);
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
drmgr_generic_event_add_ex(cb_list_t *list,
                           void *rwlock,
                           void (*func)(void),
                           drmgr_priority_t *priority,
                           bool is_ex)
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
    }
    dr_rwlock_write_unlock(rwlock);
    return res;
}

static bool
drmgr_generic_event_add(cb_list_t *list,
                        void *rwlock,
                        void (*func)(void),
                        drmgr_priority_t *priority)
{
    return drmgr_generic_event_add_ex(list, rwlock, func, priority, false);
}

static bool
drmgr_generic_event_remove(cb_list_t *list,
                           void *rwlock,
                           void (*func)(void))
{
    bool res = false;
    uint i;
    generic_event_entry_t *e;
    if (func == NULL)
        return false;
    dr_rwlock_write_lock(rwlock);
    for (i = 0; i < list->num; i++) {
        e = &list->cbs.generic[i];
        if (!e->pri.valid)
            continue;
        if (e->cb.generic_cb == func) {
            res = true;
            e->pri.valid = false;
            break;
        }
    }
    dr_rwlock_write_unlock(rwlock);
    return res;
}

static void
drmgr_event_init(void)
{
    cblist_init(&cb_list_thread_init, sizeof(generic_event_entry_t));
    cblist_init(&cb_list_thread_exit, sizeof(generic_event_entry_t));
    cblist_init(&cblist_cls_init, sizeof(generic_event_entry_t));
    cblist_init(&cblist_cls_exit, sizeof(generic_event_entry_t));
    cblist_init(&cblist_presys, sizeof(generic_event_entry_t));
    cblist_init(&cblist_postsys, sizeof(generic_event_entry_t));
    cblist_init(&cblist_modload, sizeof(generic_event_entry_t));
    cblist_init(&cblist_modunload, sizeof(generic_event_entry_t));
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
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_thread_init_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_remove(&cb_list_thread_init, thread_event_lock,
                                      (void (*)(void)) func);
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
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_thread_exit_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_remove(&cb_list_thread_exit, thread_event_lock,
                                      (void (*)(void)) func);
}

DR_EXPORT
bool
drmgr_register_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_add(&cblist_presys, presys_event_lock,
                                   (void (*)(void)) func, NULL);
}

DR_EXPORT
bool
drmgr_register_pre_syscall_event_ex(bool (*func)(void *drcontext, int sysnum),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_presys, presys_event_lock,
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_remove(&cblist_presys, presys_event_lock,
                                      (void (*)(void)) func);
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

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        execute = (*iter.cbs.generic[i].cb.presys_cb)(drcontext, sysnum) && execute;
    }

    /* this must go last (the whole reason we're wrapping this) */
    execute = drmgr_cls_presys_event(drcontext, sysnum) && execute;

    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));

    return execute;
}

DR_EXPORT
bool
drmgr_register_post_syscall_event(void (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_add(&cblist_postsys, postsys_event_lock,
                                   (void (*)(void)) func, NULL);
}

DR_EXPORT
bool
drmgr_register_post_syscall_event_ex(void (*func)(void *drcontext, int sysnum),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_postsys, postsys_event_lock,
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_post_syscall_event(void (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_remove(&cblist_postsys, postsys_event_lock,
                                      (void (*)(void)) func);
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

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.postsys_cb)(drcontext, sysnum);
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
                                   (void (*)(void)) func, NULL);
}

DR_EXPORT
bool
drmgr_register_module_load_event_ex(void (*func)
                                    (void *drcontext, const module_data_t *info,
                                     bool loaded),
                                    drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_modload, modload_event_lock,
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_module_load_event(void (*func)
                                   (void *drcontext, const module_data_t *info,
                                    bool loaded))
{
    return drmgr_generic_event_remove(&cblist_modload, modload_event_lock,
                                      (void (*)(void)) func);
}

static void
drmgr_modload_event(void *drcontext, const module_data_t *info,
                    bool loaded)
{
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    dr_rwlock_read_lock(modload_event_lock);
    cblist_create_local(drcontext, &cblist_modload, &iter, (byte *)local,
                        BUFFER_SIZE_ELEMENTS(local));
    dr_rwlock_read_unlock(modload_event_lock);

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.modload_cb)(drcontext, info, loaded);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

DR_EXPORT
bool
drmgr_register_module_unload_event(void (*func)
                                   (void *drcontext, const module_data_t *info))
{
    return drmgr_generic_event_add(&cblist_modunload, modunload_event_lock,
                                   (void (*)(void)) func, NULL);
}

DR_EXPORT
bool
drmgr_register_module_unload_event_ex(void (*func)
                                      (void *drcontext, const module_data_t *info),
                                      drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_modunload, modunload_event_lock,
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_module_unload_event(void (*func)
                                     (void *drcontext, const module_data_t *info))
{
    return drmgr_generic_event_remove(&cblist_modunload, modunload_event_lock,
                                      (void (*)(void)) func);
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

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.modunload_cb)(drcontext, info);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
}

/***************************************************************************
 * WRAPPED FAULT EVENTS
 */

#ifdef UNIX
DR_EXPORT
bool
drmgr_register_signal_event(dr_signal_action_t (*func)
                         (void *drcontext, dr_siginfo_t *siginfo))
{
    return drmgr_generic_event_add(&cblist_signal, signal_event_lock,
                                   (void (*)(void)) func, NULL);
}

DR_EXPORT
bool
drmgr_register_signal_event_ex(dr_signal_action_t (*func)
                               (void *drcontext, dr_siginfo_t *siginfo),
                               drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_signal, signal_event_lock,
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_signal_event(dr_signal_action_t (*func)
                              (void *drcontext, dr_siginfo_t *siginfo))
{
    return drmgr_generic_event_remove(&cblist_signal, signal_event_lock,
                                      (void (*)(void)) func);
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

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        /* follow DR semantics: short-circuit on first handler to "own" the signal */
        res = (*iter.cbs.generic[i].cb.signal_cb)(drcontext, siginfo);
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
                                   (void (*)(void)) func, NULL);
}

DR_EXPORT
bool
drmgr_register_exception_event_ex(bool (*func)(void *drcontext, dr_exception_t *excpt),
                                  drmgr_priority_t *priority)
{
    return drmgr_generic_event_add(&cblist_exception, exception_event_lock,
                                   (void (*)(void)) func, priority);
}

DR_EXPORT
bool
drmgr_unregister_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt))
{
    return drmgr_generic_event_remove(&cblist_exception, exception_event_lock,
                                      (void (*)(void)) func);
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

    for (i = 0; i < iter.num; i++) {
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
drmgr_register_restore_state_event(void (*func)
                                   (void *drcontext, void *tag, dr_mcontext_t *mcontext,
                                    bool restore_memory, bool app_code_consistent))
{
    drmgr_register_fault_event();
    return drmgr_generic_event_add_ex(&cblist_fault, fault_event_lock,
                                      (void (*)(void)) func, NULL, false/*!ex*/);
}

DR_EXPORT
bool
drmgr_register_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                   dr_restore_state_info_t *info))
{
    drmgr_register_fault_event();
    return drmgr_generic_event_add_ex(&cblist_fault, fault_event_lock,
                                      (void (*)(void)) func, NULL, true/*ex*/);
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
                                      (void (*)(void)) func, priority, true/*ex*/);
}

DR_EXPORT
bool
drmgr_unregister_restore_state_event(void (*func)
                                     (void *drcontext, void *tag, dr_mcontext_t *mcontext,
                                      bool restore_memory, bool app_code_consistent))
{
    /* we never unregister our event once registered */
    return drmgr_generic_event_remove(&cblist_fault, fault_event_lock,
                                      (void (*)(void)) func);
}

DR_EXPORT
bool
drmgr_unregister_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                     dr_restore_state_info_t *info))
{
    return drmgr_generic_event_remove(&cblist_fault, fault_event_lock,
                                      (void (*)(void)) func);
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

    for (i = 0; i < iter.num; i++) {
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

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.thread_cb)(drcontext);
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

    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.thread_cb)(drcontext);
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
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    /* no need to check for tls_taken since would return NULL anyway (i#484) */
    if (idx < 0 || idx > MAX_NUM_TLS || tls == NULL)
        return NULL;
    return tls->tls[idx];
}

DR_EXPORT
bool
drmgr_set_tls_field(void *drcontext, int idx, void *value)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
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
drmgr_insert_read_tls_field(void *drcontext, int idx,
                            instrlist_t *ilist, instr_t *where, reg_id_t reg)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !tls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, reg);
    instrlist_meta_preinsert(ilist, where, XINST_CREATE_load
                             (drcontext, opnd_create_reg(reg),
                              OPND_CREATE_MEMPTR(reg, offsetof(tls_array_t, tls) +
                                                 idx*sizeof(void*))));
    return true;
}

DR_EXPORT
bool
drmgr_insert_write_tls_field(void *drcontext, int idx,
                             instrlist_t *ilist, instr_t *where, reg_id_t reg,
                             reg_id_t scratch)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !tls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg) ||
        !reg_is_gpr(scratch) || !reg_is_pointer_sized(scratch))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, scratch);
    instrlist_meta_preinsert(ilist, where, XINST_CREATE_store
                             (drcontext,
                              OPND_CREATE_MEMPTR(scratch, offsetof(tls_array_t, tls) +
                                                 idx*sizeof(void*)),
                               opnd_create_reg(reg)));
    return true;
}


/***************************************************************************
 * CLS
 */

#ifdef WINDOWS
static int cls_initialized; /* 0=not tried; >0=success; <0=failure */
#endif

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
    for (i = 0; i < iter.num; i++) {
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
    return drmgr_cls_stack_push_event(drcontext, true/*new_depth*/);
}

static bool
drmgr_cls_stack_push(void)
{
    void *drcontext = dr_get_current_drcontext();
    tls_array_t *tls_parent = (tls_array_t *) dr_get_tls_field(drcontext);
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
    memcpy(tls_child->tls, tls_parent->tls, sizeof(*tls_child->tls)*MAX_NUM_TLS);
    /* swap in as the current structure */
    dr_set_tls_field(drcontext, (void *)tls_child);

    return drmgr_cls_stack_push_event(drcontext, new_depth);
}

static bool
drmgr_cls_stack_pop(void)
{
    /* Our callback enter is AFTER DR's, but our callback exit is BEFORE. */
    generic_event_entry_t local[EVENTS_STACK_SZ];
    cb_list_t iter;
    uint i;
    void *drcontext = dr_get_current_drcontext();
    tls_array_t *tls_child = (tls_array_t *) dr_get_tls_field(drcontext);
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
    for (i = 0; i < iter.num; i++) {
        if (!iter.cbs.generic[i].pri.valid)
            continue;
        (*iter.cbs.generic[i].cb.cls_cb)(drcontext, false/*!thread_exit*/);
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));

    /* update tls w/ any changes while in child context */
    memcpy(tls_parent->tls, tls_child->tls, sizeof(*tls_child->tls)*MAX_NUM_TLS);
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
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
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
        for (i = 0; i < iter.num; i++) {
            if (!iter.cbs.generic[i].pri.valid)
                continue;
            (*iter.cbs.generic[i].cb.cls_cb)(drcontext, true/*thread_exit*/);
        }
        dr_thread_free(drcontext, tmp, sizeof(*tmp));
    }
    cblist_delete_local(drcontext, &iter, BUFFER_SIZE_ELEMENTS(local));
    dr_set_tls_field(drcontext, NULL);
    return true;
}

#ifdef WINDOWS
static bool
drmgr_event_filter_syscall(void *drcontext, int sysnum)
{
    if (sysnum == sysnum_NtCallbackReturn)
        return true;
    else
        return false;
}

static bool
drmgr_cls_presys_event(void *drcontext, int sysnum)
{
    /* we wrap the pre-syscall event to ensure this goes last,
     * after all other presys events, so we have no references
     * to the cls data before we swap it
     */
    if (sysnum == sysnum_NtCallbackReturn)
        drmgr_cls_stack_pop();
    return true;
}

/* Goes first with high negative priority */
static dr_emit_flags_t
drmgr_event_insert_cb(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    if (instr_get_app_pc(inst) == addr_KiCallback) {
        dr_insert_clean_call(drcontext, bb, inst, (void *)drmgr_cls_stack_push,
                             false, 0);
    }
    return DR_EMIT_DEFAULT;
}

/* Goes last with high positive priority */
static dr_emit_flags_t
drmgr_event_insert_cbret(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                         bool for_trace, bool translating, void *user_data)
{
    if (instr_opcode_valid(inst) &&
        /* For -fast_client_decode we can have level 0 instrs so check
         * to ensure this is an single instr with valid opcode.
         */
        instr_get_opcode(inst) == OP_int &&
        opnd_get_immed_int(instr_get_src(inst, 0)) == CBRET_INTERRUPT_NUM) {
        dr_insert_clean_call(drcontext, bb, inst, (void *)drmgr_cls_stack_pop,
                             false, 0);
    }
    return DR_EMIT_DEFAULT;
}

/* Determines the syscall from its Nt* wrapper.
 * Returns -1 on error.
 * FIXME: does not handle somebody hooking the wrapper.
 */
/* XXX: exporting this so drwrap can use it but I might prefer to have
 * this in drutil or the upcoming drsys
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
            num = (int) opnd_get_immed_int(instr_get_src(&instr, 0));
            break; /* success */
        }
        /* stop at call to vsyscall (wow64) or at int itself */
    } while (opc != OP_call_ind && opc != OP_int &&
             opc != OP_sysenter && opc != OP_syscall &&
             opc != OP_ret);
    instr_free(drcontext, &instr);
    return num;
}

static bool
drmgr_cls_init(void)
{
    /* For callback init we watch for KiUserCallbackDispatcher.
     * For callback exit we watch for NtCallbackReturn or int 0x2b.
     */
    module_data_t *data;
    module_handle_t ntdll_lib;
    app_pc addr_cbret;
    /* We need to go very early to push the new CLS context */
    drmgr_priority_t pri_cb = {sizeof(pri_cb), DRMGR_PRIORITY_NAME_CLS_ENTRY,
                               NULL, NULL, DRMGR_PRIORITY_INSERT_CLS_ENTRY};
    drmgr_priority_t pri_cbret = {sizeof(pri_cbret), DRMGR_PRIORITY_NAME_CLS_EXIT,
                                  NULL, NULL, DRMGR_PRIORITY_INSERT_CLS_EXIT};

    if (cls_initialized > 0)
        return true;
    else if (cls_initialized < 0)
        return false;
    cls_initialized = -1;

    data = dr_lookup_module_by_name("ntdll.dll");
    if (data == NULL) {
        /* fatal error: something is really wrong w/ underlying DR */
        return false;
    }
    ntdll_lib = data->handle;
    dr_free_module_data(data);
    addr_KiCallback = (app_pc) dr_get_proc_address(ntdll_lib, "KiUserCallbackDispatcher");
    if (addr_KiCallback == NULL)
        return false; /* should not happen */
    /* the wrapper is not good enough for two reasons: one, we want to swap
     * contexts at the last possible moment, not prior to executing a few
     * instrs; second, we'll miss hand-rolled syscalls
     */
    addr_cbret = (app_pc) dr_get_proc_address(ntdll_lib, "NtCallbackReturn");
    if (addr_cbret == NULL)
        return false; /* should not happen */
    sysnum_NtCallbackReturn = drmgr_decode_sysnum_from_wrapper(addr_cbret);
    if (sysnum_NtCallbackReturn == -1)
        return false; /* should not happen */

    if (!drmgr_register_bb_instrumentation_event(NULL, drmgr_event_insert_cb, &pri_cb) ||
        !drmgr_register_bb_instrumentation_event(NULL, drmgr_event_insert_cbret,
                                                 &pri_cbret))
        return false;
    dr_register_filter_syscall_event(drmgr_event_filter_syscall);
    cls_initialized = 1;
    return true;
}

static void
drmgr_cls_exit(void)
{
    if (cls_initialized > 0)
        dr_unregister_filter_syscall_event(drmgr_event_filter_syscall);
}

#else
static bool
drmgr_cls_presys_event(void *drcontext, int sysnum)
{
    return true;
}
#endif /* WINDOWS */

DR_EXPORT
int
drmgr_register_cls_field(void (*cb_init_func)(void *drcontext, bool new_depth),
                         void (*cb_exit_func)(void *drcontext, bool thread_exit))
{
#ifdef WINDOWS
    if (!drmgr_cls_init())
        return -1;
#endif
    if (cb_init_func == NULL || cb_exit_func == NULL)
        return -1;
    if (!drmgr_generic_event_add(&cblist_cls_init, cls_event_lock,
                                 (void (*)(void)) cb_init_func, NULL))
        return -1;
    if (!drmgr_generic_event_add(&cblist_cls_exit, cls_event_lock,
                                 (void (*)(void)) cb_exit_func, NULL))
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
                                          (void (*)(void)) cb_init_func);
    res = drmgr_generic_event_remove(&cblist_cls_exit, cls_event_lock,
                                     (void (*)(void)) cb_exit_func) && res;
    res = drmgr_unreserve_tls_cls_field(cls_taken, idx) && res;
    return res;
}

DR_EXPORT
void *
drmgr_get_cls_field(void *drcontext, int idx)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return NULL;
    return tls->cls[idx];
}

DR_EXPORT
bool
drmgr_set_cls_field(void *drcontext, int idx, void *value)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return false;
    tls->cls[idx] = value;
    return true;
}

DR_EXPORT
void *
drmgr_get_parent_cls_field(void *drcontext, int idx)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return NULL;
    if (tls->prev != NULL)
        return tls->prev->cls[idx];
    return NULL;
}

DR_EXPORT
bool
drmgr_insert_read_cls_field(void *drcontext, int idx,
                            instrlist_t *ilist, instr_t *where, reg_id_t reg)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, reg);
    instrlist_meta_preinsert(ilist, where, XINST_CREATE_load
                             (drcontext, opnd_create_reg(reg),
                              OPND_CREATE_MEMPTR(reg, offsetof(tls_array_t, cls) +
                                                 idx*sizeof(void*))));
    return true;
}

DR_EXPORT
bool
drmgr_insert_write_cls_field(void *drcontext, int idx,
                             instrlist_t *ilist, instr_t *where, reg_id_t reg,
                             reg_id_t scratch)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    if (idx < 0 || idx > MAX_NUM_TLS || !cls_taken[idx] || tls == NULL)
        return false;
    if (!reg_is_gpr(reg) || !reg_is_pointer_sized(reg) ||
        !reg_is_gpr(scratch) || !reg_is_pointer_sized(scratch))
        return false;
    dr_insert_read_tls_field(drcontext, ilist, where, scratch);
    instrlist_meta_preinsert(ilist, where, XINST_CREATE_store
                             (drcontext,
                              OPND_CREATE_MEMPTR(scratch, offsetof(tls_array_t, cls) +
                                                 idx*sizeof(void*)),
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
 * INSTRUCTION NOTE FIELD
 */

enum {
    /* if drmgr itself needed note values we'd put them here */
    DRMGR_NOTE_FIRST_FREE = DRMGR_NOTE_NONE + 1,
};

static ptr_uint_t note_next = DRMGR_NOTE_FIRST_FREE;

/* un-reserving is not supported (would require interval tree to impl) */
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
