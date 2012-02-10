/* **********************************************************
 * Copyright (c) 2010-2012 Google, Inc.   All rights reserved.
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
#ifdef LINUX
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

/* bb event list entry */
typedef struct _cb_entry_t {
    const char *name;
    int priority;
    union {
        drmgr_xform_cb_t xform_cb;
        struct {
            drmgr_analysis_cb_t analysis_cb;
            drmgr_insertion_cb_t insertion_cb;
        } pair;
    } cb;
    struct _cb_entry_t *next;
} cb_entry_t;

/* generic event list entry */
typedef struct _generic_event_entry_t {
    union {
        void (*generic_cb)(void);
        void (*thread_cb)(void *);
        void (*cls_cb)(void *, bool);
        bool (*presys_cb)(void *, int);
    } cb;
    struct _generic_event_entry_t *next;
} generic_event_entry_t;

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
static cb_entry_t *cblist_app2app;
static cb_entry_t *cblist_instrumentation;
static cb_entry_t *cblist_instru2instru;

/* Length of cblist_instrumentation; protected by bb_cb_lock */
static uint instrulist_count;

static dr_emit_flags_t
drmgr_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating);

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

static void *exit_lock;

/* Thread event cbs and rwlock */
static generic_event_entry_t *cblist_thread_init;
static generic_event_entry_t *cblist_thread_exit;
static void *thread_event_lock;

static generic_event_entry_t *cblist_cls_init;
static generic_event_entry_t *cblist_cls_exit;
static void *cls_event_lock;

/* Yet another event we must wrap to ensure we go last */
static generic_event_entry_t *cblist_presys;
static void *presys_event_lock;

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
drmgr_event_exit(void);

static bool
drmgr_presyscall_event(void *drcontext, int sysnum);

static bool
drmgr_cls_presys_event(void *drcontext, int sysnum);


/***************************************************************************
 * INIT
 */

DR_EXPORT
bool
drmgr_init(void)
{
    static bool initialized;
    if (initialized)
        return true;
    initialized = true;
    exit_lock = dr_mutex_create();

    bb_cb_lock = dr_rwlock_create();
    thread_event_lock = dr_rwlock_create();
    tls_lock = dr_mutex_create();
    cls_event_lock = dr_rwlock_create();
    presys_event_lock = dr_rwlock_create();

    dr_register_thread_init_event(drmgr_thread_init_event);
    dr_register_thread_exit_event(drmgr_thread_exit_event);
    dr_register_pre_syscall_event(drmgr_presyscall_event);

    return true;
}

DR_EXPORT
void
drmgr_exit(void)
{
    static bool exited;
    /* try to handle multiple calls to exit.  still possible to crash
     * trying to lock a destroyed lock.
     */
    if (exited || !dr_mutex_trylock(exit_lock) || exited)
        return;
    exited = true;

    drmgr_bb_exit();
    drmgr_event_exit();

    dr_rwlock_destroy(presys_event_lock);
    dr_rwlock_destroy(cls_event_lock);
    dr_mutex_destroy(tls_lock);
    dr_rwlock_destroy(thread_event_lock);
    dr_rwlock_destroy(bb_cb_lock);

    dr_mutex_unlock(exit_lock);
    dr_mutex_destroy(exit_lock);
}

/***************************************************************************
 * BB EVENTS
 */

