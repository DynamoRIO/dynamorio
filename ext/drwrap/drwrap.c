/* **********************************************************
 * Copyright (c) 2010-2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* drwrap: DynamoRIO Function Wrapping and Replacing Extension
 * Derived from Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; 
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* DynamoRIO Function Wrapping and Replacing Extension
 *
 * Handles tailcalls made via direct jump.
 *
 * XXX: does not handle tailcalls made via indirect jump that are not
 * via a simple address table: so if the containing call and the
 * indirect tailcall target are both wrapped, the indirect post cb
 * will be missed.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "hashtable.h"
#include "drvector.h"
#include <string.h>

/* currently using asserts on internal logic sanity checks (never on
 * input from user)
 */
#ifdef DEBUG
# define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#else
# define ASSERT(x, msg) /* nothing */
#endif

/***************************************************************************
 * REQUEST TRACKING
 */

/* There can only be one replacement for any target so we store just app_pc */
#define REPLACE_TABLE_HASH_BITS 6
static hashtable_t replace_table;

/* For each target wrap address, we store a list of wrap requests */
typedef struct _wrap_entry_t {
    app_pc func;
    void (*pre_cb)(void *, OUT void **);
    void (*post_cb)(void *, void *);
    /* To support delayed removal.  We don't set pre_cb and post_cb
     * to NULL instead b/c we want to support re-wrapping.
     */
    bool enabled;
    struct _wrap_entry_t *next;
} wrap_entry_t;

#define WRAP_TABLE_HASH_BITS 6
static hashtable_t wrap_table;
/* We need recursive locking on the table to support drwrap_unwrap
 * being called from a post event so we use this lock instead of
 * hashtable_lock(&wrap_table)
 */
static void *wrap_lock;

static void
wrap_entry_free(void *v)
{
    wrap_entry_t *e = (wrap_entry_t *) v;
    wrap_entry_t *tmp;
    ASSERT(e != NULL, "invalid hashtable deletion");
    while (e != NULL) {
        tmp = e;
        e = e->next;
        dr_global_free(tmp, sizeof(*tmp));
    }
}

/* TLS.  OK to be callback-shared: just more nesting. */
static int tls_idx;

/* We could dynamically allocate: for now assuming no truly recursive func */
#define MAX_WRAP_NESTING 64

/* When a wrapping is disabled, we lazily flush, b/c it's less costly to
 * execute the already-instrumented pre and post points than to do a flush.
 * Only after enough executions do we decide the flush is worthwhile.
 */
#define DISABLED_COUNT_FLUSH_THRESHOLD 1024
/* Lazy removal and flushing.  Protected by wrap_lock. */
static uint disabled_count;

typedef struct _per_thread_t {
    int wrap_level;
    /* record which wrap routine */
    app_pc last_wrap_func[MAX_WRAP_NESTING];
    /* record app esp to handle tailcalls, etc. */
    reg_t app_esp[MAX_WRAP_NESTING];
    /* user_data for passing between pre and post cbs */
    size_t user_data_count[MAX_WRAP_NESTING];
    void **user_data[MAX_WRAP_NESTING];
    void **user_data_pre_cb[MAX_WRAP_NESTING];
    void **user_data_post_cb[MAX_WRAP_NESTING];
    /* whether to skip */
    bool skip[MAX_WRAP_NESTING];
} per_thread_t;

/***************************************************************************
 * WRAPPING INSTRUMENTATION TRACKING
 */

/* We need to know whether we've inserted instrumentation at the call site .
 * The separate post_call_table tells us whether we've set up the return site
 * for instrumentation. 
 */
#define CALL_SITE_TABLE_HASH_BITS 10
static hashtable_t call_site_table;

/* Hashtable so we can remember post-call pcs (since
 * post-cti-instrumentation is not supported by DR).
 * Synchronized externally to safeguard the externally-allocated payload.
 */
#define POST_CALL_TABLE_HASH_BITS 10
static hashtable_t post_call_table;

typedef struct _post_call_entry_t {
    /* PR 454616: we need two flags in the post_call_table: one that
     * says "please add instru for this callee" and one saying "all
     * existing fragments have instru"
     */
    bool existing_instrumented;
} post_call_entry_t;

static void
post_call_entry_free(void *v)
{
    post_call_entry_t *e = (post_call_entry_t *) v;
    ASSERT(e != NULL, "invalid hashtable deletion");
    dr_global_free(e, sizeof(*e));
}


