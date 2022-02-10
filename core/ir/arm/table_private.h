/* **********************************************************
 * Copyright (c) 2014-2016 Google, Inc.  All rights reserved.
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
 * * Neither the name of Goole, Inc. nor the names of its contributors may be
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

/* file "decode_private.h" */

#ifndef TABLE_PRIVATE_H
#define TABLE_PRIVATE_H

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

/****************************************************************************
 * Macros to make tables legible
 */

/* flags */
#define no 0
#define pred DECODE_PREDICATE_28
#define predAL DECODE_PREDICATE_28_AL
#define pred22 DECODE_PREDICATE_22
#define pred8 DECODE_PREDICATE_8
#define xop DECODE_EXTRA_OPERANDS
#define xop_shift DECODE_EXTRA_SHIFT
#define xop_wb DECODE_EXTRA_WRITEBACK
#define xop_wb2 DECODE_EXTRA_WRITEBACK2
#define v8 DECODE_ARM_V8
#define vfp DECODE_ARM_VFP
#define srcX4 DECODE_4_SRCS
#define dstX3 DECODE_3_DSTS
#define unp DECODE_UNPREDICTABLE

/* eflags */
#define x 0
#define fRNZCV EFLAGS_READ_NZCV
#define fWNZCV EFLAGS_WRITE_NZCV
#define fRNZCVQG (EFLAGS_READ_NZCV | EFLAGS_READ_Q | EFLAGS_READ_GE)
#define fWNZCVQG (EFLAGS_WRITE_NZCV | EFLAGS_WRITE_Q | EFLAGS_WRITE_GE)
#define fRZ EFLAGS_READ_Z
#define fRC EFLAGS_READ_C
#define fRV EFLAGS_READ_V
#define fRNV (EFLAGS_READ_N | EFLAGS_READ_V)
#define fRNZV (EFLAGS_READ_N | EFLAGS_READ_Z | EFLAGS_READ_V)
#define fWNZ (EFLAGS_WRITE_N | EFLAGS_WRITE_Z)
#define fWNZC (EFLAGS_WRITE_N | EFLAGS_WRITE_Z | EFLAGS_WRITE_C)
#define fRGE EFLAGS_READ_GE
#define fWGE EFLAGS_WRITE_GE
#define fWQ EFLAGS_WRITE_Q