static dr_emit_flags_t
drmgr_bb_event(void *drcontext, void *tag, instrlist_t *bb,
               bool for_trace, bool translating)
{
    cb_entry_t *e;
    dr_emit_flags_t res = DR_EMIT_DEFAULT;
    instr_t *inst, *next_inst;
    void **user_data;
    uint i;

    dr_rwlock_read_lock(bb_cb_lock);

    /* Pass 1: app2app */
    for (e = cblist_app2app; e != NULL; e = e->next) {
        res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    /* Pass 2: analysis */
    /* We need per-thread user_data */
    user_data = (void **) dr_thread_alloc(drcontext, sizeof(void*)*instrulist_count);
    for (i = 0, e = cblist_instrumentation; e != NULL; i++, e = e->next) {
        res |= (*e->cb.pair.analysis_cb)
            (drcontext, tag, bb, for_trace, translating, &user_data[i]);
        /* XXX: add checks that cb followed the rules */
    }

    /* Pass 3: instru, per instr */
    for (inst = instrlist_first(bb); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);
        for (i = 0, e = cblist_instrumentation; e != NULL; i++, e = e->next) {
            res |= (*e->cb.pair.insertion_cb)
                (drcontext, tag, bb, inst, for_trace, translating, user_data[i]);
            /* XXX: add checks that cb followed the rules */
        }
    }
    dr_thread_free(drcontext, user_data, sizeof(void*)*instrulist_count);

    /* Pass 4: final */
    for (e = cblist_instru2instru; e != NULL; e = e->next) {
        res |= (*e->cb.xform_cb)(drcontext, tag, bb, for_trace, translating);
    }

    dr_rwlock_read_unlock(bb_cb_lock);

    return res;
}

static bool
drmgr_bb_cb_add(cb_entry_t **list,
                drmgr_xform_cb_t xform_func,
                drmgr_analysis_cb_t analysis_func,
                drmgr_insertion_cb_t insertion_func,
                drmgr_priority_t *priority)
{
    cb_entry_t *e, *prev_e, *new_e;
    bool past_after; /* are we past the "after" constraint */
    bool res = true;
    ASSERT(list != NULL && priority != NULL, "invalid internal params");
    ASSERT((xform_func != NULL && analysis_func == NULL && insertion_func == NULL) ||
           (xform_func == NULL && analysis_func != NULL && insertion_func != NULL),
           "invalid internal params");
    if (priority == NULL || priority->name == NULL)
        return false; /* must have a name */
    /* if we add fields in the future this is where we decide which to use */
    if (priority->struct_size < sizeof(drmgr_priority_t))
        return false; /* incorrect struct */

    new_e = (cb_entry_t *) dr_global_alloc(sizeof(*new_e));
    new_e->name = priority->name;
    new_e->priority = priority->priority;
    if (xform_func != NULL)
        new_e->cb.xform_cb = xform_func;
    else {
        new_e->cb.pair.analysis_cb = analysis_func;
        new_e->cb.pair.insertion_cb = insertion_func;
    }

    dr_rwlock_write_lock(bb_cb_lock);

    /* check for duplicate names.
     * not expecting a very long list, so simpler to do full walk
     * here rather than more complex logic to fold into walk below.
     */
    for (prev_e = NULL, e = *list; e != NULL; prev_e = e, e = e->next) {
        if (strcmp(priority->name, e->name) == 0) {
            dr_global_free(new_e, sizeof(*new_e));
            dr_rwlock_write_unlock(bb_cb_lock);
            return false; /* duplicate name */
        }
    }

    /* keep the list sorted */
    past_after = (priority->after == NULL);
    for (prev_e = NULL, e = *list; e != NULL; prev_e = e, e = e->next) {
        /* Priority 1: must be before "before" */
        if (priority->before != NULL && strcmp(priority->before, e->name) == 0)
            break;
        /* Priority 2: must be after "after" */
        else if (!past_after) {
            ASSERT(priority->after != NULL, "past_after should be true");
            if (strcmp(priority->after, e->name) == 0)
                past_after = true;
        }
        /* Priority 3: numeric priority.  Tie goes to 1st to register. */
        else if (e->priority > priority->priority)
            break;
    }
    if (past_after) {
        if (bb_event_count == 0)
            dr_register_bb_event(drmgr_bb_event);
        bb_event_count++;

        if (prev_e == NULL)
            *list = new_e;
        else
            prev_e->next = new_e;
        new_e->next = e;

        if (xform_func == NULL)
            instrulist_count++;
    } else {
        /* cannot satisfy both the before and after requests */
        res = false;
        dr_global_free(e, sizeof(*e));
    }

    dr_rwlock_write_unlock(bb_cb_lock);
    return res;
}

DR_EXPORT
bool
drmgr_register_bb_app2app_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL || priority == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_app2app, func, NULL, NULL, priority);
}

