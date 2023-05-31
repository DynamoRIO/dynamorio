/* **********************************************************
 * Copyright (c) 2012-2014 Google, Inc.  All rights reserved.
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
#include "hashtable.h"

#include "client_tools.h" /* For ASSERT; DR_ASSERT raises a message box. */

static byte *mybase;
static uint bb_execs;
static uint resurrect_success;
static bool verbose;

/* test hashtable persistence via a table that contains one entry per
 * pcache written or loaded.  key is start, payload is size.
 */
static hashtable_t sample_inlined_table;
static hashtable_t sample_pointer_table;

static void
free_payload(void *entry)
{
    dr_global_free(entry, sizeof(size_t));
}

static void
at_bb(app_pc bb_addr)
{
#ifdef X64
    /* FIXME: abs ref is rip-rel so doesn't work w/o patching.
     * need to impl patching here: finalize interface.
     */
#else
    /* global reference => won't work w/o same base or relocation */
    bb_execs++;
#endif
}

/* We want to persist our clean call, which will only work if our library
 * is at the same base
 */

static size_t
event_persist_ro_size(void *drcontext, void *perscxt, size_t file_offs,
                      void **user_data OUT)
{
    return sizeof(mybase) +
        hashtable_persist_size(drcontext, &sample_pointer_table, sizeof(size_t), perscxt,
                               DR_HASHPERS_REBASE_KEY | DR_HASHPERS_ONLY_IN_RANGE |
                                   DR_HASHPERS_ONLY_PERSISTED) +
        hashtable_persist_size(drcontext, &sample_inlined_table, sizeof(size_t), perscxt,
                               DR_HASHPERS_REBASE_KEY | DR_HASHPERS_ONLY_IN_RANGE |
                                   DR_HASHPERS_ONLY_PERSISTED);
}

static bool
event_persist_patch(void *drcontext, void *perscxt, byte *bb_start, size_t bb_size,
                    void *user_data)
{
    /* XXX: add more sophisticated example that needs patching.
     * For that, we want cti's to be allowed, which is i#665.  Then
     * we can have a jmp or call into our lib (e.g., change at_bb to
     * not be inlinable) and go patch it to go through gencode or sthg,
     * and patch the gencode at load, or sthg.
     */
    return true;
}

static bool
event_persist_ro(void *drcontext, void *perscxt, file_t fd, void *user_data)
{
    bool ok = true;
    app_pc start = dr_persist_start(perscxt);
    if (dr_write_file(fd, &mybase, sizeof(mybase)) != (ssize_t)sizeof(mybase))
        return false;

    ok = ok &&
        hashtable_persist(drcontext, &sample_pointer_table, sizeof(size_t), fd, perscxt,
                          DR_HASHPERS_PAYLOAD_IS_POINTER | DR_HASHPERS_REBASE_KEY |
                              DR_HASHPERS_ONLY_IN_RANGE | DR_HASHPERS_ONLY_PERSISTED);
    ok = ok &&
        hashtable_persist(drcontext, &sample_inlined_table, sizeof(size_t), fd, perscxt,
                          DR_HASHPERS_REBASE_KEY | DR_HASHPERS_ONLY_IN_RANGE |
                              DR_HASHPERS_ONLY_PERSISTED);

    return ok;
}