/* for constructing linked lists of table entries */
#define NA 0
#define END_LIST 0
#define DUP_ENTRY 0
/* A32 */
#define exop (ptr_int_t) & A32_extra_operands
#define top8 (ptr_int_t) & A32_pred_opc8
#define top4x (ptr_int_t) & A32_ext_opc4x
#define top4y (ptr_int_t) & A32_ext_opc4y
#define top4 (ptr_int_t) & A32_ext_opc4
#define ti19 (ptr_int_t) & A32_ext_imm1916
#define tb0 (ptr_int_t) & A32_ext_bits0
#define tb8 (ptr_int_t) & A32_ext_bits8
#define tb9 (ptr_int_t) & A32_ext_bit9
#define tb5 (ptr_int_t) & A32_ext_bit5
#define tb4 (ptr_int_t) & A32_ext_bit4
#define tfp (ptr_int_t) & A32_ext_fp
#define tfpA (ptr_int_t) & A32_ext_opc4fpA
#define tfpB (ptr_int_t) & A32_ext_opc4fpB
#define t16 (ptr_int_t) & A32_ext_bits16
#define trapc (ptr_int_t) & A32_ext_RAPC
#define trbpc (ptr_int_t) & A32_ext_RBPC
#define trdpc (ptr_int_t) & A32_ext_RDPC
#define ti5 (ptr_int_t) & A32_ext_imm5
#define top7 (ptr_int_t) & A32_unpred_opc7
#define tb20 (ptr_int_t) & A32_ext_bits20
#define ti20 (ptr_int_t) & A32_ext_imm2016
#define ti18 (ptr_int_t) & A32_ext_imm1816
#define tb7 (ptr_int_t) & A32_ext_bit7
#define tb6 (ptr_int_t) & A32_ext_bit6
#define tb19 (ptr_int_t) & A32_ext_bit19
#define tb22 (ptr_int_t) & A32_ext_bit22
#define tsi6 (ptr_int_t) & A32_ext_simd6
#define tsi5 (ptr_int_t) & A32_ext_simd5
#define tsi5b (ptr_int_t) & A32_ext_simd5b
#define tsi8 (ptr_int_t) & A32_ext_simd8
#define tsi6b (ptr_int_t) & A32_ext_simd6b
#define tsi2 (ptr_int_t) & A32_ext_simd2
#define ti6l (ptr_int_t) & A32_ext_imm6L
#define tvlA (ptr_int_t) & A32_ext_vldA
#define tvlB (ptr_int_t) & A32_ext_vldB
#define tvlC (ptr_int_t) & A32_ext_vldC
#define tvlD (ptr_int_t) & A32_ext_vldD
#define tvtb (ptr_int_t) & A32_ext_vtb
/* T32.32 */
#define xbase (ptr_int_t) & T32_base_e
#define xbasf (ptr_int_t) & T32_base_f
#define xfop8 (ptr_int_t) & T32_ext_fopc8
#define xa97 (ptr_int_t) & T32_ext_A9_7_eq1
#define xa106 (ptr_int_t) & T32_ext_bits_A10_6_4
#define xopbx (ptr_int_t) & T32_ext_opcBX
#define xb108 (ptr_int_t) & T32_ext_bits_B10_8
#define xb74 (ptr_int_t) & T32_ext_bits_B7_4
#define xb741 (ptr_int_t) & T32_ext_B7_4_eq1
#define xb64 (ptr_int_t) & T32_ext_bits_B6_4
#define xb54 (ptr_int_t) & T32_ext_bits_B5_4
#define xb20 (ptr_int_t) & T32_ext_bits_B2_0
#define xb4 (ptr_int_t) & T32_ext_bit_B4
#define xb5 (ptr_int_t) & T32_ext_bit_B5
#define xb7 (ptr_int_t) & T32_ext_bit_B7
#define xb11 (ptr_int_t) & T32_ext_bit_B11
#define xb13 (ptr_int_t) & T32_ext_bit_B13
#define xrapc (ptr_int_t) & T32_ext_RAPC
#define xrbpc (ptr_int_t) & T32_ext_RBPC
#define xrcpc (ptr_int_t) & T32_ext_RCPC
#define xi126 (ptr_int_t) & T32_ext_imm126
#define xexop (ptr_int_t) & T32_extra_operands
#define xcope (ptr_int_t) & T32_coproc_e
#define xcopf (ptr_int_t) & T32_coproc_f
#define xfp (ptr_int_t) & T32_ext_fp
#define xopc4 (ptr_int_t) & T32_ext_opc4
#define xi19 (ptr_int_t) & T32_ext_imm1916
#define xfpA (ptr_int_t) & T32_ext_opc4fpA
#define xfpB (ptr_int_t) & T32_ext_opc4fpB
#define xbi16 (ptr_int_t) & T32_ext_bits16
#define xbi20 (ptr_int_t) & T32_ext_bits20
#define xi20 (ptr_int_t) & T32_ext_imm2016
#define xi18 (ptr_int_t) & T32_ext_imm1816
#define xb6 (ptr_int_t) & T32_ext_bit6
#define xb19 (ptr_int_t) & T32_ext_bit19
#define xsi6 (ptr_int_t) & T32_ext_simd6
#define xsi5 (ptr_int_t) & T32_ext_simd5
#define xsi5b (ptr_int_t) & T32_ext_simd5b
#define xsi8 (ptr_int_t) & T32_ext_simd8
#define xsi6b (ptr_int_t) & T32_ext_simd6b
#define xsi2 (ptr_int_t) & T32_ext_simd2
#define xi6l (ptr_int_t) & T32_ext_imm6L
#define xvlA (ptr_int_t) & T32_ext_vldA
#define xvlB (ptr_int_t) & T32_ext_vldB
#define xvlC (ptr_int_t) & T32_ext_vldC
#define xvlD (ptr_int_t) & T32_ext_vldD
#define xvtb (ptr_int_t) & T32_ext_vtb
/* T32.16 */
#define ytop (ptr_int_t) & T32_16_opc4
#define y11 (ptr_int_t) & T32_16_ext_bit_11
#define y1110 (ptr_int_t) & T32_16_ext_bits_11_10
#define y119 (ptr_int_t) & T32_16_ext_bits_11_9
#define y118 (ptr_int_t) & T32_16_ext_bits_11_8
#define y96 (ptr_int_t) & T32_16_ext_bits_9_6
#define y7 (ptr_int_t) & T32_16_ext_bit_7
#define y54 (ptr_int_t) & T32_16_ext_bits_5_4
#define y109 (ptr_int_t) & T32_16_ext_bits_10_9
#define y108 (ptr_int_t) & T32_16_ext_bits_10_8
#define y76 (ptr_int_t) & T32_16_ext_bits_7_6
#define y64 (ptr_int_t) & T32_16_ext_bits_6_4
#define y30 (ptr_int_t) & T32_16_ext_imm_3_0
#define y106 (ptr_int_t) & T32_16_ext_imm_10_6
/* T32.16 IT block tables */
#define ztop (ptr_int_t) & T32_16_it_opc4
#define z11 (ptr_int_t) & T32_16_it_ext_bit_11
#define z1110 (ptr_int_t) & T32_16_it_ext_bits_11_10
#define z119 (ptr_int_t) & T32_16_it_ext_bits_11_9
#define z118 (ptr_int_t) & T32_16_it_ext_bits_11_8
#define z96 (ptr_int_t) & T32_16_it_ext_bits_9_6
#define z7 (ptr_int_t) & T32_16_it_ext_bit_7
#define z109 (ptr_int_t) & T32_16_it_ext_bits_10_9
#define z108 (ptr_int_t) & T32_16_it_ext_bits_10_8
#define z76 (ptr_int_t) & T32_16_it_ext_bits_7_6
#define z64 (ptr_int_t) & T32_16_it_ext_bits_6_4
#define z106 (ptr_int_t) & T32_16_it_ext_imm_10_6

