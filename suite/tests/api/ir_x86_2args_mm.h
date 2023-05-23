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
OPCODE(vmovddup, vmovddup, vmovddup, 0, REGARG(XMM0), MEMARG(OPSZ_16))
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
OPCODE(vcvtps2pd, vcvtps2pd, vcvtps2pd, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vcvtpd2ps, vcvtpd2ps, vcvtpd2ps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtdq2ps, vcvtdq2ps, vcvtdq2ps, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvttps2dq, vcvttps2dq, vcvttps2dq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vcvtps2dq, vcvtps2dq, vcvtps2dq, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovd_xld, vmovd, vmovd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vmovd_xst, vmovd, vmovd, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovq_xld, vmovq, vmovq, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vmovq_xst, vmovq, vmovq, X64_ONLY, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovd_xr32, vmovd, vmovd, 0, REGARG(XMM0), REGARG(EAX))
OPCODE(vmovd_r32x, vmovd, vmovd, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovq_xr64, vmovq, vmovq, X64_ONLY, REGARG(XMM0), REGARG(RAX))
OPCODE(vmovq_r64x, vmovq, vmovq, X64_ONLY, REGARG(RAX), REGARG_PARTIAL(XMM0, OPSZ_8))
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
OPCODE(vpmovsxbwm, vpmovsxbw, vpmovsxbw, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpmovsxbwr, vpmovsxbw, vpmovsxbw, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovsxbdm, vpmovsxbd, vpmovsxbd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpmovsxbdr, vpmovsxbd, vpmovsxbd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovsxbqm, vpmovsxbq, vpmovsxbq, 0, REGARG(XMM0), MEMARG(OPSZ_2))
OPCODE(vpmovsxbqr, vpmovsxbq, vpmovsxbq, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpmovsxwdm, vpmovsxwd, vpmovsxwd, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpmovsxwdr, vpmovsxwd, vpmovsxwd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovsxwqm, vpmovsxwq, vpmovsxwq, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpmovsxwqr, vpmovsxwq, vpmovsxwq, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovsxdqm, vpmovsxdq, vpmovsxdq, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpmovsxdqr, vpmovsxdq, vpmovsxdq, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vmovntdqa, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vpmovzxbwm, vpmovzxbw, vpmovzxbw, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpmovzxbwr, vpmovzxbw, vpmovzxbw, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovzxbdm, vpmovzxbd, vpmovzxbd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpmovzxbdr, vpmovzxbd, vpmovzxbd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovzxbqm, vpmovzxbq, vpmovzxbq, 0, REGARG(XMM0), MEMARG(OPSZ_2))
OPCODE(vpmovzxbqr, vpmovzxbq, vpmovzxbq, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpmovzxwdm, vpmovzxwd, vpmovzxwd, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpmovzxwdr, vpmovzxwd, vpmovzxwd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpmovzxwqm, vpmovzxwq, vpmovzxwq, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpmovzxwqr, vpmovzxwq, vpmovzxwq, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpmovzxdqm, vpmovzxdq, vpmovzxdq, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpmovzxdqr, vpmovzxdq, vpmovzxdq, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vphminposuw, vphminposuw, vphminposuw, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vaesimc, vaesimc, vaesimc, 0, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vmovss_ld, vmovss, vmovss, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vmovsd_ld, vmovsd, vmovsd, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vmovss_st, vmovss, vmovss, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovsd_st, vmovsd, vmovsd, 0, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmaskmovdqu, vmaskmovdqu, vmaskmovdqu, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(vcvtph2ps, vcvtph2ps, vcvtph2ps, 0, REGARG(XMM0), MEMARG(OPSZ_8))
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
OPCODE(vcvtps2pd_256, vcvtps2pd, vcvtps2pd, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vcvtpd2ps_256, vcvtpd2ps, vcvtpd2ps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtdq2ps_256, vcvtdq2ps, vcvtdq2ps, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvttps2dq_256, vcvttps2dq, vcvttps2dq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtps2dq_256, vcvtps2dq, vcvtps2dq, 0, REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vcvtdq2pd_256, vcvtdq2pd, vcvtdq2pd, 0, REGARG(YMM0), MEMARG(OPSZ_16))
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
OPCODE(vcvtph2ps_256, vcvtph2ps, vcvtph2ps, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vbroadcastss_256, vbroadcastss, vbroadcastss, 0, REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vbroadcastsd, vbroadcastsd, vbroadcastsd, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vbroadcastf128, vbroadcastf128, vbroadcastf128, 0, REGARG(YMM0), MEMARG(OPSZ_16))

/* AVX2 256-bit */
OPCODE(vpmovmskb_256, vpmovmskb, vpmovmskb, 0, REGARG(EAX), REGARG(YMM0))
OPCODE(vpmovsxbw_256m, vpmovsxbw, vpmovsxbw, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxbw_256r, vpmovsxbw, vpmovsxbw, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovsxbd_256m, vpmovsxbd, vpmovsxbd, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpmovsxbd_256r, vpmovsxbd, vpmovsxbd, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovsxbq_256m, vpmovsxbq, vpmovsxbq, 0, REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpmovsxbq_256r, vpmovsxbq, vpmovsxbq, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_4))
OPCODE(vpmovsxwd_256m, vpmovsxwd, vpmovsxwd, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxwd_256r, vpmovsxwd, vpmovsxwd, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovsxwq_256m, vpmovsxwq, vpmovsxwq, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpmovsxwq_256r, vpmovsxwq, vpmovsxwq, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovsxdq_256m, vpmovsxdq, vpmovsxdq, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpmovsxdq_256r, vpmovsxdq, vpmovsxdq, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vmovntdqa_256, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vpmovzxbw_256m, vpmovzxbw, vpmovzxbw, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxbw_256r, vpmovzxbw, vpmovzxbw, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovzxbd_256m, vpmovzxbd, vpmovzxbd, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpmovzxbd_256r, vpmovzxbd, vpmovzxbd, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovzxbq_256m, vpmovzxbq, vpmovzxbq, 0, REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpmovzxbq_256r, vpmovzxbq, vpmovzxbq, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_4))
OPCODE(vpmovzxwd_256m, vpmovzxwd, vpmovzxwd, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxwd_256r, vpmovzxwd, vpmovzxwd, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_16))
OPCODE(vpmovzxwq_256m, vpmovzxwq, vpmovzxwq, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpmovzxwq_256r, vpmovzxwq, vpmovzxwq, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_8))
OPCODE(vpmovzxdq_256m, vpmovzxdq, vpmovzxdq, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpmovzxdq_256r, vpmovzxdq, vpmovzxdq, 0, REGARG(YMM0),
       REGARG_PARTIAL(YMM1, OPSZ_16))

