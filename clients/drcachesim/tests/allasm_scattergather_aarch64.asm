 /* **********************************************************
 * Copyright (c) 2023 Arm Limited.  All rights reserved.
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
 * * Neither the name of Arm Limited nor the names of its contributors may be
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

/* This is a statically-linked app.
 * XXX i#3107: Pure-asm apps like this are difficult to maintain; We need this to be
 * pure-asm so that we can verify exact instruction, load and store counts for the
 * expanded scatter/gather sequence. With support for #3107, we can use annotations to
 * mark app phases and compute those counts for only the intended parts in the C+asm app.
 *
 * The drx-scattergather-aarch64.cpp test app covers functional testing of the expansion.
 * This test just needs to execute some scatter/gather/predicated contiguous loads and
 * stores so we can verify the counts in the basic_counts output are correct.
 */
.text
        .global  _start

        .align   6

// Destination registers used in load instructions
#define DEST_REG1 z28
#define DEST_REG2 z29
#define DEST_REG3 z30
#define DEST_REG4 z31

// Source registers used in store instructions. These alias DEST_REG* because this test
// doesn't care about the actual values being loaded/stored.
#define SRC_REG1 z28
#define SRC_REG2 z29
#define SRC_REG3 z30
#define SRC_REG4 z31

#define B_MASK_REG p0 // Governing predicate for byte-element instructions
#define H_MASK_REG p1 // Governing predicate for half-element instructions
#define S_MASK_REG p2 // Governing predicate for word-element instructions
#define D_MASK_REG p3 // Governing predicate for doubleword-element instructions

#define BUFFER_REG x1
#define Z_BASE_REG z0 // base reg used in vector+immediate/vector+scalar instructions

#define S_INDEX_REG z1 // index reg used in scalar+immed instructions with 32-bit elements
#define D_INDEX_REG z2 // index reg used in scalar+immed instructions with 64-bit elements

#define X_INDEX_REG x2 // index reg used in scalar+scalar/vector+scalar instructions

/*
 * Test functions. The commented number after each instruction indicates the number of
 * elements this instruction accesses with a 128-bit vector length. We can add these
 * numbers up to determine how many loads/stores we expect to see in the basic_counts
 * output when all elements are active. To find the number for hardware with a larger
 * vector size, multiply by vl_bytes/16.
 */

test_scalar_plus_vector:
        // ld1b scalar+vector
        ld1b    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw] // 4
        ld1b    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw] // 4
        ld1b    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw] // 2
        ld1b    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw] // 2
        ld1b    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]       // 2
                                                                             // Total: 14
        // ld1sb scalar+vector
        ld1sb   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw] // 4
        ld1sb   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw] // 4
        ld1sb   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw] // 2
        ld1sb   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw] // 2
        ld1sb   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]       // 2
                                                                             // Total: 14
        // ld1h scalar+vector
        ld1h    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw #1] // 4
        ld1h    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw #1] // 4
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw #1] // 2
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw #1] // 2
        ld1h    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw]    // 4
        ld1h    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw]    // 4
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, lsl #1]  // 2
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                                // Total: 28
        // ld1sh scalar+vector
        ld1sh   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw #1] // 4
        ld1sh   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw #1] // 4
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw #1] // 2
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw #1] // 2
        ld1sh   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw]    // 4
        ld1sh   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw]    // 4
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, lsl #1]  // 2
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                                // Total: 28
        // ld1w scalar+vector
        ld1w    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw #2] // 4
        ld1w    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw #2] // 4
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw #2] // 2
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw #2] // 2
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        ld1w    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, uxtw]    // 4
        ld1w    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, S_INDEX_REG.s, sxtw]    // 4
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, lsl #2]  // 2
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                                // Total: 28
        // ld1sw scalar+vector
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw #2] // 2
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw #2] // 2
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, lsl #2]  // 2
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                                // Total: 12
        // ld1d scalar+vector
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw #3] // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw #3] // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d, lsl #3]  // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                                // Total: 12
        // Total loads: 14 + 14 + 28 + 28 + 28 + 12 + 12 = 136

        // st1b scalar+vector
        st1b    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw] // 2
        st1b    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw] // 2
        st1b    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, uxtw] // 4
        st1b    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, sxtw] // 4
        st1b    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d]       // 2
                                                                          // Total: 14
        // st1h scalar+vector
        st1h    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, uxtw #1] // 4
        st1h    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, sxtw #1] // 4
        st1h    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw #1] // 2
        st1h    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw #1] // 2
        st1h    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        st1h    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        st1h    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, uxtw]    // 4
        st1h    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, sxtw]    // 4
        st1h    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, lsl #1]  // 2
        st1h    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                             // Total: 28
        // st1w scalar+vector
        st1w    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, uxtw #2] // 4
        st1w    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, sxtw #2] // 4
        st1w    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw #2] // 2
        st1w    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw #2] // 2
        st1w    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        st1w    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        st1w    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, uxtw]    // 4
        st1w    SRC_REG1.s, S_MASK_REG, [BUFFER_REG, S_INDEX_REG.s, sxtw]    // 4
        st1w    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, lsl #2]  // 2
        st1w    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                             // Total: 28
        // st1d scalar+vector
        st1d    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw #3] // 2
        st1d    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw #3] // 2
        st1d    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, uxtw]    // 2
        st1d    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, sxtw]    // 2
        st1d    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d, lsl #3]  // 2
        st1d    SRC_REG1.d, D_MASK_REG, [BUFFER_REG, D_INDEX_REG.d]          // 2
                                                                             // Total: 12

        // Total stores: 14 + 28 + 28 + 12 = 82

        ret


