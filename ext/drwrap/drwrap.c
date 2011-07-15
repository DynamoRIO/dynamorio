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
 * XXX: does not handle tailcalls made via indirect jump: so if the
 * containing call and the indirect tailcall target are both wrapped,
 * the indirect post cb will be missed.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "hashtable.h"
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
    struct _wrap_entry_t *next;
} wrap_entry_t;

#define WRAP_TABLE_HASH_BITS 6
static hashtable_t wrap_table;

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

static bool
is_wrapped_routine(app_pc pc)
{
    bool res;
    hashtable_lock(&wrap_table);
    res = (hashtable_lookup(&wrap_table, (void *)pc) != NULL);
    hashtable_unlock(&wrap_table);
    return res;
}

/* TLS.  OK to be callback-shared: just more nesting. */
static int tls_idx;

/* We could dynamically allocate: for now assuming no truly recursive func */
#define MAX_WRAP_NESTING 64

typedef struct _per_thread_t {
    int wrap_level;
    /* record which wrap routine */
    app_pc last_wrap_func[MAX_WRAP_NESTING];
    /* handle nested tailcalls */
    app_pc tailcall_target[MAX_WRAP_NESTING];
    app_pc tailcall_post_call[MAX_WRAP_NESTING];
    /* user_data for passing between pre and post cbs */
    size_t user_data_count[MAX_WRAP_NESTING];
    void **user_data[MAX_WRAP_NESTING];
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
    app_pc callee;
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
    dr_register_module_unload_event(drwrap_event_module_unload);

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
drwrap_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    int i;
    for (i = 0; i < MAX_WRAP_NESTING; i++) {
        if (pt->user_data[i] != NULL) {
            dr_thread_free(drcontext, pt->user_data[i],
                           sizeof(void*)*pt->user_data_count[i]);
        }
        if (pt->user_data_post_cb[i] != NULL) {
            dr_thread_free(drcontext, pt->user_data_post_cb[i],
                           sizeof(void*)*pt->user_data_count[i]);
        }
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
    /* XXX: we're assuming void* tag == pc */
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
drwrap_mark_retaddr_for_instru(void *drcontext, app_pc pc, drwrap_context_t *wrapcxt)
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
            e->callee = pc;
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
            /* Since the flush will remove the fragment we're already in,
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

    hashtable_lock(&wrap_table);

    /* ensure we have post-call instru */
    wrap = hashtable_lookup(&wrap_table, (void *)pc);
    if (wrap != NULL) {
        if (wrapcxt.retaddr != NULL) {
            hashtable_lock(&post_call_table);
            if (hashtable_lookup(&post_call_table, (void*)wrapcxt.retaddr) == NULL) {
                /* this function may not return: but in that case it will redirect
                 * and we'll come back here to do the wrapping.
                 * release all locks.
                 */
                hashtable_unlock(&post_call_table);
                hashtable_unlock(&wrap_table);
                drwrap_mark_retaddr_for_instru(drcontext, pc, &wrapcxt);
                /* if we come back, re-lookup */
                hashtable_lock(&wrap_table);
                wrap = hashtable_lookup(&wrap_table, (void *)pc);
            } else
                hashtable_unlock(&post_call_table);
        }
    }

    pt->wrap_level++;
    ASSERT(pt->wrap_level < MAX_WRAP_NESTING, "max wrapped nesting reached");
    if (pt->wrap_level >= MAX_WRAP_NESTING) {
        hashtable_unlock(&wrap_table);
        return; /* we'll have to skip stuff */
    }
    pt->last_wrap_func[pt->wrap_level] = pc;
    /* because the list could change between pre and post events we count
     * and store here instead of maintaining count in wrap_table
     */
    for (idx = 0, e = wrap; e != NULL; idx++, e = e->next)
        ; /* nothing */
    pt->user_data_count[pt->wrap_level] = idx;
    pt->user_data[pt->wrap_level] = dr_thread_alloc(drcontext, sizeof(void*)*idx);
    pt->user_data_post_cb[pt->wrap_level] = dr_thread_alloc(drcontext, sizeof(void*)*idx);

    for (idx = 0; wrap != NULL; idx++, wrap = wrap->next) {
        /* if the list does change try to match up in post */
        pt->user_data_post_cb[pt->wrap_level][idx] = (void *) wrap->post_cb;
        (*wrap->pre_cb)(&wrapcxt, &pt->user_data[pt->wrap_level][idx]);
        /* was there a request to skip? */
        if (pt->skip[pt->wrap_level])
            break;
    } 
    hashtable_unlock(&wrap_table);
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
drwrap_after_callee(app_pc pc, app_pc retaddr)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    wrap_entry_t *wrap;
    dr_mcontext_t mc = {sizeof(mc),};
    uint idx;
    drwrap_context_t wrapcxt;
    ASSERT(pc != NULL, "drwrap_after_callee: pc is NULL!");
    ASSERT(pt != NULL, "drwrap_after_callee: pt is NULL!");

    dr_get_mcontext(drcontext, &mc);
    drwrap_context_init(&wrapcxt, pc, &mc, retaddr);

    if (pt->wrap_level >= MAX_WRAP_NESTING) {
        pt->wrap_level--;
        return; /* we skipped the wrap */
    }
    if (pt->skip[pt->wrap_level]) {
        pt->skip[pt->wrap_level] = false;
        pt->wrap_level--;
        return; /* skip the post-func cbs */
    }

    /* process any skipped post-tailcall events */
    while (pt->wrap_level > 0 &&
           /* tailcall was recorded at cur level minus one */
           pt->tailcall_target[pt->wrap_level - 1] != NULL) {
        /* We've missed the return from a tailcalled alloc routine,
         * so process that now before this "outer" return.  PR 418138.
         */
        app_pc inner_func = pt->tailcall_target[pt->wrap_level - 1];
        app_pc inner_post = pt->tailcall_post_call[pt->wrap_level - 1];
        /* If the tailcall was not an exit from the outer (e.g., it's
         * an exit from a helper routine) just clear and proceed.
         * We assume no self-recursive tailcalls.
         */
        pt->tailcall_target[pt->wrap_level - 1] = NULL;
        pt->tailcall_post_call[pt->wrap_level - 1] = NULL;
        if (inner_func != pc) {
            /* tailcall bypassed inner_func before outer func */
            ASSERT(pt->wrap_level > 1, "tailcall mistake");
            drwrap_after_callee(inner_func, inner_post);
        }
    }
    if (pt->wrap_level < MAX_WRAP_NESTING &&
        pt->last_wrap_func[pt->wrap_level] != pc) {
        /* a jump or other transfer to a post-call site, where the
         * transfer happens inside a heap routine and so we can't use
         * the wrap_level==0 check above.  we distinguish
         * from a real post-call by comparing the last_wrap_func.
         * this was hit in PR 465516.
         */
        return;
    }

    hashtable_lock(&wrap_table);
    wrap = hashtable_lookup(&wrap_table, (void *)pc);
    for (idx = 0; wrap != NULL; idx++, wrap = wrap->next) {
        /* handle the list changing between pre and post events */
        uint tmp = idx;
        /* we may have to skip some entries */
        for (; idx < pt->user_data_count[pt->wrap_level]; idx++) {
            if (wrap->post_cb == pt->user_data_post_cb[pt->wrap_level][idx])
                break; /* all set */
        }
        if (idx == pt->user_data_count[pt->wrap_level]) {
            /* we didn't find it, it must be new, so had no pre => skip post */
            idx = tmp; /* reset */
        } else {
            (*wrap->post_cb)(&wrapcxt, pt->user_data[pt->wrap_level][idx]);
        }
    } 
    hashtable_unlock(&wrap_table);
    if (wrapcxt.mc_modified)
        dr_set_mcontext(drcontext, wrapcxt.mc);

    dr_thread_free(drcontext, pt->user_data[pt->wrap_level],
                   sizeof(void*)*pt->user_data_count[pt->wrap_level]);
    dr_thread_free(drcontext, pt->user_data_post_cb[pt->wrap_level],
                   sizeof(void*)*pt->user_data_count[pt->wrap_level]);
    pt->user_data[pt->wrap_level] = NULL;
    pt->user_data_post_cb[pt->wrap_level] = NULL;

    ASSERT(pt->wrap_level >= 0, "internal wrapping error");
    pt->wrap_level--;
}

/* called via clean call at tailcall jmp
 * post_call is the address after the jmp instr
 */
static void
drwrap_handle_tailcall(app_pc callee, app_pc post_call)
{
    /* For a func that uses a tailcall to call an alloc routine, we have
     * to get the retaddr dynamically.
     */
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *) drmgr_get_tls_field(drcontext, tls_idx);
    /* ignore tailcall at very outer: we'll process at subsequent retaddr */
    if (pt->wrap_level >= 0) {
        /* Store the target so we can process both this and the "outer"
         * alloc routine at the outer's post-call point (PR 418138).
         */
        ASSERT(pt->tailcall_target[pt->wrap_level] == NULL,
               "tailcall var not cleaned up");
        pt->tailcall_target[pt->wrap_level] = callee;
        pt->tailcall_post_call[pt->wrap_level] = post_call;
    }
}

