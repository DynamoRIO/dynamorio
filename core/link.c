/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
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
 * link.c - fragment linker routines
 */

#include "globals.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "emit.h"
#include "monitor.h"
#include "perscache.h"
#include "instr.h" /* PC_RELATIVE_TARGET */

/* fragment_t and future_fragment_t are guaranteed to have flags field at same offset,
 * so we use it to find incoming_stubs offset
 */
#define FRAG_INCOMING_ADDR(f)                                        \
    ((common_direct_linkstub_t **)(!TEST(FRAG_IS_FUTURE, f->flags)   \
                                       ? &f->in_xlate.incoming_stubs \
                                       : &((future_fragment_t *)f)->incoming_stubs))

/* forward decl */
static void
coarse_stubs_init(void);

static void
coarse_stubs_free(void);

static fragment_t *
fragment_link_lookup_same_sharing(dcontext_t *dcontext, app_pc tag, linkstub_t *last_exit,
                                  uint flags);

static void
link_new_coarse_grain_fragment(dcontext_t *dcontext, fragment_t *f);

static void
coarse_link_to_fine(dcontext_t *dcontext, cache_pc src_stub, fragment_t *src_f,
                    fragment_t *target_f, bool do_link /*else just add incoming*/);

static coarse_incoming_t *
prepend_new_coarse_incoming(coarse_info_t *info, cache_pc coarse, linkstub_t *fine);

static fragment_t *
fragment_coarse_link_wrapper(dcontext_t *dcontext, app_pc target_tag,
                             cache_pc know_coarse);

static void
set_fake_direct_linkstub(direct_linkstub_t *proxy, app_pc target, cache_pc stub);

/* Removes an incoming entry from a fine fragment to a coarse unit */
static void
coarse_remove_incoming(dcontext_t *dcontext, fragment_t *src_f, linkstub_t *src_l,
                       fragment_t *targetf);

/* We use temporary structs to pass targets to is_linkable().
 * change_linking_lock protects use of these.
 * FIXME: change is_linkable to take in field values directly?
 */
DECLARE_FREQPROT_VAR(static fragment_t temp_targetf, { 0 });
DECLARE_FREQPROT_VAR(static direct_linkstub_t temp_linkstub, { { { 0 } } });

/* This lock makes changes in a shared fragment's link state consistent --
 * the flags indicating whether it is linked and the link state of each of
 * its exits.  Since future fragments are driven by linking, this lock
 * also synchronizes creation and deletion of future fragments.
 * Exported so micro routines can assert whether held.
 */
DECLARE_CXTSWPROT_VAR(recursive_lock_t change_linking_lock,
                      INIT_RECURSIVE_LOCK(change_linking_lock));

/* Special executable heap for separate stubs
 * To avoid wasting capacity space we use a shared heap for all stubs
 */
void *stub_heap;
#if defined(X86) && defined(X64)
void *stub32_heap;
#endif

#if defined(X86) && defined(X64)
#    define SEPARATE_STUB_HEAP(flags) (FRAG_IS_32(flags) ? stub32_heap : stub_heap)
#else
#    define SEPARATE_STUB_HEAP(flags) stub_heap
#endif

/* We save 1 byte per stub by not aligning to 16/24 bytes, since
 * infrequently executed and infrequently accessed (heap free list
 * adds to start so doesn't walk list).
 */
#define SEPARATE_STUB_ALLOC_SIZE(flags) (DIRECT_EXIT_STUB_SIZE(flags)) /* 15x23 */

/* Coarse stubs must be hot-patchable, so we avoid having their last
 * 4 bytes cross cache lines.
 * For x64, the stub is 29 bytes long, so the last 4 bytes are fine
 * for a 16-byte cache line.
 */
#define COARSE_STUB_ALLOC_SIZE(flags) \
    (ALIGN_FORWARD(STUB_COARSE_DIRECT_SIZE(flags), 4)) /* 16x32 */

/* Static linkstubs to give information on special exits from the cache.
 * We make them const to get them in read-only memory even if we have to cast a lot.
 * Our accessor routines do NOT cast since equality tests can use const and they
 * are the most common use.
 * FIXME: we're accumulating a lot of these -- but we don't have room in our flags
 * space to distinguish any other way nicely, so we carry on w/ a bunch of
 * identical static linkstubs.
 */
/* linkstub_fragment() returns a static fragment_t for these fake linkstubs: */
static const fragment_t linkstub_empty_fragment = {
    NULL,
    FRAG_FAKE,
};
#if defined(X86) && defined(X64)
static const fragment_t linkstub_empty_fragment_x86 = {
    NULL,
    FRAG_FAKE | FRAG_32_BIT,
};
#endif
static const linkstub_t linkstub_starting = { LINK_FAKE, 0 };
static const linkstub_t linkstub_reset = { LINK_FAKE, 0 };
static const linkstub_t linkstub_syscall = { LINK_FAKE, 0 };
/* On AArch64 we need to refer to linkstub_selfmod from aarch64.asm. */
#ifndef AARCH64
static
#endif
    const linkstub_t linkstub_selfmod = { LINK_FAKE, 0 };
static const linkstub_t linkstub_ibl_deleted = { LINK_FAKE, 0 };
static const linkstub_t linkstub_asynch = { LINK_FAKE, 0 };
static const linkstub_t linkstub_native_exec = { LINK_FAKE, 0 };
/* this one we give the flag LINK_NI_SYSCALL for executing a syscall in d_r_dispatch() */
static const linkstub_t linkstub_native_exec_syscall = { LINK_FAKE | LINK_NI_SYSCALL, 0 };

#ifdef WINDOWS
/* used for shared_syscall exits */
static const fragment_t linkstub_shared_syscall_trace_fragment = {
    NULL,
    FRAG_FAKE | FRAG_HAS_SYSCALL | FRAG_IS_TRACE,
};
static const fragment_t linkstub_shared_syscall_bb_fragment = {
    NULL,
    FRAG_FAKE | FRAG_HAS_SYSCALL,
};
static const linkstub_t linkstub_shared_syscall_trace = {
    LINK_FAKE | LINK_INDIRECT | LINK_JMP, 0
};
static const linkstub_t linkstub_shared_syscall_bb = {
    LINK_FAKE | LINK_INDIRECT | LINK_JMP, 0
};
/* NOT marked as LINK_INDIRECT|LINK_JMP since we don't bother updating the ibl table
 * on the unlink path */
static const linkstub_t linkstub_shared_syscall_unlinked = { LINK_FAKE, 0 };
#endif

/* A unique fragment_t for use when the details don't matter */
static const fragment_t coarse_fragment = {
    NULL,
    FRAG_FAKE | FRAG_COARSE_GRAIN | FRAG_SHARED |
        /* We mark as linked so is_linkable() won't reject it on any side of a link */
        FRAG_LINKED_OUTGOING | FRAG_LINKED_INCOMING,
};

/* We don't mark as direct since not everything checks for being fake */
static const linkstub_t linkstub_coarse_exit = { LINK_FAKE, 0 };
static const linkstub_t linkstub_coarse_trace_head_exit = { LINK_FAKE, 0 };

#ifdef HOT_PATCHING_INTERFACE
/* used to change control flow in a hot patch routine */
static const linkstub_t linkstub_hot_patch = { LINK_FAKE, 0 };
#endif

/* used for dr_redirect_exection() call to transfer_to_dispatch() */
static const linkstub_t linkstub_client = { LINK_FAKE, 0 };

/* for !DYNAMO_OPTION(indirect_stubs)
 * FIXME: these are used for shared_syscall as well, yet not marked as
 * FRAG_HAS_SYSCALL, but nobody checks for that currently
 */
static const fragment_t linkstub_ibl_trace_fragment = {
    NULL,
    FRAG_FAKE | FRAG_IS_TRACE,
};
static const fragment_t linkstub_ibl_bb_fragment = {
    NULL,
    FRAG_FAKE,
};

static const linkstub_t linkstub_ibl_trace_ret = {
    LINK_FAKE | LINK_INDIRECT | LINK_RETURN, 0
};
static const linkstub_t linkstub_ibl_trace_jmp = { LINK_FAKE | LINK_INDIRECT | LINK_JMP,
                                                   0 };
static const linkstub_t linkstub_ibl_trace_call = { LINK_FAKE | LINK_INDIRECT | LINK_CALL,
                                                    0 };
static const linkstub_t linkstub_ibl_bb_ret = { LINK_FAKE | LINK_INDIRECT | LINK_RETURN,
                                                0 };
static const linkstub_t linkstub_ibl_bb_jmp = { LINK_FAKE | LINK_INDIRECT | LINK_JMP, 0 };
static const linkstub_t linkstub_ibl_bb_call = { LINK_FAKE | LINK_INDIRECT | LINK_CALL,
                                                 0 };
static const linkstub_t linkstub_special_ibl_bb_ret = {
    LINK_FAKE | LINK_INDIRECT | LINK_RETURN, 0
}; /* client_ibl */
static const linkstub_t linkstub_special_ibl_bb_call = {
    LINK_FAKE | LINK_INDIRECT | LINK_CALL, 0
}; /* native_plt_ibl */
static const linkstub_t linkstub_special_ibl_trace_ret = {
    LINK_FAKE | LINK_INDIRECT | LINK_RETURN, 0
}; /* client_ibl */
static const linkstub_t linkstub_special_ibl_trace_call = {
    LINK_FAKE | LINK_INDIRECT | LINK_CALL, 0
}; /* native_plt_ibl */

#ifdef DEBUG
static inline bool
is_empty_fragment(fragment_t *f)
{
    return (f ==
            (fragment_t *)&linkstub_empty_fragment IF_X86_64(
                || f == (fragment_t *)&linkstub_empty_fragment_x86));
}
#endif

#if defined(X86) && defined(X64)
fragment_t *
empty_fragment_mark_x86(fragment_t *f)
{
    ASSERT(f == (fragment_t *)&linkstub_empty_fragment);
    return (fragment_t *)&linkstub_empty_fragment_x86;
}
#endif

/* used to hold important fields for last_exits that are flushed */
typedef struct thread_link_data_t {
    linkstub_t linkstub_deleted;
    fragment_t linkstub_deleted_fragment;
    /* The ordinal is the count from the end.
     * -1 means invalid.
     * The value corresponds to the get_deleted_linkstub() linkstub_t only and
     * may be stale wrt dcontext->last_exit.
     */
    int linkstub_deleted_ordinal;
} thread_link_data_t;

/* thread-shared initialization that should be repeated after a reset */
void
link_reset_init(void)
{
    if (DYNAMO_OPTION(separate_private_stubs) || DYNAMO_OPTION(separate_shared_stubs)) {
        stub_heap = special_heap_init(SEPARATE_STUB_ALLOC_SIZE(0 /*default*/),
                                      true /* must synch */, true /* +x */,
                                      false /* not persistent */);
#if defined(X86) && defined(X64)
        stub32_heap = special_heap_init(SEPARATE_STUB_ALLOC_SIZE(FRAG_32_BIT),
                                        true /* must synch */, true /* +x */,
                                        false /* not persistent */);
#endif
    }
}

/* Free all thread-shared state not critical to forward progress;
 * link_reset_init() will be called before continuing.
 */
void
link_reset_free(void)
{
    if (DYNAMO_OPTION(separate_private_stubs) || DYNAMO_OPTION(separate_shared_stubs)) {
        special_heap_exit(stub_heap);
#if defined(X86) && defined(X64)
        special_heap_exit(stub32_heap);
#endif
    }
}

void
d_r_link_init()
{
    link_reset_init();
    coarse_stubs_init();
}

void
d_r_link_exit()
{
    coarse_stubs_free();
    link_reset_free();
    DELETE_RECURSIVE_LOCK(change_linking_lock);
}

void
link_thread_init(dcontext_t *dcontext)
{
    thread_link_data_t *ldata =
        HEAP_TYPE_ALLOC(dcontext, thread_link_data_t, ACCT_OTHER, PROTECTED);
    dcontext->link_field = (void *)ldata;
    memset(&ldata->linkstub_deleted, 0, sizeof(ldata->linkstub_deleted));
    memset(&ldata->linkstub_deleted_fragment, 0,
           sizeof(ldata->linkstub_deleted_fragment));
    ldata->linkstub_deleted_ordinal = -1;
    /* Mark as fake */
    ldata->linkstub_deleted_fragment.flags = FRAG_FAKE;
    ldata->linkstub_deleted.flags = LINK_FAKE;
}

void
link_thread_exit(dcontext_t *dcontext)
{
    thread_link_data_t *ldata = (thread_link_data_t *)dcontext->link_field;
    HEAP_TYPE_FREE(dcontext, ldata, thread_link_data_t, ACCT_OTHER, PROTECTED);
}

/* Initializes an array of linkstubs beginning with first */
void
linkstubs_init(linkstub_t *first, int num_direct, int num_indirect, fragment_t *f)
{
    /* we don't know the sequencing of direct and indirect so all we do
     * here is zero everything out
     */
    uint size = linkstubs_heap_size(f->flags, num_direct, num_indirect);
    ASSERT(num_direct + num_indirect > 0);
    memset(first, 0, size);
    /* place the offset to the fragment_t*, if necessary */
    if (linkstub_frag_offs_at_end(f->flags, num_direct, num_indirect)) {
        ushort offs;
        /* size includes post_linkstub_t so we have to subtract it off */
        post_linkstub_t *post =
            (post_linkstub_t *)(((byte *)first) + size - sizeof(post_linkstub_t));
        ASSERT_TRUNCATE(offs, ushort, (((byte *)post) - ((byte *)f)));
        offs = (ushort)(((byte *)post) - ((byte *)f));
        post->fragment_offset = offs;
    }
}

uint
linkstub_size(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    /* FIXME: optimization: now for ind exits we compute a pc from
     * flags and then exit_stub_size looks up the pc to find whether
     * indirect -- should pass that flag.
     * There are a number of uses of this idiom in emit_utils.c as well.
     */
    return exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l), f->flags);
}

/* we don't want to propagate our cbr hack during trace building, etc. */
uint
linkstub_propagatable_flags(uint flags)
{
    if (LINKSTUB_CBR_FALLTHROUGH(flags))
        flags &= ~LINK_INDIRECT;
    return flags;
}

/* is a post_linkstub_t structure required to store the fragment_t offset? */
bool
linkstub_frag_offs_at_end(uint flags, int direct_exits, int indirect_exits)
{
    ASSERT((direct_exits + indirect_exits > 0) || TEST(FRAG_COARSE_GRAIN, flags));
    /* common bb types do not have an offset at the end:
     * 1) single indirect exit
     * 2) two direct exits
     * 3) coarse-grain, which of course have no fragment_t
     * see linkstub_fragment() for how we find their owning fragment_t's.
     * Since we have to assume sizeof(fragment_t) we can only allow
     * shared fragments to not have an offset, unless we add a LINK_SHARED
     * flag, which we can do if we end up having a product config w/ private bbs.
     * FIXME: make the sizeof calculation dynamic such that the dominant
     * type of bb fragment is the one w/o the post_linkstub_t.
     */
    return !(!TEST(FRAG_IS_TRACE, flags) && TEST(FRAG_SHARED, flags) &&
             /* We can't tell from the linkstub_t whether there is a
              * translation field.  FIXME: we could avoid this problem
              * by storing the translation field after the linkstubs.
              */
             !TEST(FRAG_HAS_TRANSLATION_INFO, flags) &&
             ((direct_exits == 2 && indirect_exits == 0) ||
              (direct_exits == 0 && indirect_exits == 1))) &&
        !TEST(FRAG_COARSE_GRAIN, flags);
}

bool
use_cbr_fallthrough_short(uint flags, int direct_exits, int indirect_exits)
{
    ASSERT((direct_exits + indirect_exits > 0) || TEST(FRAG_COARSE_GRAIN, flags));
    if (direct_exits != 2 || indirect_exits != 0)
        return false;
    /* cannot handle instrs inserted between cbr and fall-through jmp */
    return false;
}

/* includes the post_linkstub_t offset struct size */
uint
linkstubs_heap_size(uint flags, int direct_exits, int indirect_exits)
{
    uint offset_sz, linkstub_size;
    ASSERT((direct_exits + indirect_exits > 0) || TEST(FRAG_COARSE_GRAIN, flags));
    if (use_cbr_fallthrough_short(flags, direct_exits, indirect_exits)) {
        linkstub_size = sizeof(direct_linkstub_t) + sizeof(cbr_fallthrough_linkstub_t);
    } else {
        linkstub_size = direct_exits * sizeof(direct_linkstub_t) +
            indirect_exits * sizeof(indirect_linkstub_t);
    }
    if (linkstub_frag_offs_at_end(flags, direct_exits, indirect_exits))
        offset_sz = sizeof(post_linkstub_t);
    else
        offset_sz = 0;
    return linkstub_size + offset_sz;
}

/* Locate the owning fragment_t for a linkstub_t */
fragment_t *
linkstub_fragment(dcontext_t *dcontext, linkstub_t *l)
{
    if (LINKSTUB_FAKE(l)) {
#ifdef WINDOWS
        if (l == &linkstub_shared_syscall_trace)
            return (fragment_t *)&linkstub_shared_syscall_trace_fragment;
        else if (l == &linkstub_shared_syscall_bb)
            return (fragment_t *)&linkstub_shared_syscall_bb_fragment;
#endif
        if (l == &linkstub_ibl_trace_ret || l == &linkstub_ibl_trace_jmp ||
            l == &linkstub_ibl_trace_call)
            return (fragment_t *)&linkstub_ibl_trace_fragment;
        else if (l == &linkstub_ibl_bb_ret || l == &linkstub_ibl_bb_jmp ||
                 l == &linkstub_ibl_bb_call)
            return (fragment_t *)&linkstub_ibl_bb_fragment;
        if (dcontext != NULL && dcontext != GLOBAL_DCONTEXT) {
            thread_link_data_t *ldata = (thread_link_data_t *)dcontext->link_field;
            /* This point is reachable (via set_last_exit) from initialize_dynamo_context,
             * which is called by dynamo_thread_init before link_thread_init. The latter
             * initializes dcontext->link_field, so it's possible for ldata to be NULL.
             */
            if (ldata != NULL && l == &ldata->linkstub_deleted)
                return &ldata->linkstub_deleted_fragment;
        }
        /* For coarse proxies, we need a fake FRAG_SHARED fragment_t for is_linkable */
        if (LINKSTUB_COARSE_PROXY(l->flags))
            return (fragment_t *)&coarse_fragment;
        return (fragment_t *)&linkstub_empty_fragment;
    }
    /* To save space we no longer store a backpointer in the linkstub_t.
     * Instead we use several schemes based on the type of owning
     * fragment_t.  We could walk backward but that gets complicated with
     * hacks to distinguish types of structs by their final fields w/o
     * adding secondary flags fields.
     */
    if (TEST(LINK_FRAG_OFFS_AT_END, l->flags)) {
        /* For traces and unusual bbs, we store an offset to the
         * fragment_t at the end of the linkstub_t list.
         */
        post_linkstub_t *post;
        for (; !LINKSTUB_FINAL(l); l = LINKSTUB_NEXT_EXIT(l))
            ; /* nothing */
        ASSERT(l != NULL && TEST(LINK_END_OF_LIST, l->flags));
        post = (post_linkstub_t *)(((byte *)l) + LINKSTUB_SIZE(l));
        return (fragment_t *)(((byte *)post) - post->fragment_offset);
    } else {
        /* Otherwise, we assume this is one of 2 types of basic block! */
        if (LINKSTUB_INDIRECT(l->flags)) {
            /* Option 1: a single indirect exit */
            ASSERT(TEST(LINK_END_OF_LIST, l->flags));
            return (fragment_t *)(((byte *)l) - sizeof(fragment_t));
        } else {
            ASSERT(LINKSTUB_DIRECT(l->flags));
            /* Option 2: two direct exits (doesn't matter if 2nd uses
             * cbr_fallthrough_linkstub_t or not)
             * (single direct exit is very rare but if later becomes common
             * could add a LINK_ flag to distinguish)
             */
            if (TEST(LINK_END_OF_LIST, l->flags)) {
                return (fragment_t *)(((byte *)l) - sizeof(direct_linkstub_t) -
                                      sizeof(fragment_t));
            } else {
                return (fragment_t *)(((byte *)l) - sizeof(fragment_t));
            }
        }
    }
    ASSERT_NOT_REACHED();
    return NULL;
}