/* random extended reg tests */
OPCODE(vmovdqu_256_ext, vmovdqu, vmovdqu, X64_ONLY, REGARG(YMM11), MEMARG(OPSZ_32))
OPCODE(vtestpd_256_ext, vtestpd, vtestpd, X64_ONLY, REGARG(YMM15), MEMARG(OPSZ_32))

/* AVX2 */
OPCODE(vbroadcasti128, vbroadcasti128, vbroadcasti128, 0, REGARG(YMM0), MEMARG(OPSZ_16))
OPCODE(vpbroadcastb_m, vpbroadcastb, vpbroadcastb, 0, REGARG(XMM0), MEMARG(OPSZ_1))
OPCODE(vpbroadcastw_m, vpbroadcastw, vpbroadcastw, 0, REGARG(XMM0), MEMARG(OPSZ_2))
OPCODE(vpbroadcastd_m, vpbroadcastd, vpbroadcastd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vpbroadcastq_m, vpbroadcastq, vpbroadcastq, 0, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vpbroadcastb_r, vpbroadcastb, vpbroadcastb, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_1))
OPCODE(vpbroadcastw_r, vpbroadcastw, vpbroadcastw, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpbroadcastd_r, vpbroadcastd, vpbroadcastd, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpbroadcastq_r, vpbroadcastq, vpbroadcastq, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vpbroadcastb_256m, vpbroadcastb, vpbroadcastb, 0, REGARG(YMM0), MEMARG(OPSZ_1))
OPCODE(vpbroadcastw_256m, vpbroadcastw, vpbroadcastw, 0, REGARG(YMM0), MEMARG(OPSZ_2))
OPCODE(vpbroadcastd_256m, vpbroadcastd, vpbroadcastd, 0, REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vpbroadcastq_256m, vpbroadcastq, vpbroadcastq, 0, REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vpbroadcastb_256r, vpbroadcastb, vpbroadcastb, 0, REGARG(YMM0),
       REGARG_PARTIAL(XMM1, OPSZ_1))
