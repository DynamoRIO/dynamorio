/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2025 Arm Limited.       All rights reserved.
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

/* An app that checks registers after an attach to test contents have not
 * changed inadvertantly due to the attach process. Currently this is for
 * AArch64 only.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

/* This is the main test function. Implemented as assembly in
 * attach_state_aarch64.asm, it sets registers with reference values before
 * sitting in a loop waiting for the attach. The attach is synchronised using
 * the "Name:" field of /proc/<PID>/status which is set just before the loop
 * starts using the prctl syscall. This status is polled by runall.cmake which
 * calls the client.attach_state test which attaches during the loop. When the
 * loop ends, the contents of registers are checked against expected reference
 * values.
 */
void
attach_state_test(const uint64_t *gpr_ref, const __uint128_t *simd_ref, uint64_t *gpr_att,
                  __uint128_t *simd_att, uint32_t *fpcr_att, uint32_t *fpsr_att,
                  uint64_t *sp_ref, uint64_t *pc_before, uint32_t *nzcv_ref,
                  uint64_t *sp_att, uint64_t *pc_after, uint32_t *nzcv_att);

/* Register reference data. i#7577: SVE registers to be added. */
static uint64_t gpr_ref[31];
static __uint128_t simd_ref[32] __attribute__((aligned(16)));

/* Non-zero reference values for status registers. */
static const uint32_t nzcv_ref = 0x60000000; /* N=0 Z=1 C=1 V=0                */
static const uint32_t fpcr_ref = 0x03400000; /* DN=1 FZ=1 RMode=+Inf (RP mode) */
static const uint32_t fpsr_ref = 0x08000001; /* QC=1 IOC=1                     */

/* Theses are set in attach_state_test() before attaching using ADR. The PC test is a
 * sanity check based on the label address values, rather than a comparison of
 * machine state before and after attaching.
 */
extern char pc_before_label;
extern char pc_after_label;

/* This sets a period of time during which the app loops waiting for an attach. */
volatile uint64_t loop_count = 15ull * 1024ull * 1024ull * 1024ull;
/* x9 is used as the loop counter and will not be used to test attach state.   */
enum { GPR_SKIP = 9 };

/* Reference data checking functions and helpers. */
static int
check_gprs(const uint64_t *reg)
{
    for (int i = 0; i < 31; i++) {
        if (i == GPR_SKIP)
            continue;
        if (reg[i] != gpr_ref[i]) {
            printf("GPR mismatch x%-2d: expected 0x%016llx, got 0x%016llx\n", i,
                   (unsigned long long)gpr_ref[i], (unsigned long long)reg[i]);
            return -1;
        }
    }
    return 0;
}

static inline void
u128_to_2xu64(__uint128_t v, uint64_t *hi, uint64_t *lo)
{
    *lo = (uint64_t)v;
    *hi = (uint64_t)(v >> 64);
}

static int
check_simd(const __uint128_t *reg)
{
    for (int i = 0; i < 32; i++) {
        if (reg[i] != simd_ref[i]) {
            unsigned long long got_hi, got_lo, exp_hi, exp_lo;
            u128_to_2xu64(reg[i], &got_hi, &got_lo);
            u128_to_2xu64(simd_ref[i], &exp_hi, &exp_lo);
            printf(
                "SIMD mismatch v%-2d: expected 0x%016llx%016llx, got 0x%016llx%016llx\n",
                i, exp_hi, exp_lo, got_hi, got_lo);
            return -1;
        }
    }
    return 0;
}

#define UNSET_PASS_IF(cond, ...) \
    do {                         \
        if ((cond)) {            \
            printf(__VA_ARGS__); \
            pass = 0;            \
        }                        \
    } while (0)

int
main(void)
{
    /* Register values after the attach to be checked against _ref values. */
    uint64_t gpr_att[31];
    __uint128_t simd_att[32] __attribute__((aligned(16)));
    uint32_t fpcr_att = 0, fpsr_att = 0;

    uint32_t nzcv_ref = 0, nzcv_att = 0;
    uint64_t sp_ref = 0, sp_att = 0;
    uint64_t pc_before = 0, pc_after = 0;

    setbuf(stdout, NULL);
    printf("starting\n");

    /* Set GPR and SIMD reference data. */
    const uint64_t base = 0x0123456789abcdefULL;
    for (int i = 0; i < 31; i++)
        gpr_ref[i] = base + (uint64_t)i;

    for (int i = 0; i < 32; i++) {
        uint64_t lo = 0xaaaabbbbccccddddULL + (uint64_t)i;
        uint64_t hi = 0x111122223333aaaaULL ^ (uint64_t)(31 - i);
        simd_ref[i] = (((__uint128_t)hi) << 64) | lo;
    }

    /* Useful for manual attach testing. */
    printf("PID: %ld\n", (long)getpid());

    attach_state_test(gpr_ref, simd_ref, gpr_att, simd_att, &fpcr_att, &fpsr_att, &sp_ref,
                      &pc_before, &nzcv_ref, &sp_att, &pc_after, &nzcv_att);

    int pass = 1;

    if (check_gprs(gpr_att) != 0)
        pass = 0;
    if (check_simd(simd_att) != 0)
        pass = 0;

    UNSET_PASS_IF(nzcv_att != nzcv_ref, "NZCV mismatch: expected 0x%08x, got 0x%08x\n",
                  nzcv_ref, nzcv_att);

    UNSET_PASS_IF(fpcr_att != fpcr_ref, "FPCR mismatch: expected 0x%08x, got 0x%08x\n",
                  fpcr_ref, fpcr_att);

    UNSET_PASS_IF(fpsr_att != fpsr_ref, "FPSR mismatch: expected 0x%08x, got 0x%08x\n",
                  fpsr_ref, fpsr_att);

    UNSET_PASS_IF(sp_att != sp_ref, "SP changed: expected 0x%016llx, got 0x%016llx\n",
                  (unsigned long long)sp_ref, (unsigned long long)sp_att);

    UNSET_PASS_IF((sp_att & 0xF) != 0, "SP is not 16-byte aligned: 0x%016llx\n",
                  (unsigned long long)sp_att);

    UNSET_PASS_IF(pc_before != (uint64_t)(uintptr_t)&pc_before_label,
                  "PC(before) mismatch: expected 0x%016llx, got 0x%016llx\n",
                  (unsigned long long)(uintptr_t)&pc_before_label,
                  (unsigned long long)pc_before);

    UNSET_PASS_IF(pc_after != (uint64_t)(uintptr_t)&pc_after_label,
                  "PC(after) mismatch: expected 0x%016llx, got 0x%016llx\n",
                  (unsigned long long)(uintptr_t)&pc_after_label,
                  (unsigned long long)pc_after);

    if (pass) {
        printf("done\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
