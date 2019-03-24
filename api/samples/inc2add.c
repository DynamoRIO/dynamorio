/* **********************************************************
 * Copyright (c) 2014-2018 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2008 VMware, Inc.  All rights reserved.
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
 * inc2add.c
 *
 * Performs a dynamic optimization: converts the "inc" instruction to
 * "add 1" whenever worthwhile and feasible without perturbing the
 * target application's behavior.  Illustrates a
 * microarchitecture-specific optimization best performed at runtime
 * when the underlying processor is known.
 */

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox(msg)
#    define ATOMIC_INC(var) _InterlockedIncrement((volatile LONG *)(&(var)))
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#    define ATOMIC_INC(var) __asm__ __volatile__("lock incl %0" : "=m"(var) : : "memory")
#endif

static bool enable;

/* Use atomic operations to increment these to avoid the hassle of locking. */
static int num_examined, num_converted;

/* Replaces inc with add 1, dec with sub 1.
 * Returns true if successful, false if not.
 */
static bool
replace_inc_with_add(void *drcontext, instr_t *inst, instrlist_t *trace);

static dr_emit_flags_t
event_instruction_change(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating);

static void
event_exit(void);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* We only used drreg for liveness, not for spilling, so we need no slots. */
    drreg_options_t ops = { sizeof(ops), 0 /*no slots needed*/, false };
    dr_set_client_name("DynamoRIO Sample Client 'inc2add'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);

    /* Register for our events: process exit, and code transformation.
     * We're changing the app's code, rather than just inserting observational
     * instrumentation.
     */
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_app2app_event(event_instruction_change, NULL))
        DR_ASSERT(false);

    /* Long ago, this optimization would target the Pentium 4 (identified via
     * "proc_get_family() == FAMILY_PENTIUM_4"), where an add of 1 is faster
     * than an inc.  For illustration purposes we leave a boolean controlling it
     * but we turn it on all the time in this sample and leave it for future
     * work to determine whether to disable it on certain microarchitectures.
     */
    enable = true;

    /* Make it easy to tell by looking at the log file which client executed. */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'inc2add' initializing\n");
#ifdef SHOW_RESULTS
    /* Also give notification to stderr */
    if (dr_is_notify_on()) {
#    ifdef WINDOWS
        /* Ask for best-effort printing to cmd window.  Must be called at init. */
        dr_enable_console_printing();
#    endif
        dr_fprintf(STDERR, "Client inc2add is running\n");
    }
#endif
    /* Initialize our global variables. */
    num_examined = 0;
    num_converted = 0;
}

static void
event_exit(void)
{
#ifdef SHOW_RESULTS
    char msg[256];
    int len;
    if (enable) {
        len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                          "converted %d out of %d inc/dec to add/sub\n", num_converted,
                          num_examined);
    } else {
        len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                          "decided to keep all original inc/dec\n");
    }
    DR_ASSERT(len > 0);
    msg[sizeof(msg) / sizeof(msg[0]) - 1] = '\0';
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
    if (!drmgr_unregister_bb_app2app_event(event_instruction_change) ||
        drreg_exit() != DRREG_SUCCESS)
        DR_ASSERT(false);
    drmgr_exit();
}

/* Replaces inc with add 1, dec with sub 1.
 * If cannot replace (eflags constraints), leaves original instruction alone.
 */
static dr_emit_flags_t
event_instruction_change(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                         bool translating)
{
    int opcode;
    instr_t *instr, *next_instr;

    /* Only bother replacing for hot code, i.e., when for_trace is true, and
     * when the underlying microarchitecture calls for it.
     */
    if (!for_trace || !enable)
        return DR_EMIT_DEFAULT;

    for (instr = instrlist_first_app(bb); instr != NULL; instr = next_instr) {
        /* We're deleting some instrs, so get the next first. */
        next_instr = instr_get_next_app(instr);
        opcode = instr_get_opcode(instr);
        if (opcode == OP_inc || opcode == OP_dec) {
            if (!translating)
                ATOMIC_INC(num_examined);
            if (replace_inc_with_add(drcontext, instr, bb)) {
                if (!translating)
                    ATOMIC_INC(num_converted);
            }
        }
    }

    return DR_EMIT_DEFAULT;
}

/* Replaces inc with add 1, dec with sub 1.
 * Returns true if successful, false if not.
 */
static bool
replace_inc_with_add(void *drcontext, instr_t *instr, instrlist_t *bb)
{
    instr_t *new_instr;
    uint eflags;
    int opcode = instr_get_opcode(instr);

    DR_ASSERT(opcode == OP_inc || opcode == OP_dec);
#ifdef VERBOSE
    dr_print_instr(drcontext, STDOUT, instr, "in replace_inc_with_add:\n\t");
#endif

    /* Add/sub writes CF, inc/dec does not, so we make sure that's ok.
     * We use drreg's liveness analysis, which includes the rest of this block.
     * To be more sophisticated, we could examine instructions at target of each
     * direct exit instead of assuming CF is live across any branch.
     */
    if (drreg_aflags_liveness(drcontext, instr, &eflags) != DRREG_SUCCESS ||
        (eflags & EFLAGS_READ_CF) != 0) {
#ifdef VERBOSE
        dr_printf("\tCF is live, cannot replace inc with add ");
#endif
        return false;
    }
    if (opcode == OP_inc) {
#ifdef VERBOSE
        dr_printf("\treplacing inc with add\n");
#endif
        new_instr =
            INSTR_CREATE_add(drcontext, instr_get_dst(instr, 0), OPND_CREATE_INT8(1));
    } else {
#ifdef VERBOSE
        dr_printf("\treplacing dec with sub\n");
#endif
        new_instr =
            INSTR_CREATE_sub(drcontext, instr_get_dst(instr, 0), OPND_CREATE_INT8(1));
    }
    if (instr_get_prefix_flag(instr, PREFIX_LOCK))
        instr_set_prefix_flag(new_instr, PREFIX_LOCK);
    instr_set_translation(new_instr, instr_get_app_pc(instr));
    instrlist_replace(bb, instr, new_instr);
    instr_destroy(drcontext, instr);
    return true;
}
