/* **********************************************************
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "proc.h"
#include "instr.h"

static int num_simd_saved;
static int num_simd_registers;
static int num_opmask_registers;

void
proc_init_arch(void)
{
    num_simd_saved = MCXT_NUM_SIMD_SLOTS;
    num_simd_registers = MCXT_NUM_SIMD_SLOTS;
    num_opmask_registers = MCXT_NUM_OPMASK_SLOTS;

    /* FIXME i#1569: NYI */
}

bool
proc_has_feature(feature_bit_t f)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache)
{
    clear_icache(pc_start, pc_end);
}

DR_API
size_t
proc_fpstate_save_size(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
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
    /* Does not apply to AArch64. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_zmm_hi256_offs(void)
{
    /* Does not apply to AArch64. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_hi16_zmm_offs(void)
{
    /* Does not apply to AArch64. */
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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

uint64
proc_get_timestamp(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}
