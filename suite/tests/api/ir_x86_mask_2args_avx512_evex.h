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
OPCODE(vmovups_zlok0zlo, vmovups, mask_vmovups, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovups_zhik0zlo, vmovups, mask_vmovups, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovups_zlok0zhi, vmovups, mask_vmovups, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovups_zlok7zlo, vmovups, mask_vmovups, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovups_zhik7zlo, vmovups, mask_vmovups, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovups_zlok7zhi, vmovups, mask_vmovups, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovups_zlokm, vmovups, mask_vmovups, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovups_mkzlo, vmovups, mask_vmovups, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovups_ylok0ylo, vmovups, mask_vmovups, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovups_yhik0ylo, vmovups, mask_vmovups, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovups_ylok0yhi, vmovups, mask_vmovups, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovups_ylok7ylo, vmovups, mask_vmovups, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovups_yhik7ylo, vmovups, mask_vmovups, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovups_ylok7yhi, vmovups, mask_vmovups, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovups_ylokm, vmovups, mask_vmovups, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovups_mkylo, vmovups, mask_vmovups, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovups_xlok0xlo, vmovups, mask_vmovups, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovups_xhik0xlo, vmovups, mask_vmovups, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovups_xlok0xhi, vmovups, mask_vmovups, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovups_xlok7xlo, vmovups, mask_vmovups, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovups_xhik7xlo, vmovups, mask_vmovups, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovups_xlok7xhi, vmovups, mask_vmovups, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovups_xlokm, vmovups, mask_vmovups, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovups_mkxlo, vmovups, mask_vmovups, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovupd_zlok0zlo, vmovupd, mask_vmovupd, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovupd_zhik0zlo, vmovupd, mask_vmovupd, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovupd_zlok0zhi, vmovupd, mask_vmovupd, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovupd_zlok7zlo, vmovupd, mask_vmovupd, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovupd_zhik7zlo, vmovupd, mask_vmovupd, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovupd_zlok7zhi, vmovupd, mask_vmovupd, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovupd_zlokm, vmovupd, mask_vmovupd, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovupd_mkzlo, vmovupd, mask_vmovupd, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovupd_ylok0ylo, vmovupd, mask_vmovupd, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovupd_yhik0ylo, vmovupd, mask_vmovupd, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovupd_ylok0yhi, vmovupd, mask_vmovupd, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovupd_ylok7ylo, vmovupd, mask_vmovupd, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovupd_yhik7ylo, vmovupd, mask_vmovupd, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovupd_ylok7yhi, vmovupd, mask_vmovupd, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovupd_ylokm, vmovupd, mask_vmovupd, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovupd_mkylo, vmovupd, mask_vmovupd, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovupd_xlok0xlo, vmovupd, mask_vmovupd, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovupd_xhik0xlo, vmovupd, mask_vmovupd, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovupd_xlok0xhi, vmovupd, mask_vmovupd, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovupd_xlok7xlo, vmovupd, mask_vmovupd, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovupd_xhik7xlo, vmovupd, mask_vmovupd, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovupd_xlok7xhi, vmovupd, mask_vmovupd, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovupd_xlokm, vmovupd, mask_vmovupd, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovupd_mkxlo, vmovupd, mask_vmovupd, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovaps_zlok0zlo, vmovaps, mask_vmovaps, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovaps_zhik0zlo, vmovaps, mask_vmovaps, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovaps_zlok0zhi, vmovaps, mask_vmovaps, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovaps_zlok7zlo, vmovaps, mask_vmovaps, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovaps_zhik7zlo, vmovaps, mask_vmovaps, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovaps_zlok7zhi, vmovaps, mask_vmovaps, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovaps_zlokm, vmovaps, mask_vmovaps, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovaps_mkzlo, vmovaps, mask_vmovaps, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovaps_ylok0ylo, vmovaps, mask_vmovaps, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovaps_yhik0ylo, vmovaps, mask_vmovaps, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovaps_ylok0yhi, vmovaps, mask_vmovaps, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovaps_ylok7ylo, vmovaps, mask_vmovaps, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovaps_yhik7ylo, vmovaps, mask_vmovaps, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovaps_ylok7yhi, vmovaps, mask_vmovaps, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovaps_ylokm, vmovaps, mask_vmovaps, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovaps_mkylo, vmovaps, mask_vmovaps, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovaps_xlok0xlo, vmovaps, mask_vmovaps, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovaps_xhik0xlo, vmovaps, mask_vmovaps, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovaps_xlok0xhi, vmovaps, mask_vmovaps, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovaps_xlok7xlo, vmovaps, mask_vmovaps, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovaps_xhik7xlo, vmovaps, mask_vmovaps, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovaps_xlok7xhi, vmovaps, mask_vmovaps, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovaps_xlokm, vmovaps, mask_vmovaps, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovaps_mkxlo, vmovaps, mask_vmovaps, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
OPCODE(vmovapd_zlok0zlo, vmovapd, mask_vmovapd, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1))
OPCODE(vmovapd_zhik0zlo, vmovapd, mask_vmovapd, X64_ONLY, REGARG(ZMM16), REGARG(K0),
       REGARG(ZMM0))