/* operands */
#define xx TYPE_NONE, OPSZ_NA

#define i1_3 TYPE_I_b3, OPSZ_1b
#define i1_4 TYPE_I_b4, OPSZ_1b
#define i1_5 TYPE_I_b5, OPSZ_1b
#define i1_7 TYPE_I_b7, OPSZ_1b
#define i1_9 TYPE_I_b9, OPSZ_1b
#define i1_19 TYPE_I_b19, OPSZ_1b
#define i1_21 TYPE_I_b21, OPSZ_1b
#define i2 TYPE_I_b0, OPSZ_2b
#define i2_18 TYPE_I_b18, OPSZ_2b
#define i2_4 TYPE_I_b4, OPSZ_2b
#define i2_6 TYPE_I_b6, OPSZ_2b
#define i2_20 TYPE_I_b20, OPSZ_2b
#define i2x5_3 TYPE_I_b5_b3, OPSZ_2b
#define i2x21_6 TYPE_I_b21_b6, OPSZ_2b
#define i3 TYPE_I_b0, OPSZ_3b
#define i3_5 TYPE_I_b5, OPSZ_3b
#define i3_6 TYPE_I_b6, OPSZ_3b
#define i3_16 TYPE_I_b16, OPSZ_3b
#define i3_17 TYPE_I_b17, OPSZ_3b
#define i3_21 TYPE_I_b21, OPSZ_3b
#define i3x21_5 TYPE_I_b21_b5, OPSZ_3b
#define i4 TYPE_I_b0, OPSZ_4b
#define i4_4 TYPE_I_b4, OPSZ_4b
#define i4_7 TYPE_I_b7, OPSZ_4b
#define i4_8 TYPE_I_b8, OPSZ_4b
#define i4_16 TYPE_I_b16, OPSZ_4b
#define i4_20 TYPE_I_b20, OPSZ_4b
#define i5 TYPE_I_b0, OPSZ_5b
#define i5_6 TYPE_I_b6, OPSZ_5b
#define i5_7 TYPE_I_b7, OPSZ_5b
#define i5_16 TYPE_I_b16, OPSZ_5b
#define i5x0_5 TYPE_I_b0_b5, OPSZ_5b
#define i5x4_8 TYPE_I_b4_b8, OPSZ_5b
#define i5x4_16 TYPE_I_b4_b16, OPSZ_5b
#define i5x8_16 TYPE_I_b8_b16, OPSZ_5b
#define i5x12_6 TYPE_I_b12_b6, OPSZ_5b
#define i6 TYPE_I_b0, OPSZ_6b
#define i6_16 TYPE_I_b16, OPSZ_6b
#define i7x4 TYPE_I_x4_b0, OPSZ_7b
#define i8 TYPE_I_b0, OPSZ_1
#define n8 TYPE_NI_b0, OPSZ_1
#define i8x4 TYPE_I_x4_b0, OPSZ_1
#define n8x4 TYPE_NI_x4_b0, OPSZ_1
#define n8x8_0 TYPE_NI_b8_b0, OPSZ_1
#define i8x8_0 TYPE_I_b8_b0, OPSZ_1
#define i8x16_0 TYPE_I_b16_b0, OPSZ_1
#define i12x8_24_16_0 TYPE_I_b8_b24_b16_b0, OPSZ_12b
#define i12x8_28_16_0 TYPE_I_b8_b28_b16_b0, OPSZ_12b
#define i9 TYPE_I_b0, OPSZ_9b
#define i12 TYPE_I_b0, OPSZ_12b
#define i12sh TYPE_I_SHIFTED_b0, OPSZ_12b
#define i12x26_12_0 TYPE_I_b26_b12_b0, OPSZ_12b
#define i12x26_12_0_z TYPE_I_b26_b12_b0_z, OPSZ_12b
#define n12 TYPE_NI_b0, OPSZ_12b
#define i16x8_0 TYPE_I_b8_b0, OPSZ_2
#define i16x16_0 TYPE_I_b16_b0, OPSZ_2
#define i16x16_26_12_0 TYPE_I_b16_b26_b12_b0, OPSZ_2
#define i24 TYPE_I_b0, OPSZ_3
#define j6x9_3 TYPE_J_b9_b3, OPSZ_6b
#define j8 TYPE_J_b0, OPSZ_1
#define j11 TYPE_J_b0, OPSZ_11b
#define j24_x4 TYPE_J_x4_b0, OPSZ_3
#define j25x0_24 TYPE_J_b0_b24, OPSZ_25b
#define j20x26_11_13_16_0 TYPE_J_b26_b11_b13_b16_b0, OPSZ_20b
#define j24x26_13_11_16_0 TYPE_J_b26_b13_b11_b16_b0, OPSZ_3
#define ro2 TYPE_I_b10, OPSZ_2b
#define ro2_4 TYPE_I_b4, OPSZ_2b
#define sh2 TYPE_SHIFT_b5, OPSZ_2b
#define sh2_4 TYPE_SHIFT_b4, OPSZ_2b
#define sh1 TYPE_SHIFT_b6, OPSZ_1b     /* value is :0 */
#define sh1_21 TYPE_SHIFT_b21, OPSZ_1b /* value is :0 */

