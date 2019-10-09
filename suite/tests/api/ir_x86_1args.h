/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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
/* single memory argument */

OPCODE(lldt, lldt, lldt, 0, MEMARG(OPSZ_2))
OPCODE(ltr, ltr, ltr, 0, MEMARG(OPSZ_2))
OPCODE(verr, verr, verr, 0, MEMARG(OPSZ_2))
OPCODE(verw, verw, verw, 0, MEMARG(OPSZ_2))
OPCODE(lgdt, lgdt, lgdt, 0, MEMARG(OPSZ_lgdt))
OPCODE(lidt, lidt, lidt, 0, MEMARG(OPSZ_lidt))
OPCODE(lmsw, lmsw, lmsw, 0, MEMARG(OPSZ_2))
OPCODE(invlpg, invlpg, invlpg, 0, MEMARG(OPSZ_lea))
OPCODE(fxrstor32, fxrstor32, fxrstor32, 0, MEMARG(OPSZ_fxrstor))
OPCODE(fxrstor64, fxrstor64, fxrstor64, X64_ONLY, MEMARG(OPSZ_fxrstor))
OPCODE(ldmxcsr, ldmxcsr, ldmxcsr, 0, MEMARG(OPSZ_4))
OPCODE(nop_modrm, nop_modrm, nop_modrm, 0, MEMARG(OPSZ_4))
OPCODE(prefetchnta, prefetchnta, prefetchnta, 0, MEMARG(OPSZ_prefetch))
OPCODE(prefetcht0, prefetcht0, prefetcht0, 0, MEMARG(OPSZ_prefetch))
OPCODE(prefetcht1, prefetcht1, prefetcht1, 0, MEMARG(OPSZ_prefetch))
OPCODE(prefetcht2, prefetcht2, prefetcht2, 0, MEMARG(OPSZ_prefetch))
OPCODE(prefetch, prefetch, prefetch, 0, MEMARG(OPSZ_prefetch))
OPCODE(prefetchw, prefetchw, prefetchw, 0, MEMARG(OPSZ_prefetch))
OPCODE(clflush, clflush, clflush, 0, MEMARG(OPSZ_clflush))
OPCODE(fldenv, fldenv, fldenv, 0, MEMARG(OPSZ_fldenv))
OPCODE(fldcw, fldcw, fldcw, 0, MEMARG(OPSZ_2))
OPCODE(frstor, frstor, frstor, 0, MEMARG(OPSZ_frstor))
OPCODE(fcom, fcom, fcom, 0, MEMARG(OPSZ_8))
OPCODE(fcomp, fcomp, fcomp, 0, MEMARG(OPSZ_8))
OPCODE(sldt, sldt, sldt, 0, MEMARG(OPSZ_2))
OPCODE(str, str, str, 0, MEMARG(OPSZ_2))
OPCODE(sgdt, sgdt, sgdt, 0, MEMARG(OPSZ_sgdt))
OPCODE(sidt, sidt, sidt, 0, MEMARG(OPSZ_sidt))
OPCODE(smsw, smsw, smsw, 0, MEMARG(OPSZ_2))
OPCODE(fxsave32, fxsave32, fxsave32, 0, MEMARG(OPSZ_fxsave))
OPCODE(fxsave64, fxsave64, fxsave64, X64_ONLY, MEMARG(OPSZ_fxsave))
OPCODE(stmxcsr, stmxcsr, stmxcsr, 0, MEMARG(OPSZ_4))
OPCODE(fnstenv, fnstenv, fnstenv, 0, MEMARG(OPSZ_fnstenv))
OPCODE(fnstcw, fnstcw, fnstcw, 0, MEMARG(OPSZ_2))
OPCODE(fnsave, fnsave, fnsave, 0, MEMARG(OPSZ_fnsave))
OPCODE(fnstsw, fnstsw, fnstsw, 0, MEMARG(OPSZ_2))
OPCODE(inc, inc, inc, 0, MEMARG(OPSZ_4))
OPCODE(dec, dec, dec, 0, MEMARG(OPSZ_4))
OPCODE(not, not, not, 0, MEMARG(OPSZ_4))
OPCODE(neg, neg, neg, 0, MEMARG(OPSZ_4))
OPCODE(fst, fst, fst, 0, MEMARG(OPSZ_4))
OPCODE(fstp, fstp, fstp, 0, MEMARG(OPSZ_10))
OPCODE(fld, fld, fld, 0, MEMARG(OPSZ_10))
OPCODE(fist, fist, fist, 0, MEMARG(OPSZ_4))
OPCODE(fistp, fistp, fistp, 0, MEMARG(OPSZ_4))
OPCODE(fisttp, fisttp, fisttp, 0, MEMARG(OPSZ_4))
OPCODE(fbstp, fbstp, fbstp, 0, MEMARG(OPSZ_10))
OPCODE(fild, fild, fild, 0, MEMARG(OPSZ_4))
OPCODE(fbld, fbld, fbld, 0, MEMARG(OPSZ_10))
OPCODE(fiadd, fiadd, fiadd, 0, MEMARG(OPSZ_4))
OPCODE(fimul, fimul, fimul, 0, MEMARG(OPSZ_4))
OPCODE(fidiv, fidiv, fidiv, 0, MEMARG(OPSZ_4))
OPCODE(fidivr, fidivr, fidivr, 0, MEMARG(OPSZ_4))
OPCODE(fisub, fisub, fisub, 0, MEMARG(OPSZ_4))
OPCODE(fisubr, fisubr, fisubr, 0, MEMARG(OPSZ_4))
OPCODE(ficom, ficom, ficom, 0, MEMARG(OPSZ_4))
OPCODE(ficomp, ficomp, ficomp, 0, MEMARG(OPSZ_4))
OPCODE(imul1, imul, imul_1, 0, MEMARG(OPSZ_1))
OPCODE(imul4, imul, imul_4, 0, MEMARG(OPSZ_4))
OPCODE(mul1, mul, mul_1, 0, MEMARG(OPSZ_1))
OPCODE(mul4, mul, mul_4, 0, MEMARG(OPSZ_4))
OPCODE(div1, div, div_1, 0, MEMARG(OPSZ_1))
OPCODE(div4, div, div_4, 0, MEMARG(OPSZ_4))
OPCODE(idiv1, idiv, idiv_1, 0, MEMARG(OPSZ_1))
OPCODE(idiv4, idiv, idiv_4, 0, MEMARG(OPSZ_4))
OPCODE(pop, pop, pop, 0, MEMARG(OPSZ_4x8))
OPCODE(push, push, push, 0, MEMARG(OPSZ_4x8))
OPCODE(cmpxchg8b, cmpxchg8b, cmpxchg8b, 0, MEMARG(OPSZ_8))
OPCODE(jmp_ind, jmp_ind, jmp_ind, 0, MEMARG(OPSZ_4x8))
XOPCODE(jump_mem, jmp_ind, jump_mem, 0, MEMARG(OPSZ_4x8))
XOPCODE(jump_reg, jmp_ind, jump_reg, 0, REGARG(XAX))
OPCODE(call_ind, call_ind, call_ind, 0, MEMARG(OPSZ_4x8))
OPCODE(vmptrst, vmptrst, vmptrst, X64_ONLY, MEMARG(OPSZ_8))
OPCODE(vmptrld, vmptrld, vmptrld, X64_ONLY, MEMARG(OPSZ_8))
OPCODE(vmxon, vmxon, vmxon, X64_ONLY, MEMARG(OPSZ_8))
OPCODE(vmclear, vmclear, vmclear, X64_ONLY, MEMARG(OPSZ_8))
OPCODE(xsave32, xsave32, xsave32, 0, MEMARG(OPSZ_xsave))
OPCODE(xsave64, xsave64, xsave64, X64_ONLY, MEMARG(OPSZ_xsave))
OPCODE(xrstor32, xrstor32, xrstor32, 0, MEMARG(OPSZ_xsave))
OPCODE(xrstor64, xrstor64, xrstor64, X64_ONLY, MEMARG(OPSZ_xsave))
OPCODE(xsaveopt32, xsaveopt32, xsaveopt32, 0, MEMARG(OPSZ_xsave))
OPCODE(xsaveopt64, xsaveopt64, xsaveopt64, X64_ONLY, MEMARG(OPSZ_xsave))
OPCODE(xsavec32, xsavec32, xsavec32, 0, MEMARG(OPSZ_xsave))
OPCODE(xsavec64, xsavec64, xsavec64, X64_ONLY, MEMARG(OPSZ_xsave))

