/* **********************************************************
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

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
# define ATOMIC_INC(var) _InterlockedIncrement((volatile LONG *)(&(var)))
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
# define ATOMIC_INC(var) __asm__ __volatile__("lock incl %0" : "=m" (var) : : "memory")
#endif

static bool enable;

/* use atomic operations to increment these to avoid the hassle of locking. */
static int num_examined, num_converted;

/* replaces inc with add 1, dec with sub 1
 * returns true if successful, false if not
 */
static bool
replace_inc_with_add(void *drcontext, instr_t *inst, instrlist_t *trace);

static dr_emit_flags_t
event_trace(void *drcontext, void *tag, instrlist_t *trace, bool translating);

static void
event_exit(void);

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_exit_event(event_exit);
    dr_register_trace_event(event_trace);
    /* this optimization is only worthwhile on the Pentium 4, where
     * an add of 1 is faster than an inc
     */
    enable = (proc_get_family() == FAMILY_PENTIUM_4);
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'inc2add' initializing\n");
#ifdef SHOW_RESULTS
    /* also give notification to stderr */
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called in dr_init(). */
        dr_enable_console_printing();
# endif
        dr_fprintf(STDERR, "Client inc2add is running\n");
    }
#endif
    /* initialize our global variables */
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
        len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                          "converted %d out of %d inc/dec to add/sub\n",
                          num_converted, num_examined);
    } else {
        len = dr_snprintf(msg, sizeof(msg)/sizeof(msg[0]),
                          "decided to keep all original inc/dec\n");
    }
    DR_ASSERT(len > 0);
    msg[sizeof(msg)/sizeof(msg[0])-1] = '\0';
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */
}

/* replaces all inc with add 1, dec with sub 1
 * if cannot replace (eflags constraints), leaves original instruction alone
 */
static dr_emit_flags_t
event_trace(void *drcontext, void *tag, instrlist_t *trace, bool translating)
{
    instr_t *instr, *next_instr;
    int opcode;

    if (!enable)
        return DR_EMIT_DEFAULT;

#ifdef VERBOSE
    dr_printf("in dynamorio_trace(tag="PFX")\n", tag);
    instrlist_disassemble(drcontext, tag, trace, STDOUT);
#endif

    for (instr = instrlist_first(trace); instr != NULL; instr = next_instr) {
        /* grab next now so we don't go over instructions we insert */
        next_instr = instr_get_next(instr);
        opcode = instr_get_opcode(instr);
        if (opcode == OP_inc || opcode == OP_dec) {
            if (!translating)
                ATOMIC_INC(num_examined);
            if (replace_inc_with_add(drcontext, instr, trace)) {
                if (!translating)
                    ATOMIC_INC(num_converted);
            }
        }
    }

#ifdef VERBOSE
    dr_printf("after dynamorio_trace(tag="PFX"):\n", tag);
    instrlist_disassemble(drcontext, tag, trace, STDOUT);
#endif

    return DR_EMIT_DEFAULT;
}

/* replaces inc with add 1, dec with sub 1
 * returns true if successful, false if not
 */
static bool
replace_inc_with_add(void *drcontext, instr_t *instr, instrlist_t *trace)
{
    instr_t *in;
    uint eflags;
    int opcode = instr_get_opcode(instr);
    bool ok_to_replace = false;

    DR_ASSERT(opcode == OP_inc || opcode == OP_dec);
#ifdef VERBOSE
    dr_print_instr(drcontext, STDOUT, instr, "in replace_inc_with_add:\n\t");
#endif

    /* add/sub writes CF, inc/dec does not, make sure that's ok */
    for (in = instr; in != NULL; in = instr_get_next(in)) {
        eflags = instr_get_eflags(in);
        if ((eflags & EFLAGS_READ_CF) != 0) {
#ifdef VERBOSE
            dr_print_instr(drcontext, STDOUT, in,
                           "\treads CF => cannot replace inc with add: ");
#endif
            return false;
        }
        if (instr_is_exit_cti(in)) {
            /* to be more sophisticated, examine instructions at
             * target of exit cti (if it is a direct branch).
             * for this example, we give up if we hit a branch.
             */
            return false;
        }
        /* if writes but doesn't read, ok */
        if ((eflags & EFLAGS_WRITE_CF) != 0) {
            ok_to_replace = true;
            break;
        }
    }
    if (!ok_to_replace) {
#ifdef VERBOSE
        dr_printf("\tno write to CF => cannot replace inc with add\n");
#endif
        return false;
    }
    if (opcode == OP_inc) {
#ifdef VERBOSE
        dr_printf("\treplacing inc with add\n");
#endif
        in = INSTR_CREATE_add(drcontext, instr_get_dst(instr, 0),
                              OPND_CREATE_INT8(1));
    } else {
#ifdef VERBOSE
        dr_printf("\treplacing dec with sub\n");
#endif
        in = INSTR_CREATE_sub(drcontext, instr_get_dst(instr, 0),
                              OPND_CREATE_INT8(1));
    }
    if (instr_get_prefix_flag(instr, PREFIX_LOCK))
        instr_set_prefix_flag(in, PREFIX_LOCK);
    instr_set_translation(in, instr_get_app_pc(instr));
    instrlist_replace(trace, instr, in);
    instr_destroy(drcontext, instr);
    return true;
}
