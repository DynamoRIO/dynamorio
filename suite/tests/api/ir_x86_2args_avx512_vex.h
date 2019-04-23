/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

/* AVX-512 VEX scalar opmask instructions */
OPCODE(kmovw_kk, kmovw, kmovw, 0, REGARG(K0), REGARG(K1))
OPCODE(kmovb_kk, kmovb, kmovb, 0, REGARG(K2), REGARG(K3))
OPCODE(kmovq_kk, kmovq, kmovq, 0, REGARG(K4), REGARG(K5))
OPCODE(kmovd_kk, kmovd, kmovd, 0, REGARG(K6), REGARG(K7))
OPCODE(kmovw_km, kmovw, kmovw, 0, REGARG(K0), MEMARG(OPSZ_2))
OPCODE(kmovb_km, kmovb, kmovb, 0, REGARG(K1), MEMARG(OPSZ_1))
OPCODE(kmovq_km, kmovq, kmovq, 0, REGARG(K2), MEMARG(OPSZ_8))
OPCODE(kmovd_km, kmovd, kmovd, 0, REGARG(K3), MEMARG(OPSZ_4))
OPCODE(kmovw_mk, kmovw, kmovw, 0, MEMARG(OPSZ_2), REGARG(K4))
OPCODE(kmovb_mk, kmovb, kmovb, 0, MEMARG(OPSZ_1), REGARG(K5))
OPCODE(kmovq_mk, kmovq, kmovq, 0, MEMARG(OPSZ_8), REGARG(K6))
OPCODE(kmovd_mk, kmovd, kmovd, 0, MEMARG(OPSZ_4), REGARG(K7))
OPCODE(kmovw_kr, kmovw, kmovw, 0, REGARG(K0), REGARG(EAX))
OPCODE(kmovb_kr, kmovb, kmovb, 0, REGARG(K1), REGARG(EBX))
OPCODE(kmovq_kr, kmovq, kmovq, X64_ONLY, REGARG(K2), REGARG(RCX))
OPCODE(kmovd_kr, kmovd, kmovd, 0, REGARG(K3), REGARG(EDX))
OPCODE(kmovw_rk, kmovw, kmovw, 0, REGARG(ESI), REGARG(K4))
OPCODE(kmovb_rk, kmovb, kmovb, 0, REGARG(EDI), REGARG(K5))
OPCODE(kmovq_rk, kmovq, kmovq, X64_ONLY, REGARG(RAX), REGARG(K6))
OPCODE(kmovd_rk, kmovd, kmovd, 0, REGARG(EBX), REGARG(K7))
OPCODE(knotw_kk, knotw, knotw, 0, REGARG(K0), REGARG(K1))
OPCODE(knotb_kk, knotb, knotb, 0, REGARG(K2), REGARG(K3))
OPCODE(knotq_kk, knotq, knotq, 0, REGARG(K4), REGARG(K5))
OPCODE(knotd_kk, knotd, knotd, 0, REGARG(K6), REGARG(K7))
OPCODE(kortestw_kk, kortestw, kortestw, 0, REGARG(K0), REGARG(K1))
OPCODE(kortestb_kk, kortestb, kortestb, 0, REGARG(K2), REGARG(K3))
OPCODE(kortestq_kk, kortestq, kortestq, 0, REGARG(K4), REGARG(K5))
OPCODE(kortestd_kk, kortestd, kortestd, 0, REGARG(K6), REGARG(K7))
OPCODE(ktestw_kk, ktestw, ktestw, 0, REGARG(K0), REGARG(K1))
OPCODE(ktestb_kk, ktestb, ktestb, 0, REGARG(K2), REGARG(K3))
OPCODE(ktestq_kk, ktestq, ktestq, 0, REGARG(K4), REGARG(K5))
OPCODE(ktestd_kk, ktestd, ktestd, 0, REGARG(K6), REGARG(K7))
