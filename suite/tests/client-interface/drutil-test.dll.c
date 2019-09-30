/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Tests the drutil extension */

#include "dr_api.h"
#include "drmgr.h"
#include "drutil.h"
#include <string.h> /* memcpy */

#define CHECK(x, msg)                        \
    do {                                     \
        if (!(x)) {                          \
            dr_fprintf(STDERR, "%s\n", msg); \
            dr_abort();                      \
        }                                    \
    } while (0);

static bool verbose;

static int repstr_seen;

#define MAGIC_NOTE 0x9a9b9c9d
dr_instr_label_data_t magic_vals = { 0xdeadbeef, 0xeeeebabe, 0x12345678, 0x8765432 };

static void
event_exit(void);
static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating);
static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data);
static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data);

DR_EXPORT void
dr_init(client_id_t id)
{
    drmgr_priority_t priority = { sizeof(priority), "drutil-test", NULL, NULL, 0 };
    bool ok;

    drmgr_init();
    drutil_init();
    dr_register_exit_event(event_exit);

    ok = drmgr_register_bb_app2app_event(event_bb_app2app, &priority);
    CHECK(ok, "drmgr register bb failed");

    ok = drmgr_register_bb_instrumentation_event(event_bb_analysis, event_bb_insert,
                                                 &priority);
    CHECK(ok, "drmgr register bb failed");
}

static void
event_exit(void)
{
    drutil_exit();
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
    if (verbose) {
        /* I see 62 for win x64, and 16 for linux x86 */
        dr_fprintf(STDERR, "saw %d rep str instrs\n", repstr_seen);
    }
}

static bool
instr_is_stringop_loop(instr_t *inst)
{
#if defined(X86)
    int opc = instr_get_opcode(inst);
    return (opc == OP_rep_ins || opc == OP_rep_outs || opc == OP_rep_movs ||
            opc == OP_rep_stos || opc == OP_rep_lods || opc == OP_rep_cmps ||
            opc == OP_repne_cmps || opc == OP_rep_scas || opc == OP_repne_scas);
#elif defined(AARCHXX)
    return false;
#endif
}

static dr_emit_flags_t
event_bb_app2app(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating)
{
    instr_t *inst;
    bool expanded;

    for (inst = instrlist_first(bb); inst != NULL; inst = instr_get_next(inst)) {
        if (instr_is_stringop_loop(inst))
            repstr_seen++;
    }

    /* insert a meta instr to test drutil_expand_rep_string() handling it (i#1055) */
    instrlist_meta_preinsert(bb, instrlist_first(bb), INSTR_CREATE_label(drcontext));

    if (!drutil_expand_rep_string(drcontext, bb)) {
        CHECK(false, "drutil_expand_rep_string failed");
    }

    expanded = true;
    inst = instrlist_first(bb);
    if (!drutil_expand_rep_string_ex(drcontext, bb, &expanded, &inst)) {
        CHECK(false, "drutil_expand_rep_string_ex failed");
    }
    CHECK(repstr_seen != 0 || (!expanded && inst == NULL),
          "drutil_expand_rep_string_ex bad OUT values");

    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                  bool translating, OUT void **user_data)
{
    /* test label data (i#675) */
    instr_t *first = instrlist_first(bb);
    if (first != NULL) {
        instr_t *l = INSTR_CREATE_label(drcontext);
        dr_instr_label_data_t *data = instr_get_label_data_area(l);
        CHECK(data != NULL, "failed to get data area");
        memcpy(data, &magic_vals, sizeof(*data));
        instr_set_note(l, (void *)(ptr_uint_t)MAGIC_NOTE);
        instrlist_meta_preinsert(bb, first, l);
    }
    return DR_EMIT_DEFAULT;
}

static void
check_label_data(instrlist_t *bb)
{
    instr_t *first = instrlist_first(bb);
    if (first != NULL) {
        dr_instr_label_data_t *data = instr_get_label_data_area(first);
        CHECK(data != NULL, "failed to get data area");
        CHECK(instr_is_label(first), "expected label");
        CHECK(memcmp(data, &magic_vals, sizeof(*data)) == 0,
              "label data was not preserved");
        CHECK(instr_get_note(first) == (void *)(ptr_uint_t)MAGIC_NOTE,
              "label note was not preserved");
    }
}

static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                bool for_trace, bool translating, void *user_data)
{
    int i;
    reg_id_t reg1 = IF_X86_ELSE(REG_XAX, DR_REG_R0);
    reg_id_t reg2 = IF_X86_ELSE(REG_XDX, DR_REG_R1);
    CHECK(!instr_is_stringop_loop(instr), "rep str conversion missed one");
    if (instr_writes_memory(instr)) {
        for (i = 0; i < instr_num_srcs(instr); i++) {
            if (opnd_is_memory_reference(instr_get_src(instr, i))) {
                dr_save_reg(drcontext, bb, instr, reg1, SPILL_SLOT_1);
                dr_save_reg(drcontext, bb, instr, reg2, SPILL_SLOT_2);
                /* XXX: should come up w/ some clever way to ensure
                 * this gets the right address: for now just making sure
                 * it doesn't crash
                 */
                drutil_insert_get_mem_addr(drcontext, bb, instr, instr_get_src(instr, i),
                                           reg1, reg2);
                dr_restore_reg(drcontext, bb, instr, reg2, SPILL_SLOT_2);
                dr_restore_reg(drcontext, bb, instr, reg1, SPILL_SLOT_1);
            }
        }
        /* We test the _ex version on the dests. */
        for (i = 0; i < instr_num_dsts(instr); i++) {
            if (opnd_is_memory_reference(instr_get_dst(instr, i))) {
                bool used;
                dr_save_reg(drcontext, bb, instr, reg1, SPILL_SLOT_1);
                dr_save_reg(drcontext, bb, instr, reg2, SPILL_SLOT_2);
                drutil_insert_get_mem_addr_ex(drcontext, bb, instr,
                                              instr_get_dst(instr, i), reg1, reg2, &used);
                dr_restore_reg(drcontext, bb, instr, reg2, SPILL_SLOT_2);
                dr_restore_reg(drcontext, bb, instr, reg1, SPILL_SLOT_1);
            }
        }
#ifdef X86
        if (instr_is_xsave(instr)) {
            ushort size =
                (ushort)drutil_opnd_mem_size_in_bytes(instr_get_dst(instr, 0), instr);
            /* We're checking for a reasonable xsave area size which is at least 576
             * bytes for the x87 + SSE user state components, or up to 2688 if AVX-512
             * is enabled.
             */
            CHECK(size >= 576 && size <= 2688, "xsave area size unexpected");
        }
#endif
    }
    check_label_data(bb);
    return DR_EMIT_DEFAULT;
}
