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
/* no arguments */

OPCODE(fwait, fwait, fwait, 0)
OPCODE(hlt, hlt, hlt, 0)
OPCODE(cmc, cmc, cmc, 0)
OPCODE(clc, clc, clc, 0)
OPCODE(stc, stc, stc, 0)
OPCODE(cli, cli, cli, 0)
OPCODE(sti, sti, sti, 0)
OPCODE(cld, cld, cld, 0)
OPCODE(std, std, std, 0)
OPCODE(clts, clts, clts, 0)
OPCODE(invd, invd, invd, 0)
OPCODE(wbinvd, wbinvd, wbinvd, 0)
OPCODE(ud2, ud2, ud2, 0)
OPCODE(emms, emms, emms, 0)
OPCODE(rsm, rsm, rsm, 0)
OPCODE(lfence, lfence, lfence, 0)
OPCODE(mfence, mfence, mfence, 0)
OPCODE(sfence, sfence, sfence, 0)
OPCODE(nop, nop, nop, 0)
XOPCODE(x_nop, nop, nop, 0)
OPCODE(pause, pause, pause, 0)
OPCODE(fnop, fnop, fnop, 0)
OPCODE(fdecstp, fdecstp, fdecstp, 0)
OPCODE(fincstp, fincstp, fincstp, 0)
OPCODE(fnclex, fnclex, fnclex, 0)
OPCODE(fninit, fninit, fninit, 0)
OPCODE(sysret, sysret, sysret, 0)
OPCODE(femms, femms, femms, 0)
OPCODE(swapgs, swapgs, swapgs, X64_ONLY)
OPCODE(vmcall, vmcall, vmcall, X64_ONLY)
OPCODE(vmlaunch, vmlaunch, vmlaunch, X64_ONLY)
OPCODE(vmresume, vmresume, vmresume, X64_ONLY)
OPCODE(vmxoff, vmxoff, vmxoff, X64_ONLY)
OPCODE(vmfunc, vmfunc, vmfunc, X64_ONLY)
OPCODE(fxam, fxam, fxam, 0)
OPCODE(sahf, sahf, sahf, 0)
OPCODE(mwait, mwait, mwait, 0)
OPCODE(monitor, monitor, monitor, 0)
OPCODE(mwaitx, mwaitx, mwaitx, 0)
OPCODE(monitorx, monitorx, monitorx, 0)
OPCODE(fucompp, fucompp, fucompp, 0)
OPCODE(fcompp, fcompp, fcompp, 0)
OPCODE(lahf, lahf, lahf, 0)
OPCODE(sysenter, sysenter, sysenter, 0)
OPCODE(sysexit, sysexit, sysexit, 0)
OPCODE(syscall, syscall, syscall, 0)
OPCODE(daa, daa, daa, X86_ONLY)
OPCODE(das, das, das, X86_ONLY)
OPCODE(aaa, aaa, aaa, X86_ONLY)
OPCODE(aas, aas, aas, X86_ONLY)
OPCODE(cwde, cwde, cwde, 0)
OPCODE(xlat, xlat, xlat, 0)
OPCODE(fchs, fchs, fchs, 0)
OPCODE(fabs, fabs, fabs, 0)
OPCODE(ftst, ftst, ftst, 0)
OPCODE(fld1, fld1, fld1, 0)
OPCODE(fldl2t, fldl2t, fldl2t, 0)
OPCODE(fldl2e, fldl2e, fldl2e, 0)
OPCODE(fldpi, fldpi, fldpi, 0)
OPCODE(fldlg2, fldlg2, fldlg2, 0)
OPCODE(fldln2, fldln2, fldln2, 0)
OPCODE(fldz, fldz, fldz, 0)
OPCODE(f2xm1, f2xm1, f2xm1, 0)
OPCODE(fptan, fptan, fptan, 0)
OPCODE(fxtract, fxtract, fxtract, 0)
OPCODE(fsqrt, fsqrt, fsqrt, 0)
OPCODE(fsincos, fsincos, fsincos, 0)
OPCODE(frndint, frndint, frndint, 0)
OPCODE(fsin, fsin, fsin, 0)
OPCODE(fcos, fcos, fcos, 0)
OPCODE(fscale, fscale, fscale, 0)
OPCODE(fyl2x, fyl2x, fyl2x, 0)
OPCODE(fyl2xp1, fyl2xp1, fyl2xp1, 0)
OPCODE(fpatan, fpatan, fpatan, 0)
OPCODE(fprem, fprem, fprem, 0)
OPCODE(fprem1, fprem1, fprem1, 0)
OPCODE(popf, popf, popf, 0)
OPCODE(ret, ret, ret, 0)
XOPCODE(return, ret, return, 0)
OPCODE(ret_far, ret_far, ret_far, 0)
OPCODE(iret, iret, iret, 0)
OPCODE(rdtsc, rdtsc, rdtsc, 0)
OPCODE(cdq, cdq, cdq, 0)
OPCODE(pushf, pushf, pushf, 0)
OPCODE(int3, int3, int3, 0)
XOPCODE(debug_instr, int3, debug_instr, 0)
OPCODE(into, into, into, X86_ONLY)
OPCODE(rdmsr, rdmsr, rdmsr, 0)
OPCODE(rdpmc, rdpmc, rdpmc, 0)
OPCODE(leave, leave, leave, 0)
OPCODE(pusha, pusha, pusha, X86_ONLY)
OPCODE(wrmsr, wrmsr, wrmsr, 0)
OPCODE(cpuid, cpuid, cpuid, 0)
OPCODE(popa, popa, popa, X86_ONLY)
OPCODE(salc, salc, salc, X86_ONLY)
OPCODE(int1, int1, int1, 0)
OPCODE(vmmcall, vmmcall, vmmcall, 0)
OPCODE(stgi, stgi, stgi, 0)
OPCODE(clgi, clgi, clgi, 0)
OPCODE(xgetbv, xgetbv, xgetbv, 0)
OPCODE(xsetbv, xsetbv, xsetbv, 0)

