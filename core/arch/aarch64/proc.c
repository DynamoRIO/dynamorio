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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
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
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

DR_API
void
proc_restore_fpstate(byte *buf)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
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
