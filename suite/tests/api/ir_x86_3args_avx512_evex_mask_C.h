/* **********************************************************
 * Copyright (c) 2019-2025 Google, Inc.  All rights reserved.
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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 1 source. */
OPCODE(vpconflictd_xlok0ld, vpconflictd, vpconflictd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpconflictd_xlok0bcst, vpconflictd, vpconflictd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpconflictd_xlok0xlo, vpconflictd, vpconflictd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vpconflictd_xhik7xhi, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vpconflictd_xhik7ld, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vpconflictd_xhik7bcst, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vpconflictd_ylok0ld, vpconflictd, vpconflictd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpconflictd_ylok0bcst, vpconflictd, vpconflictd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpconflictd_ylok0ylo, vpconflictd, vpconflictd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vpconflictd_yhik7yhi, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vpconflictd_yhik7ld, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vpconflictd_yhik7bcst, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vpconflictd_zlok0ld, vpconflictd, vpconflictd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vpconflictd_zlok0bcst, vpconflictd, vpconflictd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpconflictd_zlok0zlo, vpconflictd, vpconflictd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vpconflictd_zhik7zhi, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vpconflictd_zhik7ld, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vpconflictd_zhik7bcst, vpconflictd, vpconflictd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vpconflictq_xlok0ld, vpconflictq, vpconflictq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpconflictq_xlok0bcst, vpconflictq, vpconflictq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpconflictq_xlok0xlo, vpconflictq, vpconflictq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vpconflictq_xhik7xhi, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vpconflictq_xhik7ld, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vpconflictq_xhik7bcst, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vpconflictq_ylok0ld, vpconflictq, vpconflictq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpconflictq_ylok0bcst, vpconflictq, vpconflictq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpconflictq_ylok0ylo, vpconflictq, vpconflictq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vpconflictq_yhik7yhi, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vpconflictq_yhik7ld, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vpconflictq_yhik7bcst, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vpconflictq_zlok0ld, vpconflictq, vpconflictq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vpconflictq_zlok0bcst, vpconflictq, vpconflictq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpconflictq_zlok0zlo, vpconflictq, vpconflictq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vpconflictq_zhik7zhi, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vpconflictq_zhik7ld, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vpconflictq_zhik7bcst, vpconflictq, vpconflictq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vplzcntd_xlok0ld, vplzcntd, vplzcntd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vplzcntd_xlok0bcst, vplzcntd, vplzcntd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vplzcntd_xlok0xlo, vplzcntd, vplzcntd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vplzcntd_xhik7xhi, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vplzcntd_xhik7ld, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vplzcntd_xhik7bcst, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vplzcntd_ylok0ld, vplzcntd, vplzcntd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vplzcntd_ylok0bcst, vplzcntd, vplzcntd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vplzcntd_ylok0ylo, vplzcntd, vplzcntd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vplzcntd_yhik7yhi, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vplzcntd_yhik7ld, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vplzcntd_yhik7bcst, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vplzcntd_zlok0ld, vplzcntd, vplzcntd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vplzcntd_zlok0bcst, vplzcntd, vplzcntd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vplzcntd_zlok0zlo, vplzcntd, vplzcntd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vplzcntd_zhik7zhi, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vplzcntd_zhik7ld, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vplzcntd_zhik7bcst, vplzcntd, vplzcntd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vplzcntq_xlok0ld, vplzcntq, vplzcntq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vplzcntq_xlok0bcst, vplzcntq, vplzcntq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vplzcntq_xlok0xlo, vplzcntq, vplzcntq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vplzcntq_xhik7xhi, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vplzcntq_xhik7ld, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vplzcntq_xhik7bcst, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vplzcntq_ylok0ld, vplzcntq, vplzcntq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vplzcntq_ylok0bcst, vplzcntq, vplzcntq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vplzcntq_ylok0ylo, vplzcntq, vplzcntq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vplzcntq_yhik7yhi, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vplzcntq_yhik7ld, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vplzcntq_yhik7bcst, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vplzcntq_zlok0ld, vplzcntq, vplzcntq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vplzcntq_zlok0bcst, vplzcntq, vplzcntq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vplzcntq_zlok0zlo, vplzcntq, vplzcntq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vplzcntq_zhik7zhi, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vplzcntq_zhik7ld, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vplzcntq_zhik7bcst, vplzcntq, vplzcntq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpgatherdd_xlovsibk1, vpgatherdd, vpgatherdd_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_4))
OPCODE(vpgatherdd_xhivsibk7, vpgatherdd, vpgatherdd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_4))
OPCODE(vpgatherdd_ylovsibk1, vpgatherdd, vpgatherdd_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_4))
OPCODE(vpgatherdd_yhivsibk7, vpgatherdd, vpgatherdd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_4))
OPCODE(vpgatherdd_zlovsibk1, vpgatherdd, vpgatherdd_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vpgatherdd_zhivsibk7, vpgatherdd, vpgatherdd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_4))
OPCODE(vpgatherdq_xlovsibk1, vpgatherdq, vpgatherdq_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_8))
OPCODE(vpgatherdq_xhivsibk7, vpgatherdq, vpgatherdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_8))
OPCODE(vpgatherdq_ylovsibk1, vpgatherdq, vpgatherdq_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vpgatherdq_yhivsibk7, vpgatherdq, vpgatherdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_8))
OPCODE(vpgatherdq_zlovsibk1, vpgatherdq, vpgatherdq_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vpgatherdq_zhivsibk7, vpgatherdq, vpgatherdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_8))
OPCODE(vpgatherqd_xlovsibk1, vpgatherqd, vpgatherqd_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_4))
OPCODE(vpgatherqd_xhivsibk7, vpgatherqd, vpgatherqd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_4))
OPCODE(vpgatherqd_ylovsibk1, vpgatherqd, vpgatherqd_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_4))
OPCODE(vpgatherqd_yhivsibk7, vpgatherqd, vpgatherqd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_4))
OPCODE(vpgatherqd_zlovsibk1, vpgatherqd, vpgatherqd_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vpgatherqd_zhivsibk7, vpgatherqd, vpgatherqd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_4))
OPCODE(vpgatherqq_xlovsibk1, vpgatherqq, vpgatherqq_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_8))
OPCODE(vpgatherqq_xhivsibk7, vpgatherqq, vpgatherqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_8))
OPCODE(vpgatherqq_ylovsibk1, vpgatherqq, vpgatherqq_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vpgatherqq_yhivsibk7, vpgatherqq, vpgatherqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_8))
OPCODE(vpgatherqq_zlovsibk1, vpgatherqq, vpgatherqq_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vpgatherqq_zhivsibk7, vpgatherqq, vpgatherqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_8))
OPCODE(vgatherdps_xlovsibk1, vgatherdps, vgatherdps_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_4))
OPCODE(vgatherdps_xhivsibk7, vgatherdps, vgatherdps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_4))
OPCODE(vgatherdps_ylovsibk1, vgatherdps, vgatherdps_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_4))
OPCODE(vgatherdps_yhivsibk7, vgatherdps, vgatherdps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_4))
OPCODE(vgatherdps_zlovsibk1, vgatherdps, vgatherdps_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vgatherdps_zhivsibk7, vgatherdps, vgatherdps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_4))
OPCODE(vgatherdpd_xlovsibk1, vgatherdpd, vgatherdpd_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_8))
OPCODE(vgatherdpd_xhivsibk7, vgatherdpd, vgatherdpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_8))
OPCODE(vgatherdpd_ylovsibk1, vgatherdpd, vgatherdpd_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vgatherdpd_yhivsibk7, vgatherdpd, vgatherdpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_8))
OPCODE(vgatherdpd_zlovsibk1, vgatherdpd, vgatherdpd_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vgatherdpd_zhivsibk7, vgatherdpd, vgatherdpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_8))
OPCODE(vgatherqps_xlovsibk1, vgatherqps, vgatherqps_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_4))
OPCODE(vgatherqps_xhivsibk7, vgatherqps, vgatherqps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_4))
OPCODE(vgatherqps_ylovsibk1, vgatherqps, vgatherqps_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_4))
OPCODE(vgatherqps_yhivsibk7, vgatherqps, vgatherqps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_4))
OPCODE(vgatherqps_zlovsibk1, vgatherqps, vgatherqps_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vgatherqps_zhivsibk7, vgatherqps, vgatherqps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_4))
OPCODE(vgatherqpd_xlovsibk1, vgatherqpd, vgatherqpd_mask, 0, REGARG(XMM0), REGARG(K1),
       VSIBX6(OPSZ_8))
