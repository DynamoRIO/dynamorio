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
 * Part A: Split up to avoid VS running out of memory (i#3992,i#4610).
 */
OPCODE(vmovss_xlok0xlo, vmovss, vmovss_NDS_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vmovss_xhik0xlo, vmovss, vmovss_NDS_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovss_xlok0xhi, vmovss, vmovss_NDS_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vmovss_xlok7xlo, vmovss, vmovss_NDS_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vmovss_xhik7xlo, vmovss, vmovss_NDS_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovss_xlok7xhi, vmovss, vmovss_NDS_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vmovsd_xlok0xlo, vmovsd, vmovsd_NDS_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovsd_xhik0xlo, vmovsd, vmovsd_NDS_mask, X64_ONLY, REGARG(XMM16), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovsd_xlok0xhi, vmovsd, vmovsd_NDS_mask, X64_ONLY, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vmovsd_xlok7xlo, vmovsd, vmovsd_NDS_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovsd_xhik7xlo, vmovsd, vmovsd_NDS_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovsd_xlok7xhi, vmovsd, vmovsd_NDS_mask, X64_ONLY, REGARG(XMM0), REGARG(K7),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vunpcklps_xlok0xlold, vunpcklps, vunpcklps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpcklps_xhik7xhild, vunpcklps, vunpcklps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vunpcklps_ylok0ylold, vunpcklps, vunpcklps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpcklps_yhik7yhild, vunpcklps, vunpcklps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vunpcklps_zlok0zlold, vunpcklps, vunpcklps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vunpcklps_zhik7zhild, vunpcklps, vunpcklps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vunpcklpd_xlok0xlold, vunpcklpd, vunpcklpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpcklpd_xhik7xhild, vunpcklpd, vunpcklpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vunpcklpd_ylok0ylold, vunpcklpd, vunpcklpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpcklpd_yhik7yhild, vunpcklpd, vunpcklpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vunpcklpd_zlok0zlold, vunpcklpd, vunpcklpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vunpcklpd_zhik7zhild, vunpcklpd, vunpcklpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vunpckhps_xlok0xlold, vunpckhps, vunpckhps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpckhps_xhik7xhild, vunpckhps, vunpckhps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vunpckhps_ylok0ylold, vunpckhps, vunpckhps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpckhps_yhik7yhild, vunpckhps, vunpckhps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vunpckhps_zlok0zlold, vunpckhps, vunpckhps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vunpckhps_zhik7zhild, vunpckhps, vunpckhps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vunpckhpd_xlok0xlold, vunpckhpd, vunpckhpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpckhpd_xhik7xhild, vunpckhpd, vunpckhpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vunpckhpd_ylok0ylold, vunpckhpd, vunpckhpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpckhpd_yhik7yhild, vunpckhpd, vunpckhpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vunpckhpd_zlok0zlold, vunpckhpd, vunpckhpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vunpckhpd_zhik7zhild, vunpckhpd, vunpckhpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vandps_xlok0xloxlo, vandps, vandps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vandps_xlok0xlold, vandps, vandps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vandps_xlok0xlobcst, vandps, vandps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vandps_xhik7xhixhi, vandps, vandps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandps_xhik7xhild, vandps, vandps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandps_xhik7xhibcst, vandps, vandps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vandps_ylok0yloylo, vandps, vandps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vandps_ylok0ylold, vandps, vandps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vandps_ylok0ylobcst, vandps, vandps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vandps_yhik7yhiyhi, vandps, vandps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandps_yhik7yhild, vandps, vandps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandps_yhik7yhibcst, vandps, vandps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vandps_zlok0zlozlo, vandps, vandps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vandps_zlok0zlold, vandps, vandps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vandps_zlok0zlobcst, vandps, vandps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vandps_zhik7zhizhi, vandps, vandps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandps_zhik7zhild, vandps, vandps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandps_zhik7zhibcst, vandps, vandps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vandpd_xlok0xloxlo, vandpd, vandpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vandpd_xlok0xlold, vandpd, vandpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vandpd_xlok0xlobcst, vandpd, vandpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vandpd_xhik7xhixhi, vandpd, vandpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandpd_xhik7xhild, vandpd, vandpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandpd_xhik7xhibcst, vandpd, vandpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vandpd_ylok0yloylo, vandpd, vandpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vandpd_ylok0ylold, vandpd, vandpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vandpd_ylok0ylobcst, vandpd, vandpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vandpd_yhik7yhiyhi, vandpd, vandpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandpd_yhik7yhild, vandpd, vandpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandpd_yhik7yhibcst, vandpd, vandpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vandpd_zlok0zlozlo, vandpd, vandpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vandpd_zlok0zlold, vandpd, vandpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vandpd_zlok0zlobcst, vandpd, vandpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vandpd_zhik7zhizhi, vandpd, vandpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandpd_zhik7zhild, vandpd, vandpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandpd_zhik7zhibcst, vandpd, vandpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vandnps_xlok0xloxlo, vandnps, vandnps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vandnps_xlok0xlold, vandnps, vandnps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vandnps_xlok0xlobcst, vandnps, vandnps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vandnps_xhik7xhixhi, vandnps, vandnps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandnps_xhik7xhild, vandnps, vandnps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandnps_xhik7xhibcst, vandnps, vandnps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vandnps_ylok0yloylo, vandnps, vandnps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vandnps_ylok0ylold, vandnps, vandnps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandnps_ylok0ylobcst, vandnps, vandnps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vandnps_yhik7yhiyhi, vandnps, vandnps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandnps_yhik7yhild, vandnps, vandnps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandnps_yhik7yhibcst, vandnps, vandnps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vandnps_zlok0zlozlo, vandnps, vandnps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vandnps_zlok0zlold, vandnps, vandnps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vandnps_zlok0zlobcst, vandnps, vandnps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vandnps_zhik7zhizhi, vandnps, vandnps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandnps_zhik7zhild, vandnps, vandnps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandnps_zhik7zhibcst, vandnps, vandnps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vandnpd_xlok0xloxlo, vandnpd, vandnpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vandnpd_xlok0xlold, vandnpd, vandnpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vandnpd_xlok0xlobcst, vandnpd, vandnpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vandnpd_xhik7xhixhi, vandnpd, vandnpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandnpd_xhik7xhild, vandnpd, vandnpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandnpd_xhik7xhibcst, vandnpd, vandnpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vandnpd_ylok0yloylo, vandnpd, vandnpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vandnpd_ylok0ylold, vandnpd, vandnpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandnpd_ylok0ylobcst, vandnpd, vandnpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vandnpd_yhik7yhiyhi, vandnpd, vandnpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandnpd_yhik7yhild, vandnpd, vandnpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandnpd_yhik7yhibcst, vandnpd, vandnpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vandnpd_zlok0zlozlo, vandnpd, vandnpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vandnpd_zlok0zlold, vandnpd, vandnpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vandnpd_zlok0zlobcst, vandnpd, vandnpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vandnpd_zhik7zhizhi, vandnpd, vandnpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandnpd_zhik7zhild, vandnpd, vandnpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandnpd_zhik7zhibcst, vandnpd, vandnpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vorps_xlok0xloxlo, vorps, vorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vorps_xlok0xlold, vorps, vorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vorps_xlok0xlobcst, vorps, vorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_4))
OPCODE(vorps_xhik7xhixhi, vorps, vorps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vorps_xhik7xhild, vorps, vorps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vorps_xhik7xhibcst, vorps, vorps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vorps_ylok0yloylo, vorps, vorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vorps_ylok0ylold, vorps, vorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vorps_ylok0ylobcst, vorps, vorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_4))
OPCODE(vorps_yhik7yhiyhi, vorps, vorps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vorps_yhik7yhild, vorps, vorps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vorps_yhik7yhibcst, vorps, vorps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vorps_zlok0zlozlo, vorps, vorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vorps_zlok0zlold, vorps, vorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vorps_zlok0zlobcst, vorps, vorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_4))
OPCODE(vorps_zhik7zhizhi, vorps, vorps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vorps_zhik7zhild, vorps, vorps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vorps_zhik7zhibcst, vorps, vorps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vorpd_xlok0xloxlo, vorpd, vorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vorpd_xlok0xlold, vorpd, vorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vorpd_xlok0xlobcst, vorpd, vorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_8))
OPCODE(vorpd_xhik7xhixhi, vorpd, vorpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vorpd_xhik7xhild, vorpd, vorpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vorpd_xhik7xhibcst, vorpd, vorpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vorpd_ylok0yloylo, vorpd, vorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vorpd_ylok0ylold, vorpd, vorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vorpd_ylok0ylobcst, vorpd, vorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_8))
OPCODE(vorpd_yhik7yhiyhi, vorpd, vorpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vorpd_yhik7yhild, vorpd, vorpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vorpd_yhik7yhibcst, vorpd, vorpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vorpd_zlok0zlozlo, vorpd, vorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vorpd_zlok0zlold, vorpd, vorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vorpd_zlok0zlobcst, vorpd, vorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_8))
OPCODE(vorpd_zhik7zhizhi, vorpd, vorpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vorpd_zhik7zhild, vorpd, vorpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vorpd_zhik7zhibcst, vorpd, vorpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vxorps_xlok0xloxlo, vxorps, vxorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vxorps_xlok0xlold, vxorps, vxorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vxorps_xlok0xlobcst, vxorps, vxorps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vxorps_xhik7xhixhi, vxorps, vxorps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vxorps_xhik7xhild, vxorps, vxorps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vxorps_xhik7xhibcst, vxorps, vxorps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vxorps_ylok0yloylo, vxorps, vxorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vxorps_ylok0ylold, vxorps, vxorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vxorps_ylok0ylobcst, vxorps, vxorps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vxorps_yhik7yhiyhi, vxorps, vxorps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vxorps_yhik7yhild, vxorps, vxorps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vxorps_yhik7yhibcst, vxorps, vxorps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vxorps_zlok0zlozlo, vxorps, vxorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vxorps_zlok0zlold, vxorps, vxorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vxorps_zlok0zlobcst, vxorps, vxorps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vxorps_zhik7zhizhi, vxorps, vxorps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vxorps_zhik7zhild, vxorps, vxorps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vxorps_zhik7zhibcst, vxorps, vxorps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vxorpd_xlok0xloxlo, vxorpd, vxorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vxorpd_xlok0xlold, vxorpd, vxorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vxorpd_xlok0xlobcst, vxorpd, vxorpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vxorpd_xhik7xhixhi, vxorpd, vxorpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vxorpd_xhik7xhild, vxorpd, vxorpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vxorpd_xhik7xhibcst, vxorpd, vxorpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vxorpd_ylok0yloylo, vxorpd, vxorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vxorpd_ylok0ylold, vxorpd, vxorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vxorpd_ylok0ylobcst, vxorpd, vxorpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vxorpd_yhik7yhiyhi, vxorpd, vxorpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vxorpd_yhik7yhild, vxorpd, vxorpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vxorpd_yhik7yhibcst, vxorpd, vxorpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vxorpd_zlok0zlozlo, vxorpd, vxorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vxorpd_zlok0zlold, vxorpd, vxorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vxorpd_zlok0zlobcst, vxorpd, vxorpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vxorpd_zhik7zhizhi, vxorpd, vxorpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vxorpd_zhik7zhild, vxorpd, vxorpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vxorpd_zhik7zhibcst, vxorpd, vxorpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpandd_xlok0xloxlo, vpandd, vpandd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpandd_xlok0xlold, vpandd, vpandd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpandd_xlok0xlobcst, vpandd, vpandd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpandd_xhik7xhixhi, vpandd, vpandd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandd_xhik7xhild, vpandd, vpandd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandd_xhik7xhibcst, vpandd, vpandd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpandd_ylok0yloylo, vpandd, vpandd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpandd_ylok0ylold, vpandd, vpandd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpandd_ylok0ylobcst, vpandd, vpandd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpandd_yhik7yhiyhi, vpandd, vpandd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandd_yhik7yhild, vpandd, vpandd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandd_yhik7yhibcst, vpandd, vpandd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpandd_zlok0zlozlo, vpandd, vpandd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpandd_zlok0zlold, vpandd, vpandd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpandd_zlok0zlobcst, vpandd, vpandd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpandd_zhik7zhizhi, vpandd, vpandd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandd_zhik7zhild, vpandd, vpandd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandd_zhik7zhibcst, vpandd, vpandd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpandq_xlok0xloxlo, vpandq, vpandq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpandq_xlok0xlold, vpandq, vpandq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpandq_xlok0xlobcst, vpandq, vpandq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpandq_xhik7xhixhi, vpandq, vpandq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandq_xhik7xhild, vpandq, vpandq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandq_xhik7xhibcst, vpandq, vpandq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpandq_ylok0yloylo, vpandq, vpandq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpandq_ylok0ylold, vpandq, vpandq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpandq_ylok0ylobcst, vpandq, vpandq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpandq_yhik7yhiyhi, vpandq, vpandq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandq_yhik7yhild, vpandq, vpandq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandq_yhik7yhibcst, vpandq, vpandq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpandq_zlok0zlozlo, vpandq, vpandq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpandq_zlok0zlold, vpandq, vpandq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpandq_zlok0zlobcst, vpandq, vpandq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpandq_zhik7zhizhi, vpandq, vpandq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandq_zhik7zhild, vpandq, vpandq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandq_zhik7zhibcst, vpandq, vpandq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpandnd_xlok0xloxlo, vpandnd, vpandnd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpandnd_xlok0xlold, vpandnd, vpandnd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpandnd_xlok0xlobcst, vpandnd, vpandnd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpandnd_xhik7xhixhi, vpandnd, vpandnd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandnd_xhik7xhild, vpandnd, vpandnd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandnd_xhik7xhibcst, vpandnd, vpandnd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpandnd_ylok0yloylo, vpandnd, vpandnd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpandnd_ylok0ylold, vpandnd, vpandnd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpandnd_ylok0ylobcst, vpandnd, vpandnd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpandnd_yhik7yhiyhi, vpandnd, vpandnd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandnd_yhik7yhild, vpandnd, vpandnd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandnd_yhik7yhibcst, vpandnd, vpandnd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpandnd_zlok0zlozlo, vpandnd, vpandnd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpandnd_zlok0zlozld, vpandnd, vpandnd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpandnd_zlok0zlozbcst, vpandnd, vpandnd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpandnd_zhik7zhizhi, vpandnd, vpandnd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandnd_zhik7zhild, vpandnd, vpandnd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandnd_zhik7zhibcst, vpandnd, vpandnd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpandnq_xlok0xloxlo, vpandnq, vpandnq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpandnq_xlok0xlold, vpandnq, vpandnq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpandnq_xlok0xlobcst, vpandnq, vpandnq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpandnq_xhik7xhixhi, vpandnq, vpandnq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandnq_xhik7xhild, vpandnq, vpandnq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandnq_xhik7xhibcst, vpandnq, vpandnq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpandnq_ylok0yloylo, vpandnq, vpandnq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpandnq_ylok0ylold, vpandnq, vpandnq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpandnq_ylok0ylobcst, vpandnq, vpandnq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpandnq_yhik7yhiyhi, vpandnq, vpandnq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandnq_yhik7yhild, vpandnq, vpandnq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandnq_yhik7yhibcst, vpandnq, vpandnq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpandnq_zlok0zlozlo, vpandnq, vpandnq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpandnq_zlok0zlold, vpandnq, vpandnq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpandnq_zlok0zlobcst, vpandnq, vpandnq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpandnq_zhik7zhizhi, vpandnq, vpandnq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandnq_zhik7zhild, vpandnq, vpandnq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandnq_zhik7zhibcst, vpandnq, vpandnq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpord_xlok0xloxlo, vpord, vpord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpord_xlok0xlold, vpord, vpord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpord_xlok0xlobcst, vpord, vpord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_4))
OPCODE(vpord_xhik7xhixhi, vpord, vpord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpord_xhik7xhild, vpord, vpord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpord_xhik7xhibcst, vpord, vpord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpord_ylok0yloylo, vpord, vpord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpord_ylok0ylold, vpord, vpord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpord_ylok0ylobcst, vpord, vpord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_4))
OPCODE(vpord_yhik7yhiyhi, vpord, vpord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpord_yhik7yhild, vpord, vpord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpord_yhik7yhibcst, vpord, vpord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpord_zlok0zlozlo, vpord, vpord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpord_zlok0zlold, vpord, vpord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpord_zlok0zlobcst, vpord, vpord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_4))
OPCODE(vpord_zhik7zhizhi, vpord, vpord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpord_zhik7zhild, vpord, vpord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpord_zhik7zhibcst, vpord, vpord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vporq_xlok0xloxlo, vporq, vporq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vporq_xlok0xlold, vporq, vporq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vporq_xlok0xlobcst, vporq, vporq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_8))
OPCODE(vporq_xhik7xhixhi, vporq, vporq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vporq_xhik7xhild, vporq, vporq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vporq_xhik7xhibcst, vporq, vporq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vporq_ylok0yloylo, vporq, vporq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vporq_ylok0ylold, vporq, vporq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vporq_ylok0ylobcst, vporq, vporq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_8))
OPCODE(vporq_yhik7yhiyhi, vporq, vporq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vporq_yhik7yhild, vporq, vporq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vporq_yhik7yhibcst, vporq, vporq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vporq_zlok0zlozlo, vporq, vporq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vporq_zlok0zlold, vporq, vporq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vporq_zlok0zlobcst, vporq, vporq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_8))
OPCODE(vporq_zhik7zhizhi, vporq, vporq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vporq_zhik7zhild, vporq, vporq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vporq_zhik7zhibcst, vporq, vporq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpxord_xlok0xloxlo, vpxord, vpxord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpxord_xlok0xlold, vpxord, vpxord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpxord_xlok0xlobcst, vpxord, vpxord_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpxord_xhik7xhixhi, vpxord, vpxord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpxord_xhik7xhild, vpxord, vpxord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpxord_xhik7xhibcst, vpxord, vpxord_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpxord_ylok0yloylo, vpxord, vpxord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpxord_ylok0ylold, vpxord, vpxord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpxord_ylok0ylobcst, vpxord, vpxord_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpxord_yhik7yhiyhi, vpxord, vpxord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpxord_yhik7yhild, vpxord, vpxord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpxord_yhik7yhibcst, vpxord, vpxord_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpxord_zlok0zlozlo, vpxord, vpxord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpxord_zlok0zlold, vpxord, vpxord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpxord_zlok0zlobcst, vpxord, vpxord_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpxord_zhik7zhizhi, vpxord, vpxord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpxord_zhik7zhild, vpxord, vpxord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpxord_zhik7zhibcst, vpxord, vpxord_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpxorq_xlok0xloxlo, vpxorq, vpxorq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpxorq_xlok0xlold, vpxorq, vpxorq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpxorq_xlok0xlobcst, vpxorq, vpxorq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpxorq_xhik7xhixhi, vpxorq, vpxorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpxorq_xhik7xhild, vpxorq, vpxorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpxorq_xhik7xhibcst, vpxorq, vpxorq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpxorq_ylok0yloylo, vpxorq, vpxorq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpxorq_ylok0ylold, vpxorq, vpxorq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpxorq_ylok0ylobcst, vpxorq, vpxorq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpxorq_yhik7yhiyhi, vpxorq, vpxorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpxorq_yhik7yhild, vpxorq, vpxorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpxorq_yhik7yhibcst, vpxorq, vpxorq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpxorq_zlok0zlozlo, vpxorq, vpxorq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpxorq_zlok0zlold, vpxorq, vpxorq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpxorq_zlok0zlobcst, vpxorq, vpxorq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpxorq_zhik7zhizhi, vpxorq, vpxorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpxorq_zhik7zhild, vpxorq, vpxorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpxorq_zhik7zhibcst, vpxorq, vpxorq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vaddps_xlok0xloxlo, vaddps, vaddps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vaddps_xlok0xlold, vaddps, vaddps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vaddps_xlok0xlobcst, vaddps, vaddps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vaddps_xhik7xhixhi, vaddps, vaddps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vaddps_xhik7xhild, vaddps, vaddps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vaddps_xhik7xhibcst, vaddps, vaddps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vaddps_ylok0yloylo, vaddps, vaddps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vaddps_ylok0ylold, vaddps, vaddps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vaddps_ylok0ylobcst, vaddps, vaddps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vaddps_yhik7yhiyhi, vaddps, vaddps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vaddps_yhik7yhild, vaddps, vaddps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vaddps_yhik7yhibcst, vaddps, vaddps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vaddps_zlok0zlozlo, vaddps, vaddps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vaddps_zlok0zlold, vaddps, vaddps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vaddps_zlok0zlobcst, vaddps, vaddps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vaddps_zhik7zhizhi, vaddps, vaddps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vaddps_zhik7zhild, vaddps, vaddps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vaddps_zhik7zhibcst, vaddps, vaddps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vaddpd_xlok0xloxlo, vaddpd, vaddpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vaddpd_xlok0xlold, vaddpd, vaddpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vaddpd_xlok0xlobcst, vaddpd, vaddpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vaddpd_xhik7xhixhi, vaddpd, vaddpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vaddpd_xhik7xhild, vaddpd, vaddpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vaddpd_xhik7xhibcst, vaddpd, vaddpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vaddpd_ylok0yloylo, vaddpd, vaddpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vaddpd_ylok0ylold, vaddpd, vaddpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vaddpd_ylok0ylobcst, vaddpd, vaddpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vaddpd_yhik7yhiyhi, vaddpd, vaddpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vaddpd_yhik7yhild, vaddpd, vaddpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vaddpd_yhik7yhibcst, vaddpd, vaddpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vaddpd_zlok0zlozlo, vaddpd, vaddpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vaddpd_zlok0zlold, vaddpd, vaddpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vaddpd_zlok0zlobcst, vaddpd, vaddpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vaddpd_zhik7zhizhi, vaddpd, vaddpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vaddpd_zhik7zhild, vaddpd, vaddpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vaddpd_zhik7zhibcst, vaddpd, vaddpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vsubps_xlok0xloxlo, vsubps, vsubps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vsubps_xlok0xlold, vsubps, vsubps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vsubps_xlok0xlobcst, vsubps, vsubps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vsubps_xhik7xhixhi, vsubps, vsubps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vsubps_xhik7xhild, vsubps, vsubps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vsubps_xhik7xhibcst, vsubps, vsubps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vsubps_ylok0yloylo, vsubps, vsubps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vsubps_ylok0ylold, vsubps, vsubps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vsubps_ylok0ylobcst, vsubps, vsubps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vsubps_yhik7yhiyhi, vsubps, vsubps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vsubps_yhik7yhild, vsubps, vsubps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vsubps_yhik7yhibcst, vsubps, vsubps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vsubps_zlok0zlozlo, vsubps, vsubps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vsubps_zlok0zlold, vsubps, vsubps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vsubps_zlok0zlobcst, vsubps, vsubps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vsubps_zhik7zhizhi, vsubps, vsubps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vsubps_zhik7zhild, vsubps, vsubps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vsubps_zhik7zhibcst, vsubps, vsubps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vsubpd_xlok0xloxlo, vsubpd, vsubpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vsubpd_xlok0xlold, vsubpd, vsubpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vsubpd_xlok0xlobcst, vsubpd, vsubpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vsubpd_xhik7xhixhi, vsubpd, vsubpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vsubpd_xhik7xhild, vsubpd, vsubpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vsubpd_xhik7xhibcst, vsubpd, vsubpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vsubpd_ylok0yloylo, vsubpd, vsubpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vsubpd_ylok0ylold, vsubpd, vsubpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vsubpd_ylok0ylobcst, vsubpd, vsubpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vsubpd_yhik7yhiyhi, vsubpd, vsubpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vsubpd_yhik7yhild, vsubpd, vsubpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vsubpd_yhik7yhibcst, vsubpd, vsubpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vsubpd_zlok0zlozlo, vsubpd, vsubpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vsubpd_zlok0zlold, vsubpd, vsubpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vsubpd_zlok0zlobcst, vsubpd, vsubpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vsubpd_zhik7zhizhi, vsubpd, vsubpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vsubpd_zhik7zhild, vsubpd, vsubpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vsubpd_zhik7zhibcst, vsubpd, vsubpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vaddss_xlok0xloxlo, vaddss, vaddss_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vaddss_xhik7xhixhi, vaddss, vaddss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vaddss_xhik7xhild, vaddss, vaddss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vaddsd_xlok0xloxlo, vaddsd, vaddsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vaddsd_xhik7xhixhi, vaddsd, vaddsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vaddsd_xhik7xhild, vaddsd, vaddsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vsubss_xlok0xloxlo, vsubss, vsubss_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vsubss_xhik7xhixhi, vsubss, vsubss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vsubss_xhik7xhild, vsubss, vsubss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vsubsd_xlok0xloxlo, vsubsd, vsubsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vsubsd_xhik7xhixhi, vsubsd, vsubsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vsubsd_xhik7xhild, vsubsd, vsubsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpaddb_xlok0xloxlo, vpaddb, vpaddb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpaddb_xhik7xhixhi, vpaddb, vpaddb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddb_xhik7xhild, vpaddb, vpaddb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddb_ylok0yloylo, vpaddb, vpaddb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpaddb_yhik7yhiyhi, vpaddb, vpaddb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddb_yhik7yhild, vpaddb, vpaddb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddb_zlok0zlozlo, vpaddb, vpaddb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpaddb_zhik7zhizhi, vpaddb, vpaddb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddb_zhik7zhild, vpaddb, vpaddb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddw_xlok0xloxlo, vpaddw, vpaddw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpaddw_xhik7xhixhi, vpaddw, vpaddw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddw_xhik7xhild, vpaddw, vpaddw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddw_ylok0yloylo, vpaddw, vpaddw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpaddw_yhik7yhiyhi, vpaddw, vpaddw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddw_yhik7yhild, vpaddw, vpaddw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddw_zlok0zlozlo, vpaddw, vpaddw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpaddw_zhik7zhizhi, vpaddw, vpaddw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddw_zhik7zhild, vpaddw, vpaddw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddd_xlok0xloxlo, vpaddd, vpaddd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpaddd_xlok0xlold, vpaddd, vpaddd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpaddd_xlok0xlobcst, vpaddd, vpaddd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpaddd_xhik7xhixhi, vpaddd, vpaddd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddd_xhik7xhild, vpaddd, vpaddd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddd_xhik7xhibcst, vpaddd, vpaddd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpaddd_ylok0yloylo, vpaddd, vpaddd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpaddd_ylok0ylold, vpaddd, vpaddd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpaddd_ylok0ylobcst, vpaddd, vpaddd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpaddd_yhik7yhiyhi, vpaddd, vpaddd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddd_yhik7yhild, vpaddd, vpaddd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddd_yhik7yhibcst, vpaddd, vpaddd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpaddd_zlok0zlozlo, vpaddd, vpaddd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpaddd_zlok0zlold, vpaddd, vpaddd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpaddd_zlok0zlobcst, vpaddd, vpaddd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpaddd_zhik7zhizhi, vpaddd, vpaddd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddd_zhik7zhild, vpaddd, vpaddd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddd_zhik7zhibcst, vpaddd, vpaddd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpaddq_xlok0xloxlo, vpaddq, vpaddq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpaddq_xlok0xlold, vpaddq, vpaddq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpaddq_xlok0xlobcst, vpaddq, vpaddq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpaddq_xhik7xhixhi, vpaddq, vpaddq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddq_xhik7xhild, vpaddq, vpaddq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddq_xhik7xhibcst, vpaddq, vpaddq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpaddq_ylok0yloylo, vpaddq, vpaddq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpaddq_ylok0ylold, vpaddq, vpaddq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpaddq_ylok0ylobcst, vpaddq, vpaddq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpaddq_yhik7yhiyhi, vpaddq, vpaddq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddq_yhik7yhild, vpaddq, vpaddq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddq_yhik7yhibcst, vpaddq, vpaddq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpaddq_zlok0zlozlo, vpaddq, vpaddq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpaddq_zlok0zlold, vpaddq, vpaddq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpaddq_zlok0zlobcst, vpaddq, vpaddq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpaddq_zhik7zhizhi, vpaddq, vpaddq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddq_zhik7zhild, vpaddq, vpaddq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddq_zhik7zhibcst, vpaddq, vpaddq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpsubb_xlok0xloxlo, vpsubb, vpsubb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpsubb_xhik7xhixhi, vpsubb, vpsubb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubb_xhik7xhild, vpsubb, vpsubb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubb_ylok0yloylo, vpsubb, vpsubb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpsubb_yhik7yhiyhi, vpsubb, vpsubb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubb_yhik7yhild, vpsubb, vpsubb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubb_zlok0zlozlo, vpsubb, vpsubb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpsubb_zhik7zhizhi, vpsubb, vpsubb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubb_zhik7zhild, vpsubb, vpsubb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubw_xlok0xloxlo, vpsubw, vpsubw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpsubw_xhik7xhixhi, vpsubw, vpsubw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubw_xhik7xhild, vpsubw, vpsubw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubw_ylok0yloylo, vpsubw, vpsubw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpsubw_yhik7yhiyhi, vpsubw, vpsubw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubw_yhik7yhild, vpsubw, vpsubw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubw_zlok0zlozlo, vpsubw, vpsubw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpsubw_zhik7zhizhi, vpsubw, vpsubw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubw_zhik7zhild, vpsubw, vpsubw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubd_xlok0xloxlo, vpsubd, vpsubd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpsubd_xlok0xlold, vpsubd, vpsubd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpsubd_xlok0xlobcst, vpsubd, vpsubd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpsubd_xhik7xhixhi, vpsubd, vpsubd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubd_xhik7xhild, vpsubd, vpsubd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubd_xhik7xhibcst, vpsubd, vpsubd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpsubd_ylok0yloylo, vpsubd, vpsubd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpsubd_ylok0ylold, vpsubd, vpsubd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpsubd_ylok0ylobcst, vpsubd, vpsubd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpsubd_yhik7yhiyhi, vpsubd, vpsubd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubd_yhik7yhild, vpsubd, vpsubd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubd_yhik7yhibcst, vpsubd, vpsubd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpsubd_zlok0zlozlo, vpsubd, vpsubd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpsubd_zlok0zlold, vpsubd, vpsubd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpsubd_zlok0zlobcst, vpsubd, vpsubd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpsubd_zhik7zhizhi, vpsubd, vpsubd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubd_zhik7zhild, vpsubd, vpsubd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubd_zhik7zhibcst, vpsubd, vpsubd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpsubq_xlok0xloxlo, vpsubq, vpsubq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpsubq_xlok0xlold, vpsubq, vpsubq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpsubq_xlok0xlobcst, vpsubq, vpsubq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpsubq_xhik7xhixhi, vpsubq, vpsubq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubq_xhik7xhild, vpsubq, vpsubq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubq_xhik7xhibcst, vpsubq, vpsubq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpsubq_ylok0yloylo, vpsubq, vpsubq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpsubq_ylok0ylold, vpsubq, vpsubq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpsubq_ylok0ylobcst, vpsubq, vpsubq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpsubq_yhik7yhiyhi, vpsubq, vpsubq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubq_yhik7yhild, vpsubq, vpsubq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubq_yhik7yhibcst, vpsubq, vpsubq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpsubq_zlok0zlozlo, vpsubq, vpsubq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpsubq_zlok0zlold, vpsubq, vpsubq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpsubq_zlok0zlobcst, vpsubq, vpsubq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpsubq_zhik7zhizhi, vpsubq, vpsubq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubq_zhik7zhild, vpsubq, vpsubq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubq_zhik7zhibcst, vpsubq, vpsubq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpaddusb_xlok0xloxlo, vpaddusb, vpaddusb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpaddusb_xhik7xhixhi, vpaddusb, vpaddusb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddusb_xhik7xhild, vpaddusb, vpaddusb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddusb_ylok0yloylo, vpaddusb, vpaddusb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpaddusb_yhik7yhiyhi, vpaddusb, vpaddusb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddusb_yhik7yhild, vpaddusb, vpaddusb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddusb_zlok0zlozlo, vpaddusb, vpaddusb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpaddusb_zhik7zhizhi, vpaddusb, vpaddusb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddusb_zhik7zhild, vpaddusb, vpaddusb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddusw_xlok0xloxlo, vpaddusw, vpaddusw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpaddusw_xhik7xhixhi, vpaddusw, vpaddusw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddusw_xhik7xhild, vpaddusw, vpaddusw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddusw_ylok0yloylo, vpaddusw, vpaddusw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpaddusw_yhik7yhiyhi, vpaddusw, vpaddusw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddusw_yhik7yhild, vpaddusw, vpaddusw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddusw_zlok0zlozlo, vpaddusw, vpaddusw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpaddusw_zhik7zhizhi, vpaddusw, vpaddusw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddusw_zhik7zhild, vpaddusw, vpaddusw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddsb_xlok0xloxlo, vpaddsb, vpaddsb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpaddsb_xhik7xhixhi, vpaddsb, vpaddsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddsb_xhik7xhild, vpaddsb, vpaddsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddsb_ylok0yloylo, vpaddsb, vpaddsb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpaddsb_yhik7yhiyhi, vpaddsb, vpaddsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddsb_yhik7yhild, vpaddsb, vpaddsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddsb_zlok0zlozlo, vpaddsb, vpaddsb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpaddsb_zhik7zhizhi, vpaddsb, vpaddsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddsb_zhik7zhild, vpaddsb, vpaddsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpaddsw_xlok0xloxlo, vpaddsw, vpaddsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpaddsw_xhik7xhixhi, vpaddsw, vpaddsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpaddsw_xhik7xhild, vpaddsw, vpaddsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpaddsw_ylok0yloylo, vpaddsw, vpaddsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpaddsw_yhik7yhiyhi, vpaddsw, vpaddsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpaddsw_yhik7yhild, vpaddsw, vpaddsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpaddsw_zlok0zlozlo, vpaddsw, vpaddsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpaddsw_zhik7zhizhi, vpaddsw, vpaddsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpaddsw_zhik7zhild, vpaddsw, vpaddsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubusb_xlok0xloxlo, vpsubusb, vpsubusb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpsubusb_xhik7xhixhi, vpsubusb, vpsubusb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubusb_xhik7xhild, vpsubusb, vpsubusb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubusb_ylok0yloylo, vpsubusb, vpsubusb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpsubusb_yhik7yhiyhi, vpsubusb, vpsubusb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubusb_yhik7yhild, vpsubusb, vpsubusb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubusb_zlok0zlozlo, vpsubusb, vpsubusb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpsubusb_zhik7zhizhi, vpsubusb, vpsubusb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubusb_zhik7zhild, vpsubusb, vpsubusb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubusw_xlok0xloxlo, vpsubusw, vpsubusw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpsubusw_xhik7xhixhi, vpsubusw, vpsubusw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubusw_xhik7xhild, vpsubusw, vpsubusw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubusw_ylok0yloylo, vpsubusw, vpsubusw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpsubusw_yhik7yhiyhi, vpsubusw, vpsubusw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubusw_yhik7yhild, vpsubusw, vpsubusw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubusw_zlok0zlozlo, vpsubusw, vpsubusw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpsubusw_zhik7zhizhi, vpsubusw, vpsubusw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubusw_zhik7zhild, vpsubusw, vpsubusw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubsb_xlok0xloxlo, vpsubsb, vpsubsb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpsubsb_xhik7xhixhi, vpsubsb, vpsubsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubsb_xhik7xhild, vpsubsb, vpsubsb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubsb_ylok0yloylo, vpsubsb, vpsubsb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpsubsb_yhik7yhiyhi, vpsubsb, vpsubsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubsb_yhik7yhild, vpsubsb, vpsubsb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubsb_zlok0zlozlo, vpsubsb, vpsubsb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpsubsb_zhik7zhizhi, vpsubsb, vpsubsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubsb_zhik7zhild, vpsubsb, vpsubsb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpsubsw_xlok0xloxlo, vpsubsw, vpsubsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpsubsw_xhik7xhixhi, vpsubsw, vpsubsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpsubsw_xhik7xhild, vpsubsw, vpsubsw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpsubsw_ylok0yloylo, vpsubsw, vpsubsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpsubsw_yhik7yhiyhi, vpsubsw, vpsubsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpsubsw_yhik7yhild, vpsubsw, vpsubsw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpsubsw_zlok0zlozlo, vpsubsw, vpsubsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpsubsw_zhik7zhizhi, vpsubsw, vpsubsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpsubsw_zhik7zhild, vpsubsw, vpsubsw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaddwd_xlok0xloxlo, vpmaddwd, vpmaddwd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaddwd_xhik7xhixhi, vpmaddwd, vpmaddwd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaddwd_xhik7xhild, vpmaddwd, vpmaddwd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaddwd_ylok0yloylo, vpmaddwd, vpmaddwd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaddwd_yhik7yhiyhi, vpmaddwd, vpmaddwd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaddwd_yhik7yhild, vpmaddwd, vpmaddwd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaddwd_zlok0zlozlo, vpmaddwd, vpmaddwd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaddwd_zhik7zhizhi, vpmaddwd, vpmaddwd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaddwd_zhik7zhild, vpmaddwd, vpmaddwd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmaddubsw_xlok0xloxlo, vpmaddubsw, vpmaddubsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmaddubsw_xhik7xhixhi, vpmaddubsw, vpmaddubsw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmaddubsw_xhik7xhild, vpmaddubsw, vpmaddubsw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmaddubsw_ylok0yloylo, vpmaddubsw, vpmaddubsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmaddubsw_yhik7yhiyhi, vpmaddubsw, vpmaddubsw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmaddubsw_yhik7yhild, vpmaddubsw, vpmaddubsw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmaddubsw_zlok0zlozlo, vpmaddubsw, vpmaddubsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmaddubsw_zhik7zhizhi, vpmaddubsw, vpmaddubsw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmaddubsw_zhik7zhild, vpmaddubsw, vpmaddubsw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vmulps_xlok0xloxlo, vmulps, vmulps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vmulps_xlok0xlold, vmulps, vmulps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vmulps_xlok0xlobcst, vmulps, vmulps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vmulps_xhik7xhixhi, vmulps, vmulps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vmulps_xhik7xhild, vmulps, vmulps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vmulps_xhik7xhibcst, vmulps, vmulps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vmulps_ylok0yloylo, vmulps, vmulps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vmulps_ylok0ylold, vmulps, vmulps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmulps_ylok0ylobcst, vmulps, vmulps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vmulps_yhik7yhiyhi, vmulps, vmulps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vmulps_yhik7yhild, vmulps, vmulps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vmulps_yhik7yhibcst, vmulps, vmulps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vmulps_zlok0zlozlo, vmulps, vmulps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vmulps_zlok0zlold, vmulps, vmulps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vmulps_zlok0zlobcst, vmulps, vmulps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vmulps_zhik7zhizhi, vmulps, vmulps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vmulps_zhik7zhild, vmulps, vmulps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vmulps_zhik7zhibcst, vmulps, vmulps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vmulpd_xlok0xloxlo, vmulpd, vmulpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vmulpd_xlok0xlold, vmulpd, vmulpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vmulpd_xlok0xlobcst, vmulpd, vmulpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmulpd_xhik7xhixhi, vmulpd, vmulpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vmulpd_xhik7xhild, vmulpd, vmulpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vmulpd_xhik7xhibcst, vmulpd, vmulpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vmulpd_ylok0yloylo, vmulpd, vmulpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vmulpd_ylok0ylold, vmulpd, vmulpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmulpd_ylok0ylobcst, vmulpd, vmulpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vmulpd_yhik7yhiyhi, vmulpd, vmulpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vmulpd_yhik7yhild, vmulpd, vmulpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vmulpd_yhik7yhibcst, vmulpd, vmulpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vmulpd_zlok0zlozlo, vmulpd, vmulpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vmulpd_zlok0zlold, vmulpd, vmulpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vmulpd_zlok0zlobcst, vmulpd, vmulpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vmulpd_zhik7zhizhi, vmulpd, vmulpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vmulpd_zhik7zhild, vmulpd, vmulpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vmulpd_zhik7zhibcst, vmulpd, vmulpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vmulss_xlok0xloxlo, vmulss, vmulss_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vmulss_xhik7xhixhi, vmulss, vmulss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vmulss_xhik7xhild, vmulss, vmulss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vmulsd_xlok0xloxlo, vmulsd, vmulsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmulsd_xhik7xhixhi, vmulsd, vmulsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vmulsd_xhik7xhild, vmulsd, vmulsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmullw_xlok0xloxlo, vpmullw, vpmullw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmullw_xhik7xhixhi, vpmullw, vpmullw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmullw_xhik7xhild, vpmullw, vpmullw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmullw_ylok0yloylo, vpmullw, vpmullw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmullw_yhik7yhiyhi, vpmullw, vpmullw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmullw_yhik7yhild, vpmullw, vpmullw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmullw_zlok0zlozlo, vpmullw, vpmullw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmullw_zhik7zhizhi, vpmullw, vpmullw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmullw_zhik7zhild, vpmullw, vpmullw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmulld_xlok0xloxlo, vpmulld, vpmulld_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmulld_xlok0xlold, vpmulld, vpmulld_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmulld_xlok0xlobcst, vpmulld, vpmulld_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpmulld_xhik7xhixhi, vpmulld, vpmulld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmulld_xhik7xhild, vpmulld, vpmulld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmulld_xhik7xhibcst, vpmulld, vpmulld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vpmulld_ylok0yloylo, vpmulld, vpmulld_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmulld_ylok0ylold, vpmulld, vpmulld_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmulld_ylok0ylobcst, vpmulld, vpmulld_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpmulld_yhik7yhiyhi, vpmulld, vpmulld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmulld_yhik7yhild, vpmulld, vpmulld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmulld_yhik7yhibcst, vpmulld, vpmulld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vpmulld_zlok0zlozlo, vpmulld, vpmulld_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmulld_zlok0zlold, vpmulld, vpmulld_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmulld_zlok0zlobcst, vpmulld, vpmulld_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpmulld_zhik7zhizhi, vpmulld, vpmulld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmulld_zhik7zhild, vpmulld, vpmulld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmulld_zhik7zhibcst, vpmulld, vpmulld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vpmullq_xlok0xloxlo, vpmullq, vpmullq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmullq_xlok0xlold, vpmullq, vpmullq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmullq_xlok0xlobcst, vpmullq, vpmullq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmullq_xhik7xhixhi, vpmullq, vpmullq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmullq_xhik7xhild, vpmullq, vpmullq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmullq_xhik7xhibcst, vpmullq, vpmullq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmullq_ylok0yloylo, vpmullq, vpmullq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmullq_ylok0ylold, vpmullq, vpmullq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmullq_ylok0ylobcst, vpmullq, vpmullq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmullq_yhik7yhiyhi, vpmullq, vpmullq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmullq_yhik7yhild, vpmullq, vpmullq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmullq_yhik7yhibcst, vpmullq, vpmullq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmullq_zlok0zlozlo, vpmullq, vpmullq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmullq_zlok0zlold, vpmullq, vpmullq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmullq_zlok0zlobcst, vpmullq, vpmullq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmullq_zhik7zhizhi, vpmullq, vpmullq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmullq_zhik7zhild, vpmullq, vpmullq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmullq_zhik7zhibcst, vpmullq, vpmullq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpmuldq_xlok0xloxlo, vpmuldq, vpmuldq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmuldq_xlok0xlold, vpmuldq, vpmuldq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmuldq_xlok0xlobcst, vpmuldq, vpmuldq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmuldq_xhik7xhixhi, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmuldq_xhik7xhild, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmuldq_xhik7xhibcst, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmuldq_ylok0yloylo, vpmuldq, vpmuldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmuldq_ylok0ylold, vpmuldq, vpmuldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmuldq_ylok0ylobcst, vpmuldq, vpmuldq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmuldq_yhik7yhiyhi, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmuldq_yhik7yhild, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmuldq_yhik7yhibcst, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmuldq_zlok0zlozlo, vpmuldq, vpmuldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmuldq_zlok0zlold, vpmuldq, vpmuldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmuldq_zlok0zlobcst, vpmuldq, vpmuldq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmuldq_zhik7zhizhi, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmuldq_zhik7zhild, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmuldq_zhik7zhibcst, vpmuldq, vpmuldq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpmulhw_xlok0xloxlo, vpmulhw, vpmulhw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmulhw_xhik7xhixhi, vpmulhw, vpmulhw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmulhw_xhik7xhild, vpmulhw, vpmulhw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmulhw_ylok0yloylo, vpmulhw, vpmulhw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmulhw_yhik7yhiyhi, vpmulhw, vpmulhw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmulhw_yhik7yhild, vpmulhw, vpmulhw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmulhw_zlok0zlozlo, vpmulhw, vpmulhw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmulhw_zhik7zhizhi, vpmulhw, vpmulhw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmulhw_zhik7zhild, vpmulhw, vpmulhw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmulhuw_xlok0xloxlo, vpmulhuw, vpmulhuw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmulhuw_xhik7xhixhi, vpmulhuw, vpmulhuw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmulhuw_xhik7xhild, vpmulhuw, vpmulhuw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmulhuw_ylok0yloylo, vpmulhuw, vpmulhuw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmulhuw_yhik7yhiyhi, vpmulhuw, vpmulhuw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmulhuw_yhik7yhild, vpmulhuw, vpmulhuw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmulhuw_zlok0zlozlo, vpmulhuw, vpmulhuw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmulhuw_zhik7zhizhi, vpmulhuw, vpmulhuw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmulhuw_zhik7zhild, vpmulhuw, vpmulhuw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmuludq_xlok0xloxlo, vpmuludq, vpmuludq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmuludq_xlok0xlold, vpmuludq, vpmuludq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmuludq_xlok0xlobcst, vpmuludq, vpmuludq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmuludq_xhik7xhixhi, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmuludq_xhik7xhild, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmuludq_xhik7xhibcst, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmuludq_ylok0yloylo, vpmuludq, vpmuludq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmuludq_ylok0ylold, vpmuludq, vpmuludq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmuludq_ylok0ylobcst, vpmuludq, vpmuludq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmuludq_yhik7yhiyhi, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmuludq_yhik7yhild, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmuludq_yhik7yhibcst, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmuludq_zlok0zlozlo, vpmuludq, vpmuludq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmuludq_zlok0zlold, vpmuludq, vpmuludq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmuludq_zlok0zlobcst, vpmuludq, vpmuludq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmuludq_zhik7zhizhi, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmuludq_zhik7zhild, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmuludq_zhik7zhibcst, vpmuludq, vpmuludq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpmulhrsw_xlok0xloxlo, vpmulhrsw, vpmulhrsw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmulhrsw_xhik7xhixhi, vpmulhrsw, vpmulhrsw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmulhrsw_xhik7xhild, vpmulhrsw, vpmulhrsw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmulhrsw_ylok0yloylo, vpmulhrsw, vpmulhrsw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmulhrsw_yhik7yhiyhi, vpmulhrsw, vpmulhrsw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmulhrsw_yhik7yhild, vpmulhrsw, vpmulhrsw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmulhrsw_zlok0zlozlo, vpmulhrsw, vpmulhrsw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmulhrsw_zhik7zhizhi, vpmulhrsw, vpmulhrsw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmulhrsw_zhik7zhild, vpmulhrsw, vpmulhrsw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vdivps_xlok0xloxlo, vdivps, vdivps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vdivps_xlok0xlold, vdivps, vdivps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vdivps_xlok0xlobcst, vdivps, vdivps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vdivps_xhik7xhixhi, vdivps, vdivps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vdivps_xhik7xhild, vdivps, vdivps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vdivps_xhik7xhibcst, vdivps, vdivps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vdivps_ylok0yloylo, vdivps, vdivps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vdivps_ylok0ylold, vdivps, vdivps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vdivps_ylok0ylobcst, vdivps, vdivps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vdivps_yhik7yhiyhi, vdivps, vdivps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vdivps_yhik7yhild, vdivps, vdivps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vdivps_yhik7yhibcst, vdivps, vdivps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vdivps_zlok0zlozlo, vdivps, vdivps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vdivps_zlok0zlold, vdivps, vdivps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vdivps_zlok0zlobcst, vdivps, vdivps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vdivps_zhik7zhizhi, vdivps, vdivps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vdivps_zhik7zhild, vdivps, vdivps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vdivps_zhik7zhibcst, vdivps, vdivps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vdivpd_xlok0xloxlo, vdivpd, vdivpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vdivpd_xlok0xlold, vdivpd, vdivpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vdivpd_xlok0xlobcst, vdivpd, vdivpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vdivpd_xhik7xhixhi, vdivpd, vdivpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vdivpd_xhik7xhild, vdivpd, vdivpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vdivpd_xhik7xhibcst, vdivpd, vdivpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vdivpd_ylok0yloylo, vdivpd, vdivpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vdivpd_ylok0ylold, vdivpd, vdivpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vdivpd_ylok0ylobcst, vdivpd, vdivpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vdivpd_yhik7yhiyhi, vdivpd, vdivpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vdivpd_yhik7yhild, vdivpd, vdivpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vdivpd_yhik7yhibcst, vdivpd, vdivpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vdivpd_zlok0zlozlo, vdivpd, vdivpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vdivpd_zlok0zlold, vdivpd, vdivpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vdivpd_zlok0zlobcst, vdivpd, vdivpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vdivpd_zhik7zhizhi, vdivpd, vdivpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vdivpd_zhik7zhild, vdivpd, vdivpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vdivpd_zhik7zhibcst, vdivpd, vdivpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vdivss_xlok0xloxlo, vdivss, vdivss_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vdivss_xhik7xhixhi, vdivss, vdivss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vdivss_xhik7xhild, vdivss, vdivss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vdivsd_xlok0xloxlo, vdivsd, vdivsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vdivsd_xhik7xhixhi, vdivsd, vdivsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vdivsd_xhik7xhild, vdivsd, vdivsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vminps_xlok0xloxlo, vminps, vminps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vminps_xlok0xlold, vminps, vminps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vminps_xlok0xlobcst, vminps, vminps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vminps_xhik7xhixhi, vminps, vminps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vminps_xhik7xhild, vminps, vminps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vminps_xhik7xhibcst, vminps, vminps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vminps_ylok0yloylo, vminps, vminps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vminps_ylok0ylold, vminps, vminps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vminps_ylok0ylobcst, vminps, vminps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vminps_yhik7yhiyhi, vminps, vminps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vminps_yhik7yhild, vminps, vminps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vminps_yhik7yhibcst, vminps, vminps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vminps_zlok0zlozlo, vminps, vminps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vminps_zlok0zlold, vminps, vminps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vminps_zlok0zlobcst, vminps, vminps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vminps_zhik7zhizhi, vminps, vminps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vminps_zhik7zhild, vminps, vminps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vminps_zhik7zhibcst, vminps, vminps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vminpd_xlok0xloxlo, vminpd, vminpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vminpd_xlok0xlold, vminpd, vminpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vminpd_xlok0xlobcst, vminpd, vminpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vminpd_xhik7xhixhi, vminpd, vminpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vminpd_xhik7xhild, vminpd, vminpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vminpd_xhik7xhibcst, vminpd, vminpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vminpd_ylok0yloylo, vminpd, vminpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vminpd_ylok0ylold, vminpd, vminpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vminpd_ylok0ylobcst, vminpd, vminpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vminpd_yhik7yhiyhi, vminpd, vminpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vminpd_yhik7yhild, vminpd, vminpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vminpd_yhik7yhibcst, vminpd, vminpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vminpd_zlok0zlozlo, vminpd, vminpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vminpd_zlok0zlold, vminpd, vminpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vminpd_zlok0zlobcst, vminpd, vminpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vminpd_zhik7zhizhi, vminpd, vminpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vminpd_zhik7zhild, vminpd, vminpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vminpd_zhik7zhibcst, vminpd, vminpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vminss_xlok0xloxlo, vminss, vminss_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vminss_xhik7xhixhi, vminss, vminss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vminss_xhik7xhild, vminss, vminss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vminsd_xlok0xloxlo, vminsd, vminsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vminsd_xhik7xhixhi, vminsd, vminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vminsd_xhik7xhild, vminsd, vminsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vmaxps_xlok0xloxlo, vmaxps, vmaxps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vmaxps_xlok0xlold, vmaxps, vmaxps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vmaxps_xlok0xlobcst, vmaxps, vmaxps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vmaxps_xhik7xhixhi, vmaxps, vmaxps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vmaxps_xhik7xhild, vmaxps, vmaxps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vmaxps_xhik7xhibcst, vmaxps, vmaxps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vmaxps_ylok0yloylo, vmaxps, vmaxps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vmaxps_ylok0ylold, vmaxps, vmaxps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmaxps_ylok0ylobcst, vmaxps, vmaxps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vmaxps_yhik7yhiyhi, vmaxps, vmaxps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vmaxps_yhik7yhild, vmaxps, vmaxps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vmaxps_yhik7yhibcst, vmaxps, vmaxps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vmaxps_zlok0zlozlo, vmaxps, vmaxps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vmaxps_zlok0zlold, vmaxps, vmaxps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vmaxps_zlok0zlobcst, vmaxps, vmaxps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vmaxps_zhik7zhizhi, vmaxps, vmaxps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vmaxps_zhik7zhild, vmaxps, vmaxps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vmaxps_zhik7zhibcst, vmaxps, vmaxps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vmaxpd_xlok0xloxlo, vmaxpd, vmaxpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vmaxpd_xlok0xlold, vmaxpd, vmaxpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vmaxpd_xlok0xlobcst, vmaxpd, vmaxpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmaxpd_xhik7xhixhi, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vmaxpd_xhik7xhild, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vmaxpd_xhik7xhibcst, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vmaxpd_ylok0yloylo, vmaxpd, vmaxpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vmaxpd_ylok0ylold, vmaxpd, vmaxpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmaxpd_ylok0ylobcst, vmaxpd, vmaxpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vmaxpd_yhik7yhiyhi, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vmaxpd_yhik7yhild, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vmaxpd_yhik7yhibcst, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vmaxpd_zlok0zlozlo, vmaxpd, vmaxpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vmaxpd_zlok0zlold, vmaxpd, vmaxpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vmaxpd_zlok0zlobcst, vmaxpd, vmaxpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vmaxpd_zhik7zhizhi, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vmaxpd_zhik7zhild, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vmaxpd_zhik7zhibcst, vmaxpd, vmaxpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vmaxss_xlok0xloxlo, vmaxss, vmaxss_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vmaxss_xhik7xhixhi, vmaxss, vmaxss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vmaxss_xhik7xhild, vmaxss, vmaxss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vmaxsd_xlok0xloxlo, vmaxsd, vmaxsd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmaxsd_xhik7xhixhi, vmaxsd, vmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vmaxsd_xhik7xhild, vmaxsd, vmaxsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vcvtss2sd_xlok0ld, vcvtss2sd, vcvtss2sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_4))
OPCODE(vcvtss2sd_xhik7ld, vcvtss2sd, vcvtss2sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_4))
OPCODE(vcvtss2sd_xlok0xlo, vcvtss2sd, vcvtss2sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vcvtss2sd_xhik7xhi, vcvtss2sd, vcvtss2sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vcvtsd2ss_xlok0ld, vcvtsd2ss, vcvtsd2ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_8))
OPCODE(vcvtsd2ss_xhik7ld, vcvtsd2ss, vcvtsd2ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_8))
OPCODE(vcvtsd2ss_xlok0xlo, vcvtsd2ss, vcvtsd2ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vcvtsd2ss_xhik7xhi, vcvtsd2ss, vcvtsd2ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM16, OPSZ_12), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vcvtps2ph_xlok0ld, vcvtps2ph, vcvtps2ph_mask, 0, MEMARG(OPSZ_8), REGARG(K0),
       REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_xlok0xlo, vcvtps2ph, vcvtps2ph_mask, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG(K0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_xhik7xhi, vcvtps2ph, vcvtps2ph_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_ylok0ld, vcvtps2ph, vcvtps2ph_mask, 0, MEMARG(OPSZ_16), REGARG(K0),
       REGARG(YMM0), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_ylok0ylo, vcvtps2ph, vcvtps2ph_mask, 0, REGARG_PARTIAL(YMM0, OPSZ_16),
       REGARG(K0), REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_yhik7yhi, vcvtps2ph, vcvtps2ph_mask, X64_ONLY,
       REGARG_PARTIAL(YMM16, OPSZ_16), REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_zlok0ld, vcvtps2ph, vcvtps2ph_mask, 0, MEMARG(OPSZ_32), REGARG(K0),
       REGARG(ZMM0), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_zlok0zlo, vcvtps2ph, vcvtps2ph_mask, 0, REGARG_PARTIAL(ZMM0, OPSZ_32),
       REGARG(K0), REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_zhik7zhi, vcvtps2ph, vcvtps2ph_mask, X64_ONLY,
       REGARG_PARTIAL(ZMM16, OPSZ_32), REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vfmadd132ps_xlok0xloxlo, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd132ps_ylok0yloylo, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd132ps_zlok0zlozlo, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd132ps_xhik7xhixhi, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd132ps_yhik7yhiyhi, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd132ps_zhik7zhizhi, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd132ps_xlok0xlom, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd132ps_ylok0ylom, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd132ps_zlok0zlom, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd132ps_xhik0xhim, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd132ps_yhik0yhim, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd132ps_zhik0zhim, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd132ps_xlok0xlobcst, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd132ps_ylok0ylobcst, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd132ps_zlok0zlobcst, vfmadd132ps, vfmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd132ps_xhik0xhibcst, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd132ps_yhik0yhibcst, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd132ps_zhik0zhibcst, vfmadd132ps, vfmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd132pd_xlok0xloxlo, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd132pd_ylok0yloylo, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd132pd_zlok0zlozlo, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd132pd_xhik7xhixhi, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd132pd_yhik7yhiyhi, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd132pd_zhik7zhizhi, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd132pd_xlok0xlom, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd132pd_ylok0ylom, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd132pd_zlok0zlom, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd132pd_xhik0xhim, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd132pd_yhik0yhim, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd132pd_zhik0zhim, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd132pd_xlok0xlobcst, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd132pd_ylok0ylobcst, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd132pd_zlok0zlobcst, vfmadd132pd, vfmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd132pd_xhik0xhibcst, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd132pd_yhik0yhibcst, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd132pd_zhik0zhibcst, vfmadd132pd, vfmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd213ps_xlok0xloxlo, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd213ps_ylok0yloylo, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd213ps_zlok0zlozlo, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd213ps_xhik7xhixhi, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd213ps_yhik7yhiyhi, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd213ps_zhik7zhizhi, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd213ps_xlok0xlom, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd213ps_ylok0ylom, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd213ps_zlok0zlom, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd213ps_xhik0xhim, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd213ps_yhik0yhim, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd213ps_zhik0zhim, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd213ps_xlok0xlobcst, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd213ps_ylok0ylobcst, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd213ps_zlok0zlobcst, vfmadd213ps, vfmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd213ps_xhik0xhibcst, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd213ps_yhik0yhibcst, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd213ps_zhik0zhibcst, vfmadd213ps, vfmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd213pd_xlok0xloxlo, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd213pd_ylok0yloylo, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd213pd_zlok0zlozlo, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd213pd_xhik7xhixhi, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd213pd_yhik7yhiyhi, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd213pd_zhik7zhizhi, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd213pd_xlok0xlom, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd213pd_ylok0ylom, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd213pd_zlok0zlom, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd213pd_xhik0xhim, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd213pd_yhik0yhim, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd213pd_zhik0zhim, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd213pd_xlok0xlobcst, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd213pd_ylok0ylobcst, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd213pd_zlok0zlobcst, vfmadd213pd, vfmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd213pd_xhik0xhibcst, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd213pd_yhik0yhibcst, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd213pd_zhik0zhibcst, vfmadd213pd, vfmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd231ps_xlok0xloxlo, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd231ps_ylok0yloylo, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd231ps_zlok0zlozlo, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd231ps_xhik7xhixhi, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd231ps_yhik7yhiyhi, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd231ps_zhik7zhizhi, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd231ps_xlok0xlom, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd231ps_ylok0ylom, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd231ps_zlok0zlom, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd231ps_xhik0xhim, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd231ps_yhik0yhim, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd231ps_zhik0zhim, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd231ps_xlok0xlobcst, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_ylok0ylobcst, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_zlok0zlobcst, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_xhik0xhibcst, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_yhik0yhibcst, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_zhik0zhibcst, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd231pd_xlok0xloxlo, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd231pd_ylok0yloylo, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd231pd_zlok0zlozlo, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd231pd_xhik7xhixhi, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd231pd_yhik7yhiyhi, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd231pd_zhik7zhizhi, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd231pd_xlok0xlom, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd231pd_ylok0ylom, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd231pd_zlok0zlom, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd231pd_xhik0xhim, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd231pd_yhik0yhim, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd231pd_zhik0zhim, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd231pd_xlok0xlobcst, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_ylok0ylobcst, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_zlok0zlobcst, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_xhik0xhibcst, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_yhik0yhibcst, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_zhik0zhibcst, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd132ss_xlok0xloxlo, vfmadd132ss, vfmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmadd132ss_xhik7xhixhi, vfmadd132ss, vfmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmadd132ss_xlok0xlom, vfmadd132ss, vfmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd132ss_xhik0xhim, vfmadd132ss, vfmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd132sd_xlok0xloxlo, vfmadd132sd, vfmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmadd132sd_xhik7xhixhi, vfmadd132sd, vfmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmadd132sd_xlok0xlom, vfmadd132sd, vfmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd132sd_xhik0xhim, vfmadd132sd, vfmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd213ss_xlok0xloxlo, vfmadd213ss, vfmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmadd213ss_xhik7xhixhi, vfmadd213ss, vfmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmadd213ss_xlok0xlom, vfmadd213ss, vfmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd213ss_xhik0xhim, vfmadd213ss, vfmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd213sd_xlok0xloxlo, vfmadd213sd, vfmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmadd213sd_xhik7xhixhi, vfmadd213sd, vfmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmadd213sd_xlok0xlom, vfmadd213sd, vfmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd213sd_xhik0xhim, vfmadd213sd, vfmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd231ss_xlok0xloxlo, vfmadd231ss, vfmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmadd231ss_xhik7xhixhi, vfmadd231ss, vfmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmadd231ss_xlok0xlom, vfmadd231ss, vfmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd231ss_xhik0xhim, vfmadd231ss, vfmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd231sd_xlok0xloxlo, vfmadd231sd, vfmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmadd231sd_xhik7xhixhi, vfmadd231sd, vfmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmadd231sd_xlok0xlom, vfmadd231sd, vfmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd231sd_xhik0xhim, vfmadd231sd, vfmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmaddsub132ps_xlok0xloxlo, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub132ps_ylok0yloylo, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub132ps_zlok0zlozlo, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub132ps_xhik7xhixhi, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub132ps_yhik7yhiyhi, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub132ps_zhik7zhizhi, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub132ps_xlok0xlom, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132ps_ylok0ylom, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132ps_zlok0zlom, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132ps_xhik7xhim, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132ps_yhik7yhim, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132ps_zhik7zhim, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132ps_xlok0xlobcst, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_ylok0ylobcst, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_zlok0zlobcst, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_xhik7xhibcst, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_yhik7yhibcst, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_zhik7zhibcst, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_xlok0xloxlo, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub213ps_ylok0yloylo, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub213ps_zlok0zlozlo, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub213ps_xhik7xhixhi, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub213ps_yhik7yhiyhi, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub213ps_zhik7zhizhi, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub213ps_xlok0xlom, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213ps_ylok0ylom, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213ps_zlok0zlom, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213ps_xhik7xhim, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213ps_yhik7yhim, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213ps_zhik7zhim, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213ps_xlok0xlobcst, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_ylok0ylobcst, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_zlok0zlobcst, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_xhik7xhibcst, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_yhik7yhibcst, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_zhik7zhibcst, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_xlok0xloxlo, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub231ps_ylok0yloylo, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub231ps_zlok0zlozlo, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub231ps_xhik7xhixhi, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub231ps_yhik7yhiyhi, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub231ps_zhik7zhizhi, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub231ps_xlok0xlom, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231ps_ylok0ylom, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231ps_zlok0zlom, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231ps_xhik7xhim, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231ps_yhik7yhim, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231ps_zhik7zhim, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231ps_xlok0xlobcst, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_ylok0ylobcst, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_zlok0zlobcst, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_xhik7xhibcst, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_yhik7yhibcst, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_zhik7zhibcst, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132pd_xlok0xloxlo, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub132pd_ylok0yloylo, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub132pd_zlok0zlozlo, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub132pd_xhik7xhixhi, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub132pd_yhik7yhiyhi, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub132pd_zhik7zhizhi, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub132pd_xlok0xlom, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132pd_ylok0ylom, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132pd_zlok0zlom, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132pd_xhik0xhim, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132pd_yhik0yhim, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132pd_zhik0zhim, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132pd_xlok0xlobcst, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_ylok0ylobcst, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_zlok0zlobcst, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_xhik0xhibcst, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_yhik0yhibcst, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_zhik0zhibcst, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_xlok0xloxlo, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub213pd_ylok0yloylo, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub213pd_zlok0zlozlo, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub213pd_xhik7xhixhi, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub213pd_yhik7yhiyhi, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub213pd_zhik7zhizhi, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub213pd_xlok0xlom, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213pd_ylok0ylom, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213pd_zlok0zlom, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213pd_xhik7xhim, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213pd_yhik7yhim, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213pd_zhik7zhim, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213pd_xlok0xlobcst, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_ylok0ylobcst, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_zlok0zlobcst, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_xhik7xhibcst, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_yhik7yhibcst, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_zhik7zhibcst, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_xlok0xloxlo, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub231pd_ylok0yloylo, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub231pd_zlok0zlozlo, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub231pd_xhik7xhixhi, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub231pd_yhik7yhiyhi, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub231pd_zhik7zhizhi, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub231pd_xlok0xlom, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231pd_ylok0ylom, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231pd_zlok0zlom, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231pd_xhik7xhim, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231pd_yhik7yhim, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231pd_zhik7zhim, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231pd_xlok0xlobcst, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_ylok0ylobcst, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_zlok0zlobcst, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_xhik7xhibcst, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_yhik7yhibcst, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_zhik7zhibcst, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132ps_xlok0xloxlo, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd132ps_ylok0yloylo, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd132ps_zlok0zlozlo, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd132ps_xhik7xhixhi, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd132ps_yhik7yhiyhi, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd132ps_zhik7zhizhi, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd132ps_xlok0xlom, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132ps_ylok0ylom, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132ps_zlok0zlom, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132ps_xhik7xhim, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132ps_yhik7yhim, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132ps_zhik7zhim, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132ps_xlok0xlobcst, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_ylok0ylobcst, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_zlok0zlobcst, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_xhik7xhibcst, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_yhik7yhibcst, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_zhik7zhibcst, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_xlok0xloxlo, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd213ps_ylok0yloylo, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd213ps_zlok0zlozlo, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd213ps_xhik7xhixhi, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd213ps_yhik7yhiyhi, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd213ps_zhik7zhizhi, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd213ps_xlok0xlom, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213ps_ylok0ylom, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213ps_zlok0zlom, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213ps_xhik7xhim, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213ps_yhik7yhim, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213ps_zhik7zhim, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213ps_xlok0xlobcst, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_ylok0ylobcst, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_zlok0zlobcst, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_xhik7xhibcst, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_yhik7yhibcst, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_zhik7zhibcst, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_xlok0xloxlo, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd231ps_ylok0yloylo, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd231ps_zlok0zlozlo, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd231ps_xhik7xhixhi, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd231ps_yhik7yhiyhi, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd231ps_zhik7zhizhi, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd231ps_xlok0xlom, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231ps_ylok0ylom, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231ps_zlok0zlom, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231ps_xhik0xhim, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231ps_yhik0yhim, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231ps_zhik0zhim, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231ps_xlok0xlobcst, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_ylok0ylobcst, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_zlok0zlobcst, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_xhik0xhibcst, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_yhik0yhibcst, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_zhik0zhibcst, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132pd_xlok0xloxlo, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd132pd_ylok0yloylo, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd132pd_zlok0zlozlo, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd132pd_xhik7xhixhi, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd132pd_yhik7yhiyhi, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd132pd_zhik7zhizhi, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd132pd_xlok0xlom, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132pd_ylok0ylom, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132pd_zlok0zlom, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132pd_xhik7xhim, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132pd_yhik7yhim, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132pd_zhik7zhim, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132pd_xlok0xlobcst, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_ylok0ylobcst, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_zlok0zlobcst, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_xhik7xhibcst, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_yhik7yhibcst, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_zhik7zhibcst, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_xlok0xloxlo, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd213pd_ylok0yloylo, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd213pd_zlok0zlozlo, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd213pd_xhik7xhixhi, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd213pd_yhik7yhiyhi, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd213pd_zhik7zhizhi, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd213pd_xlok0xlom, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213pd_ylok0ylom, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213pd_zlok0zlom, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213pd_xhik7xhim, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213pd_yhik7yhim, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213pd_zhik7zhim, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213pd_xlok0xlobcst, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_ylok0ylobcst, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_zlok0zlobcst, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_xhik7xhibcst, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_yhik7yhibcst, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_zhik7zhibcst, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_xlok0xloxlo, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd231pd_ylok0yloylo, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd231pd_zlok0zlozlo, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd231pd_xhik7xhixhi, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd231pd_yhik7yhiyhi, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd231pd_zhik7zhizhi, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd231pd_xlok0xlom, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231pd_ylok0ylom, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231pd_zlok0zlom, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231pd_xhik7xhim, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231pd_yhik7yhim, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231pd_zhik7zhim, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231pd_xlok0xlobcst, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_ylok0ylobcst, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_zlok0zlobcst, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_xhik7xhibcst, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_yhik7yhibcst, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_zhik7zhibcst, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132ps_xlok0xloxlo, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub132ps_ylok0yloylo, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub132ps_zlok0zlozlo, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub132ps_xhik7xhixhi, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub132ps_yhik7yhiyhi, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub132ps_zhik7zhizhi, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub132ps_xlok0xlom, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub132ps_ylok0ylom, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub132ps_zlok0zlom, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub132ps_xhik7xhim, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub132ps_yhik7yhim, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub132ps_zhik7zhim, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub132ps_xlok0xlobcst, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_ylok0ylobcst, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_zlok0zlobcst, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_xhik7xhibcst, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_yhik7yhibcst, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_zhik7zhibcst, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_xlok0xloxlo, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub213ps_ylok0yloylo, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub213ps_zlok0zlozlo, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub213ps_xhik7xhixhi, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub213ps_yhik7yhiyhi, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub213ps_zhik7zhizhi, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub213ps_xlok0xlom, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub213ps_ylok0ylom, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub213ps_zlok0zlom, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub213ps_xhik7xhim, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub213ps_yhik7yhim, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub213ps_zhik7zhim, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub213ps_xlok0xlobcst, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_ylok0ylobcst, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_zlok0zlobcst, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_xhik7xhibcst, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_yhik7yhibcst, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_zhik7zhibcst, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_xlok0xloxlo, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub231ps_ylok0yloylo, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub231ps_zlok0zlozlo, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub231ps_xhik7xhixhi, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub231ps_yhik7yhiyhi, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub231ps_zhik7zhizhi, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub231ps_xlok0xlom, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub231ps_ylok0ylom, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub231ps_zlok0zlom, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub231ps_xhik0xhim, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub231ps_yhik0yhim, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub231ps_zhik7zhim, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub231ps_xlok0xlobcst, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_ylok0ylobcst, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_zlok0zlobcst, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_xhik0xhibcst, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_yhik0yhibcst, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_zhik7zhibcst, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub132pd_xlok0xloxlo, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub132pd_ylok0yloylo, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub132pd_zlok0zlozlo, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub132pd_xhik7xhixhi, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub132pd_yhik7yhiyhi, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub132pd_zhik7zhizhi, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub132pd_xlok0xlom, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub132pd_ylok0ylom, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub132pd_zlok0zlom, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub132pd_xhik7xhim, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub132pd_yhik7yhim, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub132pd_zhik7zhim, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub132pd_xlok0xlobcst, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_ylok0ylobcst, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_zlok0zlobcst, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_xhik7xhibcst, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_yhik7yhibcst, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_zhik7zhibcst, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_xlok0xloxlo, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub213pd_ylok0yloylo, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub213pd_zlok0zlozlo, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub213pd_xhik7xhixhi, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub213pd_yhik7yhiyhi, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub213pd_zhik7zhizhi, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub213pd_xlok0xlom, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub213pd_ylok0ylom, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub213pd_zlok0zlom, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub213pd_xhik7xhim, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub213pd_yhik7yhim, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub213pd_zhik7zhim, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub213pd_xlok0xlobcst, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_ylok0ylobcst, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_zlok0zlobcst, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_xhik7xhibcst, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_yhik7yhibcst, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_zhik7zhibcst, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_xlok0xloxlo, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub231pd_ylok0yloylo, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub231pd_zlok0zlozlo, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub231pd_xhik7xhixhi, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub231pd_yhik7yhiyhi, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub231pd_zhik7zhizhi, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub231pd_xlok0xlom, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub231pd_ylok0ylom, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub231pd_zlok0zlom, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub231pd_xhik7xhim, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub231pd_yhik7yhim, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub231pd_zhik7zhim, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub231pd_xlok0xlobcst, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_ylok0ylobcst, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_zlok0zlobcst, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_xhik7xhibcst, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_yhik7yhibcst, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_zhik7zhibcst, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132ss_xlok0xloxlo, vfmsub132ss, vfmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmsub132ss_xhik7xhixhi, vfmsub132ss, vfmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmsub132ss_xlok0xlom, vfmsub132ss, vfmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub132ss_xhik0xhim, vfmsub132ss, vfmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub132sd_xlok0xloxlo, vfmsub132sd, vfmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmsub132sd_xhik7xhixhi, vfmsub132sd, vfmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmsub132sd_xlok0xlom, vfmsub132sd, vfmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub132sd_xhik0xhim, vfmsub132sd, vfmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub213ss_xlok0xloxlo, vfmsub213ss, vfmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmsub213ss_xhik7xhixhi, vfmsub213ss, vfmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmsub213ss_xlok0xlom, vfmsub213ss, vfmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub213ss_xhik0xhim, vfmsub213ss, vfmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub213sd_xlok0xloxlo, vfmsub213sd, vfmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmsub213sd_xhik7xhixhi, vfmsub213sd, vfmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmsub213sd_xlok0xlom, vfmsub213sd, vfmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub213sd_xhik0xhim, vfmsub213sd, vfmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub231ss_xlok0xloxlo, vfmsub231ss, vfmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmsub231ss_xhik7xhixhi, vfmsub231ss, vfmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmsub231ss_xlok0xlom, vfmsub231ss, vfmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub231ss_xhik0xhim, vfmsub231ss, vfmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub231sd_xlok0xloxlo, vfmsub231sd, vfmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmsub231sd_xhik7xhixhi, vfmsub231sd, vfmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmsub231sd_xlok0xlom, vfmsub231sd, vfmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub231sd_xhik0xhim, vfmsub231sd, vfmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd132ps_xlok0xloxlo, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd132ps_ylok0yloylo, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd132ps_zlok0zlozlo, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd132ps_xhik7xhixhi, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd132ps_yhik7yhiyhi, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd132ps_zhik7zhizhi, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd132ps_xlok0xlom, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd132ps_ylok0ylom, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd132ps_zlok0zlom, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd132ps_xhik7xhim, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd132ps_yhik7yhim, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd132ps_zhik7zhim, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd132ps_xlok0xlobcst, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_ylok0ylobcst, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_zlok0zlobcst, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_xhik7xhibcst, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_yhik7yhibcst, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_zhik7zhibcst, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd132pd_xlok0xloxlo, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd132pd_ylok0yloylo, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd132pd_zlok0zlozlo, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd132pd_xhik7xhixhi, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd132pd_yhik7yhiyhi, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd132pd_zhik7zhizhi, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd132pd_xlok0xlom, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd132pd_ylok0ylom, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd132pd_zlok0zlom, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd132pd_xhik7xhim, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd132pd_yhik7yhim, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd132pd_zhik7zhim, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd132pd_xlok0xlobcst, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_ylok0ylobcst, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_zlok0zlobcst, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_xhik7xhibcst, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_yhik7yhibcst, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_zhik7zhibcst, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd213ps_xlok0xloxlo, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd213ps_ylok0yloylo, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd213ps_zlok0zlozlo, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd213ps_xhik7xhixhi, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd213ps_yhik7yhiyhi, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd213ps_zhik7zhizhi, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd213ps_xlok0xlom, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd213ps_ylok0ylom, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd213ps_zlok0zlom, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd213ps_xhik7xhim, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd213ps_yhik7yhim, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd213ps_zhik7zhim, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd213ps_xlok0xlobcst, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_ylok0ylobcst, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_zlok0zlobcst, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_xhik7xhibcst, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_yhik7yhibcst, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_zhik7zhibcst, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd213pd_xlok0xloxlo, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd213pd_ylok0yloylo, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd213pd_zlok0zlozlo, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd213pd_xhik7xhixhi, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd213pd_yhik7yhiyhi, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd213pd_zhik7zhizhi, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd213pd_xlok0xlom, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd213pd_ylok0ylom, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd213pd_zlok0zlom, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd213pd_xhik7xhim, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd213pd_yhik7yhim, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd213pd_zhik7zhim, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd213pd_xlok0xlobcst, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_ylok0ylobcst, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_zlok0zlobcst, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_xhik7xhibcst, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_yhik7yhibcst, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_zhik7zhibcst, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd231ps_xlok0xloxlo, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd231ps_ylok0yloylo, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd231ps_zlok0zlozlo, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd231ps_xhik7xhixhi, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd231ps_yhik7yhiyhi, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd231ps_zhik7zhizhi, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd231ps_xlok0xlom, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd231ps_ylok0ylom, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd231ps_zlok0zlom, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd231ps_xhik7xhim, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd231ps_yhik7yhim, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd231ps_zhik7zhim, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd231ps_xlok0xlobcst, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_ylok0ylobcst, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_zlok0zlobcst, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_xhik7xhibcst, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_yhik7yhibcst, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_zhik7zhibcst, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd231pd_xlok0xloxlo, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd231pd_ylok0yloylo, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd231pd_zlok0zlozlo, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd231pd_xhik7xhixhi, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd231pd_yhik7yhiyhi, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd231pd_zhik7zhizhi, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd231pd_xlok0xlom, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd231pd_ylok0ylom, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd231pd_zlok0zlom, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd231pd_xhik7xhim, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd231pd_yhik7yhim, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd231pd_zhik7zhim, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd231pd_xlok0xlobcst, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_ylok0ylobcst, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_zlok0zlobcst, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_xhik7xhibcst, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_yhik7yhibcst, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_zhik7zhibcst, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd132ss_xlok0xloxlo, vfnmadd132ss, vfnmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmadd132ss_xhik7xhixhi, vfnmadd132ss, vfnmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmadd132ss_xlok0xlom, vfnmadd132ss, vfnmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd132ss_xhik0xhim, vfnmadd132ss, vfnmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd132sd_xlok0xloxlo, vfnmadd132sd, vfnmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmadd132sd_xhik7xhixhi, vfnmadd132sd, vfnmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmadd132sd_xlok0xlom, vfnmadd132sd, vfnmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd132sd_xhik0xhim, vfnmadd132sd, vfnmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd213ss_xlok0xloxlo, vfnmadd213ss, vfnmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmadd213ss_xhik7xhixhi, vfnmadd213ss, vfnmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmadd213ss_xlok0xlom, vfnmadd213ss, vfnmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd213ss_xhik0xhim, vfnmadd213ss, vfnmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd213sd_xlok0xloxlo, vfnmadd213sd, vfnmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmadd213sd_xhik7xhixhi, vfnmadd213sd, vfnmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmadd213sd_xlok0xlom, vfnmadd213sd, vfnmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd213sd_xhik0xhim, vfnmadd213sd, vfnmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd231ss_xlok0xloxlo, vfnmadd231ss, vfnmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmadd231ss_xhik7xhixhi, vfnmadd231ss, vfnmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmadd231ss_xlok0xlom, vfnmadd231ss, vfnmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd231ss_xhik0xhim, vfnmadd231ss, vfnmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd231sd_xlok0xloxlo, vfnmadd231sd, vfnmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmadd231sd_xhik7xhixhi, vfnmadd231sd, vfnmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmadd231sd_xlok0xlom, vfnmadd231sd, vfnmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd231sd_xhik0xhim, vfnmadd231sd, vfnmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub132ps_xlok0xloxlo, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub132ps_ylok0yloylo, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub132ps_zlok0zlozlo, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub132ps_xhik7xhixhi, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub132ps_yhik7yhiyhi, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub132ps_zhik7zhizhi, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub132ps_xlok0xlom, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub132ps_ylok0ylom, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub132ps_zlok0zlom, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub132ps_xhik7xhim, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub132ps_yhik7yhim, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub132ps_zhik7zhim, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub132ps_xlok0xlobcst, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_ylok0ylobcst, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_zlok0zlobcst, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_xhik7xhibcst, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_yhik7yhibcst, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_zhik7zhibcst, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_xlok0xloxlo, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub213ps_ylok0yloylo, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub213ps_zlok0zlozlo, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub213ps_xhik7xhixhi, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub213ps_yhik7yhiyhi, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub213ps_zhik7zhizhi, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub213ps_xlok0xlom, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub213ps_ylok0ylom, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub213ps_zlok0zlom, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub213ps_xhik7xhim, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub213ps_yhik7yhim, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub213ps_zhik7zhim, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub213ps_xlok0xlobcst, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_ylok0ylobcst, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_zlok0zlobcst, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_xhik7xhibcst, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_yhik7yhibcst, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_zhik7zhibcst, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_xlok0xloxlo, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub231ps_ylok0yloylo, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub231ps_zlok0zlozlo, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub231ps_xhik7xhixhi, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub231ps_yhik7yhiyhi, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub231ps_zhik7zhizhi, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub231ps_xlok0xlom, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub231ps_ylok0ylom, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub231ps_zlok0zlom, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub231ps_xhik7xhim, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub231ps_yhik7yhim, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub231ps_zhik7zhim, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub231ps_xlok0xlobcst, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_ylok0ylobcst, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_zlok0zlobcst, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_xhik7xhibcst, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_yhik7yhibcst, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_zhik7zhibcst, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub132pd_xlok0xloxlo, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub132pd_ylok0yloylo, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub132pd_zlok0zlozlo, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub132pd_xhik7xhixhi, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub132pd_yhik7yhiyhi, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub132pd_zhik7zhizhi, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub132pd_xlok0xlom, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub132pd_ylok0ylom, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub132pd_zlok0zlom, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub132pd_xhik7xhim, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub132pd_yhik7yhim, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub132pd_zhik7zhim, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub132pd_xlok0xlobcst, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_ylok0ylobcst, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_zlok0zlobcst, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_xhik7xhibcst, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_yhik7yhibcst, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_zhik7zhibcst, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_xlok0xloxlo, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub213pd_ylok0yloylo, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub213pd_zlok0zlozlo, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub213pd_xhik7xhixhi, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub213pd_yhik7yhiyhi, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub213pd_zhik7zhizhi, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub213pd_xlok0xlom, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub213pd_ylok0ylom, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub213pd_zlok0zlom, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub213pd_xhik7xhim, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub213pd_yhik7yhim, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub213pd_zhik7zhim, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub213pd_xlok0xlobcst, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_ylok0ylobcst, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_zlok0zlobcst, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_xhik7xhibcst, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_yhik7yhibcst, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_zhik7zhibcst, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_xlok0xloxlo, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub231pd_ylok0yloylo, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub231pd_zlok0zlozlo, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub231pd_xhik7xhixhi, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub231pd_yhik7yhiyhi, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub231pd_zhik7zhizhi, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub231pd_xlok0xlom, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub231pd_ylok0ylom, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub231pd_zlok0zlom, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub231pd_xhik7xhim, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub231pd_yhik7yhim, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub231pd_zhik7zhim, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub231pd_xlok0xlobcst, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_ylok0ylobcst, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_zlok0zlobcst, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_xhik7xhibcst, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_yhik7yhibcst, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_zhik7zhibcst, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub132ss_xlok0xloxlo, vfnmsub132ss, vfnmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmsub132ss_xhik7xhixhi, vfnmsub132ss, vfnmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmsub132ss_xlok0xlom, vfnmsub132ss, vfnmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub132ss_xhik0xhim, vfnmsub132ss, vfnmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub132sd_xlok0xloxlo, vfnmsub132sd, vfnmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmsub132sd_xhik7xhixhi, vfnmsub132sd, vfnmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmsub132sd_xlok0xlom, vfnmsub132sd, vfnmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub132sd_xhik0xhim, vfnmsub132sd, vfnmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub213ss_xlok0xloxlo, vfnmsub213ss, vfnmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmsub213ss_xhik7xhixhi, vfnmsub213ss, vfnmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmsub213ss_xlok0xlom, vfnmsub213ss, vfnmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub213ss_xhik0xhim, vfnmsub213ss, vfnmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub213sd_xlok0xloxlo, vfnmsub213sd, vfnmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmsub213sd_xhik7xhixhi, vfnmsub213sd, vfnmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmsub213sd_xlok0xlom, vfnmsub213sd, vfnmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub213sd_xhik0xhim, vfnmsub213sd, vfnmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub231ss_xlok0xloxlo, vfnmsub231ss, vfnmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmsub231ss_xhik7xhixhi, vfnmsub231ss, vfnmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmsub231ss_xlok0xlom, vfnmsub231ss, vfnmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub231ss_xhik0xhim, vfnmsub231ss, vfnmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub231sd_xlok0xloxlo, vfnmsub231sd, vfnmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmsub231sd_xhik7xhixhi, vfnmsub231sd, vfnmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmsub231sd_xlok0xlom, vfnmsub231sd, vfnmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub231sd_xhik0xhim, vfnmsub231sd, vfnmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
