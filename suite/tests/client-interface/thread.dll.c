/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
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
#include "client_tools.h"
#ifdef UNIX
#    include <sys/time.h>
#endif

#ifdef WINDOWS
#    define THREAD_ARG ((void *)dr_get_process_id())
#    define TLS_ATTR __declspec(thread)
#else
/* thread actually has own pid so just using a constant to test arg passing */
#    define THREAD_ARG ((void *)37)
#    define TLS_ATTR __thread
#endif

/* Eventually this routine will test i/o by waiting on a file */

#define MINSERT instrlist_meta_preinsert

static uint num_lea = 0;

static reg_id_t tls_seg;
static uint tls_offs;
#define CANARY 0xbadcab42
#define NUM_TLS_SLOTS 4

#ifdef X64
#    define ASM_XAX "rax"
#    define ASM_XDX "rdx"
#    define ASM_XBP "rbp"
#    define ASM_XSP "rsp"
#    define ASM_SEG "gs"
#else
#    define ASM_XAX "eax"
#    define ASM_XDX "edx"
#    define ASM_XBP "ebp"
#    define ASM_XSP "esp"
#    define ASM_SEG "fs"
#endif

static void *child_alive;
static void *child_continue;
static void *child_dead;
static bool nops_matched;

static int client_thread_count;
static int counter32;
#ifdef X64
static int64 counter64;
#endif

#ifdef UNIX
/* test PR 368737: add client timer support */
static void
event_timer(void *drcontext, dr_mcontext_t *mcontext)
{
    dr_fprintf(STDERR, "event_timer fired\n");
    if (!dr_set_itimer(ITIMER_REAL, 0, event_timer))
        dr_fprintf(STDERR, "unable to disable timer\n");
}
#endif

static TLS_ATTR int tls = 42;

static void
thread_func(void *arg)
{
    /* FIXME: should really test corner cases: do raw system calls, etc. to
     * ensure we're treating it as a true native thread
     */
    ASSERT(arg == THREAD_ARG);
    dr_fprintf(STDERR, "client thread is alive tls=%d\n", tls);
    tls++;
    dr_event_signal(child_alive);

    /* Just a sanity check that these functions operate.  We do not take the
     * time to set up racing threads or sthg.
     */
    int count = dr_atomic_add32_return_sum(&counter32, 1);
    ASSERT(count > 0 && count <= counter32);
    int local_counter;
    dr_atomic_store32(&local_counter, 42);
    count = dr_atomic_load32(&local_counter);
    ASSERT(count == 42);
    ASSERT(local_counter == 42);
#ifdef X64
    int64 count64 = dr_atomic_add64_return_sum(&counter64, 1);
    ASSERT(count64 > 0 && count64 <= counter64);
    int64 local_counter64;
    dr_atomic_store64(&local_counter64, 42);
    count64 = dr_atomic_load64(&local_counter64);
    ASSERT(count64 == 42);
    ASSERT(local_counter64 == 42);
#endif

#ifdef UNIX
    if (!dr_set_itimer(ITIMER_REAL, 10, event_timer))
        dr_fprintf(STDERR, "unable to set timer callback\n");
    dr_sleep(30);
#endif
    dr_event_wait(child_continue);
    dr_fprintf(STDERR, "client thread is dying\n");
    dr_event_signal(child_dead);
}

