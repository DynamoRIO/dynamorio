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
 * ptrace_lib.c - low-level ptrace helpers for internal attach support
 */

#include "configure.h"
#include "../globals.h"
#include "os_private.h"
#include "ptrace_lib.h"
#include "include/syscall.h"
#include <errno.h>
#include <linux/elf.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#if !defined(LINUX) || !(defined(AARCH64) || defined(X86))
#    error "ptrace_lib.c currently supports only Linux AArch64 and Linux x86"
#endif

#ifndef NT_PRSTATUS
#    define NT_PRSTATUS 1
#endif
#ifndef NT_FPREGSET
#    define NT_FPREGSET 2
#endif

/* For older Linux versions these constants may not be defined in
 * libc headers even though a kernel supports them.
 */
#ifndef PTRACE_GETSIGMASK
#    define PTRACE_GETSIGMASK 0x420a
#    define PTRACE_SETSIGMASK 0x420b
#endif
#ifndef PR_SET_PTRACER
#    define PR_SET_PTRACER 0x59616d61
#endif

/* The PTRACE_{GET,SET}SIGMASK API requires a size argument but the size varies
 * by kernel and userspace ABI. Probe several candidate sizes and cache the one
 * that works.
 */
#define PTRACE_SIGMASK_MAX_SIZE 128
static size_t ptrace_sigmask_size;

/* Keep the helpers in this file architecture-neutral where possible. The
 * register read/write entry points at the bottom are the main ISA-specific seam
 * future ports should replace.
 */

static bool
ptrace_wait_for_stop(thread_id_t tid)
{
    int status = 0;
    while (true) {
        ptr_int_t res = dynamorio_syscall(SYS_wait4, 4, tid, &status, __WALL, NULL);
        if (res == -EINTR)
            continue;
        if (res < 0)
            return false;
        return WIFSTOPPED(status);
    }
}

static bool
ptrace_get_sigmask_with_size(thread_id_t tid, size_t sz, kernel_sigset_t *mask)
{
    byte buf[PTRACE_SIGMASK_MAX_SIZE];
    size_t to_copy;

    if (sz == 0 || sz > sizeof(buf))
        return false;
    memset(buf, 0, sz);
    if (dynamorio_syscall(SYS_ptrace, 4, PTRACE_GETSIGMASK, tid, (void *)sz, buf) != 0)
        return false;
    memset(mask, 0, sizeof(*mask));
    to_copy = sz < sizeof(*mask) ? sz : sizeof(*mask);
    memcpy(mask, buf, to_copy);
    return true;
}

bool
ptrace_allow_tracer(thread_id_t tracer_tid)
{
    return dynamorio_syscall(SYS_prctl, 5, PR_SET_PTRACER, tracer_tid, 0, 0, 0) == 0;
}

bool
ptrace_attach_and_stop(thread_id_t tid, bool *used_attach)
{
    ptr_int_t res = dynamorio_syscall(SYS_ptrace, 4, PTRACE_SEIZE, tid, NULL, NULL);

    if (res == 0) {
        if (used_attach != NULL)
            *used_attach = false;
        res = dynamorio_syscall(SYS_ptrace, 4, PTRACE_INTERRUPT, tid, NULL, NULL);
        if (res != 0)
            return false;
        return ptrace_wait_for_stop(tid);
    }
    if (res == -EINVAL || res == -ENOSYS) {
        res = dynamorio_syscall(SYS_ptrace, 4, PTRACE_ATTACH, tid, NULL, NULL);
        if (res != 0)
            return false;
        if (used_attach != NULL)
            *used_attach = true;
        return ptrace_wait_for_stop(tid);
    }
    return false;
}

bool
ptrace_detach(thread_id_t tid)
{
    return dynamorio_syscall(SYS_ptrace, 4, PTRACE_DETACH, tid, NULL, NULL) == 0;
}

bool
ptrace_get_sigmask(thread_id_t tid, kernel_sigset_t *mask)
{
    static const size_t sizes[] = { sizeof(kernel_sigset_t), 8, 16, 32, 64, 128 };

    if (ptrace_sigmask_size != 0) {
        return ptrace_get_sigmask_with_size(tid, ptrace_sigmask_size, mask);
    }
    for (uint i = 0; i < BUFFER_SIZE_ELEMENTS(sizes); i++) {
        size_t sz = sizes[i];
        if (ptrace_get_sigmask_with_size(tid, sz, mask)) {
            ptrace_sigmask_size = sz;
            return true;
        }
    }
    return false;
}

bool
ptrace_set_sigmask(thread_id_t tid, const kernel_sigset_t *mask)
{
    byte buf[PTRACE_SIGMASK_MAX_SIZE];
    size_t sz = ptrace_sigmask_size;
    size_t to_copy;

    if (sz == 0) {
        kernel_sigset_t unused;
        if (!ptrace_get_sigmask(tid, &unused))
            return false;
        sz = ptrace_sigmask_size;
        if (sz == 0)
            return false;
    }
    if (sz == 0 || sz > sizeof(buf))
        return false;
    memset(buf, 0, sz);
    to_copy = sz < sizeof(*mask) ? sz : sizeof(*mask);
    memcpy(buf, mask, to_copy);
    return dynamorio_syscall(SYS_ptrace, 4, PTRACE_SETSIGMASK, tid, (void *)sz, buf) == 0;
}

bool
ptrace_unmask_signal(thread_id_t tid, int sig)
{
    kernel_sigset_t mask;

    if (sig <= 0 || sig > MAX_SIGNUM)
        return false;
    if (!ptrace_get_sigmask(tid, &mask))
        return false;
    if (kernel_sigismember(&mask, sig)) {
        kernel_sigdelset(&mask, sig);
        if (!ptrace_set_sigmask(tid, &mask))
            return false;
    }
    return true;
}

#if defined(AARCH64) && defined(DR_HOST_AARCH64)
bool
ptrace_get_regs(thread_id_t tid, struct user_pt_regs *regs,
                struct user_fpsimd_struct *vregs)
{
    struct iovec iov = { regs, sizeof(*regs) };

    /* TODO i#7805: Introduce per-architecture register container types and
     * helpers here so callers can stay generic when x86 or other ports are
     * added.
     */
    ASSERT(regs != NULL);
    ASSERT(vregs != NULL);

    if (dynamorio_syscall(SYS_ptrace, 4, PTRACE_GETREGSET, tid, (void *)NT_PRSTATUS,
                          &iov) != 0)
        return false;
    iov.iov_base = vregs;
    iov.iov_len = sizeof(*vregs);
    if (dynamorio_syscall(SYS_ptrace, 4, PTRACE_GETREGSET, tid, (void *)NT_FPREGSET,
                          &iov) != 0)
        return false;
    return true;
}

bool
ptrace_set_regs(thread_id_t tid, struct user_pt_regs *regs)
{
    struct iovec iov = { regs, sizeof(*regs) };

    /* TODO i#7805: Pair this with per-architecture takeover-entry setup so new
     * ports only replace the ISA-specific register programming.
     */
    ASSERT(regs != NULL);
    return dynamorio_syscall(SYS_ptrace, 4, PTRACE_SETREGSET, tid, (void *)NT_PRSTATUS,
                             &iov) == 0;
}
#endif /* AARCH64 && DR_HOST_AARCH64 */
