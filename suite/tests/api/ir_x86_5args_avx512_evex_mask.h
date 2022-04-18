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
OPCODE(vpcmpd_k0k0xlobcst, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpd_k1k7xhixhi, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpd_k1k7xhild, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpd_k1k7xhibcst, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpd_k0k0yloylo, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpd_k0k0ylold, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpd_k0k0ylobcst, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpd_k1k7yhiyhi, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpd_k1k7yhild, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpd_k1k7yhibcst, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpd_k0k0zlozlo, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpd_k0k0zlold, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpd_k0k0zlobcst, vpcmpd, vpcmpd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpd_k1k7zhizhi, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpd_k1k7zhild, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpd_k1k7zhibcst, vpcmpd, vpcmpd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpud_k0k0xloxlo, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpud_k0k0xlold, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpud_k0k0xlobcst, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpud_k1k7xhixhi, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpud_k1k7xhild, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpud_k1k7xhibcst, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpud_k0k0yloylo, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpud_k0k0ylold, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpud_k0k0ylobcst, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpud_k1k7yhiyhi, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpud_k1k7yhild, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpud_k1k7yhibcst, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpud_k0k0zlozlo, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpud_k0k0zlold, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpud_k0k0zlobcst, vpcmpud, vpcmpud_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpud_k1k7zhizhi, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpud_k1k7zhild, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpud_k1k7zhibcst, vpcmpud, vpcmpud_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpq_k0k0xloxlo, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpq_k0k0xlold, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpq_k0k0xlobcst, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpq_k1k7xhixhi, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpq_k1k7xhild, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpq_k1k7xhibcst, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpq_k0k0yloylo, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpq_k0k0ylold, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpq_k0k0ylobcst, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpq_k1k7yhiyhi, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpq_k1k7yhild, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpq_k1k7yhibcst, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpq_k0k0zlozlo, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpq_k0k0zlold, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpq_k0k0zlobcst, vpcmpq, vpcmpq_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpq_k1k7zhizhi, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpq_k1k7zhild, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpq_k1k7zhibcst, vpcmpq, vpcmpq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpuq_k0k0xloxlo, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpuq_k0k0xlold, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpuq_k0k0xlobcst, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpuq_k1k7xhixhi, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpuq_k1k7xhild, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpuq_k1k7xhibcst, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpuq_k0k0yloylo, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpuq_k0k0ylold, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpuq_k0k0ylobcst, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpuq_k1k7yhiyhi, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpuq_k1k7yhild, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpuq_k1k7yhibcst, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpuq_k0k0zlozlo, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpuq_k0k0zlold, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpuq_k0k0zlobcst, vpcmpuq, vpcmpuq_mask, 0, REGARG(K0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpuq_k1k7zhizhi, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpuq_k1k7zhild, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpuq_k1k7zhibcst, vpcmpuq, vpcmpuq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vcmpps_k0k0xloxlo, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmpps_k0k0xlold, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmpps_k0k0xlobcst, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vcmpps_k1k7xhixhi, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmpps_k1k7xhild, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmpps_k1k7xhibcst, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vcmpps_k0k0yloylo, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmpps_k0k0ylold, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmpps_k0k0ylobcst, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vcmpps_k1k7yhiyhi, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmpps_k1k7yhild, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmpps_k1k7yhibcst, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vcmpps_k0k0zlozlo, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmpps_k0k0zlold, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmpps_k0k0zlobcst, vcmpps, vcmpps_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vcmpps_k1k7zhizhi, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmpps_k1k7zhild, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmpps_k1k7zhibcst, vcmpps, vcmpps_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_4))
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
OPCODE(vcmppd_k0k0xlobcst, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vcmppd_k1k7xhixhi, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG(XMM1))
OPCODE(vcmppd_k1k7xhild, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcmppd_k1k7xhibcst, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vcmppd_k0k0yloylo, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmppd_k0k0ylold, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmppd_k0k0ylobcst, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vcmppd_k1k7yhiyhi, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), REGARG(YMM1))
OPCODE(vcmppd_k1k7yhild, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcmppd_k1k7yhibcst, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vcmppd_k0k0zlozlo, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmppd_k0k0zlold, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmppd_k0k0zlobcst, vcmppd, vcmppd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vcmppd_k1k7zhizhi, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vcmppd_k1k7zhild, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vcmppd_k1k7zhibcst, vcmppd, vcmppd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vcmpsd_k0k0xloxlo, vcmpsd, vcmpsd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcmpsd_k0k0xlold, vcmpsd, vcmpsd_mask, 0, REGARG(K0), REGARG(K0), IMMARG(OPSZ_1),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vcmpsd_k1k7xhixhi, vcmpsd, vcmpsd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vcmpsd_k1k7xhild, vcmpsd, vcmpsd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vshufps_xlok0xloxlo, vshufps, vshufps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vshufps_xlok0xlold, vshufps, vshufps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vshufps_xlok0xlobcst, vshufps, vshufps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vshufps_xhik7xhixhi, vshufps, vshufps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vshufps_xhik7xhild, vshufps, vshufps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vshufps_xhik7xhibcst, vshufps, vshufps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vshufps_ylok0yloylo, vshufps, vshufps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vshufps_ylok0ylold, vshufps, vshufps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vshufps_ylok0ylobcst, vshufps, vshufps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vshufps_yhik7yhiyhi, vshufps, vshufps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vshufps_yhik7yhild, vshufps, vshufps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vshufps_yhik7yhibcst, vshufps, vshufps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vshufps_zlok0zlozlo, vshufps, vshufps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vshufps_zlok0zlold, vshufps, vshufps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vshufps_zlok0zlobcst, vshufps, vshufps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vshufps_zhik7zhizhi, vshufps, vshufps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vshufps_zhik7zhild, vshufps, vshufps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vshufps_zhik7zhibcst, vshufps, vshufps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vshufpd_xlok0xloxlo, vshufpd, vshufpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vshufpd_xlok0xlold, vshufpd, vshufpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vshufpd_xlok0xlobcst, vshufpd, vshufpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vshufpd_xhik7xhixhi, vshufpd, vshufpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vshufpd_xhik7xhild, vshufpd, vshufpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vshufpd_xhik7xhibcst, vshufpd, vshufpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vshufpd_ylok0yloylo, vshufpd, vshufpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vshufpd_ylok0ylold, vshufpd, vshufpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vshufpd_ylok0ylobcst, vshufpd, vshufpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vshufpd_yhik7yhiyhi, vshufpd, vshufpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vshufpd_yhik7yhild, vshufpd, vshufpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vshufpd_yhik7yhibcst, vshufpd, vshufpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vshufpd_zlok0zlozlo, vshufpd, vshufpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vshufpd_zlok0zlold, vshufpd, vshufpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vshufpd_zlok0zlobcst, vshufpd, vshufpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vshufpd_zhik7zhizhi, vshufpd, vshufpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vshufpd_zhik7zhild, vshufpd, vshufpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vshufpd_zhik7zhibcst, vshufpd, vshufpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vshuff32x4_ylok0yloylo, vshuff32x4, vshuff32x4_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vshuff32x4_ylok0ylold, vshuff32x4, vshuff32x4_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vshuff32x4_ylok0ylobcst, vshuff32x4, vshuff32x4_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vshuff32x4_yhik7yhiyhi, vshuff32x4, vshuff32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vshuff32x4_yhik7yhild, vshuff32x4, vshuff32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vshuff32x4_yhik7yhibcst, vshuff32x4, vshuff32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vshuff32x4_zlok0zlozlo, vshuff32x4, vshuff32x4_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vshuff32x4_zlok0zlold, vshuff32x4, vshuff32x4_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vshuff32x4_zlok0zlobcst, vshuff32x4, vshuff32x4_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vshuff32x4_zhik7zhizhi, vshuff32x4, vshuff32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vshuff32x4_zhik7zhild, vshuff32x4, vshuff32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vshuff32x4_zhik7zhibcst, vshuff32x4, vshuff32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vshuff64x2_ylok0yloylo, vshuff64x2, vshuff64x2_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vshuff64x2_ylok0ylold, vshuff64x2, vshuff64x2_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vshuff64x2_ylok0ylobcst, vshuff64x2, vshuff64x2_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vshuff64x2_yhik7yhiyhi, vshuff64x2, vshuff64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vshuff64x2_yhik7yhild, vshuff64x2, vshuff64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vshuff64x2_yhik7yhibcst, vshuff64x2, vshuff64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vshuff64x2_zlok0zlozlo, vshuff64x2, vshuff64x2_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vshuff64x2_zlok0zlold, vshuff64x2, vshuff64x2_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vshuff64x2_zlok0zlobcst, vshuff64x2, vshuff64x2_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vshuff64x2_zhik7zhizhi, vshuff64x2, vshuff64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vshuff64x2_zhik7zhild, vshuff64x2, vshuff64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vshuff64x2_zhik7zhibcst, vshuff64x2, vshuff64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vshufi32x4_ylok0yloylo, vshufi32x4, vshufi32x4_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vshufi32x4_ylok0ylold, vshufi32x4, vshufi32x4_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vshufi32x4_ylok0ylobcst, vshufi32x4, vshufi32x4_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vshufi32x4_yhik7yhiyhi, vshufi32x4, vshufi32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vshufi32x4_yhik7yhild, vshufi32x4, vshufi32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vshufi32x4_yhik7yhibcst, vshufi32x4, vshufi32x4_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vshufi32x4_zlok0zlozlo, vshufi32x4, vshufi32x4_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vshufi32x4_zlok0zlold, vshufi32x4, vshufi32x4_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vshufi32x4_zlok0zlobcst, vshufi32x4, vshufi32x4_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vshufi32x4_zhik7zhizhi, vshufi32x4, vshufi32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vshufi32x4_zhik7zhild, vshufi32x4, vshufi32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vshufi32x4_zhik7zhibcst, vshufi32x4, vshufi32x4_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vshufi64x2_ylok0yloylo, vshufi64x2, vshufi64x2_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vshufi64x2_ylok0ylold, vshufi64x2, vshufi64x2_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vshufi64x2_ylok0ylobcst, vshufi64x2, vshufi64x2_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vshufi64x2_yhik7yhiyhi, vshufi64x2, vshufi64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vshufi64x2_yhik7yhild, vshufi64x2, vshufi64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vshufi64x2_yhik7yhibcst, vshufi64x2, vshufi64x2_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vshufi64x2_zlok0zlozlo, vshufi64x2, vshufi64x2_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vshufi64x2_zlok0zlold, vshufi64x2, vshufi64x2_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vshufi64x2_zlok0zlobcst, vshufi64x2, vshufi64x2_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vshufi64x2_zhik7zhizhi, vshufi64x2, vshufi64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vshufi64x2_zhik7zhild, vshufi64x2, vshufi64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vshufi64x2_zhik7zhibcst, vshufi64x2, vshufi64x2_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpalignr_xlok0xloxlo, vpalignr, vpalignr_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpalignr_xlok0xlold, vpalignr, vpalignr_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpalignr_xhik7xhixhi, vpalignr, vpalignr_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpalignr_xhik7xhild, vpalignr, vpalignr_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpalignr_ylok0yloylo, vpalignr, vpalignr_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpalignr_ylok0ylold, vpalignr, vpalignr_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpalignr_yhik7yhiyhi, vpalignr, vpalignr_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpalignr_yhik7yhild, vpalignr, vpalignr_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpalignr_zlok0zlozlo, vpalignr, vpalignr_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpalignr_zlok0zlold, vpalignr, vpalignr_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpalignr_zhik7zhizhi, vpalignr, vpalignr_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpalignr_zhik7zhild, vpalignr, vpalignr_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(valignd_xlok0xloxlo, valignd, valignd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(valignd_xlok0xlold, valignd, valignd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(valignd_xlok0xlobcst, valignd, valignd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(valignd_xhik7xhixhi, valignd, valignd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(valignd_xhik7xhild, valignd, valignd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(valignd_xhik7xhibcst, valignd, valignd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(valignd_ylok0yloylo, valignd, valignd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(valignd_ylok0ylold, valignd, valignd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(valignd_ylok0ylobcst, valignd, valignd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(valignd_yhik7yhiyhi, valignd, valignd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(valignd_yhik7yhild, valignd, valignd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(valignd_yhik7yhibcst, valignd, valignd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(valignd_zlok0zlozlo, valignd, valignd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(valignd_zlok0zlold, valignd, valignd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(valignd_zlok0zlobcst, valignd, valignd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(valignd_zhik7zhizhi, valignd, valignd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(valignd_zhik7zhild, valignd, valignd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(valignd_zhik7zhibcst, valignd, valignd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(valignq_xlok0xloxlo, valignq, valignq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(valignq_xlok0xlold, valignq, valignq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(valignq_xlok0xlobcst, valignq, valignq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(valignq_xhik7xhixhi, valignq, valignq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(valignq_xhik7xhild, valignq, valignq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(valignq_xhik7xhibcst, valignq, valignq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(valignq_ylok0yloylo, valignq, valignq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(valignq_ylok0ylold, valignq, valignq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(valignq_ylok0ylobcst, valignq, valignq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(valignq_yhik7yhiyhi, valignq, valignq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(valignq_yhik7yhild, valignq, valignq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(valignq_yhik7yhibcst, valignq, valignq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(valignq_zlok0zlozlo, valignq, valignq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(valignq_zlok0zlold, valignq, valignq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(valignq_zlok0zlobcst, valignq, valignq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(valignq_zhik7zhizhi, valignq, valignq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(valignq_zhik7zhild, valignq, valignq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(valignq_zhik7zhibcst, valignq, valignq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfixupimmps_xlok0xloxlo, vfixupimmps, vfixupimmps_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfixupimmps_xlok0xlold, vfixupimmps, vfixupimmps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfixupimmps_xlok0xlobcst, vfixupimmps, vfixupimmps_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfixupimmps_xhik7xhixhi, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfixupimmps_xhik7xhild, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfixupimmps_xhik7xhibcst, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfixupimmps_ylok0yloylo, vfixupimmps, vfixupimmps_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfixupimmps_ylok0ylold, vfixupimmps, vfixupimmps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfixupimmps_ylok0ylobcst, vfixupimmps, vfixupimmps_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfixupimmps_yhik7yhiyhi, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfixupimmps_yhik7yhild, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfixupimmps_yhik7yhibcst, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfixupimmps_zlok0zlozlo, vfixupimmps, vfixupimmps_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfixupimmps_zlok0zlold, vfixupimmps, vfixupimmps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfixupimmps_zlok0zlobcst, vfixupimmps, vfixupimmps_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfixupimmps_zhik7zhizhi, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfixupimmps_zhik7zhild, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfixupimmps_zhik7zhibcst, vfixupimmps, vfixupimmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfixupimmpd_xlok0xloxlo, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfixupimmpd_xlok0xlold, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfixupimmpd_xlok0xlobcst, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfixupimmpd_xhik7xhixhi, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfixupimmpd_xhik7xhild, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfixupimmpd_xhik7xhibcst, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfixupimmpd_ylok0yloylo, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfixupimmpd_ylok0ylold, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfixupimmpd_ylok0ylobcst, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfixupimmpd_yhik7yhiyhi, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfixupimmpd_yhik7yhild, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfixupimmpd_yhik7yhibcst, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfixupimmpd_zlok0zlozlo, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfixupimmpd_zlok0zlold, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfixupimmpd_zlok0zlobcst, vfixupimmpd, vfixupimmpd_mask, 0, REGARG(ZMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfixupimmpd_zhik7zhizhi, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfixupimmpd_zhik7zhild, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfixupimmpd_zhik7zhibcst, vfixupimmpd, vfixupimmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfixupimmss_xlok0xloxlo, vfixupimmss, vfixupimmss_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(XMM1), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfixupimmss_xlok0xlold, vfixupimmss, vfixupimmss_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfixupimmss_xhik7xhixhi, vfixupimmss, vfixupimmss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfixupimmss_xhik7xhild, vfixupimmss, vfixupimmss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfixupimmsd_xlok0xloxlo, vfixupimmsd, vfixupimmsd_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG(XMM1), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfixupimmsd_xlok0xlold, vfixupimmsd, vfixupimmsd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfixupimmsd_xhik7xhixhi, vfixupimmsd, vfixupimmsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfixupimmsd_xhik7xhild, vfixupimmsd, vfixupimmsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vgetmantss_xlok0xloxlo, vgetmantss, vgetmantss_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vgetmantss_xlok0xlold, vgetmantss, vgetmantss_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vgetmantss_xhik7xhixhi, vgetmantss, vgetmantss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_12),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vgetmantss_xhik7xhild, vgetmantss, vgetmantss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vgetmantsd_xlok0xloxlo, vgetmantsd, vgetmantsd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vgetmantsd_xlok0xlold, vgetmantsd, vgetmantsd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vgetmantsd_xhik7xhixhi, vgetmantsd, vgetmantsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vgetmantsd_xhik7xhild, vgetmantsd, vgetmantsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrangeps_xlok0xloxlo, vrangeps, vrangeps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vrangeps_xlok0xlold, vrangeps, vrangeps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vrangeps_xlok0xlobcst, vrangeps, vrangeps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vrangeps_xhik7xhixhi, vrangeps, vrangeps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vrangeps_xhik7xhild, vrangeps, vrangeps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vrangeps_xhik7xhibcst, vrangeps, vrangeps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vrangeps_ylok0yloylo, vrangeps, vrangeps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vrangeps_ylok0ylold, vrangeps, vrangeps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vrangeps_ylok0ylobcst, vrangeps, vrangeps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vrangeps_yhik7yhiyhi, vrangeps, vrangeps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vrangeps_yhik7yhild, vrangeps, vrangeps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vrangeps_yhik7yhibcst, vrangeps, vrangeps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vrangeps_zlok0zlozlo, vrangeps, vrangeps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vrangeps_zlok0zlold, vrangeps, vrangeps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vrangeps_zlok0zlobcst, vrangeps, vrangeps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vrangeps_zhik7zhizhi, vrangeps, vrangeps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vrangeps_zhik7zhild, vrangeps, vrangeps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vrangeps_zhik7zhibcst, vrangeps, vrangeps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vrangepd_xlok0xloxlo, vrangepd, vrangepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vrangepd_xlok0xlold, vrangepd, vrangepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vrangepd_xlok0xlobcst, vrangepd, vrangepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vrangepd_xhik7xhixhi, vrangepd, vrangepd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vrangepd_xhik7xhild, vrangepd, vrangepd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vrangepd_xhik7xhibcst, vrangepd, vrangepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vrangepd_ylok0yloylo, vrangepd, vrangepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vrangepd_ylok0ylold, vrangepd, vrangepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vrangepd_ylok0ylobcst, vrangepd, vrangepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vrangepd_yhik7yhiyhi, vrangepd, vrangepd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vrangepd_yhik7yhild, vrangepd, vrangepd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vrangepd_yhik7yhibcst, vrangepd, vrangepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vrangepd_zlok0zlozlo, vrangepd, vrangepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vrangepd_zlok0zlold, vrangepd, vrangepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vrangepd_zlok0zlobcst, vrangepd, vrangepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vrangepd_zhik7zhizhi, vrangepd, vrangepd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vrangepd_zhik7zhild, vrangepd, vrangepd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vrangepd_zhik7zhibcst, vrangepd, vrangepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vrangess_xlok0xloxlo, vrangess, vrangess_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vrangess_xlok0xlold, vrangess, vrangess_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrangess_xhik7xhixhi, vrangess, vrangess_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_12), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vrangess_xhik7xhild, vrangess, vrangess_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrangesd_xlok0xloxlo, vrangesd, vrangesd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vrangesd_xlok0xlold, vrangesd, vrangesd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrangesd_xhik7xhixhi, vrangesd, vrangesd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_8), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vrangesd_xhik7xhild, vrangesd, vrangesd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vreducess_xlok0xloxlo, vreducess, vreducess_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vreducess_xlok0xlold, vreducess, vreducess_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vreducess_xhik7xhixhi, vreducess, vreducess_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_12),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vreducess_xhik7xhild, vreducess, vreducess_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vreducesd_xlok0xloxlo, vreducesd, vreducesd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vreducesd_xlok0xlold, vreducesd, vreducesd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vreducesd_xhik7xhixhi, vreducesd, vreducesd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vreducesd_xhik7xhild, vreducesd, vreducesd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vdbpsadbw_xlok0xloxlo, vdbpsadbw, vdbpsadbw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vdbpsadbw_xlok0xlold, vdbpsadbw, vdbpsadbw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vdbpsadbw_xhik7xhixhi, vdbpsadbw, vdbpsadbw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vdbpsadbw_xhik7xhild, vdbpsadbw, vdbpsadbw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vdbpsadbw_ylok0yloylo, vdbpsadbw, vdbpsadbw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vdbpsadbw_ylok0ylold, vdbpsadbw, vdbpsadbw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vdbpsadbw_yhik7yhiyhi, vdbpsadbw, vdbpsadbw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vdbpsadbw_yhik7yhild, vdbpsadbw, vdbpsadbw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vdbpsadbw_zlok0zlozlo, vdbpsadbw, vdbpsadbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vdbpsadbw_zlok0zlold, vdbpsadbw, vdbpsadbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vdbpsadbw_zhik7zhizhi, vdbpsadbw, vdbpsadbw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vdbpsadbw_zhik7zhild, vdbpsadbw, vdbpsadbw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpternlogd_xlok0xloxlo, vpternlogd, vpternlogd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpternlogd_xlok0xlold, vpternlogd, vpternlogd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpternlogd_xlok0xlobcst, vpternlogd, vpternlogd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpternlogd_xhik7xhixhi, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpternlogd_xhik7xhild, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpternlogd_xhik7xhibcst, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpternlogd_ylok0yloylo, vpternlogd, vpternlogd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpternlogd_ylok0ylold, vpternlogd, vpternlogd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpternlogd_ylok0ylobcst, vpternlogd, vpternlogd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpternlogd_yhik7yhiyhi, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpternlogd_yhik7yhild, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpternlogd_yhik7yhibcst, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpternlogd_zlok0zlozlo, vpternlogd, vpternlogd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpternlogd_zlok0zlold, vpternlogd, vpternlogd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpternlogd_zlok0zlobcst, vpternlogd, vpternlogd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpternlogd_zhik7zhizhi, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpternlogd_zhik7zhild, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpternlogd_zhik7zhibcst, vpternlogd, vpternlogd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpternlogq_xlok0xloxlo, vpternlogq, vpternlogq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpternlogq_xlok0xlold, vpternlogq, vpternlogq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpternlogq_xlok0xlobcst, vpternlogq, vpternlogq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpternlogq_xhik7xhixhi, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpternlogq_xhik7xhild, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpternlogq_xhik7xhibcst, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpternlogq_ylok0yloylo, vpternlogq, vpternlogq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpternlogq_ylok0ylold, vpternlogq, vpternlogq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpternlogq_ylok0ylobcst, vpternlogq, vpternlogq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpternlogq_yhik7yhiyhi, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpternlogq_yhik7yhild, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpternlogq_yhik7yhibcst, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpternlogq_zlok0zlozlo, vpternlogq, vpternlogq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpternlogq_zlok0zlold, vpternlogq, vpternlogq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpternlogq_zlok0zlobcst, vpternlogq, vpternlogq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpternlogq_zhik7zhizhi, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpternlogq_zhik7zhild, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpternlogq_zhik7zhibcst, vpternlogq, vpternlogq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31), MEMARG(OPSZ_8))
