/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
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
 */

#include "dr_api.h"
#include "drwrap.h"
#include "drmgr.h"
#include "hashtable.h"
#include "drvector.h"
#include "../ext_utils.h"
#include <string.h>
#include <stddef.h> /* offsetof */
#include <limits.h> /* USHRT_MAX */

/* currently using asserts on internal logic sanity checks (never on
 * input from user)
 */
#ifdef DEBUG
#    define ASSERT(x, msg) DR_ASSERT_MSG(x, msg)
#    define DODEBUG(statement) \
        do {                   \
            statement          \
        } while (0)
#else
#    define ASSERT(x, msg) /* nothing */
#    define DODEBUG(statement)
#endif

#define EXCLUDE_CALLCONV(flags) ((flags) & ~(DRWRAP_CALLCONV_MASK))
#define EXTRACT_CALLCONV(flags) ((drwrap_callconv_t)((flags) & (DRWRAP_CALLCONV_MASK)))

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x) /* nothing */
#endif

#ifdef DEBUG
static uint verbose = 0;
#    define NOTIFY(level, ...)                   \
        do {                                     \
            if (verbose >= (level)) {            \
                dr_fprintf(STDERR, __VA_ARGS__); \
            }                                    \
        } while (0)
#else
#    define NOTIFY(...) /* nothing */
#endif

#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)

/* We rely on being able to clobber this register at call sites. */
/* At return, we assume we can clobber caller-saved regs not used for return values. */
#ifdef ARM
#    define CALL_POINT_SCRATCH_REG DR_REG_R12
#    define RETURN_POINT_SCRATCH_REG DR_REG_R1
#elif defined(AARCH64)
#    define CALL_POINT_SCRATCH_REG DR_REG_X12
#    define RETURN_POINT_SCRATCH_REG DR_REG_X1
#elif defined(RISCV64)
/* FIXME i#3544: Check if T6 is valid */
#    define CALL_POINT_SCRATCH_REG DR_REG_T6
#    define RETURN_POINT_SCRATCH_REG DR_REG_A1
#elif defined(X64)
#    define CALL_POINT_SCRATCH_REG DR_REG_R11
#    define RETURN_POINT_SCRATCH_REG DR_REG_RCX
#elif defined(X86)
#    define CALL_POINT_SCRATCH_REG DR_REG_NULL /* Unused. */
#    define RETURN_POINT_SCRATCH_REG DR_REG_ECX
#else
#    define CALL_POINT_SCRATCH_REG DR_REG_NULL
#    define RETURN_POINT_SCRATCH_REG DR_REG_NULL
#endif

/* XXX i#4215: DR should provide 64-bit-sized atomics for 32-bit code. */
#ifdef X64
#    define dr_atomic_add_stat_return_sum dr_atomic_add64_return_sum
#    define dr_atomic_load_stat dr_atomic_load64
#else
#    define dr_atomic_add_stat_return_sum dr_atomic_add32_return_sum
#    define dr_atomic_load_stat dr_atomic_load32
#endif

/* protected by wrap_lock */
static drwrap_global_flags_t global_flags;

#ifdef WINDOWS
static int sysnum_NtContinue = -1;
#endif

/* in drwrap_asm.asm */
void
replace_native_xfer(void);

void
replace_native_rets(void);

void
replace_native_ret_imms(void);

void
replace_native_ret_imms_end(void);

void
replace_retaddr_sentinel(void);

#ifdef AARCHXX
byte *
get_cur_xsp(void);
#endif

/***************************************************************************
 * REQUEST TRACKING
 */

/* There can only be one replacement for any target so we store just app_pc */
#define REPLACE_TABLE_HASH_BITS 6
/* i#1689: we store the decorated (LSB=1) pc (passed from client) in the table */
static hashtable_t replace_table;

/* Native replacements need to store the stack adjust and user data */
typedef struct _replace_native_t {
    app_pc replacement;
    bool at_entry;
    uint stack_adjust;
    void *user_data;
} replace_native_t;

#define REPLACE_NATIVE_TABLE_HASH_BITS 6
/* i#1689: we store the decorated (LSB=1) pc (passed from client) in the table */
static hashtable_t replace_native_table;

static void
replace_native_free(void *v)
{
    replace_native_t *rn = (replace_native_t *)v;
    dr_global_free(rn, sizeof(*rn));
}

/* For each target wrap address, we store a list of wrap requests */
typedef struct _wrap_entry_t {
    app_pc func;
    void (*pre_cb)(void *, OUT void **);
    void (*post_cb)(void *, void *);
    /* To support delayed removal.  We don't set pre_cb and post_cb
     * to NULL instead b/c we want to support re-wrapping.
     */
    bool enabled;
    drwrap_wrap_flags_t flags;
    /* While it would probably be incorrect for the user to wrap the same function
     * multiple times using differing calling conventions, it is possible. Therefore
     * we propagate the callconv to the wrapcxt prior to each pre/post wrap callback.
     */
    drwrap_callconv_t callconv;
    void *user_data;
    struct _wrap_entry_t *next;
} wrap_entry_t;

#define WRAP_TABLE_HASH_BITS 6
/* i#1689: we store the decorated (LSB=1) pc (passed from client) in the table */
static hashtable_t wrap_table;
/* We need recursive locking on the table to support drwrap_unwrap
 * being called from a post event so we use this lock instead of
 * hashtable_lock(&wrap_table)
 */
static void *wrap_lock;

static void
wrap_entry_free(void *v)
{
    wrap_entry_t *e = (wrap_entry_t *)v;
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

/* i#1713: per-thread state, similar to where_am_i_t */
typedef enum _drwrap_where_t {
    DRWRAP_WHERE_OUTSIDE_CALLBACK,
    DRWRAP_WHERE_PRE_FUNC,
    DRWRAP_WHERE_POST_FUNC
} drwrap_where_t;

typedef struct _per_thread_t {
    int wrap_level;
    /* record which wrap routine */
    app_pc last_wrap_func[MAX_WRAP_NESTING];
    /* for no-frills we store wrap_entry_t */
    wrap_entry_t *last_wrap_entry[MAX_WRAP_NESTING];
    void *user_data_nofrills[MAX_WRAP_NESTING];
    /* record app esp to handle tailcalls, etc. */
    reg_t app_esp[MAX_WRAP_NESTING];
    /* user_data for passing between pre and post cbs */
    size_t user_data_count[MAX_WRAP_NESTING];
    void **user_data[MAX_WRAP_NESTING];
    void **user_data_pre_cb[MAX_WRAP_NESTING];
    void **user_data_post_cb[MAX_WRAP_NESTING];
    /* whether to skip */
    bool skip[MAX_WRAP_NESTING];
#ifdef WINDOWS
    /* did we see an exception while in a wrapped routine? */
    bool hit_exception;
#endif
    app_pc retaddr[MAX_WRAP_NESTING];
    /* For drbbdup don't-wrap cases. */
    bool cleanup_only;
} per_thread_t;

/***************************************************************************
 * UTILITIES
 */

/* Rather than a mutex we use atomic increments. */
static drwrap_stats_t drwrap_stats;

/* XXX: should DR provide this variant of dr_safe_read?  DrMem uses this too. */
bool
fast_safe_read(void *base, size_t size, void *out_buf)
{
#ifdef WINDOWS
    /* For all of our uses, a failure is rare, so we do not want
     * to pay the cost of the syscall (DrMemi#265).
     */
    bool res = true;
    DR_TRY_EXCEPT(
        dr_get_current_drcontext(), { memcpy(out_buf, base, size); }, { /* EXCEPT */
                                                                        res = false;
        });
    return res;
#else
    return dr_safe_read(base, size, out_buf, NULL);
#endif
}

/***************************************************************************
 * WRAPPING INSTRUMENTATION TRACKING
 */

/* Hashtable so we can remember post-call pcs (since
 * post-cti-instrumentation is not supported by DR).
 * Synchronized externally to safeguard the externally-allocated payload,
 * using an rwlock b/c read on every instruction.
 */
#define POST_CALL_TABLE_HASH_BITS 10
/* i#1689: we store the aligned (LSB=0) pc here */
static hashtable_t post_call_table;
static void *post_call_rwlock;

typedef struct _post_call_entry_t {
    /* PR 454616: we need two flags in the post_call_table: one that
     * says "please add instru for this callee" and one saying "all
     * existing fragments have instru"
     */
    bool existing_instrumented;
    /* There seems to be no easy solution to correctly removing from
     * the table without extra removals from our own non-consistency
     * flushes: with delayed deletion we can easily have races, and if
     * conservative we have performance problems where one tag's flush
     * removes a whole buch of post-call, delayed deletion causes
     * table removal after re-instrumentation, and then the next
     * retaddr check causes another flush.
     * Xref DrMemi#673, DRi#409, DrMemi#114, DrMemi#260.
     */
#define POST_CALL_PRIOR_BYTES_STORED 6 /* max normal call size */
    byte prior[POST_CALL_PRIOR_BYTES_STORED];
} post_call_entry_t;

/* Support for external post-call caching */
typedef struct _post_call_notify_t {
    void (*cb)(app_pc);
    struct _post_call_notify_t *next;
} post_call_notify_t;

/* protected by post_call_rwlock */
post_call_notify_t *post_call_notify_list;

/* FIFO cache read w/o a lock (which we assume is fine b/c it's word-aligned
 * and thus does not cross a cache line) and written under post_call_rwlock
 */
static uint postcall_cache_idx;
#define POSTCALL_CACHE_SIZE 8
static app_pc postcall_cache[POSTCALL_CACHE_SIZE];

static void
post_call_entry_free(void *v)
{
    post_call_entry_t *e = (post_call_entry_t *)v;
    ASSERT(e != NULL, "invalid hashtable deletion");
    dr_global_free(e, sizeof(*e));
}

/* caller must hold write lock */
static post_call_entry_t *
post_call_entry_add(app_pc postcall, bool external)
{
    post_call_entry_t *e = (post_call_entry_t *)dr_global_alloc(sizeof(*e));
    ASSERT(dr_rwlock_self_owns_write_lock(post_call_rwlock), "must hold write lock");
    e->existing_instrumented = false;
    if (!fast_safe_read(postcall - POST_CALL_PRIOR_BYTES_STORED,
                        POST_CALL_PRIOR_BYTES_STORED, e->prior)) {
        /* notify client somehow?  we'll carry on and invalidate on next bb */
        memset(e->prior, 0, sizeof(e->prior));
    }
    if (!hashtable_add(&post_call_table, (void *)postcall, (void *)e)) {
        NOTIFY(2, "%s: failed to add %p external=%d\n", __FUNCTION__, postcall, external);
        post_call_entry_free(e);
        return NULL;
    }
    if (!external && post_call_notify_list != NULL) {
        post_call_notify_t *cb = post_call_notify_list;
        while (cb != NULL) {
            cb->cb(postcall);
            cb = cb->next;
        }
    }
    NOTIFY(3, "%s %p external=%d\n", __FUNCTION__, postcall, external);
    return e;
}

/* caller must hold post_call_rwlock read lock or write lock */
static bool
post_call_consistent(app_pc postcall, post_call_entry_t *e)
{
    byte cur[POST_CALL_PRIOR_BYTES_STORED];
    ASSERT(e != NULL, "invalid param");
    /* i#673: to avoid all the problems w/ invalidating on delete, we instead
     * invalidate on lookup.  We store the prior 6 bytes which is the call
     * instruction, which is what we care about.  Note that it's ok for us
     * to not be 100% accurate b/c we now use stored-esp on
     * post-call and so make no assumptions about post-call sites.
     */
    if (!fast_safe_read(postcall - POST_CALL_PRIOR_BYTES_STORED,
                        POST_CALL_PRIOR_BYTES_STORED, cur)) {
        /* notify client somehow? */
        return false;
    }
    return (memcmp(e->prior, cur, POST_CALL_PRIOR_BYTES_STORED) == 0);
}

#ifdef X86
static bool
post_call_lookup(app_pc pc)
{
    bool res = false;
    dr_rwlock_read_lock(post_call_rwlock);
    res = (hashtable_lookup(&post_call_table, (void *)pc) != NULL);
    dr_rwlock_read_unlock(post_call_rwlock);
    return res;
}
#endif

/* marks as having instrumentation if it finds the entry */
static bool
post_call_lookup_for_instru(app_pc pc)
{
    bool res = false;
    post_call_entry_t *e;
    dr_rwlock_read_lock(post_call_rwlock);
    e = (post_call_entry_t *)hashtable_lookup(&post_call_table, (void *)pc);
    if (e != NULL) {
        res = post_call_consistent(pc, e);
        if (!res) {
            int i;
            /* need the write lock */
            dr_rwlock_read_unlock(post_call_rwlock);
            e = NULL; /* no longer safe */
            dr_rwlock_write_lock(post_call_rwlock);
            /* might not be found now if racily removed: but that's fine */
            NOTIFY(2, "%s: removing %p\n", __FUNCTION__, pc);
            hashtable_remove(&post_call_table, (void *)pc);
            /* invalidate cache */
            for (i = 0; i < POSTCALL_CACHE_SIZE; i++) {
                if (pc == postcall_cache[i])
                    postcall_cache[i] = NULL;
            }
            dr_rwlock_write_unlock(post_call_rwlock);
            return res;
        } else {
            NOTIFY(2, "%s: marking %p instrumented\n", __FUNCTION__, pc);
            e->existing_instrumented = true;
        }
    }
    /* N.B.: we don't need DrMem i#559's storage of postcall points and
     * check here to see if our postcall was flushed from underneath us,
     * b/c we use invalidation on bb creation rather than deletion.
     * So the postcall entry will be removed only if the code changed:
     * and if it did, we don't want to re-add the entry or instru.
     * In that case we'll miss the post-hook at the post-call point, but
     * we'll execute it along w/ the next post-hook b/c of our stored esp.
     * That seems sufficient.
     */
    dr_rwlock_read_unlock(post_call_rwlock);
    return res;
}

DR_EXPORT
bool
drwrap_register_post_call_notify(void (*cb)(app_pc pc))
{
    post_call_notify_t *e;
    if (cb == NULL)
        return false;
    e = dr_global_alloc(sizeof(*e));
    e->cb = cb;
    dr_rwlock_write_lock(post_call_rwlock);
    e->next = post_call_notify_list;
    post_call_notify_list = e;
    dr_rwlock_write_unlock(post_call_rwlock);
    return true;
}

DR_EXPORT
bool
drwrap_unregister_post_call_notify(void (*cb)(app_pc pc))
{
    post_call_notify_t *e, *prev_e;
    bool res;
    if (cb == NULL)
        return false;
    dr_rwlock_write_lock(post_call_rwlock);
    for (prev_e = NULL, e = post_call_notify_list; e != NULL; prev_e = e, e = e->next) {
        if (e->cb == cb)
            break;
    }
    if (e != NULL) {
        if (prev_e == NULL)
            post_call_notify_list = e->next;
        else
            prev_e->next = e->next;
        dr_global_free(e, sizeof(*e));
        res = true;
    } else
        res = false;
    dr_rwlock_write_unlock(post_call_rwlock);
    return res;
}

DR_EXPORT
bool
drwrap_mark_as_post_call(app_pc pc)
{
    /* XXX: a tool adding a whole bunch of these would be better off acquiring
     * the lock just once.  Should we export lock+unlock routines?
     */
    if (pc == NULL)
        return false;
    dr_rwlock_write_lock(post_call_rwlock);
    post_call_entry_add(pc, true);
    dr_rwlock_write_unlock(post_call_rwlock);
    return true;
}

/***************************************************************************
 * WRAPPING CONTEXT
 */

/* An opaque pointer we pass to callbacks and the user passes back for queries */
typedef struct _drwrap_context_t {
    void *drcontext;
    app_pc func;
    dr_mcontext_t *mc;
    app_pc retaddr;
    bool mc_modified;
    /* i#1713: client redirection request */
    bool is_redirect_requested;
    drwrap_callconv_t callconv;
    drwrap_where_t where_am_i;
} drwrap_context_t;

static void
drwrap_context_init(void *drcontext, drwrap_context_t *wrapcxt, app_pc func,
                    dr_mcontext_t *mc, drwrap_where_t where, app_pc retaddr)
{
    wrapcxt->drcontext = drcontext;
    wrapcxt->func = func;
    wrapcxt->mc = mc;
    wrapcxt->retaddr = retaddr;
    wrapcxt->mc_modified = false;
    wrapcxt->is_redirect_requested = false;
    wrapcxt->callconv = 0; /* must be set per wrap_entry_t for each pre/post callback */
    wrapcxt->where_am_i = where;
}

DR_EXPORT
app_pc
drwrap_get_drcontext(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return wrapcxt->drcontext;
}

DR_EXPORT
app_pc
drwrap_get_func(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return wrapcxt->func;
}

DR_EXPORT
app_pc
drwrap_get_retaddr(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return wrapcxt->retaddr;
}

static dr_mcontext_t *
drwrap_get_mcontext_internal(drwrap_context_t *wrapcxt, dr_mcontext_flags_t flags)
{
    dr_mcontext_t tmp;
    flags &= DR_MC_ALL; /* throw away invalid flags */
    /* lazily fill in info if more is requested than we have so far.
     * unfortunately, dr_get_mcontext() clobbers what was there, so we
     * can't just re-get whenever we see a new flag.  the xmm/ymm regs
     * are the bottleneck, so we just separate that out.
     */
    if (!TESTALL(flags, wrapcxt->mc->flags)) {
        dr_mcontext_flags_t old_flags = wrapcxt->mc->flags;
        wrapcxt->mc->flags |= flags | DR_MC_INTEGER | DR_MC_CONTROL;
        if (old_flags == 0) { /* nothing to clobber */
            dr_get_mcontext(wrapcxt->drcontext, wrapcxt->mc);
            if (wrapcxt->where_am_i == DRWRAP_WHERE_PRE_FUNC)
                wrapcxt->mc->pc = wrapcxt->func;
            else
                wrapcxt->mc->pc = wrapcxt->retaddr;
        } else {
            ASSERT(TEST(DR_MC_MULTIMEDIA, flags) && !TEST(DR_MC_MULTIMEDIA, old_flags) &&
                       TESTALL(DR_MC_INTEGER | DR_MC_CONTROL, old_flags),
                   "logic error");
            /* the pre-ymm is smaller than ymm so we make a temp copy and then
             * restore afterward.  ugh, too many copies: but should be worth it
             * for the typical case of not needing multimedia at all and thus
             * having a faster dr_get_mcontext() call above
             */
            memcpy(&tmp, wrapcxt->mc, offsetof(dr_mcontext_t, pc) + sizeof(tmp.pc));
            dr_get_mcontext(wrapcxt->drcontext, wrapcxt->mc);
            memcpy(wrapcxt->mc, &tmp, offsetof(dr_mcontext_t, pc) + sizeof(tmp.pc));
        }
        if (TEST(DRWRAP_FAST_CLEANCALLS, global_flags)) {
            /* N.B: it's fine to have garbage in the xmm slots we didn't save
             * (which is all but the x64 param ones) since we only redirect
             * to start of callee where they're scratch or to retaddr on a
             * skip where client should have retrieved param xmm (should happen
             * automatically on get) and then filled in any xmm retvals.
             */
            if (TEST(DR_MC_CONTROL, flags)) {
                /* we need a sane eflags for redirect, but we also need to preserve
                 * trap flag or other flags so instead of zeroing we copy cur flags
                 * (xref i#806).
                 */
#ifdef AARCHXX
                wrapcxt->mc->xflags = 0; /*0 is fine for ARM */
#elif defined(RISCV64)
                /* No condition flags that need to be cleaned/preserved. */
#else
#    ifdef WINDOWS
                wrapcxt->mc->xflags = __readeflags();
#    else
                ptr_uint_t val;
                __asm__ __volatile__(
                    "pushf" IF_X64_ELSE("q", "l") "; pop" IF_X64_ELSE("q", "l") " %0"
                    : "=m"(val));
                wrapcxt->mc->xflags = val;
#    endif
                ASSERT(!TEST(EFLAGS_DF, wrapcxt->mc->xflags), "DF not cleared");
#endif
            }
        }
    }
    return wrapcxt->mc;
}

DR_EXPORT
dr_mcontext_t *
drwrap_get_mcontext_ex(void *wrapcxt_opaque, dr_mcontext_flags_t flags)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt == NULL)
        return NULL;
    return drwrap_get_mcontext_internal(wrapcxt, flags);
}

