/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
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

XOPCODE(add_sll, lea, add_sll, 0, REGARG(XAX), REGARG(XCX), REGARG(XDX), 3)

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
OPCODE(vpinsrw_r, vpinsrw, vpinsrw, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_14),
       REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrw_m, vpinsrw, vpinsrw, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_14),
       MEMARG(OPSZ_2), IMMARG(OPSZ_1))
OPCODE(vshufps, vshufps, vshufps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vshufpd, vshufpd, vshufpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpalignr_r, vpalignr, vpalignr, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM2),
       IMMARG(OPSZ_1))
OPCODE(vpalignr_m, vpalignr, vpalignr, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vblendps, vblendps, vblendps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vblendpd, vblendpd, vblendpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpblendw, vpblendw, vpblendw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpinsrb_r, vpinsrb, vpinsrb, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_15),
       REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrb_m, vpinsrb, vpinsrb, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_15),
       MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(vinsertps, vinsertps, vinsertps, 0, REGARG(XMM0), IMMARG(OPSZ_1), REGARG(XMM1),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vpinsrd_r, vpinsrd, vpinsrd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrd_m, vpinsrd, vpinsrd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpinsrq_r, vpinsrq, vpinsrq, X64_ONLY, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG(RAX), IMMARG(OPSZ_1))
OPCODE(vpinsrq_m, vpinsrq, vpinsrq, X64_ONLY, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vdpps, vdpps, vdpps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vdppd, vdppd, vdppd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vmpsadbw, vmpsadbw, vmpsadbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpclmulqdq, vpclmulqdq, vpclmulqdq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vroundss, vroundss, vroundss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vroundsd, vroundsd, vroundsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))

/* AVX 256-bit */
OPCODE(vcmpps_256, vcmpps, vcmpps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vcmppd_256, vcmppd, vcmppd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vshufps_256, vshufps, vshufps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vshufpd_256, vshufpd, vshufpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vblendvps_256, vblendvps, vblendvps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vblendvpd_256, vblendvpd, vblendvpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vblendps_256, vblendps, vblendps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vblendpd_256, vblendpd, vblendpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vdpps_256, vdpps, vdpps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vperm2f128_256, vperm2f128, vperm2f128, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vinsertf128_m128, vinsertf128, vinsertf128, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vinsertf128_r128, vinsertf128, vinsertf128, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(XMM2), IMMARG(OPSZ_1))

/****************************************************************************/
/* FMA4 128 and 256*/
OPCODE(vfmaddsubps_a, vfmaddsubps, vfmaddsubps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), REGARG(XMM3))
OPCODE(vfmaddsubps_b, vfmaddsubps, vfmaddsubps, 0, REGARG(XMM0), REGARG(XMM1),
       REGARG(XMM3), MEMARG(OPSZ_16))
OPCODE(vfmaddsubps_a_256, vfmaddsubps, vfmaddsubps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vfmaddsubps_b_256, vfmaddsubps, vfmaddsubps, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(YMM3), MEMARG(OPSZ_32))

OPCODE(vfmaddsubpd_a, vfmaddsubpd, vfmaddsubpd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), REGARG(XMM3))
OPCODE(vfmaddsubpd_b, vfmaddsubpd, vfmaddsubpd, 0, REGARG(XMM0), REGARG(XMM1),
       REGARG(XMM3), MEMARG(OPSZ_16))
OPCODE(vfmaddsubpd_a_256, vfmaddsubpd, vfmaddsubpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vfmaddsubpd_b_256, vfmaddsubpd, vfmaddsubpd, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(YMM3), MEMARG(OPSZ_32))

OPCODE(vfmsubaddps_a, vfmsubaddps, vfmsubaddps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), REGARG(XMM3))
OPCODE(vfmsubaddps_b, vfmsubaddps, vfmsubaddps, 0, REGARG(XMM0), REGARG(XMM1),
       REGARG(XMM3), MEMARG(OPSZ_16))
OPCODE(vfmsubaddps_a_256, vfmsubaddps, vfmsubaddps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vfmsubaddps_b_256, vfmsubaddps, vfmsubaddps, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(YMM3), MEMARG(OPSZ_32))

OPCODE(vfmsubaddpd_a, vfmsubaddpd, vfmsubaddpd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), REGARG(XMM3))
OPCODE(vfmsubaddpd_b, vfmsubaddpd, vfmsubaddpd, 0, REGARG(XMM0), REGARG(XMM1),
       REGARG(XMM3), MEMARG(OPSZ_16))
OPCODE(vfmsubaddpd_a_256, vfmsubaddpd, vfmsubaddpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vfmsubaddpd_b_256, vfmsubaddpd, vfmsubaddpd, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(YMM3), MEMARG(OPSZ_32))

OPCODE(vfmaddps_a, vfmaddps, vfmaddps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfmaddps_b, vfmaddps, vfmaddps, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfmaddpd_a, vfmaddpd, vfmaddpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfmaddpd_b, vfmaddpd, vfmaddpd, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfmaddss_a, vfmaddss, vfmaddss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4), REGARG_PARTIAL(XMM3, OPSZ_4))
OPCODE(vfmaddss_b, vfmaddss, vfmaddss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM3, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmaddsd_a, vfmaddsd, vfmaddsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8), REGARG_PARTIAL(XMM3, OPSZ_8))
OPCODE(vfmaddsd_b, vfmaddsd, vfmaddsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM3, OPSZ_8), MEMARG(OPSZ_8))

