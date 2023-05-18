/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/* AVX 256-bit */
OPCODE(vunpcklps_256, vunpcklps, vunpcklps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vunpcklpd_256, vunpcklpd, vunpcklpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vunpckhps_256, vunpckhps, vunpckhps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vunpckhpd_256, vunpckhpd, vunpckhpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vandps_256, vandps, vandps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandpd_256, vandpd, vandpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandnps_256, vandnps, vandnps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandnpd_256, vandnpd, vandnpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vorps_256, vorps, vorps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vorpd_256, vorpd, vorpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vxorps_256, vxorps, vxorps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vxorpd_256, vxorpd, vxorpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddps_256, vaddps, vaddps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddpd_256, vaddpd, vaddpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmulps_256, vmulps, vmulps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmulpd_256, vmulpd, vmulpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vsubps_256, vsubps, vsubps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vsubpd_256, vsubpd, vsubpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vminps_256, vminps, vminps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vminpd_256, vminpd, vminpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vdivps_256, vdivps, vdivps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vdivpd_256, vdivpd, vdivpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmaxps_256, vmaxps, vmaxps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmaxpd_256, vmaxpd, vmaxpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhaddpd_256, vhaddpd, vhaddpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhaddps_256, vhaddps, vhaddps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhsubpd_256, vhsubpd, vhsubpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhsubps_256, vhsubps, vhsubps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddsubpd_256, vaddsubpd, vaddsubpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vaddsubps_256, vaddsubps, vaddsubps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vroundps_256, vroundps, vroundps, 0, REGARG(YMM0), REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vroundpd_256, vroundpd, vroundpd, 0, REGARG(YMM0), REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpcmpestrm, vpcmpestrm, vpcmpestrm, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcmpestri, vpcmpestri, vpcmpestri, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcmpistrm, vpcmpistrm, vpcmpistrm, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcmpistri, vpcmpistri, vpcmpistri, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_256, vcvtps2ph, vcvtps2ph, 0, MEMARG(OPSZ_16), REGARG(YMM0),
       IMMARG(OPSZ_1))
