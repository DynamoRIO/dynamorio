/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
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

/* AVX 256-bit */
OPCODE(vunpcklps_256, vunpcklps, vunpcklps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpcklpd_256, vunpcklpd, vunpcklpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpckhps_256, vunpckhps, vunpckhps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vunpckhpd_256, vunpckhpd, vunpckhpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandps_256, vandps, vandps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandpd_256, vandpd, vandpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandnps_256, vandnps, vandnps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vandnpd_256, vandnpd, vandnpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vorps_256, vorps, vorps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vorpd_256, vorpd, vorpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vxorps_256, vxorps, vxorps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vxorpd_256, vxorpd, vxorpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddps_256, vaddps, vaddps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddpd_256, vaddpd, vaddpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmulps_256, vmulps, vmulps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmulpd_256, vmulpd, vmulpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vsubps_256, vsubps, vsubps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vsubpd_256, vsubpd, vsubpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vminps_256, vminps, vminps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vminpd_256, vminpd, vminpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vdivps_256, vdivps, vdivps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vdivpd_256, vdivpd, vdivpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmaxps_256, vmaxps, vmaxps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vmaxpd_256, vmaxpd, vmaxpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhaddpd_256, vhaddpd, vhaddpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhaddps_256, vhaddps, vhaddps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhsubpd_256, vhsubpd, vhsubpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vhsubps_256, vhsubps, vhsubps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddsubpd_256, vaddsubpd, vaddsubpd, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vaddsubps_256, vaddsubps, vaddsubps, 0, REGARG(YMM0), REGARG(YMM1), MEMARG(OPSZ_32))
OPCODE(vroundps_256, vroundps, vroundps, 0, REGARG(YMM0), REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vroundpd_256, vroundpd, vroundpd, 0, REGARG(YMM0), REGARG(YMM1), IMMARG(OPSZ_1))
OPCODE(vpcmpestrm, vpcmpestrm, vpcmpestrm, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcmpestri, vpcmpestri, vpcmpestri, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcmpistrm, vpcmpistrm, vpcmpistrm, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vpcmpistri, vpcmpistri, vpcmpistri, 0, REGARG(XMM0), MEMARG(OPSZ_16),
       IMMARG(OPSZ_1))
OPCODE(vcvtps2ph_256, vcvtps2ph, vcvtps2ph, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       IMMARG(OPSZ_1))
OPCODE(vmaskmovps_ld_256, vmaskmovps, vmaskmovps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmaskmovps_st_256, vmaskmovps, vmaskmovps, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       REGARG(YMM1))
OPCODE(vmaskmovpd_ld_256, vmaskmovpd, vmaskmovpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vmaskmovpd_st_256, vmaskmovpd, vmaskmovpd, 0, MEMARG(OPSZ_32), REGARG(YMM0),
       REGARG(YMM1))
OPCODE(vpermilp_256s, vpermilps, vpermilps, 0, REGARG(YMM0), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vpermilpd_256, vpermilpd, vpermilpd, 0, REGARG(YMM0), MEMARG(OPSZ_32),
       IMMARG(OPSZ_1))
OPCODE(vpermilps_noimm_256, vpermilps, vpermilps, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vpermilpd_noimm_256, vpermilpd, vpermilpd, 0, REGARG(YMM0), REGARG(YMM1),
       MEMARG(OPSZ_32))
OPCODE(vextractf128_256, vextractf128, vextractf128, 0, MEMARG(OPSZ_16), REGARG(YMM0),
       IMMARG(OPSZ_1))

