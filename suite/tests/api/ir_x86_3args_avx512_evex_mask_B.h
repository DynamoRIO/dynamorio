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
OPCODE(vpmovsxbq_xlok0xlo, vpmovsxbq, vpmovsxbq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpmovsxbq_xhik7xhi, vpmovsxbq, vpmovsxbq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_2))
OPCODE(vpmovsxbq_xlok0ld, vpmovsxbq, vpmovsxbq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_2))
OPCODE(vpmovsxbq_xhik7ld, vpmovsxbq, vpmovsxbq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_2))
OPCODE(vpmovsxbq_ylok0ylo, vpmovsxbq, vpmovsxbq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_4))
OPCODE(vpmovsxbq_yhik7yhi, vpmovsxbq, vpmovsxbq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_4))
OPCODE(vpmovsxbq_ylok0ld, vpmovsxbq, vpmovsxbq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpmovsxbq_yhik7ld, vpmovsxbq, vpmovsxbq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpmovsxbq_zlok0zlo, vpmovsxbq, vpmovsxbq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_8))
OPCODE(vpmovsxbq_zhik7zhi, vpmovsxbq, vpmovsxbq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_8))
OPCODE(vpmovsxbq_zlok0ld, vpmovsxbq, vpmovsxbq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxbq_zhik7ld, vpmovsxbq, vpmovsxbq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxwd_xlok0xlo, vpmovsxwd, vpmovsxwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovsxwd_xhik7xhi, vpmovsxwd, vpmovsxwd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovsxwd_xlok0ld, vpmovsxwd, vpmovsxwd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxwd_xhik7ld, vpmovsxwd, vpmovsxwd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxwd_ylok0ylo, vpmovsxwd, vpmovsxwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovsxwd_yhik7yhi, vpmovsxwd, vpmovsxwd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vpmovsxwd_ylok0ld, vpmovsxwd, vpmovsxwd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxwd_yhik7ld, vpmovsxwd, vpmovsxwd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxwd_zlok0zlo, vpmovsxwd, vpmovsxwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vpmovsxwd_zhik7zhi, vpmovsxwd, vpmovsxwd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpmovsxwd_zlok0ld, vpmovsxwd, vpmovsxwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpmovsxwd_zhik7ld, vpmovsxwd, vpmovsxwd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpmovsxwq_xlok0xlo, vpmovsxwq, vpmovsxwq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovsxwq_xhik7xhi, vpmovsxwq, vpmovsxwq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpmovsxwq_xlok0ld, vpmovsxwq, vpmovsxwq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpmovsxwq_xhik7ld, vpmovsxwq, vpmovsxwq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpmovsxwq_ylok0ylo, vpmovsxwq, vpmovsxwq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovsxwq_yhik7yhi, vpmovsxwq, vpmovsxwq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_8))
OPCODE(vpmovsxwq_ylok0ld, vpmovsxwq, vpmovsxwq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxwq_yhik7ld, vpmovsxwq, vpmovsxwq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxwq_zlok0zlo, vpmovsxwq, vpmovsxwq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vpmovsxwq_zhik7zhi, vpmovsxwq, vpmovsxwq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vpmovsxwq_zlok0ld, vpmovsxwq, vpmovsxwq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxwq_zhik7ld, vpmovsxwq, vpmovsxwq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxdq_xlok0xlo, vpmovsxdq, vpmovsxdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovsxdq_xhik7xhi, vpmovsxdq, vpmovsxdq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovsxdq_xlok0ld, vpmovsxdq, vpmovsxdq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxdq_xhik7ld, vpmovsxdq, vpmovsxdq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxdq_ylok0ylo, vpmovsxdq, vpmovsxdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovsxdq_yhik7yhi, vpmovsxdq, vpmovsxdq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vpmovsxdq_ylok0ld, vpmovsxdq, vpmovsxdq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxdq_yhik7ld, vpmovsxdq, vpmovsxdq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxdq_zlok0zlo, vpmovsxdq, vpmovsxdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vpmovsxdq_zhik7zhi, vpmovsxdq, vpmovsxdq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpmovsxdq_zlok0ld, vpmovsxdq, vpmovsxdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpmovsxdq_zhik7ld, vpmovsxdq, vpmovsxdq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpmovzxbw_xlok0xlo, vpmovzxbw, vpmovzxbw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovzxbw_xhik7xhi, vpmovzxbw, vpmovzxbw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovzxbw_xlok0ld, vpmovzxbw, vpmovzxbw_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxbw_xhik7ld, vpmovzxbw, vpmovzxbw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxbw_ylok0ylo, vpmovzxbw, vpmovzxbw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovzxbw_yhik7yhi, vpmovzxbw, vpmovzxbw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vpmovzxbw_ylok0ld, vpmovzxbw, vpmovzxbw_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxbw_yhik7ld, vpmovzxbw, vpmovzxbw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxbw_zlok0zlo, vpmovzxbw, vpmovzxbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vpmovzxbw_zhik7zhi, vpmovzxbw, vpmovzxbw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpmovzxbw_zlok0ld, vpmovzxbw, vpmovzxbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpmovzxbw_zhik7ld, vpmovzxbw, vpmovzxbw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpmovzxbd_xlok0xlo, vpmovzxbd, vpmovzxbd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovzxbd_xhik7xhi, vpmovzxbd, vpmovzxbd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpmovzxbd_xlok0ld, vpmovzxbd, vpmovzxbd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpmovzxbd_xhik7ld, vpmovzxbd, vpmovzxbd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpmovzxbd_ylok0ylo, vpmovzxbd, vpmovzxbd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovzxbd_yhik7yhi, vpmovzxbd, vpmovzxbd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_8))
OPCODE(vpmovzxbd_ylok0ld, vpmovzxbd, vpmovzxbd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxbd_yhik7ld, vpmovzxbd, vpmovzxbd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxbd_zlok0zlo, vpmovzxbd, vpmovzxbd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vpmovzxbd_zhik7zhi, vpmovzxbd, vpmovzxbd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vpmovzxbd_zlok0ld, vpmovzxbd, vpmovzxbd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxbd_zhik7ld, vpmovzxbd, vpmovzxbd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxbq_xlok0xlo, vpmovzxbq, vpmovzxbq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpmovzxbq_xhik7xhi, vpmovzxbq, vpmovzxbq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_2))
OPCODE(vpmovzxbq_xlok0ld, vpmovzxbq, vpmovzxbq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_2))
OPCODE(vpmovzxbq_xhik7ld, vpmovzxbq, vpmovzxbq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_2))
OPCODE(vpmovzxbq_ylok0ylo, vpmovzxbq, vpmovzxbq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_4))
OPCODE(vpmovzxbq_yhik7yhi, vpmovzxbq, vpmovzxbq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_4))
OPCODE(vpmovzxbq_ylok0ld, vpmovzxbq, vpmovzxbq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpmovzxbq_yhik7ld, vpmovzxbq, vpmovzxbq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpmovzxbq_zlok0zlo, vpmovzxbq, vpmovzxbq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_8))
OPCODE(vpmovzxbq_zhik7zhi, vpmovzxbq, vpmovzxbq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_8))
OPCODE(vpmovzxbq_zlok0ld, vpmovzxbq, vpmovzxbq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxbq_zhik7ld, vpmovzxbq, vpmovzxbq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxwd_xlok0xlo, vpmovzxwd, vpmovzxwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovzxwd_xhik7xhi, vpmovzxwd, vpmovzxwd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovzxwd_xlok0ld, vpmovzxwd, vpmovzxwd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxwd_xhik7ld, vpmovzxwd, vpmovzxwd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxwd_ylok0ylo, vpmovzxwd, vpmovzxwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovzxwd_yhik7yhi, vpmovzxwd, vpmovzxwd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vpmovzxwd_ylok0ld, vpmovzxwd, vpmovzxwd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxwd_yhik7ld, vpmovzxwd, vpmovzxwd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxwd_zlok0zlo, vpmovzxwd, vpmovzxwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vpmovzxwd_zhik7zhi, vpmovzxwd, vpmovzxwd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpmovzxwd_zlok0ld, vpmovzxwd, vpmovzxwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpmovzxwd_zhik7ld, vpmovzxwd, vpmovzxwd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpmovzxwq_xlok0xlo, vpmovzxwq, vpmovzxwq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovzxwq_xhik7xhi, vpmovzxwq, vpmovzxwq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpmovzxwq_xlok0ld, vpmovzxwq, vpmovzxwq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpmovzxwq_xhik7ld, vpmovzxwq, vpmovzxwq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpmovzxwq_ylok0ylo, vpmovzxwq, vpmovzxwq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovzxwq_yhik7yhi, vpmovzxwq, vpmovzxwq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_8))
OPCODE(vpmovzxwq_ylok0ld, vpmovzxwq, vpmovzxwq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxwq_yhik7ld, vpmovzxwq, vpmovzxwq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxwq_zlok0zlo, vpmovzxwq, vpmovzxwq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vpmovzxwq_zhik7zhi, vpmovzxwq, vpmovzxwq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vpmovzxwq_zlok0ld, vpmovzxwq, vpmovzxwq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxwq_zhik7ld, vpmovzxwq, vpmovzxwq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxdq_xlok0xlo, vpmovzxdq, vpmovzxdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovzxdq_xhik7xhi, vpmovzxdq, vpmovzxdq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovzxdq_xlok0ld, vpmovzxdq, vpmovzxdq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxdq_xhik7ld, vpmovzxdq, vpmovzxdq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovzxdq_ylok0ylo, vpmovzxdq, vpmovzxdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovzxdq_yhik7yhi, vpmovzxdq, vpmovzxdq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vpmovzxdq_ylok0ld, vpmovzxdq, vpmovzxdq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxdq_yhik7ld, vpmovzxdq, vpmovzxdq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovzxdq_zlok0zlo, vpmovzxdq, vpmovzxdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vpmovzxdq_zhik7zhi, vpmovzxdq, vpmovzxdq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpmovzxdq_zlok0ld, vpmovzxdq, vpmovzxdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpmovzxdq_zhik7ld, vpmovzxdq, vpmovzxdq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpmovqb_xlok0xlo, vpmovqb, vpmovqb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_2),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovqb_xhik7xhi, vpmovqb, vpmovqb_mask, X64_ONLY, REGARG_PARTIAL(XMM31, OPSZ_2),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpmovqb_xlok0ld, vpmovqb, vpmovqb_mask, 0, MEMARG(OPSZ_2), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovqb_xhik7ld, vpmovqb, vpmovqb_mask, X64_ONLY, MEMARG(OPSZ_2), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovqb_ylok0ylo, vpmovqb, vpmovqb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_4),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovqb_yhik7yhi, vpmovqb, vpmovqb_mask, X64_ONLY, REGARG_PARTIAL(YMM31, OPSZ_4),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovqb_ylok0ld, vpmovqb, vpmovqb_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovqb_yhik7ld, vpmovqb, vpmovqb_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovqb_zlok0zlo, vpmovqb, vpmovqb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_8),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovqb_zhik7zhi, vpmovqb, vpmovqb_mask, X64_ONLY, REGARG_PARTIAL(ZMM31, OPSZ_8),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovqb_zlok0ld, vpmovqb, vpmovqb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovqb_zhik7ld, vpmovqb, vpmovqb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovsqb_xlok0xlo, vpmovsqb, vpmovsqb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_2),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovsqb_xhik7xhi, vpmovsqb, vpmovsqb_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_2), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovsqb_xlok0ld, vpmovsqb, vpmovsqb_mask, 0, MEMARG(OPSZ_2), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovsqb_xhik7ld, vpmovsqb, vpmovsqb_mask, X64_ONLY, MEMARG(OPSZ_2), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovsqb_ylok0ylo, vpmovsqb, vpmovsqb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_4),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovsqb_yhik7yhi, vpmovsqb, vpmovsqb_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_4), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovsqb_ylok0ld, vpmovsqb, vpmovsqb_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovsqb_yhik7ld, vpmovsqb, vpmovsqb_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovsqb_zlok0zlo, vpmovsqb, vpmovsqb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_8),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovsqb_zhik7zhi, vpmovsqb, vpmovsqb_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_8), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovsqb_zlok0ld, vpmovsqb, vpmovsqb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovsqb_zhik7ld, vpmovsqb, vpmovsqb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovusqb_xlok0xlo, vpmovusqb, vpmovusqb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_2),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovusqb_xhik7xhi, vpmovusqb, vpmovusqb_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_2), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovusqb_xlok0ld, vpmovusqb, vpmovusqb_mask, 0, MEMARG(OPSZ_2), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovusqb_xhik7ld, vpmovusqb, vpmovusqb_mask, X64_ONLY, MEMARG(OPSZ_2), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovusqb_ylok0ylo, vpmovusqb, vpmovusqb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_4),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovusqb_yhik7yhi, vpmovusqb, vpmovusqb_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_4), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusqb_ylok0ld, vpmovusqb, vpmovusqb_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovusqb_yhik7ld, vpmovusqb, vpmovusqb_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovusqb_zlok0zlo, vpmovusqb, vpmovusqb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_8),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovusqb_zhik7zhi, vpmovusqb, vpmovusqb_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_8), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovusqb_zlok0ld, vpmovusqb, vpmovusqb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovusqb_zhik7ld, vpmovusqb, vpmovusqb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovqw_xlok0xlo, vpmovqw, vpmovqw_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovqw_xhik7xhi, vpmovqw, vpmovqw_mask, X64_ONLY, REGARG_PARTIAL(XMM31, OPSZ_4),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpmovqw_xlok0ld, vpmovqw, vpmovqw_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovqw_xhik7ld, vpmovqw, vpmovqw_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovqw_ylok0ylo, vpmovqw, vpmovqw_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_8),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovqw_yhik7yhi, vpmovqw, vpmovqw_mask, X64_ONLY, REGARG_PARTIAL(YMM31, OPSZ_8),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovqw_ylok0ld, vpmovqw, vpmovqw_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovqw_yhik7ld, vpmovqw, vpmovqw_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovqw_zlok0zlo, vpmovqw, vpmovqw_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_16),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovqw_zhik7zhi, vpmovqw, vpmovqw_mask, X64_ONLY, REGARG_PARTIAL(ZMM31, OPSZ_16),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovqw_zlok0ld, vpmovqw, vpmovqw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovqw_zhik7ld, vpmovqw, vpmovqw_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovsqw_xlok0xlo, vpmovsqw, vpmovsqw_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovsqw_xhik7xhi, vpmovsqw, vpmovsqw_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_4), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovsqw_xlok0ld, vpmovsqw, vpmovsqw_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovsqw_xhik7ld, vpmovsqw, vpmovsqw_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovsqw_ylok0ylo, vpmovsqw, vpmovsqw_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_8),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovsqw_yhik7yhi, vpmovsqw, vpmovsqw_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_8), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovsqw_ylok0ld, vpmovsqw, vpmovsqw_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovsqw_yhik7ld, vpmovsqw, vpmovsqw_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovsqw_zlok0zlo, vpmovsqw, vpmovsqw_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_16),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovsqw_zhik7zhi, vpmovsqw, vpmovsqw_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_16), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovsqw_zlok0ld, vpmovsqw, vpmovsqw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovsqw_zhik7ld, vpmovsqw, vpmovsqw_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovusqw_xlok0xlo, vpmovusqw, vpmovusqw_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovusqw_xhik7xhi, vpmovusqw, vpmovusqw_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_4), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovusqw_xlok0ld, vpmovusqw, vpmovusqw_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovusqw_xhik7ld, vpmovusqw, vpmovusqw_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovusqw_ylok0ylo, vpmovusqw, vpmovusqw_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_8),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovusqw_yhik7yhi, vpmovusqw, vpmovusqw_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_8), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusqw_ylok0ld, vpmovusqw, vpmovusqw_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovusqw_yhik7ld, vpmovusqw, vpmovusqw_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovusqw_zlok0zlo, vpmovusqw, vpmovusqw_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_16),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovusqw_zhik7zhi, vpmovusqw, vpmovusqw_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_16), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovusqw_zlok0ld, vpmovusqw, vpmovusqw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovusqw_zhik7ld, vpmovusqw, vpmovusqw_mask, X64_ONLY, MEMARG(OPSZ_16),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovqd_xlok0xlo, vpmovqd, vpmovqd_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovqd_xhik7xhi, vpmovqd, vpmovqd_mask, X64_ONLY, REGARG_PARTIAL(XMM31, OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpmovqd_xlok0ld, vpmovqd, vpmovqd_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovqd_xhik7ld, vpmovqd, vpmovqd_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovqd_ylok0ylo, vpmovqd, vpmovqd_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovqd_yhik7yhi, vpmovqd, vpmovqd_mask, X64_ONLY, REGARG_PARTIAL(YMM31, OPSZ_16),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovqd_ylok0ld, vpmovqd, vpmovqd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovqd_yhik7ld, vpmovqd, vpmovqd_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovqd_zlok0zlo, vpmovqd, vpmovqd_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovqd_zhik7zhi, vpmovqd, vpmovqd_mask, X64_ONLY, REGARG_PARTIAL(ZMM31, OPSZ_32),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovqd_zlok0ld, vpmovqd, vpmovqd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovqd_zhik7ld, vpmovqd, vpmovqd_mask, X64_ONLY, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovsqd_xlok0xlo, vpmovsqd, vpmovsqd_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovsqd_xhik7xhi, vpmovsqd, vpmovsqd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovsqd_xlok0ld, vpmovsqd, vpmovsqd_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovsqd_xhik7ld, vpmovsqd, vpmovsqd_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovsqd_ylok0ylo, vpmovsqd, vpmovsqd_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovsqd_yhik7yhi, vpmovsqd, vpmovsqd_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_16), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovsqd_ylok0ld, vpmovsqd, vpmovsqd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovsqd_yhik7ld, vpmovsqd, vpmovsqd_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovsqd_zlok0zlo, vpmovsqd, vpmovsqd_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovsqd_zhik7zhi, vpmovsqd, vpmovsqd_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_32), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovsqd_zlok0ld, vpmovsqd, vpmovsqd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovsqd_zhik7ld, vpmovsqd, vpmovsqd_mask, X64_ONLY, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovusqd_xlok0xlo, vpmovusqd, vpmovusqd_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovusqd_xhik7xhi, vpmovusqd, vpmovusqd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovusqd_xlok0ld, vpmovusqd, vpmovusqd_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovusqd_xhik7ld, vpmovusqd, vpmovusqd_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovusqd_ylok0ylo, vpmovusqd, vpmovusqd_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovusqd_yhik7yhi, vpmovusqd, vpmovusqd_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_16), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusqd_ylok0ld, vpmovusqd, vpmovusqd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovusqd_yhik7ld, vpmovusqd, vpmovusqd_mask, X64_ONLY, MEMARG(OPSZ_16),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusqd_zlok0zlo, vpmovusqd, vpmovusqd_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovusqd_zhik7zhi, vpmovusqd, vpmovusqd_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_32), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovusqd_zlok0ld, vpmovusqd, vpmovusqd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovusqd_zhik7ld, vpmovusqd, vpmovusqd_mask, X64_ONLY, MEMARG(OPSZ_32),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovdb_xlok0xlo, vpmovdb, vpmovdb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovdb_xhik7xhi, vpmovdb, vpmovdb_mask, X64_ONLY, REGARG_PARTIAL(XMM31, OPSZ_4),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpmovdb_xlok0ld, vpmovdb, vpmovdb_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovdb_xhik7ld, vpmovdb, vpmovdb_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovdb_ylok0ylo, vpmovdb, vpmovdb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_8),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovdb_yhik7yhi, vpmovdb, vpmovdb_mask, X64_ONLY, REGARG_PARTIAL(YMM31, OPSZ_8),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovdb_ylok0ld, vpmovdb, vpmovdb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovdb_yhik7ld, vpmovdb, vpmovdb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovdb_zlok0zlo, vpmovdb, vpmovdb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_16),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovdb_zhik7zhi, vpmovdb, vpmovdb_mask, X64_ONLY, REGARG_PARTIAL(ZMM31, OPSZ_16),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovdb_zlok0ld, vpmovdb, vpmovdb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovdb_zhik7ld, vpmovdb, vpmovdb_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovsdb_xlok0xlo, vpmovsdb, vpmovsdb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovsdb_xhik7xhi, vpmovsdb, vpmovsdb_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_4), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovsdb_xlok0ld, vpmovsdb, vpmovsdb_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovsdb_xhik7ld, vpmovsdb, vpmovsdb_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovsdb_ylok0ylo, vpmovsdb, vpmovsdb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_8),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovsdb_yhik7yhi, vpmovsdb, vpmovsdb_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_8), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovsdb_ylok0ld, vpmovsdb, vpmovsdb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovsdb_yhik7ld, vpmovsdb, vpmovsdb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovsdb_zlok0zlo, vpmovsdb, vpmovsdb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_16),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovsdb_zhik7zhi, vpmovsdb, vpmovsdb_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_16), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovsdb_zlok0ld, vpmovsdb, vpmovsdb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovsdb_zhik7ld, vpmovsdb, vpmovsdb_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovusdb_xlok0xlo, vpmovusdb, vpmovusdb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovusdb_xhik7xhi, vpmovusdb, vpmovusdb_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_4), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovusdb_xlok0ld, vpmovusdb, vpmovusdb_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovusdb_xhik7ld, vpmovusdb, vpmovusdb_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovusdb_ylok0ylo, vpmovusdb, vpmovusdb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_8),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovusdb_yhik7yhi, vpmovusdb, vpmovusdb_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_8), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusdb_ylok0ld, vpmovusdb, vpmovusdb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovusdb_yhik7ld, vpmovusdb, vpmovusdb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovusdb_zlok0zlo, vpmovusdb, vpmovusdb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_16),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovusdb_zhik7zhi, vpmovusdb, vpmovusdb_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_16), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovusdb_zlok0ld, vpmovusdb, vpmovusdb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovusdb_zhik7ld, vpmovusdb, vpmovusdb_mask, X64_ONLY, MEMARG(OPSZ_16),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovdw_xlok0xlo, vpmovdw, vpmovdw_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovdw_xhik7xhi, vpmovdw, vpmovdw_mask, X64_ONLY, REGARG_PARTIAL(XMM31, OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpmovdw_xlok0ld, vpmovdw, vpmovdw_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovdw_xhik7ld, vpmovdw, vpmovdw_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovdw_ylok0ylo, vpmovdw, vpmovdw_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovdw_yhik7yhi, vpmovdw, vpmovdw_mask, X64_ONLY, REGARG_PARTIAL(YMM31, OPSZ_16),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovdw_ylok0ld, vpmovdw, vpmovdw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovdw_yhik7ld, vpmovdw, vpmovdw_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovdw_zlok0zlo, vpmovdw, vpmovdw_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovdw_zhik7zhi, vpmovdw, vpmovdw_mask, X64_ONLY, REGARG_PARTIAL(ZMM31, OPSZ_32),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovdw_zlok0ld, vpmovdw, vpmovdw_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovdw_zhik7ld, vpmovdw, vpmovdw_mask, X64_ONLY, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovsdw_xlok0xlo, vpmovsdw, vpmovsdw_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovsdw_xhik7xhi, vpmovsdw, vpmovsdw_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovsdw_xlok0ld, vpmovsdw, vpmovsdw_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovsdw_xhik7ld, vpmovsdw, vpmovsdw_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovsdw_ylok0ylo, vpmovsdw, vpmovsdw_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovsdw_yhik7yhi, vpmovsdw, vpmovsdw_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_16), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovsdw_ylok0ld, vpmovsdw, vpmovsdw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovsdw_yhik7ld, vpmovsdw, vpmovsdw_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovsdw_zlok0zlo, vpmovsdw, vpmovsdw_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovsdw_zhik7zhi, vpmovsdw, vpmovsdw_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_32), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovsdw_zlok0ld, vpmovsdw, vpmovsdw_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovsdw_zhik7ld, vpmovsdw, vpmovsdw_mask, X64_ONLY, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovusdw_xlok0xlo, vpmovusdw, vpmovusdw_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovusdw_xhik7xhi, vpmovusdw, vpmovusdw_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovusdw_xlok0ld, vpmovusdw, vpmovusdw_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovusdw_xhik7ld, vpmovusdw, vpmovusdw_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovusdw_ylok0ylo, vpmovusdw, vpmovusdw_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovusdw_yhik7yhi, vpmovusdw, vpmovusdw_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_16), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusdw_ylok0ld, vpmovusdw, vpmovusdw_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovusdw_yhik7ld, vpmovusdw, vpmovusdw_mask, X64_ONLY, MEMARG(OPSZ_16),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovusdw_zlok0zlo, vpmovusdw, vpmovusdw_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovusdw_zhik7zhi, vpmovusdw, vpmovusdw_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_32), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovusdw_zlok0ld, vpmovusdw, vpmovusdw_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovusdw_zhik7ld, vpmovusdw, vpmovusdw_mask, X64_ONLY, MEMARG(OPSZ_32),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovwb_xlok0xlo, vpmovwb, vpmovwb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovwb_xhik7xhi, vpmovwb, vpmovwb_mask, X64_ONLY, REGARG_PARTIAL(XMM31, OPSZ_8),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpmovwb_xlok0ld, vpmovwb, vpmovwb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovwb_xhik7ld, vpmovwb, vpmovwb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovwb_ylok0ylo, vpmovwb, vpmovwb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovwb_yhik7yhi, vpmovwb, vpmovwb_mask, X64_ONLY, REGARG_PARTIAL(YMM31, OPSZ_16),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovwb_ylok0ld, vpmovwb, vpmovwb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovwb_yhik7ld, vpmovwb, vpmovwb_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovwb_zlok0zlo, vpmovwb, vpmovwb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovwb_zhik7zhi, vpmovwb, vpmovwb_mask, X64_ONLY, REGARG_PARTIAL(ZMM31, OPSZ_32),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovwb_zlok0ld, vpmovwb, vpmovwb_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovwb_zhik7ld, vpmovwb, vpmovwb_mask, X64_ONLY, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovswb_xlok0xlo, vpmovswb, vpmovswb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovswb_xhik7xhi, vpmovswb, vpmovswb_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovswb_xlok0ld, vpmovswb, vpmovswb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovswb_xhik7ld, vpmovswb, vpmovswb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovswb_ylok0ylo, vpmovswb, vpmovswb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovswb_yhik7yhi, vpmovswb, vpmovswb_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_16), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovswb_ylok0ld, vpmovswb, vpmovswb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovswb_yhik7ld, vpmovswb, vpmovswb_mask, X64_ONLY, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpmovswb_zlok0zlo, vpmovswb, vpmovswb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovswb_zhik7zhi, vpmovswb, vpmovswb_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_32), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovswb_zlok0ld, vpmovswb, vpmovswb_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovswb_zhik7ld, vpmovswb, vpmovswb_mask, X64_ONLY, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpmovuswb_xlok0xlo, vpmovuswb, vpmovuswb_mask, 0, REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(K0), REGARG(XMM0))
OPCODE(vpmovuswb_xhik7xhi, vpmovuswb, vpmovuswb_mask, X64_ONLY,
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(K7), REGARG(XMM16))
OPCODE(vpmovuswb_xlok0ld, vpmovuswb, vpmovuswb_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpmovuswb_xhik7ld, vpmovuswb, vpmovuswb_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpmovuswb_ylok0ylo, vpmovuswb, vpmovuswb_mask, 0, REGARG_PARTIAL(YMM1, OPSZ_16),
       REGARG(K0), REGARG(YMM0))
