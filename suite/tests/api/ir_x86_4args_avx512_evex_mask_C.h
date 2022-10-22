/* **********************************************************
 * Copyright (c) 2019-2020 Google, Inc.  All rights reserved.
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
 * Part C: Split up to avoid VS running out of memory (i#3992,i#4610).
 */
OPCODE(vpsraw_xlok0xloimm, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsraw_xlok0immld, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraw_xhik7xhiimm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsraw_xhik7immld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraw_ylok0yloimm, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsraw_ylok0immld, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraw_yhik7yhiimm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsraw_yhik7immld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraw_zlok0zloimm, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsraw_zlok0immld, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraw_zhik7zhiimm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsraw_zhik7immld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrad_xlok0xloimm, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrad_xlok0immld, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrad_xlok0immbcst, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrad_xhik7xhiimm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrad_xhik7immld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrad_xhik7immbcst, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrad_ylok0yloimm, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrad_ylok0immld, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrad_ylok0immbcst, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrad_yhik7yhiimm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrad_yhik7immld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrad_yhik7immbcst, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrad_zlok0zloimm, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrad_zlok0immld, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrad_zlok0immbcst, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrad_zhik7zhiimm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrad_zhik7immld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrad_zhik7immbcst, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsraq_xlok0xloimm, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsraq_xlok0immld, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraq_xlok0immbcst, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsraq_xhik7xhiimm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsraq_xhik7immld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsraq_xhik7immbcst, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsraq_ylok0yloimm, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsraq_ylok0immld, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraq_ylok0immbcst, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsraq_yhik7yhiimm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsraq_yhik7immld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsraq_yhik7immbcst, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsraq_zlok0zloimm, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsraq_zlok0immld, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraq_zlok0immbcst, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsraq_zhik7zhiimm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsraq_zhik7immld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsraq_zhik7immbcst, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsrlw_xlok0xloimm, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrlw_xlok0immld, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlw_xhik7xhiimm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrlw_xhik7immld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlw_ylok0yloimm, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrlw_ylok0immld, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlw_yhik7yhiimm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrlw_yhik7immld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlw_zlok0zloimm, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrlw_zlok0immld, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlw_zhik7zhiimm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrlw_zhik7immld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrld_xlok0xloimm, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrld_xlok0immld, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrld_xlok0immbcst, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrld_xhik7xhiimm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrld_xhik7immld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrld_xhik7immbcst, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrld_ylok0yloimm, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrld_ylok0immld, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrld_ylok0immbcst, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrld_yhik7yhiimm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrld_yhik7immld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrld_yhik7immbcst, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrld_zlok0zloimm, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrld_zlok0immld, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrld_zlok0immbcst, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrld_zhik7zhiimm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrld_zhik7immld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrld_zhik7immbcst, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpsrlq_xlok0xloimm, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsrlq_xlok0immld, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlq_xlok0immbcst, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsrlq_xhik7xhiimm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsrlq_xhik7immld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsrlq_xhik7immbcst, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsrlq_ylok0yloimm, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsrlq_ylok0immld, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlq_ylok0immbcst, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsrlq_yhik7yhiimm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsrlq_yhik7immld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsrlq_yhik7immbcst, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsrlq_zlok0zloimm, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsrlq_zlok0immld, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlq_zlok0immbcst, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsrlq_zhik7zhiimm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsrlq_zhik7immld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsrlq_zhik7immbcst, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsraw_xlok0xloxmm, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsraw_xlok0xmmld, vpsraw, vpsraw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraw_xhik7xhixmm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsraw_xhik7xmmld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsraw_ylok0yloxmm, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsraw_ylok0xmmld, vpsraw, vpsraw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraw_yhik7yhixmm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsraw_yhik7xmmld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsraw_zlok0zloxmm, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsraw_zlok0xmmld, vpsraw, vpsraw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraw_zhik7zhixmm, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsraw_zhik7xmmld, vpsraw, vpsraw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrad_xlok0xloxmm, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrad_xlok0xmmld, vpsrad, vpsrad_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrad_xhik7xhixmm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrad_xhik7xmmld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrad_ylok0yloxmm, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrad_ylok0xmmld, vpsrad, vpsrad_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrad_yhik7yhixmm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrad_yhik7xmmld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrad_zlok0zloxmm, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrad_zlok0xmmld, vpsrad, vpsrad_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrad_zhik7zhixmm, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrad_zhik7xmmld, vpsrad, vpsrad_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsraq_xlok0xloxmm, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsraq_xlok0xmmld, vpsraq, vpsraq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraq_xhik7xhixmm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsraq_xhik7xmmld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsraq_ylok0yloxmm, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsraq_ylok0xmmld, vpsraq, vpsraq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraq_yhik7yhixmm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsraq_yhik7xmmld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsraq_zlok0zloxmm, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsraq_zlok0xmmld, vpsraq, vpsraq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsraq_zhik7zhixmm, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsraq_zhik7xmmld, vpsraq, vpsraq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlw_xlok0xloxmm, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrlw_xlok0xmmld, vpsrlw, vpsrlw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlw_xhik7xhixmm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlw_xhik7xmmld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlw_ylok0yloxmm, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrlw_ylok0xmmld, vpsrlw, vpsrlw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlw_yhik7yhixmm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrlw_yhik7xmmld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlw_zlok0zloxmm, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrlw_zlok0xmmld, vpsrlw, vpsrlw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlw_zhik7zhixmm, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrlw_zhik7xmmld, vpsrlw, vpsrlw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrld_xlok0xloxmm, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrld_xlok0xmmld, vpsrld, vpsrld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrld_xhik7xhixmm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrld_xhik7xmmld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrld_ylok0yloxmm, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrld_ylok0xmmld, vpsrld, vpsrld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrld_yhik7yhixmm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrld_yhik7xmmld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrld_zlok0zloxmm, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrld_zlok0xmmld, vpsrld, vpsrld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrld_zhik7zhixmm, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrld_zhik7xmmld, vpsrld, vpsrld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlq_xlok0xloxmm, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsrlq_xlok0xmmld, vpsrlq, vpsrlq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlq_xhik7xhixmm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlq_xhik7xmmld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlq_ylok0yloxmm, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsrlq_ylok0xmmld, vpsrlq, vpsrlq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlq_yhik7yhixmm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsrlq_yhik7xmmld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlq_zlok0zloxmm, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsrlq_zlok0xmmld, vpsrlq, vpsrlq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsrlq_zhik7zhixmm, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsrlq_zhik7xmmld, vpsrlq, vpsrlq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsravw_xlok0xloxlo, vpsravw, vpsravw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsravw_xlok0xlold, vpsravw, vpsravw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravw_xhik7xhixhi, vpsravw, vpsravw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsravw_xhik7xhild, vpsravw, vpsravw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravw_ylok0yloylo, vpsravw, vpsravw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsravw_ylok0ylold, vpsravw, vpsravw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravw_yhik7yhiyhi, vpsravw, vpsravw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsravw_yhik7yhild, vpsravw, vpsravw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravw_zlok0zlozlo, vpsravw, vpsravw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsravw_zlok0zlold, vpsravw, vpsravw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravw_zhik7zhizhi, vpsravw, vpsravw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsravw_zhik7zhild, vpsravw, vpsravw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravd_xlok0xloxlo, vpsravd, vpsravd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsravd_xlok0xlold, vpsravd, vpsravd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravd_xlok0xlobcst, vpsravd, vpsravd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_4))
OPCODE(vpsravd_xhik7xhixhi, vpsravd, vpsravd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsravd_xhik7xhild, vpsravd, vpsravd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravd_xhik7xhibcst, vpsravd, vpsravd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_4))
OPCODE(vpsravd_ylok0yloylo, vpsravd, vpsravd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsravd_ylok0ylold, vpsravd, vpsravd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravd_ylok0ylobcst, vpsravd, vpsravd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_4))
OPCODE(vpsravd_yhik7yhiyhi, vpsravd, vpsravd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsravd_yhik7yhild, vpsravd, vpsravd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravd_yhik7yhibcst, vpsravd, vpsravd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_4))
OPCODE(vpsravd_zlok0zlozlo, vpsravd, vpsravd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsravd_zlok0zlold, vpsravd, vpsravd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravd_zlok0zlobcst, vpsravd, vpsravd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_4))
OPCODE(vpsravd_zhik7zhizhi, vpsravd, vpsravd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsravd_zhik7zhild, vpsravd, vpsravd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravd_zhik7zhibcst, vpsravd, vpsravd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_4))
OPCODE(vpsravq_xlok0xloxlo, vpsravq, vpsravq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsravq_xlok0xlold, vpsravq, vpsravq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravq_xlok0xlobcst, vpsravq, vpsravq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_8))
OPCODE(vpsravq_xhik7xhixhi, vpsravq, vpsravq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsravq_xhik7xhild, vpsravq, vpsravq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsravq_xhik7xhibcst, vpsravq, vpsravq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_8))
OPCODE(vpsravq_ylok0yloylo, vpsravq, vpsravq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsravq_ylok0ylold, vpsravq, vpsravq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravq_ylok0ylobcst, vpsravq, vpsravq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_8))
OPCODE(vpsravq_yhik7yhiyhi, vpsravq, vpsravq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsravq_yhik7yhild, vpsravq, vpsravq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsravq_yhik7yhibcst, vpsravq, vpsravq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_8))
OPCODE(vpsravq_zlok0zlozlo, vpsravq, vpsravq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsravq_zlok0zlold, vpsravq, vpsravq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravq_zlok0zlobcst, vpsravq, vpsravq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_8))
OPCODE(vpsravq_zhik7zhizhi, vpsravq, vpsravq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsravq_zhik7zhild, vpsravq, vpsravq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsravq_zhik7zhibcst, vpsravq, vpsravq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_8))
OPCODE(vpsrlvw_xlok0xloxlo, vpsrlvw, vpsrlvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsrlvw_xlok0xlold, vpsrlvw, vpsrlvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvw_xhik7xhixhi, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlvw_xhik7xhild, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvw_ylok0yloylo, vpsrlvw, vpsrlvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsrlvw_ylok0ylold, vpsrlvw, vpsrlvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvw_yhik7yhiyhi, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsrlvw_yhik7yhild, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvw_zlok0zlozlo, vpsrlvw, vpsrlvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsrlvw_zlok0zlold, vpsrlvw, vpsrlvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvw_zhik7zhizhi, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsrlvw_zhik7zhild, vpsrlvw, vpsrlvw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvd_xlok0xloxlo, vpsrlvd, vpsrlvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsrlvd_xlok0xlold, vpsrlvd, vpsrlvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvd_xlok0xlobcst, vpsrlvd, vpsrlvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_4))
OPCODE(vpsrlvd_xhik7xhixhi, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlvd_xhik7xhild, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvd_xhik7xhibcst, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_4))
OPCODE(vpsrlvd_ylok0yloylo, vpsrlvd, vpsrlvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsrlvd_ylok0ylold, vpsrlvd, vpsrlvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvd_ylok0ylobcst, vpsrlvd, vpsrlvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_4))
OPCODE(vpsrlvd_yhik7yhiyhi, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsrlvd_yhik7yhild, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvd_yhik7yhibcst, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_4))
OPCODE(vpsrlvd_zlok0zlozlo, vpsrlvd, vpsrlvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsrlvd_zlok0zlold, vpsrlvd, vpsrlvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvd_zlok0zlobcst, vpsrlvd, vpsrlvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_4))
OPCODE(vpsrlvd_zhik7zhizhi, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsrlvd_zhik7zhild, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvd_zhik7zhibcst, vpsrlvd, vpsrlvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_4))
OPCODE(vpsrlvq_xlok0xloxlo, vpsrlvq, vpsrlvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsrlvq_xlok0xlold, vpsrlvq, vpsrlvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvq_xlok0xlobcst, vpsrlvq, vpsrlvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_8))
OPCODE(vpsrlvq_xhik7xhixhi, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsrlvq_xhik7xhild, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsrlvq_xhik7xhibcst, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_8))
OPCODE(vpsrlvq_ylok0yloylo, vpsrlvq, vpsrlvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsrlvq_ylok0ylold, vpsrlvq, vpsrlvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvq_ylok0ylobcst, vpsrlvq, vpsrlvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_8))
OPCODE(vpsrlvq_yhik7yhiyhi, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsrlvq_yhik7yhild, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsrlvq_yhik7yhibcst, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_8))
OPCODE(vpsrlvq_zlok0zlozlo, vpsrlvq, vpsrlvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsrlvq_zlok0zlold, vpsrlvq, vpsrlvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvq_zlok0zlobcst, vpsrlvq, vpsrlvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_8))
OPCODE(vpsrlvq_zhik7zhizhi, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsrlvq_zhik7zhild, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsrlvq_zhik7zhibcst, vpsrlvq, vpsrlvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_8))
OPCODE(vpsllw_xlok0xloimm, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsllw_xlok0immld, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllw_xhik7xhiimm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsllw_xhik7immld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllw_ylok0yloimm, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsllw_ylok0immld, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllw_yhik7yhiimm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsllw_yhik7immld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllw_zlok0zloimm, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsllw_zlok0immld, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllw_zhik7zhiimm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsllw_zhik7immld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllw_xlok0xloxmm, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsllw_xlok0xmmld, vpsllw, vpsllw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllw_xhik7xhixmm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllw_xhik7xmmld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllw_ylok0yloxmm, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsllw_ylok0xmmld, vpsllw, vpsllw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllw_yhik7yhixmm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsllw_yhik7xmmld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsllw_zlok0zloxmm, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsllw_zlok0xmmld, vpsllw, vpsllw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllw_zhik7zhixmm, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsllw_zhik7xmmld, vpsllw, vpsllw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpslld_xlok0xloimm, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpslld_xlok0immld, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpslld_xlok0immbcst, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpslld_xhik7xhiimm, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpslld_xhik7immld, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpslld_xhik7immbcst, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpslld_ylok0yloimm, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpslld_ylok0immld, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpslld_ylok0immbcst, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpslld_yhik7yhiimm, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpslld_yhik7immld, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpslld_yhik7immbcst, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpslld_zlok0zloimm, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpslld_zlok0immld, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpslld_zlok0immbcst, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpslld_zhik7zhiimm, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpslld_zhik7immld, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpslld_zhik7immbcst, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpslld_xlok0xloxmm, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpslld_xlok0xmmld, vpslld, vpslld_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpslld_xhik7xhixmm, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpslld_xhik7xmmld, vpslld, vpslld_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpslld_ylok0yloxmm, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpslld_ylok0xmmld, vpslld, vpslld_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpslld_yhik7yhixmm, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpslld_yhik7xmmld, vpslld, vpslld_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpslld_zlok0zloxmm, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpslld_zlok0xmmld, vpslld, vpslld_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpslld_zhik7zhixmm, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpslld_zhik7xmmld, vpslld, vpslld_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsllq_xlok0xloimm, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpsllq_xlok0immld, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllq_xlok0immbcst, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsllq_xhik7xhiimm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM17))
OPCODE(vpsllq_xhik7immld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpsllq_xhik7immbcst, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsllq_ylok0yloimm, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpsllq_ylok0immld, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllq_ylok0immbcst, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsllq_yhik7yhiimm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM17))
OPCODE(vpsllq_yhik7immld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpsllq_yhik7immbcst, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsllq_zlok0zloimm, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpsllq_zlok0immld, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllq_zlok0immbcst, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsllq_zhik7zhiimm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM17))
OPCODE(vpsllq_zhik7immld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpsllq_zhik7immbcst, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpsllq_xlok0xloxmm, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       REGARG(XMM1))
OPCODE(vpsllq_xlok0xmmld, vpsllq, vpsllq_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllq_xhik7xhixmm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllq_xhik7xmmld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllq_ylok0yloxmm, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       REGARG(XMM1))
OPCODE(vpsllq_ylok0xmmld, vpsllq, vpsllq_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllq_yhik7yhixmm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(XMM17))
OPCODE(vpsllq_yhik7xmmld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_16))
OPCODE(vpsllq_zlok0zloxmm, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       REGARG(XMM1))
OPCODE(vpsllq_zlok0xmmld, vpsllq, vpsllq_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM2),
       MEMARG(OPSZ_16))