OPCODE(vfmsubps_a, vfmsubps, vfmsubps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfmsubps_b, vfmsubps, vfmsubps, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfmsubpd_a, vfmsubpd, vfmsubpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfmsubpd_b, vfmsubpd, vfmsubpd, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfmsubss_a, vfmsubss, vfmsubss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4), REGARG_PARTIAL(XMM3, OPSZ_4))
OPCODE(vfmsubss_b, vfmsubss, vfmsubss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM3, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfmsubsd_a, vfmsubsd, vfmsubsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8), REGARG_PARTIAL(XMM3, OPSZ_8))
OPCODE(vfmsubsd_b, vfmsubsd, vfmsubsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM3, OPSZ_8), MEMARG(OPSZ_8))

OPCODE(vfnmaddps_a, vfnmaddps, vfnmaddps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfnmaddps_b, vfnmaddps, vfnmaddps, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfnmaddpd_a, vfnmaddpd, vfnmaddpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfnmaddpd_b, vfnmaddpd, vfnmaddpd, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfnmaddss_a, vfnmaddss, vfnmaddss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4), REGARG_PARTIAL(XMM3, OPSZ_4))
OPCODE(vfnmaddss_b, vfnmaddss, vfnmaddss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM3, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmaddsd_a, vfnmaddsd, vfnmaddsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8), REGARG_PARTIAL(XMM3, OPSZ_8))
OPCODE(vfnmaddsd_b, vfnmaddsd, vfnmaddsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM3, OPSZ_8), MEMARG(OPSZ_8))

OPCODE(vfnmsubps_a, vfnmsubps, vfnmsubps, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfnmsubps_b, vfnmsubps, vfnmsubps, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfnmsubpd_a, vfnmsubpd, vfnmsubpd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vfnmsubpd_b, vfnmsubpd, vfnmsubpd, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vfnmsubss_a, vfnmsubss, vfnmsubss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4), REGARG_PARTIAL(XMM3, OPSZ_4))
OPCODE(vfnmsubss_b, vfnmsubss, vfnmsubss, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM3, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vfnmsubsd_a, vfnmsubsd, vfnmsubsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8), REGARG_PARTIAL(XMM3, OPSZ_8))
OPCODE(vfnmsubsd_b, vfnmsubsd, vfnmsubsd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM3, OPSZ_8), MEMARG(OPSZ_8))

/****************************************************************************/
/* XOP */
OPCODE(vpmacssww, vpmacssww, vpmacssww, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacsswd, vpmacsswd, vpmacsswd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacssdql, vpmacssdql, vpmacssdql, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacssdd, vpmacssdd, vpmacssdd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacssdqh, vpmacssdqh, vpmacssdqh, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacsww, vpmacsww, vpmacsww, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacswd, vpmacswd, vpmacswd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacsdql, vpmacsdql, vpmacsdql, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacsdd, vpmacsdd, vpmacsdd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmacsdqh, vpmacsdqh, vpmacsdqh, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmadcsswd, vpmadcsswd, vpmadcsswd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpmadcswd, vpmadcswd, vpmadcswd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))

OPCODE(vpperm_a, vpperm, vpperm, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpperm_b, vpperm, vpperm, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))

OPCODE(vpcmov_a, vpcmov, vpcmov, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       REGARG(XMM3))
OPCODE(vpcmov_b, vpcmov, vpcmov, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vpcmov_a_256, vpcmov, vpcmov, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       REGARG(YMM3))
OPCODE(vpcmov_b_256, vpcmov, vpcmov, 0, REGARG(YMM0), REGARG(YMM1), REGARG(YMM3),
       MEMARG(OPSZ_32))

OPCODE(vpermil2pd_a, vpermil2pd, vpermil2pd, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), REGARG(XMM3))
OPCODE(vpermil2pd_b, vpermil2pd, vpermil2pd, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vpermil2pd_a_256, vpermil2pd, vpermil2pd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vpermil2pd_b_256, vpermil2pd, vpermil2pd, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(YMM3), MEMARG(OPSZ_32))

OPCODE(vpermil2ps_a, vpermil2ps, vpermil2ps, 0, REGARG(XMM0), REGARG(XMM1),
       MEMARG(OPSZ_16), REGARG(XMM3))
OPCODE(vpermil2ps_b, vpermil2ps, vpermil2ps, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM3),
       MEMARG(OPSZ_16))
OPCODE(vpermil2ps_a_256, vpermil2ps, vpermil2ps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vpermil2ps_b_256, vpermil2ps, vpermil2ps, 0, REGARG(YMM0), REGARG(YMM1),
       REGARG(YMM3), MEMARG(OPSZ_32))

OPCODE(vpcomb, vpcomb, vpcomb, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomw, vpcomw, vpcomw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomd, vpcomd, vpcomd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomq, vpcomq, vpcomq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomub, vpcomub, vpcomub, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomuw, vpcomuw, vpcomuw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomud, vpcomud, vpcomud, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcomuq, vpcomuq, vpcomuq, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))

/****************************************************************************/
/* AVX 256 */
OPCODE(vpalignr_256r, vpalignr, vpalignr, 0, REGARG(YMM0), REGARG(YMM1), REGARG(YMM2),
       IMMARG(OPSZ_1))
OPCODE(vpalignr_256m, vpalignr, vpalignr, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vpblendvb_256, vpblendvb, vpblendvb, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32), REGARG(YMM3))
OPCODE(vpblendw_256, vpblendw, vpblendw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vmpsadbw_256, vmpsadbw, vmpsadbw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))

OPCODE(vinserti128, vinserti128, vinserti128, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpblendd, vpblendd, vpblendd, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpblendd_256, vpblendd, vpblendd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))

/****************************************************************************/
/* AVX2 */
OPCODE(vperm2i128, vperm2i128, vperm2i128, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
