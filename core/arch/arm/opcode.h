/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

/* file "opcode.h" -- opcode definitions and utilities */

#ifndef _OPCODE_H_
#define _OPCODE_H_ 1

/* DR_API EXPORT TOFILE dr_ir_opcodes_arm.h */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * OPCODES
 */
/**
 * @file dr_ir_opcodes_arm.h
 * @brief Instruction opcode constants for ARM.
 */
#ifdef AVOID_API_EXPORT
/*
 * This enum corresponds with the arrays in table_*.c.
 * They must be kept consistent, using tools/x86opnums.pl.
 * When adding new instructions, be sure to update all of these places:
 *   1) table_* op_instr arrays
 *   2) table_* decoding table entries
 *   3) OP_ enum (here) via x86opnums.pl
 *   4) update OP_LAST at end of enum here
 *   5) instr_create macros
 *   6) suite/tests/api/ir* tests
 */
/* XXX: add "x86" vs "ARM" to doxygen comments? */
#endif
/** Opcode constants for use in the instr_t data structure. */
enum {
/*   0 */     OP_INVALID,  /* NULL, */ /**< INVALID opcode */
/*   1 */     OP_UNDECODED,  /* NULL, */ /**< UNDECODED opcode */
/*   2 */     OP_CONTD,    /* NULL, */ /**< CONTD opcode */
/*   3 */     OP_LABEL,    /* NULL, */ /**< LABEL opcode */

/* FIXME i#1551: add doxygen comments to these */
    OP_adc,
    OP_adcs,
    OP_add,
    OP_adds,
    OP_and,
    OP_ands,
    OP_asr,
    OP_asrs,
    OP_bfc,
    OP_bfi,
    OP_bic,
    OP_bics,
    OP_bkpt,
    OP_bl,
    OP_blx,
    OP_blx_ind,
    OP_bx,
    OP_bxj,
    OP_cdp,
    OP_clz,
    OP_cmn,
    OP_cmp,
    OP_crc32,
    OP_crc32c,
    OP_dbg,
    OP_eor,
    OP_eors,
    OP_eret,
    OP_hlt,
    OP_hvc,
    OP_lda,
    OP_ldab,
    OP_ldaex,
    OP_ldaexb,
    OP_ldaexd,
    OP_ldaexh,
    OP_ldah,
    OP_ldc,
    OP_ldcl,
    OP_ldm,
    OP_ldm_priv,
    OP_ldmda,
    OP_ldmda_priv,
    OP_ldmdb,
    OP_ldmdb_priv,
    OP_ldmia_priv,
    OP_ldmib,
    OP_ldr,
    OP_ldrb,
    OP_ldrbt,
    OP_ldrd,
    OP_ldrex,
    OP_ldrexd,
    OP_ldrexh,
    OP_ldrh,
    OP_ldrht,
    OP_ldrsb,
    OP_ldrsbt,
    OP_ldrsh,
    OP_ldrsht,
    OP_ldrt,
    OP_lsl,
    OP_lsls,
    OP_lsr,
    OP_lsrs,
    OP_mcr,
    OP_mcrr,
    OP_mla,
    OP_mlas,
    OP_mls,
    OP_mov,
    OP_movs,
    OP_movt,
    OP_movw,
    OP_mrc,
    OP_mrrc,
    OP_mrs,
    OP_msr,
    OP_mul,
    OP_muls,
    OP_mvn,
    OP_mvns,
    OP_nop,
    OP_orr,
    OP_orrs,
    OP_pkhbt,
    OP_qadd,
    OP_qadd16,
    OP_qadd8,
    OP_qasx,
    OP_qdadd,
    OP_qdsub,
    OP_qsax,
    OP_qsub,
    OP_qsub16,
    OP_qsub8,
    OP_rbit,
    OP_rev,
    OP_rev16,
    OP_revsh,
    OP_ror,
    OP_rors,
    OP_rrx,
    OP_rrxs,
    OP_rsb,
    OP_rsbs,
    OP_rsc,
    OP_rscs,
    OP_sadd16,
    OP_sadd8,
    OP_sasx,
    OP_sbc,
    OP_sbcs,
    OP_sbfx,
    OP_sdiv,
    OP_sel,
    OP_sev,
    OP_sevl,
    OP_shadd16,
    OP_shadd8,
    OP_shasx,
    OP_shsax,
    OP_shsub16,
    OP_shsub8,
    OP_smlabb,
    OP_smlabt,
    OP_smlad,
    OP_smladx,
    OP_smlal,
    OP_smlalbb,
    OP_smlalbt,
    OP_smlald,
    OP_smlaldx,
    OP_smlals,
    OP_smlaltb,
    OP_smlaltt,
    OP_smlatb,
    OP_smlatt,
    OP_smlawb,
    OP_smlawt,
    OP_smlsd,
    OP_smlsdx,
    OP_smlsld,
    OP_smlsldx,
    OP_smmla,
    OP_smmls,
    OP_smmlsr,
    OP_smulbb,
    OP_smulbt,
    OP_smull,
    OP_smulls,
    OP_smultb,
    OP_smultt,
    OP_smulwb,
    OP_smulwt,
    OP_ssat,
    OP_ssat16,
    OP_ssax,
    OP_ssub16,
    OP_ssub8,
    OP_stc,
    OP_stcl,
    OP_stl,
    OP_stlb,
    OP_stlex,
    OP_stlexb,
    OP_stlexd,
    OP_stlexh,
    OP_stlh,
    OP_stm,
    OP_stm_priv,
    OP_stmda,
    OP_stmda_priv,
    OP_stmdb,
    OP_stmdb_priv,
    OP_stmib,
    OP_str,
    OP_strb,
    OP_strbt,
    OP_strd,
    OP_strex,
    OP_strexb,
    OP_strexd,
    OP_strexh,
    OP_strh,
    OP_strht,
    OP_strt,
    OP_sub,
    OP_subs,
    OP_svc,
    OP_sxtab,
    OP_sxtab16,
    OP_sxtah,
    OP_teq,
    OP_tst,
    OP_uadd16,
    OP_uadd8,
    OP_uasx,
    OP_ubfx,
    OP_udiv,
    OP_uhadd16,
    OP_uhadd8,
    OP_uhasx,
    OP_uhsax,
    OP_uhsub16,
    OP_uhsub8,
    OP_umaal,
    OP_umlal,
    OP_umlals,
    OP_umull,
    OP_umulls,
    OP_uqadd16,
    OP_uqadd8,
    OP_uqasx,
    OP_uqsax,
    OP_uqsub16,
    OP_uqsub8,
    OP_usada8,
    OP_usat,
    OP_usat16,
    OP_usax,
    OP_usub16,
    OP_usub8,
    OP_uxtab,
    OP_uxtab16,
    OP_uxtah,
    OP_vabs_F32,
    OP_vabs_F64,
    OP_vadd_F32,
    OP_vadd_F64,
    OP_vcmpe_F32,
    OP_vcmpe_F64,
    OP_vcmp_F32,
    OP_vcmp_F64,
    OP_vcvt_F32_F64,
    OP_vcvt_F32_S16,
    OP_vcvt_F32_S32,
    OP_vcvt_F32_U16,
    OP_vcvt_F32_U32,
    OP_vcvt_F64_F32,
    OP_vcvt_F64_S16,
    OP_vcvt_F64_S32,
    OP_vcvt_F64_U16,
    OP_vcvt_F64_U32,
    OP_vcvtr_S32_F32,
    OP_vcvtr_S32_F64,
    OP_vcvtr_U32_F32,
    OP_vcvtr_U32_F64,
    OP_vcvt_S16_F32,
    OP_vcvt_S16_F64,
    OP_vcvt_S32_F32,
    OP_vcvt_S32_F64,
    OP_vcvt_U16_F32,
    OP_vcvt_U16_F64,
    OP_vcvt_U32_F32,
    OP_vcvt_U32_F64,
    OP_vdiv_F32,
    OP_vdiv_F64,
    OP_vdup_16,
    OP_vdup_32,
    OP_vdup_8,
    OP_vfma_F32,
    OP_vfma_F64,
    OP_vfms_F32,
    OP_vfms_F64,
    OP_vfnma_F32,
    OP_vfnma_F64,
    OP_vfnms_F32,
    OP_vfnms_F64,
    OP_vldmdb,
    OP_vldmia,
    OP_vldr,
    OP_vmla_F32,
    OP_vmla_F64,
    OP_vmls_F32,
    OP_vmls_F64,
    OP_vmov,
    OP_vmov_16,
    OP_vmov_32,
    OP_vmov_8,
    OP_vmov_F32,
    OP_vmov_F64,
    OP_vmov_S16,
    OP_vmov_S8,
    OP_vmov_U16,
    OP_vmov_U8,
    OP_vmrs,
    OP_vmrs_apsr,
    OP_vmsr,
    OP_vmul_F32,
    OP_vmul_F64,
    OP_vneg_F32,
    OP_vneg_F64,
    OP_vnmla_F32,
    OP_vnmla_F64,
    OP_vnmls_F32,
    OP_vnmls_F64,
    OP_vnmul_F32,
    OP_vnmul_F64,
    OP_vrintr_F32,
    OP_vrintr_F64,
    OP_vrintx_F32,
    OP_vrintx_F64,
    OP_vrintz_F32,
    OP_vrintz_F64,
    OP_vsqrt_F32,
    OP_vsqrt_F64,
    OP_vstmdb,
    OP_vstmia,
    OP_vstr,
    OP_vsub_F32,
    OP_vsub_F64,
    OP_wfe,
    OP_wfi,
    OP_yield,

    OP_AFTER_LAST,
    OP_FIRST = OP_adc,            /**< First real opcode. */
    OP_LAST  = OP_AFTER_LAST - 1, /**< Last real opcode. */
};

/* alternative names */
#define OP_pop       OP_ldr    /**< Alternative opcode name for pop. */
#define OP_push      OP_stmdb  /**< Alternative opcode name for push. */
#define OP_vpop      OP_vldmia /**< Alternative opcode name for vpop. */
#define OP_vpush     OP_vstmdb /**< Alternative opcode name for vpush. */

/****************************************************************************/
/* DR_API EXPORT END */

#endif /* _OPCODE_H_ */