#ifdef DEBUG
bool
linkstub_owned_by_fragment(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    linkstub_t *ls;
    /* handle fake linkstubs first since FRAGMENT_EXIT_STUBS shouldn't be called
     * on fake fragments!
     */
    if (LINKSTUB_FAKE(l)) {
        ASSERT(TEST(FRAG_FAKE, f->flags) ||
               /* during emit, coarse-grain has real fragment_t but sometimes fake
                * linkstubs during linking
                */
               TEST(FRAG_COARSE_GRAIN, f->flags));
        /* Coarse-grain temp fragment_t's also have temp fake linkstub_t's */
        if (TEST(FRAG_COARSE_GRAIN, f->flags))
            return true;
        else {
            return (
                linkstub_fragment(dcontext, l) ==
                f
                    /* For reset exit stub we need a fake empty fragment marked as x86 */
                    IF_X86_64(||
                              (is_empty_fragment(linkstub_fragment(dcontext, l)) &&
                               f == (fragment_t *)&linkstub_empty_fragment_x86)));
        }
    }
    for (ls = FRAGMENT_EXIT_STUBS(f); ls != NULL; ls = LINKSTUB_NEXT_EXIT(ls)) {
        if (ls == l)
            return true;
    }
    return false;
}
#endif

/* N.B.: all the actions of this routine are mirrored in insert_set_last_exit(),
 * so any changes here should be mirrored there.
 */
void
set_last_exit(dcontext_t *dcontext, linkstub_t *l)
{
    /* We try to set last_fragment every time we set last_exit, rather than
     * leaving it as a dangling pointer until the next d_r_dispatch entrance,
     * to avoid cases like bug 7534.  However, fcache_return only sets
     * last_exit, though it hits the d_r_dispatch point that sets last_fragment
     * soon after, and everyone who frees fragments that other threads can
     * see should call last_exit_deleted() on the other threads.
     */
    ASSERT(l != NULL);
    dcontext->last_exit = l;
    dcontext->last_fragment = linkstub_fragment(dcontext, dcontext->last_exit);
    ASSERT(dcontext->last_fragment != NULL);
    /* We also cannot leave dir_exit as a dangling pointer */
    dcontext->coarse_exit.dir_exit = NULL;
}

void
last_exit_deleted(dcontext_t *dcontext)
{
    thread_link_data_t *ldata = (thread_link_data_t *)dcontext->link_field;
    linkstub_t *l;
    /* if this gets called twice, second is a nop
     * FIXME: measure how often, reduce dup calls
     */
    if (LINKSTUB_FAKE(dcontext->last_exit) &&
        /* be defensive (remember case 7534!): re-do in case someone
         * sets last_exit to FAKE but leaves last_fragment dangling
         */
        dcontext->last_fragment == &ldata->linkstub_deleted_fragment)
        return;
    ASSERT(LINKSTUB_FAKE(&ldata->linkstub_deleted));
    ASSERT(linkstub_fragment(dcontext, &ldata->linkstub_deleted) ==
           &ldata->linkstub_deleted_fragment);
    DOCHECK(1, {
        /* easier to clear these and ensure whole thing is 0 for the fragment */
        ldata->linkstub_deleted_fragment.tag = 0;
        ldata->linkstub_deleted_fragment.flags = 0;
        ldata->linkstub_deleted_fragment.id = 0;
        ASSERT(is_region_memset_to_char((byte *)&ldata->linkstub_deleted_fragment,
                                        sizeof(ldata->linkstub_deleted_fragment), 0));
    });
    /* FIME: should we have special dcontext fields last_exit_flags &
     * last_fragment_{flags,tag} and make everyone use those to ensure
     * nobody treats these as the real thing?
     * But trace building and some others need the real thing, so would
     * have to check whether valid anyway, so just as costly to check
     * whether these are valid.  FRAG_FAKE and LINK_FAKE tags help us out here.
     */
    ldata->linkstub_deleted.flags = dcontext->last_exit->flags;
    ldata->linkstub_deleted.flags |= LINK_FAKE;
    ldata->linkstub_deleted_fragment.tag = dcontext->last_fragment->tag;
    ldata->linkstub_deleted_fragment.flags |= FRAG_FAKE;
#ifdef DEBUG_MEMORY
    ASSERT(dcontext->last_fragment->flags != HEAP_UNALLOCATED_UINT);
    ASSERT(dcontext->last_fragment->id != HEAP_UNALLOCATED_UINT);
#endif
    ldata->linkstub_deleted_fragment.flags = dcontext->last_fragment->flags;
    DODEBUG({ ldata->linkstub_deleted_fragment.id = dcontext->last_fragment->id; });

    /* Store which exit this is, for trace building.
     * Our ordinal is the count from the end.
     */
    if (TEST(FRAG_FAKE, dcontext->last_fragment->flags))
        ldata->linkstub_deleted_ordinal = -1; /* invalid */
    else {
        ldata->linkstub_deleted_ordinal = 0;
        for (l = FRAGMENT_EXIT_STUBS(dcontext->last_fragment);
             l != NULL && l != dcontext->last_exit; l = LINKSTUB_NEXT_EXIT(l)) {
            /* nothing */
        }
        if (l == dcontext->last_exit) {
            /* 0 means the last one */
            for (l = LINKSTUB_NEXT_EXIT(l); l != NULL; l = LINKSTUB_NEXT_EXIT(l))
                ldata->linkstub_deleted_ordinal++;
        } else {
            ASSERT_NOT_REACHED();
            ldata->linkstub_deleted_ordinal = -1; /* invalid */
        }
    }

    /* now install the copy as the last exit, but don't overwrite an existing
     * fake exit (like a native_exec exit: case 8033)
     */
    if (!LINKSTUB_FAKE(dcontext->last_exit))
        dcontext->last_exit = &ldata->linkstub_deleted;
    dcontext->last_fragment = &ldata->linkstub_deleted_fragment;

    /* We also cannot leave dir_exit as a dangling pointer */
    dcontext->coarse_exit.dir_exit = NULL;
}

static inline bool
is_special_ibl_linkstub(const linkstub_t *l)
{
    if (l == &linkstub_special_ibl_trace_ret || l == &linkstub_special_ibl_trace_call ||
        l == &linkstub_special_ibl_bb_ret || l == &linkstub_special_ibl_bb_call)
        return true;
    return false;
}

void
set_coarse_ibl_exit(dcontext_t *dcontext)
{
    thread_link_data_t *ldata = (thread_link_data_t *)dcontext->link_field;
    app_pc src_tag;

    /* Special ibl is incompatible with knowing the source tag (so can't use
     * dr_redirect_native_target() with PROGRAM_SHEPHERDING).
     */
    if (is_special_ibl_linkstub((const linkstub_t *)dcontext->last_exit))
        return;

    src_tag = dcontext->coarse_exit.src_tag;
    ASSERT(src_tag != NULL);

    if (!DYNAMO_OPTION(coarse_units) ||
        !is_ibl_sourceless_linkstub((const linkstub_t *)dcontext->last_exit))
        return;

    /* We use the sourceless linkstubs for the ibl, but we do have source info
     * in dcontext->coarse_exit.src_tag, so once back in DR we switch to our
     * deleted structs to fill in the src info
     */
    /* Re-use the deletion routine to fill in the fields for us.
     * It won't replace last_exit itself as it's already fake, but it will
     * replace last_fragment.  FIXME: now we have last_fragment != result of
     * linkstub_fragment()!
     */
    last_exit_deleted(dcontext);
    ldata->linkstub_deleted_fragment.tag = src_tag;
}

/* The ordinal is the count from the end.
 * -1 means invalid.
 * The value corresponds to the get_deleted_linkstub() linkstub_t only and
 * may be stale wrt dcontext->last_exit.
 */
int
get_last_linkstub_ordinal(dcontext_t *dcontext)
{
    thread_link_data_t *ldata;
    ASSERT(dcontext != GLOBAL_DCONTEXT);
    ldata = (thread_link_data_t *)dcontext->link_field;
    return ldata->linkstub_deleted_ordinal;
}

linkstub_t *
get_deleted_linkstub(dcontext_t *dcontext)
{
    thread_link_data_t *ldata;
    ASSERT(dcontext != GLOBAL_DCONTEXT);
    ldata = (thread_link_data_t *)dcontext->link_field;
    return &ldata->linkstub_deleted;
}

const linkstub_t *
get_starting_linkstub()
{
    return &linkstub_starting;
}

const linkstub_t *
get_reset_linkstub()
{
    return &linkstub_reset;
}

const linkstub_t *
get_syscall_linkstub()
{
    return &linkstub_syscall;
}

const linkstub_t *
get_selfmod_linkstub()
{
    return &linkstub_selfmod;
}

const linkstub_t *
get_ibl_deleted_linkstub()
{
    return &linkstub_ibl_deleted;
}

const linkstub_t *
get_asynch_linkstub()
{
    return &linkstub_asynch;
}

const linkstub_t *
get_native_exec_linkstub()
{
    return &linkstub_native_exec;
}

const linkstub_t *
get_native_exec_syscall_linkstub()
{
    return &linkstub_native_exec_syscall;
}

#ifdef WINDOWS
const linkstub_t *
get_shared_syscalls_unlinked_linkstub()
{
    return &linkstub_shared_syscall_unlinked;
}

const linkstub_t *
get_shared_syscalls_trace_linkstub()
{
    return &linkstub_shared_syscall_trace;
}

const linkstub_t *
get_shared_syscalls_bb_linkstub()
{
    return &linkstub_shared_syscall_bb;
}
#endif /* WINDOWS */

#ifdef HOT_PATCHING_INTERFACE
const linkstub_t *
get_hot_patch_linkstub()
{
    return &linkstub_hot_patch;
}
#endif

const linkstub_t *
get_client_linkstub()
{
    return &linkstub_client;
}

bool
is_ibl_sourceless_linkstub(const linkstub_t *l)
{
    return (l == &linkstub_ibl_trace_ret || l == &linkstub_ibl_trace_jmp ||
            l == &linkstub_ibl_trace_call || l == &linkstub_ibl_bb_ret ||
            l == &linkstub_ibl_bb_jmp || l == &linkstub_ibl_bb_call ||
            is_special_ibl_linkstub(l));
}

const linkstub_t *
get_ibl_sourceless_linkstub(uint link_flags, uint frag_flags)
{
    if (TEST(FRAG_IS_TRACE, frag_flags)) {
        if (TEST(LINK_RETURN, link_flags))
            return &linkstub_ibl_trace_ret;
        if (EXIT_IS_JMP(link_flags))
            return &linkstub_ibl_trace_jmp;
        if (EXIT_IS_CALL(link_flags))
            return &linkstub_ibl_trace_call;
    } else {
        if (TEST(LINK_RETURN, link_flags))
            return &linkstub_ibl_bb_ret;
        if (EXIT_IS_JMP(link_flags))
            return &linkstub_ibl_bb_jmp;
        if (EXIT_IS_CALL(link_flags))
            return &linkstub_ibl_bb_call;
    }
    ASSERT_NOT_REACHED();
    return NULL;
}

const linkstub_t *
get_special_ibl_linkstub(ibl_branch_type_t ibl_type, bool is_trace)
{
    switch (ibl_type) {
    case IBL_RETURN:
        return (is_trace ? &linkstub_special_ibl_trace_ret
                         : &linkstub_special_ibl_bb_ret);
    case IBL_INDCALL:
        return (is_trace ? &linkstub_special_ibl_trace_call
                         : &linkstub_special_ibl_bb_call);
    default:
        /* we only have ret/call for client_ibl and native_plt_ibl */
        ASSERT_NOT_REACHED();
    }
    return NULL;
}

/* Direct exit not targeting a trace head */
const linkstub_t *
get_coarse_exit_linkstub()
{
    return &linkstub_coarse_exit;
}

/* Direct exit targeting a trace head */
const linkstub_t *
get_coarse_trace_head_exit_linkstub()
{
    return &linkstub_coarse_trace_head_exit;
}

bool
should_separate_stub(dcontext_t *dcontext, app_pc target, uint fragment_flags)
{
    return (local_exit_stub_size(dcontext, target, fragment_flags) == 0);
}

int
local_exit_stub_size(dcontext_t *dcontext, app_pc target, uint fragment_flags)
{
    /* linking shared separate stubs is not yet atomic so we only support
     * separate private stubs
     * FIXME: optimization: some callers have a linkstub_t so we could provide
     * a separate routine for that to avoid the now-costly computation of
     * target for indirect exits.
     */
    int sz = exit_stub_size(dcontext, target, fragment_flags);
    if (((DYNAMO_OPTION(separate_private_stubs) &&
          !TEST(FRAG_COARSE_GRAIN, fragment_flags) &&
          !TEST(FRAG_SHARED, fragment_flags)) ||
         (DYNAMO_OPTION(separate_shared_stubs) &&
          !TEST(FRAG_COARSE_GRAIN, fragment_flags) &&
          TEST(FRAG_SHARED, fragment_flags)) ||
         /* entrance stubs are always separated */
         (TEST(FRAG_COARSE_GRAIN, fragment_flags)
          /* FIXME: for now we inline ind stubs but eventually we want to separate.
           * We need this check only for coarse since its stubs are the same size
           * as the direct stubs.
           */
          && !is_indirect_branch_lookup_routine(dcontext, (cache_pc)target))) &&
        /* we only separate stubs of the regular type, which we determine
         * by letting exit_stub_size dispatch on flags and return its
         * results in the stub size
         */
        sz ==
            (TEST(FRAG_COARSE_GRAIN, fragment_flags)
                 ? STUB_COARSE_DIRECT_SIZE(fragment_flags)
                 : DIRECT_EXIT_STUB_SIZE(fragment_flags)))
        return 0;
    else
        return sz;
}

static inline bool
is_cbr_of_cbr_fallthrough(linkstub_t *l)
{
    linkstub_t *nxt = LINKSTUB_NEXT_EXIT(l);
    bool yes = (nxt != NULL && LINKSTUB_CBR_FALLTHROUGH(nxt->flags));
    ASSERT(!yes || LINKSTUB_FINAL(nxt));
    return yes;
}

void
separate_stub_create(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    cache_pc stub_pc;
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(DYNAMO_OPTION(separate_private_stubs) || DYNAMO_OPTION(separate_shared_stubs));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(TEST(LINK_SEPARATE_STUB, l->flags));
    if (LINKSTUB_CBR_FALLTHROUGH(l->flags)) {
        /* there is no field for the fallthrough of a short cbr -- it assumes
         * the cbr and fallthrough stubs are contiguous, and calculates its
         * stub pc from the cbr stub pc, which we assume here has already
         * been created since we create them in forward order
         */
        stub_pc = EXIT_STUB_PC(dcontext, f, l);
    } else {
        direct_linkstub_t *dl = (direct_linkstub_t *)l;
        ASSERT(LINKSTUB_NORMAL_DIRECT(l->flags));
        ASSERT(dl->stub_pc == NULL);
        /* if -cbr_single_stub, LINKSTUB_CBR_FALLTHROUGH _always_ shares a stub,
         * as its requirements are a superset of cbr stub sharing, so we don't
         * need a separate flag.  (if we do need one, we could use the 00 combo
         * for both cbr linkstubs (except if 2nd is cbr-fallthrough, but then
         * just -cbr_single_stub is enough), testing !FAKE.)
         */
        if (is_cbr_of_cbr_fallthrough(l) && !INTERNAL_OPTION(cbr_single_stub)) {
            /* we have to allocate a pair together */
            dl->stub_pc = (cache_pc)special_heap_calloc(SEPARATE_STUB_HEAP(f->flags), 2);
        } else
            dl->stub_pc = (cache_pc)special_heap_alloc(SEPARATE_STUB_HEAP(f->flags));
        ASSERT(dl->stub_pc == EXIT_STUB_PC(dcontext, f, l));
        stub_pc = dl->stub_pc;
    }
    DEBUG_DECLARE(int emit_sz =) insert_exit_stub(dcontext, f, l, stub_pc);
    ASSERT(emit_sz <= SEPARATE_STUB_ALLOC_SIZE(f->flags));
    DOSTATS({
        size_t alloc_size = SEPARATE_STUB_ALLOC_SIZE(f->flags);
        STATS_INC(num_separate_stubs);
        if (TEST(FRAG_SHARED, f->flags)) {
            if (TEST(FRAG_IS_TRACE, f->flags))
                STATS_ADD(separate_shared_trace_direct_stubs, alloc_size);
            else
                STATS_ADD(separate_shared_bb_direct_stubs, alloc_size);
        } else if (TEST(FRAG_IS_TRACE, f->flags))
            STATS_ADD(separate_trace_direct_stubs, alloc_size);
        else
            STATS_ADD(separate_bb_direct_stubs, alloc_size);
    });
}

/* deletion says are we freeing b/c we're freeing the whole fragment,
 * or just freeing b/c we're linking and don't need this stub anymore?
 */
static void
separate_stub_free(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, bool deletion)
{
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(DYNAMO_OPTION(separate_private_stubs) || DYNAMO_OPTION(separate_shared_stubs));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(TEST(LINK_SEPARATE_STUB, l->flags));
    ASSERT(exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l), f->flags) <=
           SEPARATE_STUB_ALLOC_SIZE(f->flags));
    if (LINKSTUB_CBR_FALLTHROUGH(l->flags)) {
        ASSERT(deletion);
        /* was already freed by forward walk hitting 1st exit */
    } else {
        direct_linkstub_t *dl = (direct_linkstub_t *)l;
        ASSERT(LINKSTUB_NORMAL_DIRECT(l->flags));
        /* for -cbr_single_stub, non-deletion-freeing is disallowed, and
         * for deletion freeing, up to caller to not call us twice.
         * FIXME: we could support freeing when both stubs are linked if
         * we either added an identifying flag or re-calculated whether should
         * share (won't be able to use stub_pc equality anymore if can be NULL)
         */
        ASSERT(dl->stub_pc == EXIT_STUB_PC(dcontext, f, l));
        ASSERT(dl->stub_pc != NULL);
        if (is_cbr_of_cbr_fallthrough(l) && !INTERNAL_OPTION(cbr_single_stub)) {
            /* we allocated a pair */
            special_heap_cfree(SEPARATE_STUB_HEAP(f->flags), dl->stub_pc, 2);
        } else {
            special_heap_free(SEPARATE_STUB_HEAP(f->flags), dl->stub_pc);
        }
        dl->stub_pc = NULL;
    }
    DOSTATS({
        size_t alloc_size = SEPARATE_STUB_ALLOC_SIZE(f->flags);
        if (TEST(FRAG_SHARED, f->flags)) {
            if (TEST(FRAG_IS_TRACE, f->flags)) {
                STATS_ADD(separate_shared_trace_direct_stubs, -(int)alloc_size);
            } else {
                STATS_ADD(separate_shared_bb_direct_stubs, -(int)alloc_size);
            }
        } else if (TEST(FRAG_IS_TRACE, f->flags))
            STATS_SUB(separate_trace_direct_stubs, alloc_size);
        else
            STATS_SUB(separate_bb_direct_stubs, alloc_size);
    });
}

/* if this linkstub shares the stub with the next linkstub, returns the
 * next linkstub; else returns NULL
 */
linkstub_t *
linkstub_shares_next_stub(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    linkstub_t *nxt;
    if (!INTERNAL_OPTION(cbr_single_stub))
        return NULL;
    nxt = LINKSTUB_NEXT_EXIT(l);
    if (nxt != NULL &&
        /* avoid stub computation for indirect, which fails if fcache is freed */
        LINKSTUB_DIRECT(nxt->flags) && LINKSTUB_DIRECT(l->flags) &&
        EXIT_STUB_PC(dcontext, f, nxt) == EXIT_STUB_PC(dcontext, f, l)) {
        ASSERT(LINKSTUB_FINAL(nxt));
        return nxt;
    } else
        return NULL;
}