/* XXX: since A64 will need a completely separate decoder table set, should
 * we abandon this "ptr" abstraction and switch to d instead of w?
 */
#define RAw TYPE_R_A, OPSZ_PTR
#define RBw TYPE_R_B, OPSZ_PTR
#define RCw TYPE_R_C, OPSZ_PTR
#define RDw TYPE_R_D, OPSZ_PTR
#define RAh TYPE_R_A, OPSZ_2_of_4
#define RBh TYPE_R_B, OPSZ_2_of_4
#define RCh TYPE_R_C, OPSZ_2_of_4
#define RDh TYPE_R_D, OPSZ_2_of_4
#define RAt TYPE_R_A_TOP, OPSZ_2_of_4
#define RBt TYPE_R_B_TOP, OPSZ_2_of_4
#define RCt TYPE_R_C_TOP, OPSZ_2_of_4
#define RDt TYPE_R_D_TOP, OPSZ_2_of_4
#define RAb TYPE_R_A, OPSZ_1_of_4
#define RBb TYPE_R_B, OPSZ_1_of_4
#define RCb TYPE_R_C, OPSZ_1_of_4
#define RDb TYPE_R_D, OPSZ_1_of_4
#define RAd TYPE_R_A, OPSZ_4
#define RBd TYPE_R_B, OPSZ_4
#define RDNw TYPE_R_D_NEGATED, OPSZ_PTR
#define RBEw TYPE_R_B_EVEN, OPSZ_PTR
#define RB2w TYPE_R_B_PLUS1, OPSZ_PTR
#define RDEw TYPE_R_D_EVEN, OPSZ_PTR
#define RD2w TYPE_R_D_PLUS1, OPSZ_PTR
#define RA_EQ_Dw TYPE_R_A_EQ_D, OPSZ_PTR
#define RA_EQ_Dh TYPE_R_A_EQ_D, OPSZ_2_of_4

