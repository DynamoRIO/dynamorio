/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
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

#ifdef X86
#    define LOOP_TEST_REG_INNER DR_REG_XBX
#    define LOOP_TEST_REG_INNER_ASM REG_XBX
#    define LOOP_TEST_REG_INNER_SIG SC_XBX
#    define LOOP_TEST_REG_OUTER DR_REG_XBP
#    define LOOP_TEST_REG_OUTER_ASM REG_XBP
#    define LOOP_TEST_REG_OUTER_SIG SC_XBP
#    define TEST_1_LOOP_COUNT_REG DR_REG_XCX
#    define TEST_1_LOOP_COUNT_REG_ASM REG_XCX
#    define TEST_1_LOOP_COUNT_REG_SIG SC_XCX
#    define TEST_1_LOOP_COUNT_REG_MC mc.xcx
#    define TEST_2_LOOP_COUNT_REG DR_REG_XAX
#    define TEST_2_LOOP_COUNT_REG_ASM REG_XAX
#    define TEST_2_LOOP_COUNT_REG_SIG SC_XAX
#    define TEST_2_LOOP_COUNT_REG_MC mc.xax
#    define TEST_2_CHECK_REG DR_REG_XCX
#    define TEST_2_CHECK_REG_ASM REG_XCX
#    define TEST_2_CHECK_REG_SIG SC_XCX
#    define TEST_2_CHECK_REG_MC mc.xcx
#    define SUSPEND_TEST_REG DR_REG_XDX
#    define SUSPEND_TEST_REG_ASM REG_XDX
#    define SUSPEND_TEST_REG_SIG SC_XDX
#endif

#define LOOP_COUNT_INNER 1000
#define LOOP_COUNT_OUTER 100000

#define MAKE_HEX_ASM(n) HEX(n)
#define MAKE_HEX(n) 0x##n
#define MAKE_HEX_C(n) MAKE_HEX(n)

/* Immediates that we look for in the app code to identify places for
 * specific tests in the client.
 * We limit to 16 bits to work on ARM.
 */
#define TEST_CONST(num) f1f##num

#define TEST_VAL MAKE_HEX_ASM(TEST_CONST(1))
#define TEST_VAL_C MAKE_HEX_C(TEST_CONST(1))

#define SUSPEND_VAL_TEST_1 MAKE_HEX_ASM(TEST_CONST(2))
#define SUSPEND_VAL_TEST_1_C MAKE_HEX_C(TEST_CONST(2))

#define SUSPEND_VAL_TEST_2 MAKE_HEX_ASM(TEST_CONST(3))
#define SUSPEND_VAL_TEST_2_C MAKE_HEX_C(TEST_CONST(3))