/***************************************************************************
 * WRAPPING CONTEXT
 */

/* An opaque pointer we pass to callbacks and the user passes back for queries */
typedef struct _drwrap_context_t {
    app_pc func;
    dr_mcontext_t *mc;
    app_pc retaddr;
    bool mc_modified;
} drwrap_context_t;

static void
drwrap_context_init(drwrap_context_t *wrapcxt, app_pc func, dr_mcontext_t *mc,
                    app_pc retaddr)
{
    wrapcxt->func = func;
    wrapcxt->mc = mc;
    wrapcxt->retaddr = retaddr;
    wrapcxt->mc_modified = false;
}

DR_EXPORT
app_pc
drwrap_get_func(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return wrapcxt->func;
}

DR_EXPORT
app_pc
drwrap_get_retaddr(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return wrapcxt->retaddr;
}

DR_EXPORT
dr_mcontext_t *
drwrap_get_mcontext(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return wrapcxt->mc;
}

DR_EXPORT
bool
drwrap_set_mcontext(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL)
        return false;
    wrapcxt->mc_modified = true;
    return true;
}

static reg_t *
drwrap_arg_addr(drwrap_context_t *wrapcxt, int arg)
{
    if (wrapcxt == NULL || wrapcxt->mc == NULL)
        return NULL;
#ifdef X64
# ifdef LINUX
    switch (arg) {
    case 0: return &wrapcxt->mc->rdi;
    case 1: return &wrapcxt->mc->rsi;
    case 2: return &wrapcxt->mc->rdx;
    case 3: return &wrapcxt->mc->rcx;
    case 4: return &wrapcxt->mc->r8;
    case 5: return &wrapcxt->mc->r9;
    default: return (reg_t *)(wrapcxt->mc->xsp + (arg - 6 + 1/*retaddr*/)*sizeof(reg_t));
    }
# else
    switch (arg) {
    case 0: return &wrapcxt->mc->rcx;
    case 1: return &wrapcxt->mc->rdx;
    case 2: return &wrapcxt->mc->r8;
    case 3: return &wrapcxt->mc->r9;
    default: return (reg_t *)(wrapcxt->mc->xsp + (arg + 1/*retaddr*/)*sizeof(reg_t));
    }
# endif
#else
    return (reg_t *)(wrapcxt->mc->xsp + (arg + 1/*retaddr*/)*sizeof(reg_t));
#endif
}

DR_EXPORT
void *
drwrap_get_arg(void *wrapcxt_opaque, int arg)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    reg_t *addr = drwrap_arg_addr(wrapcxt, arg);
    if (addr == NULL)
        return NULL;
    else
        return (void *) *addr;
}

DR_EXPORT
bool
drwrap_set_arg(void *wrapcxt_opaque, int arg, void *val)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    reg_t *addr = drwrap_arg_addr(wrapcxt, arg);
    if (addr == NULL)
        return false;
    else {
        wrapcxt->mc_modified = true;
        *addr = (reg_t) val;
        return true;
    }
}

DR_EXPORT
void *
drwrap_get_retval(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL || wrapcxt->mc == NULL)
        return NULL;
    return (void *) wrapcxt->mc->xax;
}

DR_EXPORT
bool
drwrap_set_retval(void *wrapcxt_opaque, void *val)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL || wrapcxt->mc == NULL)
        return false;
    wrapcxt->mc->xax = (reg_t) val;
    wrapcxt->mc_modified = true;
    return true;
}

DR_EXPORT
bool
drwrap_skip_call(void *wrapcxt_opaque, void *retval, size_t stdcall_args_size)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    drwrap_context_t *wrapcxt = (drwrap_context_t *) wrapcxt_opaque;
    if (wrapcxt == NULL || wrapcxt->mc == NULL || wrapcxt->retaddr == NULL)
        return false;
    if (!drwrap_set_retval(wrapcxt_opaque, retval))
        return false;
    wrapcxt->mc->xsp += stdcall_args_size + sizeof(void*)/*retaddr*/;
    wrapcxt->mc->xip = wrapcxt->retaddr;
    /* we can't redirect here b/c we need to release locks */
    pt->skip[pt->wrap_level] = true;
    return true;
}


/***************************************************************************
 * FORWARD DECLS
 */

static void
drwrap_thread_init(void *drcontext);

static void
drwrap_thread_exit(void *drcontext);

static dr_emit_flags_t
drwrap_event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating);

