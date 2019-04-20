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
OPCODE(kandw_kkk, kandw, kandw, 0, REGARG(K0), REGARG(K1), REGARG(K2))
OPCODE(kandb_kkk, kandb, kandb, 0, REGARG(K3), REGARG(K4), REGARG(K5))
OPCODE(kandq_kkk, kandq, kandq, 0, REGARG(K6), REGARG(K7), REGARG(K0))
OPCODE(kandd_kkk, kandd, kandd, 0, REGARG(K1), REGARG(K2), REGARG(K3))
OPCODE(kandnw_kkk, kandnw, kandnw, 0, REGARG(K0), REGARG(K1), REGARG(K2))
OPCODE(kandnb_kkk, kandnb, kandnb, 0, REGARG(K3), REGARG(K4), REGARG(K5))
OPCODE(kandnq_kkk, kandnq, kandnq, 0, REGARG(K6), REGARG(K7), REGARG(K0))
OPCODE(kandnd_kkk, kandnd, kandnd, 0, REGARG(K1), REGARG(K2), REGARG(K3))
OPCODE(kunpckbw_kkk, kunpckbw, kunpckbw, 0, REGARG(K4), REGARG(K5), REGARG(K6))
OPCODE(kunpckwd_kkk, kunpckwd, kunpckwd, 0, REGARG(K7), REGARG(K0), REGARG(K1))
OPCODE(kunpckdq_kkk, kunpckdq, kunpckdq, 0, REGARG(K2), REGARG(K3), REGARG(K4))
OPCODE(korw_kkk, korw, korw, 0, REGARG(K0), REGARG(K1), REGARG(K2))
OPCODE(korb_kkk, korb, korb, 0, REGARG(K3), REGARG(K4), REGARG(K5))
OPCODE(korq_kkk, korq, korq, 0, REGARG(K6), REGARG(K7), REGARG(K0))
OPCODE(kord_kkk, kord, kord, 0, REGARG(K1), REGARG(K2), REGARG(K3))
OPCODE(kxnorw_kkk, kxnorw, kxnorw, 0, REGARG(K0), REGARG(K1), REGARG(K2))
OPCODE(kxnorb_kkk, kxnorb, kxnorb, 0, REGARG(K3), REGARG(K4), REGARG(K5))
OPCODE(kxnorq_kkk, kxnorq, kxnorq, 0, REGARG(K6), REGARG(K7), REGARG(K0))
OPCODE(kxnord_kkk, kxnord, kxnord, 0, REGARG(K1), REGARG(K2), REGARG(K3))
OPCODE(kxorw_kkk, kxorw, kxorw, 0, REGARG(K0), REGARG(K1), REGARG(K2))
OPCODE(kxorb_kkk, kxorb, kxorb, 0, REGARG(K3), REGARG(K4), REGARG(K5))
OPCODE(kxorq_kkk, kxorq, kxorq, 0, REGARG(K6), REGARG(K7), REGARG(K0))
OPCODE(kxord_kkk, kxord, kxord, 0, REGARG(K1), REGARG(K2), REGARG(K3))
OPCODE(kaddw_kkk, kaddw, kaddw, 0, REGARG(K0), REGARG(K1), REGARG(K2))
OPCODE(kaddb_kkk, kaddb, kaddb, 0, REGARG(K3), REGARG(K4), REGARG(K5))
OPCODE(kaddq_kkk, kaddq, kaddq, 0, REGARG(K6), REGARG(K7), REGARG(K0))
OPCODE(kaddd_kkk, kaddd, kaddd, 0, REGARG(K1), REGARG(K2), REGARG(K3))
OPCODE(kshiftlw_kki, kshiftlw, kshiftlw, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftlb_kki, kshiftlb, kshiftlb, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftlq_kki, kshiftlq, kshiftlq, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftld_kki, kshiftld, kshiftld, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftrw_kki, kshiftrw, kshiftrw, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftrb_kki, kshiftrb, kshiftrb, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftrq_kki, kshiftrq, kshiftrq, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
OPCODE(kshiftrd_kki, kshiftrd, kshiftrd, 0, REGARG(K0), REGARG(K1), IMMARG(OPSZ_1))
