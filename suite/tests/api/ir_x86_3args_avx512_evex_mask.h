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
OPCODE(vmovss_xlok0ld, vmovss, vmovss_mask, 0, REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vmovsd_xlok0ld, vmovsd, vmovsd_mask, 0, REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vmovss_k0stxlo, vmovss, vmovss_mask, 0, MEMARG(OPSZ_4), REGARG(K0),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovsd_k0stxlo, vmovsd, vmovsd_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovss_xlok7ld, vmovss, vmovss_mask, 0, REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K7),
       MEMARG(OPSZ_4))
OPCODE(vmovsd_xlok7ld, vmovsd, vmovsd_mask, 0, REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vmovss_k7stxlo, vmovss, vmovss_mask, 0, MEMARG(OPSZ_4), REGARG(K7),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovsd_k7stxlo, vmovsd, vmovsd_mask, 0, MEMARG(OPSZ_8), REGARG(K7),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovss_xhik7ld, vmovss, vmovss_mask, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_4),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vmovsd_xhik7ld, vmovsd, vmovsd_mask, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vmovss_k7stxhi, vmovss, vmovss_mask, X64_ONLY, MEMARG(OPSZ_4), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vmovsd_k7stxhi, vmovsd, vmovsd_mask, X64_ONLY, MEMARG(OPSZ_8), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_8))
/* TODO i#1312: Add missing instructions. */
