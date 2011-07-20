/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

OPCODE(insertq_imm, insertq, insertq_imm, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1), 
       IMMARG(OPSZ_1))

/****************************************************************************/
/* AVX */
OPCODE(vpblendvb, vpblendvb, vpblendvb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vblendvps, vblendvps, vblendvps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vblendvpd, vblendvpd, vblendvpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vcmpps, vcmpps, vcmpps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vcmpss, vcmpss, vcmpss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(vcmppd, vcmppd, vcmppd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vcmpsd, vcmpsd, vcmpsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8),
       IMMARG(OPSZ_1))
OPCODE(vpinsrw, vpinsrw, vpinsrw, 0, REGARG(XMM0), REGARG(XMM1), REGARG(EAX),
       IMMARG(OPSZ_1))
OPCODE(vshufps, vshufps, vshufps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vshufpd, vshufpd, vshufpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpalignr, vpalignr, vpalignr, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vblendps, vblendps, vblendps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vblendpd, vblendpd, vblendpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpblendw, vpblendw, vpblendw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpinsrb, vpinsrb, vpinsrb, 0, REGARG(XMM0), REGARG(XMM1), REGARG(AL),
       IMMARG(OPSZ_1))
OPCODE(vinsertps, vinsertps, vinsertps, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM2),
       IMMARG(OPSZ_1))
OPCODE(vpinsrd, vpinsrd, vpinsrd, 0, REGARG(XMM0), REGARG(XMM1), REGARG(EAX),
       IMMARG(OPSZ_1))
OPCODE(vdpps, vdpps, vdpps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vdppd, vdppd, vdppd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vmpsadbw, vmpsadbw, vmpsadbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpclmulqdq, vpclmulqdq, vpclmulqdq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vroundss, vroundss, vroundss, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(vroundsd, vroundsd, vroundsd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_8),
       IMMARG(OPSZ_1))
OPCODE(vperm2f128, vperm2f128, vperm2f128, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vinsertf128, vinsertf128, vinsertf128, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))

/* AVX 256-bit */
OPCODE(vcmpps_256, vcmpps, vcmpps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vcmppd_256, vcmppd, vcmppd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vshufps_256, vshufps, vshufps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vshufpd_256, vshufpd, vshufpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vblendvps_256, vblendvps, vblendvps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       REGARG(YMM3))
OPCODE(vblendvpd_256, vblendvpd, vblendvpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       REGARG(YMM3))
OPCODE(vblendps_256, vblendps, vblendps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vblendpd_256, vblendpd, vblendpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vdpps_256, vdpps, vdpps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vperm2f128_256, vperm2f128, vperm2f128, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vinsertf128_256, vinsertf128, vinsertf128, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
