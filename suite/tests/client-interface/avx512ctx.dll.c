/* **********************************************************
 * Copyright (c) 2019-2021 Google, Inc.  All rights reserved.
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
#include "avx512ctx-shared.h"
#include <string.h>

static void
print_zmm(byte zmm_buf[], byte zmm_ref[])
{
    int zmm_off = 0;
    for (int i = 0; i < NUM_SIMD_REGS; ++i, zmm_off += 64) {
        dr_fprintf(STDERR, "got zmm[%d]:\n", i);
        for (int b = zmm_off; b < zmm_off + 64; ++b)
            dr_fprintf(STDERR, "%x", zmm_buf[b]);
        dr_fprintf(STDERR, "\nref zmm[%d]:\n", i);
        for (int b = zmm_off; b < zmm_off + 64; ++b)
            dr_fprintf(STDERR, "%x", zmm_ref[b]);
        dr_fprintf(STDERR, "\n");
    }
}

static void
print_opmask(byte opmask_buf[], byte opmask_ref[])
{
    int opmask_off = 0;
    for (int i = 0; i < NUM_OPMASK_REGS; ++i, opmask_off += 8) {
        dr_fprintf(STDERR, "got k[%i]:\n", i);
        for (int b = opmask_off; b < opmask_off + 8; ++b)
            dr_fprintf(STDERR, "%x", opmask_buf[b]);
        dr_fprintf(STDERR, "\nref k[%d]:\n", i);
        for (int b = opmask_off; b < opmask_off + 8; ++b)
            dr_fprintf(STDERR, "%x", opmask_ref[b]);
        dr_fprintf(STDERR, "\n");
    }
}

static void
read_avx512_state()
{
    byte zmm_buf[NUM_SIMD_REGS * 64];
    byte zmm_ref[NUM_SIMD_REGS * 64];
    byte opmask_buf[NUM_OPMASK_REGS * 8];
    byte opmask_ref[NUM_OPMASK_REGS * 8];
    memset(zmm_buf, 0xde, sizeof(zmm_buf));
    memset(zmm_ref, 0xab, sizeof(zmm_ref));
    memset(opmask_buf, 0xde, sizeof(opmask_buf));
    memset(opmask_ref, 0, sizeof(opmask_ref));
    for (int i = 0; i < NUM_OPMASK_REGS; ++i) {
        /* See comment in applicaton part of the test: We limit the test
         * to 2 byte wide mask registers.
         */
        memset(&opmask_ref[i * 8], 0xabab, 2);
    }

    dr_fprintf(STDERR, "Reading application state\n");

    void *drcontext = dr_get_current_drcontext();
    dr_mcontext_t mcontext;
    mcontext.size = sizeof(mcontext);
    mcontext.flags = DR_MC_ALL;
    dr_get_mcontext(drcontext, &mcontext);

    bool get_reg_value_ok = true;
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM0, &mcontext, &zmm_buf[0 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM1, &mcontext, &zmm_buf[1 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM2, &mcontext, &zmm_buf[2 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM3, &mcontext, &zmm_buf[3 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM4, &mcontext, &zmm_buf[4 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM5, &mcontext, &zmm_buf[5 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM6, &mcontext, &zmm_buf[6 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM7, &mcontext, &zmm_buf[7 * 64]);
#ifdef X64
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM8, &mcontext, &zmm_buf[8 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM9, &mcontext, &zmm_buf[9 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM10, &mcontext, &zmm_buf[10 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM11, &mcontext, &zmm_buf[11 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM12, &mcontext, &zmm_buf[12 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM13, &mcontext, &zmm_buf[13 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM14, &mcontext, &zmm_buf[14 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM15, &mcontext, &zmm_buf[15 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM16, &mcontext, &zmm_buf[16 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM17, &mcontext, &zmm_buf[17 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM18, &mcontext, &zmm_buf[18 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM19, &mcontext, &zmm_buf[19 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM20, &mcontext, &zmm_buf[20 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM21, &mcontext, &zmm_buf[21 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM22, &mcontext, &zmm_buf[22 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM23, &mcontext, &zmm_buf[23 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM24, &mcontext, &zmm_buf[24 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM25, &mcontext, &zmm_buf[25 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM26, &mcontext, &zmm_buf[26 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM27, &mcontext, &zmm_buf[27 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM28, &mcontext, &zmm_buf[28 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM29, &mcontext, &zmm_buf[29 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM30, &mcontext, &zmm_buf[30 * 64]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_ZMM31, &mcontext, &zmm_buf[31 * 64]);
    if (!get_reg_value_ok)
        dr_fprintf(STDERR, "ERROR: problem reading zmm value\n");
#endif
    if (memcmp(zmm_buf, zmm_ref, sizeof(zmm_buf)) != 0) {
#if VERBOSE
        print_zmm(zmm_buf, zmm_ref);
#endif
        dr_fprintf(STDERR, "ERROR: wrong zmm value\n");
    }
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K0, &mcontext, &opmask_buf[0 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K1, &mcontext, &opmask_buf[1 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K2, &mcontext, &opmask_buf[2 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K3, &mcontext, &opmask_buf[3 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K4, &mcontext, &opmask_buf[4 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K5, &mcontext, &opmask_buf[5 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K6, &mcontext, &opmask_buf[6 * 8]);
    get_reg_value_ok =
        get_reg_value_ok && reg_get_value_ex(DR_REG_K7, &mcontext, &opmask_buf[7 * 8]);
    if (!get_reg_value_ok)
        dr_fprintf(STDERR, "ERROR: problem reading mask value\n");
    if (memcmp(opmask_buf, opmask_ref, sizeof(opmask_buf)) != 0) {
#if VERBOSE
        print_opmask(opmask_buf, opmask_ref);
#endif
        dr_fprintf(STDERR, "ERROR: wrong mask value\n");
    }
}

static void
clobber_avx512_state()
{
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
}

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
                opnd_get_reg(instr_get_dst(instr, 0)) == MARKER_REG) {
                if (val1 == TEST1_MARKER) {
                    clobber_avx512_state();
                } else if (val1 == TEST2_MARKER) {
                    dr_insert_clean_call_ex(drcontext, bb, instr,
                                            (void *)read_avx512_state,
                                            DR_CLEANCALL_READS_APP_CONTEXT, 0);
                    dr_insert_clean_call(drcontext, bb, instr,
                                         (void *)clobber_avx512_state, false, 0);
                }
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