OPCODE(vgatherqpd_xhivsibk7, vgatherqpd, vgatherqpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), VSIBX6(OPSZ_8))
OPCODE(vgatherqpd_ylovsibk1, vgatherqpd, vgatherqpd_mask, 0, REGARG(YMM0), REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vgatherqpd_yhivsibk7, vgatherqpd, vgatherqpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), VSIBY6(OPSZ_8))
OPCODE(vgatherqpd_zlovsibk1, vgatherqpd, vgatherqpd_mask, 0, REGARG(ZMM0), REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vgatherqpd_zhivsibk7, vgatherqpd, vgatherqpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), VSIBZ31(OPSZ_8))
OPCODE(vpscatterdd_vsibxlok1, vpscatterdd, vpscatterdd_mask, 0, VSIBX6(OPSZ_4),
       REGARG(K1), REGARG(XMM0))
OPCODE(vpscatterdd_vsibxhik7, vpscatterdd, vpscatterdd_mask, X64_ONLY, VSIBX6(OPSZ_4),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpscatterdd_vsibylok1, vpscatterdd, vpscatterdd_mask, 0, VSIBY6(OPSZ_4),
       REGARG(K1), REGARG(YMM0))
OPCODE(vpscatterdd_vsibyhik7, vpscatterdd, vpscatterdd_mask, X64_ONLY, VSIBY6(OPSZ_4),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpscatterdd_vsibzlok1, vpscatterdd, vpscatterdd_mask, 0, VSIBZ6(OPSZ_4),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vpscatterdd_vsibzhik7, vpscatterdd, vpscatterdd_mask, X64_ONLY, VSIBZ31(OPSZ_4),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpscatterdq_vsibxlok1, vpscatterdq, vpscatterdq_mask, 0, VSIBX6(OPSZ_8),
       REGARG(K1), REGARG(XMM0))
