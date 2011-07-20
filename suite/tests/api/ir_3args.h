/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

OPCODE(cmovae, cmovae, cmovcc, 0, OP_cmovae, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(imul_imm, imul, imul_imm, 0, REGARG(EAX), MEMARG(OPSZ_4), IMMARG(OPSZ_4))

OPCODE(extrq_imm, extrq, extrq_imm, 0, REGARG(XMM0), IMMARG(OPSZ_1), IMMARG(OPSZ_1))

OPCODE(shld, shld, shld, 0, MEMARG(OPSZ_4), REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(shrd, shrd, shrd, 0, MEMARG(OPSZ_4), REGARG(EAX), IMMARG(OPSZ_1))

OPCODE(pshufw, pshufw, pshufw, 0, REGARG(MM0), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(pshufd, pshufd, pshufd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pshufhw, pshufhw, pshufhw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pshuflw, pshuflw, pshuflw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pinsrw, pinsrw, pinsrw, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(pextrw, pextrw, pextrw, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(pextrb, pextrb, pextrb, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(pextrd, pextrd, pextrd, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(extractps, extractps, extractps, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(roundps, roundps, roundps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(roundpd, roundpd, roundpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(roundss, roundss, roundss, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(roundsd, roundsd, roundsd, 0, REGARG(XMM0), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(blendps, blendps, blendps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(blendpd, blendpd, blendpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pblendw, pblendw, pblendw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pinsrb, pinsrb, pinsrb, 0, REGARG(XMM0), MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(insertps, insertps, insertps, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(pinsrd, pinsrd, pinsrd, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(shufps, shufps, shufps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(shufpd, shufpd, shufpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpps, cmpps, cmpps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpss, cmpss, cmpss, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(cmppd, cmppd, cmppd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpsd, cmpsd, cmpsd, 0, REGARG(XMM0), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(palignr, palignr, palignr, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))

OPCODE(dpps, dpps, dpps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(dppd, dppd, dppd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(mpsadbw, mpsadbw, mpsadbw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))

OPCODE(pcmpistrm, pcmpistrm, pcmpistrm, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pcmpistri, pcmpistri, pcmpistri, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pcmpestrm, pcmpestrm, pcmpestrm, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pcmpestri, pcmpestri, pcmpestri, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))

OPCODE(pclmulqdq, pclmulqdq, pclmulqdq, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(aeskeygenassist, aeskeygenassist, aeskeygenassist, 0, REGARG(XMM0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))

/****************************************************************************/
/* AVX */
OPCODE(vmovlps_NDS, vmovlps, vmovlps_NDS, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmovlpd_NDS, vmovlpd, vmovlpd_NDS, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vunpcklps, vunpcklps, vunpcklps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vunpcklpd, vunpcklpd, vunpcklpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vunpckhps, vunpckhps, vunpckhps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vunpckhpd, vunpckhpd, vunpckhpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmovhps_NDS, vmovhps, vmovhps_NDS, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmovhpd_NDS, vmovhpd, vmovhpd_NDS, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vcvtsi2ss, vcvtsi2ss, vcvtsi2ss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vcvtsi2sd, vcvtsi2sd, vcvtsi2sd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vsqrtss, vsqrtss, vsqrtss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vsqrtsd, vsqrtsd, vsqrtsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vrsqrtss, vrsqrtss, vrsqrtss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vrcpss, vrcpss, vrcpss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vandps, vandps, vandps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vandpd, vandpd, vandpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vandnps, vandnps, vandnps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vandnpd, vandnpd, vandnpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vorps, vorps, vorps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vorpd, vorpd, vorpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vxorps, vxorps, vxorps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vxorpd, vxorpd, vxorpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaddps, vaddps, vaddps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaddss, vaddss, vaddss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vaddpd, vaddpd, vaddpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaddsd, vaddsd, vaddsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmulps, vmulps, vmulps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vmulss, vmulss, vmulss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vmulpd, vmulpd, vmulpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vmulsd, vmulsd, vmulsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vcvtss2sd, vcvtss2sd, vcvtss2sd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vcvtsd2ss, vcvtsd2ss, vcvtsd2ss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vsubps, vsubps, vsubps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vsubss, vsubss, vsubss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vsubpd, vsubpd, vsubpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vsubsd, vsubsd, vsubsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vminps, vminps, vminps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vminss, vminss, vminss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vminpd, vminpd, vminpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vminsd, vminsd, vminsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vdivps, vdivps, vdivps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vdivss, vdivss, vdivss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vdivpd, vdivpd, vdivpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vdivsd, vdivsd, vdivsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vmaxps, vmaxps, vmaxps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vmaxss, vmaxss, vmaxss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vmaxpd, vmaxpd, vmaxpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vmaxsd, vmaxsd, vmaxsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpunpcklbw, vpunpcklbw, vpunpcklbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpcklwd, vpunpcklwd, vpunpcklwd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckldq, vpunpckldq, vpunpckldq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpacksswb, vpacksswb, vpacksswb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpgtb, vpcmpgtb, vpcmpgtb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpgtw, vpcmpgtw, vpcmpgtw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpgtd, vpcmpgtd, vpcmpgtd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpackuswb, vpackuswb, vpackuswb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhbw, vpunpckhbw, vpunpckhbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhwd, vpunpckhwd, vpunpckhwd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhdq, vpunpckhdq, vpunpckhdq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpackssdw, vpackssdw, vpackssdw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpcklqdq, vpunpcklqdq, vpunpcklqdq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpunpckhqdq, vpunpckhqdq, vpunpckhqdq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshufhw, vpshufhw, vpshufhw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpshufd, vpshufd, vpshufd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpshuflw, vpshuflw, vpshuflw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpcmpeqb, vpcmpeqb, vpcmpeqb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpeqw, vpcmpeqw, vpcmpeqw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpeqd, vpcmpeqd, vpcmpeqd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpextrw, vpextrw, vpextrw, 0, REGARG(EAX), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpsrlw, vpsrlw, vpsrlw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsrld, vpsrld, vpsrld, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsrlq, vpsrlq, vpsrlq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddq, vpaddq, vpaddq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmullw, vpmullw, vpmullw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubusb, vpsubusb, vpsubusb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubusw, vpsubusw, vpsubusw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminub, vpminub, vpminub, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpand, vpand, vpand, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddusb, vpaddusb, vpaddusb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddusw, vpaddusw, vpaddusw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxub, vpmaxub, vpmaxub, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpandn, vpandn, vpandn, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpavgb, vpavgb, vpavgb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsraw, vpsraw, vpsraw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsrad, vpsrad, vpsrad, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpavgw, vpavgw, vpavgw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmulhuw, vpmulhuw, vpmulhuw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmulhw, vpmulhw, vpmulhw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubsb, vpsubsb, vpsubsb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubsw, vpsubsw, vpsubsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsw, vpminsw, vpminsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpor, vpor, vpor, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddsb, vpaddsb, vpaddsb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddsw, vpaddsw, vpaddsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsw, vpmaxsw, vpmaxsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpxor, vpxor, vpxor, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsllw, vpsllw, vpsllw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpslld, vpslld, vpslld, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsllq, vpsllq, vpsllq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmuludq, vpmuludq, vpmuludq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaddwd, vpmaddwd, vpmaddwd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsadbw, vpsadbw, vpsadbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubb, vpsubb, vpsubb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubw, vpsubw, vpsubw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubd, vpsubd, vpsubd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsubq, vpsubq, vpsubq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddb, vpaddb, vpaddb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddw, vpaddw, vpaddw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpaddd, vpaddd, vpaddd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsrldq, vpsrldq, vpsrldq, 0, REGARG(XMM0), IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpslldq, vpslldq, vpslldq, 0, REGARG(XMM0), IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vhaddpd, vhaddpd, vhaddpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vhaddps, vhaddps, vhaddps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vhsubpd, vhsubpd, vhsubpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vhsubps, vhsubps, vhsubps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaddsubpd, vaddsubpd, vaddsubpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaddsubps, vaddsubps, vaddsubps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vphaddw, vphaddw, vphaddw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vphaddd, vphaddd, vphaddd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vphaddsw, vphaddsw, vphaddsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaddubsw, vpmaddubsw, vpmaddubsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vphsubw, vphsubw, vphsubw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vphsubd, vphsubd, vphsubd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vphsubsw, vphsubsw, vphsubsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsignb, vpsignb, vpsignb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsignw, vpsignw, vpsignw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsignd, vpsignd, vpsignd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmulhrsw, vpmulhrsw, vpmulhrsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpabsb, vpabsb, vpabsb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpabsw, vpabsw, vpabsw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpabsd, vpabsd, vpabsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmuldq, vpmuldq, vpmuldq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpeqq, vpcmpeqq, vpcmpeqq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpackusdw, vpackusdw, vpackusdw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpgtq, vpcmpgtq, vpcmpgtq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsb, vpminsb, vpminsb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminsd, vpminsd, vpminsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminuw, vpminuw, vpminuw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpminud, vpminud, vpminud, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsb, vpmaxsb, vpmaxsb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxsd, vpmaxsd, vpmaxsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxuw, vpmaxuw, vpmaxuw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmaxud, vpmaxud, vpmaxud, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmulld, vpmulld, vpmulld, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaesenc, vaesenc, vaesenc, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaesenclast, vaesenclast, vaesenclast, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaesdec, vaesdec, vaesdec, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaesdeclast, vaesdeclast, vaesdeclast, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpextrb, vpextrb, vpextrb, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(vpextrd, vpextrd, vpextrd, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(vextractps, vextractps, vextractps, 0, REGARG(EAX), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vroundps, vroundps, vroundps, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vroundpd, vroundpd, vroundpd, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vaeskeygenassist, vaeskeygenassist, vaeskeygenassist, 0, REGARG(XMM0), REGARG(XMM1),
       IMMARG(OPSZ_1))
OPCODE(vmovss_NDS, vmovss, vmovss_NDS, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vmovsd_NDS, vmovsd, vmovsd_NDS, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vcvtps2ph, vcvtps2ph, vcvtps2ph, 0, MEMARG(OPSZ_16), REGARG(XMM0),
       IMMARG(OPSZ_1))
OPCODE(vmaskmovps_ld, vmaskmovps, vmaskmovps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vmaskmovps_st, vmaskmovps, vmaskmovps, 0, MEMARG(OPSZ_16), REGARG(XMM0),
       REGARG(XMM1))
OPCODE(vmaskmovpd_ld, vmaskmovpd, vmaskmovpd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vmaskmovpd_st, vmaskmovpd, vmaskmovpd, 0, MEMARG(OPSZ_16), REGARG(XMM0),
       REGARG(XMM1))
OPCODE(vpermilps, vpermilps, vpermilps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd, vpermilpd, vpermilpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_noimm, vpermilps, vpermilps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpermilpd_noimm, vpermilpd, vpermilpd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vextractf128, vextractf128, vextractf128, 0, MEMARG(OPSZ_16), REGARG(XMM0),
       IMMARG(OPSZ_1))

/* AVX 256-bit */
OPCODE(vunpcklps_256, vunpcklps, vunpcklps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpcklpd_256, vunpcklpd, vunpcklpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpckhps_256, vunpckhps, vunpckhps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpckhpd_256, vunpckhpd, vunpckhpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
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
OPCODE(vaddsubpd_256, vaddsubpd, vaddsubpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddsubps_256, vaddsubps, vaddsubps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
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
OPCODE(vcvtps2ph_256, vcvtps2ph, vcvtps2ph, 0, MEMARG(OPSZ_32), REGARG(YMM0),
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
OPCODE(vextractf128_256, vextractf128, vextractf128, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       IMMARG(OPSZ_1))

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
OPCODE(vfmadd132ss, vfmadd132ss, vfmadd132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmadd132sd, vfmadd132sd, vfmadd132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmadd213ss, vfmadd213ss, vfmadd213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmadd213sd, vfmadd213sd, vfmadd213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmadd231ss, vfmadd231ss, vfmadd231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmadd231sd, vfmadd231sd, vfmadd231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
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
OPCODE(vfmsub132ss, vfmsub132ss, vfmsub132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmsub132sd, vfmsub132sd, vfmsub132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmsub213ss, vfmsub213ss, vfmsub213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmsub213sd, vfmsub213sd, vfmsub213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmsub231ss, vfmsub231ss, vfmsub231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmsub231sd, vfmsub231sd, vfmsub231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
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
OPCODE(vfnmadd132ss, vfnmadd132ss, vfnmadd132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmadd132sd, vfnmadd132sd, vfnmadd132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmadd213ss, vfnmadd213ss, vfnmadd213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmadd213sd, vfnmadd213sd, vfnmadd213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmadd231ss, vfnmadd231ss, vfnmadd231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmadd231sd, vfnmadd231sd, vfnmadd231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
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
OPCODE(vfnmsub132ss, vfnmsub132ss, vfnmsub132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmsub132sd, vfnmsub132sd, vfnmsub132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmsub213ss, vfnmsub213ss, vfnmsub213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmsub213sd, vfnmsub213sd, vfnmsub213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmsub231ss, vfnmsub231ss, vfnmsub231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmsub231sd, vfnmsub231sd, vfnmsub231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))

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
