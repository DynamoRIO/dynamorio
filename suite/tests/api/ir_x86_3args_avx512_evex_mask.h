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
       MEMARG(OPSZ_16))
OPCODE(vmovddup_xhik7ld, vmovddup, vmovddup_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vmovddup_ylok0ld, vmovddup, vmovddup_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vmovddup_yhik7ld, vmovddup, vmovddup_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vmovddup_zlok0ld, vmovddup, vmovddup_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vmovddup_zhik7ld, vmovddup, vmovddup_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
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
       MEMARG(OPSZ_8))
OPCODE(vcvtps2pd_xhik7ld, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vcvtps2pd_ylok0ld, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2pd_yhik7ld, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2pd_zlok0ld, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2pd_zhik7ld, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2pd_xlok0xlo, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvtps2pd_xhik7xhi, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvtps2pd_ylok0ylo, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvtps2pd_yhik7yhi, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM16, OPSZ_16))
OPCODE(vcvtps2pd_zlok0zlo, vcvtps2pd, vcvtps2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvtps2pd_zhik7zhi, vcvtps2pd, vcvtps2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM16, OPSZ_32))
OPCODE(vcvtpd2ps_xlok0ld, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2ps_xlok0bcst, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2ps_xhik7ld, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2ps_xhik7bcst, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2ps_ylok0ld, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2ps_ylok0bcst, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2ps_yhik7ld, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2ps_yhik7bcst, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2ps_zlok0ld, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2ps_zlok0bcst, vcvtpd2ps, vcvtpd2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2ps_zhik7ld, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2ps_zhik7bcst, vcvtpd2ps, vcvtpd2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
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
OPCODE(vcvtdq2ps_xlok0bcst, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtdq2ps_xlok0xlo, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtdq2ps_xhik7xhi, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtdq2ps_xhik7ld, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtdq2ps_xhik7bcst, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtdq2ps_ylok0ld, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtdq2ps_ylok0bcst, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtdq2ps_ylok0ylo, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtdq2ps_yhik7yhi, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtdq2ps_yhik7ld, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtdq2ps_yhik7bcst, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtdq2ps_zlok0ld, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtdq2ps_zlok0bcst, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtdq2ps_zlok0zlo, vcvtdq2ps, vcvtdq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtdq2ps_zhik7zhi, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtdq2ps_zhik7ld, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtdq2ps_zhik7bcst, vcvtdq2ps, vcvtdq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2dq_xlok0ld, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2dq_xlok0bcst, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2dq_xlok0xlo, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtps2dq_xhik7xhi, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtps2dq_xhik7ld, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2dq_xhik7bcst, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2dq_ylok0ld, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2dq_ylok0bcst, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2dq_ylok0ylo, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtps2dq_yhik7yhi, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtps2dq_yhik7ld, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2dq_yhik7bcst, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2dq_zlok0ld, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2dq_zlok0bcst, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2dq_zlok0zlo, vcvtps2dq, vcvtps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtps2dq_zhik7zhi, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtps2dq_zhik7ld, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2dq_zhik7bcst, vcvtps2dq, vcvtps2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2dq_xlok0ld, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2dq_xlok0bcst, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2dq_xlok0xlo, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttps2dq_xhik7xhi, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttps2dq_xhik7ld, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttps2dq_xhik7bcst, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2dq_ylok0ld, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2dq_ylok0bcst, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2dq_ylok0ylo, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttps2dq_yhik7yhi, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttps2dq_yhik7ld, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttps2dq_yhik7bcst, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2dq_zlok0ld, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttps2dq_zlok0bcst, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2dq_zlok0zlo, vcvttps2dq, vcvttps2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttps2dq_zhik7zhi, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttps2dq_zhik7ld, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvttps2dq_zhik7bcst, vcvttps2dq, vcvttps2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtdq2pd_xlok0ld, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtdq2pd_xlok0xlo, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvtdq2pd_xhik7xhi, vcvtdq2pd, vcvtdq2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvtdq2pd_ylok0ld, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtdq2pd_ylok0ylo, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvtdq2pd_yhik7yhi, vcvtdq2pd, vcvtdq2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvtdq2pd_zlok0ld, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtdq2pd_zlok0zlo, vcvtdq2pd, vcvtdq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvtdq2pd_zhik7zhi, vcvtdq2pd, vcvtdq2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvttpd2dq_xlok0ld, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2dq_xlok0bcst, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2dq_xlok0xlo, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2dq_xhik7xhi, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2dq_xhik7ld, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttpd2dq_xhik7bcst, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2dq_ylok0ld, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2dq_ylok0bcst, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2dq_ylok0ylo, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2dq_yhik7yhi, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2dq_yhik7ld, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttpd2dq_yhik7bcst, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2dq_zlok0ld, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2dq_zlok0bcst, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2dq_zlok0zlo, vcvttpd2dq, vcvttpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2dq_zhik7zhi, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2dq_zhik7ld, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvttpd2dq_zhik7bcst, vcvttpd2dq, vcvttpd2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2dq_xlok0ld, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2dq_xlok0bcst, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2dq_xlok0xlo, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2dq_xhik7xhi, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtpd2dq_xhik7ld, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2dq_xhik7bcst, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2dq_ylok0ld, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2dq_ylok0bcst, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2dq_ylok0ylo, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2dq_yhik7yhi, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtpd2dq_yhik7ld, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2dq_yhik7bcst, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2dq_zlok0ld, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2dq_zlok0bcst, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2dq_zlok0zlo, vcvtpd2dq, vcvtpd2dq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2dq_zhik7zhi, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtpd2dq_zhik7ld, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2dq_zhik7bcst, vcvtpd2dq, vcvtpd2dq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtph2ps_xlok0ld, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtph2ps_xlok0xlo, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvtph2ps_xhik7xhi, vcvtph2ps, vcvtph2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvtph2ps_ylok0ld, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtph2ps_ylok0ylo, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvtph2ps_yhik7yhi, vcvtph2ps, vcvtph2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvtph2ps_zlok0ld, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtph2ps_zlok0zlo, vcvtph2ps, vcvtph2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvtph2ps_zhik7zhi, vcvtph2ps, vcvtph2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvtpd2qq_xlok0ld, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2qq_xlok0bcst, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2qq_xlok0xlo, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2qq_xhik7xhi, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtpd2qq_xhik7ld, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2qq_xhik7bcst, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2qq_ylok0ld, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2qq_ylok0bcst, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2qq_ylok0ylo, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2qq_yhik7yhi, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtpd2qq_yhik7ld, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2qq_yhik7bcst, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2qq_zlok0ld, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2qq_zlok0bcst, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2qq_zlok0zlo, vcvtpd2qq, vcvtpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2qq_zhik7zhi, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtpd2qq_zhik7ld, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2qq_zhik7bcst, vcvtpd2qq, vcvtpd2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2udq_xlok0ld, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2udq_xlok0bcst, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2udq_xlok0xlo, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2udq_xhik7xhi, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtpd2udq_xhik7ld, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtpd2udq_xhik7bcst, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2udq_ylok0ld, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2udq_ylok0bcst, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2udq_ylok0ylo, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2udq_yhik7yhi, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtpd2udq_yhik7ld, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtpd2udq_yhik7bcst, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2udq_zlok0ld, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2udq_zlok0bcst, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2udq_zlok0zlo, vcvtpd2udq, vcvtpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2udq_zhik7zhi, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtpd2udq_zhik7ld, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtpd2udq_zhik7bcst, vcvtpd2udq, vcvtpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2uqq_xlok0ld, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtpd2uqq_xlok0bcst, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2uqq_xlok0xlo, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtpd2uqq_xhik7xhi, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtpd2uqq_xhik7ld, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtpd2uqq_xhik7bcst, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2uqq_ylok0ld, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtpd2uqq_ylok0bcst, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2uqq_ylok0ylo, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtpd2uqq_yhik7yhi, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtpd2uqq_yhik7ld, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtpd2uqq_yhik7bcst, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtpd2uqq_zlok0ld, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtpd2uqq_zlok0bcst, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtpd2uqq_zlok0zlo, vcvtpd2uqq, vcvtpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtpd2uqq_zhik7zhi, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtpd2uqq_zhik7ld, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtpd2uqq_zhik7bcst, vcvtpd2uqq, vcvtpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtps2qq_xlok0ld, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtps2qq_xlok0bcst, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2qq_xlok0xlo, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvtps2qq_xhik7xhi, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvtps2qq_xhik7ld, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_8))
OPCODE(vcvtps2qq_xhik7bcst, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2qq_ylok0ld, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2qq_ylok0bcst, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2qq_ylok0ylo, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvtps2qq_yhik7yhi, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvtps2qq_yhik7ld, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2qq_yhik7bcst, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2qq_zlok0ld, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2qq_zlok0bcst, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2qq_zlok0zlo, vcvtps2qq, vcvtps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvtps2qq_zhik7zhi, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvtps2qq_zhik7ld, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2qq_zhik7bcst, vcvtps2qq, vcvtps2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2uqq_xlok0ld, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtps2uqq_xlok0bcst, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2uqq_xlok0xlo, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvtps2uqq_xhik7xhi, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvtps2uqq_xhik7ld, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtps2uqq_xhik7bcst, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2uqq_ylok0ld, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2uqq_ylok0bcst, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2uqq_ylok0ylo, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvtps2uqq_yhik7yhi, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvtps2uqq_yhik7ld, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtps2uqq_yhik7bcst, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2uqq_zlok0ld, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2uqq_zlok0bcst, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2uqq_zlok0zlo, vcvtps2uqq, vcvtps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvtps2uqq_zhik7zhi, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvtps2uqq_zhik7ld, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtps2uqq_zhik7bcst, vcvtps2uqq, vcvtps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2udq_xlok0ld, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtps2udq_xlok0bcst, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2udq_xlok0xlo, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtps2udq_xhik7xhi, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtps2udq_xhik7ld, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtps2udq_xhik7bcst, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2udq_ylok0ld, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtps2udq_ylok0bcst, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2udq_ylok0ylo, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtps2udq_yhik7yhi, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtps2udq_yhik7ld, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtps2udq_yhik7bcst, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtps2udq_zlok0ld, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtps2udq_zlok0bcst, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtps2udq_zlok0zlo, vcvtps2udq, vcvtps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtps2udq_zhik7zhi, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtps2udq_zhik7ld, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtps2udq_zhik7bcst, vcvtps2udq, vcvtps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2udq_xlok0ld, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2udq_xlok0bcst, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2udq_xlok0xlo, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttps2udq_xhik7xhi, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttps2udq_xhik7ld, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttps2udq_xhik7bcst, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2udq_ylok0ld, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2udq_ylok0bcst, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2udq_ylok0ylo, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttps2udq_yhik7yhi, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttps2udq_yhik7ld, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttps2udq_yhik7bcst, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2udq_zlok0ld, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttps2udq_zlok0bcst, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2udq_zlok0zlo, vcvttps2udq, vcvttps2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttps2udq_zhik7zhi, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttps2udq_zhik7ld, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvttps2udq_zhik7bcst, vcvttps2udq, vcvttps2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttpd2udq_xlok0ld, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2udq_xlok0bcst, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2udq_xlok0xlo, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2udq_xhik7xhi, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2udq_xhik7ld, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttpd2udq_xhik7bcst, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2udq_ylok0ld, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2udq_ylok0bcst, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2udq_ylok0ylo, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2udq_yhik7yhi, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2udq_yhik7ld, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttpd2udq_yhik7bcst, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2udq_zlok0ld, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2udq_zlok0bcst, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2udq_zlok0zlo, vcvttpd2udq, vcvttpd2udq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2udq_zhik7zhi, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2udq_zhik7ld, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvttpd2udq_zhik7bcst, vcvttpd2udq, vcvttpd2udq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttps2qq_xlok0ld, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttps2qq_xlok0bcst, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2qq_xlok0xlo, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvttps2qq_xhik7xhi, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvttps2qq_xhik7ld, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttps2qq_xhik7bcst, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2qq_ylok0ld, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2qq_ylok0bcst, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2qq_ylok0ylo, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvttps2qq_yhik7yhi, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvttps2qq_yhik7ld, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttps2qq_yhik7bcst, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2qq_zlok0ld, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2qq_zlok0bcst, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2qq_zlok0zlo, vcvttps2qq, vcvttps2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvttps2qq_zhik7zhi, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvttps2qq_zhik7ld, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttps2qq_zhik7bcst, vcvttps2qq, vcvttps2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttpd2qq_xlok0ld, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2qq_xlok0bcst, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2qq_xlok0xlo, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2qq_xhik7xhi, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2qq_xhik7ld, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttpd2qq_xhik7bcst, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2qq_ylok0ld, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2qq_ylok0bcst, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2qq_ylok0ylo, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2qq_yhik7yhi, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2qq_yhik7ld, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttpd2qq_yhik7bcst, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2qq_zlok0ld, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2qq_zlok0bcst, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2qq_zlok0zlo, vcvttpd2qq, vcvttpd2qq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2qq_zhik7zhi, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2qq_zhik7ld, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvttpd2qq_zhik7bcst, vcvttpd2qq, vcvttpd2qq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttps2uqq_xlok0ld, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttps2uqq_xlok0bcst, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2uqq_xlok0xlo, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvttps2uqq_xhik7xhi, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvttps2uqq_xhik7ld, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttps2uqq_xhik7bcst, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2uqq_ylok0ld, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttps2uqq_ylok0bcst, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2uqq_ylok0ylo, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvttps2uqq_yhik7yhi, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvttps2uqq_yhik7ld, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttps2uqq_yhik7bcst, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttps2uqq_zlok0ld, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttps2uqq_zlok0bcst, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvttps2uqq_zlok0zlo, vcvttps2uqq, vcvttps2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvttps2uqq_zhik7zhi, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvttps2uqq_zhik7ld, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttps2uqq_zhik7bcst, vcvttps2uqq, vcvttps2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvttpd2uqq_xlok0ld, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvttpd2uqq_xlok0bcst, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2uqq_xlok0xlo, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvttpd2uqq_xhik7xhi, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvttpd2uqq_xhik7ld, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvttpd2uqq_xhik7bcst, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2uqq_ylok0ld, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvttpd2uqq_ylok0bcst, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2uqq_ylok0ylo, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvttpd2uqq_yhik7yhi, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvttpd2uqq_yhik7ld, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvttpd2uqq_yhik7bcst, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvttpd2uqq_zlok0ld, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvttpd2uqq_zlok0bcst, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvttpd2uqq_zlok0zlo, vcvttpd2uqq, vcvttpd2uqq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvttpd2uqq_zhik7zhi, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvttpd2uqq_zhik7ld, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvttpd2uqq_zhik7bcst, vcvttpd2uqq, vcvttpd2uqq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtqq2ps_xlok0ld, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtqq2ps_xlok0bcst, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtqq2ps_xlok0xlo, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtqq2ps_xhik7xhi, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtqq2ps_xhik7ld, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtqq2ps_xhik7bcst, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtqq2ps_ylok0ld, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtqq2ps_ylok0bcst, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtqq2ps_ylok0ylo, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtqq2ps_yhik7yhi, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtqq2ps_yhik7ld, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtqq2ps_yhik7bcst, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtqq2ps_zlok0ld, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtqq2ps_zlok0bcst, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtqq2ps_zlok0zlo, vcvtqq2ps, vcvtqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtqq2ps_zhik7zhi, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtqq2ps_zhik7ld, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtqq2ps_zhik7bcst, vcvtqq2ps, vcvtqq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtqq2pd_xlok0ld, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtqq2pd_xlok0bcst, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtqq2pd_xlok0xlo, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtqq2pd_xhik7xhi, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31))
OPCODE(vcvtqq2pd_xhik7ld, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vcvtqq2pd_xhik7bcst, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtqq2pd_ylok0ld, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtqq2pd_ylok0bcst, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtqq2pd_ylok0ylo, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtqq2pd_yhik7yhi, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31))
OPCODE(vcvtqq2pd_yhik7ld, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vcvtqq2pd_yhik7bcst, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtqq2pd_zlok0ld, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtqq2pd_zlok0bcst, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtqq2pd_zlok0zlo, vcvtqq2pd, vcvtqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtqq2pd_zhik7zhi, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31))
OPCODE(vcvtqq2pd_zhik7ld, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vcvtqq2pd_zhik7bcst, vcvtqq2pd, vcvtqq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtudq2ps_xlok0ld, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtudq2ps_xlok0bcst, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtudq2ps_xlok0xlo, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtudq2ps_xhik7xhi, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtudq2ps_xhik7ld, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtudq2ps_xhik7bcst, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtudq2ps_ylok0ld, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtudq2ps_ylok0bcst, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtudq2ps_ylok0ylo, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtudq2ps_yhik7yhi, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtudq2ps_yhik7ld, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtudq2ps_yhik7bcst, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtudq2ps_zlok0ld, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtudq2ps_zlok0bcst, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vcvtudq2ps_zlok0zlo, vcvtudq2ps, vcvtudq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtudq2ps_zhik7zhi, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtudq2ps_zhik7ld, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtudq2ps_zhik7bcst, vcvtudq2ps, vcvtudq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vcvtudq2pd_xlok0ld, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtudq2pd_xlok0xlo, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcvtudq2pd_xhik7xhi, vcvtudq2pd, vcvtudq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvtudq2pd_ylok0ld, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtudq2pd_ylok0ylo, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vcvtudq2pd_yhik7yhi, vcvtudq2pd, vcvtudq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vcvtudq2pd_zlok0ld, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtudq2pd_zlok0zlo, vcvtudq2pd, vcvtudq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vcvtudq2pd_zhik7zhi, vcvtudq2pd, vcvtudq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vcvtuqq2ps_xlok0ld, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtuqq2ps_xlok0bcst, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtuqq2ps_xlok0xlo, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtuqq2ps_xhik7xhi, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtuqq2ps_xhik7ld, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtuqq2ps_xhik7bcst, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtuqq2ps_ylok0ld, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtuqq2ps_ylok0bcst, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtuqq2ps_ylok0ylo, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtuqq2ps_yhik7yhi, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtuqq2ps_yhik7ld, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtuqq2ps_yhik7bcst, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtuqq2ps_zlok0ld, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtuqq2ps_zlok0bcst, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtuqq2ps_zlok0zlo, vcvtuqq2ps, vcvtuqq2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtuqq2ps_zhik7zhi, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtuqq2ps_zhik7ld, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtuqq2ps_zhik7bcst, vcvtuqq2ps, vcvtuqq2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtuqq2pd_xlok0ld, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vcvtuqq2pd_xlok0bcst, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtuqq2pd_xlok0xlo, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vcvtuqq2pd_xhik7xhi, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31))
OPCODE(vcvtuqq2pd_xhik7ld, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vcvtuqq2pd_xhik7bcst, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtuqq2pd_ylok0ld, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vcvtuqq2pd_ylok0bcst, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtuqq2pd_ylok0ylo, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vcvtuqq2pd_yhik7yhi, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31))
OPCODE(vcvtuqq2pd_yhik7ld, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vcvtuqq2pd_yhik7bcst, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vcvtuqq2pd_zlok0ld, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vcvtuqq2pd_zlok0bcst, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vcvtuqq2pd_zlok0zlo, vcvtuqq2pd, vcvtuqq2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vcvtuqq2pd_zhik7zhi, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31))
OPCODE(vcvtuqq2pd_zhik7ld, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vcvtuqq2pd_zhik7bcst, vcvtuqq2pd, vcvtuqq2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrcp14ps_xlok0xloxmm, vrcp14ps, vrcp14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vrcp14ps_xlok0xmmld, vrcp14ps, vrcp14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vrcp14ps_xlok0xmmbcst, vrcp14ps, vrcp14ps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrcp14ps_xhik7xhixmm, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM1))
OPCODE(vrcp14ps_xhik7xmmld, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vrcp14ps_xhik7xmmbcst, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrcp14ps_ylok0yloxmm, vrcp14ps, vrcp14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vrcp14ps_ylok0xmmld, vrcp14ps, vrcp14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vrcp14ps_ylok0xmmbcst, vrcp14ps, vrcp14ps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrcp14ps_yhik7yhixmm, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17))
OPCODE(vrcp14ps_yhik7xmmld, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vrcp14ps_yhik7xmmbcst, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrcp14ps_zlok0zloxmm, vrcp14ps, vrcp14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp14ps_zlok0xmmld, vrcp14ps, vrcp14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp14ps_zlok0xmmbcst, vrcp14ps, vrcp14ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrcp14ps_zhik7zhixmm, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp14ps_zhik7xmmld, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp14ps_zhik7xmmbcst, vrcp14ps, vrcp14ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrcp14pd_xlok0xloxmm, vrcp14pd, vrcp14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1))
OPCODE(vrcp14pd_xlok0xmmld, vrcp14pd, vrcp14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16))
OPCODE(vrcp14pd_xlok0xmmbcst, vrcp14pd, vrcp14pd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrcp14pd_xhik7xhixmm, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17))
OPCODE(vrcp14pd_xhik7xmmld, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       MEMARG(OPSZ_16))
OPCODE(vrcp14pd_xhik7xmmbcst, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrcp14pd_ylok0yloxmm, vrcp14pd, vrcp14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1))
OPCODE(vrcp14pd_ylok0xmmld, vrcp14pd, vrcp14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32))
OPCODE(vrcp14pd_ylok0xmmbcst, vrcp14pd, vrcp14pd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrcp14pd_yhik7yhixmm, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17))
OPCODE(vrcp14pd_yhik7xmmld, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32))
OPCODE(vrcp14pd_yhik7xmmbcst, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrcp14pd_zlok0zloxmm, vrcp14pd, vrcp14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp14pd_zlok0xmmld, vrcp14pd, vrcp14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp14pd_zlok0xmmbcst, vrcp14pd, vrcp14pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrcp14pd_zhik7zhixmm, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp14pd_zhik7xmmld, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp14pd_zhik7xmmbcst, vrcp14pd, vrcp14pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
OPCODE(vrcp28ps_zlok0zloxmm, vrcp28ps, vrcp28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp28ps_zlok0xmmld, vrcp28ps, vrcp28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp28ps_zlok0xmmbcst, vrcp28ps, vrcp28ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4))
OPCODE(vrcp28ps_zhik7zhixmm, vrcp28ps, vrcp28ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp28ps_zhik7xmmld, vrcp28ps, vrcp28ps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp28ps_zhik7xmmbcst, vrcp28ps, vrcp28ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4))
OPCODE(vrcp28pd_zlok0zloxmm, vrcp28pd, vrcp28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1))
OPCODE(vrcp28pd_zlok0xmmld, vrcp28pd, vrcp28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64))
OPCODE(vrcp28pd_zlok0xmmbcst, vrcp28pd, vrcp28pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8))
OPCODE(vrcp28pd_zhik7zhixmm, vrcp28pd, vrcp28pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17))
OPCODE(vrcp28pd_zhik7xmmld, vrcp28pd, vrcp28pd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64))
OPCODE(vrcp28pd_zhik7xmmbcst, vrcp28pd, vrcp28pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8))
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
OPCODE(vpexpandd_xlok0st, vpexpandd, vpexpandd_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpexpandd_xlok0xlo, vpexpandd, vpexpandd_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpexpandd_xhik7xhi, vpexpandd, vpexpandd_mask, X64_ONLY, REGARG(XMM31), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpexpandd_ylok0st, vpexpandd, vpexpandd_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpexpandd_ylok0ylo, vpexpandd, vpexpandd_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpexpandd_yhik7yhi, vpexpandd, vpexpandd_mask, X64_ONLY, REGARG(YMM31), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpexpandd_zlok0st, vpexpandd, vpexpandd_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpexpandd_zlok0zlo, vpexpandd, vpexpandd_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpexpandd_zhik7zhi, vpexpandd, vpexpandd_mask, X64_ONLY, REGARG(ZMM31), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vpexpandq_xlok0st, vpexpandq, vpexpandq_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpexpandq_xlok0xlo, vpexpandq, vpexpandq_mask, 0, REGARG(XMM1), REGARG(K0),
       REGARG(XMM0))
OPCODE(vpexpandq_xhik7xhi, vpexpandq, vpexpandq_mask, X64_ONLY, REGARG(XMM31), REGARG(K7),
       REGARG(XMM16))
OPCODE(vpexpandq_ylok0st, vpexpandq, vpexpandq_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpexpandq_ylok0ylo, vpexpandq, vpexpandq_mask, 0, REGARG(YMM1), REGARG(K0),
       REGARG(YMM0))
OPCODE(vpexpandq_yhik7yhi, vpexpandq, vpexpandq_mask, X64_ONLY, REGARG(YMM31), REGARG(K7),
       REGARG(YMM16))
OPCODE(vpexpandq_zlok0st, vpexpandq, vpexpandq_mask, 0, MEMARG(OPSZ_64), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpexpandq_zlok0zlo, vpexpandq, vpexpandq_mask, 0, REGARG(ZMM1), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vpexpandq_zhik7zhi, vpexpandq, vpexpandq_mask, X64_ONLY, REGARG(ZMM31), REGARG(K7),
       REGARG(ZMM16))
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