OPCODE(vpmovuswb_yhik7yhi, vpmovuswb, vpmovuswb_mask, X64_ONLY,
       REGARG_PARTIAL(YMM31, OPSZ_16), REGARG(K7), REGARG(YMM16))
OPCODE(vpmovuswb_ylok0ld, vpmovuswb, vpmovuswb_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpmovuswb_yhik7ld, vpmovuswb, vpmovuswb_mask, X64_ONLY, MEMARG(OPSZ_16),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpmovuswb_zlok0zlo, vpmovuswb, vpmovuswb_mask, 0, REGARG_PARTIAL(ZMM1, OPSZ_32),
       REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovuswb_zhik7zhi, vpmovuswb, vpmovuswb_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM31, OPSZ_32), REGARG(K7), REGARG(ZMM16))
OPCODE(vpmovuswb_zlok0ld, vpmovuswb, vpmovuswb_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpmovuswb_zhik7ld, vpmovuswb, vpmovuswb_mask, X64_ONLY, MEMARG(OPSZ_32),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpabsb_xlok0ld, vpabsb, vpabsb_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vpabsb_xlok0xlo, vpabsb, vpabsb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vpabsb_xhik7xhi, vpabsb, vpabsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpabsb_ylok0ld, vpabsb, vpabsb_mask, 0, REGARG(YMM0), REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vpabsb_ylok0ylo, vpabsb, vpabsb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vpabsb_yhik7yhi, vpabsb, vpabsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpabsb_zlok0ld, vpabsb, vpabsb_mask, 0, REGARG(ZMM0), REGARG(K0), MEMARG(OPSZ_64))
OPCODE(vpabsb_zlok0zlo, vpabsb, vpabsb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vpabsb_zhik7zhi, vpabsb, vpabsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vpabsw_xlok0ld, vpabsw, vpabsw_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vpabsw_xlok0xlo, vpabsw, vpabsw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vpabsw_xhik7xhi, vpabsw, vpabsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpabsw_ylok0ld, vpabsw, vpabsw_mask, 0, REGARG(YMM0), REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vpabsw_ylok0ylo, vpabsw, vpabsw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vpabsw_yhik7yhi, vpabsw, vpabsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpabsw_zlok0ld, vpabsw, vpabsw_mask, 0, REGARG(ZMM0), REGARG(K0), MEMARG(OPSZ_64))
OPCODE(vpabsw_zlok0zlo, vpabsw, vpabsw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vpabsw_zhik7zhi, vpabsw, vpabsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vpabsd_xlok0ld, vpabsd, vpabsd_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vpabsd_xlok0bcst, vpabsd, vpabsd_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_4))
OPCODE(vpabsd_xlok0xlo, vpabsd, vpabsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vpabsd_xhik7xhi, vpabsd, vpabsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpabsd_xhik7ld, vpabsd, vpabsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpabsd_xhik7bcst, vpabsd, vpabsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpabsd_ylok0ld, vpabsd, vpabsd_mask, 0, REGARG(YMM0), REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vpabsd_ylok0bcst, vpabsd, vpabsd_mask, 0, REGARG(YMM0), REGARG(K0), MEMARG(OPSZ_4))
OPCODE(vpabsd_ylok0ylo, vpabsd, vpabsd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vpabsd_yhik7yhi, vpabsd, vpabsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpabsd_yhik7ld, vpabsd, vpabsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpabsd_yhik7bcst, vpabsd, vpabsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpabsd_zlok0ld, vpabsd, vpabsd_mask, 0, REGARG(ZMM0), REGARG(K0), MEMARG(OPSZ_64))
OPCODE(vpabsd_zlok0bcst, vpabsd, vpabsd_mask, 0, REGARG(ZMM0), REGARG(K0), MEMARG(OPSZ_4))
OPCODE(vpabsd_zlok0zlo, vpabsd, vpabsd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vpabsd_zhik7zhi, vpabsd, vpabsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vpabsd_zhik7ld, vpabsd, vpabsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpabsd_zhik7bcst, vpabsd, vpabsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpabsq_xlok0ld, vpabsq, vpabsq_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vpabsq_xlok0bcst, vpabsq, vpabsq_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vpabsq_xlok0xlo, vpabsq, vpabsq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vpabsq_xhik7xhi, vpabsq, vpabsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpabsq_xhik7ld, vpabsq, vpabsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpabsq_xhik7bcst, vpabsq, vpabsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpabsq_ylok0ld, vpabsq, vpabsq_mask, 0, REGARG(YMM0), REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vpabsq_ylok0bcst, vpabsq, vpabsq_mask, 0, REGARG(YMM0), REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vpabsq_ylok0ylo, vpabsq, vpabsq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vpabsq_yhik7yhi, vpabsq, vpabsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpabsq_yhik7ld, vpabsq, vpabsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpabsq_yhik7bcst, vpabsq, vpabsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpabsq_zlok0ld, vpabsq, vpabsq_mask, 0, REGARG(ZMM0), REGARG(K0), MEMARG(OPSZ_64))
OPCODE(vpabsq_zlok0bcst, vpabsq, vpabsq_mask, 0, REGARG(ZMM0), REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vpabsq_zlok0zlo, vpabsq, vpabsq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vpabsq_zhik7zhi, vpabsq, vpabsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vpabsq_zhik7ld, vpabsq, vpabsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpabsq_zhik7bcst, vpabsq, vpabsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vbroadcastsd_ylok0xlo, vbroadcastsd, vbroadcastsd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcastsd_ylok0ld, vbroadcastsd, vbroadcastsd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vbroadcastsd_yhik7xhi, vbroadcastsd, vbroadcastsd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcastsd_yhik7ld, vbroadcastsd, vbroadcastsd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcastsd_zlok0xlo, vbroadcastsd, vbroadcastsd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcastsd_zlok0ld, vbroadcastsd, vbroadcastsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vbroadcastsd_zhik7xhi, vbroadcastsd, vbroadcastsd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcastsd_zhik7ld, vbroadcastsd, vbroadcastsd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcastss_xlok0xlo, vbroadcastss, vbroadcastss_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vbroadcastss_xlok0ld, vbroadcastss, vbroadcastss_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vbroadcastss_xhik7xhi, vbroadcastss, vbroadcastss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vbroadcastss_xhik7ld, vbroadcastss, vbroadcastss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vbroadcastss_ylok0xlo, vbroadcastss, vbroadcastss_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vbroadcastss_ylok0ld, vbroadcastss, vbroadcastss_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vbroadcastss_yhik7xhi, vbroadcastss, vbroadcastss_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vbroadcastss_yhik7ld, vbroadcastss, vbroadcastss_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vbroadcastss_zlok0xlo, vbroadcastss, vbroadcastss_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vbroadcastss_zlok0ld, vbroadcastss, vbroadcastss_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vbroadcastss_zhik7xhi, vbroadcastss, vbroadcastss_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vbroadcastss_zhik7ld, vbroadcastss, vbroadcastss_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vbroadcastf32x2_ylok0xlo, vbroadcastf32x2, vbroadcastf32x2_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcastf32x2_ylok0ld, vbroadcastf32x2, vbroadcastf32x2_mask, 0, REGARG(YMM0),
       REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vbroadcastf32x2_yhik7xhi, vbroadcastf32x2, vbroadcastf32x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcastf32x2_yhik7ld, vbroadcastf32x2, vbroadcastf32x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcastf32x2_zlok0xlo, vbroadcastf32x2, vbroadcastf32x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcastf32x2_zlok0ld, vbroadcastf32x2, vbroadcastf32x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vbroadcastf32x2_zhik7xhi, vbroadcastf32x2, vbroadcastf32x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcastf32x2_zhik7ld, vbroadcastf32x2, vbroadcastf32x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcastf32x4_ylok0ld, vbroadcastf32x4, vbroadcastf32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcastf32x4_yhik7ld, vbroadcastf32x4, vbroadcastf32x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcastf32x4_zlok0ld, vbroadcastf32x4, vbroadcastf32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcastf32x4_zhik7ld, vbroadcastf32x4, vbroadcastf32x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcastf64x2_ylok0ld, vbroadcastf64x2, vbroadcastf64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcastf64x2_yhik7ld, vbroadcastf64x2, vbroadcastf64x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcastf64x2_zlok0ld, vbroadcastf64x2, vbroadcastf64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcastf64x2_zhik7ld, vbroadcastf64x2, vbroadcastf64x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcastf32x8_zlok0ld, vbroadcastf32x8, vbroadcastf32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vbroadcastf32x8_zhik7ld, vbroadcastf32x8, vbroadcastf32x8_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vbroadcastf64x4_zlok0ld, vbroadcastf64x4, vbroadcastf64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vbroadcastf64x4_zhik7ld, vbroadcastf64x4, vbroadcastf64x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vpbroadcastb_xlok0ld, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_1))