DR_EXPORT
dr_mcontext_t *
drwrap_get_mcontext(void *wrapcxt_opaque)
{
    return drwrap_get_mcontext_ex(wrapcxt_opaque,
                                  TEST(DRWRAP_FAST_CLEANCALLS, global_flags)
                                      ? (DR_MC_INTEGER | DR_MC_CONTROL)
                                      : DR_MC_ALL);
}

DR_EXPORT
bool
drwrap_set_mcontext(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt == NULL || wrapcxt->is_redirect_requested)
        return false;
    wrapcxt->mc_modified = true;
    return true;
}

static inline reg_t *
drwrap_stack_arg_addr(drwrap_context_t *wrapcxt, uint arg, uint reg_arg_count,
                      uint stack_arg_offset)
{
    return (reg_t *)(wrapcxt->mc->xsp +
                     (arg - reg_arg_count + stack_arg_offset) * sizeof(reg_t));
}

static inline reg_t *
drwrap_arg_addr(drwrap_context_t *wrapcxt, int arg)
{
    if (wrapcxt == NULL || wrapcxt->mc == NULL)
        return NULL;
    if (wrapcxt->callconv != DRWRAP_CALLCONV_CDECL)
        drwrap_get_mcontext_internal(wrapcxt, DR_MC_INTEGER); /* already have xsp */

    switch (wrapcxt->callconv) {
#if defined(ARM)
    case DRWRAP_CALLCONV_ARM:
        switch (arg) {
        case 0: return &wrapcxt->mc->r0;
        case 1: return &wrapcxt->mc->r1;
        case 2: return &wrapcxt->mc->r2;
        case 3: return &wrapcxt->mc->r3;
        default: return drwrap_stack_arg_addr(wrapcxt, arg, 4, 0);
        }
#elif defined(AARCH64)
    case DRWRAP_CALLCONV_AARCH64:
        switch (arg) {
        case 0: return &wrapcxt->mc->r0;
        case 1: return &wrapcxt->mc->r1;
        case 2: return &wrapcxt->mc->r2;
        case 3: return &wrapcxt->mc->r3;
        case 4: return &wrapcxt->mc->r4;
        case 5: return &wrapcxt->mc->r5;
        case 6: return &wrapcxt->mc->r6;
        case 7: return &wrapcxt->mc->r7;
        default: return drwrap_stack_arg_addr(wrapcxt, arg, 8, 0);
        }
#elif defined(RISCV64)
    case DRWRAP_CALLCONV_RISCV_LP64:
        switch (arg) {
        case 0: return &wrapcxt->mc->a0;
        case 1: return &wrapcxt->mc->a1;
        case 2: return &wrapcxt->mc->a2;
        case 3: return &wrapcxt->mc->a3;
        case 4: return &wrapcxt->mc->a4;
        case 5: return &wrapcxt->mc->a5;
        case 6: return &wrapcxt->mc->a6;
        case 7: return &wrapcxt->mc->a7;
        default: return drwrap_stack_arg_addr(wrapcxt, arg, 8, 0);
        }
#else          /* Intel x86 or x64 */
#    ifdef X64 /* registers are platform-exclusive */
    case DRWRAP_CALLCONV_AMD64:
        switch (arg) {
        case 0: return &wrapcxt->mc->rdi;
        case 1: return &wrapcxt->mc->rsi;
        case 2: return &wrapcxt->mc->rdx;
        case 3: return &wrapcxt->mc->rcx;
        case 4: return &wrapcxt->mc->r8;
        case 5: return &wrapcxt->mc->r9;
        default: return drwrap_stack_arg_addr(wrapcxt, arg, 6, 1 /*retaddr*/);
        }
    case DRWRAP_CALLCONV_MICROSOFT_X64:
        switch (arg) {
        case 0: return &wrapcxt->mc->rcx;
        case 1: return &wrapcxt->mc->rdx;
        case 2: return &wrapcxt->mc->r8;
        case 3: return &wrapcxt->mc->r9;
        default:
            return drwrap_stack_arg_addr(wrapcxt, arg, 4, 1 /*retaddr*/ + 4 /*reserved*/);
        }
#    endif
    case DRWRAP_CALLCONV_CDECL:
        return drwrap_stack_arg_addr(wrapcxt, arg, 0, 1 /*retaddr*/);
    case DRWRAP_CALLCONV_FASTCALL:
        switch (arg) {
        case 0: return &wrapcxt->mc->xcx;
        case 1: return &wrapcxt->mc->xdx;
        default: return drwrap_stack_arg_addr(wrapcxt, arg, 2, 1 /*retaddr*/);
        }
    case DRWRAP_CALLCONV_THISCALL:
        if (arg == 0)
            return &wrapcxt->mc->xcx;
        else
            return drwrap_stack_arg_addr(wrapcxt, arg, 1, 1 /*retaddr*/);
#endif
    default: ASSERT(false, "unknown or unsupported calling convention"); return NULL;
    }
}

DR_EXPORT
void *
drwrap_get_arg(void *wrapcxt_opaque, int arg)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    reg_t *addr = drwrap_arg_addr(wrapcxt, arg);
    if (wrapcxt->where_am_i != DRWRAP_WHERE_PRE_FUNC)
        return NULL; /* can only get args in pre */
    if (addr == NULL)
        return NULL;
    else if (TEST(DRWRAP_SAFE_READ_ARGS, global_flags)) {
        void *value;
        if (!fast_safe_read(addr, sizeof(value), &value))
            return NULL;
        return value;
    } else
        return (void *)*addr;
}

DR_EXPORT
bool
drwrap_set_arg(void *wrapcxt_opaque, int arg, void *val)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    reg_t *addr = drwrap_arg_addr(wrapcxt, arg);
    if (wrapcxt->where_am_i != DRWRAP_WHERE_PRE_FUNC)
        return false; /* can only set args in pre */
    if (addr == NULL)
        return false;
    else {
        bool in_memory = true;
        in_memory = !(addr >= (reg_t *)wrapcxt->mc && addr < (reg_t *)(wrapcxt->mc + 1));
        if (!in_memory)
            wrapcxt->mc_modified = true;
        if (in_memory && TEST(DRWRAP_SAFE_READ_ARGS, global_flags)) {
            if (!dr_safe_write((void *)addr, sizeof(val), val, NULL))
                return false;
        } else
            *addr = (reg_t)val;
    }
    return true;
}

DR_EXPORT
void *
drwrap_get_retval(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt->where_am_i != DRWRAP_WHERE_POST_FUNC)
        return false; /* can only get retval in post */
    if (wrapcxt == NULL || wrapcxt->mc == NULL)
        return NULL;
    /* ensure we have the info we need */
    drwrap_get_mcontext_internal(wrapcxt_opaque, DR_MC_INTEGER);
    return (void *)wrapcxt->mc->IF_X86_ELSE(xax, IF_RISCV64_ELSE(a0, r0));
}

DR_EXPORT
bool
drwrap_set_retval(void *wrapcxt_opaque, void *val)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(wrapcxt->drcontext, tls_idx);
    if (wrapcxt == NULL || wrapcxt->mc == NULL)
        return false;
    /* ensure we have the info we need */
    if (wrapcxt->where_am_i != DRWRAP_WHERE_POST_FUNC && !pt->skip[pt->wrap_level])
        return false; /* can only set retval in post, or if skipping */
    drwrap_get_mcontext_internal(wrapcxt_opaque, DR_MC_INTEGER);
    wrapcxt->mc->IF_X86_ELSE(xax, IF_RISCV64_ELSE(a0, r0)) = (reg_t)val;
    wrapcxt->mc_modified = true;
    return true;
}