OPCODE(vpbroadcastw_256r, vpbroadcastw, vpbroadcastw, 0, REGARG(YMM0),
       REGARG_PARTIAL(XMM1, OPSZ_2))
OPCODE(vpbroadcastd_256r, vpbroadcastd, vpbroadcastd, 0, REGARG(YMM0),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vpbroadcastq_256r, vpbroadcastq, vpbroadcastq, 0, REGARG(YMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8))

/* SHA */
OPCODE(sha1msg1_xloxlo, sha1msg1, sha1msg1, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(sha1msg1_xloxhi, sha1msg1, sha1msg1, X64_ONLY, REGARG(XMM7), REGARG(XMM15))
OPCODE(sha1msg1_xlom, sha1msg1, sha1msg1, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(sha1msg1_xhim, sha1msg1, sha1msg1, X64_ONLY, REGARG(XMM15), MEMARG(OPSZ_16))
OPCODE(sha1msg2_xloxlo, sha1msg2, sha1msg2, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(sha1msg2_xloxhi, sha1msg2, sha1msg2, X64_ONLY, REGARG(XMM7), REGARG(XMM15))
OPCODE(sha1msg2_xlom, sha1msg2, sha1msg2, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(sha1msg2_xhim, sha1msg2, sha1msg2, X64_ONLY, REGARG(XMM15), MEMARG(OPSZ_16))
OPCODE(sha1nexte_xloxlo, sha1nexte, sha1nexte, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(sha1nexte_xloxhi, sha1nexte, sha1nexte, X64_ONLY, REGARG(XMM7), REGARG(XMM15))
OPCODE(sha1nexte_xlom, sha1nexte, sha1nexte, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(sha1nexte_xhim, sha1nexte, sha1nexte, X64_ONLY, REGARG(XMM15), MEMARG(OPSZ_16))
OPCODE(sha256rnds2_xloxlo, sha256rnds2, sha256rnds2, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(sha256rnds2_xloxhi, sha256rnds2, sha256rnds2, X64_ONLY, REGARG(XMM7),
       REGARG(XMM15))
OPCODE(sha256rnds2_xlom, sha256rnds2, sha256rnds2, X64_ONLY, REGARG(XMM0),
       MEMARG(OPSZ_16))
OPCODE(sha256rnds2_xhim, sha256rnds2, sha256rnds2, X64_ONLY, REGARG(XMM15),
       MEMARG(OPSZ_16))
OPCODE(sha256msg1_xloxlo, sha256msg1, sha256msg1, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(sha256msg1_xloxhi, sha256msg1, sha256msg1, X64_ONLY, REGARG(XMM7), REGARG(XMM15))
OPCODE(sha256msg1_xlom, sha256msg1, sha256msg1, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(sha256msg1_xhim, sha256msg1, sha256msg1, X64_ONLY, REGARG(XMM15), MEMARG(OPSZ_16))
OPCODE(sha256msg2_xloxlo, sha256msg2, sha256msg2, 0, REGARG(XMM0), REGARG(XMM1))
OPCODE(sha256msg2_xloxhi, sha256msg2, sha256msg2, X64_ONLY, REGARG(XMM7), REGARG(XMM15))
OPCODE(sha256msg2_xlom, sha256msg2, sha256msg2, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(sha256msg2_xhim, sha256msg2, sha256msg2, X64_ONLY, REGARG(XMM15), MEMARG(OPSZ_16))

/* MPX */
OPCODE(bndmov_b0b1, bndmov, bndmov, 0, REGARG(BND0), REGARG(BND1))
OPCODE(bndmov_b2b3, bndmov, bndmov, 0, REGARG(BND2), REGARG(BND3))
OPCODE(bndmov_b0m64, bndmov, bndmov, X64_ONLY, REGARG(BND0), MEMARG(OPSZ_16))
OPCODE(bndmov_b0m32, bndmov, bndmov, X86_ONLY, REGARG(BND0), MEMARG(OPSZ_8))
OPCODE(bndmov_b3m64, bndmov, bndmov, X64_ONLY, REGARG(BND3), MEMARG(OPSZ_16))
OPCODE(bndmov_b3m32, bndmov, bndmov, X86_ONLY, REGARG(BND3), MEMARG(OPSZ_8))
OPCODE(bndcl_b0r32, bndcl, bndcl, X86_ONLY, REGARG(BND0), REGARG(EAX))
OPCODE(bndcl_b0r64, bndcl, bndcl, X64_ONLY, REGARG(BND0), REGARG(RAX))
OPCODE(bndcl_b3r32, bndcl, bndcl, X86_ONLY, REGARG(BND3), REGARG(EAX))
OPCODE(bndcl_b3r64, bndcl, bndcl, X64_ONLY, REGARG(BND3), REGARG(RAX))
OPCODE(bndcu_b0r32, bndcu, bndcu, X86_ONLY, REGARG(BND0), REGARG(EAX))
OPCODE(bndcu_b0r64, bndcu, bndcu, X64_ONLY, REGARG(BND0), REGARG(RAX))
OPCODE(bndcu_b3r32, bndcu, bndcu, X86_ONLY, REGARG(BND3), REGARG(EAX))
OPCODE(bndcu_b3r64, bndcu, bndcu, X64_ONLY, REGARG(BND3), REGARG(RAX))
OPCODE(bndcn_b0r32, bndcn, bndcn, X86_ONLY, REGARG(BND0), REGARG(EAX))
OPCODE(bndcn_b0r64, bndcn, bndcn, X64_ONLY, REGARG(BND0), REGARG(RAX))
OPCODE(bndcn_b3r32, bndcn, bndcn, X86_ONLY, REGARG(BND3), REGARG(EAX))
OPCODE(bndcn_b3r64, bndcn, bndcn, X64_ONLY, REGARG(BND3), REGARG(RAX))
OPCODE(bndmk_b0r32, bndmk, bndmk, X86_ONLY, REGARG(BND0), MEMARG(OPSZ_4))
OPCODE(bndmk_b0r64, bndmk, bndmk, X64_ONLY, REGARG(BND0), MEMARG(OPSZ_8))
OPCODE(bndmk_b3r32, bndmk, bndmk, X86_ONLY, REGARG(BND3), MEMARG(OPSZ_4))
OPCODE(bndmk_b3r64, bndmk, bndmk, X64_ONLY, REGARG(BND3), MEMARG(OPSZ_8))
OPCODE(bndldx_b0ld, bndldx, bndldx, 0, REGARG(BND0), MEMARG(OPSZ_bnd))
OPCODE(bndldx_b3ld, bndldx, bndldx, 0, REGARG(BND3), MEMARG(OPSZ_bnd))
OPCODE(bndstx_b0st, bndstx, bndstx, 0, MEMARG(OPSZ_bnd), REGARG(BND0))
OPCODE(bndstx_b3st, bndstx, bndstx, 0, MEMARG(OPSZ_bnd), REGARG(BND3))
