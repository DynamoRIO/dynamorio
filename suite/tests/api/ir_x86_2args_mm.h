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

/****************************************************************************/
/* 3D Now */
OPCODE(pavgusb, pavgusb, pavgusb, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfadd, pfadd, pfadd, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfacc, pfacc, pfacc, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfcmpge, pfcmpge, pfcmpge, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfcmpgt, pfcmpgt, pfcmpgt, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfcmpeq, pfcmpeq, pfcmpeq, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfmin, pfmin, pfmin, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfmax, pfmax, pfmax, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfmul, pfmul, pfmul, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfrcp, pfrcp, pfrcp, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfrcpit1, pfrcpit1, pfrcpit1, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfrcpit2, pfrcpit2, pfrcpit2, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfrsqrt, pfrsqrt, pfrsqrt, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfrsqit1, pfrsqit1, pfrsqit1, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pmulhrw, pmulhrw, pmulhrw, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfsub, pfsub, pfsub, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfsubr, pfsubr, pfsubr, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pi2fd, pi2fd, pi2fd, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pf2id, pf2id, pf2id, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pi2fw, pi2fw, pi2fw, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pf2iw, pf2iw, pf2iw, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfnacc, pfnacc, pfnacc, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pfpnacc, pfpnacc, pfpnacc, 0, REGARG(MM0), MEMARG(OPSZ_8))
OPCODE(pswapd, pswapd, pswapd, 0, REGARG(MM0), MEMARG(OPSZ_8))