/****************************************************************************/
/* single immed argument */

OPCODE(out_1_imm, out, out_1_imm, 0, IMMARG(OPSZ_1))
OPCODE(out_4_imm, out, out_4_imm, 0, IMMARG(OPSZ_1))
OPCODE(in_1_imm, in, in_1_imm, 0, IMMARG(OPSZ_1))
OPCODE(in_4_imm, in, in_4_imm, 0, IMMARG(OPSZ_1))
OPCODE(aam, aam, aam, X86_ONLY, IMMARG(OPSZ_1))
OPCODE(aad, aad, aad, X86_ONLY, IMMARG(OPSZ_1))
OPCODE(ret_imm, ret, ret_imm, 0, IMMARG(OPSZ_2))
OPCODE(ret_far_imm, ret_far, ret_far_imm, 0, IMMARG(OPSZ_2))
OPCODE(push_imm, push_imm, push_imm, 0, IMMARG(OPSZ_4))
OPCODE(int, int, int, 0, IMMARG(OPSZ_1))
XOPCODE(interrupt, int, interrupt, 0, IMMARG(OPSZ_1))
OPCODE(xabort, xabort, xabort, 0, IMMARG(OPSZ_1))

/****************************************************************************/
/* single register argument */

OPCODE(bswap, bswap, bswap, 0, REGARG(EAX))
OPCODE(fcomi, fcomi, fcomi, 0, REGARG(ST0))
OPCODE(fcomip, fcomip, fcomip, 0, REGARG(ST0))
OPCODE(fucomi, fucomi, fucomi, 0, REGARG(ST0))
OPCODE(fucomip, fucomip, fucomip, 0, REGARG(ST0))
OPCODE(fucom, fucom, fucom, 0, REGARG(ST0))
OPCODE(fucomp, fucomp, fucomp, 0, REGARG(ST0))
OPCODE(ffree, ffree, ffree, 0, REGARG(ST0))
OPCODE(faddp, faddp, faddp, 0, REGARG(ST0))
OPCODE(fmulp, fmulp, fmulp, 0, REGARG(ST0))
OPCODE(fdivp, fdivp, fdivp, 0, REGARG(ST0))
OPCODE(fdivrp, fdivrp, fdivrp, 0, REGARG(ST0))
OPCODE(fsubp, fsubp, fsubp, 0, REGARG(ST0))
OPCODE(fsubrp, fsubrp, fsubrp, 0, REGARG(ST0))
OPCODE(fxch, fxch, fxch, 0, REGARG(ST0))
OPCODE(ffreep, ffreep, ffreep, 0, REGARG(ST0))
OPCODE(rdrand, rdrand, rdrand, 0, REGARG(ESI))
OPCODE(rdseed, rdseed, rdseed, 0, REGARG(EDI))
OPCODE(rdfsbase, rdfsbase, rdfsbase, X64_ONLY, REGARG(EBX))
OPCODE(rdgsbase, rdgsbase, rdgsbase, X64_ONLY, REGARG(EBX))
OPCODE(wrfsbase, wrfsbase, wrfsbase, X64_ONLY, REGARG(EBX))
OPCODE(wrgsbase, wrgsbase, wrgsbase, X64_ONLY, REGARG(EBX))

