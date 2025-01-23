/* **********************************************************
 * Copyright (c) 2025 ARM Limited. All rights reserved.
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
#include "../../unix/module_private.h"
#include "mangle.h"
#include <stddef.h> /* for offsetof */
#include "asm_offsets.h"

namespace dynamorio {

/* Check that macros defined in asm_offsets.h are correct. */

#define CHECK(x) static_assert(x, "macro in asm_offsets.h defined incorrectly")

CHECK(dcontext_t_OFFSET_dstack == offsetof(dcontext_t, dstack));
CHECK(dcontext_t_OFFSET_is_exiting == offsetof(dcontext_t, is_exiting));

CHECK(icache_op_struct_t_OFFSET_flag == offsetof(icache_op_struct_t, flag));
CHECK(icache_op_struct_t_OFFSET_lock == offsetof(icache_op_struct_t, lock));
CHECK(icache_op_struct_t_OFFSET_linesize == offsetof(icache_op_struct_t, linesize));
CHECK(icache_op_struct_t_OFFSET_begin == offsetof(icache_op_struct_t, begin));
CHECK(icache_op_struct_t_OFFSET_end == offsetof(icache_op_struct_t, end));
CHECK(icache_op_struct_t_OFFSET_spill == offsetof(icache_op_struct_t, spill));

CHECK(priv_mcontext_t_OFFSET_simd == offsetof(priv_mcontext_t, simd));
CHECK(priv_mcontext_t_SIZE == sizeof(priv_mcontext_t));

CHECK(spill_state_t_OFFSET_r0 == offsetof(spill_state_t, r0));
CHECK(spill_state_t_OFFSET_r1 == offsetof(spill_state_t, r1));
CHECK(spill_state_t_OFFSET_r2 == offsetof(spill_state_t, r2));
CHECK(spill_state_t_OFFSET_r3 == offsetof(spill_state_t, r3));
CHECK(spill_state_t_OFFSET_r4 == offsetof(spill_state_t, r4));
CHECK(spill_state_t_OFFSET_r5 == offsetof(spill_state_t, r5));
CHECK(spill_state_t_OFFSET_fcache_return == offsetof(spill_state_t, fcache_return));

CHECK(struct_tlsdesc_t_OFFSET_arg == offsetof(struct tlsdesc_t, arg));

} // namespace dynamorio
