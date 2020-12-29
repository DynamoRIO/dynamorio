/* **********************************************************
 * Copyright (c) 2019-2020 Google, Inc.  All rights reserved.
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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 2 sources.
 * Part B: Split up to avoid VS running out of memory (i#3992,i#4610).
 */
OPCODE(vpermilps_xlok0xloxlo, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermilps_xlok0xlold, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermilps_xlok0xlobcst, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermilps_ylok0yloylo, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermilps_ylok0ylold, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermilps_ylok0ylobcst, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermilps_zlok0zlozlo, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilps_zlok0zlold, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermilps_zlok0zlobcst, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermilps_xhik7xhixhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermilps_xhik7xhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermilps_xhik7xhibcst, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermilps_yhik7yhiyhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermilps_yhik7yhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermilps_yhik7yhibcst, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermilps_zhik7zhizhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermilps_zhik7zhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermilps_zhik7zhibcst, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermilps_xlok0xloimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_xlok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_xlok0bcstimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0yloimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0bcstimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0zloimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0bcstimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7xhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7bcstimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7yhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7bcstimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7zhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7bcstimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0xloxlo, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermilpd_xlok0xlold, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermilpd_xlok0xlobcst, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpermilpd_ylok0yloylo, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermilpd_ylok0ylold, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermilpd_ylok0ylobcst, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermilpd_zlok0zlozlo, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilpd_zlok0zlold, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermilpd_zlok0zlobcst, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermilpd_xhik7xhixhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermilpd_xhik7xhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermilpd_xhik7xhibcst, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpermilpd_yhik7yhiyhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermilpd_yhik7yhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermilpd_yhik7yhibcst, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermilpd_zhik7zhizhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermilpd_zhik7zhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermilpd_zhik7zhibcst, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermilpd_xlok0xloimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0bcstimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0yloimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0bcstimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0zloimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0bcstimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7xhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7bcstimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7yhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7bcstimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7zhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7bcstimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermb_xlok0xloxlo, vpermb, vpermb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpermb_xlok0xlold, vpermb, vpermb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpermb_ylok0yloylo, vpermb, vpermb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermb_ylok0ylold, vpermb, vpermb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermb_zlok0zlozlo, vpermb, vpermb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermb_zlok0zlold, vpermb, vpermb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermb_xhik7xhixhi, vpermb, vpermb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermb_xhik7xhild, vpermb, vpermb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermb_yhik7yhiyhi, vpermb, vpermb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermb_yhik7yhild, vpermb, vpermb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermb_zhik7zhizhi, vpermb, vpermb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermb_zhik7zhild, vpermb, vpermb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermd_ylok0yloylo, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermd_ylok0ylold, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermd_ylok0ylobcst, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermd_zlok0zlozlo, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermd_zlok0zlold, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermd_zlok0zlobcst, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermd_yhik7yhiyhi, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermd_yhik7yhild, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermd_yhik7yhibcst, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermd_zhik7zhizhi, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermd_zhik7zhild, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermd_zhik7zhibcst, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermw_xlok0xloxlo, vpermw, vpermw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpermw_xlok0xlold, vpermw, vpermw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpermw_ylok0yloylo, vpermw, vpermw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermw_ylok0ylold, vpermw, vpermw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermw_zlok0zlozlo, vpermw, vpermw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermw_zlok0zlold, vpermw, vpermw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermw_xhik7xhixhi, vpermw, vpermw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermw_xhik7xhild, vpermw, vpermw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermw_yhik7yhiyhi, vpermw, vpermw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermw_yhik7yhild, vpermw, vpermw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermw_zhik7zhizhi, vpermw, vpermw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermw_zhik7zhild, vpermw, vpermw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermps_ylok0yloylo, vpermps, vpermps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermps_ylok0ylold, vpermps, vpermps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermps_ylok0ylobcst, vpermps, vpermps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermps_zlok0zlozlo, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermps_zlok0zlold, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermps_zlok0zlobcst, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermps_yhik7yhiyhi, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermps_yhik7yhild, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermps_yhik7yhibcst, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermps_zhik7zhizhi, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermps_zhik7zhild, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermps_zhik7zhibcst, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermq_ylok0yloylo, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermq_ylok0ylold, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermq_ylok0ylobcst, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermq_zlok0zlozlo, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermq_zlok0zlold, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermq_zlok0zlobcst, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermq_yhik7yhiyhi, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermq_yhik7yhild, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermq_yhik7yhibcst, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermq_zhik7zhizhi, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermq_zhik7zhild, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermq_zhik7zhibcst, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermq_ylok0yloimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       IMMARG(OPSZ_1))
OPCODE(vpermq_ylok0ldimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermq_ylok0bcstimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0zloimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0ldimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0bcstimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7yhiimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7ldimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7bcstimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7zhiimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7ldimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7bcstimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0yloimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0ldimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0bcstimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7yhiimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7ldimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7bcstimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0zloimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0ldimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0bcstimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7zhiimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7ldimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7bcstimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0yloylo, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermpd_ylok0ylold, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermpd_ylok0ylobcst, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermpd_zlok0zlozlo, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermpd_zlok0zlold, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermpd_zlok0zlobcst, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermpd_yhik7yhiyhi, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermpd_yhik7yhild, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermpd_yhik7yhibcst, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermpd_zhik7zhizhi, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermpd_zhik7zhild, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermpd_zhik7zhibcst, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2ps_xlok0xloxlo, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2ps_xlok0xlold, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2ps_xlok0xlobcst, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_ylok0yloylo, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2ps_ylok0ylold, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2ps_ylok0ylobcst, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_zlok0zlozlo, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2ps_zlok0zlold, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2ps_zlok0zlobcst, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_xhik7xhixhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2ps_xhik7xhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2ps_xhik7xhibcst, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_yhik7yhiyhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2ps_yhik7yhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2ps_yhik7yhibcst, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_zhik7zhizhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2ps_zhik7zhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2ps_zhik7zhibcst, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2pd_xlok0xloxlo, vpermi2pd, vpermi2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2pd_xlok0xlold, vpermi2pd, vpermi2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2pd_xlok0xlobcst, vpermi2pd, vpermi2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpermi2pd_ylok0yloylo, vpermi2pd, vpermi2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2pd_ylok0ylold, vpermi2pd, vpermi2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2pd_ylok0ylobcst, vpermi2pd, vpermi2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermi2pd_zlok0zlozlo, vpermi2pd, vpermi2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2pd_zlok0zlold, vpermi2pd, vpermi2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2pd_zlok0zlobcst, vpermi2pd, vpermi2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermi2pd_xhik7xhixhi, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2pd_xhik7xhild, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2pd_xhik7xhibcst, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2pd_yhik7yhiyhi, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2pd_yhik7yhild, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2pd_yhik7yhibcst, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2pd_zhik7zhizhi, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2pd_zhik7zhild, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2pd_zhik7zhibcst, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2d_xlok0xloxlo, vpermi2d, vpermi2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2d_xlok0xlold, vpermi2d, vpermi2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2d_xlok0xlobcst, vpermi2d, vpermi2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2d_ylok0yloylo, vpermi2d, vpermi2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2d_ylok0ylold, vpermi2d, vpermi2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2d_ylok0ylobcst, vpermi2d, vpermi2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2d_zlok0zlozlo, vpermi2d, vpermi2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2d_zlok0zlold, vpermi2d, vpermi2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2d_zlok0zlobcst, vpermi2d, vpermi2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2d_xhik7xhixhi, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2d_xhik7xhild, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2d_xhik7xhibcst, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2d_yhik7yhiyhi, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2d_yhik7yhild, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2d_yhik7yhibcst, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2d_zhik7zhizhi, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2d_zhik7zhild, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2d_zhik7zhibcst, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2q_xlok0xloxlo, vpermi2q, vpermi2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2q_xlok0xlold, vpermi2q, vpermi2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2q_xlok0xlobcst, vpermi2q, vpermi2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpermi2q_ylok0yloylo, vpermi2q, vpermi2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2q_ylok0ylold, vpermi2q, vpermi2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2q_ylok0ylobcst, vpermi2q, vpermi2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermi2q_zlok0zlozlo, vpermi2q, vpermi2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2q_zlok0zlold, vpermi2q, vpermi2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2q_zlok0zlobcst, vpermi2q, vpermi2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermi2q_xhik7xhixhi, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2q_xhik7xhild, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2q_xhik7xhibcst, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2q_yhik7yhiyhi, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2q_yhik7yhild, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2q_yhik7yhibcst, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2q_zhik7zhizhi, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2q_zhik7zhild, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2q_zhik7zhibcst, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2b_xlok0xloxlo, vpermi2b, vpermi2b_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2b_xlok0xlold, vpermi2b, vpermi2b_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2b_ylok0yloylo, vpermi2b, vpermi2b_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2b_ylok0ylold, vpermi2b, vpermi2b_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2b_zlok0zlozlo, vpermi2b, vpermi2b_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2b_zlok0zlold, vpermi2b, vpermi2b_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2b_xhik7xhixhi, vpermi2b, vpermi2b_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2b_xhik7xhild, vpermi2b, vpermi2b_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2b_yhik7yhiyhi, vpermi2b, vpermi2b_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2b_yhik7yhild, vpermi2b, vpermi2b_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2b_zhik7zhizhi, vpermi2b, vpermi2b_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2b_zhik7zhild, vpermi2b, vpermi2b_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2w_xlok0xloxlo, vpermi2w, vpermi2w_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2w_xlok0xlold, vpermi2w, vpermi2w_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2w_ylok0yloylo, vpermi2w, vpermi2w_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2w_ylok0ylold, vpermi2w, vpermi2w_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2w_zlok0zlozlo, vpermi2w, vpermi2w_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2w_zlok0zlold, vpermi2w, vpermi2w_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2w_xhik7xhixhi, vpermi2w, vpermi2w_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2w_xhik7xhild, vpermi2w, vpermi2w_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2w_yhik7yhiyhi, vpermi2w, vpermi2w_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2w_yhik7yhild, vpermi2w, vpermi2w_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2w_zhik7zhizhi, vpermi2w, vpermi2w_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2w_zhik7zhild, vpermi2w, vpermi2w_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2d_xlok0xloxlo, vpermt2d, vpermt2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2d_xlok0xlold, vpermt2d, vpermt2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2d_xlok0xlobcst, vpermt2d, vpermt2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermt2d_ylok0yloylo, vpermt2d, vpermt2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2d_ylok0ylold, vpermt2d, vpermt2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2d_ylok0ylobcst, vpermt2d, vpermt2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermt2d_zlok0zlozlo, vpermt2d, vpermt2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2d_zlok0zlold, vpermt2d, vpermt2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2d_zlok0zlobcst, vpermt2d, vpermt2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermt2d_xhik7xhixhi, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2d_xhik7xhild, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2d_xhik7xhibcst, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermt2d_yhik7yhiyhi, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2d_yhik7yhild, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2d_yhik7yhibcst, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermt2d_zhik7zhizhi, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2d_zhik7zhild, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2d_zhik7zhibcst, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermt2q_xlok0xloxlo, vpermt2q, vpermt2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2q_xlok0xlold, vpermt2q, vpermt2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2q_xlok0xlobcst, vpermt2q, vpermt2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpermt2q_ylok0yloylo, vpermt2q, vpermt2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2q_ylok0ylold, vpermt2q, vpermt2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2q_ylok0ylobcst, vpermt2q, vpermt2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermt2q_zlok0zlozlo, vpermt2q, vpermt2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2q_zlok0zlold, vpermt2q, vpermt2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2q_zlok0zlobcst, vpermt2q, vpermt2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermt2q_xhik7xhixhi, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2q_xhik7xhild, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2q_xhik7xhibcst, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpermt2q_yhik7yhiyhi, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2q_yhik7yhild, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2q_yhik7yhibcst, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermt2q_zhik7zhizhi, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2q_zhik7zhild, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2q_zhik7zhibcst, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermt2b_xlok0xloxlo, vpermt2b, vpermt2b_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2b_xlok0xlold, vpermt2b, vpermt2b_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2b_ylok0yloylo, vpermt2b, vpermt2b_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2b_ylok0ylold, vpermt2b, vpermt2b_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2b_zlok0zlozlo, vpermt2b, vpermt2b_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2b_zlok0zlold, vpermt2b, vpermt2b_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2b_xhik7xhixhi, vpermt2b, vpermt2b_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2b_xhik7xhild, vpermt2b, vpermt2b_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2b_yhik7yhiyhi, vpermt2b, vpermt2b_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2b_yhik7yhild, vpermt2b, vpermt2b_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2b_zhik7zhizhi, vpermt2b, vpermt2b_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2b_zhik7zhild, vpermt2b, vpermt2b_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2w_xlok0xloxlo, vpermt2w, vpermt2w_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2w_xlok0xlold, vpermt2w, vpermt2w_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2w_ylok0yloylo, vpermt2w, vpermt2w_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2w_ylok0ylold, vpermt2w, vpermt2w_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2w_zlok0zlozlo, vpermt2w, vpermt2w_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2w_zlok0zlold, vpermt2w, vpermt2w_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2w_xhik7xhixhi, vpermt2w, vpermt2w_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2w_xhik7xhild, vpermt2w, vpermt2w_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2w_yhik7yhiyhi, vpermt2w, vpermt2w_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2w_yhik7yhild, vpermt2w, vpermt2w_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2w_zhik7zhizhi, vpermt2w, vpermt2w_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2w_zhik7zhild, vpermt2w, vpermt2w_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2ps_xlok0xloxlo, vpermt2ps, vpermt2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2ps_xlok0xlold, vpermt2ps, vpermt2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2ps_xlok0xlobcst, vpermt2ps, vpermt2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermt2ps_ylok0yloylo, vpermt2ps, vpermt2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2ps_ylok0ylold, vpermt2ps, vpermt2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2ps_ylok0ylobcst, vpermt2ps, vpermt2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermt2ps_zlok0zlozlo, vpermt2ps, vpermt2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2ps_zlok0zlold, vpermt2ps, vpermt2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2ps_zlok0zlobcst, vpermt2ps, vpermt2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermt2ps_xhik7xhixhi, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2ps_xhik7xhild, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2ps_xhik7xhibcst, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermt2ps_yhik7yhiyhi, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2ps_yhik7yhild, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2ps_yhik7yhibcst, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermt2ps_zhik7zhizhi, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2ps_zhik7zhild, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2ps_zhik7zhibcst, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermt2pd_xlok0xloxlo, vpermt2pd, vpermt2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2pd_xlok0xlold, vpermt2pd, vpermt2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2pd_xlok0xlobcst, vpermt2pd, vpermt2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpermt2pd_ylok0yloylo, vpermt2pd, vpermt2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2pd_ylok0ylold, vpermt2pd, vpermt2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2pd_ylok0ylobcst, vpermt2pd, vpermt2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermt2pd_zlok0zlozlo, vpermt2pd, vpermt2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2pd_zlok0zlold, vpermt2pd, vpermt2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2pd_zlok0zlobcst, vpermt2pd, vpermt2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermt2pd_xhik7xhixhi, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2pd_xhik7xhild, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2pd_xhik7xhibcst, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpermt2pd_yhik7yhiyhi, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2pd_yhik7yhild, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2pd_yhik7yhibcst, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermt2pd_zhik7zhizhi, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2pd_zhik7zhild, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2pd_zhik7zhibcst, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpunpcklbw_xlok0xloxlo, vpunpcklbw, vpunpcklbw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpcklbw_xlok0xlold, vpunpcklbw, vpunpcklbw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpcklbw_xhik7xhixhi, vpunpcklbw, vpunpcklbw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpcklbw_xhik7xhild, vpunpcklbw, vpunpcklbw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpcklbw_ylok0yloylo, vpunpcklbw, vpunpcklbw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpcklbw_ylok0ylold, vpunpcklbw, vpunpcklbw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpcklbw_yhik7yhiyhi, vpunpcklbw, vpunpcklbw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpcklbw_yhik7yhild, vpunpcklbw, vpunpcklbw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpcklbw_zlok0zlozlo, vpunpcklbw, vpunpcklbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpcklbw_zlok0zlold, vpunpcklbw, vpunpcklbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpcklbw_zhik7zhizhi, vpunpcklbw, vpunpcklbw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpcklbw_zhik7zhild, vpunpcklbw, vpunpcklbw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpcklwd_xlok0xloxlo, vpunpcklwd, vpunpcklwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpcklwd_xlok0xlold, vpunpcklwd, vpunpcklwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpcklwd_xhik7xhixhi, vpunpcklwd, vpunpcklwd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpcklwd_xhik7xhild, vpunpcklwd, vpunpcklwd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpcklwd_ylok0yloylo, vpunpcklwd, vpunpcklwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpcklwd_ylok0ylold, vpunpcklwd, vpunpcklwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpcklwd_yhik7yhiyhi, vpunpcklwd, vpunpcklwd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpcklwd_yhik7yhild, vpunpcklwd, vpunpcklwd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpcklwd_zlok0zlozlo, vpunpcklwd, vpunpcklwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpcklwd_zlok0zlold, vpunpcklwd, vpunpcklwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpcklwd_zhik7zhizhi, vpunpcklwd, vpunpcklwd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpcklwd_zhik7zhild, vpunpcklwd, vpunpcklwd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckldq_xlok0xloxlo, vpunpckldq, vpunpckldq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpckldq_xlok0xlold, vpunpckldq, vpunpckldq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckldq_xlok0xlobcst, vpunpckldq, vpunpckldq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpunpckldq_xhik7xhixhi, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckldq_xhik7xhild, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckldq_xhik7xhibcst, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpunpckldq_ylok0yloylo, vpunpckldq, vpunpckldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckldq_ylok0ylold, vpunpckldq, vpunpckldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckldq_ylok0ylobcst, vpunpckldq, vpunpckldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpunpckldq_yhik7yhiyhi, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckldq_yhik7yhild, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckldq_yhik7yhibcst, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpunpckldq_zlok0zlozlo, vpunpckldq, vpunpckldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckldq_zlok0zlold, vpunpckldq, vpunpckldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckldq_zlok0zlobcst, vpunpckldq, vpunpckldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpunpckldq_zhik7zhizhi, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckldq_zhik7zhild, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckldq_zhik7zhibcst, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpunpcklqdq_xlok0xloxlo, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpcklqdq_xlok0xlold, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpcklqdq_xlok0xlobcst, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpunpcklqdq_xhik7xhixhi, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpcklqdq_xhik7xhild, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpcklqdq_xhik7xhibcst, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpunpcklqdq_ylok0yloylo, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpcklqdq_ylok0ylold, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpcklqdq_ylok0ylobcst, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpunpcklqdq_yhik7yhiyhi, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpcklqdq_yhik7yhild, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpcklqdq_yhik7yhibcst, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpunpcklqdq_zlok0zlozlo, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpcklqdq_zlok0zlold, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpcklqdq_zlok0zlobcst, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpunpcklqdq_zhik7zhizhi, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpcklqdq_zhik7zhild, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpcklqdq_zhik7zhibcst, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpunpckhbw_xlok0xloxlo, vpunpckhbw, vpunpckhbw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpckhbw_xlok0xlold, vpunpckhbw, vpunpckhbw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhbw_xhik7xhixhi, vpunpckhbw, vpunpckhbw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckhbw_xhik7xhild, vpunpckhbw, vpunpckhbw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckhbw_ylok0yloylo, vpunpckhbw, vpunpckhbw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckhbw_ylok0ylold, vpunpckhbw, vpunpckhbw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckhbw_yhik7yhiyhi, vpunpckhbw, vpunpckhbw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckhbw_yhik7yhild, vpunpckhbw, vpunpckhbw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckhbw_zlok0zlozlo, vpunpckhbw, vpunpckhbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckhbw_zlok0zlold, vpunpckhbw, vpunpckhbw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckhbw_zhik7zhizhi, vpunpckhbw, vpunpckhbw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckhbw_zhik7zhild, vpunpckhbw, vpunpckhbw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckhwd_xlok0xloxlo, vpunpckhwd, vpunpckhwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpckhwd_xlok0xlold, vpunpckhwd, vpunpckhwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhwd_xhik7xhixhi, vpunpckhwd, vpunpckhwd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckhwd_xhik7xhild, vpunpckhwd, vpunpckhwd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckhwd_ylok0yloylo, vpunpckhwd, vpunpckhwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckhwd_ylok0ylold, vpunpckhwd, vpunpckhwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckhwd_yhik7yhiyhi, vpunpckhwd, vpunpckhwd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckhwd_yhik7yhild, vpunpckhwd, vpunpckhwd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckhwd_zlok0zlozlo, vpunpckhwd, vpunpckhwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckhwd_zlok0zlold, vpunpckhwd, vpunpckhwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckhwd_zhik7zhizhi, vpunpckhwd, vpunpckhwd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckhwd_zhik7zhild, vpunpckhwd, vpunpckhwd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckhdq_xlok0xloxlo, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpckhdq_xlok0xlold, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhdq_xlok0xlobcst, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpunpckhdq_xhik7xhixhi, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckhdq_xhik7xhild, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckhdq_xhik7xhibcst, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpunpckhdq_ylok0yloylo, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckhdq_ylok0ylold, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckhdq_ylok0ylobcst, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpunpckhdq_yhik7yhiyhi, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckhdq_yhik7yhild, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckhdq_yhik7yhibcst, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpunpckhdq_zlok0zlozlo, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckhdq_zlok0zlold, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckhdq_zlok0zlobcst, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpunpckhdq_zhik7zhizhi, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckhdq_zhik7zhild, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckhdq_zhik7zhibcst, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpunpckhqdq_xlok0xloxlo, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpckhqdq_xlok0xlold, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhqdq_xlok0xlobcst, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpunpckhqdq_xhik7xhixhi, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckhqdq_xhik7xhild, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckhqdq_xhik7xhibcst, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpunpckhqdq_ylok0yloylo, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckhqdq_ylok0ylold, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckhqdq_ylok0ylobcst, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpunpckhqdq_yhik7yhiyhi, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckhqdq_yhik7yhild, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckhqdq_yhik7yhibcst, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpunpckhqdq_zlok0zlozlo, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckhqdq_zlok0zlold, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckhqdq_zlok0zlobcst, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpunpckhqdq_zhik7zhizhi, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckhqdq_zhik7zhild, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckhqdq_zhik7zhibcst, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpacksswb_xlok0xloxlo, vpacksswb, vpacksswb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpacksswb_xlok0xlold, vpacksswb, vpacksswb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpacksswb_xhik7xhixhi, vpacksswb, vpacksswb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpacksswb_xhik7xhild, vpacksswb, vpacksswb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpacksswb_ylok0yloylo, vpacksswb, vpacksswb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpacksswb_ylok0ylold, vpacksswb, vpacksswb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpacksswb_yhik7yhiyhi, vpacksswb, vpacksswb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpacksswb_yhik7yhild, vpacksswb, vpacksswb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpacksswb_zlok0zlozlo, vpacksswb, vpacksswb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpacksswb_zlok0zlold, vpacksswb, vpacksswb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpacksswb_zhik7zhizhi, vpacksswb, vpacksswb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpacksswb_zhik7zhild, vpacksswb, vpacksswb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpackssdw_xlok0xloxlo, vpackssdw, vpackssdw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpackssdw_xlok0xlold, vpackssdw, vpackssdw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpackssdw_xlok0xlobcst, vpackssdw, vpackssdw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpackssdw_xhik7xhixhi, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpackssdw_xhik7xhild, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpackssdw_xhik7xhibcst, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpackssdw_ylok0yloylo, vpackssdw, vpackssdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpackssdw_ylok0ylold, vpackssdw, vpackssdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpackssdw_ylok0ylobcst, vpackssdw, vpackssdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpackssdw_yhik7yhiyhi, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpackssdw_yhik7yhild, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpackssdw_yhik7yhibcst, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpackssdw_zlok0zlozlo, vpackssdw, vpackssdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpackssdw_zlok0zlold, vpackssdw, vpackssdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpackssdw_zlok0zlobcst, vpackssdw, vpackssdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpackssdw_zhik7zhizhi, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpackssdw_zhik7zhild, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpackssdw_zhik7zhibcst, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpackusdw_xlok0xloxlo, vpackusdw, vpackusdw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpackusdw_xlok0xlold, vpackusdw, vpackusdw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpackusdw_xlok0xlobcst, vpackusdw, vpackusdw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpackusdw_xhik7xhixhi, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpackusdw_xhik7xhild, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpackusdw_xhik7xhibcst, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpackusdw_ylok0yloylo, vpackusdw, vpackusdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpackusdw_ylok0ylold, vpackusdw, vpackusdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpackusdw_ylok0ylobcst, vpackusdw, vpackusdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpackusdw_yhik7yhiyhi, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpackusdw_yhik7yhild, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpackusdw_yhik7yhibcst, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpackusdw_zlok0zlozlo, vpackusdw, vpackusdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpackusdw_zlok0zlold, vpackusdw, vpackusdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpackusdw_zlok0zlobcst, vpackusdw, vpackusdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpackusdw_zhik7zhizhi, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpackusdw_zhik7zhild, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpackusdw_zhik7zhibcst, vpackusdw, vpackusdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vextractf32x4_xlok0yloi, vextractf32x4, vextractf32x4_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextractf32x4_xhik7yhii, vextractf32x4, vextractf32x4_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextractf32x4_k0yloist, vextractf32x4, vextractf32x4_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextractf32x4_k7yhiist, vextractf32x4, vextractf32x4_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextractf32x4_xlok0zloi, vextractf32x4, vextractf32x4_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextractf32x4_xhik7zhii, vextractf32x4, vextractf32x4_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextractf32x4_k0zloist, vextractf32x4, vextractf32x4_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextractf32x4_k7zhiist, vextractf32x4, vextractf32x4_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextractf64x2_xlok0yloi, vextractf64x2, vextractf64x2_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextractf64x2_xhik7yhii, vextractf64x2, vextractf64x2_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextractf64x2_k0yloist, vextractf64x2, vextractf64x2_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextractf64x2_k7yhiist, vextractf64x2, vextractf64x2_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextractf64x2_xlok0zloi, vextractf64x2, vextractf64x2_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextractf64x2_xhik7zhii, vextractf64x2, vextractf64x2_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextractf64x2_k0zloist, vextractf64x2, vextractf64x2_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextractf64x2_k7zhiist, vextractf64x2, vextractf64x2_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextractf32x8_xlok0zloi, vextractf32x8, vextractf32x8_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextractf32x8_xhik7zhii, vextractf32x8, vextractf32x8_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextractf32x8_k0zloist, vextractf32x8, vextractf32x8_mask, 0, MEMARG(OPSZ_32),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextractf32x8_k7zhiist, vextractf32x8, vextractf32x8_mask, X64_ONLY,
       MEMARG(OPSZ_32), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextractf64x4_xlok0zloi, vextractf64x4, vextractf64x4_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextractf64x4_xhik7zhii, vextractf64x4, vextractf64x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextractf64x4_k0zloist, vextractf64x4, vextractf64x4_mask, 0, MEMARG(OPSZ_32),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextractf64x4_k7zhiist, vextractf64x4, vextractf64x4_mask, X64_ONLY,
       MEMARG(OPSZ_32), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextracti32x4_xlok0yloi, vextracti32x4, vextracti32x4_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextracti32x4_xhik7yhii, vextracti32x4, vextracti32x4_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextracti32x4_k0yloist, vextracti32x4, vextracti32x4_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextracti32x4_k7yhiist, vextracti32x4, vextracti32x4_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextracti32x4_xlok0zloi, vextracti32x4, vextracti32x4_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextracti32x4_xhik7zhii, vextracti32x4, vextracti32x4_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextracti32x4_k0zloist, vextracti32x4, vextracti32x4_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextracti32x4_k7zhiist, vextracti32x4, vextracti32x4_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextracti64x2_xlok0yloi, vextracti64x2, vextracti64x2_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextracti64x2_xhik7yhii, vextracti64x2, vextracti64x2_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextracti64x2_k0yloist, vextracti64x2, vextracti64x2_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vextracti64x2_k7yhiist, vextracti64x2, vextracti64x2_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(YMM31, OPSZ_16))
