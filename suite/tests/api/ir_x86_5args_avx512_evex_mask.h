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
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinsertf32x4_ylok0yloldi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf32x4_zlok0zloxloi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinsertf32x4_zlok0zloldi, vinsertf32x4, vinsertf32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf32x4_yhik7yhixhii, vinsertf32x4, vinsertf32x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinsertf32x4_yhik7yhildi, vinsertf32x4, vinsertf32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf32x4_zhik7zhixhii, vinsertf32x4, vinsertf32x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinsertf32x4_zhik7zhildi, vinsertf32x4, vinsertf32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf64x2_ylok0yloxloi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinsertf64x2_ylok0yloldi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf64x2_zlok0zloxloi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinsertf64x2_zlok0zloldi, vinsertf64x2, vinsertf64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf64x2_yhik7yhixhii, vinsertf64x2, vinsertf64x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinsertf64x2_yhik7yhildi, vinsertf64x2, vinsertf64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf64x2_zhik7zhixhii, vinsertf64x2, vinsertf64x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinsertf64x2_zhik7zhildi, vinsertf64x2, vinsertf64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf32x8_zlok0zloxloi, vinsertf32x8, vinsertf32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinsertf32x8_zlok0zloldi, vinsertf32x8, vinsertf32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf32x8_zhik7zhixhii, vinsertf32x8, vinsertf32x8_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinsertf32x8_zhik7zhildi, vinsertf32x8, vinsertf32x8_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf64x4_zlok0zloxloi, vinsertf64x4, vinsertf64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinsertf64x4_zlok0zloldi, vinsertf64x4, vinsertf64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinsertf64x4_zhik7zhixhii, vinsertf64x4, vinsertf64x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinsertf64x4_zhik7zhildi, vinsertf64x4, vinsertf64x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti32x4_ylok0yloxloi, vinserti32x4, vinserti32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinserti32x4_ylok0yloldi, vinserti32x4, vinserti32x4_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti32x4_zlok0zloxloi, vinserti32x4, vinserti32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinserti32x4_zlok0zloldi, vinserti32x4, vinserti32x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti32x4_yhik7yhixhii, vinserti32x4, vinserti32x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinserti32x4_yhik7yhildi, vinserti32x4, vinserti32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti32x4_zhik7zhixhii, vinserti32x4, vinserti32x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinserti32x4_zhik7zhildi, vinserti32x4, vinserti32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti64x2_ylok0yloxloi, vinserti64x2, vinserti64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinserti64x2_ylok0yloldi, vinserti64x2, vinserti64x2_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti64x2_zlok0zloxloi, vinserti64x2, vinserti64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinserti64x2_zlok0zloldi, vinserti64x2, vinserti64x2_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti64x2_yhik7yhixhii, vinserti64x2, vinserti64x2_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinserti64x2_yhik7yhildi, vinserti64x2, vinserti64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti64x2_zhik7zhixhii, vinserti64x2, vinserti64x2_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinserti64x2_zhik7zhildi, vinserti64x2, vinserti64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti32x8_zlok0zloxloi, vinserti32x8, vinserti32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinserti32x8_zlok0zloldi, vinserti32x8, vinserti32x8_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti32x8_zhik7zhixhii, vinserti32x8, vinserti32x8_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinserti32x8_zhik7zhildi, vinserti32x8, vinserti32x8_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti64x4_zlok0zloxloi, vinserti64x4, vinserti64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), REGARG(XMM2))
OPCODE(vinserti64x4_zlok0zloldi, vinserti64x4, vinserti64x4_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vinserti64x4_zhik7zhixhii, vinserti64x4, vinserti64x4_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM17, OPSZ_16),
       REGARG(XMM31))