OPCODE(vpsllq_zhik7zhixmm, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(XMM17))
OPCODE(vpsllq_zhik7xmmld, vpsllq, vpsllq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvw_xlok0xloxlo, vpsllvw, vpsllvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsllvw_xlok0xlold, vpsllvw, vpsllvw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvw_xhik7xhixhi, vpsllvw, vpsllvw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllvw_xhik7xhild, vpsllvw, vpsllvw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvw_ylok0yloylo, vpsllvw, vpsllvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsllvw_ylok0ylold, vpsllvw, vpsllvw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsllvw_yhik7yhiyhi, vpsllvw, vpsllvw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsllvw_yhik7yhild, vpsllvw, vpsllvw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsllvw_zlok0zlozlo, vpsllvw, vpsllvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsllvw_zlok0zlold, vpsllvw, vpsllvw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllvw_zhik7zhizhi, vpsllvw, vpsllvw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsllvw_zhik7zhild, vpsllvw, vpsllvw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllvd_xlok0xloxlo, vpsllvd, vpsllvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsllvd_xlok0xlold, vpsllvd, vpsllvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvd_xlok0xlobcst, vpsllvd, vpsllvd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_4))
OPCODE(vpsllvd_xhik7xhixhi, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllvd_xhik7xhild, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvd_xhik7xhibcst, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_4))
OPCODE(vpsllvd_ylok0yloylo, vpsllvd, vpsllvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsllvd_ylok0ylold, vpsllvd, vpsllvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsllvd_ylok0ylobcst, vpsllvd, vpsllvd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_4))
OPCODE(vpsllvd_yhik7yhiyhi, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsllvd_yhik7yhild, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsllvd_yhik7yhibcst, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_4))
OPCODE(vpsllvd_zlok0zlozlo, vpsllvd, vpsllvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsllvd_zlok0zlold, vpsllvd, vpsllvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllvd_zlok0zlobcst, vpsllvd, vpsllvd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_4))
OPCODE(vpsllvd_zhik7zhizhi, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsllvd_zhik7zhild, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllvd_zhik7zhibcst, vpsllvd, vpsllvd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_4))
OPCODE(vpsllvq_xlok0xloxlo, vpsllvq, vpsllvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), REGARG(XMM1))
OPCODE(vpsllvq_xlok0xlold, vpsllvq, vpsllvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvq_xlok0xlobcst, vpsllvq, vpsllvq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM2), MEMARG(OPSZ_8))
OPCODE(vpsllvq_xhik7xhixhi, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), REGARG(XMM17))
OPCODE(vpsllvq_xhik7xhild, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_16))
OPCODE(vpsllvq_xhik7xhibcst, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM2), MEMARG(OPSZ_8))
OPCODE(vpsllvq_ylok0yloylo, vpsllvq, vpsllvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), REGARG(YMM1))
OPCODE(vpsllvq_ylok0ylold, vpsllvq, vpsllvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsllvq_ylok0ylobcst, vpsllvq, vpsllvq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM2), MEMARG(OPSZ_8))
OPCODE(vpsllvq_yhik7yhiyhi, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), REGARG(YMM17))
OPCODE(vpsllvq_yhik7yhild, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_32))
OPCODE(vpsllvq_yhik7yhibcst, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM2), MEMARG(OPSZ_8))
OPCODE(vpsllvq_zlok0zlozlo, vpsllvq, vpsllvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), REGARG(ZMM1))
OPCODE(vpsllvq_zlok0zlold, vpsllvq, vpsllvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllvq_zlok0zlobcst, vpsllvq, vpsllvq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM2), MEMARG(OPSZ_8))
OPCODE(vpsllvq_zhik7zhizhi, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), REGARG(ZMM17))
OPCODE(vpsllvq_zhik7zhild, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_64))
OPCODE(vpsllvq_zhik7zhibcst, vpsllvq, vpsllvq_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM2), MEMARG(OPSZ_8))
OPCODE(vrcp14ss_xlok0xloxmm, vrcp14ss, vrcp14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrcp14ss_xlok0xmmld, vrcp14ss, vrcp14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp14ss_xhik7xhixmm, vrcp14ss, vrcp14ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrcp14ss_xhik7xmmld, vrcp14ss, vrcp14ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp14sd_xlok0xloxmm, vrcp14sd, vrcp14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrcp14sd_xlok0xmmld, vrcp14sd, vrcp14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrcp14sd_xhik7xhixmm, vrcp14sd, vrcp14sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrcp14sd_xhik7xmmld, vrcp14sd, vrcp14sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrcp28ss_xlok0xloxmm, vrcp28ss, vrcp28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrcp28ss_xlok0xmmld, vrcp28ss, vrcp28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp28ss_xhik7xhixmm, vrcp28ss, vrcp28ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrcp28ss_xhik7xmmld, vrcp28ss, vrcp28ss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrcp28sd_xlok0xloxmm, vrcp28sd, vrcp28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrcp28sd_xlok0xmmld, vrcp28sd, vrcp28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrcp28sd_xhik7xhixmm, vrcp28sd, vrcp28sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrcp28sd_xhik7xmmld, vrcp28sd, vrcp28sd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vpshufhw_xlok0xlo, vpshufhw, vpshufhw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpshufhw_xlok0ld, vpshufhw, vpshufhw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufhw_xhik7xhi, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vpshufhw_xhik7ld, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufhw_ylok0ylo, vpshufhw, vpshufhw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpshufhw_ylok0ld, vpshufhw, vpshufhw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufhw_yhik7yhi, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vpshufhw_yhik7ld, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufhw_zlok0zlo, vpshufhw, vpshufhw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpshufhw_zlok0ld, vpshufhw, vpshufhw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufhw_zhik7zhi, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vpshufhw_zhik7ld, vpshufhw, vpshufhw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufd_xlok0xlo, vpshufd, vpshufd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpshufd_xlok0ld, vpshufd, vpshufd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufd_xlok0bcst, vpshufd, vpshufd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpshufd_xhik7xhi, vpshufd, vpshufd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vpshufd_xhik7ld, vpshufd, vpshufd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshufd_xhik7bcst, vpshufd, vpshufd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpshufd_ylok0ylo, vpshufd, vpshufd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpshufd_ylok0ld, vpshufd, vpshufd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufd_ylok0bcst, vpshufd, vpshufd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpshufd_yhik7yhi, vpshufd, vpshufd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vpshufd_yhik7ld, vpshufd, vpshufd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshufd_yhik7bcst, vpshufd, vpshufd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpshufd_zlok0zlo, vpshufd, vpshufd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpshufd_zlok0ld, vpshufd, vpshufd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufd_zlok0bcst, vpshufd, vpshufd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpshufd_zhik7zhi, vpshufd, vpshufd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vpshufd_zhik7ld, vpshufd, vpshufd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufd_zhik7bcst, vpshufd, vpshufd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vpshuflw_xlok0xlo, vpshuflw, vpshuflw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vpshuflw_xlok0ld, vpshuflw, vpshuflw_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshuflw_xhik7xhi, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vpshuflw_xhik7ld, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vpshuflw_ylok0ylo, vpshuflw, vpshuflw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vpshuflw_ylok0ld, vpshuflw, vpshuflw_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshuflw_yhik7yhi, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vpshuflw_yhik7ld, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vpshuflw_zlok0zlo, vpshuflw, vpshuflw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vpshuflw_zlok0ld, vpshuflw, vpshuflw_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshuflw_zhik7zhi, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vpshuflw_zhik7ld, vpshuflw, vpshuflw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vpshufb_xlok0xlo, vpshufb, vpshufb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpshufb_xlok0ld, vpshufb, vpshufb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpshufb_xhik7xhi, vpshufb, vpshufb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpshufb_xhik7ld, vpshufb, vpshufb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpshufb_ylok0ylo, vpshufb, vpshufb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpshufb_ylok0ld, vpshufb, vpshufb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpshufb_yhik7yhi, vpshufb, vpshufb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpshufb_yhik7ld, vpshufb, vpshufb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpshufb_zlok0zlo, vpshufb, vpshufb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpshufb_zlok0ld, vpshufb, vpshufb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpshufb_zhik7zhi, vpshufb, vpshufb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpshufb_zhik7ld, vpshufb, vpshufb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpavgb_xlok0xloxlo, vpavgb, vpavgb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpavgb_xlok0xlold, vpavgb, vpavgb_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpavgb_xhik7xhixhi, vpavgb, vpavgb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpavgb_xhik7xhild, vpavgb, vpavgb_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))