DR_EXPORT
bool
drwrap_skip_call(void *wrapcxt_opaque, void *retval, size_t stdcall_args_size)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(wrapcxt->drcontext, tls_idx);
    bool was_skipped = pt->skip[pt->wrap_level];
    if (wrapcxt->where_am_i != DRWRAP_WHERE_PRE_FUNC)
        return false; /* must be in pre to skip */
    if (wrapcxt == NULL || wrapcxt->mc == NULL || wrapcxt->retaddr == NULL)
        return false;
    /* ensure we have the info we need */
    drwrap_get_mcontext_internal(wrapcxt_opaque, DR_MC_INTEGER | DR_MC_CONTROL);
    /* we can't redirect here b/c we need to release locks */
    pt->skip[pt->wrap_level] = true;
    if (!drwrap_set_retval(wrapcxt_opaque, retval)) {
        pt->skip[pt->wrap_level] = was_skipped;
        return false;
    }
#ifdef X86
    wrapcxt->mc->xsp += stdcall_args_size + sizeof(void *) /*retaddr*/;
#endif
    wrapcxt->mc->pc = wrapcxt->retaddr;
    return true;
}

/* i#1713: redirect execution from a post-function callback */
drext_status_t
drwrap_redirect_execution(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;

    if (wrapcxt_opaque == NULL) {
        NOTIFY(2, "%s: rejected redirect: NULL wrapcxt\n", __FUNCTION__);
        return DREXT_ERROR;
    }

    if (wrapcxt->where_am_i != DRWRAP_WHERE_POST_FUNC) {
        NOTIFY(2, "%s: rejected redirect in state %d\n", __FUNCTION__,
               wrapcxt->where_am_i);
        return DREXT_ERROR_INCOMPATIBLE_STATE;
    }

    if (wrapcxt->is_redirect_requested) {
        NOTIFY(2, "%s: rejected redirect: already redirected\n", __FUNCTION__);
        return DREXT_ERROR_INCOMPATIBLE_STATE;
    }

    DODEBUG({
        per_thread_t *pt =
            (per_thread_t *)drmgr_get_tls_field(wrapcxt->drcontext, tls_idx);
        ASSERT(pt->wrap_level >= 0, "must be in a post-wrap");
        NOTIFY(2, "%s: accepted redirect request from the return of " PFX " to " PFX "\n",
               __FUNCTION__, pt->last_wrap_func[pt->wrap_level], wrapcxt->mc->pc);
    });

    drwrap_set_mcontext(wrapcxt_opaque);
    wrapcxt->is_redirect_requested = true;
    return DREXT_SUCCESS;
}

bool
drwrap_is_redirect_requested(void *wrapcxt_opaque)
{
    drwrap_context_t *wrapcxt = (drwrap_context_t *)wrapcxt_opaque;
    if (wrapcxt->where_am_i != DRWRAP_WHERE_POST_FUNC)
        return false;
    return wrapcxt->is_redirect_requested;
}

/***************************************************************************
 * FORWARD DECLS
 */

static void
drwrap_thread_init(void *drcontext);

static void
drwrap_thread_exit(void *drcontext);

static dr_emit_flags_t
drwrap_event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating);

static dr_emit_flags_t
drwrap_event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                       bool for_trace, bool translating, void *user_data);

/* This version takes a separate "instr" and "where", and cleanup_only, for drbbdup use.
 */
static dr_emit_flags_t
drwrap_event_bb_insert_where(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                             instr_t *where, bool for_trace, bool translating,
                             void *user_data, bool cleanup_only);

static void
drwrap_event_module_unload(void *drcontext, const module_data_t *info);

static bool
drwrap_event_restore_state_ex(void *drcontext, bool restore_memory,
                              dr_restore_state_info_t *info);

static inline void
drwrap_in_callee_check_unwind(void *drcontext, per_thread_t *pt, dr_mcontext_t *mc);

#ifdef WINDOWS
static bool
drwrap_event_filter_syscall(void *drcontext, int sysnum);

static bool
drwrap_event_pre_syscall(void *drcontext, int sysnum);

bool
drwrap_event_exception(void *drcontext, dr_exception_t *excpt);
#endif

static void
drwrap_replace_init(void);

/***************************************************************************
 * INIT
 */

static int drwrap_init_count;

DR_EXPORT
bool
drwrap_init(void)
{
    /* make sure replace goes before app2app so use negative priority */
    drmgr_priority_t pri_replace = { sizeof(pri_replace), DRMGR_PRIORITY_NAME_DRWRAP,
                                     NULL, NULL, DRMGR_PRIORITY_APP2APP_DRWRAP };
    drmgr_priority_t pri_insert = { sizeof(pri_insert), DRMGR_PRIORITY_NAME_DRWRAP, NULL,
                                    NULL, DRMGR_PRIORITY_INSERT_DRWRAP };
#ifdef WINDOWS
    /* DrMem i#1098: We use a late priority so we don't unwind if the client
     * handles the fault.
     */
    drmgr_priority_t pri_fault = { sizeof(pri_fault), DRMGR_PRIORITY_NAME_DRWRAP, NULL,
                                   NULL, DRMGR_PRIORITY_FAULT_DRWRAP };
    module_data_t *ntdll;
#endif

    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drwrap_init_count, 1);
    if (count > 1)
        return true;

    /* We have to fail if the stolen reg matches what we need */
    if (dr_get_stolen_reg() != DR_REG_NULL &&
        dr_get_stolen_reg() == CALL_POINT_SCRATCH_REG)
        return false;

    drmgr_init();
    if (!drmgr_register_bb_app2app_event(drwrap_event_bb_app2app, &pri_replace))
        return false;
    if (!TEST(DRWRAP_INVERT_CONTROL, global_flags)) {
        if (!drmgr_register_bb_instrumentation_event(NULL, drwrap_event_bb_insert,
                                                     &pri_insert))
            return false;
    }
    if (!drmgr_register_restore_state_ex_event(drwrap_event_restore_state_ex))
        return false;

    hashtable_init(&replace_table, REPLACE_TABLE_HASH_BITS, HASH_INTPTR,
                   false /*!strdup*/);
    hashtable_init_ex(&replace_native_table, REPLACE_NATIVE_TABLE_HASH_BITS, HASH_INTPTR,
                      false /*!strdup*/, false /*!synch*/, replace_native_free, NULL,
                      NULL);
    hashtable_init_ex(&wrap_table, WRAP_TABLE_HASH_BITS, HASH_INTPTR, false /*!str_dup*/,
                      false /*!synch*/, wrap_entry_free, NULL, NULL);
    hashtable_init_ex(&post_call_table, POST_CALL_TABLE_HASH_BITS, HASH_INTPTR,
                      false /*!str_dup*/, false /*!synch*/, post_call_entry_free, NULL,
                      NULL);
    post_call_rwlock = dr_rwlock_create();
    /* This lock may have been set up by drwrap_set_global_flags() (in this thread). */
    if (wrap_lock == NULL)
        wrap_lock = dr_recurlock_create();
    drmgr_register_module_unload_event(drwrap_event_module_unload);

    tls_idx = drmgr_register_tls_field();
    if (tls_idx == -1)
        return false;
    if (!drmgr_register_thread_init_event(drwrap_thread_init))
        return false;
    if (!drmgr_register_thread_exit_event(drwrap_thread_exit))
        return false;

#ifdef WINDOWS
    ntdll = dr_lookup_module_by_name("ntdll.dll");
    ASSERT(ntdll != NULL, "failed to find ntdll");
    if (ntdll != NULL) {
        app_pc wrapper = (app_pc)dr_get_proc_address(ntdll->handle, "NtContinue");
        ASSERT(wrapper != NULL, "failed to find NtContinue wrapper");
        if (wrapper != NULL) {
            sysnum_NtContinue = drmgr_decode_sysnum_from_wrapper(wrapper);
            ASSERT(sysnum_NtContinue != -1, "error decoding NtContinue");
            dr_register_filter_syscall_event(drwrap_event_filter_syscall);
            drmgr_register_pre_syscall_event(drwrap_event_pre_syscall);
        }
        dr_free_module_data(ntdll);
    }
    drmgr_register_exception_event_ex(drwrap_event_exception, &pri_fault);
#endif

    drwrap_replace_init();

    return true;
}

DR_EXPORT
void
drwrap_exit(void)
{
    /* handle multiple sets of init/exit calls */
    int count = dr_atomic_add32_return_sum(&drwrap_init_count, -1);
    if (count != 0)
        return;

    if (!TEST(DRWRAP_INVERT_CONTROL, global_flags)) {
        if (!drmgr_unregister_bb_insertion_event(drwrap_event_bb_insert))
            ASSERT(false, "failed to unregister in drwrap_exit");
    }
    if (!drmgr_unregister_bb_app2app_event(drwrap_event_bb_app2app) ||
        !drmgr_unregister_restore_state_ex_event(drwrap_event_restore_state_ex) ||
        !drmgr_unregister_module_unload_event(drwrap_event_module_unload) ||
        !drmgr_unregister_thread_init_event(drwrap_thread_init) ||
        !drmgr_unregister_thread_exit_event(drwrap_thread_exit) ||
        !drmgr_unregister_tls_field(tls_idx))
        ASSERT(false, "failed to unregister in drwrap_exit");

#ifdef WINDOWS
    if (sysnum_NtContinue != -1) {
        if (!dr_unregister_filter_syscall_event(drwrap_event_filter_syscall) ||
            !drmgr_unregister_pre_syscall_event(drwrap_event_pre_syscall))
            ASSERT(false, "failed to unregister in drwrap_exit");
    }
    if (!drmgr_unregister_exception_event(drwrap_event_exception))
        ASSERT(false, "failed to unregister in drwrap_exit");
#endif

    if (dr_is_detaching()) {
        memset(&drwrap_stats, 0, sizeof(drwrap_stats_t));
        for (int i = 0; i < POSTCALL_CACHE_SIZE; i++) {
            postcall_cache[i] = NULL;
        }
        postcall_cache_idx = 0;
#ifdef WINDOWS
        sysnum_NtContinue = -1;
#endif
        disabled_count = 0;
    }

    hashtable_delete(&replace_table);
    hashtable_delete(&replace_native_table);
    hashtable_delete(&wrap_table);
    hashtable_delete(&post_call_table);
    dr_rwlock_destroy(post_call_rwlock);
    dr_recurlock_destroy(wrap_lock);
    wrap_lock = NULL; /* For early drwrap_set_global_flags() after re-attach. */
    global_flags = 0; /* For re-attach. */
    drmgr_exit();

    while (post_call_notify_list != NULL) {
        post_call_notify_t *tmp = post_call_notify_list->next;
        dr_global_free(post_call_notify_list, sizeof(*post_call_notify_list));
        post_call_notify_list = tmp;
    }
}

static void
drwrap_thread_init(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(*pt));
    memset(pt, 0, sizeof(*pt));
    pt->wrap_level = -1;
    drmgr_set_tls_field(drcontext, tls_idx, (void *)pt);
}

static void
drwrap_free_user_data(void *drcontext, per_thread_t *pt, int i)
{
    if (pt->user_data[i] != NULL) {
        dr_thread_free(drcontext, pt->user_data[i],
                       sizeof(void *) * pt->user_data_count[i]);
        pt->user_data[i] = NULL;
    }
    if (pt->user_data_pre_cb[i] != NULL) {
        dr_thread_free(drcontext, pt->user_data_pre_cb[i],
                       sizeof(void *) * pt->user_data_count[i]);
        pt->user_data_pre_cb[i] = NULL;
    }
    if (pt->user_data_post_cb[i] != NULL) {
        dr_thread_free(drcontext, pt->user_data_post_cb[i],
                       sizeof(void *) * pt->user_data_count[i]);
        pt->user_data_post_cb[i] = NULL;
    }
}

static void
drwrap_thread_exit(void *drcontext)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    int i;
    for (i = 0; i < MAX_WRAP_NESTING; i++) {
        drwrap_free_user_data(drcontext, pt, i);
    }
    dr_thread_free(drcontext, pt, sizeof(*pt));
}

DR_EXPORT
bool
drwrap_set_global_flags(drwrap_global_flags_t flags)
{
    drwrap_global_flags_t old_flags;
    bool res;
    /* This can be called prior to drwrap_init().
     * We only support this being done in the single DR-initialization thread.
     */
    if (wrap_lock == NULL) {
        ASSERT(dr_atomic_load32(&drwrap_init_count) == 0,
               "Unsupported race between drwrap_init() and drwrap_set_global_flags()");
        wrap_lock = dr_recurlock_create();
    }
    dr_recurlock_lock(wrap_lock);
    if (dr_atomic_load32(&drwrap_init_count) > 0 &&
        /* After drwrap_init() was called, control inversion cannot be changed. */
        TEST(DRWRAP_INVERT_CONTROL, flags) &&
        !TEST(DRWRAP_INVERT_CONTROL, global_flags)) {
        dr_recurlock_unlock(wrap_lock);
        return false;
    }
    /* if anyone asks for safe, be safe.
     * since today the only 2 flags ask for safe, we can accomplish that
     * by simply or-ing in each request.
     * we now have DRWRAP_NO_FRILLS but it's documented as not being removable
     * so we can continue or-ing.
     */
    old_flags = global_flags;
    global_flags |= flags;
    res = (global_flags != old_flags);
    dr_recurlock_unlock(wrap_lock);
    return res;
}

