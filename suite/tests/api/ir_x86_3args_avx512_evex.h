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

/* AVX-512 EVEX instructions with 1 destination, and 2 sources, no mask. */
OPCODE(vmovlps_xloxlom, vmovlps, vmovlps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovlps_xloxloxlo, vmovlps, vmovlps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovlps_xhixhim, vmovlps, vmovlps_NDS, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG_PARTIAL(XMM17, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovlps_xhixhixhi, vmovlps, vmovlps_NDS, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG_PARTIAL(XMM17, OPSZ_8), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vmovlpd_xloxlom, vmovlpd, vmovlpd_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovlpd_xhixhim, vmovlpd, vmovlpd_NDS, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG_PARTIAL(XMM17, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovhps_xloxlom, vmovhps, vmovhps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovhps_xloxloxlo, vmovhps, vmovhps_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovhps_xhixhim, vmovhps, vmovhps_NDS, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG_PARTIAL(XMM17, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovhps_xhixhixhi, vmovhps, vmovhps_NDS, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG_PARTIAL(XMM17, OPSZ_8), REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vmovhpd_xloxlom, vmovhpd, vmovhpd_NDS, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovhpd_xhixhim, vmovhpd, vmovhpd_NDS, X64_ONLY, REGARG_PARTIAL(XMM16, OPSZ_8),
       REGARG_PARTIAL(XMM17, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcvtsi2ss_xloxlom32, vcvtsi2ss, vcvtsi2ss, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vcvtsi2ss_xloxlom64, vcvtsi2ss, vcvtsi2ss, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_8))
OPCODE(vcvtsi2ss_xloxlor32, vcvtsi2ss, vcvtsi2ss, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG(EAX))
OPCODE(vcvtsi2ss_xloxlor64, vcvtsi2ss, vcvtsi2ss, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG(RAX))
OPCODE(vcvtsi2ss_xhixhim32, vcvtsi2ss, vcvtsi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vcvtsi2ss_xhixhim64, vcvtsi2ss, vcvtsi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_8))
OPCODE(vcvtsi2ss_xhixhir32, vcvtsi2ss, vcvtsi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), REGARG(EAX))
OPCODE(vcvtsi2ss_xhixhir64, vcvtsi2ss, vcvtsi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), REGARG(RAX))
OPCODE(vcvtsi2sd_xloxlom32, vcvtsi2sd, vcvtsi2sd, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_4))
OPCODE(vcvtsi2sd_xloxlom64, vcvtsi2sd, vcvtsi2sd, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcvtsi2sd_xloxlor32, vcvtsi2sd, vcvtsi2sd, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG(EAX))
OPCODE(vcvtsi2sd_xloxlor64, vcvtsi2sd, vcvtsi2sd, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG(RAX))
OPCODE(vcvtsi2sd_xhixhim32, vcvtsi2sd, vcvtsi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_4))
OPCODE(vcvtsi2sd_xhixhim64, vcvtsi2sd, vcvtsi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcvtsi2sd_xhixhir32, vcvtsi2sd, vcvtsi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(EAX))
OPCODE(vcvtsi2sd_xhixhir64, vcvtsi2sd, vcvtsi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(RAX))
OPCODE(vcvtusi2ss_xloxlom32, vcvtusi2ss, vcvtusi2ss, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vcvtusi2ss_xloxlom64, vcvtusi2ss, vcvtusi2ss, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_8))
OPCODE(vcvtusi2ss_xloxlor32, vcvtusi2ss, vcvtusi2ss, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG(EAX))
OPCODE(vcvtusi2ss_xloxlor64, vcvtusi2ss, vcvtusi2ss, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG(RAX))
OPCODE(vcvtusi2ss_xhixhim32, vcvtusi2ss, vcvtusi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vcvtusi2ss_xhixhim64, vcvtusi2ss, vcvtusi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_8))
OPCODE(vcvtusi2ss_xhixhir32, vcvtusi2ss, vcvtusi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), REGARG(EAX))
OPCODE(vcvtusi2ss_xhixhir64, vcvtusi2ss, vcvtusi2ss, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), REGARG(RAX))
OPCODE(vcvtusi2sd_xloxlom32, vcvtusi2sd, vcvtusi2sd, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_4))
OPCODE(vcvtusi2sd_xloxlom64, vcvtusi2sd, vcvtusi2sd, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcvtusi2sd_xloxlor32, vcvtusi2sd, vcvtusi2sd, 0, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG(EAX))
OPCODE(vcvtusi2sd_xloxlor64, vcvtusi2sd, vcvtusi2sd, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG(RAX))
OPCODE(vcvtusi2sd_xhixhim32, vcvtusi2sd, vcvtusi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_4))
OPCODE(vcvtusi2sd_xhixhim64, vcvtusi2sd, vcvtusi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vcvtusi2sd_xhixhir32, vcvtusi2sd, vcvtusi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(EAX))
OPCODE(vcvtusi2sd_xhixhir64, vcvtusi2sd, vcvtusi2sd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(RAX))
OPCODE(vextractps_rxloi, vextractps, vextractps, 0, REGARG(EAX), IMMARG(OPSZ_1),
       REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vextractps_rxhii, vextractps, vextractps, X64_ONLY, REGARG(EAX), IMMARG(OPSZ_1),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpsrldq_xloxloimm, vpsrldq, vpsrldq, 0, REGARG(XMM0), IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrldq_xloimmld, vpsrldq, vpsrldq, 0, REGARG(XMM0), IMMARG(OPSZ_1),
       MEMARG(OPSZ_16))