OPCODE(aesimc, aesimc, aesimc, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(aesenc, aesenc, aesenc, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(aesenclast, aesenclast, aesenclast, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(aesdec, aesdec, aesdec, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(aesdeclast, aesdeclast, aesdeclast, 0, REGARG(XMM0), MEMARG(OPSZ_16))

/****************************************************************************/
/* AVX */
OPCODE(vmovlps, vmovlps, vmovlps, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovlpd, vmovlpd, vmovlpd, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovhps, vmovhps, vmovhps, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovhpd, vmovhpd, vmovhpd, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vucomiss, vucomiss, vucomiss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vucomisd, vucomisd, vucomisd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcomiss, vcomiss, vcomiss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vcomisd, vcomisd, vcomisd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vptest, vptest, vptest, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vtestps, vtestps, vtestps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vtestpd, vtestpd, vtestpd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovups_ld, vmovups, vmovups, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovupd_ld, vmovupd, vmovupd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovups_st, vmovups, vmovups, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovupd_st, vmovupd, vmovupd, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovsldup, vmovsldup, vmovsldup, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovddup, vmovddup, vmovddup, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vmovshdup, vmovshdup, vmovshdup, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovaps_ld, vmovaps, vmovaps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovapd_ld, vmovapd, vmovapd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovaps_st, vmovaps, vmovaps, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovapd_st, vmovapd, vmovapd, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovntps, vmovntps, vmovntps, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovntpd, vmovntpd, vmovntpd, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vcvttss2si, vcvttss2si, vcvttss2si, 0, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(vcvttsd2si, vcvttsd2si, vcvttsd2si, 0, REGARG(EAX), MEMARG(OPSZ_8))
OPCODE(vcvtss2si, vcvtss2si, vcvtss2si, 0, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(vcvtsd2si, vcvtsd2si, vcvtsd2si, 0, REGARG(EAX), MEMARG(OPSZ_8))
OPCODE(vmovmskps, vmovmskps, vmovmskps, 0, REGARG(XAX), REGARG(XMM0))
OPCODE(vmovmskpd, vmovmskpd, vmovmskpd, 0, REGARG(XAX), REGARG(XMM0))
OPCODE(vsqrtps, vsqrtps, vsqrtps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vsqrtpd, vsqrtpd, vsqrtpd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vrsqrtps, vrsqrtps, vrsqrtps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vrcpps, vrcpps, vrcpps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtps2pd, vcvtps2pd, vcvtps2pd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtpd2ps, vcvtpd2ps, vcvtpd2ps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtdq2ps, vcvtdq2ps, vcvtdq2ps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvttps2dq, vcvttps2dq, vcvttps2dq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtps2dq, vcvtps2dq, vcvtps2dq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovd_ld, vmovd, vmovd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vmovd_st, vmovd, vmovd, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovq_ld, vmovq, vmovq, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vmovq_st, vmovq, vmovq, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vpmovmskb, vpmovmskb, vpmovmskb, 0, REGARG(EAX), REGARG(XMM0))
OPCODE(vcvtdq2pd, vcvtdq2pd, vcvtdq2pd, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vcvttpd2dq, vcvttpd2dq, vcvttpd2dq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtpd2dq, vcvtpd2dq, vcvtpd2dq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovntdq, vmovntdq, vmovntdq, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovdqu_ld, vmovdqu, vmovdqu, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovdqa_ld, vmovdqa, vmovdqa, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovdqu_st, vmovdqu, vmovdqu, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovdqa_st, vmovdqa, vmovdqa, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vlddqu, vlddqu, vlddqu, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxbw, vpmovsxbw, vpmovsxbw, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxbd, vpmovsxbd, vpmovsxbd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxbq, vpmovsxbq, vpmovsxbq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxwd, vpmovsxwd, vpmovsxwd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxwq, vpmovsxwq, vpmovsxwq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxdq, vpmovsxdq, vpmovsxdq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovntdqa, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vpmovzxbw, vpmovzxbw, vpmovzxbw, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxbd, vpmovzxbd, vpmovzxbd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxbq, vpmovzxbq, vpmovzxbq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxwd, vpmovzxwd, vpmovzxwd, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxwq, vpmovzxwq, vpmovzxwq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxdq, vpmovzxdq, vpmovzxdq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vphminposuw, vphminposuw, vphminposuw, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vaesimc, vaesimc, vaesimc, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovss_ld, vmovss, vmovss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vmovsd_ld, vmovsd, vmovsd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovss_st, vmovss, vmovss, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovsd_st, vmovsd, vmovsd, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmaskmovdqu, vmaskmovdqu, vmaskmovdqu, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(vcvtph2ps, vcvtph2ps, vcvtph2ps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vbroadcastss, vbroadcastss, vbroadcastss, 0, REGARG(XMM0), MEMARG(OPSZ_4))

/* AVX 256-bit */
OPCODE(vmovups_ld_256, vmovups, vmovups, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovupd_ld_256, vmovupd, vmovupd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovups_st_256, vmovups, vmovups, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovupd_s_256t, vmovupd, vmovupd, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovsldup_256, vmovsldup, vmovsldup, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovddup_256, vmovddup, vmovddup, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovshdup_256, vmovshdup, vmovshdup, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovaps_256, vmovaps, vmovaps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovapd_256, vmovapd, vmovapd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovntps_256, vmovntps, vmovntps, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovntpd_256, vmovntpd, vmovntpd, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovmskps_256, vmovmskps, vmovmskps, 0, REGARG(XAX), REGARG(XMM0))
OPCODE(vmovmskpd_256, vmovmskpd, vmovmskpd, 0, REGARG(XAX), REGARG(XMM0))
OPCODE(vsqrtps_256, vsqrtps, vsqrtps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vsqrtpd_256, vsqrtpd, vsqrtpd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vrsqrtps_256, vrsqrtps, vrsqrtps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vrcpps_256, vrcpps, vrcpps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtps2pd_256, vcvtps2pd, vcvtps2pd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtpd2ps_256, vcvtpd2ps, vcvtpd2ps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtdq2ps_256, vcvtdq2ps, vcvtdq2ps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvttps2dq_256, vcvttps2dq, vcvttps2dq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtps2dq_256, vcvtps2dq, vcvtps2dq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtdq2pd_256, vcvtdq2pd, vcvtdq2pd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvttpd2dq_256, vcvttpd2dq, vcvttpd2dq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtpd2dq_256, vcvtpd2dq, vcvtpd2dq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovntdq_256, vmovntdq, vmovntdq, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovdqu_ld_256, vmovdqu, vmovdqu, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovdqa_ld_256, vmovdqa, vmovdqa, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovdqu_st_256, vmovdqu, vmovdqu, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovdqa_st_256, vmovdqa, vmovdqa, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vlddqu_256, vlddqu, vlddqu, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptest_256, vptest, vptest, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vtestps_256, vtestps, vtestps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vtestpd_256, vtestpd, vtestpd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtph2ps_256, vcvtph2ps, vcvtph2ps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vbroadcastss_256, vbroadcastss, vbroadcastss, 0, REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vbroadcastsd, vbroadcastsd, vbroadcastsd, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vbroadcastf128, vbroadcastf128, vbroadcastf128, 0, REGARG(YMM0), MEMARG(OPSZ_16))

/* AVX2 256-bit */
OPCODE(vpmovmskb_256, vpmovmskb, vpmovmskb, 0, REGARG(EAX), REGARG(YMM0))
OPCODE(vpmovsxbw_256, vpmovsxbw, vpmovsxbw, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovsxbd_256, vpmovsxbd, vpmovsxbd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovsxbq_256, vpmovsxbq, vpmovsxbq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovsxwd_256, vpmovsxwd, vpmovsxwd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovsxwq_256, vpmovsxwq, vpmovsxwq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovsxdq_256, vpmovsxdq, vpmovsxdq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vmovntdqa_256, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vpmovzxbw_256, vpmovzxbw, vpmovzxbw, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovzxbd_256, vpmovzxbd, vpmovzxbd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovzxbq_256, vpmovzxbq, vpmovzxbq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovzxwd_256, vpmovzxwd, vpmovzxwd, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovzxwq_256, vpmovzxwq, vpmovzxwq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vpmovzxdq_256, vpmovzxdq, vpmovzxdq, 0, REGARG(YMM0), MEMARG(OPSZ_32))

/* random extended reg tests */
OPCODE(vmovdqu_256_ext, vmovdqu, vmovdqu, X64_ONLY, REGARG(YMM11), MEMARG(OPSZ_32))
OPCODE(vtestpd_256_ext, vtestpd, vtestpd, X64_ONLY, REGARG(YMM15), MEMARG(OPSZ_32))

/* AVX2 */
OPCODE(vbroadcasti128, vbroadcasti128, vbroadcasti128, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpbroadcastb, vpbroadcastb, vpbroadcastb, 0, REGARG(XMM0), MEMARG(OPSZ_1))
OPCODE(vpbroadcastw, vpbroadcastw, vpbroadcastw, 0, REGARG(XMM0), MEMARG(OPSZ_2))
OPCODE(vpbroadcastd, vpbroadcastd, vpbroadcastd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpbroadcastq, vpbroadcastq, vpbroadcastq, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpbroadcastb_256, vpbroadcastb, vpbroadcastb, 0, REGARG(YMM0), MEMARG(OPSZ_1))
OPCODE(vpbroadcastw_256, vpbroadcastw, vpbroadcastw, 0, REGARG(YMM0), MEMARG(OPSZ_2))
OPCODE(vpbroadcastd_256, vpbroadcastd, vpbroadcastd, 0, REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpbroadcastq_256, vpbroadcastq, vpbroadcastq, 0, REGARG(YMM0), MEMARG(OPSZ_8))