/* T16 */
#define RUw TYPE_R_U, OPSZ_PTR
#define RVw TYPE_R_V, OPSZ_PTR
#define RWw TYPE_R_W, OPSZ_PTR
#define RXw TYPE_R_X, OPSZ_PTR
#define RYw TYPE_R_Y, OPSZ_PTR
#define RZw TYPE_R_Z, OPSZ_PTR
#define RYh TYPE_R_Y, OPSZ_2_of_4
#define RYb TYPE_R_Y, OPSZ_1_of_4
#define RZh TYPE_R_Z, OPSZ_2_of_4
#define RZb TYPE_R_Z, OPSZ_1_of_4
#define RVDw TYPE_R_V_DUP, OPSZ_PTR
#define RWDw TYPE_R_W_DUP, OPSZ_PTR
#define RZDw TYPE_R_Z_DUP, OPSZ_PTR

#define VAdq TYPE_V_A, OPSZ_16
#define VBdq TYPE_V_B, OPSZ_16
#define VCdq TYPE_V_C, OPSZ_16
#define VAq TYPE_V_A, OPSZ_8
#define VBq TYPE_V_B, OPSZ_8
#define VCq TYPE_V_C, OPSZ_8
#define VAd TYPE_V_A, OPSZ_4
#define VBd TYPE_V_B, OPSZ_4
#define VCd TYPE_V_C, OPSZ_4
#define VAb_q TYPE_V_A, OPSZ_1_of_8
#define VAh_q TYPE_V_A, OPSZ_2_of_8
#define VAd_q TYPE_V_A, OPSZ_4_of_8
#define VBb_q TYPE_V_B, OPSZ_1_of_8
#define VBh_q TYPE_V_B, OPSZ_2_of_8
#define VBd_q TYPE_V_B, OPSZ_4_of_8
#define VCb_q TYPE_V_C, OPSZ_1_of_8
#define VCh_q TYPE_V_C, OPSZ_2_of_8
#define VCd_q TYPE_V_C, OPSZ_4_of_8
#define VC3h_q TYPE_V_C_3b, OPSZ_2_of_8
#define VC4d_q TYPE_V_C_4b, OPSZ_4_of_8
#define WAd TYPE_W_A, OPSZ_4
#define WBd TYPE_W_B, OPSZ_4
#define WCd TYPE_W_C, OPSZ_4
#define WC2d TYPE_W_C_PLUS1, OPSZ_4
#define WAq TYPE_W_A, OPSZ_8
#define WBq TYPE_W_B, OPSZ_8
#define WCq TYPE_W_C, OPSZ_8
/* XXX: would s be better than h?  Or w to match x86 and use d for RAw, etc.? */
#define WAh TYPE_W_A, OPSZ_2
#define WBh TYPE_W_B, OPSZ_2
#define WCh TYPE_W_C, OPSZ_2

