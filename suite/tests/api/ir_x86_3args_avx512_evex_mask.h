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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 1 source. */
OPCODE(vmovups_zlok0zlo, vmovups, vmovups_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovups_zhik0zlo, vmovups, vmovups_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovups_zlok0zhi, vmovups, vmovups_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovups_zlok7zlo, vmovups, vmovups_mask, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovups_zhik7zlo, vmovups, vmovups_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovups_zlok7zhi, vmovups, vmovups_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovups_zlokm, vmovups, vmovups_mask, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovups_mkzlo, vmovups, vmovups_mask, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovups_ylok0ylo, vmovups, vmovups_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovups_yhik0ylo, vmovups, vmovups_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovups_ylok0yhi, vmovups, vmovups_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovups_ylok7ylo, vmovups, vmovups_mask, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovups_yhik7ylo, vmovups, vmovups_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovups_ylok7yhi, vmovups, vmovups_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovups_ylokm, vmovups, vmovups_mask, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovups_mkylo, vmovups, vmovups_mask, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovups_xlok0xlo, vmovups, vmovups_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovups_xhik0xlo, vmovups, vmovups_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovups_xlok0xhi, vmovups, vmovups_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovups_xlok7xlo, vmovups, vmovups_mask, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovups_xhik7xlo, vmovups, vmovups_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovups_xlok7xhi, vmovups, vmovups_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovups_xlokm, vmovups, vmovups_mask, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovups_mkxlo, vmovups, vmovups_mask, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovupd_zlok0zlo, vmovupd, vmovupd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovupd_zhik0zlo, vmovupd, vmovupd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovupd_zlok0zhi, vmovupd, vmovupd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovupd_zlok7zlo, vmovupd, vmovupd_mask, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovupd_zhik7zlo, vmovupd, vmovupd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovupd_zlok7zhi, vmovupd, vmovupd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovupd_zlokm, vmovupd, vmovupd_mask, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovupd_mkzlo, vmovupd, vmovupd_mask, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovupd_ylok0ylo, vmovupd, vmovupd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovupd_yhik0ylo, vmovupd, vmovupd_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovupd_ylok0yhi, vmovupd, vmovupd_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovupd_ylok7ylo, vmovupd, vmovupd_mask, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovupd_yhik7ylo, vmovupd, vmovupd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovupd_ylok7yhi, vmovupd, vmovupd_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovupd_ylokm, vmovupd, vmovupd_mask, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovupd_mkylo, vmovupd, vmovupd_mask, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovupd_xlok0xlo, vmovupd, vmovupd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovupd_xhik0xlo, vmovupd, vmovupd_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovupd_xlok0xhi, vmovupd, vmovupd_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovupd_xlok7xlo, vmovupd, vmovupd_mask, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovupd_xhik7xlo, vmovupd, vmovupd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovupd_xlok7xhi, vmovupd, vmovupd_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovupd_xlokm, vmovupd, vmovupd_mask, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovupd_mkxlo, vmovupd, vmovupd_mask, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovaps_zlok0zlo, vmovaps, vmovaps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovaps_zhik0zlo, vmovaps, vmovaps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovaps_zlok0zhi, vmovaps, vmovaps_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovaps_zlok7zlo, vmovaps, vmovaps_mask, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovaps_zhik7zlo, vmovaps, vmovaps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovaps_zlok7zhi, vmovaps, vmovaps_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovaps_zlokm, vmovaps, vmovaps_mask, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovaps_mkzlo, vmovaps, vmovaps_mask, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovaps_ylok0ylo, vmovaps, vmovaps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovaps_yhik0ylo, vmovaps, vmovaps_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovaps_ylok0yhi, vmovaps, vmovaps_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovaps_ylok7ylo, vmovaps, vmovaps_mask, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovaps_yhik7ylo, vmovaps, vmovaps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovaps_ylok7yhi, vmovaps, vmovaps_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovaps_ylokm, vmovaps, vmovaps_mask, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovaps_mkylo, vmovaps, vmovaps_mask, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovaps_xlok0xlo, vmovaps, vmovaps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovaps_xhik0xlo, vmovaps, vmovaps_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovaps_xlok0xhi, vmovaps, vmovaps_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovaps_xlok7xlo, vmovaps, vmovaps_mask, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovaps_xhik7xlo, vmovaps, vmovaps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovaps_xlok7xhi, vmovaps, vmovaps_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovaps_xlokm, vmovaps, vmovaps_mask, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovaps_mkxlo, vmovaps, vmovaps_mask, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovapd_zlok0zlo, vmovapd, vmovapd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovapd_zhik0zlo, vmovapd, vmovapd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovapd_zlok0zhi, vmovapd, vmovapd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovapd_zlok7zlo, vmovapd, vmovapd_mask, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovapd_zhik7zlo, vmovapd, vmovapd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovapd_zlok7zhi, vmovapd, vmovapd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovapd_zlokm, vmovapd, vmovapd_mask, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovapd_mkzlo, vmovapd, vmovapd_mask, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovapd_ylok0ylo, vmovapd, vmovapd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovapd_yhik0ylo, vmovapd, vmovapd_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovapd_ylok0yhi, vmovapd, vmovapd_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovapd_ylok7ylo, vmovapd, vmovapd_mask, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovapd_yhik7ylo, vmovapd, vmovapd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovapd_ylok7yhi, vmovapd, vmovapd_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovapd_ylokm, vmovapd, vmovapd_mask, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovapd_mkylo, vmovapd, vmovapd_mask, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovapd_xlok0xlo, vmovapd, vmovapd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovapd_xhik0xlo, vmovapd, vmovapd_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovapd_xlok0xhi, vmovapd, vmovapd_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovapd_xlok7xlo, vmovapd, vmovapd_mask, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovapd_xhik7xlo, vmovapd, vmovapd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovapd_xlok7xhi, vmovapd, vmovapd_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovapd_xlokm, vmovapd, vmovapd_mask, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovapd_mkxlo, vmovapd, vmovapd_mask, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovdqa32_zlok0zlo, vmovdqa32, vmovdqa32_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vmovdqa32_zhik0zlo, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovdqa32_zlok0zhi, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovdqa32_zlok7zlo, vmovdqa32, vmovdqa32_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1))
OPCODE(vmovdqa32_zhik7zlo, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqa32_zlok7zhi, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovdqa32_zlokm, vmovdqa32, vmovdqa32_mask, 0, REGARG(ZMM0), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovdqa32_mkzlo, vmovdqa32, vmovdqa32_mask, 0, MEMARG(OPSZ_64), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqa32_ylok0ylo, vmovdqa32, vmovdqa32_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vmovdqa32_yhik0ylo, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovdqa32_ylok0yhi, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovdqa32_ylok7ylo, vmovdqa32, vmovdqa32_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1))
OPCODE(vmovdqa32_yhik7ylo, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqa32_ylok7yhi, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovdqa32_ylokm, vmovdqa32, vmovdqa32_mask, 0, REGARG(YMM0), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovdqa32_mkylo, vmovdqa32, vmovdqa32_mask, 0, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqa32_xlok0xlo, vmovdqa32, vmovdqa32_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vmovdqa32_xhik0xlo, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovdqa32_xlok0xhi, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovdqa32_xlok7xlo, vmovdqa32, vmovdqa32_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1))
OPCODE(vmovdqa32_xhik7xlo, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqa32_xlok7xhi, vmovdqa32, vmovdqa32_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovdqa32_xlokm, vmovdqa32, vmovdqa32_mask, 0, REGARG(XMM0), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovdqa32_mkxlo, vmovdqa32, vmovdqa32_mask, 0, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqa64_zlok0zlo, vmovdqa64, vmovdqa64_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vmovdqa64_zhik0zlo, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovdqa64_zlok0zhi, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovdqa64_zlok7zlo, vmovdqa64, vmovdqa64_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1))
OPCODE(vmovdqa64_zhik7zlo, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqa64_zlok7zhi, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovdqa64_zlokm, vmovdqa64, vmovdqa64_mask, 0, REGARG(ZMM0), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovdqa64_mkzlo, vmovdqa64, vmovdqa64_mask, 0, MEMARG(OPSZ_64), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqa64_ylok0ylo, vmovdqa64, vmovdqa64_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vmovdqa64_yhik0ylo, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovdqa64_ylok0yhi, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovdqa64_ylok7ylo, vmovdqa64, vmovdqa64_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1))
OPCODE(vmovdqa64_yhik7ylo, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqa64_ylok7yhi, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovdqa64_ylokm, vmovdqa64, vmovdqa64_mask, 0, REGARG(YMM0), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovdqa64_mkylo, vmovdqa64, vmovdqa64_mask, 0, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqa64_xlok0xlo, vmovdqa64, vmovdqa64_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vmovdqa64_xhik0xlo, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovdqa64_xlok0xhi, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovdqa64_xlok7xlo, vmovdqa64, vmovdqa64_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1))
OPCODE(vmovdqa64_xhik7xlo, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqa64_xlok7xhi, vmovdqa64, vmovdqa64_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovdqa64_xlokm, vmovdqa64, vmovdqa64_mask, 0, REGARG(XMM0), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovdqa64_mkxlo, vmovdqa64, vmovdqa64_mask, 0, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu8_zlok0zlo, vmovdqu8, vmovdqu8_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vmovdqu8_zhik0zlo, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovdqu8_zlok0zhi, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovdqu8_zlok7zlo, vmovdqu8, vmovdqu8_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1))
OPCODE(vmovdqu8_zhik7zlo, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu8_zlok7zhi, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovdqu8_zlokm, vmovdqu8, vmovdqu8_mask, 0, REGARG(ZMM0), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovdqu8_mkzlo, vmovdqu8, vmovdqu8_mask, 0, MEMARG(OPSZ_64), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu8_ylok0ylo, vmovdqu8, vmovdqu8_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vmovdqu8_yhik0ylo, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovdqu8_ylok0yhi, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovdqu8_ylok7ylo, vmovdqu8, vmovdqu8_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1))
OPCODE(vmovdqu8_yhik7ylo, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu8_ylok7yhi, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovdqu8_ylokm, vmovdqu8, vmovdqu8_mask, 0, REGARG(YMM0), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovdqu8_mkylo, vmovdqu8, vmovdqu8_mask, 0, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu8_xlok0xlo, vmovdqu8, vmovdqu8_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vmovdqu8_xhik0xlo, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovdqu8_xlok0xhi, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovdqu8_xlok7xlo, vmovdqu8, vmovdqu8_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1))
OPCODE(vmovdqu8_xhik7xlo, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu8_xlok7xhi, vmovdqu8, vmovdqu8_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovdqu8_xlokm, vmovdqu8, vmovdqu8_mask, 0, REGARG(XMM0), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovdqu8_mkxlo, vmovdqu8, vmovdqu8_mask, 0, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu16_zlok0zlo, vmovdqu16, vmovdqu16_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vmovdqu16_zhik0zlo, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovdqu16_zlok0zhi, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovdqu16_zlok7zlo, vmovdqu16, vmovdqu16_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1))
OPCODE(vmovdqu16_zhik7zlo, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu16_zlok7zhi, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovdqu16_zlokm, vmovdqu16, vmovdqu16_mask, 0, REGARG(ZMM0), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovdqu16_mkzlo, vmovdqu16, vmovdqu16_mask, 0, MEMARG(OPSZ_64), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu16_ylok0ylo, vmovdqu16, vmovdqu16_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vmovdqu16_yhik0ylo, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovdqu16_ylok0yhi, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovdqu16_ylok7ylo, vmovdqu16, vmovdqu16_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1))
OPCODE(vmovdqu16_yhik7ylo, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu16_ylok7yhi, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovdqu16_ylokm, vmovdqu16, vmovdqu16_mask, 0, REGARG(YMM0), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovdqu16_mkylo, vmovdqu16, vmovdqu16_mask, 0, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu16_xlok0xlo, vmovdqu16, vmovdqu16_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vmovdqu16_xhik0xlo, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovdqu16_xlok0xhi, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovdqu16_xlok7xlo, vmovdqu16, vmovdqu16_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1))
OPCODE(vmovdqu16_xhik7xlo, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu16_xlok7xhi, vmovdqu16, vmovdqu16_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovdqu16_xlokm, vmovdqu16, vmovdqu16_mask, 0, REGARG(XMM0), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovdqu16_mkxlo, vmovdqu16, vmovdqu16_mask, 0, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu32_zlok0zlo, vmovdqu32, vmovdqu32_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vmovdqu32_zhik0zlo, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovdqu32_zlok0zhi, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovdqu32_zlok7zlo, vmovdqu32, vmovdqu32_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1))
OPCODE(vmovdqu32_zhik7zlo, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu32_zlok7zhi, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovdqu32_zlokm, vmovdqu32, vmovdqu32_mask, 0, REGARG(ZMM0), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovdqu32_mkzlo, vmovdqu32, vmovdqu32_mask, 0, MEMARG(OPSZ_64), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu32_ylok0ylo, vmovdqu32, vmovdqu32_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vmovdqu32_yhik0ylo, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovdqu32_ylok0yhi, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovdqu32_ylok7ylo, vmovdqu32, vmovdqu32_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1))
OPCODE(vmovdqu32_yhik7ylo, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu32_ylok7yhi, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovdqu32_ylokm, vmovdqu32, vmovdqu32_mask, 0, REGARG(YMM0), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovdqu32_mkylo, vmovdqu32, vmovdqu32_mask, 0, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu32_xlok0xlo, vmovdqu32, vmovdqu32_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vmovdqu32_xhik0xlo, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovdqu32_xlok0xhi, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovdqu32_xlok7xlo, vmovdqu32, vmovdqu32_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1))
OPCODE(vmovdqu32_xhik7xlo, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu32_xlok7xhi, vmovdqu32, vmovdqu32_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovdqu32_xlokm, vmovdqu32, vmovdqu32_mask, 0, REGARG(XMM0), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovdqu32_mkxlo, vmovdqu32, vmovdqu32_mask, 0, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu64_zlok0zlo, vmovdqu64, vmovdqu64_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vmovdqu64_zhik0zlo, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovdqu64_zlok0zhi, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovdqu64_zlok7zlo, vmovdqu64, vmovdqu64_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1))
OPCODE(vmovdqu64_zhik7zlo, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu64_zlok7zhi, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovdqu64_zlokm, vmovdqu64, vmovdqu64_mask, 0, REGARG(ZMM0), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovdqu64_mkzlo, vmovdqu64, vmovdqu64_mask, 0, MEMARG(OPSZ_64), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovdqu64_ylok0ylo, vmovdqu64, vmovdqu64_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vmovdqu64_yhik0ylo, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovdqu64_ylok0yhi, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovdqu64_ylok7ylo, vmovdqu64, vmovdqu64_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1))
OPCODE(vmovdqu64_yhik7ylo, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu64_ylok7yhi, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovdqu64_ylokm, vmovdqu64, vmovdqu64_mask, 0, REGARG(YMM0), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovdqu64_mkylo, vmovdqu64, vmovdqu64_mask, 0, MEMARG(OPSZ_32), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovdqu64_xlok0xlo, vmovdqu64, vmovdqu64_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vmovdqu64_xhik0xlo, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovdqu64_xlok0xhi, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovdqu64_xlok7xlo, vmovdqu64, vmovdqu64_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1))
OPCODE(vmovdqu64_xhik7xlo, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovdqu64_xlok7xhi, vmovdqu64, vmovdqu64_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovdqu64_xlokm, vmovdqu64, vmovdqu64_mask, 0, REGARG(XMM0), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovdqu64_mkxlo, vmovdqu64, vmovdqu64_mask, 0, MEMARG(OPSZ_16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovss_xlok0ld, vmovss, vmovss_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_4))
OPCODE(vmovsd_xlok0ld, vmovsd, vmovsd_mask, 0, REGARG(XMM0), REGARG(K0), MEMARG(OPSZ_8))
OPCODE(vmovss_k0stxlo, vmovss, vmovss_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovsd_k0stxlo, vmovsd, vmovsd_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovss_xlok7ld, vmovss, vmovss_mask, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vmovsd_xlok7ld, vmovsd, vmovsd_mask, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vmovss_k7stxlo, vmovss, vmovss_mask, 0, MEMARG(OPSZ_4), REGARG(K7),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovsd_k7stxlo, vmovsd, vmovsd_mask, 0, MEMARG(OPSZ_8), REGARG(K7),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovss_xhik7ld, vmovss, vmovss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vmovsd_xhik7ld, vmovsd, vmovsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vmovss_k7stxhi, vmovss, vmovss_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vmovsd_k7stxhi, vmovsd, vmovsd_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vmovsldup_xlok0ld, vmovsldup, vmovsldup_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vmovsldup_xhik7ld, vmovsldup, vmovsldup_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovsldup_ylok0ld, vmovsldup, vmovsldup_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vmovsldup_yhik7ld, vmovsldup, vmovsldup_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovsldup_zlok0ld, vmovsldup, vmovsldup_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vmovsldup_zhik7ld, vmovsldup, vmovsldup_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vmovddup_xlok0ld, vmovddup, vmovddup_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vmovddup_xhik7ld, vmovddup, vmovddup_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vmovddup_ylok0ld, vmovddup, vmovddup_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vmovddup_yhik7ld, vmovddup, vmovddup_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovddup_zlok0ld, vmovddup, vmovddup_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vmovddup_zhik7ld, vmovddup, vmovddup_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovshdup_xlok0ld, vmovshdup, vmovshdup_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vmovshdup_xhik7ld, vmovshdup, vmovshdup_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovshdup_ylok0ld, vmovshdup, vmovshdup_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vmovshdup_yhik7ld, vmovshdup, vmovshdup_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovshdup_zlok0ld, vmovshdup, vmovshdup_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vmovshdup_zhik7ld, vmovshdup, vmovshdup_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2pd_xlok0ld, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2pd_xhik7ld, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2pd_ylok0ld, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2pd_yhik7ld, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2pd_zlok0ld, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2pd_zhik7ld, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2pd_xlok0xlo, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtps2pd_xhik7xhi, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM16))
OPCODE(vcvtps2pd_ylok0ylo, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtps2pd_yhik7yhi, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vcvtps2pd_zlok0zlo, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtps2pd_zhik7zhi, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vcvtpd2ps_xlok0ld, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2ps_xhik7ld, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2ps_ylok0ld, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2ps_yhik7ld, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2ps_zlok0ld, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2ps_zhik7ld, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2ps_xlok0xlo, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2ps_xhik7xhi, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM16))
OPCODE(vcvtpd2ps_ylok0ylo, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2ps_yhik7yhi, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM16))
OPCODE(vcvtpd2ps_zlok0zlo, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2ps_zhik7zhi, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vcvtdq2ps_xlok0ld, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtdq2ps_xlok0xlo, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtdq2ps_xhik7xhi, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtdq2ps_ylok0ld, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtdq2ps_ylok0ylo, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtdq2ps_yhik7yhi, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtdq2ps_zlok0ld, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtdq2ps_zlok0zlo, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtdq2ps_zhik7zhi, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvttps2dq_xlok0ld, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2dq_xlok0xlo, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttps2dq_xhik7xhi, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttps2dq_ylok0ld, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2dq_ylok0ylo, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttps2dq_yhik7yhi, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttps2dq_zlok0ld, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttps2dq_zlok0zlo, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttps2dq_zhik7zhi, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtdq2pd_xlok0ld, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtdq2pd_xlok0xlo, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtdq2pd_xhik7xhi, vcvtdq2pd, vcvtdq2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtdq2pd_ylok0ld, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtdq2pd_ylok0ylo, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtdq2pd_yhik7yhi, vcvtdq2pd, vcvtdq2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtdq2pd_zlok0ld, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtdq2pd_zlok0zlo, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtdq2pd_zhik7zhi, vcvtdq2pd, vcvtdq2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvttpd2dq_xlok0ld, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2dq_xlok0xlo, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2dq_xhik7xhi, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2dq_ylok0ld, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2dq_ylok0ylo, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2dq_yhik7yhi, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2dq_zlok0ld, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2dq_zlok0zlo, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2dq_zhik7zhi, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtpd2dq_xlok0ld, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2dq_xlok0xlo, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2dq_xhik7xhi, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtpd2dq_ylok0ld, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2dq_ylok0ylo, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2dq_yhik7yhi, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtpd2dq_zlok0ld, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2dq_zlok0zlo, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2dq_zhik7zhi, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtph2ps_xlok0ld, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtph2ps_xlok0xlo, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtph2ps_xhik7xhi, vcvtph2ps, vcvtph2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtph2ps_ylok0ld, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtph2ps_ylok0ylo, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtph2ps_yhik7yhi, vcvtph2ps, vcvtph2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtph2ps_zlok0ld, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtph2ps_zlok0zlo, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtph2ps_zhik7zhi, vcvtph2ps, vcvtph2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtpd2qq_xlok0ld, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2qq_xlok0xlo, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2qq_xhik7xhi, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtpd2qq_ylok0ld, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2qq_ylok0ylo, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2qq_yhik7yhi, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtpd2qq_zlok0ld, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2qq_zlok0zlo, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2qq_zhik7zhi, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtpd2udq_xlok0ld, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2udq_xlok0xlo, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2udq_xhik7xhi, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtpd2udq_ylok0ld, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2udq_ylok0ylo, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2udq_yhik7yhi, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtpd2udq_zlok0ld, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2udq_zlok0zlo, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2udq_zhik7zhi, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtpd2uqq_xlok0ld, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2uqq_xlok0xlo, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2uqq_xhik7xhi, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtpd2uqq_ylok0ld, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2uqq_ylok0ylo, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2uqq_yhik7yhi, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtpd2uqq_zlok0ld, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2uqq_zlok0zlo, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2uqq_zhik7zhi, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtps2qq_xlok0ld, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2qq_xlok0xlo, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtps2qq_xhik7xhi, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtps2qq_ylok0ld, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2qq_ylok0ylo, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtps2qq_yhik7yhi, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtps2qq_zlok0ld, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2qq_zlok0zlo, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtps2qq_zhik7zhi, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtps2uqq_xlok0ld, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2uqq_xlok0xlo, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtps2uqq_xhik7xhi, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtps2uqq_ylok0ld, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2uqq_ylok0ylo, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtps2uqq_yhik7yhi, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtps2uqq_zlok0ld, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2uqq_zlok0zlo, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtps2uqq_zhik7zhi, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtps2udq_xlok0ld, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2udq_xlok0xlo, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtps2udq_xhik7xhi, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtps2udq_ylok0ld, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2udq_ylok0ylo, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtps2udq_yhik7yhi, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtps2udq_zlok0ld, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2udq_zlok0zlo, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtps2udq_zhik7zhi, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttps2udq_xlok0ld, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2udq_xlok0xlo, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttps2udq_xhik7xhi, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttps2udq_ylok0ld, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2udq_ylok0ylo, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttps2udq_yhik7yhi, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttps2udq_zlok0ld, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttps2udq_zlok0zlo, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttps2udq_zhik7zhi, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2udq_xlok0ld, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2udq_xlok0xlo, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2udq_xhik7xhi, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2udq_ylok0ld, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2udq_ylok0ylo, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2udq_yhik7yhi, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2udq_zlok0ld, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2udq_zlok0zlo, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2udq_zhik7zhi, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttps2qq_xlok0ld, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2qq_xlok0xlo, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttps2qq_xhik7xhi, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttps2qq_ylok0ld, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2qq_ylok0ylo, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttps2qq_yhik7yhi, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttps2qq_zlok0ld, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttps2qq_zlok0zlo, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttps2qq_zhik7zhi, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2qq_xlok0ld, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2qq_xlok0xlo, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2qq_xhik7xhi, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2qq_ylok0ld, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2qq_ylok0ylo, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2qq_yhik7yhi, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2qq_zlok0ld, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2qq_zlok0zlo, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2qq_zhik7zhi, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttps2uqq_xlok0ld, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2uqq_xlok0xlo, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttps2uqq_xhik7xhi, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttps2uqq_ylok0ld, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2uqq_ylok0ylo, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttps2uqq_yhik7yhi, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttps2uqq_zlok0ld, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttps2uqq_zlok0zlo, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttps2uqq_zhik7zhi, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2uqq_xlok0ld, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2uqq_xlok0xlo, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2uqq_xhik7xhi, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2uqq_ylok0ld, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2uqq_ylok0ylo, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2uqq_yhik7yhi, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2uqq_zlok0ld, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2uqq_zlok0zlo, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2uqq_zhik7zhi, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtqq2ps_xlok0ld, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtqq2ps_xlok0xlo, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtqq2ps_xhik7xhi, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtqq2ps_ylok0ld, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtqq2ps_ylok0ylo, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtqq2ps_yhik7yhi, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtqq2ps_zlok0ld, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtqq2ps_zlok0zlo, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtqq2ps_zhik7zhi, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtqq2pd_xlok0ld, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtqq2pd_xlok0xlo, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtqq2pd_xhik7xhi, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtqq2pd_ylok0ld, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtqq2pd_ylok0ylo, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtqq2pd_yhik7yhi, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtqq2pd_zlok0ld, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtqq2pd_zlok0zlo, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtqq2pd_zhik7zhi, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtudq2ps_xlok0ld, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtudq2ps_xlok0xlo, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtudq2ps_xhik7xhi, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtudq2ps_ylok0ld, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtudq2ps_ylok0ylo, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtudq2ps_yhik7yhi, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtudq2ps_zlok0ld, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtudq2ps_zlok0zlo, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtudq2ps_zhik7zhi, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtudq2pd_xlok0ld, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtudq2pd_xlok0xlo, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtudq2pd_xhik7xhi, vcvtudq2pd, vcvtudq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtudq2pd_ylok0ld, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtudq2pd_ylok0ylo, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtudq2pd_yhik7yhi, vcvtudq2pd, vcvtudq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtudq2pd_zlok0ld, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtudq2pd_zlok0zlo, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtudq2pd_zhik7zhi, vcvtudq2pd, vcvtudq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtuqq2ps_xlok0ld, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtuqq2ps_xlok0xlo, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtuqq2ps_xhik7xhi, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtuqq2ps_ylok0ld, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtuqq2ps_ylok0ylo, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtuqq2ps_yhik7yhi, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtuqq2ps_zlok0ld, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtuqq2ps_zlok0zlo, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtuqq2ps_zhik7zhi, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtuqq2pd_xlok0ld, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtuqq2pd_xlok0xlo, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtuqq2pd_xhik7xhi, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtuqq2pd_ylok0ld, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtuqq2pd_ylok0ylo, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtuqq2pd_yhik7yhi, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtuqq2pd_zlok0ld, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtuqq2pd_zlok0zlo, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtuqq2pd_zhik7zhi, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vrcp14ps_xlok0xloxmm, vrcp14ps, vrcp14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vrcp14ps_xlok0xmmld, vrcp14ps, vrcp14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vrcp14ps_xhik7xhixmm, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM1))
OPCODE(vrcp14ps_xhik7xmmld, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vrcp14ps_ylok0yloxmm, vrcp14ps, vrcp14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vrcp14ps_ylok0xmmld, vrcp14ps, vrcp14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vrcp14ps_yhik7yhixmm, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17))
OPCODE(vrcp14ps_yhik7xmmld, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vrcp14ps_zlok0zloxmm, vrcp14ps, vrcp14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp14ps_zlok0xmmld, vrcp14ps, vrcp14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp14ps_zhik7zhixmm, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp14ps_zhik7xmmld, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp14pd_xlok0xloxmm, vrcp14pd, vrcp14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vrcp14pd_xlok0xmmld, vrcp14pd, vrcp14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vrcp14pd_xhik7xhixmm, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17))
OPCODE(vrcp14pd_xhik7xmmld, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vrcp14pd_ylok0yloxmm, vrcp14pd, vrcp14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vrcp14pd_ylok0xmmld, vrcp14pd, vrcp14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vrcp14pd_yhik7yhixmm, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17))
OPCODE(vrcp14pd_yhik7xmmld, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vrcp14pd_zlok0zloxmm, vrcp14pd, vrcp14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp14pd_zlok0xmmld, vrcp14pd, vrcp14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp14pd_zhik7zhixmm, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp14pd_zhik7xmmld, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp28ps_zlok0zloxmm, vrcp28ps, vrcp28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp28ps_zlok0xmmld, vrcp28ps, vrcp28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp28ps_zhik7zhixmm, vrcp28ps, vrcp28ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp28ps_zhik7xmmld, vrcp28ps, vrcp28ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp28pd_zlok0zloxmm, vrcp28pd, vrcp28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp28pd_zlok0xmmld, vrcp28pd, vrcp28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp28pd_zhik7zhixmm, vrcp28pd, vrcp28pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp28pd_zhik7xmmld, vrcp28pd, vrcp28pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vpmovsxbw_xlok0xlo, vpmovsxbw, vpmovsxbw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovsxbw_xhik7xhi, vpmovsxbw, vpmovsxbw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovsxbw_xlok0ld, vpmovsxbw, vpmovsxbw_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxbw_xhik7ld, vpmovsxbw, vpmovsxbw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxbw_ylok0ylo, vpmovsxbw, vpmovsxbw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovsxbw_yhik7yhi, vpmovsxbw, vpmovsxbw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vpmovsxbw_ylok0ld, vpmovsxbw, vpmovsxbw_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxbw_yhik7ld, vpmovsxbw, vpmovsxbw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxbw_zlok0zlo, vpmovsxbw, vpmovsxbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vpmovsxbw_zhik7zhi, vpmovsxbw, vpmovsxbw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpmovsxbw_zlok0ld, vpmovsxbw, vpmovsxbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vpmovsxbw_zhik7ld, vpmovsxbw, vpmovsxbw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vpmovsxbd_xlok0xlo, vpmovsxbd, vpmovsxbd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovsxbd_xhik7xhi, vpmovsxbd, vpmovsxbd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpmovsxbd_xlok0ld, vpmovsxbd, vpmovsxbd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vpmovsxbd_xhik7ld, vpmovsxbd, vpmovsxbd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vpmovsxbd_ylok0ylo, vpmovsxbd, vpmovsxbd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovsxbd_yhik7yhi, vpmovsxbd, vpmovsxbd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_8))
OPCODE(vpmovsxbd_ylok0ld, vpmovsxbd, vpmovsxbd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxbd_yhik7ld, vpmovsxbd, vpmovsxbd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vpmovsxbd_zlok0zlo, vpmovsxbd, vpmovsxbd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vpmovsxbd_zhik7zhi, vpmovsxbd, vpmovsxbd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vpmovsxbd_zlok0ld, vpmovsxbd, vpmovsxbd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vpmovsxbd_zhik7ld, vpmovsxbd, vpmovsxbd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_16))
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
