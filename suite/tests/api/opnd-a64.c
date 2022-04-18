/* **********************************************************
 * Copyright (c) 2018 Arm Limited.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
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

/* This file contains unit tests for the APIs exported from opnd.h. */

#include "configure.h"
#include "dr_api.h"
#include <stdio.h>

#define ASSERT(x)                                                                        \
    ((void)((!(x)) ? (fprintf(stderr, "ASSERT FAILURE: %s:%d: %s\n", __FILE__, __LINE__, \
                              #x),                                                       \
                      abort(), 0)                                                        \
                   : 0))

static void
test_get_size()
{
    /* Check sizes of special registers. */
    ASSERT(reg_get_size(DR_REG_WZR) == OPSZ_4);
    ASSERT(reg_get_size(DR_REG_XZR) == OPSZ_8);
    ASSERT(reg_get_size(DR_REG_SP) == OPSZ_8);
    ASSERT(reg_get_size(DR_REG_XSP) == OPSZ_8);

    // Check sizes of GPRs.
    for (uint i = 0; i < DR_NUM_GPR_REGS; i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_W0 + i) == OPSZ_4);
        ASSERT(reg_get_size((reg_id_t)DR_REG_X0 + i) == OPSZ_8);
    }

    // Check sizes of FP/SIMD regs.
    for (int i = 0; i < proc_num_simd_registers(); i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_H0 + i) == OPSZ_2);
        ASSERT(reg_get_size((reg_id_t)DR_REG_S0 + i) == OPSZ_4);
        ASSERT(reg_get_size((reg_id_t)DR_REG_D0 + i) == OPSZ_8);
        ASSERT(reg_get_size((reg_id_t)DR_REG_Q0 + i) == OPSZ_16);
    }

    // Check sizes of SVE vector regs.
    for (uint i = 0; i < 32; i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_Z0 + i) == OPSZ_SCALABLE);
    }

    // Check sizes of SVE predicate regs.
    for (uint i = 0; i < 16; i++) {
        ASSERT(reg_get_size((reg_id_t)DR_REG_P0 + i) == OPSZ_SCALABLE_PRED);
    }
}

static void
test_opnd_compute_address()
{
    dr_mcontext_t mc = { .size = sizeof(mc),
                         .flags = DR_MC_ALL,
                         .r0 = 256,
                         .r1 = 4,
                         .r2 = 8,
                         .r3 = -4,
                         .r4 = -8,
                         .xsp = 16 };

    opnd_t memref;
    app_pc loc;

    /* No shift or extend */

    // ldr w0, [sp]
    // 16 + 0 = 16
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_NULL, DR_EXTEND_UXTX, false,
                                           0, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, #4]
    // 16 + 4 = 20
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_NULL, DR_EXTEND_UXTX, false,
                                           4, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, #-4]
    // 16 - 4 = 12
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_NULL, DR_EXTEND_UXTX, false,
                                           -4, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    /* Shift and extend: 32 bit variant */

    // ldr w0, [sp, w2, uxtw #0]
    // 16 + 8 = 24
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, false,
                                           0, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, w2, uxtw #3]
    // 16 + (8 << 2) = 48
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, x1, lsl #0]
    // 16 + 4 = 20
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, false,
                                           0, 0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [sp, x1, lsl #3]
    // 16 + (4 << 2) = 32
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w4, sxtw #0]
    // 256 - 8 = 248
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, false, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w4, sxtw #3]
    // 256 - (8 << 2) = 224
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w3, sxtx #0]
    // 256 - 4 = 252
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, false, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr w0, [x0, w3, sxtx #3]
    // 256 - (4 << 2) = 240
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, true, 0,
                                           0, OPSZ_4);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    /* Shift and extend: 64 bit variant */

    // ldr x0, [sp, w2, uxtw #0]
    // 16 + 8 = 24
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, false,
                                           0, 0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [sp, w2, uxtw #3]
    // 16 + (8 << 3) = 80
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_W2, DR_EXTEND_UXTW, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [sp, x1, lsl #0]
    // 16 + 4 = 20
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, false,
                                           0, 0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [sp, x1, lsl #3]
    // 16 + (4 << 3) = 48
    memref = opnd_create_base_disp_aarch64(DR_REG_XSP, DR_REG_X1, DR_EXTEND_UXTX, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w4, sxtw #0]
    // 256 - 8 = 248
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, false, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w4, sxtw #3]
    // 256 - (8 << 3) = 192
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_W4, DR_EXTEND_SXTW, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w3, sxtx #0]
    // 256 - 4 = 252
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, false, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);

    // ldr x0, [x0, w3, sxtx #3]
    // 256 - (4 << 3) = 224
    memref = opnd_create_base_disp_aarch64(DR_REG_X0, DR_REG_X3, DR_EXTEND_SXTX, true, 0,
                                           0, OPSZ_8);
    loc = opnd_compute_address(memref, &mc);
    printf("location: %ld\n", (reg_t)loc);
}

int
main(int argc, char *argv[])
{
    test_get_size();

    test_opnd_compute_address();

    printf("all done\n");
    return 0;
}