#define L8w TYPE_L_8b, OPSZ_PTR
#define L9Lw TYPE_L_9b_LR, OPSZ_PTR
#define L9Pw TYPE_L_9b_PC, OPSZ_PTR
#define L16w TYPE_L_16b, OPSZ_PTR
#define L15w TYPE_L_16b_NO_SP, OPSZ_PTR
#define L14w TYPE_L_16b_NO_SP_PC, OPSZ_PTR
#define LCd TYPE_L_CONSEC, OPSZ_4
#define LCq TYPE_L_CONSEC, OPSZ_8
#define LX2q TYPE_L_VBx2, OPSZ_8
#define LX3q TYPE_L_VBx3, OPSZ_8
#define LX4q TYPE_L_VBx4, OPSZ_8
#define LX2b_q TYPE_L_VBx2, OPSZ_1_of_8
#define LX3b_q TYPE_L_VBx3, OPSZ_1_of_8
#define LX4b_q TYPE_L_VBx4, OPSZ_1_of_8
#define LX2h_q TYPE_L_VBx2, OPSZ_2_of_8
#define LX3h_q TYPE_L_VBx3, OPSZ_2_of_8
#define LX4h_q TYPE_L_VBx4, OPSZ_2_of_8
#define LX2d_q TYPE_L_VBx2, OPSZ_4_of_8
#define LX3d_q TYPE_L_VBx3, OPSZ_4_of_8
#define LX4d_q TYPE_L_VBx4, OPSZ_4_of_8
#define LX2Dq TYPE_L_VBx2D, OPSZ_8
#define LX3Dq TYPE_L_VBx3D, OPSZ_8
#define LX4Dq TYPE_L_VBx4D, OPSZ_8
#define LX2Db_q TYPE_L_VBx2D, OPSZ_1_of_8
#define LX3Db_q TYPE_L_VBx3D, OPSZ_1_of_8
#define LX4Db_q TYPE_L_VBx4D, OPSZ_1_of_8
#define LX2Dh_q TYPE_L_VBx2D, OPSZ_2_of_8
#define LX3Dh_q TYPE_L_VBx3D, OPSZ_2_of_8
#define LX4Dh_q TYPE_L_VBx4D, OPSZ_2_of_8
#define LX2Dd_q TYPE_L_VBx2D, OPSZ_4_of_8
#define LX3Dd_q TYPE_L_VBx3D, OPSZ_4_of_8
#define LX4Dd_q TYPE_L_VBx4D, OPSZ_4_of_8
#define LXA2q TYPE_L_VAx2, OPSZ_8
#define LXA3q TYPE_L_VAx3, OPSZ_8
#define LXA4q TYPE_L_VAx4, OPSZ_8

#define CRAw TYPE_CR_A, OPSZ_PTR
#define CRBw TYPE_CR_B, OPSZ_PTR
#define CRCw TYPE_CR_C, OPSZ_PTR
#define CRDw TYPE_CR_D, OPSZ_PTR

#define SPSR TYPE_SPSR, OPSZ_PTR
#define CPSR TYPE_CPSR, OPSZ_PTR
#define FPSCR TYPE_FPSCR, OPSZ_PTR
#define LRw TYPE_LR, OPSZ_PTR
#define SPw TYPE_SP, OPSZ_PTR
#define PCw TYPE_PC, OPSZ_PTR

