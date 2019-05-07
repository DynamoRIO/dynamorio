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
OPCODE(vmovlps_xloxlom, vmovlps, vmovlps_NDS_evex, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovlps_xloxloxlo, vmovlps, vmovlps_NDS_evex, 0, REGARG_PARTIAL(XMM0, OPSZ_8),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vmovlps_xhixhim, vmovlps, vmovlps_NDS_evex, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vmovlps_xhixhixhi, vmovlps, vmovlps_NDS_evex, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
/* TODO i#1312: Add missing instructions. */
