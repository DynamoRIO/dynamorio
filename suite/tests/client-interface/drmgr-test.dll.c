/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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


/* Tests the drmgr extension */

#include "dr_api.h"
#include "drmgr.h"
#include <string.h> /* memset */

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "%s\n", msg); \
        dr_abort();                      \
    }                                    \
} while (0);

/* CLS tests: easiest to assume a single thread (the 2nd thread the
 * app creates, in this case) hitting callbacks and use global data to
 * check preservation
 */
static int tls_idx;
static int cls_idx;
static thread_id_t main_thread;
static int cb_depth;

#define MAGIC_NUMBER_FROM_CACHE 0x0eadbeef

static bool checked_tls_from_cache;
static bool checked_cls_from_cache;
static bool checked_tls_write_from_cache;
static bool checked_cls_write_from_cache;

static void event_exit(void);
static void event_thread_init(void *drcontext);
static void event_thread_exit(void *drcontext);
static void event_thread_context_init(void *drcontext, bool new_depth);
static void event_thread_context_exit(void *drcontext, bool process_exit);
static dr_emit_flags_t event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                                         bool for_trace, bool translating,
                                         OUT void **user_data);
static dr_emit_flags_t event_bb_insert(void *drcontext, void *tag, instrlist_t *bb,
                                       instr_t *inst, bool for_trace, bool translating,
                                       void *user_data);

DR_EXPORT void 
dr_init(client_id_t id)
{
    drmgr_priority_t priority = {sizeof(priority), "drmgr-test", NULL, NULL, 0};
    bool ok;

    drmgr_init();
    dr_register_exit_event(event_exit);
    drmgr_register_thread_init_event(event_thread_init);
    drmgr_register_thread_exit_event(event_thread_exit);

    ok = drmgr_register_bb_instrumentation_event(event_bb_analysis,
                                                 event_bb_insert,
                                                 &priority);
    CHECK(ok, "drmgr register bb failed");

    tls_idx = drmgr_register_tls_field();
    CHECK(tls_idx != 1, "drmgr_register_tls_field failed");
    cls_idx = drmgr_register_cls_field(event_thread_context_init,
                                       event_thread_context_exit);
    CHECK(cls_idx != 1, "drmgr_register_tls_field failed");
}

static void 
event_exit(void)
{
    CHECK(checked_tls_from_cache, "failed to hit clean call");
    CHECK(checked_cls_from_cache, "failed to hit clean call");
    CHECK(checked_tls_write_from_cache, "failed to hit clean call");
    CHECK(checked_cls_write_from_cache, "failed to hit clean call");
    drmgr_unregister_cls_field(event_thread_context_init,
                               event_thread_context_exit,
                               cls_idx);
    drmgr_exit();
    dr_fprintf(STDERR, "all done\n");
}

static void
event_thread_init(void *drcontext)
{
    if (main_thread == 0)
        main_thread = dr_get_thread_id(drcontext);
    drmgr_set_tls_field(drcontext, tls_idx,
                        (void *)(ptr_int_t)dr_get_thread_id(drcontext));
}

static void
event_thread_exit(void *drcontext)
{
    CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
          (void *)(ptr_int_t)dr_get_thread_id(drcontext),
          "tls not preserved");
}

static void
event_thread_context_init(void *drcontext, bool new_depth)
{
    if (dr_get_thread_id(drcontext) != main_thread) {
        cb_depth++;
#if VERBOSE
        /* # cbs differs on xp vs win7 so not part of output */
        dr_fprintf(STDERR, "non-main thread entering callback depth=%d\n", cb_depth);
#endif
        CHECK(new_depth || drmgr_get_cls_field(drcontext, cls_idx) ==
              (void *)(ptr_int_t)cb_depth,
              "not re-using prior callback value");
        drmgr_set_cls_field(drcontext, cls_idx, (void *)(ptr_int_t)cb_depth);
        CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
              (void *)(ptr_int_t)dr_get_thread_id(drcontext), "tls not preserved");
    }
}

static void 
event_thread_context_exit(void *drcontext, bool thread_exit)
{
    if (!thread_exit && dr_get_thread_id(drcontext) != main_thread) {
#if VERBOSE
        dr_fprintf(STDERR, "  non-main thread exiting callback depth=%d cls=%d\n",
                   cb_depth, (int)(ptr_int_t) drmgr_get_cls_field(drcontext, cls_idx));
#endif
        CHECK(drmgr_get_cls_field(drcontext, cls_idx) == 
              (void *)(ptr_int_t)cb_depth,
              "cls not preserved");
        cb_depth--;
        CHECK(drmgr_get_tls_field(drcontext, tls_idx) ==
              (void *)(ptr_int_t)dr_get_thread_id(drcontext), "tls not preserved");
    }
}

