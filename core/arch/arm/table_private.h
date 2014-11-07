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
#define no        0
#define pred      DECODE_PREDICATE
#define predAL    DECODE_PREDICATE_AL_ONLY
#define xop       DECODE_EXTRA_OPERANDS
#define xop_shift DECODE_EXTRA_SHIFT
#define xop_wb    DECODE_EXTRA_WRITEBACK
#define xop_wb2   DECODE_EXTRA_WRITEBACK2
#define v8        DECODE_ARM_V8
#define vfp       DECODE_ARM_VFP
#define srcX4     DECODE_4_SRCS
#define dstX3     DECODE_3_DSTS
#define unp       DECODE_UNPREDICTABLE

/* eflags */
#define x     0
#define fRNZCV    EFLAGS_READ_NZCV
#define fWNZCV    EFLAGS_WRITE_NZCV
#define fRZ       EFLAGS_READ_Z
#define fRV       EFLAGS_READ_V
#define fRNV      (EFLAGS_READ_N | EFLAGS_READ_V)
#define fRNZV     (EFLAGS_READ_N | EFLAGS_READ_Z | EFLAGS_READ_V)
#define fWNZ      (EFLAGS_WRITE_N | EFLAGS_WRITE_Z)
#define fWNZC     (EFLAGS_WRITE_N | EFLAGS_WRITE_Z | EFLAGS_WRITE_C)
#define fRGE      EFLAGS_READ_GE
#define fWQ       EFLAGS_WRITE_Q

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
#define top7  (ptr_int_t)&A32_unpred_opc7
#define tb20  (ptr_int_t)&A32_ext_bits20
#define ti20  (ptr_int_t)&A32_ext_imm2016
#define ti18  (ptr_int_t)&A32_ext_imm1816
#define tb7   (ptr_int_t)&A32_ext_bit7
#define tb6   (ptr_int_t)&A32_ext_bit6
#define tb19  (ptr_int_t)&A32_ext_bit19
#define tb22  (ptr_int_t)&A32_ext_bit22
#define tsi6  (ptr_int_t)&A32_ext_simd6
#define tsi5  (ptr_int_t)&A32_ext_simd5
#define tsi5b (ptr_int_t)&A32_ext_simd5b
#define tsi8  (ptr_int_t)&A32_ext_simd8
#define tsi6b (ptr_int_t)&A32_ext_simd6b
#define tsi6c (ptr_int_t)&A32_ext_simd6c
#define tsi2  (ptr_int_t)&A32_ext_simd2
#define tvlA  (ptr_int_t)&A32_ext_vldA
#define tvlB  (ptr_int_t)&A32_ext_vldB
#define tvlC  (ptr_int_t)&A32_ext_vldC

/* operands */
#define xx  TYPE_NONE, OPSZ_NA

#define i1_4     TYPE_I_b4, OPSZ_1b
#define i1_5     TYPE_I_b5, OPSZ_1b
#define i1_7     TYPE_I_b7, OPSZ_1b
#define i1_9     TYPE_I_b9, OPSZ_1b
#define i1_19    TYPE_I_b19, OPSZ_1b
#define i1_21    TYPE_I_b21, OPSZ_1b
#define i2_18    TYPE_I_b18, OPSZ_2b
#define i2_4     TYPE_I_b4, OPSZ_2b
#define i2_6     TYPE_I_b6, OPSZ_2b
#define i2_20    TYPE_I_b20, OPSZ_2b
#define i2x5_3   TYPE_I_b5_b3, OPSZ_2b
#define i2x21_6  TYPE_I_b21_b6, OPSZ_2b
#define i3_5     TYPE_I_b5, OPSZ_3b
#define i3_6     TYPE_I_b6, OPSZ_3b
#define i3_16    TYPE_I_b16, OPSZ_3b
#define i3_17    TYPE_I_b17, OPSZ_3b
#define i3_21    TYPE_I_b3, OPSZ_21b
#define i3x21_5  TYPE_I_b21_b5, OPSZ_3b
#define i4       TYPE_I_b0, OPSZ_4b
#define i4_7     TYPE_I_b7, OPSZ_4b
#define i4_8     TYPE_I_b8, OPSZ_4b
#define i4_16    TYPE_I_b16, OPSZ_4b
#define i4_20    TYPE_I_b20, OPSZ_4b
#define i5       TYPE_I_b7, OPSZ_5b
#define i5_7     TYPE_I_b7, OPSZ_5b
#define i5_16    TYPE_I_b16, OPSZ_5b
#define i5x0_5   TYPE_I_b0_b5, OPSZ_5b
#define i5x16_9  TYPE_I_b16_b9, OPSZ_5b
#define i5x16_8  TYPE_I_b16_b8, OPSZ_5b
#define i6_16    TYPE_I_b16, OPSZ_6b
#define i8       TYPE_I_b0, OPSZ_1
#define n8       TYPE_NI_b0, OPSZ_1
#define n8x0_8   TYPE_NI_b0_b8, OPSZ_1
#define i8x0_8   TYPE_I_b0_b8, OPSZ_1
#define i8x0_16  TYPE_I_b0_b16, OPSZ_1
#define i8x24_16_0 TYPE_I_b24_b16_b0, OPSZ_1
#define i12      TYPE_I_b0, OPSZ_12b
#define n12      TYPE_NI_b0, OPSZ_12b
#define i16x0_8  TYPE_I_b0_b8, OPSZ_2
#define i16x0_16 TYPE_I_b0_b16, OPSZ_2
#define i24      TYPE_I_b0, OPSZ_3
#define i25x0_24 TYPE_I_b0_b24, OPSZ_25b
#define ro2      TYPE_I_b10, OPSZ_2b
#define sh2      TYPE_SHIFT_b5, OPSZ_2b
#define sh1      TYPE_SHIFT_b6, OPSZ_1b /* value is :0 */

