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

/* AVX-512 EVEX instructions with 1 destination, and 1 source, no mask. */
OPCODE(vmovlps_mxlo, vmovlps, vmovlps, VERIFY_EVEX, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovlps_mxhi, vmovlps, vmovlps, X64_ONLY, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vmovlpd_mxlo, vmovlpd, vmovlpd, VERIFY_EVEX, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovlpd_mxhi, vmovlpd, vmovlpd, X64_ONLY, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvtss2si_r32m, vcvtss2si, vcvtss2si, 0, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(vcvtss2si_r64m, vcvtss2si, vcvtss2si, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_4))
OPCODE(vcvtss2si_r32xlo, vcvtss2si, vcvtss2si, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvtss2si_r32xhi, vcvtss2si, vcvtss2si, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvtss2si_r64xlo, vcvtss2si, vcvtss2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvtss2si_r64xhi, vcvtss2si, vcvtss2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvtsd2si_r32m, vcvtsd2si, vcvtsd2si, 0, REGARG(EAX), MEMARG(OPSZ_8))
OPCODE(vcvtsd2si_r64m, vcvtsd2si, vcvtsd2si, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_8))
OPCODE(vcvtsd2si_r32xlo, vcvtsd2si, vcvtsd2si, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvtsd2si_r32xhi, vcvtsd2si, vcvtsd2si, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvtsd2si_r64xlo, vcvtsd2si, vcvtsd2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvtsd2si_r64xhi, vcvtsd2si, vcvtsd2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvttss2si_r32m, vcvttss2si, vcvttss2si, 0, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(vcvttss2si_r64m, vcvttss2si, vcvttss2si, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_4))
OPCODE(vcvttss2si_r32xlo, vcvttss2si, vcvttss2si, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvttss2si_r32xhi, vcvttss2si, vcvttss2si, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvttss2si_r64xlo, vcvttss2si, vcvttss2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvttss2si_r64xhi, vcvttss2si, vcvttss2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvttsd2si_r32m, vcvttsd2si, vcvttsd2si, 0, REGARG(EAX), MEMARG(OPSZ_8))
OPCODE(vcvttsd2si_r64m, vcvttsd2si, vcvttsd2si, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_8))
OPCODE(vcvttsd2si_r32xlo, vcvttsd2si, vcvttsd2si, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvttsd2si_r32xhi, vcvttsd2si, vcvttsd2si, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvttsd2si_r64xlo, vcvttsd2si, vcvttsd2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvttsd2si_r64xhi, vcvttsd2si, vcvttsd2si, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vmovntps_mxlo, vmovntps, vmovntps, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovntps_mxhi, vmovntps, vmovntps, X64_ONLY, MEMARG(OPSZ_16), REGARG(XMM16))
OPCODE(vmovntps_mylo, vmovntps, vmovntps, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovntps_myhi, vmovntps, vmovntps, X64_ONLY, MEMARG(OPSZ_32), REGARG(YMM16))
OPCODE(vmovntps_mzlo, vmovntps, vmovntps, 0, MEMARG(OPSZ_64), REGARG(ZMM0))
OPCODE(vmovntps_mzhi, vmovntps, vmovntps, X64_ONLY, MEMARG(OPSZ_64), REGARG(ZMM16))
OPCODE(vmovntpd_mxlo, vmovntpd, vmovntpd, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovntpd_mxhi, vmovntpd, vmovntpd, X64_ONLY, MEMARG(OPSZ_16), REGARG(XMM16))
OPCODE(vmovntpd_mylo, vmovntpd, vmovntpd, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovntpd_myhi, vmovntpd, vmovntpd, X64_ONLY, MEMARG(OPSZ_32), REGARG(YMM16))
OPCODE(vmovntpd_mzlo, vmovntpd, vmovntpd, 0, MEMARG(OPSZ_64), REGARG(ZMM0))
OPCODE(vmovntpd_mzhi, vmovntpd, vmovntpd, X64_ONLY, MEMARG(OPSZ_64), REGARG(ZMM16))
OPCODE(vucomiss_xlom, vucomiss, vucomiss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vucomiss_xhim, vucomiss, vucomiss, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vucomisd_xlom, vucomisd, vucomisd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vucomisd_xhim, vucomisd, vucomisd, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vcomiss_xlom, vcomiss, vcomiss, 0, REGARG_PARTIAL(XMM0, OPSZ_4), MEMARG(OPSZ_4))
OPCODE(vcomiss_xhim, vcomiss, vcomiss, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vcomisd_xlom, vcomisd, vcomisd, 0, REGARG_PARTIAL(XMM0, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcomisd_xhim, vcomisd, vcomisd, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vmovntdq_mxlo, vmovntdq, vmovntdq, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovntdq_mxhi, vmovntdq, vmovntdq, X64_ONLY, MEMARG(OPSZ_16), REGARG(XMM16))
OPCODE(vmovntdq_mylo, vmovntdq, vmovntdq, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovntdq_myhi, vmovntdq, vmovntdq, X64_ONLY, MEMARG(OPSZ_32), REGARG(YMM16))
OPCODE(vmovntdq_mzlo, vmovntdq, vmovntdq, 0, MEMARG(OPSZ_64), REGARG(ZMM0))
OPCODE(vmovntdq_mzhi, vmovntdq, vmovntdq, X64_ONLY, MEMARG(OPSZ_64), REGARG(ZMM16))
OPCODE(vmovntdqa_mxlo, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_16), REGARG(XMM0))
OPCODE(vmovntdqa_mxhi, vmovntdqa, vmovntdqa, X64_ONLY, MEMARG(OPSZ_16), REGARG(XMM16))
OPCODE(vmovntdqa_mylo, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_32), REGARG(YMM0))
OPCODE(vmovntdqa_myhi, vmovntdqa, vmovntdqa, X64_ONLY, MEMARG(OPSZ_32), REGARG(YMM16))
OPCODE(vmovntdqa_mzlo, vmovntdqa, vmovntdqa, 0, MEMARG(OPSZ_64), REGARG(ZMM0))
OPCODE(vmovntdqa_mzhi, vmovntdqa, vmovntdqa, X64_ONLY, MEMARG(OPSZ_64), REGARG(ZMM16))
OPCODE(vcvtss2usi_r32m, vcvtss2usi, vcvtss2usi, 0, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(vcvtss2usi_r64m, vcvtss2usi, vcvtss2usi, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_4))
OPCODE(vcvtss2usi_r32xlo, vcvtss2usi, vcvtss2usi, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvtss2usi_r32xhi, vcvtss2usi, vcvtss2usi, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvtss2usi_r64xlo, vcvtss2usi, vcvtss2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvtss2usi_r64xhi, vcvtss2usi, vcvtss2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvtsd2usi_r32m, vcvtsd2usi, vcvtsd2usi, 0, REGARG(EAX), MEMARG(OPSZ_8))
OPCODE(vcvtsd2usi_r64m, vcvtsd2usi, vcvtsd2usi, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_8))
OPCODE(vcvtsd2usi_r32xlo, vcvtsd2usi, vcvtsd2usi, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvtsd2usi_r32xhi, vcvtsd2usi, vcvtsd2usi, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvtsd2usi_r64xlo, vcvtsd2usi, vcvtsd2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvtsd2usi_r64xhi, vcvtsd2usi, vcvtsd2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvttss2usi_r32m, vcvttss2usi, vcvttss2usi, 0, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(vcvttss2usi_r64m, vcvttss2usi, vcvttss2usi, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_4))
OPCODE(vcvttss2usi_r32xlo, vcvttss2usi, vcvttss2usi, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvttss2usi_r32xhi, vcvttss2usi, vcvttss2usi, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvttss2usi_r64xlo, vcvttss2usi, vcvttss2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vcvttss2usi_r64xhi, vcvttss2usi, vcvttss2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_4))
OPCODE(vcvttsd2usi_r32m, vcvttsd2usi, vcvttsd2usi, 0, REGARG(EAX), MEMARG(OPSZ_8))
OPCODE(vcvttsd2usi_r64m, vcvttsd2usi, vcvttsd2usi, X64_ONLY, REGARG(RAX), MEMARG(OPSZ_8))
OPCODE(vcvttsd2usi_r32xlo, vcvttsd2usi, vcvttsd2usi, 0, REGARG(EAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvttsd2usi_r32xhi, vcvttsd2usi, vcvttsd2usi, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vcvttsd2usi_r64xlo, vcvttsd2usi, vcvttsd2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vcvttsd2usi_r64xhi, vcvttsd2usi, vcvttsd2usi, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM16, OPSZ_8))
OPCODE(vmovd_xlor32, vmovd, vmovd, 0, REGARG(XMM0), REGARG(EAX))
OPCODE(vmovd_xlom32, vmovd, vmovd, 0, REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vmovd_r32xlo, vmovd, vmovd, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovd_m32xlo, vmovd, vmovd, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4))
OPCODE(vmovd_xhir32, vmovd, vmovd, X64_ONLY, REGARG(XMM31), REGARG(EAX))
OPCODE(vmovd_xhim32, vmovd, vmovd, X64_ONLY, REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vmovd_r32xhi, vmovd, vmovd, X64_ONLY, REGARG(EAX), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vmovd_m32xhi, vmovd, vmovd, X64_ONLY, MEMARG(OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vmovq_xlor64, vmovq, vmovq, X64_ONLY, REGARG(XMM0), REGARG(RAX))
OPCODE(vmovq_xlom64, vmovq, vmovq, X64_ONLY, REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vmovq_r64xlo, vmovq, vmovq, X64_ONLY, REGARG(RAX), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovq_m64xlo, vmovq, vmovq, X64_ONLY, MEMARG(OPSZ_8), REGARG_PARTIAL(XMM0, OPSZ_8))
OPCODE(vmovq_xhir64, vmovq, vmovq, X64_ONLY, REGARG(XMM31), REGARG(RAX))
OPCODE(vmovq_xhim64, vmovq, vmovq, X64_ONLY, REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vmovq_r64xhi, vmovq, vmovq, X64_ONLY, REGARG(RAX), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vmovq_m64xhi, vmovq, vmovq, X64_ONLY, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vmovq_xloxlo, vmovq, vmovq, X64_ONLY, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vmovq_xhixhi, vmovq, vmovq, X64_ONLY, REGARG(XMM30), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vpmovm2b_xlok0, vpmovm2b, vpmovm2b, 0, REGARG(XMM0), REGARG(K0))
OPCODE(vpmovm2b_xhik7, vpmovm2b, vpmovm2b, X64_ONLY, REGARG(XMM31), REGARG(K7))
OPCODE(vpmovm2b_ylok0, vpmovm2b, vpmovm2b, 0, REGARG(YMM0), REGARG(K0))
OPCODE(vpmovm2b_yhik7, vpmovm2b, vpmovm2b, X64_ONLY, REGARG(YMM31), REGARG(K7))
OPCODE(vpmovm2b_zlok0, vpmovm2b, vpmovm2b, 0, REGARG(ZMM0), REGARG(K0))
OPCODE(vpmovm2b_zhik7, vpmovm2b, vpmovm2b, X64_ONLY, REGARG(ZMM31), REGARG(K7))
OPCODE(vpmovm2w_xlok0, vpmovm2w, vpmovm2w, 0, REGARG(XMM0), REGARG(K0))
OPCODE(vpmovm2w_xhik7, vpmovm2w, vpmovm2w, X64_ONLY, REGARG(XMM31), REGARG(K7))
OPCODE(vpmovm2w_ylok0, vpmovm2w, vpmovm2w, 0, REGARG(YMM0), REGARG(K0))
OPCODE(vpmovm2w_yhik7, vpmovm2w, vpmovm2w, X64_ONLY, REGARG(YMM31), REGARG(K7))
OPCODE(vpmovm2w_zlok0, vpmovm2w, vpmovm2w, 0, REGARG(ZMM0), REGARG(K0))
OPCODE(vpmovm2w_zhik7, vpmovm2w, vpmovm2w, X64_ONLY, REGARG(ZMM31), REGARG(K7))
OPCODE(vpmovm2d_xlok0, vpmovm2d, vpmovm2d, 0, REGARG(XMM0), REGARG(K0))
OPCODE(vpmovm2d_xhik7, vpmovm2d, vpmovm2d, X64_ONLY, REGARG(XMM31), REGARG(K7))
OPCODE(vpmovm2d_ylok0, vpmovm2d, vpmovm2d, 0, REGARG(YMM0), REGARG(K0))
OPCODE(vpmovm2d_yhik7, vpmovm2d, vpmovm2d, X64_ONLY, REGARG(YMM31), REGARG(K7))
OPCODE(vpmovm2d_zlok0, vpmovm2d, vpmovm2d, 0, REGARG(ZMM0), REGARG(K0))
OPCODE(vpmovm2d_zhik7, vpmovm2d, vpmovm2d, X64_ONLY, REGARG(ZMM31), REGARG(K7))
OPCODE(vpmovm2q_xlok0, vpmovm2q, vpmovm2q, 0, REGARG(XMM0), REGARG(K0))
OPCODE(vpmovm2q_xhik7, vpmovm2q, vpmovm2q, X64_ONLY, REGARG(XMM31), REGARG(K7))
OPCODE(vpmovm2q_ylok0, vpmovm2q, vpmovm2q, 0, REGARG(YMM0), REGARG(K0))
OPCODE(vpmovm2q_yhik7, vpmovm2q, vpmovm2q, X64_ONLY, REGARG(YMM31), REGARG(K7))
OPCODE(vpmovm2q_zlok0, vpmovm2q, vpmovm2q, 0, REGARG(ZMM0), REGARG(K0))
OPCODE(vpmovm2q_zhik7, vpmovm2q, vpmovm2q, X64_ONLY, REGARG(ZMM31), REGARG(K7))
OPCODE(vpmovb2m_xlok0, vpmovb2m, vpmovb2m, 0, REGARG(K0), REGARG(XMM0))
OPCODE(vpmovb2m_xhik7, vpmovb2m, vpmovb2m, X64_ONLY, REGARG(K7), REGARG(XMM31))
OPCODE(vpmovb2m_ylok0, vpmovb2m, vpmovb2m, 0, REGARG(K0), REGARG(YMM0))
OPCODE(vpmovb2m_yhik7, vpmovb2m, vpmovb2m, X64_ONLY, REGARG(K7), REGARG(YMM31))
OPCODE(vpmovb2m_zlok0, vpmovb2m, vpmovb2m, 0, REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovb2m_zhik7, vpmovb2m, vpmovb2m, X64_ONLY, REGARG(K7), REGARG(ZMM31))
OPCODE(vpmovw2m_xlok0, vpmovw2m, vpmovw2m, 0, REGARG(K0), REGARG(XMM0))
OPCODE(vpmovw2m_xhik7, vpmovw2m, vpmovw2m, X64_ONLY, REGARG(K7), REGARG(XMM31))
OPCODE(vpmovw2m_ylok0, vpmovw2m, vpmovw2m, 0, REGARG(K0), REGARG(YMM0))
OPCODE(vpmovw2m_yhik7, vpmovw2m, vpmovw2m, X64_ONLY, REGARG(K7), REGARG(YMM31))
OPCODE(vpmovw2m_zlok0, vpmovw2m, vpmovw2m, 0, REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovw2m_zhik7, vpmovw2m, vpmovw2m, X64_ONLY, REGARG(K7), REGARG(ZMM31))
OPCODE(vpmovd2m_xlok0, vpmovd2m, vpmovd2m, 0, REGARG(K0), REGARG(XMM0))
OPCODE(vpmovd2m_xhik7, vpmovd2m, vpmovd2m, X64_ONLY, REGARG(K7), REGARG(XMM31))
OPCODE(vpmovd2m_ylok0, vpmovd2m, vpmovd2m, 0, REGARG(K0), REGARG(YMM0))
OPCODE(vpmovd2m_yhik7, vpmovd2m, vpmovd2m, X64_ONLY, REGARG(K7), REGARG(YMM31))
OPCODE(vpmovd2m_zlok0, vpmovd2m, vpmovd2m, 0, REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovd2m_zhik7, vpmovd2m, vpmovd2m, X64_ONLY, REGARG(K7), REGARG(ZMM31))
OPCODE(vpmovq2m_xlok0, vpmovq2m, vpmovq2m, 0, REGARG(K0), REGARG(XMM0))
OPCODE(vpmovq2m_xhik7, vpmovq2m, vpmovq2m, X64_ONLY, REGARG(K7), REGARG(XMM31))
OPCODE(vpmovq2m_ylok0, vpmovq2m, vpmovq2m, 0, REGARG(K0), REGARG(YMM0))
OPCODE(vpmovq2m_yhik7, vpmovq2m, vpmovq2m, X64_ONLY, REGARG(K7), REGARG(YMM31))
OPCODE(vpmovq2m_zlok0, vpmovq2m, vpmovq2m, 0, REGARG(K0), REGARG(ZMM0))
OPCODE(vpmovq2m_zhik7, vpmovq2m, vpmovq2m, X64_ONLY, REGARG(K7), REGARG(ZMM31))
OPCODE(vpbroadcastmb2q_xlok0, vpbroadcastmb2q, vpbroadcastmb2q, 0, REGARG(XMM0),
       REGARG(K0))
OPCODE(vpbroadcastmb2q_xhik7, vpbroadcastmb2q, vpbroadcastmb2q, X64_ONLY, REGARG(XMM31),
       REGARG(K7))
OPCODE(vpbroadcastmb2q_ylok0, vpbroadcastmb2q, vpbroadcastmb2q, 0, REGARG(YMM0),
       REGARG(K0))
OPCODE(vpbroadcastmb2q_yhik7, vpbroadcastmb2q, vpbroadcastmb2q, X64_ONLY, REGARG(YMM31),
       REGARG(K7))
OPCODE(vpbroadcastmb2q_zlok0, vpbroadcastmb2q, vpbroadcastmb2q, 0, REGARG(ZMM0),
       REGARG(K0))
OPCODE(vpbroadcastmb2q_zhik7, vpbroadcastmb2q, vpbroadcastmb2q, X64_ONLY, REGARG(ZMM31),
       REGARG(K7))
OPCODE(vpbroadcastmw2d_xlok0, vpbroadcastmw2d, vpbroadcastmw2d, 0, REGARG(XMM0),
       REGARG(K0))
OPCODE(vpbroadcastmw2d_xhik7, vpbroadcastmw2d, vpbroadcastmw2d, X64_ONLY, REGARG(XMM31),
       REGARG(K7))
OPCODE(vpbroadcastmw2d_ylok0, vpbroadcastmw2d, vpbroadcastmw2d, 0, REGARG(YMM0),
       REGARG(K0))
OPCODE(vpbroadcastmw2d_yhik7, vpbroadcastmw2d, vpbroadcastmw2d, X64_ONLY, REGARG(YMM31),
       REGARG(K7))
OPCODE(vpbroadcastmw2d_zlok0, vpbroadcastmw2d, vpbroadcastmw2d, 0, REGARG(ZMM0),
       REGARG(K0))
OPCODE(vpbroadcastmw2d_zhik7, vpbroadcastmw2d, vpbroadcastmw2d, X64_ONLY, REGARG(ZMM31),
       REGARG(K7))
