/* **********************************************************
 * Copyright (c) 2019-2025 Google, Inc.  All rights reserved.
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

/* AVX-512 EVEX instructions with 1 destination, 1 mask and 2 sources.
 * Part B: Split up to avoid VS running out of memory (i#3992,i#4610).
 */
OPCODE(vfmadd231ps_xlok0xloxlo, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd231ps_ylok0yloylo, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd231ps_zlok0zlozlo, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd231ps_xhik7xhixhi, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd231ps_yhik7yhiyhi, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd231ps_zhik7zhizhi, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd231ps_xlok0xlom, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd231ps_ylok0ylom, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd231ps_zlok0zlom, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd231ps_xhik0xhim, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd231ps_yhik0yhim, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd231ps_zhik0zhim, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd231ps_xlok0xlobcst, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_ylok0ylobcst, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_zlok0zlobcst, vfmadd231ps, vfmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_xhik0xhibcst, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_yhik0yhibcst, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd231ps_zhik0zhibcst, vfmadd231ps, vfmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmadd231pd_xlok0xloxlo, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmadd231pd_ylok0yloylo, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmadd231pd_zlok0zlozlo, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmadd231pd_xhik7xhixhi, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmadd231pd_yhik7yhiyhi, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmadd231pd_zhik7zhizhi, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmadd231pd_xlok0xlom, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmadd231pd_ylok0ylom, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmadd231pd_zlok0zlom, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmadd231pd_xhik0xhim, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmadd231pd_yhik0yhim, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmadd231pd_zhik0zhim, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmadd231pd_xlok0xlobcst, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_ylok0ylobcst, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_zlok0zlobcst, vfmadd231pd, vfmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_xhik0xhibcst, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_yhik0yhibcst, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd231pd_zhik0zhibcst, vfmadd231pd, vfmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmadd132ss_xlok0xloxlo, vfmadd132ss, vfmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmadd132ss_xhik7xhixhi, vfmadd132ss, vfmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmadd132ss_xlok0xlom, vfmadd132ss, vfmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd132ss_xhik0xhim, vfmadd132ss, vfmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd132sd_xlok0xloxlo, vfmadd132sd, vfmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmadd132sd_xhik7xhixhi, vfmadd132sd, vfmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmadd132sd_xlok0xlom, vfmadd132sd, vfmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd132sd_xhik0xhim, vfmadd132sd, vfmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd213ss_xlok0xloxlo, vfmadd213ss, vfmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmadd213ss_xhik7xhixhi, vfmadd213ss, vfmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmadd213ss_xlok0xlom, vfmadd213ss, vfmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd213ss_xhik0xhim, vfmadd213ss, vfmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd213sd_xlok0xloxlo, vfmadd213sd, vfmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmadd213sd_xhik7xhixhi, vfmadd213sd, vfmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmadd213sd_xlok0xlom, vfmadd213sd, vfmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd213sd_xhik0xhim, vfmadd213sd, vfmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd231ss_xlok0xloxlo, vfmadd231ss, vfmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmadd231ss_xhik7xhixhi, vfmadd231ss, vfmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmadd231ss_xlok0xlom, vfmadd231ss, vfmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd231ss_xhik0xhim, vfmadd231ss, vfmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmadd231sd_xlok0xloxlo, vfmadd231sd, vfmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmadd231sd_xhik7xhixhi, vfmadd231sd, vfmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmadd231sd_xlok0xlom, vfmadd231sd, vfmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmadd231sd_xhik0xhim, vfmadd231sd, vfmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmaddsub132ps_xlok0xloxlo, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub132ps_ylok0yloylo, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub132ps_zlok0zlozlo, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub132ps_xhik7xhixhi, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub132ps_yhik7yhiyhi, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub132ps_zhik7zhizhi, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub132ps_xlok0xlom, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132ps_ylok0ylom, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132ps_zlok0zlom, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132ps_xhik7xhim, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132ps_yhik7yhim, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132ps_zhik7zhim, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132ps_xlok0xlobcst, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_ylok0ylobcst, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_zlok0zlobcst, vfmaddsub132ps, vfmaddsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_xhik7xhibcst, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_yhik7yhibcst, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132ps_zhik7zhibcst, vfmaddsub132ps, vfmaddsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_xlok0xloxlo, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub213ps_ylok0yloylo, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub213ps_zlok0zlozlo, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub213ps_xhik7xhixhi, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub213ps_yhik7yhiyhi, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub213ps_zhik7zhizhi, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub213ps_xlok0xlom, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213ps_ylok0ylom, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213ps_zlok0zlom, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213ps_xhik7xhim, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213ps_yhik7yhim, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213ps_zhik7zhim, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213ps_xlok0xlobcst, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_ylok0ylobcst, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_zlok0zlobcst, vfmaddsub213ps, vfmaddsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_xhik7xhibcst, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_yhik7yhibcst, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub213ps_zhik7zhibcst, vfmaddsub213ps, vfmaddsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_xlok0xloxlo, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub231ps_ylok0yloylo, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub231ps_zlok0zlozlo, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub231ps_xhik7xhixhi, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub231ps_yhik7yhiyhi, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub231ps_zhik7zhizhi, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub231ps_xlok0xlom, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231ps_ylok0ylom, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231ps_zlok0zlom, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231ps_xhik7xhim, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231ps_yhik7yhim, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231ps_zhik7zhim, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231ps_xlok0xlobcst, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_ylok0ylobcst, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_zlok0zlobcst, vfmaddsub231ps, vfmaddsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_xhik7xhibcst, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_yhik7yhibcst, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub231ps_zhik7zhibcst, vfmaddsub231ps, vfmaddsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmaddsub132pd_xlok0xloxlo, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub132pd_ylok0yloylo, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub132pd_zlok0zlozlo, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub132pd_xhik7xhixhi, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub132pd_yhik7yhiyhi, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub132pd_zhik7zhizhi, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub132pd_xlok0xlom, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132pd_ylok0ylom, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132pd_zlok0zlom, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132pd_xhik0xhim, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub132pd_yhik0yhim, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub132pd_zhik0zhim, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub132pd_xlok0xlobcst, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_ylok0ylobcst, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_zlok0zlobcst, vfmaddsub132pd, vfmaddsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_xhik0xhibcst, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_yhik0yhibcst, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub132pd_zhik0zhibcst, vfmaddsub132pd, vfmaddsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_xlok0xloxlo, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub213pd_ylok0yloylo, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub213pd_zlok0zlozlo, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub213pd_xhik7xhixhi, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub213pd_yhik7yhiyhi, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub213pd_zhik7zhizhi, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub213pd_xlok0xlom, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213pd_ylok0ylom, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213pd_zlok0zlom, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213pd_xhik7xhim, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub213pd_yhik7yhim, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub213pd_zhik7zhim, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub213pd_xlok0xlobcst, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_ylok0ylobcst, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_zlok0zlobcst, vfmaddsub213pd, vfmaddsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_xhik7xhibcst, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_yhik7yhibcst, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub213pd_zhik7zhibcst, vfmaddsub213pd, vfmaddsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_xlok0xloxlo, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmaddsub231pd_ylok0yloylo, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmaddsub231pd_zlok0zlozlo, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmaddsub231pd_xhik7xhixhi, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmaddsub231pd_yhik7yhiyhi, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmaddsub231pd_zhik7zhizhi, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmaddsub231pd_xlok0xlom, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231pd_ylok0ylom, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231pd_zlok0zlom, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231pd_xhik7xhim, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmaddsub231pd_yhik7yhim, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmaddsub231pd_zhik7zhim, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmaddsub231pd_xlok0xlobcst, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_ylok0ylobcst, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_zlok0zlobcst, vfmaddsub231pd, vfmaddsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_xhik7xhibcst, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_yhik7yhibcst, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmaddsub231pd_zhik7zhibcst, vfmaddsub231pd, vfmaddsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132ps_xlok0xloxlo, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd132ps_ylok0yloylo, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd132ps_zlok0zlozlo, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd132ps_xhik7xhixhi, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd132ps_yhik7yhiyhi, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd132ps_zhik7zhizhi, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd132ps_xlok0xlom, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132ps_ylok0ylom, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132ps_zlok0zlom, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132ps_xhik7xhim, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132ps_yhik7yhim, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132ps_zhik7zhim, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132ps_xlok0xlobcst, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_ylok0ylobcst, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_zlok0zlobcst, vfmsubadd132ps, vfmsubadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_xhik7xhibcst, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_yhik7yhibcst, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132ps_zhik7zhibcst, vfmsubadd132ps, vfmsubadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_xlok0xloxlo, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd213ps_ylok0yloylo, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd213ps_zlok0zlozlo, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd213ps_xhik7xhixhi, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd213ps_yhik7yhiyhi, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd213ps_zhik7zhizhi, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd213ps_xlok0xlom, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213ps_ylok0ylom, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213ps_zlok0zlom, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213ps_xhik7xhim, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213ps_yhik7yhim, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213ps_zhik7zhim, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213ps_xlok0xlobcst, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_ylok0ylobcst, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_zlok0zlobcst, vfmsubadd213ps, vfmsubadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_xhik7xhibcst, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_yhik7yhibcst, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd213ps_zhik7zhibcst, vfmsubadd213ps, vfmsubadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_xlok0xloxlo, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd231ps_ylok0yloylo, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd231ps_zlok0zlozlo, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd231ps_xhik7xhixhi, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd231ps_yhik7yhiyhi, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd231ps_zhik7zhizhi, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd231ps_xlok0xlom, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231ps_ylok0ylom, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231ps_zlok0zlom, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231ps_xhik0xhim, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231ps_yhik0yhim, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231ps_zhik0zhim, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231ps_xlok0xlobcst, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_ylok0ylobcst, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_zlok0zlobcst, vfmsubadd231ps, vfmsubadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_xhik0xhibcst, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_yhik0yhibcst, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd231ps_zhik0zhibcst, vfmsubadd231ps, vfmsubadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsubadd132pd_xlok0xloxlo, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd132pd_ylok0yloylo, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd132pd_zlok0zlozlo, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd132pd_xhik7xhixhi, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd132pd_yhik7yhiyhi, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd132pd_zhik7zhizhi, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd132pd_xlok0xlom, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132pd_ylok0ylom, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132pd_zlok0zlom, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132pd_xhik7xhim, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd132pd_yhik7yhim, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd132pd_zhik7zhim, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd132pd_xlok0xlobcst, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_ylok0ylobcst, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_zlok0zlobcst, vfmsubadd132pd, vfmsubadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_xhik7xhibcst, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_yhik7yhibcst, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd132pd_zhik7zhibcst, vfmsubadd132pd, vfmsubadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_xlok0xloxlo, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd213pd_ylok0yloylo, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd213pd_zlok0zlozlo, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd213pd_xhik7xhixhi, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd213pd_yhik7yhiyhi, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd213pd_zhik7zhizhi, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd213pd_xlok0xlom, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213pd_ylok0ylom, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213pd_zlok0zlom, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213pd_xhik7xhim, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd213pd_yhik7yhim, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd213pd_zhik7zhim, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd213pd_xlok0xlobcst, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_ylok0ylobcst, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_zlok0zlobcst, vfmsubadd213pd, vfmsubadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_xhik7xhibcst, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_yhik7yhibcst, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd213pd_zhik7zhibcst, vfmsubadd213pd, vfmsubadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_xlok0xloxlo, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsubadd231pd_ylok0yloylo, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsubadd231pd_zlok0zlozlo, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsubadd231pd_xhik7xhixhi, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsubadd231pd_yhik7yhiyhi, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsubadd231pd_zhik7zhizhi, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsubadd231pd_xlok0xlom, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231pd_ylok0ylom, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231pd_zlok0zlom, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231pd_xhik7xhim, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsubadd231pd_yhik7yhim, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsubadd231pd_zhik7zhim, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsubadd231pd_xlok0xlobcst, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_ylok0ylobcst, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_zlok0zlobcst, vfmsubadd231pd, vfmsubadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_xhik7xhibcst, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_yhik7yhibcst, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsubadd231pd_zhik7zhibcst, vfmsubadd231pd, vfmsubadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132ps_xlok0xloxlo, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub132ps_ylok0yloylo, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub132ps_zlok0zlozlo, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub132ps_xhik7xhixhi, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub132ps_yhik7yhiyhi, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub132ps_zhik7zhizhi, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub132ps_xlok0xlom, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub132ps_ylok0ylom, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub132ps_zlok0zlom, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub132ps_xhik7xhim, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub132ps_yhik7yhim, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub132ps_zhik7zhim, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub132ps_xlok0xlobcst, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_ylok0ylobcst, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_zlok0zlobcst, vfmsub132ps, vfmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_xhik7xhibcst, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_yhik7yhibcst, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub132ps_zhik7zhibcst, vfmsub132ps, vfmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_xlok0xloxlo, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub213ps_ylok0yloylo, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub213ps_zlok0zlozlo, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub213ps_xhik7xhixhi, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub213ps_yhik7yhiyhi, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub213ps_zhik7zhizhi, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub213ps_xlok0xlom, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub213ps_ylok0ylom, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub213ps_zlok0zlom, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub213ps_xhik7xhim, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub213ps_yhik7yhim, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub213ps_zhik7zhim, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub213ps_xlok0xlobcst, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_ylok0ylobcst, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_zlok0zlobcst, vfmsub213ps, vfmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_xhik7xhibcst, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_yhik7yhibcst, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub213ps_zhik7zhibcst, vfmsub213ps, vfmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_xlok0xloxlo, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub231ps_ylok0yloylo, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub231ps_zlok0zlozlo, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub231ps_xhik7xhixhi, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub231ps_yhik7yhiyhi, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub231ps_zhik7zhizhi, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub231ps_xlok0xlom, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub231ps_ylok0ylom, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub231ps_zlok0zlom, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub231ps_xhik0xhim, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub231ps_yhik0yhim, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub231ps_zhik7zhim, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub231ps_xlok0xlobcst, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_ylok0ylobcst, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_zlok0zlobcst, vfmsub231ps, vfmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_xhik0xhibcst, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_yhik0yhibcst, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub231ps_zhik7zhibcst, vfmsub231ps, vfmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfmsub132pd_xlok0xloxlo, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub132pd_ylok0yloylo, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub132pd_zlok0zlozlo, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub132pd_xhik7xhixhi, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub132pd_yhik7yhiyhi, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub132pd_zhik7zhizhi, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub132pd_xlok0xlom, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub132pd_ylok0ylom, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub132pd_zlok0zlom, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub132pd_xhik7xhim, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub132pd_yhik7yhim, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub132pd_zhik7zhim, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub132pd_xlok0xlobcst, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_ylok0ylobcst, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_zlok0zlobcst, vfmsub132pd, vfmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_xhik7xhibcst, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_yhik7yhibcst, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132pd_zhik7zhibcst, vfmsub132pd, vfmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_xlok0xloxlo, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub213pd_ylok0yloylo, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub213pd_zlok0zlozlo, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub213pd_xhik7xhixhi, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub213pd_yhik7yhiyhi, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub213pd_zhik7zhizhi, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub213pd_xlok0xlom, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub213pd_ylok0ylom, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub213pd_zlok0zlom, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub213pd_xhik7xhim, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub213pd_yhik7yhim, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub213pd_zhik7zhim, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub213pd_xlok0xlobcst, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_ylok0ylobcst, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_zlok0zlobcst, vfmsub213pd, vfmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_xhik7xhibcst, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_yhik7yhibcst, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub213pd_zhik7zhibcst, vfmsub213pd, vfmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_xlok0xloxlo, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfmsub231pd_ylok0yloylo, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfmsub231pd_zlok0zlozlo, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfmsub231pd_xhik7xhixhi, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfmsub231pd_yhik7yhiyhi, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfmsub231pd_zhik7zhizhi, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfmsub231pd_xlok0xlom, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfmsub231pd_ylok0ylom, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfmsub231pd_zlok0zlom, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfmsub231pd_xhik7xhim, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfmsub231pd_yhik7yhim, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfmsub231pd_zhik7zhim, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfmsub231pd_xlok0xlobcst, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_ylok0ylobcst, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_zlok0zlobcst, vfmsub231pd, vfmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_xhik7xhibcst, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_yhik7yhibcst, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub231pd_zhik7zhibcst, vfmsub231pd, vfmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfmsub132ss_xlok0xloxlo, vfmsub132ss, vfmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmsub132ss_xhik7xhixhi, vfmsub132ss, vfmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmsub132ss_xlok0xlom, vfmsub132ss, vfmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub132ss_xhik0xhim, vfmsub132ss, vfmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub132sd_xlok0xloxlo, vfmsub132sd, vfmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmsub132sd_xhik7xhixhi, vfmsub132sd, vfmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmsub132sd_xlok0xlom, vfmsub132sd, vfmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub132sd_xhik0xhim, vfmsub132sd, vfmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub213ss_xlok0xloxlo, vfmsub213ss, vfmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmsub213ss_xhik7xhixhi, vfmsub213ss, vfmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmsub213ss_xlok0xlom, vfmsub213ss, vfmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub213ss_xhik0xhim, vfmsub213ss, vfmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub213sd_xlok0xloxlo, vfmsub213sd, vfmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmsub213sd_xhik7xhixhi, vfmsub213sd, vfmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmsub213sd_xlok0xlom, vfmsub213sd, vfmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub213sd_xhik0xhim, vfmsub213sd, vfmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub231ss_xlok0xloxlo, vfmsub231ss, vfmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfmsub231ss_xhik7xhixhi, vfmsub231ss, vfmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfmsub231ss_xlok0xlom, vfmsub231ss, vfmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub231ss_xhik0xhim, vfmsub231ss, vfmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfmsub231sd_xlok0xloxlo, vfmsub231sd, vfmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfmsub231sd_xhik7xhixhi, vfmsub231sd, vfmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfmsub231sd_xlok0xlom, vfmsub231sd, vfmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfmsub231sd_xhik0xhim, vfmsub231sd, vfmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd132ps_xlok0xloxlo, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd132ps_ylok0yloylo, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd132ps_zlok0zlozlo, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd132ps_xhik7xhixhi, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd132ps_yhik7yhiyhi, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd132ps_zhik7zhizhi, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd132ps_xlok0xlom, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd132ps_ylok0ylom, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd132ps_zlok0zlom, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd132ps_xhik7xhim, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd132ps_yhik7yhim, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd132ps_zhik7zhim, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd132ps_xlok0xlobcst, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_ylok0ylobcst, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_zlok0zlobcst, vfnmadd132ps, vfnmadd132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_xhik7xhibcst, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_yhik7yhibcst, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd132ps_zhik7zhibcst, vfnmadd132ps, vfnmadd132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd132pd_xlok0xloxlo, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd132pd_ylok0yloylo, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd132pd_zlok0zlozlo, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd132pd_xhik7xhixhi, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd132pd_yhik7yhiyhi, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd132pd_zhik7zhizhi, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd132pd_xlok0xlom, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd132pd_ylok0ylom, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd132pd_zlok0zlom, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd132pd_xhik7xhim, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd132pd_yhik7yhim, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd132pd_zhik7zhim, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd132pd_xlok0xlobcst, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_ylok0ylobcst, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_zlok0zlobcst, vfnmadd132pd, vfnmadd132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_xhik7xhibcst, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_yhik7yhibcst, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd132pd_zhik7zhibcst, vfnmadd132pd, vfnmadd132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd213ps_xlok0xloxlo, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd213ps_ylok0yloylo, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd213ps_zlok0zlozlo, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd213ps_xhik7xhixhi, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd213ps_yhik7yhiyhi, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd213ps_zhik7zhizhi, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd213ps_xlok0xlom, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd213ps_ylok0ylom, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd213ps_zlok0zlom, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd213ps_xhik7xhim, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd213ps_yhik7yhim, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd213ps_zhik7zhim, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd213ps_xlok0xlobcst, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_ylok0ylobcst, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_zlok0zlobcst, vfnmadd213ps, vfnmadd213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_xhik7xhibcst, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_yhik7yhibcst, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd213ps_zhik7zhibcst, vfnmadd213ps, vfnmadd213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd213pd_xlok0xloxlo, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd213pd_ylok0yloylo, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd213pd_zlok0zlozlo, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd213pd_xhik7xhixhi, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd213pd_yhik7yhiyhi, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd213pd_zhik7zhizhi, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd213pd_xlok0xlom, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd213pd_ylok0ylom, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd213pd_zlok0zlom, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd213pd_xhik7xhim, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd213pd_yhik7yhim, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd213pd_zhik7zhim, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd213pd_xlok0xlobcst, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_ylok0ylobcst, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_zlok0zlobcst, vfnmadd213pd, vfnmadd213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_xhik7xhibcst, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_yhik7yhibcst, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd213pd_zhik7zhibcst, vfnmadd213pd, vfnmadd213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd231ps_xlok0xloxlo, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd231ps_ylok0yloylo, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd231ps_zlok0zlozlo, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd231ps_xhik7xhixhi, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd231ps_yhik7yhiyhi, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd231ps_zhik7zhizhi, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd231ps_xlok0xlom, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd231ps_ylok0ylom, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd231ps_zlok0zlom, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd231ps_xhik7xhim, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd231ps_yhik7yhim, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd231ps_zhik7zhim, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd231ps_xlok0xlobcst, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_ylok0ylobcst, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_zlok0zlobcst, vfnmadd231ps, vfnmadd231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_xhik7xhibcst, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_yhik7yhibcst, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd231ps_zhik7zhibcst, vfnmadd231ps, vfnmadd231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmadd231pd_xlok0xloxlo, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmadd231pd_ylok0yloylo, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmadd231pd_zlok0zlozlo, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmadd231pd_xhik7xhixhi, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmadd231pd_yhik7yhiyhi, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmadd231pd_zhik7zhizhi, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmadd231pd_xlok0xlom, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmadd231pd_ylok0ylom, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmadd231pd_zlok0zlom, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmadd231pd_xhik7xhim, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmadd231pd_yhik7yhim, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmadd231pd_zhik7zhim, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmadd231pd_xlok0xlobcst, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_ylok0ylobcst, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_zlok0zlobcst, vfnmadd231pd, vfnmadd231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_xhik7xhibcst, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_yhik7yhibcst, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd231pd_zhik7zhibcst, vfnmadd231pd, vfnmadd231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmadd132ss_xlok0xloxlo, vfnmadd132ss, vfnmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmadd132ss_xhik7xhixhi, vfnmadd132ss, vfnmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmadd132ss_xlok0xlom, vfnmadd132ss, vfnmadd132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd132ss_xhik0xhim, vfnmadd132ss, vfnmadd132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd132sd_xlok0xloxlo, vfnmadd132sd, vfnmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmadd132sd_xhik7xhixhi, vfnmadd132sd, vfnmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmadd132sd_xlok0xlom, vfnmadd132sd, vfnmadd132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd132sd_xhik0xhim, vfnmadd132sd, vfnmadd132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd213ss_xlok0xloxlo, vfnmadd213ss, vfnmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmadd213ss_xhik7xhixhi, vfnmadd213ss, vfnmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmadd213ss_xlok0xlom, vfnmadd213ss, vfnmadd213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd213ss_xhik0xhim, vfnmadd213ss, vfnmadd213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd213sd_xlok0xloxlo, vfnmadd213sd, vfnmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmadd213sd_xhik7xhixhi, vfnmadd213sd, vfnmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmadd213sd_xlok0xlom, vfnmadd213sd, vfnmadd213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd213sd_xhik0xhim, vfnmadd213sd, vfnmadd213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd231ss_xlok0xloxlo, vfnmadd231ss, vfnmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmadd231ss_xhik7xhixhi, vfnmadd231ss, vfnmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmadd231ss_xlok0xlom, vfnmadd231ss, vfnmadd231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd231ss_xhik0xhim, vfnmadd231ss, vfnmadd231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmadd231sd_xlok0xloxlo, vfnmadd231sd, vfnmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmadd231sd_xhik7xhixhi, vfnmadd231sd, vfnmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmadd231sd_xlok0xlom, vfnmadd231sd, vfnmadd231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmadd231sd_xhik0xhim, vfnmadd231sd, vfnmadd231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub132ps_xlok0xloxlo, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub132ps_ylok0yloylo, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub132ps_zlok0zlozlo, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub132ps_xhik7xhixhi, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub132ps_yhik7yhiyhi, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub132ps_zhik7zhizhi, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub132ps_xlok0xlom, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub132ps_ylok0ylom, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub132ps_zlok0zlom, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub132ps_xhik7xhim, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub132ps_yhik7yhim, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub132ps_zhik7zhim, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub132ps_xlok0xlobcst, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_ylok0ylobcst, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_zlok0zlobcst, vfnmsub132ps, vfnmsub132ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_xhik7xhibcst, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_yhik7yhibcst, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub132ps_zhik7zhibcst, vfnmsub132ps, vfnmsub132ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_xlok0xloxlo, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub213ps_ylok0yloylo, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub213ps_zlok0zlozlo, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub213ps_xhik7xhixhi, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub213ps_yhik7yhiyhi, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub213ps_zhik7zhizhi, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub213ps_xlok0xlom, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub213ps_ylok0ylom, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub213ps_zlok0zlom, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub213ps_xhik7xhim, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub213ps_yhik7yhim, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub213ps_zhik7zhim, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub213ps_xlok0xlobcst, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_ylok0ylobcst, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_zlok0zlobcst, vfnmsub213ps, vfnmsub213ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_xhik7xhibcst, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_yhik7yhibcst, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub213ps_zhik7zhibcst, vfnmsub213ps, vfnmsub213ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_xlok0xloxlo, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub231ps_ylok0yloylo, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub231ps_zlok0zlozlo, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub231ps_xhik7xhixhi, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub231ps_yhik7yhiyhi, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub231ps_zhik7zhizhi, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub231ps_xlok0xlom, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub231ps_ylok0ylom, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub231ps_zlok0zlom, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub231ps_xhik7xhim, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub231ps_yhik7yhim, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub231ps_zhik7zhim, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub231ps_xlok0xlobcst, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_ylok0ylobcst, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_zlok0zlobcst, vfnmsub231ps, vfnmsub231ps_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_xhik7xhibcst, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_yhik7yhibcst, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub231ps_zhik7zhibcst, vfnmsub231ps, vfnmsub231ps_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vfnmsub132pd_xlok0xloxlo, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub132pd_ylok0yloylo, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub132pd_zlok0zlozlo, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub132pd_xhik7xhixhi, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub132pd_yhik7yhiyhi, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub132pd_zhik7zhizhi, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub132pd_xlok0xlom, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub132pd_ylok0ylom, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub132pd_zlok0zlom, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub132pd_xhik7xhim, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub132pd_yhik7yhim, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub132pd_zhik7zhim, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub132pd_xlok0xlobcst, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_ylok0ylobcst, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_zlok0zlobcst, vfnmsub132pd, vfnmsub132pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_xhik7xhibcst, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_yhik7yhibcst, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub132pd_zhik7zhibcst, vfnmsub132pd, vfnmsub132pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_xlok0xloxlo, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub213pd_ylok0yloylo, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub213pd_zlok0zlozlo, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub213pd_xhik7xhixhi, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub213pd_yhik7yhiyhi, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub213pd_zhik7zhizhi, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub213pd_xlok0xlom, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub213pd_ylok0ylom, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub213pd_zlok0zlom, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub213pd_xhik7xhim, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub213pd_yhik7yhim, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub213pd_zhik7zhim, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub213pd_xlok0xlobcst, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_ylok0ylobcst, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_zlok0zlobcst, vfnmsub213pd, vfnmsub213pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_xhik7xhibcst, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_yhik7yhibcst, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub213pd_zhik7zhibcst, vfnmsub213pd, vfnmsub213pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_xlok0xloxlo, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vfnmsub231pd_ylok0yloylo, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vfnmsub231pd_zlok0zlozlo, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vfnmsub231pd_xhik7xhixhi, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vfnmsub231pd_yhik7yhiyhi, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vfnmsub231pd_zhik7zhizhi, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vfnmsub231pd_xlok0xlom, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vfnmsub231pd_ylok0ylom, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vfnmsub231pd_zlok0zlom, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vfnmsub231pd_xhik7xhim, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vfnmsub231pd_yhik7yhim, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vfnmsub231pd_zhik7zhim, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vfnmsub231pd_xlok0xlobcst, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_ylok0ylobcst, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_zlok0zlobcst, vfnmsub231pd, vfnmsub231pd_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_xhik7xhibcst, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY,
       REGARG(XMM16), REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_yhik7yhibcst, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY,
       REGARG(YMM16), REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub231pd_zhik7zhibcst, vfnmsub231pd, vfnmsub231pd_mask, X64_ONLY,
       REGARG(ZMM16), REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vfnmsub132ss_xlok0xloxlo, vfnmsub132ss, vfnmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmsub132ss_xhik7xhixhi, vfnmsub132ss, vfnmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmsub132ss_xlok0xlom, vfnmsub132ss, vfnmsub132ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub132ss_xhik0xhim, vfnmsub132ss, vfnmsub132ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub132sd_xlok0xloxlo, vfnmsub132sd, vfnmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmsub132sd_xhik7xhixhi, vfnmsub132sd, vfnmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmsub132sd_xlok0xlom, vfnmsub132sd, vfnmsub132sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub132sd_xhik0xhim, vfnmsub132sd, vfnmsub132sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub213ss_xlok0xloxlo, vfnmsub213ss, vfnmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmsub213ss_xhik7xhixhi, vfnmsub213ss, vfnmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmsub213ss_xlok0xlom, vfnmsub213ss, vfnmsub213ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub213ss_xhik0xhim, vfnmsub213ss, vfnmsub213ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub213sd_xlok0xloxlo, vfnmsub213sd, vfnmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmsub213sd_xhik7xhixhi, vfnmsub213sd, vfnmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmsub213sd_xlok0xlom, vfnmsub213sd, vfnmsub213sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub213sd_xhik0xhim, vfnmsub213sd, vfnmsub213sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub231ss_xlok0xloxlo, vfnmsub231ss, vfnmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vfnmsub231ss_xhik7xhixhi, vfnmsub231ss, vfnmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_4),
       REGARG_PARTIAL(XMM31, OPSZ_4))
