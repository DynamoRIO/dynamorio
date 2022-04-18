/* **********************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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
 * cbr.c
 *
 * This sample shows how to update or replace instrumented code after
 * it executes.  We focus on cbr instructions, inserting
 * instrumentation to record the fallthrough and taken addresses when
 * they first execute.  After a particular branch first executes, we
 * re-instrument the basic block to remove the instrumentation for the
 * direction taken.  If and when we see the other direction, we remove
 * all instrumentation for that branch.  We design this sample to
 * avoid the instrumentation overhead for a particular direction until
 * it is taken.  Furthermore, we remove all overhead for that
 * direction after it triggers.
 *
 * This sample might form part of a dynamic CFG builder, where we want
 * to record each control-flow edge, but we don't want to pay the
 * execution overhead of the instrumentation after we've noted the
 * edge.
 *
 * We use the following replacement scheme:
 * 1) In the BB event, insert instrumentation for both the taken and
 *    fallthrough edges.
 * 2) When the BB executes, note the direction taken and flush the
 *    fragment from the code cache.
 * 3) When the BB event triggers again, insert new instrumentation.
 */

#include "dr_api.h"
#include "drmgr.h"

#define MINSERT instrlist_meta_preinsert

#define ASSERT(x)                                            \
    do {                                                     \
        if (!(x)) {                                          \
            dr_printf("ASSERT failed on line %d", __LINE__); \
            dr_flush_file(STDOUT);                           \
            dr_abort();                                      \
        }                                                    \
    } while (0)

/* We need a table to store the state of each cbr (i.e., "seen taken
 * edge", "seen fallthrough edge", or "seen both").  We'll use a
 * simple hash table.
 */
#define HASH_TABLE_SIZE 7919

/* Possible cbr states */
typedef enum { CBR_NEITHER = 0x00, CBR_TAKEN = 0x01, CBR_NOT_TAKEN = 0x10 } cbr_state_t;

/* Each bucket in the hash table is a list of the following elements.
 * For each cbr, we store its address and its state.
 */
typedef struct _elem_t {
    struct _elem_t *next;
    cbr_state_t state;
    app_pc addr;
} elem_t;

typedef struct _list_t {
    elem_t *head;
    elem_t *tail;
} list_t;

/* We'll use one global hash table */
typedef list_t **hash_table_t;
hash_table_t global_table = NULL;

static elem_t *
new_elem(app_pc addr, cbr_state_t state)
{
    elem_t *elem = (elem_t *)dr_global_alloc(sizeof(elem_t));
    ASSERT(elem != NULL);

    elem->next = NULL;
    elem->addr = addr;
    elem->state = state;

    return elem;
}

static void
delete_elem(elem_t *elem)
{
    dr_global_free(elem, sizeof(elem_t));
}

static void
append_elem(list_t *list, elem_t *elem)
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

static elem_t *
find_elem(list_t *list, app_pc addr)
{
    elem_t *elem = list->head;
    while (elem != NULL) {
        if (elem->addr == addr)
            return elem;
        elem = elem->next;
    }

    return NULL;
}

static list_t *
new_list()
{
    list_t *list = (list_t *)dr_global_alloc(sizeof(list_t));
    list->head = NULL;
    list->tail = NULL;
    return list;
}

static void
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

static uint
hash_func(app_pc addr)
{
    return ((uint)(((ptr_uint_t)addr) % HASH_TABLE_SIZE));
}

elem_t *
lookup(hash_table_t table, app_pc addr)
{
    list_t *list = table[hash_func(addr)];
    if (list != NULL)
        return find_elem(list, addr);

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

    append_elem(list, elem);
}

/*
 * End hash table implementation
 */

/* Clean call for the 'taken' case */
static void
at_taken(app_pc src, app_pc targ)
{
    /*
     * We've found that the time taken to zero a large struct shows up as
     * noticeable overhead for optimized clients. So, instead of using partial
     * struct initialization, we set the fields required individually.
     */
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    void *drcontext = dr_get_current_drcontext();

    /*
     * Record the fact that we've seen the taken case.
     */
    elem_t *elem = lookup(global_table, src);
    ASSERT(elem != NULL);
    elem->state |= CBR_TAKEN;

    /* Remove the bb from the cache so it will be re-built the next
     * time it executes.
     */
    /* Since the flush will remove the fragment we're already in,
     * redirect execution to the target address.
     */
    dr_flush_region(src, 1);
    dr_get_mcontext(drcontext, &mcontext);
    mcontext.pc = dr_app_pc_as_jump_target(dr_get_isa_mode(drcontext), targ);
    dr_redirect_execution(&mcontext);
}

