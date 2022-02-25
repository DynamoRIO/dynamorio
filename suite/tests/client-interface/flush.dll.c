/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include <string.h>

#define MINSERT instrlist_meta_preinsert

/* FIXME - module events are not supported on Linux */

#ifdef WINDOWS
static app_pc start = NULL;
static app_pc end = NULL;
#endif

static bool use_unlink = false;
static bool delay_flush_at_next_build = false;

static int bb_build_count = 0;
static uint callback_count = 0;

/* Keep a list that tracks which tags have been created and deleted.
 * We need to make sure we're informed of all flushed fragments.
 * The list is global -- we should use a mutex when accessing, but
 * the test is single threaded.
 */
typedef struct _elem_t {
    void *tag;
    int count;
    struct _elem_t *next;
    struct _elem_t *prev;
} elem_t;

elem_t *head = NULL;
elem_t *tail = NULL;

static elem_t *
find(void *tag)
{
    elem_t *iter = head;
    while (iter) {
        if (iter->tag == tag)
            return iter;
        iter = iter->next;
    }

    return NULL;
}

static void
increment(void *tag)
{
    elem_t *elem;

    elem = find(tag);
    if (elem != NULL) {
        elem->count++;
    } else {
        elem = dr_global_alloc(sizeof(elem_t));

        elem->tag = tag;
        elem->count = 1;
        elem->next = NULL;
        elem->prev = NULL;

        if (head == NULL) {
            head = elem;
            tail = elem;
        } else {
            tail->next = elem;
            elem->prev = tail;
            tail = elem;
        }
    }
}

static void
decrement(void *tag)
{
    elem_t *elem;

    elem = find(tag);
    if (elem == NULL) {
        dr_fprintf(STDERR, "ERROR removing " PFX "\n", tag);
    } else {
        elem->count--;

        if (elem->count == 0) {
            if (head == elem) {
                head = elem->next;
            }
            if (tail == elem) {
                tail = elem->prev;
            }
            if (elem->next) {
                elem->next->prev = elem->prev;
            }
            if (elem->prev) {
                elem->prev->next = elem->next;
            }

            dr_global_free(elem, sizeof(elem_t));
        }
    }
}

static void
exit_event(void)
{
    int count = 0;

    elem_t *iter = head;
    while (iter) {
        elem_t *next = iter->next;
        count += iter->count;
        dr_fprintf(STDERR, "ERROR: " PFX " undeleted\n", iter->tag);
        dr_global_free(iter, sizeof(elem_t));
        iter = next;
    }

    dr_fprintf(STDERR, "%d undeleted fragments\n", count);
    /* get around nondeterminism */
    if (bb_build_count >= 5 && bb_build_count <= 15)
        dr_fprintf(STDERR, "constructed BB 5-15 times\n");
    else
        dr_fprintf(STDERR, "constructed BB %d times\n", bb_build_count);
}

static dr_emit_flags_t
trace_event(void *drcontext, void *tag, instrlist_t *trace, bool translating)
{
    if (!translating)
        increment(tag);
    return DR_EMIT_DEFAULT;
}

static void
deleted_event(void *dcontext, void *tag)
{
    decrement(tag);
}

void
flush_event(int flush_id)
{
    dr_fprintf(STDERR, "Flush completion id=%d\n", flush_id);
}

static void
synch_flush_completion_callback(void *user_data)
{
    dr_fprintf(STDERR, "in synch_flush_completion_callback, user_data=%d\n",
               *(int *)user_data);
}