OPCODE(vmovapd_zlok0zhi, vmovapd, mask_vmovapd, X64_ONLY, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM16))
OPCODE(vmovapd_zlok7zlo, vmovapd, mask_vmovapd, 0, REGARG(ZMM0), REGARG(K7), REGARG(ZMM1))
OPCODE(vmovapd_zhik7zlo, vmovapd, mask_vmovapd, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM0))
OPCODE(vmovapd_zlok7zhi, vmovapd, mask_vmovapd, X64_ONLY, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM16))
OPCODE(vmovapd_zlokm, vmovapd, mask_vmovapd, 0, REGARG(ZMM0), REGARG(K7), MEMARG(OPSZ_64))
OPCODE(vmovapd_mkzlo, vmovapd, mask_vmovapd, 0, MEMARG(OPSZ_64), REGARG(K7), REGARG(ZMM0))
OPCODE(vmovapd_ylok0ylo, vmovapd, mask_vmovapd, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1))
OPCODE(vmovapd_yhik0ylo, vmovapd, mask_vmovapd, X64_ONLY, REGARG(YMM16), REGARG(K0),
       REGARG(YMM0))
OPCODE(vmovapd_ylok0yhi, vmovapd, mask_vmovapd, X64_ONLY, REGARG(YMM0), REGARG(K0),
       REGARG(YMM16))
OPCODE(vmovapd_ylok7ylo, vmovapd, mask_vmovapd, 0, REGARG(YMM0), REGARG(K7), REGARG(YMM1))
OPCODE(vmovapd_yhik7ylo, vmovapd, mask_vmovapd, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM0))
OPCODE(vmovapd_ylok7yhi, vmovapd, mask_vmovapd, X64_ONLY, REGARG(YMM0), REGARG(K7),
       REGARG(YMM16))
OPCODE(vmovapd_ylokm, vmovapd, mask_vmovapd, 0, REGARG(YMM0), REGARG(K7), MEMARG(OPSZ_32))
OPCODE(vmovapd_mkylo, vmovapd, mask_vmovapd, 0, MEMARG(OPSZ_32), REGARG(K7), REGARG(YMM0))
OPCODE(vmovapd_xlok0xlo, vmovapd, mask_vmovapd, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1))
OPCODE(vmovapd_xhik0xlo, vmovapd, mask_vmovapd, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG(XMM0))
OPCODE(vmovapd_xlok0xhi, vmovapd, mask_vmovapd, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG(XMM16))
OPCODE(vmovapd_xlok7xlo, vmovapd, mask_vmovapd, 0, REGARG(XMM0), REGARG(K7), REGARG(XMM1))
OPCODE(vmovapd_xhik7xlo, vmovapd, mask_vmovapd, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM0))
OPCODE(vmovapd_xlok7xhi, vmovapd, mask_vmovapd, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG(XMM16))
OPCODE(vmovapd_xlokm, vmovapd, mask_vmovapd, 0, REGARG(XMM0), REGARG(K7), MEMARG(OPSZ_16))
OPCODE(vmovapd_mkxlo, vmovapd, mask_vmovapd, 0, MEMARG(OPSZ_16), REGARG(K7), REGARG(XMM0))
/* TODO i#1312: Add missing instructions. */
