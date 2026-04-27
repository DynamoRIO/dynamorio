/* **********************************************************
 * Copyright (c) 2026 Arm Limited All rights reserved.
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
 * * Neither the name of Arm Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, LLC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * ptrace_attach.c - DynamoRIO core integration for ptrace-assisted attach
 */

#include "configure.h"
#include "../globals.h"
#include "../dispatch.h"
#include "include/syscall.h"

#include "dr_tools.h"
#include "os_private.h"
#include "ptrace_private.h"
#include "ptrace_lib.h"

#include <stdio.h>
#include <linux/elf.h>

#if !defined(LINUX) || !(defined(AARCH64) || defined(X86))
#    error "ptrace_attach.c currently supports only Linux AArch64 and Linux x86"
#endif

/* Most of this file is intended to remain architecture-neutral. Keep ISA-specific
 * work localized to the register/vector-state helpers and the takeover-entry setup
 * so future ports can reuse the takeover orchestration unchanged.
 */

#if defined(AARCH64) && defined(DR_HOST_AARCH64)
#    ifndef NT_ARM_SVE
#        define NT_ARM_SVE 0x405
#    endif
#    define PTRACE_TAKEOVER_STACK_SIZE (DYNAMORIO_STACK_SIZE)
#    define PTRACE_TAKEOVER_STACK_ALIGNMENT 16
#endif

typedef struct _ptrace_thread_events_t {
    event_t ready;
    event_t go;
    event_t done;
} ptrace_thread_events_t;

typedef struct _ptrace_unmask_state_t {
    ptrace_thread_events_t events;
    process_id_t target_pid;
    thread_id_t skip_tid; /* ptrace_unmask_all_threads must not ptrace itself. */
    thread_id_t tracer_tid;
    int suspend_sig;
    bool success;
} ptrace_unmask_state_t;

typedef struct _ptrace_takeover_param_t {
    priv_mcontext_t mc;
    kernel_sigset_t sigset;
    void *alloc_base;
    size_t alloc_size;
    dcontext_t *dcontext;
} ptrace_takeover_param_t;

typedef struct _ptrace_takeover_state_t {
    ptrace_thread_events_t events;
    thread_id_t tracer_tid;
    thread_id_t *tids;
    uint count;
    bool success;
} ptrace_takeover_state_t;

static void
destroy_events(ptrace_thread_events_t *events)
{
    destroy_event(events->ready);
    destroy_event(events->go);
    destroy_event(events->done);
}

/* Determine whether a given thread is blocked inside one of the signal waiting
 * syscalls: rt_sigwaitinfo, rt_sigtimedwait or rt_sigtimedwait_time64. If it
 * is, whether the syscall's signal set includes SUSPEND_SIGNAL. This is used
 * to decide whether to switch to ptrace takeover for the given thread.
 */
bool
thread_in_sigtimedwait(thread_id_t tid, int suspend_sig, bool *is_in_set)
{
    char path[MAXIMUM_PATH];
    char buf[256];
    file_t file;
    int len;
    long syscall_num = -1;
    unsigned long sigset = 0;      /* Pointer to sigset_t (wait set). */
    unsigned long siginfo = 0;     /* Pointer to siginfo_t result.    */
    unsigned long timespec = 0;    /* timespec for rt_sigtimedwait.   */
    unsigned long sigset_size = 0; /* Size in bytes of sigset_t.      */
    unsigned long unused0 = 0;     /* Unused.                         */
    unsigned long unused1 = 0;     /* Unused.                         */
    bool in_set = false;

    ASSERT(suspend_sig > 0 && suspend_sig <= MAX_SIGNUM);
    ASSERT(is_in_set != NULL);
    *is_in_set = false;

    snprintf(path, BUFFER_SIZE_ELEMENTS(path), "/proc/self/task/%d/syscall", tid);
    NULL_TERMINATE_BUFFER(path);
    file = os_open(path, OS_OPEN_READ);
    if (file == INVALID_FILE)
        return false;
    len = os_read(file, buf, sizeof(buf) - 1);
    os_close(file);
    if (len <= 0)
        return false;
    buf[len] = '\0';

    if (sscanf(buf, "%ld %lx %lx %lx %lx %lx %lx", &syscall_num, &sigset, &siginfo,
               &timespec, &sigset_size, &unused0, &unused1) < 1)
        return false;

    switch (syscall_num) {
    case SYS_rt_sigtimedwait: break;
#ifdef SYS_rt_sigwaitinfo
    case SYS_rt_sigwaitinfo: break;
#endif
#ifdef SYS_rt_sigtimedwait_time64
    case SYS_rt_sigtimedwait_time64: break;
#endif
    default: return false;
    }

    if (sigset != 0 && sigset_size != 0) {
        kernel_sigset_t kset = { 0 };
        size_t to_read = sigset_size < sizeof(kset) ? sigset_size : sizeof(kset);
        if (d_r_safe_read((void *)(ptr_uint_t)sigset, to_read, &kset)) {
            in_set = kernel_sigismember(&kset, suspend_sig);
        } else {
            SYSLOG_INTERNAL_WARNING_ONCE("thread_in_sigtimedwait: failed to read sigset "
                                         "(tid=%d, ptr=%p, sigset_size=%zu)",
                                         tid, (void *)(ptr_uint_t)sigset, sigset_size);
        }
        *is_in_set = in_set;
    } else {
        DOLOG(4, LOG_ASYNCH, {
            LOG(GLOBAL, LOG_ASYNCH, 4,
                "thread_in_sigtimedwait: no sigset (tid=%d, nr=%ld, ptr=%p, "
                "sigset_size=%lu)\n",
                tid, syscall_num, (void *)(ptr_uint_t)sigset, (ulong)sigset_size);
        });
    }
    return true;
}