OPCODE(vmaskmovps_ld_256, vmaskmovps, vmaskmovps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmaskmovps_st_256, vmaskmovps, vmaskmovps, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       REGARG(YMM1))
OPCODE(vmaskmovpd_ld_256, vmaskmovpd, vmaskmovpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmaskmovpd_st_256, vmaskmovpd, vmaskmovpd, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       REGARG(YMM1))
OPCODE(vpermilp_256s, vpermilps, vpermilps, 0, REGARG(YMM0), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vpermilpd_256, vpermilpd, vpermilpd, 0, REGARG(YMM0), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vpermilps_noimm_256, vpermilps, vpermilps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermilpd_noimm_256, vpermilpd, vpermilpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vextractf128_m256, vextractf128, vextractf128, 0, MEMARG(OPSZ_16),
       REGARG_PARTIAL(YMM0, OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vextractf128_r256, vextractf128, vextractf128, 0, REGARG(XMM0),
       REGARG_PARTIAL(YMM0, OPSZ_16), IMMARG(OPSZ_1))

/* AVX2 256-bit */
OPCODE(vpunpcklbw_256, vpunpcklbw, vpunpcklbw, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpcklwd_256, vpunpcklwd, vpunpcklwd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpckldq_256, vpunpckldq, vpunpckldq, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpacksswb_256, vpacksswb, vpacksswb, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpcmpgtb_256, vpcmpgtb, vpcmpgtb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpcmpgtw_256, vpcmpgtw, vpcmpgtw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpcmpgtd_256, vpcmpgtd, vpcmpgtd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpackuswb_256, vpackuswb, vpackuswb, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpckhbw_256, vpunpckhbw, vpunpckhbw, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpckhwd_256, vpunpckhwd, vpunpckhwd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpckhdq_256, vpunpckhdq, vpunpckhdq, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpackssdw_256, vpackssdw, vpackssdw, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpcklqdq_256, vpunpcklqdq, vpunpcklqdq, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpunpckhqdq_256, vpunpckhqdq, vpunpckhqdq, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpcmpeqb_256, vpcmpeqb, vpcmpeqb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpcmpeqw_256, vpcmpeqw, vpcmpeqw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpcmpeqd_256, vpcmpeqd, vpcmpeqd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsrlw_256, vpsrlw, vpsrlw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsrld_256, vpsrld, vpsrld, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsrlq_256, vpsrlq, vpsrlq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddq_256, vpaddq, vpaddq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmullw_256, vpmullw, vpmullw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubusb_256, vpsubusb, vpsubusb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubusw_256, vpsubusw, vpsubusw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminub_256, vpminub, vpminub, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpand_256, vpand, vpand, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddusb_256, vpaddusb, vpaddusb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddusw_256, vpaddusw, vpaddusw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxub_256, vpmaxub, vpmaxub, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpandn_256, vpandn, vpandn, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpavgb_256, vpavgb, vpavgb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsraw_256, vpsraw, vpsraw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsrad_256, vpsrad, vpsrad, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpavgw_256, vpavgw, vpavgw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmulhuw_256, vpmulhuw, vpmulhuw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmulhw_256, vpmulhw, vpmulhw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubsb_256, vpsubsb, vpsubsb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubsw_256, vpsubsw, vpsubsw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsw_256, vpminsw, vpminsw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpor_256, vpor, vpor, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddsb_256, vpaddsb, vpaddsb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddsw_256, vpaddsw, vpaddsw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsw_256, vpmaxsw, vpmaxsw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpxor_256, vpxor, vpxor, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsllw_256, vpsllw, vpsllw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpslld_256, vpslld, vpslld, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsllq_256, vpsllq, vpsllq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmuludq_256, vpmuludq, vpmuludq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaddwd_256, vpmaddwd, vpmaddwd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsadbw_256r, vpsadbw, vpsadbw, 0, REGARG(YMM0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpsadbw_256m, vpsadbw, vpsadbw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubb_256, vpsubb, vpsubb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubw_256, vpsubw, vpsubw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubd_256, vpsubd, vpsubd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsubq_256, vpsubq, vpsubq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddb_256, vpaddb, vpaddb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddw_256, vpaddw, vpaddw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpaddd_256, vpaddd, vpaddd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vphaddw_256, vphaddw, vphaddw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vphaddd_256, vphaddd, vphaddd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vphaddsw_256, vphaddsw, vphaddsw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaddubsw_256, vpmaddubsw, vpmaddubsw, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vphsubw_256, vphsubw, vphsubw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vphsubd_256, vphsubd, vphsubd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vphsubsw_256, vphsubsw, vphsubsw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsignb_256, vpsignb, vpsignb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsignw_256, vpsignw, vpsignw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsignd_256, vpsignd, vpsignd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmulhrsw_256, vpmulhrsw, vpmulhrsw, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpmuldq_256, vpmuldq, vpmuldq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpcmpeqq_256, vpcmpeqq, vpcmpeqq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpackusdw_256, vpackusdw, vpackusdw, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpcmpgtq_256, vpcmpgtq, vpcmpgtq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsb_256, vpminsb, vpminsb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminsd_256, vpminsd, vpminsd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminuw_256, vpminuw, vpminuw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpminud_256, vpminud, vpminud, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsb_256, vpmaxsb, vpmaxsb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxsd_256, vpmaxsd, vpmaxsd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxuw_256, vpmaxuw, vpmaxuw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmaxud_256, vpmaxud, vpmaxud, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmulld_256, vpmulld, vpmulld, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))

OPCODE(vpshufhw_256, vpshufhw, vpshufhw, 0, REGARG(YMM0), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpshufd_256, vpshufd, vpshufd, 0, REGARG(YMM0), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpshuflw_256, vpshuflw, vpshuflw, 0, REGARG(YMM0), MEMARG(OPSZ_32), IMMARG(OPSZ_1))

OPCODE(vpsrldq_256, vpsrldq, vpsrldq, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpslldq_256, vpslldq, vpslldq, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrlw_b_256, vpsrlw, vpsrlw, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsraw_b_256, vpsraw, vpsraw, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsllw_b_256, vpsllw, vpsllw, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrld_b_256, vpsrld, vpsrld, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrad_b_256, vpsrad, vpsrad, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpslld_b_256, vpslld, vpslld, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrlq_b_256, vpsrlq, vpsrlq, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsllq_b_256, vpsllq, vpsllq, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))

