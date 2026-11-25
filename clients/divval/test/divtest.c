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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Test application for divval.  Executes UDIV/SDIV with known operands,
 * including the corner cases that the instrumentation must handle (W forms
 * with garbage in the upper half of the X register, the DR stolen register
 * x28, the zero register, and the same register as both operands), followed
 * by enough divides to wrap the client's buffer many times.
 *
 * Writes the expected sequence (op,width,dividend,divisor) to argv[1];
 * test/check.py compares it against the client's -trace output.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *expected;
static uint64_t checksum;

static void
expect(int is_signed, int width, uint64_t n, uint64_t d)
{
    if (width == 32) {
        if (is_signed)
            fprintf(expected, "sdiv,32,%d,%d\n", (int32_t)n, (int32_t)d);
        else
            fprintf(expected, "udiv,32,%u,%u\n", (uint32_t)n, (uint32_t)d);
    } else {
        if (is_signed)
            fprintf(expected, "sdiv,64,%" PRId64 ",%" PRId64 "\n", (int64_t)n,
                    (int64_t)d);
        else
            fprintf(expected, "udiv,64,%" PRIu64 ",%" PRIu64 "\n", n, d);
    }
}

/* Each helper executes exactly one divide.  The operands are passed as full
 * 64-bit values so that the W forms see whatever is in the upper half.
 */
#define DEFINE_DIV(name, insn, mod)                              \
    static uint64_t name(uint64_t n, uint64_t d)                 \
    {                                                            \
        uint64_t q;                                              \
        __asm__ volatile(insn " %" mod "0, %" mod "1, %" mod "2" \
                         : "=r"(q)                               \
                         : "r"(n), "r"(d));                      \
        return q;                                                \
    }
DEFINE_DIV(udiv32, "udiv", "w")
DEFINE_DIV(udiv64, "udiv", "x")
DEFINE_DIV(sdiv32, "sdiv", "w")
DEFINE_DIV(sdiv64, "sdiv", "x")

static void
run(int is_signed, int width, uint64_t n, uint64_t d)
{
    uint64_t q;
    if (is_signed)
        q = width == 32 ? sdiv32(n, d) : sdiv64(n, d);
    else
        q = width == 32 ? udiv32(n, d) : udiv64(n, d);
    checksum = checksum * 31 + q;
    expect(is_signed, width, n, d);
}

static void
run_all(uint64_t n, uint64_t d)
{
    run(0, 32, n, d);
    run(0, 64, n, d);
    run(1, 32, n, d);
    run(1, 64, n, d);
}

static void
corner_register_cases(void)
{
    uint64_t q, n = 1000003, d = 17;
    /* x28 is DR's stolen register on AArch64. */
    __asm__ volatile("mov x28, %1\n\tudiv %0, x28, %2"
                     : "=r"(q)
                     : "r"(n), "r"(d)
                     : "x28");
    checksum += q;
    expect(0, 64, n, d);
    __asm__ volatile("mov x28, %2\n\tsdiv %w0, %w1, w28"
                     : "=r"(q)
                     : "r"(n), "r"(d)
                     : "x28");
    checksum += q;
    expect(1, 32, n, d);
    /* Zero register as dividend and as divisor. */
    __asm__ volatile("udiv %0, xzr, %1" : "=r"(q) : "r"(d));
    checksum += q;
    expect(0, 64, 0, d);
    __asm__ volatile("sdiv %w0, %w1, wzr" : "=r"(q) : "r"(n));
    checksum += q;
    expect(1, 32, n, 0);
    /* Same register for both operands. */
    __asm__ volatile("udiv %0, %1, %1" : "=r"(q) : "r"(n));
    checksum += q;
    expect(0, 64, n, n);
    /* Destination is also a source. */
    q = n;
    __asm__ volatile("sdiv %0, %0, %1" : "+r"(q) : "r"(d));
    checksum += q;
    expect(1, 64, n, d);
}

static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;

static uint64_t
xorshift64(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng_state = x;
}

int
main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <expected.csv>\n", argv[0]);
        return 1;
    }
    expected = fopen(argv[1], "w");
    if (expected == NULL) {
        perror("fopen");
        return 1;
    }
    fprintf(expected, "op,width,dividend,divisor\n");

    static const uint64_t corner[][2] = {
        { 100, 7 },
        { 0, 5 },
        { 5, 0 },
        { 7, 100 },
        { (uint64_t)-100, 7 },
        { 100, (uint64_t)-7 },
        { (uint64_t)-100, (uint64_t)-7 },
        { 0x80000000ULL, (uint64_t)-1 },         /* INT32_MIN / -1 (W forms). */
        { 0x8000000000000000ULL, (uint64_t)-1 }, /* INT64_MIN / -1. */
        { UINT64_MAX, 1 },
        { UINT64_MAX, 2 },
        { 1ULL << 40, 3 },
        /* Garbage in the upper half: W forms must only see the low half. */
        { 0xdeadbeef00000064ULL, 0xcafef00d00000007ULL },
        { 0x12345678fffffff6ULL, 0x8765432100000003ULL },
    };
    for (size_t i = 0; i < sizeof(corner) / sizeof(corner[0]); i++)
        run_all(corner[i][0], corner[i][1]);
    corner_register_cases();

    /* Enough divides to fill the client's buffer many times, with operand
     * magnitudes spread over the whole range.
     */
    for (int i = 0; i < 50000; i++) {
        uint64_t n = xorshift64() >> (xorshift64() & 63);
        uint64_t d = xorshift64() >> (xorshift64() & 63);
        if ((i & 7) == 0)
            d = 1ULL << (i & 31); /* Some powers of 2. */
        if ((i & 3) == 1) {       /* Some negative operands. */
            n = -n;
            d = d & 1 ? -d : d;
        }
        run_all(n, d);
    }

    fclose(expected);
    printf("checksum %" PRIx64 "\n", checksum);
    return 0;
}