static void
ptrace_unmask_all_threads(void *param)
{
    ptrace_unmask_state_t *state = (ptrace_unmask_state_t *)param;
    dcontext_t *dcontext = get_thread_private_dcontext();
    uint num_threads = 0;
    thread_id_t *tids = NULL;
    bool any_tids_attempted = false;
    bool any_tids_failed = false;

    state->tracer_tid = d_r_get_thread_id();
    signal_event(state->events.ready);
    wait_for_event(state->events.go, 0);

    tids = os_list_threads_by_pid(dcontext, state->target_pid, &num_threads);
    if (tids != NULL) {
        for (uint i = 0; i < num_threads; i++) {
            thread_id_t tid = tids[i];
            if (tid == state->skip_tid)
                continue;
            if (!ptrace_attach_and_stop(tid, NULL))
                continue;

            any_tids_attempted = true;
            if (!ptrace_unmask_signal(tid, state->suspend_sig))
                any_tids_failed = true;

            ptrace_detach(tid);
        }
        HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, num_threads, ACCT_THREAD_MGT,
                        PROTECTED);
    }

    state->success = !any_tids_attempted || !any_tids_failed;
    signal_event(state->events.done);
}

/* XXX: There is code in this function similar to that in injector.c. We may
 * want to put as much of this as possible into common helpers. Implement this
 * type of feature detection function to support other architectures enabled by
 * the -attach_unmask_suspend_signal option. All other code paths implementing
 * -attach_unmask_suspend_signal are architecture neutral.
 */
#if defined(AARCH64) && defined(DR_HOST_AARCH64)
static NOINLINE void
ptrace_takeover_entry(void *param)
{
    ptrace_takeover_param_t *p = (ptrace_takeover_param_t *)param;

    ASSERT(p != NULL);
    os_thread_take_over(&p->mc, &p->sigset); /* This should not return. */
    ASSERT_NOT_REACHED();
}

static bool
ptrace_get_sve_state(thread_id_t tid, struct user_sve_header **sve_state_out,
                     size_t *sve_state_size_out)
{
    struct user_sve_header sve_header = { 0 };
    struct iovec iov = { &sve_header, sizeof(sve_header) };
    ptr_int_t res;

    /* TODO i#7805: Add a per-architecture vector-state helper and keep the
     * takeover flow below agnostic to SVE vs XSAVE vs other ISA-specific state.
     */
    ASSERT(sve_state_out != NULL);
    ASSERT(sve_state_size_out != NULL);
    *sve_state_out = NULL;
    *sve_state_size_out = 0;

    if (!proc_has_feature(FEATURE_SVE))
        return true;

    res =
        dynamorio_syscall(SYS_ptrace, 4, PTRACE_GETREGSET, tid, (void *)NT_ARM_SVE, &iov);
    if (res != 0)
        return true; /* No SVE support or no active SVE state. */

    if (sve_header.vl > sizeof(dr_simd_t))
        return false;

    const size_t full_sve_state_size =
        SVE_PT_SIZE(sve_vq_from_vl(sve_header.vl), SVE_PT_REGS_SVE);
    struct user_sve_header *sve_state = (struct user_sve_header *)global_heap_alloc(
        full_sve_state_size HEAPACCT(ACCT_THREAD_MGT));
    if (sve_state == NULL)
        return false;

    iov.iov_base = sve_state;
    iov.iov_len = full_sve_state_size;
    res =
        dynamorio_syscall(SYS_ptrace, 4, PTRACE_GETREGSET, tid, (void *)NT_ARM_SVE, &iov);
    if (res != 0) {
        global_heap_free(sve_state, full_sve_state_size HEAPACCT(ACCT_THREAD_MGT));
        return false;
    }

    if (!TEST(SVE_PT_REGS_SVE, sve_state->flags)) {
        global_heap_free(sve_state, full_sve_state_size HEAPACCT(ACCT_THREAD_MGT));
        return true;
    }

    *sve_state_out = sve_state;
    *sve_state_size_out = full_sve_state_size;
    return true;
}