OPCODE(vpavgb_ylok0yloylo, vpavgb, vpavgb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpavgb_ylok0ylold, vpavgb, vpavgb_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpavgb_yhik7yhiyhi, vpavgb, vpavgb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpavgb_yhik7yhild, vpavgb, vpavgb_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))

OPCODE(vpavgb_zlok0zlozlo, vpavgb, vpavgb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpavgb_zlok0zlold, vpavgb, vpavgb_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpavgb_zhik7zhizhi, vpavgb, vpavgb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpavgb_zhik7zhild, vpavgb, vpavgb_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpavgw_xlok0xloxlo, vpavgw, vpavgw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       REGARG(XMM2))
OPCODE(vpavgw_xlok0xlold, vpavgw, vpavgw_mask, 0, REGARG(XMM0), REGARG(K0), REGARG(XMM1),
       MEMARG(OPSZ_16))
OPCODE(vpavgw_xhik7xhixhi, vpavgw, vpavgw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), REGARG(XMM31))
OPCODE(vpavgw_xhik7xhild, vpavgw, vpavgw_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpavgw_ylok0yloylo, vpavgw, vpavgw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       REGARG(YMM2))
OPCODE(vpavgw_ylok0ylold, vpavgw, vpavgw_mask, 0, REGARG(YMM0), REGARG(K0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpavgw_yhik7yhiyhi, vpavgw, vpavgw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), REGARG(YMM31))
OPCODE(vpavgw_yhik7yhild, vpavgw, vpavgw_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpavgw_zlok0zlozlo, vpavgw, vpavgw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       REGARG(ZMM2))
OPCODE(vpavgw_zlok0zlold, vpavgw, vpavgw_mask, 0, REGARG(ZMM0), REGARG(K0), REGARG(ZMM1),
       MEMARG(OPSZ_64))
