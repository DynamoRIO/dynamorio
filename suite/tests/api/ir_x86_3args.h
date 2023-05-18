/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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
OPCODE(pinsrw, pinsrw, pinsrw, 0, REGARG_PARTIAL(XMM0, OPSZ_2), REGARG(EDX),
       IMMARG(OPSZ_1))
OPCODE(pinsrw_mem, pinsrw, pinsrw, 0, REGARG_PARTIAL(XMM0, OPSZ_2), MEMARG(OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(pinsrw_mmx, pinsrw, pinsrw, 0, REGARG_PARTIAL(MM0, OPSZ_2), MEMARG(OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(pextrw, pextrw, pextrw, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(pextrw_mmx, pextrw, pextrw, 0, REGARG(EAX), REGARG_PARTIAL(MM0, OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(pextrw_2, pextrw, pextrw, 0, MEMARG(OPSZ_2), REGARG_PARTIAL(XMM0, OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(pextrb, pextrb, pextrb, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(pextrb_mem, pextrb, pextrb, 0, MEMARG(OPSZ_1), REGARG_PARTIAL(XMM0, OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(pextrd, pextrd, pextrd, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(pextrq, pextrd, pextrd, X64_ONLY, REGARG(RAX), REGARG_PARTIAL(XMM0, OPSZ_8),
       IMMARG(OPSZ_1))
OPCODE(extractps, extractps, extractps, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(roundps, roundps, roundps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(roundpd, roundpd, roundpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(roundss, roundss, roundss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(roundsd, roundsd, roundsd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8),
       IMMARG(OPSZ_1))
OPCODE(blendps, blendps, blendps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(blendpd, blendpd, blendpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pblendw, pblendw, pblendw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pinsrb, pinsrb, pinsrb, 0, REGARG_PARTIAL(XMM0, OPSZ_1), REGARG(ESP),
       IMMARG(OPSZ_1))
OPCODE(pinsrb_mem, pinsrb, pinsrb, 0, REGARG_PARTIAL(XMM0, OPSZ_1), MEMARG(OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(insertps, insertps, insertps, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(pinsrd, pinsrd, pinsrd, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(pinsrq, pinsrd, pinsrd, X64_ONLY, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8),
       IMMARG(OPSZ_1))
OPCODE(shufps, shufps, shufps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(shufpd, shufpd, shufpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpps, cmpps, cmpps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpss, cmpss, cmpss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(cmppd, cmppd, cmppd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpsd, cmpsd, cmpsd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8),
       IMMARG(OPSZ_1))
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

XOPCODE(add_2src, lea, add_2src, 0, REGARG(XAX), REGARG(XCX), IMMARG(OPSZ_4))

/****************************************************************************/
/* AVX */
OPCODE(vmovlps_NDS_mem, vmovlps, vmovlps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovlps_NDS_reg, vmovlps, vmovlps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovlpd_NDS, vmovlpd, vmovlpd_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vunpcklps, vunpcklps, vunpcklps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpcklpd, vunpcklpd, vunpcklpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpckhps, vunpckhps, vunpckhps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vunpckhpd, vunpckhpd, vunpckhpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vmovhps_NDS_mem, vmovhps, vmovhps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovhps_NDS_reg, vmovhps, vmovhps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovhpd_NDS, vmovhpd, vmovhpd_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcvtsi2ss, vcvtsi2ss, vcvtsi2ss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4))
OPCODE(vcvtsi2sd, vcvtsi2sd, vcvtsi2sd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_4))
OPCODE(vsqrtss, vsqrtss, vsqrtss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4))
OPCODE(vsqrtsd, vsqrtsd, vsqrtsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vrsqrtss, vrsqrtss, vrsqrtss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4))
OPCODE(vrcpss, vrcpss, vrcpss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4))
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
OPCODE(vcvtss2sd, vcvtss2sd, vcvtss2sd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_4))
OPCODE(vcvtsd2ss, vcvtsd2ss, vcvtsd2ss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_8))
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
OPCODE(vpunpcklqdq, vpunpcklqdq, vpunpcklqdq, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpunpckhqdq, vpunpckhqdq, vpunpckhqdq, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpshufhw, vpshufhw, vpshufhw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpshufd, vpshufd, vpshufd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpshuflw, vpshuflw, vpshuflw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpcmpeqb, vpcmpeqb, vpcmpeqb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpeqw, vpcmpeqw, vpcmpeqw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpcmpeqd, vpcmpeqd, vpcmpeqd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpextrw, vpextrw, vpextrw, 0, REGARG(EAX), REGARG_PARTIAL(XMM1, OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(vpextrw_mem, vpextrw, vpextrw, 0, MEMARG(OPSZ_2), REGARG_PARTIAL(XMM1, OPSZ_2),
       IMMARG(OPSZ_1))
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
OPCODE(vpsadbw_r, vpsadbw, vpsadbw, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpsadbw_m, vpsadbw, vpsadbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
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
OPCODE(vpshufb, vpshufb, vpshufb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
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
OPCODE(vaesenclast, vaesenclast, vaesenclast, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vaesdec, vaesdec, vaesdec, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vaesdeclast, vaesdeclast, vaesdeclast, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpextrb, vpextrb, vpextrb, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(vpextrb_mem, vpextrb, vpextrb, 0, MEMARG(OPSZ_1), REGARG_PARTIAL(XMM0, OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(vpextrd, vpextrd, vpextrd, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(vpextrd_mem, vpextrd, vpextrd, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(vpextrq, vpextrd, vpextrd, X64_ONLY, REGARG(RAX), REGARG_PARTIAL(XMM0, OPSZ_8),
       IMMARG(OPSZ_1))
OPCODE(vpextrq_mem, vpextrd, vpextrd, X64_ONLY, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM0, OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vextractps, vextractps, vextractps, 0, REGARG(EAX), IMMARG(OPSZ_1),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vroundps, vroundps, vroundps, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vroundpd, vroundpd, vroundpd, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vaeskeygenassist, vaeskeygenassist, vaeskeygenassist, 0, REGARG(XMM0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vmovss_NDS, vmovss, vmovss_NDS, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vmovsd_NDS, vmovsd, vmovsd_NDS, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vcvtps2ph, vcvtps2ph, vcvtps2ph, 0, MEMARG(OPSZ_8), REGARG(XMM0), IMMARG(OPSZ_1))
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

/****************************************************************************/
/* XOP */
OPCODE(vprotb_a, vprotb, vprotb, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vprotb_b, vprotb, vprotb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprotb_c, vprotb, vprotb, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vprotw_a, vprotw, vprotw, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vprotw_b, vprotw, vprotw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprotw_c, vprotw, vprotw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vprotd_a, vprotd, vprotd, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vprotd_b, vprotd, vprotd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprotd_c, vprotd, vprotd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vprotq_a, vprotq, vprotq, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vprotq_b, vprotq, vprotq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vprotq_c, vprotq, vprotq, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpshlb_a, vpshlb, vpshlb, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshlb_b, vpshlb, vpshlb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshld_a, vpshld, vpshld, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshld_b, vpshld, vpshld, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshlq_a, vpshlq, vpshlq, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshlq_b, vpshlq, vpshlq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshlw_a, vpshlw, vpshlw, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshlw_b, vpshlw, vpshlw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshab_a, vpshab, vpshab, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshab_b, vpshab, vpshab, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshad_a, vpshad, vpshad, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshad_b, vpshad, vpshad, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshaq_a, vpshaq, vpshaq, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshaq_b, vpshaq, vpshaq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshaw_a, vpshaw, vpshaw, 0, REGARG(XMM0), MEMARG(OPSZ_16), REGARG(XMM1))
OPCODE(vpshaw_b, vpshaw, vpshaw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))

/****************************************************************************/
/* TBM */
OPCODE(bextr_a, bextr, bextr, 0, REGARG(EDI), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(bextr_b, bextr, bextr, 0, REGARG(EDI), MEMARG(OPSZ_4), IMMARG(OPSZ_4))

/****************************************************************************/
/* LWP */
OPCODE(lwpins, lwpins, lwpins, 0, REGARG(EAX), MEMARG(OPSZ_4), IMMARG(OPSZ_4))
OPCODE(lwpval, lwpval, lwpval, 0, REGARG(EAX), MEMARG(OPSZ_4), IMMARG(OPSZ_4))

/****************************************************************************/
/* BMI1 */
OPCODE(andn, andn, andn, 0, REGARG(EAX), REGARG(EBX), MEMARG(OPSZ_4))
/* Test the 2nd byte looking like a different prefix (i#3978). */
OPCODE(andn_ext, andn, andn, X64_ONLY, REGARG(R12), REGARG(R12), REGARG(RAX))

/****************************************************************************/
/* BMI2 */
OPCODE(bzhi, bzhi, bzhi, 0, REGARG(EAX), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(pext, pext, pext, 0, REGARG(EAX), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(pdep, pdep, pdep, 0, REGARG(EAX), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(sarx, sarx, sarx, 0, REGARG(EAX), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(shlx, shlx, shlx, 0, REGARG(EAX), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(shrx, shrx, shrx, 0, REGARG(EAX), MEMARG(OPSZ_4), REGARG(EBX))
OPCODE(rorx, rorx, rorx, 0, REGARG(EAX), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(mulx_a, mulx, mulx, X64_ONLY, REGARG(RAX), REGARG(R9), MEMARG(OPSZ_8))
OPCODE(mulx, mulx, mulx, 0, REGARG(EAX), REGARG(EBX), MEMARG(OPSZ_4))

/****************************************************************************/
/* AVX2 */
OPCODE(vpgatherddidxlo, vpgatherdd, vpgatherdd, 0, REGARG(XMM0), VSIBX6(OPSZ_4),
       REGARG(XMM1))
OPCODE(vpgatherdqidxlo, vpgatherdq, vpgatherdq, 0, REGARG(XMM0), VSIBX6(OPSZ_8),
       REGARG(XMM1))
OPCODE(vpgatherqdidxlo, vpgatherqd, vpgatherqd, 0, REGARG(XMM0), VSIBX6(OPSZ_4),
       REGARG(XMM1))
OPCODE(vpgatherqqidxlo, vpgatherqq, vpgatherqq, 0, REGARG(XMM0), VSIBX6(OPSZ_8),
       REGARG(XMM1))
OPCODE(vgatherdpsidxlo, vgatherdps, vgatherdps, 0, REGARG(XMM0), VSIBX6(OPSZ_4),
       REGARG(XMM1))
OPCODE(vgatherdpdidxlo, vgatherdpd, vgatherdpd, 0, REGARG(XMM0), VSIBX6(OPSZ_8),
       REGARG(XMM1))
OPCODE(vgatherqpsidxlo, vgatherqps, vgatherqps, 0, REGARG(XMM0), VSIBX6(OPSZ_4),
       REGARG(XMM1))
OPCODE(vgatherqpdidxlo, vgatherqpd, vgatherqpd, 0, REGARG(XMM0), VSIBX6(OPSZ_8),
       REGARG(XMM1))

OPCODE(vpgatherdd_256idylo, vpgatherdd, vpgatherdd, 0, REGARG(YMM0), VSIBY6(OPSZ_4),
       REGARG(YMM1))
OPCODE(vpgatherdq_256idylo, vpgatherdq, vpgatherdq, 0, REGARG(YMM0), VSIBY6(OPSZ_8),
       REGARG(YMM1))
OPCODE(vpgatherqd_256idylo, vpgatherqd, vpgatherqd, 0, REGARG(YMM0), VSIBY6(OPSZ_4),
       REGARG(YMM1))
OPCODE(vpgatherqq_256idylo, vpgatherqq, vpgatherqq, 0, REGARG(YMM0), VSIBY6(OPSZ_8),
       REGARG(YMM1))
OPCODE(vgatherdps_256idylo, vgatherdps, vgatherdps, 0, REGARG(YMM0), VSIBY6(OPSZ_4),
       REGARG(YMM1))
OPCODE(vgatherdpd_256idylo, vgatherdpd, vgatherdpd, 0, REGARG(YMM0), VSIBY6(OPSZ_8),
       REGARG(YMM1))
OPCODE(vgatherqps_256idylo, vgatherqps, vgatherqps, 0, REGARG(YMM0), VSIBY6(OPSZ_4),
       REGARG(YMM1))
OPCODE(vgatherqpd_256idylo, vgatherqpd, vgatherqpd, 0, REGARG(YMM0), VSIBY6(OPSZ_8),
       REGARG(YMM1))

OPCODE(vpgatherddidxhi, vpgatherdd, vpgatherdd, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_4),
       REGARG(XMM1))
OPCODE(vpgatherdqidxhi, vpgatherdq, vpgatherdq, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_8),
       REGARG(XMM1))
OPCODE(vpgatherqdidxhi, vpgatherqd, vpgatherqd, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_4),
       REGARG(XMM1))
OPCODE(vpgatherqqidxhi, vpgatherqq, vpgatherqq, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_8),
       REGARG(XMM1))
OPCODE(vgatherdpsidxhi, vgatherdps, vgatherdps, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_4),
       REGARG(XMM1))
OPCODE(vgatherdpdidxhi, vgatherdpd, vgatherdpd, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_8),
       REGARG(XMM1))
OPCODE(vgatherqpsidxhi, vgatherqps, vgatherqps, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_4),
       REGARG(XMM1))
OPCODE(vgatherqpdidxhi, vgatherqpd, vgatherqpd, X64_ONLY, REGARG(XMM0), VSIBX15(OPSZ_8),
       REGARG(XMM1))

OPCODE(vpgatherdd_256idyhi, vpgatherdd, vpgatherdd, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_4), REGARG(YMM1))
OPCODE(vpgatherdq_256idyhi, vpgatherdq, vpgatherdq, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_8), REGARG(YMM1))
OPCODE(vpgatherqd_256idyhi, vpgatherqd, vpgatherqd, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_4), REGARG(YMM1))
OPCODE(vpgatherqq_256idyhi, vpgatherqq, vpgatherqq, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_8), REGARG(YMM1))
OPCODE(vgatherdps_256idyhi, vgatherdps, vgatherdps, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_4), REGARG(YMM1))
OPCODE(vgatherdpd_256idyhi, vgatherdpd, vgatherdpd, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_8), REGARG(YMM1))
OPCODE(vgatherqps_256idyhi, vgatherqps, vgatherqps, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_4), REGARG(YMM1))
OPCODE(vgatherqpd_256idyhi, vgatherqpd, vgatherqpd, X64_ONLY, REGARG(YMM0),
       VSIBY15(OPSZ_8), REGARG(YMM1))

OPCODE(vpermps, vpermps, vpermps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermd, vpermd, vpermd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpshufb_256, vpshufb, vpshufb, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsravd, vpsravd, vpsravd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsravd_256, vpsravd, vpsravd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vextracti128, vextracti128, vextracti128, 0, MEMARG(OPSZ_16), REGARG(YMM0),
       IMMARG(OPSZ_1))
OPCODE(vpermq, vpermq, vpermq, 0, REGARG(YMM0), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermpd, vpermpd, vpermpd, 0, REGARG(YMM0), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpmaskmovd_ld, vpmaskmovd, vpmaskmovd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpmaskmovq_ld, vpmaskmovq, vpmaskmovq, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpmaskmovd_st, vpmaskmovd, vpmaskmovd, 0, MEMARG(OPSZ_16), REGARG(XMM0),
       REGARG(XMM1))
OPCODE(vpmaskmovq_st, vpmaskmovq, vpmaskmovq, 0, MEMARG(OPSZ_16), REGARG(XMM0),
       REGARG(XMM1))
OPCODE(vpsllvd, vpsllvd, vpsllvd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsllvq, vpsllvq, vpsllvq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsrlvd, vpsrlvd, vpsrlvd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsrlvq, vpsrlvq, vpsrlvq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))

/* SHA */
OPCODE(sha1rnds4_xloxlo, sha1rnds4, sha1rnds4, 0, REGARG(XMM0), REGARG(XMM1),
       IMMARG(OPSZ_1))
OPCODE(sha1rnds4_xloxhi, sha1rnds4, sha1rnds4, X64_ONLY, REGARG(XMM7), REGARG(XMM15),
       IMMARG(OPSZ_1))
OPCODE(sha1rnds4_xlom, sha1rnds4, sha1rnds4, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(sha1rnds4_xhim, sha1rnds4, sha1rnds4, X64_ONLY, REGARG(XMM15), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