static bool
event_resurrect_ro(void *drcontext, void *perscxt, byte **map INOUT)
{
    bool ok = true;
    uint i;
    app_pc start = dr_persist_start(perscxt);
    size_t size = dr_persist_size(perscxt);
    byte *base = *(byte **)(*map);
    *map += sizeof(base);
    /* this test relies on having a preferred base and getting it both runs */
    if (base != mybase) {
        if (verbose) {
            dr_fprintf(STDERR, "persisted base=" PFX " does not match cur base=" PFX "\n",
                       base, mybase);
        }
        return false;
    }

    ok = ok &&
        hashtable_resurrect(drcontext, map, &sample_pointer_table, sizeof(size_t),
                            perscxt,
                            DR_HASHPERS_PAYLOAD_IS_POINTER | DR_HASHPERS_REBASE_KEY |
                                DR_HASHPERS_CLONE_PAYLOAD,
                            NULL);
    ok = ok &&
        hashtable_resurrect(drcontext, map, &sample_inlined_table, sizeof(size_t),
                            perscxt, DR_HASHPERS_REBASE_KEY, NULL);

    /* we stored the 1st byte of every bb */
    for (i = 0; i < HASHTABLE_SIZE(sample_pointer_table.table_bits); i++) {
        hash_entry_t *he;
        for (he = sample_pointer_table.table[i]; he != NULL; he = he->next) {
            ASSERT(*((app_pc)he->key) == *((app_pc)he->payload) ||
                   /* XXX: we relax our assert to handle a syscall hook, which
                    * isn't in place yet at load time (i#1196).
                    */
                   *((app_pc)he->payload) == 0xe9);
        }
    }
    for (i = 0; i < HASHTABLE_SIZE(sample_inlined_table.table_bits); i++) {
        hash_entry_t *he;
        for (he = sample_inlined_table.table[i]; he != NULL; he = he->next) {
            ASSERT(*((app_pc)he->key) == (byte)(ptr_uint_t)he->payload ||
                   /* see above */
                   (byte)(ptr_uint_t)he->payload == 0xe9);
        }
    }

    resurrect_success++;
    return true;
}

#define MINSERT instrlist_meta_preinsert

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    /* test abs ref */
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb), at_bb, false, 1,
                         OPND_CREATE_INTPTR((ptr_uint_t)dr_fragment_app_pc(tag)));

    /* test intra-bb cti (i#665)
     * XXX: test in automated fashion whether these bbs are truly persisted.
     * using the client-walk patch API I can do that: but consensus is it's better
     * to switch to having DR do the cache walk, making it not clear how to test
     * this other than adding to every single bb which will result in no
     * pcaches at all if this intra-bb cti isn't handled.
     */
    {
        instr_t *skip = INSTR_CREATE_label(drcontext);
        instr_t *inst = instrlist_last(bb);
        /* XXX i#1032: move in one further to avoid assert on an unhandled
         * intra vs inter distinction case.
         */
        if (instr_get_prev(inst) != NULL)
            inst = instr_get_prev(inst);
        instrlist_meta_preinsert(bb, inst,
                                 INSTR_CREATE_jmp(drcontext, opnd_create_instr(skip)));
        instrlist_meta_preinsert(bb, inst, INSTR_CREATE_ud2(drcontext));
        instrlist_meta_preinsert(bb, inst, skip);
    }

    /* test hashtable persistence: store the 1st byte of every bb */
    {
        void *payload = dr_global_alloc(sizeof(size_t));
        app_pc pc = dr_fragment_app_pc(tag);
        *(byte *)payload = *pc; /* not bothering w/ safe_read */
        if (!hashtable_add(&sample_pointer_table, (void *)pc, payload))
            free_payload(payload);
        hashtable_add(&sample_inlined_table, (void *)pc, (void *)(ptr_uint_t)(*pc));
    }

    return DR_EMIT_DEFAULT | DR_EMIT_PERSISTABLE;
}

static void
event_exit(void)
{
    if (resurrect_success > 0)
        dr_fprintf(STDERR, "successfully resurrected at least one pcache\n");
    hashtable_delete(&sample_inlined_table);
    hashtable_delete(&sample_pointer_table);
}

DR_EXPORT
void
dr_init(client_id_t id)
{
    mybase = dr_get_client_base(id);
    dr_fprintf(STDERR, "thank you for testing the client interface\n");
    dr_register_exit_event(event_exit);
    dr_register_bb_event(event_bb);
    if (!dr_register_persist_ro(event_persist_ro_size, event_persist_ro,
                                event_resurrect_ro))
        dr_fprintf(STDERR, "failed to register ro");
    if (!dr_register_persist_patch(event_persist_patch))
        dr_fprintf(STDERR, "failed to register patch");

    hashtable_init(&sample_inlined_table, 4, HASH_INTPTR, false /*!strdup*/);
    hashtable_init_ex(&sample_pointer_table, 4, HASH_INTPTR, false /*!strdup*/,
                      true /*sync*/, free_payload, NULL, NULL);
}
