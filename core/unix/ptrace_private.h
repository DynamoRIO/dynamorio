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

#ifndef _PTRACE_PRIVATE_H_
#define _PTRACE_PRIVATE_H_

#include "os_private.h"

#if defined(LINUX) && (defined(AARCH64) || defined(X86))
/* This header declares ptrace takeover functions used by core/unix/os.c.
 * Implementing ptrace takeover for a new architecture should minimise
 * architecture specific code. Below are Advanced SIMD and SVE date structures
 * specific to AArch64.
 */
#    if defined(AARCH64) && defined(DR_HOST_AARCH64)
struct user_pt_regs;
struct user_fpsimd_struct;
struct user_sve_header;
#    endif

thread_id_t *
os_list_threads_by_pid(dcontext_t *dcontext, process_id_t pid, uint *num_threads_out);

void
ptrace_takeover_record_set(thread_id_t tid, void *param);

void *
ptrace_takeover_record_get(thread_id_t tid);

bool
ptrace_takeover_record_present(thread_id_t tid);

bool
thread_in_sigtimedwait(thread_id_t tid, int suspend_sig, bool *is_in_set);

bool
os_ptrace_takeover_threads(dcontext_t *dcontext, thread_id_t *tids, uint count);

void
ptrace_takeover_cleanup(void *param);

void
ptrace_attach_on_thread_takeover(dcontext_t *dcontext, void *param);
#endif

#endif /* _PTRACE_PRIVATE_H_ */