DR_EXPORT
dr_emit_flags_t
drwrap_invoke_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                     instr_t *where, bool for_trace, bool translating, void *user_data)
{
    ASSERT(TEST(DRWRAP_INVERT_CONTROL, global_flags),
           "must set DRWRAP_INVERT_CONTROL to call drwrap_invoke_insert");
    return drwrap_event_bb_insert_where(drcontext, tag, bb, inst, where, for_trace,
                                        translating, user_data, /*cleanup_only=*/false);
}

DR_EXPORT
dr_emit_flags_t
drwrap_invoke_insert_cleanup_only(void *drcontext, void *tag, instrlist_t *bb,
                                  instr_t *inst, instr_t *where, bool for_trace,
                                  bool translating, void *user_data)
{
    ASSERT(TEST(DRWRAP_INVERT_CONTROL, global_flags),
           "must set DRWRAP_INVERT_CONTROL to call drwrap_invoke_insert_cleanup_only");
    return drwrap_event_bb_insert_where(drcontext, tag, bb, inst, where, for_trace,
                                        translating, user_data, /*cleanup_only=*/true);
}

/***************************************************************************
 * FUNCTION REPLACING
 */

#define RET_IMM_LEN 3

static uint max_stack_adjust;

static byte *
get_func_entry(byte *addr)
{
    /* On Windows with /debug the ILT is used.  There may be a link flag to
     * avoid that (the core seems to do so for its safe_read_asm_* labels,
     * although the relevant flags /incremental and /ZI don't seem to be
     * there for drwrap so further investigation is needed)
     * but we may as well be robust to how it's built.
     */
    if (*addr == 0xe9 /*jmp*/)
        return addr + 5 + *(int *)(addr + 1); /* resolve jmp target */
    else
        return addr;
}

static void
drwrap_replace_init(void)
{
#if defined(DEBUG) && defined(X86)
    void *drcontext;
    instr_t inst;
    byte *next_pc;
    uint max_immed = 0;
#endif

    byte *pc = get_func_entry((byte *)replace_native_ret_imms);
    byte *end_pc = get_func_entry((byte *)replace_native_ret_imms_end);
    max_stack_adjust = (uint)((end_pc - pc) / RET_IMM_LEN) * sizeof(void *);

#if defined(DEBUG) && defined(X86)
    drcontext = dr_get_current_drcontext();
    instr_init(drcontext, &inst);
    while (pc < end_pc) {
        next_pc = decode(drcontext, pc, &inst);
        ASSERT(next_pc != NULL, "invalid ret asm");
        if (next_pc == NULL)
            break; /* at least don't inf loop */
        ASSERT(instr_get_opcode(&inst) == OP_ret, "invalid ret asm");
        ASSERT(next_pc - pc == RET_IMM_LEN, "invalid ret len");
        ASSERT(opnd_is_immed_int(instr_get_src(&inst, 0)), "invalid ret opnd");
        max_immed = (uint)opnd_get_immed_int(instr_get_src(&inst, 0));
        instr_reset(drcontext, &inst);
        pc = next_pc;
    }
    ASSERT(max_immed == max_stack_adjust, "invalid max imm");

    pc = get_func_entry((byte *)replace_native_rets);
    decode(drcontext, pc, &inst);
    ASSERT(instr_get_opcode(&inst) == OP_ret, "invalid ret asm");
    instr_free(drcontext, &inst);
#endif

    ASSERT(DRWRAP_REPLACE_NATIVE_DATA_SLOT != SPILL_SLOT_REDIRECT_NATIVE_TGT,
           "TLS slot conflict");
    ASSERT(DRWRAP_REPLACE_NATIVE_SP_SLOT != SPILL_SLOT_REDIRECT_NATIVE_TGT,
           "TLS slot conflict");
}

static byte *
replace_native_ret_stub(uint stack_adjust)
{
    if (stack_adjust > max_stack_adjust || !ALIGNED(stack_adjust, sizeof(void *)))
        return NULL;
    if (stack_adjust == 0)
        return (byte *)replace_native_rets;
    else {
        return ((byte *)replace_native_ret_imms) +
            ((stack_adjust / sizeof(void *)) - 1 /*skip 0*/) * RET_IMM_LEN;
    }
}

DR_EXPORT
bool
drwrap_is_replaced(app_pc func)
{
    return hashtable_lookup(&replace_table, func) != NULL;
}

DR_EXPORT
bool
drwrap_is_replaced_native(app_pc func)
{
    bool res = false;
    hashtable_lock(&replace_native_table);
    res = hashtable_lookup(&replace_native_table, func) != NULL;
    hashtable_unlock(&replace_native_table);
    return res;
}

static bool
drwrap_replace_common(hashtable_t *table, app_pc original, void *payload, bool override,
                      bool force_flush)
{
    bool res = true;
    bool flush = force_flush;
    if (original == NULL)
        return false;
    if (payload == NULL) {
        if (!override)
            res = false;
        else {
            flush = true;
            res = hashtable_remove(table, (void *)original);
        }
    } else {
        if (override) {
            flush = (hashtable_add_replace(table, (void *)original, payload) != NULL);
        } else
            res = hashtable_add(table, (void *)original, payload);
    }
    /* XXX: we're assuming void* tag == pc
     * XXX: we're assuming the replace target is not in the middle of a trace
     */
    if (flush || dr_fragment_exists_at(dr_get_current_drcontext(), original)) {
        /* we do not guarantee faster than a lazy flush.
         * we can't use dr_unlink_flush_region() unless we require that
         * caller hold no locks and be in clean call or syscall event.
         */
        dr_atomic_add_stat_return_sum(&drwrap_stats.flush_count, 1);
        if (!dr_delay_flush_region(original, 1, 0, NULL))
            ASSERT(false, "replace update flush failed");
    }
    return res;
}

DR_EXPORT
bool
drwrap_replace(app_pc original, app_pc replacement, bool override)
{
    if (TEST(DRWRAP_INVERT_CONTROL, global_flags)) {
        /* Not supported in this mode since drbbdup does not support a separate
         * app2app per case.
         */
        return false;
    }
    return drwrap_replace_common(&replace_table, original, replacement, override, false);
}

DR_EXPORT
bool
drwrap_replace_native(app_pc original, app_pc replacement, bool at_entry,
                      uint stack_adjust, void *user_data, bool override)
{
    if (TEST(DRWRAP_INVERT_CONTROL, global_flags)) {
        /* Not supported in this mode since drbbdup does not support a separate
         * app2app per case.
         */
        return false;
    }
    bool res = false;
    replace_native_t *rn;
    if (stack_adjust > max_stack_adjust ||
        !ALIGNED(stack_adjust, sizeof(void *)) IF_NOT_X86(|| stack_adjust != 0))
        return false;
    if (replacement == NULL)
        rn = NULL;
    else {
        rn = dr_global_alloc(sizeof(*rn));
        rn->replacement = replacement;
        rn->at_entry = at_entry;
        rn->stack_adjust = stack_adjust;
        rn->user_data = user_data;
    }
    hashtable_lock(&replace_native_table);
    res = drwrap_replace_common(&replace_native_table, original, rn, override,
                                /* i#1438: if we're not at the entry, we'd better
                                 * flush to ensure we replace.  If this is done at
                                 * module load, the flush will be empty and will cost
                                 * just a few mutexes.
                                 */
                                !at_entry);
    hashtable_unlock(&replace_native_table);
    return res;
}

/* removes all instrs from start to the end of ilist
 * XXX: add to DR and export?
 */
static void
instrlist_truncate(void *drcontext, instrlist_t *ilist, instr_t *start)
{
    instr_t *next = start, *tmp;
    while (next != NULL) {
        tmp = next;
        next = instr_get_next(next);
        instrlist_remove(ilist, tmp);
        instr_destroy(drcontext, tmp);
    }
}

static void
drwrap_replace_bb(void *drcontext, instrlist_t *bb, instr_t *inst, app_pc pc,
                  app_pc replace)
{
#if defined(ARM) || defined(X64)
    instr_t *first, *last;
#endif
    /* remove the rest of the bb and replace w/ jmp to target.
     * with i#427 we'd call instrlist_clear(drcontext, bb)
     */
    instrlist_truncate(drcontext, bb, inst);
#if defined(ARM) || defined(X64)
    /* XXX: simple jmp has reachability issues.
     * Jumping through DR memory doesn't work well (meta instrs in app2app,
     * ind jmp mangled w/ i#107).
     * Probably best to add DR API to set exit cti target of bb
     * which is i#429.  For now we clobber CALL_POINT_SCRATCH_REG, which is scratch
     * and unused for parameter transfer in most calling conventions.
     */
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)replace,
                                     opnd_create_reg(CALL_POINT_SCRATCH_REG), bb, NULL,
                                     &first, &last);
    for (;; first = instr_get_next(first)) {
        instr_set_app(first);
        instr_set_translation(first, pc);
        if (last == NULL || first == last)
            break;
    }
    instrlist_append(bb,
                     INSTR_XL8(XINST_CREATE_jump_reg(
                                   drcontext, opnd_create_reg(CALL_POINT_SCRATCH_REG)),
                               pc));
#else
    instrlist_append(bb,
                     INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(replace)), pc));
#endif
}

static void
drwrap_replace_native_push_retaddr(void *drcontext, instrlist_t *bb, app_pc pc,
                                   ptr_int_t pushval,
                                   opnd_size_t stacksz _IF_X86_64(bool x86))
{
#ifdef AARCHXX
    instr_t *first, *last;
    instrlist_insert_mov_immed_ptrsz(drcontext, pushval, opnd_create_reg(DR_REG_LR), bb,
                                     NULL, &first, &last);
    for (;; first = instr_get_next(first)) {
        instr_set_app(first);
        instr_set_translation(first, pc);
        if (last == NULL || first == last)
            break;
    }
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT(false, "Not implemented");
#else
    if (stacksz == OPSZ_4 IF_X64(&&x86)) {
        instrlist_append(
            bb,
            INSTR_XL8(INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32(pushval)), pc));
    }
#    if defined(X86) && defined(X64)
    else if (!x86 && stacksz == OPSZ_8) {
        /* needs 2 steps */
        instrlist_append(
            bb,
            INSTR_XL8(INSTR_CREATE_push_imm(drcontext, OPND_CREATE_INT32((int)pushval)),
                      pc));
        /* first push sign-extended so may not need 2nd */
        if ((ptr_uint_t)pushval >= 0x80000000) {
            instrlist_append(
                bb,
                INSTR_XL8(INSTR_CREATE_mov_st(
                              drcontext, OPND_CREATE_MEM32(DR_REG_XSP, 4),
                              OPND_CREATE_INT32((int)((ptr_int_t)pushval >> 32))),
                          pc));
        }
    }
#    endif
    else {
        int sz = opnd_size_in_bytes(stacksz);
        ptr_int_t val = 0;
        if (stacksz == OPSZ_2)
            val = pushval & (ptr_int_t)0x0000ffff;
#    if defined(X86) && defined(X64)
        else {
            ASSERT(stacksz == OPSZ_4 && !x86, "illegal stack size for call");
            val = (ptr_int_t)pushval & (ptr_int_t)0xffffffff;
        }
#    endif
        /* can't do a non-default operand size with a push immed so we emulate */
        instrlist_append(
            bb,
            INSTR_XL8(INSTR_CREATE_lea(drcontext, opnd_create_reg(DR_REG_XSP),
                                       opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                                             -sz, OPSZ_lea)),
                      pc));
        instrlist_append(
            bb,
            INSTR_XL8(INSTR_CREATE_mov_st(
                          drcontext,
                          opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0, 0, stacksz),
                          opnd_create_immed_int(val, stacksz)),
                      pc));
    }
#endif /* !ARM */
}

static void
drwrap_replace_native_bb(void *drcontext, instrlist_t *bb, instr_t *inst, app_pc pc,
                         replace_native_t *rn, app_pc topush)
{
    /* we're in the callee to handle indirect transfers, so the retaddr
     * is already in place.  We can't push a new retaddr on top of it b/c
     * we need the args to be accessible with normal offsets.
     * and we don't want to replace the retaddr w/ a code cache retaddr
     * b/c that causes issues w/ callstacks (see
     * https://github.com/DynamoRIO/drmemory/issues/935).
     * We end up going with a continuation-based approach, which avoids
     * a return point in the cache and thus simplifies issues w/
     * the replacement code using app resources (see
     * https://github.com/DynamoRIO/drmemory/issues/900).
     *
     * It's much easier to get the app xsp here so we store it in a
     * tls slot.  We also store the stack adjust for use later.
     *
     *  if (topush != NULL) {
     *      push app retaddr
     *  }
     *    meta mov xsp to scratch slot
     *  if (user_data != NULL) {
     *      meta mov $user_data to scratch slot 2
     *  }
     *    meta jmp to replace routine
     *    <never reached>
     *    nop (to avoid non-empty bb)
     */
    /* get data from inst before we destroy it */
#if defined(DEBUG) && defined(X86)
    uint opc = instr_get_opcode(inst);
#endif
#if defined(X86) && defined(X64)
    bool x86 = instr_get_x86_mode(inst);
#endif
    opnd_size_t stacksz = OPSZ_NA;
#ifdef X86
    if (topush != NULL) {
        ASSERT(instr_num_dsts(inst) > 1 && opnd_is_base_disp(instr_get_dst(inst, 1)),
               "expected call");
        stacksz = opnd_get_size(instr_get_dst(inst, 1));
        ASSERT(IF_X86_ELSE(opc == OP_call || opc == OP_call_ind, instr_is_call(inst)),
               "unsupported call type");
    }
#endif

    instrlist_truncate(drcontext, bb, inst);

    ASSERT(DRWRAP_REPLACE_NATIVE_DATA_SLOT <= dr_max_opnd_accessible_spill_slot(),
           "assuming TLS direct access");
    ASSERT(DRWRAP_REPLACE_NATIVE_SP_SLOT <= dr_max_opnd_accessible_spill_slot(),
           "assuming TLS direct access");

    if (topush != NULL) {
        drwrap_replace_native_push_retaddr(drcontext, bb, pc, (ptr_int_t)topush,
                                           stacksz _IF_X86_64(x86));
    }
#ifdef AARCH64
    /* We clobber CALL_POINT_SCRATCH_REG, which is scratch in most call conventions. */
    instrlist_meta_append(bb,
                          XINST_CREATE_move(drcontext,
                                            opnd_create_reg(CALL_POINT_SCRATCH_REG),
                                            opnd_create_reg(DR_REG_XSP)));
#endif
    instrlist_meta_append(
        bb,
        XINST_CREATE_store(
            drcontext, dr_reg_spill_slot_opnd(drcontext, DRWRAP_REPLACE_NATIVE_SP_SLOT),
            opnd_create_reg(IF_AARCH64_ELSE(CALL_POINT_SCRATCH_REG, DR_REG_XSP))));
    /* We go ahead and use the 3rd fast spill slot for storage */
#ifdef AARCHXX
    /* We don't support non-zero stack_adjust, so we use the slot to store LR. */
    instrlist_meta_append(
        bb,
        XINST_CREATE_store(
            drcontext, dr_reg_spill_slot_opnd(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT),
            opnd_create_reg(DR_REG_LR)));
#else
    instrlist_meta_append(
        bb,
        XINST_CREATE_store(
            drcontext, dr_reg_spill_slot_opnd(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT),
            OPND_CREATE_INT32(rn->stack_adjust)));
#endif
    if (rn->user_data != NULL) {
#if defined(ARM) || defined(X64)
        /* We clobber CALL_POINT_SCRATCH_REG, which is scratch in most call convs */
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)rn->user_data,
                                         opnd_create_reg(CALL_POINT_SCRATCH_REG), bb,
                                         NULL, NULL, NULL);
        instrlist_meta_append(
            bb,
            XINST_CREATE_store(
                drcontext,
                dr_reg_spill_slot_opnd(drcontext, DRWRAP_REPLACE_NATIVE_DATA_SLOT),
                opnd_create_reg(CALL_POINT_SCRATCH_REG)));