OPCODE(vpavgw_zhik7zhizhi, vpavgw, vpavgw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpavgw_zhik7zhild, vpavgw, vpavgw_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vblendmps_xlok0xloxlo, vblendmps, vblendmps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vblendmps_xlok0xlold, vblendmps, vblendmps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vblendmps_xlok0xlobcst, vblendmps, vblendmps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vblendmps_xhik7xhixhi, vblendmps, vblendmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vblendmps_xhik7xhild, vblendmps, vblendmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vblendmps_xhik7xhibcst, vblendmps, vblendmps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vblendmps_ylok0yloylo, vblendmps, vblendmps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vblendmps_ylok0ylold, vblendmps, vblendmps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vblendmps_ylok0ylobcst, vblendmps, vblendmps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vblendmps_yhik7yhiyhi, vblendmps, vblendmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vblendmps_yhik7yhild, vblendmps, vblendmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vblendmps_yhik7yhibcst, vblendmps, vblendmps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vblendmps_zlok0zlozlo, vblendmps, vblendmps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vblendmps_zlok0zlold, vblendmps, vblendmps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vblendmps_zlok0zlobct, vblendmps, vblendmps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vblendmps_zhik7zhizhi, vblendmps, vblendmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vblendmps_zhik7zhild, vblendmps, vblendmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vblendmps_zhik7zhibcst, vblendmps, vblendmps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vblendmpd_xlok0xloxlo, vblendmpd, vblendmpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vblendmpd_xlok0xlold, vblendmpd, vblendmpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vblendmpd_xlok0xlobcst, vblendmpd, vblendmpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vblendmpd_xhik7xhixhi, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vblendmpd_xhik7xhild, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vblendmpd_xhik7xhibcst, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vblendmpd_ylok0yloylo, vblendmpd, vblendmpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vblendmpd_ylok0ylold, vblendmpd, vblendmpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vblendmpd_ylok0ylobcst, vblendmpd, vblendmpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vblendmpd_yhik7yhiyhi, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vblendmpd_yhik7yhild, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vblendmpd_yhik7yhibcst, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vblendmpd_zlok0zlozlo, vblendmpd, vblendmpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vblendmpd_zlok0zlold, vblendmpd, vblendmpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vblendmpd_zlok0zlobcst, vblendmpd, vblendmpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vblendmpd_zhik7zhizhi, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vblendmpd_zhik7zhild, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vblendmpd_zhik7zhibcst, vblendmpd, vblendmpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vgetexpss_xlok0xloxmm, vgetexpss, vgetexpss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vgetexpss_xlok0xmmld, vgetexpss, vgetexpss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vgetexpss_xhik7xhixmm, vgetexpss, vgetexpss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vgetexpss_xhik7xmmld, vgetexpss, vgetexpss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vgetexpsd_xlok0xloxmm, vgetexpsd, vgetexpsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vgetexpsd_xlok0xmmld, vgetexpsd, vgetexpsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vgetexpsd_xhik7xhixmm, vgetexpsd, vgetexpsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vgetexpsd_xhik7xmmld, vgetexpsd, vgetexpsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vgetmantps_xlok0xlo, vgetmantps, vgetmantps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vgetmantps_xlok0ld, vgetmantps, vgetmantps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantps_xlok0bcst, vgetmantps, vgetmantps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vgetmantps_xhik7xhi, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vgetmantps_xhik7ld, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantps_xhik7bcst, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vgetmantps_ylok0ylo, vgetmantps, vgetmantps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vgetmantps_ylok0ld, vgetmantps, vgetmantps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantps_ylok0bcst, vgetmantps, vgetmantps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vgetmantps_yhik7yhi, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vgetmantps_yhik7ld, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantps_yhik7bcst, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vgetmantps_zlok0zlo, vgetmantps, vgetmantps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vgetmantps_zlok0ld, vgetmantps, vgetmantps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantps_zlok0bcst, vgetmantps, vgetmantps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vgetmantps_zhik7zhi, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vgetmantps_zhik7ld, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantps_zhik7bcst, vgetmantps, vgetmantps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vgetmantpd_xlok0xlo, vgetmantpd, vgetmantpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vgetmantpd_xlok0ld, vgetmantpd, vgetmantpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantpd_xlok0bcst, vgetmantpd, vgetmantpd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vgetmantpd_xhik7xhi, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vgetmantpd_xhik7ld, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vgetmantpd_xhik7bcst, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vgetmantpd_ylok0ylo, vgetmantpd, vgetmantpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vgetmantpd_ylok0ld, vgetmantpd, vgetmantpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantpd_ylok0bcst, vgetmantpd, vgetmantpd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vgetmantpd_yhik7yhi, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vgetmantpd_yhik7ld, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vgetmantpd_yhik7bcst, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vgetmantpd_zlok0zlo, vgetmantpd, vgetmantpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vgetmantpd_zlok0ld, vgetmantpd, vgetmantpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantpd_zlok0bcst, vgetmantpd, vgetmantpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vgetmantpd_zhik7zhi, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vgetmantpd_zhik7ld, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vgetmantpd_zhik7bcst, vgetmantpd, vgetmantpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpblendmb_xlok0xloxlo, vpblendmb, vpblendmb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmb_xlok0xlold, vpblendmb, vpblendmb_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmb_xhik7xhixhi, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmb_xhik7xhild, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmb_ylok0yloylo, vpblendmb, vpblendmb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmb_ylok0ylold, vpblendmb, vpblendmb_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmb_yhik7yhiyhi, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmb_yhik7yhild, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmb_zlok0zlozlo, vpblendmb, vpblendmb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmb_zlok0zlold, vpblendmb, vpblendmb_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmb_zhik7zhizhi, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmb_zhik7zhild, vpblendmb, vpblendmb_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmw_xlok0xloxlo, vpblendmw, vpblendmw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmw_xlok0xlold, vpblendmw, vpblendmw_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmw_xhik7xhixhi, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmw_xhik7xhild, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmw_ylok0yloylo, vpblendmw, vpblendmw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmw_ylok0ylold, vpblendmw, vpblendmw_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmw_yhik7yhiyhi, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmw_yhik7yhild, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmw_zlok0zlozlo, vpblendmw, vpblendmw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmw_zlok0zlold, vpblendmw, vpblendmw_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmw_zhik7zhizhi, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmw_zhik7zhild, vpblendmw, vpblendmw_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmd_xlok0xloxlo, vpblendmd, vpblendmd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmd_xlok0xlold, vpblendmd, vpblendmd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmd_xlok0xlobcst, vpblendmd, vpblendmd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vpblendmd_xhik7xhixhi, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmd_xhik7xhild, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmd_xhik7xhibcst, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vpblendmd_ylok0yloylo, vpblendmd, vpblendmd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmd_ylok0ylold, vpblendmd, vpblendmd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmd_ylok0ylobcst, vpblendmd, vpblendmd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vpblendmd_yhik7yhiyhi, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmd_yhik7yhild, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmd_yhik7yhibcst, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vpblendmd_zlok0zlozlo, vpblendmd, vpblendmd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmd_zlok0zlold, vpblendmd, vpblendmd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmd_zlok0zlobcst, vpblendmd, vpblendmd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vpblendmd_zhik7zhizhi, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmd_zhik7zhild, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmd_zhik7zhibcst, vpblendmd, vpblendmd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vpblendmq_xlok0xloxlo, vpblendmq, vpblendmq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vpblendmq_xlok0xlold, vpblendmq, vpblendmq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpblendmq_xlok0xlobcst, vpblendmq, vpblendmq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpblendmq_xhik7xhixhi, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpblendmq_xhik7xhild, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vpblendmq_xhik7xhibcst, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vpblendmq_ylok0yloylo, vpblendmq, vpblendmq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vpblendmq_ylok0ylold, vpblendmq, vpblendmq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpblendmq_ylok0ylobcst, vpblendmq, vpblendmq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpblendmq_yhik7yhiyhi, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpblendmq_yhik7yhild, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vpblendmq_yhik7yhibcst, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vpblendmq_zlok0zlozlo, vpblendmq, vpblendmq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpblendmq_zlok0zlold, vpblendmq, vpblendmq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpblendmq_zlok0zlobcst, vpblendmq, vpblendmq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpblendmq_zhik7zhizhi, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpblendmq_zhik7zhild, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vpblendmq_zhik7zhibcst, vpblendmq, vpblendmq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vptestmb_k1k0xloxlo, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmb_k1k0xlold, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmb_k6k7xhixhi, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmb_k6k7xhild, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmb_k1k0yloylo, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmb_k1k0ylold, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmb_k6k7yhiyhi, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmb_k6k7yhild, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmb_k1k0zlozlo, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmb_k1k0zlold, vptestmb, vptestmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmb_k6k7zhizhi, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmb_k6k7zhild, vptestmb, vptestmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmw_k1k0xloxlo, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmw_k1k0xlold, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmw_k6k7xhixhi, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmw_k6k7xhild, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmw_k1k0yloylo, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmw_k1k0ylold, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmw_k6k7yhiyhi, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmw_k6k7yhild, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmw_k1k0zlozlo, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmw_k1k0zlold, vptestmw, vptestmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmw_k6k7zhizhi, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmw_k6k7zhild, vptestmw, vptestmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmd_k1k0xloxlo, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmd_k1k0xlold, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmd_k1k0xlobcst, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vptestmd_k6k7xhixhi, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmd_k6k7xhild, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmd_k6k7xhibcst, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vptestmd_k1k0yloylo, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmd_k1k0ylold, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmd_k1k0ylobcst, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vptestmd_k6k7yhiyhi, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmd_k6k7yhild, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmd_k6k7yhibcst, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vptestmd_k1k0zlozlo, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmd_k1k0zlold, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmd_k1k0zlobcst, vptestmd, vptestmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vptestmd_k6k7zhizhi, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmd_k6k7zhild, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmd_k6k7zhibcst, vptestmd, vptestmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vptestmq_k1k0xloxlo, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestmq_k1k0xlold, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestmq_k1k0xlobcst, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vptestmq_k6k7xhixhi, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestmq_k6k7xhild, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestmq_k6k7xhibcst, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vptestmq_k1k0yloylo, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestmq_k1k0ylold, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestmq_k1k0ylobcst, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vptestmq_k6k7yhiyhi, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestmq_k6k7yhild, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestmq_k6k7yhibcst, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vptestmq_k1k0zlozlo, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestmq_k1k0zlold, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestmq_k1k0zlobcst, vptestmq, vptestmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vptestmq_k6k7zhizhi, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestmq_k6k7zhild, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestmq_k6k7zhibcst, vptestmq, vptestmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vptestnmb_k1k0xloxlo, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmb_k1k0xlold, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmb_k6k7xhixhi, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmb_k6k7xhild, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmb_k1k0yloylo, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmb_k1k0ylold, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmb_k6k7yhiyhi, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmb_k6k7yhild, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmb_k1k0zlozlo, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmb_k1k0zlold, vptestnmb, vptestnmb_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmb_k6k7zhizhi, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmb_k6k7zhild, vptestnmb, vptestnmb_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmw_k1k0xloxlo, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmw_k1k0xlold, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmw_k6k7xhixhi, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmw_k6k7xhild, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmw_k1k0yloylo, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmw_k1k0ylold, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmw_k6k7yhiyhi, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmw_k6k7yhild, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmw_k1k0zlozlo, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmw_k1k0zlold, vptestnmw, vptestnmw_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmw_k6k7zhizhi, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmw_k6k7zhild, vptestnmw, vptestnmw_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmd_k1k0xloxlo, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmd_k1k0xlold, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmd_k1k0xlobcst, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_4))
OPCODE(vptestnmd_k6k7xhixhi, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmd_k6k7xhild, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmd_k6k7xhibcst, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_4))
OPCODE(vptestnmd_k1k0yloylo, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmd_k1k0ylold, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmd_k1k0ylobcst, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_4))
OPCODE(vptestnmd_k6k7yhiyhi, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmd_k6k7yhild, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmd_k6k7yhibcst, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_4))
OPCODE(vptestnmd_k1k0zlozlo, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmd_k1k0zlold, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmd_k1k0zlobcst, vptestnmd, vptestnmd_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_4))
OPCODE(vptestnmd_k6k7zhizhi, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmd_k6k7zhild, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmd_k6k7zhibcst, vptestnmd, vptestnmd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_4))
OPCODE(vptestnmq_k1k0xloxlo, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), REGARG(XMM1))
OPCODE(vptestnmq_k1k0xlold, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_16))
OPCODE(vptestnmq_k1k0xlobcst, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(XMM0), MEMARG(OPSZ_8))
OPCODE(vptestnmq_k6k7xhixhi, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM16), REGARG(XMM31))
OPCODE(vptestnmq_k6k7xhild, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_16))
OPCODE(vptestnmq_k6k7xhibcst, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(XMM31), MEMARG(OPSZ_8))
OPCODE(vptestnmq_k1k0yloylo, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), REGARG(YMM1))
OPCODE(vptestnmq_k1k0ylold, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_32))
OPCODE(vptestnmq_k1k0ylobcst, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(YMM0), MEMARG(OPSZ_8))
OPCODE(vptestnmq_k6k7yhiyhi, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM16), REGARG(YMM31))
OPCODE(vptestnmq_k6k7yhild, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_32))
OPCODE(vptestnmq_k6k7yhibcst, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(YMM31), MEMARG(OPSZ_8))
OPCODE(vptestnmq_k1k0zlozlo, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), REGARG(ZMM1))
OPCODE(vptestnmq_k1k0zlold, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_64))
OPCODE(vptestnmq_k1k0zlobcst, vptestnmq, vptestnmq_mask, 0, REGARG(K1), REGARG(K0),
       REGARG(ZMM0), MEMARG(OPSZ_8))
