/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
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
#ifdef UNIX
#    include "../../unix/include/syscall.h"
#else
#    error NYI
#endif

/* From Linux kernel, it's the only option available. */
#define SYS_RISCV_FLUSH_ICACHE_LOCAL 1

static int num_simd_saved;
static int num_simd_registers;
static int num_opmask_registers;

static size_t
read_cache_line(const char *fname)
{
    char b[sizeof(uint64)], tmp;
    size_t res = 0;
    ssize_t n, i;
    file_t f;

    if (os_file_exists(fname, false)) {
        f = os_open(fname, OS_OPEN_READ);
        if (f != INVALID_FILE) {
            n = os_read(f, b, sizeof(b));
            if (n > 0) {
                /* byte-swap */
                for (i = 0; i < n / 2; i++) {
                    tmp = b[n - i - 1];
                    b[n - i - 1] = b[i];
                    b[i] = tmp;
                }
                res = *(uint64 *)b;
            }
        }
    }
    return res;
}

/* Obtains dcache and icache line size and sets the values at the given pointers.
 * Returns false if no value was written.
 *
 * Note:
 * - Since no CSR holds those values, sysfs+device-tree is used and this works
 *   only on Linux.
 * - This code assumes that all harts have the same L1 cache-line size.
 */
static bool
get_cache_line_size(OUT size_t *dcache_line_size, OUT size_t *icache_line_size)
{
#if !defined(DR_HOST_NOT_TARGET) && defined(LINUX)
    static const char *d_cache_fname =
        "/sys/devices/system/cpu/cpu0/of_node/d-cache-block-size";
    static const char *i_cache_fname =
        "/sys/devices/system/cpu/cpu0/of_node/i-cache-block-size";
    static size_t dcache_line = 0;
    static size_t icache_line = 0;
    bool result = true;
    size_t sz;

    if (dcache_line_size) {
        if (dcache_line == 0) {
            sz = read_cache_line(d_cache_fname);
            if (sz > 0)
                dcache_line = sz;
        }
        if (dcache_line != 0)
            *dcache_line_size = dcache_line;
        else
            result = false;
    }

    if (icache_line_size) {
        if (icache_line == 0) {
            sz = read_cache_line(i_cache_fname);
            if (sz > 0)
                icache_line = sz;
        }
        if (icache_line != 0)
            *icache_line_size = icache_line;
        else
            result = false;
    }

    return result;
#endif
    return false;
}

void
proc_init_arch(void)
{
    num_simd_saved = MCXT_NUM_SIMD_SLOTS;
    num_simd_registers = MCXT_NUM_SIMD_SLOTS;
    num_opmask_registers = MCXT_NUM_OPMASK_SLOTS;

    /* When DR_HOST_NOT_TARGET, get_cache_line_size returns false and does
     * not set any value in given args.
     */
    if (!get_cache_line_size(&cache_line_size,
                             /* icache_line_size= */ NULL)) {
        LOG(GLOBAL, LOG_TOP, 1, "Unable to obtain cache line size");
    }
}

bool
proc_has_feature(feature_bit_t f)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
machine_cache_sync(void *pc_start, void *pc_end, bool flush_icache)
{
    /* We need to flush the icache on all harts, which is not feasible for FENCE.I, so we
     * use SYS_riscv_flush_icache to let the kernel do this.
     */
    dynamorio_syscall(SYS_riscv_flush_icache, 3, pc_start, pc_end,
                      SYS_RISCV_FLUSH_ICACHE_LOCAL);
}

DR_API
size_t
proc_fpstate_save_size(void)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
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
    CLIENT_ASSERT(false, "Incorrect usage for RISC-V.");
    return 0;
}

int
proc_num_simd_sse_avx_saved(void)
{
    CLIENT_ASSERT(false, "Incorrect usage for RISC-V.");
    return 0;
}

int
proc_xstate_area_kmask_offs(void)
{
    /* Does not apply to RISC-V. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_zmm_hi256_offs(void)
{
    /* Does not apply to RISC-V. */
    ASSERT_NOT_REACHED();
    return 0;
}

int
proc_xstate_area_hi16_zmm_offs(void)
{
    /* Does not apply to RISC-V. */
    ASSERT_NOT_REACHED();
    return 0;
}

DR_API
size_t
proc_save_fpstate(byte *buf)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return DR_FPSTATE_BUF_SIZE;
}

DR_API
void
proc_restore_fpstate(byte *buf)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where, opnd_t buf)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

uint64
proc_get_timestamp(void)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}
