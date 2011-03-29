/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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

OPCODE(cmovae, cmovcc, 0, OP_cmovae, REGARG(EAX), MEMARG(OPSZ_4))
OPCODE(imul, imul_imm, 0, REGARG(EAX), MEMARG(OPSZ_4), IMMARG(OPSZ_4))

OPCODE(extrq, extrq_imm, 0, REGARG(XMM0), IMMARG(OPSZ_1), IMMARG(OPSZ_1))

OPCODE(shld, shld, 0, MEMARG(OPSZ_4), REGARG(EAX), IMMARG(OPSZ_1))
OPCODE(shrd, shrd, 0, MEMARG(OPSZ_4), REGARG(EAX), IMMARG(OPSZ_1))

OPCODE(pshufw, pshufw, 0, REGARG(MM0), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(pshufd, pshufd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pshufhw, pshufhw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pshuflw, pshuflw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pinsrw, pinsrw, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(pextrw, pextrw, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(pextrb, pextrb, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(pextrd, pextrd, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(extractps, extractps, 0, REGARG(EAX), REGARG(XMM0), IMMARG(OPSZ_1))
OPCODE(roundps, roundps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(roundpd, roundpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(roundss, roundss, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(roundsd, roundsd, 0, REGARG(XMM0), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(blendps, blendps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(blendpd, blendpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pblendw, pblendw, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pinsrb, pinsrb, 0, REGARG(XMM0), MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(insertps, insertps, 0, REGARG(XMM0), REGARG(XMM1), IMMARG(OPSZ_1))
OPCODE(pinsrd, pinsrd, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(shufps, shufps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(shufpd, shufpd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpps, cmpps, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpss, cmpss, 0, REGARG(XMM0), MEMARG(OPSZ_4), IMMARG(OPSZ_1))
OPCODE(cmppd, cmppd, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(cmpsd, cmpsd, 0, REGARG(XMM0), MEMARG(OPSZ_8), IMMARG(OPSZ_1))
OPCODE(palignr, palignr, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))

OPCODE(dpps, dpps, 0, REGARG(XMM0), MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(dppd, dppd, 0, REGARG(XMM0), MEMARG(OPSZ_1), IMMARG(OPSZ_1))
OPCODE(mpsadbw, mpsadbw, 0, REGARG(XMM0), MEMARG(OPSZ_1), IMMARG(OPSZ_1))

OPCODE(pcmpistrm, pcmpistrm, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pcmpistri, pcmpistri, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pcmpestrm, pcmpestrm, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(pcmpestri, pcmpestri, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))

OPCODE(pclmulqdq, pclmulqdq, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
OPCODE(aeskeygenassist, aeskeygenassist, 0, REGARG(XMM0), MEMARG(OPSZ_16), IMMARG(OPSZ_1))
