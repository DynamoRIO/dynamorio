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
