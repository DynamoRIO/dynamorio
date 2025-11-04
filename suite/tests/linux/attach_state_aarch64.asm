/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2025 Arm Limited.       All rights reserved.
 * *********************************************************/

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

/* This is the main attach state test function. Called by attach_state.c, it sets
 * registers with reference values before sitting in a loop waiting for the
 * attach. The attach is synchronised using the "Name:" field of
 * /proc/<PID>/status which is set just before the loop starts using the prctl
 * syscall. This status is polled by runall.cmake which calls the
 * client.attach_state test which attaches during the loop. When the loop ends,
 * the contents of registers are checked against expected reference values.
 *
 * C function signature:
 * void attach_state_test(const uint64_t *gpr_ref,         x0
 *                        const __uint128_t *simd_vals,    x1
 *                        uint64_t *gpr_att,               x2
 *                        __uint128_t *simd_att,           x3
 *                        uint32_t *fpcr_att,              x4
 *                        uint32_t *fpsr_att,              x5
 *                        uint64_t *sp_ref,                x6
 *                        uint64_t *pc_before,             x7
 *                        uint32_t *nzcv_ref,              SP + 0
 *                        uint64_t *sp_att,                SP + 8
 *                        uint64_t *pc_after,              SP + 16
 *                        uint32_t *nzcv_att,              SP + 24
 *                        const uint16_t *pred_ref,        SP + 32
 *                        uint16_t *pred_att);             SP + 40
 *
 * x9 is used as a scratch register and for the loop count so is excluded from
 * the state check.
 */
