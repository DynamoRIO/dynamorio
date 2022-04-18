/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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

/* Tests detach state restoration for DRWRAP_REPLACE_RETADDR. */

/* XXX: We undef this b/c it's easier than getting rid of from CMake with the
 * global cflags config where all the other tests want this set.
 */
#undef DR_REG_ENUM_COMPATIBILITY

#include <assert.h>
#include <atomic>
#include <iostream>
#include "configure.h"
#include "dr_api.h"
#include "drwrap.h"
#include "tools.h"
#include "thread.h"
#include "condvar.h"

#define VERBOSE 0
#if VERBOSE
#    define VPRINT(...) print(__VA_ARGS__)
#else
#    define VPRINT(...) /* nothing */
#endif

std::atomic<bool> sideline_exit;
static void *sideline_ready_for_attach;
static void *sideline_continue;
static int pre_count;
static int post_count;

extern "C" { /* Make it easy to get the name across platforms. */
EXPORT void
wrapped_subfunc(void)
{
#ifdef LINUX
    /* Test non-fcache translation by making a syscall.
     * Just easier to do on Linux so we do limit this to that OS.
     * We don't want to do this on every invocation since we'll then never
     * test the non-syscall translation.
     */
    static int syscall_sometimes = 0;
    if (syscall_sometimes++ % 10 == 0 && getpid() == 0)
        print("That's weird.\n"); /* Avoid the branch getting optimized away. */
#endif
}
EXPORT void
wrapped_func(void)
{
    wrapped_subfunc();
}
}

THREAD_FUNC_RETURN_TYPE
sideline_func(void *arg)
{
    void (*asm_func)(void) = (void (*)(void))arg;
    signal_cond_var(sideline_ready_for_attach);
    wait_cond_var(sideline_continue);
    while (!sideline_exit.load(std::memory_order_acquire)) {
        for (int i = 0; i < 10; ++i)
            wrapped_func();
    }
    return THREAD_FUNC_RETURN_ZERO;
}

static void
wrap_pre(void *wrapcxt, OUT void **user_data)
{
    ++pre_count;
}

static void
wrap_post(void *wrapcxt, void *user_data)
{
    ++post_count;
}

static void
event_exit(void)
{
    /* Depending on where we detach, pre can be up to 2 larger. */
    assert(pre_count > 0 && post_count > 0);
    assert(pre_count == post_count || pre_count - 1 == post_count ||
           pre_count - 2 == post_count);
    drwrap_exit();
    dr_fprintf(STDERR, "client done\n");
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    std::cerr << "in dr_client_main\n";
    dr_register_exit_event(event_exit);
    drwrap_init();

    module_data_t *module = dr_get_main_module();
    app_pc pc = (app_pc)dr_get_proc_address(module->handle, "wrapped_func");
    bool ok = drwrap_wrap_ex(pc, wrap_pre, wrap_post, nullptr, DRWRAP_REPLACE_RETADDR);
    assert(ok);
    assert(drwrap_is_wrapped(pc, wrap_pre, wrap_post));
    pc = (app_pc)dr_get_proc_address(module->handle, "wrapped_subfunc");
    ok = drwrap_wrap_ex(pc, wrap_pre, wrap_post, nullptr, DRWRAP_REPLACE_RETADDR);
    assert(ok);
    dr_free_module_data(module);
}

int
main(void)
{
    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc"
                   // XXX i#4219: Work around clean call xl8 problems with traces.
                   " -disable_traces"
                   " -client_lib ';;'"))
        std::cerr << "failed to set env var!\n";
    sideline_continue = create_cond_var();
    sideline_ready_for_attach = create_cond_var();

    thread_t thread = create_thread(sideline_func, nullptr);
    dr_app_setup();
    wait_cond_var(sideline_ready_for_attach);
    VPRINT("Starting DR\n");
    dr_app_start();
    signal_cond_var(sideline_continue);
    thread_sleep(1000);
    VPRINT("Detaching\n");
    dr_app_stop_and_cleanup();
    sideline_exit.store(true, std::memory_order_release);
    join_thread(thread);

    destroy_cond_var(sideline_continue);
    destroy_cond_var(sideline_ready_for_attach);
    print("app done\n");
    return 0;
}