OPCODE(vpsrldq_xhixhiimm, vpsrldq, vpsrldq, X64_ONLY, REGARG(XMM16), IMMARG(OPSZ_1),
       REGARG(XMM17))
OPCODE(vpsrldq_xhiimmld, vpsrldq, vpsrldq, X64_ONLY, REGARG(XMM16), IMMARG(OPSZ_1),
       MEMARG(OPSZ_16))
OPCODE(vpsrldq_yloyloimm, vpsrldq, vpsrldq, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrldq_yloimmld, vpsrldq, vpsrldq, 0, REGARG(YMM0), IMMARG(OPSZ_1),
       MEMARG(OPSZ_32))
OPCODE(vpsrldq_yhiyhiimm, vpsrldq, vpsrldq, X64_ONLY, REGARG(YMM16), IMMARG(OPSZ_1),
       REGARG(YMM17))
OPCODE(vpsrldq_yhiimmld, vpsrldq, vpsrldq, X64_ONLY, REGARG(YMM16), IMMARG(OPSZ_1),
       MEMARG(OPSZ_32))
OPCODE(vpsrldq_zlozloimm, vpsrldq, vpsrldq, 0, REGARG(ZMM0), IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrldq_zloimmld, vpsrldq, vpsrldq, 0, REGARG(ZMM0), IMMARG(OPSZ_1),
       MEMARG(OPSZ_64))
OPCODE(vpsrldq_zhizhiimm, vpsrldq, vpsrldq, X64_ONLY, REGARG(ZMM16), IMMARG(OPSZ_1),
       REGARG(ZMM17))
OPCODE(vpsrldq_zhiimmld, vpsrldq, vpsrldq, X64_ONLY, REGARG(ZMM16), IMMARG(OPSZ_1),
       MEMARG(OPSZ_64))
OPCODE(vpslldq_xloxloimm, vpslldq, vpslldq, 0, REGARG(XMM0), IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpslldq_xloimmld, vpslldq, vpslldq, 0, REGARG(XMM0), IMMARG(OPSZ_1),
       MEMARG(OPSZ_16))
OPCODE(vpslldq_xhixhiimm, vpslldq, vpslldq, X64_ONLY, REGARG(XMM16), IMMARG(OPSZ_1),
       REGARG(XMM17))
OPCODE(vpslldq_xhiimmld, vpslldq, vpslldq, X64_ONLY, REGARG(XMM16), IMMARG(OPSZ_1),
       MEMARG(OPSZ_16))
OPCODE(vpslldq_yloyloimm, vpslldq, vpslldq, 0, REGARG(YMM0), IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpslldq_yloimmld, vpslldq, vpslldq, 0, REGARG(YMM0), IMMARG(OPSZ_1),
       MEMARG(OPSZ_32))
OPCODE(vpslldq_yhiyhiimm, vpslldq, vpslldq, X64_ONLY, REGARG(YMM16), IMMARG(OPSZ_1),
       REGARG(YMM17))
OPCODE(vpslldq_yhiimmld, vpslldq, vpslldq, X64_ONLY, REGARG(YMM16), IMMARG(OPSZ_1),
       MEMARG(OPSZ_32))
OPCODE(vpslldq_zlozloimm, vpslldq, vpslldq, 0, REGARG(ZMM0), IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpslldq_zloimmld, vpslldq, vpslldq, 0, REGARG(ZMM0), IMMARG(OPSZ_1),
       MEMARG(OPSZ_64))
