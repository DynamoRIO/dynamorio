/* **********************************************************
 * Copyright (c) 2026 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#define AARCH64
#define ASM_CODE_ONLY
#include "../api/detach_state_shared.h"

#define SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(n) \
  SIMD_UNIQUE_VAL_DOUBLEWORD_ELEMENT(MAKE_HEX_C(Z0_0_BASE()), \
                                     MAKE_HEX_C(Z_ELEMENT_INCREMENT_BASE()), n)
#define Z1_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(8)
#define Z2_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(16)
#define Z3_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(24)
#define Z4_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(32)
#define Z5_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(40)
#define Z6_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(48)
#define Z7_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(56)
#define Z8_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(64)
#define Z9_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(72)
#define Z10_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(80)
#define Z11_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(88)
#define Z12_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(96)
#define Z13_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(104)
#define Z14_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(112)
#define Z15_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(120)
#define Z16_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(128)
#define Z17_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(136)
#define Z18_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(144)
#define Z19_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(152)
#define Z20_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(160)
#define Z21_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(168)
#define Z22_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(176)
#define Z23_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(184)
#define Z24_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(192)
#define Z25_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(200)
#define Z26_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(208)
#define Z27_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(216)
#define Z28_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(224)
#define Z29_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(232)
#define Z30_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(240)
#define Z31_VALUE SIMD_UNIQUE_VAL_DOUBLEWORD_VALUE(248)

#define SIMD_UNIQUE_VAL_BYTE_VALUE(n) \
  SIMD_UNIQUE_VAL_BYTE_ELEMENT(MAKE_HEX_C(P0_0_BASE()), \
                                             MAKE_HEX_C(P_ELEMENT_INCREMENT_BASE()), n)
#define P0_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(0)
#define P1_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(1)
#define P2_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(2)
#define P3_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(3)
#define P4_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(4)
#define P5_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(5)
#define P6_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(6)
#define P7_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(7)
#define P8_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(8)
#define P9_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(9)
#define P10_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(10)
#define P11_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(11)
#define P12_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(12)
#define P13_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(13)
#define P14_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(14)
#define P15_VALUE SIMD_UNIQUE_VAL_BYTE_VALUE(15)

#define SYS_EXIT_GROUP 94
#define SYS_WRITE 64

#define STDERR_FILE_NO 2

#define SIZE_OF_DR_SIMD_T 64
#define SIZE_OF_DR_SVEP_T 8

.macro INIT_Z_VALUE value
       .rept SIZE_OF_DR_SIMD_T/8
       .quad \value
       .endr
.endm

.macro LOAD_Z_REG index, base_address_reg
        ldr      z\index, [\base_address_reg]
        add      \base_address_reg, \base_address_reg, #SIZE_OF_DR_SIMD_T
.endm

.macro CHECK_Z_REG index, base_address_reg
        ldr      z0, [\base_address_reg]
        add      \base_address_reg, \base_address_reg, #SIZE_OF_DR_SIMD_T
        ptrue    p7.d
        cmpeq    p0.d, p7/z, z0.d, z\index\().d
        b.cs     fail
.endm

.macro LOAD_P_REG index, base_address_reg
        ldr      p\index, [\base_address_reg]
        add      \base_address_reg, \base_address_reg, #SIZE_OF_DR_SVEP_T
.endm

.macro CHECK_P_REG index, value
        adr      x1, buffer
        str      p\index, [x1]
        ldrb     w0, [x1]
        cmp      w0, \value
        bne      fail
.endm

/* This is a statically-linked app. */
        .global  _start

        .align   6