static dr_emit_flags_t
drwrap_event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                         bool for_trace, bool translating, OUT void **user_data);

static dr_emit_flags_t
drwrap_event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                       bool for_trace, bool translating, void *user_data);

static void
drwrap_event_module_unload(void *drcontext, const module_data_t *info);

static void
drwrap_fragment_delete(void *dc/*may be NULL*/, void *tag);


/***************************************************************************
 * INIT
 */

static void *exit_lock;

DR_EXPORT
bool
drwrap_init(void)
{
    drmgr_priority_t priority = {sizeof(priority), "drwrap", NULL, NULL, 0};

    static bool initialized;
    if (initialized)
        return true;
    initialized = true;
    exit_lock = dr_mutex_create();

    drmgr_init();
    if (!drmgr_register_bb_app2app_event(drwrap_event_bb_app2app, &priority))
        return false;
    if (!drmgr_register_bb_instrumentation_event(drwrap_event_bb_analysis,
                                                 drwrap_event_bb_insert,
                                                 &priority))
        return false;

    hashtable_init(&replace_table, REPLACE_TABLE_HASH_BITS,
                   HASH_INTPTR, false/*!strdup*/);
    hashtable_init_ex(&wrap_table, WRAP_TABLE_HASH_BITS, HASH_INTPTR,
                      false/*!str_dup*/, false/*!synch*/, wrap_entry_free,
                      NULL, NULL);
    hashtable_init_ex(&call_site_table, CALL_SITE_TABLE_HASH_BITS, HASH_INTPTR,
                      false/*!strdup*/, false/*!synch*/, NULL, NULL, NULL);
    hashtable_init_ex(&post_call_table, POST_CALL_TABLE_HASH_BITS, HASH_INTPTR,
                      false/*!str_dup*/, false/*!synch*/, post_call_entry_free,
                      NULL, NULL);
    wrap_lock = dr_recurlock_create();
    dr_register_module_unload_event(drwrap_event_module_unload);
    dr_register_delete_event(drwrap_fragment_delete);

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;
    if (!drmgr_register_thread_init_event(drwrap_thread_init))
        return false;
    if (!drmgr_register_thread_exit_event(drwrap_thread_exit))
        return false;
    return true;
}

DR_EXPORT
void
drwrap_exit(void)
{
    static bool exited;
    /* try to handle multiple calls to exit.  still possible to crash
     * trying to lock a destroyed lock.
     */
    if (exited || !dr_mutex_trylock(exit_lock) || exited)
        return;
    exited = true;

    hashtable_delete(&replace_table);
    hashtable_delete(&wrap_table);
    hashtable_delete(&call_site_table);
    hashtable_delete(&post_call_table);
    dr_recurlock_destroy(wrap_lock);
    drmgr_exit();

    dr_mutex_unlock(exit_lock);
    dr_mutex_destroy(exit_lock);
}

static void
drwrap_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    pt->wrap_level = -1;
    drmgr_set_tls_field(drcontext, tls_idx, (void *) pt);
}

static void
drwrap_free_user_data(void *drcontext, per_thread_t *pt, int i)
{
    if (pt->user_data[i] != NULL) {
        dr_thread_free(drcontext, pt->user_data[i],
                       sizeof(void*)*pt->user_data_count[i]);
        pt->user_data[i] = NULL;
    }
    if (pt->user_data_pre_cb[i] != NULL) {
        dr_thread_free(drcontext, pt->user_data_pre_cb[i],
                       sizeof(void*)*pt->user_data_count[i]);
        pt->user_data_pre_cb[i] = NULL;
    }
    if (pt->user_data_post_cb[i] != NULL) {
        dr_thread_free(drcontext, pt->user_data_post_cb[i],
                       sizeof(void*)*pt->user_data_count[i]);
        pt->user_data_post_cb[i] = NULL;
    }
}

static void
drwrap_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    int i;
    for (i = 0; i < MAX_WRAP_NESTING; i++) {
        drwrap_free_user_data(drcontext, pt, i);
    }
    dr_thread_free(drcontext, pt, sizeof(*pt));
}


/***************************************************************************
 * FUNCTION REPLACING
 */

