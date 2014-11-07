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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

#include "../globals.h" /* need this to include decode.h (uint, etc.) */
#include "arch.h"    /* need this to include decode.h (byte, etc. */
#include "decode.h"
#include "decode_private.h"
#include "table_private.h"

/* XXX i#1551 unresolved issues:
 * + Do we want to try and model all of the unpredictable conditions in
 *   each instruction (typically when pc or lr is used but it varies
 *   quite a bit)?  For core DR we don't care as much b/c w/ the fixed-width
 *   we can keep decoding and wait for a fault.
 */

/* Addressing mode quick reference:
 *   x x x P U x W x
 *         0 0   0     str  Rt, [Rn], -Rm            Post-indexed addressing
 *         0 1   0     str  Rt, [Rn], Rm             Post-indexed addressing
 *         0 0   1     illegal, or separate opcode
 *         0 1   1     illegal, or separate opcode
 *         1 0   0     str  Rt, [Rn - Rm]            Offset addressing
 *         1 1   0     str  Rt, [Rn + Rm]            Offset addressing
 *         1 0   1     str  Rt, [Rn - Rm]!           Pre-indexed addressing
 *         1 1   1     str  Rt, [Rn + Rm]!           Pre-indexed addressing
 */

const instr_info_t * const op_instr_A32[] = {
    /* OP_INVALID */   NULL,
    /* OP_UNDECODED */ NULL,
    /* OP_CONTD   */   NULL,
    /* OP_LABEL   */   NULL,

    /* OP_adc            */ &A32_pred_opc8[0x2a],
    /* OP_adcs           */ &A32_pred_opc8[0x2b],
    /* OP_add            */ &A32_pred_opc8[0x28],
    /* OP_adds           */ &A32_pred_opc8[0x29],
    /* OP_and            */ &A32_pred_opc8[0x20],
    /* OP_ands           */ &A32_pred_opc8[0x21],
    /* OP_asr            */ &A32_ext_opc4[4][0x04],
    /* OP_asrs           */ &A32_ext_opc4[5][0x04],
    /* OP_bfc            */ &A32_ext_bit4[8][0x01],
    /* OP_bfi            */ &A32_ext_RDPC[0][0x00],
    /* OP_bic            */ &A32_pred_opc8[0x3c],
    /* OP_bics           */ &A32_pred_opc8[0x3d],
    /* OP_bkpt           */ &A32_ext_opc4[1][0x07],
    /* OP_b              */ &A32_pred_opc8[0xa0],
    /* OP_bl             */ &A32_pred_opc8[0xb0],
    /* OP_blx            */ NULL /* FIXME: add non-pred instrs */,
    /* OP_blx_ind        */ &A32_ext_opc4[1][0x03],
    /* OP_bx             */ &A32_ext_opc4[1][0x01],
    /* OP_bxj            */ &A32_ext_opc4[1][0x02],
    /* OP_cdp            */ &A32_ext_bit4[9][0x00],
    /* OP_clz            */ &A32_ext_opc4[3][0x01],
    /* OP_cmn            */ &A32_pred_opc8[0x37],
    /* OP_cmp            */ &A32_pred_opc8[0x35],
    /* OP_crc32          */ &A32_ext_bit9[1][0x00],
    /* OP_crc32c         */ &A32_ext_bit9[1][0x01],
    /* OP_dbg            */ &A32_ext_opc4[6][0x0f],
    /* OP_eor            */ &A32_pred_opc8[0x22],
    /* OP_eors           */ &A32_pred_opc8[0x23],
    /* OP_eret           */ &A32_ext_opc4[3][0x06],
    /* OP_hlt            */ &A32_ext_opc4[0][0x07],
    /* OP_hvc            */ &A32_ext_opc4[2][0x07],
    /* OP_lda            */ &A32_ext_bits8[1][0x00],
    /* OP_ldab           */ &A32_ext_bits8[5][0x00],
    /* OP_ldaex          */ &A32_ext_bits8[1][0x02],
    /* OP_ldaexb         */ &A32_ext_bits8[5][0x02],
    /* OP_ldaexd         */ &A32_ext_bits8[3][0x02],
    /* OP_ldaexh         */ &A32_ext_bits8[7][0x02],
    /* OP_ldah           */ &A32_ext_bits8[7][0x00],
    /* OP_ldc            */ &A32_ext_fp[23][0x02],
    /* OP_ldcl           */ &A32_ext_fp[27][0x02],
    /* OP_ldm            */ &A32_pred_opc8[0x9d],
    /* OP_ldm_priv       */ &A32_pred_opc8[0x8d],
    /* OP_ldmda          */ &A32_pred_opc8[0x81],
    /* OP_ldmda_priv     */ &A32_pred_opc8[0x85],
    /* OP_ldmdb          */ &A32_pred_opc8[0x91],
    /* OP_ldmdb_priv     */ &A32_pred_opc8[0x95],
    /* OP_ldmia_priv     */ &A32_pred_opc8[0x8f],
    /* OP_ldmib          */ &A32_pred_opc8[0x99],
    /* OP_ldr            */ &A32_pred_opc8[0x59],
    /* OP_ldrb           */ &A32_pred_opc8[0x5d],
    /* OP_ldrbt          */ &A32_pred_opc8[0x4f],
    /* OP_ldrd           */ &A32_ext_opc4x[22][0x04],
    /* OP_ldrex          */ &A32_ext_bits8[1][0x03],
    /* OP_ldrexd         */ &A32_ext_bits8[3][0x03],
    /* OP_ldrexh         */ &A32_ext_bits8[7][0x03],
    /* OP_ldrh           */ &A32_ext_opc4x[23][0x03],
    /* OP_ldrht          */ &A32_ext_opc4x[15][0x03],
    /* OP_ldrsb          */ &A32_ext_opc4x[23][0x04],
    /* OP_ldrsbt         */ &A32_ext_opc4x[15][0x04],
    /* OP_ldrsh          */ &A32_ext_opc4x[23][0x05],
    /* OP_ldrsht         */ &A32_ext_opc4x[15][0x05],
    /* OP_ldrt           */ &A32_pred_opc8[0x4b],
    /* OP_lsl            */ &A32_ext_opc4[4][0x08],
    /* OP_lsls           */ &A32_ext_opc4[5][0x08],
    /* OP_lsr            */ &A32_ext_opc4[4][0x02],
    /* OP_lsrs           */ &A32_ext_opc4[5][0x02],
    /* OP_mcr            */ &A32_ext_bit4[9][0x01],
    /* OP_mcrr           */ &A32_ext_fp[2][0x02],
    /* OP_mla            */ &A32_ext_opc4x[2][0x02],
    /* OP_mlas           */ &A32_ext_opc4x[3][0x02],
    /* OP_mls            */ &A32_ext_opc4x[6][0x02],
    /* OP_mov            */ &A32_pred_opc8[0x3a],
    /* OP_movs           */ &A32_pred_opc8[0x3b],
    /* OP_movt           */ &A32_pred_opc8[0x34],
    /* OP_movw           */ &A32_pred_opc8[0x30],
    /* OP_mrc            */ &A32_ext_bit4[10][0x01],
    /* OP_mrrc           */ &A32_ext_fp[3][0x02],
    /* OP_mrs            */ &A32_ext_bit9[0][0x01],
    /* OP_msr            */ &A32_pred_opc8[0x36],
    /* OP_mul            */ &A32_ext_opc4x[0][0x02],
    /* OP_muls           */ &A32_ext_opc4x[1][0x02],
    /* OP_mvn            */ &A32_pred_opc8[0x3e],
    /* OP_mvns           */ &A32_pred_opc8[0x3f],
    /* OP_nop            */ &A32_ext_bits0[0][0x00],
    /* OP_orr            */ &A32_pred_opc8[0x38],
    /* OP_orrs           */ &A32_pred_opc8[0x39],
    /* OP_pkhbt          */ &A32_ext_opc4y[6][0x01],
    /* OP_qadd           */ &A32_ext_opc4[0][0x05],
    /* OP_qadd16         */ &A32_ext_opc4y[1][0x01],
    /* OP_qadd8          */ &A32_ext_opc4y[1][0x05],
    /* OP_qasx           */ &A32_ext_opc4y[1][0x02],
    /* OP_qdadd          */ &A32_ext_opc4[2][0x05],
    /* OP_qdsub          */ &A32_ext_opc4[3][0x05],
    /* OP_qsax           */ &A32_ext_opc4y[1][0x03],
    /* OP_qsub           */ &A32_ext_opc4[1][0x05],
    /* OP_qsub16         */ &A32_ext_opc4y[1][0x04],
    /* OP_qsub8          */ &A32_ext_opc4y[1][0x08],
    /* OP_rbit           */ &A32_ext_opc4y[11][0x02],
    /* OP_rev            */ &A32_ext_opc4y[8][0x02],
    /* OP_rev16          */ &A32_ext_opc4y[8][0x06],
    /* OP_revsh          */ &A32_ext_opc4y[11][0x06],
    /* OP_ror            */ &A32_ext_opc4[4][0x0e],
    /* OP_rors           */ &A32_ext_opc4[5][0x0e],
    /* OP_rrx            */ &A32_ext_imm5[1][0x00],
    /* OP_rrxs           */ &A32_ext_imm5[3][0x00],
    /* OP_rsb            */ &A32_pred_opc8[0x26],
    /* OP_rsbs           */ &A32_pred_opc8[0x27],
    /* OP_rsc            */ &A32_pred_opc8[0x2e],
    /* OP_rscs           */ &A32_pred_opc8[0x2f],
    /* OP_sadd16         */ &A32_ext_opc4y[0][0x01],
    /* OP_sadd8          */ &A32_ext_opc4y[0][0x05],
    /* OP_sasx           */ &A32_ext_opc4y[0][0x02],
    /* OP_sbc            */ &A32_pred_opc8[0x2c],
    /* OP_sbcs           */ &A32_pred_opc8[0x2d],
    /* OP_sbfx           */ &A32_ext_bit4[3][0x01],
    /* OP_sdiv           */ &A32_ext_bit4[0][0x01],
    /* OP_sel            */ &A32_ext_opc4y[6][0x06],
    /* OP_sev            */ &A32_ext_bits0[0][0x04],
    /* OP_sevl           */ &A32_ext_bits0[0][0x05],
    /* OP_shadd16        */ &A32_ext_opc4y[2][0x01],
    /* OP_shadd8         */ &A32_ext_opc4y[2][0x05],
    /* OP_shasx          */ &A32_ext_opc4y[2][0x02],
    /* OP_shsax          */ &A32_ext_opc4y[2][0x03],
    /* OP_shsub16        */ &A32_ext_opc4y[2][0x04],
    /* OP_shsub8         */ &A32_ext_opc4y[2][0x08],
    /* OP_smlabb         */ &A32_ext_opc4[0][0x08],
    /* OP_smlabt         */ &A32_ext_opc4[0][0x0a],
    /* OP_smlad          */ &A32_ext_opc4y[12][0x01],
    /* OP_smladx         */ &A32_ext_opc4y[12][0x02],
    /* OP_smlal          */ &A32_ext_opc4x[14][0x02],
    /* OP_smlalbb        */ &A32_ext_opc4[2][0x08],
    /* OP_smlalbt        */ &A32_ext_opc4[2][0x0a],
    /* OP_smlald         */ &A32_ext_opc4y[13][0x01],
    /* OP_smlaldx        */ &A32_ext_opc4y[13][0x02],
    /* OP_smlals         */ &A32_ext_opc4x[15][0x02],
    /* OP_smlaltb        */ &A32_ext_opc4[2][0x0c],
    /* OP_smlaltt        */ &A32_ext_opc4[2][0x0e],
    /* OP_smlatb         */ &A32_ext_opc4[0][0x0c],
    /* OP_smlatt         */ &A32_ext_opc4[0][0x0e],
    /* OP_smlawb         */ &A32_ext_opc4[1][0x08],
    /* OP_smlawt         */ &A32_ext_opc4[1][0x0c],
    /* OP_smlsd          */ &A32_ext_opc4y[12][0x03],
    /* OP_smlsdx         */ &A32_ext_opc4y[12][0x04],
    /* OP_smlsld         */ &A32_ext_opc4y[13][0x03],
    /* OP_smlsldx        */ &A32_ext_opc4y[13][0x04],
    /* OP_smmla          */ &A32_ext_opc4y[14][0x01],
    /* OP_smmls          */ &A32_ext_opc4y[14][0x07],
    /* OP_smmlsr         */ &A32_ext_opc4y[14][0x08],
    /* OP_smulbb         */ &A32_ext_opc4[3][0x08],
    /* OP_smulbt         */ &A32_ext_opc4[3][0x0a],
    /* OP_smull          */ &A32_ext_opc4x[12][0x02],
    /* OP_smulls         */ &A32_ext_opc4x[13][0x02],
    /* OP_smultb         */ &A32_ext_opc4[3][0x0c],
    /* OP_smultt         */ &A32_ext_opc4[3][0x0e],
    /* OP_smulwb         */ &A32_ext_opc4[1][0x0a],
    /* OP_smulwt         */ &A32_ext_opc4[1][0x0e],
    /* OP_ssat           */ &A32_ext_opc4y[7][0x01],
    /* OP_ssat16         */ &A32_ext_opc4y[7][0x02],
    /* OP_ssax           */ &A32_ext_opc4y[0][0x03],
    /* OP_ssub16         */ &A32_ext_opc4y[0][0x04],
    /* OP_ssub8          */ &A32_ext_opc4y[0][0x08],
    /* OP_stc            */ &A32_ext_fp[22][0x02],
    /* OP_stcl           */ &A32_ext_fp[26][0x02],
    /* OP_stl            */ &A32_ext_bits8[0][0x00],
    /* OP_stlb           */ &A32_ext_bits8[4][0x00],
    /* OP_stlex          */ &A32_ext_bits8[0][0x02],
    /* OP_stlexb         */ &A32_ext_bits8[4][0x02],
    /* OP_stlexd         */ &A32_ext_bits8[2][0x02],
    /* OP_stlexh         */ &A32_ext_bits8[6][0x02],
    /* OP_stlh           */ &A32_ext_bits8[6][0x00],
    /* OP_stm            */ &A32_pred_opc8[0x9c],
    /* OP_stm_priv       */ &A32_pred_opc8[0x8c],
    /* OP_stmda          */ &A32_pred_opc8[0x80],
    /* OP_stmda_priv     */ &A32_pred_opc8[0x84],
    /* OP_stmdb          */ &A32_pred_opc8[0x90],
    /* OP_stmdb_priv     */ &A32_pred_opc8[0x94],
    /* OP_stmib          */ &A32_pred_opc8[0x98],
    /* OP_str            */ &A32_pred_opc8[0x58],
    /* OP_strb           */ &A32_pred_opc8[0x5c],
    /* OP_strbt          */ &A32_pred_opc8[0x4e],
    /* OP_strd           */ &A32_ext_opc4x[22][0x05],
    /* OP_strex          */ &A32_ext_bits8[0][0x03],
    /* OP_strexb         */ &A32_ext_bits8[4][0x03],
    /* OP_strexd         */ &A32_ext_bits8[2][0x03],
    /* OP_strexh         */ &A32_ext_bits8[6][0x03],
    /* OP_strh           */ &A32_ext_opc4x[22][0x03],
    /* OP_strht          */ &A32_ext_opc4x[14][0x03],
    /* OP_strt           */ &A32_pred_opc8[0x4a],
    /* OP_sub            */ &A32_pred_opc8[0x24],
    /* OP_subs           */ &A32_pred_opc8[0x25],
    /* OP_svc            */ &A32_pred_opc8[0xf0],
    /* OP_sxtab          */ &A32_ext_opc4y[7][0x04],
    /* OP_sxtab16        */ &A32_ext_opc4y[6][0x04],
    /* OP_sxtah          */ &A32_ext_opc4y[8][0x04],
    /* OP_teq            */ &A32_pred_opc8[0x33],
    /* OP_tst            */ &A32_pred_opc8[0x31],
    /* OP_uadd16         */ &A32_ext_opc4y[3][0x01],
    /* OP_uadd8          */ &A32_ext_opc4y[3][0x05],
    /* OP_uasx           */ &A32_ext_opc4y[3][0x02],
    /* OP_ubfx           */ &A32_ext_bit4[5][0x01],
    /* OP_udiv           */ &A32_ext_bit4[1][0x01],
    /* OP_uhadd16        */ &A32_ext_opc4y[5][0x01],
    /* OP_uhadd8         */ &A32_ext_opc4y[5][0x05],
    /* OP_uhasx          */ &A32_ext_opc4y[5][0x02],
    /* OP_uhsax          */ &A32_ext_opc4y[5][0x03],
    /* OP_uhsub16        */ &A32_ext_opc4y[5][0x04],
    /* OP_uhsub8         */ &A32_ext_opc4y[5][0x08],
    /* OP_umaal          */ &A32_ext_opc4x[4][0x02],
    /* OP_umlal          */ &A32_ext_opc4x[10][0x02],
    /* OP_umlals         */ &A32_ext_opc4x[11][0x02],
    /* OP_umull          */ &A32_ext_opc4x[8][0x02],
    /* OP_umulls         */ &A32_ext_opc4x[9][0x02],
    /* OP_uqadd16        */ &A32_ext_opc4y[4][0x01],
    /* OP_uqadd8         */ &A32_ext_opc4y[4][0x05],
    /* OP_uqasx          */ &A32_ext_opc4y[4][0x02],
    /* OP_uqsax          */ &A32_ext_opc4y[4][0x03],
    /* OP_uqsub16        */ &A32_ext_opc4y[4][0x04],
    /* OP_uqsub8         */ &A32_ext_opc4y[4][0x08],
    /* OP_usada8         */ &A32_ext_bit4[2][0x01],
    /* OP_usat           */ &A32_ext_opc4y[10][0x01],
    /* OP_usat16         */ &A32_ext_opc4y[10][0x02],
    /* OP_usax           */ &A32_ext_opc4y[3][0x03],
    /* OP_usub16         */ &A32_ext_opc4y[3][0x04],
    /* OP_usub8          */ &A32_ext_opc4y[3][0x08],
    /* OP_uxtab          */ &A32_ext_opc4y[10][0x04],
    /* OP_uxtab16        */ &A32_ext_opc4y[9][0x04],
    /* OP_uxtah          */ &A32_ext_opc4y[11][0x04],
    /* OP_vabs_f32       */ &A32_ext_bits16[1][0x00],
    /* OP_vabs_f64       */ &A32_ext_bits16[3][0x00],
    /* OP_vadd_f32       */ &A32_ext_opc4fpA[3][0x00],
    /* OP_vadd_f64       */ &A32_ext_opc4fpB[3][0x00],
    /* OP_vcmpe_f32      */ &A32_ext_bits16[1][0x05],
    /* OP_vcmpe_f64      */ &A32_ext_bits16[3][0x05],
    /* OP_vcmp_f32       */ &A32_ext_opc4[7][0x08],
    /* OP_vcmp_f64       */ &A32_ext_opc4[8][0x08],
    /* OP_vcvt_f32_f64   */ &A32_ext_bits16[3][0x07],
    /* OP_vcvt_f32_s16   */ &A32_ext_bits16[0][0x0a],
    /* OP_vcvt_f32_s32   */ &A32_ext_bits16[1][0x0a],
    /* OP_vcvt_f32_u16   */ &A32_ext_bits16[0][0x0b],
    /* OP_vcvt_f32_u32   */ &A32_ext_bits16[1][0x0b],
    /* OP_vcvt_f64_f32   */ &A32_ext_bits16[1][0x07],
    /* OP_vcvt_f64_s16   */ &A32_ext_bits16[2][0x0a],
    /* OP_vcvt_f64_s32   */ &A32_ext_bits16[3][0x0a],
    /* OP_vcvt_f64_u16   */ &A32_ext_bits16[2][0x0b],
    /* OP_vcvt_f64_u32   */ &A32_ext_bits16[3][0x0b],
    /* OP_vcvtr_s32_f32  */ &A32_ext_bits16[0][0x0d],
    /* OP_vcvtr_s32_f64  */ &A32_ext_bits16[2][0x0d],
    /* OP_vcvtr_u32_f32  */ &A32_ext_bits16[0][0x0c],
    /* OP_vcvtr_u32_f64  */ &A32_ext_bits16[2][0x0c],
    /* OP_vcvt_s16_f32   */ &A32_ext_bits16[0][0x0e],
    /* OP_vcvt_s16_f64   */ &A32_ext_bits16[2][0x0e],
    /* OP_vcvt_s32_f32   */ &A32_ext_bits16[1][0x0e],
    /* OP_vcvt_s32_f64   */ &A32_ext_bits16[3][0x0e],
    /* OP_vcvt_u16_f32   */ &A32_ext_bits16[0][0x0f],
    /* OP_vcvt_u16_f64   */ &A32_ext_bits16[2][0x0f],
    /* OP_vcvt_u32_f32   */ &A32_ext_bits16[1][0x0f],
    /* OP_vcvt_u32_f64   */ &A32_ext_bits16[3][0x0f],
    /* OP_vdiv_f32       */ &A32_ext_opc4fpA[8][0x00],
    /* OP_vdiv_f64       */ &A32_ext_opc4fpB[8][0x00],
    /* OP_vdup_16        */ &A32_ext_opc4fpB[8][0x03],
    /* OP_vdup_32        */ &A32_ext_opc4fpB[8][0x01],
    /* OP_vdup_8         */ &A32_ext_opc4fpB[11][0x01],
    /* OP_vfma_f32       */ &A32_ext_opc4fpA[10][0x00],
    /* OP_vfma_f64       */ &A32_ext_opc4fpB[10][0x00],
    /* OP_vfms_f32       */ &A32_ext_opc4fpA[10][0x02],
    /* OP_vfms_f64       */ &A32_ext_opc4fpB[10][0x04],
    /* OP_vfnma_f32      */ &A32_ext_opc4fpA[9][0x00],
    /* OP_vfnma_f64      */ &A32_ext_opc4fpB[9][0x00],
    /* OP_vfnms_f32      */ &A32_ext_opc4fpA[9][0x02],
    /* OP_vfnms_f64      */ &A32_ext_opc4fpB[9][0x04],
    /* OP_vldmdb         */ &A32_ext_fp[17][0x00],
    /* OP_vldmia         */ &A32_ext_fp[9][0x00],
    /* OP_vldr           */ &A32_ext_fp[15][0x00],
    /* OP_vmla_f32       */ &A32_ext_opc4fpA[0][0x00],
    /* OP_vmla_f64       */ &A32_ext_opc4fpB[0][0x00],
    /* OP_vmls_f32       */ &A32_ext_opc4fpA[0][0x02],
    /* OP_vmls_f64       */ &A32_ext_opc4fpB[0][0x04],
    /* OP_vmov           */ &A32_ext_fp[2][0x00],
    /* OP_vmov_16        */ &A32_ext_opc4fpB[0][0x03],
    /* OP_vmov_32        */ &A32_ext_opc4fpB[0][0x01],
    /* OP_vmov_8         */ &A32_ext_opc4fpB[4][0x01],
    /* OP_vmov_f32       */ &A32_ext_opc4[7][0x00],
    /* OP_vmov_f64       */ &A32_ext_opc4[8][0x00],
    /* OP_vmov_s16       */ &A32_ext_opc4fpB[1][0x03],
    /* OP_vmov_s8        */ &A32_ext_opc4fpB[5][0x01],
    /* OP_vmov_u16       */ &A32_ext_opc4[8][0x03],
    /* OP_vmov_u8        */ &A32_ext_opc4[10][0x01],
    /* OP_vmrs           */ &A32_ext_RBPC[0][0x00],
    /* OP_vmrs_apsr      */ &A32_ext_RBPC[0][0x01],
    /* OP_vmsr           */ &A32_ext_opc4fpA[13][0x01],
    /* OP_vmul_f32       */ &A32_ext_opc4fpA[2][0x00],
    /* OP_vmul_f64       */ &A32_ext_opc4fpB[2][0x00],
    /* OP_vneg_f32       */ &A32_ext_bits16[0][0x01],
    /* OP_vneg_f64       */ &A32_ext_bits16[2][0x01],
    /* OP_vnmla_f32      */ &A32_ext_opc4fpA[1][0x02],
    /* OP_vnmla_f64      */ &A32_ext_opc4fpB[1][0x04],
    /* OP_vnmls_f32      */ &A32_ext_opc4fpA[1][0x00],
    /* OP_vnmls_f64      */ &A32_ext_opc4fpB[1][0x00],
    /* OP_vnmul_f32      */ &A32_ext_opc4fpA[2][0x02],
    /* OP_vnmul_f64      */ &A32_ext_opc4fpB[2][0x04],
    /* OP_vrintr_f32     */ &A32_ext_bits16[0][0x06],
    /* OP_vrintr_f64     */ &A32_ext_bits16[2][0x06],
    /* OP_vrintx_f32     */ &A32_ext_bits16[0][0x07],
    /* OP_vrintx_f64     */ &A32_ext_bits16[2][0x07],
    /* OP_vrintz_f32     */ &A32_ext_bits16[1][0x06],
    /* OP_vrintz_f64     */ &A32_ext_bits16[3][0x06],
    /* OP_vsqrt_f32      */ &A32_ext_bits16[1][0x01],
    /* OP_vsqrt_f64      */ &A32_ext_bits16[3][0x01],
    /* OP_vstmdb         */ &A32_ext_fp[16][0x00],
    /* OP_vstmia         */ &A32_ext_fp[8][0x00],
    /* OP_vstr           */ &A32_ext_fp[14][0x00],
    /* OP_vsub_f32       */ &A32_ext_opc4fpA[3][0x02],
    /* OP_vsub_f64       */ &A32_ext_opc4fpB[3][0x04],
    /* OP_wfe            */ &A32_ext_bits0[0][0x02],
    /* OP_wfi            */ &A32_ext_bits0[0][0x03],
    /* OP_yield          */ &A32_ext_bits0[0][0x01],

};

