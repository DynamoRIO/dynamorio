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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 2 sources. */
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
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpcklps_xhik7xhild, vunpcklps, vunpcklps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpcklps_ylok0ylold, vunpcklps, vunpcklps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpcklps_yhik7yhild, vunpcklps, vunpcklps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpcklps_zlok0zlold, vunpcklps, vunpcklps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpcklps_zhik7zhild, vunpcklps, vunpcklps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpcklpd_xlok0xlold, vunpcklpd, vunpcklpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpcklpd_xhik7xhild, vunpcklpd, vunpcklpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpcklpd_ylok0ylold, vunpcklpd, vunpcklpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpcklpd_yhik7yhild, vunpcklpd, vunpcklpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpcklpd_zlok0zlold, vunpcklpd, vunpcklpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpcklpd_zhik7zhild, vunpcklpd, vunpcklpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpckhps_xlok0xlold, vunpckhps, vunpckhps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpckhps_xhik7xhild, vunpckhps, vunpckhps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpckhps_ylok0ylold, vunpckhps, vunpckhps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpckhps_yhik7yhild, vunpckhps, vunpckhps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpckhps_zlok0zlold, vunpckhps, vunpckhps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpckhps_zhik7zhild, vunpckhps, vunpckhps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpckhpd_xlok0xlold, vunpckhpd, vunpckhpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpckhpd_xhik7xhild, vunpckhpd, vunpckhpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpckhpd_ylok0ylold, vunpckhpd, vunpckhpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG_PARTIAL(YMM1, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpckhpd_yhik7yhild, vunpckhpd, vunpckhpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG_PARTIAL(YMM31, OPSZ_16), MEMARG(OPSZ_16))
OPCODE(vunpckhpd_zlok0zlold, vunpckhpd, vunpckhpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG_PARTIAL(ZMM1, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vunpckhpd_zhik7zhild, vunpckhpd, vunpckhpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG_PARTIAL(ZMM31, OPSZ_32), MEMARG(OPSZ_32))
OPCODE(vandps_xlok0xloxlo, vandps, vandps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vandps_xhik7xhixhi, vandps, vandps_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandps_xhik7xhild, vandps, vandps_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandps_ylok0yloylo, vandps, vandps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vandps_yhik7yhiyhi, vandps, vandps_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandps_yhik7yhild, vandps, vandps_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandps_zlok0zlozlo, vandps, vandps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vandps_zhik7zhizhi, vandps, vandps_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandps_zhik7zhild, vandps, vandps_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandpd_xlok0xloxlo, vandpd, vandpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vandpd_xhik7xhixhi, vandpd, vandpd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandpd_xhik7xhild, vandpd, vandpd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandpd_ylok0yloylo, vandpd, vandpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vandpd_yhik7yhiyhi, vandpd, vandpd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandpd_yhik7yhild, vandpd, vandpd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandpd_zlok0zlozlo, vandpd, vandpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vandpd_zhik7zhizhi, vandpd, vandpd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandpd_zhik7zhild, vandpd, vandpd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandnps_xlok0xloxlo, vandnps, vandnps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vandnps_xhik7xhixhi, vandnps, vandnps_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandnps_xhik7xhild, vandnps, vandnps_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandnps_ylok0yloylo, vandnps, vandnps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vandnps_yhik7yhiyhi, vandnps, vandnps_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandnps_yhik7yhild, vandnps, vandnps_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandnps_zlok0zlozlo, vandnps, vandnps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vandnps_zhik7zhizhi, vandnps, vandnps_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandnps_zhik7zhild, vandnps, vandnps_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vandnpd_xlok0xloxlo, vandnpd, vandnpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vandnpd_xhik7xhixhi, vandnpd, vandnpd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vandnpd_xhik7xhild, vandnpd, vandnpd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vandnpd_ylok0yloylo, vandnpd, vandnpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vandnpd_yhik7yhiyhi, vandnpd, vandnpd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vandnpd_yhik7yhild, vandnpd, vandnpd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vandnpd_zlok0zlozlo, vandnpd, vandnpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vandnpd_zhik7zhizhi, vandnpd, vandnpd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vandnpd_zhik7zhild, vandnpd, vandnpd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vorps_xlok0xloxlo, vorps, vorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vorps_xhik7xhixhi, vorps, vorps_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       REGARG(XMM31))
OPCODE(vorps_xhik7xhild, vorps, vorps_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       MEMARG(OPSZ_16))
OPCODE(vorps_ylok0yloylo, vorps, vorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vorps_yhik7yhiyhi, vorps, vorps_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       REGARG(YMM31))
OPCODE(vorps_yhik7yhild, vorps, vorps_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       MEMARG(OPSZ_32))
OPCODE(vorps_zlok0zlozlo, vorps, vorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vorps_zhik7zhizhi, vorps, vorps_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       REGARG(ZMM31))
OPCODE(vorps_zhik7zhild, vorps, vorps_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       MEMARG(OPSZ_64))
OPCODE(vorpd_xlok0xloxlo, vorpd, vorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vorpd_xhik7xhixhi, vorpd, vorpd_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       REGARG(XMM31))
OPCODE(vorpd_xhik7xhild, vorpd, vorpd_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       MEMARG(OPSZ_16))
OPCODE(vorpd_ylok0yloylo, vorpd, vorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vorpd_yhik7yhiyhi, vorpd, vorpd_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       REGARG(YMM31))
OPCODE(vorpd_yhik7yhild, vorpd, vorpd_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       MEMARG(OPSZ_32))
OPCODE(vorpd_zlok0zlozlo, vorpd, vorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vorpd_zhik7zhizhi, vorpd, vorpd_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       REGARG(ZMM31))
OPCODE(vorpd_zhik7zhild, vorpd, vorpd_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       MEMARG(OPSZ_64))
OPCODE(vxorps_xlok0xloxlo, vxorps, vxorps_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vxorps_xhik7xhixhi, vxorps, vxorps_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vxorps_xhik7xhild, vxorps, vxorps_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vxorps_ylok0yloylo, vxorps, vxorps_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vxorps_yhik7yhiyhi, vxorps, vxorps_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vxorps_yhik7yhild, vxorps, vxorps_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vxorps_zlok0zlozlo, vxorps, vxorps_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vxorps_zhik7zhizhi, vxorps, vxorps_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vxorps_zhik7zhild, vxorps, vxorps_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vxorpd_xlok0xloxlo, vxorpd, vxorpd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vxorpd_xhik7xhixhi, vxorpd, vxorpd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vxorpd_xhik7xhild, vxorpd, vxorpd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vxorpd_ylok0yloylo, vxorpd, vxorpd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vxorpd_yhik7yhiyhi, vxorpd, vxorpd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vxorpd_yhik7yhild, vxorpd, vxorpd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vxorpd_zlok0zlozlo, vxorpd, vxorpd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vxorpd_zhik7zhizhi, vxorpd, vxorpd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vxorpd_zhik7zhild, vxorpd, vxorpd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandd_xlok0xloxlo, vpandd, vpandd_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpandd_xhik7xhixhi, vpandd, vpandd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandd_xhik7xhild, vpandd, vpandd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandd_ylok0yloylo, vpandd, vpandd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpandd_yhik7yhiyhi, vpandd, vpandd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandd_yhik7yhild, vpandd, vpandd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandd_zlok0zlozlo, vpandd, vpandd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpandd_zhik7zhizhi, vpandd, vpandd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandd_zhik7zhild, vpandd, vpandd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandq_xlok0xloxlo, vpandq, vpandq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpandq_xhik7xhixhi, vpandq, vpandq_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandq_xhik7xhild, vpandq, vpandq_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandq_ylok0yloylo, vpandq, vpandq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpandq_yhik7yhiyhi, vpandq, vpandq_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandq_yhik7yhild, vpandq, vpandq_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandq_zlok0zlozlo, vpandq, vpandq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpandq_zhik7zhizhi, vpandq, vpandq_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandq_zhik7zhild, vpandq, vpandq_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandnd_xlok0xloxlo, vpandnd, vpandnd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpandnd_xhik7xhixhi, vpandnd, vpandnd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandnd_xhik7xhild, vpandnd, vpandnd_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandnd_ylok0yloylo, vpandnd, vpandnd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpandnd_yhik7yhiyhi, vpandnd, vpandnd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandnd_yhik7yhild, vpandnd, vpandnd_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandnd_zlok0zlozlo, vpandnd, vpandnd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpandnd_zhik7zhizhi, vpandnd, vpandnd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandnd_zhik7zhild, vpandnd, vpandnd_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpandnq_xlok0xloxlo, vpandnq, vpandnq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpandnq_xhik7xhixhi, vpandnq, vpandnq_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpandnq_xhik7xhild, vpandnq, vpandnq_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpandnq_ylok0yloylo, vpandnq, vpandnq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpandnq_yhik7yhiyhi, vpandnq, vpandnq_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpandnq_yhik7yhild, vpandnq, vpandnq_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpandnq_zlok0zlozlo, vpandnq, vpandnq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpandnq_zhik7zhizhi, vpandnq, vpandnq_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpandnq_zhik7zhild, vpandnq, vpandnq_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpord_xlok0xloxlo, vpord, vpord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpord_xhik7xhixhi, vpord, vpord_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       REGARG(XMM31))
OPCODE(vpord_xhik7xhild, vpord, vpord_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       MEMARG(OPSZ_16))
OPCODE(vpord_ylok0yloylo, vpord, vpord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpord_yhik7yhiyhi, vpord, vpord_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       REGARG(YMM31))
OPCODE(vpord_yhik7yhild, vpord, vpord_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       MEMARG(OPSZ_32))
OPCODE(vpord_zlok0zlozlo, vpord, vpord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpord_zhik7zhizhi, vpord, vpord_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       REGARG(ZMM31))
OPCODE(vpord_zhik7zhild, vpord, vpord_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       MEMARG(OPSZ_64))
OPCODE(vporq_xlok0xloxlo, vporq, vporq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vporq_xhik7xhixhi, vporq, vporq_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       REGARG(XMM31))
OPCODE(vporq_xhik7xhild, vporq, vporq_mask, 0, REGARG(XMM16), REGARG(K7), REGARG(XMM17),
       MEMARG(OPSZ_16))
OPCODE(vporq_ylok0yloylo, vporq, vporq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vporq_yhik7yhiyhi, vporq, vporq_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       REGARG(YMM31))
OPCODE(vporq_yhik7yhild, vporq, vporq_mask, 0, REGARG(YMM16), REGARG(K7), REGARG(YMM17),
       MEMARG(OPSZ_32))
OPCODE(vporq_zlok0zlozlo, vporq, vporq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vporq_zhik7zhizhi, vporq, vporq_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       REGARG(ZMM31))
OPCODE(vporq_zhik7zhild, vporq, vporq_mask, 0, REGARG(ZMM16), REGARG(K7), REGARG(ZMM17),
       MEMARG(OPSZ_64))
OPCODE(vpxord_xlok0xloxlo, vpxord, vpxord_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpxord_xhik7xhixhi, vpxord, vpxord_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpxord_xhik7xhild, vpxord, vpxord_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpxord_ylok0yloylo, vpxord, vpxord_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpxord_yhik7yhiyhi, vpxord, vpxord_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpxord_yhik7yhild, vpxord, vpxord_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpxord_zlok0zlozlo, vpxord, vpxord_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpxord_zhik7zhizhi, vpxord, vpxord_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpxord_zhik7zhild, vpxord, vpxord_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpxorq_xlok0xloxlo, vpxorq, vpxorq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpxorq_xhik7xhixhi, vpxorq, vpxorq_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpxorq_xhik7xhild, vpxorq, vpxorq_mask, 0, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpxorq_ylok0yloylo, vpxorq, vpxorq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpxorq_yhik7yhiyhi, vpxorq, vpxorq_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpxorq_yhik7yhild, vpxorq, vpxorq_mask, 0, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpxorq_zlok0zlozlo, vpxorq, vpxorq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpxorq_zhik7zhizhi, vpxorq, vpxorq_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpxorq_zhik7zhild, vpxorq, vpxorq_mask, 0, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
/* TODO i#1312: Add missing instructions. */
