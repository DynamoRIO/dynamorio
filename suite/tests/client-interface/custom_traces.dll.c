/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * trace-inline.c
 *
 * Uses the custom trace API to inline entire callees into traces.
 */

#include "dr_api.h"
#include <string.h> /* memset */

/****************************************************************************/
/* global */

static int num_complete_inlines;

static void *htable_mutex; /* for multithread support */

static void
event_exit(void);
static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating);
static void
event_fragment_deleted(void *drcontext, void *tag);
static dr_custom_trace_action_t
query_end_trace(void *drcontext, void *trace_tag, void *next_tag);

/****************************************************************************/
/* hashtable so we know if a particular tag is for a call trace or a
 * normal back branch trace
 */

typedef struct _trace_head_entry_t {
    void *tag;
    bool is_trace_head;
    bool has_ret;
    /* have to end at next block after see return */
    int end_next;
    /* some callees are too large to inline, so have size limit */
    uint size;
    struct _trace_head_entry_t *next;
} trace_head_entry_t;

/* global table */
static trace_head_entry_t **htable;

/* max call-trace size */
#define INLINE_SIZE_LIMIT (4 * 1024)

/* no instruction alignment -> use the lsb! */
#define HASH_MASK(num_bits) ((~0U) >> (32 - (num_bits)))
#define HASH_FUNC_BITS(val, num_bits) ((val) & (HASH_MASK(num_bits)))
#define HASH_FUNC(val, mask) ((val) & (mask))
#define HASHTABLE_SIZE(num_bits) (1U << (num_bits))

#define HASH_BITS 13
#define TABLE_SIZE HASHTABLE_SIZE(HASH_BITS) * sizeof(trace_head_entry_t *)

/* if drcontext == NULL uses global memory */
static trace_head_entry_t **
htable_create(void *drcontext)
{
    trace_head_entry_t **table = (trace_head_entry_t **)dr_global_alloc(TABLE_SIZE);
    /* assume during process init so no lock needed */
    memset(table, 0, TABLE_SIZE);
    return table;
}

/* if drcontext == NULL uses global memory */
static void
htable_free(void *drcontext, trace_head_entry_t **table)
{
    /* assume during process exit so no lock needed */
    int i;
    /* clean up memory */
    for (i = 0; i < HASHTABLE_SIZE(HASH_BITS); i++) {
        trace_head_entry_t *e = table[i];
        while (e) {
            trace_head_entry_t *nexte = e->next;
            dr_global_free(e, sizeof(trace_head_entry_t));
            e = nexte;
        }
        table[i] = NULL;
    }
    dr_global_free(table, TABLE_SIZE);
}

/* Caller must hold htable_mutex if drcontext == NULL. */
static trace_head_entry_t *
add_trace_head_entry(void *drcontext, void *tag)
{
    trace_head_entry_t **table = htable;
    trace_head_entry_t *e;
    uint hindex;
    e = (trace_head_entry_t *)dr_global_alloc(sizeof(trace_head_entry_t));
    e->tag = tag;
    e->end_next = 0;
    e->size = 0;
    e->has_ret = false;
    e->is_trace_head = false;
    hindex = (uint)HASH_FUNC_BITS((ptr_uint_t)tag, HASH_BITS);
    e->next = table[hindex];
    table[hindex] = e;
    return e;
}

/* Lookup an entry by pc and return a pointer to the corresponding entry .
 * Returns NULL if no such entry exists.
 * Caller must hold htable_mutex if drcontext == NULL.
 */
static trace_head_entry_t *
lookup_trace_head_entry(void *drcontext, void *tag)
{
    trace_head_entry_t **table = htable;
    trace_head_entry_t *e;
    uint hindex;
    hindex = (uint)HASH_FUNC_BITS((ptr_uint_t)tag, HASH_BITS);
    for (e = table[hindex]; e; e = e->next) {
        if (e->tag == tag)
            return e;
    }
    return NULL;
}

/* Lookup an entry by tag and index and delete it.
 * Returns false if no such entry exists.
 * Caller must hold htable_mutex if drcontext == NULL.
 */
static bool
remove_trace_head_entry(void *drcontext, void *tag)
{
    trace_head_entry_t **table = htable;
    trace_head_entry_t *e, *prev;
    uint hindex;
    hindex = (uint)HASH_FUNC_BITS((ptr_uint_t)tag, HASH_BITS);
    for (prev = NULL, e = table[hindex]; e; prev = e, e = e->next) {
        if (e->tag == tag) {
            if (prev)
                prev->next = e->next;
            else
                table[hindex] = e->next;
            dr_global_free(e, sizeof(trace_head_entry_t));
            return true;
        }
    }
    return false;
}

/****************************************************************************/

DR_EXPORT void
dr_init(client_id_t id)
{
    htable_mutex = dr_mutex_create();

    /* global HASH_BITS-bit addressed hash table */
    htable = htable_create(NULL /*global*/);

    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_basic_block);
    dr_register_delete_event(event_fragment_deleted);
    dr_register_end_trace_event(query_end_trace);

    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'inline' initializing\n");
    num_complete_inlines = 0;
}