/* XXX: since A64 will need a completely separate decoder table set, should
 * we abandon this "ptr" abstraction and switch to d instead of w?
 */
#define RAw  TYPE_R_A, OPSZ_PTR
#define RBw  TYPE_R_B, OPSZ_PTR
#define RCw  TYPE_R_C, OPSZ_PTR
#define RDw  TYPE_R_D, OPSZ_PTR
#define RAh  TYPE_R_A, OPSZ_PTR_HALF
#define RBh  TYPE_R_B, OPSZ_PTR_HALF
#define RCh  TYPE_R_C, OPSZ_PTR_HALF
#define RDh  TYPE_R_D, OPSZ_PTR_HALF
#define RAt  TYPE_R_A_TOP, OPSZ_PTR_HALF
#define RBt  TYPE_R_B_TOP, OPSZ_PTR_HALF
#define RCt  TYPE_R_C_TOP, OPSZ_PTR_HALF
#define RDt  TYPE_R_D_TOP, OPSZ_PTR_HALF
#define RAb  TYPE_R_A, OPSZ_1
#define RBb  TYPE_R_B, OPSZ_1
#define RCb  TYPE_R_C, OPSZ_1
#define RDb  TYPE_R_D, OPSZ_1
#define RAd  TYPE_R_A, OPSZ_4
#define RBd  TYPE_R_B, OPSZ_4
#define RDNw  TYPE_R_D_NEGATED, OPSZ_PTR
#define RBEw TYPE_R_B_EVEN, OPSZ_PTR
#define RB2w TYPE_R_B_PLUS1, OPSZ_PTR
#define RDEw TYPE_R_D_EVEN, OPSZ_PTR
#define RD2w TYPE_R_D_PLUS1, OPSZ_PTR

#define VAdq   TYPE_V_A, OPSZ_16
#define VBdq   TYPE_V_B, OPSZ_16
#define VCdq   TYPE_V_C, OPSZ_16
#define VAq   TYPE_V_A, OPSZ_8
#define VBq   TYPE_V_B, OPSZ_8
#define VCq   TYPE_V_C, OPSZ_8
#define VAd   TYPE_V_A, OPSZ_4
#define VBd   TYPE_V_B, OPSZ_4
#define VCd   TYPE_V_C, OPSZ_4
#define VAb_q  TYPE_V_A, OPSZ_1_of_8
#define VAh_q  TYPE_V_A, OPSZ_2_of_8
#define VAd_q  TYPE_V_A, OPSZ_4_of_8
#define VBb_q  TYPE_V_B, OPSZ_1_of_8
#define VBh_q  TYPE_V_B, OPSZ_2_of_8
#define VBd_q  TYPE_V_B, OPSZ_4_of_8
#define VCb_q  TYPE_V_C, OPSZ_1_of_8
#define VCh_q  TYPE_V_C, OPSZ_2_of_8
#define VCd_q  TYPE_V_C, OPSZ_4_of_8
#define VC3h_q  TYPE_V_C_3b, OPSZ_2_of_8
#define VC4d_q  TYPE_V_C_4b, OPSZ_4_of_8
#define WAd   TYPE_W_A, OPSZ_4
#define WBd   TYPE_W_B, OPSZ_4
#define WCd   TYPE_W_C, OPSZ_4
#define WC2d  TYPE_W_C_PLUS1, OPSZ_4
#define WAq   TYPE_W_A, OPSZ_8
#define WBq   TYPE_W_B, OPSZ_8
#define WCq   TYPE_W_C, OPSZ_8
/* XXX: would s be better than h?  Or w to match x86 and use d for RAw, etc.? */
#define WAh   TYPE_W_A, OPSZ_2
#define WBh   TYPE_W_B, OPSZ_2
#define WCh   TYPE_W_C, OPSZ_2

