/* **********************************************************
 * Copyright (c) 2018-2022 Google, Inc.  All rights reserved.
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

/* Tests that all app state is properly restored during detach.
 * Further tests could be added:
 * - check mxcsr
 * - check ymm
 * - check segment state
 */

#    include <assert.h>
#    include <stdio.h>
#    include <math.h>
#    include <stdint.h>
#    include "configure.h"
#    include "dr_api.h"
#    include "tools.h"
#    include "thread.h"
#    include "condvar.h"
#    include "detach_state_shared.h"
#    ifdef UNIX
#        include <signal.h>
#        include <ucontext.h>
#        define ALT_STACK_SIZE (SIGSTKSZ * 2)
#    endif

#    define VERBOSE 0
#    if VERBOSE
#        define VPRINT(...) print(__VA_ARGS__)
#    else
#        define VPRINT(...) /* nothing */
#    endif

static void
test_thread_func(void (*asm_func)(void))
{
    thread_t thread = create_thread(sideline_func, asm_func);
    dr_app_setup();
    wait_until_ready_for_attach();
    VPRINT("Starting DR\n");
    dr_app_start();
    signal_attached();
    while (!ready_for_detach())
        thread_sleep(5);

    VPRINT("Detaching\n");
    dr_app_stop_and_cleanup();
    signal_exit();
    join_thread(thread);

    sideline_reset();
}

/*
static void
ext_signal_handler(int sig)
{
    if (sig == SIGUSR1) {
        sideline_exit = true;
    }
    print("signal\n");
}

static void
test_thread_func2(void (*asm_func)(void))
{
    intercept_signal(SIGUSR1, (handler_3_t)ext_signal_handler, false);

    thread_t thread = create_thread(sideline_func, asm_func);
    wait_cond_var(sideline_ready_for_attach);
    signal_cond_var(sideline_continue);
    while (!sideline_ready_for_detach)
        thread_sleep(5);

    print("ready for detach\n");
    join_thread(thread);

    print("done\n");
    exit(1);
}
*/

/***************************************************************************
 * Client code
 */

static void *load_from;

static dr_emit_flags_t
event_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
#    ifdef X86
    /* Test i#5786 where a tool-inserted pc-relative load that doesn't reach is
     * mangled by DR and the resulting DR-mangling with no translation triggers
     * DR's (fragile) clean call identication and incorrectly tries to restore
     * state from (garbage) on the dstack.
     */
    instr_t *first = instrlist_first(bb);
    dr_save_reg(drcontext, bb, first, DR_REG_XAX, SPILL_SLOT_1);
    instrlist_meta_preinsert(bb, first,
                             XINST_CREATE_load(drcontext, opnd_create_reg(DR_REG_XAX),
                                               OPND_CREATE_ABSMEM(load_from, OPSZ_PTR)));
    dr_restore_reg(drcontext, bb, first, DR_REG_XAX, SPILL_SLOT_1);
    /* XXX i#4232: If we do not give translations, detach can fail as there are so
     * many no-translation instrs here a thread can end up never landing on a
     * full-state-translatable PC.  Thus, we set the translation, and provide a
     * restore-state event.  i#4232 covers are more automated/convenient way to solve
     * this.
     */
    for (instr_t *inst = instrlist_first(bb); inst != first; inst = instr_get_next(inst))
        instr_set_translation(inst, instr_get_app_pc(first));
#    endif
    /* Test both by alternating and assuming we hit both at the detach points
     * in the various tests.
     */
    if ((ptr_uint_t)tag % 2 == 0)
        return DR_EMIT_DEFAULT;
    else
        return DR_EMIT_STORE_TRANSLATIONS;
}

static bool
event_restore(void *drcontext, bool restore_memory, dr_restore_state_info_t *info)
{
#    ifdef X86
    /* Because we set the translation (to avoid detach timeouts) we need to restore
     * our register too.
     */
    if (info->fragment_info.cache_start_pc == NULL ||
        /* At the first instr should require no translation. */
        info->raw_mcontext->pc <= info->fragment_info.cache_start_pc)
        return true;
#        ifdef X64
    /* We have a code cache address here: verify load_from is not 32-bit-displacement
     * reachable from here.
     */
    assert(
        (ptr_int_t)load_from - (ptr_int_t)info->fragment_info.cache_start_pc < INT_MIN ||
        (ptr_int_t)load_from - (ptr_int_t)info->fragment_info.cache_start_pc > INT_MAX);
#        endif
    instr_t inst;
    instr_init(drcontext, &inst);
    byte *pc = info->fragment_info.cache_start_pc;
    while (pc < info->raw_mcontext->pc) {
        instr_reset(drcontext, &inst);
        pc = decode(drcontext, pc, &inst);
        bool tls;
        uint offs;
        bool spill;
        reg_id_t reg;
        if (instr_is_reg_spill_or_restore(drcontext, &inst, &tls, &spill, &reg, &offs) &&
            tls && !spill && reg == DR_REG_XAX) {
            /* The target point is past our restore. */
            instr_free(drcontext, &inst);
            return true;
        }
    }
    instr_free(drcontext, &inst);
    reg_set_value(DR_REG_XAX, info->mcontext, dr_read_saved_reg(drcontext, SPILL_SLOT_1));
#    endif
    return true;
}

static void
event_exit(void)
{
    dr_custom_free(NULL, 0, load_from, dr_page_size());
    dr_unregister_bb_event(event_bb);
    dr_unregister_restore_state_ex_event(event_restore);
    dr_unregister_exit_event(event_exit);
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    static bool do_once;
    if (!do_once) {
        print("in dr_client_main\n");
        do_once = true;
    }
    dr_register_bb_event(event_bb);
    dr_register_restore_state_ex_event(event_restore);
    dr_register_exit_event(event_exit);

    /* Try to get an address that is not 32-bit-displacement reachable from
     * the cache.  We have an assert on reachability in event_restore().
     */
    load_from = dr_custom_alloc(NULL, 0, dr_page_size(),
                                DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
}

/***************************************************************************
 * Top-level
 */

int
main(void)
{
    sideline_init();

    //test_thread_func2(thread_check_gprs_from_cache);

    test_thread_func(thread_check_gprs_from_cache);
#    ifdef X86 /* TODO i#1698: Port to AArch64. */
    test_thread_func(thread_check_gprs_from_DR);
    test_thread_func(thread_check_eflags_from_cache);
    test_thread_func(thread_check_eflags_from_DR);

    /* DR's detach assumes the app has its regular xsp so we can't put in
     * a weird sentinel value, unfortunately.
     */
    size_t stack_size = 128 * 1024;
    safe_stack = stack_size + allocate_mem(stack_size, ALLOW_READ | ALLOW_WRITE);
    test_thread_func(thread_check_xsp_from_cache);
    test_thread_func(thread_check_xsp_from_DR);
    free_mem(safe_stack - stack_size, stack_size);
#    endif

    test_thread_func(thread_check_sigstate);
    test_thread_func(thread_check_sigstate_from_handler);

    sideline_destroy();
    print("all done\n");
    return 0;
}