#else
        instrlist_meta_append(
            bb,
            INSTR_CREATE_mov_st(
                drcontext,
                dr_reg_spill_slot_opnd(drcontext, DRWRAP_REPLACE_NATIVE_DATA_SLOT),
                OPND_CREATE_INTPTR((ptr_int_t)rn->user_data)));
#endif
    }
#if defined(ARM) || defined(X64)
    /* XXX: simple call has reachability issues.   For now we clobber
     * CALL_POINT_SCRATCH_REG, which is scratch in most calling conventions.
     */
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)rn->replacement,
                                     opnd_create_reg(CALL_POINT_SCRATCH_REG), bb, NULL,
                                     NULL, NULL);
    instrlist_meta_append(
        bb, XINST_CREATE_jump_reg(drcontext, opnd_create_reg(CALL_POINT_SCRATCH_REG)));
#else
    instrlist_meta_append(bb,
                          INSTR_CREATE_jmp(drcontext, opnd_create_pc(rn->replacement)));
#endif
    instrlist_append(bb, INSTR_XL8(XINST_CREATE_nop(drcontext), pc));
}

/* event for function replacing */
static dr_emit_flags_t
drwrap_event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                        bool translating)
{
    instr_t *inst;
    app_pc pc, replace;
    if (dr_fragment_app_pc(tag) == (app_pc)replace_retaddr_sentinel) {
        /* This is our sentinel.  We want this to be invisible to observation clients,
         * so we remove our return instruction and replace with just a meta nop.
         * The insert event will still be called by drmgr.
         */
        inst = instrlist_first(bb);
        ASSERT(instr_get_next(inst) == NULL, "Must just be 1 instr");
        instrlist_meta_preinsert(bb, inst, XINST_CREATE_nop(drcontext));
        instrlist_remove(bb, inst);
        instr_destroy(drcontext, inst);
    }
    if (replace_table.entries == 0 && replace_native_table.entries == 0)
        return DR_EMIT_DEFAULT;
    /* XXX: if we had dr_bbs_cross_ctis() query (i#427) we could just check 1st instr */
    for (inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        /* XXX i#1689: supporting LSB=1 or LSB=0 gets complex.  For now we
         * assume calls from the client always pass LSB=1 (b/c from
         * dr_get_proc_address()), which we store in the table.  We assume that
         * DR always gives us LSB=0, so we add LSB=1 before table lookups.  This
         * breaks down if the client passes dr_fragment_app_pc()!
         */
        pc = dr_app_pc_as_jump_target(instr_get_isa_mode(inst), instr_get_app_pc(inst));
        /* non-native takes precedence */
        if (replace_table.entries > 0) {
            replace = hashtable_lookup(&replace_table, pc);
            if (replace != NULL) {
                drwrap_replace_bb(drcontext, bb, inst, pc, replace);
                break;
            }
        }
        if (replace_native_table.entries > 0) {
            replace_native_t *rn;
            hashtable_lock(&replace_native_table);
            rn = hashtable_lookup(&replace_native_table, pc);
            if (rn != NULL) {
                app_pc topush = NULL;
                if (!rn->at_entry) {
                    if (instr_is_call(inst)) {
                        topush = pc + instr_length(drcontext, inst);
                        topush =
                            dr_app_pc_as_jump_target(instr_get_isa_mode(inst), topush);
                    } else if (!instr_is_ubr(inst)) {
                        /* usage error?  no, we'll trust user knows what they're doing */
                    }
                }
                drwrap_replace_native_bb(drcontext, bb, inst, pc, rn, topush);
                hashtable_unlock(&replace_native_table);
                break;
            }
            hashtable_unlock(&replace_native_table);
        }
    }
    return DR_EMIT_DEFAULT;
}

app_pc
replace_native_xfer_app_retaddr(void)
{
    /* Return the app retaddr stored by drwrap_replace_native_fini().
     * Note that we can't just leave it in this slot to add in bb event
     * for ret gencode b/c the client ibl xfer will clobber spill slots.
     */
    /* XXX: avoid calling dr_get_current_drcontext() 3x!  See comment below. */
    void *drcontext = dr_get_current_drcontext();
    return (app_pc)dr_read_saved_reg(drcontext, DRWRAP_REPLACE_NATIVE_SP_SLOT);
}

uint
replace_native_xfer_stack_adjust(void)
{
#ifdef ARM
    return 0;
#else
    void *drcontext = dr_get_current_drcontext();
    return (uint)dr_read_saved_reg(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT);
#endif
}

byte *
replace_native_xfer_target(void)
{
    /* Retrieve the data stored in the bb and in fini */
    void *drcontext = dr_get_current_drcontext();
#ifdef AARCHXX
    byte *target = replace_native_ret_stub(0);
#else
    uint stack_adjust =
        (uint)dr_read_saved_reg(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT);
    byte *target = replace_native_ret_stub(stack_adjust);
#endif

    /* Set up for gencode.  We want to re-do the stdcall arg and retaddr teardown,
     * but we don't want the app to see it.  We can't easily do it in
     * replace_native_xfer asm code b/c we don't have enough registers, so
     * we have our bb event recognize our gencode and add meta instrs.
     */

    /* Set up for ibl xfer */
    dr_write_saved_reg(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT, (reg_t)target);

    /* XXX: should DR export client_ibl_xfer_is_thread_private()?  Then we could
     * accept NULL drcontext for thread-shared (well we'd be skirting the API by
     * passing GLOBAL_DCONTEXT to dr_{read,write}_saved_reg above)
     */
    return dr_redirect_native_target(drcontext);
}

DR_EXPORT
void
drwrap_replace_native_fini(void *drcontext)
{
    /* We change the retaddr from the original app retaddr (if left
     * there, we'd lose control: but we wanted it original for
     * callstack walk, etc. transparency) to our own routine that will
     * then do a continuation to our gencode which will run in the code
     * cache and duplicate the stdcall + retaddr teardown.
     */
    volatile app_pc app_retaddr;
    byte *xsp = (byte *)dr_read_saved_reg(drcontext, DRWRAP_REPLACE_NATIVE_SP_SLOT);
#ifdef DR_HOST_NOT_TARGET
    byte *cur_xsp = NULL;
    ASSERT(false, "cross-arch execution is not supported");
#elif defined(AARCHXX)
    byte *cur_xsp = get_cur_xsp();
#endif
    ASSERT(xsp != NULL, "did client clobber TLS slot?");
#ifdef AARCHXX
    app_retaddr = (app_pc)dr_read_saved_reg(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT);
#else
    app_retaddr = *(app_pc *)xsp;
#endif
    /* Store data for replace_native_xfer_helper */
    dr_write_saved_reg(drcontext, DRWRAP_REPLACE_NATIVE_SP_SLOT, (reg_t)app_retaddr);

    /* Redirect */
#ifdef AARCHXX
    /* We assume the replacement routine pushed LR on the stack.
     * We need to scan the stack until we find app_retaddr and then overwrite
     * that slot.
     * XXX: what if there are multiple copies?  What if the retaddr is left in
     * LR and never pushed on the stack?
     */
    for (xsp -= sizeof(app_pc); xsp > cur_xsp && *(app_pc *)xsp != app_retaddr;
         xsp -= sizeof(app_pc))
        ; /* empty body */
    /* XXX: what can we do if we hit cur xsp?  We'll lose control. */
    ASSERT(xsp > cur_xsp, "did not find return address: going to lose control");
    *(app_pc *)xsp = (app_pc)replace_native_xfer;
#else
    *(app_pc *)xsp = (app_pc)replace_native_xfer;
#endif

    /* DrMem i#1217: zero out this local to avoid messing up high-performance
     * callstack stack scans.
     */
    app_retaddr = 0;
}

/***************************************************************************
 * FUNCTION WRAPPING
 */

static void
drwrap_flush_func(app_pc func)
{
    dr_atomic_add_stat_return_sum(&drwrap_stats.flush_count, 1);
    /* we can't flush while holding the lock.
     * we do not guarantee faster than a lazy flush.
     */
    ASSERT(!dr_recurlock_self_owns(wrap_lock), "cannot hold lock while flushing");
    if (!dr_unlink_flush_region(func, 1))
        ASSERT(false, "wrap update flush failed");
}

static app_pc
get_retaddr_from_stack(reg_t xsp)
{
    app_pc retaddr = NULL;
    if (TEST(DRWRAP_SAFE_READ_RETADDR, global_flags)) {
        if (!fast_safe_read((void *)xsp, sizeof(retaddr), &retaddr))
            return NULL;
    } else
        retaddr = *(app_pc *)xsp;
    return retaddr;
}

static bool
set_retaddr_on_stack(reg_t xsp, app_pc value)
{
    bool res = true;
    if (TEST(DRWRAP_SAFE_READ_RETADDR, global_flags)) {
        DR_TRY_EXCEPT(
            dr_get_current_drcontext(), { *(app_pc *)xsp = value; }, { /* EXCEPT */
                                                                       res = false;
            });
    } else
        *(app_pc *)xsp = value;
    return res;
}

/* May not return. If it doesn't return, it decrements wrap_level and assumes
 * it's re-starting in-callee. */
static void
drwrap_mark_retaddr_for_instru(void *drcontext, per_thread_t *pt, app_pc decorated_pc,
                               drwrap_context_t *wrapcxt, bool enabled)
{
    post_call_entry_t *e;
    app_pc retaddr = dr_app_pc_as_load_target(DR_ISA_ARM_THUMB, wrapcxt->retaddr);
    /* We will come here again after the flush-redirect.
     * FIXME: should we try to flush the call instr itself: don't
     * know size though but can be pretty sure.
     */
    /* Ensure we have the retaddr instrumented for post-call events */
    dr_rwlock_write_lock(post_call_rwlock);
    e = (post_call_entry_t *)hashtable_lookup(&post_call_table, (void *)retaddr);
    /* PR 454616: we may have added an entry and started a flush
     * but not finished the flush, so we check not just the entry
     * but also the existing_instrumented flag.
     */
    NOTIFY(3, "%s => %p.%d\n", __FUNCTION__, e,
           e == NULL ? -1 : e->existing_instrumented);
    if (e == NULL || !e->existing_instrumented) {
        if (e == NULL) {
            e = post_call_entry_add(retaddr, false);
            ASSERT(e != NULL, "holding lock so cannot already exist");
        }
        /* now that we have an entry in the synchronized post_call_table
         * any new code coming in will be instrumented
         * we assume we only care about fragments starting at retaddr:
         * other than traces, nothing should cross it unless there's some
         * weird mid-call-instr target in which case it's not post-call.
         *
         * XXX: if callee is entirely inside a trace we'll miss the post-call!
         * will only happen w/ wrapping requests after trace is built.
         */
        /* XXX: we're assuming void* tag == pc */
        if (dr_fragment_exists_at(drcontext, (void *)retaddr)) {
            /* XXX: I'd use dr_unlink_flush_region but it requires -enable_full_api.
             * should we dynamically check and use it if we can?
             */
            /* unlock for the flush */
            dr_rwlock_write_unlock(post_call_rwlock);
            if (!enabled) {
                /* We have to continue to instrument post-wrap points
                 * to avoid unbalanced pre vs post hooks, but these flushes
                 * are expensive so let's get rid of the disabled wraps.
                 */
                dr_recurlock_lock(wrap_lock);
                disabled_count = DISABLED_COUNT_FLUSH_THRESHOLD + 1;
                dr_recurlock_unlock(wrap_lock);
            }
            dr_atomic_add_stat_return_sum(&drwrap_stats.flush_count, 1);
            NOTIFY(3, "%s: flushing %p\n", __FUNCTION__, retaddr);
            dr_flush_region(retaddr, 1);
            /* now we are guaranteed no thread is inside the fragment */
            /* another thread may have done a racy competing flush: should be fine */
            dr_rwlock_read_lock(post_call_rwlock);
            e = (post_call_entry_t *)hashtable_lookup(&post_call_table, (void *)retaddr);
            if (e != NULL) /* selfmod could disappear once have PR 408529 */
                e->existing_instrumented = true;
            /* XXX DrMem i#553: if e==NULL, recursion count could get off */
            dr_rwlock_read_unlock(post_call_rwlock);
            /* Since the flush may remove the fragment we're already in,
             * we have to redirect execution to the callee again.
             */
            /* ensure we have DR_MC_ALL */
            drwrap_get_mcontext_internal((void *)wrapcxt, DR_MC_ALL);
            wrapcxt->mc->pc = decorated_pc;
            /* i#4607: We should decrement wrap_level, so, it won't be
             * incremented again in drwrap_in_callee.
             */
            --pt->wrap_level;
            dr_redirect_execution(wrapcxt->mc);
            ASSERT(false, "dr_redirect_execution should not return");
        }
        e->existing_instrumented = true;
    }
    dr_rwlock_write_unlock(post_call_rwlock);
}