void
linkstub_free_exitstubs(dcontext_t *dcontext, fragment_t *f)
{
    linkstub_t *l;
    for (l = FRAGMENT_EXIT_STUBS(f); l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        if (TEST(LINK_SEPARATE_STUB, l->flags) && EXIT_STUB_PC(dcontext, f, l) != NULL) {
            linkstub_t *nxt = linkstub_shares_next_stub(dcontext, f, l);
            if (nxt != NULL && !LINKSTUB_CBR_FALLTHROUGH(nxt->flags)) {
                /* next linkstub shares our stub, so clear his now to avoid
                 * double free
                 */
                direct_linkstub_t *dl = (direct_linkstub_t *)nxt;
                dl->stub_pc = NULL;
            }
            separate_stub_free(dcontext, f, l, true);
        }
    }
}

void
linkstubs_shift(dcontext_t *dcontext, fragment_t *f, ssize_t shift)
{
    linkstub_t *l;
    for (l = FRAGMENT_EXIT_STUBS(f); l != NULL; l = LINKSTUB_NEXT_EXIT(l)) {
        /* don't need to shift separated b/c shift_ctis_in_fragment will detect
         * as an out-of-cache target and shift for us.
         * l->stub_pc does not change since it's an absolute pc pointing outside
         * of the resized cache.
         * we also don't need to shift indirect stubs as they do not store
         * an absolute pointer to their stub pc.
         */
        if (!TEST(LINK_SEPARATE_STUB, l->flags) && LINKSTUB_NORMAL_DIRECT(l->flags)) {
            direct_linkstub_t *dl = (direct_linkstub_t *)l;
            dl->stub_pc += shift;
        } /* else, no change */
    }
}

/* returns true if the exit l can be linked to the fragment f
 * if mark_new_trace_head is false, this routine does not change any state.
 */
bool
is_linkable(dcontext_t *dcontext, fragment_t *from_f, linkstub_t *from_l,
            fragment_t *to_f, bool have_link_lock, bool mark_new_trace_head)
{
    /* monitor_is_linkable is what marks trace heads, so must
     * call it no matter the result
     */
    if (!monitor_is_linkable(dcontext, from_f, from_l, to_f, have_link_lock,
                             mark_new_trace_head))
        return false;
    /* cannot link between shared and private caches
     * N.B.: we assume this in other places, like our use of
     * fragment_lookup_same_sharing()
     * for linking, so if we change this we need to change more than this routine here
     */
    if ((from_f->flags & FRAG_SHARED) != (to_f->flags & FRAG_SHARED))
        return false;
#ifdef DGC_DIAGNOSTICS
    /* restrict linking so we can study entry/exit behavior */
    if ((from_f->flags & FRAG_DYNGEN) != (to_f->flags & FRAG_DYNGEN))
        return false;
#endif
    /* do not link exit from non-ignorable syscall ending a frag */
    if (TESTANY(LINK_NI_SYSCALL_ALL, from_l->flags))
        return false;
#ifdef WINDOWS
    if (TEST(LINK_CALLBACK_RETURN, from_l->flags))
        return false;
#endif
    /* never link a selfmod or any other unlinkable exit branch */
    if (TEST(LINK_SPECIAL_EXIT, from_l->flags))
        return false;
    /* don't link from a non-outgoing-linked fragment, or to a
     * non-incoming-linked fragment, except for self-loops
     */
    if ((!TEST(FRAG_LINKED_OUTGOING, from_f->flags) ||
         !TEST(FRAG_LINKED_INCOMING, to_f->flags)) &&
        from_f != to_f)
        return false;
    /* rarely set so we test it last */
    if (INTERNAL_OPTION(nolink))
        return false;
#if defined(UNIX) && !defined(DGC_DIAGNOSTICS)
    /* i#107, fragment having a OP_mov_seg instr cannot be linked. */
    if (TEST(FRAG_HAS_MOV_SEG, to_f->flags))
        return false;
#endif
    return true;
}

/* links the branch at EXIT_CTI_PC(f, l) to jump directly to the entry point of
 * fragment targetf
 * Assumes that is_linkable has already been called!
 * does not modify targetf's incoming branch list (because that should be done
 * once while linking and unlinking may happen multiple times)
 */
static void
link_branch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, fragment_t *targetf,
            bool hot_patch)
{
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    /* ASSUMPTION: always unlink before linking to somewhere else, so nop if linked */
    if (INTERNAL_OPTION(nolink) || TEST(LINK_LINKED, l->flags))
        return;
    if (LINKSTUB_DIRECT(l->flags)) {
        LOG(THREAD, LOG_LINKS, 4,
            "    linking F%d(" PFX ")." PFX " -> F%d(" PFX ")=" PFX "\n", f->id, f->tag,
            EXIT_CTI_PC(f, l), targetf->id, targetf->tag, FCACHE_ENTRY_PC(targetf));
#ifdef TRACE_HEAD_CACHE_INCR
        {
            common_direct_linkstub_t *cdl = (common_direct_linkstub_t *)l;
            if (TEST(FRAG_IS_TRACE_HEAD, targetf->flags))
                cdl->target_fragment = targetf;
            else
                cdl->target_fragment = NULL;
        }
#endif
        if (LINKSTUB_COARSE_PROXY(l->flags)) {
            link_entrance_stub(dcontext, EXIT_STUB_PC(dcontext, f, l),
                               FCACHE_ENTRY_PC(targetf), HOT_PATCHABLE, NULL);
        } else {
            bool do_not_need_stub =
                link_direct_exit(dcontext, f, l, targetf, hot_patch) &&
                TEST(LINK_SEPARATE_STUB, l->flags) &&
                ((DYNAMO_OPTION(free_private_stubs) && !TEST(FRAG_SHARED, f->flags)) ||
                 (DYNAMO_OPTION(unsafe_free_shared_stubs) &&
                  TEST(FRAG_SHARED, f->flags)));
            ASSERT(!TEST(FRAG_FAKE, f->flags) && !LINKSTUB_FAKE(l));
            if (do_not_need_stub)
                separate_stub_free(dcontext, f, l, false);
        }
    } else if (LINKSTUB_INDIRECT(l->flags)) {
        if (INTERNAL_OPTION(link_ibl))
            link_indirect_exit(dcontext, f, l, hot_patch);
    } else
        ASSERT_NOT_REACHED();

    l->flags |= LINK_LINKED;
}

/* Unlinks one linked branch.  Returns false if the linkstub_t should be removed
 * from the target's incoming list; else returns true.
 */
static bool
unlink_branch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    bool keep = true;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    if ((l->flags & LINK_LINKED) == 0)
        return keep;
    if (LINKSTUB_DIRECT(l->flags)) {
        LOG(THREAD, LOG_LINKS, 4, "    unlinking branch F%d." PFX "\n", f->id,
            EXIT_CTI_PC(f, l));
        if (LINKSTUB_COARSE_PROXY(l->flags)) {
            uint flags = 0;
            cache_pc stub = EXIT_STUB_PC(dcontext, f, l);
            /* When we create a trace from a coarse head, we point the head's
             * entrance stub at the trace.  If we later remove the trace we need
             * to re-instate the link to the head (well, to the head inc
             * routine).
             */
            if (coarse_is_trace_head_in_own_unit(dcontext, f->tag, stub, NULL, false,
                                                 NULL))
                flags = FRAG_IS_TRACE_HEAD;
            unlink_entrance_stub(dcontext, stub, flags, NULL);
            /* The caller is now supposed to remove the incoming entry and
             * free the heap space for this proxy linkstub_t
             */
            keep = false;
        } else {
            ASSERT(!LINKSTUB_FAKE(l));
            /* stub may already exist for TRACE_HEAD_CACHE_INCR */
            if (EXIT_STUB_PC(dcontext, f, l) == NULL &&
                TEST(LINK_SEPARATE_STUB, l->flags))
                separate_stub_create(dcontext, f, l);
            unlink_direct_exit(dcontext, f, l);
        }
    } else if (LINKSTUB_INDIRECT(l->flags)) {
        if (INTERNAL_OPTION(link_ibl))
            unlink_indirect_exit(dcontext, f, l);
    } else
        ASSERT_NOT_REACHED();

    l->flags &= ~LINK_LINKED;
    return keep;
}

static inline bool
incoming_direct_linkstubs_match(common_direct_linkstub_t *dl1,
                                common_direct_linkstub_t *dl2)
{
    return ((dl1 == dl2 && !LINKSTUB_FAKE(&dl1->l) && !LINKSTUB_FAKE(&dl2->l)) ||
            /* For coarse-grain we must match by value */
            (LINKSTUB_FAKE(&dl1->l) && LINKSTUB_FAKE(&dl2->l) &&
             LINKSTUB_NORMAL_DIRECT(dl1->l.flags) &&
             LINKSTUB_NORMAL_DIRECT(dl2->l.flags) &&
             ((direct_linkstub_t *)dl1)->stub_pc == ((direct_linkstub_t *)dl2)->stub_pc));
}

/* Returns the linkstub_t * for the link from f's exit l if it exists in
 * targetf's incoming list.  N.B.: if target is coarse, its unit lock
 * is released prior to returning the pointer!
 */
static linkstub_t *
incoming_find_link(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                   fragment_t *targetf)
{
    common_direct_linkstub_t *s;
    common_direct_linkstub_t **inlist = FRAG_INCOMING_ADDR(targetf);
    common_direct_linkstub_t *dl = (common_direct_linkstub_t *)l;
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(!LINKSTUB_FAKE(l) ||
           (LINKSTUB_COARSE_PROXY(l->flags) && TEST(FRAG_COARSE_GRAIN, f->flags) &&
            LINKSTUB_NORMAL_DIRECT(l->flags)));
    if (TEST(FRAG_COARSE_GRAIN, targetf->flags)) {
        coarse_incoming_t *e;
        coarse_info_t *info = get_fragment_coarse_info(targetf);
        ASSERT(info != NULL);
        d_r_mutex_lock(&info->incoming_lock);
        for (e = info->incoming; e != NULL; e = e->next) {
            if (!e->coarse) {
                linkstub_t *ls;
                for (ls = e->in.fine_l; ls != NULL; ls = LINKSTUB_NEXT_INCOMING(ls)) {
                    if (incoming_direct_linkstubs_match(((common_direct_linkstub_t *)ls),
                                                        dl)) {
                        d_r_mutex_unlock(&info->incoming_lock);
                        return ls;
                    }
                }
            }
        }
        d_r_mutex_unlock(&info->incoming_lock);
    } else {
        for (s = *inlist; s != NULL; s = (common_direct_linkstub_t *)s->next_incoming) {
            ASSERT(LINKSTUB_DIRECT(s->l.flags));
            if (incoming_direct_linkstubs_match(s, dl))
                return (linkstub_t *)s;
        }
    }
    return NULL;
}

/* Returns true if the link from f's exit l exists in targetf's incoming list */
static bool
incoming_link_exists(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                     fragment_t *targetf)
{
    return incoming_find_link(dcontext, f, l, targetf) != NULL;
}

/* Removes the link from l to targetf from the incoming table
 * N.B.: may end up deleting targetf!
 * If l is a fake linkstub_t, then f must be coarse-grain, and this
 * routine searches targetf's incoming links for a match (since the
 * exact stored linkstub_t != l).
 */
static void
incoming_remove_link_nosearch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                              fragment_t *targetf, linkstub_t *prevl,
                              common_direct_linkstub_t **inlist)
{
    common_direct_linkstub_t *dl = (common_direct_linkstub_t *)l;
    common_direct_linkstub_t *dprev = (common_direct_linkstub_t *)prevl;
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(prevl == NULL || LINKSTUB_DIRECT(prevl->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(!LINKSTUB_FAKE(l) ||
           (LINKSTUB_COARSE_PROXY(l->flags) && TEST(FRAG_COARSE_GRAIN, f->flags) &&
            LINKSTUB_NORMAL_DIRECT(l->flags)));
    /* no links across caches so only checking f's sharedness is enough */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));

    if (dprev != NULL)
        dprev->next_incoming = dl->next_incoming;
    else {
        /* if no incoming links left, and targetf is future, we could
         * delete it here, but adaptive wset and trace head
         * counters wanting persistent info means we want to always
         * have either the future or the real thing -- unless
         * it was NEVER executed: then could remove it, which we do for
         * shared cache temporary private futures (see below)
         * FIXME: do this for other futures as well?
         */
        if (dl->next_incoming == NULL) {
            if (TESTALL(FRAG_TEMP_PRIVATE | FRAG_IS_FUTURE, targetf->flags) &&
                !TEST(FRAG_WAS_DELETED, targetf->flags)) {
                /* this is a future created only as an outgoing link of a private
                 * bb created solely for trace creation.
                 * since it never had a real fragment that was executed, we can
                 * toss it, and in fact if we don't we waste a bunch of memory.
                 */
                LOG(THREAD, LOG_LINKS, 3,
                    "incoming_remove_link: temp future " PFX
                    " has no incoming, removing\n",
                    targetf->tag);
                ASSERT(!LINKSTUB_FAKE(l));
                DODEBUG({ ((future_fragment_t *)targetf)->incoming_stubs = NULL; });
                fragment_delete_future(dcontext, (future_fragment_t *)targetf);
                /* WARNING: do not reference targetf after this */
                STATS_INC(num_trace_private_fut_del);
                return;
            }
        }
        *inlist = (common_direct_linkstub_t *)dl->next_incoming;
    }
    dl->next_incoming = NULL;
    if (LINKSTUB_COARSE_PROXY(l->flags)) {
        /* We don't have any place to keep l so we free now.  We'll
         * re-alloc if we lazily re-link.
         */
        LOG(THREAD, LOG_LINKS, 4,
            "freeing proxy incoming " PFX " from coarse " PFX " to fine tag " PFX "\n", l,
            EXIT_STUB_PC(dcontext, f, l), EXIT_TARGET_TAG(dcontext, f, l));
        NONPERSISTENT_HEAP_TYPE_FREE(GLOBAL_DCONTEXT, l, direct_linkstub_t,
                                     ACCT_COARSE_LINK);
    } else
        ASSERT(!LINKSTUB_FAKE(l));
}

static bool
incoming_remove_link_search(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                            fragment_t *targetf, common_direct_linkstub_t **inlist)
{
    common_direct_linkstub_t *s, *prevs, *dl;
    dl = (common_direct_linkstub_t *)l;
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(!LINKSTUB_FAKE(l) ||
           (LINKSTUB_COARSE_PROXY(l->flags) && TEST(FRAG_COARSE_GRAIN, f->flags) &&
            LINKSTUB_NORMAL_DIRECT(l->flags)));
    for (s = *inlist, prevs = NULL; s;
         prevs = s, s = (common_direct_linkstub_t *)s->next_incoming) {
        ASSERT(LINKSTUB_DIRECT(s->l.flags));
        if (incoming_direct_linkstubs_match(s, dl)) {
            /* We must remove s and NOT the passed-in l as coarse_remove_outgoing()
             * passes in a new proxy that we use only to match and find the
             * entry in the list to remove.
             */
            incoming_remove_link_nosearch(dcontext, f, (linkstub_t *)s, targetf,
                                          (linkstub_t *)prevs, inlist);
            return true;
        }
    }
    return false;
}

/* Removes the link from l to targetf from the incoming table
 * N.B.: may end up deleting targetf!
 * If l is a fake linkstub_t, then f must be coarse-grain, and this
 * routine searches targetf's incoming links for a match (since the
 * exact stored linkstub_t != l).
 */
static void
incoming_remove_link(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                     fragment_t *targetf)
{
    /* no links across caches so only checking f's sharedness is enough */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));
    ASSERT(!LINKSTUB_COARSE_PROXY(l->flags) || TEST(FRAG_COARSE_GRAIN, f->flags));
    ASSERT(!TEST(FRAG_COARSE_GRAIN, f->flags) ||
           !TEST(FRAG_COARSE_GRAIN, targetf->flags));
    if (TEST(FRAG_COARSE_GRAIN, targetf->flags)) {
        coarse_remove_incoming(dcontext, f, l, targetf);
    } else {
        if (incoming_remove_link_search(dcontext, f, l, targetf,
                                        FRAG_INCOMING_ADDR(targetf)))
            return;
        DODEBUG({
            ASSERT(targetf != NULL && targetf->tag == EXIT_TARGET_TAG(dcontext, f, l));
            LOG(THREAD, LOG_LINKS, 1,
                "incoming_remove_link: no link from F%d(" PFX ")." PFX " -> "
                "F%d(" PFX ")\n",
                f->id, f->tag, EXIT_CTI_PC(f, l), targetf->id,
                EXIT_TARGET_TAG(dcontext, f, l));
        });
        ASSERT_NOT_REACHED();
    }
}

/* Adds the link from l to targetf to the list of incoming links
 * for targetf.
 */
static inline void
add_to_incoming_list(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                     fragment_t *targetf, bool linked)
{
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(!incoming_link_exists(dcontext, f, l, targetf));
    /* no inter-cache incoming entries */
    ASSERT((f->flags & FRAG_SHARED) == (targetf->flags & FRAG_SHARED));
    if (TEST(FRAG_COARSE_GRAIN, targetf->flags)) {
        coarse_info_t *info = get_fragment_coarse_info(targetf);
        prepend_new_coarse_incoming(info, NULL, l);
    } else {
        common_direct_linkstub_t **inlist =
            (common_direct_linkstub_t **)FRAG_INCOMING_ADDR(targetf);
        common_direct_linkstub_t *dl = (common_direct_linkstub_t *)l;
        /* ensure not added twice b/c future not unlinked, etc. */
        ASSERT(*inlist != dl);
        ASSERT(!LINKSTUB_FAKE(l) || LINKSTUB_COARSE_PROXY(l->flags));
        ASSERT(!is_empty_fragment(linkstub_fragment(dcontext, l)));
        dl->next_incoming = (linkstub_t *)*inlist;
        *inlist = dl;
    }
}

/* Private fragment outgoing links require extra processing
 * First, a private fragment link produces both shared (for trace head
 * marking) and private (for incoming) futures -- we ensure the shared
 * exists here.
 * Second, we need to mark shared future frags as secondary trace heads
 * NOW since we won't put private traces on shared incoming lists.
 * We can do primary trace heads now, too, not a problem.
 */
static void
add_private_check_shared(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    fragment_t *targetf;
    app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    if (!SHARED_FRAGMENTS_ENABLED())
        return;
    ASSERT(!TEST(FRAG_SHARED, f->flags));
    ASSERT(!SHARED_FRAGMENTS_ENABLED() || dynamo_exited ||
           self_owns_recursive_lock(&change_linking_lock));
    targetf = fragment_link_lookup_same_sharing(dcontext, target_tag, l, FRAG_SHARED);
    if (targetf == NULL)
        targetf = (fragment_t *)fragment_lookup_future(dcontext, target_tag);
    if (targetf == NULL) {
        targetf = (fragment_t *)fragment_create_and_add_future(dcontext, target_tag,
                                                               FRAG_SHARED);
    }

    if (!TEST(FRAG_IS_TRACE_HEAD, targetf->flags)) {
        uint th = should_be_trace_head(dcontext, f, l, target_tag, targetf->flags,
                                       true /* have linking lock*/);
        if (TEST(TRACE_HEAD_YES, th)) {
            if (TEST(FRAG_IS_FUTURE, targetf->flags)) {
                LOG(THREAD, LOG_LINKS, 4, "    marking future (" PFX ") as trace head\n",
                    target_tag);
                targetf->flags |= FRAG_IS_TRACE_HEAD;
            } else {
                mark_trace_head(dcontext, targetf, f, l);
            }
            ASSERT(!TEST(TRACE_HEAD_OBTAINED_LOCK, th));
        }
    }
}

/* Adds the link l to the list of incoming future links
 * for l's target.
 */
static void
add_future_incoming(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    future_fragment_t *targetf;
    app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(!SHARED_FRAGMENTS_ENABLED() || !dynamo_exited ||
           self_owns_recursive_lock(&change_linking_lock));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    if (!TEST(FRAG_SHARED, f->flags))
        targetf = fragment_lookup_private_future(dcontext, target_tag);
    else
        targetf = fragment_lookup_future(dcontext, target_tag);
    if (targetf == NULL) {
        /* if private, lookup-and-add being atomic is not an issue;
         * for shared, the change_linking_lock atomicizes for us
         */
        targetf = fragment_create_and_add_future(
            dcontext, target_tag,
            /* take temp flag if present, so we
             * know to remove this future later
             * if never used
             */
            (f->flags & (FRAG_SHARED | FRAG_TEMP_PRIVATE)));
    }
    add_to_incoming_list(dcontext, f, l, (fragment_t *)targetf, false);

    if (!TEST(FRAG_SHARED, f->flags)) {
        /* private fragments need to ensure a shared fragment/future exists,
         * and need to perform secondary trace head marking on it here!
         */
        add_private_check_shared(dcontext, f, l);
    }
}