DR_EXPORT
bool
drmgr_register_bb_instrumentation_event(drmgr_analysis_cb_t analysis_func,
                                        drmgr_insertion_cb_t insertion_func,
                                        drmgr_priority_t *priority)
{
    if (analysis_func == NULL || insertion_func == NULL || priority == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_instrumentation, NULL, analysis_func,
                           insertion_func, priority);
}

DR_EXPORT
bool
drmgr_register_bb_instru2instru_event(drmgr_xform_cb_t func, drmgr_priority_t *priority)
{
    if (func == NULL || priority == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_add(&cblist_instru2instru, func, NULL, NULL, priority);
}

static bool
drmgr_bb_cb_remove(cb_entry_t **list,
                   drmgr_xform_cb_t xform_func,
                   drmgr_analysis_cb_t analysis_func)
{
    bool res = false;
    cb_entry_t *e, *prev_e;
    ASSERT(list != NULL, "invalid internal params");
    ASSERT((xform_func != NULL && analysis_func == NULL) ||
           (xform_func == NULL && analysis_func != NULL), "invalid internal params");

    dr_rwlock_write_lock(bb_cb_lock);

    for (prev_e = NULL, e = *list; e != NULL; prev_e = e, e = e->next) {
        if ((xform_func != NULL && xform_func == e->cb.xform_cb) ||
            (analysis_func != NULL && analysis_func == e->cb.pair.analysis_cb))
            break;
    }
    if (e != NULL) {
        res = true;
        if (prev_e == NULL)
            *list = e->next;
        else
            prev_e->next = e->next;
        dr_global_free(e, sizeof(*e));

        if (xform_func == NULL)
            instrulist_count--;

        bb_event_count--;
        if (bb_event_count == 0)
            dr_unregister_bb_event(drmgr_bb_event);
    }

    dr_rwlock_write_unlock(bb_cb_lock);
    return res;
}

static void
drmgr_bb_cb_exit(cb_entry_t *list)
{
    cb_entry_t *e, *next_e;
    dr_rwlock_write_lock(bb_cb_lock);
    for (e = list; e != NULL; e = next_e) {
        next_e = e->next;
        dr_global_free(e, sizeof(*e));
    }
    dr_rwlock_write_unlock(bb_cb_lock);
}

static void
drmgr_bb_exit(void)
{
    drmgr_bb_cb_exit(cblist_app2app);
    drmgr_bb_cb_exit(cblist_instrumentation);
    drmgr_bb_cb_exit(cblist_instru2instru);
}

DR_EXPORT
bool
drmgr_unregister_bb_app2app_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_app2app, func, NULL);
}

DR_EXPORT
bool
drmgr_unregister_bb_instrumentation_event(drmgr_analysis_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instrumentation, NULL, func);
}

DR_EXPORT
bool
drmgr_unregister_bb_instru2instru_event(drmgr_xform_cb_t func)
{
    if (func == NULL)
        return false; /* invalid params */
    return drmgr_bb_cb_remove(&cblist_instru2instru, func, NULL);
}


/***************************************************************************
 * WRAPPED EVENTS
 */

/* We must go first on thread init and last on thread exit, and DR
 * doesn't provide any priority scheme to guarantee that, so we must
 * wrap the thread events.
 */

static bool
drmgr_generic_event_add(generic_event_entry_t **list,
                        void *rwlock,
                        void (*func)(void))
{
    generic_event_entry_t *e;
    if (func == NULL)
        return false;
    dr_rwlock_write_lock(rwlock);
    e = (generic_event_entry_t *) dr_global_alloc(sizeof(*e));
    e->cb.generic_cb = func;
    e->next = *list;
    *list = e;
    dr_rwlock_write_unlock(rwlock);
    return true;
}

static bool
drmgr_generic_event_remove(generic_event_entry_t **list,
                           void *rwlock,
                           void (*func)(void))
{
    bool res = false;
    generic_event_entry_t *e, *prev_e;
    if (func == NULL)
        return false;
    dr_rwlock_write_lock(rwlock);
    for (prev_e = NULL, e = *list; e != NULL; prev_e = e, e = e->next) {
        if (e->cb.generic_cb == func) {
            if (prev_e == NULL)
                *list = e->next;
            else
                prev_e->next = e->next;
            dr_global_free(e, sizeof(*e));
            res = true;
            break;
        }
    }
    dr_rwlock_write_unlock(rwlock);
    return res;
}