static void
drwrap_check_for_tailcall(void *drcontext, instrlist_t *bb, instr_t *inst)
{
    /* Look for tail call */
    bool is_tail_call = false;
    app_pc target;
    dr_mcontext_t mc = {sizeof(mc),};
    if (instr_get_opcode(inst) != OP_jmp)
        return;
    if (!dr_get_mcontext(drcontext, &mc)) {
        ASSERT(false, "unable to get mc");
        return;
    }
    target = opnd_get_pc(instr_get_target(inst));
    if (is_wrapped_routine(target)) {
        is_tail_call = true;
    } else {
        /* May jmp to plt jmp* */
        instr_t callee;
        instr_init(drcontext, &callee);
        if (decode(drcontext, target, &callee) != NULL &&
            instr_get_opcode(&callee) == OP_jmp_ind) {
            opnd_t opnd = instr_get_target(&callee);
            size_t read;
            if (opnd_is_base_disp(opnd) && opnd_get_base(opnd) == DR_REG_NULL &&
                opnd_get_index(opnd) == DR_REG_NULL) {
                if (dr_safe_read(opnd_compute_address(opnd, &mc), sizeof(target),
                                 &target, &read) && 
                    read == sizeof(target) &&
                    is_wrapped_routine(target))
                    is_tail_call = true;
            }
        }
        instr_free(drcontext, &callee);
    }
    if (is_tail_call) {
        /* at runtime we'll record the tailcall so we can process it post-caller */
        app_pc post_call = instr_get_app_pc(inst) + instr_length(drcontext, inst);
        dr_insert_clean_call(drcontext, bb, inst, (void *)drwrap_handle_tailcall,
                             false, 2, OPND_CREATE_INTPTR((ptr_int_t)target),
                             OPND_CREATE_INTPTR((ptr_int_t)post_call));
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
    hashtable_lock(&wrap_table);
    wrap = hashtable_lookup(&wrap_table, (void *)pc);
    if (wrap != NULL) {
        dr_insert_clean_call(drcontext, bb, inst, (void *)drwrap_in_callee,
                             false, 1, OPND_CREATE_INTPTR((ptr_int_t)pc));
    }
    hashtable_unlock(&wrap_table);

    hashtable_lock(&post_call_table);
    post = (post_call_entry_t *) hashtable_lookup(&post_call_table, (void*)pc);
    if (post != NULL) {
        dr_insert_clean_call(drcontext, bb, inst, (void *)drwrap_after_callee,
                             false, 2,
                             OPND_CREATE_INTPTR((ptr_int_t)post->callee),
                             OPND_CREATE_INTPTR((ptr_int_t)pc));
    }
    hashtable_unlock(&post_call_table);

    drwrap_check_for_tailcall(drcontext, bb, inst);

    return DR_EMIT_DEFAULT;
}

/* removes all entries with key in [start..end)
 * hashtable must use caller-synch
 * XXX: add to drcontainers
 */
static void
hashtable_remove_range(hashtable_t *table, byte *start, byte *end)
{
    uint i;
    hashtable_lock(table);
    for (i = 0; i < HASHTABLE_SIZE(table->table_bits); i++) {
        hash_entry_t *he, *nxt;
        for (he = table->table[i]; he != NULL; he = nxt) {
            /* support hashtable_remove() while iterating */
            nxt = he->next;
            if ((byte *)he->key >= start && (byte *)he->key < end) {
                hashtable_remove(table, he->key);
            }
        }
    }
    hashtable_unlock(table);
}

static void
drwrap_event_module_unload(void *drcontext, const module_data_t *info)
{
    /* XXX i#114: should also remove from post_call_table and call_site_table
     * on other code modifications: for now we assume no such
     * changes to app code.
     */
    hashtable_remove_range(&call_site_table, info->start, info->end);
    hashtable_remove_range(&post_call_table, info->start, info->end);
}

DR_EXPORT
bool
drwrap_wrap(app_pc func,
            void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
            void (*post_func_cb)(void *wrapcxt, void *user_data))
{
    wrap_entry_t *wrap_cur, *wrap_new;

    if (func == NULL || pre_func_cb == NULL || post_func_cb == NULL)
        return false;

    wrap_new = dr_global_alloc(sizeof(*wrap_new));
    wrap_new->func = func;
    wrap_new->pre_cb = pre_func_cb;
    wrap_new->post_cb = post_func_cb;

    hashtable_lock(&wrap_table);
    wrap_cur = hashtable_lookup(&wrap_table, (void *)func);
    if (wrap_cur != NULL) {
        /* we add in reverse order (documented in interface) */
        wrap_entry_t *e;
        /* things will break down w/ duplicate cbs */
        for (e = wrap_cur; e != NULL; e = e->next) {
            if (e->pre_cb == pre_func_cb || e->post_cb == post_func_cb) {
                dr_global_free(wrap_new, sizeof(*wrap_new));
                hashtable_unlock(&wrap_table);
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
    hashtable_unlock(&wrap_table);
    return true;
}

DR_EXPORT
bool
drwrap_unwrap(app_pc func,
              void (*pre_func_cb)(void *wrapcxt, OUT void **user_data))
{
    wrap_entry_t *wrap;
    bool res = false;

    hashtable_lock(&wrap_table);
    wrap = hashtable_lookup(&wrap_table, (void *)func);
    if (wrap != NULL) {
        wrap_entry_t *prev;
        for (prev = NULL; wrap != NULL; prev = wrap, wrap = wrap->next) {
            if (wrap->pre_cb == pre_func_cb)
                break;
        }
        if (wrap != NULL) {
            res = true;
            if (prev == NULL) {
                if (wrap->next == NULL) {
                    hashtable_remove(&wrap_table, (void *)func);
                    wrap = NULL; /* don't double-free */
                    /* we do not guarantee faster than a lazy flush */
                    if (!dr_unlink_flush_region(func, 1))
                        ASSERT(false, "wrap update flush failed");
                } else {
                    hashtable_add_replace(&wrap_table, (void *)func, (void *)wrap->next);
                }
            } else {
                prev->next = wrap->next;
            }
            if (wrap != NULL)
                dr_global_free(wrap, sizeof(*wrap));
        }
    }
    hashtable_unlock(&wrap_table);
    return res;
}
