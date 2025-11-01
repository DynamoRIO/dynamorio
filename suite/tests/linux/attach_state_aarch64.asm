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
 *                        uint32_t *nzcv_att);             SP + 24
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
     */
    sub     sp, sp, #288
    stp     x2, x3, [sp, #0]    // gpr_att, simd_att
    stp     x4, x5, [sp, #16]   // fpcr_att, fpsr_att
    stp     x6, x7, [sp, #32]   // sp_ref, pc_before

    stp     x13, x14, [sp, #48] // nzcv_ref, sp_att
    stp     x15, x16, [sp, #64] // pc_after, nzcv_att
    str     x0,  [sp, #192]     // Save gpr_ref

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