OPCODE(vpscatterdq_vsibxhik7, vpscatterdq, vpscatterdq_mask, X64_ONLY, VSIBX6(OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpscatterdq_vsibylok1, vpscatterdq, vpscatterdq_mask, 0, VSIBY6(OPSZ_8),
       REGARG(K1), REGARG(YMM0))
OPCODE(vpscatterdq_vsibyhik7, vpscatterdq, vpscatterdq_mask, X64_ONLY, VSIBY6(OPSZ_8),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpscatterdq_vsibzlok1, vpscatterdq, vpscatterdq_mask, 0, VSIBZ6(OPSZ_8),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vpscatterdq_vsibzhik7, vpscatterdq, vpscatterdq_mask, X64_ONLY, VSIBZ31(OPSZ_8),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpscatterqd_vsibxlok1, vpscatterqd, vpscatterqd_mask, 0, VSIBX6(OPSZ_4),
       REGARG(K1), REGARG(XMM0))
OPCODE(vpscatterqd_vsibxhik7, vpscatterqd, vpscatterqd_mask, X64_ONLY, VSIBX6(OPSZ_4),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpscatterqd_vsibylok1, vpscatterqd, vpscatterqd_mask, 0, VSIBY6(OPSZ_4),
       REGARG(K1), REGARG(YMM0))
OPCODE(vpscatterqd_vsibyhik7, vpscatterqd, vpscatterqd_mask, X64_ONLY, VSIBY6(OPSZ_4),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpscatterqd_vsibzlok1, vpscatterqd, vpscatterqd_mask, 0, VSIBZ6(OPSZ_4),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vpscatterqd_vsibzhik7, vpscatterqd, vpscatterqd_mask, X64_ONLY, VSIBZ31(OPSZ_4),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpscatterqq_vsibxlok1, vpscatterqq, vpscatterqq_mask, 0, VSIBX6(OPSZ_8),
       REGARG(K1), REGARG(XMM0))
OPCODE(vpscatterqq_vsibxhik7, vpscatterqq, vpscatterqq_mask, X64_ONLY, VSIBX6(OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpscatterqq_vsibylok1, vpscatterqq, vpscatterqq_mask, 0, VSIBY6(OPSZ_8),
       REGARG(K1), REGARG(YMM0))
OPCODE(vpscatterqq_vsibyhik7, vpscatterqq, vpscatterqq_mask, X64_ONLY, VSIBY6(OPSZ_8),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpscatterqq_vsibzlok1, vpscatterqq, vpscatterqq_mask, 0, VSIBZ6(OPSZ_8),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vpscatterqq_vsibzhik7, vpscatterqq, vpscatterqq_mask, X64_ONLY, VSIBZ31(OPSZ_8),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vscatterdps_vsibxlok1, vscatterdps, vscatterdps_mask, 0, VSIBX6(OPSZ_4),
       REGARG(K1), REGARG(XMM0))
OPCODE(vscatterdps_vsibxhik7, vscatterdps, vscatterdps_mask, X64_ONLY, VSIBX6(OPSZ_4),
       REGARG(K7), REGARG(XMM16))
OPCODE(vscatterdps_vsibylok1, vscatterdps, vscatterdps_mask, 0, VSIBY6(OPSZ_4),
       REGARG(K1), REGARG(YMM0))
OPCODE(vscatterdps_vsibyhik7, vscatterdps, vscatterdps_mask, X64_ONLY, VSIBY6(OPSZ_4),
       REGARG(K7), REGARG(YMM16))
OPCODE(vscatterdps_vsibzlok1, vscatterdps, vscatterdps_mask, 0, VSIBZ6(OPSZ_4),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vscatterdps_vsibzhik7, vscatterdps, vscatterdps_mask, X64_ONLY, VSIBZ31(OPSZ_4),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vscatterdpd_vsibxlok1, vscatterdpd, vscatterdpd_mask, 0, VSIBX6(OPSZ_8),
       REGARG(K1), REGARG(XMM0))
OPCODE(vscatterdpd_vsibxhik7, vscatterdpd, vscatterdpd_mask, X64_ONLY, VSIBX6(OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vscatterdpd_vsibylok1, vscatterdpd, vscatterdpd_mask, 0, VSIBY6(OPSZ_8),
       REGARG(K1), REGARG(YMM0))
OPCODE(vscatterdpd_vsibyhik7, vscatterdpd, vscatterdpd_mask, X64_ONLY, VSIBY6(OPSZ_8),
       REGARG(K7), REGARG(YMM16))
OPCODE(vscatterdpd_vsibzlok1, vscatterdpd, vscatterdpd_mask, 0, VSIBZ6(OPSZ_8),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vscatterdpd_vsibzhik7, vscatterdpd, vscatterdpd_mask, X64_ONLY, VSIBZ31(OPSZ_8),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vscatterqps_vsibxlok1, vscatterqps, vscatterqps_mask, 0, VSIBX6(OPSZ_4),
       REGARG(K1), REGARG(XMM0))
OPCODE(vscatterqps_vsibxhik7, vscatterqps, vscatterqps_mask, X64_ONLY, VSIBX6(OPSZ_4),
       REGARG(K7), REGARG(XMM16))
OPCODE(vscatterqps_vsibylok1, vscatterqps, vscatterqps_mask, 0, VSIBY6(OPSZ_4),
       REGARG(K1), REGARG(YMM0))
OPCODE(vscatterqps_vsibyhik7, vscatterqps, vscatterqps_mask, X64_ONLY, VSIBY6(OPSZ_4),
       REGARG(K7), REGARG(YMM16))
OPCODE(vscatterqps_vsibzlok1, vscatterqps, vscatterqps_mask, 0, VSIBZ6(OPSZ_4),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vscatterqps_vsibzhik7, vscatterqps, vscatterqps_mask, X64_ONLY, VSIBZ31(OPSZ_4),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vscatterqpd_vsibxlok1, vscatterqpd, vscatterqpd_mask, 0, VSIBX6(OPSZ_8),
       REGARG(K1), REGARG(XMM0))
OPCODE(vscatterqpd_vsibxhik7, vscatterqpd, vscatterqpd_mask, X64_ONLY, VSIBX6(OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vscatterqpd_vsibylok1, vscatterqpd, vscatterqpd_mask, 0, VSIBY6(OPSZ_8),
       REGARG(K1), REGARG(YMM0))
OPCODE(vscatterqpd_vsibyhik7, vscatterqpd, vscatterqpd_mask, X64_ONLY, VSIBY6(OPSZ_8),
       REGARG(K7), REGARG(YMM16))
OPCODE(vscatterqpd_vsibzlok1, vscatterqpd, vscatterqpd_mask, 0, VSIBZ6(OPSZ_8),
       REGARG(K1), REGARG(ZMM0))
OPCODE(vscatterqpd_vsibzhik7, vscatterqpd, vscatterqpd_mask, X64_ONLY, VSIBZ31(OPSZ_8),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vsqrtps_xlok0xlo, vsqrtps, vsqrtps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vsqrtps_xlok0mem, vsqrtps, vsqrtps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vsqrtps_xlok0bcst, vsqrtps, vsqrtps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vsqrtps_xhik7xhi, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17))
OPCODE(vsqrtps_xhik7mem, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vsqrtps_xhik7bcst, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vsqrtps_ylok0ylo, vsqrtps, vsqrtps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vsqrtps_ylok0mem, vsqrtps, vsqrtps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vsqrtps_ylok0bcst, vsqrtps, vsqrtps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vsqrtps_yhik7yhi, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17))
OPCODE(vsqrtps_yhik7mem, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vsqrtps_yhik7bcst, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vsqrtps_zlok0zlo, vsqrtps, vsqrtps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vsqrtps_zlok0mem, vsqrtps, vsqrtps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vsqrtps_zlok0bcst, vsqrtps, vsqrtps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vsqrtps_zhik7zhi, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vsqrtps_zhik7mem, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vsqrtps_zhik7bcst, vsqrtps, vsqrtps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vsqrtpd_xlok0xlo, vsqrtpd, vsqrtpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vsqrtpd_xlok0mem, vsqrtpd, vsqrtpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vsqrtpd_xlok0bcst, vsqrtpd, vsqrtpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vsqrtpd_xhik7xhi, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17))
OPCODE(vsqrtpd_xhik7mem, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vsqrtpd_xhik7bcst, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vsqrtpd_ylok0ylo, vsqrtpd, vsqrtpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vsqrtpd_ylok0mem, vsqrtpd, vsqrtpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vsqrtpd_ylok0bcst, vsqrtpd, vsqrtpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vsqrtpd_yhik7yhi, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17))
OPCODE(vsqrtpd_yhik7mem, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vsqrtpd_yhik7bcst, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vsqrtpd_zlok0zlo, vsqrtpd, vsqrtpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vsqrtpd_zlok0mem, vsqrtpd, vsqrtpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vsqrtpd_zlok0bcst, vsqrtpd, vsqrtpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vsqrtpd_zhik7zhi, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vsqrtpd_zhik7mem, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vsqrtpd_zhik7bcst, vsqrtpd, vsqrtpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8))
/* AVX512 BF16 */
OPCODE(vcvtneps2bf16_xlok7xlo, vcvtneps2bf16, vcvtneps2bf16_mask, 0,
       REGARG_PARTIAL(XMM6, OPSZ_8), REGARG(K7), REGARG(XMM0))