DR_EXPORT
bool
drwrap_replace(app_pc original, app_pc replacement, bool override)
{
    bool res = true;
    bool flush = false;
    if (original == NULL)
        return false;
    if (replacement == NULL) {
        if (!override)
            res = false;
        else {
            flush = true;
            res = hashtable_remove(&replace_table, (void *)original);
        }
    } else {
        if (override) {
            flush = true;
            hashtable_add_replace(&replace_table, (void *)original, (void *)replacement);
        } else
            res = hashtable_add(&replace_table, (void *)original, (void *)replacement);
    }
    /* XXX: we're assuming void* tag == pc
     * XXX: dr_fragment_exists_at only looks at the tag, so with traces
     * we could miss a post-call (if different instr stream, not a post-call)
     */
    if (flush || dr_fragment_exists_at(dr_get_current_drcontext(), original)) {
        /* we do not guarantee faster than a lazy flush */
        if (!dr_unlink_flush_region(original, 1))
            ASSERT(false, "replace update flush failed");
    }
    return res;
}

/* event for function replacing */
static dr_emit_flags_t
drwrap_event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating)
{
    instr_t *inst;
    app_pc pc, replace;
    /* XXX: if we had dr_bbs_cross_ctis() query (i#427) we could just check 1st instr */
    for (inst = instrlist_first(bb);
         inst != NULL;
         inst = instr_get_next(inst)) {
        pc = instr_get_app_pc(inst);
        replace = hashtable_lookup(&replace_table, pc);
        if (replace != NULL) {
            /* remove the rest of the bb and replace w/ jmp to target.
             * with i#427 we'd call instrlist_clear(drcontext, bb)
             */
            instr_t *next = inst, *tmp;
            while (next != NULL) {
                tmp = next;
                next = instr_get_next(next);
                instrlist_remove(bb, tmp);
                instr_destroy(drcontext, tmp);
            }
#ifdef X64
            /* XXX: simple jmp has reachability issues.
             * Jumping through DR memory doesn't work well (meta instrs in app2app,
             * ind jmp mangled w/ i#107).
             * Probably best to add DR API to set exit cti target of bb
             * which is i#429.  For now we clobber xax, which is scratch
             * in most calling conventions.
             */
            instrlist_append(bb, INSTR_XL8(INSTR_CREATE_mov_imm
                                           (drcontext, opnd_create_reg(DR_REG_XAX),
                                            OPND_CREATE_INT64(replace)), pc));
            instrlist_append(bb, INSTR_XL8
                             (INSTR_CREATE_jmp_ind
                              (drcontext, opnd_create_reg(DR_REG_XAX)), pc));
#else
            instrlist_append(bb, INSTR_XL8(INSTR_CREATE_jmp
                                           (drcontext, opnd_create_pc(replace)), pc));
#endif
            break;
        }
    }
    return DR_EMIT_DEFAULT;
}


/***************************************************************************
 * FUNCTION WRAPPING
 */

static void
drwrap_flush_func(app_pc func)
{
    /* we can't flush while holding the lock.
     * we do not guarantee faster than a lazy flush.
     */
    ASSERT(!dr_recurlock_self_owns(wrap_lock), "cannot hold lock while flushing");
    if (!dr_unlink_flush_region(func, 1))
        ASSERT(false, "wrap update flush failed");
}

static app_pc
get_retaddr_at_entry(dr_mcontext_t *mc)
{
    app_pc retaddr = NULL;
    size_t read;
    if (!dr_safe_read((void *)mc->xsp, sizeof(retaddr), &retaddr, &read) ||
        read != sizeof(retaddr)) {
        ASSERT(false, "error reading retaddr at func entry");
        return NULL;
    }
    return retaddr;
}