OPCODE(vpmaskmovd_ld_256, vpmaskmovd, vpmaskmovd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpmaskmovq_ld_256, vpmaskmovq, vpmaskmovq, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpmaskmovd_st_256, vpmaskmovd, vpmaskmovd, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       REGARG(YMM1))
OPCODE(vpmaskmovq_st_256, vpmaskmovq, vpmaskmovq, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       REGARG(YMM1))
OPCODE(vpsllvd_256, vpsllvd, vpsllvd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsllvq_256, vpsllvq, vpsllvq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsrlvd_256, vpsrlvd, vpsrlvd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsrlvq_256, vpsrlvq, vpsrlvq, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))

/****************************************************************************/
/* FMA */
OPCODE(vfmadd132ps, vfmadd132ps, vfmadd132ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmadd132pd, vfmadd132pd, vfmadd132pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmadd213ps, vfmadd213ps, vfmadd213ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmadd213pd, vfmadd213pd, vfmadd213pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmadd231ps, vfmadd231ps, vfmadd231ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmadd231pd, vfmadd231pd, vfmadd231pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmadd132ss, vfmadd132ss, vfmadd132ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmadd132sd, vfmadd132sd, vfmadd132sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfmadd213ss, vfmadd213ss, vfmadd213ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmadd213sd, vfmadd213sd, vfmadd213sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfmadd231ss, vfmadd231ss, vfmadd231ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmadd231sd, vfmadd231sd, vfmadd231sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132ps, vfmaddsub132ps, vfmaddsub132ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub132pd, vfmaddsub132pd, vfmaddsub132pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub213ps, vfmaddsub213ps, vfmaddsub213ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub213pd, vfmaddsub213pd, vfmaddsub213pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub231ps, vfmaddsub231ps, vfmaddsub231ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub231pd, vfmaddsub231pd, vfmaddsub231pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd132ps, vfmsubadd132ps, vfmsubadd132ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd132pd, vfmsubadd132pd, vfmsubadd132pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd213ps, vfmsubadd213ps, vfmsubadd213ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd213pd, vfmsubadd213pd, vfmsubadd213pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd231ps, vfmsubadd231ps, vfmsubadd231ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd231pd, vfmsubadd231pd, vfmsubadd231pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub132ps, vfmsub132ps, vfmsub132ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub132pd, vfmsub132pd, vfmsub132pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub213ps, vfmsub213ps, vfmsub213ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub213pd, vfmsub213pd, vfmsub213pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub231ps, vfmsub231ps, vfmsub231ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub231pd, vfmsub231pd, vfmsub231pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfmsub132ss, vfmsub132ss, vfmsub132ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmsub132sd, vfmsub132sd, vfmsub132sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfmsub213ss, vfmsub213ss, vfmsub213ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmsub213sd, vfmsub213sd, vfmsub213sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfmsub231ss, vfmsub231ss, vfmsub231ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmsub231sd, vfmsub231sd, vfmsub231sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfnmadd132ps, vfnmadd132ps, vfnmadd132ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmadd132pd, vfnmadd132pd, vfnmadd132pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmadd213ps, vfnmadd213ps, vfnmadd213ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmadd213pd, vfnmadd213pd, vfnmadd213pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmadd231ps, vfnmadd231ps, vfnmadd231ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmadd231pd, vfnmadd231pd, vfnmadd231pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmadd132ss, vfnmadd132ss, vfnmadd132ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmadd132sd, vfnmadd132sd, vfnmadd132sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfnmadd213ss, vfnmadd213ss, vfnmadd213ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmadd213sd, vfnmadd213sd, vfnmadd213sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfnmadd231ss, vfnmadd231ss, vfnmadd231ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmadd231sd, vfnmadd231sd, vfnmadd231sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfnmsub132ps, vfnmsub132ps, vfnmsub132ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmsub132pd, vfnmsub132pd, vfnmsub132pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmsub213ps, vfnmsub213ps, vfnmsub213ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmsub213pd, vfnmsub213pd, vfnmsub213pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmsub231ps, vfnmsub231ps, vfnmsub231ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmsub231pd, vfnmsub231pd, vfnmsub231pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vfnmsub132ss, vfnmsub132ss, vfnmsub132ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmsub132sd, vfnmsub132sd, vfnmsub132sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfnmsub213ss, vfnmsub213ss, vfnmsub213ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmsub213sd, vfnmsub213sd, vfnmsub213sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfnmsub231ss, vfnmsub231ss, vfnmsub231ss, 0, REGARG_PARTIAL(XMM0, OPSZ_4),
       REGARG_PARTIAL(XMM1, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmsub231sd, vfnmsub231sd, vfnmsub231sd, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))

