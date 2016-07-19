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

void
proc_init_arch(void)
{
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
    cache_sync_asm(pc_start, pc_end);
}

DR_API
size_t
proc_fpstate_save_size(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

DR_API
size_t
proc_save_fpstate(byte *buf)
{
    __asm__ __volatile__
        ("st1 {v0.2d-v3.2d}, [%0], #64\n\t"
         "st1 {v4.2d-v7.2d}, [%0], #64\n\t"
         "st1 {v8.2d-v11.2d}, [%0], #64\n\t"
         "st1 {v12.2d-v15.2d}, [%0], #64\n\t"
         "st1 {v16.2d-v19.2d}, [%0], #64\n\t"
         "st1 {v20.2d-v23.2d}, [%0], #64\n\t"
         "st1 {v24.2d-v27.2d}, [%0], #64\n\t"
         "st1 {v28.2d-v31.2d}, [%0], #64\n\t"
         : "=r"(buf) : "r"(buf) : "memory");
    return DR_FPSTATE_BUF_SIZE;
}

DR_API
void
proc_restore_fpstate(byte *buf)
{
    __asm__ __volatile__
        ("ld1 {v0.2d-v3.2d}, [%0], #64\n\t"
         "ld1 {v4.2d-v7.2d}, [%0], #64\n\t"
         "ld1 {v8.2d-v11.2d}, [%0], #64\n\t"
         "ld1 {v12.2d-v15.2d}, [%0], #64\n\t"
         "ld1 {v16.2d-v19.2d}, [%0], #64\n\t"
         "ld1 {v20.2d-v23.2d}, [%0], #64\n\t"
         "ld1 {v24.2d-v27.2d}, [%0], #64\n\t"
         "ld1 {v28.2d-v31.2d}, [%0], #64\n\t"
         : "=r"(buf) : "r"(buf) : "memory");
}

void
dr_insert_save_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                       opnd_t buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
dr_insert_restore_fpstate(void *drcontext, instrlist_t *ilist, instr_t *where,
                          opnd_t buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

uint64
proc_get_timestamp(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}