/* may not return */
static void
drwrap_mark_retaddr_for_instru(void *drcontext, app_pc pc, drwrap_context_t *wrapcxt,
                               bool enabled)
{
    post_call_entry_t *e;
    app_pc retaddr = wrapcxt->retaddr;
    /* We will come here again after the flush-redirect.
     * FIXME: should we try to flush the call instr itself: don't
     * know size though but can be pretty sure.
     */
    /* Ensure we have the retaddr instrumented for post-call events */
    hashtable_lock(&post_call_table);
    e = (post_call_entry_t *) hashtable_lookup(&post_call_table, (void*)retaddr);
    /* PR 454616: we may have added an entry and started a flush
     * but not finished the flush, so we check not just the entry
     * but also the existing_instrumented flag.
     */
    if (e == NULL || !e->existing_instrumented) {
        if (e == NULL) {
            e = (post_call_entry_t *) dr_global_alloc(sizeof(*e));
            e->existing_instrumented = false;
            hashtable_add(&post_call_table, (void*)retaddr, (void*)e);
        }
        /* now that we have an entry in the synchronized post_call_table
         * any new code coming in will be instrumented
         */
        /* XXX: we're assuming void* tag == pc */
        if (dr_fragment_exists_at(drcontext, (void *)retaddr)) {
            /* I'd use dr_unlink_flush_region but it requires -enable_full_api */
            /* unlock for the flush */
            hashtable_unlock(&post_call_table);
            if (!enabled) {
                /* We have to continue to instrument post-wrap points
                 * to avoid unbalanced pre vs post hooks, but these flushes
                 * are expensive so let's get rid of the disabled wraps.
                 */
                dr_recurlock_lock(wrap_lock);
                disabled_count = DISABLED_COUNT_FLUSH_THRESHOLD + 1;
                dr_recurlock_unlock(wrap_lock);
            }
            /* XXX: have a STATS mechanism to count flushes and add call
             * site analysis if too many flushes
             */
            dr_flush_region(retaddr, 1);
            /* now we are guaranteed no thread is inside the fragment */
            /* another thread may have done a racy competing flush: should be fine */
            hashtable_lock(&post_call_table);
            e = (post_call_entry_t *)
                hashtable_lookup(&post_call_table, (void*)retaddr);
            if (e != NULL) /* selfmod could disappear once have PR 408529 */
                e->existing_instrumented = true;
            hashtable_unlock(&post_call_table);
            /* Since the flush may remove the fragment we're already in,
             * we have to redirect execution to the callee again.
             */
            wrapcxt->mc->xip = pc;
            dr_redirect_execution(wrapcxt->mc);
            ASSERT(false, "dr_redirect_execution should not return");
        }
        e->existing_instrumented = true;
    }
    hashtable_unlock(&post_call_table);
}

/* called via clean call at the top of callee */
static void
drwrap_in_callee(app_pc pc)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    wrap_entry_t *wrap, *e;
    dr_mcontext_t mc = {sizeof(mc),};
    uint idx;
    drwrap_context_t wrapcxt;
    ASSERT(pc != NULL, "drwrap_in_callee: pc is NULL!");
    ASSERT(pt != NULL, "drwrap_in_callee: pt is NULL!");

    dr_get_mcontext(drcontext, &mc);
    drwrap_context_init(&wrapcxt, pc, &mc, get_retaddr_at_entry(&mc));

    dr_recurlock_lock(wrap_lock);

    /* ensure we have post-call instru */
    wrap = hashtable_lookup(&wrap_table, (void *)pc);
    if (wrap != NULL) {
        if (wrapcxt.retaddr != NULL) {
            hashtable_lock(&post_call_table);
            if (hashtable_lookup(&post_call_table, (void*)wrapcxt.retaddr) == NULL) {
                bool enabled = wrap->enabled;
                /* this function may not return: but in that case it will redirect
                 * and we'll come back here to do the wrapping.
                 * release all locks.
                 */
                hashtable_unlock(&post_call_table);
                dr_recurlock_unlock(wrap_lock);
                drwrap_mark_retaddr_for_instru(drcontext, pc, &wrapcxt, enabled);
                /* if we come back, re-lookup */
                dr_recurlock_lock(wrap_lock);
                wrap = hashtable_lookup(&wrap_table, (void *)pc);
            } else
                hashtable_unlock(&post_call_table);
        }
    }

    pt->wrap_level++;
    ASSERT(pt->wrap_level >= 0, "wrapping level corrupted");
    ASSERT(pt->wrap_level < MAX_WRAP_NESTING, "max wrapped nesting reached");
    if (pt->wrap_level >= MAX_WRAP_NESTING) {
        dr_recurlock_unlock(wrap_lock);
        return; /* we'll have to skip stuff */
    }
    pt->last_wrap_func[pt->wrap_level] = pc;
    pt->app_esp[pt->wrap_level] = mc.xsp;
#ifdef DEBUG
    for (idx = 0; idx < (uint) pt->wrap_level; idx++) {
        ASSERT(pt->app_esp[idx] >= pt->app_esp[pt->wrap_level],
               "stack pointer off: may miss post-wrap points");
    }