OPCODE(vpbroadcastb_xlok0xlo, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_1))
OPCODE(vpbroadcastb_xhik7ld, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_1))
OPCODE(vpbroadcastb_xhik7xhi, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_1))
OPCODE(vpbroadcastb_ylok0ld, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_1))
OPCODE(vpbroadcastb_ylok0xlo, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_1))
OPCODE(vpbroadcastb_yhik7ld, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_1))
OPCODE(vpbroadcastb_yhik7xhi, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_1))
OPCODE(vpbroadcastb_zlok0ld, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_1))
OPCODE(vpbroadcastb_zlok0xlo, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_1))
OPCODE(vpbroadcastb_zhik7ld, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_1))
OPCODE(vpbroadcastb_zhik7xhi, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_1))
OPCODE(vpbroadcastw_xlok0ld, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_2))
OPCODE(vpbroadcastw_xlok0xlo, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpbroadcastw_xhik7ld, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_2))
OPCODE(vpbroadcastw_xhik7xhi, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_2))
OPCODE(vpbroadcastw_ylok0ld, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_2))
OPCODE(vpbroadcastw_ylok0xlo, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpbroadcastw_yhik7ld, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_2))
OPCODE(vpbroadcastw_yhik7xhi, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_2))
OPCODE(vpbroadcastw_zlok0ld, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_2))
OPCODE(vpbroadcastw_zlok0xlo, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpbroadcastw_zhik7ld, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_2))
OPCODE(vpbroadcastw_zhik7xhi, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_2))
OPCODE(vpbroadcastd_xlok0ld, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpbroadcastd_xlok0xlo, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpbroadcastd_xhik7ld, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vpbroadcastd_xhik7xhi, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpbroadcastd_ylok0ld, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpbroadcastd_ylok0xlo, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpbroadcastd_yhik7ld, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vpbroadcastd_yhik7xhi, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpbroadcastd_zlok0ld, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpbroadcastd_zlok0xlo, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpbroadcastd_zhik7ld, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vpbroadcastd_zhik7xhi, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpbroadcastq_xlok0ld, vpbroadcastq, vpbroadcastq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpbroadcastq_xlok0xlo, vpbroadcastq, vpbroadcastq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpbroadcastq_xhik7ld, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vpbroadcastq_xhik7xhi, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpbroadcastq_ylok0ld, vpbroadcastq, vpbroadcastq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpbroadcastq_ylok0xlo, vpbroadcastq, vpbroadcastq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpbroadcastq_yhik7ld, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vpbroadcastq_yhik7xhi, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpbroadcastq_zlok0ld, vpbroadcastq, vpbroadcastq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpbroadcastq_zlok0xlo, vpbroadcastq, vpbroadcastq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpbroadcastq_zhik7ld, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vpbroadcastq_zhik7xhi, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpbroadcastb_xlok0r, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastb_xhik7r, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastb_ylok0r, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastb_yhik7r, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastb_zlok0r, vpbroadcastb, vpbroadcastb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastb_zhik7r, vpbroadcastb, vpbroadcastb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastw_xlok0r, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastw_xhik7r, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastw_ylok0r, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastw_yhik7r, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastw_zlok0r, vpbroadcastw, vpbroadcastw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastw_zhik7r, vpbroadcastw, vpbroadcastw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastd_xlok0r, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastd_xhik7r, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastd_ylok0r, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastd_yhik7r, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastd_zlok0r, vpbroadcastd, vpbroadcastd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(EAX))
OPCODE(vpbroadcastd_zhik7r, vpbroadcastd, vpbroadcastd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(EAX))
OPCODE(vpbroadcastq_xlok0r, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K0), REGARG(RAX))
OPCODE(vpbroadcastq_xhik7r, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(RAX))
OPCODE(vpbroadcastq_ylok0r, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K0), REGARG(RAX))
OPCODE(vpbroadcastq_yhik7r, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(RAX))
OPCODE(vpbroadcastq_zlok0r, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K0), REGARG(RAX))
OPCODE(vpbroadcastq_zhik7r, vpbroadcastq, vpbroadcastq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(RAX))
OPCODE(vbroadcasti32x2_xlok0ld, vbroadcasti32x2, vbroadcasti32x2_mask, 0, REGARG(XMM0),
       REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vbroadcasti32x2_xlok0xlo, vbroadcasti32x2, vbroadcasti32x2_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcasti32x2_xhik7ld, vbroadcasti32x2, vbroadcasti32x2_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcasti32x2_xhik7xhi, vbroadcasti32x2, vbroadcasti32x2_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcasti32x2_ylok0ld, vbroadcasti32x2, vbroadcasti32x2_mask, 0, REGARG(YMM0),
       REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vbroadcasti32x2_ylok0xlo, vbroadcasti32x2, vbroadcasti32x2_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcasti32x2_yhik7ld, vbroadcasti32x2, vbroadcasti32x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcasti32x2_yhik7xhi, vbroadcasti32x2, vbroadcasti32x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcasti32x2_zlok0ld, vbroadcasti32x2, vbroadcasti32x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vbroadcasti32x2_zlok0xlo, vbroadcasti32x2, vbroadcasti32x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vbroadcasti32x2_zhik7ld, vbroadcasti32x2, vbroadcasti32x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vbroadcasti32x2_zhik7xhi, vbroadcasti32x2, vbroadcasti32x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vbroadcasti32x4_ylok0ld, vbroadcasti32x4, vbroadcasti32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcasti32x4_yhik7ld, vbroadcasti32x4, vbroadcasti32x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcasti32x4_zlok0ld, vbroadcasti32x4, vbroadcasti32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcasti32x4_zhik7ld, vbroadcasti32x4, vbroadcasti32x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcasti64x2_ylok0ld, vbroadcasti64x2, vbroadcasti64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcasti64x2_yhik7ld, vbroadcasti64x2, vbroadcasti64x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcasti64x2_zlok0ld, vbroadcasti64x2, vbroadcasti64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_16))