_start:
        // Initialize SVE registers except z0. z0 is used to check the values of
        // other SVE registers.
        adr      x1, sve_vector_register_values
        LOAD_Z_REG 1, x1
        LOAD_Z_REG 2, x1
        LOAD_Z_REG 3, x1
        LOAD_Z_REG 4, x1
        LOAD_Z_REG 5, x1
        LOAD_Z_REG 6, x1
        LOAD_Z_REG 7, x1
        LOAD_Z_REG 8, x1
        LOAD_Z_REG 9, x1
        LOAD_Z_REG 10, x1
        LOAD_Z_REG 11, x1
        LOAD_Z_REG 12, x1
        LOAD_Z_REG 13, x1
        LOAD_Z_REG 14, x1
        LOAD_Z_REG 15, x1
        LOAD_Z_REG 16, x1
        LOAD_Z_REG 17, x1
        LOAD_Z_REG 18, x1
        LOAD_Z_REG 19, x1
        LOAD_Z_REG 20, x1
        LOAD_Z_REG 21, x1
        LOAD_Z_REG 22, x1
        LOAD_Z_REG 23, x1
        LOAD_Z_REG 24, x1
        LOAD_Z_REG 25, x1
        LOAD_Z_REG 26, x1
        LOAD_Z_REG 27, x1
        LOAD_Z_REG 28, x1
        LOAD_Z_REG 29, x1
        LOAD_Z_REG 30, x1
        LOAD_Z_REG 31, x1
        // Two consecutive nop is used as a sentinel to insert a clean call.
        nop
        nop
        // Verify the values of the SVE registers.
        adr      x1, sve_vector_register_values
        CHECK_Z_REG 1, x1
        CHECK_Z_REG 2, x1
        CHECK_Z_REG 3, x1
        CHECK_Z_REG 4, x1
        CHECK_Z_REG 5, x1
        CHECK_Z_REG 6, x1
        CHECK_Z_REG 7, x1
        CHECK_Z_REG 8, x1
        CHECK_Z_REG 9, x1
        CHECK_Z_REG 10, x1
        CHECK_Z_REG 11, x1
        CHECK_Z_REG 12, x1
        CHECK_Z_REG 13, x1
        CHECK_Z_REG 14, x1
        CHECK_Z_REG 15, x1
        CHECK_Z_REG 16, x1
        CHECK_Z_REG 17, x1
        CHECK_Z_REG 18, x1
        CHECK_Z_REG 19, x1
        CHECK_Z_REG 20, x1
        CHECK_Z_REG 21, x1
        CHECK_Z_REG 22, x1
        CHECK_Z_REG 23, x1
        CHECK_Z_REG 24, x1
        CHECK_Z_REG 25, x1
        CHECK_Z_REG 26, x1
        CHECK_Z_REG 27, x1
        CHECK_Z_REG 28, x1
        CHECK_Z_REG 29, x1
        CHECK_Z_REG 30, x1
        CHECK_Z_REG 31, x1
        // Initialize SVE predicate registers.
        adr      x1, sve_predicate_register_values
        LOAD_P_REG 0, x1
        LOAD_P_REG 1, x1
        LOAD_P_REG 2, x1
        LOAD_P_REG 3, x1
        LOAD_P_REG 4, x1
        LOAD_P_REG 5, x1
        LOAD_P_REG 6, x1
        LOAD_P_REG 7, x1
        LOAD_P_REG 8, x1
        LOAD_P_REG 9, x1
        LOAD_P_REG 10, x1
        LOAD_P_REG 11, x1
        LOAD_P_REG 12, x1
        LOAD_P_REG 13, x1
        LOAD_P_REG 14, x1
        LOAD_P_REG 15, x1
        // Two consecutive nop is used as a sentinel to insert a clean call.
        nop
        nop
        // Verify the values of the SVE predicate registers.
        CHECK_P_REG 0, P0_VALUE
        CHECK_P_REG 1, P1_VALUE
        CHECK_P_REG 2, P2_VALUE
        CHECK_P_REG 3, P3_VALUE
        CHECK_P_REG 4, P4_VALUE
        CHECK_P_REG 5, P5_VALUE
        CHECK_P_REG 6, P6_VALUE
        CHECK_P_REG 7, P7_VALUE
        CHECK_P_REG 8, P8_VALUE
        CHECK_P_REG 9, P9_VALUE
        CHECK_P_REG 10, P10_VALUE
        CHECK_P_REG 11, P11_VALUE
        CHECK_P_REG 12, P12_VALUE
        CHECK_P_REG 13, P13_VALUE
        CHECK_P_REG 14, P14_VALUE
        CHECK_P_REG 15, P15_VALUE

        mov      w0, STDERR_FILE_NO
        adr      x1, done_string
        mov      w2, #5            // sizeof(done_string)
        mov      w8, SYS_WRITE
        svc      #0
        mov      w0, #0            // status
        mov      w8, SYS_EXIT_GROUP
        svc      #0

fail:
        mov      w0, 1             // status
        mov      w8, SYS_EXIT_GROUP
        svc      #0

        .data
        .align   6
done_string:
        .ascii   "Done\n"
sve_vector_register_values:
        INIT_Z_VALUE Z1_VALUE
        INIT_Z_VALUE Z2_VALUE
        INIT_Z_VALUE Z3_VALUE
        INIT_Z_VALUE Z4_VALUE
        INIT_Z_VALUE Z5_VALUE
        INIT_Z_VALUE Z6_VALUE
        INIT_Z_VALUE Z7_VALUE
        INIT_Z_VALUE Z8_VALUE
        INIT_Z_VALUE Z9_VALUE
        INIT_Z_VALUE Z10_VALUE
        INIT_Z_VALUE Z11_VALUE
        INIT_Z_VALUE Z12_VALUE
        INIT_Z_VALUE Z13_VALUE
        INIT_Z_VALUE Z14_VALUE
        INIT_Z_VALUE Z15_VALUE
        INIT_Z_VALUE Z16_VALUE
        INIT_Z_VALUE Z17_VALUE
        INIT_Z_VALUE Z18_VALUE
        INIT_Z_VALUE Z19_VALUE
        INIT_Z_VALUE Z20_VALUE
        INIT_Z_VALUE Z21_VALUE
        INIT_Z_VALUE Z22_VALUE
        INIT_Z_VALUE Z23_VALUE
        INIT_Z_VALUE Z24_VALUE
        INIT_Z_VALUE Z25_VALUE
        INIT_Z_VALUE Z26_VALUE
        INIT_Z_VALUE Z27_VALUE
        INIT_Z_VALUE Z28_VALUE
        INIT_Z_VALUE Z29_VALUE
        INIT_Z_VALUE Z30_VALUE
        INIT_Z_VALUE Z31_VALUE
sve_predicate_register_values:
        .fill    SIZE_OF_DR_SVEP_T, 1, P0_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P1_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P2_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P3_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P4_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P5_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P6_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P7_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P8_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P9_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P10_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P11_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P12_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P13_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P14_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P15_VALUE
buffer:
        .fill    SIZE_OF_DR_SVEP_T, 1, 0
