/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
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

#ifndef ASM_CODE_ONLY /* C code */

#    include "configure.h"
#    include "dr_api.h"
#    include "tools.h"
#    include <math.h>

/* asm routine and labels */
void
asm_func(void);
void
asm_label1(void);
void
asm_label2(void);
void
asm_label3(void);
void
asm_return(void);
static bool bb_seen_func;
static bool bb_seen_label1;
static bool bb_seen_label2;
static bool bb_seen_label3;
static bool bb_seen_return;

static void
clean_callee(void)
{
    /* Nothing */
}

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (tag == asm_func)
        dr_fprintf(STDERR, "bb asm_func\n");
    else if (tag == asm_label1)
        dr_fprintf(STDERR, "bb asm_label1\n");
    else if (tag == asm_label2)
        dr_fprintf(STDERR, "bb asm_label2\n");
    else if (tag == asm_label3)
        dr_fprintf(STDERR, "bb asm_label3\n");
    else if (tag == asm_return)
        dr_fprintf(STDERR, "bb asm_return\n");

    /* Test instrumentation. */
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb), clean_callee, false, 0);
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_fprintf(STDERR, "Exit event\n");
}

static void
event_post_attach(void)
{
    dr_fprintf(STDERR, "in %s\n", __FUNCTION__);
}

static void
event_pre_detach(void)
{
    dr_fprintf(STDERR, "in %s\n", __FUNCTION__);
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    print("in dr_client_main\n");
    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);
    if (!dr_register_post_attach_event(event_post_attach))
        print("Failed to register post-attach event");
    dr_register_pre_detach_event(event_pre_detach);

    /* XXX i#975: add some more thorough tests of different events */
}

static int
do_some_work(void)
{
    static int iters = 8192;
    int i;
    double val = iters;
    for (i = 0; i < iters; ++i) {
        val += sin(val);
    }
    asm_func();
    return (val > 0);
}

int
main(int argc, const char *argv[])
{
    bool success;
    app_pc tags[] = { (app_pc)asm_func, (app_pc)asm_label1, (app_pc)asm_label2,
                      (app_pc)asm_label3, (app_pc)asm_return };
    app_pc return_tags[] = { (app_pc)asm_return };

    // For testing ibt prepop, we want bb's to be indirect branch targets.
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-disable_traces -shared_bb_ibt_tables "
                   // Standard test options.
                   "-stderr_mask 0xc"))
        dr_fprintf(STDERR, "Failed to set env var!\n");

    // Attach and re-attach.
    for (int i = 0; i < 2; ++i) {
        print("pre-DR init\n");
        dr_app_setup();
        assert(!dr_app_running_under_dynamorio());
        dr_stats_t stats = { sizeof(dr_stats_t) };
        bool got_stats = dr_get_stats(&stats);
        assert(got_stats);
        assert(stats.basic_block_count == 0);
#    ifdef ARM
        /* Our asm is arm, not thumb. */
        dr_isa_mode_t old_mode;
        dr_set_isa_mode(dr_get_current_drcontext(), DR_ISA_ARM_A32, &old_mode);
#    endif
        success = dr_prepopulate_cache(tags, sizeof(tags) / sizeof(tags[0]));
#    ifdef ARM
        dr_set_isa_mode(dr_get_current_drcontext(), old_mode, NULL);
#    endif
        assert(success);
        success =
            dr_prepopulate_indirect_targets(DR_INDIRECT_RETURN, return_tags,
                                            sizeof(return_tags) / sizeof(return_tags[0]));
        // There's no simple way to verify DR did not have to lazily add asm_return
        // to any ibt table: we could export the num_exits_ind_bad_miss stat, but it's
        // going to make a flaky test as it depends on the compiler precisely how
        // many we see in the base case.  Maybe we could run twice, once with and once
        // without indirect prepop, and compare those.  The stats export is problematic
        // though as it's a debug-only stat and we want to limit dr_stats_t to
        // stats available in release build too.  For now, I did a manual test and
        // saw 8 "ind target in cache but not table" exits w/o prepop of the table
        // and 7 with and confirmed there's no lazy filling for asm_return.
        assert(success);
        got_stats = dr_get_stats(&stats);
        assert(got_stats);
        assert(stats.basic_block_count > 0);
        assert(stats.peak_num_threads > 0);
        assert(stats.num_threads_created > 0);

        print("pre-DR start\n");
        dr_app_start();
        assert(dr_app_running_under_dynamorio());

        if (do_some_work() < 0)
            print("error in computation\n");

        if (i > 0) {
            print("pre-DR detach with stats\n");
            dr_stats_t end_stats = { sizeof(dr_stats_t) };
            dr_app_stop_and_cleanup_with_stats(&end_stats);
            assert(end_stats.basic_block_count > 0);
        } else {
            print("pre-DR detach\n");
            dr_app_stop_and_cleanup();
        }
        assert(!dr_app_running_under_dynamorio());

        if (do_some_work() < 0)
            print("error in computation\n");
    }
    print("all done\n");
    return 0;
}

#else /* asm code *************************************************************/
#    include "asm_defines.asm"
/* clang-format off */
START_FILE

DECLARE_GLOBAL(asm_label1)
DECLARE_GLOBAL(asm_label2)
DECLARE_GLOBAL(asm_label3)
DECLARE_GLOBAL(asm_return)

#define FUNCNAME asm_func
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        INC(ARG1)
#   ifdef AARCH64
        stp      x29, x30, [sp, #-16]!
#   elif defined(ARM)
        push     {lr}
#   endif
#   ifdef ARM
        /* We don't use CALLC0 b/c it uses blx which swaps to Thumb. */
        bl       asm_label1
#   else
        CALLC0(asm_label1)
#   endif
ADDRTAKEN_LABEL(asm_return:)
#   ifdef AARCH64
        ldp      x29, x30, [sp], #16
#   elif defined(ARM)
        pop      {lr}
#   endif
        JUMP     asm_label2

ADDRTAKEN_LABEL(asm_label1:)
        INC(ARG2)
        RETURN

ADDRTAKEN_LABEL(asm_label2:)
        INC(ARG3)
        JUMP     asm_label3

ADDRTAKEN_LABEL(asm_label3:)
        INC(ARG4)
        RETURN
        END_FUNC(FUNCNAME)


END_FILE
/* Not turning clang format back on b/c of a clang-format-diff wart. */
#endif
