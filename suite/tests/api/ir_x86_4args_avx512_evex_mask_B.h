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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 2 sources.
 * Part B: Split in half to avoid a VS2013 compiler bug i#3992.
 */
OPCODE(vpermilps_xlok0xloxlo, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermilps_xlok0xlold, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermilps_ylok0yloylo, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermilps_ylok0ylold, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermilps_zlok0zlozlo, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilps_zlok0zlold, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermilps_xhik7xhixhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermilps_xhik7xhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermilps_yhik7yhiyhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermilps_yhik7yhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermilps_zhik7zhizhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermilps_zhik7zhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermilps_xlok0xloimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_xlok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0yloimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0zloimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7xhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7yhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7zhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0xloxlo, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermilpd_xlok0xlold, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermilpd_ylok0yloylo, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermilpd_ylok0ylold, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermilpd_zlok0zlozlo, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilpd_zlok0zlold, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermilpd_xhik7xhixhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermilpd_xhik7xhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermilpd_yhik7yhiyhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermilpd_yhik7yhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermilpd_zhik7zhizhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermilpd_zhik7zhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermilpd_xlok0xloimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0yloimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0zloimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7xhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7yhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7zhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermd_ylok0yloylo, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermd_ylok0ylold, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermd_zlok0zlozlo, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermd_zlok0zlold, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermd_yhik7yhiyhi, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermd_yhik7yhild, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermd_zhik7zhizhi, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermd_zhik7zhild, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpermps_zlok0zlozlo, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermps_zlok0zlold, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermps_yhik7yhiyhi, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermps_yhik7yhild, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermps_zhik7zhizhi, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermps_zhik7zhild, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermq_ylok0yloylo, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermq_ylok0ylold, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermq_zlok0zlozlo, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermq_zlok0zlold, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermq_yhik7yhiyhi, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermq_yhik7yhild, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermq_zhik7zhizhi, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermq_zhik7zhild, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermq_ylok0yloimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       IMMARG(OPSZ_1))
OPCODE(vpermq_ylok0ldimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0zloimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0ldimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7yhiimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7ldimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7zhiimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7ldimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0yloimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7yhiimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0zloimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7zhiimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0yloylo, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermpd_ylok0ylold, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermpd_zlok0zlozlo, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermpd_zlok0zlold, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermpd_yhik7yhiyhi, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermpd_yhik7yhild, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermpd_zhik7zhizhi, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermpd_zhik7zhild, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2ps_xlok0xloxlo, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2ps_xlok0xlold, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2ps_ylok0yloylo, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2ps_ylok0ylold, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2ps_zlok0zlozlo, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2ps_zlok0zlold, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2ps_xhik7xhixhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2ps_xhik7xhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2ps_yhik7yhiyhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2ps_yhik7yhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2ps_zhik7zhizhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2ps_zhik7zhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2pd_xlok0xloxlo, vpermi2pd, vpermi2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2pd_xlok0xlold, vpermi2pd, vpermi2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2pd_ylok0yloylo, vpermi2pd, vpermi2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2pd_ylok0ylold, vpermi2pd, vpermi2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2pd_zlok0zlozlo, vpermi2pd, vpermi2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2pd_zlok0zlold, vpermi2pd, vpermi2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2pd_xhik7xhixhi, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2pd_xhik7xhild, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2pd_yhik7yhiyhi, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2pd_yhik7yhild, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2pd_zhik7zhizhi, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2pd_zhik7zhild, vpermi2pd, vpermi2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2d_xlok0xloxlo, vpermi2d, vpermi2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2d_xlok0xlold, vpermi2d, vpermi2d_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2d_ylok0yloylo, vpermi2d, vpermi2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2d_ylok0ylold, vpermi2d, vpermi2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2d_zlok0zlozlo, vpermi2d, vpermi2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2d_zlok0zlold, vpermi2d, vpermi2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2d_xhik7xhixhi, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2d_xhik7xhild, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2d_yhik7yhiyhi, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2d_yhik7yhild, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2d_zhik7zhizhi, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2d_zhik7zhild, vpermi2d, vpermi2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2q_xlok0xloxlo, vpermi2q, vpermi2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2q_xlok0xlold, vpermi2q, vpermi2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2q_ylok0yloylo, vpermi2q, vpermi2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2q_ylok0ylold, vpermi2q, vpermi2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2q_zlok0zlozlo, vpermi2q, vpermi2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2q_zlok0zlold, vpermi2q, vpermi2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2q_xhik7xhixhi, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2q_xhik7xhild, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2q_yhik7yhiyhi, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2q_yhik7yhild, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2q_zhik7zhizhi, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2q_zhik7zhild, vpermi2q, vpermi2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpermt2d_ylok0yloylo, vpermt2d, vpermt2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2d_ylok0ylold, vpermt2d, vpermt2d_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2d_zlok0zlozlo, vpermt2d, vpermt2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2d_zlok0zlold, vpermt2d, vpermt2d_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2d_xhik7xhixhi, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2d_xhik7xhild, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2d_yhik7yhiyhi, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2d_yhik7yhild, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2d_zhik7zhizhi, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2d_zhik7zhild, vpermt2d, vpermt2d_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2q_xlok0xloxlo, vpermt2q, vpermt2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2q_xlok0xlold, vpermt2q, vpermt2q_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2q_ylok0yloylo, vpermt2q, vpermt2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2q_ylok0ylold, vpermt2q, vpermt2q_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2q_zlok0zlozlo, vpermt2q, vpermt2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2q_zlok0zlold, vpermt2q, vpermt2q_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2q_xhik7xhixhi, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2q_xhik7xhild, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2q_yhik7yhiyhi, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2q_yhik7yhild, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2q_zhik7zhizhi, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2q_zhik7zhild, vpermt2q, vpermt2q_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpermt2ps_ylok0yloylo, vpermt2ps, vpermt2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2ps_ylok0ylold, vpermt2ps, vpermt2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2ps_zlok0zlozlo, vpermt2ps, vpermt2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2ps_zlok0zlold, vpermt2ps, vpermt2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2ps_xhik7xhixhi, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2ps_xhik7xhild, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2ps_yhik7yhiyhi, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2ps_yhik7yhild, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2ps_zhik7zhizhi, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2ps_zhik7zhild, vpermt2ps, vpermt2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermt2pd_xlok0xloxlo, vpermt2pd, vpermt2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermt2pd_xlok0xlold, vpermt2pd, vpermt2pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermt2pd_ylok0yloylo, vpermt2pd, vpermt2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermt2pd_ylok0ylold, vpermt2pd, vpermt2pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermt2pd_zlok0zlozlo, vpermt2pd, vpermt2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermt2pd_zlok0zlold, vpermt2pd, vpermt2pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermt2pd_xhik7xhixhi, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermt2pd_xhik7xhild, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermt2pd_yhik7yhiyhi, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermt2pd_yhik7yhild, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermt2pd_zhik7zhizhi, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermt2pd_zhik7zhild, vpermt2pd, vpermt2pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpunpckldq_xhik7xhixhi, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckldq_xhik7xhild, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckldq_ylok0yloylo, vpunpckldq, vpunpckldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckldq_ylok0ylold, vpunpckldq, vpunpckldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckldq_yhik7yhiyhi, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckldq_yhik7yhild, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckldq_zlok0zlozlo, vpunpckldq, vpunpckldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckldq_zlok0zlold, vpunpckldq, vpunpckldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckldq_zhik7zhizhi, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckldq_zhik7zhild, vpunpckldq, vpunpckldq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpcklqdq_xlok0xloxlo, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpcklqdq_xlok0xlold, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpcklqdq_xhik7xhixhi, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpcklqdq_xhik7xhild, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpcklqdq_ylok0yloylo, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpcklqdq_ylok0ylold, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpcklqdq_yhik7yhiyhi, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpcklqdq_yhik7yhild, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpcklqdq_zlok0zlozlo, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpcklqdq_zlok0zlold, vpunpcklqdq, vpunpcklqdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpcklqdq_zhik7zhizhi, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpcklqdq_zhik7zhild, vpunpcklqdq, vpunpcklqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpunpckhdq_xhik7xhixhi, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckhdq_xhik7xhild, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckhdq_ylok0yloylo, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckhdq_ylok0ylold, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckhdq_yhik7yhiyhi, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckhdq_yhik7yhild, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckhdq_zlok0zlozlo, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckhdq_zlok0zlold, vpunpckhdq, vpunpckhdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckhdq_zhik7zhizhi, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckhdq_zhik7zhild, vpunpckhdq, vpunpckhdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpunpckhqdq_xlok0xloxlo, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpunpckhqdq_xlok0xlold, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhqdq_xhik7xhixhi, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpunpckhqdq_xhik7xhild, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpunpckhqdq_ylok0yloylo, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpunpckhqdq_ylok0ylold, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpunpckhqdq_yhik7yhiyhi, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpunpckhqdq_yhik7yhild, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpunpckhqdq_zlok0zlozlo, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpunpckhqdq_zlok0zlold, vpunpckhqdq, vpunpckhqdq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpunpckhqdq_zhik7zhizhi, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpunpckhqdq_zhik7zhild, vpunpckhqdq, vpunpckhqdq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpackssdw_xhik7xhixhi, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpackssdw_xhik7xhild, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpackssdw_ylok0yloylo, vpackssdw, vpackssdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpackssdw_ylok0ylold, vpackssdw, vpackssdw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpackssdw_yhik7yhiyhi, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpackssdw_yhik7yhild, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpackssdw_zlok0zlozlo, vpackssdw, vpackssdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpackssdw_zlok0zlold, vpackssdw, vpackssdw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpackssdw_zhik7zhizhi, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpackssdw_zhik7zhild, vpackssdw, vpackssdw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
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
OPCODE(vpcmpgtd_k0k0yloylo, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpgtd_k1k7yhiyhi, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpgtd_k0k0ylold, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpgtd_k1k7yhild, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpgtd_k0k0zlozlo, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpgtd_k1k7zhizhi, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpgtd_k0k0zlold, vpcmpgtd, vpcmpgtd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpgtd_k1k7zhild, vpcmpgtd, vpcmpgtd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpgtq_k0k0xloxlo, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpgtq_k1k7xhixhi, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpgtq_k0k0xlold, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpgtq_k1k7xhild, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpgtq_k0k0yloylo, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpgtq_k1k7yhiyhi, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpgtq_k0k0ylold, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpgtq_k1k7yhild, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpgtq_k0k0zlozlo, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpgtq_k1k7zhizhi, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpgtq_k0k0zlold, vpcmpgtq, vpcmpgtq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpgtq_k1k7zhild, vpcmpgtq, vpcmpgtq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
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
OPCODE(vpcmpeqd_k0k0yloylo, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpeqd_k1k7yhiyhi, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpeqd_k0k0ylold, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpeqd_k1k7yhild, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpeqd_k0k0zlozlo, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpeqd_k1k7zhizhi, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpeqd_k0k0zlold, vpcmpeqd, vpcmpeqd_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpeqd_k1k7zhild, vpcmpeqd, vpcmpeqd_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
OPCODE(vpcmpeqq_k0k0xloxlo, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vpcmpeqq_k1k7xhixhi, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vpcmpeqq_k0k0xlold, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpcmpeqq_k1k7xhild, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(XMM16), MEMARG(OPSZ_16))
OPCODE(vpcmpeqq_k0k0yloylo, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vpcmpeqq_k1k7yhiyhi, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vpcmpeqq_k0k0ylold, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpcmpeqq_k1k7yhild, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(YMM16), MEMARG(OPSZ_32))
OPCODE(vpcmpeqq_k0k0zlozlo, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vpcmpeqq_k1k7zhizhi, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vpcmpeqq_k0k0zlold, vpcmpeqq, vpcmpeqq_mask, 0, REGARG(K0), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vpcmpeqq_k1k7zhild, vpcmpeqq, vpcmpeqq_mask, X64_ONLY, REGARG(K1), REGARG(K7),
       REGARG(ZMM16), MEMARG(OPSZ_64))
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
OPCODE(vpminsd_xhik7xhixhi, vpminsd, vpminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminsd_xhik7xhild, vpminsd, vpminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminsd_ylok0yloylo, vpminsd, vpminsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminsd_ylok0ylold, vpminsd, vpminsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsd_yhik7yhiyhi, vpminsd, vpminsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminsd_yhik7yhild, vpminsd, vpminsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminsd_zlok0zlozlo, vpminsd, vpminsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminsd_zlok0zlold, vpminsd, vpminsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminsd_zhik7zhizhi, vpminsd, vpminsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminsd_zhik7zhild, vpminsd, vpminsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminsq_xlok0xloxlo, vpminsq, vpminsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminsq_xlok0xlold, vpminsq, vpminsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsq_xhik7xhixhi, vpminsq, vpminsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminsq_xhik7xhild, vpminsq, vpminsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminsq_ylok0yloylo, vpminsq, vpminsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminsq_ylok0ylold, vpminsq, vpminsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsq_yhik7yhiyhi, vpminsq, vpminsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminsq_yhik7yhild, vpminsq, vpminsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminsq_zlok0zlozlo, vpminsq, vpminsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminsq_zlok0zlold, vpminsq, vpminsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminsq_zhik7zhizhi, vpminsq, vpminsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminsq_zhik7zhild, vpminsq, vpminsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
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
OPCODE(vpmaxsd_xhik7xhixhi, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxsd_xhik7xhild, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxsd_ylok0yloylo, vpmaxsd, vpmaxsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxsd_ylok0ylold, vpmaxsd, vpmaxsd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsd_yhik7yhiyhi, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxsd_yhik7yhild, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxsd_zlok0zlozlo, vpmaxsd, vpmaxsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxsd_zlok0zlold, vpmaxsd, vpmaxsd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxsd_zhik7zhizhi, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxsd_zhik7zhild, vpmaxsd, vpmaxsd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxsq_xlok0xloxlo, vpmaxsq, vpmaxsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxsq_xlok0xlold, vpmaxsq, vpmaxsq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsq_xhik7xhixhi, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxsq_xhik7xhild, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxsq_ylok0yloylo, vpmaxsq, vpmaxsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxsq_ylok0ylold, vpmaxsq, vpmaxsq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsq_yhik7yhiyhi, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxsq_yhik7yhild, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxsq_zlok0zlozlo, vpmaxsq, vpmaxsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxsq_zlok0zlold, vpmaxsq, vpmaxsq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxsq_zhik7zhizhi, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxsq_zhik7zhild, vpmaxsq, vpmaxsq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
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
OPCODE(vpminud_xhik7xhixhi, vpminud, vpminud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminud_xhik7xhild, vpminud, vpminud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminud_ylok0yloylo, vpminud, vpminud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminud_ylok0ylold, vpminud, vpminud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminud_yhik7yhiyhi, vpminud, vpminud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminud_yhik7yhild, vpminud, vpminud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminud_zlok0zlozlo, vpminud, vpminud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminud_zlok0zlold, vpminud, vpminud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminud_zhik7zhizhi, vpminud, vpminud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminud_zhik7zhild, vpminud, vpminud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpminuq_xlok0xloxlo, vpminuq, vpminuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpminuq_xlok0xlold, vpminuq, vpminuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminuq_xhik7xhixhi, vpminuq, vpminuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpminuq_xhik7xhild, vpminuq, vpminuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpminuq_ylok0yloylo, vpminuq, vpminuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpminuq_ylok0ylold, vpminuq, vpminuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminuq_yhik7yhiyhi, vpminuq, vpminuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpminuq_yhik7yhild, vpminuq, vpminuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpminuq_zlok0zlozlo, vpminuq, vpminuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpminuq_zlok0zlold, vpminuq, vpminuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpminuq_zhik7zhizhi, vpminuq, vpminuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpminuq_zhik7zhild, vpminuq, vpminuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
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
OPCODE(vpmaxud_xhik7xhixhi, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxud_xhik7xhild, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxud_ylok0yloylo, vpmaxud, vpmaxud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxud_ylok0ylold, vpmaxud, vpmaxud_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxud_yhik7yhiyhi, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxud_yhik7yhild, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxud_zlok0zlozlo, vpmaxud, vpmaxud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxud_zlok0zlold, vpmaxud, vpmaxud_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxud_zhik7zhizhi, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxud_zhik7zhild, vpmaxud, vpmaxud_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaxuq_xlok0xloxlo, vpmaxuq, vpmaxuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaxuq_xlok0xlold, vpmaxuq, vpmaxuq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxuq_xhik7xhixhi, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaxuq_xhik7xhild, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaxuq_ylok0yloylo, vpmaxuq, vpmaxuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaxuq_ylok0ylold, vpmaxuq, vpmaxuq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxuq_yhik7yhiyhi, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaxuq_yhik7yhild, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaxuq_zlok0zlozlo, vpmaxuq, vpmaxuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaxuq_zlok0zlold, vpmaxuq, vpmaxuq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmaxuq_zhik7zhizhi, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaxuq_zhik7zhild, vpmaxuq, vpmaxuq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprolvd_xlok0xloxlo, vprolvd, vprolvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprolvd_xlok0xlold, vprolvd, vprolvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprolvd_xhik7xhixhi, vprolvd, vprolvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprolvd_xhik7xhild, vprolvd, vprolvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprolvd_ylok0yloylo, vprolvd, vprolvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprolvd_ylok0ylold, vprolvd, vprolvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprolvd_yhik7yhiyhi, vprolvd, vprolvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprolvd_yhik7yhild, vprolvd, vprolvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprolvd_zlok0zlozlo, vprolvd, vprolvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprolvd_zlok0zlold, vprolvd, vprolvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprolvd_zhik7zhizhi, vprolvd, vprolvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprolvd_zhik7zhild, vprolvd, vprolvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprold_xlok0xloxlo, vprold, vprold_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprold_xlok0xlold, vprold, vprold_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprold_xhik7xhixhi, vprold, vprold_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprold_xhik7xhild, vprold, vprold_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprold_ylok0yloylo, vprold, vprold_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprold_ylok0ylold, vprold, vprold_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprold_yhik7yhiyhi, vprold, vprold_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprold_yhik7yhild, vprold, vprold_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprold_zlok0zlozlo, vprold, vprold_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprold_zlok0zlold, vprold, vprold_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprold_zhik7zhizhi, vprold, vprold_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprold_zhik7zhild, vprold, vprold_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprolvq_xlok0xloxlo, vprolvq, vprolvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprolvq_xlok0xlold, vprolvq, vprolvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprolvq_xhik7xhixhi, vprolvq, vprolvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprolvq_xhik7xhild, vprolvq, vprolvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprolvq_ylok0yloylo, vprolvq, vprolvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprolvq_ylok0ylold, vprolvq, vprolvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprolvq_yhik7yhiyhi, vprolvq, vprolvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprolvq_yhik7yhild, vprolvq, vprolvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprolvq_zlok0zlozlo, vprolvq, vprolvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprolvq_zlok0zlold, vprolvq, vprolvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprolvq_zhik7zhizhi, vprolvq, vprolvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprolvq_zhik7zhild, vprolvq, vprolvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprolq_xlok0xloxlo, vprolq, vprolq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprolq_xlok0xlold, vprolq, vprolq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprolq_xhik7xhixhi, vprolq, vprolq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprolq_xhik7xhild, vprolq, vprolq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprolq_ylok0yloylo, vprolq, vprolq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprolq_ylok0ylold, vprolq, vprolq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprolq_yhik7yhiyhi, vprolq, vprolq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprolq_yhik7yhild, vprolq, vprolq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprolq_zlok0zlozlo, vprolq, vprolq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprolq_zlok0zlold, vprolq, vprolq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprolq_zhik7zhizhi, vprolq, vprolq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprolq_zhik7zhild, vprolq, vprolq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprorvd_xlok0xloxlo, vprorvd, vprorvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprorvd_xlok0xlold, vprorvd, vprorvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprorvd_xhik7xhixhi, vprorvd, vprorvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprorvd_xhik7xhild, vprorvd, vprorvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprorvd_ylok0yloylo, vprorvd, vprorvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprorvd_ylok0ylold, vprorvd, vprorvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprorvd_yhik7yhiyhi, vprorvd, vprorvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprorvd_yhik7yhild, vprorvd, vprorvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprorvd_zlok0zlozlo, vprorvd, vprorvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprorvd_zlok0zlold, vprorvd, vprorvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprorvd_zhik7zhizhi, vprorvd, vprorvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprorvd_zhik7zhild, vprorvd, vprorvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprord_xlok0xloxlo, vprord, vprord_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprord_xlok0xlold, vprord, vprord_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprord_xhik7xhixhi, vprord, vprord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprord_xhik7xhild, vprord, vprord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprord_ylok0yloylo, vprord, vprord_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprord_ylok0ylold, vprord, vprord_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprord_yhik7yhiyhi, vprord, vprord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprord_yhik7yhild, vprord, vprord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprord_zlok0zlozlo, vprord, vprord_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprord_zlok0zlold, vprord, vprord_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprord_zhik7zhizhi, vprord, vprord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprord_zhik7zhild, vprord, vprord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprorvq_xlok0xloxlo, vprorvq, vprorvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vprorvq_xlok0xlold, vprorvq, vprorvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprorvq_xhik7xhixhi, vprorvq, vprorvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vprorvq_xhik7xhild, vprorvq, vprorvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vprorvq_ylok0yloylo, vprorvq, vprorvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vprorvq_ylok0ylold, vprorvq, vprorvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vprorvq_yhik7yhiyhi, vprorvq, vprorvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vprorvq_yhik7yhild, vprorvq, vprorvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vprorvq_zlok0zlozlo, vprorvq, vprorvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vprorvq_zlok0zlold, vprorvq, vprorvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vprorvq_zhik7zhizhi, vprorvq, vprorvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vprorvq_zhik7zhild, vprorvq, vprorvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vprorq_xlok0xloxlo, vprorq, vprorq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vprorq_xlok0xlold, vprorq, vprorq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprorq_xhik7xhixhi, vprorq, vprorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vprorq_xhik7xhild, vprorq, vprorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vprorq_ylok0yloylo, vprorq, vprorq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vprorq_ylok0ylold, vprorq, vprorq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprorq_yhik7yhiyhi, vprorq, vprorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vprorq_yhik7yhild, vprorq, vprorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vprorq_zlok0zlozlo, vprorq, vprorq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vprorq_zlok0zlold, vprorq, vprorq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vprorq_zhik7zhizhi, vprorq, vprorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vprorq_zhik7zhild, vprorq, vprorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraw_xlok0xloimm, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsraw_xlok0immld, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraw_xhik7xhiimm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsraw_xhik7immld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraw_ylok0yloimm, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsraw_ylok0immld, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraw_yhik7yhiimm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsraw_yhik7immld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraw_zlok0zloimm, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsraw_zlok0immld, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraw_zhik7zhiimm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsraw_zhik7immld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrad_xlok0xloimm, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrad_xlok0immld, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrad_xhik7xhiimm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrad_xhik7immld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrad_ylok0yloimm, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrad_ylok0immld, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrad_yhik7yhiimm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrad_yhik7immld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrad_zlok0zloimm, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrad_zlok0immld, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrad_zhik7zhiimm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrad_zhik7immld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraq_xlok0xloimm, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsraq_xlok0immld, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraq_xhik7xhiimm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsraq_xhik7immld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraq_ylok0yloimm, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsraq_ylok0immld, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraq_yhik7yhiimm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsraq_yhik7immld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraq_zlok0zloimm, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsraq_zlok0immld, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraq_zhik7zhiimm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsraq_zhik7immld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlw_xlok0xloimm, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrlw_xlok0immld, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlw_xhik7xhiimm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrlw_xhik7immld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlw_ylok0yloimm, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrlw_ylok0immld, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlw_yhik7yhiimm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrlw_yhik7immld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlw_zlok0zloimm, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrlw_zlok0immld, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlw_zhik7zhiimm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrlw_zhik7immld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrld_xlok0xloimm, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrld_xlok0immld, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrld_xhik7xhiimm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrld_xhik7immld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrld_ylok0yloimm, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrld_ylok0immld, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrld_yhik7yhiimm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrld_yhik7immld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrld_zlok0zloimm, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrld_zlok0immld, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrld_zhik7zhiimm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrld_zhik7immld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlq_xlok0xloimm, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrlq_xlok0immld, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlq_xhik7xhiimm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrlq_xhik7immld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlq_ylok0yloimm, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrlq_ylok0immld, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlq_yhik7yhiimm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrlq_yhik7immld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlq_zlok0zloimm, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrlq_zlok0immld, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlq_zhik7zhiimm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrlq_zhik7immld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraw_xlok0xloxmm, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsraw_xlok0xmmld, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraw_xhik7xhixmm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsraw_xhik7xmmld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsraw_ylok0yloxmm, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsraw_ylok0xmmld, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraw_yhik7yhixmm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsraw_yhik7xmmld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsraw_zlok0zloxmm, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsraw_zlok0xmmld, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraw_zhik7zhixmm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsraw_zhik7xmmld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrad_xlok0xloxmm, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrad_xlok0xmmld, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrad_xhik7xhixmm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrad_xhik7xmmld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrad_ylok0yloxmm, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrad_ylok0xmmld, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrad_yhik7yhixmm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrad_yhik7xmmld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrad_zlok0zloxmm, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrad_zlok0xmmld, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrad_zhik7zhixmm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrad_zhik7xmmld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsraq_xlok0xloxmm, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsraq_xlok0xmmld, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraq_xhik7xhixmm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsraq_xhik7xmmld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsraq_ylok0yloxmm, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsraq_ylok0xmmld, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraq_yhik7yhixmm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsraq_yhik7xmmld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsraq_zlok0zloxmm, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsraq_zlok0xmmld, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraq_zhik7zhixmm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsraq_zhik7xmmld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlw_xlok0xloxmm, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrlw_xlok0xmmld, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlw_xhik7xhixmm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlw_xhik7xmmld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlw_ylok0yloxmm, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrlw_ylok0xmmld, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlw_yhik7yhixmm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrlw_yhik7xmmld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlw_zlok0zloxmm, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrlw_zlok0xmmld, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlw_zhik7zhixmm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrlw_zhik7xmmld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrld_xlok0xloxmm, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrld_xlok0xmmld, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrld_xhik7xhixmm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrld_xhik7xmmld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrld_ylok0yloxmm, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrld_ylok0xmmld, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrld_yhik7yhixmm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrld_yhik7xmmld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrld_zlok0zloxmm, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrld_zlok0xmmld, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrld_zhik7zhixmm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrld_zhik7xmmld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlq_xlok0xloxmm, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrlq_xlok0xmmld, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlq_xhik7xhixmm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlq_xhik7xmmld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlq_ylok0yloxmm, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrlq_ylok0xmmld, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlq_yhik7yhixmm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrlq_yhik7xmmld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlq_zlok0zloxmm, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrlq_zlok0xmmld, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlq_zhik7zhixmm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrlq_zhik7xmmld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsravw_xlok0xloxlo, vpsravw, vpsravw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsravw_xlok0xlold, vpsravw, vpsravw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravw_xhik7xhixhi, vpsravw, vpsravw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsravw_xhik7xhild, vpsravw, vpsravw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravw_ylok0yloylo, vpsravw, vpsravw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsravw_ylok0ylold, vpsravw, vpsravw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravw_yhik7yhiyhi, vpsravw, vpsravw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsravw_yhik7yhild, vpsravw, vpsravw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravw_zlok0zlozlo, vpsravw, vpsravw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsravw_zlok0zlold, vpsravw, vpsravw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravw_zhik7zhizhi, vpsravw, vpsravw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsravw_zhik7zhild, vpsravw, vpsravw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravd_xlok0xloxlo, vpsravd, vpsravd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsravd_xlok0xlold, vpsravd, vpsravd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravd_xhik7xhixhi, vpsravd, vpsravd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsravd_xhik7xhild, vpsravd, vpsravd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravd_ylok0yloylo, vpsravd, vpsravd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsravd_ylok0ylold, vpsravd, vpsravd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravd_yhik7yhiyhi, vpsravd, vpsravd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsravd_yhik7yhild, vpsravd, vpsravd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravd_zlok0zlozlo, vpsravd, vpsravd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsravd_zlok0zlold, vpsravd, vpsravd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravd_zhik7zhizhi, vpsravd, vpsravd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsravd_zhik7zhild, vpsravd, vpsravd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravq_xlok0xloxlo, vpsravq, vpsravq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsravq_xlok0xlold, vpsravq, vpsravq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravq_xhik7xhixhi, vpsravq, vpsravq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsravq_xhik7xhild, vpsravq, vpsravq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravq_ylok0yloylo, vpsravq, vpsravq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsravq_ylok0ylold, vpsravq, vpsravq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravq_yhik7yhiyhi, vpsravq, vpsravq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsravq_yhik7yhild, vpsravq, vpsravq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravq_zlok0zlozlo, vpsravq, vpsravq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsravq_zlok0zlold, vpsravq, vpsravq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravq_zhik7zhizhi, vpsravq, vpsravq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsravq_zhik7zhild, vpsravq, vpsravq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvw_xlok0xloxlo, vpsrlvw, vpsrlvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsrlvw_xlok0xlold, vpsrlvw, vpsrlvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvw_xhik7xhixhi, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlvw_xhik7xhild, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvw_ylok0yloylo, vpsrlvw, vpsrlvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsrlvw_ylok0ylold, vpsrlvw, vpsrlvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvw_yhik7yhiyhi, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsrlvw_yhik7yhild, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvw_zlok0zlozlo, vpsrlvw, vpsrlvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsrlvw_zlok0zlold, vpsrlvw, vpsrlvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvw_zhik7zhizhi, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsrlvw_zhik7zhild, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvd_xlok0xloxlo, vpsrlvd, vpsrlvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsrlvd_xlok0xlold, vpsrlvd, vpsrlvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvd_xhik7xhixhi, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlvd_xhik7xhild, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvd_ylok0yloylo, vpsrlvd, vpsrlvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsrlvd_ylok0ylold, vpsrlvd, vpsrlvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvd_yhik7yhiyhi, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsrlvd_yhik7yhild, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvd_zlok0zlozlo, vpsrlvd, vpsrlvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsrlvd_zlok0zlold, vpsrlvd, vpsrlvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvd_zhik7zhizhi, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsrlvd_zhik7zhild, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvq_xlok0xloxlo, vpsrlvq, vpsrlvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsrlvq_xlok0xlold, vpsrlvq, vpsrlvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvq_xhik7xhixhi, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlvq_xhik7xhild, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvq_ylok0yloylo, vpsrlvq, vpsrlvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsrlvq_ylok0ylold, vpsrlvq, vpsrlvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvq_yhik7yhiyhi, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsrlvq_yhik7yhild, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvq_zlok0zlozlo, vpsrlvq, vpsrlvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsrlvq_zlok0zlold, vpsrlvq, vpsrlvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvq_zhik7zhizhi, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsrlvq_zhik7zhild, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllw_xlok0xloimm, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsllw_xlok0immld, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllw_xhik7xhiimm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsllw_xhik7immld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllw_ylok0yloimm, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsllw_ylok0immld, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllw_yhik7yhiimm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsllw_yhik7immld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllw_zlok0zloimm, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsllw_zlok0immld, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllw_zhik7zhiimm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsllw_zhik7immld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllw_xlok0xloxmm, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsllw_xlok0xmmld, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllw_xhik7xhixmm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllw_xhik7xmmld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllw_ylok0yloxmm, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsllw_ylok0xmmld, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllw_yhik7yhixmm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsllw_yhik7xmmld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsllw_zlok0zloxmm, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsllw_zlok0xmmld, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllw_zhik7zhixmm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsllw_zhik7xmmld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpslld_xlok0xloimm, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpslld_xlok0immld, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpslld_xhik7xhiimm, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpslld_xhik7immld, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpslld_ylok0yloimm, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpslld_ylok0immld, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpslld_yhik7yhiimm, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpslld_yhik7immld, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpslld_zlok0zloimm, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpslld_zlok0immld, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpslld_zhik7zhiimm, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpslld_zhik7immld, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpslld_xlok0xloxmm, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpslld_xlok0xmmld, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpslld_xhik7xhixmm, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpslld_xhik7xmmld, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpslld_ylok0yloxmm, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpslld_ylok0xmmld, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpslld_yhik7yhixmm, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpslld_yhik7xmmld, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpslld_zlok0zloxmm, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpslld_zlok0xmmld, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpslld_zhik7zhixmm, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpslld_zhik7xmmld, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsllq_xlok0xloimm, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsllq_xlok0immld, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllq_xhik7xhiimm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsllq_xhik7immld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllq_ylok0yloimm, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsllq_ylok0immld, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllq_yhik7yhiimm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsllq_yhik7immld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllq_zlok0zloimm, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsllq_zlok0immld, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllq_zhik7zhiimm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsllq_zhik7immld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllq_xlok0xloxmm, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsllq_xlok0xmmld, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllq_xhik7xhixmm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllq_xhik7xmmld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllq_ylok0yloxmm, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsllq_ylok0xmmld, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllq_yhik7yhixmm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsllq_yhik7xmmld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsllq_zlok0zloxmm, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsllq_zlok0xmmld, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllq_zhik7zhixmm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsllq_zhik7xmmld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vrcp14ss_xlok0xloxmm, vrcp14ss, vrcp14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrcp14ss_xlok0xmmld, vrcp14ss, vrcp14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp14ss_xhik7xhixmm, vrcp14ss, vrcp14ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrcp14ss_xhik7xmmld, vrcp14ss, vrcp14ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp14sd_xlok0xloxmm, vrcp14sd, vrcp14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrcp14sd_xlok0xmmld, vrcp14sd, vrcp14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrcp14sd_xhik7xhixmm, vrcp14sd, vrcp14sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrcp14sd_xhik7xmmld, vrcp14sd, vrcp14sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrcp28ss_xlok0xloxmm, vrcp28ss, vrcp28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrcp28ss_xlok0xmmld, vrcp28ss, vrcp28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp28ss_xhik7xhixmm, vrcp28ss, vrcp28ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrcp28ss_xhik7xmmld, vrcp28ss, vrcp28ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp28sd_xlok0xloxmm, vrcp28sd, vrcp28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrcp28sd_xlok0xmmld, vrcp28sd, vrcp28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrcp28sd_xhik7xhixmm, vrcp28sd, vrcp28sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrcp28sd_xhik7xmmld, vrcp28sd, vrcp28sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vpshufhw_xlok0xlo, vpshufhw, vpshufhw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpshufhw_xlok0ld, vpshufhw, vpshufhw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufhw_xhik7xhi, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vpshufhw_xhik7ld, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufhw_ylok0ylo, vpshufhw, vpshufhw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpshufhw_ylok0ld, vpshufhw, vpshufhw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufhw_yhik7yhi, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vpshufhw_yhik7ld, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufhw_zlok0zlo, vpshufhw, vpshufhw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpshufhw_zlok0ld, vpshufhw, vpshufhw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufhw_zhik7zhi, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vpshufhw_zhik7ld, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufd_xlok0xlo, vpshufd, vpshufd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpshufd_xlok0ld, vpshufd, vpshufd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufd_xhik7xhi, vpshufd, vpshufd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vpshufd_xhik7ld, vpshufd, vpshufd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufd_ylok0ylo, vpshufd, vpshufd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpshufd_ylok0ld, vpshufd, vpshufd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufd_yhik7yhi, vpshufd, vpshufd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vpshufd_yhik7ld, vpshufd, vpshufd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufd_zlok0zlo, vpshufd, vpshufd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpshufd_zlok0ld, vpshufd, vpshufd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufd_zhik7zhi, vpshufd, vpshufd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vpshufd_zhik7ld, vpshufd, vpshufd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshuflw_xlok0xlo, vpshuflw, vpshuflw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpshuflw_xlok0ld, vpshuflw, vpshuflw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshuflw_xhik7xhi, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vpshuflw_xhik7ld, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshuflw_ylok0ylo, vpshuflw, vpshuflw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpshuflw_ylok0ld, vpshuflw, vpshuflw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshuflw_yhik7yhi, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vpshuflw_yhik7ld, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshuflw_zlok0zlo, vpshuflw, vpshuflw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpshuflw_zlok0ld, vpshuflw, vpshuflw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshuflw_zhik7zhi, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vpshuflw_zhik7ld, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufb_xlok0xlo, vpshufb, vpshufb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpshufb_xlok0ld, vpshufb, vpshufb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpshufb_xhik7xhi, vpshufb, vpshufb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpshufb_xhik7ld, vpshufb, vpshufb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpshufb_ylok0ylo, vpshufb, vpshufb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpshufb_ylok0ld, vpshufb, vpshufb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpshufb_yhik7yhi, vpshufb, vpshufb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpshufb_yhik7ld, vpshufb, vpshufb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpshufb_zlok0zlo, vpshufb, vpshufb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpshufb_zlok0ld, vpshufb, vpshufb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpshufb_zhik7zhi, vpshufb, vpshufb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpshufb_zhik7ld, vpshufb, vpshufb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpavgb_xlok0xloxlo, vpavgb, vpavgb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpavgb_xlok0xlold, vpavgb, vpavgb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpavgb_xhik7xhixhi, vpavgb, vpavgb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpavgb_xhik7xhild, vpavgb, vpavgb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))

OPCODE(vpavgb_ylok0yloylo, vpavgb, vpavgb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpavgb_ylok0ylold, vpavgb, vpavgb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpavgb_yhik7yhiyhi, vpavgb, vpavgb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpavgb_yhik7yhild, vpavgb, vpavgb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))

OPCODE(vpavgb_zlok0zlozlo, vpavgb, vpavgb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpavgb_zlok0zlold, vpavgb, vpavgb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpavgb_zhik7zhizhi, vpavgb, vpavgb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpavgb_zhik7zhild, vpavgb, vpavgb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpavgw_xlok0xloxlo, vpavgw, vpavgw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpavgw_xlok0xlold, vpavgw, vpavgw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpavgw_xhik7xhixhi, vpavgw, vpavgw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpavgw_xhik7xhild, vpavgw, vpavgw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpavgw_ylok0yloylo, vpavgw, vpavgw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpavgw_ylok0ylold, vpavgw, vpavgw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpavgw_yhik7yhiyhi, vpavgw, vpavgw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpavgw_yhik7yhild, vpavgw, vpavgw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpavgw_zlok0zlozlo, vpavgw, vpavgw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpavgw_zlok0zlold, vpavgw, vpavgw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpavgw_zhik7zhizhi, vpavgw, vpavgw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpavgw_zhik7zhild, vpavgw, vpavgw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vblendmps_xlok0xloxlo, vblendmps, vblendmps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vblendmps_xlok0xlold, vblendmps, vblendmps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vblendmps_xhik7xhixhi, vblendmps, vblendmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vblendmps_xhik7xhild, vblendmps, vblendmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vblendmps_ylok0yloylo, vblendmps, vblendmps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vblendmps_ylok0ylold, vblendmps, vblendmps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vblendmps_yhik7yhiyhi, vblendmps, vblendmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vblendmps_yhik7yhild, vblendmps, vblendmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vblendmps_zlok0zlozlo, vblendmps, vblendmps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vblendmps_zlok0zlold, vblendmps, vblendmps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vblendmps_zhik7zhizhi, vblendmps, vblendmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vblendmps_zhik7zhild, vblendmps, vblendmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vblendmpd_xlok0xloxlo, vblendmpd, vblendmpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vblendmpd_xlok0xlold, vblendmpd, vblendmpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vblendmpd_xhik7xhixhi, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vblendmpd_xhik7xhild, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vblendmpd_ylok0yloylo, vblendmpd, vblendmpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vblendmpd_ylok0ylold, vblendmpd, vblendmpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vblendmpd_yhik7yhiyhi, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vblendmpd_yhik7yhild, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vblendmpd_zlok0zlozlo, vblendmpd, vblendmpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vblendmpd_zlok0zlold, vblendmpd, vblendmpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vblendmpd_zhik7zhizhi, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vblendmpd_zhik7zhild, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vgetexpss_xlok0xloxmm, vgetexpss, vgetexpss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vgetexpss_xlok0xmmld, vgetexpss, vgetexpss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vgetexpss_xhik7xhixmm, vgetexpss, vgetexpss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vgetexpss_xhik7xmmld, vgetexpss, vgetexpss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vgetexpsd_xlok0xloxmm, vgetexpsd, vgetexpsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vgetexpsd_xlok0xmmld, vgetexpsd, vgetexpsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vgetexpsd_xhik7xhixmm, vgetexpsd, vgetexpsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vgetexpsd_xhik7xmmld, vgetexpsd, vgetexpsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vgetmantps_xlok0xlo, vgetmantps, vgetmantps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vgetmantps_xlok0ld, vgetmantps, vgetmantps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantps_xhik7xhi, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vgetmantps_xhik7ld, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantps_ylok0ylo, vgetmantps, vgetmantps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vgetmantps_ylok0ld, vgetmantps, vgetmantps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantps_yhik7yhi, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vgetmantps_yhik7ld, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantps_zlok0zlo, vgetmantps, vgetmantps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vgetmantps_zlok0ld, vgetmantps, vgetmantps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantps_zhik7zhi, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vgetmantps_zhik7ld, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantpd_xlok0xlo, vgetmantpd, vgetmantpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vgetmantpd_xlok0ld, vgetmantpd, vgetmantpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantpd_xhik7xhi, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vgetmantpd_xhik7ld, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantpd_ylok0ylo, vgetmantpd, vgetmantpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vgetmantpd_ylok0ld, vgetmantpd, vgetmantpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantpd_yhik7yhi, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vgetmantpd_yhik7ld, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantpd_zlok0zlo, vgetmantpd, vgetmantpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vgetmantpd_zlok0ld, vgetmantpd, vgetmantpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantpd_zhik7zhi, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vgetmantpd_zhik7ld, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpblendmb_xlok0xloxlo, vpblendmb, vpblendmb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmb_xlok0xlold, vpblendmb, vpblendmb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmb_xhik7xhixhi, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmb_xhik7xhild, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmb_ylok0yloylo, vpblendmb, vpblendmb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmb_ylok0ylold, vpblendmb, vpblendmb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmb_yhik7yhiyhi, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmb_yhik7yhild, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmb_zlok0zlozlo, vpblendmb, vpblendmb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmb_zlok0zlold, vpblendmb, vpblendmb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmb_zhik7zhizhi, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmb_zhik7zhild, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmw_xlok0xloxlo, vpblendmw, vpblendmw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmw_xlok0xlold, vpblendmw, vpblendmw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmw_xhik7xhixhi, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmw_xhik7xhild, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmw_ylok0yloylo, vpblendmw, vpblendmw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmw_ylok0ylold, vpblendmw, vpblendmw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmw_yhik7yhiyhi, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmw_yhik7yhild, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmw_zlok0zlozlo, vpblendmw, vpblendmw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmw_zlok0zlold, vpblendmw, vpblendmw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmw_zhik7zhizhi, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmw_zhik7zhild, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmd_xlok0xloxlo, vpblendmd, vpblendmd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmd_xlok0xlold, vpblendmd, vpblendmd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmd_xhik7xhixhi, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmd_xhik7xhild, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmd_ylok0yloylo, vpblendmd, vpblendmd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmd_ylok0ylold, vpblendmd, vpblendmd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmd_yhik7yhiyhi, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmd_yhik7yhild, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmd_zlok0zlozlo, vpblendmd, vpblendmd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmd_zlok0zlold, vpblendmd, vpblendmd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmd_zhik7zhizhi, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmd_zhik7zhild, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmq_xlok0xloxlo, vpblendmq, vpblendmq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmq_xlok0xlold, vpblendmq, vpblendmq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmq_xhik7xhixhi, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmq_xhik7xhild, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmq_ylok0yloylo, vpblendmq, vpblendmq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmq_ylok0ylold, vpblendmq, vpblendmq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmq_yhik7yhiyhi, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmq_yhik7yhild, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmq_zlok0zlozlo, vpblendmq, vpblendmq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmq_zlok0zlold, vpblendmq, vpblendmq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmq_zhik7zhizhi, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmq_zhik7zhild, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmb_k1k0xloxlo, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmb_k1k0xlold, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmb_k6k7xhixhi, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmb_k6k7xhild, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmb_k1k0yloylo, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmb_k1k0ylold, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmb_k6k7yhiyhi, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmb_k6k7yhild, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmb_k1k0zlozlo, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmb_k1k0zlold, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmb_k6k7zhizhi, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmb_k6k7zhild, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmw_k1k0xloxlo, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmw_k1k0xlold, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmw_k6k7xhixhi, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmw_k6k7xhild, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmw_k1k0yloylo, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmw_k1k0ylold, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmw_k6k7yhiyhi, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmw_k6k7yhild, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmw_k1k0zlozlo, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmw_k1k0zlold, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmw_k6k7zhizhi, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmw_k6k7zhild, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmd_k1k0xloxlo, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmd_k1k0xlold, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmd_k6k7xhixhi, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmd_k6k7xhild, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmd_k1k0yloylo, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmd_k1k0ylold, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmd_k6k7yhiyhi, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmd_k6k7yhild, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmd_k1k0zlozlo, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmd_k1k0zlold, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmd_k6k7zhizhi, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmd_k6k7zhild, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmq_k1k0xloxlo, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmq_k1k0xlold, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmq_k6k7xhixhi, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmq_k6k7xhild, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmq_k1k0yloylo, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmq_k1k0ylold, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmq_k6k7yhiyhi, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmq_k6k7yhild, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmq_k1k0zlozlo, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmq_k1k0zlold, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmq_k6k7zhizhi, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmq_k6k7zhild, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmb_k1k0xloxlo, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmb_k1k0xlold, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmb_k6k7xhixhi, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmb_k6k7xhild, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmb_k1k0yloylo, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmb_k1k0ylold, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmb_k6k7yhiyhi, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmb_k6k7yhild, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmb_k1k0zlozlo, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmb_k1k0zlold, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmb_k6k7zhizhi, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmb_k6k7zhild, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmw_k1k0xloxlo, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmw_k1k0xlold, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmw_k6k7xhixhi, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmw_k6k7xhild, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmw_k1k0yloylo, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmw_k1k0ylold, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmw_k6k7yhiyhi, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmw_k6k7yhild, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmw_k1k0zlozlo, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmw_k1k0zlold, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmw_k6k7zhizhi, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmw_k6k7zhild, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmd_k1k0xloxlo, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmd_k1k0xlold, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmd_k6k7xhixhi, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmd_k6k7xhild, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmd_k1k0yloylo, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmd_k1k0ylold, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmd_k6k7yhiyhi, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmd_k6k7yhild, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmd_k1k0zlozlo, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmd_k1k0zlold, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmd_k6k7zhizhi, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmd_k6k7zhild, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmq_k1k0xloxlo, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmq_k1k0xlold, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmq_k6k7xhixhi, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmq_k6k7xhild, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmq_k1k0yloylo, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmq_k1k0ylold, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmq_k6k7yhiyhi, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmq_k6k7yhild, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmq_k1k0zlozlo, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmq_k1k0zlold, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmq_k6k7zhizhi, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmq_k6k7zhild, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vreduceps_xlok0xlo, vreduceps, vreduceps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vreduceps_xlok0ld, vreduceps, vreduceps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreduceps_xhik7xhi, vreduceps, vreduceps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vreduceps_xhik7ld, vreduceps, vreduceps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreduceps_ylok0ylo, vreduceps, vreduceps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vreduceps_ylok0ld, vreduceps, vreduceps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreduceps_yhik7yhi, vreduceps, vreduceps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vreduceps_yhik7ld, vreduceps, vreduceps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreduceps_zlok0zlo, vreduceps, vreduceps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vreduceps_zlok0ld, vreduceps, vreduceps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreduceps_zhik7zhi, vreduceps, vreduceps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vreduceps_zhik7ld, vreduceps, vreduceps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreducepd_xlok0xlo, vreducepd, vreducepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vreducepd_xlok0ld, vreducepd, vreducepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreducepd_xhik7xhi, vreducepd, vreducepd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vreducepd_xhik7ld, vreducepd, vreducepd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreducepd_ylok0ylo, vreducepd, vreducepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vreducepd_ylok0ld, vreducepd, vreducepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreducepd_yhik7yhi, vreducepd, vreducepd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vreducepd_yhik7ld, vreducepd, vreducepd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreducepd_zlok0zlo, vreducepd, vreducepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vreducepd_zlok0ld, vreducepd, vreducepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreducepd_zhik7zhi, vreducepd, vreducepd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vreducepd_zhik7ld, vreducepd, vreducepd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscaleps_xlok0xlo, vrndscaleps, vrndscaleps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vrndscaleps_xlok0ld, vrndscaleps, vrndscaleps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscaleps_xhik7xhi, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vrndscaleps_xhik7ld, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscaleps_ylok0ylo, vrndscaleps, vrndscaleps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vrndscaleps_ylok0ld, vrndscaleps, vrndscaleps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscaleps_yhik7yhi, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vrndscaleps_yhik7ld, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscaleps_zlok0zlo, vrndscaleps, vrndscaleps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vrndscaleps_zlok0ld, vrndscaleps, vrndscaleps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscaleps_zhik7zhi, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vrndscaleps_zhik7ld, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscalepd_xlok0xlo, vrndscalepd, vrndscalepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vrndscalepd_xlok0ld, vrndscalepd, vrndscalepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscalepd_xhik7xhi, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vrndscalepd_xhik7ld, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscalepd_ylok0ylo, vrndscalepd, vrndscalepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vrndscalepd_ylok0ld, vrndscalepd, vrndscalepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscalepd_yhik7yhi, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vrndscalepd_yhik7ld, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscalepd_zlok0zlo, vrndscalepd, vrndscalepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vrndscalepd_zlok0ld, vrndscalepd, vrndscalepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscalepd_zhik7zhi, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vrndscalepd_zhik7ld, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrsqrt14ss_xlok0xloxmm, vrsqrt14ss, vrsqrt14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrsqrt14ss_xlok0xmmld, vrsqrt14ss, vrsqrt14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt14ss_xhik7xhixmm, vrsqrt14ss, vrsqrt14ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrsqrt14ss_xhik7xmmld, vrsqrt14ss, vrsqrt14ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt14sd_xlok0xloxmm, vrsqrt14sd, vrsqrt14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrsqrt14sd_xlok0xmmld, vrsqrt14sd, vrsqrt14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrsqrt14sd_xhik7xhixmm, vrsqrt14sd, vrsqrt14sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrsqrt14sd_xhik7xmmld, vrsqrt14sd, vrsqrt14sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrsqrt28ss_xlok0xloxmm, vrsqrt28ss, vrsqrt28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrsqrt28ss_xlok0xmmld, vrsqrt28ss, vrsqrt28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt28ss_xhik7xhixmm, vrsqrt28ss, vrsqrt28ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrsqrt28ss_xhik7xmmld, vrsqrt28ss, vrsqrt28ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt28sd_xlok0xloxmm, vrsqrt28sd, vrsqrt28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrsqrt28sd_xlok0xmmld, vrsqrt28sd, vrsqrt28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrsqrt28sd_xhik7xhixmm, vrsqrt28sd, vrsqrt28sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrsqrt28sd_xhik7xmmld, vrsqrt28sd, vrsqrt28sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vscalefps_xlok0xloxlo, vscalefps, vscalefps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vscalefps_xlok0xlold, vscalefps, vscalefps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vscalefps_xhik7xhixhi, vscalefps, vscalefps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vscalefps_xhik7xhild, vscalefps, vscalefps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vscalefps_ylok0yloylo, vscalefps, vscalefps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vscalefps_ylok0ylold, vscalefps, vscalefps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vscalefps_yhik7yhiyhi, vscalefps, vscalefps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vscalefps_yhik7yhild, vscalefps, vscalefps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vscalefps_zlok0zlozlo, vscalefps, vscalefps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vscalefps_zlok0zlold, vscalefps, vscalefps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vscalefps_zhik7zhizhi, vscalefps, vscalefps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vscalefps_zhik7zhild, vscalefps, vscalefps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vscalefpd_xlok0xloxlo, vscalefpd, vscalefpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vscalefpd_xlok0xlold, vscalefpd, vscalefpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vscalefpd_xhik7xhixhi, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vscalefpd_xhik7xhild, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vscalefpd_ylok0yloylo, vscalefpd, vscalefpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vscalefpd_ylok0ylold, vscalefpd, vscalefpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vscalefpd_yhik7yhiyhi, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vscalefpd_yhik7yhild, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vscalefpd_zlok0zlozlo, vscalefpd, vscalefpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vscalefpd_zlok0zlold, vscalefpd, vscalefpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vscalefpd_zhik7zhizhi, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vscalefpd_zhik7zhild, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vscalefss_xlok0xloxmm, vscalefss, vscalefss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vscalefss_xlok0xmmld, vscalefss, vscalefss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vscalefss_xhik7xhixmm, vscalefss, vscalefss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vscalefss_xhik7xmmld, vscalefss, vscalefss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vscalefsd_xlok0xloxmm, vscalefsd, vscalefsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vscalefsd_xlok0xmmld, vscalefsd, vscalefsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vscalefsd_xhik7xhixmm, vscalefsd, vscalefsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vscalefsd_xhik7xmmld, vscalefsd, vscalefsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfpclassps_k1k0xlo, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vfpclassps_k1k0ld16, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclassps_k6k7xhi, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vfpclassps_k6k7ld16, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclassps_k1k0ylo, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vfpclassps_k1k0ld32, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclassps_k6k7yhi, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vfpclassps_k6k7ld32, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclassps_k1k0zlo, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vfpclassps_k1k0ld64, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclassps_k6k7zhi, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vfpclassps_k6k7ld64, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclasspd_k1k0xlo, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vfpclasspd_k1k0ld16, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclasspd_k6k7xhi, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vfpclasspd_k6k7ld16, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclasspd_k1k0ylo, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vfpclasspd_k1k0ld32, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclasspd_k6k7yhi, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vfpclasspd_k6k7ld32, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclasspd_k1k0zlo, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vfpclasspd_k1k0ld64, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclasspd_k6k7zhi, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vfpclasspd_k6k7ld64, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclassss_k1k0xloxmm, vfpclassss, vfpclassss_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vfpclassss_k1k0xmmld, vfpclassss, vfpclassss_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vfpclassss_k6k7xhixmm, vfpclassss, vfpclassss_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vfpclassss_k6k7xmmld, vfpclassss, vfpclassss_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vfpclasssd_k1k0xloxmm, vfpclasssd, vfpclasssd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vfpclasssd_k1k0xmmld, vfpclasssd, vfpclasssd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vfpclasssd_k6k7xhixmm, vfpclasssd, vfpclasssd_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vfpclasssd_k6k7xmmld, vfpclasssd, vfpclasssd_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_xlok0xloxlo, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmadd52huq_xlok0xlold, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmadd52huq_xhik7xhixhi, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmadd52huq_xhik7xhild, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmadd52huq_ylok0yloylo, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmadd52huq_ylok0ylold, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmadd52huq_yhik7yhiyhi, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmadd52huq_yhik7yhild, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmadd52huq_zlok0zlozlo, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmadd52huq_zlok0zlold, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmadd52huq_zhik7zhizhi, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmadd52huq_zhik7zhild, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmadd52luq_xlok0xloxlo, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmadd52luq_xlok0xlold, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmadd52luq_xhik7xhixhi, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmadd52luq_xhik7xhild, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmadd52luq_ylok0yloylo, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmadd52luq_ylok0ylold, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmadd52luq_yhik7yhiyhi, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmadd52luq_yhik7yhild, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmadd52luq_zlok0zlozlo, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmadd52luq_zlok0zlold, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmadd52luq_zhik7zhizhi, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmadd52luq_zhik7zhild, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
