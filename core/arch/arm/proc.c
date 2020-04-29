/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * proc.c - processor-specific routines
 */

#include "../globals.h"
#include "proc.h"
#include "instr.h"
#ifdef UNIX
#    include "../../unix/include/syscall.h"
#else
#    error NYI
#endif

static int num_simd_saved;
static int num_simd_registers;
static int num_opmask_registers;

/* arch specific proc info */
void
proc_init_arch(void)
{
    num_simd_saved = MCXT_NUM_SIMD_SLOTS;
    num_simd_registers = MCXT_NUM_SIMD_SLOTS;
    num_opmask_registers = MCXT_NUM_OPMASK_SLOTS;

    /* FIXME i#1551: NYI on ARM */
    /* all of the CPUID registers are only accessible in privileged modes
     * so we either need read /proc/cpuinfo or auxiliary vector provided by
     * the Linux kernel.
     */
}

bool
proc_has_feature(feature_bit_t f)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache)
{
    if (flush_icache) {
        /* The instructions to flush the icache are privileged so we have to
         * make a syscall.
         * Note that gcc's __clear_cache just calls this syscall (and requires
         * library support that we don't build with).
         */
        dynamorio_syscall(SYS_cacheflush, 3, pc_start, pc_end, 0 /*flags: must be 0*/);
    }
}

DR_API
size_t
proc_fpstate_save_size(void)
{
    /* VFPv1: obsolete.
     * VFPv2: 32 single-precision registers s0 to s31.
     * VFPv3: adding 16 double-precision registers, d16 to d31.
     */
    return DR_FPSTATE_BUF_SIZE;
}

DR_API
int
proc_num_simd_saved(void)
{
    return num_simd_saved;
}

void
proc_set_num_simd_saved(int num)
{
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    ATOMIC_4BYTE_WRITE(&num_simd_saved, num, false);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

DR_API
int
proc_num_simd_registers(void)
{
    return num_simd_registers;
}

DR_API
int
proc_num_opmask_registers(void)
{
    return num_opmask_registers;
}

int
proc_num_simd_sse_avx_registers(void)
{
    CLIENT_ASSERT(false, "Incorrect usage for ARM/AArch64.");
    return 0;
}

int
proc_num_simd_sse_avx_saved(void)
{
    CLIENT_ASSERT(false, "Incorrect usage for ARM/AArch64.");
    return 0;
}

int
proc_xstate_area_kmask_offs(void)
{
    /* Does no apply to ARM. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_zmm_hi256_offs(void)
{
    /* Does no apply to ARM. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_hi16_zmm_offs(void)
{
    /* Does no apply to ARM. */
    ASSERT_NOT_REACHED();
    return 0;
}

DR_API
size_t
proc_save_fpstate(byte *buf)
{
    /* All registers are saved by insert_push_all_registers so nothing extra
     * needs to be saved here.
     */
    return DR_FPSTATE_BUF_SIZE;
}

DR_API
void
proc_restore_fpstate(byte *buf)
{
    /* Nothing to restore. */
}

void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
proc_avx_enabled(void)
{
    return false;
}

uint64
proc_get_timestamp(void)
{
    /* XXX i#1581: There is no simple equivalent to x86's rdtsc on ARM.
     * There is the Cycle CouNT (CNT) register, but it requires kernel support
     * to make it accessible from user mode, and it can be reset, resulting in
     * potential conflicts with the app.  Others seem to map /dev/mem and figure
     * out where the hardware counter is but this does not seem portable w/o
     * a kernel driver.
     * For now we punt on having kstats on by default and live with an expensive
     * system call.
     */
    return query_time_micros();
}