OPCODE(vbroadcasti64x2_zhik7ld, vbroadcasti64x2, vbroadcasti64x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vbroadcasti32x8_zlok0ld, vbroadcasti32x8, vbroadcasti32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vbroadcasti32x8_zhik7ld, vbroadcasti32x8, vbroadcasti32x8_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vbroadcasti64x4_zlok0ld, vbroadcasti64x4, vbroadcasti64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), MEMARG(OPSZ_32))
OPCODE(vbroadcasti64x4_zhik7ld, vbroadcasti64x4, vbroadcasti64x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcompressps_xlok0st, vcompressps, vcompressps_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vcompressps_xlok0xlo, vcompressps, vcompressps_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vcompressps_xhik7xhi, vcompressps, vcompressps_mask, X64_ONLY, REGARG(XMM31),
       REGARG(K7), REGARG(XMM16))
OPCODE(vcompressps_ylok0st, vcompressps, vcompressps_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vcompressps_ylok0ylo, vcompressps, vcompressps_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vcompressps_yhik7yhi, vcompressps, vcompressps_mask, X64_ONLY, REGARG(YMM31),
       REGARG(K7), REGARG(YMM16))
OPCODE(vcompressps_zlok0st, vcompressps, vcompressps_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vcompressps_zlok0zlo, vcompressps, vcompressps_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vcompressps_zhik7zhi, vcompressps, vcompressps_mask, X64_ONLY, REGARG(ZMM31),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vcompresspd_xlok0st, vcompresspd, vcompresspd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vcompresspd_xlok0xlo, vcompresspd, vcompresspd_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vcompresspd_xhik7xhi, vcompresspd, vcompresspd_mask, X64_ONLY, REGARG(XMM31),
       REGARG(K7), REGARG(XMM16))