/* For querying with a pc that has been normalized by throwing out LSB=1 (i#1689).
 * Caller must hold wrap_table lock.
 */
static wrap_entry_t *
wrap_table_lookup_normalized_pc(app_pc pc)
{
    wrap_entry_t *wrap = hashtable_lookup(&wrap_table, (void *)pc);
#ifdef ARM
    if (wrap == NULL && !TEST(0x1, (ptr_uint_t)pc)) {
        wrap = hashtable_lookup(&wrap_table,
                                (void *)dr_app_pc_as_jump_target(DR_ISA_ARM_THUMB, pc));
    }
#endif
    return wrap;
}

/* assumes that if TEST(DRWRAP_NO_FRILLS, global_flags) then
 * wrap_lock is held
 */
static inline void
drwrap_ensure_postcall(void *drcontext, per_thread_t *pt, wrap_entry_t *wrap,
                       drwrap_context_t *wrapcxt, app_pc decorated_pc)
{
    if (TEST(DRWRAP_NO_DYNAMIC_RETADDRS, wrap->flags)) {
        /* i#0470: On a large multithreaded app, using shared memory here and especially
         * a lock (even an rwlock where the read path is always taken) causes noticeable
         * overhead.  The postcall_cache is often not enough.  Maybe a per-thread cache
         * could help.  For now we provide an option to completely skip the retaddr
         * check and rely on post-call sites found for direct calls.
         * If this ends up seeing some use we could invest in also detecting targets
         * for PLT or IAT indirect calls.
         */
        return;
    }
    app_pc retaddr = dr_app_pc_as_load_target(DR_ISA_ARM_THUMB, wrapcxt->retaddr);
    app_pc plain_pc = dr_app_pc_as_load_target(DR_ISA_ARM_THUMB, decorated_pc);
    if (TEST(DRWRAP_REPLACE_RETADDR, wrap->flags)) {
        NOTIFY(2, "DRWRAP_REPLACE_RETADDR: saving real retaddr as [%d] " PFX "\n",
               pt->wrap_level, retaddr);
        pt->retaddr[pt->wrap_level] = wrapcxt->retaddr; /* Original, not load tgt. */
#ifdef X86
        set_retaddr_on_stack(wrapcxt->mc->xsp, (app_pc)replace_retaddr_sentinel);
#elif defined(RISCV64)
        /* FIXME i#3544: Not implemented */
        ASSERT(false, "Not implemented");
#else
        drwrap_get_mcontext_internal(wrapcxt, DR_MC_CONTROL);
        wrapcxt->mc->lr = (reg_t)replace_retaddr_sentinel;
        wrapcxt->mc_modified = true;
#endif
        return;
    }
    int i;
    /* avoid lock and hashtable lookup by caching prior retaddrs */
    for (i = 0; i < POSTCALL_CACHE_SIZE; i++) {
        if (retaddr == postcall_cache[i])
            return;
    }

    /* to write to the cache we need a write lock */
    dr_rwlock_write_lock(post_call_rwlock);

    /* add to FIFO cache */
    postcall_cache_idx++;
    if (postcall_cache_idx >= POSTCALL_CACHE_SIZE)
        postcall_cache_idx = 0;
    postcall_cache[postcall_cache_idx] = retaddr;

    if (hashtable_lookup(&post_call_table, (void *)retaddr) == NULL) {
        bool enabled = wrap->enabled;
        /* this function may not return: but in that case it will redirect
         * and we'll come back here to do the wrapping.
         * release all locks.
         */
        dr_rwlock_write_unlock(post_call_rwlock);
        if (!TEST(DRWRAP_NO_FRILLS, global_flags))
            dr_recurlock_unlock(wrap_lock);
        drwrap_mark_retaddr_for_instru(drcontext, pt, decorated_pc, wrapcxt, enabled);
        /* if we come back, re-lookup */
        if (!TEST(DRWRAP_NO_FRILLS, global_flags))
            dr_recurlock_lock(wrap_lock);
        wrap = wrap_table_lookup_normalized_pc(plain_pc);
    } else
        dr_rwlock_write_unlock(post_call_rwlock);
}

/* called via clean call at the top of callee */
static void
drwrap_in_callee(void *arg1, reg_t xsp _IF_NOT_X86(reg_t lr))
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    wrap_entry_t *wrap = NULL, *e;
    dr_mcontext_t mc;
    uint idx;
    drwrap_context_t wrapcxt;
    app_pc pc, decorated_pc;
    /* Do we care about the post wrapper?  If not we can save a lot (b/c our
     * call site method causes a lot of instrumentation when there's high fan-in)
     */
    bool intercept_post = false;

    mc.size = sizeof(mc);
    /* we use a passed-in xsp to avoid dr_get_mcontext */
    mc.xsp = xsp;
#ifdef AARCHXX
    /* ditto */
    mc.lr = lr;
#endif
    mc.flags = 0; /* if anything else is asked for, lazily initialize */

    ASSERT(arg1 != NULL, "drwrap_in_callee: arg1 is NULL!");
    ASSERT(pt != NULL, "drwrap_in_callee: pt is NULL!");

    if (TEST(DRWRAP_NO_FRILLS, global_flags)) {
        wrap = (wrap_entry_t *)arg1;
        pc = wrap->func;
    } else
        pc = (app_pc)arg1;

    NOTIFY(2, "%s: level %d function " PFX "\n", __FUNCTION__, pt->wrap_level + 1, pc);

    drwrap_context_init(drcontext, &wrapcxt, pc, &mc, DRWRAP_WHERE_PRE_FUNC,
                        IF_X86_ELSE(get_retaddr_from_stack(xsp), (app_pc)lr));

    drwrap_in_callee_check_unwind(drcontext, pt, &mc);

    if (!TEST(DRWRAP_NO_FRILLS, global_flags)) {
        dr_recurlock_lock(wrap_lock);
        wrap = hashtable_lookup(&wrap_table, (void *)pc);
        ASSERT(wrap != NULL, "failed to find wrap info");
    }

    /* i#1689: after setting wrapcxt.func for caller, clear LSB */
    decorated_pc = pc;
    pc = dr_app_pc_as_load_target(DR_ISA_ARM_THUMB, pc);

    pt->wrap_level++;
    ASSERT(pt->wrap_level >= 0, "wrapping level corrupted");
    ASSERT(pt->wrap_level < MAX_WRAP_NESTING, "max wrapped nesting reached");
    if (pt->wrap_level >= MAX_WRAP_NESTING) {
        if (!TEST(DRWRAP_NO_FRILLS, global_flags))
            dr_recurlock_unlock(wrap_lock);
        wrapcxt.where_am_i = DRWRAP_WHERE_OUTSIDE_CALLBACK;
        return; /* we'll have to skip stuff */
    }
    /* Clear this field to make state restoration easier. */
    pt->retaddr[pt->wrap_level] = NULL;

    /* ensure we have post-call instru */
    if (wrap != NULL) {
        for (e = wrap; e != NULL; e = e->next) {
            if (e->enabled && e->post_cb != NULL) {
                intercept_post = true;
                break; /* we do need a post-call hook */
            }
        }
        if (intercept_post && wrapcxt.retaddr != NULL)
            drwrap_ensure_postcall(drcontext, pt, wrap, &wrapcxt, decorated_pc);
    }

    pt->last_wrap_func[pt->wrap_level] = decorated_pc;
    if (TEST(DRWRAP_NO_FRILLS, global_flags))
        pt->last_wrap_entry[pt->wrap_level] = wrap;
    pt->app_esp[pt->wrap_level] = mc.xsp;
#ifdef DEBUG
    for (idx = 0; idx < (uint)pt->wrap_level; idx++) {
        /* note that this should no longer fire at all b/c of the check above but
         * leaving as a sanity check
         */
        ASSERT(pt->app_esp[idx] >= pt->app_esp[pt->wrap_level],
               "stack pointer off: may miss post-wrap points");
    }
#endif

    if (TEST(DRWRAP_NO_FRILLS, global_flags)) {
        if (!wrap->enabled) {
            dr_recurlock_lock(wrap_lock);
            disabled_count++;
            dr_recurlock_unlock(wrap_lock);
        } else if (wrap->pre_cb != NULL) {
            pt->user_data_nofrills[pt->wrap_level] = wrap->user_data;
            wrapcxt.callconv = wrap->callconv;
            (*wrap->pre_cb)(&wrapcxt, &pt->user_data_nofrills[pt->wrap_level]);
        }
    } else {
        /* because the list could change between pre and post events we count
         * and store here instead of maintaining count in wrap_table
         */
        for (idx = 0, e = wrap; e != NULL; idx++, e = e->next)
            ; /* nothing */
        /* if we skipped the postcall we didn't free prior data yet */
        drwrap_free_user_data(drcontext, pt, pt->wrap_level);
        pt->user_data_count[pt->wrap_level] = idx;
        pt->user_data[pt->wrap_level] = dr_thread_alloc(drcontext, sizeof(void *) * idx);
        /* we have to keep both b/c we allow one to be null (i#562) */
        pt->user_data_pre_cb[pt->wrap_level] =
            dr_thread_alloc(drcontext, sizeof(void *) * idx);
        pt->user_data_post_cb[pt->wrap_level] =
            dr_thread_alloc(drcontext, sizeof(void *) * idx);

        for (idx = 0; wrap != NULL; idx++, wrap = wrap->next) {
            /* if the list does change try to match up in post */
            pt->user_data_pre_cb[pt->wrap_level][idx] = (void *)wrap->pre_cb;
            pt->user_data_post_cb[pt->wrap_level][idx] = (void *)wrap->post_cb;
            if (!wrap->enabled) {
                disabled_count++;
                continue;
            }
            if (wrap->pre_cb != NULL) {
                pt->user_data[pt->wrap_level][idx] = wrap->user_data;
                wrapcxt.callconv = wrap->callconv;
                (*wrap->pre_cb)(&wrapcxt, &pt->user_data[pt->wrap_level][idx]);
            }
            /* was there a request to skip? */
            if (pt->skip[pt->wrap_level])
                break;
        }
        dr_recurlock_unlock(wrap_lock);
    }
    if (pt->skip[pt->wrap_level]) {
        /* drwrap_skip_call already adjusted the stack and pc */
        /* ensure we have DR_MC_ALL */
        dr_redirect_execution(drwrap_get_mcontext_internal((void *)&wrapcxt, DR_MC_ALL));
        ASSERT(false, "dr_redirect_execution should not return");
    }
    if (wrapcxt.mc_modified)
        dr_set_mcontext(drcontext, wrapcxt.mc);
    if (!intercept_post) {
        /* we won't decrement in post so decrement now.  we needed to increment
         * to set up for pt->skip, etc.
         */
        if (!TEST(DRWRAP_NO_FRILLS, global_flags))
            drwrap_free_user_data(drcontext, pt, pt->wrap_level);
        pt->wrap_level--;
    }
    wrapcxt.where_am_i = DRWRAP_WHERE_OUTSIDE_CALLBACK;
}

/* called via clean call at return address(es) of callee
 * if retaddr is NULL then this is a "fake" cleanup on abnormal stack unwind
 */
