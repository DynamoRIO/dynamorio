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

/* AVX-512 EVEX instructions with no destination, 1 mask, and 1 source. */
OPCODE(vgatherpf0dps_k1vsib, vgatherpf0dps, vgatherpf0dps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vgatherpf0dps_k7vsib, vgatherpf0dps, vgatherpf0dps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vgatherpf0dpd_k1vsib, vgatherpf0dpd, vgatherpf0dpd_mask, 0, REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vgatherpf0dpd_k7vsib, vgatherpf0dpd, vgatherpf0dpd_mask, X64_ONLY, REGARG(K7),
       VSIBY15(OPSZ_8))
OPCODE(vgatherpf0qps_k1vsib, vgatherpf0qps, vgatherpf0qps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vgatherpf0qps_k7vsib, vgatherpf0qps, vgatherpf0qps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vgatherpf0qpd_k1vsib, vgatherpf0qpd, vgatherpf0qpd_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vgatherpf0qpd_k7vsib, vgatherpf0qpd, vgatherpf0qpd_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_8))
OPCODE(vgatherpf1dps_k1vsib, vgatherpf1dps, vgatherpf1dps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vgatherpf1dps_k7vsib, vgatherpf1dps, vgatherpf1dps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vgatherpf1dpd_k1vsib, vgatherpf1dpd, vgatherpf1dpd_mask, 0, REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vgatherpf1dpd_k7vsib, vgatherpf1dpd, vgatherpf1dpd_mask, X64_ONLY, REGARG(K7),
       VSIBY15(OPSZ_8))
OPCODE(vgatherpf1qps_k1vsib, vgatherpf1qps, vgatherpf1qps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vgatherpf1qps_k7vsib, vgatherpf1qps, vgatherpf1qps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vgatherpf1qpd_k1vsib, vgatherpf1qpd, vgatherpf1qpd_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vgatherpf1qpd_k7vsib, vgatherpf1qpd, vgatherpf1qpd_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_8))
OPCODE(vscatterpf0dps_k1vsib, vscatterpf0dps, vscatterpf0dps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vscatterpf0dps_k7vsib, vscatterpf0dps, vscatterpf0dps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vscatterpf0dpd_k1vsib, vscatterpf0dpd, vscatterpf0dpd_mask, 0, REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vscatterpf0dpd_k7vsib, vscatterpf0dpd, vscatterpf0dpd_mask, X64_ONLY, REGARG(K7),
       VSIBY15(OPSZ_8))
OPCODE(vscatterpf0qps_k1vsib, vscatterpf0qps, vscatterpf0qps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vscatterpf0qps_k7vsib, vscatterpf0qps, vscatterpf0qps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vscatterpf0qpd_k1vsib, vscatterpf0qpd, vscatterpf0qpd_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vscatterpf0qpd_k7vsib, vscatterpf0qpd, vscatterpf0qpd_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_8))
OPCODE(vscatterpf1dps_k1vsib, vscatterpf1dps, vscatterpf1dps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vscatterpf1dps_k7vsib, vscatterpf1dps, vscatterpf1dps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vscatterpf1dpd_k1vsib, vscatterpf1dpd, vscatterpf1dpd_mask, 0, REGARG(K1),
       VSIBY6(OPSZ_8))
OPCODE(vscatterpf1dpd_k7vsib, vscatterpf1dpd, vscatterpf1dpd_mask, X64_ONLY, REGARG(K7),
       VSIBY15(OPSZ_8))
OPCODE(vscatterpf1qps_k1vsib, vscatterpf1qps, vscatterpf1qps_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_4))
OPCODE(vscatterpf1qps_k7vsib, vscatterpf1qps, vscatterpf1qps_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_4))
OPCODE(vscatterpf1qpd_k1vsib, vscatterpf1qpd, vscatterpf1qpd_mask, 0, REGARG(K1),
       VSIBZ6(OPSZ_8))
OPCODE(vscatterpf1qpd_k7vsib, vscatterpf1qpd, vscatterpf1qpd_mask, X64_ONLY, REGARG(K7),
       VSIBZ31(OPSZ_8))
