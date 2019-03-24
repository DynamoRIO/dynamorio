/* **********************************************************
 * Copyright (c) 2017-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2004-2007 Determina Corp. */

/*
 * kstatsx.h
 *
 * Timing statistics definitions and descriptions
 *
 */

/* This file is included multiple times
   - in stats.h once for structure definition,
   - in stats.c
*/

/* Client files that include this header should define the following macros
#define KSTAT_DEF(description, name)
#define KSTAT_SUM(description, name, var1, var2) name = var1 + var2
*/

/* Keep descriptions and names up to a reasonable length -
 * respectively 50 and 15 characters */

KSTAT_DEF("total measured and propagated in thread", thread_measured)

KSTAT_DEF("in bb building", bb_building)
KSTAT_DEF("in bb decoding", bb_decoding) /* sub-node of bb_building */
KSTAT_DEF("in emitting BB", bb_emit)     /* sub-node of bb_building */
KSTAT_DEF("in mangling", mangling)
KSTAT_DEF("in emit", emit)
KSTAT_DEF("in hotpatch lookup", hotp_lookup)
KSTAT_DEF("in trace building", trace_building)
KSTAT_DEF("making temp private bb for trace building", temp_private_bb)
KSTAT_DEF("in trace monitor ", monitor_enter)
KSTAT_DEF("in trace monitor, thci ", monitor_enter_thci)
KSTAT_DEF("cache flush unit walk ", cache_flush_unit_walk)
KSTAT_DEF("flush_region", flush_region)
KSTAT_DEF("synchall flush ", synchall_flush)
KSTAT_DEF("coarse pclookup", coarse_pclookup)
KSTAT_DEF("coarse freeze all", coarse_freeze_all)
KSTAT_DEF("persisted cache generation", persisted_generation)
KSTAT_DEF("persisted cache load", persisted_load)

KSTAT_DEF("in dispatch exit, default", dispatch_num_exits)
/* preserving STATS names for num_exits_* responsible for time in DR */
KSTAT_DEF("in dispatch exit, ind target not in cache", num_exits_ind_good_miss)
KSTAT_DEF("in dispatch exit, dir target not in cache", num_exits_dir_miss)
KSTAT_SUM("in dispatch exit, all target not in cache", num_exits_not_in_cache,
          num_exits_ind_good_miss, num_exits_dir_miss)

KSTAT_DEF("in dispatch exit, BB2BB, ind target ...", num_exits_ind_bad_miss_bb2bb)
KSTAT_DEF("in dispatch exit, BB2trace, ind target ...", num_exits_ind_bad_miss_bb2trace)
KSTAT_SUM("in dispatch exit, from BB", num_exits_ind_bad_miss_bb,
          num_exits_ind_bad_miss_bb2bb, num_exits_ind_bad_miss_bb2trace)

KSTAT_DEF("in dispatch exit, trace2trace, ind target ...",
          num_exits_ind_bad_miss_trace2trace)

KSTAT_DEF("in dispatch exit, trace2BB not trace head, ind target",
          num_exits_ind_bad_miss_trace2bb_nth)
KSTAT_DEF("in dispatch exit, trace2BB trace head, ind target",
          num_exits_ind_bad_miss_trace2bb_th)
KSTAT_SUM("in dispatch exit, trace2BB, ind target ", num_exits_ind_bad_miss_trace2bb,
          num_exits_ind_bad_miss_trace2bb_nth, num_exits_ind_bad_miss_trace2bb_th)

KSTAT_SUM("in dispatch exit, from trace", num_exits_ind_bad_miss_trace,
          num_exits_ind_bad_miss_trace2trace, num_exits_ind_bad_miss_trace2bb)

KSTAT_SUM("in dispatch exit, ind target in cache but not table", num_exits_ind_bad_miss,
          num_exits_ind_bad_miss_trace, num_exits_ind_bad_miss_bb)
KSTAT_DEF("in dispatch exit, syscall handling", num_exits_dir_syscall)
KSTAT_DEF("in dispatch exit, callback return", num_exits_dir_cbret)

KSTAT_DEF("in LOG", logging)

KSTAT_DEF("empty block overhead", overhead_empty)
KSTAT_DEF("nested block overhead", overhead_nested)

KSTAT_DEF("in syscalls [not propagated]", syscall_fcache)
KSTAT_DEF("pre-syscall handling", pre_syscall)
KSTAT_DEF("post-syscall handling", post_syscall)
KSTAT_DEF("pre-syscall FreeVM handling", pre_syscall_free)
KSTAT_DEF("pre-syscall ProtectVM handling", pre_syscall_protect)
KSTAT_DEF("pre-syscall Unmap handling", pre_syscall_unmap)
KSTAT_DEF("post-syscall AllocVM handling", post_syscall_alloc)
KSTAT_DEF("post-syscall Map handling", post_syscall_map)

KSTAT_DEF("native_exec [not propagated]", native_exec_fcache)

KSTAT_DEF("in fcache, default", fcache_default)
KSTAT_DEF("in bb cache, [not propagated]", fcache_bb_bb)
KSTAT_DEF("in trace cache, [not propagated]", fcache_trace_trace)
/* hard to SUM it up against either bb or trace only */
KSTAT_DEF("in bb cache out from trace cache, [not propagated]", fcache_bb_trace)

/* assuming we'll deal with lock contention separately we don't
 * propagate this time to callers
 */
KSTAT_DEF("wait event (+context switch) [not propagated]", wait_event)

/* FIXME: we should add all critical section bodies as suggested in
 * the mutex_t definition - if only we can share LOCK_RANK definitions
 * in lockx.h and it will just work
 */

KSTAT_DEF("in rct analysis no relocations", rct_no_reloc)
KSTAT_DEF("in rct analysis using relocations [outer loop]", rct_reloc)
KSTAT_DEF("in rct analysis using relocations [per page loop]", rct_reloc_per_page)
KSTAT_DEF("in aslr_generate_relocated_section for validation", aslr_validate_relocate)
KSTAT_DEF("in module_contents_compare or aslr_compare_in_place", aslr_compare)

#ifdef KSTAT_UNIT_TEST
KSTAT_DEF("empty block overhead", empty)
KSTAT_DEF("total measured", measured)
KSTAT_DEF("in outer loop", iloop)
KSTAT_DEF("in inner loop", jloop)
#endif /* KSTAT_UNIT_TEST */