/* Clean call for the 'not taken' case */
static void
at_not_taken(app_pc src, app_pc fall)
{
    /*
     * We've found that the time taken to zero a large struct shows up as
     * noticeable overhead for optimized clients. So, instead of using partial
     * struct initialization, we set the fields required individually.
     */
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    void *drcontext = dr_get_current_drcontext();

    /*
     * Record the fact that we've seen the not_taken case.
     */
    elem_t *elem = lookup(global_table, src);
    ASSERT(elem != NULL);
    elem->state |= CBR_NOT_TAKEN;

    /* Remove the bb from the cache so it will be re-built the next
     * time it executes.
     */
    /* Since the flush will remove the fragment we're already in,
     * redirect execution to the fallthrough address.
     */
    dr_flush_region(src, 1);
    dr_get_mcontext(drcontext, &mcontext);
    mcontext.pc = fall;
    dr_redirect_execution(&mcontext);
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
    cbr_state_t state;
    bool insert_taken, insert_not_taken;
    app_pc src;
    elem_t *elem;

    /* conditional branch only */
    if (!instr_is_cbr(instr))
        return DR_EMIT_DEFAULT;

    /* We can determine the target and fallthrough addresses here, but we
     * want to note the edge if and when it actually executes at runtime.
     * Instead of using dr_insert_cbr_instrumentation(), we'll insert
     * separate instrumentation for the taken and not taken cases and
     * remove the instrumentation for an edge after it executes.
     */

    /* First look up the state of this branch so we
     * know what instrumentation to insert, if any.
     */
    src = instr_get_app_pc(instr);
    elem = lookup(global_table, src);

    if (elem == NULL) {
        state = CBR_NEITHER;
        insert(global_table, src, CBR_NEITHER);
    } else {
        state = elem->state;
    }

    insert_taken = (state & CBR_TAKEN) == 0;
    insert_not_taken = (state & CBR_NOT_TAKEN) == 0;

    if (insert_taken || insert_not_taken) {
        app_pc fall = (app_pc)decode_next_pc(drcontext, (byte *)src);
        app_pc targ = instr_get_branch_target_pc(instr);

        /* Redirect the existing cbr to jump to a callout for
         * the 'taken' case.  We'll insert a 'not-taken'
         * callout at the fallthrough address.
         */
        instr_t *label = INSTR_CREATE_label(drcontext);
        /* should be meta, and meta-instrs shouldn't have translations */
        instr_set_meta_no_translation(instr);
        /* it may not reach (in particular for x64) w/ our added clean call */
        if (instr_is_cti_short(instr)) {
            /* if jecxz/loop we want to set the target of the long-taken
             * so set instr to the return value
             */
            instr = instr_convert_short_meta_jmp_to_long(drcontext, bb, instr);
        }
        instr_set_target(instr, opnd_create_instr(label));

        if (insert_not_taken) {
            /* Callout for the not-taken case.  Insert after
             * the cbr (i.e., 3rd argument is NULL).
             */
            dr_insert_clean_call_ex(drcontext, bb, NULL, (void *)at_not_taken,
                                    DR_CLEANCALL_READS_APP_CONTEXT |
                                        DR_CLEANCALL_MULTIPATH,
                                    2 /* 2 args for at_not_taken */,
                                    OPND_CREATE_INTPTR(src), OPND_CREATE_INTPTR(fall));
        }

        /* After the callout, jump to the original fallthrough
         * address.  Note that this is an exit cti, and should
         * not be a meta-instruction.  Therefore, we use
         * preinsert instead of meta_preinsert, and we must
         * set the translation field.  On Windows, this jump
         * and the final jump below never execute since the
         * at_taken and at_not_taken callouts redirect
         * execution and never return.  However, since the API
         * expects clients to produced well-formed code, we
         * insert explicit exits from the block for Windows as
         * well as Linux.
         */
        instrlist_preinsert(
            bb, NULL, INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(fall)), fall));

        /* label goes before the 'taken' callout */
        MINSERT(bb, NULL, label);

        if (insert_taken) {
            /* Callout for the taken case */
            dr_insert_clean_call_ex(drcontext, bb, NULL, (void *)at_taken,
                                    DR_CLEANCALL_READS_APP_CONTEXT |
                                        DR_CLEANCALL_MULTIPATH,
                                    2 /* 2 args for at_taken */, OPND_CREATE_INTPTR(src),
                                    OPND_CREATE_INTPTR(targ));
        }

        /* After the callout, jump to the original target
         * block (this should not be a meta-instruction).
         */
        instrlist_preinsert(
            bb, NULL, INSTR_XL8(INSTR_CREATE_jmp(drcontext, opnd_create_pc(targ)), targ));
    }
    /* since our added instrumentation is not constant, we ask to store
     * translations now
     */
    return DR_EMIT_STORE_TRANSLATIONS;
}

void
dr_exit(void)
{
#ifdef SHOW_RESULTS
    /* Print all the cbr's seen over the life of the process, and
     * whether we saw taken, not taken, or both.
     */
    int i;
    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        if (global_table[i] != NULL) {
            elem_t *iter;
            for (iter = global_table[i]->head; iter != NULL; iter = iter->next) {
                cbr_state_t state = iter->state;

                if (state == CBR_TAKEN) {
                    dr_printf("" PFX ": taken\n", iter->addr);
                } else if (state == CBR_NOT_TAKEN) {
                    dr_printf("" PFX ": not taken\n", iter->addr);
                } else {
                    ASSERT(state == (CBR_TAKEN | CBR_NOT_TAKEN));
                    dr_printf("" PFX ": both\n", iter->addr);
                }
            }
        }
    }
#endif

    delete_table(global_table);
    drmgr_exit();
}

DR_EXPORT
void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'cbr'", "http://dynamorio.org/issues");
    if (!drmgr_init())
        DR_ASSERT_MSG(false, "drmgr_init failed!");

    global_table = new_table();
    if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT_MSG(false, "fail to register event_app_instruction!");
    dr_register_exit_event(dr_exit);
}