static void
event_exit(void)
{
    /* On WOW64xpsp2 I see 440+, but only 230+ on 2k3 */
    if (num_complete_inlines > 100)
        dr_fprintf(STDERR, "Inlined callees in >100 traces\n");
    else
        dr_fprintf(STDERR, "Inlined callees in %d traces: < 100!!!\n",
                   num_complete_inlines);
    htable_free(NULL /*global*/, htable);
    dr_mutex_destroy(htable_mutex);
}

/****************************************************************************/
/* the work itself */

static dr_emit_flags_t
event_basic_block(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating)
{
    instr_t *instr;
    trace_head_entry_t *e = NULL;
    if (translating)
        return DR_EMIT_DEFAULT;
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        /* blocks containing calls are trace heads */
        if (instr_is_call(instr)) {
            dr_mark_trace_head(drcontext, tag);
            dr_mutex_lock(htable_mutex);
            e = add_trace_head_entry(NULL, tag);
            e->is_trace_head = true;
            dr_mutex_unlock(htable_mutex);
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3, "inline: marking bb " PFX " as trace head\n",
                   tag);
#endif
            /* doesn't matter what's in rest of bb */
            return DR_EMIT_DEFAULT;
        } else if (instr_is_return(instr)) {
            dr_mutex_lock(htable_mutex);
            e = add_trace_head_entry(NULL, tag);
            e->has_ret = true;
            dr_mutex_unlock(htable_mutex);
        }
    }
    return DR_EMIT_DEFAULT;
}

/* to keep the size of our hashtable down */
static void
event_fragment_deleted(void *drcontext, void *tag)
{
    dr_mutex_lock(htable_mutex);
    remove_trace_head_entry(NULL, tag);
    dr_mutex_unlock(htable_mutex);
}

/* Ask whether to end trace prior to adding next_tag fragment.
 * Return values:
 *   CUSTOM_TRACE_DR_DECIDES = use standard termination criteria
 *   CUSTOM_TRACE_END_NOW   = end trace
 *   CUSTOM_TRACE_CONTINUE  = do not end trace
 */
static dr_custom_trace_action_t
query_end_trace(void *drcontext, void *trace_tag, void *next_tag)
{
    /* if this is a call trace, only end on the block AFTER a return
     *   (need to get the return inlined!)
     * if this is a standard back branch trace, end it if we see a
     *   block with a call (so that we'll go into the call trace).
     *   otherwise return 0 and let DynamoRIO determine whether to
     *   terminate the trace now.
     */
    trace_head_entry_t *e;
    dr_mutex_lock(htable_mutex);
    e = lookup_trace_head_entry(NULL, trace_tag);
    if (e == NULL || !e->is_trace_head) {
        e = lookup_trace_head_entry(NULL, next_tag);
        if (e == NULL || !e->is_trace_head) {
            dr_mutex_unlock(htable_mutex);
            return CUSTOM_TRACE_DR_DECIDES;
        } else {
            /* we've found a call, end this trace now so it won't keep going and
             * end up never entering the call trace
             */
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: ending trace " PFX " before block " PFX " containing call\n",
                   trace_tag, next_tag);
#endif
            dr_mutex_unlock(htable_mutex);
            return CUSTOM_TRACE_END_NOW;
        }
    } else if (e->end_next > 0) {
        e->end_next--;
        if (e->end_next == 0) {
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: ending trace " PFX " before " PFX "\n", trace_tag, next_tag);
#endif
            num_complete_inlines++;
            dr_mutex_unlock(htable_mutex);
            return CUSTOM_TRACE_END_NOW;
        }
    } else {
        trace_head_entry_t *nxte = lookup_trace_head_entry(NULL, next_tag);
        uint size = dr_fragment_size(drcontext, next_tag);
        e->size += size;
        if (e->size > INLINE_SIZE_LIMIT) {
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: ending trace " PFX " before " PFX
                   " because reached size limit\n",
                   trace_tag, next_tag);
#endif
            dr_mutex_unlock(htable_mutex);
            return CUSTOM_TRACE_END_NOW;
        }
        if (nxte != NULL && nxte->has_ret && !nxte->is_trace_head) {
            /* end trace after NEXT block */
            e->end_next = 2;
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: going to be ending trace " PFX " after " PFX "\n", trace_tag,
                   next_tag);
#endif
            dr_mutex_unlock(htable_mutex);
            return CUSTOM_TRACE_CONTINUE;
        }
    }
    /* do not end trace */
#ifdef VERBOSE
    dr_log(drcontext, DR_LOG_ALL, 3, "inline: NOT ending trace " PFX " after " PFX "\n",
           trace_tag, next_tag);
#endif
    dr_mutex_unlock(htable_mutex);
    return CUSTOM_TRACE_CONTINUE;
}