OPCODE(vptestnmq_k6k7zhizhi, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM16), REGARG(ZMM31))
OPCODE(vptestnmq_k6k7zhild, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_64))
OPCODE(vptestnmq_k6k7zhibcst, vptestnmq, vptestnmq_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       REGARG(ZMM31), MEMARG(OPSZ_8))
OPCODE(vreduceps_xlok0xlo, vreduceps, vreduceps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vreduceps_xlok0ld, vreduceps, vreduceps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreduceps_xlok0bcst, vreduceps, vreduceps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vreduceps_xhik7xhi, vreduceps, vreduceps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vreduceps_xhik7ld, vreduceps, vreduceps_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreduceps_xhik7bcst, vreduceps, vreduceps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vreduceps_ylok0ylo, vreduceps, vreduceps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vreduceps_ylok0ld, vreduceps, vreduceps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreduceps_ylok0bcst, vreduceps, vreduceps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vreduceps_yhik7yhi, vreduceps, vreduceps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vreduceps_yhik7ld, vreduceps, vreduceps_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreduceps_yhik7bcst, vreduceps, vreduceps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vreduceps_zlok0zlo, vreduceps, vreduceps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vreduceps_zlok0ld, vreduceps, vreduceps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreduceps_zlok0bcst, vreduceps, vreduceps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vreduceps_zhik7zhi, vreduceps, vreduceps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vreduceps_zhik7ld, vreduceps, vreduceps_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreduceps_zhik7bcst, vreduceps, vreduceps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vreducepd_xlok0xlo, vreducepd, vreducepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vreducepd_xlok0ld, vreducepd, vreducepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreducepd_xlok0bcst, vreducepd, vreducepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vreducepd_xhik7xhi, vreducepd, vreducepd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vreducepd_xhik7ld, vreducepd, vreducepd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vreducepd_xhik7bcst, vreducepd, vreducepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vreducepd_ylok0ylo, vreducepd, vreducepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vreducepd_ylok0ld, vreducepd, vreducepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreducepd_ylok0bcst, vreducepd, vreducepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vreducepd_yhik7yhi, vreducepd, vreducepd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vreducepd_yhik7ld, vreducepd, vreducepd_mask, X64_ONLY, REGARG(YMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vreducepd_yhik7bcst, vreducepd, vreducepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vreducepd_zlok0zlo, vreducepd, vreducepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vreducepd_zlok0ld, vreducepd, vreducepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreducepd_zlok0bcst, vreducepd, vreducepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vreducepd_zhik7zhi, vreducepd, vreducepd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vreducepd_zhik7ld, vreducepd, vreducepd_mask, X64_ONLY, REGARG(ZMM16), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vreducepd_zhik7bcst, vreducepd, vreducepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrndscaleps_xlok0xlo, vrndscaleps, vrndscaleps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vrndscaleps_xlok0ld, vrndscaleps, vrndscaleps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscaleps_xlok0bcst, vrndscaleps, vrndscaleps_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vrndscaleps_xhik7xhi, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vrndscaleps_xhik7ld, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscaleps_xhik7bcst, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vrndscaleps_ylok0ylo, vrndscaleps, vrndscaleps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vrndscaleps_ylok0ld, vrndscaleps, vrndscaleps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscaleps_ylok0bcst, vrndscaleps, vrndscaleps_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vrndscaleps_yhik7yhi, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vrndscaleps_yhik7ld, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscaleps_yhik7bcst, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vrndscaleps_zlok0zlo, vrndscaleps, vrndscaleps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vrndscaleps_zlok0ld, vrndscaleps, vrndscaleps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscaleps_zlok0bcst, vrndscaleps, vrndscaleps_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vrndscaleps_zhik7zhi, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vrndscaleps_zhik7ld, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscaleps_zhik7bcst, vrndscaleps, vrndscaleps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vrndscalepd_xlok0xlo, vrndscalepd, vrndscalepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vrndscalepd_xlok0ld, vrndscalepd, vrndscalepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscalepd_xlok0bcst, vrndscalepd, vrndscalepd_mask, 0, REGARG(XMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrndscalepd_xhik7xhi, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vrndscalepd_xhik7ld, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vrndscalepd_xhik7bcst, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrndscalepd_ylok0ylo, vrndscalepd, vrndscalepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vrndscalepd_ylok0ld, vrndscalepd, vrndscalepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscalepd_ylok0bcst, vrndscalepd, vrndscalepd_mask, 0, REGARG(YMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrndscalepd_yhik7yhi, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vrndscalepd_yhik7ld, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vrndscalepd_yhik7bcst, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrndscalepd_zlok0zlo, vrndscalepd, vrndscalepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vrndscalepd_zlok0ld, vrndscalepd, vrndscalepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscalepd_zlok0bcst, vrndscalepd, vrndscalepd_mask, 0, REGARG(ZMM0), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrndscalepd_zhik7zhi, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vrndscalepd_zhik7ld, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vrndscalepd_zhik7bcst, vrndscalepd, vrndscalepd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vrsqrt14ss_xlok0xloxmm, vrsqrt14ss, vrsqrt14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrsqrt14ss_xlok0xmmld, vrsqrt14ss, vrsqrt14ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt14ss_xhik7xhixmm, vrsqrt14ss, vrsqrt14ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrsqrt14ss_xhik7xmmld, vrsqrt14ss, vrsqrt14ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt14sd_xlok0xloxmm, vrsqrt14sd, vrsqrt14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrsqrt14sd_xlok0xmmld, vrsqrt14sd, vrsqrt14sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrsqrt14sd_xhik7xhixmm, vrsqrt14sd, vrsqrt14sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrsqrt14sd_xhik7xmmld, vrsqrt14sd, vrsqrt14sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrsqrt28ss_xlok0xloxmm, vrsqrt28ss, vrsqrt28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vrsqrt28ss_xlok0xmmld, vrsqrt28ss, vrsqrt28ss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt28ss_xhik7xhixmm, vrsqrt28ss, vrsqrt28ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vrsqrt28ss_xhik7xmmld, vrsqrt28ss, vrsqrt28ss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vrsqrt28sd_xlok0xloxmm, vrsqrt28sd, vrsqrt28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vrsqrt28sd_xlok0xmmld, vrsqrt28sd, vrsqrt28sd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vrsqrt28sd_xhik7xhixmm, vrsqrt28sd, vrsqrt28sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vrsqrt28sd_xhik7xmmld, vrsqrt28sd, vrsqrt28sd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vscalefps_xlok0xloxlo, vscalefps, vscalefps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vscalefps_xlok0xlold, vscalefps, vscalefps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vscalefps_xlok0xlobcst, vscalefps, vscalefps_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_4))
OPCODE(vscalefps_xhik7xhixhi, vscalefps, vscalefps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vscalefps_xhik7xhild, vscalefps, vscalefps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vscalefps_xhik7xhibcst, vscalefps, vscalefps_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_4))
OPCODE(vscalefps_ylok0yloylo, vscalefps, vscalefps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vscalefps_ylok0ylold, vscalefps, vscalefps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vscalefps_ylok0ylobcst, vscalefps, vscalefps_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_4))
OPCODE(vscalefps_yhik7yhiyhi, vscalefps, vscalefps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vscalefps_yhik7yhild, vscalefps, vscalefps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vscalefps_yhik7yhibcst, vscalefps, vscalefps_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_4))
OPCODE(vscalefps_zlok0zlozlo, vscalefps, vscalefps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vscalefps_zlok0zlold, vscalefps, vscalefps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vscalefps_zlok0zlobcst, vscalefps, vscalefps_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_4))
OPCODE(vscalefps_zhik7zhizhi, vscalefps, vscalefps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vscalefps_zhik7zhild, vscalefps, vscalefps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vscalefps_zhik7zhibcst, vscalefps, vscalefps_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_4))
OPCODE(vscalefpd_xlok0xloxlo, vscalefpd, vscalefpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), REGARG(XMM2))
OPCODE(vscalefpd_xlok0xlold, vscalefpd, vscalefpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vscalefpd_xlok0xlobcst, vscalefpd, vscalefpd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vscalefpd_xhik7xhixhi, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vscalefpd_xhik7xhild, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vscalefpd_xhik7xhibcst, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vscalefpd_ylok0yloylo, vscalefpd, vscalefpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), REGARG(YMM2))
OPCODE(vscalefpd_ylok0ylold, vscalefpd, vscalefpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vscalefpd_ylok0ylobcst, vscalefpd, vscalefpd_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vscalefpd_yhik7yhiyhi, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vscalefpd_yhik7yhild, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vscalefpd_yhik7yhibcst, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vscalefpd_zlok0zlozlo, vscalefpd, vscalefpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vscalefpd_zlok0zlold, vscalefpd, vscalefpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vscalefpd_zlok0zlobcst, vscalefpd, vscalefpd_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vscalefpd_zhik7zhizhi, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vscalefpd_zhik7zhild, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vscalefpd_zhik7zhibcst, vscalefpd, vscalefpd_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vscalefss_xlok0xloxmm, vscalefss, vscalefss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vscalefss_xlok0xmmld, vscalefss, vscalefss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vscalefss_xhik7xhixmm, vscalefss, vscalefss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vscalefss_xhik7xmmld, vscalefss, vscalefss_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vscalefsd_xlok0xloxmm, vscalefsd, vscalefsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vscalefsd_xlok0xmmld, vscalefsd, vscalefsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vscalefsd_xhik7xhixmm, vscalefsd, vscalefsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vscalefsd_xhik7xmmld, vscalefsd, vscalefsd_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG_PARTIAL(XMM2, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vfpclassps_k1k0xlo, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vfpclassps_k1k0ld16, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclassps_k1k0bcst, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vfpclassps_k6k7xhi, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vfpclassps_k6k7ld16, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclassps_k6k7bcst, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vfpclassps_k1k0ylo, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vfpclassps_k1k0ld32, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclassps_k6k7yhi, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vfpclassps_k6k7ld32, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclassps_k1k0zlo, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vfpclassps_k1k0ld64, vfpclassps, vfpclassps_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclassps_k6k7zhi, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vfpclassps_k6k7ld64, vfpclassps, vfpclassps_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclasspd_k1k0xlo, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(XMM1))
OPCODE(vfpclasspd_k1k0ld16, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclasspd_k1k0bcst, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vfpclasspd_k6k7xhi, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(XMM31))
OPCODE(vfpclasspd_k6k7ld16, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_16))
OPCODE(vfpclasspd_k6k7bcst, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vfpclasspd_k1k0ylo, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(YMM1))
OPCODE(vfpclasspd_k1k0ld32, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclasspd_k6k7yhi, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(YMM31))
OPCODE(vfpclasspd_k6k7ld32, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_32))
OPCODE(vfpclasspd_k1k0zlo, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG(ZMM1))
OPCODE(vfpclasspd_k1k0ld64, vfpclasspd, vfpclasspd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclasspd_k6k7zhi, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), REGARG(ZMM31))
OPCODE(vfpclasspd_k6k7ld64, vfpclasspd, vfpclasspd_mask, X64_ONLY, REGARG(K6), REGARG(K7),
       IMMARG(OPSZ_1), MEMARG(OPSZ_64))