OPCODE(vcvtneps2bf16_xhik7mem, vcvtneps2bf16, vcvtneps2bf16_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtneps2bf16_xlok7bcst, vcvtneps2bf16, vcvtneps2bf16_mask, 0,
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG(K7), MEMARG(OPSZ_4))

OPCODE(vcvtneps2bf16_ylok7ylo, vcvtneps2bf16, vcvtneps2bf16_mask, 0,
       REGARG_PARTIAL(YMM6, OPSZ_16), REGARG(K7), REGARG(YMM0))
OPCODE(vcvtneps2bf16_yhik7mem, vcvtneps2bf16, vcvtneps2bf16_mask, X64_ONLY,
       REGARG_PARTIAL(YMM16, OPSZ_16), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtneps2bf16_ylok7bcst, vcvtneps2bf16, vcvtneps2bf16_mask, 0,
       REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(K7), MEMARG(OPSZ_4))

OPCODE(vcvtneps2bf16_zlok7zlo, vcvtneps2bf16, vcvtneps2bf16_mask, 0,
       REGARG_PARTIAL(ZMM6, OPSZ_32), REGARG(K7), REGARG(ZMM0))
OPCODE(vcvtneps2bf16_zhik7mem, vcvtneps2bf16, vcvtneps2bf16_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM16, OPSZ_32), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtneps2bf16_zlok7bcst, vcvtneps2bf16, vcvtneps2bf16_mask, 0,
       REGARG_PARTIAL(ZMM1, OPSZ_32), REGARG(K7), MEMARG(OPSZ_4))