/* Adds the link from l to targetf to the list of incoming links
 * for targetf.
 */
static void
add_incoming(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, fragment_t *targetf,
             bool linked)
{
    ASSERT(TEST(FRAG_SHARED, f->flags) == TEST(FRAG_SHARED, targetf->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    LOG(THREAD, LOG_LINKS, 4, "    add incoming F%d(" PFX ")." PFX " -> F%d(" PFX ")\n",
        f->id, f->tag, EXIT_CTI_PC(f, l), targetf->id, targetf->tag);
    add_to_incoming_list(dcontext, f, l, targetf, linked);

    if (!TEST(FRAG_SHARED, f->flags)) {
        /* private fragments need to ensure a shared fragment/future exists,
         * and need to perform secondary trace head marking on it here!
         */
        add_private_check_shared(dcontext, f, l);
    }
}

/* fragment_t f is being removed */
future_fragment_t *
incoming_remove_fragment(dcontext_t *dcontext, fragment_t *f)
{
    future_fragment_t *future = NULL;
    linkstub_t *l;

    /* pendel-del frags use fragment_t.in_xlate differently: they should
     * never call this routine once they're marked for deletion
     */
    ASSERT(!TEST(FRAG_WAS_DELETED, f->flags));

    /* link data struct change in shared fragment must be synchronized
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));

    /* if removing shared trace, move its links back to the shadowed shared trace head */
    /* flags not preserved for coarse so we have to check all coarse bbs */
    if (TESTANY(FRAG_TRACE_LINKS_SHIFTED | FRAG_COARSE_GRAIN, f->flags)) {
        if (TEST(FRAG_IS_TRACE, f->flags)) {
            fragment_t wrapper;
            /* FIXME case 8600: provide single lookup routine here */
            fragment_t *bb = fragment_lookup_bb(dcontext, f->tag);
            if (bb == NULL || (bb->flags & FRAG_SHARED) != (f->flags & FRAG_SHARED)) {
                /* Can't use lookup_fine_and_coarse since trace will shadow coarse */
                bb = fragment_coarse_lookup_wrapper(dcontext, f->tag, &wrapper);
                LOG(THREAD, LOG_LINKS, 4,
                    "incoming_remove_fragment shared trace " PFX ": %s coarse thead\n",
                    f->tag, bb == NULL ? "did not find" : "found");
            }
            if (bb != NULL &&
                (TESTANY(FRAG_TRACE_LINKS_SHIFTED | FRAG_COARSE_GRAIN, bb->flags))) {
                ASSERT(TEST(FRAG_IS_TRACE_HEAD, bb->flags) ||
                       TEST(FRAG_COARSE_GRAIN, bb->flags));
                /* FIXME: this will mark trace head as FRAG_LINKED_INCOMING --
                 * but then same thing for a new bb marked as a trace head
                 * before linking via its previous future, so not a new problem.
                 * won't actually link incoming since !linkable.
                 */
                if (TEST(FRAG_COARSE_GRAIN, bb->flags)) {
                    /* We assume the coarse-grain bb is a trace head -- our method
                     * of lookup is unable to mark it so we mark it here
                     */
                    bb->flags |= FRAG_IS_TRACE_HEAD;
                }
                shift_links_to_new_fragment(dcontext, f, bb);
                STATS_INC(links_shared_trace_to_head);
                ASSERT(f->in_xlate.incoming_stubs == NULL);
                return NULL;
            }
        } else if (TEST(FRAG_IS_TRACE_HEAD, f->flags)) {
            fragment_t *trace = fragment_lookup_trace(dcontext, f->tag);
            /* regardless of -remove_shared_trace_heads, a shared trace will
             * at least briefly shadow and shift links from a shared trace head.
             * FIXME: add a FRAG_LINKS_SHIFTED flag to know for sure?
             */
            if (trace != NULL &&
                (trace->flags & FRAG_SHARED) == (f->flags & FRAG_SHARED)) {
                /* nothing to do -- links already shifted, no future needed */
                STATS_INC(shadowed_trace_head_deleted);
                ASSERT(f->in_xlate.incoming_stubs == NULL);
                return NULL;
            }
        }
    }

    /* remove outgoing links from others' incoming lists */
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if (LINKSTUB_DIRECT(l->flags)) {
            /* must explicitly check for self -- may not be in table, if
             * flushing due to munmap
             */
            fragment_t *targetf;
            app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
            if (target_tag == f->tag)
                targetf = f;
            else {
                /* only want fragments in same shared/private cache */
                targetf = fragment_link_lookup_same_sharing(dcontext, target_tag, NULL,
                                                            f->flags);
                if (targetf == NULL) {
                    /* don't worry, all routines can handle fragment_t* that is really
                     * future_fragment_t*
                     */
                    /* make sure future is in proper shared/private table */
                    if (!TEST(FRAG_SHARED, f->flags)) {
                        targetf = (fragment_t *)fragment_lookup_private_future(
                            dcontext, target_tag);
                    } else {
                        targetf =
                            (fragment_t *)fragment_lookup_future(dcontext, target_tag);
                    }
                }
            }
            LOG(THREAD, LOG_LINKS, 4,
                "    removed F%d(" PFX ")." PFX " -> (" PFX ") from incoming list\n",
                f->id, f->tag, EXIT_CTI_PC(f, l), target_tag);
            ASSERT(targetf != NULL);
            if (targetf != NULL) /* play it safe */
                incoming_remove_link(dcontext, f, l, targetf);
        }
    }

    if (TEST(FRAG_TEMP_PRIVATE, f->flags) && f->in_xlate.incoming_stubs == NULL) {
        /* if there are incoming stubs, the private displaced a prior future, which
         * does need to be replaced -- if not, we don't need a future
         */
        LOG(THREAD, LOG_LINKS, 4,
            "    not bothering with future for temp private F%d(" PFX ")\n", f->id,
            f->tag);
        STATS_INC(num_trace_private_fut_avoid);
        return NULL;
    }
    /* add future fragment
     * FIXME: optimization is to convert f to future, that requires
     * coordinating with fragment_remove, fragment_delete, etc.
     */
    LOG(THREAD, LOG_LINKS, 4, "    adding future fragment for deleted F%d(" PFX ")\n",
        f->id, f->tag);
    DOCHECK(1, {
        if (TEST(FRAG_SHARED, f->flags))
            ASSERT(fragment_lookup_future(dcontext, f->tag) == NULL);
        else
            ASSERT(fragment_lookup_private_future(dcontext, f->tag) == NULL);
    });
    future = fragment_create_and_add_future(
        dcontext, f->tag,
        /* make sure future is in proper shared/private table.
         * Do not keep FRAG_TEMP_PRIVATE as getting here means
         * there was a real future here before the private, so our
         * future removal optimization does not apply.
         */
        (FRAG_SHARED & f->flags) | FRAG_WAS_DELETED);

    future->incoming_stubs = f->in_xlate.incoming_stubs;
    DODEBUG({ f->in_xlate.incoming_stubs = NULL; });

    return future;
}

#ifdef DEBUG
static void inline debug_after_link_change(dcontext_t *dcontext, fragment_t *f,
                                           const char *msg)
{
    DOLOG(5, LOG_LINKS, {
        LOG(THREAD, LOG_LINKS, 5, "%s\n", msg);
        disassemble_fragment(dcontext, f, d_r_stats->loglevel < 3);
    });
}
#endif

/* Link all incoming links from other fragments to f
 */
void
link_fragment_incoming(dcontext_t *dcontext, fragment_t *f, bool new_fragment)
{
    linkstub_t *l;
    LOG(THREAD, LOG_LINKS, 4, "  linking incoming links for F%d(" PFX ")\n", f->id,
        f->tag);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT_OWN_RECURSIVE_LOCK(NEED_SHARED_LOCK(f->flags) ||
                                  (new_fragment && SHARED_FRAGMENTS_ENABLED()),
                              &change_linking_lock);
    ASSERT(!TEST(FRAG_LINKED_INCOMING, f->flags));
    f->flags |= FRAG_LINKED_INCOMING;

    /* link incoming links */
    for (l = f->in_xlate.incoming_stubs; l != NULL; l = LINKSTUB_NEXT_INCOMING(l)) {
        bool local_trace_head = false;
        fragment_t *in_f = linkstub_fragment(dcontext, l);
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* Case 8907: remove trace headness markings, as each fine source
             * should only consider this a trace head considering its own
             * path to it.
             */
            local_trace_head = TEST(FRAG_IS_TRACE_HEAD, f->flags);
            f->flags &= ~FRAG_IS_TRACE_HEAD;
        }
        /* only direct branches are marked on targets' incoming */
        ASSERT(LINKSTUB_DIRECT(l->flags));
        if (is_linkable(dcontext, in_f, l, f,
                        NEED_SHARED_LOCK(f->flags) ||
                            (new_fragment && SHARED_FRAGMENTS_ENABLED()),
                        true /*mark new trace heads*/)) {
            /* unprotect on demand, caller will re-protect */
            SELF_PROTECT_CACHE(dcontext, in_f, WRITABLE);
            link_branch(dcontext, in_f, l, f, HOT_PATCHABLE);
        } else {
            LOG(THREAD, LOG_LINKS, 4,
                "    not linking F%d(" PFX ")." PFX " -> F%d(" PFX ")"
                " is not linkable!\n",
                in_f->id, in_f->tag, EXIT_CTI_PC(in_f, l), f->id, f->tag);
        }
        /* Restore trace headness, if present before and not set in is_linkable */
        if (local_trace_head && !TEST(FRAG_IS_TRACE_HEAD, f->flags))
            f->flags |= FRAG_IS_TRACE_HEAD;
    }
}

/* Link outgoing links of f to other fragments in the fcache (and
 * itself if it has self loops).  If new_fragment is true, all of f's
 * outgoing links are recorded in the incoming link lists of their
 * targets.
 */
void
link_fragment_outgoing(dcontext_t *dcontext, fragment_t *f, bool new_fragment)
{
    linkstub_t *l;
    LOG(THREAD, LOG_LINKS, 4, "  linking outgoing links for F%d(" PFX ")\n", f->id,
        f->tag);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT_OWN_RECURSIVE_LOCK(NEED_SHARED_LOCK(f->flags) ||
                                  (new_fragment && SHARED_FRAGMENTS_ENABLED()),
                              &change_linking_lock);
    ASSERT(!TEST(FRAG_LINKED_OUTGOING, f->flags));
    f->flags |= FRAG_LINKED_OUTGOING;

    /* new_fragment: already protected
     * unprotect on demand, caller will re-protect
     */
    if (!new_fragment)
        SELF_PROTECT_CACHE(dcontext, f, WRITABLE);

    /* link outgoing exits */
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if (LINKSTUB_DIRECT(l->flags)) {
            /* found a linkable direct branch */
            app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
            fragment_t *g;
            /* f may be invisible, so explicitly check for self-loops */
            if (target_tag == f->tag)
                g = f;
            else /* primarily interested in fragment of same sharing */
                g = fragment_link_lookup_same_sharing(dcontext, target_tag, l, f->flags);
            if (g != NULL) {
                if (is_linkable(dcontext, f, l, g,
                                NEED_SHARED_LOCK(f->flags) ||
                                    (new_fragment && SHARED_FRAGMENTS_ENABLED()),
                                true /*mark new trace heads*/)) {
                    link_branch(dcontext, f, l, g, HOT_PATCHABLE);
                    if (new_fragment)
                        add_incoming(dcontext, f, l, g, true);
                } else {
                    if (new_fragment)
                        add_incoming(dcontext, f, l, g, false);
                    LOG(THREAD, LOG_LINKS, 4,
                        "    not linking F%d(" PFX ")." PFX " -> "
                        "F%d(" PFX " == " PFX ")%s%s%s\n",
                        f->id, f->tag, EXIT_CTI_PC(f, l), g->id, g->tag, target_tag,
                        ((g->flags & FRAG_IS_TRACE_HEAD) != 0) ? " (trace head)" : "",
                        ((l->flags & LINK_LINKED) != 0) ? " (linked)" : "",
                        ((l->flags & LINK_SPECIAL_EXIT) != 0) ? " (special)" : "");
                }
            } else {
                if (new_fragment)
                    add_future_incoming(dcontext, f, l);
                LOG(THREAD, LOG_LINKS, 4,
                    "    future-linking F%d(" PFX ")." PFX " -> (" PFX ")\n", f->id,
                    f->tag, EXIT_CTI_PC(f, l), target_tag);
            }
        } else {
            ASSERT(LINKSTUB_INDIRECT(l->flags));
            /* indirect branches: just let link_branch handle the
             * exit stub target
             */
#ifdef DGC_DIAGNOSTICS
            /* do not link outgoing indirect so we see where it's going to go */
            if ((f->flags & FRAG_DYNGEN) == 0)
#endif
                link_branch(dcontext, f, l, NULL, HOT_PATCHABLE);
        }
    }

#ifdef DEBUG
    debug_after_link_change(dcontext, f, "Fragment after linking outgoing");
#endif
}

/* Unlinks all incoming branches into fragment f
 */
void
unlink_fragment_incoming(dcontext_t *dcontext, fragment_t *f)
{
    linkstub_t *l, *nextl, *prevl;
    LOG(THREAD, LOG_LINKS, 4, "unlinking incoming to frag F%d(" PFX ")\n", f->id, f->tag);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));
    /* allow for trace head to be unlinked in middle of being unlinked --
     * see comments in mark_trace_head in monitor.c
     */
    ASSERT(TESTANY(FRAG_LINKED_INCOMING | FRAG_IS_TRACE_HEAD, f->flags));
    /* unlink incoming branches */
    ASSERT(!TEST(FRAG_COARSE_GRAIN, f->flags));
    for (prevl = NULL, l = f->in_xlate.incoming_stubs; l != NULL; l = nextl) {
        fragment_t *in_f = linkstub_fragment(dcontext, l);
        bool keep = true;
        nextl = LINKSTUB_NEXT_INCOMING(l);
        /* not all are linked (e.g., to trace head) */
        if (TEST(LINK_LINKED, l->flags)) {
            /* unprotect on demand, caller will re-protect */
            SELF_PROTECT_CACHE(dcontext, in_f, WRITABLE);
            keep = unlink_branch(dcontext, in_f, l);
        } else
            ASSERT(!LINKSTUB_FAKE(l));
        if (!keep) {
            incoming_remove_link_nosearch(dcontext, in_f, l, f, prevl,
                                          FRAG_INCOMING_ADDR(f));
        } else
            prevl = l;
    }
    f->flags &= ~FRAG_LINKED_INCOMING;
}

/* Unlinks all outgoing branches from f
 */
void
unlink_fragment_outgoing(dcontext_t *dcontext, fragment_t *f)
{
    linkstub_t *l;
    DEBUG_DECLARE(bool keep;)
    LOG(THREAD, LOG_LINKS, 4, "unlinking outgoing from frag F%d(" PFX ")\n", f->id,
        f->tag);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));
    ASSERT(TEST(FRAG_LINKED_OUTGOING, f->flags));
    /* unprotect on demand, caller will re-protect */
    SELF_PROTECT_CACHE(dcontext, f, WRITABLE);
    /* unlink outgoing direct & indirect branches */
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if ((l->flags & LINK_LINKED) != 0) {
            DEBUG_DECLARE(keep =)
            unlink_branch(dcontext, f, l); /* works for fine and coarse targets */
            ASSERT(keep);
        }
    }
    f->flags &= ~FRAG_LINKED_OUTGOING;

#ifdef DEBUG
    debug_after_link_change(dcontext, f, "Fragment after unlinking outgoing");
#endif
}

/* Performs proactive linking, and inserts all links into appropriate
 * incoming links lists
 * f may be visible or invisible
 */
void
link_new_fragment(dcontext_t *dcontext, fragment_t *f)
{
    future_fragment_t *future;
    if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        link_new_coarse_grain_fragment(dcontext, f);
        return;
    }
    LOG(THREAD, LOG_LINKS, 4, "linking new fragment F%d(" PFX ")\n", f->id, f->tag);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));

    /* transfer existing future incoming links to this fragment */
    if (!TEST(FRAG_SHARED, f->flags))
        future = fragment_lookup_private_future(dcontext, f->tag);
    else
        future = fragment_lookup_future(dcontext, f->tag);
    if (future != NULL) {
        uint futflags = future->flags;
        LOG(THREAD, LOG_LINKS, 4,
            "  transferring incoming links from existing future frag, flags=0x%08x\n",
            futflags);
        f->in_xlate.incoming_stubs = future->incoming_stubs;
        DODEBUG({ future->incoming_stubs = NULL; });
        /* also transfer any flags that were stored in future
         * N.B.: these flags must not be anything that is required when creating
         * a fragment, they may only be things like TRACE_HEAD if they are
         * to work properly
         */
        /* we only expect certain flags on future fragments */
        ASSERT_CURIOSITY((futflags & ~(FUTURE_FLAGS_ALLOWED)) == 0);
        /* sharedness must match */
        ASSERT(TEST(FRAG_SHARED, f->flags) == TEST(FRAG_SHARED, futflags));
        f->flags |= (futflags & (FUTURE_FLAGS_TRANSFER));
        /* make sure existing flags and flags from build are compatible */
        /* trace head and frag cannot be trace head incompatible */
        if ((f->flags & FRAG_CANNOT_BE_TRACE) != 0 &&
            (f->flags & FRAG_IS_TRACE_HEAD) != 0) {
            f->flags &= ~FRAG_IS_TRACE_HEAD;
            LOG(THREAD, LOG_MONITOR, 2,
                "fragment marked as trace head before being built, but now "
                "cannot be trace head, unmarking trace head : address " PFX "\n",
                f->tag);
        }
        if (TESTALL(FRAG_IS_TRACE | FRAG_IS_TRACE_HEAD, f->flags)) {
            /* we put the trace head flag on futures to mark secondary
             * shared trace heads from private traces -- but it can end up
             * marking traces.  remove it in that case as it WILL mess up
             * linking (case 7465).
             */
            ASSERT(SHARED_FRAGMENTS_ENABLED());
            LOG(THREAD, LOG_MONITOR, 2,
                "trace F%d(" PFX ") inheriting trace head from future: discarding\n",
                f->id, f->tag);
            f->flags &= ~FRAG_IS_TRACE_HEAD;
        }
        fragment_delete_future(dcontext, future);
    }
    DOCHECK(1, {
        if (TEST(FRAG_SHARED, f->flags))
            ASSERT(fragment_lookup_future(dcontext, f->tag) == NULL);
        else
            ASSERT(fragment_lookup_private_future(dcontext, f->tag) == NULL);
    });

    /* link incoming branches first, so no conflicts w/ self-loops
     * that were just linked being added to future unlinked list
     */
    link_fragment_incoming(dcontext, f, true /*new*/);
    link_fragment_outgoing(dcontext, f, true /*new*/);
}

/* Changes all incoming links to old_f to point to new_f
 * old_f and new_f must have the same tag
 * Links up all new_f's outgoing links
 * (regardless of whether old_f is linked or not)
 * These changes are all atomic, so this routine can be run by another
 * thread while the owning thread is in the code cache (but not while it
 * is in dynamo code!)
 */
