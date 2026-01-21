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

#define Z0_VALUE 0x65
#define Z15_VALUE 0x43
#define Z31_VALUE 0x21
#define P0_VALUE 0x98
#define P8_VALUE 0x76
#define P15_VALUE 0x54

#define SYS_EXIT_GROUP 94
#define SYS_WRITE 64

#define STDERR_FILE_NO 2

#define SIZE_OF_DR_SIMD_T 64
#define SIZE_OF_DR_SVEP_T 8

/* This is a statically-linked app. */
        .global  _start

        .align   6
_start:
        // Initialize z0, z15, and z31 registers.
        adr      x1, sve_vector_register_values
        ldr      z0, [x1]
        add      x1, x1, #SIZE_OF_DR_SIMD_T // sizeof(dr_simd_t)
        ldr      z15, [x1]
        add      x1, x1, #SIZE_OF_DR_SIMD_T // sizeof(dr_simd_t)
        ldr      z31, [x1]
        // Verify the values of the z0, z15, and z31 registers.
        cmphs    p0.d, p7/z, z0.d, #Z0_VALUE
        bne      fail
        cmphs    p0.d, p7/z, z15.d, #Z15_VALUE
        bne      fail
        cmphs    p0.d, p7/z, z31.d, #Z31_VALUE
        bne      fail
        // Initialize p0, p8, p15 registers.
        adr      x1, sve_predicate_register_values
        ldr      p0, [x1]
        add      x1, x1, #SIZE_OF_DR_SVEP_T // sizeof(dr_svep_t)
        ldr      p8, [x1]
        add      x1, x1, #SIZE_OF_DR_SVEP_T // sizeof(dr_svep_t)
        ldr      p15, [x1]
        // Verify the values of the p0, 08, and p15 registers.
        adr      x0, buffer
        str      p0, [x0]
        ldrb     w1, [x0]
        cmp      w1, P0_VALUE
        bne      fail
        str      p8, [x0]
        ldrb     w1, [x0]
        cmp      w1, P8_VALUE
        bne      fail
        str      p15, [x0]
        ldrb     w1, [x0]
        cmp      w1, P15_VALUE
        bne      fail

        mov      w0, STDERR_FILE_NO// stderr
        adr      x1, done_string
        mov      w2, #5            // sizeof(done_string)
        mov      w8, SYS_WRITE     // SYS_write
        svc      #0
        mov      w0, #0            // status
        mov      w8, SYS_EXIT_GROUP// SYS_exit_group
        svc      #0

fail:
        mov      w0, 1             // status
        mov      w8, SYS_EXIT_GROUP// SYS_exit_group
        svc      #0

        .data
        .align   6
done_string:
        .ascii   "Done\n"
sve_vector_register_values:
        .fill    SIZE_OF_DR_SIMD_T, 1, Z0_VALUE
        .fill    SIZE_OF_DR_SIMD_T, 1, Z15_VALUE
        .fill    SIZE_OF_DR_SIMD_T, 1, Z31_VALUE
sve_predicate_register_values:
        .fill    SIZE_OF_DR_SVEP_T, 1, P0_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P8_VALUE
        .fill    SIZE_OF_DR_SVEP_T, 1, P15_VALUE
buffer:
        .fill    SIZE_OF_DR_SVEP_T, 1, 0