/* XXX: There is code in this function similar to that in injector.c. We may
 * want to put as much of this as possible into common helpers. Implement this
 * register state transfer function to support other architectures enabled by
 * the -attach_unmask_suspend_signal option. All other code paths implementing
 * -attach_unmask_suspend_signal are architecture neutral.
 */
static void
user_regs_to_mc_aarch64(priv_mcontext_t *mc, struct user_pt_regs *regs,
                        struct user_fpsimd_struct *vregs,
                        struct user_sve_header *sve_state)
{
    /* TODO i#7805: Add user_regs_to_mc_<arch>() variants and dispatch from a
     * narrow per-architecture seam instead of branching through the generic
     * takeover logic.
     */
    ASSERT(mc != NULL);
    ASSERT(regs != NULL);
    ASSERT(vregs != NULL);
    ASSERT(sizeof(regs->regs) / sizeof(regs->regs[0]) == 31);

    mc->r0 = regs->regs[0];
    mc->r1 = regs->regs[1];
    mc->r2 = regs->regs[2];
    mc->r3 = regs->regs[3];
    mc->r4 = regs->regs[4];
    mc->r5 = regs->regs[5];
    mc->r6 = regs->regs[6];
    mc->r7 = regs->regs[7];
    mc->r8 = regs->regs[8];
    mc->r9 = regs->regs[9];
    mc->r10 = regs->regs[10];
    mc->r11 = regs->regs[11];
    mc->r12 = regs->regs[12];
    mc->r13 = regs->regs[13];
    mc->r14 = regs->regs[14];
    mc->r15 = regs->regs[15];
    mc->r16 = regs->regs[16];
    mc->r17 = regs->regs[17];
    mc->r18 = regs->regs[18];
    mc->r19 = regs->regs[19];
    mc->r20 = regs->regs[20];
    mc->r21 = regs->regs[21];
    mc->r22 = regs->regs[22];
    mc->r23 = regs->regs[23];
    mc->r24 = regs->regs[24];
    mc->r25 = regs->regs[25];
    mc->r26 = regs->regs[26];
    mc->r27 = regs->regs[27];
    mc->r28 = regs->regs[28];
    mc->r29 = regs->regs[29];
    mc->r30 = regs->regs[30];
    mc->sp = regs->sp;
    mc->pc = (app_pc)regs->pc;
    mc->nzcv = regs->pstate & 0xF0000000;

    if (sve_state != NULL) {
        ASSERT(TEST(SVE_PT_REGS_SVE, sve_state->flags));
        ASSERT(sve_state->vl <= sizeof(dr_simd_t));
        const size_t vq = sve_vq_from_vl(sve_state->vl);
        for (size_t i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
            memcpy(&mc->simd[i].u64[0], (byte *)sve_state + SVE_PT_SVE_ZREG_OFFSET(vq, i),
                   SVE_PT_SVE_ZREG_SIZE(vq));
        }
        for (size_t i = 0; i < MCXT_NUM_SVEP_SLOTS; i++) {
            memcpy(&mc->svep[i].u16[0], (byte *)sve_state + SVE_PT_SVE_PREG_OFFSET(vq, i),
                   SVE_PT_SVE_PREG_SIZE(vq));
        }
        memcpy(&mc->ffr.u16[0], (byte *)sve_state + SVE_PT_SVE_FFR_OFFSET(vq),
               SVE_PT_SVE_FFR_SIZE(vq));
    } else {
        for (int i = 0; i < MCXT_NUM_SIMD_SVE_SLOTS; i++) {
            memcpy(&mc->simd[i].q, &vregs->vregs[i], sizeof(mc->simd->q));
        }
    }
    mc->fpsr = vregs->fpsr;
    mc->fpcr = vregs->fpcr;
}
#endif /* AARCH64 && DR_HOST_AARCH64 */

