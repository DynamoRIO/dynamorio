/* **********************************************************
 * Copyright (c) 2019-2021 Google, Inc.  All rights reserved.
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

/* This test uses rseq but does not comply with the conventions that DR requires.
 * It shares similar code to the other rseq tests but not enough to try to share
 * the separate bits of code.
 */

#include "tools.h"
#ifndef LINUX
#    error Only Linux is supported.
#endif
#include "../../core/unix/include/syscall.h"
#ifndef HAVE_RSEQ
#    error The linux/rseq header is required.
#endif
#include <linux/rseq.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sched.h>

static void
handler(int sig, siginfo_t *info, void *ucxt)
{
    print("In handler for signal %d\n", sig);
}

int
main()
{
    /* Rather than sample app code doing regular rseq things, here we're testing
     * the -disable_rseq option.  It should do two things:
     * 1) Return ENOSYS from SYS_rseq.
     * 2) Skip the SYS_rseq system call.
     */
    const size_t size = getpagesize();
    void *map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(map != MAP_FAILED);
    struct rseq *rseq_tls = (struct rseq *)map;
    rseq_tls->cpu_id = RSEQ_CPU_ID_UNINITIALIZED;
    int res = syscall(SYS_rseq, rseq_tls, sizeof(*rseq_tls), 0, 0);
    /* Ensure we got the right return value. */
    assert(res != 0 && errno == ENOSYS);
    /* Ensure the syscall was skipped by making rseq_tls unreadable.  The kernel will
     * then force a SIGSEGV if it can't read it on a potential restart point.
     */
    res = munmap(map, size);
    assert(res == 0);
    /* Trigger a restart by sending a signal. */
    intercept_signal(SIGUSR1, (handler_3_t)handler, false);
    kill(getpid(), SIGUSR1);

    print("All done\n");
    return 0;
}
