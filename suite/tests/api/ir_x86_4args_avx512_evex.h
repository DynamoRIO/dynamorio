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

/* AVX-512 EVEX instructions with 1 destination and 3 sources. */
OPCODE(vinsertps_xloxloxloi, vinsertps, vinsertps, 0, REGARG(XMM0), IMMARG(OPSZ_1),
       REGARG(XMM1), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vinsertps_xhixhixhii, vinsertps, vinsertps, X64_ONLY, REGARG(XMM16),
       IMMARG(OPSZ_1), REGARG(XMM17), REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vpinsrw_xloxlor, vpinsrw, vpinsrw, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_14),
       REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrw_xloxlom, vpinsrw, vpinsrw, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_14),
       MEMARG(OPSZ_2), IMMARG(OPSZ_1))
OPCODE(vpinsrw_xhixhir, vpinsrw, vpinsrw, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_14), REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrw_xhixhim, vpinsrw, vpinsrw, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_14), MEMARG(OPSZ_2), IMMARG(OPSZ_1))
OPCODE(vpinsrb_xloxlor, vpinsrb, vpinsrb, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_15),
       REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrb_xloxlom, vpinsrb, vpinsrb, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_15),
       MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(vpinsrb_xhixhir, vpinsrb, vpinsrb, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_15), REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrb_xhixhim, vpinsrb, vpinsrb, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_15), MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(vpinsrd_xloxlor, vpinsrd, vpinsrd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrd_xloxlom, vpinsrd, vpinsrd, 0, REGARG(XMM0), REGARG_PARTIAL(XMM1, OPSZ_12),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpinsrd_xhixhir, vpinsrd, vpinsrd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(vpinsrd_xhixhim, vpinsrd, vpinsrd, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_12), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpinsrq_xloxlor, vpinsrq, vpinsrq, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG(RAX), IMMARG(OPSZ_1))
OPCODE(vpinsrq_xloxlom, vpinsrq, vpinsrq, X64_ONLY, REGARG(XMM0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpinsrq_xhixhir, vpinsrq, vpinsrq, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), REGARG(RAX), IMMARG(OPSZ_1))
OPCODE(vpinsrq_xhixhim, vpinsrq, vpinsrq, X64_ONLY, REGARG(XMM16),
       REGARG_PARTIAL(XMM31, OPSZ_8), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