/* for constructing linked lists of table entries */
#define NA 0
#define END_LIST  0
#define exop  (ptr_int_t)&A32_extra_operands
#define top8  (ptr_int_t)&A32_pred_opc8
#define top4x (ptr_int_t)&A32_ext_opc4x
#define top4y (ptr_int_t)&A32_ext_opc4y
#define top4  (ptr_int_t)&A32_ext_opc4
#define ti19  (ptr_int_t)&A32_ext_imm1916
#define tb0   (ptr_int_t)&A32_ext_bits0
#define tb8   (ptr_int_t)&A32_ext_bits8
#define tb9   (ptr_int_t)&A32_ext_bit9
#define tb4   (ptr_int_t)&A32_ext_bit4
#define tfp   (ptr_int_t)&A32_ext_fp
#define tfpA  (ptr_int_t)&A32_ext_opc4fpA
#define tfpB  (ptr_int_t)&A32_ext_opc4fpB
#define t16   (ptr_int_t)&A32_ext_bits16
#define trbpc (ptr_int_t)&A32_ext_RBPC
#define trdpc (ptr_int_t)&A32_ext_RDPC
#define ti5   (ptr_int_t)&A32_ext_imm5

/****************************************************************************
 * Top-level A32 table for predicate != 1111, indexed by bits 27:20
 */
