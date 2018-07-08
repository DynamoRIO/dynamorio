/* **********************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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
#include "drmgr.h"
#include "hashtable.h"
#include <string.h> /* memset */

#define VERBOSE 1

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

/****************************************************************************/
/* Global declarations. */

#ifdef SHOW_RESULTS
static int num_traces;
static int num_complete_inlines;
#endif

static void
event_exit(void);
static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data);
static void
event_fragment_deleted(void *drcontext, void *tag);
static dr_custom_trace_action_t
query_end_trace(void *drcontext, void *trace_tag, void *next_tag);

/****************************************************************************/
/* We use a hashtable to know if a particular tag is for a call trace or a
 * normal back branch trace.
 */

typedef struct _trace_head_entry_t {
    void *tag;
    bool is_trace_head;
    bool has_ret;
    /* We have to end at the next block after we see a return. */
    int end_next;
    /* Some callees are too large to inline, so we have a size limit. */
    uint size;
    /* We use a ref count so we know when to remove in the presence of
     * thread-private duplicated blocks.
     */
    uint refcount;
    struct _trace_head_entry_t *next;
} trace_head_entry_t;

/* Global hash table. */
static hashtable_t head_table;
#define HASH_BITS 13

/* Max call-trace size */
#define INLINE_SIZE_LIMIT (4 * 1024)

static trace_head_entry_t *
create_trace_head_entry(void *tag)
{
    trace_head_entry_t *e = (trace_head_entry_t *)dr_global_alloc(sizeof(*e));
    e->tag = tag;
    e->end_next = 0;
    e->size = 0;
    e->has_ret = false;
    e->is_trace_head = false;
    e->refcount = 1;
    return e;
}

static void
free_trace_head_entry(void *entry)
{
    trace_head_entry_t *e = (trace_head_entry_t *)entry;
    dr_global_free(e, sizeof(*e));
}

/****************************************************************************/

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'inline'", "http://dynamorio.org/issues");
    if (!drmgr_init())
        DR_ASSERT(false);

    hashtable_init_ex(&head_table, HASH_BITS, HASH_INTPTR, false /*!strdup*/,
                      false /*synchronization is external*/, free_trace_head_entry, NULL,
                      NULL);

    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_analyze_bb, NULL, NULL))
        DR_ASSERT(false);
    dr_register_delete_event(event_fragment_deleted);
    dr_register_end_trace_event(query_end_trace);

    /* Make it easy to tell from the log file which client executed. */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'inline' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* Ask for best-effort printing to cmd window.  Must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client inline is running\n");
    }
#endif
}

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    /* Display the results! */
    char msg[512];
    int len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                          "Inlining results:\n"
                          "  Number of traces: %d\n"
                          "  Number of complete inlines: %d\n",
                          num_traces, num_complete_inlines);
    DR_ASSERT(len > 0);
    msg[sizeof(msg) / sizeof(msg[0]) - 1] = '\0';
    DISPLAY_STRING(msg);
#endif
    hashtable_delete(&head_table);
    if (!drmgr_unregister_bb_instrumentation_event(event_analyze_bb))
        DR_ASSERT(false);
    drmgr_exit();
}

/****************************************************************************/
/* the work itself */

static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data)
{
    instr_t *instr;
    trace_head_entry_t *e = NULL;
    if (translating)
        return DR_EMIT_DEFAULT;
    for (instr = instrlist_first_app(bb); instr != NULL;
         instr = instr_get_next_app(instr)) {
        /* Blocks containing calls are trace heads. */
        if (instr_is_call(instr)) {
            dr_mark_trace_head(drcontext, tag);
            hashtable_lock(&head_table);
            e = hashtable_lookup(&head_table, tag);
            if (e == NULL) {
                e = create_trace_head_entry(tag);
                if (!hashtable_add(&head_table, tag, (void *)e))
                    DR_ASSERT(false);
            } else
                e->refcount++;
            e->is_trace_head = true;
            hashtable_unlock(&head_table);
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: marking bb " PFX " as call trace head\n", tag);
#endif
            /* Doesn't matter what's in rest of the bb. */
            return DR_EMIT_DEFAULT;
        } else if (instr_is_return(instr)) {
            hashtable_lock(&head_table);
            e = hashtable_lookup(&head_table, tag);
            if (e == NULL) {
                e = create_trace_head_entry(tag);
                if (!hashtable_add(&head_table, tag, (void *)e))
                    DR_ASSERT(false);
            } else
                e->refcount++;
            e->has_ret = true;
            hashtable_unlock(&head_table);
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: marking bb " PFX " as return trace head\n", tag);
#endif
        }
    }
    return DR_EMIT_DEFAULT;
}

