/* **********************************************************
 * Copyright (c) 2016-2023 Google, Inc.  All rights reserved.
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

#include "instru.h"

#include <stddef.h>

#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drutil.h"
#include "trace_entry.h"

#ifdef LINUX
#    ifndef _GNU_SOURCE
#        define _GNU_SOURCE // For syscall()
#    endif
#    include <unistd.h>
#    include <sys/syscall.h>
#endif
#ifdef WINDOWS
#    include <intrin.h>
#endif

namespace dynamorio {
namespace drmemtrace {

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
    // extra instru to distinguish the 1st and subsequent iters).
    if (instr_is_rep_string_op(instr) || (repstr_expanded && instr_is_string_op(instr)))
        return TRACE_TYPE_INSTR_MAYBE_FETCH;
    return TRACE_TYPE_INSTR;
}

#ifdef AARCH64
// XXX: this decoding logic may be moved to DR's IR for reuse.
unsigned short
instru_t::get_aarch64_prefetch_target_policy(ptr_int_t prfop)
{
    // Least significant three bits of prfop represent prefetch target and
    // policy [... b2 b1 b0]
    //
    // b2 b1 : prefetch target
    // 0b00 - L1 cache, 0b01 - L2 cache, 0b10 - L3 cache
    //
    // b0 : prefetch policy
    // 0b0 - temporal prefetch, 0b1 - streaming (non-temporal) prefetch
    return prfop & 0b111;
}

unsigned short
instru_t::get_aarch64_prefetch_op_type(ptr_int_t prfop)
{
    // Bits at position 3 and 4 in prfop represent prefetch operation type
    // [... b4 b3 ...]
    // 0b00 - prefetch for load
    // 0b01 - prefetch for instruction
    // 0b10 - prefetch for store
    return (prfop >> 3) & 0b11;
}

unsigned short
instru_t::get_aarch64_prefetch_type(ptr_int_t prfop)
{
    unsigned short target_policy = get_aarch64_prefetch_target_policy(prfop);
    unsigned short op_type = get_aarch64_prefetch_op_type(prfop);
    switch (op_type) {
    case 0b00: // prefetch for load
        switch (target_policy) {
        case 0b000: return TRACE_TYPE_PREFETCH_READ_L1;
        case 0b001: return TRACE_TYPE_PREFETCH_READ_L1_NT;
        case 0b010: return TRACE_TYPE_PREFETCH_READ_L2;
        case 0b011: return TRACE_TYPE_PREFETCH_READ_L2_NT;
        case 0b100: return TRACE_TYPE_PREFETCH_READ_L3;
        case 0b101: return TRACE_TYPE_PREFETCH_READ_L3_NT;
        }
        break;
    case 0b01: // prefetch for instruction
        switch (target_policy) {
        case 0b000: return TRACE_TYPE_PREFETCH_INSTR_L1;
        case 0b001: return TRACE_TYPE_PREFETCH_INSTR_L1_NT;
        case 0b010: return TRACE_TYPE_PREFETCH_INSTR_L2;
        case 0b011: return TRACE_TYPE_PREFETCH_INSTR_L2_NT;
        case 0b100: return TRACE_TYPE_PREFETCH_INSTR_L3;
        case 0b101: return TRACE_TYPE_PREFETCH_INSTR_L3_NT;
        }
        break;
    case 0b10: // prefetch for store
        switch (target_policy) {
        case 0b000: return TRACE_TYPE_PREFETCH_WRITE_L1;
        case 0b001: return TRACE_TYPE_PREFETCH_WRITE_L1_NT;
        case 0b010: return TRACE_TYPE_PREFETCH_WRITE_L2;
        case 0b011: return TRACE_TYPE_PREFETCH_WRITE_L2_NT;
        case 0b100: return TRACE_TYPE_PREFETCH_WRITE_L3;
        case 0b101: return TRACE_TYPE_PREFETCH_WRITE_L3_NT;
        }
        break;
    }

#    ifdef DEBUG
    // Some AArch64 prefetch operation encodings are not accessible using prfop.
    // For debug builds, we throw an error if we encounter any such prefetch operation.
    // For release builds, we map them to the default TRACE_TYPE_PREFETCH.
    DR_ASSERT_MSG(false, "Unsupported AArch64 prefetch operation.");
#    endif

    return TRACE_TYPE_PREFETCH;
}
#endif

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
#ifdef AARCH64
    case OP_prfm:
        return get_aarch64_prefetch_type(opnd_get_immed_int(instr_get_src(instr, 0)));
    case OP_prfum:
        return get_aarch64_prefetch_type(opnd_get_immed_int(instr_get_src(instr, 0)));
#endif
    default: return TRACE_TYPE_PREFETCH;
    }
}

#ifdef AARCH64
bool
instru_t::is_aarch64_icache_flush_op(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    // TODO i#4406: Handle privileged icache operations.
    case OP_ic_ivau: return true;
    }
    return false;
}

bool
instru_t::is_aarch64_dcache_flush_op(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    // TODO i#4406: Handle all privileged dcache operations.
    case OP_dc_ivac:
    case OP_dc_cvau:
    case OP_dc_civac:
    case OP_dc_cvac: return true;
    }
    return false;
}

bool
instru_t::is_aarch64_dc_zva_instr(instr_t *instr)
{
    return instr_get_opcode(instr) == OP_dc_zva;
}
#endif

bool
instru_t::instr_is_flush(instr_t *instr)
{
    // Assuming we won't see any privileged instructions.
#ifdef X86
    if (instr_get_opcode(instr) == OP_clflush)
        return true;
#endif
#ifdef AARCH64
    if (is_aarch64_dcache_flush_op(instr) || is_aarch64_icache_flush_op(instr))
        return true;
#endif
    return false;
}

unsigned short
instru_t::instr_to_flush_type(instr_t *instr)
{
    DR_ASSERT(instr_is_flush(instr));
#ifdef X86
    // XXX: OP_clflush invalidates all levels of the processor cache
    // hierarchy (data and instruction)
    if (instr_get_opcode(instr) == OP_clflush)
        return TRACE_TYPE_DATA_FLUSH;
#endif
#ifdef AARCH64
    if (is_aarch64_icache_flush_op(instr))
        return TRACE_TYPE_INSTR_FLUSH;
    if (is_aarch64_dcache_flush_op(instr))
        return TRACE_TYPE_DATA_FLUSH;
#endif
    DR_ASSERT(false);
    return TRACE_TYPE_DATA_FLUSH; // unreachable.
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
    if (!ok) {
        // Provide diagnostics to make it much easier to see what the problematic
        // operand is.
        // XXX: Should we honor user's op_verbose or anything?  We have no current
        // precedent for printing from instru_t.
        dr_fprintf(STDERR, "FATAL: %s: drutil_insert_get_mem_addr failed @ " PFX ": ",
                   __FUNCTION__, instr_get_app_pc(where));
        instr_disassemble(drcontext, where, STDERR);
        dr_fprintf(STDERR, "\n");
        DR_ASSERT(ok);
    }
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
#elif defined(MACOS)
    /* TODO i#5383: Add an M1 solution. */
    DR_ASSERT_MSG(false, "Not implemented for M1");
    return -1;
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

int
instru_t::count_app_instrs(instrlist_t *ilist)
{
    int count = 0;
    bool in_emulation_region = false;
    for (instr_t *inst = instrlist_first(ilist); inst != NULL;
         inst = instr_get_next(inst)) {
        if (!in_emulation_region && drmgr_is_emulation_start(inst)) {
            in_emulation_region = true;
            // Each emulation region corresponds to a single app instr.
            ++count;
        }
        if (!in_emulation_region && instr_is_app(inst)) {
            // Hooked native functions end up with an artificial jump whose translation
            // is its target.  We do not want to count these.
            if (!(instr_is_ubr(inst) && opnd_is_pc(instr_get_target(inst)) &&
                  opnd_get_pc(instr_get_target(inst)) == instr_get_app_pc(inst)))
                ++count;
        }
        if (in_emulation_region && drmgr_is_emulation_end(inst))
            in_emulation_region = false;
    }
    return count;
}

} // namespace drmemtrace
} // namespace dynamorio