static void
drmgr_generic_event_exit(generic_event_entry_t *list, void *rwlock)
{
    generic_event_entry_t *e, *next_e;
    dr_rwlock_write_lock(rwlock);
    for (e = list; e != NULL; e = next_e) {
        next_e = e->next;
        dr_global_free(e, sizeof(*e));
    }
    dr_rwlock_write_unlock(rwlock);
}

static void
drmgr_event_exit(void)
{
    drmgr_generic_event_exit(cblist_thread_init, thread_event_lock);
    drmgr_generic_event_exit(cblist_thread_exit, thread_event_lock);
    drmgr_generic_event_exit(cblist_cls_init, cls_event_lock);
    drmgr_generic_event_exit(cblist_cls_exit, cls_event_lock);
    drmgr_generic_event_exit(cblist_presys, presys_event_lock);
}

DR_EXPORT
bool
drmgr_register_thread_init_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_add(&cblist_thread_init, thread_event_lock,
                                   (void (*)(void)) func);
}

DR_EXPORT
bool
drmgr_unregister_thread_init_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_remove(&cblist_thread_init, thread_event_lock,
                                      (void (*)(void)) func);
}

DR_EXPORT
bool
drmgr_register_thread_exit_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_add(&cblist_thread_exit, thread_event_lock,
                                   (void (*)(void)) func);
}

DR_EXPORT
bool
drmgr_unregister_thread_exit_event(void (*func)(void *drcontext))
{
    return drmgr_generic_event_remove(&cblist_thread_exit, thread_event_lock,
                                      (void (*)(void)) func);
}

DR_EXPORT
bool
drmgr_register_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return drmgr_generic_event_add(&cblist_presys, presys_event_lock,
                                   (void (*)(void)) func);
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
    generic_event_entry_t *e;
    bool execute = true;
    dr_rwlock_read_lock(presys_event_lock);
    for (e = cblist_presys; e != NULL; e = e->next)
        execute = (*e->cb.presys_cb)(drcontext, sysnum) && execute;
    dr_rwlock_read_unlock(presys_event_lock);

    /* this must go last (the whole reason we're wrapping this) */
    execute = drmgr_cls_presys_event(drcontext, sysnum) && execute;
    return execute;
}

/***************************************************************************
 * TLS
 */

static void
drmgr_thread_init_event(void *drcontext)
{
    generic_event_entry_t *e;
    tls_array_t *tls = dr_thread_alloc(drcontext, sizeof(*tls));
    memset(tls, 0, sizeof(*tls));
    dr_set_tls_field(drcontext, (void *)tls);

    dr_rwlock_read_lock(thread_event_lock);
    for (e = cblist_thread_init; e != NULL; e = e->next)
        (*e->cb.thread_cb)(drcontext);
    dr_rwlock_read_unlock(thread_event_lock);

    drmgr_cls_stack_init(drcontext);
}