static bool
ptrace_takeover_thread(thread_id_t tid)
{
#if defined(AARCH64) && defined(DR_HOST_AARCH64)
    struct user_pt_regs regs;
    struct user_fpsimd_struct vregs;
    struct user_sve_header *sve_state = NULL;
    size_t sve_state_size = 0;
    ptrace_takeover_param_t *param;
    byte *alloc_base;
    size_t alloc_size;
    byte *stack;
    byte *stack_top;

    /* TODO i#7805: Keep this control flow shared across architectures. New
     * ports should plug in architecture-specific register fetch/mcontext
     * translation/takeover-entry setup helpers here rather than cloning the
     * whole function.
     */
    if (!ptrace_attach_and_stop(tid, NULL))
        return false;
    if (!ptrace_get_regs(tid, &regs, &vregs)) {
        ptrace_detach(tid);
        return false;
    }
    if (!ptrace_get_sve_state(tid, &sve_state, &sve_state_size)) {
        ptrace_detach(tid);
        return false;
    }

    alloc_size = sizeof(ptrace_takeover_param_t) + PTRACE_TAKEOVER_STACK_SIZE +
        PTRACE_TAKEOVER_STACK_ALIGNMENT;
    alloc_base = global_heap_alloc(alloc_size HEAPACCT(ACCT_THREAD_MGT));
    if (alloc_base == NULL) {
        ptrace_detach(tid);
        return false;
    }
    param = (ptrace_takeover_param_t *)alloc_base;
    memset(param, 0, sizeof(*param));
    param->alloc_base = alloc_base;
    param->alloc_size = alloc_size;
    user_regs_to_mc_aarch64(&param->mc, &regs, &vregs, sve_state);
    if (sve_state != NULL) {
        global_heap_free(sve_state, sve_state_size HEAPACCT(ACCT_THREAD_MGT));
        sve_state = NULL;
        sve_state_size = 0;
    }
    if (!ptrace_get_sigmask(tid, &param->sigset))
        memset(&param->sigset, 0, sizeof(param->sigset));

    stack = alloc_base + sizeof(ptrace_takeover_param_t);
    stack_top = stack + PTRACE_TAKEOVER_STACK_SIZE;
    stack_top = (byte *)ALIGN_BACKWARD(stack_top, PTRACE_TAKEOVER_STACK_ALIGNMENT);

    /* XXX: Executing on the stack violates security policies so this will fail
     * on some platforms. One fix is to mmap() executable memory.
     */
    /* TODO i#7805: Add a small helper to program the takeover entry register
     * state for each architecture (argument register, stack pointer, pc).
     */
    regs.sp = (uint64)stack_top;
    regs.pc = (uint64)(ptr_uint_t)ptrace_takeover_entry;
    regs.regs[0] = (uint64)(ptr_uint_t)param;

    if (!ptrace_set_regs(tid, &regs)) {
        ptrace_detach(tid);
        global_heap_free(alloc_base, alloc_size HEAPACCT(ACCT_THREAD_MGT));
        return false;
    }

    ptrace_takeover_record_set(tid, param);
    if (!ptrace_detach(tid)) {
        ptrace_takeover_record_set(tid, NULL);
        global_heap_free(alloc_base, alloc_size HEAPACCT(ACCT_THREAD_MGT));
        return false;
    }
    return true;
#else
    ASSERT_NOT_IMPLEMENTED(false &&
                           "ptrace takeover currently only implemented on Linux AArch64");
    return false;
#endif
}

static void
ptrace_takeover_worker(void *param)
{
    ptrace_takeover_state_t *state = (ptrace_takeover_state_t *)param;

    state->tracer_tid = d_r_get_thread_id();
    signal_event(state->events.ready);
    wait_for_event(state->events.go, 0);

    state->success = true;
    for (uint i = 0; i < state->count; i++) {
        if (!ptrace_takeover_thread(state->tids[i]))
            state->success = false;
    }
    signal_event(state->events.done);
}

static void
ptrace_takeover_resume(void *arg)
{
    ptrace_takeover_param_t *param = (ptrace_takeover_param_t *)arg;
    dcontext_t *dcontext = param->dcontext;

    ptrace_takeover_cleanup(param);
    ASSERT(dcontext != NULL);
    d_r_dispatch(dcontext);
    ASSERT_NOT_REACHED();
}