#define L16w TYPE_L_16b, OPSZ_PTR
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

#define CRAw TYPE_CR_A, OPSZ_PTR
#define CRBw TYPE_CR_B, OPSZ_PTR
#define CRCw TYPE_CR_C, OPSZ_PTR
#define CRDw TYPE_CR_D, OPSZ_PTR

#define SPSR TYPE_SPSR, OPSZ_PTR
#define CPSR TYPE_CPSR, OPSZ_PTR
#define FPSCR TYPE_FPSCR, OPSZ_PTR
#define LRw TYPE_LR, OPSZ_PTR
#define SPw TYPE_SP, OPSZ_PTR

#define Mw  TYPE_M, OPSZ_PTR
#define Mb  TYPE_M, OPSZ_1
#define Mh  TYPE_M, OPSZ_PTR_HALF
#define Md  TYPE_M, OPSZ_4
#define Mq  TYPE_M, OPSZ_PTR_DBL
#define M3  TYPE_M, OPSZ_3
#define M6  TYPE_M, OPSZ_6
#define M12 TYPE_M, OPSZ_12
#define Mdq  TYPE_M, OPSZ_16
#define M24  TYPE_M, OPSZ_24
#define Mqq  TYPE_M, OPSZ_32
#define MP12w  TYPE_M_POS_I12, OPSZ_PTR
#define MP12b  TYPE_M_POS_I12, OPSZ_1
#define MP12z  TYPE_M_POS_I12, OPSZ_0
#define MN12w  TYPE_M_NEG_I12, OPSZ_4
#define MN12b  TYPE_M_NEG_I12, OPSZ_1
#define MN12z  TYPE_M_NEG_I12, OPSZ_0
#define MP8w  TYPE_M_POS_I8, OPSZ_PTR
#define MP8q  TYPE_M_POS_I8, OPSZ_8
#define MP8d  TYPE_M_POS_I8, OPSZ_4
#define MP8b  TYPE_M_POS_I8, OPSZ_1
#define MN8w  TYPE_M_NEG_I8, OPSZ_PTR
#define MN8q  TYPE_M_NEG_I8, OPSZ_8
#define MN8d  TYPE_M_NEG_I8, OPSZ_4
#define MN8b  TYPE_M_NEG_I8, OPSZ_1
#define MP44b  TYPE_M_POS_I4_4, OPSZ_1
#define MP44h  TYPE_M_POS_I4_4, OPSZ_PTR_HALF
#define MP44q  TYPE_M_POS_I4_4, OPSZ_PTR_DBL
#define MN44b  TYPE_M_NEG_I4_4, OPSZ_1
#define MN44h  TYPE_M_NEG_I4_4, OPSZ_PTR_HALF
#define MN44q  TYPE_M_NEG_I4_4, OPSZ_PTR_DBL
#define MPRw  TYPE_M_POS_REG, OPSZ_PTR
#define MPRh  TYPE_M_POS_REG, OPSZ_PTR_HALF
#define MPRq  TYPE_M_POS_REG, OPSZ_PTR_DBL
#define MPRb  TYPE_M_POS_REG, OPSZ_1
#define MNRw  TYPE_M_NEG_REG, OPSZ_PTR
#define MNRh  TYPE_M_NEG_REG, OPSZ_PTR_HALF
#define MNRq  TYPE_M_NEG_REG, OPSZ_PTR_DBL
#define MNRb  TYPE_M_NEG_REG, OPSZ_1
#define MPSw  TYPE_M_POS_SHREG, OPSZ_PTR
#define MPSb  TYPE_M_POS_SHREG, OPSZ_1
#define MPSz  TYPE_M_POS_SHREG, OPSZ_0
#define MNSw  TYPE_M_NEG_SHREG, OPSZ_PTR
#define MNSb  TYPE_M_NEG_SHREG, OPSZ_1
#define MNSz  TYPE_M_NEG_SHREG, OPSZ_0
#define Ml  TYPE_M, OPSZ_VAR_REGLIST
#define MUBl  TYPE_M_UP_OFFS, OPSZ_VAR_REGLIST
#define MDAl  TYPE_M_DOWN, OPSZ_VAR_REGLIST
#define MDBl  TYPE_M_DOWN_OFFS, OPSZ_VAR_REGLIST

/* hardcoded shift types */
#define LSL TYPE_SHIFT_LSL, OPSZ_0
#define ASR TYPE_SHIFT_ASR, OPSZ_0

/* integer constants */
#define k8  TYPE_K, 8
#define k16 TYPE_K, 16
#define k32 TYPE_K, 32

#endif /* TABLE_PRIVATE_H */