/****************************************************************************/
/* FMA */
OPCODE(vfmadd132ps, vfmadd132ps, vfmadd132ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmadd132pd, vfmadd132pd, vfmadd132pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmadd213ps, vfmadd213ps, vfmadd213ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmadd213pd, vfmadd213pd, vfmadd213pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmadd231ps, vfmadd231ps, vfmadd231ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmadd231pd, vfmadd231pd, vfmadd231pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmadd132ss, vfmadd132ss, vfmadd132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmadd132sd, vfmadd132sd, vfmadd132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmadd213ss, vfmadd213ss, vfmadd213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmadd213sd, vfmadd213sd, vfmadd213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmadd231ss, vfmadd231ss, vfmadd231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmadd231sd, vfmadd231sd, vfmadd231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmaddsub132ps, vfmaddsub132ps, vfmaddsub132ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub132pd, vfmaddsub132pd, vfmaddsub132pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub213ps, vfmaddsub213ps, vfmaddsub213ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub213pd, vfmaddsub213pd, vfmaddsub213pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub231ps, vfmaddsub231ps, vfmaddsub231ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmaddsub231pd, vfmaddsub231pd, vfmaddsub231pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd132ps, vfmsubadd132ps, vfmsubadd132ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd132pd, vfmsubadd132pd, vfmsubadd132pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd213ps, vfmsubadd213ps, vfmsubadd213ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd213pd, vfmsubadd213pd, vfmsubadd213pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd231ps, vfmsubadd231ps, vfmsubadd231ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsubadd231pd, vfmsubadd231pd, vfmsubadd231pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub132ps, vfmsub132ps, vfmsub132ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub132pd, vfmsub132pd, vfmsub132pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub213ps, vfmsub213ps, vfmsub213ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub213pd, vfmsub213pd, vfmsub213pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub231ps, vfmsub231ps, vfmsub231ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub231pd, vfmsub231pd, vfmsub231pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfmsub132ss, vfmsub132ss, vfmsub132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmsub132sd, vfmsub132sd, vfmsub132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmsub213ss, vfmsub213ss, vfmsub213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmsub213sd, vfmsub213sd, vfmsub213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfmsub231ss, vfmsub231ss, vfmsub231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfmsub231sd, vfmsub231sd, vfmsub231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmadd132ps, vfnmadd132ps, vfnmadd132ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmadd132pd, vfnmadd132pd, vfnmadd132pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmadd213ps, vfnmadd213ps, vfnmadd213ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmadd213pd, vfnmadd213pd, vfnmadd213pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmadd231ps, vfnmadd231ps, vfnmadd231ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmadd231pd, vfnmadd231pd, vfnmadd231pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmadd132ss, vfnmadd132ss, vfnmadd132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmadd132sd, vfnmadd132sd, vfnmadd132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmadd213ss, vfnmadd213ss, vfnmadd213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmadd213sd, vfnmadd213sd, vfnmadd213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmadd231ss, vfnmadd231ss, vfnmadd231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmadd231sd, vfnmadd231sd, vfnmadd231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmsub132ps, vfnmsub132ps, vfnmsub132ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmsub132pd, vfnmsub132pd, vfnmsub132pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmsub213ps, vfnmsub213ps, vfnmsub213ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmsub213pd, vfnmsub213pd, vfnmsub213pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmsub231ps, vfnmsub231ps, vfnmsub231ps, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmsub231pd, vfnmsub231pd, vfnmsub231pd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_16))
OPCODE(vfnmsub132ss, vfnmsub132ss, vfnmsub132ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmsub132sd, vfnmsub132sd, vfnmsub132sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmsub213ss, vfnmsub213ss, vfnmsub213ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmsub213sd, vfnmsub213sd, vfnmsub213sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))
OPCODE(vfnmsub231ss, vfnmsub231ss, vfnmsub231ss, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_4))
OPCODE(vfnmsub231sd, vfnmsub231sd, vfnmsub231sd, 0, REGARG(XMM0), REGARG(XMM1), 
       MEMARG(OPSZ_8))

/* FMA 256-bit */
OPCODE(vfmadd132ps_256, vfmadd132ps, vfmadd132ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmadd132pd_256, vfmadd132pd, vfmadd132pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmadd213ps_256, vfmadd213ps, vfmadd213ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmadd213pd_256, vfmadd213pd, vfmadd213pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmadd231ps_256, vfmadd231ps, vfmadd231ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmadd231pd_256, vfmadd231pd, vfmadd231pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub132ps_256, vfmaddsub132ps, vfmaddsub132ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub132pd_256, vfmaddsub132pd, vfmaddsub132pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub213ps_256, vfmaddsub213ps, vfmaddsub213ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub213pd_256, vfmaddsub213pd, vfmaddsub213pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub231ps_256, vfmaddsub231ps, vfmaddsub231ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmaddsub231pd_256, vfmaddsub231pd, vfmaddsub231pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd132ps_256, vfmsubadd132ps, vfmsubadd132ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd132pd_256, vfmsubadd132pd, vfmsubadd132pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd213ps_256, vfmsubadd213ps, vfmsubadd213ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd213pd_256, vfmsubadd213pd, vfmsubadd213pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd231ps_256, vfmsubadd231ps, vfmsubadd231ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsubadd231pd_256, vfmsubadd231pd, vfmsubadd231pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsub132ps_256, vfmsub132ps, vfmsub132ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsub132pd_256, vfmsub132pd, vfmsub132pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsub213ps_256, vfmsub213ps, vfmsub213ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsub213pd_256, vfmsub213pd, vfmsub213pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsub231ps_256, vfmsub231ps, vfmsub231ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfmsub231pd_256, vfmsub231pd, vfmsub231pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmadd132ps_256, vfnmadd132ps, vfnmadd132ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmadd132pd_256, vfnmadd132pd, vfnmadd132pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmadd213ps_256, vfnmadd213ps, vfnmadd213ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmadd213pd_256, vfnmadd213pd, vfnmadd213pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmadd231ps_256, vfnmadd231ps, vfnmadd231ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmadd231pd_256, vfnmadd231pd, vfnmadd231pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmsub132ps_256, vfnmsub132ps, vfnmsub132ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmsub132pd_256, vfnmsub132pd, vfnmsub132pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmsub213ps_256, vfnmsub213ps, vfnmsub213ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmsub213pd_256, vfnmsub213pd, vfnmsub213pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmsub231ps_256, vfnmsub231ps, vfnmsub231ps, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
OPCODE(vfnmsub231pd_256, vfnmsub231pd, vfnmsub231pd, 0, REGARG(YMM0), REGARG(YMM1), 
       MEMARG(OPSZ_32))