OPCODE(vfnmsub231ss_xlok0xlom, vfnmsub231ss, vfnmsub231ss_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_4), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub231ss_xhik0xhim, vfnmsub231ss, vfnmsub231ss_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_4), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_4),
       MEMARG(OPSZ_4))
OPCODE(vfnmsub231sd_xlok0xloxlo, vfnmsub231sd, vfnmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vfnmsub231sd_xhik7xhixhi, vfnmsub231sd, vfnmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM17, OPSZ_8),
       REGARG_PARTIAL(XMM31, OPSZ_8))
OPCODE(vfnmsub231sd_xlok0xlom, vfnmsub231sd, vfnmsub231sd_mask, 0,
       REGARG_PARTIAL(XMM0, OPSZ_8), REGARG(K0), REGARG_PARTIAL(XMM1, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vfnmsub231sd_xhik0xhim, vfnmsub231sd, vfnmsub231sd_mask, X64_ONLY,
       REGARG_PARTIAL(XMM16, OPSZ_8), REGARG(K7), REGARG_PARTIAL(XMM31, OPSZ_8),
       MEMARG(OPSZ_8))
OPCODE(vpshldvw_xlok0xloxlo, vpshldvw, vpshldvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshldvw_xlok0xlold, vpshldvw, vpshldvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshldvw_xhik0xhixhi, vpshldvw, vpshldvw_mask, X64_ONLY, REGARG(XMM31), REGARG(K0),
       REGARG(XMM16), REGARG(XMM17))
OPCODE(vpshldvw_xlok7xloxlo, vpshldvw, vpshldvw_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshldvw_ylok0yloylo, vpshldvw, vpshldvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshldvw_ylok0ylold, vpshldvw, vpshldvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpshldvw_yhik0yhiyhi, vpshldvw, vpshldvw_mask, X64_ONLY, REGARG(YMM31), REGARG(K0),
       REGARG(YMM16), REGARG(YMM17))
OPCODE(vpshldvw_ylok7yloylo, vpshldvw, vpshldvw_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshldvw_zlok0zlozlo, vpshldvw, vpshldvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshldvw_zlok0zlold, vpshldvw, vpshldvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpshldvw_zhik0zhizhi, vpshldvw, vpshldvw_mask, X64_ONLY, REGARG(ZMM31), REGARG(K0),
       REGARG(ZMM16), REGARG(ZMM17))
OPCODE(vpshldvw_zlok7zlozlo, vpshldvw, vpshldvw_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshldvd_xlok0xloxlo, vpshldvd, vpshldvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshldvd_xlok0xlold, vpshldvd, vpshldvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshldvd_xlok0xlobcst, vpshldvd, vpshldvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpshldvd_xhik0xhixhi, vpshldvd, vpshldvd_mask, X64_ONLY, REGARG(XMM31), REGARG(K0),
       REGARG(XMM16), REGARG(XMM17))
OPCODE(vpshldvd_xlok7xloxlo, vpshldvd, vpshldvd_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshldvd_ylok0yloylo, vpshldvd, vpshldvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshldvd_ylok0ylold, vpshldvd, vpshldvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpshldvd_ylok0ylobcst, vpshldvd, vpshldvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpshldvd_yhik0yhiyhi, vpshldvd, vpshldvd_mask, X64_ONLY, REGARG(YMM31), REGARG(K0),
       REGARG(YMM16), REGARG(YMM17))
OPCODE(vpshldvd_ylok7yloylo, vpshldvd, vpshldvd_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshldvd_zlok0zlozlo, vpshldvd, vpshldvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshldvd_zlok0zlold, vpshldvd, vpshldvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpshldvd_zlok0zlobcst, vpshldvd, vpshldvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpshldvd_zhik0zhizhi, vpshldvd, vpshldvd_mask, X64_ONLY, REGARG(ZMM31), REGARG(K0),
       REGARG(ZMM16), REGARG(ZMM17))
OPCODE(vpshldvd_zlok7zlozlo, vpshldvd, vpshldvd_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshrdvw_xlok0xloxlo, vpshrdvw, vpshrdvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshrdvw_xlok0xlold, vpshrdvw, vpshrdvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshrdvw_xhik0xhixhi, vpshrdvw, vpshrdvw_mask, X64_ONLY, REGARG(XMM31), REGARG(K0),
       REGARG(XMM16), REGARG(XMM17))
OPCODE(vpshrdvw_xlok7xloxlo, vpshrdvw, vpshrdvw_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshrdvw_ylok0yloylo, vpshrdvw, vpshrdvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshrdvw_ylok0ylold, vpshrdvw, vpshrdvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpshrdvw_yhik0yhiyhi, vpshrdvw, vpshrdvw_mask, X64_ONLY, REGARG(YMM31), REGARG(K0),
       REGARG(YMM16), REGARG(YMM17))
OPCODE(vpshrdvw_ylok7yloylo, vpshrdvw, vpshrdvw_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshrdvw_zlok0zlozlo, vpshrdvw, vpshrdvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshrdvw_zlok0zlold, vpshrdvw, vpshrdvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpshrdvw_zhik0zhizhi, vpshrdvw, vpshrdvw_mask, X64_ONLY, REGARG(ZMM31), REGARG(K0),
       REGARG(ZMM16), REGARG(ZMM17))
OPCODE(vpshrdvw_zlok7zlozlo, vpshrdvw, vpshrdvw_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshrdvd_xlok0xloxlo, vpshrdvd, vpshrdvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshrdvd_xlok0xlold, vpshrdvd, vpshrdvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpshrdvd_xlok0xlobcst, vpshrdvd, vpshrdvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpshrdvd_xhik0xhixhi, vpshrdvd, vpshrdvd_mask, X64_ONLY, REGARG(XMM31), REGARG(K0),
       REGARG(XMM16), REGARG(XMM17))
OPCODE(vpshrdvd_xlok7xloxlo, vpshrdvd, vpshrdvd_mask, 0, REGARG(XMM0), REGARG(K7),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpshrdvd_ylok0yloylo, vpshrdvd, vpshrdvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshrdvd_ylok0ylold, vpshrdvd, vpshrdvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpshrdvd_ylok0ylobcst, vpshrdvd, vpshrdvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpshrdvd_yhik0yhiyhi, vpshrdvd, vpshrdvd_mask, X64_ONLY, REGARG(YMM31), REGARG(K0),
       REGARG(YMM16), REGARG(YMM17))
OPCODE(vpshrdvd_ylok7yloylo, vpshrdvd, vpshrdvd_mask, 0, REGARG(YMM0), REGARG(K7),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpshrdvd_zlok0zlozlo, vpshrdvd, vpshrdvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpshrdvd_zlok0zlold, vpshrdvd, vpshrdvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpshrdvd_zlok0zlobcst, vpshrdvd, vpshrdvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpshrdvd_zhik0zhizhi, vpshrdvd, vpshrdvd_mask, X64_ONLY, REGARG(ZMM31), REGARG(K0),
       REGARG(ZMM16), REGARG(ZMM17))
OPCODE(vpshrdvd_zlok7zlozlo, vpshrdvd, vpshrdvd_mask, 0, REGARG(ZMM0), REGARG(K7),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilps_xlok0xloxlo, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermilps_xlok0xlold, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermilps_xlok0xlobcst, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermilps_ylok0yloylo, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermilps_ylok0ylold, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermilps_ylok0ylobcst, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermilps_zlok0zlozlo, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilps_zlok0zlold, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermilps_zlok0zlobcst, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermilps_xhik7xhixhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermilps_xhik7xhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermilps_xhik7xhibcst, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermilps_yhik7yhiyhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermilps_yhik7yhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermilps_yhik7yhibcst, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermilps_zhik7zhizhi, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermilps_zhik7zhild, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermilps_zhik7zhibcst, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermilps_xlok0xloimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_xlok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_xlok0bcstimm, vpermilps, vpermilps_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0yloimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilps_ylok0bcstimm, vpermilps, vpermilps_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0zloimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0ldimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilps_zlok0bcstimm, vpermilps, vpermilps_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7xhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilps_xhik7bcstimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7yhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilps_yhik7bcstimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7zhiimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7ldimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilps_zhik7bcstimm, vpermilps, vpermilps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0xloxlo, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermilpd_xlok0xlold, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermilpd_xlok0xlobcst, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpermilpd_ylok0yloylo, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermilpd_ylok0ylold, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermilpd_ylok0ylobcst, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermilpd_zlok0zlozlo, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermilpd_zlok0zlold, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermilpd_zlok0zlobcst, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermilpd_xhik7xhixhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermilpd_xhik7xhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermilpd_xhik7xhibcst, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpermilpd_yhik7yhiyhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermilpd_yhik7yhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermilpd_yhik7yhibcst, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermilpd_zhik7zhizhi, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermilpd_zhik7zhild, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermilpd_zhik7zhibcst, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermilpd_xlok0xloimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xlok0bcstimm, vpermilpd, vpermilpd_mask, 0, REGARG(XMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0yloimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilpd_ylok0bcstimm, vpermilpd, vpermilpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0zloimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0ldimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zlok0bcstimm, vpermilpd, vpermilpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7xhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(vpermilpd_xhik7bcstimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7yhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermilpd_yhik7bcstimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7zhiimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7ldimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermilpd_zhik7bcstimm, vpermilpd, vpermilpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermb_xlok0xloxlo, vpermb, vpermb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpermb_xlok0xlold, vpermb, vpermb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpermb_ylok0yloylo, vpermb, vpermb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermb_ylok0ylold, vpermb, vpermb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermb_zlok0zlozlo, vpermb, vpermb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermb_zlok0zlold, vpermb, vpermb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermb_xhik7xhixhi, vpermb, vpermb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermb_xhik7xhild, vpermb, vpermb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermb_yhik7yhiyhi, vpermb, vpermb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermb_yhik7yhild, vpermb, vpermb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermb_zhik7zhizhi, vpermb, vpermb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermb_zhik7zhild, vpermb, vpermb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermd_ylok0yloylo, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermd_ylok0ylold, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermd_ylok0ylobcst, vpermd, vpermd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermd_zlok0zlozlo, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermd_zlok0zlold, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermd_zlok0zlobcst, vpermd, vpermd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermd_yhik7yhiyhi, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermd_yhik7yhild, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermd_yhik7yhibcst, vpermd, vpermd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermd_zhik7zhizhi, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermd_zhik7zhild, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermd_zhik7zhibcst, vpermd, vpermd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermw_xlok0xloxlo, vpermw, vpermw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpermw_xlok0xlold, vpermw, vpermw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpermw_ylok0yloylo, vpermw, vpermw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermw_ylok0ylold, vpermw, vpermw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermw_zlok0zlozlo, vpermw, vpermw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermw_zlok0zlold, vpermw, vpermw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermw_xhik7xhixhi, vpermw, vpermw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermw_xhik7xhild, vpermw, vpermw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermw_yhik7yhiyhi, vpermw, vpermw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermw_yhik7yhild, vpermw, vpermw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermw_zhik7zhizhi, vpermw, vpermw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermw_zhik7zhild, vpermw, vpermw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermps_ylok0yloylo, vpermps, vpermps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermps_ylok0ylold, vpermps, vpermps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermps_ylok0ylobcst, vpermps, vpermps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermps_zlok0zlozlo, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermps_zlok0zlold, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermps_zlok0zlobcst, vpermps, vpermps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermps_yhik7yhiyhi, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermps_yhik7yhild, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermps_yhik7yhibcst, vpermps, vpermps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermps_zhik7zhizhi, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermps_zhik7zhild, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermps_zhik7zhibcst, vpermps, vpermps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpermq_ylok0yloylo, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpermq_ylok0ylold, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermq_ylok0ylobcst, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermq_zlok0zlozlo, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpermq_zlok0zlold, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpermq_zlok0zlobcst, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermq_yhik7yhiyhi, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermq_yhik7yhild, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermq_yhik7yhibcst, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermq_zhik7zhizhi, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermq_zhik7zhild, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermq_zhik7zhibcst, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermq_ylok0yloimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       IMMARG(OPSZ_1))
OPCODE(vpermq_ylok0ldimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermq_ylok0bcstimm, vpermq, vpermq_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0zloimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0ldimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermq_zlok0bcstimm, vpermq, vpermq_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7yhiimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7ldimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermq_yhik7bcstimm, vpermq, vpermq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7zhiimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7ldimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermq_zhik7bcstimm, vpermq, vpermq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0yloimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0ldimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0bcstimm, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7yhiimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7ldimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_32), IMMARG(OPSZ_1))
OPCODE(vpermpd_yhik7bcstimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0zloimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0ldimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermpd_zlok0bcstimm, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7zhiimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7ldimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_64), IMMARG(OPSZ_1))
OPCODE(vpermpd_zhik7bcstimm, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(vpermpd_ylok0yloylo, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermpd_ylok0ylold, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermpd_ylok0ylobcst, vpermpd, vpermpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpermpd_zlok0zlozlo, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermpd_zlok0zlold, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermpd_zlok0zlobcst, vpermpd, vpermpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpermpd_yhik7yhiyhi, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermpd_yhik7yhild, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermpd_yhik7yhibcst, vpermpd, vpermpd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpermpd_zhik7zhizhi, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermpd_zhik7zhild, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermpd_zhik7zhibcst, vpermpd, vpermpd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vpermi2ps_xlok0xloxlo, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpermi2ps_xlok0xlold, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpermi2ps_xlok0xlobcst, vpermi2ps, vpermi2ps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_ylok0yloylo, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpermi2ps_ylok0ylold, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpermi2ps_ylok0ylobcst, vpermi2ps, vpermi2ps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_zlok0zlozlo, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpermi2ps_zlok0zlold, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpermi2ps_zlok0zlobcst, vpermi2ps, vpermi2ps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_xhik7xhixhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpermi2ps_xhik7xhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpermi2ps_xhik7xhibcst, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_yhik7yhiyhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpermi2ps_yhik7yhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpermi2ps_yhik7yhibcst, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpermi2ps_zhik7zhizhi, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpermi2ps_zhik7zhild, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpermi2ps_zhik7zhibcst, vpermi2ps, vpermi2ps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