const instr_info_t A32_pred_opc8[] = {
    /* {op/type, op encoding, name, dst1, src1, src2, src3, src4, flags, eflags, code} */
    /* 00 */
    {EXT_OPC4X , 0x00000000, "(ext opc4x 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4X , 0x00100000, "(ext opc4x 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4X , 0x00200000, "(ext opc4x 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_OPC4X , 0x00300000, "(ext opc4x 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_OPC4X , 0x00400000, "(ext opc4x 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_OPC4X , 0x00500000, "(ext opc4x 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_OPC4X , 0x00600000, "(ext opc4x 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_OPC4X , 0x00700000, "(ext opc4x 7)", xx, xx, xx, xx, xx, no, x, 7},
    /* 08 */
    {EXT_OPC4X , 0x00800000, "(ext opc4x 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_OPC4X , 0x00900000, "(ext opc4x 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_OPC4X , 0x00a00000, "(ext opc4x 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_OPC4X , 0x00b00000, "(ext opc4x 11)", xx, xx, xx, xx, xx, no, x, 11},
    {EXT_OPC4X , 0x00c00000, "(ext opc4x 12)", xx, xx, xx, xx, xx, no, x, 12},
    {EXT_OPC4X , 0x00d00000, "(ext opc4x 13)", xx, xx, xx, xx, xx, no, x, 13},
    {EXT_OPC4X , 0x00e00000, "(ext opc4x 14)", xx, xx, xx, xx, xx, no, x, 14},
    {EXT_OPC4X , 0x00f00000, "(ext opc4x 15)", xx, xx, xx, xx, xx, no, x, 15},
    /* 10 */
    {EXT_OPC4  , 0x01000000, "(ext opc4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4X , 0x01100000, "(ext opc4x 16)", xx, xx, xx, xx, xx, no, x, 16},
    {EXT_OPC4  , 0x01200000, "(ext opc4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4X , 0x01300000, "(ext opc4x 17)", xx, xx, xx, xx, xx, no, x, 17},
    {EXT_OPC4  , 0x01400000, "(ext opc4 2)", xx, xx, xx, xx, xx, no, x, 2},
    {EXT_OPC4X , 0x01500000, "(ext opc4x 18)", xx, xx, xx, xx, xx, no, x, 18},
    {EXT_OPC4  , 0x01600000, "(ext opc4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_OPC4X , 0x01700000, "(ext opc4x 19)", xx, xx, xx, xx, xx, no, x, 19},
    /* 18 */
    {EXT_OPC4X , 0x01800000, "(ext opc4x 20)", xx, xx, xx, xx, xx, no, x, 20},
    {EXT_OPC4X , 0x01900000, "(ext opc4x 21)", xx, xx, xx, xx, xx, no, x, 21},
    {EXT_OPC4  , 0x01a00000, "(ext opc4 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_OPC4  , 0x01b00000, "(ext opc4 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_OPC4X , 0x01c00000, "(ext opc4x 22)", xx, xx, xx, xx, xx, no, x, 22},
    {EXT_OPC4X , 0x01d00000, "(ext opc4x 23)", xx, xx, xx, xx, xx, no, x, 23},
    {EXT_OPC4X , 0x01e00000, "(ext opc4x 24)", xx, xx, xx, xx, xx, no, x, 24},
    {EXT_OPC4  , 0x01f00000, "(ext opc4 7)", xx, xx, xx, xx, xx, no, x, 7},
    /* 20 */
    {OP_and    , 0x02000000, "and"   , RBw, xx, RAw, i12, xx, pred, x, top4x[0][0x00]},
    {OP_ands   , 0x02100000, "ands"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[1][0x00]},
    {OP_eor    , 0x02200000, "eor"   , RBw, xx, RAw, i12, xx, pred, x, top4x[2][0x00]},
    {OP_eors   , 0x02300000, "eors"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[3][0x00]},
    {OP_sub    , 0x02400000, "sub"   , RBw, xx, RAw, i12, xx, pred, x, top4x[4][0x00]},/* XXX disasm: RA=r15 => "adr" */
    {OP_subs   , 0x02500000, "subs"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[5][0x00]},
    {OP_rsb    , 0x02600000, "rsb"   , RBw, xx, RAw, i12, xx, pred, x, top4x[6][0x00]},
    {OP_rsbs   , 0x02700000, "rsbs"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[7][0x00]},
    /* 28 */
    {OP_add    , 0x02800000, "add"   , RBw, xx, RAw, i12, xx, pred, x, top4x[8][0x00]}, /* XXX disasm: RA=r15 => "adr" */
    {OP_adds   , 0x02900000, "adds"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[9][0x00]},
    {OP_adc    , 0x02a00000, "adc"   , RBw, xx, RAw, i12, xx, pred, x, top4x[10][0x00]},
    {OP_adcs   , 0x02b00000, "adcs"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[11][0x00]},
    {OP_sbc    , 0x02c00000, "sbc"   , RBw, xx, RAw, i12, xx, pred, x, top4x[12][0x00]},
    {OP_sbcs   , 0x02d00000, "sbcs"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[13][0x00]},
    {OP_rsc    , 0x02e00000, "rsc"   , RBw, xx, RAw, i12, xx, pred, x, top4x[14][0x00]},
    {OP_rscs   , 0x02f00000, "rscs"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[15][0x00]},
    /* 30 */
    {OP_movw   , 0x03000000, "movw"  , RBw, xx, i16x0_16, xx, xx, pred, x, END_LIST},
    {OP_tst    , 0x03100000, "tst"   , RAw, xx, i12, xx, xx, pred, fWNZC, top4x[16][0x00]},
    {EXT_IMM1916,0x03200000, "(ext imm1916 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_teq    , 0x03300000, "teq"   , RAw, xx, i12, xx, xx, pred, fWNZC, top4x[17][0x00]},
    {OP_movt   , 0x03400000, "movt"  , RBt, xx, i16x0_16, xx, xx, pred, x, END_LIST},
    {OP_cmp    , 0x03500000, "cmp"   , RAw, xx, i12, xx, xx, pred, fWNZCV, top4x[18][0x00]},
    {OP_msr    , 0x03600000, "msr"   , SPSR, xx, i4_16, i12, xx, pred, x, ti19[0][0x01]},
    {OP_cmn    , 0x03700000, "cmn"   , RAw, xx, i12, xx, xx, pred, fWNZCV, top4x[19][0x00]},
    /* 38 */
    {OP_orr    , 0x03800000, "orr"   , RBw, xx, RAw, i12, xx, pred, x, top4x[20][0x00]},
    {OP_orrs   , 0x03900000, "orrs"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[21][0x00]},
    {OP_mov    , 0x03a00000, "mov"   , RBw, xx, i12, xx, xx, pred, x, ti5[0][0x00]},
    {OP_movs   , 0x03b00000, "movs"  , RBw, xx, i12, xx, xx, pred, fWNZCV, ti5[2][0x00]},
    {OP_bic    , 0x03c00000, "bic"   , RBw, xx, RAw, i12, xx, pred, x, top4x[22][0x00]},
    {OP_bics   , 0x03d00000, "bics"  , RBw, xx, RAw, i12, xx, pred, fWNZCV, top4x[23][0x00]},
    {OP_mvn    , 0x03e00000, "mvn"   , RBw, xx, i12, xx, xx, pred, x, top4x[24][0x00]},
    {OP_mvns   , 0x03f00000, "mvns"  , RBw, xx, i12, xx, xx, pred, fWNZCV, top4x[25][0x00]},
    /* 40 */
    {OP_str    , 0x04000000, "str"   , Mw, RAw, RBw, RAw, n12, pred, x, top4y[6][0x00]},/*PUW=000*/
    {OP_ldr    , 0x04100000, "ldr"   , RBw, RAw, Mw, RAw, n12, pred, x, top8[0x69]},/*PUW=000*/
    {OP_strt   , 0x04200000, "strt"  , Mw, RAw, RBw, RAw, n12, pred, x, top4y[7][0x00]},/*PUW=001*/
    {OP_ldrt   , 0x04300000, "ldrt"  , RBw, RAw, Mw, RAw, n12, pred, x, top4y[8][0x00]},/*PUW=001*/
    {OP_strb   , 0x04400000, "strb"  , Mb, RAw, RBb, RAw, n12, pred, x, top4y[9][0x00]},/*PUW=000*/
    {OP_ldrb   , 0x04500000, "ldrb"  , RBw, RAw, Mb, RAw, n12, pred, x, top8[0x6d]},/*PUW=000*/
    {OP_strbt  , 0x04600000, "strbt" , Mb, RAw, RBb, RAw, n12, pred, x, top4y[10][0x00]},/*PUW=001*/
    {OP_ldrbt  , 0x04700000, "ldrbt" , RBw, RAw, Mb, RAw, n12, pred, x, top4y[11][0x00]},/*PUW=001*/
    /* 48 */
    {OP_str    , 0x04800000, "str"   , Mw, RAw, RBw, RAw, i12, pred, x, top8[0x40]},/*PUW=010*/
    {OP_ldr    , 0x04900000, "ldr"   , RBw, RAw, Mw, RAw, i12, pred, x, top8[0x41]},/*PUW=010*//* XXX: RA=SP + imm12=8, then "pop RBw" */
    {OP_strt   , 0x04a00000, "strt"  , Mw, RAw, RBw, RAw, i12, pred, x, top8[0x42]},/*PUW=011*/
    {OP_ldrt   , 0x04b00000, "ldrt"  , RBw, RAw, Mw, RAw, i12, pred, x, top8[0x43]},/*PUW=011*/
    {OP_strb   , 0x04c00000, "strb"  , Mb, RAw, RBb, RAw, i12, pred, x, top8[0x44]},/*PUW=010*/
    {OP_ldrb   , 0x04d00000, "ldrb"  , RBw, RAw, Mb, RAw, i12, pred, x, top8[0x45]},/*PUW=010*/
    {OP_strbt  , 0x04e00000, "strbt" , Mb, RAw, RBb, RAw, i12, pred, x, top8[0x46]},/*PUW=011*/
    {OP_ldrbt  , 0x04f00000, "ldrbt" , RBw, RAw, Mb, RAw, i12, pred, x, top8[0x47]},/*PUW=011*/
    /* 50 */
    {OP_str    , 0x05000000, "str"   , MN12w, xx, RBw, xx, xx, pred, x, tb4[2][0x00]},/*PUW=100*/
    {OP_ldr    , 0x05100000, "ldr"   , RBw, xx, MN12w, xx, xx, pred, x, top8[0x79]},/*PUW=100*/
    {OP_str    , 0x05200000, "str"   , MN12w, RAw, RBw, RAw, n12, pred, x, tb4[3][0x00]},/*PUW=101*/
    {OP_ldr    , 0x05300000, "ldr"   , RBw, RAw, MN12w, RAw, n12, pred, x, tb4[4][0x00]},/*PUW=101*/
    {OP_strb   , 0x05400000, "strb"  , MN12b, xx, RBb, xx, xx, pred, x, top4y[13][0x00]},/*PUW=100*/
    {OP_ldrb   , 0x05500000, "ldrb"  , RBw, xx, MN12b, xx, xx, pred, x, tb4[8][0x00]},/*PUW=100*/
    {OP_strb   , 0x05600000, "strb"  , MN12b, RAw, RBb, RAw, n12, pred, x, tb4[5][0x00]},/*PUW=101*/
    {OP_ldrb   , 0x05700000, "ldrb"  , RBw, RAw, MN12b, RAw, n12, pred, x, tb4[6][0x00]},/*PUW=101*/
    /* 58 */
    {OP_str    , 0x05800000, "str"   , MP12w, xx, RBw, xx, xx, pred, x, top8[0x50]},/*PUW=110*/
    {OP_ldr    , 0x05900000, "ldr"   , RBw, xx, MP12w, xx, xx, pred, x, top8[0x51]},/*PUW=110*/
    {OP_str    , 0x05a00000, "str"   , MP12w, RAw, RBw, RAw, i12, pred, x, top8[0x52]},/*PUW=111*/
    {OP_ldr    , 0x05b00000, "ldr"   , RBw, RAw, MP12w, RAw, i12, pred, x, top8[0x53]},/*PUW=111*/
    {OP_strb   , 0x05c00000, "strb"  , MP12b, xx, RBb, xx, xx, pred, x, top8[0x54]},/*PUW=110*/
    {OP_ldrb   , 0x05d00000, "ldrb"  , RBw, xx, MN12b, xx, xx, pred, x, top8[0x55]},/*PUW=110*/
    {OP_strb   , 0x05e00000, "strb"  , MP12b, RAw, RBb, RAw, i12, pred, x, top8[0x56]},/*PUW=111*/
    {OP_ldrb   , 0x05f00000, "ldrb"  , RBw, RAw, MP12b, RAw, i12, pred, x, top8[0x57]},/*PUW=111*/
    /* 60 */
    {OP_str    , 0x06000000, "str"   , Mw, RAw, RBw, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {EXT_OPC4Y , 0x06100000, "(ext opc4y 0)", xx, xx, xx, xx, xx, no, x, 0},
    {EXT_OPC4Y , 0x06200000, "(ext opc4y 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4Y , 0x06300000, "(ext opc4y 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_strb   , 0x06400000, "strb"  , Mb, RAw, RBb, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {EXT_OPC4Y , 0x06500000, "(ext opc4y 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_OPC4Y , 0x06600000, "(ext opc4y 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_OPC4Y , 0x06700000, "(ext opc4y 5)", xx, xx, xx, xx, xx, no, x, 5},
    /* 68 */
    {EXT_OPC4Y , 0x06800000, "(ext opc4y 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_ldr    , 0x06900000, "ldr"   , RBw, RAw, Mw, RAw, RDw, xop_shift|pred, x, top4y[0][0x00]},/*PUW=010*/
    {EXT_OPC4Y , 0x06a00000, "(ext opc4y 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_OPC4Y , 0x06b00000, "(ext opc4y 8)", xx, xx, xx, xx, xx, no, x, 8},
    {EXT_OPC4Y , 0x06c00000, "(ext opc4y 9)", xx, xx, xx, xx, xx, no, x, 9},
    {OP_ldrb   , 0x06d00000, "ldrb"  , RBw, RAw, Mb, RAw, RDw, xop_shift|pred, x, top4y[3][0x00]},/*PUW=010*/
    {EXT_OPC4Y , 0x06e00000, "(ext opc4y 10)", xx, xx, xx, xx, xx, no, x, 10},
    {EXT_OPC4Y , 0x06f00000, "(ext opc4y 11)", xx, xx, xx, xx, xx, no, x, 11},
    /* 70 */
    {EXT_OPC4Y , 0x07000000, "(ext opc4y 12)", xx, xx, xx, xx, xx, no, x, 12},
    {EXT_BIT4  , 0x07100000, "(ext bit4 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_str    , 0x07200000, "str"   , MNSw, RAw, RBw, RAw, RDw, xop_shift|pred, x, top8[0x48]},/*PUW=101*/
    {EXT_BIT4  , 0x07300000, "(ext bit4 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXT_OPC4Y , 0x07400000, "(ext opc4y 13)", xx, xx, xx, xx, xx, no, x, 13},
    {EXT_OPC4Y , 0x07500000, "(ext opc4y 14)", xx, xx, xx, xx, xx, no, x, 14},
    {OP_strb   , 0x07600000, "strb"  , MNSb, RAw, RBb, RAw, RDw, xop_shift|pred, x, top8[0x4c]},/*PUW=101*/
    {OP_ldrb   , 0x07700000, "ldrb"  , RBw, RAw, MNSb, RAw, RDw, xop_shift|pred, x, top8[0x4d]},/*PUW=101*/
    /* 78 */
    {EXT_BIT4  , 0x07800000, "(ext bit4 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_ldr    , 0x07900000, "ldr"   , RBw, xx, MPSw, xx, xx, pred, x, tb4[0][0x00]},/*PUW=110*/
    {EXT_BIT4  , 0x07a00000, "(ext bit4 3)", xx, xx, xx, xx, xx, no, x, 3},
    {EXT_BIT4  , 0x07b00000, "(ext bit4 4)", xx, xx, xx, xx, xx, no, x, 4},
    {EXT_BIT4  , 0x07c00000, "(ext bit4 5)", xx, xx, xx, xx, xx, no, x, 5},
    {EXT_BIT4  , 0x07d00000, "(ext bit4 6)", xx, xx, xx, xx, xx, no, x, 6},
    {EXT_BIT4  , 0x07e00000, "(ext bit4 7)", xx, xx, xx, xx, xx, no, x, 7},
    {EXT_BIT4  , 0x07f00000, "(ext bit4 8)", xx, xx, xx, xx, xx, no, x, 8},
    /* 80 */
    {OP_stmda  , 0x08000000, "stmda" , MDAl, xx, L16w, xx, xx, pred, x, top8[0x82]},/*PUW=000*/
    {OP_ldmda  , 0x08100000, "ldmda" , L16w, xx, MDAl, xx, xx, pred, x, top8[0x83]},/*PUW=000*/
    {OP_stmda  , 0x08200000, "stmda" , MDAl, RAw, L16w, xx, xx, pred, x, END_LIST},/*PUW=001*/
    {OP_ldmda  , 0x08300000, "ldmda" , L16w, RAw, MDAl, xx, xx, pred, x, END_LIST},/*PUW=001*/
    {OP_stmda_priv,0x08400000,"stmda", MDAl, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=000*/
    {OP_ldmda_priv,0x08500000,"ldmda", L16w, xx, MDAl, xx, xx, pred, x, top8[0x87]},/*PUW=000*/
    {INVALID   , 0x08600000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldmda_priv,0x08700000,"ldmda", L16w, RAw, MDAl, xx, xx, pred, x, END_LIST},/*PUW=001*/
    /* 88 */
    {OP_stm    , 0x08800000, "stm"   , Ml, xx, L16w, xx, xx, pred, x, top8[0x8a]},/*PUW=010*//* XXX: "stmia" alias (used inconsistently by gdb) */
    {OP_ldm    , 0x08900000, "ldm"   , L16w, xx, Ml, xx, xx, pred, x, top8[0x8b]},/*PUW=010*//* XXX: "ldmia" and "ldmfb" aliases */
    {OP_stm    , 0x08a00000, "stm"   , Ml, RAw, L16w, xx, xx, pred, x, END_LIST},/*PUW=011*/
    {OP_ldm    , 0x08b00000, "ldm"   , L16w, RAw, Ml, xx, xx, pred, x, END_LIST},/*PUW=011*/
    {OP_stm_priv,0x08c00000, "stm"   , Ml, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=010*/
    {OP_ldm_priv,0x08d00000, "ldm"   , L16w, xx, Ml, xx, xx, pred, x, END_LIST},/*PUW=010*/
    {INVALID   , 0x08e00000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldmia_priv,0x08f00000,"ldmia", L16w, RAw, Ml, xx, xx, pred, x, END_LIST},/*PUW=011*/
    /* 90 */
    {OP_stmdb  , 0x09000000, "stmdb" , MDBl, xx, L16w, xx, xx, pred, x, top8[0x92]},/*PUW=100*/
    {OP_ldmdb  , 0x09100000, "ldmdb" , L16w, xx, MDBl, xx, xx, pred, x, top8[0x93]},/*PUW=100*/
    {OP_stmdb  , 0x09200000, "stmdb" , MDBl, RAw, L16w, xx, xx, pred, x, END_LIST},/*PUW=101*//* XXX: if RA=SP, then "push" */
    {OP_ldmdb  , 0x09300000, "ldmdb" , L16w, RAw, MDBl, xx, xx, pred, x, END_LIST},/*PUW=101*/
    {OP_stmdb_priv,0x09400000,"stmdb", MDBl, xx, L16w, xx, xx, pred, x, END_LIST},/*PUW=100*/
    {OP_ldmdb_priv,0x09500000,"ldmdb", L16w, xx, MDBl, xx, xx, pred, x, top8[0x97]},/*PUW=100*/
    {INVALID   , 0x09600000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldmdb_priv,0x09700000,"ldmdb", L16w, RAw, MDBl, xx, xx, pred, x, END_LIST},/*PUW=101*/
    /* 98 */
    {OP_stmib  , 0x09800000, "stmib" , MUBl, xx, L16w, xx, xx, pred, x, top8[0x9a]},/*PUW=110*//* XXX: "stmia" or "stmea" alias */
    {OP_ldmib  , 0x09900000, "ldmib" , L16w, xx, MUBl, xx, xx, pred, x, top8[0x9b]},/*PUW=110*//* XXX: "ldmia" alias */
    {OP_stmib  , 0x09a00000, "stmib" , MUBl, RAw, L16w, xx, xx, pred, x, END_LIST},/*PUW=111*/
    {OP_ldmib  , 0x09b00000, "ldmib" , L16w, RAw, MUBl, xx, xx, pred, x, END_LIST},/*PUW=111*/
    {OP_stm    , 0x09c00000, "stm"   , MUBl, xx, L16w, xx, xx, pred, x, top8[0x88]},/*PUW=110*/
    {OP_ldm    , 0x09d00000, "ldm"   , L16w, xx, MUBl, xx, xx, pred, x, top8[0x9f]},/*PUW=110*/
    {INVALID   , 0x09e00000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldm    , 0x09f00000, "ldm"   , L16w, RAw, MUBl, xx, xx, pred, x, top8[0x89]},/*PUW=111*/
    /* a0 */
    {OP_b      , 0x0a000000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},/*no chain nec.*/
    {OP_b      , 0x0a100000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a200000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a300000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a400000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a500000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a600000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a700000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    /* a8 */
    {OP_b      , 0x0a800000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0a900000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0aa00000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0ab00000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0ac00000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0ad00000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0ae00000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    {OP_b      , 0x0af00000, "b"     , xx, xx, i24, xx, xx, no, x, END_LIST},
    /* b0 */
    {OP_bl     , 0x0b000010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},/*no chain nec.*/
    {OP_bl     , 0x0b100010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b200010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b300010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b400010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b500010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b600010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b700010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    /* b8 */
    {OP_bl     , 0x0b800010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0b900010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0ba00010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0bb00010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0bc00010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0bd00010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0be00010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    {OP_bl     , 0x0bf00010, "bl"    , LRw, xx, i24, xx, xx, pred, x, END_LIST},
    /* c0 */
    {INVALID   , 0x0c000000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0c100000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_FP    , 0x0c200000, "(ext fp 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FP    , 0x0c300000, "(ext fp 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_FP    , 0x0c400000, "(ext fp 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_FP    , 0x0c500000, "(ext fp 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_FP    , 0x0c600000, "(ext fp 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_FP    , 0x0c700000, "(ext fp 5)",  xx, xx, xx, xx, xx, no, x, 5},
    /* c8 */
    {EXT_FP    , 0x0c800000, "(ext fp 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_FP    , 0x0c900000, "(ext fp 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_FP    , 0x0ca00000, "(ext fp 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_FP    , 0x0cb00000, "(ext fp 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_FP    , 0x0cc00000, "(ext fp 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_FP    , 0x0cd00000, "(ext fp 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_FP    , 0x0ce00000, "(ext fp 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_FP    , 0x0cf00000, "(ext fp 13)",  xx, xx, xx, xx, xx, no, x, 13},
    /* d0 */
    {EXT_FP    , 0x0d000000, "(ext fp 14)",  xx, xx, xx, xx, xx, no, x, 14},
    {EXT_FP    , 0x0d100000, "(ext fp 15)",  xx, xx, xx, xx, xx, no, x, 15},
    {EXT_FP    , 0x0d200000, "(ext fp 16)",  xx, xx, xx, xx, xx, no, x, 16},
    {EXT_FP    , 0x0d300000, "(ext fp 17)",  xx, xx, xx, xx, xx, no, x, 17},
    {EXT_FP    , 0x0d400000, "(ext fp 18)",  xx, xx, xx, xx, xx, no, x, 18},
    {EXT_FP    , 0x0d500000, "(ext fp 19)",  xx, xx, xx, xx, xx, no, x, 19},
    {EXT_FP    , 0x0d600000, "(ext fp 20)",  xx, xx, xx, xx, xx, no, x, 20},
    {EXT_FP    , 0x0d700000, "(ext fp 21)",  xx, xx, xx, xx, xx, no, x, 21},
    /* d8 */
    {EXT_FP    , 0x0d800000, "(ext fp 22)",  xx, xx, xx, xx, xx, no, x, 22},
    {EXT_FP    , 0x0d900000, "(ext fp 23)",  xx, xx, xx, xx, xx, no, x, 23},
    {EXT_FP    , 0x0da00000, "(ext fp 24)",  xx, xx, xx, xx, xx, no, x, 24},
    {EXT_FP    , 0x0db00000, "(ext fp 25)",  xx, xx, xx, xx, xx, no, x, 25},
    {EXT_FP    , 0x0dc00000, "(ext fp 26)",  xx, xx, xx, xx, xx, no, x, 26},
    {EXT_FP    , 0x0dd00000, "(ext fp 27)",  xx, xx, xx, xx, xx, no, x, 27},
    {EXT_FP    , 0x0de00000, "(ext fp 28)",  xx, xx, xx, xx, xx, no, x, 28},
    {EXT_FP    , 0x0df00000, "(ext fp 29)",  xx, xx, xx, xx, xx, no, x, 29},
    /* e0 */
    {EXT_FP    , 0x0e000000, "(ext fp 30)",  xx, xx, xx, xx, xx, no, x, 30},
    {EXT_FP    , 0x0e100000, "(ext fp 31)",  xx, xx, xx, xx, xx, no, x, 31},
    {EXT_FP    , 0x0e200000, "(ext fp 32)",  xx, xx, xx, xx, xx, no, x, 32},
    {EXT_FP    , 0x0e300000, "(ext fp 33)",  xx, xx, xx, xx, xx, no, x, 33},
    {EXT_FP    , 0x0e400000, "(ext fp 34)",  xx, xx, xx, xx, xx, no, x, 34},
    {EXT_FP    , 0x0e500000, "(ext fp 35)",  xx, xx, xx, xx, xx, no, x, 35},
    {EXT_FP    , 0x0e600000, "(ext fp 36)",  xx, xx, xx, xx, xx, no, x, 36},
    {EXT_FP    , 0x0e700000, "(ext fp 37)",  xx, xx, xx, xx, xx, no, x, 37},
    /* e8 */
    {EXT_FP    , 0x0e800000, "(ext fp 38)",  xx, xx, xx, xx, xx, no, x, 38},
    {EXT_FP    , 0x0e900000, "(ext fp 39)",  xx, xx, xx, xx, xx, no, x, 39},
    {EXT_FP    , 0x0ea00000, "(ext fp 40)",  xx, xx, xx, xx, xx, no, x, 40},
    {EXT_FP    , 0x0eb00000, "(ext fp 41)",  xx, xx, xx, xx, xx, no, x, 41},
    {EXT_FP    , 0x0ec00000, "(ext fp 42)",  xx, xx, xx, xx, xx, no, x, 42},
    {EXT_FP    , 0x0ed00000, "(ext fp 43)",  xx, xx, xx, xx, xx, no, x, 43},
    {EXT_FP    , 0x0ee00000, "(ext fp 44)",  xx, xx, xx, xx, xx, no, x, 44},
    {EXT_FP    , 0x0ef00000, "(ext fp 45)",  xx, xx, xx, xx, xx, no, x, 45},
    /* f0 */
    {OP_svc    , 0x0f000000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf1]},
    {OP_svc    , 0x0f100000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf2]},
    {OP_svc    , 0x0f200000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf3]},
    {OP_svc    , 0x0f300000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf4]},
    {OP_svc    , 0x0f400000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf5]},
    {OP_svc    , 0x0f500000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf6]},
    {OP_svc    , 0x0f600000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf7]},
    {OP_svc    , 0x0f700000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf8]},
    /* f8 */
    {OP_svc    , 0x0f800000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xf9]},
    {OP_svc    , 0x0f900000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xfa]},
    {OP_svc    , 0x0fa00000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xfb]},
    {OP_svc    , 0x0fb00000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xfc]},
    {OP_svc    , 0x0fc00000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xfd]},
    {OP_svc    , 0x0fd00000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xfe]},
    {OP_svc    , 0x0fe00000, "svc"   , xx, xx, i24, xx, xx, pred, x, top8[0xff]},
    {OP_svc    , 0x0ff00000, "svc"   , xx, xx, i24, xx, xx, pred, x, END_LIST},
};

/* Indexed by bits 7:4 but in the following manner:
 * + If bit 4 == 0, take entry 0;
 * + If bit 4 == 1 and bit 7 == 0, take entry 1;
 * + Else, take entry 2 + bits 6:5
 */
const instr_info_t A32_ext_opc4x[][6] = {
  { /* 0 */
    {OP_and    , 0x00000000, "and"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[0][0x01]},
    {OP_and    , 0x00000010, "and"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_mul    , 0x00000090, "mul"   , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_strh   , 0x000000b0, "strh"  , Mh, RAw, RBh, RAw, RDNw, pred, x, END_LIST},/*PUW=000*/
    {OP_ldrd   , 0x000000d0, "ldrd"  , RBEw, RB2w, RAw, Md, RDNw, xop_wb|pred|dstX3, x, top4x[2][0x04]},/*PUW=000*/
    {OP_strd   , 0x000000f0, "strd"  , Md, RAw, RBEw, RB2w, RDNw, xop_wb|pred, x, top4x[2][0x05]},/*PUW=000*/
  }, { /* 1 */
    {OP_ands   , 0x00100000, "ands"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[1][0x01]},
    {OP_ands   , 0x00100010, "ands"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {OP_muls   , 0x00100090, "muls"  , RBw, xx, RAw, RDw, xx, pred, fWNZCV, END_LIST},
    {OP_ldrh   , 0x001000b0, "ldrh"  , RBw, RAw, Mw, RAw, RDNw, pred, x, top4x[3][0x03]},/*PUW=000*/
    {OP_ldrsb  , 0x001000d0, "ldrsb" , RBw, RAw, Mb, RAw, RDNw, pred, x, END_LIST},/*PUW=000*/
    {OP_ldrsh  , 0x001000f0, "ldrsh" , RBw, RAw, Mh, RAw, RDNw, pred, x, END_LIST},/*PUW=000*/
  }, { /* 2 */
    {OP_eor    , 0x00200000, "eor"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[2][0x01]},
    {OP_eor    , 0x00200010, "eor"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_mla    , 0x00200090, "mla"   , RAw, xx, RBw, RCw, RDw, pred, x, END_LIST},
    {OP_strht  , 0x002000b0, "strht" , Mh, RAw, RBh, RAw, RDNw, pred, x, END_LIST},/*PUW=001*/
    {OP_ldrd   , 0x002000d0, "ldrd"  , RBEw, RB2w, RAw, Md, RDNw, xop_wb|pred|dstX3|unp, x, END_LIST},/*PUW=001*/
    {OP_strd   , 0x002000f0, "strd"  , Md, RAw, RBEw, RB2w, RDNw, xop_wb|pred|unp, x, END_LIST},/*PUW=001*/
  }, { /* 3 */
    {OP_eors   , 0x00300000, "eors"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[3][0x01]},
    {OP_eors   , 0x00300010, "eors"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {OP_mlas   , 0x00300090, "mlas"  , RAw, xx, RBw, RCw, RDw, pred, fWNZCV, END_LIST},
    {OP_ldrh   , 0x003000b0, "ldrht" , RBw, RAw, Mh, RAw, RDNw, pred, x, END_LIST},/*PUW=001*/
    {OP_ldrsbt , 0x003000d0, "ldrsbt", RBw, RAw, Mb, RAw, RDNw, pred, x, END_LIST},/*PUW=001*/
    {OP_ldrsht , 0x003000f0, "ldrsht", RBw, RAw, Mh, RAw, RDNw, pred, x, END_LIST},/*PUW=001*/
  }, { /* 4 */
    {OP_sub    , 0x00400000, "sub"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[4][0x01]},
    {OP_sub    , 0x00400010, "sub"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_umaal  , 0x00400090, "umaal" , RAw, xx, RBw, RCw, RDw, pred, x, END_LIST},
    {OP_strh   , 0x004000b0, "strh"  , Mh, RAw, RBh, RAw, n8x0_8, pred, x, top4x[6][0x03]},/*PUW=000*/
    {OP_ldrd   , 0x004000d0, "ldrd"  , RBEw, RB2w, RAw, Md, n8x0_8, xop_wb|pred|dstX3, x, top4x[6][0x04]},/*PUW=000*/
    {OP_strd   , 0x004000f0, "strd"  , Md, RAw, RBEw, RB2w, n8x0_8, xop_wb|pred, x, top4x[6][0x05]},/*PUW=000*/
  }, { /* 5 */
    {OP_subs   , 0x00500000, "subs"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[5][0x01]},
    {OP_subs   , 0x00500010, "subs"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {INVALID   , 0x00500090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh   , 0x005000b0, "ldrh"  , RBw, RAw, Mh, RAw, n8x0_8, pred, x, top4x[9][0x03]},/*PUW=000*/
    {OP_ldrsb  , 0x005000d0, "ldrsb" , RBw, RAw, Mb, RAw, n8x0_8, pred, x, top4x[9][0x04]},/*PUW=000*/
    {OP_ldrsh  , 0x005000f0, "ldrsh" , RBw, RAw, Mh, RAw, n8x0_8, pred, x, top4x[9][0x05]},/*PUW=000*/
  }, { /* 6 */
    {OP_rsb    , 0x00600000, "rsb"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[6][0x01]},
    {OP_rsb    , 0x00600010, "rsb"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_mls    , 0x00600090, "mls"   , RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_strh   , 0x006000b0, "strht" , Mh, RAw, RBw, RAw, n8x0_8, pred, x, top4x[8][0x03]},/*PUW=001*/
    {OP_ldrd   , 0x006000d0, "ldrd"  , RBEw, RB2w, RAw, Md, n8x0_8, xop_wb|dstX3|pred|unp, x, top4x[8][0x04]},/*PUW=001*/
    {OP_strd   , 0x006000f0, "strd"  , Md, RAw, RBEw, RB2w, n8x0_8, xop_wb|pred|unp, x, top4x[8][0x05]},/*PUW=001*/
  }, { /* 7 */
    {OP_rsbs   , 0x00700000, "rsbs"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[7][0x01]},
    {OP_rsbs   , 0x00700010, "rsbs"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {INVALID   , 0x00700090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrht  , 0x007000b0, "ldrht" , RBw, RAw, Mh, RAw, n8x0_8, pred, x, top4x[11][0x03]},/*PUW=001*/
    {OP_ldrsbt , 0x007000d0, "ldrsbt", RBw, RAw, Mb, RAw, n8x0_8, pred, x, top4x[11][0x04]},/*PUW=001*/
    {OP_ldrsht , 0x007000f0, "ldrsht", RBw, RAw, Mh, RAw, n8x0_8, pred, x, top4x[11][0x05]},/*PUW=001*/
  }, { /* 8 */
    {OP_add    , 0x00800000, "add"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[8][0x01]},
    {OP_add    , 0x00800010, "add"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_umull  , 0x00800090, "umull" , RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strh   , 0x008000b0, "strh"  , MPRh, RAw, RBh, RAw, RDw, pred, x, top4x[0][0x03]},/*PUW=010*/
    {OP_ldrd   , 0x008000d0, "ldrd"  , RBEw, RB2w, RAw, MPRd, RDw, xop_wb|pred|dstX3, x, top4x[10][0x04]},/*PUW=010*/
    {OP_strd   , 0x008000f0, "strd"  , MPRd, RAw, RBEw, RB2w, RDw, xop_wb|pred, x, top4x[10][0x05]},/*PUW=010*/
  }, { /* 9 */
    {OP_adds   , 0x00900000, "adds"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[9][0x01]},
    {OP_adds   , 0x00900010, "adds"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {OP_umulls , 0x00900090, "umulls", RAw, RBw, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_ldrh   , 0x009000b0, "ldrh"  , RBw, RAw, MPRw, RAw, RDw, pred, x, top4x[1][0x03]},/*PUW=010*/
    {OP_ldrsb  , 0x009000d0, "ldrsb" , RBw, RAw, MPRb, RAw, RDw, pred, x, top4x[1][0x04]},/*PUW=010*/
    {OP_ldrsh  , 0x009000f0, "ldrsh" , RBw, RAw, MPRh, RAw, RDw, pred, x, top4x[1][0x05]},/*PUW=010*/
  }, { /* 10 */
    {OP_adc    , 0x00a00000, "adc"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[10][0x01]},
    {OP_adc    , 0x00a00010, "adc"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_umlal  , 0x00a00090, "umlal" , RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strht  , 0x00a000b0, "strht" , MPRh, RAw, RBh, RAw, RDw, pred, x, top4x[2][0x03]},/*PUW=011*/
    {OP_ldrd   , 0x00a000d0, "ldrd"  , RBEw, RB2w, RAw, MPRd, RDw, xop_wb|pred|dstX3|unp, x, top4x[0][0x04]},/*PUW=011*/
    {OP_strd   , 0x00a000f0, "strd"  , MPRd, RAw, RBEw, RB2w, RDw, xop_wb|pred|unp, x, top4x[0][0x05]},/*PUW=011*/
  }, { /* 11 */
    {OP_adcs   , 0x00b00000, "adcs"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[11][0x01]},
    {OP_adcs   , 0x00b00010, "adcs"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {OP_umlals , 0x00b00090, "umlals", RAw, RBw, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_ldrht  , 0x00b000b0, "ldrht" , RBw, RAw, MPRh, RAw, RDw, pred, x, END_LIST},/*PUW=011*/
    {OP_ldrsbt , 0x00b000d0, "ldrsbt", RBw, RAw, MPRb, RAw, RDw, pred, x, top4x[3][0x04]},/*PUW=011*/
    {OP_ldrsht , 0x00b000f0, "ldrsht", RBw, RAw, MPRh, RAw, RDw, pred, x, top4x[3][0x05]},/*PUW=011*/
  }, { /* 12 */
    {OP_sbc    , 0x00c00000, "sbc"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[12][0x01]},
    {OP_sbc    , 0x00c00010, "sbc"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_smull  , 0x00c00090, "smull" , RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strh   , 0x00c000b0, "strh"  , MP44h, RAw, RBh, RAw, i8x0_8, pred, x, top4x[4][0x03]},/*PUW=010*/
    {OP_ldrd   , 0x00c000d0, "ldrd"  , RBEw, RB2w, RAw, MPRd, i8x0_8, xop_wb|pred|dstX3, x, top4x[4][0x04]},/*PUW=010*/
    {OP_strd   , 0x00c000f0, "strd"  , MP44d, RAw, RBEw, RB2w, i8x0_8, xop_wb|pred, x, top4x[14][0x05]},/*PUW=010*/
  }, { /* 13 */
    {OP_sbcs   , 0x00d00000, "sbcs"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[13][0x01]},
    {OP_sbcs   , 0x00d00010, "sbcs"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {OP_smulls , 0x00d00090, "smulls", RAw, RBw, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_ldrh   , 0x00d000b0, "ldrh"  , RBw, RAw, Mh, RAw, i8x0_8, pred, x, top4x[5][0x03]},/*PUW=010*/
    {OP_ldrsb  , 0x00d000d0, "ldrsb" , RBw, RAw, Mb, RAw, i8x0_8, pred, x, top4x[5][0x04]},/*PUW=010*/
    {OP_ldrsh  , 0x00d000f0, "ldrsh" , RBw, RAw, Mh, RAw, i8x0_8, pred, x, top4x[5][0x05]},/*PUW=010*/
  }, { /* 14 */
    {OP_rsc    , 0x00e00000, "rsc"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[14][0x01]},
    {OP_rsc    , 0x00e00010, "rsc"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {OP_smlal  , 0x00e00090, "smlal" , RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_strht  , 0x00e000b0, "strht" , MP44h, RAw, RBw, RAw, i8x0_8, pred, x, top4x[10][0x03]},/*PUW=011*/
    {OP_ldrd   , 0x00e000d0, "ldrd"  , RBEw, RB2w, RAw, MP44d, i8x0_8, xop_wb|dstX3|pred|unp, x, top4x[12][0x04]},/*PUW=011*/
    {OP_strd   , 0x00e000f0, "strd"  , MP44d, RAw, RBEw, RB2w, i8x0_8, xop_wb|pred|unp, x, top4x[4][0x05]},/*PUW=011*/
  }, { /* 15 */
    {OP_rscs   , 0x00f00000, "rscs"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[15][0x01]},
    {OP_rscs   , 0x00f00010, "rscs"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {OP_smlals , 0x00f00090, "smlals", RAw, RBw, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_ldrht  , 0x00f000b0, "ldrht" , RBw, RAw, MP44h, RAw, i8x0_8, pred, x, top4x[7][0x03]},/*PUW=011*/
    {OP_ldrsbt , 0x00f000d0, "ldrsbt", RBw, RAw, MP44b, RAw, i8x0_8, pred, x, top4x[7][0x04]},/*PUW=011*/
    {OP_ldrsht , 0x00f000f0, "ldrsht", RBw, RAw, MP44h, RAw, i8x0_8, pred, x, top4x[7][0x05]},/*PUW=011*/
  }, { /* 16 */
    {OP_tst    , 0x01100000, "tst"   , xx, RAw, RDw, sh2, i5, pred, fWNZC, top4x[16][0x01]},
    {OP_tst    , 0x01100010, "tst"   , xx, RAw, RDw, sh2, RCw, pred, fWNZC, END_LIST},
    {INVALID   , 0x01100090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh   , 0x011000b0, "ldrh"  , RBw, xx, MNRh, xx, xx, pred, x, top4x[25][0x03]},/*PUW=100*/
    {OP_ldrsb  , 0x011000d0, "ldrsb" , RBw, xx, MNRb, xx, xx, pred, x, top4x[25][0x04]},/*PUW=100*/
    {OP_ldrsh  , 0x011000f0, "ldrsh" , RBw, xx, MNRh, xx, xx, pred, x, top4x[25][0x05]},/*PUW=100*/
  }, { /* 17 */
    {OP_teq    , 0x01300000, "teq"   , RAw, xx, RDw, sh2, i5, pred, fWNZC, top4x[17][0x01]},
    {OP_teq    , 0x01300010, "teq"   , RAw, xx, RDw, sh2, RCw, pred, fWNZC, END_LIST},
    {INVALID   , 0x01300090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh   , 0x013000b0, "ldrh"  , RBw, RAw, MNRw, RAw, RDNw, pred, x, top4x[13][0x03]},/*PUW=101*/
    {OP_ldrsb  , 0x013000d0, "ldrsb" , RBw, RAw, MNRb, RAw, RDNw, pred, x, top4x[13][0x04]},/*PUW=101*/
    {OP_ldrsh  , 0x013000f0, "ldrsh" , RBw, RAw, MNRh, RAw, RDNw, pred, x, top4x[13][0x05]},/*PUW=101*/
  }, { /* 18 */
    {OP_cmp    , 0x01500000, "cmp"   , RAw, xx, RDw, sh2, i5, pred, fWNZCV, top4x[18][0x01]},
    {OP_cmp    , 0x01500010, "cmp"   , RAw, xx, RDw, sh2, RCw, pred, fWNZCV, END_LIST},
    {INVALID   , 0x01500090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh   , 0x015000b0, "ldrh"  , RBw, xx, MN44h, xx, xx, pred, x, top4x[21][0x03]},/*PUW=100*/
    {OP_ldrsb  , 0x015000d0, "ldrsb" , RBw, xx, MN44b, xx, xx, pred, x, top4x[21][0x04]},/*PUW=100*/
    {OP_ldrsh  , 0x015000f0, "ldrsh" , RBw, xx, MN44h, xx, xx, pred, x, top4x[21][0x05]},/*PUW=100*/
  }, { /* 19 */
    {OP_cmn    , 0x01700000, "cmn"   , RAw, xx, RDw, sh2, i5, pred, fWNZCV, top4x[19][0x01]},
    {OP_cmn    , 0x01700010, "cmn"   , RAw, xx, RDw, sh2, RCw, pred, fWNZCV, END_LIST},
    {INVALID   , 0x01700090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldrh   , 0x017000b0, "ldrh"  , RBw, RAw, MN44h, RAw, n8x0_8, pred, x, top4[5][0x0b]},/*PUW=101*/
    {OP_ldrsb  , 0x017000d0, "ldrsb" , RBw, RAw, MN44b, RAw, n8x0_8, pred, x, top4[5][0x0d]},/*PUW=101*/
    {OP_ldrsh  , 0x017000f0, "ldrsh" , RBw, RAw, MN44h, RAw, n8x0_8, pred, x, top4[5][0x0f]},/*PUW=101*/
  }, { /* 20 */
    {OP_orr    , 0x01800000, "orr"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[20][0x01]},
    {OP_orr    , 0x01800010, "orr"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {EXT_BITS8 , 0x01800090, "(ext bits8 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_strh   , 0x018000b0, "strh"  , MPRh, xx, RBh, xx, xx, pred, x, top4[2][0x0b]},/*PUW=110*/
    {OP_ldrd   , 0x018000d0, "ldrd"  , RBEw, RB2w, MPRd, xx, xx, xop_wb|pred, x, top4[2][0x0d]},/*PUW=110*/
    {OP_strd   , 0x018000f0, "strd"  , MPRd, xx, RBEw, RB2w, xx, xop_wb|pred, x, top4[2][0x0f]},/*PUW=110*/
  }, { /* 21 */
    {OP_orrs   , 0x01900000, "orrs"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[21][0x01]},
    {OP_orrs   , 0x01900010, "orrs"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {EXT_BITS8 , 0x01900090, "(ext bits8 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_ldrh   , 0x019000b0, "ldrh"  , RBw, xx, MPRh, xx, xx, pred, x, top4x[16][0x03]},/*PUW=110*/
    {OP_ldrsb  , 0x019000d0, "ldrsb" , RBw, xx, MPRb, xx, xx, pred, x, top4x[16][0x04]},/*PUW=110*/
    {OP_ldrsh  , 0x019000f0, "ldrsh" , RBw, xx, MPRh, xx, xx, pred, x, top4x[16][0x05]},/*PUW=110*/
  }, { /* 22 */
    {OP_bic    , 0x01c00000, "bic"   , RBw, RAw, RDw, sh2, i5, pred|srcX4, x, top4x[22][0x01]},
    {OP_bic    , 0x01c00010, "bic"   , RBw, RAw, RDw, sh2, RCw, pred|srcX4, x, END_LIST},
    {EXT_BITS8 , 0x01b00090, "(ext bits8 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_strh   , 0x01c000b0, "strh"  , MP44h, xx, RBw, xx, xx, pred, x, top4x[20][0x03]},/*PUW=110*/
    {OP_ldrd   , 0x01c000d0, "ldrd"  , RBEw, RB2w, MP44d, xx, xx, pred, x, top4x[20][0x04]},/*PUW=110*/
    {OP_strd   , 0x01c000f0, "strd"  , MP44d, xx, RBEw, RB2w, xx, pred, x, top4x[20][0x05]},/*PUW=110*/
  }, { /* 23 */
    {OP_bics   , 0x01d00000, "bics"  , RBw, RAw, RDw, sh2, i5, pred|srcX4, fWNZCV, top4x[23][0x01]},
    {OP_bics   , 0x01d00010, "bics"  , RBw, RAw, RDw, sh2, RCw, pred|srcX4, fWNZCV, END_LIST},
    {EXT_BITS8 , 0x01d00090, "(ext bits8 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_ldrh   , 0x01d000b0, "ldrh"  , RBw, xx, MP44h, xx, xx, pred, x, top4x[18][0x03]},/*PUW=110*/
    {OP_ldrsb  , 0x01d000d0, "ldrsb" , RBw, xx, MP44b, xx, xx, pred, x, top4x[18][0x04]},/*PUW=110*/
    {OP_ldrsh  , 0x01d000f0, "ldrsh" , RBw, xx, MP44h, xx, xx, pred, x, top4x[18][0x05]},/*PUW=110*/
  }, { /* 24 */
    {OP_mvn    , 0x01e00000, "mvn"   , RBw, xx, RDw, sh2, i5, pred, x, top4x[24][0x01]},
    {OP_mvn    , 0x01e00010, "mvn"   , RBw, xx, RDw, sh2, RCw, pred, x, END_LIST},
    {EXT_BITS8 , 0x01e00090, "(ext bits8 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_strh   , 0x01e000b0, "strh"  , MP44h, RAw, RBw, RAw, i8x0_8, pred, x, top4[3][0x0b]},/*PUW=111*/
    {OP_ldrd   , 0x01e000d0, "ldrd"  , RBEw, RB2w, RAw, MP44d, i8x0_8, xop_wb|pred|dstX3, x, top4[3][0x0d]},/*PUW=111*/
    {OP_strd   , 0x01e000f0, "strd"  , MP44d, RAw, RBw, RAw, i8x0_8, pred, x, top4[3][0x0f]},/*PUW=111*/
  }, { /* 25 */
    {OP_mvns   , 0x01f00000, "mvns"  , RBw, xx, RDw, sh2, i5, pred, fWNZCV, top4x[25][0x01]},
    {OP_mvns   , 0x01f00010, "mvns"  , RBw, xx, RDw, sh2, RCw, pred, fWNZCV, END_LIST},
    {EXT_BITS8 , 0x01f00090, "(ext bits8 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_ldrh   , 0x01f000b0, "ldrh"  , RBw, RAw, MP44h, RAw, i8x0_8, pred, x, top4x[19][0x03]},/*PUW=111*/
    {OP_ldrsb  , 0x01f000d0, "ldrsb" , RBw, RAw, MP44b, RAw, i8x0_8, pred, x, top4x[19][0x04]},/*PUW=111*/
    {OP_ldrsh  , 0x01f000f0, "ldrsh" , RBw, RAw, MP44h, RAw, i8x0_8, pred, x, top4x[19][0x05]},/*PUW=111*/
  },
};

/* Indexed by bits 7:4 but in the following manner:
 * + If bit 4 == 0, take entry 0;
 * + Else, take entry 1 + bits 7:5
 */
const instr_info_t A32_ext_opc4y[][9] = {
  { /* 0 */
    {OP_ldr    , 0x06100000, "ldr"   , RBw, RAw, Mw, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {OP_sadd16 , 0x06100f10, "sadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_sasx   , 0x06100f30, "sasx"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_ssax   , 0x06100f50, "ssax"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_ssub16 , 0x06100f70, "ssub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_sadd8  , 0x06100f90, "sadd8" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x061000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x061000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ssub8  , 0x06100ff0, "ssub8" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 1 */
    {OP_strt   , 0x06200000, "strt"  , Mw, RAw, RBw, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_qadd16 , 0x06200f10, "qadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qasx   , 0x06200f30, "qasx"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qsax   , 0x06200f50, "qsax"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qsub16 , 0x06200f70, "qsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_qadd8  , 0x06200f90, "qadd8" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x062000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x062000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_qsub8  , 0x06200ff0, "qsub8" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 2 */
    {OP_ldrt   , 0x06300000, "ldrt"  , RBw, RAw, Mw, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_shadd16, 0x06300f10, "shadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shasx  , 0x06300f30, "shasx" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shsax  , 0x06300f50, "shsax" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shsub16, 0x06300f70, "shsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_shadd8 , 0x06300f90, "shadd8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x063000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x063000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_shsub8 , 0x06300ff0, "shsub8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 3 */
    {OP_ldrb   , 0x06500000, "ldrb"  , RBw, RAw, Mb, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=000*/
    {OP_uadd16 , 0x06500f10, "uadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uasx   , 0x06500f30, "uasx"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_usax   , 0x06500f50, "usax"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_usub16 , 0x06500f70, "usub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uadd8  , 0x06500f90, "uadd8" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x065000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x065000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_usub8  , 0x06500ff0, "usub8" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 4 */
    {OP_strbt  , 0x06600000, "strbt" , Mb, RAw, RBb, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_uqadd16, 0x06600f10, "uqadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqasx  , 0x06600f30, "uqasx" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqsax  , 0x06600f50, "uqsax" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqsub16, 0x06600f70, "uqsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uqadd8 , 0x06600f90, "uqadd8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x066000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x066000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_uqsub8 , 0x06600ff0, "uqsub8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 5 */
    {OP_ldrbt  , 0x06700000, "ldrbt" , RBw, RAw, Mb, RAw, RDNw, xop_shift|pred, x, END_LIST},/*PUW=001*/
    {OP_uhadd16, 0x06700f10, "uhadd16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhasx  , 0x06700f30, "uhasx" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhsax  , 0x06700f50, "uhsax" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhsub16, 0x06700f70, "uhsub16", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_uhadd8 , 0x06700f90, "uhadd8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x067000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x067000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_uhsub8 , 0x06700ff0, "uhsub8", RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
  }, { /* 6 */
    {OP_str    , 0x06800000, "str"   , Mw, RAw, RBw, RAw, RDw, xop_shift|pred, x, top8[0x60]},/*PUW=010*/
    {OP_pkhbt  , 0x06800010, "pkhbt" , RBw, RAh, RDt, LSL, i5, pred|srcX4, x, top4y[6][0x03]},
    {INVALID   , 0x06800030, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_pkhbt  , 0x06800050, "pkhtb" , RBw, RAt, RDh, ASR, i5, pred|srcX4, x, top4y[6][0x05]},
    {OP_sxtab16, 0x06800070, "sxtab16", RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* XXX: "sxtb16" on PC */ /* rotates RDw then extracts 2 8-bit parts: model as reading whole thing, for now at least */
    {OP_pkhbt  , 0x06800090, "pkhbt" , RBw, RAh, RDt, LSL, i5, pred|srcX4, x, top4y[6][0x07]},
    {OP_sel    , 0x06800fb0, "sel"   , RBw, xx, RAw, RDw, xx, pred, fRGE, END_LIST},
    {OP_pkhbt  , 0x068000d0, "pkhtb" , RBw, RAt, RDh, ASR, i5, pred|srcX4, x, END_LIST},
    {INVALID   , 0x068000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 7 */
    {OP_strt   , 0x06a00000, "strt"  , Mw, RAw, RBw, RAw, RDw, xop_shift|pred, x, top4y[1][0x00]},/*PUW=011*/
    {OP_ssat   , 0x06a00010, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[7][0x03]},
    {OP_ssat16 , 0x06a00f30, "ssat16", RBw, xx, i4_16, RDw, xx, pred, x, END_LIST},
    {OP_ssat   , 0x06a00050, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[7][0x05]},
    {OP_sxtab  , 0x06a00070, "sxtab" , RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* XXX: "sxtb" on PC */ /* rotates RDw then extracts 8 bits: model as reading whole thing, for now at least */
    {OP_ssat   , 0x06a00090, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[7][0x07]},
    {INVALID   , 0x06a000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ssat   , 0x06a000d0, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[8][0x01]},
    {INVALID   , 0x06a000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    {OP_ldrt   , 0x06b00000, "ldrt"  , RBw, RAw, Mw, RAw, RDw, xop_shift|pred, x, top4y[2][0x00]},/*PUW=011*/
    {OP_ssat   , 0x06b00010, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[8][0x03]},
    {OP_rev    , 0x06bf0f30, "rev"   , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_ssat   , 0x06b00050, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[8][0x05]},
    {OP_sxtah  , 0x06b00070, "sxtah" , RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* XXX: "sxth" on PC */ /* rotates RDw then extracts 8 bits: model as reading whole thing, for now at least */
    {OP_ssat   , 0x06b00090, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[8][0x07]},
    {OP_rev16  , 0x06bf0fb0, "rev16" , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_ssat   , 0x06b000d0, "ssat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, END_LIST},
    {INVALID   , 0x06b000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_strb   , 0x06c00000, "strb"  , Mb, RAw, RBb, RAw, RDw, xop_shift|pred, x, top8[0x64]},/*PUW=010*/
    {INVALID   , 0x06c00010, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x06c00030, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x06c00050, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_uxtab16, 0x06c00070, "uxtab16", RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* XXX: "uxtb16" on PC */ /* rotates RDw then extracts 2x8 bits: model as reading whole thing, for now at least */
    {INVALID   , 0x06c00090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x06c000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x06c000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x06c000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 10 */
    {OP_strbt  , 0x06e00000, "strbt" , Mb, RAw, RBb, RAw, RDw, xop_shift|pred, x, top4y[4][0x00]},/*PUW=011*/
    {OP_usat   , 0x06e00010, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[10][0x03]},
    {OP_usat16 , 0x06e00f30, "usat16", RBw, xx, i4_16, RDw, xx, pred, x, END_LIST},
    {OP_usat   , 0x06e00050, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[10][0x05]},
    {OP_uxtab  , 0x06e00070, "uxtab" , RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* XXX: "uxtb" on PC */ /* rotates RDw then extracts 8 bits: model as reading whole thing, for now at least */
    {OP_usat   , 0x06e00090, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[10][0x07]},
    {INVALID   , 0x06e000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_usat   , 0x06e000d0, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[11][0x01]},
    {INVALID   , 0x06e000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 11 */
    {OP_ldrbt  , 0x06f00000, "ldrbt" , RBw, RAw, Mb, RAw, RDw, xop_shift|pred, x, top4y[5][0x00]},/*PUW=011*/
    {OP_usat   , 0x06f00010, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[11][0x03]},
    {OP_rbit   , 0x06ff0f30, "rbit"  , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_usat   , 0x06f00050, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[11][0x05]},
    {OP_uxtah  , 0x06f00070, "uxtah" , RBw, xx, RAw, RDw, ro2, pred, x, END_LIST},/* XXX: "uxth" on PC */ /* rotates RDw then extracts 16 bits: model as reading whole thing, for now at least */
    {OP_usat   , 0x06f00090, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, top4y[11][0x07]},
    {OP_revsh  , 0x06ff0fb0, "revsh" , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_usat   , 0x06f000d0, "usat"  , RBw, i5_16, RDw, sh1, i5, pred|srcX4, x, END_LIST},
    {INVALID   , 0x06f000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_str    , 0x07000000, "str"   , MNSw, xx, RBw, xx, xx, pred, x, top8[0x5a]},/*PUW=100*/
    {OP_smlad  , 0x07000010, "smlad" , RAw, xx, RDw, RCw, RBw, pred, fWQ, END_LIST},/* XXX: "smuad" on PC */
    {OP_smladx , 0x07000030, "smladx", RAw, xx, RDw, RCw, RBw, pred, fWQ, END_LIST},/* XXX: "smuad" on PC */
    {OP_smlsd  , 0x07000050, "smlsd" , RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},/* XXX: "smusd" on PC */
    {OP_smlsdx , 0x07000070, "smlsdx", RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},/* XXX: "smusd" on PC */
    {INVALID   , 0x07000090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x070000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x070000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x070000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 13 */
    {OP_strb   , 0x07400000, "strb"  , MNSb, xx, RBb, xx, xx, pred, x, top8[0x5e]},/*PUW=100*/
    {OP_smlald , 0x07400010, "smlald", RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_smlaldx, 0x07400030, "smlaldx",RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_smlsld , 0x07400050, "smlsld", RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {OP_smlsldx, 0x07400070, "smlsldx",RAw, RBw, RDw, RCw, xx, pred, x, END_LIST},
    {INVALID   , 0x07400090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x074000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x074000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x074000f0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 14 */
    {OP_ldrb   , 0x07500000, "ldrb"  , RBw, xx, MNSb, xx, xx, pred, x, top8[0x5f]},/*PUW=100*/
    {OP_smmla  , 0x07500010, "smmla" , RAw, xx, RDw, RCw, RBw, pred, x, top4y[14][0x02]},/* XXX: "smmul" if RBw==PC */
    {OP_smmla  , 0x07500030, "smmla" , RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},/* XXX: "smmul" if RBw==PC */
    {INVALID   , 0x07500050, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x07500070, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x07500090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x075000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_smmls  , 0x075000d0, "smmls" , RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
    {OP_smmlsr , 0x075000f0, "smmlsr", RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},
  },
};

/* Indexed by bits 7:4 */
const instr_info_t A32_ext_opc4[][16] = {
  { /* 0 */
    {EXT_BIT9  , 0x01000000, "(ext bit9 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID   , 0x01000010, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01000020, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01000030, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT9  , 0x01000040, "(ext bit9 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_qadd   , 0x01000050, "qadd"  , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x01000060, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_hlt    , 0xe1000070, "hlt"   , i16x0_8, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_smlabb , 0x01000080, "smlabb", RAw, xx, RDh, RCh, RBw, pred, x, END_LIST},
    {INVALID   , 0x01000090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_smlabt , 0x010000a0, "smlabt", RAw, xx, RDh, RCt, RBw, pred, x, END_LIST},
    {OP_strh   , 0x010000b0, "strh"  , MNRh, xx, RBh, xx, xx, pred, x, top4x[24][0x03]},/*PUW=100*/
    {OP_smlatb , 0x010000c0, "smlatb", RAw, xx, RDt, RCh, RBw, pred, x, END_LIST},
    {OP_ldrd   , 0x010000d0, "ldrd"  , MNRd, xx, RBEw, RB2w, xx, pred, x, top4x[24][0x04]},/*PUW=100*/
    {OP_smlatt , 0x010000e0, "smlatt", RAw, xx, RDt, RCt, RBw, pred, x, END_LIST},
    {OP_strd   , 0x010000f0, "strd"  , MNRd, xx, RBEw, RB2w, xx, pred, x, top4x[24][0x05]},/*PUW=100*/
  }, { /* 1 */
    {EXT_BIT9  , 0x01200000, "(ext bit9 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_bx     , 0x01200010, "bx"    , xx, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_bxj    , 0x01200020, "bxj"   , xx, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_blx_ind, 0x01200030, "blx"   , LRw, xx, RDw, xx, xx, pred, x, END_LIST},
    {EXT_BIT9  , 0x01200040, "(ext bit9 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_qsub   , 0x01200050, "qsub"  , RBw, xx, RDw, RAw, xx, pred, x, END_LIST},
    {INVALID   , 0x01200060, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_bkpt   , 0xe1200070, "bkpt"  , i16x0_8, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_smlawb , 0x01200080, "smlawb", RAw, xx, RDh, RCh, RBw, pred, x, END_LIST},
    {INVALID   , 0x01200090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_smulwb , 0x012000a0, "smulwb", RAw, xx, RDw, RCh, xx, pred, x, END_LIST},
    {OP_strh   , 0x012000b0, "strh"  , MNRh, RAw, RBw, RAw, RDNw, pred, x, top4x[12][0x03]},/*PUW=101*/
    {OP_smlawt , 0x012000c0, "smlawt", RAw, xx, RDt, RCt, RBw, pred, x, END_LIST},
    {OP_ldrd   , 0x012000d0, "ldrd"  , RBEw, RB2w, RAw, MNRd, RDNw, xop_wb|pred|dstX3, x, top4x[14][0x04]},/*PUW=101*/
    {OP_smulwt , 0x012000e0, "smulwt", RAw, xx, RDw, RCt, xx, pred, x, END_LIST},
    {OP_strd   , 0x012000f0, "strd"  , MNRd, RAw, RBEw, RB2w, RDNw, xop_wb|pred, x, top4x[12][0x05]},/*PUW=101*/
  }, { /* 2 */
    {EXT_BIT9  , 0x01400000, "(ext bit9 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID   , 0x01400010, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01400020, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01400030, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT9  , 0x01400040, "(ext bit9 5)", xx, xx, xx, xx, xx, no, x, 5},
    {OP_qdadd  , 0x01400050, "qdadd" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {INVALID   , 0x01400060, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_hvc    , 0x01400070, "hvc"   , i16x0_8, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_smlalbb, 0x01400080, "smlalbb", RAw, RBw, RAw, RBw, RCh, pred|xop, x, exop[0x4]},
    {INVALID   , 0x01400090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_smlalbt, 0x014000a0, "smlalbt", RAw, RBw, RAw, RBw, RCh, pred|xop, x, exop[0x5]},
    {OP_strh   , 0x014000b0, "strh"  , MN44h, xx, RBw, xx, xx, pred, x, top4[0][0x0b]},/*PUW=100*/
    {OP_smlaltb, 0x014000c0, "smlaltb", RAw, RBw, RAw, RBw, RCt, pred|xop, x, exop[0x4]},
    {OP_ldrd   , 0x014000d0, "ldrd"  , RBEw, RB2w, MN44d, xx, xx, pred, x, top4[0][0x0d]},/*PUW=100*/
    {OP_smlaltt, 0x014000e0, "smlaltt", RAw, RBw, RAw, RBw, RCt, pred|xop, x, exop[0x5]},
    {OP_strd   , 0x014000f0, "strd"  , MN44d, xx, RBEw, RB2w, xx, pred, x, top4[0][0x0f]},/*PUW=100*/
  }, { /* 3 */
    {EXT_BIT9  , 0x01600000, "(ext bit9 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_clz    , 0x016f0f10, "clz"   , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01600020, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01600030, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BIT9  , 0x01600040, "(ext bit9 7)", xx, xx, xx, xx, xx, no, x, 7},
    {OP_qdsub  , 0x01600050, "qdsub" , RBw, xx, RAw, RDw, xx, pred, x, END_LIST},
    {OP_eret   , 0x0160006e, "eret"  , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01600070, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_smulbb , 0x01600080, "smulbb", RAw, xx, RCh, RDh, xx, pred, x, END_LIST},
    {INVALID   , 0x01600090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_smulbt , 0x016000a0, "smulbt", RAw, xx, RCh, RDt, xx, pred, x, END_LIST},
    {OP_strh   , 0x016000b0, "strh"  , MN44h, RAw, RBh, RAw, n8x0_8, pred, x, top4[4][0x0b]},/*PUW=101*/
    {OP_smultb , 0x016000c0, "smultb", RAw, xx, RCt, RDh, xx, pred, x, END_LIST},
    {OP_ldrd   , 0x016000d0, "ldrd"  , RBEw, RB2w, RAw, MN44d, n8x0_8, xop_wb|pred|dstX3, x, top4[4][0x0d]},/*PUW=101*/
    {OP_smultt , 0x016000e0, "smultt", RAw, xx, RCt, RDt, xx, pred, x, END_LIST},
    {OP_strd   , 0x016000f0, "strd"  , MN44d, RAw, RBEw, RB2w, n8x0_8, xop_wb|pred, x, top4[4][0x0f]},/*PUW=101*/
  }, { /* 4 */
    {EXT_IMM5  , 0x01a00000, "(ext imm5 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_lsl    , 0x01a00010, "lsl"   , RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_lsr    , 0x01a00020, "lsr"   , RBw, xx, RDw, i5, xx, pred, x, top4[4][0x0a]},
    {OP_lsr    , 0x01a00030, "lsr"   , RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_asr    , 0x01a00040, "asr"   , RBw, xx, RDw, i5, xx, pred, x, top4[4][0x0c]},
    {OP_asr    , 0x01a00050, "asr"   , RBw, xx, RDw, RAw, xx, pred, x, top4[4][0xc]},
    {EXT_IMM5  , 0x01a00060, "(ext imm5 1)", xx, xx, xx, xx, xx, no, x, 1},
    {OP_ror    , 0x01a00070, "ror"   , RBw, xx, RDw, RCw, xx, pred, x, END_LIST},
    {OP_lsl    , 0x01a00080, "lsl"   , RBw, xx, RDw, i5, xx, pred, x, ti5[0][0x01]},
    {EXT_BITS8 , 0x01a00090, "(ext bits8 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_lsr    , 0x01a000a0, "lsr"   , RBw, xx, RDw, i5, xx, pred, x, top4[4][0x03]},
    {OP_strh   , 0x01a000b0, "strh"  , MPRh, RAw, RBh, RAw, RDw, pred, x, top4[1][0x0b]},/*PUW=111*/
    {OP_asr    , 0x01a000c0, "asr"   , RBw, xx, RDw, i5, xx, pred, x, top4[4][0x05]},
    {OP_ldrd   , 0x01a000d0, "ldrd"  , RBEw, RB2w, RAw, MPRd, RDw, xop_wb|pred|dstX3, x, top4[1][0x0d]},/*PUW=111*/
    {OP_ror    , 0x01a000e0, "ror"   , RBw, xx, RDw, i5, xx, pred, x, ti5[1][0x01]},
    {OP_strd   , 0x01a000f0, "strd"  , MPRd, RAw, RBEw, RB2w, RDw, xop_wb|pred, x, top4[1][0x0f]},/*PUW=111*/
  }, { /* 5 */
    {EXT_IMM5  , 0x01b00000, "(ext imm5 2)", xx, xx, xx, xx, xx, no, x, 2},
    {OP_lsls   , 0x01b00010, "lsls"  , RBw, xx, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_lsrs   , 0x01b00020, "lsrs"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, top4[5][0x0a]},
    {OP_lsrs   , 0x01b00030, "lsrs"  , RBw, xx, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_asrs   , 0x01b00040, "asrs"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, top4[5][0x0c]},
    {OP_asrs   , 0x01b00050, "asrs"  , RBw, xx, RDw, RAw, xx, pred, fWNZCV, top4[5][0xc]},
    {EXT_IMM5  , 0x01b00060, "(ext imm5 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_rors   , 0x01b00070, "rors"  , RBw, xx, RDw, RCw, xx, pred, fWNZCV, END_LIST},
    {OP_lsls   , 0x01b00080, "lsls"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, ti5[2][0x01]},
    {EXT_BITS8 , 0x01b00090, "(ext bits8 3)", xx, xx, xx, xx, xx, no, x, 3},
    {OP_lsrs   , 0x01b000a0, "lsrs"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, top4[5][0x03]},
    {OP_ldrh   , 0x01b000b0, "ldrh"  , RBw, RAw, MPRh, RAw, RDw, pred, x, top4x[17][0x03]},/*PUW=111*/
    {OP_asrs   , 0x01b000c0, "asrs"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, top4[5][0x05]},
    {OP_ldrsb  , 0x01b000d0, "ldrsb" , RBw, RAw, MPRb, RAw, RDw, pred, x, top4x[17][0x04]},/*PUW=111*/
    {OP_rors   , 0x01b000e0, "rors"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, ti5[3][0x01]},
    {OP_ldrsh  , 0x01b000f0, "ldrsh" , RBw, RAw, MPRh, RAw, RDw, pred, x, top4x[17][0x05]},/*PUW=111*/
  }, { /* 6 */
    {EXT_BITS0 , 0x03200000, "(ext bits0 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID   , 0x03200010, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200020, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200030, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200040, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200050, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200060, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200070, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200080, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200090, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x032000a0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x032000b0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x032000c0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x032000d0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x032000e0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_dbg    , 0x0320f0f0, "dbg"   , i4, xx, xx, xx, xx, pred, x, END_LIST},
  }, { /* 7 */
    {OP_vmov_f32,0x0eb00a00, "vmov.f32", WBd, xx, i8x0_16, xx, xx, pred|vfp, x, top4[9][0x00]},
    {INVALID   , 0x0eb00a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00a20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00a30, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00a40, "(ext bits16 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID   , 0x0eb00a50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00a60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00a70, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmp_f32,0x0eb50a80, "vcmp.f32", FPSCR, xx, WBd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID   , 0x0eb00a90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00aa0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00ab0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00ac0, "(ext bits16 1)", xx, xx, xx, xx, xx, no, x, 1},
    {INVALID   , 0x0eb00ad0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00ae0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00af0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 8 */
    {OP_vmov_f64,0x0eb00b00, "vmov.f64", VBq, xx, i8x0_16, xx, xx, pred|vfp, x, top4[10][0x00]},
    {INVALID   , 0x0eb00b10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16,0x0eb00b30, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {EXT_BITS16, 0x0eb00b40, "(ext bits16 2)", xx, xx, xx, xx, xx, no, x, 2},
    {INVALID   , 0x0eb00b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16,0x0eb00b70, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vcmp_f64,0x0eb50b80, "vcmp.f64", FPSCR, xx, VBq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   , 0x0eb00b90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00ba0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00bb0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0eb00bc0, "(ext bits16 3)", xx, xx, xx, xx, xx, no, x, 3},
    {INVALID   , 0x0eb00bd0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00be0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0eb00bf0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vmov_f32,0x0ef00a00, "vmov.f32", WBd, xx, i8x0_16, xx, xx, pred|vfp, x, t16[0][0x00]},
    {EXT_RBPC,   0x0ef00a10, "(ext rbpc 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID   , 0x0ef00a20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00a30, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00a40, "(ext bits16 4)", xx, xx, xx, xx, xx, no, x, 4},
    {INVALID   , 0x0ef00a50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00a60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00a70, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmp_f32,0x0ef50a80, "vcmp.f32", FPSCR, xx, WBd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID   , 0x0ef00a90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00aa0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00ab0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00ac0, "(ext bits16 5)", xx, xx, xx, xx, xx, no, x, 5},
    {INVALID   , 0x0ef00ad0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00ae0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00af0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 10 */
    {OP_vmov_f64,0x0ef00b00, "vmov.f64", VBq, xx, i8x0_16, xx, xx, pred|vfp, x, t16[2][0x00]},
    {OP_vmov_u8, 0x0ef00b10, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID   , 0x0ef00b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0x0ef00b30, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {EXT_BITS16, 0x0ef00b40, "(ext bits16 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_vmov_u8, 0x0ef00b50, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID   , 0x0ef00b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8, 0x0ef00b70, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vcmp_f64,0x0ef50b80, "vcmp.f64", FPSCR, xx, VBq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   , 0x0ef00b90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00ba0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00bb0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_BITS16, 0x0ef00bc0, "(ext bits16 7)", xx, xx, xx, xx, xx, no, x, 7},
    {INVALID   , 0x0ef00bd0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00be0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x0ef00bf0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by whether imm4 in 19:16 is zero or not */
const instr_info_t A32_ext_imm1916[][2] = {
  { /* 0 */
    {EXT_OPC4  , 0x03200000, "(ext opc4 6)", xx, xx, xx, xx, xx, no, x, 6},
    {OP_msr    , 0x0320f000, "msr"   , CPSR, xx, i4_16, i12, xx, pred, x, tb9[2][0x00]},
  },
};

/* Indexed by bits 2:0 */
const instr_info_t A32_ext_bits0[][8] = {
  { /* 0 */
    {OP_nop    , 0x0320f000, "nop"   , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_yield  , 0x0320f001, "yield" , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_wfe    , 0x0320f002, "wfe"   , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_wfi    , 0x0320f003, "wfi"   , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_sev    , 0x0320f004, "sev"   , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {OP_sevl   , 0x0320f005, "sevl"  , xx, xx, xx, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x03200006, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x03200007, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 9:8 */
const instr_info_t A32_ext_bits8[][4] = {
  { /* 0 */
    {OP_stl    , 0x0180fc90, "stl"   , Mw, xx, RDw, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01800d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlex  , 0x01800e90, "stlex" , Mw, RBw, RDw, xx, xx, pred, x, END_LIST},
    {OP_strex  , 0x01800f90, "strex" , Mw, RBw, RDw, xx, xx, pred, x, END_LIST},
  }, { /* 1 */
    {OP_lda    , 0x01900c9f, "lda"   , RBw, xx, Mw, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01900d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaex  , 0x01900e9f, "ldaex" , RBw, xx, Mw, xx, xx, pred, x, END_LIST},
    {OP_ldrex  , 0x01900f9f, "ldrex" , RBw, xx, Mw, xx, xx, pred, x, END_LIST},
  }, { /* 2 */
    {INVALID   , 0x01a00c90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01a00d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexd , 0x01a00e90, "stlexd", Md, RBw, RDEw, RD2w, xx, pred, x, END_LIST},
    {OP_strexd , 0x01a00f90, "strexd", Md, RBw, RDEw, RD2w, xx, pred, x, END_LIST},
  }, { /* 3 */
    {INVALID   , 0x01b00c90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   , 0x01b00d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexd , 0x01b00e9f, "ldaexd", RBEw, RB2w, Md, xx, xx, pred, x, END_LIST},
    {OP_ldrexd , 0x01b00f9f, "ldrexd", RBEw, RB2w, Md, xx, xx, pred, x, END_LIST},
  }, { /* 4 */
    {OP_stlb   , 0x01c00c90, "stlb"  , Mb, xx, RDb, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01c00d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexb , 0x01c00e90, "stlexb", Mb, RBw, RDb, xx, xx, pred, x, END_LIST},
    {OP_strexb , 0x01c00f90, "strexb", Mb, RBw, RDb, xx, xx, pred, x, END_LIST},
  }, { /* 5 */
    {OP_ldab   , 0x01d00c9f, "ldab"  , RBw, xx, Mb, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01d00d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexb , 0x01d00e9f, "ldaexb", RBw, xx, Mb, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01d00f90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 6 */
    {OP_stlh   , 0x01e0fc90, "stlh"  , Mh, xx, RDh, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01e00d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_stlexh , 0x01e00e90, "stlexh", Mh, RBw, RDh, xx, xx, pred, x, END_LIST},
    {OP_strexh , 0x01e00f90, "strexh", Mh, RBw, RDh, xx, xx, pred, x, END_LIST},
  }, { /* 7 */
    {OP_ldah   , 0x01f00c9f, "ldah"  , RBw, xx, Mh, xx, xx, pred, x, END_LIST},
    {INVALID   , 0x01f00d90, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_ldaexh , 0x01f00e9f, "ldaexh", RBw, xx, Mh, xx, xx, pred, x, END_LIST},
    {OP_ldrexh , 0x01f00f9f, "ldrexh", RBw, xx, Mh, xx, xx, pred, x, END_LIST},
  },
};

/* Indexed by bit 9 */
const instr_info_t A32_ext_bit9[][2] = {
  { /* 0 */
    {OP_mrs    , 0x010f0000, "mrs"   , RBw, xx, CPSR, xx, xx, pred, x, tb9[4][0x00]},
    {OP_mrs    , 0x01000200, "mrs"   , RBw, xx, CPSR, i5x16_9, xx, pred, x, tb9[4][0x01]},
  }, { /* 1 */
    /* XXX: or for crc32 should we model the sz field as some prefix, like the sf for A64? */
    {OP_crc32  , 0x01000040, "crc32b",  RBw, xx, RAw, RDb, xx, predAL|v8, x, tb9[3][0x00]},
    {OP_crc32c , 0x01000240, "crc32cb", RBw, xx, RAw, RDb, xx, predAL|v8, x, tb9[3][0x01]},
  }, { /* 2 */
    {OP_msr    , 0x0120f000, "msr"   , CPSR, xx, i4_16, RAw, xx, pred, x, tb9[2][0x01]},
    {OP_msr    , 0x0120f000, "msr"   , CPSR, xx, i5x16_8, RAw, xx, pred, x, tb9[6][0x00]},
  }, { /* 3 */
    {OP_crc32  , 0x01200040, "crc32h",  RBw, xx, RAw, RDh, xx, predAL|v8, x, tb9[5][0x00]},
    {OP_crc32c , 0x01200240, "crc32ch", RBw, xx, RAw, RDh, xx, predAL|v8, x, tb9[5][0x01]},
  }, { /* 4 */
    {OP_mrs    , 0x014f0000, "mrs"   , RBw, xx, SPSR, xx, xx, pred, x, END_LIST},
    {OP_mrs    , 0x01400200, "mrs"   , RBw, xx, SPSR, i5x16_9, xx, pred, x, tb9[0][0x00]},
  }, { /* 5 */
    {OP_crc32  , 0x01400040, "crc32w",  RBw, xx, RAw, RDw, xx, predAL|v8, x, tb9[7][0x00]},
    {OP_crc32c , 0x01400240, "crc32cw", RBw, xx, RAw, RDw, xx, predAL|v8, x, tb9[7][0x01]},
  }, { /* 6 */
    {OP_msr    , 0x0160f000, "msr"   , SPSR, xx, i4_16, RAw, xx, pred, x, tb9[6][0x01]},
    {OP_msr    , 0x0160f000, "msr"   , SPSR, xx, i5x16_8, RAw, xx, pred, x, END_LIST},
  }, { /* 7 */
    {OP_crc32  , 0x01600040, "crc32w",  RBw, xx, RAw, RDw, xx, predAL|v8|unp, x, END_LIST},
    {OP_crc32c , 0x01600240, "crc32cw", RBw, xx, RAw, RDw, xx, predAL|v8|unp, x, END_LIST},
  },
};

/* Indexed by bit 4 */
const instr_info_t A32_ext_bit4[][2] = {
  { /* 0 */
    {OP_ldr    , 0x07100000, "ldr"   , RBw, xx, MNSw, xx, xx, pred, x, top8[0x5b]},/*PUW=100*/
    {OP_sdiv   , 0x0710f010, "sdiv"  , RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 1 */
    {OP_ldr    , 0x07300000, "ldr"   , RBw, RAw, MNSw, RAw, RDNw, xop_shift|pred, x, top8[0x49]},/*PUW=101*/
    {OP_udiv   , 0x0730f010, "udiv"  , RAw, xx, RDw, RCw, xx, pred, x, END_LIST},
  }, { /* 2 */
    {OP_str    , 0x07800000, "str"   , MPSw, xx, RBw, xx, xx, pred, x, top4y[12][0x00]},/*PUW=110*/
    {OP_usada8 , 0x07800010, "usada8", RAw, xx, RDw, RCw, RBw, pred, x, END_LIST},/* "usad8" on PC */
  }, { /* 3 */
    {OP_str    , 0x07a00000, "str"   , MPSw, RAw, RBw, RAw, RDw, xop_shift|pred, x, top8[0x72]},/*PUW=111*/
    {OP_sbfx   , 0x07a00050, "sbfx"  , RBw, xx, RDw, i5, i5_16, pred, x, tb4[4][0x01]},
  }, { /* 4 */
    {OP_ldr    , 0x07b00000, "ldr"   , RBw, RAw, MPSw, RAw, RDw, xop_shift|pred, x, tb4[1][0x00]},/*PUW=111*/
    {OP_sbfx   , 0x07b00050, "sbfx"  , RBw, xx, RDw, i5, i5_16, pred, x, END_LIST},
  }, { /* 5 */
    {OP_strb   , 0x07e00000, "strb"  , MPSb, RAw, RBb, RAw, RDw, xop_shift|pred, x, top8[0x76]},/*PUW=111*/
    {OP_ubfx   , 0x07e00050, "ubfx"  , RBw, xx, RDw, i5, i5_16, pred, x, tb4[6][0x01]},
  }, { /* 6 */
    {OP_ldrb   , 0x07f00000, "ldrb"  , RBw, RAw, MPSb, RAw, RDw, xop_shift|pred, x, top8[0x77]},/*PUW=111*/
    {OP_ubfx   , 0x07f00050, "ubfx"  , RBw, xx, RDw, i5, i5_16, pred, x, END_LIST},
  }, { /* 7 */
    {INVALID   , 0x07c00000, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {EXT_RDPC  , 0x07c00000, "(ext RDPC 0)", xx, xx, xx, xx, xx, no, x, 0},
  }, { /* 8 */
    {OP_ldrb   , 0x07d00000, "ldrb"  , RBw, xx, MPSb, xx, xx, pred, x, top4y[14][0x00]},/*PUW=110*/
    {OP_bfc    , 0x07d0001f, "bfc"   , RBw, xx, i5_16, i5_7, xx, pred, x, trdpc[0][0x01]},
  }, { /* 9 */
    {OP_cdp    , 0x0e000000, "cdp"   , CRBw, i4_8, i4_20, CRAw, CRDw, pred|xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*//*no chain nec.*/
    {OP_mcr    , 0x0e000010, "mcr"   , CRAw, CRDw, i4_8, i3_21, RBw, pred|xop, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
  }, { /* 10*/
    {OP_cdp    , 0x0e100000, "cdp"   , CRBw, i4_8, i4_20, CRAw, CRDw, pred|xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
    {OP_mrc    , 0x0e100010, "mrc"   , RBw, i4_8, i3_21, CRAw, CRDw, pred|xop|srcX4, x, exop[0x3]},/*XXX: disasm not in dst-src order*/
  },
};

/* Indexed by whether coprocessor (11:8) is:
 * + 0xa   => index 0
 * + 0xb   => index 1
 * + other => index 2
 */
const instr_info_t A32_ext_fp[][3] = {
  { /* 0 */
    {INVALID   , 0x0c200a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID   , 0x0c200b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_stc    , 0x0c200000, "stc"   , MN8w, RAw, i4_8, CRBw, n8, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 1 */
    {INVALID   , 0x0c300a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID   , 0x0c300b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_ldc    , 0x0c300000, "ldc"   , CRBw, RAw, MN8w, i4_8, n8, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 2 */
    {OP_vmov   , 0x0c400a10, "vmov"   ,  WCd, WC2d, RBd, RAd, xx, pred|vfp, x, tfp[2][0x01]},
    {OP_vmov   , 0x0c400b10, "vmov"   ,  VCq, xx, RBd, RAd, xx, pred|vfp, x, tfp[3][0x00]},
    {OP_mcrr   , 0x0c400000, "mcrr"  , CRDw, RAw, RBw, i4_8, i4_7, pred|srcX4, x, END_LIST},
  }, { /* 3 */
    {OP_vmov   , 0x0c500a10, "vmov"   ,  RBd, RAd, WCd, WC2d, xx, pred|vfp, x, tfp[3][0x01]},
    {OP_vmov   , 0x0c500b10, "vmov"   ,  VCq, xx, RBd, RAd, xx, pred|vfp, x, tfpA[0][0x01]},
    {OP_mrrc   , 0x0c500000, "mrrc"  , RBw, RAw, i4_8, i4_7, CRDw, pred, x, END_LIST},
  }, { /* 4 */
    {INVALID   , 0x0c600a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID   , 0x0c600b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_stcl   , 0x0c600000, "stcl"  , MN8w, RAw, i4_8, CRBw, n8, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 5 */
    {INVALID   , 0x0c700a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {INVALID   , 0x0c700b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=001*/
    {OP_ldcl   , 0x0c700000, "ldcl"  , CRBw, RAw, MN8w, i4_8, n8, xop_wb|pred, x, END_LIST},/*PUW=001*/
  }, { /* 6 */
    {OP_vstmia , 0x0c800a00, "vstmia", Ml, xx, WBd, LCd, xx, pred|vfp, x, tfp[6][0x01]},/*PUW=010*/
    {OP_vstmia , 0x0c800b00, "vstmia", Ml, xx, VBq, LCq, xx, pred|vfp, x, tfp[10][0x00]},/*PUW=010*/
    {OP_stc    , 0x0c800000, "stc"   , MP8w, xx, i4_8, CRBw, i8, pred, x, tfp[8][0x02]},/*PUW=010*/
  }, { /* 7 */
    {OP_vldmia , 0x0c900a00, "vldmia", WBd, LCd, Ml, xx, xx, pred|vfp, x, tfp[7][0x01]},/*PUW=010*/
    {OP_vldmia , 0x0c900b00, "vldmia", VBq, LCq, Ml, xx, xx, pred|vfp, x, tfp[11][0x00]},/*PUW=010*/
    {OP_ldc    , 0x0c900000, "ldc"   , CRBw, xx, MP8w, i4_8, i8, pred, x, tfp[9][0x02]},/*PUW=010*/
  }, { /* 8 */
    {OP_vstmia , 0x0ca00a00, "vstmia", Ml, RAw, WBd, LCd, i8, pred|vfp, x, tfp[8][0x01]},/*PUW=011*/
    {OP_vstmia , 0x0ca00b00, "vstmia", Ml, RAw, VBq, LCq, i8, pred|vfp, x, tfp[12][0x00]},/*PUW=011*/
    {OP_stc    , 0x0ca00000, "stc"   , MP8w, RAw, i4_8, CRBw, i8, xop_wb|pred, x, tfp[0][0x02]},/*PUW=011*/
  }, { /* 9 */
    {OP_vldmia , 0x0cb00a00, "vldmia", WBd, LCd, RAw, Ml, i8, pred|vfp|dstX3, x, tfp[9][0x01]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_vldmia , 0x0cb00b00, "vldmia", VBq, LCq, RAw, Ml, i8, pred|vfp|dstX3, x, tfp[13][0x00]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_ldc    , 0x0cb00000, "ldc"   , CRBw, RAw, MP8w, i4_8, i8, xop_wb|pred, x, tfp[1][0x02]},/*PUW=011*/
  }, { /* 10 */
    {OP_vstmia , 0x0cc00a00, "vstmia", Ml, xx, WBd, LCd, xx, pred|vfp, x, END_LIST},/*PUW=010*/
    {OP_vstmia , 0x0cc00b00, "vstmia", Ml, xx, VBq, LCq, xx, pred|vfp, x, END_LIST},/*PUW=010*/
    {OP_stcl   , 0x0cc00000, "stcl"  , MP8w, xx, i4_8, CRBw, i8, pred, x, tfp[12][0x02]},/*PUW=010*/
  }, { /* 11 */
    {OP_vldmia , 0x0cd00a00, "vldmia", WBd, LCd, Ml, xx, xx, pred|vfp, x, END_LIST},/*PUW=010*/
    {OP_vldmia , 0x0cd00b00, "vldmia", VBq, LCq, Ml, xx, xx, pred|vfp, x, END_LIST},/*PUW=010*/
    {OP_ldcl   , 0x0cd00000, "ldcl"  , CRBw, xx, MP8w, i4_8, i8, pred, x, tfp[13][0x02]},/*PUW=010*/
  }, { /* 12 */
    {OP_vstmia , 0x0ce00a00, "vstmia", Ml, RAw, WBd, LCd, i8, pred|vfp, x, tfp[12][0x01]},/*PUW=011*/
    {OP_vstmia , 0x0ce00b00, "vstmia", Ml, RAw, VBq, LCq, i8, pred|vfp, x, tfp[6][0x00]},/*PUW=011*/
    {OP_stcl   , 0x0ce00000, "stcl"  , MP8w, RAw, i4_8, CRBw, i8, xop_wb|pred, x, tfp[4][0x02]},/*PUW=011*/
  }, { /* 13 */
    {OP_vldmia , 0x0cf00a00, "vldmia", WBd, LCd, RAw, Ml, i8, pred|vfp|dstX3, x, tfp[13][0x01]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_vldmia , 0x0cf00b00, "vldmia", VBq, LCq, RAw, Ml, i8, pred|vfp|dstX3, x, tfp[7][0x00]},/*PUW=011*//*XXX: if RA=sp then "vpop"*/
    {OP_ldcl   , 0x0cf00000, "ldcl"  , CRBw, RAw, MP8w, i4_8, i8, xop_wb|pred, x, tfp[5][0x02]},/*PUW=011*/
  }, { /* 14 */
    {OP_vstr   , 0x0d000a00, "vstr"  , MN8d, xx, WBd, xx, xx, pred|vfp, x, tfp[14][0x01]},
    {OP_vstr   , 0x0d000b00, "vstr"  , MN8q, xx, VBq, xx, xx, pred|vfp, x, tfp[18][0x00]},
    {OP_stc    , 0x0d000000, "stc"   , MN8w, xx, i4_8, CRBw, xx, pred, x, tfp[24][0x02]},/*PUW=100*/
  }, { /* 15 */
    {OP_vldr   , 0x0d100a00, "vldr"  , WBd, xx, MN8d, xx, xx, pred|vfp, x, tfp[15][0x01]},
    {OP_vldr   , 0x0d100b00, "vldr"  , VBq, xx, MN8q, xx, xx, pred|vfp, x, tfp[19][0x00]},
    {OP_ldc    , 0x0d100000, "ldc"   , CRBw, xx, MN8w, i4_8, i8, pred, x, tfp[25][0x02]},/*PUW=100*/
  }, { /* 16 */
    {OP_vstmdb , 0x0d200a00, "vstmdb", Ml, RAw, WBd, LCd, n8, pred|vfp, x, tfp[16][0x01]},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_vstmdb , 0x0d200b00, "vstmdb", Ml, RAw, VBq, LCq, n8, pred|vfp, x, tfp[20][0x00]},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_stc    , 0x0d200000, "stc"   , MN8w, RAw, i4_8, CRBw, n8, xop_wb|pred, x, tfp[6][0x02]},/*PUW=101*/
  }, { /* 17 */
    {OP_vldmdb , 0x0d300a00, "vldmdb", WBd, LCd, RAw, Ml, n8, pred|vfp|dstX3, x, tfp[17][0x01]},/*PUW=101*/
    {OP_vldmdb , 0x0d300b00, "vldmdb", VBq, LCq, RAw, Ml, n8, pred|vfp|dstX3, x, tfp[21][0x00]},/*PUW=101*/
    {OP_ldc    , 0x0d300000, "ldc"   , CRBw, RAw, MN8w, i4_8, n8, xop_wb|pred, x, tfp[7][0x02]},/*PUW=101*/
  }, { /* 18 */
    {OP_vstr   , 0x0d400a00, "vstr"  , MN8d, xx, WBd, xx, xx, pred|vfp, x, tfp[18][0x01]},
    {OP_vstr   , 0x0d400b00, "vstr"  , MN8q, xx, VBq, xx, xx, pred|vfp, x, tfp[22][0x00]},
    {OP_stcl   , 0x0d400000, "stcl"  , MN8w, xx, i4_8, CRBw, xx, pred, x, tfp[28][0x02]},/*PUW=100*/
  }, { /* 19 */
    {OP_vldr   , 0x0d500a00, "vldr"  , WBd, xx, MN8d, xx, xx, pred|vfp, x, tfp[19][0x01]},
    {OP_vldr   , 0x0d500b00, "vldr"  , VBq, xx, MN8q, xx, xx, pred|vfp, x, tfp[23][0x00]},
    {OP_ldcl   , 0x0d500000, "ldcl"  , CRBw, xx, MN8w, i4_8, i8, pred, x, tfp[29][0x02]},/*PUW=100*/
  }, { /* 20 */
    {OP_vstmdb , 0x0d600a00, "vstmdb", Ml, RAw, WBd, LCd, n8, pred|vfp, x, END_LIST},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_vstmdb , 0x0d600b00, "vstmdb", Ml, RAw, VBq, LCq, n8, pred|vfp, x, END_LIST},/*PUW=101*//*XXX: if RA=sp then "vpush"*/
    {OP_stcl   , 0x0d600000, "stcl"  , MN8w, RAw, i4_8, CRBw, n8, xop_wb|pred, x, tfp[10][0x02]},/*PUW=101*/
  }, { /* 21 */
    {OP_vldmdb , 0x0d700a00, "vldmdb", WBd, LCd, RAw, Ml, n8, pred|vfp|dstX3, x, END_LIST},/*PUW=101*/
    {OP_vldmdb , 0x0d700b00, "vldmdb", VBq, LCq, RAw, Ml, n8, pred|vfp|dstX3, x, END_LIST},/*PUW=101*/
    {OP_ldcl   , 0x0d700000, "ldcl"  , CRBw, RAw, MN8w, i4_8, n8, xop_wb|pred, x, tfp[11][0x02]},/*PUW=101*/
  }, { /* 22 */
    {OP_vstr   , 0x0d800a00, "vstr"  , MP8d, xx, WBd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vstr   , 0x0d800b00, "vstr"  , MN8q, xx, VBq, xx, xx, pred|vfp, x, END_LIST},
    {OP_stc    , 0x0d800000, "stc"   , MP8w, xx, i4_8, CRBw, i8, pred, x, tfp[14][0x02]},/*PUW=110*/
  }, { /* 23 */
    {OP_vldr   , 0x0d900a00, "vldr"  , WBd, xx, MP8d, xx, xx, pred|vfp, x, END_LIST},
    {OP_vldr   , 0x0d900b00, "vldr"  , VBq, xx, MN8q, xx, xx, pred|vfp, x, END_LIST},
    {OP_ldc    , 0x0d900000, "ldc"   , CRBw, xx, MP8w, i4_8, i8, pred, x, tfp[15][0x02]},/*PUW=110*/
  }, { /* 24 */
    {INVALID   , 0x0da00a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID   , 0x0da00b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_stc    , 0x0da00000, "stc"   , MP8w, RAw, i4_8, CRBw, i8, xop_wb|pred, x, tfp[16][0x02]},/*PUW=111*/
  }, { /* 25 */
    {INVALID   , 0x0db00a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID   , 0x0db00b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_ldc    , 0x0db00000, "ldc"   , CRBw, RAw, MP8w, i4_8, i8, xop_wb|pred, x, tfp[17][0x02]},/*PUW=111*/
  }, { /* 26 */
    {OP_vstr   , 0x0dc00a00, "vstr"  , MP8d, xx, WBd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vstr   , 0x0dc00b00, "vstr"  , MN8q, xx, VBq, xx, xx, pred|vfp, x, END_LIST},
    {OP_stcl   , 0x0dc00000, "stcl"  , MP8w, xx, i4_8, CRBw, i8, pred, x, tfp[18][0x02]},/*PUW=110*/
  }, { /* 27 */
    {OP_vldr   , 0x0dd00a00, "vldr"  , WBd, xx, MP8d, xx, xx, pred|vfp, x, END_LIST},
    {OP_vldr   , 0x0dd00b00, "vldr"  , VBq, xx, MN8q, xx, xx, pred|vfp, x, END_LIST},
    {OP_ldcl   , 0x0dd00000, "ldcl"  , CRBw, xx, MP8w, i4_8, i8, pred, x, tfp[19][0x02]},/*PUW=110*/
  }, { /* 28 */
    {INVALID   , 0x0de00a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID   , 0x0de00b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_stcl   , 0x0de00000, "stcl"  , MP8w, RAw, i4_8, CRBw, i8, xop_wb|pred, x, tfp[20][0x02]},/*PUW=111*/
  }, { /* 29 */
    {INVALID   , 0x0df00a00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {INVALID   , 0x0df00b00, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},/*PUW=111*/
    {OP_ldcl   , 0x0df00000, "ldcl"  , CRBw, RAw, MP8w, i4_8, i8, xop_wb|pred, x, tfp[21][0x02]},/*PUW=111*/
  }, { /* 30 */
    {EXT_FPA   , 0x0e000000, "(ext fpA 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_FPB   , 0x0e000000, "(ext fpB 0)",  xx, xx, xx, xx, xx, no, x, 0},
    {EXT_BIT4  , 0x0e000000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 31 */
    {EXT_FPA   , 0x0e100000, "(ext fpA 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_FPB   , 0x0e100000, "(ext fpB 1)",  xx, xx, xx, xx, xx, no, x, 1},
    {EXT_BIT4  , 0x0e100000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 32 */
    {EXT_FPA   , 0x0e200000, "(ext fpA 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_FPB   , 0x0e200000, "(ext fpB 2)",  xx, xx, xx, xx, xx, no, x, 2},
    {EXT_BIT4  , 0x0e200000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 33 */
    {EXT_FPA   , 0x0e300000, "(ext fpA 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_FPB   , 0x0e300000, "(ext fpB 3)",  xx, xx, xx, xx, xx, no, x, 3},
    {EXT_BIT4  , 0x0e300000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 34 */
    {EXT_FPA   , 0x0e400000, "(ext fpA 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_FPB   , 0x0e400000, "(ext fpB 4)",  xx, xx, xx, xx, xx, no, x, 4},
    {EXT_BIT4  , 0x0e400000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 35 */
    {EXT_FPA   , 0x0e500000, "(ext fpA 5)",  xx, xx, xx, xx, xx, no, x, 5},
    {EXT_FPB   , 0x0e500000, "(ext fpB 5)",  xx, xx, xx, xx, xx, no, x, 5},
    {EXT_BIT4  , 0x0e500000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 36 */
    {EXT_FPA   , 0x0e600000, "(ext fpA 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_FPB   , 0x0e600000, "(ext fpB 6)",  xx, xx, xx, xx, xx, no, x, 6},
    {EXT_BIT4  , 0x0e600000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 37 */
    {EXT_FPA   , 0x0e700000, "(ext fpA 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_FPB   , 0x0e700000, "(ext fpB 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_BIT4  , 0x0e700000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 38 */
    {EXT_FPA   , 0x0e800000, "(ext fpA 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_FPB   , 0x0e800000, "(ext fpB 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_BIT4  , 0x0e800000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 39 */
    {EXT_FPA   , 0x0e900000, "(ext fpA 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_FPB   , 0x0e900000, "(ext fpB 9)",  xx, xx, xx, xx, xx, no, x, 9},
    {EXT_BIT4  , 0x0e900000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 40 */
    {EXT_FPA   , 0x0ea00000, "(ext fpA 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_FPB   , 0x0ea00000, "(ext fpB 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_BIT4  , 0x0ea00000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 41 */
    {EXT_OPC4  , 0x0eb00000, "(ext opc4 7)",  xx, xx, xx, xx, xx, no, x, 7},
    {EXT_OPC4  , 0x0eb00000, "(ext opc4 8)",  xx, xx, xx, xx, xx, no, x, 8},
    {EXT_BIT4  , 0x0eb00000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 42 */
    {EXT_FPA   , 0x0ec00000, "(ext fpA 11)",  xx, xx, xx, xx, xx, no, x, 11},
    {EXT_FPB   , 0x0ec00000, "(ext fpB 11)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_BIT4  , 0x0ec00000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 43 */
    {EXT_FPA   , 0x0ed00000, "(ext fpB 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_FPB   , 0x0ed00000, "(ext fpB 12)",  xx, xx, xx, xx, xx, no, x, 12},
    {EXT_BIT4  , 0x0ed00000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  }, { /* 44 */
    {EXT_FPA   , 0x0ee00000, "(ext fpA 13)",  xx, xx, xx, xx, xx, no, x, 13},
    {EXT_FPB   , 0x0ee00000, "(ext fpB 13)",  xx, xx, xx, xx, xx, no, x, 13},
    {EXT_BIT4  , 0x0ee00000, "(ext bit4 9)",  xx, xx, xx, xx, xx, no, x, 9},
  }, { /* 45 */
    {EXT_OPC4  , 0x0ef00000, "(ext opc4 9)",  xx, xx, xx, xx, xx, no, x,  9},
    {EXT_OPC4  , 0x0ef00000, "(ext opc4 10)",  xx, xx, xx, xx, xx, no, x, 10},
    {EXT_BIT4  , 0x0ef00000, "(ext bit4 10)",  xx, xx, xx, xx, xx, no, x, 10},
  },
};

/* Indexed by bits 6:4 but in the following manner:
 * + If 6:4 == 0, take entry 0;
 * + If 6:4 == 1, take entry 1;
 * + If 6:4 == 4, take entry 2;
 * + Else, invalid.
 */
const instr_info_t A32_ext_opc4fpA[][3] = {
  { /* 0 */
    {OP_vmla_f32, 0x0e000a00, "vmla.f32" , WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vmov   ,  0x0e000a10, "vmov"   ,  WAd, xx, RBd, xx, xx, pred|vfp, x, tfpA[1][0x01]},
    {OP_vmls_f32, 0x0e000a40, "vmls.f32" , WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vnmls_f32,0x0e100a00, "vnmls.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vmov   ,  0x0e100a10, "vmov"   ,  RBd, xx, WAd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vnmla_f32,0x0e100a40, "vnmla.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 2 */
    {OP_vmul_f32, 0x0e200a00, "vmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e200a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f32,0x0e200a40, "vnmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vadd_f32, 0x0e300a00, "vadd.f32", WBd, xx, WAd, VCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e300a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f32, 0x0e300a40, "vsub.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 4 */
    {OP_vmla_f32, 0x0e400a00, "vmla.f32" , WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e400a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmls_f32, 0x0e400a40, "vmls.f32" , WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 5 */
    {OP_vnmls_f32,0x0e500a00, "vnmls.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e500a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmla_f32,0x0e500a40, "vnmla.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 6 */
    {OP_vmul_f32, 0x0e600a00, "vmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e600a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vnmul_f32,0x0e600a40, "vnmul.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 7 */
    {OP_vadd_f32, 0x0e700a00, "vadd.f32", WBd, xx, WAd, VCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e700a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vsub_f32, 0x0e700a40, "vsub.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 8 */
      {OP_vdiv_f32, 0x0e800a00, "vdiv.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e800a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e800a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vfnma_f32,0x0e900a00, "vfnma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e900a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnms_f32,0x0e900a40, "vfnms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 10 */
    {OP_vfma_f32, 0x0ea00a00, "vfma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ea00a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f32, 0x0ea00a40, "vfms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 11 */
    {OP_vdiv_f32, 0x0ec00a00, "vdiv.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ec00a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ec00a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_vfnma_f32,0x0ed00a00, "vfnma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ed00a10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfnms_f32,0x0ed00a40, "vfnms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  }, { /* 13 */
    {OP_vfma_f32, 0x0ee00a00, "vfma.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vmsr   ,  0x0ee10a10, "vmsr"  , FPSCR, xx, RBd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vfms_f32, 0x0ee00a40, "vfms.f32", WBd, xx, WAd, WCd, xx, pred|vfp, x, END_LIST},
  },
};

/* Indexed by bits 6:4 */
const instr_info_t A32_ext_opc4fpB[][8] = {
  { /* 0 */
    {OP_vmla_f64, 0x0e000b00, "vmla.f64" , VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e000b10, "vmov.32",  VAd_q, xx, RBd, i1_21, xx, pred|vfp, x, tfpB[1][0x01]},
    {INVALID    , 0x0e000b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_16,  0x0e000b30, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vmls_f64, 0x0e000b40, "vmls.f64" , VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   ,  0x0e000b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e000b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_16,  0x0e000b70, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vnmls_f64,0x0e100b00, "vnmls.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e100b10, "vmov.32",  RBd, xx, VAd_q, i1_21, xx, pred|vfp, x, tfpB[2][0x01]},
    {INVALID    , 0x0e100b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s16, 0x0e100b30, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vnmla_f64,0x0e100b40, "vnmla.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   ,  0x0e100b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e100b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s16, 0x0e100b70, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
  }, { /* 2 */
    {OP_vmul_f64, 0x0e200b00, "vmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e200b10, "vmov.32",  VAd_q, xx, RBd, i1_21, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e200b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_16,  0x0e200b30, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vnmul_f64,0x0e200b40, "vnmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   ,  0x0e200b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e200b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_16,  0x0e200b70, "vmov.16",  VAh_q, xx, RBh, i2x21_6, xx, pred|vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vadd_f64, 0x0e300b00, "vadd.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_32,  0x0e300b10, "vmov.32",  RBd, xx, VAd_q, i1_21, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e300b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s16, 0x0e300b30, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vsub_f64, 0x0e300b40, "vsub.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   ,  0x0e300b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e300b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s16, 0x0e300b70, "vmov.s16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
  }, { /* 4 */
    {OP_vmla_f64, 0x0e400b00, "vmla.f64" , VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_8 ,  0x0e400b10, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e400b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_8 ,  0x0e400b30, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vmls_f64, 0x0e400b40, "vmls.f64" , VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_8 ,  0x0e400b50, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e400b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_8 ,  0x0e400b70, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
  }, { /* 5 */
    {OP_vnmls_f64,0x0e500b00, "vnmls.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_s8,  0x0e500b10, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e500b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s8,  0x0e500b30, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vnmla_f64,0x0e500b40, "vnmla.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_s8,  0x0e500b50, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e500b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s8,  0x0e500b70, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
  }, { /* 6 */
    {OP_vmul_f64, 0x0e600b00, "vmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_8 ,  0x0e600b10, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e600b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_8 ,  0x0e600b30, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vnmul_f64,0x0e600b40, "vnmul.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_8 ,  0x0e600b50, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e600b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_8 ,  0x0e600b70, "vmov.8" ,  VAb_q, xx, RBb, i3x21_5, xx, pred|vfp, x, END_LIST},
  }, { /* 7 */
    {OP_vadd_f64, 0x0e700b00, "vadd.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_s8,  0x0e700b10, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e700b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s8,  0x0e700b30, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vsub_f64, 0x0e700b40, "vsub.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_s8,  0x0e700b50, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e700b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_s8,  0x0e700b70, "vmov.s8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
  }, { /* 8 */
    {OP_vdiv_f64, 0x0e800b00, "vdiv.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vdup_32,  0x0e800b10, "vdup.32",  WAd, xx, RBd, xx, xx, pred|vfp, x, tfpB[10][0x01]},
    {INVALID    , 0x0e800b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_16,  0x0e800b30, "vdup.16",  WAd, xx, RBh, xx, xx, pred|vfp, x, tfpB[10][0x03]},
    {INVALID    , 0x0e800b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e800b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e800b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e800b70, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 9 */
    {OP_vfnma_f64,0x0e900b00, "vfnma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID   ,  0x0e900b10, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e900b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16, 0x0e900b30, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
    {OP_vfnms_f64,0x0e900b40, "vfnms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e900b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0e900b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u16, 0x0e900b70, "vmov.u16", RBd, xx, VAh_q, i2x21_6, xx, pred|vfp, x, END_LIST},
  }, { /* 10 */
    {OP_vfma_f64, 0x0ea00b00, "vfma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vdup_32,  0x0ea00b10, "vdup.32",  VAq, xx, RBd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0e000b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vdup_16,  0x0ea00b30, "vdup.16",  VAq, xx, RBh, xx, xx, pred|vfp, x, END_LIST},
    {OP_vfms_f64, 0x0ea00b40, "vfms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ea00b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ea00b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ea00b70, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 11 */
    {OP_vdiv_f64, 0x0ec00b00, "vdiv.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vdup_8,   0x0ec00b10, "vdup.8",   WAd, xx, RBb, xx, xx, pred|vfp, x, tfpB[13][0x01]},
    {INVALID    , 0x0ec00b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   ,  0x0ec00b30, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ec00b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ec00b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ec00b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ec00b70, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  }, { /* 12 */
    {OP_vfnma_f64,0x0ed00b00, "vfnma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_u8,  0x0ed00b10, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ed00b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8,  0x0ed00b30, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {OP_vfnms_f64,0x0ed00b40, "vfnms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vmov_u8,  0x0ed00b50, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ed00b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmov_u8,  0x0ed00b70, "vmov.u8",  RBd, xx, VAb_q, i3x21_5, xx, pred|vfp, x, END_LIST},
  }, { /* 13 */
    {OP_vfma_f64, 0x0ee00b00, "vfma.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vdup_8,   0x0ee00b10, "vdup.8",   VAq, xx, RBb, xx, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ee00b20, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID   ,  0x0ee00b30, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vfms_f64, 0x0ee00b40, "vfms.f64", VBq, xx, VAq, VCq, xx, pred|vfp, x, END_LIST},
    {INVALID    , 0x0ee00b50, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ee00b60, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID    , 0x0ee00b70, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
  },
};

/* Indexed by bits 19:16 */
const instr_info_t A32_ext_bits16[][16] = {
  { /* 0 */
    {OP_vmov_f32,     0x0eb00a40, "vmov.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vneg_f32,     0x0eb10a40, "vneg.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb20a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb30a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb40a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb50a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintr_f32,   0x0eb60a40, "vrintr.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vrintx_f32,   0x0eb70a40, "vrintx.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u32, 0x0eb80a40, "vcvt.f32.u32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb90a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s16, 0x0eba0a40, "vcvt.f32.s16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u16, 0x0ebb0a40, "vcvt.f32.u16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_u32_f32,0x0ebc0a40, "vcvtr.u32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_s32_f32,0x0ebd0a40, "vcvtr.s32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s16_f32, 0x0ebe0a40, "vcvt.s16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u16_f32, 0x0ebf0a40, "vcvt.u16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 1 */
    {OP_vabs_f32,     0x0eb00ac0, "vabs.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vsqrt_f32,    0x0eb10ac0, "vsqrt.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb20ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb30ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb40ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f32,    0x0eb50ac0, "vcmpe.f32",FPSCR, xx, WBd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vrintz_f32,   0x0eb60ac0, "vrintz.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_f32, 0x0eb70ac0, "vcvt.f64.f32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_s32, 0x0eb80ac0, "vcvt.f32.s32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb90ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32, 0x0eba0ac0, "vcvt.f32.s32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[1][0x08]},
    {OP_vcvt_f32_u32, 0x0ebb0ac0, "vcvt.f32.u32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[0][0x08]},
    {OP_vcvt_u32_f32, 0x0ebc0ac0, "vcvt.u32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0x0ebd0ac0, "vcvt.s32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0x0ebe0ac0, "vcvt.s32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[1][0x0d]},
    {OP_vcvt_u32_f32, 0x0ebf0ac0, "vcvt.u32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[1][0x0c]},
  }, { /* 2 */
    {OP_vmov_f64,     0x0eb00b40, "vmov.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vneg_f64,     0x0eb10b40, "vneg.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb20b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb30b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb40b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb50b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintr_f64,   0x0eb60b40, "vrintr.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vrintx_f64,   0x0eb70b40, "vrintx.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u32, 0x0eb80b40, "vcvt.f64.u32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb90b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s16, 0x0eba0b40, "vcvt.f64.s16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u16, 0x0ebb0b40, "vcvt.f64.u16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_u32_f64,0x0ebc0b40, "vcvtr.u32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_s32_f64,0x0ebd0b40, "vcvtr.s32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s16_f64, 0x0ebe0b40, "vcvt.s16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u16_f64, 0x0ebf0b40, "vcvt.u16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 3 */
    {OP_vabs_f64,     0x0eb00bc0, "vabs.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vsqrt_f64,    0x0eb10bc0, "vsqrt.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb20bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb30bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0eb40bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f64,    0x0eb50bc0, "vcmpe.f64",FPSCR, xx, VBq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vrintz_f64,   0x0eb60bc0, "vrintz.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_f64, 0x0eb70bc0, "vcvt.f32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_s32, 0x0eb80bc0, "vcvt.f64.s32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0eb90bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s32, 0x0eba0bc0, "vcvt.f64.s32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[3][0x08]},
    {OP_vcvt_f64_u32, 0x0ebb0bc0, "vcvt.f64.u32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, t16[2][0x08]},
    {OP_vcvt_u32_f64, 0x0ebc0bc0, "vcvt.u32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0x0ebd0bc0, "vcvt.s32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0x0ebe0bc0, "vcvt.s32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, t16[3][0x0d]},
    {OP_vcvt_u32_f64, 0x0ebf0bc0, "vcvt.u32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, t16[3][0x0c]},
  }, { /* 4 */
    {OP_vmov_f32,     0x0ef00a40, "vmov.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vneg_f32,     0x0ef10a40, "vneg.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef20a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef30a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef40a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef50a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintr_f32,   0x0ef60a40, "vrintr.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vrintx_f32,   0x0ef70a40, "vrintx.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u32, 0x0ef80a40, "vcvt.f32.u32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef90a40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s16, 0x0efa0a40, "vcvt.f32.s16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u16, 0x0efb0a40, "vcvt.f32.u16", WBd, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_u32_f32,0x0efc0a40, "vcvtr.u32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_s32_f32,0x0efd0a40, "vcvtr.s32.f32",WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s16_f32, 0x0efe0a40, "vcvt.s16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u16_f32, 0x0eff0a40, "vcvt.u16.f32", WBh, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 5 */
    {OP_vabs_f32,     0x0ef00ac0, "vabs.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vsqrt_f32,    0x0ef10ac0, "vsqrt.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef20ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef30ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef40ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f32,    0x0ef50ac0, "vcmpe.f32",FPSCR, xx, WBd, WCd, xx, pred|vfp, x, END_LIST},
    {OP_vrintz_f32,   0x0ef60ac0, "vrintz.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_f32, 0x0ef70ac0, "vcvt.f64.f32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_s32, 0x0ef80ac0, "vcvt.f32.s32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef90ac0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f32_s32, 0x0efa0ac0, "vcvt.f32.s32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_u32, 0x0efb0ac0, "vcvt.f32.u32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u32_f32, 0x0efc0ac0, "vcvt.u32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0x0efd0ac0, "vcvt.s32.f32", WBd, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f32, 0x0efe0ac0, "vcvt.s32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u32_f32, 0x0eff0ac0, "vcvt.u32.f32", WBd, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 6 */
    {OP_vmov_f64,     0x0ef00b40, "vmov.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vneg_f64,     0x0ef10b40, "vneg.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef20b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef30b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef40b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef50b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrintr_f64,   0x0ef60b40, "vrintr.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vrintx_f64,   0x0ef70b40, "vrintx.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u32, 0x0ef80b40, "vcvt.f64.u32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef90b40, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s16, 0x0efa0b40, "vcvt.f64.s16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u16, 0x0efb0b40, "vcvt.f64.u16", VBq, xx, WCh, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_u32_f64,0x0efc0b40, "vcvtr.u32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvtr_s32_f64,0x0efd0b40, "vcvtr.s32.f64",WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s16_f64, 0x0efe0b40, "vcvt.s16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u16_f64, 0x0eff0b40, "vcvt.u16.f64", WBh, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
  }, { /* 7 */
    {OP_vabs_f64,     0x0ef00bc0, "vabs.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vsqrt_f64,    0x0ef10bc0, "vsqrt.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef20bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef30bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {INVALID        , 0x0ef40bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcmpe_f64,    0x0ef50bc0, "vcmpe.f64",FPSCR, xx, VBq, VCq, xx, pred|vfp, x, END_LIST},
    {OP_vrintz_f64,   0x0ef60bc0, "vrintz.f64", VBq, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f32_f64, 0x0ef70bc0, "vcvt.f32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_s32, 0x0ef80bc0, "vcvt.f64.s32", VBq, xx, WCd, xx, xx, pred|vfp, x, END_LIST},
    {INVALID        , 0x0ef90bc0, "(bad)" , xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvt_f64_s32, 0x0efa0bc0, "vcvt.f64.s32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_f64_u32, 0x0efb0bc0, "vcvt.f64.u32", VBq, xx, WCd, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u32_f64, 0x0efc0bc0, "vcvt.u32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0x0efd0bc0, "vcvt.s32.f64", WBd, xx, VCq, xx, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_s32_f64, 0x0efe0bc0, "vcvt.s32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
    {OP_vcvt_u32_f64, 0x0eff0bc0, "vcvt.u32.f64", WBd, xx, VCq, i5x0_5, xx, pred|vfp, x, END_LIST},
  },
};

/* Indexed by whether RB != PC */
const instr_info_t A32_ext_RBPC[][2] = {
  { /* 0 */
    {OP_vmrs     , 0x0ef10a10, "vmrs"  , RBd, xx, FPSCR, xx, xx, pred|vfp, x, END_LIST},
    {OP_vmrs_apsr, 0x0ef1fa10, "vmrs"  , CPSR, xx, FPSCR, xx, xx, pred|vfp, x, END_LIST},
  },
};

/* Indexed by whether RD != PC */
const instr_info_t A32_ext_RDPC[][2] = {
  { /* 0 */
    {OP_bfi    , 0x07c00010, "bfi"   , RBw, xx, RDw, i5_16, i5_7, pred, x, END_LIST},
    {OP_bfc    , 0x07c0001f, "bfc"   , RBw, xx, i5_16, i5_7, xx, pred, x, END_LIST},
  },
};

/* Indexed by whether imm5 11:7 is zero or not */
const instr_info_t A32_ext_imm5[][2] = {
  { /* 0 */
    {OP_mov    , 0x01a00000, "mov"   , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_lsl    , 0x01a00000, "lsl"   , RBw, xx, RDw, i5, xx, pred, x, top4[4][0x01]},
  }, { /* 1 */
    {OP_rrx    , 0x01a00060, "rrx"   , RBw, xx, RDw, xx, xx, pred, x, END_LIST},
    {OP_ror    , 0x01a00060, "ror"   , RBw, xx, RDw, i5, xx, pred, x, top4[4][0x07]},
  }, { /* 2 */
    {OP_movs   , 0x01b00000, "movs"  , RBw, xx, RDw, xx, xx, pred, fWNZCV, END_LIST},
    {OP_lsls   , 0x01b00000, "lsls"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, top4[5][0x01]},
  }, { /* 3 */
    {OP_rrxs   , 0x01b00060, "rrxs"  , RBw, xx, RDw, xx, xx, pred, fWNZCV, END_LIST},
    {OP_rors   , 0x01b00060, "rors"  , RBw, xx, RDw, i5, xx, pred, fWNZCV, top4[5][0x07]},
  },
};

const instr_info_t A32_nopred_opc8[] = {
    /* FIXME i#1551: add A32_nopred_opc8[] table for top bits 0xf */
};

/****************************************************************************
 * Extra operands beyond the ones that fit into instr_info_t.
 * All cases where we have extra operands are single-encoding-only instructions,
 * so we can have instr_info_t.code point here.
 *
 * XXX: just add more opnd fields, eat cost in data size and src line
 * length, for simpler tables?
 */
const instr_info_t A32_extra_operands[] =
{
    /* 0x00 */
    {OP_CONTD, 0x00000000, "shifted index reg", xx, xx, sh2, i5, xx, no, x, END_LIST},/*xop_shift*/
    {OP_CONTD, 0x00000000, "writeback base src", xx, xx, RAw, xx, xx, no, x, END_LIST},/*xop_wb*/
    {OP_CONTD, 0x00000000, "writeback base + index", xx, xx, RCw, RAw, xx, no, x, END_LIST},/*xop_wb2*/
    {OP_CONTD, 0x00000000, "<cdp/mcr/mrc cont'd>", xx, xx, i3_5, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<smlalxb cont'd>",  xx, xx, RDh, xx, xx, no, x, END_LIST},
    {OP_CONTD, 0x00000000, "<smlalxt cont'd>",  xx, xx, RDt, xx, xx, no, x, END_LIST},
};
