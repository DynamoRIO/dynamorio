/* **********************************************************
 * Copyright (c) 2019-2022 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "client_tools.h"
#include <string.h>

#ifdef CLIENT_COMPILED_WITH_AVX512
/* This just double-checks the implicit __AVX512__ flag if the client was compiled
 * with AVX-512.
 */
#    ifndef __AVX512F__
#        error "Compiler provided __AVX512F__ define not set"
#    endif
#endif

/* This library assumes a single-threaded test. */
static bool seen_before;

static dr_emit_flags_t
bb_event(void *drcontext, void *tag, instrlist_t *bb, bool for_trace, bool translating)
{
    if (translating || for_trace)
        return DR_EMIT_DEFAULT;
    instr_t *instr;
    bool prev_was_mov_const = false;
    ptr_int_t val1, val2;
    for (instr = instrlist_first(bb); instr != NULL; instr = instr_get_next(instr)) {
        if (instr_is_mov_constant(instr, prev_was_mov_const ? &val2 : &val1)) {
            if (prev_was_mov_const && val1 == val2 &&
                val1 != 0 && /* rule out xor w/ self */
                opnd_is_reg(instr_get_dst(instr, 0)) &&
                opnd_get_reg(instr_get_dst(instr, 0)) == REG_XAX) {
                if (seen_before) {
                    CHECK(dr_mcontext_zmm_fields_valid(),
                          "Error: dr_mcontext_zmm_fields_valid() should return true.");
                    dr_fprintf(STDERR, "After\n");
                } else {
                    /* The *-initial version of this test runs the client compiled with
                     * AVX-512. Even in this case, the initial value of
                     * dr_mcontext_zmm_fields_valid() is expected to be false. The only
                     * time it should be true before application AVX-512 code has actually
                     * been seen is the "attach" case. This is tested by
                     * api.startstop_avx512lazy.
                     */
                    CHECK(!dr_mcontext_zmm_fields_valid(),
                          "Error: dr_mcontext_zmm_fields_valid() should return false.");
                    dr_fprintf(STDERR, "Before\n");
                }
                seen_before = true;
            } else
                prev_was_mov_const = true;
        } else
            prev_was_mov_const = false;
    }
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_register_bb_event(bb_event);
}
