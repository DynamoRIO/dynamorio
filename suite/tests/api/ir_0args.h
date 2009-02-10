/* **********************************************************
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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
/* no arguments */

OPCODE(fwait, fwait, 0)
OPCODE(hlt, hlt, 0)
OPCODE(cmc, cmc, 0)
OPCODE(clc, clc, 0)
OPCODE(stc, stc, 0)
OPCODE(cli, cli, 0)
OPCODE(sti, sti, 0)
OPCODE(cld, cld, 0)
OPCODE(std, std, 0)
OPCODE(clts, clts, 0)
OPCODE(invd, invd, 0)
OPCODE(wbinvd, wbinvd, 0)
OPCODE(ud2a, ud2a, 0)
OPCODE(emms, emms, 0)
OPCODE(rsm, rsm, 0)
OPCODE(ud2b, ud2b, 0)
OPCODE(lfence, lfence, 0)
OPCODE(mfence, mfence, 0)
OPCODE(sfence, sfence, 0)
OPCODE(nop, nop, 0)
OPCODE(pause, pause, 0)
OPCODE(fnop, fnop, 0)
OPCODE(fdecstp, fdecstp, 0)
OPCODE(fincstp, fincstp, 0)
OPCODE(fnclex, fnclex, 0)
OPCODE(fninit, fninit, 0)
OPCODE(sysret, sysret, 0)
OPCODE(femms, femms, 0)
OPCODE(swapgs, swapgs, X64_ONLY)
OPCODE(vmcall, vmcall, X64_ONLY)
OPCODE(vmlaunch, vmlaunch, X64_ONLY)
OPCODE(vmresume, vmresume, X64_ONLY)
OPCODE(vmxoff, vmxoff, X64_ONLY)
OPCODE(fxam, fxam, 0)
OPCODE(sahf, sahf, 0)
OPCODE(mwait, mwait, 0)
OPCODE(monitor, monitor, 0)
OPCODE(fucompp, fucompp, 0)
OPCODE(fcompp, fcompp, 0)
OPCODE(lahf, lahf, 0)
OPCODE(sysenter, sysenter, 0)
OPCODE(sysexit, sysexit, 0)
OPCODE(syscall, syscall, 0)
OPCODE(daa, daa, X86_ONLY)
OPCODE(das, das, X86_ONLY)
OPCODE(aaa, aaa, X86_ONLY)
OPCODE(aas, aas, X86_ONLY)
OPCODE(cwde, cwde, 0)
OPCODE(xlat, xlat, 0)
OPCODE(fchs, fchs, 0)
OPCODE(fabs, fabs, 0)
OPCODE(ftst, ftst, 0)
OPCODE(fld1, fld1, 0)
OPCODE(fldl2t, fldl2t, 0)
OPCODE(fldl2e, fldl2e, 0)
OPCODE(fldpi, fldpi, 0)
OPCODE(fldlg2, fldlg2, 0)
OPCODE(fldln2, fldln2, 0)
OPCODE(fldz, fldz, 0)
OPCODE(f2xm1, f2xm1, 0)
OPCODE(fptan, fptan, 0)
OPCODE(fxtract, fxtract, 0)
OPCODE(fsqrt, fsqrt, 0)
OPCODE(fsincos, fsincos, 0)
OPCODE(frndint, frndint, 0)
OPCODE(fsin, fsin, 0)
OPCODE(fcos, fcos, 0)
OPCODE(fscale, fscale, 0)
OPCODE(fyl2x, fyl2x, 0)
OPCODE(fyl2xp1, fyl2xp1, 0)
OPCODE(fpatan, fpatan, 0)
OPCODE(fprem, fprem, 0)
OPCODE(fprem1, fprem1, 0)
OPCODE(popf, popf, 0)
OPCODE(ret, ret, 0)
OPCODE(ret_far, ret_far, 0)
OPCODE(iret, iret, 0)
OPCODE(rdtsc, rdtsc, 0)
OPCODE(cdq, cdq, 0)
OPCODE(pushf, pushf, 0)
OPCODE(int3, int3, 0)
OPCODE(into, into, X86_ONLY)
OPCODE(rdmsr, rdmsr, 0)
OPCODE(rdpmc, rdpmc, 0)
OPCODE(leave, leave, 0)
OPCODE(pusha, pusha, X86_ONLY)
OPCODE(wrmsr, wrmsr, 0)
OPCODE(cpuid, cpuid, 0)
OPCODE(popa, popa, X86_ONLY)
OPCODE(salc, salc, X86_ONLY)
OPCODE(int1, int1, 0)

OPCODE(LABEL, label, 0)
OPCODE(out, out_1, 0)
OPCODE(out, out_4, 0)
OPCODE(in, in_1, 0)
OPCODE(in, in_4, 0)
OPCODE(ins, ins_1, 0)
OPCODE(ins, ins_4, 0)
OPCODE(stos, stos_1, 0)
OPCODE(stos, stos_4, 0)
OPCODE(lods, lods_1, 0)
OPCODE(lods, lods_4, 0)
OPCODE(movs, movs_1, 0)
OPCODE(movs, movs_4, 0)
OPCODE(rep_ins, rep_ins_1, 0)
OPCODE(rep_ins, rep_ins_4, 0)
OPCODE(rep_stos, rep_stos_1, 0)
OPCODE(rep_stos, rep_stos_4, 0)
OPCODE(rep_lods, rep_lods_1, 0)
OPCODE(rep_lods, rep_lods_4, 0)
OPCODE(rep_movs, rep_movs_1, 0)
OPCODE(rep_movs, rep_movs_4, 0)
OPCODE(outs, outs_1, 0)
OPCODE(outs, outs_4, 0)
OPCODE(cmps, cmps_1, 0)
OPCODE(cmps, cmps_4, 0)
OPCODE(scas, scas_1, 0)
OPCODE(scas, scas_4, 0)
OPCODE(rep_outs, rep_outs_1, 0)
OPCODE(rep_outs, rep_outs_4, 0)
OPCODE(rep_cmps, rep_cmps_1, 0)
OPCODE(rep_cmps, rep_cmps_4, 0)
OPCODE(repne_cmps, repne_cmps_1, 0)
OPCODE(repne_cmps, repne_cmps_4, 0)
OPCODE(rep_scas, rep_scas_1, 0)
OPCODE(rep_scas, rep_scas_4, 0)
OPCODE(repne_scas, repne_scas_1, 0)
OPCODE(repne_scas, repne_scas_4, 0)