#define Mw TYPE_M, OPSZ_PTR
#define Mb TYPE_M, OPSZ_1
#define Mh TYPE_M, OPSZ_PTR_HALF
#define Md TYPE_M, OPSZ_4
#define Mq TYPE_M, OPSZ_PTR_DBL
#define M3 TYPE_M, OPSZ_3
#define M6 TYPE_M, OPSZ_6
#define M12 TYPE_M, OPSZ_12
#define Mdq TYPE_M, OPSZ_16
#define M24 TYPE_M, OPSZ_24
#define Mqq TYPE_M, OPSZ_32
#define MP12w TYPE_M_POS_I12, OPSZ_PTR
#define MP12h TYPE_M_POS_I12, OPSZ_PTR_HALF
#define MP12b TYPE_M_POS_I12, OPSZ_1
#define MP12z TYPE_M_POS_I12, OPSZ_0
#define MN12w TYPE_M_NEG_I12, OPSZ_4
#define MN12b TYPE_M_NEG_I12, OPSZ_1
#define MN12z TYPE_M_NEG_I12, OPSZ_0
#define MPCP8w TYPE_M_PCREL_POS_I8x4, OPSZ_PTR
#define MPCN12w TYPE_M_PCREL_NEG_I12, OPSZ_PTR
#define MPCN12h TYPE_M_PCREL_NEG_I12, OPSZ_PTR_HALF
#define MPCN12b TYPE_M_PCREL_NEG_I12, OPSZ_1
#define MPCN12z TYPE_M_PCREL_NEG_I12, OPSZ_0
#define MPCP12w TYPE_M_PCREL_POS_I12, OPSZ_PTR
#define MPCP12h TYPE_M_PCREL_POS_I12, OPSZ_PTR_HALF
#define MPCP12b TYPE_M_PCREL_POS_I12, OPSZ_1
#define MPCP12z TYPE_M_PCREL_POS_I12, OPSZ_0
#define MSPP8w TYPE_M_SP_POS_I8x4, OPSZ_PTR
#define MP8w TYPE_M_POS_I8, OPSZ_PTR
#define MP8h TYPE_M_POS_I8, OPSZ_PTR_HALF
#define MP8b TYPE_M_POS_I8, OPSZ_1
#define MN8w TYPE_M_NEG_I8, OPSZ_PTR
#define MN8h TYPE_M_NEG_I8, OPSZ_PTR_HALF
#define MN8b TYPE_M_NEG_I8, OPSZ_1
#define MN8z TYPE_M_NEG_I8, OPSZ_0
#define MP8Xw TYPE_M_POS_I8x4, OPSZ_PTR
#define MP8Xq TYPE_M_POS_I8x4, OPSZ_8
#define MP8Xd TYPE_M_POS_I8x4, OPSZ_4
#define MN8Xw TYPE_M_NEG_I8x4, OPSZ_PTR
#define MN8Xq TYPE_M_NEG_I8x4, OPSZ_8
#define MN8Xd TYPE_M_NEG_I8x4, OPSZ_4
#define MP5w TYPE_M_POS_I5x4, OPSZ_PTR
#define MP5h TYPE_M_POS_I5x2, OPSZ_PTR_HALF
#define MP5b TYPE_M_POS_I5, OPSZ_1
#define MP44b TYPE_M_POS_I4_4, OPSZ_1
#define MP44h TYPE_M_POS_I4_4, OPSZ_PTR_HALF
#define MP44q TYPE_M_POS_I4_4, OPSZ_PTR_DBL
#define MN44b TYPE_M_NEG_I4_4, OPSZ_1
#define MN44h TYPE_M_NEG_I4_4, OPSZ_PTR_HALF
#define MN44q TYPE_M_NEG_I4_4, OPSZ_PTR_DBL
#define MPRw TYPE_M_POS_REG, OPSZ_PTR
#define MPRh TYPE_M_POS_REG, OPSZ_PTR_HALF
#define MPRq TYPE_M_POS_REG, OPSZ_PTR_DBL
#define MPRb TYPE_M_POS_REG, OPSZ_1
#define MNRw TYPE_M_NEG_REG, OPSZ_PTR
#define MNRh TYPE_M_NEG_REG, OPSZ_PTR_HALF
#define MNRq TYPE_M_NEG_REG, OPSZ_PTR_DBL
#define MNRb TYPE_M_NEG_REG, OPSZ_1
#define MPSw TYPE_M_POS_SHREG, OPSZ_PTR
#define MPSb TYPE_M_POS_SHREG, OPSZ_1
#define MPSz TYPE_M_POS_SHREG, OPSZ_0
#define MNSw TYPE_M_NEG_SHREG, OPSZ_PTR
#define MNSb TYPE_M_NEG_SHREG, OPSZ_1
#define MNSz TYPE_M_NEG_SHREG, OPSZ_0
#define MLSw TYPE_M_POS_LSHREG, OPSZ_PTR
#define MLSh TYPE_M_POS_LSHREG, OPSZ_PTR_HALF
#define MLSb TYPE_M_POS_LSHREG, OPSZ_1
#define MLSz TYPE_M_POS_LSHREG, OPSZ_0
#define MPLS1h TYPE_M_POS_LSH1REG, OPSZ_PTR_HALF
#define Ml TYPE_M, OPSZ_VAR_REGLIST
#define MSPl TYPE_M_SP, OPSZ_VAR_REGLIST
#define MUBl TYPE_M_UP_OFFS, OPSZ_VAR_REGLIST
#define MDAl TYPE_M_DOWN, OPSZ_VAR_REGLIST
#define MDBl TYPE_M_DOWN_OFFS, OPSZ_VAR_REGLIST
#define MSPDBl TYPE_M_SP_DOWN_OFFS, OPSZ_VAR_REGLIST

/* hardcoded shift types */
#define LSL TYPE_SHIFT_LSL, OPSZ_0
#define ASR TYPE_SHIFT_ASR, OPSZ_0

/* integer constants */
#define k0 TYPE_K, 0
#define k8 TYPE_K, 8
#define k16 TYPE_K, 16
#define k32 TYPE_K, 32

#endif /* TABLE_PRIVATE_H */