#endif
    
    /* because the list could change between pre and post events we count
     * and store here instead of maintaining count in wrap_table
     */
    for (idx = 0, e = wrap; e != NULL; idx++, e = e->next)
        ; /* nothing */
    /* if we skipped the postcall we didn't free prior data yet */
    drwrap_free_user_data(drcontext, pt, pt->wrap_level);
    pt->user_data_count[pt->wrap_level] = idx;
    pt->user_data[pt->wrap_level] = dr_thread_alloc(drcontext, sizeof(void*)*idx);
    /* we have to keep both b/c we allow one to be null (i#562) */
    pt->user_data_pre_cb[pt->wrap_level] = dr_thread_alloc(drcontext, sizeof(void*)*idx);
    pt->user_data_post_cb[pt->wrap_level] = dr_thread_alloc(drcontext, sizeof(void*)*idx);

    for (idx = 0; wrap != NULL; idx++, wrap = wrap->next) {
        /* if the list does change try to match up in post */
        pt->user_data_pre_cb[pt->wrap_level][idx] = (void *) wrap->pre_cb;
        pt->user_data_post_cb[pt->wrap_level][idx] = (void *) wrap->post_cb;
        if (!wrap->enabled) {
            disabled_count++;
            continue;
        }
        if (wrap->pre_cb != NULL)
            (*wrap->pre_cb)(&wrapcxt, &pt->user_data[pt->wrap_level][idx]);
        /* was there a request to skip? */
        if (pt->skip[pt->wrap_level])
            break;
    } 
    dr_recurlock_unlock(wrap_lock);
    if (pt->skip[pt->wrap_level]) {
        /* drwrap_skip_call already adjusted the stack and pc */
        dr_redirect_execution(wrapcxt.mc);
        ASSERT(false, "dr_redirect_execution should not return");
    }
    if (wrapcxt.mc_modified)
        dr_set_mcontext(drcontext, wrapcxt.mc);
}

/* called via clean call at return address(es) of callee */
static void
drwrap_after_callee_func(void *drcontext, dr_mcontext_t *mc,
                         app_pc pc, app_pc retaddr)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    wrap_entry_t *wrap, *next;
    uint idx;
    drwrap_context_t wrapcxt;
    drvector_t toflush = {0,};
    bool do_flush = false;
    ASSERT(pc != NULL, "drwrap_after_callee: pc is NULL!");
    ASSERT(pt != NULL, "drwrap_after_callee: pt is NULL!");

    drwrap_context_init(&wrapcxt, pc, mc, retaddr);

    if (pt->wrap_level >= MAX_WRAP_NESTING) {
        pt->wrap_level--;
        return; /* we skipped the wrap */
    }
    if (pt->skip[pt->wrap_level]) {
        pt->skip[pt->wrap_level] = false;
        pt->wrap_level--;
        return; /* skip the post-func cbs */
    }

    dr_recurlock_lock(wrap_lock);
    wrap = hashtable_lookup(&wrap_table, (void *)pc);
    for (idx = 0; wrap != NULL; idx++, wrap = next) {
        /* handle the list changing between pre and post events */
        uint tmp = idx;
        /* handle drwrap_unwrap being called in post_cb */
        next = wrap->next;
        if (!wrap->enabled) {
            disabled_count++;
            continue;
        }
        /* we may have to skip some entries */
        for (; idx < pt->user_data_count[pt->wrap_level]; idx++) {
            /* we have to check both b/c we allow one to be null (i#562) */
            if (wrap->pre_cb == pt->user_data_pre_cb[pt->wrap_level][idx] &&
                wrap->post_cb == pt->user_data_post_cb[pt->wrap_level][idx])
                break; /* all set */
        }
        if (idx == pt->user_data_count[pt->wrap_level]) {
            /* we didn't find it, it must be new, so had no pre => skip post
             * (even if only has post, to be consistent w/ timing)
             */
            idx = tmp; /* reset */
        } else if (wrap->post_cb != NULL) {
            (*wrap->post_cb)(&wrapcxt, pt->user_data[pt->wrap_level][idx]);
            /* note that at this point wrap might be deleted */
        }
    } 
    if (disabled_count > DISABLED_COUNT_FLUSH_THRESHOLD) {
        /* Lazy removal and flushing.  To be non-lazy requires storing
         * info inside unwrap and/or limiting when unwrap can be called.
         * Lazy also means a wrap reversing an unwrap doesn't cost anything.
         * More importantly, flushes are expensive, so we batch them up here.
         * We can't flush while holding the lock so we use a local vector.
         */
        uint i;
        drvector_init(&toflush, 10, false/*no synch: wrapcxt-local*/, NULL);
        for (i = 0; i < HASHTABLE_SIZE(wrap_table.table_bits); i++) {
            hash_entry_t *he, *next_he;
            for (he = wrap_table.table[i]; he != NULL; he = next_he) {
                wrap_entry_t *prev = NULL, *next;
                next_he = he->next; /* allow removal */
                for (wrap = (wrap_entry_t *) he->payload; wrap != NULL;
                     prev = wrap, wrap = next) {
                    next = wrap->next;
                    if (!wrap->enabled) {
                        if (prev == NULL) {
                            if (next == NULL) {
                                /* No wrappings left for this function so
                                 * let's flush it
                                 */
                                drvector_append(&toflush, (void *)wrap->func);
                                hashtable_remove(&wrap_table, (void *)wrap->func);
                                wrap = NULL; /* don't double-free */
                            } else {
                                hashtable_add_replace(&wrap_table, (void *)wrap->func,
                                                      (void *)next);
                            }
                        } else
                            prev->next = next;
                        if (wrap != NULL)
                            dr_global_free(wrap, sizeof(*wrap));
                    }
                }
            }
        }
        do_flush = true;
        disabled_count = 0;
    }
    dr_recurlock_unlock(wrap_lock);
    if (wrapcxt.mc_modified)
        dr_set_mcontext(drcontext, wrapcxt.mc);

    if (do_flush) {
        /* handle delayed flushes while holding no lock 
         * XXX: optimization: combine nearby addresses to reduce # flushes
         */
        uint i;
        for (i = 0; i < toflush.entries; i++)
            drwrap_flush_func((app_pc)toflush.array[i]);
        drvector_delete(&toflush);
    }

    drwrap_free_user_data(drcontext, pt, pt->wrap_level);

    ASSERT(pt->wrap_level >= 0, "internal wrapping error");
    pt->wrap_level--;
}