void
shift_links_to_new_fragment(dcontext_t *dcontext, fragment_t *old_f, fragment_t *new_f)
{
    linkstub_t *l;
    bool have_link_lock =
        (TEST(FRAG_SHARED, old_f->flags) || TEST(FRAG_SHARED, new_f->flags)) &&
        !INTERNAL_OPTION(single_thread_in_DR);
    cache_pc old_stub = NULL;
    cache_pc old_body = NULL;
    coarse_info_t *info = NULL;
    DEBUG_DECLARE(bool keep;)
    ASSERT(old_f->tag == new_f->tag);
    /* usually f's flags are just copied to new, so don't assert
     * that new_f's LINKED_ flags are 0, but we do assume it has
     * never been linked!
     */

    LOG(THREAD, LOG_LINKS, 4, "shifting links from F%d(" PFX ") to F%d(" PFX ")\n",
        old_f->id, old_f->tag, new_f->id, new_f->tag);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT(!have_link_lock || self_owns_recursive_lock(&change_linking_lock));

    /* if the new fragment had the exact same sequence of exits as the old,
     * we could walk in lockstep, calling incoming_table_change_linkstub,
     * but the new fragment could have completely different exits, so we'd
     * better remove all the old from others' incoming lists, and then add
     * the new -- don't worry about synchronization, owning thread can
     * only be in fcache, not in dynamo code
     */
    /* remove old outgoing links from others' incoming lists */
    LOG(THREAD, LOG_LINKS, 4, "  removing outgoing links for F%d(" PFX ")\n", old_f->id,
        old_f->tag);
    if (TEST(FRAG_COARSE_GRAIN, old_f->flags)) {
        /* FIXME: we could implement full non-fake fragment_t recovery, and
         * engineer the normal link paths to do the right thing for coarse
         * fragments, to avoid all the coarse checks in this routine
         */
        info = get_fragment_coarse_info(old_f);
        ASSERT(info != NULL);
        fragment_coarse_lookup_in_unit(dcontext, info, old_f->tag, &old_stub, &old_body);
        ASSERT(old_stub != NULL || info->frozen);
        ASSERT(old_body == FCACHE_ENTRY_PC(old_f));

        /* We should only call this when emitting a trace */
        ASSERT(TESTALL(FRAG_IS_TRACE | FRAG_SHARED, new_f->flags));

        /* Case 8627: Since we can't really remove the fragment, we may as well
         * leave the links intact.  Besides, it's difficult to walk the outgoing
         * links: the best way is to decode_fragment() and then walk the exit
         * ctis (xref case 8571 on decoding up to linkstub_t level).  There are no
         * uniqueness requirements on incoming entries so long as there aren't
         * two from the same coarse unit, so it's fine for the trace to also be
         * in target incoming lists.  Plus, this means we don't have to do
         * anything if we later delete the trace.
         */
        ASSERT(!TEST(FRAG_COARSE_GRAIN, new_f->flags)); /* ensure distinct incomings */
    } else {
        for (l = FRAGMENT_EXIT_STUBS(old_f); l; l = LINKSTUB_NEXT_EXIT(l)) {
            if (LINKSTUB_DIRECT(l->flags)) {
                /* incoming links do not cross sharedness boundaries */
                app_pc target_tag = EXIT_TARGET_TAG(dcontext, old_f, l);
                fragment_t *targetf = fragment_link_lookup_same_sharing(
                    dcontext, target_tag, l, old_f->flags);
                if (targetf == NULL) {
                    /* don't worry, all routines can handle fragment_t* that is really
                     * future_fragment_t*
                     */
                    /* make sure future is in proper shared/private table */
                    if (!TEST(FRAG_SHARED, old_f->flags)) {
                        targetf = (fragment_t *)fragment_lookup_private_future(
                            dcontext, target_tag);
                    } else {
                        targetf =
                            (fragment_t *)fragment_lookup_future(dcontext, target_tag);
                    }
                }
                ASSERT(targetf != NULL);
                /* if targetf == old_f, must remove self-link, b/c it won't be
                 * redirected below as it won't appear on incoming list (new
                 * self-link will).
                 *
                 * to avoid dangling links to other fragments (that may be deleted
                 * and leave the link there since not in incoming) we also unlink
                 * everything else.
                 */
                if (TEST(LINK_LINKED, l->flags)) {
                    DEBUG_DECLARE(keep =) unlink_branch(dcontext, old_f, l);
                    ASSERT(keep);
                }
                LOG(THREAD, LOG_LINKS, 4,
                    "    removed F%d(" PFX ")." PFX " -> (" PFX ") from incoming lists\n",
                    old_f->id, old_f->tag, EXIT_CTI_PC(old_f, l), target_tag);
                incoming_remove_link(dcontext, old_f, l, targetf);
            } else {
                ASSERT(LINKSTUB_INDIRECT(l->flags));
                if (TEST(LINK_LINKED, l->flags)) {
                    DEBUG_DECLARE(keep =) unlink_branch(dcontext, old_f, l);
                    ASSERT(keep);
                }
            }
        }
    }
    old_f->flags &= ~FRAG_LINKED_OUTGOING;

    /* We copy prior to link-outgoing as that can add to incoming for self-links */
    ASSERT(new_f->in_xlate.incoming_stubs == NULL);
    ASSERT(!TEST(FRAG_COARSE_GRAIN, old_f->flags) ||
           old_f->in_xlate.incoming_stubs == NULL);
    new_f->in_xlate.incoming_stubs = old_f->in_xlate.incoming_stubs;
    old_f->in_xlate.incoming_stubs = NULL;
    if (TEST(FRAG_COARSE_GRAIN, new_f->flags) && new_f->in_xlate.incoming_stubs != NULL) {
        if (info == NULL)
            info = get_fragment_coarse_info(new_f);
        prepend_new_coarse_incoming(info, NULL, new_f->in_xlate.incoming_stubs);
    }

    /* N.B.: unlike linking a new fragment, the owning thread could be in
     * the fcache, and the 1st link is NOT guaranteed to be atomic,
     * so we have to link the outgoing links for the new fragment before
     * we ever link anybody up to it
     */
    /* link outgoing exits
     * linking an already-linked branch is a nop, but as an optimization,
     * rather than unlinking we mark them as not linked and put the proper
     * link in with one cache write rather than two
     */
    if (!TEST(FRAG_COARSE_GRAIN, new_f->flags)) {
        if (TEST(FRAG_LINKED_OUTGOING, new_f->flags)) {
            for (l = FRAGMENT_EXIT_STUBS(new_f); l; l = LINKSTUB_NEXT_EXIT(l)) {
                if (LINKSTUB_DIRECT(l->flags) && TEST(LINK_LINKED, l->flags)) {
                    l->flags &= ~LINK_LINKED; /* leave inconsistent briefly */
                }
            }
        }
        new_f->flags &= ~FRAG_LINKED_OUTGOING; /* avoid assertion failure */
        link_fragment_outgoing(dcontext, new_f, true /*add incoming*/);
    } else {
        ASSERT(TESTALL(FRAG_IS_TRACE | FRAG_SHARED, old_f->flags));
        /* We assume this is re-instating a coarse trace head upon deleting
         * a shared trace -- and that we never did unlink the trace head's outgoing
         */
    }
    ASSERT(TEST(FRAG_LINKED_OUTGOING, new_f->flags));

    /* now shift incoming links from old fragment to new one */
    LOG(THREAD, LOG_LINKS, 4, "  transferring incoming links from F%d to F%d\n",
        old_f->id, new_f->id);
    old_f->flags &= ~FRAG_LINKED_INCOMING;
    LOG(THREAD, LOG_LINKS, 4, "  linking incoming links for F%d(" PFX ")\n", new_f->id,
        new_f->tag);
    if (TEST(FRAG_COARSE_GRAIN, old_f->flags)) {
        coarse_incoming_t *e, *prev_e = NULL, *next_e;
        bool remove_entry;
        /* We don't yet support coarse to coarse shifts (see above for one reason) */
        ASSERT(!TEST(FRAG_COARSE_GRAIN, new_f->flags));
        /* Change the entrance stub to point to the trace, which redirects
         * all incoming from inside the unit.
         */
        if (old_stub != NULL) {
            ASSERT(!entrance_stub_linked(old_stub, info) ||
                   entrance_stub_jmp_target(old_stub) == FCACHE_ENTRY_PC(old_f));
            if (entrance_stub_linked(old_stub, info)) {
                /* If was never marked as trace head we must mark now,
                 * else we will lose track of the body pc!
                 */
                coarse_mark_trace_head(dcontext, old_f, info, old_stub,
                                       FCACHE_ENTRY_PC(old_f));
                STATS_INC(coarse_th_on_shift);
            }
            coarse_link_to_fine(dcontext, old_stub, old_f, new_f, true /*do link*/);
        } else {
            /* FIXME: patch the frozen trace head to redirect?
             * Else once in the unit will not go to trace.
             * But by case 8151 this is only a trace head for paths coming
             * from outside, which will go to the trace, so it should be ok.
             */
            ASSERT(info->frozen);
        }
        new_f->flags |= FRAG_LINKED_INCOMING;

        /* Now re-route incoming from outside the unit */
        d_r_mutex_lock(&info->incoming_lock);
        for (e = info->incoming; e != NULL; e = next_e) {
            next_e = e->next;
            remove_entry = false;
            if (!e->coarse) {
                app_pc tgt = NULL;
                linkstub_t *next_l;
                for (l = e->in.fine_l; l != NULL; l = next_l) {
                    fragment_t *in_f = linkstub_fragment(dcontext, l);
                    next_l = LINKSTUB_NEXT_INCOMING(l);
                    ASSERT(!TEST(LINK_FAKE, l->flags));
                    if (tgt == NULL) {
                        tgt = EXIT_TARGET_TAG(dcontext, in_f, l);
                    } else {
                        /* Every fine incoming in a single coarse-list entry should
                         * target the same tag
                         */
                        ASSERT(EXIT_TARGET_TAG(dcontext, in_f, l) == tgt);
                    }
                    if (tgt == old_f->tag) {
                        /* unprotect on demand (caller will re-protect)
                         * FIXME: perhaps that's true for other link routines,
                         * but shift_links_to_new_fragment() is called from
                         * places where we need to double-check
                         */
                        SELF_PROTECT_CACHE(dcontext, in_f, WRITABLE);
                        DEBUG_DECLARE(keep =) unlink_branch(dcontext, in_f, l);
                        ASSERT(keep);
                        if (is_linkable(dcontext, in_f, l, new_f, have_link_lock,
                                        true /*mark trace heads*/)) {
                            link_branch(dcontext, in_f, l, new_f, HOT_PATCHABLE);
                            add_incoming(dcontext, in_f, l, new_f, true /*linked*/);
                        } else {
                            LOG(THREAD, LOG_LINKS, 4,
                                "    not linking F%d(" PFX ")." PFX " -> F%d(" PFX ")"
                                " is not linkable!\n",
                                in_f->id, in_f->tag, EXIT_CTI_PC(in_f, l), new_f->id,
                                new_f->tag);
                            add_incoming(dcontext, in_f, l, new_f, false /*!linked*/);
                        }
                        remove_entry = true;
                    }
                }
            } else if (entrance_stub_jmp_target(e->in.stub_pc) ==
                       FCACHE_ENTRY_PC(old_f)) {
                /* FIXME: we don't know the tag of the src (and cannot find it
                 * (case 8565))!  We'll use old_f's tag to avoid triggering any
                 * new trace head rules.  Presumably they would have already
                 * been triggered unless they vary based on coarse or fine.
                 */
                fragment_t *src_f =
                    fragment_coarse_link_wrapper(dcontext, old_f->tag, e->in.stub_pc);
                set_fake_direct_linkstub(&temp_linkstub, old_f->tag, e->in.stub_pc);
                LOG(THREAD, LOG_LINKS, 4,
                    "    shifting coarse link " PFX " -> %s " PFX " to F%d(" PFX ")\n",
                    e->in.stub_pc, info->module, old_f->tag, new_f->id, new_f->tag);
                if (is_linkable(dcontext, src_f, (linkstub_t *)&temp_linkstub, new_f,
                                have_link_lock, true /*mark trace heads*/)) {
                    DODEBUG({
                        /* avoid assert about being already linked */
                        unlink_entrance_stub(dcontext, e->in.stub_pc, new_f->flags, NULL);
                    });
                    coarse_link_to_fine(dcontext, e->in.stub_pc, src_f, new_f,
                                        true /*do link*/);
                }
                remove_entry = true;
            }
            if (remove_entry) {
                if (prev_e == NULL)
                    info->incoming = e->next;
                else
                    prev_e->next = e->next;
                LOG(THREAD, LOG_LINKS, 4, "freeing coarse_incoming_t " PFX "\n", e);
                NONPERSISTENT_HEAP_TYPE_FREE(GLOBAL_DCONTEXT, e, coarse_incoming_t,
                                             ACCT_COARSE_LINK);
            } else
                prev_e = e;
        }
        d_r_mutex_unlock(&info->incoming_lock);

    } else if (TEST(FRAG_COARSE_GRAIN, new_f->flags)) {
        /* Change the entrance stub to point to the trace head routine again
         * (we only shift to coarse trace heads).
         */
        coarse_info_t *new_f_info = get_fragment_coarse_info(new_f);
        cache_pc new_stub, new_body;
        ASSERT(new_f_info != NULL);
        fragment_coarse_lookup_in_unit(dcontext, new_f_info, old_f->tag, &new_stub,
                                       &new_body);
        ASSERT(new_body == FCACHE_ENTRY_PC(new_f));
        if (new_stub != NULL) {
            unlink_entrance_stub(dcontext, new_stub, FRAG_IS_TRACE_HEAD, new_f_info);
            ASSERT(coarse_is_trace_head_in_own_unit(dcontext, new_f->tag, new_stub,
                                                    new_body, true, new_f_info));
            /* If we ever support shifting to non-trace-heads we'll want to point the
             * stub at the fragment and not at the head incr routine
             */
        } else
            ASSERT(new_f_info->frozen);
        /* We can re-use link_fragment_incoming, but be careful of any future
         * changes that require splitting out the coarse-and-fine-shared part.
         */
        new_f->flags &= ~FRAG_LINKED_INCOMING; /* wrapper is marked as linked */
        link_fragment_incoming(dcontext, new_f, true /*new*/);
        ASSERT(TEST(FRAG_LINKED_INCOMING, new_f->flags));
    } else {
        new_f->flags |= FRAG_LINKED_INCOMING;
        for (l = new_f->in_xlate.incoming_stubs; l != NULL;
             l = LINKSTUB_NEXT_INCOMING(l)) {
            fragment_t *in_f = linkstub_fragment(dcontext, l);
            ASSERT(!is_empty_fragment(in_f));
            if (is_linkable(dcontext, in_f, l, new_f, have_link_lock,
                            true /*mark new trace heads*/)) {
                /* used to check to make sure was linked but no reason to? */
                /* already did self-links */
                if (in_f != new_f) {
                    if (TEST(LINK_LINKED, l->flags))
                        l->flags &= ~LINK_LINKED; /* else, link_branch is a nop */
                    link_branch(dcontext, in_f, l, new_f, HOT_PATCHABLE);
                } else {
                    LOG(THREAD, LOG_LINKS, 4,
                        "    not linking F%d(" PFX ")." PFX " -> "
                        "F%d(" PFX " == " PFX ") (self-link already linked)\n",
                        in_f->id, in_f->tag, EXIT_CTI_PC(in_f, l), new_f->id, new_f->tag);
                }
            } else if ((l->flags & LINK_LINKED) != 0) {
                DEBUG_DECLARE(keep =) unlink_branch(dcontext, in_f, l);
                ASSERT(keep);
                LOG(THREAD, LOG_LINKS, 4,
                    "    not linking F%d(" PFX ")." PFX " -> F%d(" PFX ")"
                    "(src not outgoing-linked)\n",
                    in_f->id, in_f->tag, EXIT_CTI_PC(in_f, l), new_f->id, new_f->tag);
            } else {
                LOG(THREAD, LOG_LINKS, 4,
                    "    not linking F%d(" PFX ")." PFX " -> "
                    "F%d(" PFX " == " PFX ")\n",
                    in_f->id, in_f->tag, EXIT_CTI_PC(in_f, l), new_f->id, new_f->tag);
            }
        }
    }

    /* For the common case of a trace shadowing a trace head
     * (happens w/ shared traces, and with custom traces),
     * ensure that when we delete the trace we shift back and
     * when we delete the head we don't complain that we're
     * missing links.
     */
    if (TEST(FRAG_IS_TRACE, new_f->flags) && TEST(FRAG_IS_TRACE_HEAD, old_f->flags)) {
        ASSERT((!NEED_SHARED_LOCK(new_f->flags) && !NEED_SHARED_LOCK(old_f->flags)) ||
               self_owns_recursive_lock(&change_linking_lock));
        LOG(THREAD, LOG_LINKS, 4, "Marking old and new as FRAG_TRACE_LINKS_SHIFTED\n");
        new_f->flags |= FRAG_TRACE_LINKS_SHIFTED;
        old_f->flags |= FRAG_TRACE_LINKS_SHIFTED;
    }

    DOLOG(4, LOG_LINKS, {
        LOG(THREAD, LOG_LINKS, 4, "Old fragment after shift:\n");
        disassemble_fragment(dcontext, old_f, d_r_stats->loglevel < 4);
        LOG(THREAD, LOG_LINKS, 4, "New fragment after shift:\n");
        disassemble_fragment(dcontext, new_f, d_r_stats->loglevel < 4);
    });
}

/***************************************************************************
 * COARSE-GRAIN UNITS
 */

static vm_area_vector_t *coarse_stub_areas;

static void
coarse_stubs_init()
{
    VMVECTOR_ALLOC_VECTOR(coarse_stub_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED | VECTOR_NEVER_MERGE, coarse_stub_areas);
}

static void
coarse_stubs_free()
{
    ASSERT(coarse_stub_areas != NULL);
    /* should be empty from special_heap_exit(), from vm_area_coarse_units_reset_free() */
    ASSERT(vmvector_empty(coarse_stub_areas));
    vmvector_delete_vector(GLOBAL_DCONTEXT, coarse_stub_areas);
}

/* current prefix size is 37 bytes, so need 3 stub slots */
static inline uint
num_coarse_stubs_for_prefix(coarse_info_t *info)
{
    uint prefix_size = coarse_exit_prefix_size(info);
    size_t stub_size = COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info));
    ASSERT(CHECK_TRUNCATE_TYPE_uint(ALIGN_FORWARD(prefix_size, stub_size)));
    return (uint)(ALIGN_FORWARD(prefix_size, stub_size) / stub_size);
}

uint
coarse_stub_alignment(coarse_info_t *info)
{
    return (uint)COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info));
}

/* Pass in NULL for pc to have stubs created.
 * Size must include room for prefixes as well as stubs.
 */
cache_pc
coarse_stubs_create(coarse_info_t *info, cache_pc pc, size_t size)
{
    byte *end_pc;
    ASSERT(coarse_stub_areas != NULL);
    info->stubs = special_heap_pclookup_init(
        (uint)COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info)), true /* must synch */,
        true /* +x */, false /* not persistent */, coarse_stub_areas,
        info /* support pclookup w/ info retval */, pc, size,
        pc != NULL /*full if pre-alloc*/);
    /* Create the fcache_return_coarse prefix for this unit.
     * We keep it here rather than at the top of the fcache unit because:
     * 1) stubs are writable while fcache should be read-only, and we may want
     *    to patch the prefix when persistent to point to the current fcache_return
     * 2) we need to find the prefix given just a stub and no info on the
     *    src body in fcache
     * We have to make sure stub walks skip over the prefix.
     */
    if (pc != NULL) {
        /* Header is separate, so we know we can start at the top */
        info->fcache_return_prefix = pc;
    } else {
        info->fcache_return_prefix =
            (cache_pc)special_heap_calloc(info->stubs, num_coarse_stubs_for_prefix(info));
    }
    end_pc = emit_coarse_exit_prefix(GLOBAL_DCONTEXT, info->fcache_return_prefix, info);
    /* we have to align for pc != NULL;
     * caller should be using calloc if pc==NULL but we align just in case
     */
    end_pc = (cache_pc)ALIGN_FORWARD(end_pc, coarse_stub_alignment(info));
    ASSERT(pc == NULL || end_pc <= pc + size);
    ASSERT(info->trace_head_return_prefix != NULL);
    ASSERT(info->ibl_ret_prefix != NULL);
    ASSERT(info->ibl_call_prefix != NULL);
    ASSERT(info->ibl_jmp_prefix != NULL);
    ASSERT(end_pc - info->fcache_return_prefix <=
           (int)(COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info)) *
                 num_coarse_stubs_for_prefix(info)));
    DOCHECK(1, {
        /* FIXME i#1551: need diff versions for diff isa modes */
        SET_TO_NOPS(DEFAULT_ISA_MODE, end_pc,
                    info->fcache_return_prefix +
                        COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info)) *
                            num_coarse_stubs_for_prefix(info) -
                        end_pc);
    });
    return end_pc;
}

/* Iteration support over coarse entrance stubs */

typedef struct {
    special_heap_iterator_t shi;
    cache_pc start, end, pc;
    coarse_info_t *info;
} coarse_stubs_iterator_t;

