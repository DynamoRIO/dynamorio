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

#ifndef _DETACH_STATE_SHARED_H_
#define _DETACH_STATE_SHARED_H_ 1

#define MAKE_HEX_ASM(n) HEX(n)
#define MAKE_HEX(n) 0x##n
#define MAKE_HEX_C(n) MAKE_HEX(n)

#ifdef X64
#    define XAX_BASE() 12345678abcdef01
#    define XCX_BASE() 2345678abcdef012
#    define XDX_BASE() 345678abcdef0123
#    define XBX_BASE() 45678abcdef01234
#    define XBP_BASE() 5678abcdef012345
#    define XSI_BASE() 678abcdef0123456
#    define XDI_BASE() 78abcdef01234567
#    define R8_BASE() 1234567890abcdef
#    define R9_BASE() f1234567890abcde
#    define R10_BASE() ef1234567890abcd
#    define R11_BASE() def1234567890abc
#    define R12_BASE() cdef1234567890ab
#    define R13_BASE() bcdef1234567890a
#    define R14_BASE() abcdef1234567890
#    define R15_BASE() 0abcdef123456789
#    define XMM0_LOW_BASE() 2384626433832795
#    define XMM0_HIGH_BASE() 3141592653589793
#    define XMM1_LOW_BASE() 1384626433832795
#    define XMM1_HIGH_BASE() 1141592653589793
#    define XMM2_LOW_BASE() 2384626433832795
#    define XMM2_HIGH_BASE() 2141592653589793
#    define XMM3_LOW_BASE() 3384626433832795
#    define XMM3_HIGH_BASE() 3141592653589793
#    define XMM4_LOW_BASE() 4384626433832795
#    define XMM4_HIGH_BASE() 4141592653589793
#    define XMM5_LOW_BASE() 5384626433832795
#    define XMM5_HIGH_BASE() 5141592653589793
#    define XMM6_LOW_BASE() 6384626433832795
#    define XMM6_HIGH_BASE() 6141592653589793
#    define XMM7_LOW_BASE() 7384626433832795
#    define XMM7_HIGH_BASE() 7141592653589793
#    define XMM8_LOW_BASE() 8384626433832795
#    define XMM8_HIGH_BASE() 8141592653589793
#    define XMM9_LOW_BASE() 9384626433832795
#    define XMM9_HIGH_BASE() 9141592653589793
#    define XMM10_LOW_BASE() a384626433832795
#    define XMM10_HIGH_BASE() a141592653589793
#    define XMM11_LOW_BASE() b384626433832795
#    define XMM11_HIGH_BASE() b141592653589793
#    define XMM12_LOW_BASE() c384626433832795
#    define XMM12_HIGH_BASE() c141592653589793
#    define XMM13_LOW_BASE() d384626433832795
#    define XMM13_HIGH_BASE() d141592653589793
#    define XMM14_LOW_BASE() e384626433832795
#    define XMM14_HIGH_BASE() e141592653589793
#    define XMM15_LOW_BASE() f384626433832795
#    define XMM15_HIGH_BASE() f141592653589793
#else
#    error NYI
#    define XAX_BASE() 12345678
#    define XCX_BASE() 23456780
#    define XDX_BASE() 34567801
#    define XBX_BASE() 45678012
#    define XBP_BASE() 56780123
#    define XSI_BASE() 67801234
#    define XDI_BASE() 78012345
#endif
#define XFLAGS_BASE() 00000ed7

#endif /* _DETACH_STATE_SHARED_H_ */