/* called via clean call at return address(es) of callee */
static void
drwrap_after_callee(app_pc retaddr)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    dr_mcontext_t mc = {sizeof(mc),};
    ASSERT(pt != NULL, "drwrap_after_callee: pt is NULL!");
    dr_get_mcontext(drcontext, &mc);

    if (pt->wrap_level < 0) {
        /* jump or other method of targeting post-call site w/o executing
         * call; or, did an indirect call that no longer matches */
        return;
    }

    /* process post for all funcs whose frames we bypassed.
     * we assume they were all bypassed b/c of tailcalls and that their
     * posts should be called (on an exception we clear out our data
     * and won't come here; for longjmp we assume we want to call the post
     * although the retval won't be there...XXX).
     *
     * we no longer store the callee for a post-call site b/c there
     * can be multiple and it's complex to control which one is used
     * (outer or inner or middle) consistently.  we don't need the
     * callee to distinguish a jump or other transfer to a post-call
     * site where the transfer happens inside a wrapped routine (passing
     * the wrap_level==0 check above) b/c our stack
     * check will identify whether we've left any wrapped routines we
     * entered.
     */
    while (pt->wrap_level >= 0 && pt->app_esp[pt->wrap_level] < mc.xsp) {
        drwrap_after_callee_func(drcontext, &mc,
                                 pt->last_wrap_func[pt->wrap_level], retaddr);
    }
}

static dr_emit_flags_t
drwrap_event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                         bool for_trace, bool translating, OUT void **user_data)
{
    /* nothing to do */
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
drwrap_event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                       bool for_trace, bool translating, void *user_data)
{
    /* XXX: if we had dr_bbs_cross_ctis() query (i#427) we could just check 1st instr */
    wrap_entry_t *wrap;
    post_call_entry_t *post;
    app_pc pc = instr_get_app_pc(inst);

    /* Strategy: we don't bother to look at call sites; we wait for the callee
     * and flush, under the assumption that we won't have already seen the
     * return point and so won't have to incur the cost of a flush very often
     */
    dr_recurlock_lock(wrap_lock);
    wrap = hashtable_lookup(&wrap_table, (void *)pc);
    if (wrap != NULL) {
        dr_insert_clean_call(drcontext, bb, inst, (void *)drwrap_in_callee,
                             false, 1, OPND_CREATE_INTPTR((ptr_int_t)pc));
    }
    dr_recurlock_unlock(wrap_lock);

    hashtable_lock(&post_call_table);
    post = (post_call_entry_t *) hashtable_lookup(&post_call_table, (void*)pc);
    if (post != NULL) {
        post->existing_instrumented = true;
        dr_insert_clean_call(drcontext, bb, inst, (void *)drwrap_after_callee,
                             false, 1,
                             OPND_CREATE_INTPTR((ptr_int_t)pc));
    }
    hashtable_unlock(&post_call_table);

    return DR_EMIT_DEFAULT;
}