static void
drwrap_after_callee_func(void *drcontext, per_thread_t *pt, dr_mcontext_t *mc, int level,
                         app_pc retaddr, bool unwind, bool only_requested_unwind)
{
    wrap_entry_t *wrap, *next;
    uint idx;
    drwrap_context_t wrapcxt;
    drvector_t toflush = {
        0,
    };
    bool do_flush = false;
    bool unwound_all = true;
    app_pc pc = pt->last_wrap_func[level];
    ASSERT(pc != NULL, "drwrap_after_callee: pc is NULL!");
    ASSERT(pt != NULL, "drwrap_after_callee: pt is NULL!");
    ASSERT(!only_requested_unwind || unwind, "only_requested_unwind implies unwind");

    NOTIFY(2, "%s: level %d function " PFX "%s\n", __FUNCTION__, level, pc,
           unwind ? " abnormal" : "");

    drwrap_context_init(drcontext, &wrapcxt, pc, mc, DRWRAP_WHERE_POST_FUNC, retaddr);

    if (level >= MAX_WRAP_NESTING) {
        if (level == pt->wrap_level)
            pt->wrap_level--;
        return; /* we skipped the wrap */
    }
    if (pt->skip[level]) {
        pt->skip[level] = false;
        if (level == pt->wrap_level)
            pt->wrap_level--;
        else {
            /* doing an SEH unwind and didn't clear earlier on stack */
            pt->last_wrap_func[level] = NULL;
        }
        return; /* skip the post-func cbs */
    }

    if (TEST(DRWRAP_NO_FRILLS, global_flags)) {
        wrap = pt->last_wrap_entry[level];
    } else {
        dr_recurlock_lock(wrap_lock);
        wrap = wrap_table_lookup_normalized_pc(pc);
    }
    for (idx = 0; wrap != NULL; idx++, wrap = next) {
        /* handle the list changing between pre and post events */
        uint tmp = idx;
        void *user_data = NULL;
        /* handle drwrap_unwrap being called in post_cb */
        next = wrap->next;
        if (!wrap->enabled) {
            if (TEST(DRWRAP_NO_FRILLS, global_flags))
                dr_recurlock_lock(wrap_lock);
            disabled_count++;
            if (TEST(DRWRAP_NO_FRILLS, global_flags))
                dr_recurlock_unlock(wrap_lock);
            continue;
        }
        if (TEST(DRWRAP_REPLACE_RETADDR, wrap->flags)) {
            NOTIFY(2, "DRWRAP_REPLACE_RETADDR: setting real retaddr as [%d] " PFX "\n",
                   level, pt->retaddr[level]);
            dr_write_saved_reg(drcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT,
                               (reg_t)pt->retaddr[level]);
        }
        if (TEST(DRWRAP_NO_FRILLS, global_flags)) {
            user_data = pt->user_data_nofrills[level];
        } else {
            /* we may have to skip some entries */
            for (; idx < pt->user_data_count[level]; idx++) {
                /* we have to check both b/c we allow one to be null (i#562) */
                if (wrap->pre_cb == pt->user_data_pre_cb[level][idx] &&
                    wrap->post_cb == pt->user_data_post_cb[level][idx])
                    break; /* all set */
            }
            user_data = pt->user_data[level][idx];
        }
        if (pt->cleanup_only) {
            /* Skip post handlers. */
        } else if (!TEST(DRWRAP_NO_FRILLS, global_flags) &&
                   idx == pt->user_data_count[level]) {
            /* we didn't find it, it must be new, so had no pre => skip post
             * (even if only has post, to be consistent w/ timing)
             */
            idx = tmp; /* reset */
        } else if (wrap->post_cb != NULL) {
            if (!unwind) {
                wrapcxt.callconv = wrap->callconv;
                (*wrap->post_cb)(&wrapcxt, user_data);
            } else if (!only_requested_unwind ||
                       TEST(DRWRAP_UNWIND_ON_EXCEPTION, wrap->flags)) {
                /* don't double-call those that we presumably already called
                 * that had the flag set.  there might have been a longjmp and no
                 * exception (e.g., any longjmp on linux): but that's too bad:
                 * current impl doesn't handle that.
                 */
                if (only_requested_unwind ||
                    !TEST(DRWRAP_UNWIND_ON_EXCEPTION, wrap->flags)) {
                    (*wrap->post_cb)(NULL, user_data);
                }
            } else
                unwound_all = false;
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
        drvector_init(&toflush, 10, false /*no synch: wrapcxt-local*/, NULL);
        if (TEST(DRWRAP_NO_FRILLS, global_flags))
            dr_recurlock_lock(wrap_lock);
        for (i = 0; i < HASHTABLE_SIZE(wrap_table.table_bits); i++) {
            hash_entry_t *he, *next_he;
            for (he = wrap_table.table[i]; he != NULL; he = next_he) {
                wrap_entry_t *prev = NULL;
                next_he = he->next; /* allow removal */
                for (wrap = (wrap_entry_t *)he->payload; wrap != NULL; wrap = next) {
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
                        if (wrap != NULL) {
                            dr_global_free(wrap, sizeof(*wrap));
                        }
                    } else
                        prev = wrap;
                }
            }
        }
        do_flush = true;
        disabled_count = 0;
        if (TEST(DRWRAP_NO_FRILLS, global_flags))
            dr_recurlock_unlock(wrap_lock);
    }
    if (!TEST(DRWRAP_NO_FRILLS, global_flags))
        dr_recurlock_unlock(wrap_lock);
    if (wrapcxt.mc_modified && !unwind)
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

    if (unwound_all) {
        if (!TEST(DRWRAP_NO_FRILLS, global_flags))
            drwrap_free_user_data(drcontext, pt, level);

        if (level == pt->wrap_level) {
            ASSERT(pt->wrap_level >= 0, "internal wrapping error");
            pt->wrap_level--;
        } else {
            /* doing an SEH unwind and didn't clear earlier on stack */
            pt->last_wrap_func[level] = NULL;
        }
    } /* else, hopefully our unwind detection heuristics will */

    if (wrapcxt.is_redirect_requested) {
        NOTIFY(2, "%s: redirecting to " PFX "; stack at " PFX "\n", __FUNCTION__, mc->pc,
               mc->xsp);

        drwrap_get_mcontext_internal((void *)&wrapcxt, DR_MC_ALL);
        wrapcxt.is_redirect_requested = false;
        dr_redirect_execution(mc);
        ASSERT(false, "reached code following a redirect");
    }
    wrapcxt.where_am_i = DRWRAP_WHERE_OUTSIDE_CALLBACK;
}

/* called via clean call at return address(es) of callee */
static void
drwrap_after_callee(app_pc retaddr, reg_t xsp)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    /* we use a passed-in xsp to avoid dr_get_mcontext */
    mc.xsp = xsp;
    mc.flags = 0; /* if anything else is asked for, lazily initialize */

    ASSERT(pt != NULL, "drwrap_after_callee: pt is NULL!");

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
    /* XXX: I'm worried about the <= here.  We need it for ARM b/c w/ no retaddr on
     * the stack it's easy to have the same sp value, but I'm being conservative
     * and leaving the < for x86.
     */
    while (pt->wrap_level >= 0 && pt->app_esp[pt->wrap_level] IF_X86_ELSE(<, <=) mc.xsp) {
        drwrap_after_callee_func(drcontext, pt, &mc, pt->wrap_level, retaddr, false,
                                 false);
    }
}

static void
drwrap_after_callee_cleanup_only(app_pc retaddr, reg_t xsp)
{
    void *drcontext = dr_get_current_drcontext();
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* To reduce overhead for non-drbbdup cases we avoid an extra arg to a common
     * routine and keep the cost purely on this path.
     */
    pt->cleanup_only = true;
    drwrap_after_callee(retaddr, xsp);
    pt->cleanup_only = false;
}

static void
drwrap_insert_post_call(void *drcontext, instrlist_t *bb, instr_t *where,
                        app_pc pc_as_jmp_target, bool cleanup_only)
{
    /* XXX: for DRWRAP_FAST_CLEANCALLS we must preserve state b/c
     * our post-call points can be reached through non-return paths.
     * We could insert an inline check for "pt->wrap_level >= 0" but
     * that requires spilling a GPR and flags and gets messy w/o drreg
     * vs other components' spill slots.
     */
    dr_cleancall_save_t flags =
        DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT;
    NOTIFY(2, "drwrap inserting post-call cb at " PFX "\n", pc_as_jmp_target);
    dr_insert_clean_call_ex(
        drcontext, bb, where,
        (void *)(cleanup_only ? drwrap_after_callee_cleanup_only : drwrap_after_callee),
        flags, 2,
        /* i#1689: retaddrs do have LSB=1 */
        OPND_CREATE_INTPTR((ptr_int_t)pc_as_jmp_target),
        /* pass in xsp to avoid dr_get_mcontext */
        opnd_create_reg(DR_REG_XSP));
}

/* This version takes a separate "instr" and "where" for use with drbbdup.
 * The separate "where" handles cases such as with drbbdup's final app
 * instruction (which cannot be duplicated into each case) or with
 * emulation where the instruction "inst" to monitor is distinct from
 * the location "where" to insert instrumentation.
 */
static dr_emit_flags_t
drwrap_event_bb_insert_where(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                             instr_t *where, bool for_trace, bool translating,
                             void *user_data, bool cleanup_only)
{
    dr_emit_flags_t res = DR_EMIT_DEFAULT;
    /* XXX: if we had dr_bbs_cross_ctis() query (i#427) we could just check 1st instr
     */
    wrap_entry_t *wrap;
    /* i#1689: we store in drwrap_table as the original from the client (which
     * may have LSB=1), as well as in wrapcxt.  We then clear LSB for all other
     * uses, including all postcall uses.
     */
    app_pc pc =
        dr_app_pc_as_jump_target(instr_get_isa_mode(inst), instr_get_app_pc(inst));

    if (!cleanup_only) {
        /* Strategy: for the pre-hook, do not insert at the call site but rather wait for
         * the callee.  For the post-hook, record the post-call site when we see the
         * call instruction, and additionally record the actual retaddr when in the
         * callee. By doing both we minimize flushes from the return point having already
         * been reached before the callee hook can mark it.
         */
        dr_recurlock_lock(wrap_lock);
        wrap = hashtable_lookup(&wrap_table, (void *)pc);
        if (wrap != NULL) {
            void *arg1 = TEST(DRWRAP_NO_FRILLS, global_flags) ? (void *)wrap : (void *)pc;
            /* i#690: do not bother saving registers that should be scratch at
             * function entry, if requested by user.
             * I considered building a custom context switch but that would require
             * having DR expose PEB/TEB swapping code and duplicating
             * stack alignment code; plus, skipping preservation was already
             * mostly in place for clean call auto opt.
             */
            dr_cleancall_save_t flags = TEST(DRWRAP_FAST_CLEANCALLS, global_flags)
                ? (DR_CLEANCALL_NOSAVE_FLAGS | DR_CLEANCALL_NOSAVE_XMM_NONPARAM)
                : 0;
            flags |= DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT;
            /* FIXME i#3544: Adapt to a real RISC-V clean call implementation */
            dr_insert_clean_call_ex(
                drcontext, bb, where, (void *)drwrap_in_callee, flags,
                IF_AARCHXX_ELSE(3, 2), OPND_CREATE_INTPTR((ptr_int_t)arg1),
                /* pass in xsp to avoid dr_get_mcontext */
                opnd_create_reg(DR_REG_XSP) _IF_AARCHXX(opnd_create_reg(DR_REG_LR)));
        }
        dr_recurlock_unlock(wrap_lock);
    }

    if (post_call_lookup_for_instru(instr_get_app_pc(inst) /*normalized*/)) {
        drwrap_insert_post_call(drcontext, bb, where, pc, cleanup_only);
    }

    if (dr_fragment_app_pc(tag) == (app_pc)replace_retaddr_sentinel) {
        drwrap_insert_post_call(drcontext, bb, where, pc, cleanup_only);
        /* The post-call C code put the real retaddr into the DR slot that will be
         * used by dr_redirect_native_target().
         */
        app_pc tgt = dr_redirect_native_target(drcontext);
        reg_id_t scratch = RETURN_POINT_SCRATCH_REG;
        instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)tgt,
                                         opnd_create_reg(scratch), bb, where, NULL, NULL);
        instrlist_meta_preinsert(
            bb, where, XINST_CREATE_jump_reg(drcontext, opnd_create_reg(scratch)));
        /* This unusual transition confuses DR trying to stitch blocks together into
         * a trace.
         */
        res = DR_EMIT_MUST_END_TRACE;
    }

    if (instr_is_call(inst) && instr_is_app(inst) && opnd_is_pc(instr_get_target(inst))) {
        app_pc target = dr_app_pc_as_jump_target(instr_get_isa_mode(inst),
                                                 opnd_get_pc(instr_get_target(inst)));
        dr_recurlock_lock(wrap_lock);
        wrap = hashtable_lookup(&wrap_table, (void *)target);
        bool add_post = wrap != NULL && wrap->post_cb != NULL &&
            !TEST(DRWRAP_REPLACE_RETADDR, wrap->flags);
        dr_recurlock_unlock(wrap_lock);
        if (add_post) {
            /* Add the pc-as-load-target (so *not* "pc"). */
            dr_rwlock_write_lock(post_call_rwlock);
            post_call_entry_add(instr_get_app_pc(inst) + instr_length(drcontext, inst),
                                false);
            dr_rwlock_write_unlock(post_call_rwlock);
        }
    }

    return res;
}

static dr_emit_flags_t
drwrap_event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                       bool for_trace, bool translating, void *user_data)
{
    ASSERT(!TEST(DRWRAP_INVERT_CONTROL, global_flags),
           "should not get here if DRWRAP_INVERT_CONTROL is set");
    return drwrap_event_bb_insert_where(drcontext, tag, bb, inst, inst, for_trace,
                                        translating, user_data, /*cleanup_only=*/false);
}

static void
drwrap_event_module_unload(void *drcontext, const module_data_t *info)
{
    /* XXX: should also remove on other code modifications: for now we assume no such
     * changes to app code that's being targeted for wrapping.
     */
    NOTIFY(2, "%s: removing %p..%p\n", __FUNCTION__, info->start, info->end);
    dr_rwlock_write_lock(post_call_rwlock);
    hashtable_remove_range(&post_call_table, (void *)info->start, (void *)info->end);
    /* Invalidate cache. */
    for (int i = 0; i < POSTCALL_CACHE_SIZE; i++) {
        if (postcall_cache[i] >= info->start && postcall_cache[i] < info->end)
            postcall_cache[i] = NULL;
    }
    dr_rwlock_write_unlock(post_call_rwlock);

    /* XXX: It's arguable whether we should remove from replace_table,
     * replace_native_table, and wrap_table: we could expect the client to un-replace
     * or un-wrap, and if they don't, and a new module is loaded at the same place
     * and we wrap something weird, we could blame the client.  That's the current
     * approach.
     */
}

static bool
drwrap_event_restore_state_ex(void *drcontext, bool restore_memory,
                              dr_restore_state_info_t *info)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    if (pt->wrap_level < 0)
        return true;
    if (info->mcontext->pc == (app_pc)replace_retaddr_sentinel) {
        NOTIFY(1, "%s: updating T" TIDFMT " PC to real level %d retaddr " PFX "\n",
               __FUNCTION__, dr_get_thread_id(drcontext), pt->wrap_level,
               pt->retaddr[pt->wrap_level]);
        info->mcontext->pc = pt->retaddr[pt->wrap_level];
    }
