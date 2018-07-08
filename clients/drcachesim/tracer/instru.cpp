/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

/* instru: instrumentation utilities.
 */

#include "dr_api.h"
#include "drreg.h"
#include "drutil.h"
#include "instru.h"
#include "../common/trace_entry.h"
#ifdef LINUX
#    include <sched.h>
#    ifndef _GNU_SOURCE
#        define _GNU_SOURCE // For syscall()
#    endif
#    include <unistd.h>
#    include <sys/syscall.h>
#endif
#ifdef WINDOWS
#    include <intrin.h>
#endif

unsigned short
instru_t::instr_to_instr_type(instr_t *instr, bool repstr_expanded)
{
    if (instr_is_call_direct(instr))
        return TRACE_TYPE_INSTR_DIRECT_CALL;
    if (instr_is_call_indirect(instr))
        return TRACE_TYPE_INSTR_INDIRECT_CALL;
    if (instr_is_return(instr))
        return TRACE_TYPE_INSTR_RETURN;
    if (instr_is_ubr(instr))
        return TRACE_TYPE_INSTR_DIRECT_JUMP;
    if (instr_is_mbr(instr)) // But not a return or call.
        return TRACE_TYPE_INSTR_INDIRECT_JUMP;
    if (instr_is_cbr(instr))
        return TRACE_TYPE_INSTR_CONDITIONAL_JUMP;
#ifdef X86
    if (instr_get_opcode(instr) == OP_sysenter)
        return TRACE_TYPE_INSTR_SYSENTER;
#endif
    // i#2051: to satisfy both cache and core simulators we mark subsequent iters
    // of string loops as TRACE_TYPE_INSTR_NO_FETCH, converted from this
    // TRACE_TYPE_INSTR_MAYBE_FETCH by reader_t (since online traces would need
    // extra insru to distinguish the 1st and subsequent iters).
    if (instr_is_rep_string_op(instr) || (repstr_expanded && instr_is_string_op(instr)))
        return TRACE_TYPE_INSTR_MAYBE_FETCH;
    return TRACE_TYPE_INSTR;
}

unsigned short
instru_t::instr_to_prefetch_type(instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    DR_ASSERT(instr_is_prefetch(instr));
    switch (opcode) {
#ifdef X86
    case OP_prefetcht0: return TRACE_TYPE_PREFETCHT0;
    case OP_prefetcht1: return TRACE_TYPE_PREFETCHT1;
    case OP_prefetcht2: return TRACE_TYPE_PREFETCHT2;
    case OP_prefetchnta: return TRACE_TYPE_PREFETCHNTA;
#endif
#ifdef ARM
    case OP_pld: return TRACE_TYPE_PREFETCH_READ;
    case OP_pldw: return TRACE_TYPE_PREFETCH_WRITE;
    case OP_pli: return TRACE_TYPE_PREFETCH_INSTR;
#endif
    default: return TRACE_TYPE_PREFETCH;
    }
}

bool
instru_t::instr_is_flush(instr_t *instr)
{
    // Assuming we won't see any privileged instructions.
#ifdef X86
    if (instr_get_opcode(instr) == OP_clflush)
        return true;
#endif
    return false;
}

void
instru_t::insert_obtain_addr(void *drcontext, instrlist_t *ilist, instr_t *where,
                             reg_id_t reg_addr, reg_id_t reg_scratch, opnd_t ref,
                             OUT bool *scratch_used)
{
    bool ok;
    bool we_used_scratch = false;
    if (opnd_uses_reg(ref, reg_scratch)) {
        drreg_get_app_value(drcontext, ilist, where, reg_scratch, reg_scratch);
        we_used_scratch = true;
    }
    if (opnd_uses_reg(ref, reg_addr))
        drreg_get_app_value(drcontext, ilist, where, reg_addr, reg_addr);
    ok = drutil_insert_get_mem_addr_ex(drcontext, ilist, where, ref, reg_addr,
                                       reg_scratch, scratch_used);
    DR_ASSERT(ok);
    if (scratch_used != NULL && we_used_scratch)
        *scratch_used = true;
}

// Returns -1 on error.  It's hard for callers to omit the cpu marker though
// because the tracer assumes unit headers are always the same size: thus
// we insert a marker with a -1 (=> unsigned) cpu id.
int
instru_t::get_cpu_id()
{
#ifdef LINUX
    // We'd like to use sched_getcpu() but it crashes on secondary threads: some
    // kind of TLS issue with the private libc's query of __vdso_getcpu.
    // We could directly find and use __vdso_getcpu ourselves (i#2842).
#endif
#ifdef X86
    if (proc_has_feature(FEATURE_RDTSCP)) {
#    ifdef WINDOWS
        uint cpu;
        __rdtscp(&cpu);
#    else
        int cpu;
        __asm__ __volatile__("rdtscp" : "=c"(cpu) : : "eax", "edx");
#    endif
        return cpu;
    } else {
        // We could get the processor serial # from cpuid but we just bail since
        // this should be pretty rare and we can live without it.
        return -1;
    }
#else
    uint cpu;
    if (syscall(SYS_getcpu, &cpu, NULL, NULL) < 0)
        return -1;
    return cpu;
#endif
}

uint64
instru_t::get_timestamp()
{
    // We use dr_get_microseconds() for a simple, cross-platform implementation.
    // We call this just once per buffer write, so a syscall here should be ok.
    // If we want something faster we can try to use the VDSO gettimeofday (via
    // libc) or KUSER_SHARED_DATA on Windows (i#2842).
    return dr_get_microseconds();
}