static void
drwrap_event_module_unload(void *drcontext, const module_data_t *info)
{
    /* XXX: should also remove from post_call_table and call_site_table
     * on other code modifications: for now we assume no such
     * changes to app code that's being targeted for wrapping.
     */
    hashtable_remove_range(&call_site_table, (void *)info->start, (void *)info->end);
    hashtable_remove_range(&post_call_table, (void *)info->start, (void *)info->end);
}

static void
drwrap_fragment_delete(void *dc/*may be NULL*/, void *tag)
{
    /* For post_call_table, just like in our use of dr_fragment_exists_at, we
     * assume no traces and that we only care about fragments starting there.
     *
     * XXX: if we had DRi#409 we could avoid doing this removal on
     * non-cache-consistency deletion: though usually if we remove on our
     * own flush we should re-mark as post-call w/o another flush since in
     * most cases the post-call is reached via the call.
     *
     * An alternative would be to not remove here, to store the
     * pre-call bytes, and to check on new bbs whether they changed.
     */
    hashtable_lock(&post_call_table);
    hashtable_remove(&post_call_table, tag);
    hashtable_unlock(&post_call_table);
}

DR_EXPORT
bool
drwrap_wrap(app_pc func,
            void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
            void (*post_func_cb)(void *wrapcxt, void *user_data))
{
    wrap_entry_t *wrap_cur, *wrap_new;

    /* allow one side to be NULL (i#562) */
    if (func == NULL || (pre_func_cb == NULL && post_func_cb == NULL))
        return false;

    wrap_new = dr_global_alloc(sizeof(*wrap_new));
    wrap_new->func = func;
    wrap_new->pre_cb = pre_func_cb;
    wrap_new->post_cb = post_func_cb;
    wrap_new->enabled = true;

    dr_recurlock_lock(wrap_lock);
    wrap_cur = hashtable_lookup(&wrap_table, (void *)func);
    if (wrap_cur != NULL) {
        /* we add in reverse order (documented in interface) */
        wrap_entry_t *e;
        /* things will break down w/ duplicate cbs */
        for (e = wrap_cur; e != NULL; e = e->next) {
            if ((e->pre_cb != NULL && e->pre_cb == pre_func_cb) ||
                (e->post_cb != NULL && e->post_cb == post_func_cb)) {
                /* matches existing request: re-enable if necessary */
                e->enabled = true;
                dr_global_free(wrap_new, sizeof(*wrap_new));
                dr_recurlock_unlock(wrap_lock);
                return false;
            }
        }
        wrap_new->next = wrap_cur;
        hashtable_add_replace(&wrap_table, (void *)func, (void *)wrap_new);
    } else {
        wrap_new->next = NULL;
        hashtable_add(&wrap_table, (void *)func, (void *)wrap_new);
        /* XXX: we're assuming void* tag == pc */
        if (dr_fragment_exists_at(dr_get_current_drcontext(), func)) {
            /* we do not guarantee faster than a lazy flush */
            if (!dr_unlink_flush_region(func, 1))
                ASSERT(false, "wrap update flush failed");
        }
    }
    dr_recurlock_unlock(wrap_lock);
    return true;
}

DR_EXPORT
bool
drwrap_unwrap(app_pc func,
              void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
              void (*post_func_cb)(void *wrapcxt, void *user_data))
{
    wrap_entry_t *wrap;
    bool res = false;

    if (func == NULL ||
        (pre_func_cb == NULL && post_func_cb == NULL))
        return false;

    dr_recurlock_lock(wrap_lock);
    wrap = hashtable_lookup(&wrap_table, (void *)func);
    for (; wrap != NULL; wrap = wrap->next) {
        if (wrap->pre_cb == pre_func_cb &&
            wrap->post_cb == post_func_cb) {
            /* We use lazy removal and flushing to avoid complications of
             * removing and flushing from a post-wrap callback (it's currently
             * iterating, and it holds a lock).
             */
            wrap->enabled = false;
            res = true;
            break;
        }
    }
    dr_recurlock_unlock(wrap_lock);
    return res;
}