static void
coarse_stubs_iterator_start(coarse_info_t *info, coarse_stubs_iterator_t *csi)
{
    special_heap_iterator_start(info->stubs, &csi->shi);
    csi->info = info;
    if (special_heap_iterator_hasnext(&csi->shi)) {
        special_heap_iterator_next(&csi->shi, &csi->start, &csi->end);
        /* skip the prefix kept at the top of the first unit */
        ASSERT(csi->start == info->fcache_return_prefix);
        /* coarse_stubs_iterator_next() will add 1 */
        csi->pc = csi->start +
            COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info)) *
                (num_coarse_stubs_for_prefix(info) - 1);
    } else {
        csi->start = NULL;
        csi->end = NULL;
        csi->pc = NULL;
    }
}

/* If we wanted coarse_stubs_iterator_hasnext() it would look like this:
 *   return (csi->pc < csi->end || special_heap_iterator_hasnext(&csi->shi))
 */
static cache_pc inline coarse_stubs_iterator_next(coarse_stubs_iterator_t *csi)
{
    if (csi->pc == NULL)
        return NULL;
    ASSERT(csi->pc < csi->end);
    csi->pc += COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(csi->info));
    if (csi->pc >= csi->end) {
        if (special_heap_iterator_hasnext(&csi->shi)) {
            special_heap_iterator_next(&csi->shi, &csi->start, &csi->end);
            csi->pc = csi->start;
        } else
            return NULL;
    }
    /* N.B.: once we free entrance stubs we'll need to identify a
     * freed pattern here.  For now we assume everything is
     * a stub.
     */
    ASSERT(coarse_is_entrance_stub(csi->pc));
    return csi->pc;
}

static void inline coarse_stubs_iterator_stop(coarse_stubs_iterator_t *csi)
{
    special_heap_iterator_stop(&csi->shi);
}

void
coarse_stubs_delete(coarse_info_t *info)
{
    DEBUG_DECLARE(coarse_stubs_iterator_t csi;)
    DEBUG_DECLARE(cache_pc pc;)
    ASSERT_OWN_MUTEX(!dynamo_all_threads_synched && !info->is_local, &info->lock);
    if (info->stubs == NULL) {
        /* lazily initialized, so common to have empty units */
        ASSERT(info->fcache_return_prefix == NULL);
        return;
    }
#ifdef DEBUG
    if (info->frozen) {
        /* We allocated the stub unit ourselves so nothing to free here */
    } else {
        special_heap_cfree(info->stubs, info->fcache_return_prefix,
                           num_coarse_stubs_for_prefix(info));
        /* We can't free while using the iterator (lock issues) so we
         * free all at once
         */
        coarse_stubs_iterator_start(info, &csi);
        for (pc = coarse_stubs_iterator_next(&csi); pc != NULL;
             pc = coarse_stubs_iterator_next(&csi)) {
            special_heap_free(info->stubs, pc);
        }
        coarse_stubs_iterator_stop(&csi);
    }
#endif
    LOG(THREAD_GET, LOG_LINKS | LOG_HEAP, 1, "Coarse special heap exit %s\n",
        info->module);
    special_heap_exit(info->stubs);
    info->stubs = NULL;
    info->fcache_return_prefix = NULL;
    info->trace_head_return_prefix = NULL;
    info->ibl_ret_prefix = NULL;
    info->ibl_call_prefix = NULL;
    info->ibl_jmp_prefix = NULL;
}

/* N.B.: once we start freeing entrance stubs, we need to fill the space,
 * so that our stub walk can identify live stubs
 */
static cache_pc
entrance_stub_create(dcontext_t *dcontext, coarse_info_t *info, fragment_t *f,
                     linkstub_t *l)
{
    cache_pc stub_pc;
    DEBUG_DECLARE(size_t stub_size = COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info));)
    ASSERT(DYNAMO_OPTION(coarse_units));
    ASSERT(info != NULL && info->stubs != NULL);
    ASSERT(LINKSTUB_DIRECT(l->flags));
    ASSERT(TEST(LINK_SEPARATE_STUB, l->flags));
    ASSERT(exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l), f->flags) <=
           (ssize_t)stub_size);
    /* We hot-patch our stubs and we assume that aligning them to 16 is enough */
    ASSERT((cache_line_size % stub_size) == 0);
    stub_pc = (cache_pc)special_heap_alloc(info->stubs);
    ASSERT(ALIGNED(stub_pc, coarse_stub_alignment(info)));
    DEBUG_DECLARE(uint emit_sz =) insert_exit_stub(dcontext, f, l, stub_pc);
    LOG(THREAD, LOG_LINKS, 4,
        "created new entrance stub @" PFX " for " PFX " w/ source F%d(" PFX ")." PFX "\n",
        stub_pc, EXIT_TARGET_TAG(dcontext, f, l), f->id, f->tag, FCACHE_ENTRY_PC(f));
    ASSERT(emit_sz <= stub_size);
    DOCHECK(1, {
        SET_TO_NOPS(dr_get_isa_mode(dcontext), stub_pc + emit_sz, stub_size - emit_sz);
    });
    STATS_ADD(separate_shared_bb_entrance_stubs, stub_size);
    STATS_INC(num_entrance_stubs);
    return stub_pc;
}

/* Sets flags for a fake linkstub_t for an incoming list entry for a coarse source */
static void
set_fake_direct_linkstub(direct_linkstub_t *proxy, app_pc target, cache_pc stub)
{
    /* ASSUMPTION: this combination is unique to coarse-grain proxy stubs.
     * The LINKSTUB_COARSE_PROXY() macro tests these (except LINK_LINKED).
     */
    proxy->cdl.l.flags = LINK_FAKE | LINK_DIRECT | LINK_LINKED | LINK_SEPARATE_STUB;
    proxy->cdl.l.cti_offset = 0;
    proxy->target_tag = target;
    proxy->stub_pc = stub;
}

#if defined(DEBUG) && defined(INTERNAL)
static void
print_coarse_incoming(dcontext_t *dcontext, coarse_info_t *info)
{
    coarse_incoming_t *e;
    ASSERT_OWN_MUTEX(true, &info->incoming_lock);
    LOG(THREAD, LOG_LINKS, 1, "coarse incoming entries for %s\n", info->module);
    for (e = info->incoming; e != NULL; e = e->next) {
        LOG(THREAD, LOG_LINKS, 1, "\t" PFX " %d ", e, e->coarse);
        if (e->coarse)
            LOG(THREAD, LOG_LINKS, 1, PFX "\n", e->in.stub_pc);
        else {
            fragment_t *f = linkstub_fragment(dcontext, e->in.fine_l);
            LOG(THREAD, LOG_LINKS, 1, "F%d(" PFX ")\n", f->id, f->tag);
        }
    }
}
#endif

/* Must pass NULL for exactly one of coarse or fine */
static coarse_incoming_t *
prepend_new_coarse_incoming(coarse_info_t *info, cache_pc coarse, linkstub_t *fine)
{
    coarse_incoming_t *entry = NONPERSISTENT_HEAP_TYPE_ALLOC(
        GLOBAL_DCONTEXT, coarse_incoming_t, ACCT_COARSE_LINK);
    /* entries are freed in coarse_remove_outgoing()/coarse_unit_unlink() */
    if (fine == NULL) {
        /* FIXME: rather than separate entries per stub pc, to save memory
         * we could have a single one for the whole unit (and we'd search
         * here to see if it already exists) and search when unlinking to
         * find the individual stubs
         */
        ASSERT(coarse != NULL);
        entry->coarse = true;
        entry->in.stub_pc = coarse;
        LOG(THREAD_GET, LOG_LINKS, 4,
            "created new coarse_incoming_t " PFX " coarse from " PFX "\n", entry,
            entry->in.stub_pc);
    } else {
        ASSERT(coarse == NULL);
        entry->coarse = false;
        /* We put the whole linkstub_t list as one entry */
        entry->in.fine_l = fine;
        LOG(THREAD_GET, LOG_LINKS, 4,
            "created new coarse_incoming_t " PFX " fine from " PFX "\n", entry,
            entry->in.fine_l);
        DOCHECK(1, {
            linkstub_t *l;
            for (l = fine; l != NULL; l = LINKSTUB_NEXT_INCOMING(l))
                ASSERT(!TEST(LINK_FAKE, l->flags));
        });
    }
    ASSERT(info != NULL);
    d_r_mutex_lock(&info->incoming_lock);
    entry->next = info->incoming;
    info->incoming = entry;
    DOLOG(5, LOG_LINKS, {
        LOG(GLOBAL, LOG_LINKS, 4, "after adding new incoming " PFX ":\n", entry);
        print_coarse_incoming(GLOBAL_DCONTEXT, info);
    });
    d_r_mutex_unlock(&info->incoming_lock);
    return entry;
}

/* Pass in coarse if you already know it; else, this routine will look it up */
static fragment_t *
fragment_coarse_link_wrapper(dcontext_t *dcontext, app_pc target_tag,
                             cache_pc know_coarse)
{
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    if (know_coarse == NULL) {
        return fragment_coarse_lookup_wrapper(dcontext, target_tag, &temp_targetf);
    } else {
        fragment_coarse_wrapper(&temp_targetf, target_tag, know_coarse);
        return &temp_targetf;
    }
    return NULL;
}

static fragment_t *
fragment_link_lookup_same_sharing(dcontext_t *dcontext, app_pc tag, linkstub_t *last_exit,
                                  uint flags)
{
    /* Assumption: if asking for private won't use temp_targetf.
     * Else need to grab the lock when linking private fragments
     * (in particular, temps for trace building)
     */
    ASSERT(!TEST(FRAG_SHARED, flags) || self_owns_recursive_lock(&change_linking_lock));
    return fragment_lookup_fine_and_coarse_sharing(dcontext, tag, &temp_targetf,
                                                   last_exit, flags);
}

static void
coarse_link_to_fine(dcontext_t *dcontext, cache_pc src_stub, fragment_t *src_f,
                    fragment_t *target_f, bool do_link /*else just add incoming*/)
{
    /* We may call this multiple times for multiple sources going through the
     * same entrance stub
     */
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    set_fake_direct_linkstub(&temp_linkstub, target_f->tag, src_stub);
    if (incoming_link_exists(dcontext, src_f, (linkstub_t *)&temp_linkstub, target_f)) {
        ASSERT(entrance_stub_jmp_target(src_stub) == FCACHE_ENTRY_PC(target_f));
        LOG(THREAD, LOG_LINKS, 4,
            "    already linked coarse " PFX "." PFX "->F%d(" PFX ")\n", src_f->tag,
            FCACHE_ENTRY_PC(src_f), target_f->id, target_f->tag);
    } else {
        direct_linkstub_t *proxy = NONPERSISTENT_HEAP_TYPE_ALLOC(
            GLOBAL_DCONTEXT, direct_linkstub_t, ACCT_COARSE_LINK);
        LOG(THREAD, LOG_LINKS, 4,
            "created new proxy incoming " PFX " from coarse " PFX " to fine F%d\n", proxy,
            src_stub, target_f->id);
        LOG(THREAD, LOG_LINKS, 4, "    linking coarse stub " PFX "->F%d(" PFX ")\n",
            src_stub, target_f->id, target_f->tag);
        /* Freed in incoming_remove_link() called from unlink_fragment_incoming()
         * or from coarse_unit_unlink() calling coarse_remove_outgoing().
         * Note that we do not unlink fine fragments on reset/exit (case 7697)
         * so we can't rely solely on unlink_fragment_incoming() to free these
         * for us.
         */
        set_fake_direct_linkstub(proxy, target_f->tag, src_stub);
        add_incoming(dcontext, src_f, (linkstub_t *)proxy, target_f, true /*linked*/);
        if (do_link) {
            /* Should not be linked somewhere else */
            ASSERT(!entrance_stub_linked(src_stub, NULL) ||
                   entrance_stub_jmp_target(src_stub) == FCACHE_ENTRY_PC(target_f));
            proxy->cdl.l.flags &= ~LINK_LINKED; /* so link_branch isn't a nop */
            link_branch(dcontext, src_f, (linkstub_t *)proxy, target_f, HOT_PATCHABLE);
        } else {
            /* Already linked (we're freezing or shifting) */
            ASSERT(entrance_stub_linked(src_stub, NULL) &&
                   entrance_stub_jmp_target(src_stub) == FCACHE_ENTRY_PC(target_f));
        }
    }
}

/* Links up an entrance stub to its target, whether that is a local coarse,
 * remote coarse, or remote fine fragment.
 * Takes in f and l since for a new coarse fragment those already exist;
 * other callers will have to fabricate.
 * Returns true if the link was enacted.
 */
static bool
coarse_link_direct(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                   coarse_info_t *src_info, cache_pc stub, app_pc target_tag,
                   cache_pc local_tgt_in, bool new_stub)
{
    bool linked = false;
    /* Targets are always body pcs.  stub is the stub pc we'll link through. */
    cache_pc local_tgt = NULL;
    cache_pc remote_tgt = NULL;
    cache_pc coarse_tgt = NULL;
    fragment_t *target_f = NULL;
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    ASSERT(entrance_stub_target_tag(stub, src_info) == target_tag);
    /* Note that it is common for stub to already be linked (b/c we have
     * entrance stubs shared by multiple sources), yet we still need to call
     * is_linkable
     */
    DOSTATS({
        if (entrance_stub_linked(stub, src_info))
            STATS_INC(coarse_relinks);
    });
    /* Since we leave shadowed trace heads visible we must first look in the
     * fine tables
     */
    ASSERT(TEST(FRAG_SHARED, f->flags));
    target_f = fragment_lookup_same_sharing(dcontext, target_tag, FRAG_SHARED);
    if (target_f == NULL) {
        if (local_tgt_in == NULL) {
            /* Use src_info if available -- else look up by tag */
            coarse_info_t *info = src_info;
            if (info == NULL) {
                ASSERT(f->tag != NULL);
                info = get_fragment_coarse_info(f);
            }
            if (info != NULL) {
                fragment_coarse_lookup_in_unit(dcontext, info, target_tag, NULL,
                                               &local_tgt);
            }
        } else
            local_tgt = local_tgt_in;
        if (local_tgt == NULL) {
            remote_tgt = fragment_coarse_lookup(dcontext, target_tag);
            coarse_tgt = remote_tgt;
        } else
            coarse_tgt = local_tgt;
    } else
        ASSERT(!TEST(FRAG_COARSE_GRAIN, target_f->flags));
    if (coarse_tgt != NULL || target_f != NULL) {
        if (target_f == NULL) {
            /* No fine-grain fragment_t so make a fake one to use for the
             * is_linkable() and mark_trace_head() paths.
             * We can only recover certain flags, and we assume that others
             * cannot be represented in a coarse unit anyway.
             */
            target_f = fragment_coarse_link_wrapper(dcontext, target_tag, coarse_tgt);
            if (stub != NULL && coarse_is_trace_head(stub))
                target_f->flags |= FRAG_IS_TRACE_HEAD;
        }
        if (is_linkable(dcontext, f, l, target_f,
                        NEED_SHARED_LOCK(f->flags) || SHARED_FRAGMENTS_ENABLED(),
                        true /*mark new trace heads*/)) {
            linked = true;
            if (local_tgt == NULL) {
                /* Target is outside this unit, either a fine fragment_t
                 * or another unit's coarse fragment
                 */
                if (!TEST(FRAG_COARSE_GRAIN, target_f->flags)) {
                    if (!entrance_stub_linked(stub, src_info))
                        coarse_link_to_fine(dcontext, stub, f, target_f, true /*link*/);
                    else {
                        ASSERT(entrance_stub_jmp_target(stub) ==
                               FCACHE_ENTRY_PC(target_f));
                    }
                } else {
                    if (!entrance_stub_linked(stub, src_info)) {
                        coarse_info_t *target_info = get_fcache_coarse_info(coarse_tgt);
                        ASSERT(stub != remote_tgt);
                        LOG(THREAD, LOG_LINKS, 4,
                            "    linking coarse " PFX "." PFX "->%s " PFX "\n", f->tag,
                            FCACHE_ENTRY_PC(f), target_info->module, coarse_tgt);
                        link_entrance_stub(dcontext, stub, coarse_tgt, HOT_PATCHABLE,
                                           src_info);
                        /* add to incoming list */
                        ASSERT(target_info != NULL);
                        ASSERT(coarse_tgt != NULL);
                        prepend_new_coarse_incoming(target_info, stub, NULL);
                    } else
                        ASSERT(entrance_stub_jmp_target(stub) == coarse_tgt);
                }
            } else {
                coarse_info_t *target_info = get_fcache_coarse_info(coarse_tgt);
                ASSERT(target_info != NULL);
                if (new_stub || target_info->persisted) {
                    if (!entrance_stub_linked(stub, src_info)) {
                        LOG(THREAD, LOG_LINKS, 4,
                            "    linking coarse " PFX "." PFX "->" PFX " intra-unit\n",
                            f->tag, FCACHE_ENTRY_PC(f), coarse_tgt);
                        link_entrance_stub(dcontext, stub, coarse_tgt, HOT_PATCHABLE,
                                           src_info);
                    } else
                        ASSERT(entrance_stub_jmp_target(stub) == coarse_tgt);
                } else {
                    /* Should only need to link if target is created,
                     * when it should be linked by link_new_coarse_grain_fragment(),
                     * or for persisted trace heads
                     */
                    ASSERT(entrance_stub_linked(stub, src_info));
                }
            }
        } else {
            /* No incoming needed since linking lazily */
            /* Currently we do not support non-linkable coarse exits except
             * for trace head targets (as they can all share an entrance).
             * For others we'd need per-exit entrance stubs and custom fcache
             * return paths: FIXME.
             */
            /* We should have converted the entrance stub to a trace head
             * stub in mark_trace_head()
             */
            ASSERT((!TEST(FRAG_COARSE_GRAIN, target_f->flags) &&
                    (TEST(FRAG_IS_TRACE_HEAD, target_f->flags) ||
                     !TEST(FRAG_SHARED, target_f->flags))) ||
                   coarse_is_trace_head(stub));
            ASSERT(!entrance_stub_linked(stub, src_info));
            LOG(THREAD, LOG_LINKS, 4,
                "    NOT linking coarse " PFX "." PFX "->F%d(" PFX ") (!linkable th)\n",
                f->tag, FCACHE_ENTRY_PC(f), target_f->id, target_f->tag);
        }
    } else {
        /* We use our entrance stub as a future fragment placeholder */
        ASSERT(!entrance_stub_linked(stub, src_info));
        LOG(THREAD, LOG_LINKS, 4,
            "    NOT linking coarse " PFX "." PFX "->" PFX " (doesn't exist)\n", f->tag,
            FCACHE_ENTRY_PC(f), target_tag);
    }
    return linked;
}

