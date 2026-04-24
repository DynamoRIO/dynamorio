/* **********************************************************
 * Copyright (c) 2010-2025 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/*
 * ptrace_lib.h - low-level ptrace helpers shared by internal attach code
 */

#ifndef _PTRACE_LIB_H_
#define _PTRACE_LIB_H_

#include "configure.h"
#include "../globals.h"
#include "os_exports.h"

#if defined(LINUX) && (defined(AARCH64) || (defined(X86) && defined(X64)))
#    include <sys/ptrace.h>

bool
ptrace_allow_tracer(thread_id_t tracer_tid);

bool
ptrace_attach_and_stop(thread_id_t tid, bool *used_attach);

bool
ptrace_detach(thread_id_t tid);

bool
ptrace_get_sigmask(thread_id_t tid, kernel_sigset_t *mask);

bool
ptrace_set_sigmask(thread_id_t tid, const kernel_sigset_t *mask);

bool
ptrace_unmask_signal(thread_id_t tid, int sig);

#    if defined(AARCH64) && defined(DR_HOST_AARCH64)
#        include <sys/uio.h>
#        include <linux/ptrace.h>

/* TODO i#7805: When additional architectures are supported, hide these
 * AArch64-specific register types behind a small per-architecture abstraction
 * and keep the takeover orchestration generic.
 */
bool
ptrace_get_regs(thread_id_t tid, struct user_pt_regs *regs,
                struct user_fpsimd_struct *vregs);

bool
ptrace_set_regs(thread_id_t tid, struct user_pt_regs *regs);
#    endif
#endif

#endif /* _PTRACE_LIB_H_ */