OPCODE(vfpclassss_k1k0xloxmm, vfpclassss, vfpclassss_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_4))
OPCODE(vfpclassss_k1k0xmmld, vfpclassss, vfpclassss_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vfpclassss_k6k7xhixmm, vfpclassss, vfpclassss_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_4))
OPCODE(vfpclassss_k6k7xmmld, vfpclassss, vfpclassss_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_4))
OPCODE(vfpclasssd_k1k0xloxmm, vfpclasssd, vfpclasssd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), REGARG_PARTIAL(XMM1, OPSZ_8))
OPCODE(vfpclasssd_k1k0xmmld, vfpclasssd, vfpclasssd_mask, 0, REGARG(K1), REGARG(K0),
       IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vfpclasssd_k6k7xhixmm, vfpclasssd, vfpclasssd_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), REGARG_PARTIAL(XMM17, OPSZ_8))
OPCODE(vfpclasssd_k6k7xmmld, vfpclasssd, vfpclasssd_mask, X64_ONLY, REGARG(K6),
       REGARG(K7), IMMARG(OPSZ_1), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_xlok0xloxlo, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmadd52huq_xlok0xlold, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmadd52huq_xlok0xlobcst, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_xhik7xhixhi, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmadd52huq_xhik7xhild, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmadd52huq_xhik7xhibcst, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_ylok0yloylo, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmadd52huq_ylok0ylold, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmadd52huq_ylok0ylobcst, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_yhik7yhiyhi, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmadd52huq_yhik7yhild, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmadd52huq_yhik7yhibcst, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_zlok0zlozlo, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmadd52huq_zlok0zlold, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmadd52huq_zlok0zlobcst, vpmadd52huq, vpmadd52huq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmadd52huq_zhik7zhizhi, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmadd52huq_zhik7zhild, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmadd52huq_zhik7zhibcst, vpmadd52huq, vpmadd52huq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vpmadd52luq_xlok0xloxlo, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), REGARG(XMM2))