static void
at_lea(uint opc, app_pc tag)
{
    /* PR 223285: test (one side of) DR_ASSERT for something we know will succeed
     * (we don't want msgboxes in regressions)
     */
    DR_ASSERT(opc == OP_lea);
    ASSERT((process_id_t)(ptr_uint_t)dr_get_tls_field(dr_get_current_drcontext()) ==
           dr_get_process_id() + 1 /*we added 1 inline*/);
    dr_set_tls_field(dr_get_current_drcontext(), (void *)(ptr_uint_t)dr_get_process_id());
    num_lea++;
    /* FIXME: should do some fp ops and really test the fp state preservation */
}

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    instr_t *instr, *next_instr;
    int num_nops = 0;
    bool in_nops = false;

    if (child_alive == NULL) {
        /* We run this once here: we can't do it in dr_init() b/c the client thread
         * won't execute until the app starts (i#2335).
         */
        bool success;
        child_alive = dr_event_create();
        child_continue = dr_event_create();
        child_dead = dr_event_create();

        /* PR 222812: start up and shut down a client thread */
        success = dr_create_client_thread(thread_func, THREAD_ARG);
        ASSERT(success);
        client_thread_count++; /* app is single-threaded so no races */
        dr_event_wait(child_alive);
        dr_event_signal(child_continue);
        dr_event_wait(child_dead);
        dr_fprintf(STDERR, "PR 222812: client thread test passed\n");
    }

    for (instr = instrlist_first(bb); instr != NULL; instr = next_instr) {
        next_instr = instr_get_next(instr);
        if (instr_get_opcode(instr) == OP_lea) {
            /* PR 200411: test inline tls access by adding 1 */
            dr_save_reg(drcontext, bb, instr, REG_XAX, SPILL_SLOT_1);
            dr_insert_read_tls_field(drcontext, bb, instr, REG_XAX);
            instrlist_meta_preinsert(
                bb, instr,
                INSTR_CREATE_lea(
                    drcontext, opnd_create_reg(REG_XAX),
                    opnd_create_base_disp(REG_XAX, REG_NULL, 0, 1, OPSZ_lea)));
            dr_insert_write_tls_field(drcontext, bb, instr, REG_XAX);
            dr_restore_reg(drcontext, bb, instr, REG_XAX, SPILL_SLOT_1);
            dr_insert_clean_call(drcontext, bb, instr, at_lea, true /*save fp*/, 2,
                                 OPND_CREATE_INT32(instr_get_opcode(instr)),
                                 OPND_CREATE_INTPTR(tag));
        }
        if (instr_get_opcode(instr) == OP_nop) {
            if (!in_nops) {
                in_nops = true;
                num_nops = 1;
            } else
                num_nops++;
        } else
            in_nops = false;
    }
    if (num_nops == 17 && !nops_matched) {
        /* PR 210591: test transparency by having client create a thread after
         * app has loaded a library and ensure its DllMain is not notified
         */
        bool success;
        nops_matched = true;
        /* reset cond vars */
        dr_event_reset(child_alive);
        dr_event_reset(child_continue);
        dr_event_reset(child_dead);
        dr_fprintf(STDERR, "PR 210591: testing client transparency\n");
        success = dr_create_client_thread(thread_func, THREAD_ARG);
        ASSERT(success);
        client_thread_count++; /* app is single-threaded so no races */
        dr_event_wait(child_alive);
        /* We leave the client thread alive until the app exits, to test i#1489 */
#ifdef UNIX
        dr_sleep(30); /* ensure we get an alarm */
#endif
    }
    return DR_EMIT_DEFAULT;
}

void
exit_event(void)
{
    bool success = dr_raw_tls_cfree(tls_offs, NUM_TLS_SLOTS);
    ASSERT(success);
    ASSERT(num_lea > 0);
#ifdef UNIX /* XXX i#2346: we should delay client threads termination on Windows too. */
    dr_fprintf(STDERR, "process is exiting\n");
    dr_event_signal(child_continue);
    dr_event_wait(child_dead);
#endif
    /* DR should have terminated the client thread for us */
    dr_event_destroy(child_alive);
    dr_event_destroy(child_continue);
    dr_event_destroy(child_dead);
    ASSERT(counter32 == client_thread_count);
#ifdef X64
    ASSERT(counter64 == client_thread_count);
#endif
}

static bool
str_eq(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL)
        return false;
    while (*s1 == *s2) {
        if (*s1 == '\0')
            return true;
        s1++;
        s2++;
    }
    return false;
}