OPCODE(vcompresspd_ylok0st, vcompresspd, vcompresspd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vcompresspd_ylok0ylo, vcompresspd, vcompresspd_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vcompresspd_yhik7yhi, vcompresspd, vcompresspd_mask, X64_ONLY, REGARG(YMM31),
       REGARG(K7), REGARG(YMM16))
OPCODE(vcompresspd_zlok0st, vcompresspd, vcompresspd_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vcompresspd_zlok0zlo, vcompresspd, vcompresspd_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vcompresspd_zhik7zhi, vcompresspd, vcompresspd_mask, X64_ONLY, REGARG(ZMM31),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vexpandps_xlok0st, vexpandps, vexpandps_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vexpandps_xlok0xlo, vexpandps, vexpandps_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vexpandps_xhik7xhi, vexpandps, vexpandps_mask, X64_ONLY, REGARG(XMM31), REGARG(K7),
       REGARG(XMM16))
OPCODE(vexpandps_ylok0st, vexpandps, vexpandps_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vexpandps_ylok0ylo, vexpandps, vexpandps_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vexpandps_yhik7yhi, vexpandps, vexpandps_mask, X64_ONLY, REGARG(YMM31), REGARG(K7),
       REGARG(YMM16))
OPCODE(vexpandps_zlok0st, vexpandps, vexpandps_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vexpandps_zlok0zlo, vexpandps, vexpandps_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vexpandps_zhik7zhi, vexpandps, vexpandps_mask, X64_ONLY, REGARG(ZMM31), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vexpandpd_xlok0st, vexpandpd, vexpandpd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vexpandpd_xlok0xlo, vexpandpd, vexpandpd_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vexpandpd_xhik7xhi, vexpandpd, vexpandpd_mask, X64_ONLY, REGARG(XMM31), REGARG(K7),
       REGARG(XMM16))
