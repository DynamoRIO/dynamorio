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
#define v8        DECODE_ARM_V8
#define vfp       DECODE_ARM_VFP
#define srcX4     DECODE_4_SRCS
#define dstX3     DECODE_3_DSTS
#define unp       DECODE_UNPREDICTABLE

/* eflags */
#define x     0
#define fRNZCV    EFLAGS_READ_NZCV
#define fWNZCV    EFLAGS_WRITE_NZCV
#define fWNZ      (EFLAGS_WRITE_N | EFLAGS_WRITE_Z)
#define fWNZC     (EFLAGS_WRITE_N | EFLAGS_WRITE_Z | EFLAGS_WRITE_C)
#define fRGE      EFLAGS_READ_GE
#define fWQ       EFLAGS_WRITE_Q

/* for constructing linked lists of table entries */
#define NA 0
#define END_LIST  0

/* operands */
#define xx  TYPE_NONE, OPSZ_NA

#define i12      TYPE_I_b0, OPSZ_12b
#define i8       TYPE_I_b0, OPSZ_1
#define n8       TYPE_NI_b0, OPSZ_1
#define n12      TYPE_NI_b0, OPSZ_12b
#define i24      TYPE_I_b0, OPSZ_3
#define i16x0_8  TYPE_I_b0_b8, OPSZ_2
#define i16x0_16 TYPE_I_b0_b16, OPSZ_2
#define n8x0_8   TYPE_NI_b0_b8, OPSZ_1
#define i8x0_8   TYPE_I_b0_b8, OPSZ_1
#define i8x0_16  TYPE_I_b0_b16, OPSZ_1
#define i5x16_9  TYPE_I_b16_b9, OPSZ_5b
#define i5x16_8  TYPE_I_b16_b8, OPSZ_5b
#define i5x0_5   TYPE_I_b0_b5, OPSZ_5b
#define i1_21    TYPE_I_b21, OPSZ_1b
#define i2x21_6  TYPE_I_b21_b6, OPSZ_2b
#define i3x21_5  TYPE_I_b21_b5, OPSZ_3b
#define ro2      TYPE_I_b10, OPSZ_2b
#define i4       TYPE_I_b0, OPSZ_4b
#define i4_7     TYPE_I_b7, OPSZ_4b
#define i4_8     TYPE_I_b8, OPSZ_4b
#define i4_20    TYPE_I_b20, OPSZ_4b
#define i3_5     TYPE_I_b5, OPSZ_3b
#define i5       TYPE_I_b7, OPSZ_5b
#define i5_16    TYPE_I_b16, OPSZ_5b
#define i4_16    TYPE_I_b16, OPSZ_4b
#define i5_7     TYPE_I_b7, OPSZ_5b
#define i3_21    TYPE_I_b3, OPSZ_21b
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

#define VAq   TYPE_V_A, OPSZ_8
#define VBq   TYPE_V_B, OPSZ_8
#define VCq   TYPE_V_C, OPSZ_8
#define VAd   TYPE_V_A, OPSZ_4
#define VBd   TYPE_V_B, OPSZ_4
#define VCd   TYPE_V_C, OPSZ_4
#define WAd   TYPE_W_A, OPSZ_4
#define WBd   TYPE_W_B, OPSZ_4
#define WCd   TYPE_W_C, OPSZ_4
#define WC2d  TYPE_W_C_PLUS1, OPSZ_4
#define WAq   TYPE_W_A, OPSZ_8
#define WBq   TYPE_W_B, OPSZ_8
#define WCq   TYPE_W_C, OPSZ_8
/* XXX: would s be better than h? */
#define WAh   TYPE_W_A, OPSZ_2
#define WBh   TYPE_W_B, OPSZ_2
#define WCh   TYPE_W_C, OPSZ_2

#define L16w TYPE_L_16, OPSZ_PTR
#define LCd TYPE_L_CONSEC, OPSZ_4
#define LCq TYPE_L_CONSEC, OPSZ_8

#define CRAw TYPE_CR_A, OPSZ_PTR
#define CRBw TYPE_CR_B, OPSZ_PTR
#define CRCw TYPE_CR_C, OPSZ_PTR
#define CRDw TYPE_CR_D, OPSZ_PTR

#define SPSR TYPE_SPSR, OPSZ_PTR
#define CPSR TYPE_CPSR, OPSZ_PTR
#define FPSCR TYPE_FPSCR, OPSZ_PTR

#define Mw  TYPE_M, OPSZ_PTR
#define Mb  TYPE_M, OPSZ_1
#define Mh  TYPE_M, OPSZ_PTR_HALF
#define Md  TYPE_M, OPSZ_PTR_DBL
#define MP12w  TYPE_M_POS_I12, OPSZ_PTR
#define MP12b  TYPE_M_POS_I12, OPSZ_1
#define MN12w  TYPE_M_NEG_I12, OPSZ_4
#define MN12b  TYPE_M_NEG_I12, OPSZ_1
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
#define MP44d  TYPE_M_POS_I4_4, OPSZ_PTR_DBL
#define MN44b  TYPE_M_NEG_I4_4, OPSZ_1
#define MN44h  TYPE_M_NEG_I4_4, OPSZ_PTR_HALF
#define MN44d  TYPE_M_NEG_I4_4, OPSZ_PTR_DBL
#define MPRw  TYPE_M_POS_REG, OPSZ_PTR
#define MPRh  TYPE_M_POS_REG, OPSZ_PTR_HALF
#define MPRd  TYPE_M_POS_REG, OPSZ_PTR_DBL
#define MPRb  TYPE_M_POS_REG, OPSZ_1
#define MNRw  TYPE_M_NEG_REG, OPSZ_PTR
#define MNRh  TYPE_M_NEG_REG, OPSZ_PTR_HALF
#define MNRd  TYPE_M_NEG_REG, OPSZ_PTR_DBL
#define MNRb  TYPE_M_NEG_REG, OPSZ_1
#define MPSw  TYPE_M_POS_SHREG, OPSZ_PTR
#define MPSb  TYPE_M_POS_SHREG, OPSZ_1
#define MNSw  TYPE_M_NEG_SHREG, OPSZ_PTR
#define MNSb  TYPE_M_NEG_SHREG, OPSZ_1
#define Ml  TYPE_M, OPSZ_VAR_REGLIST
#define MUBl  TYPE_M_UP_OFFS, OPSZ_VAR_REGLIST
#define MDAl  TYPE_M_DOWN, OPSZ_VAR_REGLIST
#define MDBl  TYPE_M_DOWN_OFFS, OPSZ_VAR_REGLIST

/* hardcoded shift types */
#define LSL TYPE_SHIFT_LSL, OPSZ_0
#define ASR TYPE_SHIFT_ASR, OPSZ_0


#endif /* TABLE_PRIVATE_H */