OPCODE(vpslldq_zhizhiimm, vpslldq, vpslldq, X64_ONLY, REGARG(ZMM16), IMMARG(OPSZ_1),
       REGARG(ZMM17))
OPCODE(vpslldq_zhiimmld, vpslldq, vpslldq, X64_ONLY, REGARG(ZMM16), IMMARG(OPSZ_1),
       MEMARG(OPSZ_64))
OPCODE(vpextrw_rxlo, vpextrw, vpextrw, 0, REGARG(EAX), REGARG_PARTIAL(XMM1, OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(vpextrw_mxlo, vpextrw, vpextrw, 0, MEMARG(OPSZ_2), REGARG_PARTIAL(XMM1, OPSZ_2),
       IMMARG(OPSZ_1))
OPCODE(vpextrw_rxhi, vpextrw, vpextrw, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM31, OPSZ_2), IMMARG(OPSZ_1))
OPCODE(vpextrw_mxhi, vpextrw, vpextrw, X64_ONLY, MEMARG(OPSZ_2),
       REGARG_PARTIAL(XMM31, OPSZ_2), IMMARG(OPSZ_1))
OPCODE(vpextrb_rxlo, vpextrb, vpextrb, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(vpextrb_mxlo, vpextrb, vpextrb, 0, MEMARG(OPSZ_1), REGARG_PARTIAL(XMM0, OPSZ_1),
       IMMARG(OPSZ_1))
OPCODE(vpextrb_rxhi, vpextrb, vpextrb, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM31, OPSZ_1), IMMARG(OPSZ_1))
OPCODE(vpextrb_mxhi, vpextrb, vpextrb, X64_ONLY, MEMARG(OPSZ_1),
       REGARG_PARTIAL(XMM31, OPSZ_1), IMMARG(OPSZ_1))
OPCODE(vpextrd_rxlo, vpextrd, vpextrd, 0, REGARG(EAX), REGARG_PARTIAL(XMM0, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(vpextrd_mxlo, vpextrd, vpextrd, 0, MEMARG(OPSZ_4), REGARG_PARTIAL(XMM0, OPSZ_4),
       IMMARG(OPSZ_1))
OPCODE(vpextrd_rxhi, vpextrd, vpextrq, X64_ONLY, REGARG(EAX),
       REGARG_PARTIAL(XMM31, OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpextrd_mxhi, vpextrd, vpextrq, X64_ONLY, MEMARG(OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpextrq_rxlo, vpextrq, vpextrq, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM0, OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpextrq_mxlo, vpextrq, vpextrq, X64_ONLY, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM0, OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpextrq_rxhi, vpextrq, vpextrq, X64_ONLY, REGARG(RAX),
       REGARG_PARTIAL(XMM31, OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpextrq_mxhi, vpextrq, vpextrq, X64_ONLY, MEMARG(OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpsadbw_xloxloxlo, vpsadbw, vpsadbw, 0, REGARG(XMM0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpsadbw_xloxlold, vpsadbw, vpsadbw, 0, REGARG(XMM0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpsadbw_xhixhixhi, vpsadbw, vpsadbw, X64_ONLY, REGARG(XMM16), REGARG(XMM17),
       REGARG(XMM31))
OPCODE(vpsadbw_xhixhild, vpsadbw, vpsadbw, X64_ONLY, REGARG(XMM16), REGARG(XMM31),
       MEMARG(OPSZ_16))
OPCODE(vpsadbw_yloyloylo, vpsadbw, vpsadbw, 0, REGARG(YMM0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpsadbw_yloylold, vpsadbw, vpsadbw, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpsadbw_yhiyhiyhi, vpsadbw, vpsadbw, X64_ONLY, REGARG(YMM16), REGARG(YMM17),
       REGARG(YMM31))
OPCODE(vpsadbw_yhiyhild, vpsadbw, vpsadbw, X64_ONLY, REGARG(YMM16), REGARG(YMM31),
       MEMARG(OPSZ_32))
OPCODE(vpsadbw_zlozlozlo, vpsadbw, vpsadbw, 0, REGARG(ZMM0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpsadbw_zlozlold, vpsadbw, vpsadbw, 0, REGARG(ZMM0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpsadbw_zhizhizhi, vpsadbw, vpsadbw, X64_ONLY, REGARG(ZMM16), REGARG(ZMM17),
       REGARG(ZMM31))
OPCODE(vpsadbw_zhizhild, vpsadbw, vpsadbw, X64_ONLY, REGARG(ZMM16), REGARG(ZMM31),
       MEMARG(OPSZ_64))