static void
ptrace_takeover_dispatch(void *token)
{
    ptrace_takeover_param_t *param = (ptrace_takeover_param_t *)token;

    ASSERT(param != NULL);
    ASSERT(param->dcontext != NULL);
    call_switch_stack(param, param->dcontext->dstack, ptrace_takeover_resume,
                      NULL /* not on d_r_initstack */, false /* should not return */);
    ASSERT_NOT_REACHED();
}

void
ptrace_takeover_cleanup(void *token)
{
    ptrace_takeover_param_t *param = (ptrace_takeover_param_t *)token;

    if (param != NULL && param->alloc_base != NULL && param->alloc_size != 0) {
        global_heap_free(param->alloc_base, param->alloc_size HEAPACCT(ACCT_THREAD_MGT));
    }
}

void
ptrace_attach_on_thread_takeover(dcontext_t *dcontext, void *token)
{
    ptrace_takeover_param_t *param = (ptrace_takeover_param_t *)token;

    if (param == NULL)
        return;
#if !defined(AARCH64) || !defined(DR_HOST_AARCH64)
    ASSERT_NOT_IMPLEMENTED(false &&
                           "ptrace takeover resume NYI without native Linux AArch64");
    return;
#endif
    param->dcontext = dcontext;
    call_switch_stack(param, dcontext->dstack, ptrace_takeover_dispatch,
                      NULL /* not on d_r_initstack */, false /* should not return */);
    ASSERT_NOT_REACHED();
}

bool
os_ptrace_takeover_threads(dcontext_t *dcontext, thread_id_t *tids, uint count)
{
    ptrace_takeover_state_t *state;
    bool ok = false;

    (void)dcontext;
    if (count == 0)
        return true;
#if !defined(AARCH64) || !defined(DR_HOST_AARCH64)
    ASSERT_NOT_IMPLEMENTED(false &&
                           "ptrace takeover currently only implemented on Linux AArch64");
    return false;
#endif
    state = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, ptrace_takeover_state_t, ACCT_THREAD_MGT,
                            PROTECTED);
    memset(state, 0, sizeof(*state));
    state->events.ready = create_event();
    state->events.go = create_event();
    state->events.done = create_event();
    state->tids = tids;
    state->count = count;

    if (!dr_create_client_thread(ptrace_takeover_worker, state)) {
        destroy_events(&state->events);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, state, ptrace_takeover_state_t, ACCT_THREAD_MGT,
                       PROTECTED);
        return false;
    }

    wait_for_event(state->events.ready, 0);
    if (state->tracer_tid != INVALID_THREAD_ID)
        ptrace_allow_tracer(state->tracer_tid);
    signal_event(state->events.go);
    wait_for_event(state->events.done, 0);
    ok = state->success;

    destroy_events(&state->events);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, state, ptrace_takeover_state_t, ACCT_THREAD_MGT,
                   PROTECTED);
    return ok;
}

/* This function creates a thread, ptrace_unmask_all_threads, which uses
 * /proc/<pid>/task to list threads by PID, attaching and stopping each thread
 * with ptrace. PTRACE_{GETSIGMASK,SETSIGMASK} are used to clear SUSPEND_SIGNAL
 * from each thread's blocked mask before detaching with ptrace.
 */
bool
os_unmask_suspend_signal_via_ptrace(thread_id_t skip_tid)
{
    ptrace_unmask_state_t *state;
    bool ok = false;

    state = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, ptrace_unmask_state_t, ACCT_THREAD_MGT,
                            PROTECTED);
    memset(state, 0, sizeof(*state));

    state->events.ready = create_event();
    state->events.go = create_event();
    state->events.done = create_event();
    state->target_pid = get_process_id();
    state->skip_tid = skip_tid;
    state->suspend_sig = suspend_signum;

    if (!dr_create_client_thread(ptrace_unmask_all_threads, state)) {
        destroy_events(&state->events);
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, state, ptrace_unmask_state_t, ACCT_THREAD_MGT,
                       PROTECTED);
        return false;
    }

    wait_for_event(state->events.ready, 0);
    if (state->tracer_tid != INVALID_THREAD_ID)
        ptrace_allow_tracer(state->tracer_tid);
    signal_event(state->events.go);
    wait_for_event(state->events.done, 0);
    ok = state->success;

    destroy_events(&state->events);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, state, ptrace_unmask_state_t, ACCT_THREAD_MGT,
                   PROTECTED);
    return ok;
}