static void
callback(void *tag, app_pc next_pc)
{
    callback_count++;

    /* Flush all fragments containing this tag twice every hundred calls alternating
     * between a sync_all and delay flush (if available) and an unlink and delay flush
     * (if available). */
    if (callback_count % 100 == 0) {
        if (callback_count % 200 == 0) {
            /* For windows test dr_flush_region() half the time */
            dr_mcontext_t mcontext;
            mcontext.size = sizeof(mcontext);
            mcontext.flags = DR_MC_ALL;
            dr_delay_flush_region((app_pc)tag - 20, 30, callback_count, flush_event);
            void *drcontext = dr_get_current_drcontext();
            dr_get_mcontext(drcontext, &mcontext);
            mcontext.pc = dr_app_pc_as_jump_target(dr_get_isa_mode(drcontext), next_pc);
            dr_flush_region_ex(tag, 1, synch_flush_completion_callback, &callback_count);
            dr_redirect_execution(&mcontext);
            *(volatile uint *)NULL = 0; /* ASSERT_NOT_REACHED() */
        } else if (use_unlink) {
            /* Test dr_unlink_flush_region() half the time (if available).
             * FIXME - extend once we add unlink callback. */
            delay_flush_at_next_build = true;
            dr_unlink_flush_region(tag, 1);
        }
    }
}

#ifdef WINDOWS
static bool
string_match(const char *str1, const char *str2)
{
    if (str1 == NULL || str2 == NULL)
        return false;

    while (*str1 == *str2) {
        if (*str1 == '\0')
            return true;
        str1++;
        str2++;
    }

    return false;
}

static void
module_load_event(void *dcontext, const module_data_t *data, bool loaded)
{
    if (string_match(dr_module_preferred_name(data), "client.flush.exe")) {
        start = data->start;
        end = data->end;
    }
}
#endif

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr;
    if (!translating)
        increment(tag);

        /* I'm looking for a specific BB in the test .exe.  I've marked
         * it with a couple nops.
         */
#ifdef WINDOWS
    if ((app_pc)tag >= start && (app_pc)tag < end) {
#endif
        instr = instrlist_first(bb);

        if (instr_is_nop(instr)) {
            instr_t *next = instr_get_next(instr);

            /* The test app uses two nops as a marker to identify a specific bb.  Since
             * 2 nop instructions in a row aren't that uncommon on Linux (where we can't
             * restrict our search to just the test.exe module) we use an unusual nop
             * for the second one: xchg xbp, xbp */
            if (next != NULL && instr_is_nop(next) && instr_get_opcode(next) == OP_xchg &&
                instr_writes_to_exact_reg(next, REG_XBP, DR_QUERY_DEFAULT)) {

                bb_build_count++;

                if (delay_flush_at_next_build) {
                    delay_flush_at_next_build = false;
                    dr_delay_flush_region((app_pc)tag - 20, 30, callback_count,
                                          flush_event);
                }

                dr_insert_clean_call_ex(drcontext, bb, instr, (void *)callback,
                                        DR_CLEANCALL_READS_APP_CONTEXT, 2,
                                        OPND_CREATE_INTPTR(tag),
                                        OPND_CREATE_INTPTR(instr_get_app_pc(instr)));
            }
        }
#ifdef WINDOWS
    }
#endif
    return DR_EMIT_DEFAULT;
}

static void
kernel_xfer_event(void *drcontext, const dr_kernel_xfer_info_t *info)
{
    /* Test kernel xfer on dr_redirect_execution */
    dr_fprintf(STDERR, "%s: type %d\n", __FUNCTION__, info->type);
    ASSERT(info->source_mcontext != NULL);
    dr_mcontext_t mc;
    mc.size = sizeof(mc);
    mc.flags = DR_MC_CONTROL;
    bool ok = dr_get_mcontext(drcontext, &mc);
    ASSERT(ok);
    ASSERT(mc.pc == info->target_pc);
    ASSERT(mc.xsp == info->target_xsp);
    mc.flags = DR_MC_ALL;
    ok = dr_get_mcontext(drcontext, &mc);
    ASSERT(ok);
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    const char *options = dr_get_options(id);
    dr_fprintf(STDERR, "options = %s\n", options);
    if (options != NULL && strstr(options, "use_unlink") != NULL) {
        use_unlink = true;
    }
#ifdef WINDOWS
    dr_register_module_load_event(module_load_event);
#endif
    dr_register_exit_event(exit_event);
    dr_register_trace_event(trace_event);
    dr_register_delete_event(deleted_event);
    dr_register_bb_event(bb_event);
    dr_register_kernel_xfer_event(kernel_xfer_event);
}