test_vector_plus_immediate:
        ld1b    DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2
        ld1sb   DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2
        ld1h    DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2
        ld1w    DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, #0] // 2

        // Total loads: 14

        st1b    SRC_REG1.d, D_MASK_REG, [Z_BASE_REG.d, #0] // 2
        st1h    SRC_REG1.d, D_MASK_REG, [Z_BASE_REG.d, #0] // 2
        st1w    SRC_REG1.d, D_MASK_REG, [Z_BASE_REG.d, #0] // 2
        st1d    SRC_REG1.d, D_MASK_REG, [Z_BASE_REG.d, #0] // 2

        // Total stores: 8

        ret

test_scalar_plus_scalar:
        ld1b     DEST_REG1.b, B_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 16
        ld1b     DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 8
        ld1b     DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 4
        ld1b     DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 2
        ldnt1b   DEST_REG1.b, B_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 16
        ld1sb    DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 8
        ld1sb    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 4
        ld1sb    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 2
        ld1h     DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 8
        ld1h     DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 4
        ld1h     DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 2
        ldnt1h   DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 8
        ld1sh    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 4
        ld1sh    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 2
        ld1w     DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 4
        ld1w     DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 2
        ldnt1w   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 4
        ld1sw    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 2
        ld1d     DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #3] // 2
        ldnt1d   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #3] // 2
                                                                              // Total: 104

        ld2b     {DEST_REG1.b, DEST_REG2.b}, B_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 32
        ld2h     {DEST_REG1.h, DEST_REG2.h}, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 16
        ld2w     {DEST_REG1.s, DEST_REG2.s}, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 8
        ld2d     {DEST_REG1.d, DEST_REG2.d}, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #3] // 4
                                                                                             // Total: 60

        ld3b     {DEST_REG1.b, DEST_REG2.b, DEST_REG3.b}, B_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 48
        ld3h     {DEST_REG1.h, DEST_REG2.h, DEST_REG3.h}, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 24
        ld3w     {DEST_REG1.s, DEST_REG2.s, DEST_REG3.s}, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 12
        ld3d     {DEST_REG1.d, DEST_REG2.d, DEST_REG3.d}, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #3] // 6
                                                                                                          // Total: 90

        ld4b     {DEST_REG1.b, DEST_REG2.b, DEST_REG3.b, DEST_REG4.b}, B_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 64
        ld4h     {DEST_REG1.h, DEST_REG2.h, DEST_REG3.h, DEST_REG4.h}, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 32
        ld4w     {DEST_REG1.s, DEST_REG2.s, DEST_REG3.s, DEST_REG4.s}, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 16
        ld4d     {DEST_REG1.d, DEST_REG2.d, DEST_REG3.d, DEST_REG4.d}, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #3] // 8
                                                                                                                       // Total: 120

        // Total loads: 104 + 60 + 90 + 120 = 374

        st1b    DEST_REG1.b, B_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 16
        st1b    DEST_REG1.h, H_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 8
        st1b    DEST_REG1.s, S_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 4
        st1b    DEST_REG1.d, D_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 2
        st1h    DEST_REG1.h, H_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #1] // 8
        st1h    DEST_REG1.s, S_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #1] // 4
        st1h    DEST_REG1.d, D_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #1] // 2
        st1w    DEST_REG1.s, S_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #2] // 4
        st1w    DEST_REG1.d, D_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #2] // 2
        st1d    DEST_REG1.d, D_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #3] // 2
                                                                           // Total: 52

        st2b    {DEST_REG1.b, DEST_REG2.b}, B_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 32
        st2h    {DEST_REG1.h, DEST_REG2.h}, H_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #1] // 16
        st2w    {DEST_REG1.s, DEST_REG2.s}, S_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #2] // 8
        st2d    {DEST_REG1.d, DEST_REG2.d}, D_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #3] // 4
                                                                                          // Total: 60

        st3b    {DEST_REG1.b, DEST_REG2.b, DEST_REG3.b}, B_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 48
        st3h    {DEST_REG1.h, DEST_REG2.h, DEST_REG3.h}, H_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #1] // 24
        st3w    {DEST_REG1.s, DEST_REG2.s, DEST_REG3.s}, S_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #2] // 12
        st3d    {DEST_REG1.d, DEST_REG2.d, DEST_REG3.d}, D_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #3] // 6
                                                                                                       // Total: 90

        st4b    {DEST_REG1.b, DEST_REG2.b, DEST_REG3.b, DEST_REG4.b}, B_MASK_REG, [BUFFER_REG, X_INDEX_REG]         // 64
        st4h    {DEST_REG1.h, DEST_REG2.h, DEST_REG3.h, DEST_REG4.h}, H_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #1] // 32
        st4w    {DEST_REG1.s, DEST_REG2.s, DEST_REG3.s, DEST_REG4.s}, S_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #2] // 16
        st4d    {DEST_REG1.d, DEST_REG2.d, DEST_REG3.d, DEST_REG4.d}, D_MASK_REG, [BUFFER_REG, X_INDEX_REG, lsl #3] // 8
                                                                                                                    // Total: 120

        // Total stores: 52 + 60 + 90 + 120 = 322

        ret


test_scalar_plus_immediate:
        ld1b    DEST_REG1.b, B_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 16
        ld1b    DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 8
        ld1b    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
        ld1b    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ldnt1b  DEST_REG1.b, B_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 16
        ld1sb   DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 8
        ld1sb   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
        ld1sb   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ld1h    DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 8
        ld1h    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
        ld1h    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ldnt1h  DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 8
        ld1sh   DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
        ld1sh   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ld1w    DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
        ld1w    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ldnt1w  DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
        ld1sw   DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ld1d    DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
        ldnt1d  DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 2
                                                                    // Total: 104

        ld2b { DEST_REG1.b, DEST_REG2.b }, B_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 32
        ld2h { DEST_REG1.h, DEST_REG2.h }, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 16
        ld2w { DEST_REG1.s, DEST_REG2.s }, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 8
        ld2d { DEST_REG1.d, DEST_REG2.d }, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 4
                                                                                  // Total: 60

        ld3b { DEST_REG1.b, DEST_REG2.b, DEST_REG3.b }, B_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 48
        ld3h { DEST_REG1.h, DEST_REG2.h, DEST_REG3.h }, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 24
        ld3w { DEST_REG1.s, DEST_REG2.s, DEST_REG3.s }, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 12
        ld3d { DEST_REG1.d, DEST_REG2.d, DEST_REG3.d }, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 6
                                                                                               // Total: 90

        ld4b { DEST_REG1.b, DEST_REG2.b, DEST_REG3.b, DEST_REG4.b }, B_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 64
        ld4h { DEST_REG1.h, DEST_REG2.h, DEST_REG3.h, DEST_REG4.h }, H_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 32
        ld4w { DEST_REG1.s, DEST_REG2.s, DEST_REG3.s, DEST_REG4.s }, S_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 16
        ld4d { DEST_REG1.d, DEST_REG2.d, DEST_REG3.d, DEST_REG4.d }, D_MASK_REG/z, [BUFFER_REG, #0, mul vl] // 8
                                                                                                            // Total: 120
        // Total loads: 104 + 60 + 90 + 120 = 374

        st1b SRC_REG1.b, B_MASK_REG, [BUFFER_REG, #0, mul vl] // 16
        st1b SRC_REG1.h, H_MASK_REG, [BUFFER_REG, #0, mul vl] // 8
        st1b SRC_REG1.s, S_MASK_REG, [BUFFER_REG, #0, mul vl] // 4
        st1b SRC_REG1.d, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 2
        st1h SRC_REG1.h, H_MASK_REG, [BUFFER_REG, #0, mul vl] // 8
        st1h SRC_REG1.s, S_MASK_REG, [BUFFER_REG, #0, mul vl] // 4
        st1h SRC_REG1.d, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 2
        st1w SRC_REG1.s, S_MASK_REG, [BUFFER_REG, #0, mul vl] // 4
        st1w SRC_REG1.d, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 2
        st1d SRC_REG1.d, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 2
                                                              // Total: 52

        st2b { SRC_REG1.b, SRC_REG2.b }, B_MASK_REG, [BUFFER_REG, #0, mul vl] // 32
        st2h { SRC_REG1.h, SRC_REG2.h }, H_MASK_REG, [BUFFER_REG, #0, mul vl] // 16
        st2w { SRC_REG1.s, SRC_REG2.s }, S_MASK_REG, [BUFFER_REG, #0, mul vl] // 8
        st2d { SRC_REG1.d, SRC_REG2.d }, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 4
                                                                              // Total: 60

        st3b { SRC_REG1.b, SRC_REG2.b, SRC_REG3.b }, B_MASK_REG, [BUFFER_REG, #0, mul vl] // 48
        st3h { SRC_REG1.h, SRC_REG2.h, SRC_REG3.h }, H_MASK_REG, [BUFFER_REG, #0, mul vl] // 24
        st3w { SRC_REG1.s, SRC_REG2.s, SRC_REG3.s }, S_MASK_REG, [BUFFER_REG, #0, mul vl] // 12
        st3d { SRC_REG1.d, SRC_REG2.d, SRC_REG3.d }, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 6
                                                                                          // Total: 90

        st4b { SRC_REG1.b, SRC_REG2.b, SRC_REG3.b, SRC_REG4.b }, B_MASK_REG, [BUFFER_REG, #0, mul vl] // 64
        st4h { SRC_REG1.h, SRC_REG2.h, SRC_REG3.h, SRC_REG4.h }, H_MASK_REG, [BUFFER_REG, #0, mul vl] // 32
        st4w { SRC_REG1.s, SRC_REG2.s, SRC_REG3.s, SRC_REG4.s }, S_MASK_REG, [BUFFER_REG, #0, mul vl] // 16
        st4d { SRC_REG1.d, SRC_REG2.d, SRC_REG3.d, SRC_REG4.d }, D_MASK_REG, [BUFFER_REG, #0, mul vl] // 8
                                                                                                      // Total: 120
        // Total stores: 52 + 60 + 90 + 120 = 322

        ret

test_replicating_loads:
        ld1rqb  DEST_REG1.b, B_MASK_REG/z, [BUFFER_REG, X_INDEX_REG]         // 16
        ld1rqh  DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #1] // 8
        ld1rqw  DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #2] // 4
        ld1rqd  DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, X_INDEX_REG, lsl #3] // 2
                                                                             // Total: 30

        ld1rqb  DEST_REG1.b, B_MASK_REG/z, [BUFFER_REG, #0] // 16
        ld1rqh  DEST_REG1.h, H_MASK_REG/z, [BUFFER_REG, #0] // 8
        ld1rqw  DEST_REG1.s, S_MASK_REG/z, [BUFFER_REG, #0] // 4
        ld1rqd  DEST_REG1.d, D_MASK_REG/z, [BUFFER_REG, #0] // 2
                                                            // Total: 30

        ret

#ifdef __ARM_FEATURE_SVE2

test_vector_plus_scalar:
        ldnt1b  DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
        ldnt1sb DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
        ldnt1h  DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
        ldnt1sh DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
        ldnt1w  DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
        ldnt1sw DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
        ldnt1d  DEST_REG1.d, D_MASK_REG/z, [Z_BASE_REG.d, X_INDEX_REG] // 2
                                                                       // Total: 14

        stnt1b  {SRC_REG1.d}, D_MASK_REG, [Z_BASE_REG.d, X_INDEX_REG] // 2
        stnt1h  {SRC_REG1.d}, D_MASK_REG, [Z_BASE_REG.d, X_INDEX_REG] // 2
        stnt1w  {SRC_REG1.d}, D_MASK_REG, [Z_BASE_REG.d, X_INDEX_REG] // 2
        stnt1d  {SRC_REG1.d}, D_MASK_REG, [Z_BASE_REG.d, X_INDEX_REG] // 2
                                                                      // Total: 8
        ret
#endif // __ARM_FEATURE_SVE2

_start:
#ifdef __APPLE__
        adrp     BUFFER_REG, buffer@PAGE
        add      BUFFER_REG, BUFFER_REG, buffer@PAGEOFF
#else
        adr      BUFFER_REG, buffer
#endif

        // Initialize the registers used by the test instructions.

        // Set the scalar+vector index registers to an incrementing sequence of offsets
        index   S_INDEX_REG.s, #0, #1
        index   D_INDEX_REG.d, #0, #1
        // Set the vector+immediate base register so that all elements point to the start
        // of the buffer
        index   Z_BASE_REG.d, BUFFER_REG, #0

        // Set the scalar+scalar index to point to the start of the buffer
        mov     X_INDEX_REG, #0


        /* Run all the instructions with all elements active */

        ptrue   B_MASK_REG.b // Set all elements to active in the mask regs
        ptrue   H_MASK_REG.h
        ptrue   S_MASK_REG.s
        ptrue   D_MASK_REG.d

        // The expanded code should have one load/store per element per register.
        // The total number of loads/stores depends on the current vector length.
        bl      test_scalar_plus_vector    // +(136 * vl_bytes/16) loads
                                           // +(82 * vl_bytes/16) stores

        bl      test_vector_plus_immediate // +(14 * vl_bytes/16) loads
                                           // +(8 * vl_bytes/16) stores

        bl      test_scalar_plus_scalar    // +(374 * vl_bytes/16) loads
                                           // +(322 * vl_bytes/16) stores

        bl      test_scalar_plus_immediate // +(374 * vl_bytes/16) loads
                                           // +(322 * vl_bytes/16) stores
        bl      test_replicating_loads     // +60 loads
                                           // +0 stores
#ifdef __ARM_FEATURE_SVE2
        bl      test_vector_plus_scalar    // +(14 * vl_bytes/16) loads
                                           // +(8 * vl_bytes/16) stores
#endif
        // Running total:
        // SVE only:
        // Loads: (136 + 14 + 374 + 374) * vl_bytes/16 + 60 = 898 * vl_bytes/16 + 60
        // Stores: (82 + 8 + 322 + 322) * vl_bytes/16 = 734 * vl_bytes/16

        // Including SVE2:
        // Loads: ((898 + 14) * vl_bytes/16) + 60 = (912 * vl_bytes/16) + 60
        // Stores: (734 + 8) * vl_bytes/16 = 742 * vl_bytes/16

        /* Run all the instructions with no active elements */

        pfalse  B_MASK_REG.b // Set all elements to inactive in the mask regs
        pfalse  H_MASK_REG.b
        pfalse  S_MASK_REG.b
        pfalse  D_MASK_REG.b

        bl      test_scalar_plus_vector    // +0 loads, +0 stores
        bl      test_vector_plus_immediate // +0 loads, +0 stores
        bl      test_scalar_plus_scalar    // +0 loads, +0 stores
        bl      test_scalar_plus_immediate // +0 loads, +0 stores
        bl      test_replicating_loads     // +0 loads, +0 stores
#ifdef __ARM_FEATURE_SVE2
        bl      test_vector_plus_scalar    // +0 loads, +0 stores
#endif

        // Running total (unchanged from above):
        // SVE only:
        // Loads:  (898 * vl_bytes/16) + 60
        // Stores: 734 * vl_bytes/16

        // Including SVE2:
        // Loads: (912 * vl_bytes/16) + 60
        // Stores: 742 * vl_bytes/16

        /* Run all instructions with one active element */
        ptrue    B_MASK_REG.b, VL1 // Set 1 element to active in the mask regs.
        ptrue    H_MASK_REG.h, VL1 // The rest of the elements are inactive.
        ptrue    S_MASK_REG.s, VL1
        ptrue    D_MASK_REG.d, VL1

        bl      test_scalar_plus_vector    // +52 loads, +31 stores
        bl      test_vector_plus_immediate // +7 loads,  +4 stores
        bl      test_scalar_plus_scalar    // +56 loads, +46 stores
        bl      test_scalar_plus_immediate // +56 loads, +46 stores
        bl      test_replicating_loads     // +8 loads, +0 stores
#ifdef __ARM_FEATURE_SVE2
        bl      test_vector_plus_scalar    // +7 loads, +4 stores
#endif

        // Running total:
        // SVE only:
        // Loads:  (898 * vl_bytes/16) + 60 + 52 + 7 + 56 + 56 + 8 = (898 * vl_bytes/16) + 239
        // Stores: (734 * vl_bytes/16) + 41 + 4 + 46 + 46 = (734 * vl_bytes/16) + 127

        // Including SVE2:
        // Loads:  (912 * vl_bytes/16) + 239 + 7 = (912 * vl_bytes/16) + 246
        // Stores: (742 * vl_bytes/16) + 127 + 4 = (742 * vl_bytes/16) + 131

        // The functions in this file have the following instructions counts:
        //     _start                       40 (+3 SVE2)
        //     test_scalar_plus_vector      84
        //     test_vector_plus_immediate   12
        //     test_scalar_plus_scalar      55
        //     test_scalar_plus_immediate   55
        //     test_replicating_loads       9
        //     test_vector_plus_scalar      12
        // So there are 40 + 84 + 12 + 55 + 55 + 9 = 255 unique instructions
        // (or 255 + 12 + 3 = 270 including SVE2)
        // We run the test_* functions 3 times each so the total instruction executed is
        //     ((84 + 12 + 55 + 55 + 9) * 3) + 40 = (215 * 3) + 37 = 685
        // (or 685 + 3 + (12 * 3) = 724 including SVE2)

        // Totals:
        // SVE only:
        // Loads:  (898 * vl_bytes/16) + 239
        // Stores: (734 * vl_bytes/16) + 127
        // Instructions: 685
        // Unique instructions: 255

        // Including SVE2:
        // Loads:  (912 * vl_bytes/16) + 246
        // Stores: (742 * vl_bytes/16) + 131
        // Instructions: 724
        // Unique instructions: 270

// Exit.
        mov      w0, #1            // stdout
#ifdef __APPLE__
        adrp     x1, helloworld@PAGE
        add      x1, x1, helloworld@PAGEOFF
#else
        adr      x1, helloworld
#endif

        mov      w2, #14           // sizeof(helloworld)
        mov      w8, #64           // SYS_write
        svc      #0

        mov      w0, #0            // status
        mov      w8, #94           // SYS_exit_group
        svc      #0

        .data
        .align  6

helloworld:
        .ascii   "Hello, world!\n"

buffer:
        .zero   1024                // Maximum size of an SVE Z register * 4.
