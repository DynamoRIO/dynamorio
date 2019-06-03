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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 3 sources. */
OPCODE(vinsertf32x4_ylok0yloxloi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_ylok0yloldi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_zlok0zloxloi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_zlok0zloldi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_yhik7yhixhii, vinsertf32x4, vinsertf32x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG_PARTIAL(YMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_yhik7yhildi, vinsertf32x4, vinsertf32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_zhik7zhixhii, vinsertf32x4, vinsertf32x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinsertf32x4_zhik7zhildi, vinsertf32x4, vinsertf32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_ylok0yloxloi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_ylok0yloldi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_zlok0zloxloi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_zlok0zloldi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_yhik7yhixhii, vinsertf64x2, vinsertf64x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG_PARTIAL(YMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_yhik7yhildi, vinsertf64x2, vinsertf64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_zhik7zhixhii, vinsertf64x2, vinsertf64x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinsertf64x2_zhik7zhildi, vinsertf64x2, vinsertf64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf32x8_zlok0zloxloi, vinsertf32x8, vinsertf32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinsertf32x8_zlok0zloldi, vinsertf32x8, vinsertf32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf32x8_zhik7zhixhii, vinsertf32x8, vinsertf32x8_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinsertf32x8_zhik7zhildi, vinsertf32x8, vinsertf32x8_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf64x4_zlok0zloxloi, vinsertf64x4, vinsertf64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinsertf64x4_zlok0zloldi, vinsertf64x4, vinsertf64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf64x4_zhik7zhixhii, vinsertf64x4, vinsertf64x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinsertf64x4_zhik7zhildi, vinsertf64x4, vinsertf64x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti32x4_ylok0yloxloi, vinserti32x4, vinserti32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinserti32x4_ylok0yloldi, vinserti32x4, vinserti32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti32x4_zlok0zloxloi, vinserti32x4, vinserti32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinserti32x4_zlok0zloldi, vinserti32x4, vinserti32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti32x4_yhik7yhixhii, vinserti32x4, vinserti32x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG_PARTIAL(YMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinserti32x4_yhik7yhildi, vinserti32x4, vinserti32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti32x4_zhik7zhixhii, vinserti32x4, vinserti32x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinserti32x4_zhik7zhildi, vinserti32x4, vinserti32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti64x2_ylok0yloxloi, vinserti64x2, vinserti64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinserti64x2_ylok0yloldi, vinserti64x2, vinserti64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti64x2_zlok0zloxloi, vinserti64x2, vinserti64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinserti64x2_zlok0zloldi, vinserti64x2, vinserti64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti64x2_yhik7yhixhii, vinserti64x2, vinserti64x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG_PARTIAL(YMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinserti64x2_yhik7yhildi, vinserti64x2, vinserti64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti64x2_zhik7zhixhii, vinserti64x2, vinserti64x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinserti64x2_zhik7zhildi, vinserti64x2, vinserti64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti32x8_zlok0zloxloi, vinserti32x8, vinserti32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinserti32x8_zlok0zloldi, vinserti32x8, vinserti32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti32x8_zhik7zhixhii, vinserti32x8, vinserti32x8_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinserti32x8_zhik7zhildi, vinserti32x8, vinserti32x8_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti64x4_zlok0zloxloi, vinserti64x4, vinserti64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2), IMMARG(OPSZ_1))
OPCODE(vinserti64x4_zlok0zloldi, vinserti64x4, vinserti64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinserti64x4_zhik7zhixhii, vinserti64x4, vinserti64x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG_PARTIAL(ZMM17, OPSZ_16), REGARG(XMM31),
       IMMARG(OPSZ_1))
OPCODE(vinserti64x4_zhik7zhildi, vinserti64x4, vinserti64x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