OPCODE(vexpandpd_ylok0st, vexpandpd, vexpandpd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vexpandpd_ylok0ylo, vexpandpd, vexpandpd_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vexpandpd_yhik7yhi, vexpandpd, vexpandpd_mask, X64_ONLY, REGARG(YMM31), REGARG(K7),
       REGARG(YMM16))
OPCODE(vexpandpd_zlok0st, vexpandpd, vexpandpd_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vexpandpd_zlok0zlo, vexpandpd, vexpandpd_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vexpandpd_zhik7zhi, vexpandpd, vexpandpd_mask, X64_ONLY, REGARG(ZMM31), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vgetexpps_xlok0ld, vgetexpps, vgetexpps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vgetexpps_xlok0bcst, vgetexpps, vgetexpps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vgetexpps_xlok0xlo, vgetexpps, vgetexpps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vgetexpps_xhik7xhi, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vgetexpps_xhik7ld, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vgetexpps_xhik7bcst, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vgetexpps_ylok0ld, vgetexpps, vgetexpps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vgetexpps_ylok0bcst, vgetexpps, vgetexpps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vgetexpps_ylok0ylo, vgetexpps, vgetexpps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vgetexpps_yhik7yhi, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vgetexpps_yhik7ld, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vgetexpps_yhik7bcst, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vgetexpps_zlok0ld, vgetexpps, vgetexpps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vgetexpps_zlok0bcst, vgetexpps, vgetexpps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vgetexpps_zlok0zlo, vgetexpps, vgetexpps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vgetexpps_zhik7zhi, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vgetexpps_zhik7ld, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vgetexpps_zhik7bcst, vgetexpps, vgetexpps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vgetexppd_xlok0ld, vgetexppd, vgetexppd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vgetexppd_xlok0bcst, vgetexppd, vgetexppd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vgetexppd_xlok0xlo, vgetexppd, vgetexppd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vgetexppd_xhik7xhi, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vgetexppd_xhik7ld, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vgetexppd_xhik7bcst, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vgetexppd_ylok0ld, vgetexppd, vgetexppd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vgetexppd_ylok0bcst, vgetexppd, vgetexppd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vgetexppd_ylok0ylo, vgetexppd, vgetexppd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vgetexppd_yhik7yhi, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vgetexppd_yhik7ld, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vgetexppd_yhik7bcst, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vgetexppd_zlok0ld, vgetexppd, vgetexppd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vgetexppd_zlok0bcst, vgetexppd, vgetexppd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vgetexppd_zlok0zlo, vgetexppd, vgetexppd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vgetexppd_zhik7zhi, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vgetexppd_zhik7ld, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vgetexppd_zhik7bcst, vgetexppd, vgetexppd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vpcompressd_xlok0st, vpcompressd, vpcompressd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressd_xlok0xlo, vpcompressd, vpcompressd_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressd_xhik7xhi, vpcompressd, vpcompressd_mask, X64_ONLY, REGARG(XMM31),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpcompressd_ylok0st, vpcompressd, vpcompressd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressd_ylok0ylo, vpcompressd, vpcompressd_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressd_yhik7yhi, vpcompressd, vpcompressd_mask, X64_ONLY, REGARG(YMM31),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpcompressd_zlok0st, vpcompressd, vpcompressd_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressd_zlok0zlo, vpcompressd, vpcompressd_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressd_zhik7zhi, vpcompressd, vpcompressd_mask, X64_ONLY, REGARG(ZMM31),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpcompressq_xlok0st, vpcompressq, vpcompressq_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressq_xlok0xlo, vpcompressq, vpcompressq_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpcompressq_xhik7xhi, vpcompressq, vpcompressq_mask, X64_ONLY, REGARG(XMM31),
       REGARG(K7), REGARG(XMM16))