OPCODE(vmrun, vmrun, vmrun, 0)
OPCODE(vmload, vmload, vmload, 0)
OPCODE(vmsave, vmsave, vmsave, 0)
OPCODE(skinit, skinit, skinit, 0)
OPCODE(invlpga, invlpga, invlpga, 0)
OPCODE(rdtscp, rdtscp, rdtscp, 0)

OPCODE(encls, encls, encls, 0)
OPCODE(enclu, enclu, enclu, 0)
OPCODE(enclv, enclv, enclv, 0)

OPCODE(LABEL, LABEL, label, 0)
OPCODE(out, out, out_1, 0)
OPCODE(out4, out, out_4, 0)
OPCODE(in, in, in_1, 0)
OPCODE(in4, in, in_4, 0)
OPCODE(ins, ins, ins_1, 0)
OPCODE(ins4, ins, ins_4, 0)
OPCODE(stos, stos, stos_1, 0)
OPCODE(stos4, stos, stos_4, 0)
OPCODE(stos8, stos, stos_8, X64_ONLY)
OPCODE(lods, lods, lods_1, 0)
OPCODE(lods4, lods, lods_4, 0)
OPCODE(lods8, lods, lods_8, X64_ONLY)
OPCODE(movs, movs, movs_1, 0)
OPCODE(movs4, movs, movs_4, 0)
OPCODE(rep_ins, rep_ins, rep_ins_1, 0)
OPCODE(rep_ins4, rep_ins, rep_ins_4, 0)
OPCODE(rep_stos, rep_stos, rep_stos_1, 0)
OPCODE(rep_stos4, rep_stos, rep_stos_4, 0)
OPCODE(rep_lods, rep_lods, rep_lods_1, 0)
OPCODE(rep_lods4, rep_lods, rep_lods_4, 0)
OPCODE(rep_movs, rep_movs, rep_movs_1, 0)
OPCODE(rep_movs4, rep_movs, rep_movs_4, 0)
OPCODE(outs, outs, outs_1, 0)
OPCODE(outs4, outs, outs_4, 0)
OPCODE(cmps, cmps, cmps_1, 0)
OPCODE(cmps4, cmps, cmps_4, 0)
OPCODE(scas, scas, scas_1, 0)
OPCODE(scas4, scas, scas_4, 0)
OPCODE(rep_outs, rep_outs, rep_outs_1, 0)
OPCODE(rep_outs4, rep_outs, rep_outs_4, 0)
OPCODE(rep_cmps, rep_cmps, rep_cmps_1, 0)
OPCODE(rep_cmps4, rep_cmps, rep_cmps_4, 0)
OPCODE(repne_cmps, repne_cmps, repne_cmps_1, 0)
OPCODE(repne_cmps4, repne_cmps, repne_cmps_4, 0)
OPCODE(rep_scas, rep_scas, rep_scas_1, 0)
OPCODE(rep_scas4, rep_scas, rep_scas_4, 0)
OPCODE(repne_scas, repne_scas, repne_scas_1, 0)
OPCODE(repne_scas4, repne_scas, repne_scas_4, 0)

OPCODE(vzeroupper, vzeroupper, vzeroupper, 0)
OPCODE(vzeroall, vzeroall, vzeroall, 0)

OPCODE(getsec, getsec, getsec, 0)

OPCODE(xend, xend, xend, 0)
OPCODE(xtest, xtest, xtest, 0)

OPCODE(rdpkru, rdpkru, rdpkru, 0)
OPCODE(wrpkru, wrpkru, wrpkru, 0)