/* AVX 512 VPOPCNTDQ */
OPCODE(vpopcntd_xlok7xlo, vpopcntd, vpopcntd_mask, 0, REGARG(XMM6), REGARG(K7),
       REGARG(XMM0))
OPCODE(vpopcntd_xhik7mem, vpopcntd, vpopcntd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpopcntd_xlok7bcst, vpopcntd, vpopcntd_mask, 0, REGARG(XMM1), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpopcntd_ylok7xlo, vpopcntd, vpopcntd_mask, 0, REGARG(YMM6), REGARG(K7),
       REGARG(YMM0))
OPCODE(vpopcntd_yhik7mem, vpopcntd, vpopcntd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpopcntd_ylok7bcst, vpopcntd, vpopcntd_mask, 0, REGARG(YMM1), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpopcntd_zlok7xlo, vpopcntd, vpopcntd_mask, 0, REGARG(ZMM6), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vpopcntd_zhik7mem, vpopcntd, vpopcntd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpopcntd_zlok7bcst, vpopcntd, vpopcntd_mask, 0, REGARG(ZMM1), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpopcntq_xlok7xlo, vpopcntq, vpopcntq_mask, 0, REGARG(XMM6), REGARG(K7),
       REGARG(XMM0))
OPCODE(vpopcntq_xhik7mem, vpopcntq, vpopcntq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpopcntq_xlok7bcst, vpopcntq, vpopcntq_mask, 0, REGARG(XMM1), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpopcntq_ylok7xlo, vpopcntq, vpopcntq_mask, 0, REGARG(YMM6), REGARG(K7),
       REGARG(YMM0))