/* FMA 256-bit */
OPCODE(vfmadd132ps_256, vfmadd132ps, vfmadd132ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmadd132pd_256, vfmadd132pd, vfmadd132pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmadd213ps_256, vfmadd213ps, vfmadd213ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmadd213pd_256, vfmadd213pd, vfmadd213pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmadd231ps_256, vfmadd231ps, vfmadd231ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmadd231pd_256, vfmadd231pd, vfmadd231pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub132ps_256, vfmaddsub132ps, vfmaddsub132ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub132pd_256, vfmaddsub132pd, vfmaddsub132pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub213ps_256, vfmaddsub213ps, vfmaddsub213ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub213pd_256, vfmaddsub213pd, vfmaddsub213pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub231ps_256, vfmaddsub231ps, vfmaddsub231ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub231pd_256, vfmaddsub231pd, vfmaddsub231pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd132ps_256, vfmsubadd132ps, vfmsubadd132ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd132pd_256, vfmsubadd132pd, vfmsubadd132pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd213ps_256, vfmsubadd213ps, vfmsubadd213ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd213pd_256, vfmsubadd213pd, vfmsubadd213pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd231ps_256, vfmsubadd231ps, vfmsubadd231ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd231pd_256, vfmsubadd231pd, vfmsubadd231pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsub132ps_256, vfmsub132ps, vfmsub132ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsub132pd_256, vfmsub132pd, vfmsub132pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsub213ps_256, vfmsub213ps, vfmsub213ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsub213pd_256, vfmsub213pd, vfmsub213pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsub231ps_256, vfmsub231ps, vfmsub231ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfmsub231pd_256, vfmsub231pd, vfmsub231pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmadd132ps_256, vfnmadd132ps, vfnmadd132ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmadd132pd_256, vfnmadd132pd, vfnmadd132pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmadd213ps_256, vfnmadd213ps, vfnmadd213ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmadd213pd_256, vfnmadd213pd, vfnmadd213pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmadd231ps_256, vfnmadd231ps, vfnmadd231ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmadd231pd_256, vfnmadd231pd, vfnmadd231pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmsub132ps_256, vfnmsub132ps, vfnmsub132ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmsub132pd_256, vfnmsub132pd, vfnmsub132pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmsub213ps_256, vfnmsub213ps, vfnmsub213ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmsub213pd_256, vfnmsub213pd, vfnmsub213pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmsub231ps_256, vfnmsub231ps, vfnmsub231ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vfnmsub231pd_256, vfnmsub231pd, vfnmsub231pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpdpbusd_xhixhixhi, vpdpbusd, vpdpbusd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpdpbusds_xhixhixhi, vpdpbusds, vpdpbusds, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpdpwssd_xhixhixhi, vpdpwssd, vpdpwssd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpdpwssds_xhixhixhi, vpdpwssds, vpdpwssds, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
