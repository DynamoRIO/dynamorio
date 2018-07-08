/* **********************************************************
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

/*
 * Tests re-instrumentation using the following scheme:
 * 1) Insert instrumentation normally in the BB event.
 * 2) In the callout in the instrumented code, directly replace
 *    the instrumentation.
 *
 * In contrast to cbr3, which uses a flushing scheme, we replace the
 * instrumented code directly with dr_replace_fragment().  As with
 * cbr3, we focus on cbr ops, inserting instrumentation to capture the
 * fallthrough and taken addresses.  After the first branch, we
 * re-instrument the BB to remove the instrumentation for the
 * direction taken.  If and when we see the other direction, we remove
 * all instrumentation for that branch.
 *
 * This test is adapted from a dynamic CFG builder, where we want to
 * record each control-flow edge, but we don't want to pay the
 * execution overhead of the instrumentation after we've recorded
 * the edge.
 */

#include "dr_api.h"
#include "client_tools.h"

#define MINSERT instrlist_meta_preinsert
#define inline __inline

/* We need a table to store the state of each cbr (i.e., "seen taken
 * edge", "seen fallthrough edge", or "seen both").  The test program
 * itself is small, so the following hash table is overkill, but I
 * guess it makes this client easily extendable.
 */
#define HASH_TABLE_SIZE 7919

typedef enum { CBR_NONE = 0x00, CBR_TAKEN = 0x01, CBR_NOT_TAKEN = 0x10 } cbr_state_t;

typedef struct _elem_t {
    struct _elem_t *next;
    cbr_state_t state;
    app_pc addr;
} elem_t;

typedef struct _list_t {
    elem_t *head;
    elem_t *tail;
} list_t;

/* Global hash table */
typedef list_t **hash_table_t;
hash_table_t table = NULL;

static inline elem_t *
new_elem(app_pc addr, cbr_state_t state)
{
    elem_t *elem = (elem_t *)dr_global_alloc(sizeof(elem_t));
    ASSERT(elem != NULL);

    elem->next = NULL;
    elem->addr = addr;
    elem->state = state;

    return elem;
}

static inline void
delete_elem(elem_t *elem)
{
    dr_global_free(elem, sizeof(elem_t));
}

static inline void
append(list_t *list, elem_t *elem)
{
    if (list->head == NULL) {
        ASSERT(list->tail == NULL);
        list->head = elem;
        list->tail = elem;
    } else {
        list->tail->next = elem;
        list->tail = elem;
    }
}

static inline elem_t *
find(list_t *list, app_pc addr)
{
    elem_t *elem = list->head;
    while (elem != NULL) {
        if (elem->addr == addr)
            return elem;
        elem = elem->next;
    }

    return NULL;
}

static inline list_t *
new_list()
{
    list_t *list = (list_t *)dr_global_alloc(sizeof(list_t));
    list->head = NULL;
    list->tail = NULL;
    return list;
}

static inline void
delete_list(list_t *list)
{
    elem_t *iter = list->head;
    while (iter != NULL) {
        elem_t *next = iter->next;
        delete_elem(iter);
        iter = next;
    }

    dr_global_free(list, sizeof(list_t));
}

hash_table_t
new_table()
{
    int i;
    hash_table_t table =
        (hash_table_t)dr_global_alloc(sizeof(list_t *) * HASH_TABLE_SIZE);

    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        table[i] = NULL;
    }

    return table;
}

void
delete_table(hash_table_t table)
{
    int i;
    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        if (table[i] != NULL) {
            delete_list(table[i]);
        }
    }

    dr_global_free(table, sizeof(list_t *) * HASH_TABLE_SIZE);
}

static inline uint
hash_func(app_pc addr)
{
    return (uint)((ptr_uint_t)addr % HASH_TABLE_SIZE);
}

elem_t *
lookup(hash_table_t table, app_pc addr)
{
    list_t *list = table[hash_func(addr)];
    if (list != NULL)
        return find(list, addr);

    return NULL;
}

void
insert(hash_table_t table, app_pc addr, cbr_state_t state)
{
    elem_t *elem = new_elem(addr, state);

    uint index = hash_func(addr);
    list_t *list = table[index];
    if (list == NULL) {
        list = new_list();
        table[index] = list;
    }

    append(list, elem);
}

/*
 * End hash table implementation
 */

static dr_emit_flags_t
instrument_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating);

static void
at_taken(app_pc src, app_pc targ, void *tag)
{
    instrlist_t *bb;
    void *drcontext = dr_get_current_drcontext();

    /*
     * Record the fact that we've seen the taken case.
     */
    elem_t *elem = lookup(table, src);
    ASSERT(elem != NULL);
    elem->state |= CBR_TAKEN;

    dr_fprintf(STDERR, "cbr taken\n");

    /*
     * Re-instrument and replace the fragment.
     */
    ASSERT(dr_bb_exists_at(drcontext, tag));
    bb = decode_as_bb(drcontext, tag);
    instrument_bb(drcontext, tag, bb, false, false);
    dr_replace_fragment(drcontext, tag, bb);
}