static void
drmgr_thread_exit_event(void *drcontext)
{
    generic_event_entry_t *e;

    dr_rwlock_read_lock(thread_event_lock);
    for (e = cblist_thread_exit; e != NULL; e = e->next)
        (*e->cb.thread_cb)(drcontext);
    dr_rwlock_read_unlock(thread_event_lock);

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
    instrlist_meta_preinsert(ilist, where, INSTR_CREATE_mov_ld
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
    instrlist_meta_preinsert(ilist, where, INSTR_CREATE_mov_st
                             (drcontext,
                              OPND_CREATE_MEMPTR(scratch, offsetof(tls_array_t, tls) +
                                                 idx*sizeof(void*)),
                               opnd_create_reg(reg)));
    return true;
}


/***************************************************************************
 * CLS
 */

static bool
drmgr_cls_stack_push_event(void *drcontext, bool new_depth)
{
    generic_event_entry_t *e;
    /* let client initialize cls slots (and allocate new ones if new_depth) */
    dr_rwlock_read_lock(cls_event_lock);
    for (e = cblist_cls_init; e != NULL; e = e->next)
        (*e->cb.cls_cb)(drcontext, new_depth);
    dr_rwlock_read_unlock(cls_event_lock);
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
    void *drcontext = dr_get_current_drcontext();
    tls_array_t *tls_child = (tls_array_t *) dr_get_tls_field(drcontext);
    tls_array_t *tls_parent;
    generic_event_entry_t *e;
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
    for (e = cblist_cls_exit; e != NULL; e = e->next)
        (*e->cb.cls_cb)(drcontext, false/*!thread_exit*/);
    dr_rwlock_read_unlock(cls_event_lock);

    /* update tls w/ any changes while in child context */
    memcpy(tls_parent->tls, tls_child->tls, sizeof(*tls_child->tls)*MAX_NUM_TLS);
    /* swap in as the current structure */
    dr_set_tls_field(drcontext, (void *)tls_parent);

    return true;
}

static bool
drmgr_cls_stack_exit(void *drcontext)
{
    tls_array_t *tls = (tls_array_t *) dr_get_tls_field(drcontext);
    tls_array_t *nxt, *tmp;
    generic_event_entry_t *e;
    if (tls == NULL)
        return false;
    for (nxt = tls; nxt->prev != NULL; nxt = nxt->prev)
        ; /* nothing */
    dr_rwlock_read_lock(cls_event_lock);
    while (nxt != NULL) {
        tmp = nxt;
        nxt = nxt->next;
        /* set the field in case client queries */
        dr_set_tls_field(drcontext, (void *)tmp);
        for (e = cblist_cls_exit; e != NULL; e = e->next)
            (*e->cb.cls_cb)(drcontext, true/*thread_exit*/);
        dr_thread_free(drcontext, tmp, sizeof(*tmp));
    }
    dr_rwlock_read_unlock(cls_event_lock);
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

static dr_emit_flags_t
drmgr_event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating, OUT void **user_data)
{
    /* nothing to do */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drmgr_event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                      bool for_trace, bool translating, void *user_data)
{
    if (instr_get_app_pc(inst) == addr_KiCallback) {
        dr_insert_clean_call(drcontext, bb, inst, (void *)drmgr_cls_stack_push,
                             false, 0);
    }
    if (instr_get_opcode(inst) == OP_int &&
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
static int
drmgr_decode_sysnum_from_wrapper(byte *entry)
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
             opc != OP_sysenter && opc != OP_syscall);
    instr_free(drcontext, &instr);
    return num;
}

static bool
drmgr_cls_init(void)
{
    /* For callback init we watch for KiUserCallbackDispatcher.
     * For callback exit we watch for NtCallbackReturn or int 0x2b.
     */
    static int cls_initialized; /* 0=not tried; >0=success; <0=failure */
    module_data_t *data;
    byte *ntdll_lib, *addr_cbret;
    drmgr_priority_t priority = {sizeof(priority), "drmgr_cls", NULL, NULL, 0};

    if (cls_initialized > 0)
        return true;
    else if (cls_initialized < 0)
        return false;
    cls_initialized = -1;

    if (!drmgr_register_bb_instrumentation_event(drmgr_event_bb_analysis,
                                                 drmgr_event_bb_insert,
                                                 &priority))
        return false;
    dr_register_filter_syscall_event(drmgr_event_filter_syscall);

    data = dr_lookup_module_by_name("ntdll.dll");
    if (data == NULL) {
        /* fatal error: something is really wrong w/ underlying DR */
        return false;
    }
    ntdll_lib = data->start;
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
    cls_initialized = 1;
    return true;
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
                                 (void (*)(void)) cb_init_func))
        return -1;
    if (!drmgr_generic_event_add(&cblist_cls_exit, cls_event_lock,
                                 (void (*)(void)) cb_exit_func))
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
    instrlist_meta_preinsert(ilist, where, INSTR_CREATE_mov_ld
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
    instrlist_meta_preinsert(ilist, where, INSTR_CREATE_mov_st
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