static void
thread_init_event(void *drcontext)
{
    int i;
    dr_set_tls_field(drcontext, (void *)(ptr_uint_t)dr_get_process_id());
    for (i = 0; i < NUM_TLS_SLOTS; i++) {
        int idx = tls_offs + i * sizeof(void *);
        ptr_uint_t val = (ptr_uint_t)(CANARY + i);
#ifdef WINDOWS
        IF_X64_ELSE(__writegsqword, __writefsdword)(idx, val);
#else
        asm("mov %0, %%" ASM_XAX : : "m"((val)) : ASM_XAX);
        asm("mov %0, %%edx" : : "m"((idx)) : ASM_XDX);
        asm("mov %%" ASM_XAX ", %%" ASM_SEG ":(%%" ASM_XDX ")" : : : ASM_XAX, ASM_XDX);
#endif
    }
}

static void
thread_exit_event(void *drcontext)
{
    int i;
    instrlist_t *ilist = instrlist_create(drcontext);
    dr_insert_read_raw_tls(drcontext, ilist, NULL, tls_seg, tls_offs, DR_REG_START_GPR);
    ASSERT(opnd_same(dr_raw_tls_opnd(drcontext, tls_seg, tls_offs),
                     instr_get_src(instrlist_first(ilist), 0)));
    instrlist_clear_and_destroy(drcontext, ilist);

    for (i = 0; i < NUM_TLS_SLOTS; i++) {
        int idx = tls_offs + i * sizeof(void *);
        ptr_uint_t val;
#ifdef WINDOWS
        val = IF_X64_ELSE(__readgsqword, __readfsdword)(idx);
#else
        asm("mov %0, %%eax" : : "m"((idx)) : ASM_XAX);
        asm("mov %%" ASM_SEG ":(%%" ASM_XAX "), %%" ASM_XAX : : : ASM_XAX);
        asm("mov %%" ASM_XAX ", %0" : "=m"((val)) : : ASM_XAX);
#endif
        dr_fprintf(STDERR, "TLS slot %d is " PFX "\n", i, val);
    }
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    bool success;
    /* PR 216931: client options */
    const char *ops = dr_get_options(id);
    dr_fprintf(STDERR, "PR 216931: client options are %s\n", ops);
    ASSERT(str_eq(ops, "-paramx -paramy"));
    ASSERT(argc == 3 && str_eq(argv[1], "-paramx") && str_eq(argv[2], "-paramy"));

    dr_register_bb_event(bb_event);
    dr_register_exit_event(exit_event);
    dr_register_thread_init_event(thread_init_event);
    dr_register_thread_exit_event(thread_exit_event);

    /* i#108: client raw TLS */
    success = dr_raw_tls_calloc(&tls_seg, &tls_offs, NUM_TLS_SLOTS, 0);
    ASSERT(success);
    ASSERT(tls_seg == IF_X64_ELSE(SEG_GS, SEG_FS));

    /* PR 219381: dr_get_application_name() and dr_get_process_id() */
#ifdef WINDOWS
    dr_fprintf(STDERR, "inside app %s\n", dr_get_application_name());
#else /* UNIX - append .exe so can use same expect file. */
    dr_fprintf(STDERR, "inside app %s.exe\n", dr_get_application_name());
#endif

    {
        /* test PR 198871: client locks are all at same rank */
        void *lock1 = dr_mutex_create();
        void *lock2 = dr_mutex_create();
        dr_mutex_lock(lock1);
        dr_mutex_lock(lock2);
        dr_fprintf(STDERR, "PR 198871 locking test...");
        dr_mutex_unlock(lock2);
        dr_mutex_unlock(lock1);
        dr_mutex_destroy(lock1);
        dr_mutex_destroy(lock2);
        dr_fprintf(STDERR, "...passed\n");
    }
}
