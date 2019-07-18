/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

#define CHECK(x, msg)                                                                \
    do {                                                                             \
        if (!(x)) {                                                                  \
            dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
            dr_abort();                                                              \
        }                                                                            \
    } while (0);

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
                byte buf[64];
                memset(buf, 0, sizeof(buf));
                dr_fprintf(STDERR, "Clobbering all zmm registers\n");
                __asm__ __volatile__("vmovups %0, %%zmm0" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm1" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm2" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm3" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm4" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm5" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm6" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm7" : : "m"(buf));
#ifdef X64
                __asm__ __volatile__("vmovups %0, %%zmm8" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm9" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm10" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm11" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm12" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm13" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm14" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm15" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm16" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm17" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm18" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm19" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm20" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm21" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm22" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm23" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm24" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm25" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm26" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm27" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm28" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm29" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm30" : : "m"(buf));
                __asm__ __volatile__("vmovups %0, %%zmm31" : : "m"(buf));
#endif
                dr_fprintf(STDERR, "Clobbering all mask registers\n");
                __asm__ __volatile__("kmovw %0, %%k0" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k1" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k2" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k3" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k4" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k5" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k6" : : "m"(buf));
                __asm__ __volatile__("kmovw %0, %%k7" : : "m"(buf));
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
