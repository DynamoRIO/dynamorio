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
/* TODO i#1312: Add missing instructions. */
