/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

/*
 * instrument.h - interface for instrumentation
 */

#ifndef _INSTRUMENT_H_
#define _INSTRUMENT_H_ 1

#include "../globals.h"
#include "../module_shared.h"
#include "arch.h"
#include "instr.h"
#include "dr_config.h"

/* The core of the public API */
#include "dr_events.h"
#include "dr_tools.h"
#include "dr_ir_utils.h"

/* Xref _USES_DR_VERSION_ in dr_api.h (PR 250952) and compatibility
 * check in instrument.c (OLDEST_COMPATIBLE_VERSION, etc.).
 */
#define CURRENT_API_VERSION VERSION_NUMBER_INTEGER

/* to make our own code shorter */
#define MINSERT instrlist_meta_preinsert

void
instrument_load_client_libs(void);
void
instrument_init(void);
void
instrument_exit_event(void);
void
instrument_post_attach_event(void);
void
instrument_pre_detach_event(void);
void
instrument_exit(void);
bool
is_in_client_lib(app_pc addr);
/* Does not consider auxiliary client libraries, avoiding a lock acquisition along
 * the way (making this more suitable for diagnostics in sensitive locations at
 * the downside of missing aux libs).
 */
bool
is_in_client_lib_ignore_aux(app_pc addr);
bool
get_client_bounds(client_id_t client_id, app_pc *start /*OUT*/, app_pc *end /*OUT*/);
const char *
get_client_path_from_addr(app_pc addr);
bool
is_valid_client_id(client_id_t id);
void
instrument_client_thread_init(dcontext_t *dcontext, bool client_thread);
void
instrument_thread_init(dcontext_t *dcontext, bool client_thread, bool valid_mc);
void
instrument_thread_exit_event(dcontext_t *dcontext);
void
instrument_thread_exit(dcontext_t *dcontext);
#ifdef UNIX
void
instrument_fork_init(dcontext_t *dcontext);
#endif
bool
instrument_basic_block(dcontext_t *dcontext, app_pc tag, instrlist_t *bb, bool for_trace,
                       bool translating, dr_emit_flags_t *emitflags);
dr_emit_flags_t
instrument_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace, bool translating);
dr_custom_trace_action_t
instrument_end_trace(dcontext_t *dcontext, app_pc trace_tag, app_pc next_tag);
void
instrument_fragment_deleted(dcontext_t *dcontext, app_pc tag, uint flags);
bool
instrument_restore_state(dcontext_t *dcontext, bool restore_memory,
                         dr_restore_state_info_t *info);
bool
instrument_restore_nonfcache_state(dcontext_t *dcontext, bool restore_memory,
                                   INOUT priv_mcontext_t *mcontext);
bool
instrument_restore_nonfcache_state_prealloc(dcontext_t *dcontext, bool restore_memory,
                                            INOUT priv_mcontext_t *mcontext,
                                            OUT dr_mcontext_t *client_mcontext);

module_data_t *
copy_module_area_to_module_data(const module_area_t *area);
void
instrument_module_load_trigger(app_pc pc);
void
instrument_module_load(module_data_t *data, bool previously_loaded);
void
instrument_module_unload(module_data_t *data);

/* returns whether this sysnum should be intercepted */
bool
instrument_filter_syscall(dcontext_t *dcontext, int sysnum);
/* returns whether this syscall should execute */
bool
instrument_pre_syscall(dcontext_t *dcontext, int sysnum);
void
instrument_post_syscall(dcontext_t *dcontext, int sysnum);
bool
instrument_invoke_another_syscall(dcontext_t *dcontext);
void
instrument_low_on_memory();
/* returns whether a client event was called which might have changed the context */
bool
instrument_kernel_xfer(dcontext_t *dcontext, dr_kernel_xfer_type_t type,
                       /* only one of these 3 should be non-NULL */
                       os_cxt_ptr_t source_os_cxt, dr_mcontext_t *source_dmc,
                       priv_mcontext_t *source_mc, app_pc target_pc, reg_t target_xsp,
                       /* only one of these 2 should be non-NULL */
                       os_cxt_ptr_t target_os_cxt, priv_mcontext_t *target_mc, int sig);

void
instrument_nudge(dcontext_t *dcontext, client_id_t id, uint64 arg);
#ifdef WINDOWS
bool
instrument_exception(dcontext_t *dcontext, dr_exception_t *exception);
void
wait_for_outstanding_nudges(void);
#else
dr_signal_action_t
instrument_signal(dcontext_t *dcontext, dr_siginfo_t *siginfo);
bool
dr_signal_hook_exists(void);
#endif
int
get_num_client_threads(void);
#ifdef PROGRAM_SHEPHERDING
void
instrument_security_violation(dcontext_t *dcontext, app_pc target_pc,
                              security_violation_t violation, action_type_t *action);
#endif

void
set_client_error_code(dcontext_t *dcontext, dr_error_code_t error_code);

bool
dr_get_mcontext_priv(dcontext_t *dcontext, dr_mcontext_t *dmc, priv_mcontext_t *mc);
bool
dr_modload_hook_exists(void);

void
instrument_client_lib_loaded(byte *start, byte *end);
void
instrument_client_lib_unloaded(byte *start, byte *end);

bool
dr_bb_hook_exists(void);
bool
dr_trace_hook_exists(void);
bool
dr_fragment_deleted_hook_exists(void);
bool
dr_end_trace_hook_exists(void);
bool
dr_thread_exit_hook_exists(void);
bool
dr_exit_hook_exists(void);
bool
dr_xl8_hook_exists(void);
bool
hide_tag_from_client(app_pc tag);
bool
should_track_where_am_i(void);

uint
instrument_persist_ro_size(dcontext_t *dcontext, void *perscxt, size_t file_offs);
bool
instrument_persist_ro(dcontext_t *dcontext, void *perscxt, file_t fd);
bool
instrument_resurrect_ro(dcontext_t *dcontext, void *perscxt, byte *map);
uint
instrument_persist_rx_size(dcontext_t *dcontext, void *perscxt, size_t file_offs);
bool
instrument_persist_rx(dcontext_t *dcontext, void *perscxt, file_t fd);
bool
instrument_resurrect_rx(dcontext_t *dcontext, void *perscxt, byte *map);
uint
instrument_persist_rw_size(dcontext_t *dcontext, void *perscxt, size_t file_offs);
bool
instrument_persist_rw(dcontext_t *dcontext, void *perscxt, file_t fd);
bool
instrument_resurrect_rw(dcontext_t *dcontext, void *perscxt, byte *map);
bool
instrument_persist_patch(dcontext_t *dcontext, void *perscxt, byte *bb_start,
                         size_t bb_size);

#endif /* _INSTRUMENT_H_ */