static void
link_new_coarse_grain_fragment(dcontext_t *dcontext, fragment_t *f)
{
    coarse_info_t *info = get_fragment_coarse_info(f);
    linkstub_t *l;
    cache_pc orig_stub, self_stub;
    cache_pc local_stub, local_body;
    future_fragment_t *future;
    bool th_unlink = false;

    ASSERT(info != NULL);
    /* ensure some higher-level lock is held if f is shared
     * no links across caches so only checking f's sharedness is enough
     */
    ASSERT(!NEED_SHARED_LOCK(f->flags) || self_owns_recursive_lock(&change_linking_lock));
    LOG(THREAD, LOG_LINKS, 4, "linking coarse-grain fragment F%d(" PFX ")\n", f->id,
        f->tag);
    ASSERT(TESTALL(FRAG_COARSE_GRAIN | FRAG_SHARED, f->flags));

    /* Transfer existing fine-grained future incoming links to this fragment */
    future = fragment_lookup_future(dcontext, f->tag); /* shared only */
    if (future != NULL) {
        uint futflags = future->flags;
        LOG(THREAD, LOG_LINKS, 4,
            "  transferring flags 0x%08x from existing future frag\n", futflags);
        /* we only expect certain flags on future fragments */
        ASSERT_CURIOSITY((futflags & ~(FUTURE_FLAGS_ALLOWED)) == 0);
        /* sharedness must match */
        ASSERT(TEST(FRAG_SHARED, f->flags) == TEST(FRAG_SHARED, futflags));
        /* FIXME: we will discard all of these flags, which right now only include
         * secondary shared trace heads from private traces, fortunately, and
         * we'll use that by converting to a trace head below.
         */
        f->flags |= (futflags & (FUTURE_FLAGS_TRANSFER));
        /* we shouldn't have any of the incompatibilites a new fine fragment does */
        ASSERT(!TESTANY(FRAG_CANNOT_BE_TRACE | FRAG_IS_TRACE, f->flags));
        if (TEST(FRAG_IS_TRACE_HEAD, f->flags))
            mark_trace_head(dcontext, f, f, NULL);

        if (future->incoming_stubs != NULL) {
            LOG(THREAD, LOG_LINKS, 4,
                "  transferring incoming links from existing future frag\n");
            prepend_new_coarse_incoming(info, NULL, future->incoming_stubs);
            /* We can re-use link_fragment_incoming, but be careful of any future
             * changes that require splitting out the coarse-and-fine-shared part.
             */
            f->in_xlate.incoming_stubs = future->incoming_stubs;
            DODEBUG({ future->incoming_stubs = NULL; });
            link_fragment_incoming(dcontext, f, true /*new*/);
        }

        fragment_delete_future(dcontext, future);
    }
    ASSERT(fragment_lookup_future(dcontext, f->tag) == NULL);

    /* There is no proactive linking from coarse-grain fragments: it's
     * all done lazily, so there are no records of who wanted to link
     * to this fragment from other coarse units.
     * For sources inside this unit, they all point at the entrance stub, so
     * our only incoming link action is to link the entrance stub to us.
     * FIXME: we assume that is_linkable() conditions haven't changed -- that
     * it only needs to be called once for coarse-grain intra-unit links.
     * This means that trace head definitions, etc., will not see any changed
     * data about this target.
     */
    fragment_coarse_lookup_in_unit(dcontext, info, f->tag, &orig_stub, NULL);
    if (orig_stub == NULL) {
        /* To enable easy trace head unlinking we want an entrance stub for
         * every intra-linked-to fragment.  Currently we proactively create an
         * entrance stub for ourselves at creation time.
         *
         * Case 8628: An alternative is to delay the stub creation while
         * maintaining the easy unlink: put body pc in htable now, and only
         * create stub if we see a second (creation implies a first :)) link to
         * here.  That means we have mixed body and stub pc's in the htable for
         * non-frozen units, requiring a vector binary search (for
         * coarse_is_entrance_stub()) on every lookup.  For now we avoid that,
         * but perhaps the waste of memory is less efficient, since once-only
         * code won't ever use this entrance stub.  Plus, we need that vector
         * search on every lookup for frozen units anyway, requiring us to eat
         * the cost or split the htable.  If we switch to the alternative we can
         * move the htable-add back to emit().
         */
        set_fake_direct_linkstub(&temp_linkstub, f->tag, NULL);
        temp_linkstub.cdl.l.flags &= ~LINK_LINKED;
        self_stub = entrance_stub_create(dcontext, info, f, (linkstub_t *)&temp_linkstub);
    } else
        self_stub = orig_stub;
    if (self_stub != NULL) {
        ASSERT(coarse_is_entrance_stub(self_stub));
        if (TEST(FRAG_IS_TRACE_HEAD, f->flags)) { /* from future or incoming */
            if (orig_stub == NULL) {
                /* mark_trace_head didn't know where stub is -- so we must unlink: */
                ASSERT(!entrance_stub_linked(self_stub, info) ||
                       entrance_stub_jmp_target(self_stub) == FCACHE_ENTRY_PC(f));
                /* unlink the stub and add the body pc to the th htable -- but
                 * do it after the linking below, so that no one uses the body
                 * pc table entry before the fragment is fully linked
                 */
                th_unlink = true;
            } else {
                if (!coarse_is_trace_head(self_stub)) {
                    /* mark_trace_head() was called for incoming and marked it for a
                     * different source coarse unit -- so we don't consider it a
                     * trace head here.  FIXME: should we consider it one, and keep
                     * path-dependent trace heads to a minimum?
                     */
                    f->flags &= ~FRAG_IS_TRACE_HEAD;
                    STATS_INC(coarse_th_path_dependent);
                    LOG(THREAD, LOG_LINKS, 4,
                        "    trace head via inter-unit path, not intra-unit\n");
                }
            }
        }
    } else
        ASSERT_NOT_REACHED(); /* won't happen w/ current proactive stub creation */

    f->flags |= FRAG_LINKED_OUTGOING;
    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        if (LINKSTUB_DIRECT(l->flags)) {
            bool new_stub = false;
            app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
            direct_linkstub_t *dl = (direct_linkstub_t *)l;
            fragment_coarse_lookup_in_unit(dcontext, info, target_tag, &local_stub,
                                           &local_body);
            if (local_stub == NULL) {
                /* We need a new entrance stub, even for intra-unit links where
                 * the target exists (alternative of directly linking intra-unit
                 * now requires unlink if mark target as trace head later).  If
                 * the target doesn't exist we need one regardless.
                 */
                new_stub = true;
                if (target_tag == f->tag) {
                    local_stub = self_stub;
                } else
                    local_stub = entrance_stub_create(dcontext, info, f, l);
            }
            ASSERT(dl->stub_pc == NULL); /* not set in emit */
            /* Case 9708: must set stub so we can mark extra-unit trace head
             * if local stub is new.
             */
            dl->stub_pc = local_stub;
            LOG(THREAD, LOG_LINKS, 4,
                "  linking coarse " PFX "." PFX "->" PFX " to entrance stub\n", f->tag,
                FCACHE_ENTRY_PC(f), local_stub);
            patch_branch(FRAG_ISA_MODE(f->flags), EXIT_CTI_PC(f, l), local_stub,
                         /*new, unreachable*/ NOT_HOT_PATCHABLE);
            /* case 9009: can't link stub to self until self fully linked;
             * no incoming needed so fine to rely on self link below.
             */
            if (local_stub != self_stub) {
                coarse_link_direct(dcontext, f, l, info, local_stub, target_tag,
                                   local_body, new_stub);
            }
            /* case 9009: can't add to htable prior to linking, though
             * here change_linking_lock should prevent anyone from
             * using this stub for the first time.  Still, can't be
             * too safe.
             */
            if (new_stub) {
                if (local_body != NULL) {
                    /* If we don't proactively create self entrance stubs,
                     * here we would need to replace the htable entry of the
                     * target's body with its new entrance stub -- but w/
                     * current design we shouldn't enter this if.
                     */
                    ASSERT_NOT_REACHED();
                    /* This is an entrance stub for a target in our own unit.
                     * Point htable at stub instead of fragment.
                     */
                    fragment_coarse_replace(dcontext, info, f->tag, local_stub);
                } else {
                    /* We add to htable so others targeting this tag will use
                     * the same entrance stub.
                     * Case 9009: we cannot add self yet before we link our
                     * other exits!  We'll do the add below.
                     */
                    if (local_stub != self_stub)
                        fragment_coarse_add(dcontext, info, target_tag, local_stub);
                }
            }
        } else {
            ASSERT(LINKSTUB_INDIRECT(l->flags));
            /* indirect branches: just let link_branch handle the
             * exit stub target
             */
#ifdef DGC_DIAGNOSTICS
            /* We don't support unlinked indirect branches.
             * FIXME: should turn on -no_link_ibl
             */
            ASSERT_NOT_IMPLEMENTED(false);
#endif
            l->flags |= LINK_LINKED;
        }
    }

    /* Perform self-stub-linking and htable adding only when fragment is fully
     * linked (case 9009).  Outgoing linking shouldn't change incoming link or
     * trace head status, so we shouldn't get asserts about not being a trace head
     * yet not being linked.
     */
    if (self_stub != NULL) {
        if (th_unlink) {
            fragment_coarse_th_unlink_and_add(dcontext, f->tag, self_stub,
                                              FCACHE_ENTRY_PC(f));
        }
        if (!TEST(FRAG_IS_TRACE_HEAD, f->flags)) {
            LOG(THREAD, LOG_LINKS, 4,
                "    linking coarse entrance stub to self " PFX "->" PFX "." PFX "\n",
                self_stub, f->tag, FCACHE_ENTRY_PC(f));
            link_entrance_stub(dcontext, self_stub, FCACHE_ENTRY_PC(f),
                               /*new fragment: no races*/ NOT_HOT_PATCHABLE, NULL);
            ASSERT(entrance_stub_jmp_target(self_stub) == FCACHE_ENTRY_PC(f));
        } else {
            LOG(THREAD, LOG_LINKS, 4,
                "    NOT linking coarse entrance stub " PFX "->" PFX
                " since trace head\n",
                self_stub, f->tag);
        }
    }

    /* we add here rather than in emit() caller since we have self_stub ptr */
    if (orig_stub == NULL)
        fragment_coarse_add(dcontext, info, f->tag, self_stub);
    else {
        DOCHECK(1, {
            /* Stub was added by earlier target of this tag */
            fragment_coarse_lookup_in_unit(dcontext, info, f->tag, &local_stub,
                                           &local_body);
            ASSERT(local_stub == self_stub);
            ASSERT(local_body == FCACHE_ENTRY_PC(f));
        });
    }
}

/* Removes an incoming entry from a fine fragment to a coarse unit */
static void
coarse_remove_incoming(dcontext_t *dcontext, fragment_t *src_f, linkstub_t *src_l,
                       fragment_t *targetf)
{
    coarse_info_t *info = get_fragment_coarse_info(targetf);
    coarse_incoming_t *e, *prev_e;
    ASSERT(!TEST(FRAG_COARSE_GRAIN, src_f->flags));
    ASSERT(!LINKSTUB_FAKE(src_l));
    ASSERT(TEST(FRAG_COARSE_GRAIN, targetf->flags));
    LOG(THREAD, LOG_LINKS, 4, "coarse_remove_incoming %s " PFX " to " PFX "\n",
        info->module, src_f->tag, targetf->tag);

    d_r_mutex_lock(&info->incoming_lock);
    for (prev_e = NULL, e = info->incoming; e != NULL; prev_e = e, e = e->next) {
        if (!e->coarse) {
            if (incoming_remove_link_search(dcontext, src_f, src_l, targetf,
                                            (common_direct_linkstub_t **)&e->in.fine_l)) {
                if (e->in.fine_l == NULL) {
                    /* If no fine entries left, remove the incoming entry wrapper */
                    if (prev_e == NULL)
                        info->incoming = e->next;
                    else
                        prev_e->next = e->next;
                    LOG(THREAD, LOG_LINKS, 4, "freeing coarse_incoming_t " PFX "\n", e);
                    NONPERSISTENT_HEAP_TYPE_FREE(GLOBAL_DCONTEXT, e, coarse_incoming_t,
                                                 ACCT_COARSE_LINK);
                }
                break;
            }
        }
    }
    DOLOG(5, LOG_LINKS, {
        LOG(THREAD, LOG_LINKS, 4, "after removing incoming:\n");
        print_coarse_incoming(dcontext, info);
    });
    d_r_mutex_unlock(&info->incoming_lock);
}

/* Removes any incoming data recording the outgoing link from stub */
void
coarse_remove_outgoing(dcontext_t *dcontext, cache_pc stub, coarse_info_t *src_info)
{
    cache_pc target_tag = entrance_stub_target_tag(stub, src_info);
    /* ASSUMPTION: coarse-grain are always shared and cannot target private */
    fragment_t *targetf = fragment_lookup_same_sharing(dcontext, target_tag, FRAG_SHARED);
    ASSERT(entrance_stub_linked(stub, src_info));
    if (targetf != NULL) {
        /* targeting a real fragment_t */
        direct_linkstub_t proxy;
        set_fake_direct_linkstub(&proxy, target_tag, stub);
        LOG(THREAD, LOG_LINKS, 4,
            "    removing coarse link " PFX " -> F%d(" PFX ")." PFX "\n", stub,
            targetf->id, targetf->tag, FCACHE_ENTRY_PC(targetf));
        incoming_remove_link(dcontext, (fragment_t *)&coarse_fragment,
                             (linkstub_t *)&proxy, targetf);
        ASSERT(entrance_stub_linked(stub, src_info));
        /* case 9635: pass flags */
        unlink_entrance_stub(dcontext, stub, targetf->flags, src_info);
    } else {
        cache_pc target_pc = entrance_stub_jmp_target(stub);
        coarse_info_t *target_info = get_fcache_coarse_info(target_pc);
        /* can only be NULL if targeting fine fragment, which is covered above! */
        ASSERT(target_info != NULL);
        if (target_info != src_info) {
            coarse_incoming_t *e, *prev_e = NULL;
            DEBUG_DECLARE(bool found = false;)
            unlink_entrance_stub(dcontext, stub, 0, src_info);
            LOG(THREAD, LOG_LINKS, 4, "    removing coarse link " PFX " -> %s " PFX "\n",
                stub, target_info->module, target_tag);
            d_r_mutex_lock(&target_info->incoming_lock);
            for (e = target_info->incoming; e != NULL; e = e->next) {
                if (e->coarse && e->in.stub_pc == stub) {
                    if (prev_e == NULL)
                        target_info->incoming = e->next;
                    else
                        prev_e->next = e->next;
                    LOG(THREAD, LOG_LINKS, 4, "freeing coarse_incoming_t " PFX "\n", e);
                    NONPERSISTENT_HEAP_TYPE_FREE(GLOBAL_DCONTEXT, e, coarse_incoming_t,
                                                 ACCT_COARSE_LINK);
                    DODEBUG(found = true;);
                    break;
                } else
                    prev_e = e;
            }
            DOLOG(5, LOG_LINKS, {
                LOG(THREAD, LOG_LINKS, 4, "after removing outgoing, target:\n");
                print_coarse_incoming(dcontext, target_info);
            });
            d_r_mutex_unlock(&target_info->incoming_lock);
            ASSERT(found);
        } else {
            /* an intra-unit link, so no incoming entry */
            LOG(THREAD, LOG_LINKS, 4,
                "    not removing coarse link to self " PFX " -> %s " PFX "\n", stub,
                target_info->module, target_tag);
        }
    }
}

void
coarse_mark_trace_head(dcontext_t *dcontext, fragment_t *f, coarse_info_t *info,
                       cache_pc stub, cache_pc body)
{
    /* We do NOT keep incoming info for extra-unit trace head targets,
     * as we do not need it (though we could unlink in coarse_unit_unlink).
     */
    if (entrance_stub_linked(stub, info)) {
        LOG(THREAD, LOG_LINKS, 4, "  removing outgoing %s @" PFX "\n", info->module,
            stub);
        coarse_remove_outgoing(dcontext, stub, info);
    }

    LOG(THREAD, LOG_LINKS, 4,
        "\tunlinking entrance stub " PFX ", pointing at th routine\n", stub);
    /* Convert to a trace head stub.  If body is in this unit (i.e.,
     * not an extra-unit trace head targets), since we can no longer
     * find the body pc from the stub, we must also store it in a
     * separate htable.
     */
    fragment_coarse_th_unlink_and_add(dcontext, f->tag, stub, body);
}

/* Unlinks both incoming and outgoing links.
 * Coarse units cannot be re-linked, so unlinking will remove all data
 * structs in incoming lists.
 * Due to lock rank order, caller must hold change_linking_lock in addition
 * to info->lock.
 */
void
coarse_unit_unlink(dcontext_t *dcontext, coarse_info_t *info)
{
    /* Unlink incoming */
    coarse_incoming_t *e, *next_e;
    DEBUG_DECLARE(bool keep;)
    ASSERT(info != NULL);
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    ASSERT_OWN_MUTEX(true, &info->lock);
    if (info->stubs == NULL) /* lazily initialized, so common to have empty units */
        return;
    d_r_mutex_lock(&info->incoming_lock);
#ifndef DEBUG
    /* case 8599: fast exit/reset path: all incoming links are in
     * nonpersistent memory
     */
    if (dynamo_exited || dynamo_resetting) {
        info->incoming = NULL;
        d_r_mutex_unlock(&info->incoming_lock);
        return;
    }
#endif
    LOG(THREAD, LOG_LINKS, 4, "coarse_unit_unlink %s\n", info->module);

    DOLOG(5, LOG_LINKS, {
        LOG(THREAD, LOG_LINKS, 4, "about to remove all incoming:\n");
        print_coarse_incoming(dcontext, info);
    });
    for (e = info->incoming; e != NULL; e = next_e) {
        next_e = e->next;
        if (e->coarse) {
            unlink_entrance_stub(dcontext, e->in.stub_pc, 0, NULL);
        } else {
            linkstub_t *l, *last_l = NULL;
            future_fragment_t *future;
            app_pc tgt = NULL;
            for (l = e->in.fine_l; l != NULL; last_l = l, l = LINKSTUB_NEXT_INCOMING(l)) {
                fragment_t *in_f = linkstub_fragment(dcontext, l);
                if (tgt == NULL) {
                    /* unprotect on demand (caller will re-protect) */
                    SELF_PROTECT_CACHE(dcontext, in_f, WRITABLE);
                    tgt = EXIT_TARGET_TAG(dcontext, in_f, l);
                } else {
                    /* Every fine incoming in a single coarse-list entry should
                     * target the same tag
                     */
                    ASSERT(EXIT_TARGET_TAG(dcontext, in_f, l) == tgt);
                }
                DEBUG_DECLARE(keep =) unlink_branch(dcontext, in_f, l);
                ASSERT(keep);
            }
            /* Ensure we shifted links properly to traces replacing coarse heads */
            ASSERT(fragment_lookup_trace(dcontext, tgt) == NULL);
            future = fragment_lookup_future(dcontext, tgt);
            if (future == NULL) {
                LOG(THREAD, LOG_LINKS, 4,
                    "    adding future fragment for removed coarse target " PFX "\n",
                    tgt);
                future = fragment_create_and_add_future(dcontext, tgt,
                                                        FRAG_SHARED | FRAG_WAS_DELETED);
                future->incoming_stubs = e->in.fine_l;
            } else {
                /* It's possible to have multiple incoming entries here for later-linked
                 * fine sources, and thus for this routine to have created a future
                 * from an earlier entry.
                 */
                common_direct_linkstub_t *dl = (common_direct_linkstub_t *)last_l;
                ASSERT(last_l != NULL);
                dl->next_incoming = future->incoming_stubs;
                future->incoming_stubs = e->in.fine_l;
            }
        }
        LOG(THREAD, LOG_LINKS, 4, "freeing coarse_incoming_t " PFX "\n", e);
        NONPERSISTENT_HEAP_TYPE_FREE(GLOBAL_DCONTEXT, e, coarse_incoming_t,
                                     ACCT_COARSE_LINK);
    }
    info->incoming = NULL;
    d_r_mutex_unlock(&info->incoming_lock);

    coarse_unit_unlink_outgoing(dcontext, info);
}

void
coarse_unit_unlink_outgoing(dcontext_t *dcontext, coarse_info_t *info)
{
    coarse_stubs_iterator_t csi;
    cache_pc pc;
    ASSERT(info != NULL);
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    ASSERT_OWN_MUTEX(true, &info->lock);

    /* Unlink outgoing by walking the stubs */
    coarse_stubs_iterator_start(info, &csi);
    for (pc = coarse_stubs_iterator_next(&csi); pc != NULL;
         pc = coarse_stubs_iterator_next(&csi)) {
        if (entrance_stub_linked(pc, info)) {
            LOG(THREAD, LOG_LINKS, 4, "  removing outgoing %s @" PFX "\n", info->module,
                pc);
            coarse_remove_outgoing(dcontext, pc, info);
        } else {
            /* Lazy linking, and when we unlink (for trace head) we remove
             * incoming then, so no incoming entries to remove here.
             */
            LOG(THREAD, LOG_LINKS, 4, "  not removing unlinked outgoing %s @" PFX "\n",
                info->module, pc);
        }
    }
    coarse_stubs_iterator_stop(&csi);
}

#ifdef DEBUG
bool
coarse_unit_outgoing_linked(dcontext_t *dcontext, coarse_info_t *info)
{
    coarse_stubs_iterator_t csi;
    cache_pc pc;
    bool linked = false;
    ASSERT(info != NULL);
    d_r_mutex_lock(&info->lock);

    /* Check outgoing links by walking the stubs */
    coarse_stubs_iterator_start(info, &csi);
    for (pc = coarse_stubs_iterator_next(&csi); pc != NULL;
         pc = coarse_stubs_iterator_next(&csi)) {
        if (entrance_stub_linked(pc, info)) {
            linked = true;
            goto coarse_unit_outgoing_linked_done;
        }
    }
coarse_unit_outgoing_linked_done:
    coarse_stubs_iterator_stop(&csi);
    d_r_mutex_unlock(&info->lock);
    return linked;
}
#endif