/* To keep the size of our hashtable down. */
static void
event_fragment_deleted(void *drcontext, void *tag)
{
    trace_head_entry_t *e;
    hashtable_lock(&head_table);
    e = hashtable_lookup(&head_table, tag);
    if (e != NULL) {
        e->refcount--;
        if (e->refcount == 0)
            hashtable_remove(&head_table, tag);
    }
    hashtable_unlock(&head_table);
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
    /* If this is a call trace, only end on the block AFTER a return
     *   (need to get the return inlined!).
     * If this is a standard back branch trace, end it if we see a
     *   block with a call (so that we'll go into the call trace).
     *   otherwise return 0 and let DynamoRIO determine whether to
     *   terminate the trace now.
     */
    trace_head_entry_t *e;
    hashtable_lock(&head_table);
    e = hashtable_lookup(&head_table, trace_tag);
    if (e == NULL || !e->is_trace_head) {
        e = hashtable_lookup(&head_table, next_tag);
        if (e == NULL || !e->is_trace_head) {
            hashtable_unlock(&head_table);
            return CUSTOM_TRACE_DR_DECIDES;
        } else {
            /* We've found a call: end this trace now so it won't keep going and
             * end up never entering the call trace.
             */
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: ending trace " PFX " before block " PFX " containing call\n",
                   trace_tag, next_tag);
#endif
#ifdef SHOW_RESULTS
            num_traces++;
#endif
            hashtable_unlock(&head_table);
            return CUSTOM_TRACE_END_NOW;
        }
    } else if (e->end_next > 0) {
        e->end_next--;
        if (e->end_next == 0) {
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: ending trace " PFX " before " PFX "\n", trace_tag, next_tag);
#endif
#ifdef SHOW_RESULTS
            num_complete_inlines++;
            num_traces++;
#endif
            hashtable_unlock(&head_table);
            return CUSTOM_TRACE_END_NOW;
        }
    } else {
        trace_head_entry_t *nxte = hashtable_lookup(&head_table, next_tag);
        uint size = dr_fragment_size(drcontext, next_tag);
        e->size += size;
        if (e->size > INLINE_SIZE_LIMIT) {
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: ending trace " PFX " before " PFX
                   " because reached size limit\n",
                   trace_tag, next_tag);
#endif
#ifdef SHOW_RESULTS
            num_traces++;
#endif
            hashtable_unlock(&head_table);
            return CUSTOM_TRACE_END_NOW;
        }
        if (nxte != NULL && nxte->has_ret && !nxte->is_trace_head) {
            /* End trace after NEXT block */
            e->end_next = 2;
#ifdef VERBOSE
            dr_log(drcontext, DR_LOG_ALL, 3,
                   "inline: going to be ending trace " PFX " after " PFX "\n", trace_tag,
                   next_tag);
#endif
            hashtable_unlock(&head_table);
            return CUSTOM_TRACE_CONTINUE;
        }
    }
    /* Do not end trace */
#ifdef VERBOSE
    dr_log(drcontext, DR_LOG_ALL, 3, "inline: NOT ending trace " PFX " after " PFX "\n",
           trace_tag, next_tag);
#endif
    hashtable_unlock(&head_table);
    return CUSTOM_TRACE_CONTINUE;
}