OPCODE(vpopcntq_yhik7mem, vpopcntq, vpopcntq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpopcntq_ylok7bcst, vpopcntq, vpopcntq_mask, 0, REGARG(YMM1), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpopcntq_zlok7xlo, vpopcntq, vpopcntq_mask, 0, REGARG(ZMM6), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vpopcntq_zhik7mem, vpopcntq, vpopcntq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpopcntq_zlok7bcst, vpopcntq, vpopcntq_mask, 0, REGARG(ZMM1), REGARG(K7),
       MEMARG(OPSZ_8))
/* AVX 512 BITALG */
OPCODE(vpopcntb_xlok7xlo, vpopcntb, vpopcntb_mask, 0, REGARG(XMM6), REGARG(K7),
       REGARG(XMM0))
OPCODE(vpopcntb_xhik7mem, vpopcntb, vpopcntb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpopcntb_ylok7xlo, vpopcntb, vpopcntb_mask, 0, REGARG(YMM6), REGARG(K7),
       REGARG(YMM0))
OPCODE(vpopcntb_yhik7mem, vpopcntb, vpopcntb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpopcntb_zlok7xlo, vpopcntb, vpopcntb_mask, 0, REGARG(ZMM6), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vpopcntb_zhik7mem, vpopcntb, vpopcntb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpopcntw_xlok7xlo, vpopcntw, vpopcntw_mask, 0, REGARG(XMM6), REGARG(K7),
       REGARG(XMM0))
