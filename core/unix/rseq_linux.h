/* *******************************************************************************
 * Copyright (c) 2019-2023 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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
 * rseq_linux.h - Linux restartable sequence ("rseq") support exposed to unix/ only.
 */

#ifndef _RSEQ_H_
#define _RSEQ_H_ 1

#include "../globals.h"
#include "../module_shared.h"
#include "module_private.h"

#ifndef LINUX
#    error Rseq is a Linux-only feature
#endif

/* Routines exported outside of unix/ are in os_exports.h. */

void
d_r_rseq_init(void);

void
d_r_rseq_exit(void);

void
rseq_thread_attach(dcontext_t *dcontext);

bool
rseq_is_registered_for_current_thread(void);

void
rseq_locate_rseq_regions(bool saw_glibc_rseq_reg);

void
rseq_module_init(module_area_t *ma, bool at_map);

void
rseq_process_syscall(dcontext_t *dcontext);

#endif /* _RSEQ_H_ */
