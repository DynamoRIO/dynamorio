/* ******************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************/

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

#ifndef _JITOPT_H_
#define _JITOPT_H_ 1

#include "monitor.h"

void
jitopt_init();

void
jitopt_exit();

void
annotation_manage_code_area(app_pc start, size_t len);

void
annotation_unmanage_code_area(app_pc start, size_t len);

void
annotation_flush_fragments(app_pc start, size_t len);

#ifdef JITOPT
app_pc
instrument_writer(dcontext_t *dcontext, priv_mcontext_t *mc, fragment_t *f, app_pc instr_app_pc,
                  app_pc write_target, size_t write_size, uint prot, bool is_jit_self_write);

bool
apply_dgc_plan(app_pc pc);

void
add_patchable_bb(app_pc start, app_pc end, bool is_trace_head);

bool
add_patchable_trace(monitor_data_t *md);

void
patchable_bb_linked(dcontext_t *dcontext, fragment_t *f);

uint
remove_patchable_fragments(dcontext_t *dcontext, app_pc patch_start, app_pc patch_end);

void
dgc_table_dereference_bb(app_pc tag);

void
dgc_notify_region_cleared(app_pc start, app_pc end);

void
dgc_cache_reset();

#endif /* JIJTOPT */

#endif
