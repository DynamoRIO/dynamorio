/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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
# define TEST_REG DR_REG_XDX
# define TEST_REG_ASM REG_XDX
# define TEST_REG_ASM_LSB dl
# define TEST_REG_CXT IF_X64_ELSE(Rdx, Edx)
# define TEST_REG_SIG SC_XDX
#endif

#ifdef ARM
# define TEST_REG DR_REG_R12
# define TEST_REG_ASM r12
# define TEST_REG_SIG arm_ip
#endif

#ifdef AARCH64
# define TEST_REG DR_REG_X4
# define TEST_REG_ASM x4
# define TEST_REG_SIG regs[4]
#endif

#define TEST_FLAGS_SIG SC_XFLAGS

/* Immediates that we look for in the app code to identify places for
 * specific tests in the client.
 * We limit to 16 bits to work on ARM.
 */
#define DRREG_TEST_CONST(num) f1f##num
#define MAKE_HEX_ASM(n) HEX(n)
#define MAKE_HEX(n) 0x##n
#define MAKE_HEX_C(n) MAKE_HEX(n)

#ifdef X86
/* Set SF,ZF,AF,PF,CF, and bit 1 is always 1 => 0xd7 */
# define DRREG_TEST_AFLAGS_ASM MAKE_HEX_ASM(d7)
# define DRREG_TEST_AFLAGS_C MAKE_HEX(d7)
#endif

#ifdef ARM
/* Set N,Z,C,V,Q => 0xf8000000, and LSB is really MSB */
# define DRREG_TEST_AFLAGS_ASM MAKE_HEX_ASM(f8000000)
# define DRREG_TEST_AFLAGS_C MAKE_HEX(f8000000)
#endif

#ifdef AARCH64
# define DRREG_TEST_AFLAGS_H_ASM MAKE_HEX_ASM(f000)
# define DRREG_TEST_AFLAGS_ASM MAKE_HEX_ASM(f0000000)
# define DRREG_TEST_AFLAGS_C MAKE_HEX(f0000000)
#endif

#define DRREG_TEST_1_ASM MAKE_HEX_ASM(DRREG_TEST_CONST(1))
#define DRREG_TEST_1_C   MAKE_HEX_C(DRREG_TEST_CONST(1))

#define DRREG_TEST_2_ASM MAKE_HEX_ASM(DRREG_TEST_CONST(2))
#define DRREG_TEST_2_C   MAKE_HEX_C(DRREG_TEST_CONST(2))

#define DRREG_TEST_3_ASM MAKE_HEX_ASM(DRREG_TEST_CONST(3))
#define DRREG_TEST_3_C   MAKE_HEX_C(DRREG_TEST_CONST(3))

#define DRREG_TEST_4_ASM MAKE_HEX_ASM(DRREG_TEST_CONST(4))
#define DRREG_TEST_4_C   MAKE_HEX_C(DRREG_TEST_CONST(4))

#define DRREG_TEST_5_ASM MAKE_HEX_ASM(DRREG_TEST_CONST(5))
#define DRREG_TEST_5_C   MAKE_HEX_C(DRREG_TEST_CONST(5))