/* Returns the entrance stub that targets target_pc.
 * We cannot find the unique source tag, only the stub (case 8565).
 */
cache_pc
coarse_stub_lookup_by_target(dcontext_t *dcontext, coarse_info_t *info,
                             cache_pc target_tag)
{
    cache_pc stub;
    ASSERT(info != NULL);
    fragment_coarse_lookup_in_unit(dcontext, info, target_tag, &stub, NULL);
    return stub;
}

/* Coarse-grain lazy linking: if either source or target is coarse.
 *
 * We don't keep the bookkeeping around (viz., future fragments) to support
 * proactive linking.  Lazy linking does mean that we need some source info,
 * unfortunately.  We use a per-unit fcache_return miss path to identify source
 * unit, and then search that unit's stubs for the target recorded by the
 * entrance stub.  We cannot find the unique source tag, only the entrance stub,
 * which we assume is enough for linking (case 8565).
 */
void
coarse_lazy_link(dcontext_t *dcontext, fragment_t *targetf)
{
    DEBUG_DECLARE(bool linked = false;)
    DEBUG_DECLARE(bool already_linked = false;)
    ASSERT(dcontext->next_tag == targetf->tag);
    if (dcontext->last_exit == get_coarse_exit_linkstub() &&
        /* rule out !is_linkable targets now to avoid work and assert below */
        TEST(FRAG_SHARED, targetf->flags) &&
        (!TEST(FRAG_IS_TRACE_HEAD, targetf->flags) ||
         TEST(FRAG_COARSE_GRAIN, targetf->flags) || DYNAMO_OPTION(disable_traces))) {
        /* Source is coarse */
        fragment_t temp_sourcef;
        cache_pc stub;
        coarse_info_t *info = dcontext->coarse_exit.dir_exit;
        if (info != NULL) {
            ASSERT(is_dynamo_address((byte *)info)); /* ensure union used as expected */
            stub = coarse_stub_lookup_by_target(dcontext, info, dcontext->next_tag);
            if (stub == NULL) {
                /* We may be deliving a signal */
                IF_WINDOWS(ASSERT_NOT_REACHED());
                goto lazy_link_done;
            }
            /* May already be linked (we may have just built its target)
             * Case 8825: we must hold the change_linking_lock when we check.
             * Cheaper to just grab lock than to test first, unless high contention.
             */
            acquire_recursive_lock(&change_linking_lock);
            if (!entrance_stub_linked(stub, info) &&
                (!coarse_is_trace_head(stub) || DYNAMO_OPTION(disable_traces))) {
                /* We don't know the specific tag of the source, as multiple may
                 * share a single entrance stub.  ASSUMPTION (case 8565): the
                 * only way in which is_linkable depends on tags is for trace
                 * head identification, which should happen at
                 * coarse-fragment-to-entrance-stub link time.
                 */
                memset(&temp_sourcef, 0, sizeof(temp_sourcef));
                temp_sourcef.tag = NULL; /* thus no trace head trigger */
                DODEBUG({ temp_sourcef.start_pc = stub; });
                temp_sourcef.flags = FRAG_SHARED | FRAG_COARSE_GRAIN |
                    FRAG_LINKED_OUTGOING | FRAG_LINKED_INCOMING;

                set_fake_direct_linkstub(&temp_linkstub, dcontext->next_tag, NULL);
                temp_linkstub.cdl.l.flags &= ~LINK_LINKED;

                /* FIXME: we have targetf, we should use it here
                 * FIXME: Our source f has no tag, so we pass in info -- fragile!
                 */
                if (coarse_link_direct(
                        dcontext, &temp_sourcef, (linkstub_t *)&temp_linkstub, info, stub,
                        dcontext->next_tag, NULL, false /*stub should already exist*/)) {
                    LOG(THREAD, LOG_LINKS, 4,
                        "lazy linked " PFX " -> F%d(" PFX ")." PFX "\n", stub,
                        targetf->id, targetf->tag, FCACHE_ENTRY_PC(targetf));
                    STATS_INC(lazy_links_from_coarse);
                    DOSTATS({
                        if (info->persisted)
                            STATS_INC(lazy_links_from_persisted);
                    });
                    DODEBUG({ linked = true; });
                }
            } else {
                DODEBUG({ already_linked = true; });
            }
            release_recursive_lock(&change_linking_lock);
        }
    } else if (TEST(FRAG_COARSE_GRAIN, targetf->flags)) {
        /* Target is coarse */
        linkstub_t *l = dcontext->last_exit;
        fragment_t *f = dcontext->last_fragment;
        if (dcontext->next_tag != EXIT_TARGET_TAG(dcontext, f, l) ||
            !TEST(FRAG_SHARED, f->flags)) {
            /* Rule out syscall-skip and other cases where we cannot link.
             * is_linkable() should catch, but this way we avoid work and avoid
             * the incoming_link_exists() assert below.
             */
            goto lazy_link_done;
        }
        /* May already be linked (we may have just built its target) */
        if (LINKSTUB_DIRECT(l->flags) && !LINKSTUB_FAKE(l) &&
            !TEST(FRAG_FAKE, f->flags) && !TEST(LINK_LINKED, l->flags)) {
            acquire_recursive_lock(&change_linking_lock);
            /* FIXME: provide common routine for this: dup of link_fragment_outgoing */
            if (!TEST(LINK_LINKED, l->flags) /* case 8825: test w/ lock! */ &&
                is_linkable(dcontext, f, l, targetf, true /*have change_linking_lock*/,
                            true /*mark new trace heads*/)) {
                LOG(THREAD, LOG_LINKS, 4,
                    "lazy linking F%d(" PFX ") -> F%d(" PFX ")." PFX "\n", f->id, f->tag,
                    targetf->id, targetf->tag, FCACHE_ENTRY_PC(targetf));
                link_branch(dcontext, f, l, targetf, HOT_PATCHABLE);
                add_incoming(dcontext, f, l, targetf, true);
                STATS_INC(lazy_links_from_fine);
                DODEBUG({ linked = true; });
            } else {
                DOCHECK(CHKLVL_DEFAULT + 1,
                        { /* PR 307698: perf hit */
                          ASSERT(incoming_link_exists(dcontext, f, l, targetf) ||
                                 /* case 8786: another thread could have built a
                                  * shared trace to replace this coarse fragment and
                                  * already shifted incoming links to the new trace */
                                 (DYNAMO_OPTION(shared_traces) &&
                                  fragment_lookup_trace(dcontext, dcontext->next_tag) !=
                                      NULL));
                        });
            }
            release_recursive_lock(&change_linking_lock);
        } else {
            /* We'll treat indirect xfer as already-linked */
            DODEBUG({ already_linked = true; });
        }
    }
lazy_link_done:
    DODEBUG({
        if (!linked) {
            LOG(THREAD, LOG_LINKS, 4,
                "NOT lazy linking F%d(" PFX ") -> F%d(" PFX ")." PFX "%s\n",
                dcontext->last_fragment->id, dcontext->last_fragment->tag, targetf->id,
                targetf->tag, FCACHE_ENTRY_PC(targetf),
                already_linked ? " already linked" : "");
            if (!already_linked) {
                STATS_INC(lazy_links_failed);
            }
        }
    });
}

/* Passing in stub_pc's info avoids a vmvector lookup */
cache_pc
fcache_return_coarse_prefix(cache_pc stub_pc, coarse_info_t *info /*OPTIONAL*/)
{
    if (info == NULL)
        info = (coarse_info_t *)vmvector_lookup(coarse_stub_areas, stub_pc);
    else {
        DOCHECK(CHKLVL_DEFAULT + 1,
                { /* PR 307698: perf hit */
                  ASSERT(info == vmvector_lookup(coarse_stub_areas, stub_pc));
                });
    }
    if (info != NULL) {
        return info->fcache_return_prefix;
    }
    return NULL;
}

/* Passing in stub_pc's info avoids a vmvector lookup */
cache_pc
trace_head_return_coarse_prefix(cache_pc stub_pc, coarse_info_t *info /*OPTIONAL*/)
{
    if (info == NULL)
        info = (coarse_info_t *)vmvector_lookup(coarse_stub_areas, stub_pc);
    else {
        DOCHECK(CHKLVL_DEFAULT + 1,
                { /* PR 307698: perf hit */
                  ASSERT(info == vmvector_lookup(coarse_stub_areas, stub_pc));
                });
    }
    if (info != NULL) {
        return info->trace_head_return_prefix;
    }
    return NULL;
}

/* Either the stub pc or the cti pc will work, as indirect stubs are inlined */
cache_pc
get_coarse_ibl_prefix(dcontext_t *dcontext, cache_pc stub_pc,
                      ibl_branch_type_t branch_type)
{
    /* Indirect stubs are inlined in the cache */
    coarse_info_t *info = (coarse_info_t *)get_fcache_coarse_info(stub_pc);
    if (info != NULL) {
        if (branch_type == IBL_RETURN)
            return info->ibl_ret_prefix;
        else if (branch_type == IBL_INDCALL)
            return info->ibl_call_prefix;
        else if (branch_type == IBL_INDJMP)
            return info->ibl_jmp_prefix;
        else
            ASSERT_NOT_REACHED();
    }
    return NULL;
}

bool
in_coarse_stubs(cache_pc pc)
{
    return (vmvector_lookup(coarse_stub_areas, pc) != NULL);
}

bool
in_coarse_stub_prefixes(cache_pc pc)
{
    coarse_info_t *info = get_stub_coarse_info(pc);
    if (info != NULL) {
        return (pc >= info->fcache_return_prefix &&
                pc < info->fcache_return_prefix +
                        COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info)) *
                            num_coarse_stubs_for_prefix(info));
    }
    return false;
}

/* If target is a coarse ibl prefix, returns its target (i.e., the final
 * ibl routine target); else, returns the original target passed in.
 */
cache_pc
coarse_deref_ibl_prefix(dcontext_t *dcontext, cache_pc target)
{
    coarse_info_t *info = get_stub_coarse_info(target);
    if (info != NULL) {
        if (target >= info->ibl_ret_prefix && target <= info->ibl_jmp_prefix) {
#ifdef X86
            ASSERT(*target == JMP_OPCODE);
            return (cache_pc)PC_RELATIVE_TARGET(target + 1);
#elif defined(ARM)
            /* FIXME i#1551: NYI on ARM */
            ASSERT_NOT_IMPLEMENTED(false);
            return NULL;
#endif /* X86/ARM */
        }
    }
    return target;
}

coarse_info_t *
get_stub_coarse_info(cache_pc pc)
{
    return (coarse_info_t *)vmvector_lookup(coarse_stub_areas, pc);
}

/* Returns the total size needed for stubs (including prefixes) if info is frozen. */
size_t
coarse_frozen_stub_size(dcontext_t *dcontext, coarse_info_t *info, uint *num_fragments,
                        uint *num_stubs)
{
    coarse_stubs_iterator_t csi;
    cache_pc pc;
    size_t size = 0;
    size_t stub_size = COARSE_STUB_ALLOC_SIZE(COARSE_32_FLAG(info));
    uint num_unlinked = 0;
    uint num_inter = 0;
    uint num_intra = 0;
    ASSERT(info != NULL);
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    ASSERT_OWN_MUTEX(true, &info->lock);
    if (info->stubs == NULL) /* lazily initialized, so common to have empty units */
        return size;
    LOG(THREAD, LOG_LINKS, 4, "coarse_frozen_stub_size %s\n", info->module);
    coarse_stubs_iterator_start(info, &csi);
    /* we do include the prefix size here */
    size += stub_size * num_coarse_stubs_for_prefix(info);
    LOG(THREAD, LOG_LINKS, 2, "coarse_frozen_stub_size %s: %d prefix\n", info->module,
        size);
    for (pc = coarse_stubs_iterator_next(&csi); pc != NULL;
         pc = coarse_stubs_iterator_next(&csi)) {
        if (entrance_stub_linked(pc, info)) {
            cache_pc target = entrance_stub_jmp_target(pc);
            /* When frozen we keep trace head stubs and only eliminate
             * stubs that are linked and pointing at this unit
             */
            if (get_fcache_coarse_info(target) != info) {
                size += stub_size;
                num_inter++;
            } else {
                num_intra++;
            }
        } else {
            /* Lazy linking is only for extra-unit targets so an unlinked
             * stub must have no intra-unit target
             */
            size += stub_size;
            num_unlinked++; /* trace head, or just no target hit yet */
        }
    }
    coarse_stubs_iterator_stop(&csi);
    LOG(THREAD, LOG_LINKS, 2,
        "coarse_frozen_stub_size %s: %d intra, %d inter, %d unlinked => %d bytes\n",
        info->module, num_intra, num_inter, num_unlinked, size);
    if (num_fragments != NULL)
        *num_fragments = num_intra;
    if (num_stubs != NULL)
        *num_stubs = num_inter + num_unlinked;
    ASSERT(size ==
           stub_size * (num_inter + num_unlinked + num_coarse_stubs_for_prefix(info)));
    return size;
}

/* Intended to be called after freezing shifts a unit's fragments around.
 * Updates the stub stored in the incoming entry for this outoing link.
 * src_info should be the old, currently-in-vmareas info.
 * Does not dereference old_stub (so ok if now invalid memory), but assumes
 * that new_stub is already linked properly.
 */
void
coarse_update_outgoing(dcontext_t *dcontext, cache_pc old_stub, cache_pc new_stub,
                       coarse_info_t *src_info, bool replace)
{
    cache_pc target_tag = entrance_stub_target_tag(new_stub, src_info);
    /* ASSUMPTION: coarse-grain are always shared and cannot target private */
    fragment_t *targetf = fragment_lookup_same_sharing(dcontext, target_tag, FRAG_SHARED);
    DOCHECK(CHKLVL_DEFAULT + 1,
            { /* PR 307698: perf hit */
              ASSERT(entrance_stub_linked(new_stub, NULL /*may not point to src_info*/));
            });
    if (targetf != NULL) {
        /* targeting a real fragment_t */
        LOG(THREAD, LOG_LINKS, 4,
            "    %s coarse link [" PFX "=>" PFX "] -> F%d(" PFX ")." PFX "\n",
            replace ? "updating" : "adding", old_stub, new_stub, targetf->id,
            targetf->tag, FCACHE_ENTRY_PC(targetf));
        ASSERT(entrance_stub_jmp_target(new_stub) == FCACHE_ENTRY_PC(targetf));
        if (replace) {
            direct_linkstub_t *l;
            direct_linkstub_t proxy;
            set_fake_direct_linkstub(&proxy, target_tag, old_stub);
            l = (direct_linkstub_t *)incoming_find_link(
                dcontext, (fragment_t *)&coarse_fragment, (linkstub_t *)&proxy, targetf);
            ASSERT(l != NULL);
            ASSERT(LINKSTUB_NORMAL_DIRECT(l->cdl.l.flags));
            l->stub_pc = new_stub;
        } else {
            coarse_link_to_fine(dcontext, new_stub, (fragment_t *)&coarse_fragment,
                                targetf, false /*just add incoming*/);
        }
    } else {
        cache_pc target_pc = entrance_stub_jmp_target(new_stub);
        coarse_info_t *target_info = get_fcache_coarse_info(target_pc);
        /* can only be NULL if targeting fine fragment, which is covered above! */
        ASSERT(target_info != NULL);
        if (target_info != src_info) {
            coarse_incoming_t *e;
            DEBUG_DECLARE(bool found = false;)
            LOG(THREAD, LOG_LINKS, 4,
                "    %s coarse link [" PFX "=>" PFX "] -> %s " PFX "\n",
                replace ? "updating" : "adding", old_stub, new_stub, target_info->module,
                target_tag);
            if (replace) {
                d_r_mutex_lock(&target_info->incoming_lock);
                for (e = target_info->incoming; e != NULL; e = e->next) {
                    if (e->coarse && e->in.stub_pc == old_stub) {
                        e->in.stub_pc = new_stub;
                        DODEBUG(found = true;);
                        break;
                    }
                }
                DOLOG(5, LOG_LINKS, {
                    LOG(THREAD, LOG_LINKS, 4, "after updating outgoing, target:\n");
                    print_coarse_incoming(dcontext, target_info);
                });
                d_r_mutex_unlock(&target_info->incoming_lock);
                ASSERT(found);
            } else {
                prepend_new_coarse_incoming(target_info, new_stub, NULL);
            }
        } else {
            ASSERT_NOT_REACHED(); /* currently caller checks for intra */
            /* an intra-unit link, so no incoming entry */
            LOG(THREAD, LOG_LINKS, 4,
                "    not updating coarse link to self " PFX " -> %s " PFX "\n", old_stub,
                target_info->module, target_tag);
        }
    }
}

/* Intended to be called after freezing has shifted a unit's fragments around.
 * Re-links incoming links to point at their targets' new locations.
 * Does NOT update outgoing links (which should be done incrementally
 * at freeze time when both old and new stub pcs are known).
 * Due to lock rank order, caller must hold change_linking_lock in addition
 * to info->lock.  Caller is also assumed to have stopped the world.
 */
void
coarse_unit_shift_links(dcontext_t *dcontext, coarse_info_t *info)
{
    /* Re-link incoming */
    coarse_incoming_t *e;
    cache_pc new_tgt;
    bool hot_patch = (dynamo_all_threads_synched ? NOT_HOT_PATCHABLE : HOT_PATCHABLE);
    ASSERT(info != NULL);
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    ASSERT_OWN_MUTEX(!dynamo_all_threads_synched, &info->lock);
    if (info->stubs == NULL) /* lazily initialized, so common to have empty units */
        return;
    LOG(THREAD, LOG_LINKS, 4, "coarse_unit_shift_links %s\n", info->module);

    d_r_mutex_lock(&info->incoming_lock);
    DOLOG(5, LOG_LINKS, {
        LOG(THREAD, LOG_LINKS, 4, "about to patch all incoming links:\n");
        print_coarse_incoming(dcontext, info);
    });
    for (e = info->incoming; e != NULL; e = e->next) {
        new_tgt = NULL;
        if (e->coarse) {
            /* coarse never have incoming structs for unlinked links */
            app_pc tag = entrance_stub_target_tag(e->in.stub_pc, info);
            fragment_coarse_lookup_in_unit(dcontext, info, tag, NULL, &new_tgt);
            ASSERT(new_tgt != NULL);
            link_entrance_stub(dcontext, e->in.stub_pc, new_tgt, hot_patch, NULL);
        } else {
            linkstub_t *l;
            app_pc tag = NULL;
            for (l = e->in.fine_l; l != NULL; l = LINKSTUB_NEXT_INCOMING(l)) {
                /* fine can have incoming structs yet not be linked (trace head, etc.) */
                if (TEST(LINK_LINKED, l->flags)) {
                    fragment_t *in_f = linkstub_fragment(dcontext, l);
                    if (tag == NULL) {
                        /* unprotect on demand (caller will re-protect) */
                        SELF_PROTECT_CACHE(dcontext, in_f, WRITABLE);
                        tag = EXIT_TARGET_TAG(dcontext, in_f, l);
                        fragment_coarse_lookup_in_unit(dcontext, info, tag, NULL,
                                                       &new_tgt);
                    } else
                        ASSERT(EXIT_TARGET_TAG(dcontext, in_f, l) == tag);
                    ASSERT(new_tgt != NULL);
                    patch_branch(FRAG_ISA_MODE(in_f->flags), EXIT_CTI_PC(in_f, l),
                                 new_tgt, hot_patch);
                }
            }
        }
    }
    d_r_mutex_unlock(&info->incoming_lock);
}

/* Updates the info pointers embedded in the coarse_stub_areas vector */
void
coarse_stubs_set_info(coarse_info_t *info)
{
    special_heap_set_vector_data(info->stubs, info);
}

void
coarse_stubs_set_end_pc(coarse_info_t *info, byte *end_pc)
{
    DEBUG_DECLARE(bool ok =) special_heap_set_unit_end(info->stubs, end_pc);
    ASSERT(ok);
    info->stubs_end_pc = end_pc;
}