OPCODE(vinserti64x4_zhik7zhildi, vinserti64x4, vinserti64x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vpcmpb_k0k0xloxlo, vpcmpb, vpcmpb_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpb_k0k0xlold, vpcmpb, vpcmpb_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpb_k1k7xhixhi, vpcmpb, vpcmpb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpb_k1k7xhild, vpcmpb, vpcmpb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpb_k0k0yloylo, vpcmpb, vpcmpb_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpb_k0k0ylold, vpcmpb, vpcmpb_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpb_k1k7yhiyhi, vpcmpb, vpcmpb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpb_k1k7yhild, vpcmpb, vpcmpb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpb_k0k0zlozlo, vpcmpb, vpcmpb_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpb_k0k0zlold, vpcmpb, vpcmpb_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpb_k1k7zhizhi, vpcmpb, vpcmpb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpb_k1k7zhild, vpcmpb, vpcmpb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpub_k0k0xloxlo, vpcmpub, vpcmpub_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpub_k0k0xlold, vpcmpub, vpcmpub_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpub_k1k7xhixhi, vpcmpub, vpcmpub_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpub_k1k7xhild, vpcmpub, vpcmpub_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpub_k0k0yloylo, vpcmpub, vpcmpub_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpub_k0k0ylold, vpcmpub, vpcmpub_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpub_k1k7yhiyhi, vpcmpub, vpcmpub_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpub_k1k7yhild, vpcmpub, vpcmpub_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpub_k0k0zlozlo, vpcmpub, vpcmpub_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpub_k0k0zlold, vpcmpub, vpcmpub_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpub_k1k7zhizhi, vpcmpub, vpcmpub_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpub_k1k7zhild, vpcmpub, vpcmpub_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpw_k0k0xloxlo, vpcmpw, vpcmpw_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpw_k0k0xlold, vpcmpw, vpcmpw_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpw_k1k7xhixhi, vpcmpw, vpcmpw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpw_k1k7xhild, vpcmpw, vpcmpw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpw_k0k0yloylo, vpcmpw, vpcmpw_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpw_k0k0ylold, vpcmpw, vpcmpw_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpw_k1k7yhiyhi, vpcmpw, vpcmpw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpw_k1k7yhild, vpcmpw, vpcmpw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpw_k0k0zlozlo, vpcmpw, vpcmpw_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpw_k0k0zlold, vpcmpw, vpcmpw_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpw_k1k7zhizhi, vpcmpw, vpcmpw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpw_k1k7zhild, vpcmpw, vpcmpw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpuw_k0k0xloxlo, vpcmpuw, vpcmpuw_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpuw_k0k0xlold, vpcmpuw, vpcmpuw_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpuw_k1k7xhixhi, vpcmpuw, vpcmpuw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpuw_k1k7xhild, vpcmpuw, vpcmpuw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpuw_k0k0yloylo, vpcmpuw, vpcmpuw_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpuw_k0k0ylold, vpcmpuw, vpcmpuw_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpuw_k1k7yhiyhi, vpcmpuw, vpcmpuw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpuw_k1k7yhild, vpcmpuw, vpcmpuw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpuw_k0k0zlozlo, vpcmpuw, vpcmpuw_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpuw_k0k0zlold, vpcmpuw, vpcmpuw_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpuw_k1k7zhizhi, vpcmpuw, vpcmpuw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpuw_k1k7zhild, vpcmpuw, vpcmpuw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpd_k0k0xloxlo, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpd_k0k0xlold, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpd_k1k7xhixhi, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpd_k1k7xhild, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpd_k0k0yloylo, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpd_k0k0ylold, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpd_k1k7yhiyhi, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpd_k1k7yhild, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpd_k0k0zlozlo, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpd_k0k0zlold, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpd_k1k7zhizhi, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpd_k1k7zhild, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpud_k0k0xloxlo, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpud_k0k0xlold, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpud_k1k7xhixhi, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpud_k1k7xhild, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpud_k0k0yloylo, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpud_k0k0ylold, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpud_k1k7yhiyhi, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpud_k1k7yhild, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpud_k0k0zlozlo, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpud_k0k0zlold, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpud_k1k7zhizhi, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpud_k1k7zhild, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpq_k0k0xloxlo, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpq_k0k0xlold, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpq_k1k7xhixhi, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpq_k1k7xhild, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpq_k0k0yloylo, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpq_k0k0ylold, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpq_k1k7yhiyhi, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpq_k1k7yhild, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpq_k0k0zlozlo, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpq_k0k0zlold, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpq_k1k7zhizhi, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpq_k1k7zhild, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpuq_k0k0xloxlo, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpuq_k0k0xlold, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpuq_k1k7xhixhi, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpuq_k1k7xhild, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpuq_k0k0yloylo, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpuq_k0k0ylold, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpuq_k1k7yhiyhi, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpuq_k1k7yhild, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpuq_k0k0zlozlo, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpuq_k0k0zlold, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpuq_k1k7zhizhi, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpuq_k1k7zhild, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmpps_k0k0xloxlo, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmpps_k0k0xlold, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmpps_k1k7xhixhi, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmpps_k1k7xhild, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmpps_k0k0yloylo, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmpps_k0k0ylold, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmpps_k1k7yhiyhi, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmpps_k1k7yhild, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmpps_k0k0zlozlo, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmpps_k0k0zlold, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmpps_k1k7zhizhi, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmpps_k1k7zhild, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmpss_k0k0xloxlo, vcmpss, vcmpss_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vcmpss_k0k0xlold, vcmpss, vcmpss_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vcmpss_k1k7xhixhi, vcmpss, vcmpss_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vcmpss_k1k7xhild, vcmpss, vcmpss_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vcmppd_k0k0xloxlo, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmppd_k0k0xlold, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmppd_k1k7xhixhi, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmppd_k1k7xhild, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmppd_k0k0yloylo, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmppd_k0k0ylold, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmppd_k1k7yhiyhi, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmppd_k1k7yhild, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmppd_k0k0zlozlo, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmppd_k0k0zlold, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmppd_k1k7zhizhi, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmppd_k1k7zhild, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmpsd_k0k0xloxlo, vcmpsd, vcmpsd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcmpsd_k0k0xlold, vcmpsd, vcmpsd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vcmpsd_k1k7xhixhi, vcmpsd, vcmpsd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcmpsd_k1k7xhild, vcmpsd, vcmpsd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_8))