.text
.align  2
.global attach_state_test
.type   attach_state_test, %function
attach_state_test:
    // Read function arguments from stack before changing SP.
    mov     x12, sp
    ldp     x13, x14, [x12, #0]   // nzcv_ref, sp_att
    ldp     x15, x16, [x12, #16]  // pc_after, nzcv_att
    ldp     x8,  x10, [x12, #32]  // pred_ref, pred_att

    /* Local stack frame layout (288 bytes, 16 byte aligned):
     * [sp+0]   = gpr_att     (x2)
     * [sp+8]   = simd_att    (x3)
     * [sp+16]  = fpcr_att    (x4)
     * [sp+24]  = fpsr_att    (x5)
     * [sp+32]  = sp_ref      (x6)
     * [sp+40]  = pc_before   (x7)
     * [sp+48]  = nzcv_ref    (x13)
     * [sp+56]  = sp_att      (x14)
     * [sp+64]  = pc_after    (x15)
     * [sp+72]  = nzcv_att    (x16)
     * [sp+80]  = saved x9 loop counter value
     * [sp+88] -> [sp+168] = callee-saved registers x19..x29
     * [sp+184] = saved x30   (LR)
     * [sp+192] = saved gpr_ref array base pointer
     * [sp+200] = shadow x0  (used for prctl syscall)
     * [sp+208] = shadow x1  (           "          )
     * [sp+216] = shadow x2  (           "          )
     * [sp+224] = shadow x3  (           "          )
     * [sp+232] = shadow x4  (           "          )
     * [sp+240] = shadow x5  (           "          )
     * [sp+248] = shadow x8  (           "          )
     * [sp+256] = shadow x10 (           "          )
     * [sp+288] = pred_att    (x10)
     */
    sub     sp, sp, #288
    stp     x2, x3, [sp, #0]    // gpr_att, simd_att
    stp     x4, x5, [sp, #16]   // fpcr_att, fpsr_att
    stp     x6, x7, [sp, #32]   // sp_ref, pc_before

    stp     x13, x14, [sp, #48] // nzcv_ref, sp_att
    stp     x15, x16, [sp, #64] // pc_after, nzcv_att
    str     x0,  [sp, #192]     // Save gpr_ref
    str     x10, [sp, #264]     // Save pred_att

    // Save callee-saved registers and LR.
    stp     x19, x20, [sp, #96]
    stp     x21, x22, [sp, #112]
    stp     x23, x24, [sp, #128]
    stp     x25, x26, [sp, #144]
    stp     x27, x28, [sp, #160]
    stp     x29, x30, [sp, #176]

    // Set bits in NZCV as reference data before attach.
    cmp     xzr, xzr   // NZCV=0x60000000 (N=0 Z=1 C=1 V=0)

    // Touch FP unit to ensure kernel enables FP/SIMD state for the thread.
    fmov    d0, xzr
    // Set bits in FPCR as reference data before attach.
    mov     x10, #0x03400000        // DN=1 FZ=1 RMode=+Inf (RP mode)
    msr     fpcr, x10
    isb
    // Set bits in FPSR as reference data before attach.
    msr     fpsr, xzr
    isb
    fmov    d0, xzr
    fdiv    d0, d0, d0               // IOC=1
    movi    v1.16b, #0x7f
    sqadd   v1.16b, v1.16b, v1.16b   // QC=1

    ldr     x9,  [sp, #32]           // sp_ref
    mov     x10, sp
    str     x10, [x9]

.global pc_before_label
pc_before_label:
    ldr     x9,  [sp, #40]           // pc_before
    adr     x10, pc_before_label
    str     x10, [x9]

    ldr     x9,  [sp, #48]           // nzcv_ref
    mrs     x10, nzcv
    str     w10, [x9]

    // Load SIMD registers from simd_ref (x1).
    mov     x2, x1
#if defined(__ARM_FEATURE_SVE)
    ldr     z0, [x2, #0, mul vl]
    ldr     z1, [x2, #1, mul vl]
    ldr     z2, [x2, #2, mul vl]
    ldr     z3, [x2, #3, mul vl]
    ldr     z4, [x2, #4, mul vl]
    ldr     z5, [x2, #5, mul vl]
    ldr     z6, [x2, #6, mul vl]
    ldr     z7, [x2, #7, mul vl]
    ldr     z8, [x2, #8, mul vl]
    ldr     z9, [x2, #9, mul vl]
    ldr     z10, [x2, #10, mul vl]
    ldr     z11, [x2, #11, mul vl]
    ldr     z12, [x2, #12, mul vl]
    ldr     z13, [x2, #13, mul vl]
    ldr     z14, [x2, #14, mul vl]
    ldr     z15, [x2, #15, mul vl]
    ldr     z16, [x2, #16, mul vl]
    ldr     z17, [x2, #17, mul vl]
    ldr     z18, [x2, #18, mul vl]
    ldr     z19, [x2, #19, mul vl]
    ldr     z20, [x2, #20, mul vl]
    ldr     z21, [x2, #21, mul vl]
    ldr     z22, [x2, #22, mul vl]
    ldr     z23, [x2, #23, mul vl]
    ldr     z24, [x2, #24, mul vl]
    ldr     z25, [x2, #25, mul vl]
    ldr     z26, [x2, #26, mul vl]
    ldr     z27, [x2, #27, mul vl]
    ldr     z28, [x2, #28, mul vl]
    ldr     z29, [x2, #29, mul vl]
    ldr     z30, [x2, #30, mul vl]
    ldr     z31, [x2, #31, mul vl]
    // Load predicate registers from pred_ref (x8).
    ldr     p0, [x8, #0, mul vl]
    ldr     p1, [x8, #1, mul vl]
    ldr     p2, [x8, #2, mul vl]
    ldr     p3, [x8, #3, mul vl]
    ldr     p4, [x8, #4, mul vl]
    ldr     p5, [x8, #5, mul vl]
    ldr     p6, [x8, #6, mul vl]
    ldr     p7, [x8, #7, mul vl]
    ldr     p8, [x8, #8, mul vl]
    ldr     p9, [x8, #9, mul vl]
    ldr     p10, [x8, #10, mul vl]
    ldr     p11, [x8, #11, mul vl]
    ldr     p12, [x8, #12, mul vl]
    ldr     p13, [x8, #13, mul vl]
    ldr     p14, [x8, #14, mul vl]
    ldr     p15, [x8, #16, mul vl]
    wrffr   p15.b
    ldr     p15, [x8, #15, mul vl]
#else
    ldp     q0,  q1,  [x2, #  0]
    ldp     q2,  q3,  [x2, # 32]
    ldp     q4,  q5,  [x2, # 64]
    ldp     q6,  q7,  [x2, # 96]
    ldp     q8,  q9,  [x2, #128]
    ldp     q10, q11, [x2, #160]
    ldp     q12, q13, [x2, #192]
    ldp     q14, q15, [x2, #224]
    ldp     q16, q17, [x2, #256]
    ldp     q18, q19, [x2, #288]
    ldp     q20, q21, [x2, #320]
    ldp     q22, q23, [x2, #352]
    ldp     q24, q25, [x2, #384]
    ldp     q26, q27, [x2, #416]
    ldp     q28, q29, [x2, #448]
    ldp     q30, q31, [x2, #480]
#endif /* defined(__ARM_FEATURE_SVE) */

    // Load GPRs from gpr_ref (x0).
    mov     x27, x0
    ldp     x1,  x2,  [x27, #  8]
    ldp     x3,  x4,  [x27, # 24]
    ldp     x5,  x6,  [x27, # 40]
    ldp     x7,  x9,  [x27, # 56]
    ldr     x10, [x27, # 80]
    ldr     x0,  [x27, #  0]
    ldr     x8,  [x27, # 64]
    ldp     x11, x12, [x27, # 88]
    ldp     x13, x14, [x27, #104]
    ldp     x15, x16, [x27, #120]
    ldp     x17, x18, [x27, #136]
    ldp     x19, x20, [x27, #152]
    ldp     x21, x22, [x27, #168]
    ldp     x23, x24, [x27, #184]
    ldp     x25, x26, [x27, #200]
    ldp     x29, x30, [x27, #232]
    ldp     x27, x28, [x27, #216]

    /* Save registers to be used in prctl syscall which sets Name to
     * "att_test_loop" in /proc/<PID>/status to tell the client.attach_state
     * test in runall.cmake the process is ready for attach.
     */
    str     x0,  [sp, #200]
    str     x1,  [sp, #208]
    str     x2,  [sp, #216]
    str     x3,  [sp, #224]
    str     x4,  [sp, #232]
    str     x5,  [sp, #240]
    str     x8,  [sp, #248]
    str     x10, [sp, #256]
#if defined(__ARM_FEATURE_SVE)
    addvl   sp, sp,  #-32
    str     z0,  [sp, #0, mul vl]
    str     z1,  [sp, #1, mul vl]
    str     z2,  [sp, #2, mul vl]
    str     z3,  [sp, #3, mul vl]
    str     z4,  [sp, #4, mul vl]
    str     z5,  [sp, #5, mul vl]
    str     z6,  [sp, #6, mul vl]
    str     z7,  [sp, #7, mul vl]
    str     z8,  [sp, #8, mul vl]
    str     z9,  [sp, #9, mul vl]
    str     z10, [sp, #10, mul vl]
    str     z11, [sp, #11, mul vl]
    str     z12, [sp, #12, mul vl]
    str     z13, [sp, #13, mul vl]
    str     z14, [sp, #14, mul vl]
    str     z15, [sp, #15, mul vl]
    str     z16, [sp, #16, mul vl]
    str     z17, [sp, #17, mul vl]
    str     z18, [sp, #18, mul vl]
    str     z19, [sp, #19, mul vl]
    str     z20, [sp, #20, mul vl]
    str     z21, [sp, #21, mul vl]
    str     z22, [sp, #22, mul vl]
    str     z23, [sp, #23, mul vl]
    str     z24, [sp, #24, mul vl]
    str     z25, [sp, #25, mul vl]
    str     z26, [sp, #26, mul vl]
    str     z27, [sp, #27, mul vl]
    str     z28, [sp, #28, mul vl]
    str     z29, [sp, #29, mul vl]
    str     z30, [sp, #30, mul vl]
    str     z31, [sp, #31, mul vl]
    addpl   sp, sp,  #-24
    str     p0,  [sp, #0, mul vl]
    str     p1,  [sp, #1, mul vl]
    str     p2,  [sp, #2, mul vl]
    str     p3,  [sp, #3, mul vl]
    str     p4,  [sp, #4, mul vl]
    str     p5,  [sp, #5, mul vl]
    str     p6,  [sp, #6, mul vl]
    str     p7,  [sp, #7, mul vl]
    str     p8,  [sp, #8, mul vl]
    str     p9,  [sp, #9, mul vl]
    str     p10, [sp, #10, mul vl]
    str     p11, [sp, #11, mul vl]
    str     p12, [sp, #12, mul vl]
    str     p13, [sp, #13, mul vl]
    str     p14, [sp, #14, mul vl]
    str     p15, [sp, #15, mul vl]
    rdffr   p15.b
    str     p15, [sp, #16, mul vl]
#endif

    adrp    x1, name_att_test_loop
    add     x1, x1, :lo12:name_att_test_loop
    mov     x0, #15   // PR_SET_NAME
    mov     x2, xzr
    mov     x3, xzr
    mov     x4, xzr
    mov     x5, xzr
    mov     x8, #167  // __NR_prctl
    svc     #0

    /* This sequence of loads is a race condition (if the client.attach_state
     * test in runall.cmake invokes an attach before the loads complete) but it
     * is minimal and will suffice for this test.
     */
#if defined(__ARM_FEATURE_SVE)
    ldr     p0,  [sp, #0, mul vl]
    ldr     p1,  [sp, #1, mul vl]
    ldr     p2,  [sp, #2, mul vl]
    ldr     p3,  [sp, #3, mul vl]
    ldr     p4,  [sp, #4, mul vl]
    ldr     p5,  [sp, #5, mul vl]
    ldr     p6,  [sp, #6, mul vl]
    ldr     p7,  [sp, #7, mul vl]
    ldr     p8,  [sp, #8, mul vl]
    ldr     p9,  [sp, #9, mul vl]
    ldr     p10, [sp, #10, mul vl]
    ldr     p11, [sp, #11, mul vl]
    ldr     p12, [sp, #12, mul vl]
    ldr     p13, [sp, #13, mul vl]
    ldr     p14, [sp, #14, mul vl]
    ldr     p15, [sp, #16, mul vl]
    wrffr   p15.b
    ldr     p15, [sp, #15, mul vl]
    addpl   sp, sp, #24
    ldr     z0,  [sp, #0, mul vl]
    ldr     z1,  [sp, #1, mul vl]
    ldr     z2,  [sp, #2, mul vl]
    ldr     z3,  [sp, #3, mul vl]
    ldr     z4,  [sp, #4, mul vl]
    ldr     z5,  [sp, #5, mul vl]
    ldr     z6,  [sp, #6, mul vl]
    ldr     z7,  [sp, #7, mul vl]
    ldr     z8,  [sp, #8, mul vl]
    ldr     z9,  [sp, #9, mul vl]
    ldr     z10, [sp, #10, mul vl]
    ldr     z11, [sp, #11, mul vl]
    ldr     z12, [sp, #12, mul vl]
    ldr     z13, [sp, #13, mul vl]
    ldr     z14, [sp, #14, mul vl]
    ldr     z15, [sp, #15, mul vl]
    ldr     z16, [sp, #16, mul vl]
    ldr     z17, [sp, #17, mul vl]
    ldr     z18, [sp, #18, mul vl]
    ldr     z19, [sp, #19, mul vl]
    ldr     z20, [sp, #20, mul vl]
    ldr     z21, [sp, #21, mul vl]
    ldr     z22, [sp, #22, mul vl]
    ldr     z23, [sp, #23, mul vl]
    ldr     z24, [sp, #24, mul vl]
    ldr     z25, [sp, #25, mul vl]
    ldr     z26, [sp, #26, mul vl]
    ldr     z27, [sp, #27, mul vl]
    ldr     z28, [sp, #28, mul vl]
    ldr     z29, [sp, #29, mul vl]
    ldr     z30, [sp, #30, mul vl]
    ldr     z31, [sp, #31, mul vl]
    addvl   sp, sp, #31 /* Maximum offset is 31. */
    addvl   sp, sp, #1
#endif
    ldr     x0,  [sp, #200]
    ldr     x1,  [sp, #208]
    ldr     x2,  [sp, #216]
    ldr     x3,  [sp, #224]
    ldr     x4,  [sp, #232]
    ldr     x5,  [sp, #240]
    ldr     x8,  [sp, #248]
    ldr     x10, [sp, #256]

    /* Loop during which the client.attach_state test in runall.cmake will
     * invoke an attach.
     */
    adrp    x9, loop_count
    ldr     x9, [x9, :lo12:loop_count]
1:  sub     x9, x9, #1
    cbnz    x9, 1b
    str     x9, [sp, #80]

    // Save post-attach GPRs to gpr_att.
    ldr     x9, [sp, #0]
    stp     x0,  x1,  [x9, #0]
    stp     x2,  x3,  [x9, #16]
    stp     x4,  x5,  [x9, #32]
    stp     x6,  x7,  [x9, #48]
    str     x10, [x9, #80]
    ldr     x10, [sp, #80]
    stp     x8,  x10, [x9, #64]
    stp     x11, x12, [x9, #88]
    stp     x13, x14, [x9, #104]
    stp     x15, x16, [x9, #120]
    stp     x17, x18, [x9, #136]
    stp     x19, x20, [x9, #152]
    stp     x21, x22, [x9, #168]
    stp     x23, x24, [x9, #184]
    stp     x25, x26, [x9, #200]
    stp     x27, x28, [x9, #216]
    stp     x29, x30, [x9, #232]

    // Save post-attach SIMDs to simd_att.
    ldr     x9, [sp, #8]
#if defined(__ARM_FEATURE_SVE)
    str     z0, [x9, #0, mul vl]
    str     z1, [x9, #1, mul vl]
    str     z2, [x9, #2, mul vl]
    str     z3, [x9, #3, mul vl]
    str     z4, [x9, #4, mul vl]
    str     z5, [x9, #5, mul vl]
    str     z6, [x9, #6, mul vl]
    str     z7, [x9, #7, mul vl]
    str     z8, [x9, #8, mul vl]
    str     z9, [x9, #9, mul vl]
    str     z10, [x9, #10, mul vl]
    str     z11, [x9, #11, mul vl]
    str     z12, [x9, #12, mul vl]
    str     z13, [x9, #13, mul vl]
    str     z14, [x9, #14, mul vl]
    str     z15, [x9, #15, mul vl]
    str     z16, [x9, #16, mul vl]
    str     z17, [x9, #17, mul vl]
    str     z18, [x9, #18, mul vl]
    str     z19, [x9, #19, mul vl]
    str     z20, [x9, #20, mul vl]
    str     z21, [x9, #21, mul vl]
    str     z22, [x9, #22, mul vl]
    str     z23, [x9, #23, mul vl]
    str     z24, [x9, #24, mul vl]
    str     z25, [x9, #25, mul vl]
    str     z26, [x9, #26, mul vl]
    str     z27, [x9, #27, mul vl]
    str     z28, [x9, #28, mul vl]
    str     z29, [x9, #29, mul vl]
    str     z30, [x9, #30, mul vl]
    str     z31, [x9, #31, mul vl]
    // Save post-attach predicates to pred_att.
    ldr     x9, [sp, #264]
    str     p0, [x9, #0, mul vl]
    str     p1, [x9, #1, mul vl]
    str     p2, [x9, #2, mul vl]
    str     p3, [x9, #3, mul vl]
    str     p4, [x9, #4, mul vl]
    str     p5, [x9, #5, mul vl]
    str     p6, [x9, #6, mul vl]
    str     p7, [x9, #7, mul vl]
    str     p8, [x9, #8, mul vl]
    str     p9, [x9, #9, mul vl]
    str     p10, [x9, #10, mul vl]
    str     p11, [x9, #11, mul vl]
    str     p12, [x9, #12, mul vl]
    str     p13, [x9, #13, mul vl]
    str     p14, [x9, #14, mul vl]
    str     p15, [x9, #15, mul vl]
    rdffr   p15.b
    str     p15, [x9, #16, mul vl]
#else
    stp     q0,  q1,  [x9, #  0]
    stp     q2,  q3,  [x9, # 32]
    stp     q4,  q5,  [x9, # 64]
    stp     q6,  q7,  [x9, # 96]
    stp     q8,  q9,  [x9, #128]
    stp     q10, q11, [x9, #160]
    stp     q12, q13, [x9, #192]
    stp     q14, q15, [x9, #224]
    stp     q16, q17, [x9, #256]
    stp     q18, q19, [x9, #288]
    stp     q20, q21, [x9, #320]
    stp     q22, q23, [x9, #352]
    stp     q24, q25, [x9, #384]
    stp     q26, q27, [x9, #416]
    stp     q28, q29, [x9, #448]
    stp     q30, q31, [x9, #480]
#endif /* defined(__ARM_FEATURE_SVE) */

    // Save post-attach FPCR/FPSR to fpcr_att/fpsr_att.
    ldr     x9, [sp, #16]
    mrs     x10, fpcr
    str     w10, [x9]
    ldr     x9, [sp, #24]
    mrs     x10, fpsr
    str     w10, [x9]

    // Save post-attach SP/PC/NZCV.
    ldr     x9,  [sp, #56]      // sp_att
    mov     x10, sp
    str     x10, [x9]

.global pc_after_label
pc_after_label:
    ldr     x9,  [sp, #64]      // pc_after
    adr     x10, pc_after_label
    str     x10, [x9]

    ldr     x9,  [sp, #72]      // nzcv_att
    mrs     x10, nzcv
    str     w10, [x9]

    // Restore callee-saved registers and LR.
    ldp     x19, x20, [sp, #96]
    ldp     x21, x22, [sp, #112]
    ldp     x23, x24, [sp, #128]
    ldp     x25, x26, [sp, #144]
    ldp     x27, x28, [sp, #160]
    ldp     x29, x30, [sp, #176]

    add     sp, sp, #288
    ret
.size attach_state_test, .-attach_state_test

.section .rodata
.align 3
name_att_test_loop:
    .asciz "att_test_loop"
.previous