static void
at_not_taken(app_pc src, app_pc fall, void *tag)
{
    instrlist_t *bb;
    void *drcontext = dr_get_current_drcontext();

    /*
     * Record the fact that we've seen the fallthrough case.
     */
    elem_t *elem = lookup(table, src);
    ASSERT(elem != NULL);
    elem->state |= CBR_NOT_TAKEN;

    dr_fprintf(STDERR, "cbr not taken\n");

    /*
     * Re-instrument and replace the fragment.
     */
    ASSERT(dr_bb_exists_at(drcontext, tag));
    bb = decode_as_bb(drcontext, tag);
    instrument_bb(drcontext, tag, bb, false, false);
    dr_replace_fragment(drcontext, tag, bb);
}

static dr_emit_flags_t
instrument_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
              bool translating)
{
    instr_t *instr, *next_instr;

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);

        /*
         * Conditional branch. We can determine the target and
         * fallthrough addresses here, but we need to instrument if we
         * want to record the edge only if it actually executes at
         * runtime. Instead of using dr_insert_cbr_instrumentation,
         * we'll insert separate instrumentation for the taken and not
         * taken cases and remove it separately after we see each
         * case.
         */
        if (instr_is_cbr(instr)) {
            app_pc src = instr_get_app_pc(instr);

            cbr_state_t state;
            bool insert_taken, insert_not_taken;

            /* First look up the state of this branch so we
             * know what instrumentation to insert, if any.
             */
            elem_t *elem = lookup(table, src);

            if (elem == NULL) {
                state = CBR_NONE;
                insert(table, src, CBR_NONE);
            } else {
                state = elem->state;
            }

            insert_taken = (state & CBR_TAKEN) == 0;
            insert_not_taken = (state & CBR_NOT_TAKEN) == 0;

            if (insert_taken || insert_not_taken) {
                app_pc fall = (app_pc)decode_next_pc(drcontext, (byte *)src);
                app_pc targ = instr_get_branch_target_pc(instr);

                /*
                 * Redirect the cbr to jump to the 'taken' callout.
                 * We'll insert a 'not-taken' callout at fallthrough
                 * address.
                 */
                instr_t *label = INSTR_CREATE_label(drcontext);
                instr_set_meta(instr);
                instr_set_translation(instr, NULL);
                /* If this is a short cti, make sure it can reach its new target */
                if (instr_is_cti_short(instr)) {
                    /* if jecxz/loop we want to set the target of the long-taken
                     * so set instr to the return value
                     */
                    instr = instr_convert_short_meta_jmp_to_long(drcontext, bb, instr);
                }
                instr_set_target(instr, opnd_create_instr(label));

                if (insert_not_taken) {
                    /*
                     * Callout for the not-taken case
                     */
                    dr_insert_clean_call(drcontext, bb, NULL, (void *)at_not_taken,
                                         false /* don't save fp state */,
                                         3 /* 3 args for at_not_taken */,
                                         OPND_CREATE_INTPTR((ptr_uint_t)src),
                                         OPND_CREATE_INTPTR((ptr_uint_t)fall),
                                         OPND_CREATE_INTPTR((ptr_uint_t)tag));
                }

                /*
                 * Jump to the original fall-through address.
                 * (This should not be a meta-instruction).
                 */
                instrlist_preinsert(
                    bb, NULL,
                    INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(fall)), fall));

                /* label goes before the 'taken' callout */
                MINSERT(bb, NULL, label);

                if (insert_taken) {
                    /*
                     * Callout for the taken case
                     */
                    dr_insert_clean_call(drcontext, bb, NULL, (void *)at_taken,
                                         false /* don't save fp state */,
                                         3 /* 3 args for at_taken */,
                                         OPND_CREATE_INTPTR((ptr_uint_t)src),
                                         OPND_CREATE_INTPTR((ptr_uint_t)targ),
                                         OPND_CREATE_INTPTR((ptr_uint_t)tag));
                }

                /*
                 * Jump to the original target block (this should
                 * not be a meta-instruction).
                 */
                instrlist_preinsert(
                    bb, NULL,
                    INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(targ)), targ));
            }
        }
    }
    /* since our added instrumentation is not constant, we ask to store
     * translations now
     */
    return DR_EMIT_STORE_TRANSLATIONS;
}

app_pc start_pc = NULL;  /* pc to start instrumenting */
app_pc stop_pc = NULL;   /* pc to stop instrumenting */
bool instrument = false; /* insert instrumentation? */

dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    app_pc bb_addr = dr_fragment_app_pc(tag);
    dr_emit_flags_t flags = DR_EMIT_DEFAULT;
    if (bb_addr == start_pc) {
        instrument = true;
    } else if (bb_addr == stop_pc) {
        instrument = false;
    }

    if (instrument) {
        flags = instrument_bb(drcontext, tag, bb, for_trace, translating);
    }
    return flags;
}

void
dr_exit(void)
{
    delete_table(table);
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    /* Look up start_instrument() and stop_instrument() in the app.
     * These functions are markers that tell us when to start and stop
     * instrumenting.
     */
    module_data_t *prog = dr_lookup_module_by_name("client.cbr4.exe");
    ASSERT(prog != NULL);

    start_pc = (app_pc)dr_get_proc_address(prog->handle, "start_instrument");
    stop_pc = (app_pc)dr_get_proc_address(prog->handle, "stop_instrument");

    ASSERT(start_pc != NULL && stop_pc != NULL);
    dr_free_module_data(prog);

    table = new_table();

    dr_register_bb_event(bb_event);
    dr_register_exit_event(dr_exit);
}