OPCODE(vpopcntw_xhik7mem, vpopcntw, vpopcntw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpopcntw_ylok7xlo, vpopcntw, vpopcntw_mask, 0, REGARG(YMM6), REGARG(K7),
       REGARG(YMM0))
OPCODE(vpopcntw_yhik7mem, vpopcntw, vpopcntw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpopcntw_zlok7xlo, vpopcntw, vpopcntw_mask, 0, REGARG(ZMM6), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vpopcntw_zhik7mem, vpopcntw, vpopcntw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpcompressb_xlok0st, vpcompressb, vpcompressb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressb_xlok0xlo, vpcompressb, vpcompressb_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressb_xhik7xhi, vpcompressb, vpcompressb_mask, X64_ONLY, REGARG(XMM31),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpcompressb_ylok0st, vpcompressb, vpcompressb_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressb_ylok0ylo, vpcompressb, vpcompressb_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressb_yhik7yhi, vpcompressb, vpcompressb_mask, X64_ONLY, REGARG(YMM31),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpcompressb_zlok0st, vpcompressb, vpcompressb_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressb_zlok0zlo, vpcompressb, vpcompressb_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressb_zhik7zhi, vpcompressb, vpcompressb_mask, X64_ONLY, REGARG(ZMM31),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpcompressw_xlok0st, vpcompressw, vpcompressw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressw_xlok0xlo, vpcompressw, vpcompressw_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressw_xhik7xhi, vpcompressw, vpcompressw_mask, X64_ONLY, REGARG(XMM31),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpcompressw_ylok0st, vpcompressw, vpcompressw_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressw_ylok0ylo, vpcompressw, vpcompressw_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressw_yhik7yhi, vpcompressw, vpcompressw_mask, X64_ONLY, REGARG(YMM31),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpcompressw_zlok0st, vpcompressw, vpcompressw_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressw_zlok0zlo, vpcompressw, vpcompressw_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressw_zhik7zhi, vpcompressw, vpcompressw_mask, X64_ONLY, REGARG(ZMM31),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpexpandb_xlok0ld, vpexpandb, vpexpandb_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpexpandb_xlok0xlo, vpexpandb, vpexpandb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vpexpandb_xhik7xhi, vpexpandb, vpexpandb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpexpandb_ylok0ld, vpexpandb, vpexpandb_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpexpandb_ylok0ylo, vpexpandb, vpexpandb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vpexpandb_yhik7yhi, vpexpandb, vpexpandb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpexpandb_zlok0ld, vpexpandb, vpexpandb_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vpexpandb_zlok0zlo, vpexpandb, vpexpandb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vpexpandb_zhik7zhi, vpexpandb, vpexpandb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vpexpandw_xlok0ld, vpexpandw, vpexpandw_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpexpandw_xlok0xlo, vpexpandw, vpexpandw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vpexpandw_xhik7xhi, vpexpandw, vpexpandw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpexpandw_ylok0ld, vpexpandw, vpexpandw_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpexpandw_ylok0ylo, vpexpandw, vpexpandw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vpexpandw_yhik7yhi, vpexpandw, vpexpandw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpexpandw_zlok0ld, vpexpandw, vpexpandw_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vpexpandw_zlok0zlo, vpexpandw, vpexpandw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vpexpandw_zhik7zhi, vpexpandw, vpexpandw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