OPCODE(vextracti64x2_xlok0zloi, vextracti64x2, vextracti64x2_mask, 0, REGARG(XMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextracti64x2_xhik7zhii, vextracti64x2, vextracti64x2_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextracti64x2_k0zloist, vextracti64x2, vextracti64x2_mask, 0, MEMARG(OPSZ_16),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_16))
OPCODE(vextracti64x2_k7zhiist, vextracti64x2, vextracti64x2_mask, X64_ONLY,
       MEMARG(OPSZ_16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_16))
OPCODE(vextracti32x8_xlok0zloi, vextracti32x8, vextracti32x8_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextracti32x8_xhik7zhii, vextracti32x8, vextracti32x8_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextracti32x8_k0zloist, vextracti32x8, vextracti32x8_mask, 0, MEMARG(OPSZ_32),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextracti32x8_k7zhiist, vextracti32x8, vextracti32x8_mask, X64_ONLY,
       MEMARG(OPSZ_32), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextracti64x4_xlok0zloi, vextracti64x4, vextracti64x4_mask, 0, REGARG(YMM0),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextracti64x4_xhik7zhii, vextracti64x4, vextracti64x4_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vextracti64x4_k0zloist, vextracti64x4, vextracti64x4_mask, 0, MEMARG(OPSZ_32),
       REGARG(K0), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM1, OPSZ_32))