#ifdef AARCHXX
    if (info->mcontext->lr == (reg_t)replace_retaddr_sentinel) {
        NOTIFY(1, "%s: updating T" TIDFMT " LR to real retaddr " PFX "\n", __FUNCTION__,
               dr_get_thread_id(drcontext), pt->retaddr[pt->wrap_level]);
        info->mcontext->lr = (reg_t)pt->retaddr[pt->wrap_level];
    }
#endif
    if (!restore_memory)
        return true;
    for (int i = 0; i <= pt->wrap_level; ++i) {
        /* We clear pt->retaddr each time, so if it's set we know it's a replaced one. */
        if (pt->retaddr[i] != NULL && info->mcontext->xsp <= pt->app_esp[i]) {
#ifdef X86
            app_pc ra = get_retaddr_from_stack(pt->app_esp[i]);
            if (ra != (app_pc)replace_retaddr_sentinel) {
                NOTIFY(1,
                       "%s: WARNING: T" TIDFMT " retaddr @ " PFX " is " PFX
                       ", not sentinel!\n",
                       __FUNCTION__, dr_get_thread_id(drcontext), pt->app_esp[i], ra);
                continue;
            }
            NOTIFY(1,
                   "%s: updating T" TIDFMT " retaddr @ " PFX " from sentinel " PFX
                   " to real retaddr " PFX "\n",
                   __FUNCTION__, dr_get_thread_id(drcontext), pt->app_esp[i],
                   replace_retaddr_sentinel, pt->retaddr[i]);
            set_retaddr_on_stack(pt->app_esp[i], pt->retaddr[i]);
#else
            /* Unfortunately return addresses are pushed from the link register to the
             * stack in non-uniform ways, forcing us to scan.  This can be time-consuming
             * for large frames, and it could incorrectly replace a integer value that
             * happens to look like our sentinel.  We've warned the user about this
             * risk for this flag though.
             */
            app_pc *scan_stop;
            if (i < pt->wrap_level)
                scan_stop = (app_pc *)pt->app_esp[i + 1];
            else
                scan_stop = (app_pc *)info->mcontext->xsp;
            for (app_pc *scan = (app_pc *)pt->app_esp[i]; scan >= scan_stop; --scan) {
                app_pc ra = get_retaddr_from_stack((reg_t)scan);
                if (ra == (app_pc)replace_retaddr_sentinel) {
                    NOTIFY(1,
                           "%s: updating T" TIDFMT " retaddr @ " PFX " from sentinel " PFX
                           " to real retaddr " PFX "\n",
                           __FUNCTION__, dr_get_thread_id(drcontext), scan,
                           replace_retaddr_sentinel, pt->retaddr[i]);
                    set_retaddr_on_stack((reg_t)scan, pt->retaddr[i]);
                }
            }
#endif
        }
    }
    return true;
}

DR_EXPORT
bool
drwrap_wrap_ex(app_pc func, void (*pre_func_cb)(void *wrapcxt, INOUT void **user_data),
               void (*post_func_cb)(void *wrapcxt, void *user_data), void *user_data,
               uint flags)
{
    wrap_entry_t *wrap_cur, *wrap_new;

    /* allow one side to be NULL (i#562) */
    if (func == NULL || (pre_func_cb == NULL && post_func_cb == NULL))
        return false;
    if (TEST(DRWRAP_REPLACE_RETADDR, flags) && post_func_cb == NULL) {
        /* Nonsensical combination so we fail. */
        return false;
    }

    /* XXX i#1460: should drwrap auto-flush target in case called late?
     * Currently we document that the caller must do that.
     */

    wrap_new = dr_global_alloc(sizeof(*wrap_new));
    wrap_new->func = func;
    wrap_new->pre_cb = pre_func_cb;
    wrap_new->post_cb = post_func_cb;
    wrap_new->enabled = true;
    wrap_new->user_data = user_data;
    wrap_new->flags = EXCLUDE_CALLCONV(flags);
    wrap_new->callconv = EXTRACT_CALLCONV(flags);
    if (wrap_new->callconv == 0)
        wrap_new->callconv = DRWRAP_CALLCONV_DEFAULT;

    dr_recurlock_lock(wrap_lock);
    wrap_cur = hashtable_lookup(&wrap_table, (void *)func);
    if (wrap_cur != NULL) {
        /* we add in reverse order (documented in interface) */
        wrap_entry_t *e;
        /* things will break down w/ duplicate cbs */
        for (e = wrap_cur; e != NULL; e = e->next) {
            if (e->pre_cb == pre_func_cb && e->post_cb == post_func_cb) {
                /* no-frills requires 1st entry to be the live one */
                if (!TEST(DRWRAP_NO_FRILLS, global_flags) || e == wrap_cur) {
                    /* matches existing request: re-enable if necessary */
                    e->enabled = true;
                    /* be sure to update all fields (xref drmem i#816) */
                    e->user_data = user_data;
                    e->flags = flags;
                    dr_global_free(wrap_new, sizeof(*wrap_new));
                    dr_recurlock_unlock(wrap_lock);
                    return true;
                } /* else continue */
            } else if (TEST(DRWRAP_NO_FRILLS, global_flags) && e->enabled) {
                /* more than one wrap of same address is not allowed */
                dr_global_free(wrap_new, sizeof(*wrap_new));
                dr_recurlock_unlock(wrap_lock);
                return false;
            }
        }
        if (TEST(DRWRAP_NO_FRILLS, global_flags)) {
            /* free whole chain of disabled entries */
            wrap_entry_free(wrap_cur);
            wrap_cur = NULL;
        }
        wrap_new->next = wrap_cur;
        hashtable_add_replace(&wrap_table, (void *)func, (void *)wrap_new);
    } else {
        wrap_new->next = NULL;
        hashtable_add(&wrap_table, (void *)func, (void *)wrap_new);
        /* XXX: we're assuming void* tag == pc */
        if (dr_fragment_exists_at(dr_get_current_drcontext(), func)) {
            dr_atomic_add_stat_return_sum(&drwrap_stats.flush_count, 1);
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
drwrap_wrap(app_pc func, void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
            void (*post_func_cb)(void *wrapcxt, void *user_data))
{
    return drwrap_wrap_ex(func, pre_func_cb, post_func_cb, NULL, DRWRAP_CALLCONV_DEFAULT);
}

DR_EXPORT
bool
drwrap_unwrap(app_pc func, void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
              void (*post_func_cb)(void *wrapcxt, void *user_data))
{
    wrap_entry_t *wrap;
    bool res = false;

    if (func == NULL || (pre_func_cb == NULL && post_func_cb == NULL))
        return false;

    dr_recurlock_lock(wrap_lock);
    wrap = hashtable_lookup(&wrap_table, (void *)func);
    for (; wrap != NULL; wrap = wrap->next) {
        if (wrap->pre_cb == pre_func_cb && wrap->post_cb == post_func_cb) {
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

DR_EXPORT
bool
drwrap_is_wrapped(app_pc func, void (*pre_func_cb)(void *wrapcxt, OUT void **user_data),
                  void (*post_func_cb)(void *wrapcxt, void *user_data))
{
    wrap_entry_t *wrap;
    bool res = false;

    if (func == NULL || (pre_func_cb == NULL && post_func_cb == NULL))
        return false;

    dr_recurlock_lock(wrap_lock);
    wrap = hashtable_lookup(&wrap_table, (void *)func);
    for (; wrap != NULL; wrap = wrap->next) {
        if (wrap->enabled && wrap->pre_cb == pre_func_cb &&
            wrap->post_cb == post_func_cb) {
            res = true;
            break;
        }
    }
    dr_recurlock_unlock(wrap_lock);
    return res;
}

DR_EXPORT
bool
drwrap_is_post_wrap(app_pc pc)
{
    bool res = false;
    if (pc == NULL)
        return false;
    dr_rwlock_read_lock(post_call_rwlock);
    res = (hashtable_lookup(&post_call_table, (void *)pc) != NULL);
    dr_rwlock_read_unlock(post_call_rwlock);
    return res;
}

DR_EXPORT
bool
drwrap_get_stats(INOUT drwrap_stats_t *stats)
{
    if (stats == NULL || stats->size != sizeof(*stats))
        return false;
    stats->flush_count = dr_atomic_load_stat(&drwrap_stats.flush_count);
    return true;
}

DR_EXPORT
void
drwrap_get_retaddr_if_sentinel(void *drcontext, INOUT app_pc *possibly_sentinel)
{
    ASSERT(possibly_sentinel != NULL, "Input cannot be null.");
    if ((app_pc)replace_retaddr_sentinel != *possibly_sentinel)
        return;
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* If we see the sentinel, we must be inside a wrapped function. */
    ASSERT(pt != NULL && pt->wrap_level >= 0, "Invalid drwrap state.");
    *possibly_sentinel = pt->retaddr[pt->wrap_level];
}

/***************************************************************************
 * Several different approaches to try and handle SEH/longjmp unwind
 */

static inline void
drwrap_in_callee_check_unwind(void *drcontext, per_thread_t *pt, dr_mcontext_t *mc)
{
    /* Try to handle an SEH unwind or longjmp that unrolled the stack.
     * The stack may have been extended again since then, and we don't know
     * the high-water point: so even if we're currently further down the
     * stack than any recorded prior call, we verify all entries if we
     * had an exception.
     * XXX: should we verify all the time, to handle any longjmp?
     * But our retaddr check is not bulletproof and might have issues
     * in both directions (though we don't really support wrapping
     * functions that change their retaddrs: still, it's not sufficient
     * due to stale values).
     */
    if (pt->wrap_level >= 0 &&
        pt->app_esp[pt->wrap_level] < mc->xsp IF_WINDOWS(|| pt->hit_exception)) {
        NOTIFY(1, "%s: checking for bypass @ xsp=" PFX "\n", __FUNCTION__, mc->xsp);
        while (
            pt->wrap_level >= 0 &&
            (pt->app_esp[pt->wrap_level] <
             mc->xsp
                 /* if we hit an exception, look for same xsp, b/c longjmp
                  * or handler may be at same level as aborted callee.
                  * we thus give up on correctly handling
                  * an exception in a tailcall sequence: but that's really
                  * hard anyway.
                  */
                 IF_WINDOWS(
                     || (pt->hit_exception && pt->app_esp[pt->wrap_level] <= mc->xsp)))) {
            drwrap_after_callee_func(drcontext, pt, mc, pt->wrap_level, NULL, true,
                                     false);
        }
#ifdef X86
        /* Try to clean up entries we unrolled past and then came back
         * down past in the other direction.  Note that there's a
         * decent chance retaddrs weren't clobbered though so this is
         * not guaranteed.
         */
        while (pt->wrap_level >= 0) {
            app_pc ret;
            if (TEST(DRWRAP_SAFE_READ_RETADDR, global_flags)) {
                if (!fast_safe_read((void *)pt->app_esp[pt->wrap_level], sizeof(ret),
                                    &ret))
                    ret = NULL;
            } else
                ret = *(app_pc *)pt->app_esp[pt->wrap_level];
            if ((pt->wrap_level > 0 && ret == pt->last_wrap_func[pt->wrap_level - 1]) ||
                post_call_lookup(ret))
                break;
            NOTIFY(2, "%s: found clobbered retaddr " PFX "\n", __FUNCTION__, ret);
            drwrap_after_callee_func(drcontext, pt, mc, pt->wrap_level, NULL, true,
                                     false);
        }
#else
        /* XXX i#1673: NYI for ARM */
#endif
        IF_WINDOWS(pt->hit_exception = false;)
    }
}

#ifdef WINDOWS
static bool
drwrap_event_filter_syscall(void *drcontext, int sysnum)
{
    return sysnum == sysnum_NtContinue;
}

static bool
drwrap_event_pre_syscall(void *drcontext, int sysnum)
{
    if (sysnum == sysnum_NtContinue) {
        /* XXX: we assume the syscall will succeed */
        per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
        if (pt->wrap_level >= 0) {
            CONTEXT *cxt = (CONTEXT *)dr_syscall_get_param(drcontext, 0);
            reg_t tgt_xsp = (reg_t)cxt->IF_X64_ELSE(Rsp, Esp);
            dr_mcontext_t mc;
            mc.size = sizeof(mc);
            mc.flags = DR_MC_CONTROL | DR_MC_INTEGER;
            dr_get_mcontext(drcontext, &mc);
            ASSERT(pt != NULL, "pt is NULL in pre-syscall");
            /* Call post-call for every one we're skipping in our target, but
             * pass NULL for wrapcxt to indicate this is not a normal post-call
             */
            NOTIFY(1, "%s: unwinding those we'll bypass @ target xsp=" PFX "\n",
                   __FUNCTION__, tgt_xsp);
            while (pt->wrap_level >= 0 && pt->app_esp[pt->wrap_level] < tgt_xsp) {
                NOTIFY(2, "%s: level %d\n", __FUNCTION__, pt->wrap_level);
                drwrap_after_callee_func(drcontext, pt, &mc, pt->wrap_level, NULL, true,
                                         false);
            }
        }
    }
    return true;
}

bool
drwrap_event_exception(void *drcontext, dr_exception_t *excpt)
{
    per_thread_t *pt = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);
    /* pt can be NULL for a crash at process init before any thread init (i#1219) */
    if (pt != NULL && pt->wrap_level >= 0) {
        int idx;
        /* record whether we should check all the levels in the next hook
         * if using heuristics
         */
        pt->hit_exception = true;
        /* forcibly unwind those that requested it */
        NOTIFY(1, "%s: notifying those who requested\n", __FUNCTION__);
        /* might not decrement pt->wrap_level so we use a for loop */
        for (idx = pt->wrap_level; idx >= 0; idx--) {
            NOTIFY(2, "%s: level %d\n", __FUNCTION__, idx);
            drwrap_after_callee_func(drcontext, pt, excpt->mcontext, idx, NULL, true,
                                     true);
        }
    }
    return true;
}
#endif