OPCODE(vpcompressq_ylok0st, vpcompressq, vpcompressq_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressq_ylok0ylo, vpcompressq, vpcompressq_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpcompressq_yhik7yhi, vpcompressq, vpcompressq_mask, X64_ONLY, REGARG(YMM31),
       REGARG(K7), REGARG(YMM16))
OPCODE(vpcompressq_zlok0st, vpcompressq, vpcompressq_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressq_zlok0zlo, vpcompressq, vpcompressq_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpcompressq_zhik7zhi, vpcompressq, vpcompressq_mask, X64_ONLY, REGARG(ZMM31),
       REGARG(K7), REGARG(ZMM16))
OPCODE(vpexpandd_xlok0ld, vpexpandd, vpexpandd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpexpandd_xlok0xlo, vpexpandd, vpexpandd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vpexpandd_xhik7xhi, vpexpandd, vpexpandd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpexpandd_ylok0ld, vpexpandd, vpexpandd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpexpandd_ylok0ylo, vpexpandd, vpexpandd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vpexpandd_yhik7yhi, vpexpandd, vpexpandd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpexpandd_zlok0ld, vpexpandd, vpexpandd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vpexpandd_zlok0zlo, vpexpandd, vpexpandd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vpexpandd_zhik7zhi, vpexpandd, vpexpandd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vpexpandq_xlok0ld, vpexpandq, vpexpandq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpexpandq_xlok0xlo, vpexpandq, vpexpandq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vpexpandq_xhik7xhi, vpexpandq, vpexpandq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vpexpandq_ylok0ld, vpexpandq, vpexpandq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpexpandq_ylok0ylo, vpexpandq, vpexpandq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vpexpandq_yhik7yhi, vpexpandq, vpexpandq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vpexpandq_zlok0ld, vpexpandq, vpexpandq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vpexpandq_zlok0zlo, vpexpandq, vpexpandq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vpexpandq_zhik7zhi, vpexpandq, vpexpandq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vrsqrt14ps_xlok0ld, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vrsqrt14ps_xlok0bcst, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrsqrt14ps_xlok0xlo, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vrsqrt14ps_xhik7xhi, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vrsqrt14ps_xhik7ld, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vrsqrt14ps_xhik7bcst, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrsqrt14ps_ylok0ld, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vrsqrt14ps_ylok0bcst, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrsqrt14ps_ylok0ylo, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vrsqrt14ps_yhik7yhi, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vrsqrt14ps_yhik7ld, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vrsqrt14ps_yhik7bcst, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrsqrt14ps_zlok0ld, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrsqrt14ps_zlok0bcst, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrsqrt14ps_zlok0zlo, vrsqrt14ps, vrsqrt14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrsqrt14ps_zhik7zhi, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vrsqrt14ps_zhik7ld, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vrsqrt14ps_zhik7bcst, vrsqrt14ps, vrsqrt14ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrsqrt14pd_xlok0ld, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vrsqrt14pd_xlok0bcst, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrsqrt14pd_xlok0xlo, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vrsqrt14pd_xhik7xhi, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vrsqrt14pd_xhik7ld, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vrsqrt14pd_xhik7bcst, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrsqrt14pd_ylok0ld, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vrsqrt14pd_ylok0bcst, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrsqrt14pd_ylok0ylo, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vrsqrt14pd_yhik7yhi, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vrsqrt14pd_yhik7ld, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vrsqrt14pd_yhik7bcst, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrsqrt14pd_zlok0ld, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrsqrt14pd_zlok0bcst, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrsqrt14pd_zlok0zlo, vrsqrt14pd, vrsqrt14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrsqrt14pd_zhik7zhi, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vrsqrt14pd_zhik7ld, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vrsqrt14pd_zhik7bcst, vrsqrt14pd, vrsqrt14pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrsqrt28ps_zlok0ld, vrsqrt28ps, vrsqrt28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrsqrt28ps_zlok0bcst, vrsqrt28ps, vrsqrt28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrsqrt28ps_zlok0zlo, vrsqrt28ps, vrsqrt28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrsqrt28ps_zhik7zhi, vrsqrt28ps, vrsqrt28ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vrsqrt28pd_zlok0ld, vrsqrt28pd, vrsqrt28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrsqrt28pd_zlok0bcst, vrsqrt28pd, vrsqrt28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrsqrt28pd_zlok0zlo, vrsqrt28pd, vrsqrt28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrsqrt28pd_zhik7zhi, vrsqrt28pd, vrsqrt28pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vexp2ps_zlok0ld, vexp2ps, vexp2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vexp2ps_zlok0bcst, vexp2ps, vexp2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vexp2ps_zlok0zlo, vexp2ps, vexp2ps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vexp2ps_zhik7zhi, vexp2ps, vexp2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vexp2pd_zlok0ld, vexp2pd, vexp2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vexp2pd_zlok0bcst, vexp2pd, vexp2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vexp2pd_zlok0zlo, vexp2pd, vexp2pd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vexp2pd_zhik7zhi, vexp2pd, vexp2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