OPCODE(vextracti64x4_k7zhiist, vextracti64x4, vextracti64x4_mask, X64_ONLY,
       MEMARG(OPSZ_32), REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(ZMM31, OPSZ_32))
OPCODE(vpcmpgtb_k0k0xloxlo, vpcmpgtb, vpcmpgtb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpgtb_k1k7xhixhi, vpcmpgtb, vpcmpgtb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpgtb_k0k0xlold, vpcmpgtb, vpcmpgtb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpgtb_k1k7xhild, vpcmpgtb, vpcmpgtb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpgtb_k0k0yloylo, vpcmpgtb, vpcmpgtb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpgtb_k1k7yhiyhi, vpcmpgtb, vpcmpgtb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpgtb_k0k0ylold, vpcmpgtb, vpcmpgtb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpgtb_k1k7yhild, vpcmpgtb, vpcmpgtb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpgtb_k0k0zlozlo, vpcmpgtb, vpcmpgtb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpgtb_k1k7zhizhi, vpcmpgtb, vpcmpgtb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpgtb_k0k0zlold, vpcmpgtb, vpcmpgtb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpgtb_k1k7zhild, vpcmpgtb, vpcmpgtb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpgtw_k0k0xloxlo, vpcmpgtw, vpcmpgtw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpgtw_k1k7xhixhi, vpcmpgtw, vpcmpgtw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpgtw_k0k0xlold, vpcmpgtw, vpcmpgtw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpgtw_k1k7xhild, vpcmpgtw, vpcmpgtw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpgtw_k0k0yloylo, vpcmpgtw, vpcmpgtw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpgtw_k1k7yhiyhi, vpcmpgtw, vpcmpgtw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpgtw_k0k0ylold, vpcmpgtw, vpcmpgtw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpgtw_k1k7yhild, vpcmpgtw, vpcmpgtw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpgtw_k0k0zlozlo, vpcmpgtw, vpcmpgtw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpgtw_k1k7zhizhi, vpcmpgtw, vpcmpgtw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpgtw_k0k0zlold, vpcmpgtw, vpcmpgtw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpgtw_k1k7zhild, vpcmpgtw, vpcmpgtw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpgtd_k0k0xloxlo, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpgtd_k1k7xhixhi, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpgtd_k0k0xlold, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpgtd_k1k7xhild, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpgtd_k0k0xlobcst, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpgtd_k1k7xhibcst, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_4))
OPCODE(vpcmpgtd_k0k0yloylo, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpgtd_k1k7yhiyhi, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpgtd_k0k0ylold, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpgtd_k1k7yhild, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpgtd_k0k0ylobcst, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpgtd_k1k7yhibcst, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_4))
OPCODE(vpcmpgtd_k0k0zlozlo, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpgtd_k1k7zhizhi, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpgtd_k0k0zlold, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpgtd_k1k7zhild, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpgtd_k0k0zlobcst, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpgtd_k1k7zhibcst, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_4))
OPCODE(vpcmpgtq_k0k0xloxlo, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpgtq_k1k7xhixhi, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpgtq_k0k0xlold, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpgtq_k1k7xhild, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpgtq_k0k0xlobcst, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpgtq_k1k7xhibcst, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_8))
OPCODE(vpcmpgtq_k0k0yloylo, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpgtq_k1k7yhiyhi, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpgtq_k0k0ylold, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpgtq_k1k7yhild, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpgtq_k0k0ylobcst, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpgtq_k1k7yhibcst, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_8))
OPCODE(vpcmpgtq_k0k0zlozlo, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpgtq_k1k7zhizhi, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpgtq_k0k0zlold, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpgtq_k1k7zhild, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpgtq_k0k0zlobcst, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpgtq_k1k7zhibcst, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_8))
OPCODE(vpcmpeqb_k0k0xloxlo, vpcmpeqb, vpcmpeqb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpeqb_k1k7xhixhi, vpcmpeqb, vpcmpeqb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpeqb_k0k0xlold, vpcmpeqb, vpcmpeqb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpeqb_k1k7xhild, vpcmpeqb, vpcmpeqb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpeqb_k0k0yloylo, vpcmpeqb, vpcmpeqb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpeqb_k1k7yhiyhi, vpcmpeqb, vpcmpeqb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpeqb_k0k0ylold, vpcmpeqb, vpcmpeqb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpeqb_k1k7yhild, vpcmpeqb, vpcmpeqb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpeqb_k0k0zlozlo, vpcmpeqb, vpcmpeqb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpeqb_k1k7zhizhi, vpcmpeqb, vpcmpeqb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpeqb_k0k0zlold, vpcmpeqb, vpcmpeqb_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpeqb_k1k7zhild, vpcmpeqb, vpcmpeqb_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpeqw_k0k0xloxlo, vpcmpeqw, vpcmpeqw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpeqw_k1k7xhixhi, vpcmpeqw, vpcmpeqw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpeqw_k0k0xlold, vpcmpeqw, vpcmpeqw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpeqw_k1k7xhild, vpcmpeqw, vpcmpeqw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpeqw_k0k0yloylo, vpcmpeqw, vpcmpeqw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpeqw_k1k7yhiyhi, vpcmpeqw, vpcmpeqw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpeqw_k0k0ylold, vpcmpeqw, vpcmpeqw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpeqw_k1k7yhild, vpcmpeqw, vpcmpeqw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpeqw_k0k0zlozlo, vpcmpeqw, vpcmpeqw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpeqw_k1k7zhizhi, vpcmpeqw, vpcmpeqw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpeqw_k0k0zlold, vpcmpeqw, vpcmpeqw_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpeqw_k1k7zhild, vpcmpeqw, vpcmpeqw_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpeqd_k0k0xloxlo, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpeqd_k1k7xhixhi, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpeqd_k0k0xlold, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpeqd_k1k7xhild, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpeqd_k0k0xlobcst, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpeqd_k1k7xhibcst, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_4))
OPCODE(vpcmpeqd_k0k0yloylo, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpeqd_k1k7yhiyhi, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpeqd_k0k0ylold, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpeqd_k1k7yhild, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpeqd_k0k0ylobcst, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpeqd_k1k7yhibcst, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_4))
OPCODE(vpcmpeqd_k0k0zlozlo, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpeqd_k1k7zhizhi, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpeqd_k0k0zlold, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpeqd_k1k7zhild, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpeqd_k0k0zlobcst, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vpcmpeqd_k1k7zhibcst, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_4))
OPCODE(vpcmpeqq_k0k0xloxlo, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpeqq_k1k7xhixhi, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpeqq_k0k0xlold, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpeqq_k1k7xhild, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpeqq_k0k0xlobcst, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpeqq_k1k7xhibcst, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_8))
OPCODE(vpcmpeqq_k0k0yloylo, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpeqq_k1k7yhiyhi, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpeqq_k0k0ylold, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpeqq_k1k7yhild, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpeqq_k0k0ylobcst, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpeqq_k1k7yhibcst, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_8))
OPCODE(vpcmpeqq_k0k0zlozlo, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpeqq_k1k7zhizhi, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpeqq_k0k0zlold, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpeqq_k1k7zhild, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpeqq_k0k0zlobcst, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vpcmpeqq_k1k7zhibcst, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_8))
OPCODE(vpminsb_xlok0xloxlo, vpminsb, vpminsb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminsb_xlok0xlold, vpminsb, vpminsb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsb_xhik7xhixhi, vpminsb, vpminsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminsb_xhik7xhild, vpminsb, vpminsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminsb_ylok0yloylo, vpminsb, vpminsb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminsb_ylok0ylold, vpminsb, vpminsb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsb_yhik7yhiyhi, vpminsb, vpminsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminsb_yhik7yhild, vpminsb, vpminsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminsb_zlok0zlozlo, vpminsb, vpminsb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminsb_zlok0zlold, vpminsb, vpminsb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminsb_zhik7zhizhi, vpminsb, vpminsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminsb_zhik7zhild, vpminsb, vpminsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminsw_xlok0xloxlo, vpminsw, vpminsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminsw_xlok0xlold, vpminsw, vpminsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsw_xhik7xhixhi, vpminsw, vpminsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminsw_xhik7xhild, vpminsw, vpminsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminsw_ylok0yloylo, vpminsw, vpminsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminsw_ylok0ylold, vpminsw, vpminsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsw_yhik7yhiyhi, vpminsw, vpminsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminsw_yhik7yhild, vpminsw, vpminsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminsw_zlok0zlozlo, vpminsw, vpminsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminsw_zlok0zlold, vpminsw, vpminsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminsw_zhik7zhizhi, vpminsw, vpminsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminsw_zhik7zhild, vpminsw, vpminsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminsd_xlok0xloxlo, vpminsd, vpminsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminsd_xlok0xlold, vpminsd, vpminsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsd_xlok0xlobcst, vpminsd, vpminsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpminsd_xhik7xhixhi, vpminsd, vpminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminsd_xhik7xhild, vpminsd, vpminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminsd_xhik7xhibcst, vpminsd, vpminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpminsd_ylok0yloylo, vpminsd, vpminsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminsd_ylok0ylold, vpminsd, vpminsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsd_ylok0ylobcst, vpminsd, vpminsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpminsd_yhik7yhiyhi, vpminsd, vpminsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminsd_yhik7yhild, vpminsd, vpminsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminsd_yhik7yhibcst, vpminsd, vpminsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpminsd_zlok0zlozlo, vpminsd, vpminsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminsd_zlok0zlold, vpminsd, vpminsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminsd_zlok0zlobcst, vpminsd, vpminsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpminsd_zhik7zhizhi, vpminsd, vpminsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminsd_zhik7zhild, vpminsd, vpminsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminsd_zhik7zhibcst, vpminsd, vpminsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpminsq_xlok0xloxlo, vpminsq, vpminsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminsq_xlok0xlold, vpminsq, vpminsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsq_xlok0xlobcst, vpminsq, vpminsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpminsq_xhik7xhixhi, vpminsq, vpminsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminsq_xhik7xhild, vpminsq, vpminsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminsq_xhik7xhibcst, vpminsq, vpminsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpminsq_ylok0yloylo, vpminsq, vpminsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminsq_ylok0ylold, vpminsq, vpminsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsq_ylok0ylobcst, vpminsq, vpminsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpminsq_yhik7yhiyhi, vpminsq, vpminsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminsq_yhik7yhild, vpminsq, vpminsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminsq_yhik7yhibcst, vpminsq, vpminsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpminsq_zlok0zlozlo, vpminsq, vpminsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminsq_zlok0zlold, vpminsq, vpminsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminsq_zlok0zlobcst, vpminsq, vpminsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpminsq_zhik7zhizhi, vpminsq, vpminsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminsq_zhik7zhild, vpminsq, vpminsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminsq_zhik7zhibcst, vpminsq, vpminsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpmaxsb_xlok0xloxlo, vpmaxsb, vpmaxsb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxsb_xlok0xlold, vpmaxsb, vpmaxsb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsb_xhik7xhixhi, vpmaxsb, vpmaxsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxsb_xhik7xhild, vpmaxsb, vpmaxsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxsb_ylok0yloylo, vpmaxsb, vpmaxsb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxsb_ylok0ylold, vpmaxsb, vpmaxsb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsb_yhik7yhiyhi, vpmaxsb, vpmaxsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxsb_yhik7yhild, vpmaxsb, vpmaxsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxsb_zlok0zlozlo, vpmaxsb, vpmaxsb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxsb_zlok0zlold, vpmaxsb, vpmaxsb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxsb_zhik7zhizhi, vpmaxsb, vpmaxsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxsb_zhik7zhild, vpmaxsb, vpmaxsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxsw_xlok0xloxlo, vpmaxsw, vpmaxsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxsw_xlok0xlold, vpmaxsw, vpmaxsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsw_xhik7xhixhi, vpmaxsw, vpmaxsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxsw_xhik7xhild, vpmaxsw, vpmaxsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxsw_ylok0yloylo, vpmaxsw, vpmaxsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxsw_ylok0ylold, vpmaxsw, vpmaxsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsw_yhik7yhiyhi, vpmaxsw, vpmaxsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxsw_yhik7yhild, vpmaxsw, vpmaxsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxsw_zlok0zlozlo, vpmaxsw, vpmaxsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxsw_zlok0zlold, vpmaxsw, vpmaxsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxsw_zhik7zhizhi, vpmaxsw, vpmaxsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxsw_zhik7zhild, vpmaxsw, vpmaxsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxsd_xlok0xloxlo, vpmaxsd, vpmaxsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxsd_xlok0xlold, vpmaxsd, vpmaxsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsd_xlok0xlobcst, vpmaxsd, vpmaxsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpmaxsd_xhik7xhixhi, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxsd_xhik7xhild, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxsd_xhik7xhibcst, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpmaxsd_ylok0yloylo, vpmaxsd, vpmaxsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxsd_ylok0ylold, vpmaxsd, vpmaxsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsd_ylok0ylobcst, vpmaxsd, vpmaxsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpmaxsd_yhik7yhiyhi, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxsd_yhik7yhild, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxsd_yhik7yhibcst, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpmaxsd_zlok0zlozlo, vpmaxsd, vpmaxsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxsd_zlok0zlold, vpmaxsd, vpmaxsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxsd_zlok0zlobcst, vpmaxsd, vpmaxsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpmaxsd_zhik7zhizhi, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxsd_zhik7zhild, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxsd_zhik7zhibcst, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpmaxsq_xlok0xloxlo, vpmaxsq, vpmaxsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxsq_xlok0xlold, vpmaxsq, vpmaxsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsq_xlok0xlobcst, vpmaxsq, vpmaxsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmaxsq_xhik7xhixhi, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxsq_xhik7xhild, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxsq_xhik7xhibcst, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmaxsq_ylok0yloylo, vpmaxsq, vpmaxsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxsq_ylok0ylold, vpmaxsq, vpmaxsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsq_ylok0ylobcst, vpmaxsq, vpmaxsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmaxsq_yhik7yhiyhi, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxsq_yhik7yhild, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxsq_yhik7yhibcst, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmaxsq_zlok0zlozlo, vpmaxsq, vpmaxsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxsq_zlok0zlold, vpmaxsq, vpmaxsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxsq_zlok0zlobcst, vpmaxsq, vpmaxsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmaxsq_zhik7zhizhi, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxsq_zhik7zhild, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxsq_zhik7zhibcst, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpminub_xlok0xloxlo, vpminub, vpminub_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminub_xlok0xlold, vpminub, vpminub_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminub_xhik7xhixhi, vpminub, vpminub_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminub_xhik7xhild, vpminub, vpminub_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminub_ylok0yloylo, vpminub, vpminub_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminub_ylok0ylold, vpminub, vpminub_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminub_yhik7yhiyhi, vpminub, vpminub_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminub_yhik7yhild, vpminub, vpminub_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminub_zlok0zlozlo, vpminub, vpminub_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminub_zlok0zlold, vpminub, vpminub_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminub_zhik7zhizhi, vpminub, vpminub_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminub_zhik7zhild, vpminub, vpminub_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminuw_xlok0xloxlo, vpminuw, vpminuw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminuw_xlok0xlold, vpminuw, vpminuw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminuw_xhik7xhixhi, vpminuw, vpminuw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminuw_xhik7xhild, vpminuw, vpminuw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminuw_ylok0yloylo, vpminuw, vpminuw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminuw_ylok0ylold, vpminuw, vpminuw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminuw_yhik7yhiyhi, vpminuw, vpminuw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminuw_yhik7yhild, vpminuw, vpminuw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminuw_zlok0zlozlo, vpminuw, vpminuw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminuw_zlok0zlold, vpminuw, vpminuw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminuw_zhik7zhizhi, vpminuw, vpminuw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminuw_zhik7zhild, vpminuw, vpminuw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminud_xlok0xloxlo, vpminud, vpminud_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminud_xlok0xlold, vpminud, vpminud_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminud_xlok0xlobcst, vpminud, vpminud_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpminud_xhik7xhixhi, vpminud, vpminud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminud_xhik7xhild, vpminud, vpminud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminud_xhik7xhibcst, vpminud, vpminud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpminud_ylok0yloylo, vpminud, vpminud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminud_ylok0ylold, vpminud, vpminud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminud_ylok0ylobcst, vpminud, vpminud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpminud_yhik7yhiyhi, vpminud, vpminud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminud_yhik7yhild, vpminud, vpminud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminud_yhik7yhibcst, vpminud, vpminud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpminud_zlok0zlozlo, vpminud, vpminud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminud_zlok0zlold, vpminud, vpminud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminud_zlok0zlobcst, vpminud, vpminud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpminud_zhik7zhizhi, vpminud, vpminud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminud_zhik7zhild, vpminud, vpminud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminud_zhik7zhibcst, vpminud, vpminud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpminuq_xlok0xloxlo, vpminuq, vpminuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminuq_xlok0xlold, vpminuq, vpminuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminuq_xlok0xlobcst, vpminuq, vpminuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpminuq_xhik7xhixhi, vpminuq, vpminuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminuq_xhik7xhild, vpminuq, vpminuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminuq_xhik7xhibcst, vpminuq, vpminuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpminuq_ylok0yloylo, vpminuq, vpminuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminuq_ylok0ylold, vpminuq, vpminuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminuq_ylok0ylobcst, vpminuq, vpminuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpminuq_yhik7yhiyhi, vpminuq, vpminuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminuq_yhik7yhild, vpminuq, vpminuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminuq_yhik7yhibcst, vpminuq, vpminuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpminuq_zlok0zlozlo, vpminuq, vpminuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminuq_zlok0zlold, vpminuq, vpminuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminuq_zlok0zlobcst, vpminuq, vpminuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpminuq_zhik7zhizhi, vpminuq, vpminuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminuq_zhik7zhild, vpminuq, vpminuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminuq_zhik7zhibcst, vpminuq, vpminuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpmaxub_xlok0xloxlo, vpmaxub, vpmaxub_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxub_xlok0xlold, vpmaxub, vpmaxub_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxub_xhik7xhixhi, vpmaxub, vpmaxub_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxub_xhik7xhild, vpmaxub, vpmaxub_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxub_ylok0yloylo, vpmaxub, vpmaxub_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxub_ylok0ylold, vpmaxub, vpmaxub_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxub_yhik7yhiyhi, vpmaxub, vpmaxub_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxub_yhik7yhild, vpmaxub, vpmaxub_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxub_zlok0zlozlo, vpmaxub, vpmaxub_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxub_zlok0zlold, vpmaxub, vpmaxub_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxub_zhik7zhizhi, vpmaxub, vpmaxub_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxub_zhik7zhild, vpmaxub, vpmaxub_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxuw_xlok0xloxlo, vpmaxuw, vpmaxuw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxuw_xlok0xlold, vpmaxuw, vpmaxuw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxuw_xhik7xhixhi, vpmaxuw, vpmaxuw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxuw_xhik7xhild, vpmaxuw, vpmaxuw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxuw_ylok0yloylo, vpmaxuw, vpmaxuw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxuw_ylok0ylold, vpmaxuw, vpmaxuw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxuw_yhik7yhiyhi, vpmaxuw, vpmaxuw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxuw_yhik7yhild, vpmaxuw, vpmaxuw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxuw_zlok0zlozlo, vpmaxuw, vpmaxuw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxuw_zlok0zlold, vpmaxuw, vpmaxuw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxuw_zhik7zhizhi, vpmaxuw, vpmaxuw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxuw_zhik7zhild, vpmaxuw, vpmaxuw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxud_xlok0xloxlo, vpmaxud, vpmaxud_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxud_xlok0xlold, vpmaxud, vpmaxud_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxud_xlok0xlobcst, vpmaxud, vpmaxud_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpmaxud_xhik7xhixhi, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxud_xhik7xhild, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxud_xhik7xhibcst, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpmaxud_ylok0yloylo, vpmaxud, vpmaxud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxud_ylok0ylold, vpmaxud, vpmaxud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxud_ylok0ylobcst, vpmaxud, vpmaxud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpmaxud_yhik7yhiyhi, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxud_yhik7yhild, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxud_yhik7yhibcst, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpmaxud_zlok0zlozlo, vpmaxud, vpmaxud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxud_zlok0zlold, vpmaxud, vpmaxud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxud_zlok0zlobcst, vpmaxud, vpmaxud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpmaxud_zhik7zhizhi, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxud_zhik7zhild, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxud_zhik7zhibcst, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpmaxuq_xlok0xloxlo, vpmaxuq, vpmaxuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxuq_xlok0xlold, vpmaxuq, vpmaxuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxuq_xlok0xlobcst, vpmaxuq, vpmaxuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmaxuq_xhik7xhixhi, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxuq_xhik7xhild, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxuq_xhik7xhibcst, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmaxuq_ylok0yloylo, vpmaxuq, vpmaxuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxuq_ylok0ylold, vpmaxuq, vpmaxuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxuq_ylok0ylobcst, vpmaxuq, vpmaxuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmaxuq_yhik7yhiyhi, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxuq_yhik7yhild, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxuq_yhik7yhibcst, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmaxuq_zlok0zlozlo, vpmaxuq, vpmaxuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxuq_zlok0zlold, vpmaxuq, vpmaxuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxuq_zlok0zlobcst, vpmaxuq, vpmaxuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmaxuq_zhik7zhizhi, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxuq_zhik7zhild, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxuq_zhik7zhibcst, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vprolvd_xlok0xloxlo, vprolvd, vprolvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprolvd_xlok0xlold, vprolvd, vprolvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprolvd_xlok0xlobcst, vprolvd, vprolvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vprolvd_xhik7xhixhi, vprolvd, vprolvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprolvd_xhik7xhild, vprolvd, vprolvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprolvd_xhik7xhibcst, vprolvd, vprolvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vprolvd_ylok0yloylo, vprolvd, vprolvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprolvd_ylok0ylold, vprolvd, vprolvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprolvd_ylok0ylobcst, vprolvd, vprolvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vprolvd_yhik7yhiyhi, vprolvd, vprolvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprolvd_yhik7yhild, vprolvd, vprolvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprolvd_yhik7yhibcst, vprolvd, vprolvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vprolvd_zlok0zlozlo, vprolvd, vprolvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprolvd_zlok0zlold, vprolvd, vprolvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprolvd_zlok0zlobcst, vprolvd, vprolvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vprolvd_zhik7zhizhi, vprolvd, vprolvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprolvd_zhik7zhild, vprolvd, vprolvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprolvd_zhik7zhibcst, vprolvd, vprolvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vprold_xlok0xloxlo, vprold, vprold_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprold_xlok0xlold, vprold, vprold_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprold_xlok0xlobcst, vprold, vprold_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprold_xhik7xhixhi, vprold, vprold_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprold_xhik7xhild, vprold, vprold_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprold_xhik7xhibcst, vprold, vprold_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprold_ylok0yloylo, vprold, vprold_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprold_ylok0ylold, vprold, vprold_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprold_ylok0ylobcst, vprold, vprold_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprold_yhik7yhiyhi, vprold, vprold_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprold_yhik7yhild, vprold, vprold_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprold_yhik7yhibcst, vprold, vprold_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprold_zlok0zlozlo, vprold, vprold_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprold_zlok0zlold, vprold, vprold_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprold_zlok0zlobcst, vprold, vprold_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprold_zhik7zhizhi, vprold, vprold_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprold_zhik7zhild, vprold, vprold_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprold_zhik7zhibcst, vprold, vprold_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprolvq_xlok0xloxlo, vprolvq, vprolvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprolvq_xlok0xlold, vprolvq, vprolvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprolvq_xlok0xlobcst, vprolvq, vprolvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vprolvq_xhik7xhixhi, vprolvq, vprolvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprolvq_xhik7xhild, vprolvq, vprolvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprolvq_xhik7xhibcst, vprolvq, vprolvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vprolvq_ylok0yloylo, vprolvq, vprolvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprolvq_ylok0ylold, vprolvq, vprolvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprolvq_ylok0ylobcst, vprolvq, vprolvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vprolvq_yhik7yhiyhi, vprolvq, vprolvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprolvq_yhik7yhild, vprolvq, vprolvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprolvq_yhik7yhibcst, vprolvq, vprolvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vprolvq_zlok0zlozlo, vprolvq, vprolvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprolvq_zlok0zlold, vprolvq, vprolvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprolvq_zlok0zlobcst, vprolvq, vprolvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vprolvq_zhik7zhizhi, vprolvq, vprolvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprolvq_zhik7zhild, vprolvq, vprolvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprolvq_zhik7zhibcst, vprolvq, vprolvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vprolq_xlok0xloxlo, vprolq, vprolq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprolq_xlok0xlold, vprolq, vprolq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprolq_xlok0xlobcst, vprolq, vprolq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprolq_xhik7xhixhi, vprolq, vprolq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprolq_xhik7xhild, vprolq, vprolq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprolq_xhik7xhibcst, vprolq, vprolq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprolq_ylok0yloylo, vprolq, vprolq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprolq_ylok0ylold, vprolq, vprolq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprolq_ylok0ylobcst, vprolq, vprolq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprolq_yhik7yhiyhi, vprolq, vprolq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprolq_yhik7yhild, vprolq, vprolq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprolq_yhik7yhibcst, vprolq, vprolq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprolq_zlok0zlozlo, vprolq, vprolq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprolq_zlok0zlold, vprolq, vprolq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprolq_zlok0zlobcst, vprolq, vprolq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprolq_zhik7zhizhi, vprolq, vprolq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprolq_zhik7zhild, vprolq, vprolq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprolq_zhik7zhibcst, vprolq, vprolq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprorvd_xlok0xloxlo, vprorvd, vprorvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprorvd_xlok0xlold, vprorvd, vprorvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprorvd_xlok0xlobcst, vprorvd, vprorvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vprorvd_xhik7xhixhi, vprorvd, vprorvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprorvd_xhik7xhild, vprorvd, vprorvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprorvd_xhik7xhibcst, vprorvd, vprorvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vprorvd_ylok0yloylo, vprorvd, vprorvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprorvd_ylok0ylold, vprorvd, vprorvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprorvd_ylok0ylobcst, vprorvd, vprorvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vprorvd_yhik7yhiyhi, vprorvd, vprorvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprorvd_yhik7yhild, vprorvd, vprorvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprorvd_yhik7yhibcst, vprorvd, vprorvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vprorvd_zlok0zlozlo, vprorvd, vprorvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprorvd_zlok0zlold, vprorvd, vprorvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprorvd_zlok0zlobcst, vprorvd, vprorvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vprorvd_zhik7zhizhi, vprorvd, vprorvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprorvd_zhik7zhild, vprorvd, vprorvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprorvd_zhik7zhibcst, vprorvd, vprorvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vprord_xlok0xloxlo, vprord, vprord_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprord_xlok0xlold, vprord, vprord_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprord_xlok0xlobcst, vprord, vprord_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprord_xhik7xhixhi, vprord, vprord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprord_xhik7xhild, vprord, vprord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprord_xhik7xhibcst, vprord, vprord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprord_ylok0yloylo, vprord, vprord_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprord_ylok0ylold, vprord, vprord_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprord_ylok0ylobcst, vprord, vprord_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprord_yhik7yhiyhi, vprord, vprord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprord_yhik7yhild, vprord, vprord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprord_yhik7yhibcst, vprord, vprord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprord_zlok0zlozlo, vprord, vprord_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprord_zlok0zlold, vprord, vprord_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprord_zlok0zlobcst, vprord, vprord_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprord_zhik7zhizhi, vprord, vprord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprord_zhik7zhild, vprord, vprord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprord_zhik7zhibcst, vprord, vprord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vprorvq_xlok0xloxlo, vprorvq, vprorvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprorvq_xlok0xlold, vprorvq, vprorvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprorvq_xlok0xlobcst, vprorvq, vprorvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vprorvq_xhik7xhixhi, vprorvq, vprorvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprorvq_xhik7xhild, vprorvq, vprorvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprorvq_xhik7xhibcst, vprorvq, vprorvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vprorvq_ylok0yloylo, vprorvq, vprorvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprorvq_ylok0ylold, vprorvq, vprorvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprorvq_ylok0ylobcst, vprorvq, vprorvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vprorvq_yhik7yhiyhi, vprorvq, vprorvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprorvq_yhik7yhild, vprorvq, vprorvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprorvq_yhik7yhibcst, vprorvq, vprorvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vprorvq_zlok0zlozlo, vprorvq, vprorvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprorvq_zlok0zlold, vprorvq, vprorvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprorvq_zlok0zlobcst, vprorvq, vprorvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vprorvq_zhik7zhizhi, vprorvq, vprorvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprorvq_zhik7zhild, vprorvq, vprorvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprorvq_zhik7zhibcst, vprorvq, vprorvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vprorq_xlok0xloxlo, vprorq, vprorq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprorq_xlok0xlold, vprorq, vprorq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprorq_xlok0xlobcst, vprorq, vprorq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprorq_xhik7xhixhi, vprorq, vprorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprorq_xhik7xhild, vprorq, vprorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprorq_xhik7xhibcst, vprorq, vprorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprorq_ylok0yloylo, vprorq, vprorq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprorq_ylok0ylold, vprorq, vprorq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprorq_ylok0ylobcst, vprorq, vprorq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprorq_yhik7yhiyhi, vprorq, vprorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprorq_yhik7yhild, vprorq, vprorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprorq_yhik7yhibcst, vprorq, vprorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprorq_zlok0zlozlo, vprorq, vprorq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprorq_zlok0zlold, vprorq, vprorq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprorq_zlok0zlobcst, vprorq, vprorq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vprorq_zhik7zhizhi, vprorq, vprorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprorq_zhik7zhild, vprorq, vprorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprorq_zhik7zhibcst, vprorq, vprorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