OPCODE(vpmadd52luq_xlok0xlold, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG(XMM1), MEMARG(OPSZ_16))
OPCODE(vpmadd52luq_xlok0xlobcst, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(XMM0),
       REGARG(K0), REGARG(XMM1), MEMARG(OPSZ_8))
OPCODE(vpmadd52luq_xhik7xhixhi, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), REGARG(XMM31))
OPCODE(vpmadd52luq_xhik7xhild, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_16))
OPCODE(vpmadd52luq_xhik7xhibcst, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(XMM16),
       REGARG(K7), REGARG(XMM17), MEMARG(OPSZ_8))
OPCODE(vpmadd52luq_ylok0yloylo, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), REGARG(YMM2))
OPCODE(vpmadd52luq_ylok0ylold, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(YMM0), REGARG(K0),
       REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vpmadd52luq_ylok0ylobcst, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(YMM0),
       REGARG(K0), REGARG(YMM1), MEMARG(OPSZ_8))
OPCODE(vpmadd52luq_yhik7yhiyhi, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), REGARG(YMM31))
OPCODE(vpmadd52luq_yhik7yhild, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_32))
OPCODE(vpmadd52luq_yhik7yhibcst, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(YMM16),
       REGARG(K7), REGARG(YMM17), MEMARG(OPSZ_8))
OPCODE(vpmadd52luq_zlok0zlozlo, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), REGARG(ZMM2))
OPCODE(vpmadd52luq_zlok0zlold, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(ZMM0), REGARG(K0),
       REGARG(ZMM1), MEMARG(OPSZ_64))
OPCODE(vpmadd52luq_zlok0zlobcst, vpmadd52luq, vpmadd52luq_mask, 0, REGARG(ZMM0),
       REGARG(K0), REGARG(ZMM1), MEMARG(OPSZ_8))
OPCODE(vpmadd52luq_zhik7zhizhi, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), REGARG(ZMM31))
OPCODE(vpmadd52luq_zhik7zhild, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_64))
OPCODE(vpmadd52luq_zhik7zhibcst, vpmadd52luq, vpmadd52luq_mask, X64_ONLY, REGARG(ZMM16),
       REGARG(K7), REGARG(ZMM17), MEMARG(OPSZ_8))
OPCODE(vsqrtss_xlok0xloxlo, vsqrtss, vsqrtss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), REGARG_PARTIAL(XMM2, OPSZ_4))
OPCODE(vsqrtss_xlok0xlomem, vsqrtss, vsqrtss_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vsqrtss_xhik7xhixhi, vsqrtss, vsqrtss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM17, OPSZ_12), REGARG_PARTIAL(XMM18, OPSZ_4))
OPCODE(vsqrtss_xhik7xhimem, vsqrtss, vsqrtss_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM17, OPSZ_12), MEMARG(OPSZ_4))
OPCODE(vsqrtsd_xlok0xloxlo, vsqrtsd, vsqrtsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), REGARG_PARTIAL(XMM2, OPSZ_8))
OPCODE(vsqrtsd_xlok0xlomem, vsqrtsd, vsqrtsd_mask, 0, REGARG(XMM0), REGARG(K0),
       REGARG_PARTIAL(XMM1, OPSZ_8), MEMARG(OPSZ_8))
OPCODE(vsqrtsd_xhik7xhixhi, vsqrtsd, vsqrtsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM17, OPSZ_8), REGARG_PARTIAL(XMM18, OPSZ_8))
OPCODE(vsqrtsd_xhik7xhimem, vsqrtsd, vsqrtsd_mask, X64_ONLY, REGARG(XMM16), REGARG(K7),
       REGARG_PARTIAL(XMM17, OPSZ_8), MEMARG(OPSZ_8))
/* AVX512 VNNI */
OPCODE(vpdpbusd_zhik7zhizhi, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpbusds_zhik7zhizhi, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpwssd_zhik7zhizhi, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpwssds_zhik7zhizhi, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpbusd_xloxloxlo, vpdpbusd, vpdpbusd_mask, 0, REGARG(XMM0), REGARG(K5),
       REGARG(XMM4), REGARG(XMM3))
OPCODE(vpdpbusds_xloxloxlo, vpdpbusds, vpdpbusds_mask, 0, REGARG(XMM0), REGARG(K5),
       REGARG(XMM4), REGARG(XMM3))
OPCODE(vpdpwssd_xloxloxlo, vpdpwssd, vpdpwssd_mask, 0, REGARG(XMM0), REGARG(K5),
       REGARG(XMM4), REGARG(XMM3))
OPCODE(vpdpwssds_xloxloxlo, vpdpwssds, vpdpwssds_mask, 0, REGARG(XMM0), REGARG(K5),
       REGARG(XMM4), REGARG(XMM3))

/* vpdpbusd ZMM */
OPCODE(vpdpbusd_zlok3zhizhi, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpbusd_zlok3zhild, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM24), MEMARG(OPSZ_64))
OPCODE(vpdpbusd_zlok3zlobcst, vpdpbusd, vpdpbusd_mask, 0, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM2), MEMARG(OPSZ_4))
/* vpdpbusd YMM */
OPCODE(vpdpbusd_ylok3yhiyhi, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(YMM0), REGARG(K3),
       REGARG(YMM24), REGARG(YMM31))
OPCODE(vpdpbusd_ylok3yhild, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(YMM0), REGARG(K3),
       REGARG(YMM24), MEMARG(OPSZ_32))
OPCODE(vpdpbusd_ylok3ylobcst, vpdpbusd, vpdpbusd_mask, 0, REGARG(YMM0), REGARG(K3),
       REGARG(YMM2), MEMARG(OPSZ_4))
/* vpdpbusd XMM */
OPCODE(vpdpbusd_xlok3xhixhi, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(XMM0), REGARG(K3),
       REGARG(XMM24), REGARG(XMM31))
OPCODE(vpdpbusd_xlok3xhild, vpdpbusd, vpdpbusd_mask, X64_ONLY, REGARG(XMM0), REGARG(K3),
       REGARG(XMM24), MEMARG(OPSZ_16))
