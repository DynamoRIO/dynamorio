/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include <math.h>

/* asm routine and labels */
void asm_func(void);
void asm_label1(void);
void asm_label2(void);
void asm_label3(void);
static bool bb_seen_func;
static bool bb_seen_label1;
static bool bb_seen_label2;
static bool bb_seen_label3;

static void
clean_callee(void)
{
    /* Nothing */
}

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
         bool translating)
{
    if (tag == asm_func)
        dr_fprintf(STDERR, "bb asm_func\n");
    else if (tag == asm_label1)
        dr_fprintf(STDERR, "bb asm_label1\n");
    else if (tag == asm_label2)
        dr_fprintf(STDERR, "bb asm_label2\n");
    else if (tag == asm_label3)
        dr_fprintf(STDERR, "bb asm_label3\n");

    /* Test instrumentation. */
    dr_insert_clean_call(drcontext, bb, instrlist_first(bb),
                         clean_callee, false, 0);
    return DR_EMIT_DEFAULT;
}

static void
event_exit(void)
{
    dr_fprintf(STDERR, "Exit event\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    print("in dr_client_main\n");
    dr_register_bb_event(event_bb);
    dr_register_exit_event(event_exit);

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
    app_pc tags[] = {(app_pc)asm_func,
                     (app_pc)asm_label1,
                     (app_pc)asm_label2,
                     (app_pc)asm_label3};
    print("pre-DR init\n");
    dr_app_setup();
    assert(!dr_app_running_under_dynamorio());

    success = dr_prepopulate_cache(tags, sizeof(tags)/sizeof(tags[0]));
    assert(success);

    print("pre-DR start\n");
    dr_app_start();
    assert(dr_app_running_under_dynamorio());

    if (do_some_work() < 0)
        print("error in computation\n");

    print("pre-DR detach\n");
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    if (do_some_work() < 0)
        print("error in computation\n");
    print("all done\n");
    return 0;
}

#else /* asm code *************************************************************/
#include "asm_defines.asm"
START_FILE

DECLARE_GLOBAL(asm_label1)
DECLARE_GLOBAL(asm_label2)
DECLARE_GLOBAL(asm_label3)

#define FUNCNAME asm_func
        DECLARE_FUNC(FUNCNAME)
GLOBAL_LABEL(FUNCNAME:)
        INC(ARG1)
        JUMP     asm_label1

ADDRTAKEN_LABEL(asm_label1:)
        INC(ARG2)
        JUMP     asm_label2

ADDRTAKEN_LABEL(asm_label2:)
        INC(ARG3)
        JUMP     asm_label3

ADDRTAKEN_LABEL(asm_label3:)
        INC(ARG4)
        RETURN
        END_FUNC(FUNCNAME)


END_FILE
#endif