/****************************************************************************/
/* single pc/instr argument */

OPCODE(jmp, jmp, jmp, 0, TGTARG)
XOPCODE(jump, jmp, jump, 0, TGTARG)
OPCODE(jmp_short, jmp_short, jmp_short, 0, TGTARG)
XOPCODE(jump_short, jmp_short, jump_short, 0, TGTARG)
OPCODE(jecxz, jecxz, jecxz, 0, TGTARG)
OPCODE(jcxz, jcxz, jcxz, X86_ONLY, TGTARG)
OPCODE(loopne, loopne, loopne, 0, TGTARG)
OPCODE(loope, loope, loope, 0, TGTARG)
OPCODE(loop, loop, loop, 0, TGTARG)
OPCODE(jmp_far, jmp_far, jmp_far, X86_ONLY, opnd_create_far_pc(0x1234, 0))
OPCODE(jmp_far_ind, jmp_far_ind, jmp_far_ind, 0, MEMARG(OPSZ_6))
OPCODE(call, call, call, 0, TGTARG)
XOPCODE(x_call, call, call, 0, TGTARG)
OPCODE(call_far, call_far, call_far, X86_ONLY, opnd_create_far_pc(0x1234, 0))
OPCODE(call_far_ind, call_far_ind, call_far_ind, 0, MEMARG(OPSZ_6))
OPCODE(xbegin, xbegin, xbegin, 0, TGTARG)

/****************************************************************************/
/* LWP */
OPCODE(llwpcb, llwpcb, llwpcb, 0, REGARG(EAX))
OPCODE(slwpcb, slwpcb, slwpcb, 0, REGARG(EAX))

/****************************************************************************/
/* PT */
OPCODE(ptwrite_r32, ptwrite, ptwrite, X86_ONLY, REGARG(EAX))
OPCODE(ptwrite_r64, ptwrite, ptwrite, X64_ONLY, REGARG(RAX))
OPCODE(ptwrite_mem, ptwrite, ptwrite, 0, MEMARG(OPSZ_ptwrite))