OPCODE(vpdpbusd_xlok3xlobcst, vpdpbusd, vpdpbusd_mask, 0, REGARG(XMM0), REGARG(K3),
       REGARG(XMM2), MEMARG(OPSZ_4))

/* vpdpbusds ZMM */
OPCODE(vpdpbusds_zlok3zhizhi, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpbusds_zlok3zhild, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), MEMARG(OPSZ_64))
OPCODE(vpdpbusds_zlok3zlobcst, vpdpbusds, vpdpbusds_mask, 0, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM2), MEMARG(OPSZ_4))
/* vpdpbusds YMM */
OPCODE(vpdpbusds_ylok3yhiyhi, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K3), REGARG(YMM24), REGARG(YMM31))
OPCODE(vpdpbusds_ylok3yhild, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K3), REGARG(YMM24), MEMARG(OPSZ_32))
OPCODE(vpdpbusds_ylok3ylobcst, vpdpbusds, vpdpbusds_mask, 0, REGARG(YMM0), REGARG(K3),
       REGARG(YMM2), MEMARG(OPSZ_4))
/* vpdpbusds XMM */
OPCODE(vpdpbusds_xlok3xhixhi, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K3), REGARG(XMM24), REGARG(XMM31))
OPCODE(vpdpbusds_xlok3xhild, vpdpbusds, vpdpbusds_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K3), REGARG(XMM24), MEMARG(OPSZ_16))
OPCODE(vpdpbusds_xlok3xlobcst, vpdpbusds, vpdpbusds_mask, 0, REGARG(XMM0), REGARG(K3),
       REGARG(XMM2), MEMARG(OPSZ_4))

/* vpdpwssd ZMM */
OPCODE(vpdpwssd_zlok3zhizhi, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpwssd_zlok3zhild, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM24), MEMARG(OPSZ_64))
OPCODE(vpdpwssd_zlok3zlobcst, vpdpwssd, vpdpwssd_mask, 0, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM2), MEMARG(OPSZ_4))
/* vpdpwssd YMM */
OPCODE(vpdpwssd_ylok3yhiyhi, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(YMM0), REGARG(K3),
       REGARG(YMM24), REGARG(YMM31))
OPCODE(vpdpwssd_ylok3yhild, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(YMM0), REGARG(K3),
       REGARG(YMM24), MEMARG(OPSZ_32))
OPCODE(vpdpwssd_ylok3ylobcst, vpdpwssd, vpdpwssd_mask, 0, REGARG(YMM0), REGARG(K3),
       REGARG(YMM2), MEMARG(OPSZ_4))
/* vpdpwssd XMM */
OPCODE(vpdpwssd_xlok3xhixhi, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(XMM0), REGARG(K3),
       REGARG(XMM24), REGARG(XMM31))
OPCODE(vpdpwssd_xlok3xhild, vpdpwssd, vpdpwssd_mask, X64_ONLY, REGARG(XMM0), REGARG(K3),
       REGARG(XMM24), MEMARG(OPSZ_16))
OPCODE(vpdpwssd_xlok3xlobcst, vpdpwssd, vpdpwssd_mask, 0, REGARG(XMM0), REGARG(K3),
       REGARG(XMM2), MEMARG(OPSZ_4))

/* vpdpwssds ZMM */
OPCODE(vpdpwssds_zlok3zhizhi, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vpdpwssds_zlok3zhild, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), MEMARG(OPSZ_64))
OPCODE(vpdpwssds_zlok3zlobcst, vpdpwssds, vpdpwssds_mask, 0, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM2), MEMARG(OPSZ_4))
/* vpdpwssds YMM */
OPCODE(vpdpwssds_ylok3yhiyhi, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K3), REGARG(YMM24), REGARG(YMM31))
OPCODE(vpdpwssds_ylok3yhild, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K3), REGARG(YMM24), MEMARG(OPSZ_32))
OPCODE(vpdpwssds_ylok3ylobcst, vpdpwssds, vpdpwssds_mask, 0, REGARG(YMM0), REGARG(K3),
       REGARG(YMM2), MEMARG(OPSZ_4))
/* vpdpwssds XMM */
OPCODE(vpdpwssds_xlok3xhixhi, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K3), REGARG(XMM24), REGARG(XMM31))
OPCODE(vpdpwssds_xlok3xhild, vpdpwssds, vpdpwssds_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K3), REGARG(XMM24), MEMARG(OPSZ_16))
OPCODE(vpdpwssds_xlok3xlobcst, vpdpwssds, vpdpwssds_mask, 0, REGARG(XMM0), REGARG(K3),
       REGARG(XMM2), MEMARG(OPSZ_4))

/* AVX512 BF16 */
/* vcvtne2ps2bf16 ZMM */
OPCODE(vcvtne2ps2bf16_zlok3zhizhi, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, X64_ONLY,
       REGARG(ZMM0), REGARG(K3), REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vcvtne2ps2bf16_zlok3zhild, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, X64_ONLY,
       REGARG(ZMM0), REGARG(K3), REGARG(ZMM24), MEMARG(OPSZ_64))
OPCODE(vcvtne2ps2bf16_zlok3zlobcst, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, 0, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM2), MEMARG(OPSZ_4))
/* vcvtne2ps2bf16 YMM */
OPCODE(vcvtne2ps2bf16_ylok3yhiyhi, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, X64_ONLY,
       REGARG(YMM0), REGARG(K3), REGARG(YMM24), REGARG(YMM31))
OPCODE(vcvtne2ps2bf16_ylok3yhild, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, X64_ONLY,
       REGARG(YMM0), REGARG(K3), REGARG(YMM24), MEMARG(OPSZ_32))
OPCODE(vcvtne2ps2bf16_ylok3ylobcst, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, 0, REGARG(YMM0),
       REGARG(K3), REGARG(YMM2), MEMARG(OPSZ_4))
/* vcvtne2ps2bf16 XMM */
OPCODE(vcvtne2ps2bf16_xlok3xhixhi, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, X64_ONLY,
       REGARG(XMM0), REGARG(K3), REGARG(XMM24), REGARG(XMM31))
OPCODE(vcvtne2ps2bf16_xlok3xhild, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, X64_ONLY,
       REGARG(XMM0), REGARG(K3), REGARG(XMM24), MEMARG(OPSZ_16))
OPCODE(vcvtne2ps2bf16_xlok3xlobcst, vcvtne2ps2bf16, vcvtne2ps2bf16_mask, 0, REGARG(XMM0),
       REGARG(K3), REGARG(XMM2), MEMARG(OPSZ_4))

/* vdpbf16ps ZMM */
OPCODE(vdpbf16ps_zlok3zhizhi, vdpbf16ps, vdpbf16ps_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), REGARG(ZMM31))
OPCODE(vdpbf16ps_zlok3zhild, vdpbf16ps, vdpbf16ps_mask, X64_ONLY, REGARG(ZMM0),
       REGARG(K3), REGARG(ZMM24), MEMARG(OPSZ_64))
OPCODE(vdpbf16ps_zlok3zlobcst, vdpbf16ps, vdpbf16ps_mask, 0, REGARG(ZMM0), REGARG(K3),
       REGARG(ZMM2), MEMARG(OPSZ_4))
/* vdpbf16ps YMM */
OPCODE(vdpbf16ps_ylok3yhiyhi, vdpbf16ps, vdpbf16ps_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K3), REGARG(YMM24), REGARG(YMM31))
OPCODE(vdpbf16ps_ylok3yhild, vdpbf16ps, vdpbf16ps_mask, X64_ONLY, REGARG(YMM0),
       REGARG(K3), REGARG(YMM24), MEMARG(OPSZ_32))
OPCODE(vdpbf16ps_ylok3ylobcst, vdpbf16ps, vdpbf16ps_mask, 0, REGARG(YMM0), REGARG(K3),
       REGARG(YMM2), MEMARG(OPSZ_4))
/* vdpbf16ps XMM */
OPCODE(vdpbf16ps_xlok3xhixhi, vdpbf16ps, vdpbf16ps_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K3), REGARG(XMM24), REGARG(XMM31))
OPCODE(vdpbf16ps_xlok3xhild, vdpbf16ps, vdpbf16ps_mask, X64_ONLY, REGARG(XMM0),
       REGARG(K3), REGARG(XMM24), MEMARG(OPSZ_16))
OPCODE(vdpbf16ps_xlok3xlobcst, vdpbf16ps, vdpbf16ps_mask, 0, REGARG(XMM0), REGARG(K3),
       REGARG(XMM2), MEMARG(OPSZ_4))