static dr_emit_flags_t
event_bb_analysis(void *drcontext, void *tag, instrlist_t *bb,
                  bool for_trace, bool translating, OUT void **user_data)
{
    /* point at first instr */
    *user_data = (void *) instrlist_first(bb);
    return DR_EMIT_DEFAULT;
}

static void
check_tls_from_cache(void *tls_val)
{
    CHECK(tls_val == drmgr_get_tls_field(dr_get_current_drcontext(), tls_idx),
          "tls read from cache incorrect");
    checked_tls_from_cache = true;
}

static void
check_cls_from_cache(void *cls_val)
{
    CHECK(cls_val == drmgr_get_cls_field(dr_get_current_drcontext(), cls_idx),
          "cls read from cache incorrect");
    checked_cls_from_cache = true;
}

static void
check_tls_write_from_cache(void)
{
    void *drcontext = dr_get_current_drcontext();
    CHECK(drmgr_get_tls_field(drcontext, tls_idx) == (void *) MAGIC_NUMBER_FROM_CACHE,
          "cls write from cache incorrect");
    /* now restore */
    drmgr_set_tls_field(drcontext, tls_idx,
                        (void *)(ptr_int_t)dr_get_thread_id(drcontext));
    checked_tls_write_from_cache = true;
}

static void
check_cls_write_from_cache(void)
{
    CHECK(drmgr_get_cls_field(dr_get_current_drcontext(), cls_idx) ==
          (void *) MAGIC_NUMBER_FROM_CACHE,
          "cls write from cache incorrect");
    /* now restore */
    drmgr_set_cls_field(dr_get_current_drcontext(), cls_idx,
                        (void *)(ptr_int_t)cb_depth);
    checked_cls_write_from_cache = true;
}

static dr_emit_flags_t
event_bb_insert(void *drcontext, void *tag, instrlist_t *bb, instr_t *inst,
                bool for_trace, bool translating, void *user_data)
{
    /* hack to instrument every nth bb.  assumes DR serializes bb events. */
    static int freq;
    freq++;
    if (freq % 100 == 0 && inst == (instr_t*)user_data/*first instr*/) {
        /* test read from cache */
        dr_save_reg(drcontext, bb, inst, DR_REG_XAX, SPILL_SLOT_1);
        drmgr_insert_read_tls_field(drcontext, tls_idx, bb, inst, DR_REG_XAX);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_tls_from_cache,
                             false, 1, opnd_create_reg(DR_REG_XAX));
        drmgr_insert_read_cls_field(drcontext, cls_idx, bb, inst, DR_REG_XAX);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_cls_from_cache,
                             false, 1, opnd_create_reg(DR_REG_XAX));
        dr_restore_reg(drcontext, bb, inst, DR_REG_XAX, SPILL_SLOT_1);
    }
    if (freq % 300 == 0 && inst == (instr_t*)user_data/*first instr*/) {
        /* test write from cache */
        dr_save_reg(drcontext, bb, inst, DR_REG_XAX, SPILL_SLOT_1);
        dr_save_reg(drcontext, bb, inst, DR_REG_XCX, SPILL_SLOT_2);
        instrlist_meta_preinsert(bb, inst, INSTR_CREATE_mov_imm
                                 (drcontext, opnd_create_reg(DR_REG_EAX),
                                  OPND_CREATE_INT32(MAGIC_NUMBER_FROM_CACHE)));
        drmgr_insert_write_tls_field(drcontext, tls_idx, bb, inst, DR_REG_XAX,
                                     DR_REG_XCX);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_tls_write_from_cache,
                             false, 0);
        drmgr_insert_write_cls_field(drcontext, cls_idx, bb, inst, DR_REG_XAX,
                                     DR_REG_XCX);
        dr_insert_clean_call(drcontext, bb, inst, (void *)check_cls_write_from_cache,
                             false, 0);
        dr_restore_reg(drcontext, bb, inst, DR_REG_XCX, SPILL_SLOT_2);
        dr_restore_reg(drcontext, bb, inst, DR_REG_XAX, SPILL_SLOT_1);
    }
    return DR_EMIT_DEFAULT;
}


