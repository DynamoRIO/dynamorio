/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/* decode_table.c -- tables for decoding x86 instructions
 */

#include "../globals.h" /* need this to include decode.h (uint, etc.) */
#include "arch.h"    /* need this to include decode.h (byte, etc. */
#include "instr.h" /* for REG_ constants */
#include "decode.h"
#include "decode_private.h"

/****************************************************************************
 * All code below based on tables in the ``Intel Architecture Software
 * Developer's Manual,'' Volume 2: Instruction Set Reference, 2001.
 * Updated with information from later Intel manuals and AMD manuals.
 *
 * I added many new types not present in the Intel tables: see decode.h
 *
 * I don't list %eflags as a source or dest operand, but the particular
 * flags written are encoded.
 *
 * XXX: some day it may be worth adding flags indicating which instrs
 * are valid on which models of which processors (probably best to just add
 * which cpuid flag must be set for the instr to be supported): for
 * now though we do not rely on being able to predict which instrs are
 * invalid.
 */

/****************************************************************************
 * Operand pointers into tables
 * When there are multiple encodings of an opcode, this points to the first
 * entry in a linked list.
 * This array corresponds with the enum in instr.h
 * IF YOU CHANGE ONE YOU MUST CHANGE THE OTHER
 */
const instr_info_t * const op_instr[] =
{
    /* OP_INVALID */   NULL,
    /* OP_UNDECODED */ NULL,
    /* OP_CONTD   */   NULL,
    /* OP_LABEL   */   NULL,

    /* OP_add     */   &first_byte[0x05],
    /* OP_or      */   &first_byte[0x0d],
    /* OP_adc     */   &first_byte[0x15],
    /* OP_sbb     */   &first_byte[0x1d],
    /* OP_and     */   &first_byte[0x25],
    /* OP_daa     */   &first_byte[0x27],
    /* OP_sub     */   &first_byte[0x2d],
    /* OP_das     */   &first_byte[0x2f],
    /* OP_xor     */   &first_byte[0x35],
    /* OP_aaa     */   &first_byte[0x37],
    /* OP_cmp     */   &first_byte[0x3d],
    /* OP_aas     */   &first_byte[0x3f],
    /* OP_inc     */   &x64_extensions[0][0],
    /* OP_dec     */   &x64_extensions[8][0],
    /* OP_push    */   &first_byte[0x50],
    /* OP_push_imm*/   &first_byte[0x68],
    /* OP_pop     */   &first_byte[0x58],
    /* OP_pusha   */   &first_byte[0x60],
    /* OP_popa    */   &first_byte[0x61],
    /* OP_bound   */   &first_byte[0x62],
    /* OP_arpl    */   &x64_extensions[16][0],
    /* OP_imul    */   &extensions[10][5],

    /* OP_jo_short    */   &first_byte[0x70],
    /* OP_jno_short   */   &first_byte[0x71],
    /* OP_jb_short    */   &first_byte[0x72],
    /* OP_jnb_short   */   &first_byte[0x73],
    /* OP_jz_short    */   &first_byte[0x74],
    /* OP_jnz_short   */   &first_byte[0x75],
    /* OP_jbe_short   */   &first_byte[0x76],
    /* OP_jnbe_short  */   &first_byte[0x77],
    /* OP_js_short    */   &first_byte[0x78],
    /* OP_jns_short   */   &first_byte[0x79],
    /* OP_jp_short    */   &first_byte[0x7a],
    /* OP_jnp_short   */   &first_byte[0x7b],
    /* OP_jl_short    */   &first_byte[0x7c],
    /* OP_jnl_short   */   &first_byte[0x7d],
    /* OP_jle_short   */   &first_byte[0x7e],
    /* OP_jnle_short  */   &first_byte[0x7f],

    /* OP_call          */   &first_byte[0xe8],
    /* OP_call_ind      */   &extensions[12][2],
    /* OP_call_far      */   &first_byte[0x9a],
    /* OP_call_far_ind  */   &extensions[12][3],
    /* OP_jmp           */   &first_byte[0xe9],
    /* OP_jmp_short     */   &first_byte[0xeb],
    /* OP_jmp_ind       */   &extensions[12][4],
    /* OP_jmp_far       */   &first_byte[0xea],
    /* OP_jmp_far_ind   */   &extensions[12][5],

    /* OP_loopne  */   &first_byte[0xe0],
    /* OP_loope   */   &first_byte[0xe1],
    /* OP_loop    */   &first_byte[0xe2],
    /* OP_jecxz   */   &first_byte[0xe3],

    /* point ld & st at eAX & al instrs, they save 1 byte (no modrm),
     * hopefully time taken considering them doesn't offset that */
    /* OP_mov_ld     */   &first_byte[0xa1],
    /* OP_mov_st     */   &first_byte[0xa3],
    /* PR 250397: store of immed is mov_st not mov_imm, even though can be immed->reg,
     * which we address by sharing part of the mov_st template chain */
    /* OP_mov_imm    */   &first_byte[0xb8],
    /* OP_mov_seg    */   &first_byte[0x8e],
    /* OP_mov_priv   */   &second_byte[0x20],

    /* OP_test    */   &first_byte[0xa9],
    /* OP_lea     */   &first_byte[0x8d],
    /* OP_xchg    */   &first_byte[0x91],
    /* OP_cwde    */   &first_byte[0x98],
    /* OP_cdq     */   &first_byte[0x99],
    /* OP_fwait   */   &first_byte[0x9b],
    /* OP_pushf   */   &first_byte[0x9c],
    /* OP_popf    */   &first_byte[0x9d],
    /* OP_sahf    */   &first_byte[0x9e],
    /* OP_lahf    */   &first_byte[0x9f],

    /* OP_ret      */   &first_byte[0xc2],
    /* OP_ret_far  */   &first_byte[0xca],

    /* OP_les     */   &vex_prefix_extensions[0][0],
    /* OP_lds     */   &vex_prefix_extensions[1][0],
    /* OP_enter   */   &first_byte[0xc8],
    /* OP_leave   */   &first_byte[0xc9],
    /* OP_int3    */   &first_byte[0xcc],
    /* OP_int     */   &first_byte[0xcd],
    /* OP_into    */   &first_byte[0xce],
    /* OP_iret    */   &first_byte[0xcf],
    /* OP_aam     */   &first_byte[0xd4],
    /* OP_aad     */   &first_byte[0xd5],
    /* OP_xlat    */   &first_byte[0xd7],
    /* OP_in      */   &first_byte[0xe5],
    /* OP_out     */   &first_byte[0xe7],
    /* OP_hlt     */   &first_byte[0xf4],
    /* OP_cmc     */   &first_byte[0xf5],
    /* OP_clc     */   &first_byte[0xf8],
    /* OP_stc     */   &first_byte[0xf9],
    /* OP_cli     */   &first_byte[0xfa],
    /* OP_sti     */   &first_byte[0xfb],
    /* OP_cld     */   &first_byte[0xfc],
    /* OP_std     */   &first_byte[0xfd],


    /* OP_lar         */   &second_byte[0x02],
    /* OP_lsl         */   &second_byte[0x03],
    /* OP_syscall     */   &second_byte[0x05],
    /* OP_clts        */   &second_byte[0x06],
    /* OP_sysret      */   &second_byte[0x07],
    /* OP_invd        */   &second_byte[0x08],
    /* OP_wbinvd      */   &second_byte[0x09],
    /* OP_ud2a        */   &second_byte[0x0b],
    /* OP_nop_modrm   */   &second_byte[0x1f],
    /* OP_movntps     */   &prefix_extensions[11][0],
    /* OP_movntpd     */   &prefix_extensions[11][2],
    /* OP_wrmsr       */   &second_byte[0x30],
    /* OP_rdtsc       */   &second_byte[0x31],
    /* OP_rdmsr       */   &second_byte[0x32],
    /* OP_rdpmc       */   &second_byte[0x33],
    /* OP_sysenter    */   &second_byte[0x34],
    /* OP_sysexit     */   &second_byte[0x35],

    /* OP_cmovo       */   &second_byte[0x40],
    /* OP_cmovno      */   &second_byte[0x41],
    /* OP_cmovb       */   &second_byte[0x42],
    /* OP_cmovnb      */   &second_byte[0x43],
    /* OP_cmovz       */   &second_byte[0x44],
    /* OP_cmovnz      */   &second_byte[0x45],
    /* OP_cmovbe      */   &second_byte[0x46],
    /* OP_cmovnbe     */   &second_byte[0x47],
    /* OP_cmovs       */   &second_byte[0x48],
    /* OP_cmovns      */   &second_byte[0x49],
    /* OP_cmovp       */   &second_byte[0x4a],
    /* OP_cmovnp      */   &second_byte[0x4b],
    /* OP_cmovl       */   &second_byte[0x4c],
    /* OP_cmovnl      */   &second_byte[0x4d],
    /* OP_cmovle      */   &second_byte[0x4e],
    /* OP_cmovnle     */   &second_byte[0x4f],

    /* OP_punpcklbw   */   &prefix_extensions[32][0],
    /* OP_punpcklwd   */   &prefix_extensions[33][0],
    /* OP_punpckldq   */   &prefix_extensions[34][0],
    /* OP_packsswb    */   &prefix_extensions[35][0],
    /* OP_pcmpgtb     */   &prefix_extensions[36][0],
    /* OP_pcmpgtw     */   &prefix_extensions[37][0],
    /* OP_pcmpgtd     */   &prefix_extensions[38][0],
    /* OP_packuswb    */   &prefix_extensions[39][0],
    /* OP_punpckhbw   */   &prefix_extensions[40][0],
    /* OP_punpckhwd   */   &prefix_extensions[41][0],
    /* OP_punpckhdq   */   &prefix_extensions[42][0],
    /* OP_packssdw    */   &prefix_extensions[43][0],
    /* OP_punpcklqdq  */   &prefix_extensions[44][2],
    /* OP_punpckhqdq  */   &prefix_extensions[45][2],
    /* OP_movd        */   &prefix_extensions[46][0],
    /* OP_movq        */   &prefix_extensions[112][0],
    /* OP_movdqu      */   &prefix_extensions[112][1],
    /* OP_movdqa      */   &prefix_extensions[112][2],
    /* OP_pshufw      */   &prefix_extensions[47][0],
    /* OP_pshufd      */   &prefix_extensions[47][2],
    /* OP_pshufhw     */   &prefix_extensions[47][1],
    /* OP_pshuflw     */   &prefix_extensions[47][3],
    /* OP_pcmpeqb     */   &prefix_extensions[48][0],
    /* OP_pcmpeqw     */   &prefix_extensions[49][0],
    /* OP_pcmpeqd     */   &prefix_extensions[50][0],
    /* OP_emms        */   &vex_L_extensions[0][0],

    /* OP_jo      */   &second_byte[0x80],
    /* OP_jno     */   &second_byte[0x81],
    /* OP_jb      */   &second_byte[0x82],
    /* OP_jnb     */   &second_byte[0x83],
    /* OP_jz      */   &second_byte[0x84],
    /* OP_jnz     */   &second_byte[0x85],
    /* OP_jbe     */   &second_byte[0x86],
    /* OP_jnbe    */   &second_byte[0x87],
    /* OP_js      */   &second_byte[0x88],
    /* OP_jns     */   &second_byte[0x89],
    /* OP_jp      */   &second_byte[0x8a],
    /* OP_jnp     */   &second_byte[0x8b],
    /* OP_jl      */   &second_byte[0x8c],
    /* OP_jnl     */   &second_byte[0x8d],
    /* OP_jle     */   &second_byte[0x8e],
    /* OP_jnle    */   &second_byte[0x8f],

    /* OP_seto        */   &second_byte[0x90],
    /* OP_setno       */   &second_byte[0x91],
    /* OP_setb        */   &second_byte[0x92],
    /* OP_setnb       */   &second_byte[0x93],
    /* OP_setz        */   &second_byte[0x94],
    /* OP_setnz       */   &second_byte[0x95],
    /* OP_setbe       */   &second_byte[0x96],
    /* OP_setnbe        */   &second_byte[0x97],
    /* OP_sets        */   &second_byte[0x98],
    /* OP_setns       */   &second_byte[0x99],
    /* OP_setp        */   &second_byte[0x9a],
    /* OP_setnp       */   &second_byte[0x9b],
    /* OP_setl        */   &second_byte[0x9c],
    /* OP_setnl       */   &second_byte[0x9d],
    /* OP_setle       */   &second_byte[0x9e],
    /* OP_setnle        */   &second_byte[0x9f],

    /* OP_cpuid       */   &second_byte[0xa2],
    /* OP_bt          */   &second_byte[0xa3],
    /* OP_shld        */   &second_byte[0xa4],
    /* OP_rsm         */   &second_byte[0xaa],
    /* OP_bts         */   &second_byte[0xab],
    /* OP_shrd        */   &second_byte[0xac],
    /* OP_cmpxchg     */   &second_byte[0xb1],
    /* OP_lss         */   &second_byte[0xb2],
    /* OP_btr         */   &second_byte[0xb3],
    /* OP_lfs         */   &second_byte[0xb4],
    /* OP_lgs         */   &second_byte[0xb5],
    /* OP_movzx       */   &second_byte[0xb7],
    /* OP_ud2b        */   &second_byte[0xb9],
    /* OP_btc         */   &second_byte[0xbb],
    /* OP_bsf         */   &prefix_extensions[140][0],
    /* OP_bsr         */   &prefix_extensions[136][0],
    /* OP_movsx       */   &second_byte[0xbf],
    /* OP_xadd        */   &second_byte[0xc1],
    /* OP_movnti      */   &second_byte[0xc3],
    /* OP_pinsrw      */   &prefix_extensions[53][0],
    /* OP_pextrw      */   &prefix_extensions[54][0],
    /* OP_bswap       */   &second_byte[0xc8],
    /* OP_psrlw       */   &prefix_extensions[56][0],
    /* OP_psrld       */   &prefix_extensions[57][0],
    /* OP_psrlq       */   &prefix_extensions[58][0],
    /* OP_paddq       */   &prefix_extensions[59][0],
    /* OP_pmullw      */   &prefix_extensions[60][0],
    /* OP_pmovmskb    */   &prefix_extensions[62][0],
    /* OP_psubusb     */   &prefix_extensions[63][0],
    /* OP_psubusw     */   &prefix_extensions[64][0],
    /* OP_pminub      */   &prefix_extensions[65][0],
    /* OP_pand        */   &prefix_extensions[66][0],
    /* OP_paddusb     */   &prefix_extensions[67][0],
    /* OP_paddusw     */   &prefix_extensions[68][0],
    /* OP_pmaxub      */   &prefix_extensions[69][0],
    /* OP_pandn       */   &prefix_extensions[70][0],
    /* OP_pavgb       */   &prefix_extensions[71][0],
    /* OP_psraw       */   &prefix_extensions[72][0],
    /* OP_psrad       */   &prefix_extensions[73][0],
    /* OP_pavgw       */   &prefix_extensions[74][0],
    /* OP_pmulhuw     */   &prefix_extensions[75][0],
    /* OP_pmulhw      */   &prefix_extensions[76][0],
    /* OP_movntq      */   &prefix_extensions[78][0],
    /* OP_movntdq     */   &prefix_extensions[78][2],
    /* OP_psubsb      */   &prefix_extensions[79][0],
    /* OP_psubsw      */   &prefix_extensions[80][0],
    /* OP_pminsw      */   &prefix_extensions[81][0],
    /* OP_por         */   &prefix_extensions[82][0],
    /* OP_paddsb      */   &prefix_extensions[83][0],
    /* OP_paddsw      */   &prefix_extensions[84][0],
    /* OP_pmaxsw      */   &prefix_extensions[85][0],
    /* OP_pxor        */   &prefix_extensions[86][0],
    /* OP_psllw       */   &prefix_extensions[87][0],
    /* OP_pslld       */   &prefix_extensions[88][0],
    /* OP_psllq       */   &prefix_extensions[89][0],
    /* OP_pmuludq     */   &prefix_extensions[90][0],
    /* OP_pmaddwd     */   &prefix_extensions[91][0],
    /* OP_psadbw      */   &prefix_extensions[92][0],
    /* OP_maskmovq    */   &prefix_extensions[93][0],
    /* OP_maskmovdqu  */   &prefix_extensions[93][2],
    /* OP_psubb       */   &prefix_extensions[94][0],
    /* OP_psubw       */   &prefix_extensions[95][0],
    /* OP_psubd       */   &prefix_extensions[96][0],
    /* OP_psubq       */   &prefix_extensions[97][0],
    /* OP_paddb       */   &prefix_extensions[98][0],
    /* OP_paddw       */   &prefix_extensions[99][0],
    /* OP_paddd       */   &prefix_extensions[100][0],
    /* OP_psrldq      */   &prefix_extensions[101][2],
    /* OP_pslldq      */   &prefix_extensions[102][2],


    /* OP_rol          */   &extensions[ 4][0],
    /* OP_ror          */   &extensions[ 4][1],
    /* OP_rcl          */   &extensions[ 4][2],
    /* OP_rcr          */   &extensions[ 4][3],
    /* OP_shl          */   &extensions[ 4][4],
    /* OP_shr          */   &extensions[ 4][5],
    /* OP_sar          */   &extensions[ 4][7],
    /* OP_not          */   &extensions[10][2],
    /* OP_neg          */   &extensions[10][3],
    /* OP_mul          */   &extensions[10][4],
    /* OP_div          */   &extensions[10][6],
    /* OP_idiv         */   &extensions[10][7],
    /* OP_sldt         */   &extensions[13][0],
    /* OP_str          */   &extensions[13][1],
    /* OP_lldt         */   &extensions[13][2],
    /* OP_ltr          */   &extensions[13][3],
    /* OP_verr         */   &extensions[13][4],
    /* OP_verw         */   &extensions[13][5],
    /* OP_sgdt         */   &mod_extensions[0][0],
    /* OP_sidt         */   &mod_extensions[1][0],
    /* OP_lgdt         */   &mod_extensions[5][0],
    /* OP_lidt         */   &mod_extensions[4][0],
    /* OP_smsw         */   &extensions[14][4],
    /* OP_lmsw         */   &extensions[14][6],
    /* OP_invlpg       */   &mod_extensions[2][0],
    /* OP_cmpxchg8b    */   &extensions[16][1],
    /* OP_fxsave32     */   &rex_w_extensions[0][0],
    /* OP_fxrstor32    */   &rex_w_extensions[1][0],
    /* OP_ldmxcsr      */   &vex_extensions[61][0],
    /* OP_stmxcsr      */   &vex_extensions[62][0],
    /* OP_lfence       */   &mod_extensions[6][1],
    /* OP_mfence       */   &mod_extensions[7][1],
    /* OP_clflush      */   &mod_extensions[3][0],
    /* OP_sfence       */   &mod_extensions[3][1],
    /* OP_prefetchnta  */   &extensions[23][0],
    /* OP_prefetcht0   */   &extensions[23][1],
    /* OP_prefetcht1   */   &extensions[23][2],
    /* OP_prefetcht2   */   &extensions[23][3],
    /* OP_prefetch     */   &extensions[24][0],
    /* OP_prefetchw    */   &extensions[24][1],


    /* OP_movups     */   &prefix_extensions[ 0][0],
    /* OP_movss      */   &mod_extensions[18][0],
    /* OP_movupd     */   &prefix_extensions[ 0][2],
    /* OP_movsd      */   &mod_extensions[19][0],
    /* OP_movlps     */   &prefix_extensions[ 2][0],
    /* OP_movlpd     */   &prefix_extensions[ 2][2],
    /* OP_unpcklps   */   &prefix_extensions[ 4][0],
    /* OP_unpcklpd   */   &prefix_extensions[ 4][2],
    /* OP_unpckhps   */   &prefix_extensions[ 5][0],
    /* OP_unpckhpd   */   &prefix_extensions[ 5][2],
    /* OP_movhps     */   &prefix_extensions[ 6][0],
    /* OP_movhpd     */   &prefix_extensions[ 6][2],
    /* OP_movaps     */   &prefix_extensions[ 8][0],
    /* OP_movapd     */   &prefix_extensions[ 8][2],
    /* OP_cvtpi2ps   */   &prefix_extensions[10][0],
    /* OP_cvtsi2ss   */   &prefix_extensions[10][1],
    /* OP_cvtpi2pd   */   &prefix_extensions[10][2],
    /* OP_cvtsi2sd   */   &prefix_extensions[10][3],
    /* OP_cvttps2pi  */   &prefix_extensions[12][0],
    /* OP_cvttss2si  */   &prefix_extensions[12][1],
    /* OP_cvttpd2pi  */   &prefix_extensions[12][2],
    /* OP_cvttsd2si  */   &prefix_extensions[12][3],
    /* OP_cvtps2pi   */   &prefix_extensions[13][0],
    /* OP_cvtss2si   */   &prefix_extensions[13][1],
    /* OP_cvtpd2pi   */   &prefix_extensions[13][2],
    /* OP_cvtsd2si   */   &prefix_extensions[13][3],
    /* OP_ucomiss    */   &prefix_extensions[14][0],
    /* OP_ucomisd    */   &prefix_extensions[14][2],
    /* OP_comiss     */   &prefix_extensions[15][0],
    /* OP_comisd     */   &prefix_extensions[15][2],
    /* OP_movmskps   */   &prefix_extensions[16][0],
    /* OP_movmskpd   */   &prefix_extensions[16][2],
    /* OP_sqrtps     */   &prefix_extensions[17][0],
    /* OP_sqrtss     */   &prefix_extensions[17][1],
    /* OP_sqrtpd     */   &prefix_extensions[17][2],
    /* OP_sqrtsd     */   &prefix_extensions[17][3],
    /* OP_rsqrtps    */   &prefix_extensions[18][0],
    /* OP_rsqrtss    */   &prefix_extensions[18][1],
    /* OP_rcpps      */   &prefix_extensions[19][0],
    /* OP_rcpss      */   &prefix_extensions[19][1],
    /* OP_andps      */   &prefix_extensions[20][0],
    /* OP_andpd      */   &prefix_extensions[20][2],
    /* OP_andnps     */   &prefix_extensions[21][0],
    /* OP_andnpd     */   &prefix_extensions[21][2],
    /* OP_orps       */   &prefix_extensions[22][0],
    /* OP_orpd       */   &prefix_extensions[22][2],
    /* OP_xorps      */   &prefix_extensions[23][0],
    /* OP_xorpd      */   &prefix_extensions[23][2],
    /* OP_addps      */   &prefix_extensions[24][0],
    /* OP_addss      */   &prefix_extensions[24][1],
    /* OP_addpd      */   &prefix_extensions[24][2],
    /* OP_addsd      */   &prefix_extensions[24][3],
    /* OP_mulps      */   &prefix_extensions[25][0],
    /* OP_mulss      */   &prefix_extensions[25][1],
    /* OP_mulpd      */   &prefix_extensions[25][2],
    /* OP_mulsd      */   &prefix_extensions[25][3],
    /* OP_cvtps2pd   */   &prefix_extensions[26][0],
    /* OP_cvtss2sd   */   &prefix_extensions[26][1],
    /* OP_cvtpd2ps   */   &prefix_extensions[26][2],
    /* OP_cvtsd2ss   */   &prefix_extensions[26][3],
    /* OP_cvtdq2ps   */   &prefix_extensions[27][0],
    /* OP_cvttps2dq  */   &prefix_extensions[27][1],
    /* OP_cvtps2dq   */   &prefix_extensions[27][2],
    /* OP_subps      */   &prefix_extensions[28][0],
    /* OP_subss      */   &prefix_extensions[28][1],
    /* OP_subpd      */   &prefix_extensions[28][2],
    /* OP_subsd      */   &prefix_extensions[28][3],
    /* OP_minps      */   &prefix_extensions[29][0],
    /* OP_minss      */   &prefix_extensions[29][1],
    /* OP_minpd      */   &prefix_extensions[29][2],
    /* OP_minsd      */   &prefix_extensions[29][3],
    /* OP_divps      */   &prefix_extensions[30][0],
    /* OP_divss      */   &prefix_extensions[30][1],
    /* OP_divpd      */   &prefix_extensions[30][2],
    /* OP_divsd      */   &prefix_extensions[30][3],
    /* OP_maxps      */   &prefix_extensions[31][0],
    /* OP_maxss      */   &prefix_extensions[31][1],
    /* OP_maxpd      */   &prefix_extensions[31][2],
    /* OP_maxsd      */   &prefix_extensions[31][3],
    /* OP_cmpps      */   &prefix_extensions[52][0],
    /* OP_cmpss      */   &prefix_extensions[52][1],
    /* OP_cmppd      */   &prefix_extensions[52][2],
    /* OP_cmpsd      */   &prefix_extensions[52][3],
    /* OP_shufps     */   &prefix_extensions[55][0],
    /* OP_shufpd     */   &prefix_extensions[55][2],
    /* OP_cvtdq2pd   */   &prefix_extensions[77][1],
    /* OP_cvttpd2dq  */   &prefix_extensions[77][2],
    /* OP_cvtpd2dq   */   &prefix_extensions[77][3],
    /* OP_nop        */   &rex_b_extensions[0][0],
    /* OP_pause      */   &prefix_extensions[103][1],

    /* OP_ins         */   &rep_extensions[1][0],
    /* OP_rep_ins     */   &rep_extensions[1][2],
    /* OP_outs        */   &rep_extensions[3][0],
    /* OP_rep_outs    */   &rep_extensions[3][2],
    /* OP_movs        */   &rep_extensions[5][0],
    /* OP_rep_movs    */   &rep_extensions[5][2],
    /* OP_stos        */   &rep_extensions[7][0],
    /* OP_rep_stos    */   &rep_extensions[7][2],
    /* OP_lods        */   &rep_extensions[9][0],
    /* OP_rep_lods    */   &rep_extensions[9][2],
    /* OP_cmps        */   &repne_extensions[1][0],
    /* OP_rep_cmps    */   &repne_extensions[1][2],
    /* OP_repne_cmps  */   &repne_extensions[1][4],
    /* OP_scas        */   &repne_extensions[3][0],
    /* OP_rep_scas    */   &repne_extensions[3][2],
    /* OP_repne_scas  */   &repne_extensions[3][4],


    /* OP_fadd     */   &float_low_modrm[0x00],
    /* OP_fmul     */   &float_low_modrm[0x01],
    /* OP_fcom     */   &float_low_modrm[0x02],
    /* OP_fcomp    */   &float_low_modrm[0x03],
    /* OP_fsub     */   &float_low_modrm[0x04],
    /* OP_fsubr    */   &float_low_modrm[0x05],
    /* OP_fdiv     */   &float_low_modrm[0x06],
    /* OP_fdivr    */   &float_low_modrm[0x07],
    /* OP_fld      */   &float_low_modrm[0x08],
    /* OP_fst      */   &float_low_modrm[0x0a],
    /* OP_fstp     */   &float_low_modrm[0x0b],
    /* OP_fldenv   */   &float_low_modrm[0x0c],
    /* OP_fldcw    */   &float_low_modrm[0x0d],
    /* OP_fnstenv  */   &float_low_modrm[0x0e],
    /* OP_fnstcw   */   &float_low_modrm[0x0f],
    /* OP_fiadd    */   &float_low_modrm[0x10],
    /* OP_fimul    */   &float_low_modrm[0x11],
    /* OP_ficom    */   &float_low_modrm[0x12],
    /* OP_ficomp   */   &float_low_modrm[0x13],
    /* OP_fisub    */   &float_low_modrm[0x14],
    /* OP_fisubr   */   &float_low_modrm[0x15],
    /* OP_fidiv    */   &float_low_modrm[0x16],
    /* OP_fidivr   */   &float_low_modrm[0x17],
    /* OP_fild     */   &float_low_modrm[0x18],
    /* OP_fist     */   &float_low_modrm[0x1a],
    /* OP_fistp    */   &float_low_modrm[0x1b],
    /* OP_frstor   */   &float_low_modrm[0x2c],
    /* OP_fnsave   */   &float_low_modrm[0x2e],
    /* OP_fnstsw   */   &float_low_modrm[0x2f],

    /* OP_fbld     */   &float_low_modrm[0x3c],
    /* OP_fbstp    */   &float_low_modrm[0x3e],


    /* OP_fxch      */   &float_high_modrm[1][0x08],
    /* OP_fnop      */   &float_high_modrm[1][0x10],
    /* OP_fchs      */   &float_high_modrm[1][0x20],
    /* OP_fabs      */   &float_high_modrm[1][0x21],
    /* OP_ftst      */   &float_high_modrm[1][0x24],
    /* OP_fxam      */   &float_high_modrm[1][0x25],
    /* OP_fld1      */   &float_high_modrm[1][0x28],
    /* OP_fldl2t    */   &float_high_modrm[1][0x29],
    /* OP_fldl2e    */   &float_high_modrm[1][0x2a],
    /* OP_fldpi     */   &float_high_modrm[1][0x2b],
    /* OP_fldlg2    */   &float_high_modrm[1][0x2c],
    /* OP_fldln2    */   &float_high_modrm[1][0x2d],
    /* OP_fldz      */   &float_high_modrm[1][0x2e],
    /* OP_f2xm1     */   &float_high_modrm[1][0x30],
    /* OP_fyl2x     */   &float_high_modrm[1][0x31],
    /* OP_fptan     */   &float_high_modrm[1][0x32],
    /* OP_fpatan    */   &float_high_modrm[1][0x33],
    /* OP_fxtract   */   &float_high_modrm[1][0x34],
    /* OP_fprem1    */   &float_high_modrm[1][0x35],
    /* OP_fdecstp   */   &float_high_modrm[1][0x36],
    /* OP_fincstp   */   &float_high_modrm[1][0x37],
    /* OP_fprem     */   &float_high_modrm[1][0x38],
    /* OP_fyl2xp1   */   &float_high_modrm[1][0x39],
    /* OP_fsqrt     */   &float_high_modrm[1][0x3a],
    /* OP_fsincos   */   &float_high_modrm[1][0x3b],
    /* OP_frndint   */   &float_high_modrm[1][0x3c],
    /* OP_fscale    */   &float_high_modrm[1][0x3d],
    /* OP_fsin      */   &float_high_modrm[1][0x3e],
    /* OP_fcos      */   &float_high_modrm[1][0x3f],
    /* OP_fcmovb    */   &float_high_modrm[2][0x00],
    /* OP_fcmove    */   &float_high_modrm[2][0x08],
    /* OP_fcmovbe   */   &float_high_modrm[2][0x10],
    /* OP_fcmovu    */   &float_high_modrm[2][0x18],
    /* OP_fucompp   */   &float_high_modrm[2][0x29],
    /* OP_fcmovnb   */   &float_high_modrm[3][0x00],
    /* OP_fcmovne   */   &float_high_modrm[3][0x08],
    /* OP_fcmovnbe  */   &float_high_modrm[3][0x10],
    /* OP_fcmovnu   */   &float_high_modrm[3][0x18],
    /* OP_fnclex    */   &float_high_modrm[3][0x22],
    /* OP_fninit    */   &float_high_modrm[3][0x23],
    /* OP_fucomi    */   &float_high_modrm[3][0x28],
    /* OP_fcomi     */   &float_high_modrm[3][0x30],
    /* OP_ffree     */   &float_high_modrm[5][0x00],
    /* OP_fucom     */   &float_high_modrm[5][0x20],
    /* OP_fucomp    */   &float_high_modrm[5][0x28],
    /* OP_faddp     */   &float_high_modrm[6][0x00],
    /* OP_fmulp     */   &float_high_modrm[6][0x08],
    /* OP_fcompp    */   &float_high_modrm[6][0x19],
    /* OP_fsubrp    */   &float_high_modrm[6][0x20],
    /* OP_fsubp     */   &float_high_modrm[6][0x28],
    /* OP_fdivrp    */   &float_high_modrm[6][0x30],
    /* OP_fdivp     */   &float_high_modrm[6][0x38],
    /* OP_fucomip   */   &float_high_modrm[7][0x28],
    /* OP_fcomip    */   &float_high_modrm[7][0x30],

    /* SSE3 instructions */
    /* OP_fisttp      */   &float_low_modrm[0x29],
    /* OP_haddpd      */   &prefix_extensions[114][2],
    /* OP_haddps      */   &prefix_extensions[114][3],
    /* OP_hsubpd      */   &prefix_extensions[115][2],
    /* OP_hsubps      */   &prefix_extensions[115][3],
    /* OP_addsubpd    */   &prefix_extensions[116][2],
    /* OP_addsubps    */   &prefix_extensions[116][3],
    /* OP_lddqu       */   &prefix_extensions[117][3],
    /* OP_monitor     */    &rm_extensions[1][0],
    /* OP_mwait       */    &rm_extensions[1][1],
    /* OP_movsldup    */   &prefix_extensions[ 2][1],
    /* OP_movshdup    */   &prefix_extensions[ 6][1],
    /* OP_movddup     */   &prefix_extensions[ 2][3],

    /* 3D-Now! instructions */
    /* OP_femms         */   &second_byte[0x0e],
    /* OP_unknown_3dnow */   &suffix_extensions[0],
    /* OP_pavgusb       */   &suffix_extensions[1],
    /* OP_pfadd         */   &suffix_extensions[2],
    /* OP_pfacc         */   &suffix_extensions[3],
    /* OP_pfcmpge       */   &suffix_extensions[4],
    /* OP_pfcmpgt       */   &suffix_extensions[5],
    /* OP_pfcmpeq       */   &suffix_extensions[6],
    /* OP_pfmin         */   &suffix_extensions[7],
    /* OP_pfmax         */   &suffix_extensions[8],
    /* OP_pfmul         */   &suffix_extensions[9],
    /* OP_pfrcp         */   &suffix_extensions[10],
    /* OP_pfrcpit1      */   &suffix_extensions[11],
    /* OP_pfrcpit2      */   &suffix_extensions[12],
    /* OP_pfrsqrt       */   &suffix_extensions[13],
    /* OP_pfrsqit1      */   &suffix_extensions[14],
    /* OP_pmulhrw       */   &suffix_extensions[15],
    /* OP_pfsub         */   &suffix_extensions[16],
    /* OP_pfsubr        */   &suffix_extensions[17],
    /* OP_pi2fd         */   &suffix_extensions[18],
    /* OP_pf2id         */   &suffix_extensions[19],
    /* OP_pi2fw         */   &suffix_extensions[20],
    /* OP_pf2iw         */   &suffix_extensions[21],
    /* OP_pfnacc        */   &suffix_extensions[22],
    /* OP_pfpnacc       */   &suffix_extensions[23],
    /* OP_pswapd        */   &suffix_extensions[24],

    /* SSSE3 */
    /* OP_pshufb        */   &prefix_extensions[118][0],
    /* OP_phaddw        */   &prefix_extensions[119][0],
    /* OP_phaddd        */   &prefix_extensions[120][0],
    /* OP_phaddsw       */   &prefix_extensions[121][0],
    /* OP_pmaddubsw     */   &prefix_extensions[122][0],
    /* OP_phsubw        */   &prefix_extensions[123][0],
    /* OP_phsubd        */   &prefix_extensions[124][0],
    /* OP_phsubsw       */   &prefix_extensions[125][0],
    /* OP_psignb        */   &prefix_extensions[126][0],
    /* OP_psignw        */   &prefix_extensions[127][0],
    /* OP_psignd        */   &prefix_extensions[128][0],
    /* OP_pmulhrsw      */   &prefix_extensions[129][0],
    /* OP_pabsb         */   &prefix_extensions[130][0],
    /* OP_pabsw         */   &prefix_extensions[131][0],
    /* OP_pabsd         */   &prefix_extensions[132][0],
    /* OP_palignr       */   &prefix_extensions[133][0],

    /* SSE4 (incl AMD (SSE4A) and Intel-specific (SSE4.1, SSE4.2) extensions */
    /* OP_popcnt        */   &second_byte[0xb8],
    /* OP_movntss       */   &prefix_extensions[11][1],
    /* OP_movntsd       */   &prefix_extensions[11][3],
    /* OP_extrq         */   &prefix_extensions[134][2],
    /* OP_insertq       */   &prefix_extensions[134][3],
    /* OP_lzcnt         */   &prefix_extensions[136][1],
    /* OP_pblendvb      */   &third_byte_38[16],
    /* OP_blendvps      */   &third_byte_38[17],
    /* OP_blendvpd      */   &third_byte_38[18],
    /* OP_ptest         */   &vex_extensions[3][0],
    /* OP_pmovsxbw      */   &vex_extensions[4][0],
    /* OP_pmovsxbd      */   &vex_extensions[5][0],
    /* OP_pmovsxbq      */   &vex_extensions[6][0],
    /* OP_pmovsxwd      */   &vex_extensions[7][0],
    /* OP_pmovsxwq      */   &vex_extensions[8][0],
    /* OP_pmovsxdq      */   &vex_extensions[9][0],
    /* OP_pmuldq        */   &vex_extensions[10][0],
    /* OP_pcmpeqq       */   &vex_extensions[11][0],
    /* OP_movntdqa      */   &vex_extensions[12][0],
    /* OP_packusdw      */   &vex_extensions[13][0],
    /* OP_pmovzxbw      */   &vex_extensions[14][0],
    /* OP_pmovzxbd      */   &vex_extensions[15][0],
    /* OP_pmovzxbq      */   &vex_extensions[16][0],
    /* OP_pmovzxwd      */   &vex_extensions[17][0],
    /* OP_pmovzxwq      */   &vex_extensions[18][0],
    /* OP_pmovzxdq      */   &vex_extensions[19][0],
    /* OP_pcmpgtq       */   &vex_extensions[20][0],
    /* OP_pminsb        */   &vex_extensions[21][0],
    /* OP_pminsd        */   &vex_extensions[22][0],
    /* OP_pminuw        */   &vex_extensions[23][0],
    /* OP_pminud        */   &vex_extensions[24][0],
    /* OP_pmaxsb        */   &vex_extensions[25][0],
    /* OP_pmaxsd        */   &vex_extensions[26][0],
    /* OP_pmaxuw        */   &vex_extensions[27][0],
    /* OP_pmaxud        */   &vex_extensions[28][0],
    /* OP_pmulld        */   &vex_extensions[29][0],
    /* OP_phminposuw    */   &vex_extensions[30][0],
    /* OP_crc32         */   &prefix_extensions[139][3],
    /* OP_pextrb        */   &vex_extensions[36][0],
    /* OP_pextrd        */   &vex_extensions[38][0],
    /* OP_extractps     */   &vex_extensions[39][0],
    /* OP_roundps       */   &vex_extensions[40][0],
    /* OP_roundpd       */   &vex_extensions[41][0],
    /* OP_roundss       */   &vex_extensions[42][0],
    /* OP_roundsd       */   &vex_extensions[43][0],
    /* OP_blendps       */   &vex_extensions[44][0],
    /* OP_blendpd       */   &vex_extensions[45][0],
    /* OP_pblendw       */   &vex_extensions[46][0],
    /* OP_pinsrb        */   &vex_extensions[47][0],
    /* OP_insertps      */   &vex_extensions[48][0],
    /* OP_pinsrd        */   &vex_extensions[49][0],
    /* OP_dpps          */   &vex_extensions[50][0],
    /* OP_dppd          */   &vex_extensions[51][0],
    /* OP_mpsadbw       */   &vex_extensions[52][0],
    /* OP_pcmpestrm     */   &vex_extensions[53][0],
    /* OP_pcmpestri     */   &vex_extensions[54][0],
    /* OP_pcmpistrm     */   &vex_extensions[55][0],
    /* OP_pcmpistri     */   &vex_extensions[56][0],

    /* x64 */
    /* OP_movsxd        */   &x64_extensions[16][1],
    /* OP_swapgs        */   &rm_extensions[2][0],

    /* VMX */
    /* OP_vmcall        */   &rm_extensions[0][1],
    /* OP_vmlaunch      */   &rm_extensions[0][2],
    /* OP_vmresume      */   &rm_extensions[0][3],
    /* OP_vmxoff        */   &rm_extensions[0][4],
    /* OP_vmptrst       */   &mod_extensions[13][0],
    /* OP_vmptrld       */   &prefix_extensions[137][0],
    /* OP_vmxon         */   &prefix_extensions[137][1],
    /* OP_vmclear       */   &prefix_extensions[137][2],
    /* OP_vmread        */   &prefix_extensions[134][0],
    /* OP_vmwrite       */   &prefix_extensions[135][0],

    /* undocumented */
    /* OP_int1          */   &first_byte[0xf1],
    /* OP_salc          */   &first_byte[0xd6],
    /* OP_ffreep        */   &float_high_modrm[7][0x00],

    /* AMD SVM */
    /* OP_vmrun         */   &rm_extensions[3][0],
    /* OP_vmmcall       */   &rm_extensions[3][1],
    /* OP_vmload        */   &rm_extensions[3][2],
    /* OP_vmsave        */   &rm_extensions[3][3],
    /* OP_stgi          */   &rm_extensions[3][4],
    /* OP_clgi          */   &rm_extensions[3][5],
    /* OP_skinit        */   &rm_extensions[3][6],
    /* OP_invlpga       */   &rm_extensions[3][7],
    /* AMD though not part of SVM */
    /* OP_rdtscp        */   &rm_extensions[2][1],

    /* Intel VMX additions */
    /* OP_invept        */   &third_byte_38[49],
    /* OP_invvpid       */   &third_byte_38[50],

    /* added in Intel Westmere */
    /* OP_pclmulqdq     */   &vex_extensions[57][0],
    /* OP_aesimc        */   &vex_extensions[31][0],
    /* OP_aesenc        */   &vex_extensions[32][0],
    /* OP_aesenclast    */   &vex_extensions[33][0],
    /* OP_aesdec        */   &vex_extensions[34][0],
    /* OP_aesdeclast    */   &vex_extensions[35][0],
    /* OP_aeskeygenassist*/  &vex_extensions[58][0],

    /* added in Intel Atom */
    /* OP_movbe         */   &prefix_extensions[138][0],

    /* added in Intel Sandy Bridge */
    /* OP_xgetbv        */   &rm_extensions[4][0],
    /* OP_xsetbv        */   &rm_extensions[4][1],
    /* OP_xsave32       */   &rex_w_extensions[2][0],
    /* OP_xrstor32      */   &rex_w_extensions[3][0],
    /* OP_xsaveopt32    */   &rex_w_extensions[4][0],

    /* AVX */
    /* OP_vmovss        */  &mod_extensions[ 8][0],
    /* OP_vmovsd        */  &mod_extensions[ 9][0],
    /* OP_vmovups       */  &prefix_extensions[ 0][4],
    /* OP_vmovupd       */  &prefix_extensions[ 0][6],
    /* OP_vmovlps       */  &prefix_extensions[ 2][4],
    /* OP_vmovsldup     */  &prefix_extensions[ 2][5],
    /* OP_vmovlpd       */  &prefix_extensions[ 2][6],
    /* OP_vmovddup      */  &prefix_extensions[ 2][7],
    /* OP_vunpcklps     */  &prefix_extensions[ 4][4],
    /* OP_vunpcklpd     */  &prefix_extensions[ 4][6],
    /* OP_vunpckhps     */  &prefix_extensions[ 5][4],
    /* OP_vunpckhpd     */  &prefix_extensions[ 5][6],
    /* OP_vmovhps       */  &prefix_extensions[ 6][4],
    /* OP_vmovshdup     */  &prefix_extensions[ 6][5],
    /* OP_vmovhpd       */  &prefix_extensions[ 6][6],
    /* OP_vmovaps       */  &prefix_extensions[ 8][4],
    /* OP_vmovapd       */  &prefix_extensions[ 8][6],
    /* OP_vcvtsi2ss     */  &prefix_extensions[10][5],
    /* OP_vcvtsi2sd     */  &prefix_extensions[10][7],
    /* OP_vmovntps      */  &prefix_extensions[11][4],
    /* OP_vmovntpd      */  &prefix_extensions[11][6],
    /* OP_vcvttss2si    */  &prefix_extensions[12][5],
    /* OP_vcvttsd2si    */  &prefix_extensions[12][7],
    /* OP_vcvtss2si     */  &prefix_extensions[13][5],
    /* OP_vcvtsd2si     */  &prefix_extensions[13][7],
    /* OP_vucomiss      */  &prefix_extensions[14][4],
    /* OP_vucomisd      */  &prefix_extensions[14][6],
    /* OP_vcomiss       */  &prefix_extensions[15][4],
    /* OP_vcomisd       */  &prefix_extensions[15][6],
    /* OP_vmovmskps     */  &prefix_extensions[16][4],
    /* OP_vmovmskpd     */  &prefix_extensions[16][6],
    /* OP_vsqrtps       */  &prefix_extensions[17][4],
    /* OP_vsqrtss       */  &prefix_extensions[17][5],
    /* OP_vsqrtpd       */  &prefix_extensions[17][6],
    /* OP_vsqrtsd       */  &prefix_extensions[17][7],
    /* OP_vrsqrtps      */  &prefix_extensions[18][4],
    /* OP_vrsqrtss      */  &prefix_extensions[18][5],
    /* OP_vrcpps        */  &prefix_extensions[19][4],
    /* OP_vrcpss        */  &prefix_extensions[19][5],
    /* OP_vandps        */  &prefix_extensions[20][4],
    /* OP_vandpd        */  &prefix_extensions[20][6],
    /* OP_vandnps       */  &prefix_extensions[21][4],
    /* OP_vandnpd       */  &prefix_extensions[21][6],
    /* OP_vorps         */  &prefix_extensions[22][4],
    /* OP_vorpd         */  &prefix_extensions[22][6],
    /* OP_vxorps        */  &prefix_extensions[23][4],
    /* OP_vxorpd        */  &prefix_extensions[23][6],
    /* OP_vaddps        */  &prefix_extensions[24][4],
    /* OP_vaddss        */  &prefix_extensions[24][5],
    /* OP_vaddpd        */  &prefix_extensions[24][6],
    /* OP_vaddsd        */  &prefix_extensions[24][7],
    /* OP_vmulps        */  &prefix_extensions[25][4],
    /* OP_vmulss        */  &prefix_extensions[25][5],
    /* OP_vmulpd        */  &prefix_extensions[25][6],
    /* OP_vmulsd        */  &prefix_extensions[25][7],
    /* OP_vcvtps2pd     */  &prefix_extensions[26][4],
    /* OP_vcvtss2sd     */  &prefix_extensions[26][5],
    /* OP_vcvtpd2ps     */  &prefix_extensions[26][6],
    /* OP_vcvtsd2ss     */  &prefix_extensions[26][7],
    /* OP_vcvtdq2ps     */  &prefix_extensions[27][4],
    /* OP_vcvttps2dq    */  &prefix_extensions[27][5],
    /* OP_vcvtps2dq     */  &prefix_extensions[27][6],
    /* OP_vsubps        */  &prefix_extensions[28][4],
    /* OP_vsubss        */  &prefix_extensions[28][5],
    /* OP_vsubpd        */  &prefix_extensions[28][6],
    /* OP_vsubsd        */  &prefix_extensions[28][7],
    /* OP_vminps        */  &prefix_extensions[29][4],
    /* OP_vminss        */  &prefix_extensions[29][5],
    /* OP_vminpd        */  &prefix_extensions[29][6],
    /* OP_vminsd        */  &prefix_extensions[29][7],
    /* OP_vdivps        */  &prefix_extensions[30][4],
    /* OP_vdivss        */  &prefix_extensions[30][5],
    /* OP_vdivpd        */  &prefix_extensions[30][6],
    /* OP_vdivsd        */  &prefix_extensions[30][7],
    /* OP_vmaxps        */  &prefix_extensions[31][4],
    /* OP_vmaxss        */  &prefix_extensions[31][5],
    /* OP_vmaxpd        */  &prefix_extensions[31][6],
    /* OP_vmaxsd        */  &prefix_extensions[31][7],
    /* OP_vpunpcklbw    */  &prefix_extensions[32][6],
    /* OP_vpunpcklwd    */  &prefix_extensions[33][6],
    /* OP_vpunpckldq    */  &prefix_extensions[34][6],
    /* OP_vpacksswb     */  &prefix_extensions[35][6],
    /* OP_vpcmpgtb      */  &prefix_extensions[36][6],
    /* OP_vpcmpgtw      */  &prefix_extensions[37][6],
    /* OP_vpcmpgtd      */  &prefix_extensions[38][6],
    /* OP_vpackuswb     */  &prefix_extensions[39][6],
    /* OP_vpunpckhbw    */  &prefix_extensions[40][6],
    /* OP_vpunpckhwd    */  &prefix_extensions[41][6],
    /* OP_vpunpckhdq    */  &prefix_extensions[42][6],
    /* OP_vpackssdw     */  &prefix_extensions[43][6],
    /* OP_vpunpcklqdq   */  &prefix_extensions[44][6],
    /* OP_vpunpckhqdq   */  &prefix_extensions[45][6],
    /* OP_vmovd         */  &prefix_extensions[46][6],
    /* OP_vpshufhw      */  &prefix_extensions[47][5],
    /* OP_vpshufd       */  &prefix_extensions[47][6],
    /* OP_vpshuflw      */  &prefix_extensions[47][7],
    /* OP_vpcmpeqb      */  &prefix_extensions[48][6],
    /* OP_vpcmpeqw      */  &prefix_extensions[49][6],
    /* OP_vpcmpeqd      */  &prefix_extensions[50][6],
    /* OP_vmovq         */  &prefix_extensions[51][5],
    /* OP_vcmpps        */  &prefix_extensions[52][4],
    /* OP_vcmpss        */  &prefix_extensions[52][5],
    /* OP_vcmppd        */  &prefix_extensions[52][6],
    /* OP_vcmpsd        */  &prefix_extensions[52][7],
    /* OP_vpinsrw       */  &prefix_extensions[53][6],
    /* OP_vpextrw       */  &prefix_extensions[54][6],
    /* OP_vshufps       */  &prefix_extensions[55][4],
    /* OP_vshufpd       */  &prefix_extensions[55][6],
    /* OP_vpsrlw        */  &prefix_extensions[56][6],
    /* OP_vpsrld        */  &prefix_extensions[57][6],
    /* OP_vpsrlq        */  &prefix_extensions[58][6],
    /* OP_vpaddq        */  &prefix_extensions[59][6],
    /* OP_vpmullw       */  &prefix_extensions[60][6],
    /* OP_vpmovmskb     */  &prefix_extensions[62][6],
    /* OP_vpsubusb      */  &prefix_extensions[63][6],
    /* OP_vpsubusw      */  &prefix_extensions[64][6],
    /* OP_vpminub       */  &prefix_extensions[65][6],
    /* OP_vpand         */  &prefix_extensions[66][6],
    /* OP_vpaddusb      */  &prefix_extensions[67][6],
    /* OP_vpaddusw      */  &prefix_extensions[68][6],
    /* OP_vpmaxub       */  &prefix_extensions[69][6],
    /* OP_vpandn        */  &prefix_extensions[70][6],
    /* OP_vpavgb        */  &prefix_extensions[71][6],
    /* OP_vpsraw        */  &prefix_extensions[72][6],
    /* OP_vpsrad        */  &prefix_extensions[73][6],
    /* OP_vpavgw        */  &prefix_extensions[74][6],
    /* OP_vpmulhuw      */  &prefix_extensions[75][6],
    /* OP_vpmulhw       */  &prefix_extensions[76][6],
    /* OP_vcvtdq2pd     */  &prefix_extensions[77][5],
    /* OP_vcvttpd2dq    */  &prefix_extensions[77][6],
    /* OP_vcvtpd2dq     */  &prefix_extensions[77][7],
    /* OP_vmovntdq      */  &prefix_extensions[78][6],
    /* OP_vpsubsb       */  &prefix_extensions[79][6],
    /* OP_vpsubsw       */  &prefix_extensions[80][6],
    /* OP_vpminsw       */  &prefix_extensions[81][6],
    /* OP_vpor          */  &prefix_extensions[82][6],
    /* OP_vpaddsb       */  &prefix_extensions[83][6],
    /* OP_vpaddsw       */  &prefix_extensions[84][6],
    /* OP_vpmaxsw       */  &prefix_extensions[85][6],
    /* OP_vpxor         */  &prefix_extensions[86][6],
    /* OP_vpsllw        */  &prefix_extensions[87][6],
    /* OP_vpslld        */  &prefix_extensions[88][6],
    /* OP_vpsllq        */  &prefix_extensions[89][6],
    /* OP_vpmuludq      */  &prefix_extensions[90][6],
    /* OP_vpmaddwd      */  &prefix_extensions[91][6],
    /* OP_vpsadbw       */  &prefix_extensions[92][6],
    /* OP_vmaskmovdqu   */  &prefix_extensions[93][6],
    /* OP_vpsubb        */  &prefix_extensions[94][6],
    /* OP_vpsubw        */  &prefix_extensions[95][6],
    /* OP_vpsubd        */  &prefix_extensions[96][6],
    /* OP_vpsubq        */  &prefix_extensions[97][6],
    /* OP_vpaddb        */  &prefix_extensions[98][6],
    /* OP_vpaddw        */  &prefix_extensions[99][6],
    /* OP_vpaddd        */  &prefix_extensions[100][6],
    /* OP_vpsrldq       */  &prefix_extensions[101][6],
    /* OP_vpslldq       */  &prefix_extensions[102][6],
    /* OP_vmovdqu       */  &prefix_extensions[112][5],
    /* OP_vmovdqa       */  &prefix_extensions[112][6],
    /* OP_vhaddpd       */  &prefix_extensions[114][6],
    /* OP_vhaddps       */  &prefix_extensions[114][7],
    /* OP_vhsubpd       */  &prefix_extensions[115][6],
    /* OP_vhsubps       */  &prefix_extensions[115][7],
    /* OP_vaddsubpd     */  &prefix_extensions[116][6],
    /* OP_vaddsubps     */  &prefix_extensions[116][7],
    /* OP_vlddqu        */  &prefix_extensions[117][7],
    /* OP_vpshufb       */  &prefix_extensions[118][6],
    /* OP_vphaddw       */  &prefix_extensions[119][6],
    /* OP_vphaddd       */  &prefix_extensions[120][6],
    /* OP_vphaddsw      */  &prefix_extensions[121][6],
    /* OP_vpmaddubsw    */  &prefix_extensions[122][6],
    /* OP_vphsubw       */  &prefix_extensions[123][6],
    /* OP_vphsubd       */  &prefix_extensions[124][6],
    /* OP_vphsubsw      */  &prefix_extensions[125][6],
    /* OP_vpsignb       */  &prefix_extensions[126][6],
    /* OP_vpsignw       */  &prefix_extensions[127][6],
    /* OP_vpsignd       */  &prefix_extensions[128][6],
    /* OP_vpmulhrsw     */  &prefix_extensions[129][6],
    /* OP_vpabsb        */  &prefix_extensions[130][6],
    /* OP_vpabsw        */  &prefix_extensions[131][6],
    /* OP_vpabsd        */  &prefix_extensions[132][6],
    /* OP_vpalignr      */  &prefix_extensions[133][6],
    /* OP_vpblendvb     */  &vex_extensions[ 2][1],
    /* OP_vblendvps     */  &vex_extensions[ 0][1],
    /* OP_vblendvpd     */  &vex_extensions[ 1][1],
    /* OP_vptest        */  &vex_extensions[ 3][1],
    /* OP_vpmovsxbw     */  &vex_extensions[ 4][1],
    /* OP_vpmovsxbd     */  &vex_extensions[ 5][1],
    /* OP_vpmovsxbq     */  &vex_extensions[ 6][1],
    /* OP_vpmovsxwd     */  &vex_extensions[ 7][1],
    /* OP_vpmovsxwq     */  &vex_extensions[ 8][1],
    /* OP_vpmovsxdq     */  &vex_extensions[ 9][1],
    /* OP_vpmuldq       */  &vex_extensions[10][1],
    /* OP_vpcmpeqq      */  &vex_extensions[11][1],
    /* OP_vmovntdqa     */  &vex_extensions[12][1],
    /* OP_vpackusdw     */  &vex_extensions[13][1],
    /* OP_vpmovzxbw     */  &vex_extensions[14][1],
    /* OP_vpmovzxbd     */  &vex_extensions[15][1],
    /* OP_vpmovzxbq     */  &vex_extensions[16][1],
    /* OP_vpmovzxwd     */  &vex_extensions[17][1],
    /* OP_vpmovzxwq     */  &vex_extensions[18][1],
    /* OP_vpmovzxdq     */  &vex_extensions[19][1],
    /* OP_vpcmpgtq      */  &vex_extensions[20][1],
    /* OP_vpminsb       */  &vex_extensions[21][1],
    /* OP_vpminsd       */  &vex_extensions[22][1],
    /* OP_vpminuw       */  &vex_extensions[23][1],
    /* OP_vpminud       */  &vex_extensions[24][1],
    /* OP_vpmaxsb       */  &vex_extensions[25][1],
    /* OP_vpmaxsd       */  &vex_extensions[26][1],
    /* OP_vpmaxuw       */  &vex_extensions[27][1],
    /* OP_vpmaxud       */  &vex_extensions[28][1],
    /* OP_vpmulld       */  &vex_extensions[29][1],
    /* OP_vphminposuw   */  &vex_extensions[30][1],
    /* OP_vaesimc       */  &vex_extensions[31][1],
    /* OP_vaesenc       */  &vex_extensions[32][1],
    /* OP_vaesenclast   */  &vex_extensions[33][1],
    /* OP_vaesdec       */  &vex_extensions[34][1],
    /* OP_vaesdeclast   */  &vex_extensions[35][1],
    /* OP_vpextrb       */  &vex_extensions[36][1],
    /* OP_vpextrd       */  &vex_extensions[38][1],
    /* OP_vextractps    */  &vex_extensions[39][1],
    /* OP_vroundps      */  &vex_extensions[40][1],
    /* OP_vroundpd      */  &vex_extensions[41][1],
    /* OP_vroundss      */  &vex_extensions[42][1],
    /* OP_vroundsd      */  &vex_extensions[43][1],
    /* OP_vblendps      */  &vex_extensions[44][1],
    /* OP_vblendpd      */  &vex_extensions[45][1],
    /* OP_vpblendw      */  &vex_extensions[46][1],
    /* OP_vpinsrb       */  &vex_extensions[47][1],
    /* OP_vinsertps     */  &vex_extensions[48][1],
    /* OP_vpinsrd       */  &vex_extensions[49][1],
    /* OP_vdpps         */  &vex_extensions[50][1],
    /* OP_vdppd         */  &vex_extensions[51][1],
    /* OP_vmpsadbw      */  &vex_extensions[52][1],
    /* OP_vpcmpestrm    */  &vex_extensions[53][1],
    /* OP_vpcmpestri    */  &vex_extensions[54][1],
    /* OP_vpcmpistrm    */  &vex_extensions[55][1],
    /* OP_vpcmpistri    */  &vex_extensions[56][1],
    /* OP_vpclmulqdq    */  &vex_extensions[57][1],
    /* OP_vaeskeygenassist*/ &vex_extensions[58][1],
    /* OP_vtestps       */  &vex_extensions[59][1],
    /* OP_vtestpd       */  &vex_extensions[60][1],
    /* OP_vzeroupper    */  &vex_L_extensions[0][1],
    /* OP_vzeroall      */  &vex_L_extensions[0][2],
    /* OP_vldmxcsr      */  &vex_extensions[61][1],
    /* OP_vstmxcsr      */  &vex_extensions[62][1],
    /* OP_vbroadcastss  */  &vex_extensions[64][1],
    /* OP_vbroadcastsd  */  &vex_extensions[65][1],
    /* OP_vbroadcastf128*/  &vex_extensions[66][1],
    /* OP_vmaskmovps    */  &vex_extensions[67][1],
    /* OP_vmaskmovpd    */  &vex_extensions[68][1],
    /* OP_vpermilps     */  &vex_extensions[71][1],
    /* OP_vpermilpd     */  &vex_extensions[72][1],
    /* OP_vperm2f128    */  &vex_extensions[73][1],
    /* OP_vinsertf128   */  &vex_extensions[74][1],
    /* OP_vextractf128  */  &vex_extensions[75][1],

    /* added in Ivy Bridge I believe, and covered by F16C cpuid flag */
    /* OP_vcvtph2ps     */  &vex_extensions[63][1],
    /* OP_vcvtps2ph     */  &vex_extensions[76][1],

    /* FMA */
    /* OP_vfmadd132ps   */  &vex_W_extensions[ 0][0],
    /* OP_vfmadd132pd   */  &vex_W_extensions[ 0][1],
    /* OP_vfmadd213ps   */  &vex_W_extensions[ 1][0],
    /* OP_vfmadd213pd   */  &vex_W_extensions[ 1][1],
    /* OP_vfmadd231ps   */  &vex_W_extensions[ 2][0],
    /* OP_vfmadd231pd   */  &vex_W_extensions[ 2][1],
    /* OP_vfmadd132ss   */  &vex_W_extensions[ 3][0],
    /* OP_vfmadd132sd   */  &vex_W_extensions[ 3][1],
    /* OP_vfmadd213ss   */  &vex_W_extensions[ 4][0],
    /* OP_vfmadd213sd   */  &vex_W_extensions[ 4][1],
    /* OP_vfmadd231ss   */  &vex_W_extensions[ 5][0],
    /* OP_vfmadd231sd   */  &vex_W_extensions[ 5][1],
    /* OP_vfmaddsub132ps*/  &vex_W_extensions[ 6][0],
    /* OP_vfmaddsub132pd*/  &vex_W_extensions[ 6][1],
    /* OP_vfmaddsub213ps*/  &vex_W_extensions[ 7][0],
    /* OP_vfmaddsub213pd*/  &vex_W_extensions[ 7][1],
    /* OP_vfmaddsub231ps*/  &vex_W_extensions[ 8][0],
    /* OP_vfmaddsub231pd*/  &vex_W_extensions[ 8][1],
    /* OP_vfmsubadd132ps*/  &vex_W_extensions[ 9][0],
    /* OP_vfmsubadd132pd*/  &vex_W_extensions[ 9][1],
    /* OP_vfmsubadd213ps*/  &vex_W_extensions[10][0],
    /* OP_vfmsubadd213pd*/  &vex_W_extensions[10][1],
    /* OP_vfmsubadd231ps*/  &vex_W_extensions[11][0],
    /* OP_vfmsubadd231pd*/  &vex_W_extensions[11][1],
    /* OP_vfmsub132ps   */  &vex_W_extensions[12][0],
    /* OP_vfmsub132pd   */  &vex_W_extensions[12][1],
    /* OP_vfmsub213ps   */  &vex_W_extensions[13][0],
    /* OP_vfmsub213pd   */  &vex_W_extensions[13][1],
    /* OP_vfmsub231ps   */  &vex_W_extensions[14][0],
    /* OP_vfmsub231pd   */  &vex_W_extensions[14][1],
    /* OP_vfmsub132ss   */  &vex_W_extensions[15][0],
    /* OP_vfmsub132sd   */  &vex_W_extensions[15][1],
    /* OP_vfmsub213ss   */  &vex_W_extensions[16][0],
    /* OP_vfmsub213sd   */  &vex_W_extensions[16][1],
    /* OP_vfmsub231ss   */  &vex_W_extensions[17][0],
    /* OP_vfmsub231sd   */  &vex_W_extensions[17][1],
    /* OP_vfnmadd132ps  */  &vex_W_extensions[18][0],
    /* OP_vfnmadd132pd  */  &vex_W_extensions[18][1],
    /* OP_vfnmadd213ps  */  &vex_W_extensions[19][0],
    /* OP_vfnmadd213pd  */  &vex_W_extensions[19][1],
    /* OP_vfnmadd231ps  */  &vex_W_extensions[20][0],
    /* OP_vfnmadd231pd  */  &vex_W_extensions[20][1],
    /* OP_vfnmadd132ss  */  &vex_W_extensions[21][0],
    /* OP_vfnmadd132sd  */  &vex_W_extensions[21][1],
    /* OP_vfnmadd213ss  */  &vex_W_extensions[22][0],
    /* OP_vfnmadd213sd  */  &vex_W_extensions[22][1],
    /* OP_vfnmadd231ss  */  &vex_W_extensions[23][0],
    /* OP_vfnmadd231sd  */  &vex_W_extensions[23][1],
    /* OP_vfnmsub132ps  */  &vex_W_extensions[24][0],
    /* OP_vfnmsub132pd  */  &vex_W_extensions[24][1],
    /* OP_vfnmsub213ps  */  &vex_W_extensions[25][0],
    /* OP_vfnmsub213pd  */  &vex_W_extensions[25][1],
    /* OP_vfnmsub231ps  */  &vex_W_extensions[26][0],
    /* OP_vfnmsub231pd  */  &vex_W_extensions[26][1],
    /* OP_vfnmsub132ss  */  &vex_W_extensions[27][0],
    /* OP_vfnmsub132sd  */  &vex_W_extensions[27][1],
    /* OP_vfnmsub213ss  */  &vex_W_extensions[28][0],
    /* OP_vfnmsub213sd  */  &vex_W_extensions[28][1],
    /* OP_vfnmsub231ss  */  &vex_W_extensions[29][0],
    /* OP_vfnmsub231sd  */  &vex_W_extensions[29][1],

    /* SSE2 that were omitted before */
    /* OP_movq2dq       */  &prefix_extensions[61][1],
    /* OP_movdq2q       */  &prefix_extensions[61][3],

    /* OP_fxsave64      */   &rex_w_extensions[0][1],
    /* OP_fxrstor64     */   &rex_w_extensions[1][1],
    /* OP_xsave64       */   &rex_w_extensions[2][1],
    /* OP_xrstor64      */   &rex_w_extensions[3][1],
    /* OP_xsaveopt64    */   &rex_w_extensions[4][1],

    /* added in Intel Ivy Bridge: RDRAND and FSGSBASE cpuid flags */
    /* OP_rdrand        */   &mod_extensions[12][1],
    /* OP_rdfsbase      */   &mod_extensions[14][1],
    /* OP_rdgsbase      */   &mod_extensions[15][1],
    /* OP_wrfsbase      */   &mod_extensions[16][1],
    /* OP_wrgsbase      */   &mod_extensions[17][1],

    /* coming in the future but adding now since enough details are known */
    /* OP_rdseed        */   &mod_extensions[13][1],

    /* AMD FMA4 */
    /* OP_vfmaddsubps   */   &vex_W_extensions[30][0],
    /* OP_vfmaddsubpd   */   &vex_W_extensions[31][0],
    /* OP_vfmsubaddps   */   &vex_W_extensions[32][0],
    /* OP_vfmsubaddpd   */   &vex_W_extensions[33][0],
    /* OP_vfmaddps      */   &vex_W_extensions[34][0],
    /* OP_vfmaddpd      */   &vex_W_extensions[35][0],
    /* OP_vfmaddss      */   &vex_W_extensions[36][0],
    /* OP_vfmaddsd      */   &vex_W_extensions[37][0],
    /* OP_vfmsubps      */   &vex_W_extensions[38][0],
    /* OP_vfmsubpd      */   &vex_W_extensions[39][0],
    /* OP_vfmsubss      */   &vex_W_extensions[40][0],
    /* OP_vfmsubsd      */   &vex_W_extensions[41][0],
    /* OP_vfnmaddps     */   &vex_W_extensions[42][0],
    /* OP_vfnmaddpd     */   &vex_W_extensions[43][0],
    /* OP_vfnmaddss     */   &vex_W_extensions[44][0],
    /* OP_vfnmaddsd     */   &vex_W_extensions[45][0],
    /* OP_vfnmsubps     */   &vex_W_extensions[46][0],
    /* OP_vfnmsubpd     */   &vex_W_extensions[47][0],
    /* OP_vfnmsubss     */   &vex_W_extensions[48][0],
    /* OP_vfnmsubsd     */   &vex_W_extensions[49][0],

    /* AMD XOP */
    /* OP_vfrczps       */   &xop_extensions[27],
    /* OP_vfrczpd       */   &xop_extensions[28],
    /* OP_vfrczss       */   &xop_extensions[29],
    /* OP_vfrczsd       */   &xop_extensions[30],
    /* OP_vpcmov        */   &vex_W_extensions[50][0],
    /* OP_vpcomb        */   &xop_extensions[19],
    /* OP_vpcomw        */   &xop_extensions[20],
    /* OP_vpcomd        */   &xop_extensions[21],
    /* OP_vpcomq        */   &xop_extensions[22],
    /* OP_vpcomub       */   &xop_extensions[23],
    /* OP_vpcomuw       */   &xop_extensions[24],
    /* OP_vpcomud       */   &xop_extensions[25],
    /* OP_vpcomuq       */   &xop_extensions[26],
    /* OP_vpermil2pd    */   &vex_W_extensions[65][0],
    /* OP_vpermil2ps    */   &vex_W_extensions[64][0],
    /* OP_vphaddbw      */   &xop_extensions[43],
    /* OP_vphaddbd      */   &xop_extensions[44],
    /* OP_vphaddbq      */   &xop_extensions[45],
    /* OP_vphaddwd      */   &xop_extensions[46],
    /* OP_vphaddwq      */   &xop_extensions[47],
    /* OP_vphadddq      */   &xop_extensions[48],
    /* OP_vphaddubw     */   &xop_extensions[49],
    /* OP_vphaddubd     */   &xop_extensions[50],
    /* OP_vphaddubq     */   &xop_extensions[51],
    /* OP_vphadduwd     */   &xop_extensions[52],
    /* OP_vphadduwq     */   &xop_extensions[53],
    /* OP_vphaddudq     */   &xop_extensions[54],
    /* OP_vphsubbw      */   &xop_extensions[55],
    /* OP_vphsubwd      */   &xop_extensions[56],
    /* OP_vphsubdq      */   &xop_extensions[57],
    /* OP_vpmacssww     */   &xop_extensions[ 1],
    /* OP_vpmacsswd     */   &xop_extensions[ 2],
    /* OP_vpmacssdql    */   &xop_extensions[ 3],
    /* OP_vpmacssdd     */   &xop_extensions[ 4],
    /* OP_vpmacssdqh    */   &xop_extensions[ 5],
    /* OP_vpmacsww      */   &xop_extensions[ 6],
    /* OP_vpmacswd      */   &xop_extensions[ 7],
    /* OP_vpmacsdql     */   &xop_extensions[ 8],
    /* OP_vpmacsdd      */   &xop_extensions[ 9],
    /* OP_vpmacsdqh     */   &xop_extensions[10],
    /* OP_vpmadcsswd    */   &xop_extensions[13],
    /* OP_vpmadcswd     */   &xop_extensions[14],
    /* OP_vpperm        */   &vex_W_extensions[51][0],
    /* OP_vprotb        */   &xop_extensions[15],
    /* OP_vprotw        */   &xop_extensions[16],
    /* OP_vprotd        */   &xop_extensions[17],
    /* OP_vprotq        */   &xop_extensions[18],
    /* OP_vpshlb        */   &vex_W_extensions[56][0],
    /* OP_vpshlw        */   &vex_W_extensions[57][0],
    /* OP_vpshld        */   &vex_W_extensions[58][0],
    /* OP_vpshlq        */   &vex_W_extensions[59][0],
    /* OP_vpshab        */   &vex_W_extensions[60][0],
    /* OP_vpshaw        */   &vex_W_extensions[61][0],
    /* OP_vpshad        */   &vex_W_extensions[62][0],
    /* OP_vpshaq        */   &vex_W_extensions[63][0],

    /* AMD TBM */
    /* OP_bextr         */   &prefix_extensions[141][4],
    /* OP_blcfill       */   &extensions[27][1],
    /* OP_blci          */   &extensions[28][6],
    /* OP_blcic         */   &extensions[27][5],
    /* OP_blcmsk        */   &extensions[28][1],
    /* OP_blcs          */   &extensions[27][3],
    /* OP_blsfill       */   &extensions[27][2],
    /* OP_blsic         */   &extensions[27][6],
    /* OP_t1mskc        */   &extensions[27][7],
    /* OP_tzmsk         */   &extensions[27][4],

    /* AMD LWP */
    /* OP_llwpcb        */   &extensions[29][0],
    /* OP_slwpcb        */   &extensions[29][1],
    /* OP_lwpins        */   &extensions[30][0],
    /* OP_lwpval        */   &extensions[30][1],

    /* Intel BMI1 */
    /* (includes non-immed form of OP_bextr) */
    /* OP_andn          */   &third_byte_38[100],
    /* OP_blsr          */   &extensions[31][1],
    /* OP_blsmsk        */   &extensions[31][2],
    /* OP_blsi          */   &extensions[31][3],
    /* OP_tzcnt         */   &prefix_extensions[140][1],

    /* Intel BMI2 */
    /* OP_bzhi          */   &prefix_extensions[142][4],
    /* OP_pext          */   &prefix_extensions[142][6],
    /* OP_pdep          */   &prefix_extensions[142][7],
    /* OP_sarx          */   &prefix_extensions[141][5],
    /* OP_shlx          */   &prefix_extensions[141][6],
    /* OP_shrx          */   &prefix_extensions[141][7],
    /* OP_rorx          */   &third_byte_3a[56],
    /* OP_mulx          */   &prefix_extensions[143][7],

    /* Intel Safer Mode Extensions */
    /* OP_getsec        */   &second_byte[0x37],

    /* Misc Intel additions */
    /* OP_vmfunc        */   &rm_extensions[4][4],
    /* OP_invpcid       */   &third_byte_38[103],

    /* Intel TSX */
    /* OP_xabort        */   &extensions[17][7],
    /* OP_xbegin        */   &extensions[18][7],
    /* OP_xend          */   &rm_extensions[4][5],
    /* OP_xtest         */   &rm_extensions[4][6],

    /* AVX2 */
    /* OP_vpgatherdd    */   &vex_W_extensions[66][0],
    /* OP_vpgatherdq    */   &vex_W_extensions[66][1],
    /* OP_vpgatherqd    */   &vex_W_extensions[67][0],
    /* OP_vpgatherqq    */   &vex_W_extensions[67][1],
    /* OP_vgatherdps    */   &vex_W_extensions[68][0],
    /* OP_vgatherdpd    */   &vex_W_extensions[68][1],
    /* OP_vgatherqps    */   &vex_W_extensions[69][0],
    /* OP_vgatherqpd    */   &vex_W_extensions[69][1],
    /* OP_vbroadcasti128 */  &third_byte_38[108],
    /* OP_vinserti128   */   &third_byte_3a[57],
    /* OP_vextracti128  */   &third_byte_3a[58],
    /* OP_vpmaskmovd    */   &vex_W_extensions[70][0],
    /* OP_vpmaskmovq    */   &vex_W_extensions[70][1],
    /* OP_vperm2i128    */   &third_byte_3a[62],
    /* OP_vpermd        */   &third_byte_38[112],
    /* OP_vpermps       */   &third_byte_38[111],
    /* OP_vpermq        */   &third_byte_3a[59],
    /* OP_vpermpd       */   &third_byte_3a[60],
    /* OP_vpblendd      */   &third_byte_3a[61],
    /* OP_vpsllvd       */   &vex_W_extensions[73][0],
    /* OP_vpsllvq       */   &vex_W_extensions[73][1],
    /* OP_vpsravd       */   &third_byte_38[114],
    /* OP_vpsrlvd       */   &vex_W_extensions[72][0],
    /* OP_vpsrlvq       */   &vex_W_extensions[72][1],
    /* OP_vpbroadcastb  */   &third_byte_38[116],
    /* OP_vpbroadcastw  */   &third_byte_38[117],
    /* OP_vpbroadcastd  */   &third_byte_38[118],
    /* OP_vpbroadcastq  */   &third_byte_38[119],

    /* Keep these at the end so that ifdefs don't change internal enum values */
#ifdef IA32_ON_IA64
    /* OP_jmpe      */   &extensions[13][6],
    /* OP_jmpe_abs  */   &second_byte[0xb8],
#endif
};


/****************************************************************************
 * Macros to make tables legible
 */

/* Jb is defined in dynamo.h, undefine it for this file */
#undef Jb

#define xx  TYPE_NONE, OPSZ_NA

/* from Intel tables, using our corresponding OPSZ constants */
#define Ap  TYPE_A, OPSZ_6_irex10_short4 /* NOTE - not legal for 64-bit instructions */
#define By  TYPE_B, OPSZ_4_rex8
#define Cr  TYPE_C, OPSZ_4x8
#define Dr  TYPE_D, OPSZ_4x8
#define Eb  TYPE_E, OPSZ_1
#define Ew  TYPE_E, OPSZ_2
#define Ev  TYPE_E, OPSZ_4_rex8_short2
#define Esv TYPE_E, OPSZ_4x8_short2 /* "stack v", or "d64" in Intel tables */
#define Ed  TYPE_E, OPSZ_4
#define Ep  TYPE_E, OPSZ_6_irex10_short4
#define Ed_q TYPE_E, OPSZ_4_rex8
#define Ey  TYPE_E, OPSZ_4_rex8
#define Rd_Mb TYPE_E, OPSZ_1_reg4
#define Rd_Mw TYPE_E, OPSZ_2_reg4
#define Gb  TYPE_G, OPSZ_1
#define Gw  TYPE_G, OPSZ_2
#define Gv  TYPE_G, OPSZ_4_rex8_short2
#define Gz  TYPE_G, OPSZ_4_short2
#define Gd  TYPE_G, OPSZ_4
#define Gd_q TYPE_G, OPSZ_4_rex8
#define Gr  TYPE_G, OPSZ_4x8
#define Gy  TYPE_G, OPSZ_4_rex8
#define Ib  TYPE_I, OPSZ_1
#define Iw  TYPE_I, OPSZ_2
#define Id  TYPE_I, OPSZ_4
#define Iv  TYPE_I, OPSZ_4_rex8_short2
#define Iz  TYPE_I, OPSZ_4_short2
#define Jb  TYPE_J, OPSZ_1
#define Jz  TYPE_J, OPSZ_4_short2xi4
#define Ma  TYPE_M, OPSZ_8_short4
#define Mp  TYPE_M, OPSZ_6_irex10_short4
#define Ms  TYPE_M, OPSZ_6x10
#define Ob  TYPE_O, OPSZ_1
#define Ov  TYPE_O, OPSZ_4_rex8_short2
#define Pd  TYPE_P, OPSZ_4
#define Pq  TYPE_P, OPSZ_8
#define Pw_q TYPE_P, OPSZ_2_of_8
#define Pd_q TYPE_P, OPSZ_4_of_8
#define Ppi TYPE_P, OPSZ_8
#define Nw_q  TYPE_P_MODRM, OPSZ_2_of_8
#define Nq  TYPE_P_MODRM, OPSZ_8
#define Qd  TYPE_Q, OPSZ_4
#define Qq  TYPE_Q, OPSZ_8
#define Qpi TYPE_Q, OPSZ_8
#define Rr  TYPE_R, OPSZ_4x8
#define Rv  TYPE_R, OPSZ_4_rex8_short2
#define Ry  TYPE_R, OPSZ_4_rex8
#define Sw  TYPE_S, OPSZ_2
#define Vq  TYPE_V, OPSZ_8
#define Vdq TYPE_V, OPSZ_16
#define Vb_dq TYPE_V, OPSZ_1_of_16
#define Vw_dq TYPE_V, OPSZ_2_of_16
#define Vd_dq TYPE_V, OPSZ_4_of_16
#define Vd_q_dq TYPE_V, OPSZ_4_rex8_of_16
#define Vq_dq TYPE_V, OPSZ_8_of_16
#define Vps TYPE_V, OPSZ_16
#define Vpd TYPE_V, OPSZ_16
#define Vss TYPE_V, OPSZ_4_of_16
#define Vsd TYPE_V, OPSZ_8_of_16
#define Ups TYPE_V_MODRM, OPSZ_16
#define Upd TYPE_V_MODRM, OPSZ_16
#define Udq TYPE_V_MODRM, OPSZ_16
#define Uw_dq TYPE_V_MODRM, OPSZ_2_of_16
#define Uq_dq TYPE_V_MODRM, OPSZ_8_of_16
#define Wq  TYPE_W, OPSZ_8
#define Wdq TYPE_W, OPSZ_16
#define Wb_dq TYPE_W, OPSZ_1_of_16
#define Ww_dq TYPE_W, OPSZ_2_of_16
#define Wd_dq TYPE_W, OPSZ_4_of_16
#define Wq_dq TYPE_W, OPSZ_8_of_16
#define Wps TYPE_W, OPSZ_16
#define Wpd TYPE_W, OPSZ_16
#define Wss TYPE_W, OPSZ_4_of_16
#define Wsd TYPE_W, OPSZ_8_of_16
#define Udq_Md TYPE_W, OPSZ_4_reg16
#define Xb  TYPE_X, OPSZ_1
#define Xv  TYPE_X, OPSZ_4_rex8_short2
#define Xz  TYPE_X, OPSZ_4_short2
#define Yb  TYPE_Y, OPSZ_1
#define Yv  TYPE_Y, OPSZ_4_rex8_short2
#define Yz  TYPE_Y, OPSZ_4_short2

/* AVX additions */
#define Vvs TYPE_V, OPSZ_16_vex32
#define Vvd TYPE_V, OPSZ_16_vex32
#define Vx TYPE_V, OPSZ_16_vex32
#define Vqq TYPE_V, OPSZ_32
#define Vdq_qq TYPE_V, OPSZ_16_of_32
#define Wvs TYPE_W, OPSZ_16_vex32
#define Wvd TYPE_W, OPSZ_16_vex32
#define Wx TYPE_W, OPSZ_16_vex32
#define Uvs TYPE_V_MODRM, OPSZ_16_vex32
#define Uvd TYPE_V_MODRM, OPSZ_16_vex32
#define Uss TYPE_V_MODRM, OPSZ_4_of_16
#define Usd TYPE_V_MODRM, OPSZ_8_of_16
#define Ux TYPE_V_MODRM, OPSZ_16_vex32
#define Udq TYPE_V_MODRM, OPSZ_16
#define Hvs TYPE_H, OPSZ_16_vex32
#define Hvd TYPE_H, OPSZ_16_vex32
#define Hss TYPE_H, OPSZ_4_of_16
#define Hsd TYPE_H, OPSZ_8_of_16
#define Hq_dq TYPE_H, OPSZ_8_of_16
#define Hdq TYPE_H, OPSZ_16
#define H12_dq TYPE_H, OPSZ_12_of_16
#define H12_8_dq TYPE_H, OPSZ_12_rex8_of_16
#define H14_dq TYPE_H, OPSZ_14_of_16
#define H15_dq TYPE_H, OPSZ_15_of_16
#define Hqq TYPE_H, OPSZ_32
#define Hx TYPE_H, OPSZ_16_vex32
#define Wvq_dq TYPE_W, OPSZ_8_of_16_vex32
#define Wqq TYPE_W, OPSZ_32
#define Mvs TYPE_M, OPSZ_16_vex32
#define Mvd TYPE_M, OPSZ_16_vex32
#define Mx TYPE_M, OPSZ_16_vex32
#define Ldq TYPE_L, OPSZ_16 /* immed is 1 byte but reg is xmm */
#define Lx TYPE_L, OPSZ_16_vex32 /* immed is 1 byte but reg is xmm/ymm */
#define Lvs TYPE_L, OPSZ_16_vex32 /* immed is 1 byte but reg is xmm/ymm */
#define Lss TYPE_L, OPSZ_4_of_16 /* immed is 1 byte but reg is xmm/ymm */
#define Lsd TYPE_L, OPSZ_8_of_16 /* immed is 1 byte but reg is xmm/ymm */

/* my own codes
 * size m = 32 or 16 bit depending on addr size attribute
 * B=ds:eDI, Z=xlat's mem, K=float in mem, i_==indirect
 */
#define Mb  TYPE_M, OPSZ_1
#define Md  TYPE_M, OPSZ_4
#define Md_q  TYPE_M, OPSZ_4_rex8
#define Mw  TYPE_M, OPSZ_2
#define Mm  TYPE_M, OPSZ_lea
#define Me  TYPE_M, OPSZ_512
#define Mxsave TYPE_M, OPSZ_xsave
#define Mps  TYPE_M, OPSZ_16
#define Mpd  TYPE_M, OPSZ_16
#define Mss  TYPE_M, OPSZ_4
#define Msd  TYPE_M, OPSZ_8
#define Mq  TYPE_M, OPSZ_8
#define Mdq  TYPE_M, OPSZ_16
#define Mq_dq TYPE_M, OPSZ_8_rex16
#define Mv  TYPE_M, OPSZ_4_rex8_short2
#define MVd TYPE_VSIB, OPSZ_4
#define MVq TYPE_VSIB, OPSZ_8
#define Zb  TYPE_XLAT, OPSZ_1
#define Bq  TYPE_MASKMOVQ, OPSZ_8
#define Bdq  TYPE_MASKMOVQ, OPSZ_16
#define Kw  TYPE_FLOATMEM, OPSZ_2
#define Kd  TYPE_FLOATMEM, OPSZ_4
#define Kq  TYPE_FLOATMEM, OPSZ_8
#define Kx  TYPE_FLOATMEM, OPSZ_10
#define Ky  TYPE_FLOATMEM, OPSZ_28_short14 /* _14_ if data16 */
#define Kz  TYPE_FLOATMEM, OPSZ_108_short94 /* _98_ if data16 */
#define i_dx  TYPE_INDIR_REG, REG_DX
#define i_Ev  TYPE_INDIR_E, OPSZ_4_rex8_short2
#define i_Exi  TYPE_INDIR_E, OPSZ_4x8_short2xi8
#define i_Ep  TYPE_INDIR_E, OPSZ_6_irex10_short4
#define i_xSP TYPE_INDIR_VAR_XREG, REG_ESP
#define i_iSP TYPE_INDIR_VAR_XIREG, REG_ESP
#define i_xBP TYPE_INDIR_VAR_XREG, REG_EBP
/* negative offset from (%xsp) for pushes */
#define i_iSPo1 TYPE_INDIR_VAR_XIREG_OFFS_1, REG_ESP
#define i_vSPo2 TYPE_INDIR_VAR_REG_OFFS_2, REG_ESP
#define i_xSPo1 TYPE_INDIR_VAR_XREG_OFFS_1, REG_ESP
#define i_xSPo8 TYPE_INDIR_VAR_XREG_OFFS_8, REG_ESP
#define i_xSPs8 TYPE_INDIR_VAR_XREG_SIZEx8, REG_ESP
#define i_vSPs2 TYPE_INDIR_VAR_REG_SIZEx2, REG_ESP
#define i_vSPs3 TYPE_INDIR_VAR_REG_SIZEx3x5, REG_ESP
/* pop but unusual size */
#define i_xSPoN TYPE_INDIR_VAR_XREG_OFFS_N, REG_ESP
#define c1  TYPE_1, OPSZ_0
/* we pick the right constant based on the opcode */
#define cF  TYPE_FLOATCONST, OPSZ_0

#ifdef IA32_ON_IA64
# define Av TYPE_A, OPSZ_4_short2
#endif

/* registers that are base 32 but vary down or up */
#define eAX TYPE_VAR_REG, REG_EAX
#define eCX TYPE_VAR_REG, REG_ECX
#define eDX TYPE_VAR_REG, REG_EDX
#define eBX TYPE_VAR_REG, REG_EBX
#define eSP TYPE_VAR_REG, REG_ESP
#define eBP TYPE_VAR_REG, REG_EBP
#define eSI TYPE_VAR_REG, REG_ESI
#define eDI TYPE_VAR_REG, REG_EDI

/* registers that are base 32 and can vary down but not up */
#define zAX TYPE_VARZ_REG, REG_EAX
#define zCX TYPE_VARZ_REG, REG_ECX
#define zDX TYPE_VARZ_REG, REG_EDX
#define zBX TYPE_VARZ_REG, REG_EBX
#define zSP TYPE_VARZ_REG, REG_ESP
#define zBP TYPE_VARZ_REG, REG_EBP
#define zSI TYPE_VARZ_REG, REG_ESI
#define zDI TYPE_VARZ_REG, REG_EDI

/* registers whose base matches the mode, and can vary down but not up.
 * we use the 32-bit versions but expand in resolve_var_reg()
 */
#define xAX TYPE_VAR_XREG, REG_EAX
#define xCX TYPE_VAR_XREG, REG_ECX
#define xDX TYPE_VAR_XREG, REG_EDX
#define xBX TYPE_VAR_XREG, REG_EBX
#define xSP TYPE_VAR_XREG, REG_ESP
#define xBP TYPE_VAR_XREG, REG_EBP
#define xSI TYPE_VAR_XREG, REG_ESI
#define xDI TYPE_VAR_XREG, REG_EDI

/* jecxz and loop* vary by addr16 */
#define axCX TYPE_VAR_ADDR_XREG, REG_ECX
/* string ops also use addr16 */
#define axSI TYPE_VAR_ADDR_XREG, REG_ESI
#define axDI TYPE_VAR_ADDR_XREG, REG_EDI
#define axAX TYPE_VAR_ADDR_XREG, REG_EAX

/* 8-bit implicit registers (not from modrm) that can be exteded via rex.r */
#define al_x TYPE_REG_EX, REG_AL
#define cl_x TYPE_REG_EX, REG_CL
#define dl_x TYPE_REG_EX, REG_DL
#define bl_x TYPE_REG_EX, REG_BL
#define ah_x TYPE_REG_EX, REG_AH
#define ch_x TYPE_REG_EX, REG_CH
#define dh_x TYPE_REG_EX, REG_DH
#define bh_x TYPE_REG_EX, REG_BH

/* 4_rex8_short2 implicit registers (not from modrm) that can be exteded via rex.r */
#define eAX_x TYPE_VAR_REG_EX, REG_EAX
#define eCX_x TYPE_VAR_REG_EX, REG_ECX
#define eDX_x TYPE_VAR_REG_EX, REG_EDX
#define eBX_x TYPE_VAR_REG_EX, REG_EBX
#define eSP_x TYPE_VAR_REG_EX, REG_ESP
#define eBP_x TYPE_VAR_REG_EX, REG_EBP
#define eSI_x TYPE_VAR_REG_EX, REG_ESI
#define eDI_x TYPE_VAR_REG_EX, REG_EDI

/* 4x8_short2 implicit registers (not from modrm) that can be exteded via rex.r */
#define xAX_x TYPE_VAR_XREG_EX, REG_EAX
#define xCX_x TYPE_VAR_XREG_EX, REG_ECX
#define xDX_x TYPE_VAR_XREG_EX, REG_EDX
#define xBX_x TYPE_VAR_XREG_EX, REG_EBX
#define xSP_x TYPE_VAR_XREG_EX, REG_ESP
#define xBP_x TYPE_VAR_XREG_EX, REG_EBP
#define xSI_x TYPE_VAR_XREG_EX, REG_ESI
#define xDI_x TYPE_VAR_XREG_EX, REG_EDI

/* 4_rex8 implicit registers (not from modrm) that can be exteded via rex.r */
#define uAX_x TYPE_VAR_REGX_EX, REG_EAX
#define uCX_x TYPE_VAR_REGX_EX, REG_ECX
#define uDX_x TYPE_VAR_REGX_EX, REG_EDX
#define uBX_x TYPE_VAR_REGX_EX, REG_EBX
#define uSP_x TYPE_VAR_REGX_EX, REG_ESP
#define uBP_x TYPE_VAR_REGX_EX, REG_EBP
#define uSI_x TYPE_VAR_REGX_EX, REG_ESI
#define uDI_x TYPE_VAR_REGX_EX, REG_EDI

/* 4_rex8 implicit registers (not from modrm) */
#define uDX TYPE_VAR_REGX, REG_EDX

#define ax TYPE_REG, REG_AX
#define cx TYPE_REG, REG_CX
#define dx TYPE_REG, REG_DX
#define bx TYPE_REG, REG_BX
#define sp TYPE_REG, REG_SP
#define bp TYPE_REG, REG_BP
#define si TYPE_REG, REG_SI
#define di TYPE_REG, REG_DI

#define al TYPE_REG, REG_AL
#define cl TYPE_REG, REG_CL
#define dl TYPE_REG, REG_DL
#define bl TYPE_REG, REG_BL
#define ah TYPE_REG, REG_AH
#define ch TYPE_REG, REG_CH
#define dh TYPE_REG, REG_DH
#define bh TYPE_REG, REG_BH

#define eax TYPE_REG, REG_EAX
#define ecx TYPE_REG, REG_ECX
#define edx TYPE_REG, REG_EDX
#define ebx TYPE_REG, REG_EBX
#define esp TYPE_REG, REG_ESP
#define ebp TYPE_REG, REG_EBP
#define esi TYPE_REG, REG_ESI
#define edi TYPE_REG, REG_EDI

#define xsp TYPE_XREG, REG_ESP
#define xbp TYPE_XREG, REG_EBP
#define xcx TYPE_XREG, REG_ECX

#define cs  TYPE_REG, SEG_CS
#define ss  TYPE_REG, SEG_SS
#define ds  TYPE_REG, SEG_DS
#define es  TYPE_REG, SEG_ES
#define fs  TYPE_REG, SEG_FS
#define gs  TYPE_REG, SEG_GS

#define st0 TYPE_REG, REG_ST0
#define st1 TYPE_REG, REG_ST1
#define st2 TYPE_REG, REG_ST2
#define st3 TYPE_REG, REG_ST3
#define st4 TYPE_REG, REG_ST4
#define st5 TYPE_REG, REG_ST5
#define st6 TYPE_REG, REG_ST6
#define st7 TYPE_REG, REG_ST7

#define xmm0 TYPE_REG, REG_XMM0

/* flags */
#define no       0
#define mrm      HAS_MODRM
#define xop      (HAS_EXTRA_OPERANDS|EXTRAS_IN_CODE_FIELD)
#define mrm_xop  (HAS_MODRM|HAS_EXTRA_OPERANDS|EXTRAS_IN_CODE_FIELD)
#define xop_next (HAS_EXTRA_OPERANDS)
#define i64      X64_INVALID
#define o64      X86_INVALID
#define reqp     REQUIRES_PREFIX
#define vex      REQUIRES_VEX
#define rex      REQUIRES_REX
#define reqL0    REQUIRES_VEX_L_0
#define reqL1    REQUIRES_VEX_L_1
#define predcc   HAS_PRED_CC
#define predcx   HAS_PRED_COMPLEX

/* eflags */
#define x     0
#define fRC   EFLAGS_READ_CF
#define fRP   EFLAGS_READ_PF
#define fRA   EFLAGS_READ_AF
#define fRZ   EFLAGS_READ_ZF
#define fRS   EFLAGS_READ_SF
#define fRT   EFLAGS_READ_TF
#define fRI   EFLAGS_READ_IF
#define fRD   EFLAGS_READ_DF
#define fRO   EFLAGS_READ_OF
#define fRN   EFLAGS_READ_NT
#define fRR   EFLAGS_READ_RF
#define fRX   EFLAGS_READ_ALL
#define fR6   EFLAGS_READ_6
#define fWC   EFLAGS_WRITE_CF
#define fWP   EFLAGS_WRITE_PF
#define fWA   EFLAGS_WRITE_AF
#define fWZ   EFLAGS_WRITE_ZF
#define fWS   EFLAGS_WRITE_SF
#define fWT   EFLAGS_WRITE_TF
#define fWI   EFLAGS_WRITE_IF
#define fWD   EFLAGS_WRITE_DF
#define fWO   EFLAGS_WRITE_OF
#define fWN   EFLAGS_WRITE_NT
#define fWR   EFLAGS_WRITE_RF
#define fWX   EFLAGS_WRITE_ALL
#define fW6   EFLAGS_WRITE_6
/* flags affected by OP_int*
 * FIXME: should we add AC and VM flags?
 */
#define fINT  (fRX|fWT|fWN|fWI|fWR)

/* for constructing linked lists of table entries */
#define NA 0
#define END_LIST  0
#define tfb (ptr_int_t)&first_byte
#define tsb (ptr_int_t)&second_byte
#define tex (ptr_int_t)&extensions
#define t38 (ptr_int_t)&third_byte_38
#define t3a (ptr_int_t)&third_byte_3a
#define tpe (ptr_int_t)&prefix_extensions
#define tvex (ptr_int_t)&vex_extensions
#define modx (ptr_int_t)&mod_extensions
#define tre (ptr_int_t)&rep_extensions
#define tne (ptr_int_t)&repne_extensions
#define tfl (ptr_int_t)&float_low_modrm
#define tfh (ptr_int_t)&float_high_modrm
#define exop (ptr_int_t)&extra_operands
#define t64e (ptr_int_t)&x64_extensions
#define trexb (ptr_int_t)&rex_b_extensions
#define trexw (ptr_int_t)&rex_w_extensions
#define tvex (ptr_int_t)&vex_extensions
#define tvexw (ptr_int_t)&vex_W_extensions
#define txop (ptr_int_t)&xop_extensions

/****************************************************************************
 * One-byte opcodes
 * This is from Tables A-2 & A-3
 */
const instr_info_t first_byte[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, modrm?, eflags, code} */
    /* 00 */
    {OP_add,  0x000000, "add",  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][0]},
    {OP_add,  0x010000, "add",  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x00]},
    {OP_add,  0x020000, "add",  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x01]},
    {OP_add,  0x030000, "add",  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x02]},
    {OP_add,  0x040000, "add",  al, xx, Ib, al, xx, no,  fW6, tfb[0x03]},
    {OP_add,  0x050000, "add", eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x04]},
    {OP_push, 0x060000, "push", xsp, i_xSPo1, es, xsp, xx, i64, x, tfb[0x0e]},
    {OP_pop,  0x070000, "pop", es, xsp, xsp, i_xSP, xx, i64, x, tsb[0xa1]},
    /* 08 */
    {OP_or,  0x080000, "or",  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][1]},
    {OP_or,  0x090000, "or",  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x08]},
    {OP_or,  0x0a0000, "or",  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x09]},
    {OP_or,  0x0b0000, "or",  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x0a]},
    {OP_or,  0x0c0000, "or",  al, xx, Ib, al, xx, no,  fW6, tfb[0x0b]},
    {OP_or,  0x0d0000, "or", eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x0c]},
    {OP_push,0x0e0000, "push", xsp, i_xSPo1, cs, xsp, xx, i64, x, tfb[0x16]},
    {ESCAPE, 0x0f0000, "(escape)", xx, xx, xx, xx, xx, no, x, NA},
    /* 10 */
    {OP_adc,  0x100000, "adc",  Eb, xx, Gb, Eb, xx, mrm, (fW6|fRC), tex[1][2]},
    {OP_adc,  0x110000, "adc",  Ev, xx, Gv, Ev, xx, mrm, (fW6|fRC), tfb[0x10]},
    {OP_adc,  0x120000, "adc",  Gb, xx, Eb, Gb, xx, mrm, (fW6|fRC), tfb[0x11]},
    {OP_adc,  0x130000, "adc",  Gv, xx, Ev, Gv, xx, mrm, (fW6|fRC), tfb[0x12]},
    {OP_adc,  0x140000, "adc",  al, xx, Ib, al, xx, no,  (fW6|fRC), tfb[0x13]},
    {OP_adc,  0x150000, "adc", eAX, xx, Iz, eAX, xx, no,  (fW6|fRC), tfb[0x14]},
    {OP_push, 0x160000, "push", xsp, i_xSPo1, ss, xsp, xx, i64, x, tfb[0x1e]},
    {OP_pop,  0x170000, "pop", ss, xsp, xsp, i_xSP, xx, i64, x, tfb[0x1f]},
    /* 18 */
    {OP_sbb,  0x180000, "sbb",  Eb, xx, Gb, Eb, xx, mrm, (fW6|fRC), tex[1][3]},
    {OP_sbb,  0x190000, "sbb",  Ev, xx, Gv, Ev, xx, mrm, (fW6|fRC), tfb[0x18]},
    {OP_sbb,  0x1a0000, "sbb",  Gb, xx, Eb, Gb, xx, mrm, (fW6|fRC), tfb[0x19]},
    {OP_sbb,  0x1b0000, "sbb",  Gv, xx, Ev, Gv, xx, mrm, (fW6|fRC), tfb[0x1a]},
    {OP_sbb,  0x1c0000, "sbb",  al, xx, Ib, al, xx, no,  (fW6|fRC), tfb[0x1b]},
    {OP_sbb,  0x1d0000, "sbb", eAX, xx, Iz, eAX, xx, no,  (fW6|fRC), tfb[0x1c]},
    {OP_push, 0x1e0000, "push", xsp, i_xSPo1, ds, xsp, xx, i64, x, tsb[0xa0]},
    {OP_pop,  0x1f0000, "pop", ds, xsp, xsp, i_xSP, xx, i64, x, tfb[0x07]},
    /* 20 */
    {OP_and,  0x200000, "and",  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][4]},
    {OP_and,  0x210000, "and",  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x20]},
    {OP_and,  0x220000, "and",  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x21]},
    {OP_and,  0x230000, "and",  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x22]},
    {OP_and,  0x240000, "and",  al, xx, Ib, al, xx, no,  fW6, tfb[0x23]},
    {OP_and,  0x250000, "and", eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x24]},
    {PREFIX,  0x260000, "es", xx, xx, xx, xx, xx, no, x, SEG_ES},
    {OP_daa,  0x270000, "daa", al, xx, al, xx, xx, i64, (fW6|fRC|fRA), END_LIST},
    /* 28 */
    {OP_sub,  0x280000, "sub",  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][5]},
    {OP_sub,  0x290000, "sub",  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x28]},
    {OP_sub,  0x2a0000, "sub",  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x29]},
    {OP_sub,  0x2b0000, "sub",  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x2a]},
    {OP_sub,  0x2c0000, "sub",  al, xx, Ib, al, xx, no,  fW6, tfb[0x2b]},
    {OP_sub,  0x2d0000, "sub", eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x2c]},
    {PREFIX,  0x2e0000, "cs", xx, xx, xx, xx, xx, no, x, SEG_CS},
    {OP_das,  0x2f0000, "das", al, xx, al, xx, xx, i64, (fW6|fRC|fRA), END_LIST},
    /* 30 */
    {OP_xor,  0x300000, "xor",  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][6]},
    {OP_xor,  0x310000, "xor",  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x30]},
    {OP_xor,  0x320000, "xor",  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x31]},
    {OP_xor,  0x330000, "xor",  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x32]},
    {OP_xor,  0x340000, "xor",  al, xx, Ib, al, xx, no,  fW6, tfb[0x33]},
    {OP_xor,  0x350000, "xor", eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x34]},
    {PREFIX,  0x360000, "ss", xx, xx, xx, xx, xx, no, x, SEG_SS},
    {OP_aaa,  0x370000, "aaa", ax, xx, ax, xx, xx, i64, (fW6|fRA), END_LIST},
    /* 38 */
    {OP_cmp,  0x380000, "cmp", xx, xx,  Eb, Gb, xx, mrm, fW6, tex[1][7]},
    {OP_cmp,  0x390000, "cmp", xx, xx,  Ev, Gv, xx, mrm, fW6, tfb[0x38]},
    {OP_cmp,  0x3a0000, "cmp", xx, xx,  Gb, Eb, xx, mrm, fW6, tfb[0x39]},
    {OP_cmp,  0x3b0000, "cmp", xx, xx,  Gv, Ev, xx, mrm, fW6, tfb[0x3a]},
    {OP_cmp,  0x3c0000, "cmp", xx, xx,  al, Ib, xx, no,  fW6, tfb[0x3b]},
    {OP_cmp,  0x3d0000, "cmp", xx, xx, eAX, Iz, xx, no,  fW6, tfb[0x3c]},
    {PREFIX,  0x3e0000, "ds", xx, xx, xx, xx, xx, no, x, SEG_DS},
    {OP_aas,  0x3f0000, "aas", ax, xx, ax, xx, xx, i64, (fW6|fRA), END_LIST},
    /* 40 */
    {X64_EXT, 0x400000, "(x64_ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {X64_EXT, 0x410000, "(x64_ext 1)", xx, xx, xx, xx, xx, no, x, 1},
    {X64_EXT, 0x420000, "(x64_ext 2)", xx, xx, xx, xx, xx, no, x, 2},
    {X64_EXT, 0x430000, "(x64_ext 3)", xx, xx, xx, xx, xx, no, x, 3},
    {X64_EXT, 0x440000, "(x64_ext 4)", xx, xx, xx, xx, xx, no, x, 4},
    {X64_EXT, 0x450000, "(x64_ext 5)", xx, xx, xx, xx, xx, no, x, 5},
    {X64_EXT, 0x460000, "(x64_ext 6)", xx, xx, xx, xx, xx, no, x, 6},
    {X64_EXT, 0x470000, "(x64_ext 7)", xx, xx, xx, xx, xx, no, x, 7},
    /* 48 */
    {X64_EXT, 0x480000, "(x64_ext 8)", xx, xx, xx, xx, xx, no, x, 8},
    {X64_EXT, 0x490000, "(x64_ext 9)", xx, xx, xx, xx, xx, no, x, 9},
    {X64_EXT, 0x4a0000, "(x64_ext 10)", xx, xx, xx, xx, xx, no, x, 10},
    {X64_EXT, 0x4b0000, "(x64_ext 11)", xx, xx, xx, xx, xx, no, x, 11},
    {X64_EXT, 0x4c0000, "(x64_ext 12)", xx, xx, xx, xx, xx, no, x, 12},
    {X64_EXT, 0x4d0000, "(x64_ext 13)", xx, xx, xx, xx, xx, no, x, 13},
    {X64_EXT, 0x4e0000, "(x64_ext 14)", xx, xx, xx, xx, xx, no, x, 14},
    {X64_EXT, 0x4f0000, "(x64_ext 15)", xx, xx, xx, xx, xx, no, x, 15},
    /* 50 */
    {OP_push,  0x500000, "push", xsp, i_xSPo1, xAX_x, xsp, xx, no, x, tfb[0x51]},
    {OP_push,  0x510000, "push", xsp, i_xSPo1, xCX_x, xsp, xx, no, x, tfb[0x52]},
    {OP_push,  0x520000, "push", xsp, i_xSPo1, xDX_x, xsp, xx, no, x, tfb[0x53]},
    {OP_push,  0x530000, "push", xsp, i_xSPo1, xBX_x, xsp, xx, no, x, tfb[0x54]},
    {OP_push,  0x540000, "push", xsp, i_xSPo1, xSP_x, xsp, xx, no, x, tfb[0x55]},
    {OP_push,  0x550000, "push", xsp, i_xSPo1, xBP_x, xsp, xx, no, x, tfb[0x56]},
    {OP_push,  0x560000, "push", xsp, i_xSPo1, xSI_x, xsp, xx, no, x, tfb[0x57]},
    {OP_push,  0x570000, "push", xsp, i_xSPo1, xDI_x, xsp, xx, no, x, tex[12][6]},
    /* 58 */
    {OP_pop,  0x580000, "pop", xAX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x59]},
    {OP_pop,  0x590000, "pop", xCX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5a]},
    {OP_pop,  0x5a0000, "pop", xDX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5b]},
    {OP_pop,  0x5b0000, "pop", xBX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5c]},
    {OP_pop,  0x5c0000, "pop", xSP_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5d]},
    {OP_pop,  0x5d0000, "pop", xBP_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5e]},
    {OP_pop,  0x5e0000, "pop", xSI_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5f]},
    {OP_pop,  0x5f0000, "pop", xDI_x, xsp, xsp, i_xSP, xx, no, x, tex[26][0]},
    /* 60 */
    {OP_pusha, 0x600000, "pusha", xsp, i_xSPo8, xsp, eAX, eBX, xop|i64, x, exop[0x00]},
    {OP_popa,  0x610000, "popa", xsp, eAX, xsp, i_xSPs8, xx, xop|i64, x, exop[0x02]},
    {OP_bound, 0x620000, "bound", xx, xx, Gv, Ma, xx, mrm|i64, x, END_LIST},
    {X64_EXT,  0x630000, "(x64_ext 16)", xx, xx, xx, xx, xx, no, x, 16},
    {PREFIX, 0x640000, "fs", xx, xx, xx, xx, xx, no, x, SEG_FS},
    {PREFIX, 0x650000, "gs", xx, xx, xx, xx, xx, no, x, SEG_GS},
    {PREFIX, 0x660000, "data size", xx, xx, xx, xx, xx, no, x, PREFIX_DATA},
    {PREFIX, 0x670000, "addr size", xx, xx, xx, xx, xx, no, x, PREFIX_ADDR},
    /* 68 */
    {OP_push_imm, 0x680000, "push", xsp, i_xSPo1, Iz, xsp, xx, no, x, tfb[0x6a]},
    {OP_imul,  0x690000, "imul", Gv, xx, Ev, Iz, xx, mrm, fW6, tfb[0x6b]},
    {OP_push_imm, 0x6a0000, "push", xsp, i_xSPo1, Ib, xsp, xx, no, x, END_LIST},/* sign-extend to push 2/4/8 bytes */
    {OP_imul,  0x6b0000, "imul", Gv, xx, Ev, Ib, xx, mrm, fW6, END_LIST},
    {REP_EXT,  0x6c0000, "((rep) ins)", Yb, xx, i_dx, xx, xx, no, fRD, 0},
    {REP_EXT,  0x6d0000, "((rep) ins)", Yz, xx, i_dx, xx, xx, no, fRD, 1},
    {REP_EXT,  0x6e0000, "((rep) outs)", i_dx, xx, Xb, xx, xx, no, fRD, 2},
    {REP_EXT,  0x6f0000, "((rep) outs)", i_dx, xx, Xz, xx, xx, no, fRD, 3},
    /* 70 */
    {OP_jo_short,  0x700000, "jo",  xx, xx, Jb, xx, xx, no, fRO, END_LIST},
    {OP_jno_short, 0x710000, "jno", xx, xx, Jb, xx, xx, no, fRO, END_LIST},
    {OP_jb_short,  0x720000, "jb",  xx, xx, Jb, xx, xx, no, fRC, END_LIST},
    {OP_jnb_short, 0x730000, "jnb", xx, xx, Jb, xx, xx, no, fRC, END_LIST},
    {OP_jz_short,  0x740000, "jz",  xx, xx, Jb, xx, xx, no, fRZ, END_LIST},
    {OP_jnz_short, 0x750000, "jnz", xx, xx, Jb, xx, xx, no, fRZ, END_LIST},
    {OP_jbe_short, 0x760000, "jbe", xx, xx, Jb, xx, xx, no, (fRC|fRZ), END_LIST},
    {OP_jnbe_short,0x770000, "jnbe",xx, xx, Jb, xx, xx, no, (fRC|fRZ), END_LIST},
    /* 78 */
    {OP_js_short,  0x780000, "js",  xx, xx, Jb, xx, xx, no, fRS, END_LIST},
    {OP_jns_short, 0x790000, "jns", xx, xx, Jb, xx, xx, no, fRS, END_LIST},
    {OP_jp_short,  0x7a0000, "jp",  xx, xx, Jb, xx, xx, no, fRP, END_LIST},
    {OP_jnp_short, 0x7b0000, "jnp", xx, xx, Jb, xx, xx, no, fRP, END_LIST},
    {OP_jl_short,  0x7c0000, "jl",  xx, xx, Jb, xx, xx, no, (fRS|fRO), END_LIST},
    {OP_jnl_short, 0x7d0000, "jnl", xx, xx, Jb, xx, xx, no, (fRS|fRO), END_LIST},
    {OP_jle_short, 0x7e0000, "jle", xx, xx, Jb, xx, xx, no, (fRS|fRO|fRZ), END_LIST},
    {OP_jnle_short,0x7f0000, "jnle",xx, xx, Jb, xx, xx, no, (fRS|fRO|fRZ), END_LIST},
    /* 80 */
    {EXTENSION, 0x800000, "(group 1a)", Eb, xx, Ib, xx, xx, mrm, x, 0},
    {EXTENSION, 0x810000, "(group 1b)", Ev, xx, Iz, xx, xx, mrm, x, 1},
    {EXTENSION, 0x820000, "(group 1c*)", Ev, xx, Ib, xx, xx, mrm|i64, x, 25}, /* PR 235092: gnu tools (gdb, objdump) think this is a bad opcode but windbg and the hardware disagree */
    {EXTENSION, 0x830000, "(group 1c)", Ev, xx, Ib, xx, xx, mrm, x, 2},
    {OP_test,  0x840000, "test", xx, xx, Eb, Gb, xx, mrm, fW6, tex[10][0]},
    {OP_test,  0x850000, "test", xx, xx, Ev, Gv, xx, mrm, fW6, tfb[0x84]},
    {OP_xchg,  0x860000, "xchg", Eb, Gb, Eb, Gb, xx, mrm, x, END_LIST},
    {OP_xchg,  0x870000, "xchg", Ev, Gv, Ev, Gv, xx, mrm, x, tfb[0x86]},
    /* 88 */
    {OP_mov_st,  0x880000, "mov", Eb, xx, Gb, xx, xx, mrm, x, tex[18][0]},
    {OP_mov_st,  0x890000, "mov", Ev, xx, Gv, xx, xx, mrm, x, tfb[0x88]},
    {OP_mov_ld,  0x8a0000, "mov", Gb, xx, Eb, xx, xx, mrm, x, END_LIST},
    {OP_mov_ld,  0x8b0000, "mov", Gv, xx, Ev, xx, xx, mrm, x, tfb[0x8a]},
    {OP_mov_seg, 0x8c0000, "mov", Ev, xx, Sw, xx, xx, mrm, x, END_LIST},
    {OP_lea,  0x8d0000, "lea", Gv, xx, Mm, xx, xx, mrm, x, END_LIST}, /* Intel has just M */
    {OP_mov_seg, 0x8e0000, "mov", Sw, xx, Ev, xx, xx, mrm, x, tfb[0x8c]},
    {XOP_PREFIX_EXT, 0x8f0000, "(xop_prefix_ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    /* 90 */
    {PREFIX_EXT, 0x900000, "(prefix ext 103)", xx, xx, xx, xx, xx, no, x, 103},
    {OP_xchg, 0x910000, "xchg", eCX_x, eAX, eCX_x, eAX, xx, no, x, tfb[0x92]},
    {OP_xchg, 0x920000, "xchg", eDX_x, eAX, eDX_x, eAX, xx, no, x, tfb[0x93]},
    {OP_xchg, 0x930000, "xchg", eBX_x, eAX, eBX_x, eAX, xx, no, x, tfb[0x94]},
    {OP_xchg, 0x940000, "xchg", eSP_x, eAX, eSP_x, eAX, xx, no, x, tfb[0x95]},
    {OP_xchg, 0x950000, "xchg", eBP_x, eAX, eBP_x, eAX, xx, no, x, tfb[0x96]},
    {OP_xchg, 0x960000, "xchg", eSI_x, eAX, eSI_x, eAX, xx, no, x, tfb[0x97]},
    {OP_xchg, 0x970000, "xchg", eDI_x, eAX, eDI_x, eAX, xx, no, x, tfb[0x87]},
    /* 98 */
    {OP_cwde, 0x980000, "cwde", eAX, xx, ax, xx, xx, no, x, END_LIST},/*16-bit=="cbw", src is al not ax; FIXME: newer gdb calls it "cwtl"?!?*/
    /* PR 354096: does not write to ax/eax/rax: sign-extends into dx/edx/rdx */
    {OP_cdq,  0x990000, "cdq", eDX, xx, eAX, xx, xx, no, x, END_LIST},/*16-bit=="cwd";64-bit=="cqo"*/
    {OP_call_far, 0x9a0000, "lcall",  xsp, i_vSPo2, Ap, xsp, xx, i64, x, END_LIST},
    {OP_fwait, 0x9b0000, "fwait", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pushf, 0x9c0000, "pushf", xsp, i_xSPo1, xsp, xx, xx, no, fRX, END_LIST},
    {OP_popf,  0x9d0000, "popf", xsp, xx, xsp, i_xSP, xx, no, fWX, END_LIST},
    {OP_sahf,  0x9e0000, "sahf", xx, xx, ah, xx, xx, no, (fW6&(~fWO)), END_LIST},
    {OP_lahf,  0x9f0000, "lahf", ah, xx, xx, xx, xx, no, (fR6&(~fRO)), END_LIST},
    /* a0 */
    {OP_mov_ld,  0xa00000, "mov", al, xx, Ob, xx, xx, no, x, tfb[0x8b]},
    {OP_mov_ld,  0xa10000, "mov", eAX, xx, Ov, xx, xx, no, x, tfb[0xa0]},
    {OP_mov_st,  0xa20000, "mov", Ob, xx, al, xx, xx, no, x, tfb[0x89]},
    {OP_mov_st,  0xa30000, "mov", Ov, xx, eAX, xx, xx, no, x, tfb[0xa2]},
    {REP_EXT, 0xa40000, "((rep) movs)", Yb, xx, Xb, xx, xx, no, fRD, 4},
    {REP_EXT, 0xa50000, "((rep) movs)", Yv, xx, Xv, xx, xx, no, fRD, 5},
    {REPNE_EXT, 0xa60000, "((rep/ne) cmps)", Xb, xx, Yb, xx, xx, no, (fW6|fRD|fRZ), 0},
    {REPNE_EXT, 0xa70000, "((rep/ne) cmps)", Xv, xx, Yv, xx, xx, no, (fW6|fRD|fRZ), 1},
    /* a8 */
    {OP_test,  0xa80000, "test", xx, xx,  al, Ib, xx, no, fW6, tfb[0x85]},
    {OP_test,  0xa90000, "test", xx, xx, eAX, Iz, xx, no, fW6, tfb[0xa8]},
    {REP_EXT, 0xaa0000, "((rep) stos)", Yb, xx, al, xx, xx, no, fRD, 6},
    {REP_EXT, 0xab0000, "((rep) stos)", Yv, xx, eAX, xx, xx, no, fRD, 7},
    {REP_EXT, 0xac0000, "((rep) lods)", al, xx, Xb, xx, xx, no, fRD, 8},
    {REP_EXT, 0xad0000, "((rep) lods)", eAX, xx, Xv, xx, xx, no, fRD, 9},
    {REPNE_EXT, 0xae0000, "((rep/ne) scas)", al, xx, Yb, xx, xx, no, (fW6|fRD|fRZ), 2},
    {REPNE_EXT, 0xaf0000, "((rep/ne) scas)", eAX, xx, Yv, xx, xx, no, (fW6|fRD|fRZ), 3},
    /* b0 */
    {OP_mov_imm, 0xb00000, "mov", al_x, xx, Ib, xx, xx, no, x, tfb[0xb1]},
    {OP_mov_imm, 0xb10000, "mov", cl_x, xx, Ib, xx, xx, no, x, tfb[0xb2]},
    {OP_mov_imm, 0xb20000, "mov", dl_x, xx, Ib, xx, xx, no, x, tfb[0xb3]},
    {OP_mov_imm, 0xb30000, "mov", bl_x, xx, Ib, xx, xx, no, x, tfb[0xb4]},
    {OP_mov_imm, 0xb40000, "mov", ah_x, xx, Ib, xx, xx, no, x, tfb[0xb5]},
    {OP_mov_imm, 0xb50000, "mov", ch_x, xx, Ib, xx, xx, no, x, tfb[0xb6]},
    {OP_mov_imm, 0xb60000, "mov", dh_x, xx, Ib, xx, xx, no, x, tfb[0xb7]},
    /* PR 250397: we point at the tail end of the mov_st templates */
    {OP_mov_imm, 0xb70000, "mov", bh_x, xx, Ib, xx, xx, no, x, tex[18][0]},
    /* b8 */
    {OP_mov_imm, 0xb80000, "mov", eAX_x, xx, Iv, xx, xx, no, x, tfb[0xb9]},
    {OP_mov_imm, 0xb90000, "mov", eCX_x, xx, Iv, xx, xx, no, x, tfb[0xba]},
    {OP_mov_imm, 0xba0000, "mov", eDX_x, xx, Iv, xx, xx, no, x, tfb[0xbb]},
    {OP_mov_imm, 0xbb0000, "mov", eBX_x, xx, Iv, xx, xx, no, x, tfb[0xbc]},
    {OP_mov_imm, 0xbc0000, "mov", eSP_x, xx, Iv, xx, xx, no, x, tfb[0xbd]},
    {OP_mov_imm, 0xbd0000, "mov", eBP_x, xx, Iv, xx, xx, no, x, tfb[0xbe]},
    {OP_mov_imm, 0xbe0000, "mov", eSI_x, xx, Iv, xx, xx, no, x, tfb[0xbf]},
    {OP_mov_imm, 0xbf0000, "mov", eDI_x, xx, Iv, xx, xx, no, x, tfb[0xb0]},
    /* c0 */
    {EXTENSION, 0xc00000, "(group 2a)", Eb, xx, Ib, xx, xx, mrm, x, 3},
    {EXTENSION, 0xc10000, "(group 2b)", Ev, xx, Ib, xx, xx, mrm, x, 4},
    {OP_ret,  0xc20000, "ret", xsp, xx, Iw, xsp, i_iSP, no, x, tfb[0xc3]},
    {OP_ret,  0xc30000, "ret", xsp, xx, xsp, i_iSP, xx, no, x, END_LIST},
    {VEX_PREFIX_EXT, 0xc40000, "(vex_prefix_ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {VEX_PREFIX_EXT, 0xc50000, "(vex_prefix_ext 1)", xx, xx, xx, xx, xx, no, x, 1},
    {EXTENSION, 0xc60000, "(group 11a)", Eb, xx, Ib, xx, xx, mrm, x, 17},
    {EXTENSION, 0xc70000, "(group 11b)", Ev, xx, Iz, xx, xx, mrm, x, 18},
    /* c8 */
    {OP_enter,  0xc80000, "enter", xsp, i_xSPoN, Iw, Ib, xsp, xop, x, exop[0x05]},
    {OP_leave,  0xc90000, "leave", xsp, xbp, xbp, xsp, i_xBP, no, x, END_LIST},
    {OP_ret_far,  0xca0000, "lret", xsp, xx, Iw, xsp, i_vSPs2, no, x, tfb[0xcb]},
    {OP_ret_far,  0xcb0000, "lret", xsp, xx, xsp, i_vSPs2, xx, no, x, END_LIST},
    /* we ignore the operations on the kernel stack */
    {OP_int3, 0xcc0000, "int3", xx, xx, xx, xx, xx, no, fINT, END_LIST},
    {OP_int,  0xcd0000, "int",  xx, xx, Ib, xx, xx, no, fINT, END_LIST},
    {OP_into, 0xce0000, "into", xx, xx, xx, xx, xx, i64, fINT, END_LIST},
    {OP_iret, 0xcf0000, "iret", xsp, xx, xsp, i_vSPs3, xx, no, fWX, END_LIST},
    /* d0 */
    {EXTENSION, 0xd00000, "(group 2c)", Eb, xx, c1,  xx, xx, mrm, x, 5},
    {EXTENSION, 0xd10000, "(group 2d)", Ev, xx, c1,  xx, xx, mrm, x, 6},
    {EXTENSION, 0xd20000, "(group 2e)", Eb, xx, cl, xx, xx, mrm, x, 7},
    {EXTENSION, 0xd30000, "(group 2f)", Ev, xx, cl, xx, xx, mrm, x, 8},
    {OP_aam,  0xd40000, "aam", ax, xx, Ib, ax, xx, i64, fW6, END_LIST},
    {OP_aad,  0xd50000, "aad", ax, xx, Ib, ax, xx, i64, fW6, END_LIST},
    {OP_salc,  0xd60000, "salc", al, xx, xx, xx, xx, i64, fRC, END_LIST},/*undocumented*/
    {OP_xlat,  0xd70000, "xlat", al, xx, Zb, xx, xx, no, x, END_LIST},
    /* d8 */
    {FLOAT_EXT, 0xd80000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},/* all floats need modrm */
    {FLOAT_EXT, 0xd90000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xda0000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdb0000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdc0000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdd0000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xde0000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdf0000, "(float)", xx, xx, xx, xx, xx, mrm, x, NA},
    /* e0 */
    {OP_loopne,0xe00000, "loopne", axCX, xx, Jb, axCX, xx, no, fRZ, END_LIST},
    {OP_loope, 0xe10000, "loope",  axCX, xx, Jb, axCX, xx, no, fRZ, END_LIST},
    {OP_loop,  0xe20000, "loop",   axCX, xx, Jb, axCX, xx, no, x, END_LIST},
    {OP_jecxz, 0xe30000, "jecxz",  xx, xx, Jb, axCX, xx, no, x, END_LIST},/*16-bit=="jcxz",64-bit="jrcxz"*/
    /* FIXME: in & out access "I/O ports", are these memory addresses?
     * if so, change Ib to Ob and change dx to i_dx (move to dest for out)
     */
    {OP_in,  0xe40000, "in", al, xx, Ib, xx, xx, no, x, tfb[0xed]},
    {OP_in,  0xe50000, "in", zAX, xx, Ib, xx, xx, no, x, tfb[0xe4]},
    {OP_out,  0xe60000, "out", xx, xx, Ib, al, xx, no, x, tfb[0xef]},
    {OP_out,  0xe70000, "out", xx, xx, Ib, zAX, xx, no, x, tfb[0xe6]},
    /* e8 */
    {OP_call,     0xe80000, "call",  xsp, i_iSPo1, Jz, xsp, xx, no, x, END_LIST},
    {OP_jmp,       0xe90000, "jmp", xx, xx, Jz, xx, xx, no, x, END_LIST},
    {OP_jmp_far,   0xea0000, "ljmp", xx, xx, Ap, xx, xx, i64, x, END_LIST},
    {OP_jmp_short, 0xeb0000, "jmp", xx, xx, Jb, xx, xx, no, x, END_LIST},
    {OP_in,  0xec0000, "in", al, xx, dx, xx, xx, no, x, END_LIST},
    {OP_in,  0xed0000, "in", zAX, xx, dx, xx, xx, no, x, tfb[0xec]},
    {OP_out,  0xee0000, "out", xx, xx, al, dx, xx, no, x, END_LIST},
    {OP_out,  0xef0000, "out", xx, xx, zAX, dx, xx, no, x, tfb[0xee]},
    /* f0 */
    {PREFIX, 0xf00000, "lock", xx, xx, xx, xx, xx, no, x, PREFIX_LOCK},
    /* Also called OP_icebp.  Undocumented.  I'm assuming looks like OP_int* */
    {OP_int1, 0xf10000, "int1", xx, xx, xx, xx, xx, no, fINT, END_LIST},
    {PREFIX, 0xf20000, "repne", xx, xx, xx, xx, xx, no, x, PREFIX_REPNE},
    {PREFIX, 0xf30000, "rep", xx, xx, xx, xx, xx, no, x, PREFIX_REP},
    {OP_hlt,  0xf40000, "hlt", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_cmc,  0xf50000, "cmc", xx, xx, xx, xx, xx, no, fWC, END_LIST},
    {EXTENSION, 0xf60000, "(group 3a)", Eb, xx, xx, xx, xx, mrm, x, 9},
    {EXTENSION, 0xf70000, "(group 3b)", Ev, xx, xx, xx, xx, mrm, x, 10},
    /* f8 */
    {OP_clc,  0xf80000, "clc", xx, xx, xx, xx, xx, no, fWC, END_LIST},
    {OP_stc,  0xf90000, "stc", xx, xx, xx, xx, xx, no, fWC, END_LIST},
    {OP_cli,  0xfa0000, "cli", xx, xx, xx, xx, xx, no, fWI, END_LIST},
    {OP_sti,  0xfb0000, "sti", xx, xx, xx, xx, xx, no, fWI, END_LIST},
    {OP_cld,  0xfc0000, "cld", xx, xx, xx, xx, xx, no, fWD, END_LIST},
    {OP_std,  0xfd0000, "std", xx, xx, xx, xx, xx, no, fWD, END_LIST},
    {EXTENSION, 0xfe0000, "(group 4)", xx, xx, xx, xx, xx, mrm, x, 11},
    {EXTENSION, 0xff0000, "(group 5)", xx, xx, xx, xx, xx, mrm, x, 12},
};

/****************************************************************************
 * Two-byte opcodes
 * This is from Tables A-4 & A-5
 */
const instr_info_t second_byte[] = {
  /* 00 */
  {EXTENSION, 0x0f0010, "(group 6)", xx, xx, xx, xx, xx, mrm, x, 13},
  {EXTENSION, 0x0f0110, "(group 7)", xx, xx, xx, xx, xx, mrm, x, 14},
  {OP_lar, 0x0f0210, "lar", Gv, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
  {OP_lsl, 0x0f0310, "lsl", Gv, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
  {INVALID, 0x0f0410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  /* XXX: writes ss and cs */
  {OP_syscall, 0x0f0510, "syscall", xcx, xx, xx, xx, xx, no, x, NA}, /* AMD/x64 only */
  {OP_clts, 0x0f0610, "clts", xx, xx, xx, xx, xx, no, x, END_LIST},
  /* XXX: writes ss and cs */
  {OP_sysret, 0x0f0710, "sysret", xx, xx, xx, xx, xx, no, x, NA}, /* AMD/x64 only */
  /* 08 */
  {OP_invd, 0x0f0810, "invd", xx, xx, xx, xx, xx, no, x, END_LIST},
  {OP_wbinvd, 0x0f0910, "wbinvd", xx, xx, xx, xx, xx, no, x, END_LIST},
  {INVALID, 0x0f0a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {OP_ud2a, 0x0f0b10, "ud2a", xx, xx, xx, xx, xx, no, x, END_LIST}, /* "undefined instr" instr */
  {INVALID, 0x0f0c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {EXTENSION, 0x0f0d10, "(group amd)", xx, xx, xx, xx, xx, mrm, x, 24}, /* AMD only */
  {OP_femms, 0x0f0e10, "femms", xx, xx, xx, xx, xx, no, x, END_LIST},
  {SUFFIX_EXT, 0x0f0f10, "(group 3DNow!)", xx, xx, xx, xx, xx, mrm, x, 0},
  /* 10 */
  {PREFIX_EXT, 0x0f1010, "(prefix ext 0)", xx, xx, xx, xx, xx, mrm, x, 0},
  {PREFIX_EXT, 0x0f1110, "(prefix ext 1)", xx, xx, xx, xx, xx, mrm, x, 1},
  {PREFIX_EXT, 0x0f1210, "(prefix ext 2)", xx, xx, xx, xx, xx, mrm, x, 2},
  {PREFIX_EXT, 0x0f1310, "(prefix ext 3)", xx, xx, xx, xx, xx, mrm, x, 3},
  {PREFIX_EXT, 0x0f1410, "(prefix ext 4)", xx, xx, xx, xx, xx, mrm, x, 4},
  {PREFIX_EXT, 0x0f1510, "(prefix ext 5)", xx, xx, xx, xx, xx, mrm, x, 5},
  {PREFIX_EXT, 0x0f1610, "(prefix ext 6)", xx, xx, xx, xx, xx, mrm, x, 6},
  {PREFIX_EXT, 0x0f1710, "(prefix ext 7)", xx, xx, xx, xx, xx, mrm, x, 7},
  /* 18 */
  {EXTENSION, 0x0f1810, "(group 16)", xx, xx, xx, xx, xx, mrm, x, 23},
  /* xref case 9862/PR 214297 : 0f19-0f1e are "HINT_NOP": valid on P6+.
   * we treat them the same as 0f1f but do not put on encoding chain.
   * The operand is ignored but to support encoding it we must list it.
   * i453: analysis routines now special case nop_modrm to ignore src opnd */
  {OP_nop_modrm, 0x0f1910, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1a10, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1b10, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1c10, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1d10, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1e10, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1f10, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  /* 20 */
  {OP_mov_priv, 0x0f2010, "mov", Rr, xx, Cr, xx, xx, mrm, fW6, tsb[0x21]},
  {OP_mov_priv, 0x0f2110, "mov", Rr, xx, Dr, xx, xx, mrm, fW6, tsb[0x22]},
  {OP_mov_priv, 0x0f2210, "mov", Cr, xx, Rr, xx, xx, mrm, fW6, tsb[0x23]},
  {OP_mov_priv, 0x0f2310, "mov", Dr, xx, Rr, xx, xx, mrm, fW6, END_LIST},
  {INVALID, 0x0f2410, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* FIXME: gdb thinks ok! */
  {INVALID, 0x0f2510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f2610, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* FIXME: gdb thinks ok! */
  {INVALID, 0x0f2710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  /* 28 */
  {PREFIX_EXT, 0x0f2810, "(prefix ext 8)", xx, xx, xx, xx, xx, mrm, x, 8},
  {PREFIX_EXT, 0x0f2910, "(prefix ext 9)", xx, xx, xx, xx, xx, mrm, x, 9},
  {PREFIX_EXT, 0x0f2a10, "(prefix ext 10)", xx, xx, xx, xx, xx, mrm, x, 10},
  {PREFIX_EXT, 0x0f2b10, "(prefix ext 11)", xx, xx, xx, xx, xx, mrm, x, 11},
  {PREFIX_EXT, 0x0f2c10, "(prefix ext 12)", xx, xx, xx, xx, xx, mrm, x, 12},
  {PREFIX_EXT, 0x0f2d10, "(prefix ext 13)", xx, xx, xx, xx, xx, mrm, x, 13},
  {PREFIX_EXT, 0x0f2e10, "(prefix ext 14)", xx, xx, xx, xx, xx, mrm, x, 14},
  {PREFIX_EXT, 0x0f2f10, "(prefix ext 15)", xx, xx, xx, xx, xx, mrm, x, 15},
  /* 30 */
  {OP_wrmsr, 0x0f3010, "wrmsr", xx, xx, edx, eax, ecx, no, x, END_LIST},
  {OP_rdtsc, 0x0f3110, "rdtsc", edx, eax, xx, xx, xx, no, x, END_LIST},
  {OP_rdmsr, 0x0f3210, "rdmsr", edx, eax, ecx, xx, xx, no, x, END_LIST},
  {OP_rdpmc, 0x0f3310, "rdpmc", edx, eax, ecx, xx, xx, no, x, END_LIST},
  /* XXX: sysenter writes cs and ss */
  {OP_sysenter, 0x0f3410, "sysenter", xsp, xx, xx, xx, xx, no, x, END_LIST},
  /* XXX: sysexit writes cs and ss */
  {OP_sysexit, 0x0f3510, "sysexit", xsp, xx, xcx, xx, xx, no, x, END_LIST},
  {INVALID, 0x0f3610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  /* XXX i#1313: various getsec leaf funcs at CPL 0 write to all kinds of
   * processor state including eflags and eip.  Leaf funcs are indicated by eax
   * value, though.  Here we only model the CPL > 0 effects, which conditionally
   * write to ebx + ecx.
   */
  {OP_getsec, 0x0f3710, "getsec", eax, ebx, eax, ebx, xx, xop|predcx, x, exop[13]},
  /* 38 */
  {ESCAPE_3BYTE_38, 0x0f3810, "(3byte 38)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {ESCAPE_3BYTE_3a, 0x0f3a10, "(3byte 3a)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  /* 40 */
  {OP_cmovo,  0x0f4010, "cmovo",  Gv, xx, Ev, xx, xx, mrm|predcc, fRO, END_LIST},
  {OP_cmovno, 0x0f4110, "cmovno", Gv, xx, Ev, xx, xx, mrm|predcc, fRO, END_LIST},
  {OP_cmovb,  0x0f4210, "cmovb",  Gv, xx, Ev, xx, xx, mrm|predcc, fRC, END_LIST},
  {OP_cmovnb, 0x0f4310, "cmovnb", Gv, xx, Ev, xx, xx, mrm|predcc, fRC, END_LIST},
  {OP_cmovz,  0x0f4410, "cmovz",  Gv, xx, Ev, xx, xx, mrm|predcc, fRZ, END_LIST},
  {OP_cmovnz, 0x0f4510, "cmovnz", Gv, xx, Ev, xx, xx, mrm|predcc, fRZ, END_LIST},
  {OP_cmovbe, 0x0f4610, "cmovbe", Gv, xx, Ev, xx, xx, mrm|predcc, (fRC|fRZ), END_LIST},
  {OP_cmovnbe,0x0f4710, "cmovnbe",Gv, xx, Ev, xx, xx, mrm|predcc, (fRC|fRZ), END_LIST},
  /* 48 */
  {OP_cmovs,  0x0f4810, "cmovs",  Gv, xx, Ev, xx, xx, mrm|predcc, fRS, END_LIST},
  {OP_cmovns, 0x0f4910, "cmovns", Gv, xx, Ev, xx, xx, mrm|predcc, fRS, END_LIST},
  {OP_cmovp,  0x0f4a10, "cmovp",  Gv, xx, Ev, xx, xx, mrm|predcc, fRP, END_LIST},
  {OP_cmovnp, 0x0f4b10, "cmovnp", Gv, xx, Ev, xx, xx, mrm|predcc, fRP, END_LIST},
  {OP_cmovl,  0x0f4c10, "cmovl",  Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO), END_LIST},
  {OP_cmovnl, 0x0f4d10, "cmovnl", Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO), END_LIST},
  {OP_cmovle, 0x0f4e10, "cmovle", Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO|fRZ), END_LIST},
  {OP_cmovnle,0x0f4f10, "cmovnle",Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO|fRZ), END_LIST},
  /* 50 */
  {PREFIX_EXT, 0x0f5010, "(prefix ext 16)", xx, xx, xx, xx, xx, mrm, x, 16},
  {PREFIX_EXT, 0x0f5110, "(prefix ext 17)", xx, xx, xx, xx, xx, mrm, x, 17},
  {PREFIX_EXT, 0x0f5210, "(prefix ext 18)", xx, xx, xx, xx, xx, mrm, x, 18},
  {PREFIX_EXT, 0x0f5310, "(prefix ext 19)", xx, xx, xx, xx, xx, mrm, x, 19},
  {PREFIX_EXT, 0x0f5410, "(prefix ext 20)", xx, xx, xx, xx, xx, mrm, x, 20},
  {PREFIX_EXT, 0x0f5510, "(prefix ext 21)", xx, xx, xx, xx, xx, mrm, x, 21},
  {PREFIX_EXT, 0x0f5610, "(prefix ext 22)", xx, xx, xx, xx, xx, mrm, x, 22},
  {PREFIX_EXT, 0x0f5710, "(prefix ext 23)", xx, xx, xx, xx, xx, mrm, x, 23},
  /* 58 */
  {PREFIX_EXT, 0x0f5810, "(prefix ext 24)", xx, xx, xx, xx, xx, mrm, x, 24},
  {PREFIX_EXT, 0x0f5910, "(prefix ext 25)", xx, xx, xx, xx, xx, mrm, x, 25},
  {PREFIX_EXT, 0x0f5a10, "(prefix ext 26)", xx, xx, xx, xx, xx, mrm, x, 26},
  {PREFIX_EXT, 0x0f5b10, "(prefix ext 27)", xx, xx, xx, xx, xx, mrm, x, 27},
  {PREFIX_EXT, 0x0f5c10, "(prefix ext 28)", xx, xx, xx, xx, xx, mrm, x, 28},
  {PREFIX_EXT, 0x0f5d10, "(prefix ext 29)", xx, xx, xx, xx, xx, mrm, x, 29},
  {PREFIX_EXT, 0x0f5e10, "(prefix ext 30)", xx, xx, xx, xx, xx, mrm, x, 30},
  {PREFIX_EXT, 0x0f5f10, "(prefix ext 31)", xx, xx, xx, xx, xx, mrm, x, 31},
  /* 60 */
  {PREFIX_EXT, 0x0f6010, "(prefix ext 32)", xx, xx, xx, xx, xx, mrm, x, 32},
  {PREFIX_EXT, 0x0f6110, "(prefix ext 33)", xx, xx, xx, xx, xx, mrm, x, 33},
  {PREFIX_EXT, 0x0f6210, "(prefix ext 34)", xx, xx, xx, xx, xx, mrm, x, 34},
  {PREFIX_EXT, 0x0f6310, "(prefix ext 35)", xx, xx, xx, xx, xx, mrm, x, 35},
  {PREFIX_EXT, 0x0f6410, "(prefix ext 36)", xx, xx, xx, xx, xx, mrm, x, 36},
  {PREFIX_EXT, 0x0f6510, "(prefix ext 37)", xx, xx, xx, xx, xx, mrm, x, 37},
  {PREFIX_EXT, 0x0f6610, "(prefix ext 38)", xx, xx, xx, xx, xx, mrm, x, 38},
  {PREFIX_EXT, 0x0f6710, "(prefix ext 39)", xx, xx, xx, xx, xx, mrm, x, 39},
  /* 68 */
  {PREFIX_EXT, 0x0f6810, "(prefix ext 40)", xx, xx, xx, xx, xx, mrm, x, 40},
  {PREFIX_EXT, 0x0f6910, "(prefix ext 41)", xx, xx, xx, xx, xx, mrm, x, 41},
  {PREFIX_EXT, 0x0f6a10, "(prefix ext 42)", xx, xx, xx, xx, xx, mrm, x, 42},
  {PREFIX_EXT, 0x0f6b10, "(prefix ext 43)", xx, xx, xx, xx, xx, mrm, x, 43},
  {PREFIX_EXT, 0x0f6c10, "(prefix ext 44)", xx, xx, xx, xx, xx, mrm, x, 44},
  {PREFIX_EXT, 0x0f6d10, "(prefix ext 45)", xx, xx, xx, xx, xx, mrm, x, 45},
  {PREFIX_EXT, 0x0f6e10, "(prefix ext 46)", xx, xx, xx, xx, xx, mrm, x, 46},
  {PREFIX_EXT, 0x0f6f10, "(prefix ext 112)", xx, xx, xx, xx, xx, mrm, x, 112},
  /* 70 */
  {PREFIX_EXT, 0x0f7010, "(prefix ext 47)", xx, xx, xx, xx, xx, mrm, x, 47},
  {EXTENSION, 0x0f7110, "(group 12)", xx, xx, xx, xx, xx, mrm, x, 19},
  {EXTENSION, 0x0f7210, "(group 13)", xx, xx, xx, xx, xx, mrm, x, 20},
  {EXTENSION, 0x0f7310, "(group 14)", xx, xx, xx, xx, xx, mrm, x, 21},
  {PREFIX_EXT, 0x0f7410, "(prefix ext 48)", xx, xx, xx, xx, xx, mrm, x, 48},
  {PREFIX_EXT, 0x0f7510, "(prefix ext 49)", xx, xx, xx, xx, xx, mrm, x, 49},
  {PREFIX_EXT, 0x0f7610, "(prefix ext 50)", xx, xx, xx, xx, xx, mrm, x, 50},
  {VEX_L_EXT,  0x0f7710, "(vex L ext 0)", xx, xx, xx, xx, xx, no, x, 0},
  /* 78 */
  {PREFIX_EXT, 0x0f7810, "(prefix ext 134)", xx, xx, xx, xx, xx, mrm, x, 134},
  {PREFIX_EXT, 0x0f7910, "(prefix ext 135)", xx, xx, xx, xx, xx, mrm, x, 135},
  {INVALID, 0x0f7a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f7b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {PREFIX_EXT, 0x0f7c10, "(prefix ext 114)", xx, xx, xx, xx, xx, mrm, x, 114},
  {PREFIX_EXT, 0x0f7d10, "(prefix ext 115)", xx, xx, xx, xx, xx, mrm, x, 115},
  {PREFIX_EXT, 0x0f7e10, "(prefix ext 51)", xx, xx, xx, xx, xx, mrm, x, 51},
  {PREFIX_EXT, 0x0f7f10, "(prefix ext 113)", xx, xx, xx, xx, xx, mrm, x, 113},
  /* 80 */
  {OP_jo,  0x0f8010, "jo",  xx, xx, Jz, xx, xx, no, fRO, END_LIST},
  {OP_jno, 0x0f8110, "jno", xx, xx, Jz, xx, xx, no, fRO, END_LIST},
  {OP_jb,  0x0f8210, "jb",  xx, xx, Jz, xx, xx, no, fRC, END_LIST},
  {OP_jnb, 0x0f8310, "jnb", xx, xx, Jz, xx, xx, no, fRC, END_LIST},
  {OP_jz,  0x0f8410, "jz",  xx, xx, Jz, xx, xx, no, fRZ, END_LIST},
  {OP_jnz, 0x0f8510, "jnz", xx, xx, Jz, xx, xx, no, fRZ, END_LIST},
  {OP_jbe, 0x0f8610, "jbe", xx, xx, Jz, xx, xx, no, (fRC|fRZ), END_LIST},
  {OP_jnbe,0x0f8710, "jnbe",xx, xx, Jz, xx, xx, no, (fRC|fRZ), END_LIST},
  /* 88 */
  {OP_js,  0x0f8810, "js",  xx, xx, Jz, xx, xx, no, fRS, END_LIST},
  {OP_jns, 0x0f8910, "jns", xx, xx, Jz, xx, xx, no, fRS, END_LIST},
  {OP_jp,  0x0f8a10, "jp",  xx, xx, Jz, xx, xx, no, fRP, END_LIST},
  {OP_jnp, 0x0f8b10, "jnp", xx, xx, Jz, xx, xx, no, fRP, END_LIST},
  {OP_jl,  0x0f8c10, "jl",  xx, xx, Jz, xx, xx, no, (fRS|fRO), END_LIST},
  {OP_jnl, 0x0f8d10, "jnl", xx, xx, Jz, xx, xx, no, (fRS|fRO), END_LIST},
  {OP_jle, 0x0f8e10, "jle", xx, xx, Jz, xx, xx, no, (fRS|fRO|fRZ), END_LIST},
  {OP_jnle,0x0f8f10, "jnle",xx, xx, Jz, xx, xx, no, (fRS|fRO|fRZ), END_LIST},
  /* 90 */
  {OP_seto,  0x0f9010, "seto",  Eb, xx, xx, xx, xx, mrm, fRO, END_LIST},
  {OP_setno, 0x0f9110, "setno", Eb, xx, xx, xx, xx, mrm, fRO, END_LIST},
  {OP_setb,  0x0f9210, "setb",  Eb, xx, xx, xx, xx, mrm, fRC, END_LIST},
  {OP_setnb, 0x0f9310, "setnb", Eb, xx, xx, xx, xx, mrm, fRC, END_LIST},
  {OP_setz,  0x0f9410, "setz",  Eb, xx, xx, xx, xx, mrm, fRZ, END_LIST},
  {OP_setnz, 0x0f9510, "setnz", Eb, xx, xx, xx, xx, mrm, fRZ, END_LIST},
  {OP_setbe, 0x0f9610, "setbe", Eb, xx, xx, xx, xx, mrm, (fRC|fRZ), END_LIST},
  {OP_setnbe,0x0f9710, "setnbe",Eb, xx, xx, xx, xx, mrm, (fRC|fRZ), END_LIST},
  /* 98 */
  {OP_sets,  0x0f9810, "sets",  Eb, xx, xx, xx, xx, mrm, fRS, END_LIST},
  {OP_setns, 0x0f9910, "setns", Eb, xx, xx, xx, xx, mrm, fRS, END_LIST},
  {OP_setp,  0x0f9a10, "setp",  Eb, xx, xx, xx, xx, mrm, fRP, END_LIST},
  {OP_setnp, 0x0f9b10, "setnp", Eb, xx, xx, xx, xx, mrm, fRP, END_LIST},
  {OP_setl,  0x0f9c10, "setl",  Eb, xx, xx, xx, xx, mrm, (fRS|fRO), END_LIST},
  {OP_setnl, 0x0f9d10, "setnl", Eb, xx, xx, xx, xx, mrm, (fRS|fRO), END_LIST},
  {OP_setle, 0x0f9e10, "setle", Eb, xx, xx, xx, xx, mrm, (fRS|fRO|fRZ), END_LIST},
  {OP_setnle,0x0f9f10, "setnle",Eb, xx, xx, xx, xx, mrm, (fRS|fRO|fRZ), END_LIST},
  /* a0 */
  {OP_push, 0x0fa010, "push", xsp, i_xSPo1, fs, xsp, xx, no, x, tsb[0xa8]},
  {OP_pop,  0x0fa110, "pop", fs, xsp, xsp, i_xSP, xx, no, x, tsb[0xa9]},
  {OP_cpuid, 0x0fa210, "cpuid", eax, ebx, eax, ecx, xx, xop, x, exop[0x06]},
  {OP_bt,   0x0fa310, "bt",   xx, xx, Ev, Gv, xx, mrm, fW6, tex[15][4]},
  {OP_shld, 0x0fa410, "shld", Ev, xx, Gv, Ib, Ev, mrm, fW6, tsb[0xa5]},
  {OP_shld, 0x0fa510, "shld", Ev, xx, Gv, cl, Ev, mrm, fW6, END_LIST},
  {INVALID, 0x0fa610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0fa710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  /* a8 */
  {OP_push, 0x0fa810, "push", xsp, i_xSPo1, gs, xsp, xx, no, x, END_LIST},
  {OP_pop,  0x0fa910, "pop", gs, xsp, xsp, i_xSP, xx, no, x, END_LIST},
  {OP_rsm,  0x0faa10, "rsm", xx, xx, xx, xx, xx, no, fWX, END_LIST},
  {OP_bts,  0x0fab10, "bts", Ev, xx, Gv, Ev, xx, mrm, fW6, tex[15][5]},
  {OP_shrd, 0x0fac10, "shrd", Ev, xx, Gv, Ib, Ev, mrm, fW6, tsb[0xad]},
  {OP_shrd, 0x0fad10, "shrd", Ev, xx, Gv, cl, Ev, mrm, fW6, END_LIST},
  {EXTENSION, 0x0fae10, "(group 15)", xx, xx, xx, xx, xx, mrm, x, 22},
  {OP_imul, 0x0faf10, "imul", Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x69]},
  /* b0 */
  {OP_cmpxchg, 0x0fb010, "cmpxchg", Eb, al, Gb, Eb, al, mrm, fW6, END_LIST},
  {OP_cmpxchg, 0x0fb110, "cmpxchg", Ev, eAX, Gv, Ev, eAX, mrm, fW6, tsb[0xb0]},
  {OP_lss, 0x0fb210, "lss", Gv, ss, Mp, xx, xx, mrm, x, END_LIST},
  {OP_btr, 0x0fb310, "btr", Ev, xx, Gv, Ev, xx, mrm, fW6, tex[15][6]},
  {OP_lfs, 0x0fb410, "lfs", Gv, fs, Mp, xx, xx, mrm, x, END_LIST},
  {OP_lgs, 0x0fb510, "lgs", Gv, gs, Mp, xx, xx, mrm, x, END_LIST},
  {OP_movzx, 0x0fb610, "movzx", Gv, xx, Eb, xx, xx, mrm, x, END_LIST},
  {OP_movzx, 0x0fb710, "movzx", Gv, xx, Ew, xx, xx, mrm, x, tsb[0xb6]},
  /* b8 */
#ifdef IA32_ON_IA64
  /* FIXME : unsure about this encoding */
  /* had to define type Av for this to work, see decode.c/above etc.  */
  /* is jump to absolute pc address, not realitve like all other jumps */
  /* is like the pointer type except no segement change, no modrm byte */
  /* should be 0x0fb800? no! 01 signals to encoder is 2 byte instruction */
  {OP_jmpe_abs,  0x0fb810, "jmpe", xx, xx, Av, xx, xx, no, x, END_LIST},
#else
  {OP_popcnt,0xf30fb810, "popcnt", Gv, xx, Ev, xx, xx, mrm|reqp, fW6, END_LIST},
#endif
  /* This is Group 10, but all identical (ud2b) so no reason to split opcode by /reg */
  {OP_ud2b, 0x0fb910, "ud2b", xx, xx, xx, xx, xx, no, x, END_LIST},
  {EXTENSION, 0x0fba10, "(group 8)", xx, xx, xx, xx, xx, mrm, x, 15},
  {OP_btc, 0x0fbb10, "btc", Ev, xx, Gv, Ev, xx, mrm, fW6, tex[15][7]},
  {PREFIX_EXT, 0x0fbc10, "(prefix ext 140)", xx, xx, xx, xx, xx, mrm, x, 140},
  {PREFIX_EXT, 0x0fbd10, "(prefix ext 136)", xx, xx, xx, xx, xx, mrm, x, 136},
  {OP_movsx, 0x0fbe10, "movsx", Gv, xx, Eb, xx, xx, mrm, x, END_LIST},
  {OP_movsx, 0x0fbf10, "movsx", Gv, xx, Ew, xx, xx, mrm, x, tsb[0xbe]},
  /* c0 */
  {OP_xadd, 0x0fc010, "xadd", Eb, Gb, Eb, Gb, xx, mrm, fW6, END_LIST},
  {OP_xadd, 0x0fc110, "xadd", Ev, Gv, Ev, Gv, xx, mrm, fW6, tsb[0xc0]},
  {PREFIX_EXT, 0x0fc210, "(prefix ext 52)", xx, xx, xx, xx, xx, mrm, x, 52},
  {OP_movnti, 0x0fc310, "movnti", Md_q, xx, Gd_q, xx, xx, mrm, x, END_LIST},
  {PREFIX_EXT, 0x0fc410, "(prefix ext 53)", xx, xx, xx, xx, xx, mrm, x, 53},
  {PREFIX_EXT, 0x0fc510, "(prefix ext 54)", xx, xx, xx, xx, xx, mrm, x, 54},
  {PREFIX_EXT, 0x0fc610, "(prefix ext 55)", xx, xx, xx, xx, xx, mrm, x, 55},
  {EXTENSION, 0x0fc710, "(group 9)", xx, xx, xx, xx, xx, mrm, x, 16},
  /* c8 */
  {OP_bswap, 0x0fc810, "bswap", uAX_x, xx, uAX_x, xx, xx, no, x, tsb[0xc9]},
  {OP_bswap, 0x0fc910, "bswap", uCX_x, xx, uCX_x, xx, xx, no, x, tsb[0xca]},
  {OP_bswap, 0x0fca10, "bswap", uDX_x, xx, uDX_x, xx, xx, no, x, tsb[0xcb]},
  {OP_bswap, 0x0fcb10, "bswap", uBX_x, xx, uBX_x, xx, xx, no, x, tsb[0xcc]},
  {OP_bswap, 0x0fcc10, "bswap", uSP_x, xx, uSP_x, xx, xx, no, x, tsb[0xcd]},
  {OP_bswap, 0x0fcd10, "bswap", uBP_x, xx, uBP_x, xx, xx, no, x, tsb[0xce]},
  {OP_bswap, 0x0fce10, "bswap", uSI_x, xx, uSI_x, xx, xx, no, x, tsb[0xcf]},
  {OP_bswap, 0x0fcf10, "bswap", uDI_x, xx, uDI_x, xx, xx, no, x, END_LIST},
  /* d0 */
  {PREFIX_EXT, 0x0fd010, "(prefix ext 116)", xx, xx, xx, xx, xx, mrm, x, 116},
  {PREFIX_EXT, 0x0fd110, "(prefix ext 56)", xx, xx, xx, xx, xx, mrm, x, 56},
  {PREFIX_EXT, 0x0fd210, "(prefix ext 57)", xx, xx, xx, xx, xx, mrm, x, 57},
  {PREFIX_EXT, 0x0fd310, "(prefix ext 58)", xx, xx, xx, xx, xx, mrm, x, 58},
  {PREFIX_EXT, 0x0fd410, "(prefix ext 59)", xx, xx, xx, xx, xx, mrm, x, 59},
  {PREFIX_EXT, 0x0fd510, "(prefix ext 60)", xx, xx, xx, xx, xx, mrm, x, 60},
  {PREFIX_EXT, 0x0fd610, "(prefix ext 61)", xx, xx, xx, xx, xx, mrm, x, 61},
  {PREFIX_EXT, 0x0fd710, "(prefix ext 62)", xx, xx, xx, xx, xx, mrm, x, 62},
  /* d8 */
  {PREFIX_EXT, 0x0fd810, "(prefix ext 63)", xx, xx, xx, xx, xx, mrm, x, 63},
  {PREFIX_EXT, 0x0fd910, "(prefix ext 64)", xx, xx, xx, xx, xx, mrm, x, 64},
  {PREFIX_EXT, 0x0fda10, "(prefix ext 65)", xx, xx, xx, xx, xx, mrm, x, 65},
  {PREFIX_EXT, 0x0fdb10, "(prefix ext 66)", xx, xx, xx, xx, xx, mrm, x, 66},
  {PREFIX_EXT, 0x0fdc10, "(prefix ext 67)", xx, xx, xx, xx, xx, mrm, x, 67},
  {PREFIX_EXT, 0x0fdd10, "(prefix ext 68)", xx, xx, xx, xx, xx, mrm, x, 68},
  {PREFIX_EXT, 0x0fde10, "(prefix ext 69)", xx, xx, xx, xx, xx, mrm, x, 69},
  {PREFIX_EXT, 0x0fdf10, "(prefix ext 70)", xx, xx, xx, xx, xx, mrm, x, 70},
  /* e0 */
  {PREFIX_EXT, 0x0fe010, "(prefix ext 71)", xx, xx, xx, xx, xx, mrm, x, 71},
  {PREFIX_EXT, 0x0fe110, "(prefix ext 72)", xx, xx, xx, xx, xx, mrm, x, 72},
  {PREFIX_EXT, 0x0fe210, "(prefix ext 73)", xx, xx, xx, xx, xx, mrm, x, 73},
  {PREFIX_EXT, 0x0fe310, "(prefix ext 74)", xx, xx, xx, xx, xx, mrm, x, 74},
  {PREFIX_EXT, 0x0fe410, "(prefix ext 75)", xx, xx, xx, xx, xx, mrm, x, 75},
  {PREFIX_EXT, 0x0fe510, "(prefix ext 76)", xx, xx, xx, xx, xx, mrm, x, 76},
  {PREFIX_EXT, 0x0fe610, "(prefix ext 77)", xx, xx, xx, xx, xx, mrm, x, 77},
  {PREFIX_EXT, 0x0fe710, "(prefix ext 78)", xx, xx, xx, xx, xx, mrm, x, 78},
  /* e8 */
  {PREFIX_EXT, 0x0fe810, "(prefix ext 79)", xx, xx, xx, xx, xx, mrm, x, 79},
  {PREFIX_EXT, 0x0fe910, "(prefix ext 80)", xx, xx, xx, xx, xx, mrm, x, 80},
  {PREFIX_EXT, 0x0fea10, "(prefix ext 81)", xx, xx, xx, xx, xx, mrm, x, 81},
  {PREFIX_EXT, 0x0feb10, "(prefix ext 82)", xx, xx, xx, xx, xx, mrm, x, 82},
  {PREFIX_EXT, 0x0fec10, "(prefix ext 83)", xx, xx, xx, xx, xx, mrm, x, 83},
  {PREFIX_EXT, 0x0fed10, "(prefix ext 84)", xx, xx, xx, xx, xx, mrm, x, 84},
  {PREFIX_EXT, 0x0fee10, "(prefix ext 85)", xx, xx, xx, xx, xx, mrm, x, 85},
  {PREFIX_EXT, 0x0fef10, "(prefix ext 86)", xx, xx, xx, xx, xx, mrm, x, 86},
  /* f0 */
  {PREFIX_EXT, 0x0ff010, "(prefix ext 117)", xx, xx, xx, xx, xx, mrm, x, 117},
  {PREFIX_EXT, 0x0ff110, "(prefix ext 87)", xx, xx, xx, xx, xx, mrm, x, 87},
  {PREFIX_EXT, 0x0ff210, "(prefix ext 88)", xx, xx, xx, xx, xx, mrm, x, 88},
  {PREFIX_EXT, 0x0ff310, "(prefix ext 89)", xx, xx, xx, xx, xx, mrm, x, 89},
  {PREFIX_EXT, 0x0ff410, "(prefix ext 90)", xx, xx, xx, xx, xx, mrm, x, 90},
  {PREFIX_EXT, 0x0ff510, "(prefix ext 91)", xx, xx, xx, xx, xx, mrm, x, 91},
  {PREFIX_EXT, 0x0ff610, "(prefix ext 92)", xx, xx, xx, xx, xx, mrm, x, 92},
  {PREFIX_EXT, 0x0ff710, "(prefix ext 93)", xx, xx, xx, xx, xx, mrm, x, 93},
  /* f8 */
  {PREFIX_EXT, 0x0ff810, "(prefix ext 94)", xx, xx, xx, xx, xx, mrm, x, 94},
  {PREFIX_EXT, 0x0ff910, "(prefix ext 95)", xx, xx, xx, xx, xx, mrm, x, 95},
  {PREFIX_EXT, 0x0ffa10, "(prefix ext 96)", xx, xx, xx, xx, xx, mrm, x, 96},
  {PREFIX_EXT, 0x0ffb10, "(prefix ext 97)", xx, xx, xx, xx, xx, mrm, x, 97},
  {PREFIX_EXT, 0x0ffc10, "(prefix ext 98)", xx, xx, xx, xx, xx, mrm, x, 98},
  {PREFIX_EXT, 0x0ffd10, "(prefix ext 99)", xx, xx, xx, xx, xx, mrm, x, 99},
  {PREFIX_EXT, 0x0ffe10, "(prefix ext 100)", xx, xx, xx, xx, xx, mrm, x, 100},
  {INVALID, 0x0fff10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
};

/****************************************************************************
 * Opcode extensions
 * This is from Table A-6
 */
const instr_info_t extensions[][8] = {
  /* group 1a -- first opcode byte 80: all assumed to have Ib */
  { /* extensions[0] */
    {OP_add, 0x800020, "add", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][0]},
    {OP_or,  0x800021, "or",  Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][1]},
    {OP_adc, 0x800022, "adc", Eb, xx, Ib, Eb, xx, mrm, (fW6|fRC), tex[25][2]},
    {OP_sbb, 0x800023, "sbb", Eb, xx, Ib, Eb, xx, mrm, (fW6|fRC), tex[25][3]},
    {OP_and, 0x800024, "and", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][4]},
    {OP_sub, 0x800025, "sub", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][5]},
    {OP_xor, 0x800026, "xor", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][6]},
    {OP_cmp, 0x800027, "cmp", xx, xx, Eb, Ib, xx, mrm, fW6,  tex[25][7]},
 },
  /* group 1b -- first opcode byte 81: all assumed to have Iz */
  { /* extensions[1] */
    {OP_add, 0x810020, "add", Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][0]},
    {OP_or,  0x810021, "or",  Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][1]},
    {OP_adc, 0x810022, "adc", Ev, xx, Iz, Ev, xx, mrm, (fW6|fRC), tex[2][2]},
    {OP_sbb, 0x810023, "sbb", Ev, xx, Iz, Ev, xx, mrm, (fW6|fRC), tex[2][3]},
    {OP_and, 0x810024, "and", Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][4]},
    {OP_sub, 0x810025, "sub", Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][5]},
    {OP_xor, 0x810026, "xor", Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][6]},
    {OP_cmp, 0x810027, "cmp", xx, xx, Ev, Iz, xx, mrm, fW6,  tex[2][7]},
 },
  /* group 1c -- first opcode byte 83 (for 82, see below "group 1c*"):
   * all assumed to have Ib */
  { /* extensions[2] */
    {OP_add, 0x830020, "add", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][0]},
    {OP_or,  0x830021, "or",  Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][1]},
    {OP_adc, 0x830022, "adc", Ev, xx, Ib, Ev, xx, mrm, (fW6|fRC), tex[0][2]},
    {OP_sbb, 0x830023, "sbb", Ev, xx, Ib, Ev, xx, mrm, (fW6|fRC), tex[0][3]},
    {OP_and, 0x830024, "and", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][4]},
    {OP_sub, 0x830025, "sub", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][5]},
    {OP_xor, 0x830026, "xor", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][6]},
    {OP_cmp, 0x830027, "cmp", xx, xx, Ev, Ib, xx, mrm, fW6,  tex[0][7]},
 },
  /* group 2a -- first opcode byte c0: all assumed to have Ib */
  { /* extensions[3] */
    {OP_rol, 0xc00020, "rol", Eb, xx, Ib, Eb, xx, mrm, (fWC|fWO),  tex[5][0]},
    {OP_ror, 0xc00021, "ror", Eb, xx, Ib, Eb, xx, mrm, (fWC|fWO),  tex[5][1]},
    {OP_rcl, 0xc00022, "rcl", Eb, xx, Ib, Eb, xx, mrm, (fRC|fWC|fWO), tex[5][2]},
    {OP_rcr, 0xc00023, "rcr", Eb, xx, Ib, Eb, xx, mrm, (fRC|fWC|fWO), tex[5][3]},
    {OP_shl, 0xc00024, "shl", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[5][4]},
    {OP_shr, 0xc00025, "shr", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[5][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xc00026, "shl", Eb, xx, Ib, Eb, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xc00027, "sar", Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[5][7]},
 },
  /* group 2b -- first opcode byte c1: all assumed to have Ib */
  { /* extensions[4] */
    {OP_rol, 0xc10020, "rol", Ev, xx, Ib, Ev, xx, mrm, (fWC|fWO),  tex[6][0]},
    {OP_ror, 0xc10021, "ror", Ev, xx, Ib, Ev, xx, mrm, (fWC|fWO),  tex[6][1]},
    {OP_rcl, 0xc10022, "rcl", Ev, xx, Ib, Ev, xx, mrm, (fRC|fWC|fWO), tex[6][2]},
    {OP_rcr, 0xc10023, "rcr", Ev, xx, Ib, Ev, xx, mrm, (fRC|fWC|fWO), tex[6][3]},
    {OP_shl, 0xc10024, "shl", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[6][4]},
    {OP_shr, 0xc10025, "shr", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[6][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xc10026, "shl", Ev, xx, Ib, Ev, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xc10027, "sar", Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[6][7]},
 },
  /* group 2c -- first opcode byte d0 */
  { /* extensions[5] */
    {OP_rol, 0xd00020, "rol", Eb, xx, c1, Eb, xx, mrm, (fWC|fWO),  tex[8][0]},
    {OP_ror, 0xd00021, "ror", Eb, xx, c1, Eb, xx, mrm, (fWC|fWO),  tex[8][1]},
    {OP_rcl, 0xd00022, "rcl", Eb, xx, c1, Eb, xx, mrm, (fRC|fWC|fWO), tex[8][2]},
    {OP_rcr, 0xd00023, "rcr", Eb, xx, c1, Eb, xx, mrm, (fRC|fWC|fWO), tex[8][3]},
    {OP_shl, 0xd00024, "shl", Eb, xx, c1, Eb, xx, mrm, fW6,  tex[8][4]},
    {OP_shr, 0xd00025, "shr", Eb, xx, c1, Eb, xx, mrm, fW6,  tex[8][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd00026, "shl", Eb, xx, c1, Eb, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd00027, "sar", Eb, xx, c1, Eb, xx, mrm, fW6,  tex[8][7]},
 },
  /* group 2d -- first opcode byte d1 */
  { /* extensions[6] */
    {OP_rol, 0xd10020, "rol", Ev, xx, c1, Ev, xx, mrm, (fWC|fWO),  tex[3][0]},
    {OP_ror, 0xd10021, "ror", Ev, xx, c1, Ev, xx, mrm, (fWC|fWO),  tex[3][1]},
    {OP_rcl, 0xd10022, "rcl", Ev, xx, c1, Ev, xx, mrm, (fRC|fWC|fWO), tex[3][2]},
    {OP_rcr, 0xd10023, "rcr", Ev, xx, c1, Ev, xx, mrm, (fRC|fWC|fWO), tex[3][3]},
    {OP_shl, 0xd10024, "shl", Ev, xx, c1, Ev, xx, mrm, fW6,  tex[3][4]},
    {OP_shr, 0xd10025, "shr", Ev, xx, c1, Ev, xx, mrm, fW6,  tex[3][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd10026, "shl", Ev, xx, c1, Ev, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd10027, "sar", Ev, xx, c1, Ev, xx, mrm, fW6,  tex[3][7]},
 },
  /* group 2e -- first opcode byte d2 */
  { /* extensions[7] */
    {OP_rol, 0xd20020, "rol", Eb, xx, cl, Eb, xx, mrm, (fWC|fWO),  END_LIST},
    {OP_ror, 0xd20021, "ror", Eb, xx, cl, Eb, xx, mrm, (fWC|fWO),  END_LIST},
    {OP_rcl, 0xd20022, "rcl", Eb, xx, cl, Eb, xx, mrm, (fRC|fWC|fWO), END_LIST},
    {OP_rcr, 0xd20023, "rcr", Eb, xx, cl, Eb, xx, mrm, (fRC|fWC|fWO), END_LIST},
    {OP_shl, 0xd20024, "shl", Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
    {OP_shr, 0xd20025, "shr", Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd20026, "shl", Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd20027, "sar", Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
 },
  /* group 2f -- first opcode byte d3 */
  { /* extensions[8] */
    {OP_rol, 0xd30020, "rol", Ev, xx, cl, Ev, xx, mrm, (fWC|fWO),  tex[7][0]},
    {OP_ror, 0xd30021, "ror", Ev, xx, cl, Ev, xx, mrm, (fWC|fWO),  tex[7][1]},
    {OP_rcl, 0xd30022, "rcl", Ev, xx, cl, Ev, xx, mrm, (fRC|fWC|fWO), tex[7][2]},
    {OP_rcr, 0xd30023, "rcr", Ev, xx, cl, Ev, xx, mrm, (fRC|fWC|fWO), tex[7][3]},
    {OP_shl, 0xd30024, "shl", Ev, xx, cl, Ev, xx, mrm, fW6,  tex[7][4]},
    {OP_shr, 0xd30025, "shr", Ev, xx, cl, Ev, xx, mrm, fW6,  tex[7][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd30026, "shl", Ev, xx, cl, Ev, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd30027, "sar", Ev, xx, cl, Ev, xx, mrm, fW6,  tex[7][7]},
 },
  /* group 3a -- first opcode byte f6 */
  { /* extensions[9] */
    {OP_test, 0xf60020, "test", xx, xx, Eb, Ib, xx, mrm, fW6, END_LIST},
    /* PR 332254: /1 is an alias for /0; we do not add to encoding chain though */
    {OP_test, 0xf60021, "test", xx, xx, Eb, Ib, xx, mrm, fW6, END_LIST},
    {OP_not,  0xf60022, "not", Eb, xx, Eb, xx, xx, mrm, x, END_LIST},
    {OP_neg,  0xf60023, "neg", Eb, xx, Eb, xx, xx, mrm, fW6, END_LIST},
    {OP_mul,  0xf60024, "mul", ax, xx, Eb, al, xx, mrm, fW6, END_LIST},
    {OP_imul, 0xf60025, "imul", ax, xx, Eb, al, xx, mrm, fW6, tsb[0xaf]},
    {OP_div,  0xf60026, "div", ah, al, Eb, ax, xx, mrm, fW6, END_LIST},
    {OP_idiv, 0xf60027, "idiv", ah, al, Eb, ax, xx, mrm, fW6, END_LIST},
 },
  /* group 3b -- first opcode byte f7 */
  { /* extensions[10] */
    {OP_test, 0xf70020, "test", xx,  xx, Ev, Iz, xx, mrm, fW6, tex[9][0]},
    /* PR 332254: /1 is an alias for /0; we do not add to encoding chain though */
    {OP_test, 0xf70021, "test", xx,  xx, Ev, Iz, xx, mrm, fW6, END_LIST},
    {OP_not,  0xf70022, "not", Ev,  xx, Ev, xx, xx, mrm, x, tex[9][2]},
    {OP_neg,  0xf70023, "neg", Ev,  xx, Ev, xx, xx, mrm, fW6, tex[9][3]},
    {OP_mul,  0xf70024, "mul",   eDX, eAX, Ev, eAX, xx, mrm, fW6, tex[9][4]},
    {OP_imul, 0xf70025, "imul",  eDX, eAX, Ev, eAX, xx, mrm, fW6, tex[9][5]},
    {OP_div,  0xf70026, "div",   eDX, eAX, Ev, eDX, eAX, mrm, fW6, tex[9][6]},
    {OP_idiv, 0xf70027, "idiv",  eDX, eAX, Ev, eDX, eAX, mrm, fW6, tex[9][7]},
 },
  /* group 4 (first byte fe) */
  { /* extensions[11] */
    {OP_inc, 0xfe0020, "inc", Eb, xx, Eb, xx, xx, mrm, (fW6&(~fWC)), END_LIST},
    {OP_dec, 0xfe0021, "dec", Eb, xx, Eb, xx, xx, mrm, (fW6&(~fWC)), END_LIST},
    {INVALID, 0xfe0022, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0023, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0024, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0025, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0026, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0027, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 5 (first byte ff) */
  { /* extensions[12] */
    {OP_inc, 0xff0020, "inc", Ev, xx, Ev, xx, xx, mrm, (fW6&(~fWC)), tex[11][0]},
    {OP_dec, 0xff0021, "dec", Ev, xx, Ev, xx, xx, mrm, (fW6&(~fWC)), tex[11][1]},
    {OP_call_ind,     0xff0022, "call",  xsp, i_iSPo1, i_Exi, xsp, xx, mrm, x, END_LIST},
    /* Note how a far call's stack operand size matches far ret rather than call */
    {OP_call_far_ind, 0xff0023, "lcall",  xsp, i_vSPo2, i_Ep, xsp, xx, mrm, x, END_LIST},
    {OP_jmp_ind,      0xff0024, "jmp",  xx, xx, i_Exi, xx, xx, mrm, x, END_LIST},
    {OP_jmp_far_ind,  0xff0025, "ljmp",  xx, xx, i_Ep, xx, xx, mrm, x, END_LIST},
    {OP_push, 0xff0026, "push", xsp, i_xSPo1, Esv, xsp, xx, mrm, x, tfb[0x06]},
    {INVALID, 0xff0027, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 6 (first bytes 0f 00) */
  { /* extensions[13] */
    {OP_sldt, 0x0f0030, "sldt", Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_str,  0x0f0031, "str", Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_lldt, 0x0f0032, "lldt", xx, xx, Ew, xx, xx, mrm, x, END_LIST},
    {OP_ltr,  0x0f0033, "ltr", xx, xx, Ew, xx, xx, mrm, x, END_LIST},
    {OP_verr, 0x0f0034, "verr", xx, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
    {OP_verw, 0x0f0035, "verw", xx, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
#ifdef IA32_ON_IA64
    {OP_jmpe, 0x0f0036, "jmpe", xx, xx, i_Ev, xx, xx, mrm, x, END_LIST},
#else
    {INVALID, 0x0f0036, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
#endif
    {INVALID, 0x0f0037, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 7 (first bytes 0f 01) */
  { /* extensions[14] */
    {MOD_EXT, 0x0f0130, "(group 7 mod ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {MOD_EXT, 0x0f0131, "(group 7 mod ext 1)", xx, xx, xx, xx, xx, no, x, 1},
    {MOD_EXT, 0x0f0132, "(group 7 mod ext 5)", xx, xx, xx, xx, xx, no, x, 5},
    {MOD_EXT, 0x0f0133, "(group 7 mod ext 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_smsw, 0x0f0134, "smsw",  Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f0135, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_lmsw, 0x0f0136, "lmsw",  xx, xx, Ew, xx, xx, mrm, x, END_LIST},
    {MOD_EXT, 0x0f0137, "(group 7 mod ext 2)", xx, xx, xx, xx, xx, no, x, 2},
  },
  /* group 8 (first bytes 0f ba): all assumed to have Ib */
  { /* extensions[15] */
    {INVALID, 0x0fba30, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fba31, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fba32, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fba33, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
    {OP_bt,  0x0fba34, "bt",    xx, xx, Ev, Ib, xx, mrm, fW6, END_LIST},
    {OP_bts, 0x0fba35, "bts", Ev, xx, Ib, Ev, xx, mrm, fW6, END_LIST},
    {OP_btr, 0x0fba36, "btr", Ev, xx, Ib, Ev, xx, mrm, fW6, END_LIST},
    {OP_btc, 0x0fba37, "btc", Ev, xx, Ib, Ev, xx, mrm, fW6, END_LIST},
  },
  /* group 9 (first bytes 0f c7) */
  { /* extensions[16] */
    {INVALID, 0x0fc730, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_cmpxchg8b, 0x0fc731, "cmpxchg8b", Mq_dq, eAX, Mq_dq, eAX, eDX, mrm_xop, fWZ, exop[0x07]},/*"cmpxchg16b" w/ rex.w*/
    {INVALID, 0x0fc732, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fc733, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fc734, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fc735, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {MOD_EXT, 0x0fc736, "(group 9 mod ext 12)", xx, xx, xx, xx, xx, mrm, x, 12},
    {MOD_EXT, 0x0fc737, "(mod ext 13)", xx, xx, xx, xx, xx, mrm, x, 13},
  },
  /* group 10 is all ud2b and is not used by us since identical */
  /* group 11a (first byte c6) */
  { /* extensions[17] */
    {OP_mov_st, 0xc60020, "mov", Eb, xx, Ib, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xc60021, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60022, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60023, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60024, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60025, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60026, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#1314: this also sets eip */
    {OP_xabort, 0xf8c60067, "xabort", eax, xx, Ib, xx, xx, mrm, x, END_LIST},
  },
  /* group 11b (first byte c7) */
  { /* extensions[18] */
    /* PR 250397: be aware that mov_imm shares this tail end of mov_st templates */
    {OP_mov_st, 0xc70020, "mov", Ev, xx, Iz, xx, xx, mrm, x, tex[17][0]},
    {INVALID, 0xc70021, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70022, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70023, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70024, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70025, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70026, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_xbegin, 0xf8c70067, "xbegin", xx, xx, Jz, xx, xx, mrm, x, END_LIST},
  },
  /* group 12 (first bytes 0f 71): all assumed to have Ib */
  { /* extensions[19] */
    {INVALID, 0x0f7130, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7132, "(prefix ext 104)", xx, xx, xx, xx, xx, no, x, 104},
    {INVALID, 0x0f7133, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7134, "(prefix ext 105)", xx, xx, xx, xx, xx, no, x, 105},
    {INVALID, 0x0f7135, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7136, "(prefix ext 106)", xx, xx, xx, xx, xx, no, x, 106},
    {INVALID, 0x0f7137, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 13 (first bytes 0f 72): all assumed to have Ib */
  { /* extensions[20] */
    {INVALID, 0x0f7230, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7231, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7232, "(prefix ext 107)", xx, xx, xx, xx, xx, no, x, 107},
    {INVALID, 0x0f7233, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7234, "(prefix ext 108)", xx, xx, xx, xx, xx, no, x, 108},
    {INVALID, 0x0f7235, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7236, "(prefix ext 109)", xx, xx, xx, xx, xx, no, x, 109},
    {INVALID, 0x0f7237, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 14 (first bytes 0f 73): all assumed to have Ib */
  { /* extensions[21] */
    {INVALID, 0x0f7330, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7331, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7332, "(prefix ext 110)", xx, xx, xx, xx, xx, no, x, 110},
    {PREFIX_EXT, 0x0f7333, "(prefix ext 101)", xx, xx, xx, xx, xx, no, x, 101},
    {INVALID, 0x0f7334, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7335, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7336, "(prefix ext 111)", xx, xx, xx, xx, xx, no, x, 111},
    {PREFIX_EXT, 0x0f7337, "(prefix ext 102)", xx, xx, xx, xx, xx, no, x, 102},
  },
  /* group 15 (first bytes 0f ae) */
  { /* extensions[22] */
    /* Intel tables imply they may add opcodes in the mod=3 (non-mem) space in future */
    {MOD_EXT,    0x0fae30, "(group 15 mod ext 14)", xx, xx, xx, xx, xx, mrm, x, 14},
    {MOD_EXT,    0x0fae31, "(group 15 mod ext 15)", xx, xx, xx, xx, xx, mrm, x, 15},
    {MOD_EXT,    0x0fae32, "(group 15 mod ext 16)", xx, xx, xx, xx, xx, mrm, x, 16},
    {MOD_EXT,    0x0fae33, "(group 15 mod ext 17)", xx, xx, xx, xx, xx, mrm, x, 17},
    {REX_W_EXT,  0x0fae34, "(rex.w ext 2)", xx, xx, xx, xx, xx, mrm, x, 2},
    {MOD_EXT,    0x0fae35, "(group 15 mod ext 6)", xx, xx, xx, xx, xx, no, x, 6},
    {MOD_EXT,    0x0fae36, "(group 15 mod ext 7)", xx, xx, xx, xx, xx, no, x, 7},
    {MOD_EXT,    0x0fae37, "(group 15 mod ext 3)", xx, xx, xx, xx, xx, no, x, 3},
 },
  /* group 16 (first bytes 0f 18) */
  { /* extensions[23] */
    /* Intel tables imply they may add opcodes in the mod=3 (non-mem) space in future */
    {OP_prefetchnta, 0x0f1830, "prefetchnta", xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetcht0,  0x0f1831, "prefetcht0",  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetcht1,  0x0f1832, "prefetcht1",  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetcht2,  0x0f1833, "prefetcht2",  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1834, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1835, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1836, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1837, "nop", xx, xx, Ed, xx, xx, mrm, x, END_LIST},
 },
  /* group AMD (first bytes 0f 0d) */
  { /* extensions[24] */
    {OP_prefetch,  0x0f0d30, "prefetch",  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetchw, 0x0f0d31, "prefetchw", xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f0d32, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d33, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d34, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d35, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d36, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d37, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 1c* -- first opcode byte 82
   * see PR 235092 for the discrepancies in what 0x82 should be: empirically
   * and according to recent Intel manuals it matches 0x80, not 0x83 (as old
   * Intel manuals implied) or invalid (as gnu tools claim).
   * not linked into any encode chain.
   */
  { /* extensions[25]: all assumed to have Ib */
    {OP_add, 0x820020, "add", Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_or,  0x820021, "or",  Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_adc, 0x820022, "adc", Eb, xx, Ib, Eb, xx, mrm|i64, (fW6|fRC), END_LIST},
    {OP_sbb, 0x820023, "sbb", Eb, xx, Ib, Eb, xx, mrm|i64, (fW6|fRC), END_LIST},
    {OP_and, 0x820024, "and", Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_sub, 0x820025, "sub", Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_xor, 0x820026, "xor", Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_cmp, 0x820027, "cmp", xx, xx, Eb, Ib, xx, mrm|i64, fW6,  END_LIST},
  },
  /* group 1d (Intel now calling Group 1A) -- first opcode byte 8f */
  { /* extensions[26] */
    {OP_pop,  0x8f0020, "pop", Esv, xsp, xsp, i_xSP, xx, mrm, x, tfb[0x17]},
    /* we shouldn't ever get here for these, as this becomes an XOP prefix */
    {INVALID, 0x8f0021, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0022, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0023, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0024, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0025, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0026, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0027, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* XOP group 1 */
  { /* extensions[27] */
    {INVALID,     0x090138, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_blcfill,  0x090139, "blcfill", By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsfill,  0x09013a, "blsfill", By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blcs,     0x09013b, "blcs",    By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_tzmsk,    0x09013c, "tzmsk",   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blcic,    0x09013d, "blcic",   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsic,    0x09013e, "blsic",   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_t1mskc,   0x09013f, "t1mskc",  By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
  },
  /* XOP group 2 */
  { /* extensions[28] */
    {INVALID,     0x090238, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_blcmsk,   0x090239, "blcmsk",By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {INVALID,     0x09023a, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09023b, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09023c, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09023d, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_blci,     0x09023e, "blci",  By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {INVALID,     0x09023f, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* XOP group 3 */
  { /* extensions[29] */
    /* XXX i#1311: these instrs implicitly write to memory which we should
     * find a way to encode into the IR.
     */
    {OP_llwpcb,   0x091238, "llwpcb", xx, xx, Ry, xx, xx, mrm|vex, x, END_LIST},
    {OP_slwpcb,   0x091239, "slwpcb", Ry, xx, xx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0x09123a, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123b, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123c, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123d, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123e, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123f, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* XOP group 4: all assumed to have a 4-byte immediate by xop_a_extra[] */
  { /* extensions[30] */
    /* XXX i#1311: these instrs implicitly write to memory which we should
     * find a way to encode into the IR.
     */
    {OP_lwpins,   0x0a1238, "lwpins", xx, xx, By, Ed, Id, mrm|vex, fWC, END_LIST},
    {OP_lwpval,   0x0a1239, "lwpval", xx, xx, By, Ed, Id, mrm|vex, x, END_LIST},
    {INVALID,     0x0a123a, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123b, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123c, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123d, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123e, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123f, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 17 */
  { /* extensions[31] */
    {INVALID,     0x38f338, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_blsr,     0x38f339, "blsr",   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsmsk,   0x38f33a, "blsmsk", By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsi,     0x38f33b, "blsi",   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {INVALID,     0x38f33c, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x38f33d, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x38f33e, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x38f33f, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/****************************************************************************
 * Two-byte instructions that differ depending on presence of
 * prefixes, indexed in this order:
 *   none, 0xf3, 0x66, 0xf2
 * A second set is used for vex-encoded instructions, indexed in the
 * same order by prefix.
 *
 * N.B.: to avoid having a full entry here when there is only one
 * valid opcode prefix, use |reqp in the original entry instead of
 * pointing to this table.
 */
const instr_info_t prefix_extensions[][8] = {
  /* prefix extension 0 */
  {
    {OP_movups, 0x0f1010, "movups", Vps, xx, Wps, xx, xx, mrm, x, tpe[1][0]},
    {MOD_EXT,   0xf30f1010, "(mod ext 18)",  xx, xx, xx, xx, xx, mrm, x, 18},
    {OP_movupd, 0x660f1010, "movupd", Vpd, xx, Wpd, xx, xx, mrm, x, tpe[1][2]},
    {MOD_EXT,   0xf20f1010, "(mod ext 19)",  xx, xx, xx, xx, xx, mrm, x, 19},
    {OP_vmovups,   0x0f1010, "vmovups", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tpe[1][4]},
    {MOD_EXT,    0xf30f1010, "(mod ext 8)", xx, xx, xx, xx, xx, mrm|vex, x, 8},
    {OP_vmovupd, 0x660f1010, "vmovupd", Vvd, xx, Wvd, xx, xx, mrm|vex, x, tpe[1][6]},
    {MOD_EXT,    0xf20f1010, "(mod ext 9)", xx, xx, xx, xx, xx, mrm|vex, x, 9},
  },
  /* prefix extension 1 */
  {
    {OP_movups, 0x0f1110, "movups", Wps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {OP_movss,  0xf30f1110, "movss",  Wss, xx, Vss, xx, xx, mrm, x, END_LIST},
    {OP_movupd, 0x660f1110, "movupd", Wpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {OP_movsd,  0xf20f1110, "movsd",  Wsd, xx, Vsd, xx, xx, mrm, x, END_LIST},
    {OP_vmovups,   0x0f1110, "vmovups", Wvs, xx, Vvs, xx, xx, mrm|vex, x, END_LIST},
    {MOD_EXT,    0xf30f1110, "(mod ext 10)", xx, xx, xx, xx, xx, mrm|vex, x, 10},
    {OP_vmovupd, 0x660f1110, "vmovupd", Wvd, xx, Vvd, xx, xx, mrm|vex, x, END_LIST},
    {MOD_EXT,    0xf20f1110, "(mod ext 11)", xx, xx, xx, xx, xx, mrm|vex, x, 11},
  },
  /* prefix extension 2 */
  {
    /* i#319: note that the reg-reg form of the load version (0f12) is legal
     * and has a separate pneumonic ("movhlps"), yet the reg-reg form of
     * the store version (0f13) is illegal
     */
    {OP_movlps, 0x0f1210, "movlps", Vq_dq, xx, Wq_dq, xx, xx, mrm, x, tpe[3][0]}, /*"movhlps" if reg-reg */
    {OP_movsldup, 0xf30f1210, "movsldup", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_movlpd, 0x660f1210, "movlpd", Vq_dq, xx, Mq, xx, xx, mrm, x, tpe[3][2]},
    {OP_movddup, 0xf20f1210, "movddup", Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_vmovlps,    0x0f1210, "vmovlps", Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|vex|reqL0, x, tpe[3][4]}, /*"vmovhlps" if reg-reg */
    {OP_vmovsldup,0xf30f1210, "vmovsldup", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vmovlpd,  0x660f1210, "vmovlpd", Vq_dq, xx, Hq_dq, Mq, xx, mrm|vex, x, tpe[3][6]},
    {OP_vmovddup, 0xf20f1210, "vmovddup", Vvd, xx, Wvq_dq, xx, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 3 */
  {
    {OP_movlps, 0x0f1310, "movlps", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movlpd, 0x660f1310, "movlpd", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovlps, 0x0f1310, "vmovlps", Mq, xx, Vq_dq, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovlpd, 0x660f1310, "vmovlpd", Mq, xx, Vq_dq, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 4 */
  {
    {OP_unpcklps, 0x0f1410, "unpcklps", Vps, xx, Wq_dq, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_unpcklpd, 0x660f1410, "unpcklpd", Vpd, xx, Wq_dq, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpcklps, 0x0f1410, "vunpcklps", Vvs, xx, Hvs, Wvq_dq, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpcklpd, 0x660f1410, "vunpcklpd", Vvd, xx, Hvd, Wvq_dq, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 5 */
  {
    {OP_unpckhps, 0x0f1510, "unpckhps", Vps, xx, Wq_dq, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_unpckhpd, 0x660f1510, "unpckhpd", Vpd, xx, Wq_dq, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpckhps, 0x0f1510, "vunpckhps", Vvs, xx, Hvs, Wvq_dq, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpckhpd, 0x660f1510, "vunpckhpd", Vvd, xx, Hvd, Wvq_dq, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 6 */
  {
    /* i#319: note that the reg-reg form of the load version (0f16) is legal
     * and has a separate pneumonic ("movhlps"), yet the reg-reg form of
     * the store version (0f17) is illegal
     */
    {OP_movhps, 0x0f1610, "movhps", Vq_dq, xx, Wq_dq, xx, xx, mrm, x, tpe[7][0]}, /*"movlhps" if reg-reg */
    {OP_movshdup, 0xf30f1610, "movshdup", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_movhpd, 0x660f1610, "movhpd", Vq_dq, xx, Mq, xx, xx, mrm, x, tpe[7][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhps, 0x0f1610, "vmovhps", Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|vex|reqL0, x, tpe[7][4]}, /*"vmovlhps" if reg-reg */
    {OP_vmovshdup, 0xf30f1610, "vmovshdup", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vmovhpd, 0x660f1610, "vmovhpd", Vq_dq, xx, Hq_dq, Mq, xx, mrm|vex|reqL0, x, tpe[7][6]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 7 */
  {
    {OP_movhps, 0x0f1710, "movhps", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movhpd, 0x660f1710, "movhpd", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhps, 0x0f1710, "vmovhps", Mq, xx, Vq_dq, xx, xx, mrm|vex|reqL0, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhpd, 0x660f1710, "vmovhpd", Mq, xx, Vq_dq, xx, xx, mrm|vex|reqL0, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 8 */
  {
    {OP_movaps, 0x0f2810, "movaps", Vps, xx, Wps, xx, xx, mrm, x, tpe[9][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movapd, 0x660f2810, "movapd", Vpd, xx, Wpd, xx, xx, mrm, x, tpe[9][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovaps, 0x0f2810, "vmovaps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tpe[9][4]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovapd, 0x660f2810, "vmovapd", Vvd, xx, Wvd, xx, xx, mrm|vex, x, tpe[9][6]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 9 */
  {
    {OP_movaps, 0x0f2910, "movaps", Wps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movapd, 0x660f2910, "movapd", Wpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovaps, 0x0f2910, "vmovaps", Wvs, xx, Vvs, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovapd, 0x660f2910, "vmovapd", Wvd, xx, Vvd, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 10 */
  {
    {OP_cvtpi2ps,  0x0f2a10, "cvtpi2ps", Vq_dq, xx, Qq, xx, xx, mrm, x, END_LIST},
    {OP_cvtsi2ss, 0xf30f2a10, "cvtsi2ss", Vss, xx, Ed_q, xx, xx, mrm, x, END_LIST},
    {OP_cvtpi2pd, 0x660f2a10, "cvtpi2pd", Vpd, xx, Qq, xx, xx, mrm, x, END_LIST},
    {OP_cvtsi2sd, 0xf20f2a10, "cvtsi2sd", Vsd, xx, Ed_q, xx, xx, mrm, x, END_LIST},
    {INVALID,  0x0f2a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsi2ss, 0xf30f2a10, "vcvtsi2ss", Vss, xx, H12_dq, Ed_q, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f2a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsi2sd, 0xf20f2a10, "vcvtsi2sd", Vsd, xx, Hsd, Ed_q, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 11 */
  {
    {OP_movntps,   0x0f2b10, "movntps", Mps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {OP_movntss, 0xf30f2b10, "movntss", Mss, xx, Vss, xx, xx, mrm, x, END_LIST},
    {OP_movntpd, 0x660f2b10, "movntpd", Mpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {OP_movntsd, 0xf20f2b10, "movntsd", Msd, xx, Vsd, xx, xx, mrm, x, END_LIST},
    {OP_vmovntps,   0x0f2b10, "vmovntps", Mvs, xx, Vvs, xx, xx, mrm|vex, x, END_LIST},
    /* XXX: AMD doesn't list movntss in their new manual => assuming no vex version */
    {INVALID, 0xf30f2b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovntpd, 0x660f2b10, "vmovntpd", Mvd, xx, Vvd, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f2b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 12 */
  {
    {OP_cvttps2pi, 0x0f2c10, "cvttps2pi", Pq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvttss2si, 0xf30f2c10, "cvttss2si", Gd_q, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvttpd2pi, 0x660f2c10, "cvttpd2pi", Pq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvttsd2si, 0xf20f2c10, "cvttsd2si", Gd_q, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f2c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvttss2si, 0xf30f2c10, "vcvttss2si", Gd_q, xx, Wss, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f2c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvttsd2si, 0xf20f2c10, "vcvttsd2si", Gd_q, xx, Wsd, xx, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 13 */
  {
    {OP_cvtps2pi, 0x0f2d10, "cvtps2pi", Pq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvtss2si, 0xf30f2d10, "cvtss2si", Gd_q, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2pi, 0x660f2d10, "cvtpd2pi", Pq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtsd2si, 0xf20f2d10, "cvtsd2si", Gd_q, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f2d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtss2si, 0xf30f2d10, "vcvtss2si", Gd_q, xx, Wss, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f2d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsd2si, 0xf20f2d10, "vcvtsd2si", Gd_q, xx, Wsd, xx, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 14 */
  {
    {OP_ucomiss, 0x0f2e10, "ucomiss", xx, xx, Vss, Wss, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf30f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_ucomisd, 0x660f2e10, "ucomisd", xx, xx, Vsd, Wsd, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf20f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vucomiss, 0x0f2e10, "vucomiss", xx, xx, Vss, Wss, xx, mrm|vex, fW6, END_LIST},
    {INVALID, 0xf30f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vucomisd, 0x660f2e10, "vucomisd", xx, xx, Vsd, Wsd, xx, mrm|vex, fW6, END_LIST},
    {INVALID, 0xf20f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 15 */
  {
    {OP_comiss,  0x0f2f10, "comiss",  xx, xx, Vss, Wss, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf30f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_comisd,  0x660f2f10, "comisd",  xx, xx, Vsd, Wsd, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf20f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcomiss,  0x0f2f10, "vcomiss",  xx, xx, Vss, Wss, xx, mrm|vex, fW6, END_LIST},
    {INVALID, 0xf30f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vcomisd,  0x660f2f10, "vcomisd",  xx, xx, Vsd, Wsd, xx, mrm|vex, fW6, END_LIST},
    {INVALID, 0xf20f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 16 */
  {
    {OP_movmskps, 0x0f5010, "movmskps", Gr, xx, Ups, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_movmskpd, 0x660f5010, "movmskpd", Gr, xx, Upd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovmskps, 0x0f5010, "vmovmskps", Gr, xx, Uvs, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovmskpd, 0x660f5010, "vmovmskpd", Gr, xx, Uvd, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 17 */
  {
    {OP_sqrtps, 0x0f5110, "sqrtps", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_sqrtss, 0xf30f5110, "sqrtss", Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_sqrtpd, 0x660f5110, "sqrtpd", Vpd, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_sqrtsd, 0xf20f5110, "sqrtsd", Vsd, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {OP_vsqrtps, 0x0f5110, "vsqrtps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vsqrtss, 0xf30f5110, "vsqrtss", Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vsqrtpd, 0x660f5110, "vsqrtpd", Vvd, xx, Wvd, xx, xx, mrm|vex, x, END_LIST},
    {OP_vsqrtsd, 0xf20f5110, "vsqrtsd", Vdq, xx, Hsd, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 18 */
  {
    {OP_rsqrtps, 0x0f5210, "rsqrtps", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_rsqrtss, 0xf30f5210, "rsqrtss", Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x660f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrsqrtps, 0x0f5210, "vrsqrtps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vrsqrtss, 0xf30f5210, "vrsqrtss", Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 19 */
  {
    {OP_rcpps, 0x0f5310, "rcpps", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_rcpss, 0xf30f5310, "rcpss", Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x660f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrcpps, 0x0f5310, "vrcpps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vrcpss, 0xf30f5310, "vrcpss", Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 20 */
  {
    {OP_andps,  0x0f5410, "andps",  Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_andpd,  0x660f5410, "andpd",  Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vandps,  0x0f5410, "vandps",  Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vandpd,  0x660f5410, "vandpd",  Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 21 */
  {
    {OP_andnps, 0x0f5510, "andnps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_andnpd, 0x660f5510, "andnpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vandnps, 0x0f5510, "vandnps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vandnpd, 0x660f5510, "vandnpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 22 */
  {
    {OP_orps,   0x0f5610, "orps",   Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_orpd,   0x660f5610, "orpd",   Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorps,   0x0f5610, "vorps",   Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vorpd,   0x660f5610, "vorpd",   Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 23 */
  {
    {OP_xorps,  0x0f5710, "xorps",  Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_xorpd,  0x660f5710, "xorpd",  Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vxorps,  0x0f5710, "vxorps",  Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vxorpd,  0x660f5710, "vxorpd",  Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 24 */
  {
    {OP_addps, 0x0f5810, "addps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_addss, 0xf30f5810, "addss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_addpd, 0x660f5810, "addpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_addsd, 0xf20f5810, "addsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vaddps, 0x0f5810, "vaddps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {OP_vaddss, 0xf30f5810, "vaddss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vaddpd, 0x660f5810, "vaddpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vaddsd, 0xf20f5810, "vaddsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 25 */
  {
    {OP_mulps, 0x0f5910, "mulps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_mulss, 0xf30f5910, "mulss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_mulpd, 0x660f5910, "mulpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_mulsd, 0xf20f5910, "mulsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vmulps, 0x0f5910, "vmulps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {OP_vmulss, 0xf30f5910, "vmulss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vmulpd, 0x660f5910, "vmulpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vmulsd, 0xf20f5910, "vmulsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 26 */
  {
    {OP_cvtps2pd, 0x0f5a10, "cvtps2pd", Vpd, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvtss2sd, 0xf30f5a10, "cvtss2sd", Vsd, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2ps, 0x660f5a10, "cvtpd2ps", Vps, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtsd2ss, 0xf20f5a10, "cvtsd2ss", Vss, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {OP_vcvtps2pd, 0x0f5a10, "vcvtps2pd", Vvd, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vcvtss2sd, 0xf30f5a10, "vcvtss2sd", Vsd, xx, Hsd, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vcvtpd2ps, 0x660f5a10, "vcvtpd2ps", Vvs, xx, Wvd, xx, xx, mrm|vex, x, END_LIST},
    {OP_vcvtsd2ss, 0xf20f5a10, "vcvtsd2ss", Vss, xx, H12_dq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 27 */
  {
    {OP_cvtdq2ps, 0x0f5b10, "cvtdq2ps", Vps, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {OP_cvttps2dq, 0xf30f5b10, "cvttps2dq", Vdq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvtps2dq, 0x660f5b10, "cvtps2dq", Vdq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtdq2ps, 0x0f5b10, "vcvtdq2ps", Vvs, xx, Wx, xx, xx, mrm|vex, x, END_LIST},
    {OP_vcvttps2dq, 0xf30f5b10, "vcvttps2dq", Vx, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vcvtps2dq, 0x660f5b10, "vcvtps2dq", Vx, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 28 */
  {
    {OP_subps, 0x0f5c10, "subps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_subss, 0xf30f5c10, "subss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_subpd, 0x660f5c10, "subpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_subsd, 0xf20f5c10, "subsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vsubps, 0x0f5c10, "vsubps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {OP_vsubss, 0xf30f5c10, "vsubss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vsubpd, 0x660f5c10, "vsubpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vsubsd, 0xf20f5c10, "vsubsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 29 */
  {
    {OP_minps, 0x0f5d10, "minps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_minss, 0xf30f5d10, "minss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_minpd, 0x660f5d10, "minpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_minsd, 0xf20f5d10, "minsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vminps, 0x0f5d10, "vminps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {OP_vminss, 0xf30f5d10, "vminss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vminpd, 0x660f5d10, "vminpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vminsd, 0xf20f5d10, "vminsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 30 */
  {
    {OP_divps, 0x0f5e10, "divps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_divss, 0xf30f5e10, "divss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_divpd, 0x660f5e10, "divpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_divsd, 0xf20f5e10, "divsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vdivps, 0x0f5e10, "vdivps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {OP_vdivss, 0xf30f5e10, "vdivss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vdivpd, 0x660f5e10, "vdivpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vdivsd, 0xf20f5e10, "vdivsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 31 */
  {
    {OP_maxps, 0x0f5f10, "maxps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_maxss, 0xf30f5f10, "maxss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_maxpd, 0x660f5f10, "maxpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_maxsd, 0xf20f5f10, "maxsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vmaxps, 0x0f5f10, "vmaxps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {OP_vmaxss, 0xf30f5f10, "vmaxss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, END_LIST},
    {OP_vmaxpd, 0x660f5f10, "vmaxpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vmaxsd, 0xf20f5f10, "vmaxsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 32 */
  {
    {OP_punpcklbw,   0x0f6010, "punpcklbw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[32][2]},
    {INVALID,      0xf30f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklbw, 0x660f6010, "punpcklbw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6010,   "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklbw, 0x660f6010, "vpunpcklbw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf20f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 33 */
  {
    {OP_punpcklwd,   0x0f6110, "punpcklwd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[33][2]},
    {INVALID,      0xf30f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklwd, 0x660f6110, "punpcklwd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklwd, 0x660f6110, "vpunpcklwd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf20f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 34 */
  {
    {OP_punpckldq,   0x0f6210, "punpckldq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[34][2]},
    {INVALID,      0xf30f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckldq, 0x660f6210, "punpckldq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckldq, 0x660f6210, "vpunpckldq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf20f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 35 */
  {
    {OP_packsswb,   0x0f6310, "packsswb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[35][2]},
    {INVALID,     0xf30f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packsswb, 0x660f6310, "packsswb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpacksswb, 0x660f6310, "vpacksswb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf20f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 36 */
  {
    {OP_pcmpgtb,   0x0f6410, "pcmpgtb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[36][2]},
    {INVALID,    0xf30f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtb, 0x660f6410, "pcmpgtb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtb, 0x660f6410, "vpcmpgtb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 37 */
  {
    {OP_pcmpgtw,   0x0f6510, "pcmpgtw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[37][2]},
    {INVALID,    0xf30f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtw, 0x660f6510, "pcmpgtw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtw, 0x660f6510, "vpcmpgtw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 38 */
  {
    {OP_pcmpgtd,   0x0f6610, "pcmpgtd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[38][2]},
    {INVALID,    0xf30f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtd, 0x660f6610, "pcmpgtd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtd, 0x660f6610, "vpcmpgtd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 39 */
  {
    {OP_packuswb,   0x0f6710, "packuswb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[39][2]},
    {INVALID,     0xf30f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packuswb, 0x660f6710, "packuswb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpackuswb, 0x660f6710, "vpackuswb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf20f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 40 */
  {
    {OP_punpckhbw,   0x0f6810, "punpckhbw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[40][2]},
    {INVALID,      0xf30f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhbw, 0x660f6810, "punpckhbw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhbw, 0x660f6810, "vpunpckhbw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf20f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 41 */
  {
    {OP_punpckhwd,   0x0f6910, "punpckhwd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[41][2]},
    {INVALID,      0xf30f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhwd, 0x660f6910, "punpckhwd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhwd, 0x660f6910, "vpunpckhwd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf20f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 42 */
  {
    {OP_punpckhdq,   0x0f6a10, "punpckhdq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[42][2]},
    {INVALID,      0xf30f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhdq, 0x660f6a10, "punpckhdq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhdq, 0x660f6a10, "vpunpckhdq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf20f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 43 */
  {
    {OP_packssdw,   0x0f6b10, "packssdw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[43][2]},
    {INVALID,     0xf30f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packssdw, 0x660f6b10, "packssdw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpackssdw, 0x660f6b10, "vpackssdw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf20f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 44 */
  {
    {INVALID,         0x0f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklqdq, 0x660f6c10, "punpcklqdq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,       0xf20f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklqdq, 0x660f6c10, "vpunpcklqdq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,       0xf20f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 45 */
  {
    {INVALID,         0x0f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhqdq, 0x660f6d10, "punpckhqdq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,       0xf20f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhqdq, 0x660f6d10, "vpunpckhqdq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,       0xf20f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 46 */
  {
    /* movd zeroes the top bits when the destination is an mmx or xmm reg */
    {OP_movd,   0x0f6e10, "movd", Pq, xx, Ed_q, xx, xx, mrm, x, tpe[46][2]},
    {INVALID, 0xf30f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movd, 0x660f6e10, "movd", Vdq, xx, Ed_q, xx, xx, mrm, x, tpe[51][0]},
    {INVALID, 0xf20f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovd, 0x660f6e10, "vmovd", Vdq, xx, Ed_q, xx, xx, mrm|vex, x, tpe[51][6]},
    {INVALID, 0xf20f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 47: all assumed to have Ib */
  {
    {OP_pshufw,   0x0f7010, "pshufw",   Pq, xx, Qq, Ib, xx, mrm, x, END_LIST},
    {OP_pshufhw, 0xf30f7010, "pshufhw", Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {OP_pshufd,  0x660f7010, "pshufd",  Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {OP_pshuflw, 0xf20f7010, "pshuflw", Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {INVALID,       0x0f7010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpshufhw, 0xf30f7010, "vpshufhw", Vx, xx, Wx, Ib, xx, mrm|vex, x, END_LIST},
    {OP_vpshufd,  0x660f7010, "vpshufd",  Vx, xx, Wx, Ib, xx, mrm|vex, x, END_LIST},
    {OP_vpshuflw, 0xf20f7010, "vpshuflw", Vx, xx, Wx, Ib, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 48 */
  {
    {OP_pcmpeqb,   0x0f7410, "pcmpeqb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[48][2]},
    {INVALID,    0xf30f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqb, 0x660f7410, "pcmpeqb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqb, 0x660f7410, "vpcmpeqb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 49 */
  {
    {OP_pcmpeqw,   0x0f7510, "pcmpeqw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[49][2]},
    {INVALID,    0xf30f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqw, 0x660f7510, "pcmpeqw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqw, 0x660f7510, "vpcmpeqw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 50 */
  {
    {OP_pcmpeqd,   0x0f7610, "pcmpeqd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[50][2]},
    {INVALID,    0xf30f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqd, 0x660f7610, "pcmpeqd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqd, 0x660f7610, "vpcmpeqd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 51 */
  {
    {OP_movd,   0x0f7e10, "movd", Ed_q, xx, Pd_q, xx, xx, mrm, x, tpe[51][2]},
    /* movq zeroes the top bits when the destination is an mmx or xmm reg */
    {OP_movq, 0xf30f7e10, "movq", Vdq, xx, Wq_dq, xx, xx, mrm, x, tpe[61][2]},
    {OP_movd, 0x660f7e10, "movd", Ed_q, xx, Vd_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0xf30f7e10, "vmovq", Vdq, xx, Wq_dq, xx, xx, mrm|vex, x, tpe[61][6]},
    {OP_vmovd, 0x660f7e10, "vmovd", Ed_q, xx, Vd_dq, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 52: all assumed to have Ib */
  {
    {OP_cmpps, 0x0fc210, "cmpps", Vps, xx, Wps, Ib, Vps, mrm, x, END_LIST},
    {OP_cmpss, 0xf30fc210, "cmpss", Vss, xx, Wss, Ib, Vss, mrm, x, END_LIST},
    {OP_cmppd, 0x660fc210, "cmppd", Vpd, xx, Wpd, Ib, Vpd, mrm, x, END_LIST},
    {OP_cmpsd, 0xf20fc210, "cmpsd", Vsd, xx, Wsd, Ib, Vsd, mrm, x, END_LIST},
    {OP_vcmpps, 0x0fc210, "vcmpps", Vvs, xx, Hvs, Wvs, Ib, mrm|vex, x, END_LIST},
    {OP_vcmpss, 0xf30fc210, "vcmpss", Vdq, xx, Hdq, Wss, Ib, mrm|vex, x, END_LIST},
    {OP_vcmppd, 0x660fc210, "vcmppd", Vvd, xx, Hvd, Wvd, Ib, mrm|vex, x, END_LIST},
    {OP_vcmpsd, 0xf20fc210, "vcmpsd", Vdq, xx, Hdq, Wsd, Ib, mrm|vex, x, END_LIST},
  },
  /* prefix extension 53: all assumed to have Ib */
  { /* note that gnu tools print immed first: pinsrw $0x0,(%esp),%xmm0 */
    /* FIXME i#1388: pinsrw actually reads only bottom word of reg */
    {OP_pinsrw,   0x0fc410, "pinsrw", Pw_q, xx, Rd_Mw, Ib, xx, mrm, x, tpe[53][2]},
    {INVALID,   0xf30fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pinsrw, 0x660fc410, "pinsrw", Vw_dq, xx, Rd_Mw, Ib, xx, mrm, x, END_LIST},
    {INVALID,   0xf20fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0x0fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpinsrw, 0x660fc410, "vpinsrw", Vdq, xx, H14_dq, Rd_Mw, Ib, mrm|vex, x, END_LIST},
    {INVALID,   0xf20fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 54: all assumed to have Ib */
  { /* note that gnu tools print immed first: pextrw $0x7,%xmm7,%edx */
    {OP_pextrw,   0x0fc510, "pextrw", Gd, xx, Nw_q, Ib, xx, mrm, x, tpe[54][2]},
    {INVALID,   0xf30fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pextrw, 0x660fc510, "pextrw", Gd, xx, Uw_dq, Ib, xx, mrm, x, tvex[37][0]},
    {INVALID,   0xf20fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0x0fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpextrw, 0x660fc510, "vpextrw", Gd, xx, Uw_dq, Ib, xx, mrm|vex, x, tvex[37][1]},
    {INVALID,   0xf20fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 55: all assumed to have Ib */
  {
    {OP_shufps, 0x0fc610, "shufps", Vps, xx, Wps, Ib, Vps, mrm, x, END_LIST},
    {INVALID, 0xf30fc610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_shufpd, 0x660fc610, "shufpd", Vpd, xx, Wpd, Ib, Vpd, mrm, x, END_LIST},
    {INVALID, 0xf20fc610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshufps, 0x0fc610, "vshufps", Vvs, xx, Hvs, Wvs, Ib, mrm|vex, x, END_LIST},
    {INVALID, 0xf30fc610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vshufpd, 0x660fc610, "vshufpd", Vvd, xx, Hvd, Wvd, Ib, mrm|vex, x, END_LIST},
    {INVALID, 0xf20fc610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 56 */
  {
    {OP_psrlw,   0x0fd110, "psrlw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[56][2]},
    {INVALID,  0xf30fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrlw, 0x660fd110, "psrlw", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[104][0]},
    {INVALID,  0xf20fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrlw, 0x660fd110, "vpsrlw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[104][6]},
    {INVALID,  0xf20fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 57 */
  {
    {OP_psrld,   0x0fd210, "psrld", Pq, xx, Qq, Pq, xx, mrm, x, tpe[57][2]},
    {INVALID,  0xf30fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrld, 0x660fd210, "psrld", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[107][0]},
    {INVALID,  0xf20fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrld, 0x660fd210, "vpsrld", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[107][6]},
    {INVALID,  0xf20fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 58 */
  {
    {OP_psrlq,   0x0fd310, "psrlq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[58][2]},
    {INVALID,  0xf30fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrlq, 0x660fd310, "psrlq", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[110][0]},
    {INVALID,  0xf20fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrlq, 0x660fd310, "vpsrlq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[110][6]},
    {INVALID,  0xf20fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 59 */
  {
    {OP_paddq,   0x0fd410, "paddq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[59][2]},
    {INVALID,  0xf30fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_paddq, 0x660fd410, "paddq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,  0xf20fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddq, 0x660fd410, "vpaddq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,  0xf20fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 60 */
  {
    {OP_pmullw,   0x0fd510, "pmullw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[60][2]},
    {INVALID,   0xf30fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmullw, 0x660fd510, "pmullw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmullw, 0x660fd510, "vpmullw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 61 */
  {
    {INVALID,   0x0fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movq2dq, 0xf30fd610, "movq2dq", Vdq, xx, Nq, xx, xx, mrm, x, END_LIST},
    {OP_movq, 0x660fd610, "movq", Wq_dq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {OP_movdq2q, 0xf20fd610, "movdq2q", Pq, xx, Uq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID,   0x0fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0x660fd610, "vmovq", Wq_dq, xx, Vq_dq, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 62 */
  {
    {OP_pmovmskb,   0x0fd710, "pmovmskb", Gd, xx, Nq, xx, xx, mrm, x, tpe[62][2]},
    {INVALID,     0xf30fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmovmskb, 0x660fd710, "pmovmskb", Gd, xx, Udq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0xf20fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x0fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf30fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovmskb, 0x660fd710, "vpmovmskb", Gd, xx, Ux, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf20fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 63 */
  {
    {OP_psubusb,   0x0fd810, "psubusb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[63][2]},
    {INVALID,    0xf30fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubusb, 0x660fd810, "psubusb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubusb, 0x660fd810, "vpsubusb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 64 */
  {
    {OP_psubusw,   0x0fd910, "psubusw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[64][2]},
    {INVALID,    0xf30fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubusw, 0x660fd910, "psubusw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubusw, 0x660fd910, "vpsubusw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 65 */
  {
    {OP_pminub,   0x0fda10, "pminub", Pq, xx, Qq, Pq, xx, mrm, x, tpe[65][2]},
    {INVALID,    0xf30fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pminub, 0x660fda10, "pminub", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpminub, 0x660fda10, "vpminub", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 66 */
  {
    {OP_pand,   0x0fdb10, "pand", Pq, xx, Qq, Pq, xx, mrm, x, tpe[66][2]},
    {INVALID,    0xf30fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pand, 0x660fdb10, "pand", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpand, 0x660fdb10, "vpand", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 67 */
  {
    {OP_paddusb,   0x0fdc10, "paddusb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[67][2]},
    {INVALID,    0xf30fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddusb, 0x660fdc10, "paddusb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddusb, 0x660fdc10, "vpaddusb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 68 */
  {
    {OP_paddusw,   0x0fdd10, "paddusw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[68][2]},
    {INVALID,    0xf30fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddusw, 0x660fdd10, "paddusw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddusw, 0x660fdd10, "vpaddusw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 69 */
  {
    {OP_pmaxub,   0x0fde10, "pmaxub", Pq, xx, Qq, Pq, xx, mrm, x, tpe[69][2]},
    {INVALID,    0xf30fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaxub, 0x660fde10, "pmaxub", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaxub, 0x660fde10, "vpmaxub", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 70 */
  {
    {OP_pandn,   0x0fdf10, "pandn", Pq, xx, Qq, Pq, xx, mrm, x, tpe[70][2]},
    {INVALID,    0xf30fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pandn, 0x660fdf10, "pandn", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpandn, 0x660fdf10, "vpandn", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 71 */
  {
    {OP_pavgb,   0x0fe010, "pavgb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[71][2]},
    {INVALID,    0xf30fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pavgb, 0x660fe010, "pavgb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpavgb, 0x660fe010, "vpavgb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 72 */
  {
    {OP_psraw,   0x0fe110, "psraw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[72][2]},
    {INVALID,    0xf30fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psraw, 0x660fe110, "psraw", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[105][0]},
    {INVALID,    0xf20fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsraw, 0x660fe110, "vpsraw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[105][6]},
    {INVALID,    0xf20fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 73 */
  {
    {OP_psrad,   0x0fe210, "psrad", Pq, xx, Qq, Pq, xx, mrm, x, tpe[73][2]},
    {INVALID,    0xf30fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrad, 0x660fe210, "psrad", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[108][0]},
    {INVALID,    0xf20fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrad, 0x660fe210, "vpsrad", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[108][6]},
    {INVALID,    0xf20fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 74 */
  {
    {OP_pavgw,   0x0fe310, "pavgw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[74][2]},
    {INVALID,    0xf30fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pavgw, 0x660fe310, "pavgw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpavgw, 0x660fe310, "vpavgw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 75 */
  {
    {OP_pmulhuw,   0x0fe410, "pmulhuw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[75][2]},
    {INVALID,    0xf30fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmulhuw, 0x660fe410, "pmulhuw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmulhuw, 0x660fe410, "vpmulhuw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 76 */
  {
    {OP_pmulhw,   0x0fe510, "pmulhw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[76][2]},
    {INVALID,    0xf30fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmulhw, 0x660fe510, "pmulhw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmulhw, 0x660fe510, "vpmulhw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 77 */
  {
    {INVALID, 0x0fe610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_cvtdq2pd, 0xf30fe610, "cvtdq2pd",  Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvttpd2dq,0x660fe610, "cvttpd2dq", Vdq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2dq, 0xf20fe610, "cvtpd2dq",  Vdq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {INVALID,        0x0fe610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vcvtdq2pd, 0xf30fe610, "vcvtdq2pd",  Vvd, xx, Wvq_dq, xx, xx, mrm|vex, x, END_LIST},
    {OP_vcvttpd2dq,0x660fe610, "vcvttpd2dq", Vx, xx, Wvd, xx, xx, mrm|vex, x, END_LIST},
    {OP_vcvtpd2dq, 0xf20fe610, "vcvtpd2dq",  Vx, xx, Wvd, xx, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 78 */
  {
    {OP_movntq,    0x0fe710, "movntq",  Mq, xx, Pq, xx, xx, mrm, x, END_LIST},
    {INVALID,    0xf30fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_movntdq, 0x660fe710, "movntdq", Mdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovntdq, 0x660fe710, "vmovntdq", Mx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 79 */
  {
    {OP_psubsb,   0x0fe810, "psubsb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[79][2]},
    {INVALID,    0xf30fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubsb, 0x660fe810, "psubsb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubsb, 0x660fe810, "vpsubsb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 80 */
  {
    {OP_psubsw,   0x0fe910, "psubsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[80][2]},
    {INVALID,    0xf30fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubsw, 0x660fe910, "psubsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubsw, 0x660fe910, "vpsubsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 81 */
  {
    {OP_pminsw,   0x0fea10, "pminsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[81][2]},
    {INVALID,    0xf30fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pminsw, 0x660fea10, "pminsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpminsw, 0x660fea10, "vpminsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 82 */
  {
    {OP_por,   0x0feb10, "por", Pq, xx, Qq, Pq, xx, mrm, x, tpe[82][2]},
    {INVALID,    0xf30feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_por, 0x660feb10, "por", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpor, 0x660feb10, "vpor", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 83 */
  {
    {OP_paddsb,   0x0fec10, "paddsb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[83][2]},
    {INVALID,    0xf30fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddsb, 0x660fec10, "paddsb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddsb, 0x660fec10, "vpaddsb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 84 */
  {
    {OP_paddsw,   0x0fed10, "paddsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[84][2]},
    {INVALID,    0xf30fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddsw, 0x660fed10, "paddsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddsw, 0x660fed10, "vpaddsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 85 */
  {
    {OP_pmaxsw,   0x0fee10, "pmaxsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[85][2]},
    {INVALID,    0xf30fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaxsw, 0x660fee10, "pmaxsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaxsw, 0x660fee10, "vpmaxsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 86 */
  {
    {OP_pxor,   0x0fef10, "pxor", Pq, xx, Qq, Pq, xx, mrm, x, tpe[86][2]},
    {INVALID,    0xf30fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pxor, 0x660fef10, "pxor", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpxor, 0x660fef10, "vpxor", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 87 */
  {
    {OP_psllw,   0x0ff110, "psllw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[87][2]},
    {INVALID,    0xf30ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psllw, 0x660ff110, "psllw", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[106][0]},
    {INVALID,    0xf20ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsllw, 0x660ff110, "vpsllw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[106][6]},
    {INVALID,    0xf20ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 88 */
  {
    {OP_pslld,   0x0ff210, "pslld", Pq, xx, Qq, Pq, xx, mrm, x, tpe[88][2]},
    {INVALID,    0xf30ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pslld, 0x660ff210, "pslld", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[109][0]},
    {INVALID,    0xf20ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpslld, 0x660ff210, "vpslld", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[109][6]},
    {INVALID,    0xf20ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 89 */
  {
    {OP_psllq,   0x0ff310, "psllq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[89][2]},
    {INVALID,    0xf30ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psllq, 0x660ff310, "psllq", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[111][0]},
    {INVALID,    0xf20ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsllq, 0x660ff310, "vpsllq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[111][6]},
    {INVALID,    0xf20ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 90 */
  {
    {OP_pmuludq,   0x0ff410, "pmuludq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[90][2]},
    {INVALID,    0xf30ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmuludq, 0x660ff410, "pmuludq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmuludq, 0x660ff410, "vpmuludq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 91 */
  {
    {OP_pmaddwd,   0x0ff510, "pmaddwd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[91][2]},
    {INVALID,    0xf30ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaddwd, 0x660ff510, "pmaddwd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaddwd, 0x660ff510, "vpmaddwd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 92 */
  {
    {OP_psadbw,   0x0ff610, "psadbw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[92][2]},
    {INVALID,    0xf30ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psadbw, 0x660ff610, "psadbw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsadbw, 0x660ff610, "vpsadbw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 93 */
  {
    {OP_maskmovq,     0x0ff710, "maskmovq", Bq, xx, Pq, Nq, xx, mrm|predcx, x, END_LIST}, /* Intel table says "Ppi, Qpi" */
    {INVALID,       0xf30ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_maskmovdqu, 0x660ff710, "maskmovdqu", Bdq, xx, Vdq, Udq, xx, mrm|predcx, x, END_LIST},
    {INVALID,       0xf20ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmaskmovdqu, 0x660ff710, "vmaskmovdqu", Bdq, xx, Vdq, Udq, xx, mrm|vex|reqL0|predcx, x, END_LIST},
    {INVALID,       0xf20ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 94 */
  {
    {OP_psubb,   0x0ff810, "psubb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[94][2]},
    {INVALID,    0xf30ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubb, 0x660ff810, "psubb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubb, 0x660ff810, "vpsubb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 95 */
  {
    {OP_psubw,   0x0ff910, "psubw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[95][2]},
    {INVALID,    0xf30ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubw, 0x660ff910, "psubw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubw, 0x660ff910, "vpsubw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 96 */
  {
    {OP_psubd,   0x0ffa10, "psubd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[96][2]},
    {INVALID,    0xf30ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubd, 0x660ffa10, "psubd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubd, 0x660ffa10, "vpsubd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 97 */
  {
    {OP_psubq,   0x0ffb10, "psubq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[97][2]},
    {INVALID,  0xf30ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psubq, 0x660ffb10, "psubq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,  0xf20ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubq, 0x660ffb10, "vpsubq", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,  0xf20ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 98 */
  {
    {OP_paddb,   0x0ffc10, "paddb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[98][2]},
    {INVALID,    0xf30ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddb, 0x660ffc10, "paddb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddb, 0x660ffc10, "vpaddb", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 99 */
  {
    {OP_paddw,   0x0ffd10, "paddw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[99][2]},
    {INVALID,    0xf30ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddw, 0x660ffd10, "paddw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddw, 0x660ffd10, "vpaddw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 100 */
  {
    {OP_paddd,   0x0ffe10, "paddd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[100][2]},
    {INVALID,    0xf30ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddd, 0x660ffe10, "paddd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddd, 0x660ffe10, "vpaddd", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
  },
  /* prefix extension 101: all assumed to have Ib */
  {
    {INVALID,     0x0f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrldq, 0x660f7333, "psrldq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrldq, 0x660f7333, "vpsrldq", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 102: all assumed to have Ib */
  {
    {INVALID,     0x0f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pslldq, 0x660f7337, "pslldq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslldq, 0x660f7337, "vpslldq", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 103 */
  {
    {REX_B_EXT,  0x900000, "(rex.b ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_pause,0xf3900000, "pause", xx, xx, xx, xx, xx, no, x, END_LIST},
    /* we chain these even though encoding won't find them */
    {OP_nop, 0x66900000, "nop", xx, xx, xx, xx, xx, no, x, tpe[103][3]},
    /* windbg displays as "repne nop" */
    {OP_nop, 0xf2900000, "nop", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf3900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x66900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf2900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 104: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrlw,    0x0f7132, "psrlw", Nq, xx, Ib, Nq, xx, mrm, x, tpe[104][2]},
    {INVALID,   0xf30f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrlw,  0x660f7132, "psrlw", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw,  0x660f7132, "vpsrlw", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 105: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psraw,    0x0f7134, "psraw", Nq, xx, Ib, Nq, xx, mrm, x, tpe[105][2]},
    {INVALID,   0xf30f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psraw,  0x660f7134, "psraw", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw,  0x660f7134, "vpsraw", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 106: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psllw,    0x0f7136, "psllw", Nq, xx, Ib, Nq, xx, mrm, x, tpe[106][2]},
    {INVALID,   0xf30f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psllw,  0x660f7136, "psllw", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw,  0x660f7136, "vpsllw", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 107: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrld,    0x0f7232, "psrld", Nq, xx, Ib, Nq, xx, mrm, x, tpe[107][2]},
    {INVALID,   0xf30f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrld,  0x660f7232, "psrld", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrld,  0x660f7232, "vpsrld", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 108: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrad,    0x0f7234, "psrad", Nq, xx, Ib, Nq, xx, mrm, x, tpe[108][2]},
    {INVALID,   0xf30f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrad,  0x660f7234, "psrad", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrad,  0x660f7234, "vpsrad", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 109: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_pslld,    0x0f7236, "pslld", Nq, xx, Ib, Nq, xx, mrm, x, tpe[109][2]},
    {INVALID,   0xf30f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pslld,  0x660f7236, "pslld", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslld,  0x660f7236, "vpslld", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 110: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrlq,    0x0f7332, "psrlq", Nq, xx, Ib, Nq, xx, mrm, x, tpe[110][2]},
    {INVALID,   0xf30f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrlq,  0x660f7332, "psrlq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlq,  0x660f7332, "vpsrlq", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 111: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psllq,    0x0f7336, "psllq", Nq, xx, Ib, Nq, xx, mrm, x, tpe[111][2]},
    {INVALID,   0xf30f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psllq,  0x660f7336, "psllq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq,  0x660f7336, "vpsllq", Hx, xx, Ib, Ux, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 112 */
  {
    {OP_movq,     0x0f6f10, "movq", Pq, xx, Qq, xx, xx, mrm, x, tpe[113][0]},
    {OP_movdqu, 0xf30f6f10, "movdqu", Vdq, xx, Wdq, xx, xx, mrm, x, tpe[113][1]},
    {OP_movdqa, 0x660f6f10, "movdqa", Vdq, xx, Wdq, xx, xx, mrm, x, tpe[113][2]},
    {INVALID,   0xf20f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovdqu, 0xf30f6f10, "vmovdqu", Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[113][5]},
    {OP_vmovdqa, 0x660f6f10, "vmovdqa", Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[113][6]},
    {INVALID,   0xf20f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 113 */
  {
    {OP_movq,     0x0f7f10, "movq", Qq, xx, Pq, xx, xx, mrm, x, tpe[51][1]},
    {OP_movdqu, 0xf30f7f10, "movdqu", Wdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {OP_movdqa, 0x660f7f10, "movdqa", Wdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovdqu, 0xf30f7f10, "vmovdqu", Wx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {OP_vmovdqa, 0x660f7f10, "vmovdqa", Wx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  /* prefix extension 114 */
  {
    {INVALID,     0x0f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_haddpd, 0x660f7c10, "haddpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_haddps, 0xf20f7c10, "haddps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vhaddpd, 0x660f7c10, "vhaddpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vhaddps, 0xf20f7c10, "vhaddps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 115 */
  {
    {INVALID,     0x0f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_hsubpd, 0x660f7d10, "hsubpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_hsubps, 0xf20f7d10, "hsubps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vhsubpd, 0x660f7d10, "vhsubpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vhsubps, 0xf20f7d10, "vhsubps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 116 */
  {
    {INVALID,     0x0fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_addsubpd, 0x660fd010, "addsubpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_addsubps, 0xf20fd010, "addsubps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vaddsubpd, 0x660fd010, "vaddsubpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vaddsubps, 0xf20fd010, "vaddsubps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
  },
  /* prefix extension 117 */
  {
    {INVALID,     0x0ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x660ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_lddqu,  0xf20ff010, "lddqu", Vdq, xx, Mdq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0x0ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x660ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vlddqu,  0xf20ff010, "vlddqu", Vx, xx, Mx, xx, xx, mrm|vex, x, END_LIST},
  },
  /***************************************************
   * SSSE3
   */
  { /* prefix extension 118 */
    {OP_pshufb,     0x380018, "pshufb",   Pq, xx, Qq, xx, xx, mrm, x, tpe[118][2]},
    {INVALID,     0xf3380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_pshufb,   0x66380018, "pshufb",   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0xf2380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x380018, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufb,   0x66380018, "vpshufb",   Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf2380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 119 */
    {OP_phaddw,      0x380118, "phaddw",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[119][2]},
    {INVALID,      0xf3380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddw,    0x66380118, "phaddw",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380118, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddw,    0x66380118, "vphaddw",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 120 */
    {OP_phaddd,      0x380218, "phaddd",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[120][2]},
    {INVALID,      0xf3380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddd,    0x66380218, "phaddd",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380218, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddd,    0x66380218, "vphaddd",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 121 */
    {OP_phaddsw,     0x380318, "phaddsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[121][2]},
    {INVALID,      0xf3380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddsw,   0x66380318, "phaddsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380318, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddsw,   0x66380318, "vphaddsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 122 */
    {OP_pmaddubsw,   0x380418, "pmaddubsw",Pq, xx, Qq, Pq, xx, mrm, x, tpe[122][2]},
    {INVALID,      0xf3380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmaddubsw, 0x66380418, "pmaddubsw",Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380418, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddubsw, 0x66380418, "vpmaddubsw",Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 123 */
    {OP_phsubw,      0x380518, "phsubw",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[123][2]},
    {INVALID,      0xf3380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubw,    0x66380518, "phsubw",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380518, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubw,    0x66380518, "vphsubw",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 124 */
    {OP_phsubd,      0x380618, "phsubd",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[124][2]},
    {INVALID,      0xf3380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubd,    0x66380618, "phsubd",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380618, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubd,    0x66380618, "vphsubd",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 125 */
    {OP_phsubsw,     0x380718, "phsubsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[125][2]},
    {INVALID,      0xf3380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubsw,   0x66380718, "phsubsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380718, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubsw,   0x66380718, "vphsubsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 126 */
    {OP_psignb,      0x380818, "psignb",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[126][2]},
    {INVALID,      0xf3380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignb,    0x66380818, "psignb",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380818, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignb,    0x66380818, "vpsignb",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 127 */
    {OP_psignw,      0x380918, "psignw",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[127][2]},
    {INVALID,      0xf3380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignw,    0x66380918, "psignw",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380918, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignw,    0x66380918, "vpsignw",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 128 */
    {OP_psignd,      0x380a18, "psignd",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[128][2]},
    {INVALID,      0xf3380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignd,    0x66380a18, "psignd",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380a18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignd,    0x66380a18, "vpsignd",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 129 */
    {OP_pmulhrsw,    0x380b18, "pmulhrsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[129][2]},
    {INVALID,      0xf3380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmulhrsw,  0x66380b18, "pmulhrsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380b18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhrsw,  0x66380b18, "vpmulhrsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 130 */
    {OP_pabsb,       0x381c18, "pabsb",   Pq, xx, Qq, Pq, xx, mrm, x, tpe[130][2]},
    {INVALID,      0xf3381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsb,     0x66381c18, "pabsb",   Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381c18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsb,     0x66381c18, "vpabsb",   Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 131 */
    {OP_pabsw,       0x381d18, "pabsw",   Pq, xx, Qq, Pq, xx, mrm, x, tpe[131][2]},
    {INVALID,      0xf3381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsw,     0x66381d18, "pabsw",   Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381d18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsw,     0x66381d18, "vpabsw",   Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 132 */
    {OP_pabsd,       0x381e18, "pabsd",   Pq, xx, Qq, Pq, xx, mrm, x, tpe[132][2]},
    {INVALID,      0xf3381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsd,     0x66381e18, "pabsd",   Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381e18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsd,     0x66381e18, "vpabsd",   Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 133: all assumed to have Ib */
    {OP_palignr,     0x3a0f18, "palignr", Pq, xx, Qq, Ib, Pq, mrm, x, tpe[133][2]},
    {INVALID,      0xf33a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_palignr,   0x663a0f18, "palignr", Vdq, xx, Wdq, Ib, Vdq, mrm, x, END_LIST},
    {INVALID,      0xf23a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x3a0f18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf33a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpalignr,   0x663a0f18, "vpalignr", Vx, xx, Hx, Wx, Ib, mrm|vex, x, END_LIST},
    {INVALID,      0xf23a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 134 */
    {OP_vmread,      0x0f7810, "vmread",  Ed_q, xx, Gd_q, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf30f7810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* FIXME PR 338279: this is listed as /0 but I'm not going to chain it into
     * the reg extensions table until I can verify, since gdb thinks it
     * does NOT need /0.  Waiting for a processor that actually supports it.
     * It's ok for DR proper to think a non-cti instr is valid when really it's not,
     * though for our decoding library use we should get it right.
     */
    {OP_extrq,     0x660f7810, "extrq",   Udq, xx, Ib, Ib, xx, mrm, x, tpe[135][2]},
    /* FIXME: is src or dst Udq? */
    {OP_insertq,   0xf20f7810, "insertq", Vdq, xx, Udq, Ib, Ib, mrm, x, tpe[135][3]},
    {INVALID,        0x0f7810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 135 */
    {OP_vmwrite,     0x0f7910, "vmwrite", Gd_q, xx, Ed_q, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf30f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* FIXME: is src or dst Udq? */
    {OP_extrq,     0x660f7910, "extrq",   Vdq, xx, Udq, xx, xx, mrm, x, END_LIST},
    {OP_insertq,   0xf20f7910, "insertq", Vdq, xx, Udq, xx, xx, mrm, x, END_LIST},
    {INVALID,        0x0f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 136 */
    {OP_bsr,         0x0fbd10, "bsr",     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, END_LIST},
    /* XXX: if cpuid doesn't show lzcnt support, this is treated as bsr */
    {OP_lzcnt,     0xf30fbd10, "lzcnt",   Gv, xx, Ev, xx, xx, mrm, fW6, END_LIST},
    /* This is bsr w/ DATA_PREFIX, which we indicate by omitting 0x66 (i#1118).
     * It's not in the encoding chain.  Ditto for 0xf2.  If we keep the "all
     * prefix ext marked invalid are really treated valid" we don't need these,
     * but better to be explicit where we have to so we can easily remove that.
     */
    {OP_bsr,         0x0fbd10, "bsr",     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {OP_bsr,         0x0fbd10, "bsr",     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {INVALID,        0x0fbd10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fbd10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fbd10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fbd10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 137 */
    {OP_vmptrld,     0x0fc736, "vmptrld", xx, xx, Mq, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmxon,     0xf30fc736, "vmxon",   xx, xx, Mq, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmclear,   0x660fc736, "vmclear", Mq, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf20fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 138 */
    {OP_movbe,   0x38f018, "movbe", Gv, xx, Mv, xx, xx, mrm, x, tpe[139][0]},
    {INVALID,  0xf338f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* really this is regular data-size prefix */
    {OP_movbe, 0x6638f018, "movbe", Gw, xx, Mw, xx, xx, mrm, x, tpe[139][2]},
    {OP_crc32, 0xf238f018, "crc32", Gv, xx, Eb, Gv, xx, mrm, x, END_LIST},
    {INVALID,    0x38f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf338f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0x6638f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf238f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 139 */
    {OP_movbe,   0x38f118, "movbe", Mv, xx, Gv, xx, xx, mrm, x, tpe[138][2]},
    {INVALID,  0xf338f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* really this is regular data-size prefix */
    {OP_movbe, 0x6638f118, "movbe", Mw, xx, Gw, xx, xx, mrm, x, END_LIST},
    {OP_crc32, 0xf238f118, "crc32", Gv, xx, Ev, Gv, xx, mrm, x, tpe[138][3]},
    {INVALID,    0x38f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf338f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0x6638f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf238f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX: Intel Vol2B Sep2010 decode table claims crc32 has Gd
     * instead of Gv, and that f2 f1 has Ey instead of Ev, and that
     * there is a separate instruction with both 66 and f2 prefixes!
     * But detail page doesn't corroborate that...
     */
  },
  { /* prefix extension 140 */
    {OP_bsf,         0x0fbc10, "bsf",     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, END_LIST},
    /* XXX: if cpuid doesn't show tzcnt support, this is treated as bsf */
    {OP_tzcnt,     0xf30fbc10, "tzcnt",   Gv, xx, Ev, xx, xx, mrm, fW6, END_LIST},
    /* see OP_bsr comments above -- this is the same but for bsf: */
    {OP_bsf,         0x0fbc10, "bsf",     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {OP_bsf,         0x0fbc10, "bsf",     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {INVALID,        0x0fbc10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fbc10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fbc10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fbc10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* prefix extension 141 */
    {INVALID,        0x38f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_bextr,       0x38f718, "bextr",   Gy, xx, Ey, By, xx, mrm|vex, fW6, txop[60]},
    {OP_sarx,      0xf338f718, "sarx",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_shlx,      0x6638f718, "shlx",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_shrx,      0xf238f718, "shrx",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
  },
  { /* prefix extension 142 */
    {INVALID,        0x38f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_bzhi,        0x38f518, "bzhi",    Gy, xx, Ey, By, xx, mrm|vex, fW6, END_LIST},
    {INVALID,      0xf338f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pext,      0x6638f518, "pext",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_pdep,      0xf238f518, "pdep",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
  },
  { /* prefix extension 143 */
    {INVALID,        0x38f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x38f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_mulx,      0xf238f618, "mulx",    By, Gy, Ey, uDX, xx, mrm|vex, x, END_LIST},
  },
};

/****************************************************************************
 * Instructions that differ based on whether vex-encoded or not.
 * Most of these require an 0x66 prefix but we use reqp for that
 * so there's nothing inherent here about prefixes.
 */
const instr_info_t vex_extensions[][2] = {
  {    /* vex ext  0 */
    {INVALID,   0x663a4a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vblendvps,0x663a4a18, "vblendvps", Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  1 */
    {INVALID,   0x663a4b18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vblendvpd,0x663a4b18, "vblendvpd", Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  2 */
    {INVALID,   0x663a4c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpblendvb, 0x663a4c18, "vpblendvb", Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  3 */
    {OP_ptest,    0x66381718, "ptest",    xx, xx,  Vdq,Wdq, xx, mrm|reqp, fW6, END_LIST},
    {OP_vptest,   0x66381718, "vptest",    xx, xx,  Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
  }, { /* vex ext  4 */
    {OP_pmovsxbw, 0x66382018, "pmovsxbw", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbw,0x66382018, "vpmovsxbw", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  5 */
    {OP_pmovsxbd, 0x66382118, "pmovsxbd", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbd,0x66382118, "vpmovsxbd", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  6 */
    {OP_pmovsxbq, 0x66382218, "pmovsxbq", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbq,0x66382218, "vpmovsxbq", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  7 */
    {OP_pmovsxwd, 0x66382318, "pmovsxwd", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxwd,0x66382318, "vpmovsxwd", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  8 */
    {OP_pmovsxwq, 0x66382418, "pmovsxwq", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxwq,0x66382418, "vpmovsxwq", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext  9 */
    {OP_pmovsxdq, 0x66382518, "pmovsxdq", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxdq,0x66382518, "vpmovsxdq", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 10 */
    {OP_pmuldq,   0x66382818, "pmuldq",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmuldq,  0x66382818, "vpmuldq",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 11 */
    {OP_pcmpeqq,  0x66382918, "pcmpeqq",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpcmpeqq, 0x66382918, "vpcmpeqq",  Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 12 */
    {OP_movntdqa, 0x66382a18, "movntdqa", Mdq, xx, Vdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vmovntdqa,0x66382a18, "vmovntdqa", Mx, xx, Vx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 13 */
    {OP_packusdw, 0x66382b18, "packusdw", Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpackusdw,0x66382b18, "vpackusdw", Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 14 */
    {OP_pmovzxbw, 0x66383018, "pmovzxbw", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbw,0x66383018, "vpmovzxbw", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 15 */
    {OP_pmovzxbd, 0x66383118, "pmovzxbd", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbd,0x66383118, "vpmovzxbd", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 16 */
    {OP_pmovzxbq, 0x66383218, "pmovzxbq", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbq,0x66383218, "vpmovzxbq", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 17 */
    {OP_pmovzxwd, 0x66383318, "pmovzxwd", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxwd,0x66383318, "vpmovzxwd", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 18 */
    {OP_pmovzxwq, 0x66383418, "pmovzxwq", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxwq,0x66383418, "vpmovzxwq", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 19 */
    {OP_pmovzxdq, 0x66383518, "pmovzxdq", Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxdq,0x66383518, "vpmovzxdq", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 20 */
    {OP_pcmpgtq,  0x66383718, "pcmpgtq",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpcmpgtq, 0x66383718, "vpcmpgtq",  Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 21 */
    {OP_pminsb,   0x66383818, "pminsb",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminsb,  0x66383818, "vpminsb",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 22 */
    {OP_pminsd,   0x66383918, "pminsd",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminsd,  0x66383918, "vpminsd",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 23 */
    {OP_pminuw,   0x66383a18, "pminuw",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminuw,  0x66383a18, "vpminuw",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 24 */
    {OP_pminud,   0x66383b18, "pminud",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminud,  0x66383b18, "vpminud",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 25 */
    {OP_pmaxsb,   0x66383c18, "pmaxsb",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxsb,  0x66383c18, "vpmaxsb",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 26 */
    {OP_pmaxsd,   0x66383d18, "pmaxsd",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxsd,  0x66383d18, "vpmaxsd",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 27 */
    {OP_pmaxuw,   0x66383e18, "pmaxuw",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxuw,  0x66383e18, "vpmaxuw",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 28 */
    {OP_pmaxud,   0x66383f18, "pmaxud",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxud,  0x66383f18, "vpmaxud",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 29 */
    {OP_pmulld,   0x66384018, "pmulld",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmulld,  0x66384018, "vpmulld",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 30 */
    {OP_phminposuw, 0x66384118,"phminposuw",Vdq,xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vphminposuw,0x66384118,"vphminposuw",Vdq,xx, Wdq, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 31 */
    {OP_aesimc,  0x6638db18, "aesimc",  Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vaesimc, 0x6638db18, "vaesimc",  Vdq, xx, Wdq, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 32 */
    {OP_aesenc,  0x6638dc18, "aesenc",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vaesenc, 0x6638dc18, "vaesenc",  Vdq, xx, Hdq,Wdq, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 33 */
    {OP_aesenclast, 0x6638dd18,"aesenclast",Vdq,xx,Wdq,Vdq,xx, mrm|reqp, x, END_LIST},
    {OP_vaesenclast,0x6638dd18,"vaesenclast",Vdq,xx,Hdq,Wdq,xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 34 */
    {OP_aesdec,  0x6638de18, "aesdec",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vaesdec, 0x6638de18, "vaesdec",  Vdq, xx, Hdq,Wdq, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 35 */
    {OP_aesdeclast, 0x6638df18,"aesdeclast",Vdq,xx,Wdq,Vdq,xx, mrm|reqp, x, END_LIST},
    {OP_vaesdeclast,0x6638df18,"vaesdeclast",Vdq,xx,Hdq,Wdq,xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 36 */
    {OP_pextrb,   0x663a1418, "pextrb", Rd_Mb, xx, Vb_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpextrb,  0x663a1418, "vpextrb", Rd_Mb, xx, Vb_dq, Ib, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 37 */
    {OP_pextrw,   0x663a1518, "pextrw", Rd_Mw, xx, Vw_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpextrw,  0x663a1518, "vpextrw", Rd_Mw, xx, Vw_dq, Ib, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 38 */
    {OP_pextrd,   0x663a1618, "pextrd",  Ed_q, xx, Vd_q_dq, Ib, xx, mrm|reqp, x, END_LIST},/*"pextrq" with rex.w*/
    {OP_vpextrd,  0x663a1618, "vpextrd",  Ed_q, xx, Vd_q_dq, Ib, xx, mrm|vex|reqp, x, END_LIST},/*"vpextrq" with rex.w*/
  }, { /* vex ext 39 */
    {OP_extractps, 0x663a1718, "extractps", Ed, xx, Vd_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vextractps,0x663a1718, "vextractps", Ed, xx, Vd_dq, Ib, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 40 */
    {OP_roundps,  0x663a0818, "roundps",  Vdq, xx, Wdq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundps, 0x663a0818, "vroundps",  Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 41 */
    {OP_roundpd,  0x663a0918, "roundpd",  Vdq, xx, Wdq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundpd, 0x663a0918, "vroundpd",  Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 42 */
    {OP_roundss,  0x663a0a18, "roundss",  Vss, xx, Wss, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundss, 0x663a0a18, "vroundss",  Vdq, xx, H12_dq, Wss, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 43 */
    {OP_roundsd,  0x663a0b18, "roundsd",  Vsd, xx, Wsd, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundsd, 0x663a0b18, "vroundsd",  Vdq, xx, Hsd, Wsd, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 44 */
    {OP_blendps,  0x663a0c18, "blendps",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vblendps, 0x663a0c18, "vblendps",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 45 */
    {OP_blendpd,  0x663a0d18, "blendpd",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vblendpd, 0x663a0d18, "vblendpd",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 46 */
    {OP_pblendw,  0x663a0e18, "pblendw",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vpblendw, 0x663a0e18, "vpblendw",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 47 */
    /* FIXME i#1388: pinsrb actually reads only bottom byte of reg */
    {OP_pinsrb,   0x663a2018, "pinsrb",   Vb_dq, xx, Rd_Mb,  Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpinsrb,  0x663a2018, "vpinsrb",   Vdq, xx, H15_dq, Rd_Mb, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 48 */
    {OP_insertps, 0x663a2118, "insertps", Vdq,xx,Udq_Md,Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vinsertps,0x663a2118, "vinsertps", Vdq,xx,Hdq,Udq_Md,Ib, mrm|vex|reqp|reqL0, x, END_LIST},
  }, { /* vex ext 49 */
    {OP_pinsrd,   0x663a2218, "pinsrd",   Vd_q_dq, xx, Ed_q,Ib, xx, mrm|reqp, x, END_LIST},/*"pinsrq" with rex.w*/
    {OP_vpinsrd,  0x663a2218, "vpinsrd",   Vdq, xx, H12_8_dq, Ed_q, Ib, mrm|vex|reqp, x, END_LIST},/*"vpinsrq" with rex.w*/
  }, { /* vex ext 50 */
    {OP_dpps,     0x663a4018, "dpps",     Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vdpps,    0x663a4018, "vdpps",     Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 51 */
    {OP_dppd,     0x663a4118, "dppd",     Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vdppd,    0x663a4118, "vdppd",     Vdq, xx, Hdq, Wdq, Ib, mrm|vex|reqp|reqL0, x, END_LIST},
  }, { /* vex ext 52 */
    {OP_mpsadbw,  0x663a4218, "mpsadbw",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vmpsadbw, 0x663a4218, "vmpsadbw",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 53 */
    {OP_pcmpestrm, 0x663a6018, "pcmpestrm",xmm0, xx, Vdq, Wdq, Ib, mrm|reqp|xop, fW6, exop[8]},
    {OP_vpcmpestrm,0x663a6018, "vpcmpestrm",xmm0, xx, Vdq, Wdq, Ib, mrm|vex|reqp|xop, fW6, exop[11]},
  }, { /* vex ext 54 */
    {OP_pcmpestri, 0x663a6118, "pcmpestri",ecx, xx, Vdq, Wdq, Ib, mrm|reqp|xop, fW6, exop[9]},
    {OP_vpcmpestri,0x663a6118, "vpcmpestri",ecx, xx, Vdq, Wdq, Ib, mrm|vex|reqp|xop, fW6, exop[12]},
  }, { /* vex ext 55 */
    {OP_pcmpistrm, 0x663a6218, "pcmpistrm",xmm0, xx, Vdq, Wdq, Ib, mrm|reqp, fW6, END_LIST},
    {OP_vpcmpistrm,0x663a6218, "vpcmpistrm",xmm0, xx, Vdq, Wdq, Ib, mrm|vex|reqp, fW6, END_LIST},
  }, { /* vex ext 56 */
    {OP_pcmpistri, 0x663a6318, "pcmpistri",ecx, xx, Vdq, Wdq, Ib, mrm|reqp, fW6, END_LIST},
    {OP_vpcmpistri,0x663a6318, "vpcmpistri",ecx, xx, Vdq, Wdq, Ib, mrm|vex|reqp, fW6, END_LIST},
  }, { /* vex ext 57 */
    {OP_pclmulqdq, 0x663a4418, "pclmulqdq", Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vpclmulqdq,0x663a4418, "vpclmulqdq", Vdq, xx, Hdq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 58 */
    {OP_aeskeygenassist, 0x663adf18, "aeskeygenassist",Vdq,xx,Wdq,Ib,xx,mrm|reqp,x,END_LIST},
    {OP_vaeskeygenassist,0x663adf18, "vaeskeygenassist",Vdq,xx,Wdq,Ib,xx,mrm|vex|reqp,x,END_LIST},
  }, { /* vex ext 59 */
    {INVALID,   0x66380e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtestps, 0x66380e18, "vtestps", xx, xx, Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
  }, { /* vex ext 60 */
    {INVALID,   0x66380f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtestpd, 0x66380f18, "vtestpd", xx, xx, Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
  }, { /* vex ext 61 */
    {OP_ldmxcsr, 0x0fae32, "ldmxcsr", xx, xx, Md, xx, xx, mrm, x, END_LIST},
    {OP_vldmxcsr, 0x0fae32, "vldmxcsr", xx, xx, Md, xx, xx, mrm|vex|reqL0, x, END_LIST},
  }, { /* vex ext 62 */
    {OP_stmxcsr, 0x0fae33, "stmxcsr", Md, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_vstmxcsr, 0x0fae33, "vstmxcsr", Md, xx, xx, xx, xx, mrm|vex, x, END_LIST},
  }, { /* vex ext 63 */
    {INVALID,   0x66381318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtph2ps, 0x66381318, "vcvtph2ps", Vx, xx, Wx, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 64 */
    {INVALID,   0x66381818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastss, 0x66381818, "vbroadcastss", Vx, xx, Wd_dq, xx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 65 */
    {INVALID,   0x66381918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastsd, 0x66381918, "vbroadcastsd", Vqq, xx, Wq_dq, xx, xx, mrm|vex|reqp|reqL1, x, END_LIST},
  }, { /* vex ext 66 */
    {INVALID,   0x66381a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastf128, 0x66381a18, "vbroadcastf128", Vqq, xx, Mdq, xx, xx, mrm|vex|reqp|reqL1, x, END_LIST},
  }, { /* vex ext 67 */
    {INVALID,   0x66382c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovps, 0x66382c18, "vmaskmovps", Vx, xx, Hx,Mx, xx, mrm|vex|reqp|predcx, x, tvex[69][1]},
  }, { /* vex ext 68 */
    {INVALID,   0x66382d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovpd, 0x66382d18, "vmaskmovpd", Vx, xx, Hx,Mx, xx, mrm|vex|reqp|predcx, x, tvex[70][1]},
  }, { /* vex ext 69 */
    {INVALID,   0x66382e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovps, 0x66382e18, "vmaskmovps", Mx, xx, Hx,Vx, xx, mrm|vex|reqp|predcx, x, END_LIST},
  }, { /* vex ext 70 */
    {INVALID,   0x66382f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovpd, 0x66382f18, "vmaskmovpd", Mx, xx, Hx,Vx, xx, mrm|vex|reqp|predcx, x, END_LIST},
  }, { /* vex ext 71 */
    {INVALID,   0x663a0418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilps, 0x663a0418, "vpermilps", Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, tvex[77][1]},
  }, { /* vex ext 72 */
    {INVALID,   0x663a0518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x663a0518, "vpermilpd", Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, tvex[78][1]},
  }, { /* vex ext 73 */
    {INVALID,   0x663a0618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vperm2f128, 0x663a0618, "vperm2f128", Vx, xx, Hx,Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 74 */
    {INVALID,   0x663a1818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vinsertf128, 0x663a1818, "vinsertf128", Vx, xx, Hx,Wx, Ib, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 75 */
    {INVALID,   0x663a1918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vextractf128, 0x663a1918, "vextractf128", Wdq, xx, Vdq_qq, Ib, xx, mrm|vex|reqp|reqL1, x, END_LIST},
  }, { /* vex ext 76 */
    {INVALID,   0x663a1d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtps2ph, 0x663a1d18, "vcvtps2ph", Wx, xx, Vx, Ib, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 77 */
    {INVALID,   0x66380c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilps, 0x66380c18, "vpermilps", Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  }, { /* vex ext 78 */
    {INVALID,   0x66380d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x66380d18, "vpermilpd", Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, END_LIST},
  },
};

/****************************************************************************
 * Instructions that differ depending on mod and rm bits in modrm byte
 * For mod, entry 0 is all mem ref mod values (0,1,2) while entry 1 is 3.
 * For the mem ref, we give just one of the 3 possible modrm bytes
 * (we only use it when encoding so we don't need all 3).
 */
const instr_info_t mod_extensions[][2] = {
  { /* mod extension 0 */
    {OP_sgdt, 0x0f0130, "sgdt", Ms, xx, xx, xx, xx, mrm, x, END_LIST},
    {RM_EXT,  0x0f0171, "(group 7 mod + rm ext 0)", xx, xx, xx, xx, xx, mrm, x, 0},
  },
  { /* mod extension 1 */
    {OP_sidt, 0x0f0131, "sidt",  Ms, xx, xx, xx, xx, mrm, x, END_LIST},
    {RM_EXT,  0x0f0171, "(group 7 mod + rm ext 1)", xx, xx, xx, xx, xx, mrm, x, 1},
  },
  { /* mod extension 2 */
    {OP_invlpg, 0x0f0137, "invlpg", xx, xx, Mm, xx, xx, mrm, x, END_LIST},
    {RM_EXT,    0x0f0177, "(group 7 mod + rm ext 2)", xx, xx, xx, xx, xx, mrm, x, 2},
  },
  { /* mod extension 3 */
    {OP_clflush, 0x0fae37, "clflush", xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_sfence,  0xf80fae77, "sfence",  xx, xx, xx, xx, xx, mrm, x, END_LIST},
  },
  { /* mod extension 4 */
    {OP_lidt,   0x0f0133, "lidt",  xx, xx, Ms, xx, xx, mrm, x, END_LIST},
    {RM_EXT,    0x0f0173, "(group 7 mod + rm ext 3)", xx, xx, xx, xx, xx, mrm, x, 3},
  },
  { /* mod extension 5 */
    {OP_lgdt,   0x0f0132, "lgdt",  xx, xx, Ms, xx, xx, mrm, x, END_LIST},
    {RM_EXT,    0x0f0172, "(group 7 mod + rm ext 4)", xx, xx, xx, xx, xx, mrm, x, 4},
  },
  { /* mod extension 6 */
    {REX_W_EXT, 0x0fae35, "(rex.w ext 3)", xx, xx, xx, xx, xx, mrm, x, 3},
    /* note that gdb thinks e9-ef are "lfence (bad)" (PR 239920) */
    {OP_lfence, 0xe80fae75, "lfence", xx, xx, xx, xx, xx, mrm, x, END_LIST},
  },
  { /* mod extension 7 */
    {REX_W_EXT,   0x0fae36, "(rex.w ext 4)", xx, xx, xx, xx, xx, mrm, x, 4},
    {OP_mfence,   0xf00fae76, "mfence", xx, xx, xx, xx, xx, mrm, x, END_LIST},
  },
  { /* mod extension 8 */
    {OP_vmovss,  0xf30f1010, "vmovss",  Vss, xx, Wss,  xx, xx, mrm|vex, x, modx[10][0]},
    {OP_vmovss,  0xf30f1010, "vmovss",  Vdq, xx, H12_dq, Uss, xx, mrm|vex, x, modx[10][1]},
  },
  { /* mod extension 9 */
    {OP_vmovsd,  0xf20f1010, "vmovsd",  Vsd, xx, Wsd,  xx, xx, mrm|vex, x, modx[11][0]},
    {OP_vmovsd,  0xf20f1010, "vmovsd",  Vdq, xx, Hsd, Usd, xx, mrm|vex, x, modx[11][1]},
  },
  { /* mod extension 10 */
    {OP_vmovss,  0xf30f1110, "vmovss",  Wss, xx, Vss,  xx, xx, mrm|vex, x, modx[ 8][1]},
    {OP_vmovss,  0xf30f1110, "vmovss",  Udq, xx, H12_dq, Vss, xx, mrm|vex, x, END_LIST},
  },
  { /* mod extension 11 */
    {OP_vmovsd,  0xf20f1110, "vmovsd",  Wsd, xx, Vsd,  xx, xx, mrm|vex, x, modx[ 9][1]},
    {OP_vmovsd,  0xf20f1110, "vmovsd",  Udq, xx, Hsd, Vsd, xx, mrm|vex, x, END_LIST},
  },
  { /* mod extension 12 */
    {PREFIX_EXT, 0x0fc736, "(prefix ext 137)", xx, xx, xx, xx, xx, no, x, 137},
    {OP_rdrand,  0x0fc736, "rdrand", Rv, xx, xx, xx, xx, mrm, fW6, END_LIST},
  },
  { /* mod extension 13 */
    /* The latest Intel table implies 0x66 prefix makes invalid instr but not worth
     * explicitly encoding that until we have more information.
     */
    {OP_vmptrst, 0x0fc737, "vmptrst", Mq, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_rdseed,  0x0fc737, "rdseed", Rv, xx, xx, xx, xx, mrm, fW6, END_LIST},
  },
  { /* mod extension 14 */
    {REX_W_EXT,  0x0fae30, "(rex.w ext 0)", xx, xx, xx, xx, xx, mrm, x, 0},
    /* Using reqp to avoid having to create a whole prefix_ext entry for one opcode.
     * Ditto below.
     */
    {OP_rdfsbase,0xf30fae30, "rdfsbase", Ry, xx, xx, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 15 */
    {REX_W_EXT,  0x0fae31, "(rex.w ext 1)", xx, xx, xx, xx, xx, mrm, x, 1},
    {OP_rdgsbase,0xf30fae31, "rdgsbase", Ry, xx, xx, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 16 */
    {VEX_EXT,    0x0fae32, "(vex ext 61)", xx, xx, xx, xx, xx, mrm, x, 61},
    {OP_wrfsbase,0xf30fae32, "wrfsbase", xx, xx, Ry, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 17 */
    {VEX_EXT,    0x0fae33, "(vex ext 62)", xx, xx, xx, xx, xx, mrm, x, 62},
    {OP_wrgsbase,0xf30fae33, "wrgsbase", xx, xx, Ry, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 18 */
    /* load from memory zeroes top bits */
    {OP_movss,  0xf30f1010, "movss",  Vdq, xx, Mss, xx, xx, mrm, x, modx[18][1]},
    {OP_movss,  0xf30f1010, "movss",  Vss, xx, Uss, xx, xx, mrm, x, tpe[1][1]},
  },
  { /* mod extension 19 */
    /* load from memory zeroes top bits */
    {OP_movsd,  0xf20f1010, "movsd",  Vdq, xx, Msd, xx, xx, mrm, x, modx[19][1]},
    {OP_movsd,  0xf20f1010, "movsd",  Vsd, xx, Usd, xx, xx, mrm, x, tpe[1][3]},
  },
};

/* Naturally all of these have modrm bytes even if they have no explicit operands */
const instr_info_t rm_extensions[][8] = {
  { /* rm extension 0 */
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmcall,   0xc10f0171, "vmcall",   xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmlaunch, 0xc20f0171, "vmlaunch", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmresume, 0xc30f0171, "vmresume", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmxoff,   0xc40f0171, "vmxoff",   xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* rm extension 1 */
    {OP_monitor, 0xc80f0171, "monitor",  xx, xx, eax, ecx, edx, mrm, x, END_LIST},
    {OP_mwait,   0xc90f0171, "mwait",  xx, xx, eax, ecx, xx, mrm, x, END_LIST},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* rm extension 2 */
    {OP_swapgs, 0xf80f0177, "swapgs", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_rdtscp, 0xf90f0177, "rdtscp", edx, eax, xx, xx, xx, mrm|xop, x, exop[10]},/*AMD-only*/
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* rm extension 3 */
    {OP_vmrun,  0xd80f0173, "vmrun", xx, xx, axAX, xx, xx, mrm, x, END_LIST},
    {OP_vmmcall,0xd90f0173, "vmmcall", xx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_vmload, 0xda0f0173, "vmload", xx, xx, axAX, xx, xx, mrm, x, END_LIST},
    {OP_vmsave, 0xdb0f0173, "vmsave", xx, xx, axAX, xx, xx, mrm, x, END_LIST},
    {OP_stgi,   0xdc0f0173, "stgi", xx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_clgi,   0xdd0f0173, "clgi", xx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_skinit, 0xde0f0173, "skinit", xx, xx, eax, xx, xx, mrm, x, END_LIST},
    {OP_invlpga,0xdf0f0173, "invlpga", xx, xx, axAX, ecx, xx, mrm, x, END_LIST},
  },
  { /* rm extension 4 */
    {OP_xgetbv, 0xd00f0172, "xgetbv", edx, eax, ecx, xx, xx, mrm, x, END_LIST},
    {OP_xsetbv, 0xd10f0172, "xsetbv", xx, xx, ecx, edx, eax, mrm, x, END_LIST},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmfunc, 0xd40f0172, "vmfunc", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    /* Only if the transaction fails does xend write to eax => predcx.
     * XXX i#1314: on failure eip is also written to.
     */
    {OP_xend,   0xd50f0172, "xend", eax, xx, xx, xx, xx, mrm|predcx, x, NA},
    {OP_xtest,  0xd60f0172, "xtest", xx, xx, xx, xx, xx, mrm, fW6, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
};

/****************************************************************************
 * Instructions that differ depending on whether in 64-bit mode
 */

const instr_info_t x64_extensions[][2] = {
  {    /* x64_ext 0 */
    {OP_inc,  0x400000, "inc", zAX, xx, zAX, xx, xx, i64, (fW6&(~fWC)), t64e[1][0]},
    {PREFIX,  0x400000, "rex", xx, xx, xx, xx, xx, no, x, PREFIX_REX_GENERAL},
  }, { /* x64_ext 1 */
    {OP_inc,  0x410000, "inc", zCX, xx, zCX, xx, xx, i64, (fW6&(~fWC)), t64e[2][0]},
    {PREFIX,  0x410000, "rex.b", xx, xx, xx, xx, xx, no, x, PREFIX_REX_B},
  }, { /* x64_ext 2 */
    {OP_inc,  0x420000, "inc", zDX, xx, zDX, xx, xx, i64, (fW6&(~fWC)), t64e[3][0]},
    {PREFIX,  0x420000, "rex.x", xx, xx, xx, xx, xx, no, x, PREFIX_REX_X},
  }, { /* x64_ext 3 */
    {OP_inc,  0x430000, "inc", zBX, xx, zBX, xx, xx, i64, (fW6&(~fWC)), t64e[4][0]},
    {PREFIX,  0x430000, "rex.xb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 4 */
    {OP_inc,  0x440000, "inc", zSP, xx, zSP, xx, xx, i64, (fW6&(~fWC)), t64e[5][0]},
    {PREFIX,  0x440000, "rex.r", xx, xx, xx, xx, xx, no, x, PREFIX_REX_R},
  }, { /* x64_ext 5 */
    {OP_inc,  0x450000, "inc", zBP, xx, zBP, xx, xx, i64, (fW6&(~fWC)), t64e[6][0]},
    {PREFIX,  0x450000, "rex.rb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_R|PREFIX_REX_B},
  }, { /* x64_ext 6 */
    {OP_inc,  0x460000, "inc", zSI, xx, zSI, xx, xx, i64, (fW6&(~fWC)), t64e[7][0]},
    {PREFIX,  0x460000, "rex.rx", xx, xx, xx, xx, xx, no, x, PREFIX_REX_R|PREFIX_REX_X},
  }, { /* x64_ext 7 */
    {OP_inc,  0x470000, "inc", zDI, xx, zDI, xx, xx, i64, (fW6&(~fWC)), tex[12][0]},
    {PREFIX,  0x470000, "rex.rxb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_R|PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 8 */
    {OP_dec,  0x480000, "dec", zAX, xx, zAX, xx, xx, i64, (fW6&(~fWC)), t64e[9][0]},
    {PREFIX,  0x480000, "rex.w", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W},
  }, { /* x64_ext 9 */
    {OP_dec,  0x490000, "dec", zCX, xx, zCX, xx, xx, i64, (fW6&(~fWC)), t64e[10][0]},
    {PREFIX,  0x490000, "rex.wb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_B},
  }, { /* x64_ext 10 */
    {OP_dec,  0x4a0000, "dec", zDX, xx, zDX, xx, xx, i64, (fW6&(~fWC)), t64e[11][0]},
    {PREFIX,  0x4a0000, "rex.wx", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_X},
  }, { /* x64_ext 11 */
    {OP_dec,  0x4b0000, "dec", zBX, xx, zBX, xx, xx, i64, (fW6&(~fWC)), t64e[12][0]},
    {PREFIX,  0x4b0000, "rex.wxb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 12 */
    {OP_dec,  0x4c0000, "dec", zSP, xx, zSP, xx, xx, i64, (fW6&(~fWC)), t64e[13][0]},
    {PREFIX,  0x4c0000, "rex.wr", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R},
  }, { /* x64_ext 13 */
    {OP_dec,  0x4d0000, "dec", zBP, xx, zBP, xx, xx, i64, (fW6&(~fWC)), t64e[14][0]},
    {PREFIX,  0x4d0000, "rex.wrb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R|PREFIX_REX_B},
  }, { /* x64_ext 14 */
    {OP_dec,  0x4e0000, "dec", zSI, xx, zSI, xx, xx, i64, (fW6&(~fWC)), t64e[15][0]},
    {PREFIX,  0x4e0000, "rex.wrx", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R|PREFIX_REX_X},
  }, { /* x64_ext 15 */
    {OP_dec,  0x4f0000, "dec", zDI, xx, zDI, xx, xx, i64, (fW6&(~fWC)), tex[12][1]},
    {PREFIX,  0x4f0000, "rex.wrxb", xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R|PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 16 */
    {OP_arpl,   0x630000, "arpl", Ew, xx, Gw, xx, xx, mrm|i64, fWZ, END_LIST},
    {OP_movsxd, 0x630000, "movsxd", Gv, xx, Ed, xx, xx, mrm|o64, x, END_LIST},
  },
};

/****************************************************************************
 * Instructions that differ depending on the first two bits of the 2nd byte,
 * or whether in x64 mode.
 */
const instr_info_t vex_prefix_extensions[][2] = {
  {    /* vex_prefix_ext 0 */
    {OP_les,  0xc40000, "les", Gz, es, Mp, xx, xx, mrm|i64, x, END_LIST},
    {PREFIX,  0xc40000, "vex+2b", xx, xx, xx, xx, xx, no, x, PREFIX_VEX_3B},
  }, { /* vex_prefix_ext 1 */
    {OP_lds,  0xc50000, "lds", Gz, ds, Mp, xx, xx, mrm|i64, x, END_LIST},
    {PREFIX,  0xc50000, "vex+1b", xx, xx, xx, xx, xx, no, x, PREFIX_VEX_2B},
  },
};

/****************************************************************************
 * Instructions that differ depending on bits 4 and 5 of the 2nd byte.
 */
const instr_info_t xop_prefix_extensions[][2] = {
  {    /* xop_prefix_ext 0 */
    {EXTENSION, 0x8f0000, "(group 1d)", xx, xx, xx, xx, xx, mrm, x, 26},
    {PREFIX,    0x8f0000, "xop", xx, xx, xx, xx, xx, no, x, PREFIX_XOP},
  },
};

/****************************************************************************
 * Instructions that differ depending on whether vex-encoded and vex.L
 * Index 0 = no vex, 1 = vex and vex.L=0, 2 = vex and vex.L=1
 */
const instr_info_t vex_L_extensions[][3] = {
  {    /* vex_L_ext 0 */
    {OP_emms,       0x0f7710, "emms", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vzeroupper, 0x0f7710, "vzeroupper", xx, xx, xx, xx, xx, vex, x, END_LIST},
    {OP_vzeroall,   0x0f7790, "vzeroall", xx, xx, xx, xx, xx, vex, x, END_LIST},
  },
};

/****************************************************************************
 * Instructions that differ depending on whether a rex prefix is present.
 */

/* Instructions that differ depending on whether rex.b in is present.
 * The table is indexed by rex.b: index 0 is for no rex.b.
 */
const instr_info_t rex_b_extensions[][2] = {
  { /* rex.b extension 0 */
    {OP_nop,  0x900000, "nop", xx, xx, xx, xx, xx, no, x, tpe[103][2]},
    /* For decoding we avoid needing new operand types by only getting
     * here if rex.b is set.  For encode, we would need either to take
     * REQUIRES_REX + OPCODE_SUFFIX or a new operand type for registers that
     * must be extended (could also try to list r8 instead of eax but
     * have to make sure all decode/encode routines can handle that as most
     * assume the registers listed here are 32-bit base): that's too
     * much effort for a corner case that we're not 100% certain works on
     * all x64 processors, so we just don't list in the encoding chain.
     */
    {OP_xchg, 0x900000, "xchg", eAX_x, eAX, eAX_x, eAX, xx, o64, x, END_LIST},
  },
};

/* Instructions that differ depending on whether rex.w in is present.
 * The table is indexed by rex.w: index 0 is for no rex.w.
 */
const instr_info_t rex_w_extensions[][2] = {
  { /* rex.w extension 0 */
    {OP_fxsave32, 0x0fae30, "fxsave",   Me, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_fxsave64, 0x0fae30, "fxsave64", Me, xx, xx, xx, xx, mrm|rex, x, END_LIST},
  },
  { /* rex.w extension 1 */
    {OP_fxrstor32, 0x0fae31, "fxrstor",   xx, xx, Me, xx, xx, mrm, x, END_LIST},
    {OP_fxrstor64, 0x0fae31, "fxrstor64", xx, xx, Me, xx, xx, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 2 */
    {OP_xsave32,   0x0fae34, "xsave",   Mxsave, xx, edx, eax, xx, mrm, x, END_LIST},
    {OP_xsave64,   0x0fae34, "xsave64", Mxsave, xx, edx, eax, xx, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 3 */
    {OP_xrstor32, 0x0fae35, "xrstor",   xx, xx, Mxsave, edx, eax, mrm, x, END_LIST},
    {OP_xrstor64, 0x0fae35, "xrstor64", xx, xx, Mxsave, edx, eax, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 4 */
    {OP_xsaveopt32, 0x0fae36, "xsaveopt",   Mxsave, xx, edx, eax, xx, mrm, x, END_LIST},
    {OP_xsaveopt64, 0x0fae36, "xsaveopt64", Mxsave, xx, edx, eax, xx, mrm|rex, o64, END_LIST},
  },
};

/****************************************************************************
 * 3-byte-opcode instructions: 0x0f 0x38 and 0x0f 0x3a.
 * SSSE3 and SSE4.
 *
 * XXX: if they add more 2nd byte possibilities, we could switch to one
 * large table here and one extension type with indices into which subtable.
 * For now we have two separate tables.
 *
 * N.B.: if any are added here that do not take modrm bytes, or whose
 * size can vary based on data16 or addr16, we need to modify our
 * decode_fast table assumptions!
 *
 * Many of these only come in Vdq,Wdq forms, yet still require the 0x66 prefix.
 * Rather than waste space in the prefix_extensions table for 4 entries 3 of which
 * are invalid, and need another layer of lookup, we use the new REQUIRES_PREFIX
 * flag ("reqp").
 *
 * Since large parts of the opcode space are empty, we save space by having a
 * table of 256 indices instead of 256 instr_info_t structs.
 */
const byte third_byte_38_index[256] = {
  /* 0   1   2   3    4   5   6   7    8   9   A   B    C   D   E   F */
     1,  2,  3,  4,   5,  6,  7,  8,   9, 10, 11, 12,  96, 97, 56, 57,  /* 0 */
    16,  0,  0, 88,  17, 18,111, 19,  89, 90, 91,  0,  13, 14, 15,  0,  /* 1 */
    20, 21, 22, 23,  24, 25,  0,  0,  26, 27, 28, 29,  92, 93, 94, 95,  /* 2 */
    30, 31, 32, 33,  34, 35,112, 36,  37, 38, 39, 40,  41, 42, 43, 44,  /* 3 */
    45, 46,  0,  0,   0,113,114,115,   0,  0,  0,  0,   0,  0,  0,  0,  /* 4 */
     0,  0,  0,  0,   0,  0,  0,  0, 118,119,108,  0,   0,  0,  0,  0,  /* 5 */
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,  /* 6 */
     0,  0,  0,  0,   0,  0,  0,  0, 116,117,  0,  0,   0,  0,  0,  0,  /* 7 */
    49, 50,103,  0,   0,  0,  0,  0,   0,  0,  0,  0, 109,  0,110,  0,  /* 8 */
   104,105,106,107,   0,  0, 58, 59,  60, 61, 62, 63,  64, 65, 66, 67,  /* 9 */
     0,  0,  0,  0,   0,  0, 68, 69,  70, 71, 72, 73,  74, 75, 76, 77,  /* A */
     0,  0,  0,  0,   0,  0, 78, 79,  80, 81, 82, 83,  84, 85, 86, 87,  /* B */
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,  /* C */
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0, 51,  52, 53, 54, 55,  /* D */
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,  /* E */
    47, 48,100, 99,   0,101,102, 98,   0,  0,  0,  0,   0,  0,  0,  0   /* F */
};

const instr_info_t third_byte_38[] = {
  {INVALID,     0x38ff18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},              /* 0*/
  /**** SSSE3 ****/
  {PREFIX_EXT,  0x380018,   "(prefix ext 118)", xx, xx, xx, xx, xx, mrm, x, 118},/* 1*/
  {PREFIX_EXT,  0x380118,   "(prefix ext 119)", xx, xx, xx, xx, xx, mrm, x, 119},/* 2*/
  {PREFIX_EXT,  0x380218,   "(prefix ext 120)", xx, xx, xx, xx, xx, mrm, x, 120},/* 3*/
  {PREFIX_EXT,  0x380318,   "(prefix ext 121)", xx, xx, xx, xx, xx, mrm, x, 121},/* 4*/
  {PREFIX_EXT,  0x380418,   "(prefix ext 122)", xx, xx, xx, xx, xx, mrm, x, 122},/* 5*/
  {PREFIX_EXT,  0x380518,   "(prefix ext 123)", xx, xx, xx, xx, xx, mrm, x, 123},/* 6*/
  {PREFIX_EXT,  0x380618,   "(prefix ext 124)", xx, xx, xx, xx, xx, mrm, x, 124},/* 7*/
  {PREFIX_EXT,  0x380718,   "(prefix ext 125)", xx, xx, xx, xx, xx, mrm, x, 125},/* 8*/
  {PREFIX_EXT,  0x380818,   "(prefix ext 126)", xx, xx, xx, xx, xx, mrm, x, 126},/* 9*/
  {PREFIX_EXT,  0x380918,   "(prefix ext 127)", xx, xx, xx, xx, xx, mrm, x, 127},/*10*/
  {PREFIX_EXT,  0x380a18,   "(prefix ext 128)", xx, xx, xx, xx, xx, mrm, x, 128},/*11*/
  {PREFIX_EXT,  0x380b18,   "(prefix ext 129)", xx, xx, xx, xx, xx, mrm, x, 129},/*12*/
  {PREFIX_EXT,  0x381c18,   "(prefix ext 130)", xx, xx, xx, xx, xx, mrm, x, 130},/*13*/
  {PREFIX_EXT,  0x381d18,   "(prefix ext 131)", xx, xx, xx, xx, xx, mrm, x, 131},/*14*/
  {PREFIX_EXT,  0x381e18,   "(prefix ext 132)", xx, xx, xx, xx, xx, mrm, x, 132},/*15*/
  /**** SSE4 ****/
  {OP_pblendvb, 0x66381018, "pblendvb", Vdq, xx, Wdq,xmm0,Vdq, mrm|reqp,x, END_LIST},/*16*/
  {OP_blendvps, 0x66381418, "blendvps", Vdq, xx, Wdq,xmm0,Vdq, mrm|reqp,x, END_LIST},/*17*/
  {OP_blendvpd, 0x66381518, "blendvpd", Vdq, xx, Wdq,xmm0,Vdq, mrm|reqp,x, END_LIST},/*18*/
  {VEX_EXT,  0x66381718, "(vex ext  3)", xx, xx, xx, xx, xx, mrm, x,  3},/*19*/
  /* 20 */
  {VEX_EXT,  0x66382018, "(vex ext  4)", xx, xx, xx, xx, xx, mrm, x,  4},/*20*/
  {VEX_EXT,  0x66382118, "(vex ext  5)", xx, xx, xx, xx, xx, mrm, x,  5},/*21*/
  {VEX_EXT,  0x66382218, "(vex ext  6)", xx, xx, xx, xx, xx, mrm, x,  6},/*22*/
  {VEX_EXT,  0x66382318, "(vex ext  7)", xx, xx, xx, xx, xx, mrm, x,  7},/*23*/
  {VEX_EXT,  0x66382418, "(vex ext  8)", xx, xx, xx, xx, xx, mrm, x,  8},/*24*/
  {VEX_EXT,  0x66382518, "(vex ext  9)", xx, xx, xx, xx, xx, mrm, x,  9},/*25*/
  {VEX_EXT,  0x66382818, "(vex ext 10)", xx, xx, xx, xx, xx, mrm, x, 10},/*26*/
  {VEX_EXT,  0x66382918, "(vex ext 11)", xx, xx, xx, xx, xx, mrm, x, 11},/*27*/
  {VEX_EXT,  0x66382a18, "(vex ext 12)", xx, xx, xx, xx, xx, mrm, x, 12},/*28*/
  {VEX_EXT,  0x66382b18, "(vex ext 13)", xx, xx, xx, xx, xx, mrm, x, 13},/*29*/
  /* 30 */
  {VEX_EXT,  0x66383018, "(vex ext 14)", xx, xx, xx, xx, xx, mrm, x, 14},/*30*/
  {VEX_EXT,  0x66383118, "(vex ext 15)", xx, xx, xx, xx, xx, mrm, x, 15},/*31*/
  {VEX_EXT,  0x66383218, "(vex ext 16)", xx, xx, xx, xx, xx, mrm, x, 16},/*32*/
  {VEX_EXT,  0x66383318, "(vex ext 17)", xx, xx, xx, xx, xx, mrm, x, 17},/*33*/
  {VEX_EXT,  0x66383418, "(vex ext 18)", xx, xx, xx, xx, xx, mrm, x, 18},/*34*/
  {VEX_EXT,  0x66383518, "(vex ext 19)", xx, xx, xx, xx, xx, mrm, x, 19},/*35*/
  {VEX_EXT,  0x66383718, "(vex ext 20)", xx, xx, xx, xx, xx, mrm, x, 20},/*36*/
  {VEX_EXT,  0x66383818, "(vex ext 21)", xx, xx, xx, xx, xx, mrm, x, 21},/*37*/
  {VEX_EXT,  0x66383918, "(vex ext 22)", xx, xx, xx, xx, xx, mrm, x, 22},/*38*/
  {VEX_EXT,  0x66383a18, "(vex ext 23)", xx, xx, xx, xx, xx, mrm, x, 23},/*39*/
  {VEX_EXT,  0x66383b18, "(vex ext 24)", xx, xx, xx, xx, xx, mrm, x, 24},/*40*/
  {VEX_EXT,  0x66383c18, "(vex ext 25)", xx, xx, xx, xx, xx, mrm, x, 25},/*41*/
  {VEX_EXT,  0x66383d18, "(vex ext 26)", xx, xx, xx, xx, xx, mrm, x, 26},/*42*/
  {VEX_EXT,  0x66383e18, "(vex ext 27)", xx, xx, xx, xx, xx, mrm, x, 27},/*43*/
  {VEX_EXT,  0x66383f18, "(vex ext 28)", xx, xx, xx, xx, xx, mrm, x, 28},/*44*/
  /* 40 */
  {VEX_EXT,  0x66384018, "(vex ext 29)", xx, xx, xx, xx, xx, mrm, x, 29},/*45*/
  {VEX_EXT,  0x66384118, "(vex ext 30)", xx, xx, xx, xx, xx, mrm, x, 30},/*46*/
  /* f0 */
  {PREFIX_EXT,  0x38f018,   "(prefix ext 138)", xx, xx, xx, xx, xx, mrm, x, 138},/*47*/
  {PREFIX_EXT,  0x38f118,   "(prefix ext 139)", xx, xx, xx, xx, xx, mrm, x, 139},/*48*/
  /* 80 */
  {OP_invept,   0x66388018, "invept",   xx, xx, Gr, Mdq, xx, mrm|reqp, x, END_LIST},/*49*/
  {OP_invvpid,  0x66388118, "invvpid",  xx, xx, Gr, Mdq, xx, mrm|reqp, x, END_LIST},/*50*/
  /* db-df */
  {VEX_EXT,  0x6638db18, "(vex ext 31)", xx, xx, xx, xx, xx, mrm, x, 31},/*51*/
  {VEX_EXT,  0x6638dc18, "(vex ext 32)", xx, xx, xx, xx, xx, mrm, x, 32},/*52*/
  {VEX_EXT,  0x6638dd18, "(vex ext 33)", xx, xx, xx, xx, xx, mrm, x, 33},/*53*/
  {VEX_EXT,  0x6638de18, "(vex ext 34)", xx, xx, xx, xx, xx, mrm, x, 34},/*54*/
  {VEX_EXT,  0x6638df18, "(vex ext 35)", xx, xx, xx, xx, xx, mrm, x, 35},/*55*/
  /* AVX */
  {VEX_EXT,  0x66380e18, "(vex ext 59)", xx, xx, xx, xx, xx, mrm, x, 59},/*56*/
  {VEX_EXT,  0x66380f18, "(vex ext 60)", xx, xx, xx, xx, xx, mrm, x, 60},/*57*/
  /* FMA 96-9f */
  {VEX_W_EXT, 0x66389618, "(vex_W ext  6)", xx, xx, xx, xx, xx, mrm, x,  6},/*58*/
  {VEX_W_EXT, 0x66389718, "(vex_W ext  9)", xx, xx, xx, xx, xx, mrm, x,  9},/*59*/
  {VEX_W_EXT, 0x66389818, "(vex_W ext  0)", xx, xx, xx, xx, xx, mrm, x,  0},/*60*/
  {VEX_W_EXT, 0x66389918, "(vex_W ext  3)", xx, xx, xx, xx, xx, mrm, x,  3},/*61*/
  {VEX_W_EXT, 0x66389a18, "(vex_W ext 12)", xx, xx, xx, xx, xx, mrm, x, 12},/*62*/
  {VEX_W_EXT, 0x66389b18, "(vex_W ext 15)", xx, xx, xx, xx, xx, mrm, x, 15},/*63*/
  {VEX_W_EXT, 0x66389c18, "(vex_W ext 18)", xx, xx, xx, xx, xx, mrm, x, 18},/*64*/
  {VEX_W_EXT, 0x66389d18, "(vex_W ext 21)", xx, xx, xx, xx, xx, mrm, x, 21},/*65*/
  {VEX_W_EXT, 0x66389e18, "(vex_W ext 24)", xx, xx, xx, xx, xx, mrm, x, 24},/*66*/
  {VEX_W_EXT, 0x66389f18, "(vex_W ext 27)", xx, xx, xx, xx, xx, mrm, x, 27},/*67*/
  /* FMA a6-af */
  {VEX_W_EXT, 0x6638a618, "(vex_W ext  7)", xx, xx, xx, xx, xx, mrm, x,  7},/*68*/
  {VEX_W_EXT, 0x6638a718, "(vex_W ext 10)", xx, xx, xx, xx, xx, mrm, x, 10},/*69*/
  {VEX_W_EXT, 0x6638a818, "(vex_W ext  1)", xx, xx, xx, xx, xx, mrm, x,  1},/*70*/
  {VEX_W_EXT, 0x6638a918, "(vex_W ext  4)", xx, xx, xx, xx, xx, mrm, x,  4},/*71*/
  {VEX_W_EXT, 0x6638aa18, "(vex_W ext 13)", xx, xx, xx, xx, xx, mrm, x, 13},/*72*/
  {VEX_W_EXT, 0x6638ab18, "(vex_W ext 16)", xx, xx, xx, xx, xx, mrm, x, 16},/*73*/
  {VEX_W_EXT, 0x6638ac18, "(vex_W ext 19)", xx, xx, xx, xx, xx, mrm, x, 19},/*74*/
  {VEX_W_EXT, 0x6638ad18, "(vex_W ext 22)", xx, xx, xx, xx, xx, mrm, x, 22},/*75*/
  {VEX_W_EXT, 0x6638ae18, "(vex_W ext 25)", xx, xx, xx, xx, xx, mrm, x, 25},/*76*/
  {VEX_W_EXT, 0x6638af18, "(vex_W ext 28)", xx, xx, xx, xx, xx, mrm, x, 28},/*77*/
  /* FMA b6-bf */
  {VEX_W_EXT, 0x6638b618, "(vex_W ext  8)", xx, xx, xx, xx, xx, mrm, x,  8},/*78*/
  {VEX_W_EXT, 0x6638b718, "(vex_W ext 11)", xx, xx, xx, xx, xx, mrm, x, 11},/*79*/
  {VEX_W_EXT, 0x6638b818, "(vex_W ext  2)", xx, xx, xx, xx, xx, mrm, x,  2},/*80*/
  {VEX_W_EXT, 0x6638b918, "(vex_W ext  5)", xx, xx, xx, xx, xx, mrm, x,  5},/*81*/
  {VEX_W_EXT, 0x6638ba18, "(vex_W ext 14)", xx, xx, xx, xx, xx, mrm, x, 14},/*82*/
  {VEX_W_EXT, 0x6638bb18, "(vex_W ext 17)", xx, xx, xx, xx, xx, mrm, x, 17},/*83*/
  {VEX_W_EXT, 0x6638bc18, "(vex_W ext 20)", xx, xx, xx, xx, xx, mrm, x, 20},/*84*/
  {VEX_W_EXT, 0x6638bd18, "(vex_W ext 23)", xx, xx, xx, xx, xx, mrm, x, 23},/*85*/
  {VEX_W_EXT, 0x6638be18, "(vex_W ext 26)", xx, xx, xx, xx, xx, mrm, x, 26},/*86*/
  {VEX_W_EXT, 0x6638bf18, "(vex_W ext 29)", xx, xx, xx, xx, xx, mrm, x, 29},/*87*/
  /* AVX overlooked in original pass */
  {VEX_EXT, 0x66381318, "(vex ext 63)", xx, xx, xx, xx, xx, mrm, x, 63},/*88*/
  {VEX_EXT, 0x66381818, "(vex ext 64)", xx, xx, xx, xx, xx, mrm, x, 64},/*89*/
  {VEX_EXT, 0x66381918, "(vex ext 65)", xx, xx, xx, xx, xx, mrm, x, 65},/*90*/
  {VEX_EXT, 0x66381a18, "(vex ext 66)", xx, xx, xx, xx, xx, mrm, x, 66},/*91*/
  {VEX_EXT, 0x66382c18, "(vex ext 67)", xx, xx, xx, xx, xx, mrm, x, 67},/*92*/
  {VEX_EXT, 0x66382d18, "(vex ext 68)", xx, xx, xx, xx, xx, mrm, x, 68},/*93*/
  {VEX_EXT, 0x66382e18, "(vex ext 69)", xx, xx, xx, xx, xx, mrm, x, 69},/*94*/
  {VEX_EXT, 0x66382f18, "(vex ext 70)", xx, xx, xx, xx, xx, mrm, x, 70},/*95*/
  {VEX_EXT, 0x66380c18, "(vex ext 77)", xx, xx, xx, xx, xx, mrm, x, 77},/*96*/
  {VEX_EXT, 0x66380d18, "(vex ext 78)", xx, xx, xx, xx, xx, mrm, x, 78},/*97*/
  /* TBM */
  {PREFIX_EXT, 0x38f718, "(prefix ext 141)", xx, xx, xx, xx, xx, mrm, x, 141},  /*98*/
  /* BMI1 */
  {EXTENSION, 0x38f318, "(group 17)", By, xx, Ey, xx, xx, mrm|vex, x, 31},      /*99*/
  /* marked reqp b/c it should have no prefix (prefixes for future opcodes) */
  {OP_andn, 0x38f218, "andn", Gy, xx, By, Ey, xx, mrm|vex|reqp, fW6, END_LIST},/*100*/
  /* BMI2 */
  {PREFIX_EXT, 0x38f518, "(prefix ext 142)", xx, xx, xx, xx, xx, mrm, x, 142}, /*101*/
  {PREFIX_EXT, 0x38f618, "(prefix ext 143)", xx, xx, xx, xx, xx, mrm, x, 143}, /*102*/
  {OP_invpcid, 0x66388218, "invpcid",  xx, xx, Gy, Mdq, xx, mrm|reqp, x, END_LIST},/*103*/
  /* AVX2 */
  {VEX_W_EXT, 0x66389018, "(vex_W ext 66)", xx, xx, xx, xx, xx, mrm|vex, x, 66},/*104*/
  {VEX_W_EXT, 0x66389118, "(vex_W ext 67)", xx, xx, xx, xx, xx, mrm|vex, x, 67},/*105*/
  {VEX_W_EXT, 0x66389218, "(vex_W ext 68)", xx, xx, xx, xx, xx, mrm|vex, x, 68},/*106*/
  {VEX_W_EXT, 0x66389318, "(vex_W ext 69)", xx, xx, xx, xx, xx, mrm|vex, x, 69},/*107*/
  {OP_vbroadcasti128,0x66385a18, "vbroadcasti128",Vqq,xx,Mdq,xx,xx,mrm|vex|reqp,x,END_LIST},/*108*/
  {VEX_W_EXT, 0x66388c18, "(vex_W ext 70)", xx,xx,xx,xx,xx, mrm|vex|reqp, x, 70},/*109*/
  {VEX_W_EXT, 0x66388e18, "(vex_W ext 71)", xx,xx,xx,xx,xx, mrm|vex|reqp, x, 71},/*110*/
  /* Following Intel and not marking as packed float vs ints: just "qq". */
  {OP_vpermps,0x66381618, "vpermps",Vqq,xx,Hqq,Wqq,xx, mrm|vex|reqp,x,END_LIST}, /*111*/
  {OP_vpermd, 0x66383618, "vpermd", Vqq,xx,Hqq,Wqq,xx, mrm|vex|reqp,x,END_LIST}, /*112*/
  {VEX_W_EXT, 0x66384518, "(vex_W ext 72)", xx,xx,xx,xx,xx, mrm|vex|reqp, x, 72},/*113*/
  {OP_vpsravd,0x66384618, "vpsravd", Vx,xx,Hx,Wx,xx, mrm|vex|reqp, x, END_LIST}, /*114*/
  {VEX_W_EXT, 0x66384718, "(vex_W ext 73)", xx,xx,xx,xx,xx, mrm|vex|reqp, x, 73},/*115*/
  {OP_vpbroadcastb, 0x66387818, "vpbroadcastb", Vx, xx, Wb_dq, xx, xx, mrm|vex|reqp, x, END_LIST},/*116*/
  {OP_vpbroadcastw, 0x66387918, "vpbroadcastw", Vx, xx, Ww_dq, xx, xx, mrm|vex|reqp, x, END_LIST},/*117*/
  {OP_vpbroadcastd, 0x66385818, "vpbroadcastd", Vx, xx, Wd_dq, xx, xx, mrm|vex|reqp, x, END_LIST},/*118*/
  {OP_vpbroadcastq, 0x66385918, "vpbroadcastq", Vx, xx, Wq_dq, xx, xx, mrm|vex|reqp, x, END_LIST},/*119*/
};

/* N.B.: every 0x3a instr so far has an immediate.  If a version w/o an immed
 * comes along we'll have to add a threebyte_3a_vex_extra[] table to decode_fast.c.
 */
const byte third_byte_3a_index[256] = {
  /* 0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    59,60,61, 0, 28,29,30, 0,  6, 7, 8, 9, 10,11,12, 1,  /* 0 */
     0, 0, 0, 0,  2, 3, 4, 5, 31,32, 0, 0,  0,33, 0, 0,  /* 1 */
    13,14,15, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 2 */
     0, 0, 0, 0,  0, 0, 0, 0, 57,58, 0, 0,  0, 0, 0, 0,  /* 3 */
    16,17,18, 0, 23, 0,62, 0, 54,55,25,26, 27, 0, 0, 0,  /* 4 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 34,35,36,37,  /* 5 */
    19,20,21,22,  0, 0, 0, 0, 38,39,40,41, 42,43,44,45,  /* 6 */
     0, 0, 0, 0,  0, 0, 0, 0, 46,47,48,49, 50,51,52,53,  /* 7 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 8 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 9 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* A */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* B */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* C */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0,24,  /* D */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* E */
    56, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0   /* F */
};
const instr_info_t third_byte_3a[] = {
  {INVALID,     0x3aff18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},                 /* 0*/
  /**** SSSE3 ****/
  {PREFIX_EXT,  0x3a0f18, "(prefix ext 133)", xx, xx, xx, xx, xx, mrm, x, 133},    /* 1*/
  /**** SSE4 ****/
  {VEX_EXT,  0x663a1418, "(vex ext 36)", xx, xx, xx, xx, xx, mrm, x, 36},/* 2*/
  {VEX_EXT,  0x663a1518, "(vex ext 37)", xx, xx, xx, xx, xx, mrm, x, 37},/* 3*/
  {VEX_EXT,  0x663a1618, "(vex ext 38)", xx, xx, xx, xx, xx, mrm, x, 38},/* 4*/
  {VEX_EXT,  0x663a1718, "(vex ext 39)", xx, xx, xx, xx, xx, mrm, x, 39},/* 5*/
  {VEX_EXT,  0x663a0818, "(vex ext 40)", xx, xx, xx, xx, xx, mrm, x, 40},/* 6*/
  {VEX_EXT,  0x663a0918, "(vex ext 41)", xx, xx, xx, xx, xx, mrm, x, 41},/* 7*/
  {VEX_EXT,  0x663a0a18, "(vex ext 42)", xx, xx, xx, xx, xx, mrm, x, 42},/* 8*/
  {VEX_EXT,  0x663a0b18, "(vex ext 43)", xx, xx, xx, xx, xx, mrm, x, 43},/* 9*/
  {VEX_EXT,  0x663a0c18, "(vex ext 44)", xx, xx, xx, xx, xx, mrm, x, 44},/*10*/
  {VEX_EXT,  0x663a0d18, "(vex ext 45)", xx, xx, xx, xx, xx, mrm, x, 45},/*11*/
  {VEX_EXT,  0x663a0e18, "(vex ext 46)", xx, xx, xx, xx, xx, mrm, x, 46},/*12*/
  /* 20 */
  {VEX_EXT,  0x663a2018, "(vex ext 47)", xx, xx, xx, xx, xx, mrm, x, 47},/*13*/
  {VEX_EXT,  0x663a2118, "(vex ext 48)", xx, xx, xx, xx, xx, mrm, x, 48},/*14*/
  {VEX_EXT,  0x663a2218, "(vex ext 49)", xx, xx, xx, xx, xx, mrm, x, 49},/*15*/
  /* 40 */
  {VEX_EXT,  0x663a4018, "(vex ext 50)", xx, xx, xx, xx, xx, mrm, x, 50},/*16*/
  {VEX_EXT,  0x663a4118, "(vex ext 51)", xx, xx, xx, xx, xx, mrm, x, 51},/*17*/
  {VEX_EXT,  0x663a4218, "(vex ext 52)", xx, xx, xx, xx, xx, mrm, x, 52},/*18*/
  /* 60 */
  {VEX_EXT,  0x663a6018, "(vex ext 53)", xx, xx, xx, xx, xx, mrm, x, 53},/*19*/
  {VEX_EXT,  0x663a6118, "(vex ext 54)", xx, xx, xx, xx, xx, mrm, x, 54},/*20*/
  {VEX_EXT,  0x663a6218, "(vex ext 55)", xx, xx, xx, xx, xx, mrm, x, 55},/*21*/
  {VEX_EXT,  0x663a6318, "(vex ext 56)", xx, xx, xx, xx, xx, mrm, x, 56},/*22*/
  {VEX_EXT,  0x663a4418, "(vex ext 57)", xx, xx, xx, xx, xx, mrm, x, 57},/*23*/
  {VEX_EXT,  0x663adf18, "(vex ext 58)", xx, xx, xx, xx, xx, mrm, x, 58},/*24*/
  /* AVX overlooked in original pass */
  {VEX_EXT,  0x663a4a18, "(vex ext  0)", xx, xx, xx, xx, xx, mrm, x,  0},/*25*/
  {VEX_EXT,  0x663a4b18, "(vex ext  1)", xx, xx, xx, xx, xx, mrm, x,  1},/*26*/
  {VEX_EXT,  0x663a4c18, "(vex ext  2)", xx, xx, xx, xx, xx, mrm, x,  2},/*27*/
  {VEX_EXT,  0x663a0418, "(vex ext 71)", xx, xx, xx, xx, xx, mrm, x, 71},/*28*/
  {VEX_EXT,  0x663a0518, "(vex ext 72)", xx, xx, xx, xx, xx, mrm, x, 72},/*29*/
  {VEX_EXT,  0x663a0618, "(vex ext 73)", xx, xx, xx, xx, xx, mrm, x, 73},/*30*/
  {VEX_EXT,  0x663a1818, "(vex ext 74)", xx, xx, xx, xx, xx, mrm, x, 74},/*31*/
  {VEX_EXT,  0x663a1918, "(vex ext 75)", xx, xx, xx, xx, xx, mrm, x, 75},/*32*/
  {VEX_EXT,  0x663a1d18, "(vex ext 76)", xx, xx, xx, xx, xx, mrm, x, 76},/*33*/
  /* FMA4 */
  {VEX_W_EXT,0x663a5c18, "(vex_W ext 30)", xx, xx, xx, xx, xx, mrm, x, 30},/*34*/
  {VEX_W_EXT,0x663a5d18, "(vex_W ext 31)", xx, xx, xx, xx, xx, mrm, x, 31},/*35*/
  {VEX_W_EXT,0x663a5e18, "(vex_W ext 32)", xx, xx, xx, xx, xx, mrm, x, 32},/*36*/
  {VEX_W_EXT,0x663a5f18, "(vex_W ext 33)", xx, xx, xx, xx, xx, mrm, x, 33},/*37*/
  {VEX_W_EXT,0x663a6818, "(vex_W ext 34)", xx, xx, xx, xx, xx, mrm, x, 34},/*38*/
  {VEX_W_EXT,0x663a6918, "(vex_W ext 35)", xx, xx, xx, xx, xx, mrm, x, 35},/*39*/
  {VEX_W_EXT,0x663a6a18, "(vex_W ext 36)", xx, xx, xx, xx, xx, mrm, x, 36},/*40*/
  {VEX_W_EXT,0x663a6b18, "(vex_W ext 37)", xx, xx, xx, xx, xx, mrm, x, 37},/*41*/
  {VEX_W_EXT,0x663a6c18, "(vex_W ext 38)", xx, xx, xx, xx, xx, mrm, x, 38},/*42*/
  {VEX_W_EXT,0x663a6d18, "(vex_W ext 39)", xx, xx, xx, xx, xx, mrm, x, 39},/*43*/
  {VEX_W_EXT,0x663a6e18, "(vex_W ext 40)", xx, xx, xx, xx, xx, mrm, x, 40},/*44*/
  {VEX_W_EXT,0x663a6f18, "(vex_W ext 41)", xx, xx, xx, xx, xx, mrm, x, 41},/*45*/
  {VEX_W_EXT,0x663a7818, "(vex_W ext 42)", xx, xx, xx, xx, xx, mrm, x, 42},/*46*/
  {VEX_W_EXT,0x663a7918, "(vex_W ext 43)", xx, xx, xx, xx, xx, mrm, x, 43},/*47*/
  {VEX_W_EXT,0x663a7a18, "(vex_W ext 44)", xx, xx, xx, xx, xx, mrm, x, 44},/*48*/
  {VEX_W_EXT,0x663a7b18, "(vex_W ext 45)", xx, xx, xx, xx, xx, mrm, x, 45},/*49*/
  {VEX_W_EXT,0x663a7c18, "(vex_W ext 46)", xx, xx, xx, xx, xx, mrm, x, 46},/*50*/
  {VEX_W_EXT,0x663a7d18, "(vex_W ext 47)", xx, xx, xx, xx, xx, mrm, x, 47},/*51*/
  {VEX_W_EXT,0x663a7e18, "(vex_W ext 48)", xx, xx, xx, xx, xx, mrm, x, 48},/*52*/
  {VEX_W_EXT,0x663a7f18, "(vex_W ext 49)", xx, xx, xx, xx, xx, mrm, x, 49},/*53*/
  /* XOP */
  {VEX_W_EXT,0x663a4818, "(vex_W ext 64)", xx, xx, xx, xx, xx, mrm, x, 64},/*54*/
  {VEX_W_EXT,0x663a4918, "(vex_W ext 65)", xx, xx, xx, xx, xx, mrm, x, 65},/*55*/
  /* BMI2 */
  {OP_rorx,  0xf23af018, "rorx",  Gy, xx, Ey, Ib, xx, mrm|vex|reqp, x, END_LIST},/*56*/
  /* AVX2 */
  {OP_vinserti128,0x663a3818,"vinserti128",Vqq,xx,Hqq,Wqq,Ib,mrm|vex|reqp,x,END_LIST},/*57*/
  {OP_vextracti128,0x663a3918,"vextracti128",Wdq,xx,Vqq,Ib,xx,mrm|vex|reqp,x,END_LIST},/*58*/
  {OP_vpermq, 0x663a0018, "vpermq", Vqq,xx,Wqq,Ib,xx,mrm|vex|reqp,x,END_LIST},/*59*/
  /* Following Intel and not marking as packed float vs ints: just "qq". */
  {OP_vpermpd,0x663a0118, "vpermpd",Vqq,xx,Wqq,Ib,xx,mrm|vex|reqp,x,END_LIST},/*60*/
  {OP_vpblendd,0x663a0218,"vpblendd",Vx,xx,Hx,Wx,Ib, mrm|vex|reqp,x,END_LIST},/*61*/
  {OP_vperm2i128,0x663a4618,"vperm2i128",Vqq,xx,Hqq,Wqq,Ib, mrm|vex|reqp,x,END_LIST},/*62*/
};

/****************************************************************************
 * Instructions that differ depending on vex.W
 * Index is vex.W value
 */
const instr_info_t vex_W_extensions[][2] = {
  {    /* vex_W_ext 0 */
    {OP_vfmadd132ps,0x66389818,"vfmadd132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmadd132pd,0x66389858,"vfmadd132pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 1 */
    {OP_vfmadd213ps,0x6638a818,"vfmadd213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmadd213pd,0x6638a858,"vfmadd213pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 2 */
    {OP_vfmadd231ps,0x6638b818,"vfmadd231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmadd231pd,0x6638b858,"vfmadd231pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 3 */
    {OP_vfmadd132ss,0x66389918,"vfmadd132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfmadd132sd,0x66389958,"vfmadd132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 4 */
    {OP_vfmadd213ss,0x6638a918,"vfmadd213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfmadd213sd,0x6638a958,"vfmadd213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 5 */
    {OP_vfmadd231ss,0x6638b918,"vfmadd231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfmadd231sd,0x6638b958,"vfmadd231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 6 */
    {OP_vfmaddsub132ps,0x66389618,"vfmaddsub132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmaddsub132pd,0x66389658,"vfmaddsub132pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 7 */
    {OP_vfmaddsub213ps,0x6638a618,"vfmaddsub213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmaddsub213pd,0x6638a658,"vfmaddsub213pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 8 */
    {OP_vfmaddsub231ps,0x6638b618,"vfmaddsub231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmaddsub231pd,0x6638b658,"vfmaddsub231pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 9 */
    {OP_vfmsubadd132ps,0x66389718,"vfmsubadd132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsubadd132pd,0x66389758,"vfmsubadd132pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 10 */
    {OP_vfmsubadd213ps,0x6638a718,"vfmsubadd213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsubadd213pd,0x6638a758,"vfmsubadd213pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 11 */
    {OP_vfmsubadd231ps,0x6638b718,"vfmsubadd231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsubadd231pd,0x6638b758,"vfmsubadd231pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 12 */
    {OP_vfmsub132ps,0x66389a18,"vfmsub132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsub132pd,0x66389a58,"vfmsub132pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 13 */
    {OP_vfmsub213ps,0x6638aa18,"vfmsub213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsub213pd,0x6638aa58,"vfmsub213pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 14 */
    {OP_vfmsub231ps,0x6638ba18,"vfmsub231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsub231pd,0x6638ba58,"vfmsub231pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 15 */
    {OP_vfmsub132ss,0x66389b18,"vfmsub132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsub132sd,0x66389b58,"vfmsub132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 16 */
    {OP_vfmsub213ss,0x6638ab18,"vfmsub213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsub213sd,0x6638ab58,"vfmsub213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 17 */
    {OP_vfmsub231ss,0x6638bb18,"vfmsub231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfmsub231sd,0x6638bb58,"vfmsub231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 18 */
    {OP_vfnmadd132ps,0x66389c18,"vfnmadd132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmadd132pd,0x66389c58,"vfnmadd132pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 19 */
    {OP_vfnmadd213ps,0x6638ac18,"vfnmadd213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmadd213pd,0x6638ac58,"vfnmadd213pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 20 */
    {OP_vfnmadd231ps,0x6638bc18,"vfnmadd231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmadd231pd,0x6638bc58,"vfnmadd231pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 21 */
    {OP_vfnmadd132ss,0x66389d18,"vfnmadd132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmadd132sd,0x66389d58,"vfnmadd132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 22 */
    {OP_vfnmadd213ss,0x6638ad18,"vfnmadd213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmadd213sd,0x6638ad58,"vfnmadd213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 23 */
    {OP_vfnmadd231ss,0x6638bd18,"vfnmadd231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmadd231sd,0x6638bd58,"vfnmadd231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 24 */
    {OP_vfnmsub132ps,0x66389e18,"vfnmsub132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmsub132pd,0x66389e58,"vfnmsub132pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 25 */
    {OP_vfnmsub213ps,0x6638ae18,"vfnmsub213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmsub213pd,0x6638ae58,"vfnmsub213pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 26 */
    {OP_vfnmsub231ps,0x6638be18,"vfnmsub231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmsub231pd,0x6638be58,"vfnmsub231pd",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 27 */
    {OP_vfnmsub132ss,0x66389f18,"vfnmsub132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmsub132sd,0x66389f58,"vfnmsub132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 28 */
    {OP_vfnmsub213ss,0x6638af18,"vfnmsub213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmsub213sd,0x6638af58,"vfnmsub213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 29 */
    {OP_vfnmsub231ss,0x6638bf18,"vfnmsub231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,END_LIST},
    {OP_vfnmsub231sd,0x6638bf58,"vfnmsub231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 30 */
    {OP_vfmaddsubps,0x663a5c18,"vfmaddsubps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[30][1]},
    {OP_vfmaddsubps,0x663a5c58,"vfmaddsubps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 31 */
    {OP_vfmaddsubpd,0x663a5d18,"vfmaddsubpd",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[31][1]},
    {OP_vfmaddsubpd,0x663a5d58,"vfmaddsubpd",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 32 */
    {OP_vfmsubaddps,0x663a5e18,"vfmsubaddps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[32][1]},
    {OP_vfmsubaddps,0x663a5e58,"vfmsubaddps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 33 */
    {OP_vfmsubaddpd,0x663a5f18,"vfmsubaddpd",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[33][1]},
    {OP_vfmsubaddpd,0x663a5f58,"vfmsubaddpd",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 34 */
    {OP_vfmaddps,0x663a6818,"vfmaddps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[34][1]},
    {OP_vfmaddps,0x663a6858,"vfmaddps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 35 */
    {OP_vfmaddpd,0x663a6918,"vfmaddpd",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[35][1]},
    {OP_vfmaddpd,0x663a6958,"vfmaddpd",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 36 */
    {OP_vfmaddss,0x663a6a18,"vfmaddss",Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[36][1]},
    {OP_vfmaddss,0x663a6a58,"vfmaddss",Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 37 */
    {OP_vfmaddsd,0x663a6b18,"vfmaddsd",Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[37][1]},
    {OP_vfmaddsd,0x663a6b58,"vfmaddsd",Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 38 */
    {OP_vfmsubps,0x663a6c18,"vfmsubps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[38][1]},
    {OP_vfmsubps,0x663a6c58,"vfmsubps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 39 */
    {OP_vfmsubpd,0x663a6d18,"vfmsubpd",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[39][1]},
    {OP_vfmsubpd,0x663a6d58,"vfmsubpd",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 40 */
    {OP_vfmsubss,0x663a6e18,"vfmsubss",Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[40][1]},
    {OP_vfmsubss,0x663a6e58,"vfmsubss",Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 41 */
    {OP_vfmsubsd,0x663a6f18,"vfmsubsd",Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[41][1]},
    {OP_vfmsubsd,0x663a6f58,"vfmsubsd",Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 42 */
    {OP_vfnmaddps,0x663a7818,"vfnmaddps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[42][1]},
    {OP_vfnmaddps,0x663a7858,"vfnmaddps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 43 */
    {OP_vfnmaddpd,0x663a7918,"vfnmaddpd",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[43][1]},
    {OP_vfnmaddpd,0x663a7958,"vfnmaddpd",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 44 */
    {OP_vfnmaddss,0x663a7a18,"vfnmaddss",Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[44][1]},
    {OP_vfnmaddss,0x663a7a58,"vfnmaddss",Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 45 */
    {OP_vfnmaddsd,0x663a7b18,"vfnmaddsd",Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[45][1]},
    {OP_vfnmaddsd,0x663a7b58,"vfnmaddsd",Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 46 */
    {OP_vfnmsubps,0x663a7c18,"vfnmsubps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[46][1]},
    {OP_vfnmsubps,0x663a7c58,"vfnmsubps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 47 */
    {OP_vfnmsubpd,0x663a7d18,"vfnmsubpd",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[47][1]},
    {OP_vfnmsubpd,0x663a7d58,"vfnmsubpd",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 48 */
    {OP_vfnmsubss,0x663a7e18,"vfnmsubss",Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[48][1]},
    {OP_vfnmsubss,0x663a7e58,"vfnmsubss",Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 49 */
    {OP_vfnmsubsd,0x663a7f18,"vfnmsubsd",Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[49][1]},
    {OP_vfnmsubsd,0x663a7f58,"vfnmsubsd",Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 50 */
    {OP_vpcmov,    0x08a218,"vpcmov",    Vvs,xx,Hvs,Wvs,Lvs,mrm|vex,x,tvexw[50][1]},
    {OP_vpcmov,    0x08a258,"vpcmov",    Vvs,xx,Hvs,Lvs,Wvs,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 51 */
    {OP_vpperm,    0x08a318,"vpperm",    Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,tvexw[51][1]},
    {OP_vpperm,    0x08a358,"vpperm",    Vdq,xx,Hdq,Ldq,Wdq,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 52 */
    {OP_vprotb,    0x099018,"vprotb",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[52][1]},
    {OP_vprotb,    0x099058,"vprotb",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 53 */
    {OP_vprotw,    0x099118,"vprotw",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[53][1]},
    {OP_vprotw,    0x099158,"vprotw",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 54 */
    {OP_vprotd,    0x099218,"vprotd",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[54][1]},
    {OP_vprotd,    0x099258,"vprotd",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 55 */
    {OP_vprotq,    0x099318,"vprotq",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[55][1]},
    {OP_vprotq,    0x099358,"vprotq",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 56 */
    {OP_vpshlb,    0x099418,"vpshlb",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[56][1]},
    {OP_vpshlb,    0x099458,"vpshlb",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 57 */
    {OP_vpshlw,    0x099518,"vpshlw",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[57][1]},
    {OP_vpshlw,    0x099558,"vpshlw",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 58 */
    {OP_vpshld,    0x099618,"vpshld",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[58][1]},
    {OP_vpshld,    0x099658,"vpshld",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 59 */
    {OP_vpshlq,    0x099718,"vpshlq",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[59][1]},
    {OP_vpshlq,    0x099758,"vpshlq",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 60 */
    {OP_vpshab,    0x099818,"vpshab",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[60][1]},
    {OP_vpshab,    0x099858,"vpshab",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 61 */
    {OP_vpshaw,    0x099918,"vpshaw",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[61][1]},
    {OP_vpshaw,    0x099958,"vpshaw",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 62 */
    {OP_vpshad,    0x099a18,"vpshad",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[62][1]},
    {OP_vpshad,    0x099a58,"vpshad",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 63 */
    {OP_vpshaq,    0x099b18,"vpshaq",    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[63][1]},
    {OP_vpshaq,    0x099b58,"vpshaq",    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 64 */
    {OP_vpermil2ps,0x663a4818,"vpermil2ps",Vvs,xx,Hvs,Wvs,Lvs,mrm|vex|reqp,x,tvexw[64][1]},
    {OP_vpermil2ps,0x663a4858,"vpermil2ps",Vvs,xx,Hvs,Lvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 65 */
    {OP_vpermil2pd,0x663a4918,"vpermil2pd",Vvs,xx,Hvs,Wvs,Lvs,mrm|vex|reqp,x,tvexw[65][1]},
    {OP_vpermil2pd,0x663a4958,"vpermil2pd",Vvs,xx,Hvs,Lvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 66 */
    /* XXX: OP_v*gather* raise #UD if any pair of the index, mask, or destination
     * registers are identical.  We don't bother trying to detect that.
     */
    {OP_vpgatherdd,0x66389018,"vpgatherdd",Vx,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tvexw[66][1]},
    {OP_vpgatherdq,0x66389058,"vpgatherdq",Vx,Hx,MVq,Hx,xx, mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 67 */
    {OP_vpgatherqd,0x66389118,"vpgatherdd",Vx,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tvexw[67][1]},
    {OP_vpgatherqq,0x66389158,"vpgatherdq",Vx,Hx,MVq,Hx,xx, mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 68 */
    {OP_vgatherdps,0x66389218,"vgatherdps",Vvs,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tvexw[68][1]},
    {OP_vgatherdpd,0x66389258,"vgatherdpd",Vvd,Hx,MVq,Hx,xx, mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 69 */
    {OP_vgatherqps,0x66389318,"vgatherqps",Vvs,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tvexw[69][1]},
    {OP_vgatherqpd,0x66389358,"vgatherqpd",Vvd,Hx,MVq,Hx,xx, mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 70 */
    {OP_vpmaskmovd,0x66388c18,"vpmaskmovd",Vx,xx,Hx,Mx,xx, mrm|vex|reqp|predcx,x,tvexw[71][0]},
    {OP_vpmaskmovq,0x66388c58,"vpmaskmovq",Vx,xx,Hx,Mx,xx, mrm|vex|reqp|predcx,x,tvexw[71][1]},
  }, { /* vex_W_ext 71 */
    /* Conditional store => predcx */
    {OP_vpmaskmovd,0x66388e18,"vpmaskmovd",Mx,xx,Vx,Hx,xx, mrm|vex|reqp|predcx,x,END_LIST},
    {OP_vpmaskmovq,0x66388e58,"vpmaskmovq",Mx,xx,Vx,Hx,xx, mrm|vex|reqp|predcx,x,END_LIST},
  }, { /* vex_W_ext 72 */
    {OP_vpsrlvd,0x66384518,"vpsrlvd",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,END_LIST},
    {OP_vpsrlvq,0x66384558,"vpsrlvq",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 73 */
    {OP_vpsllvd,0x66384718,"vpsllvd",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,END_LIST},
    {OP_vpsllvq,0x66384758,"vpsllvq",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,END_LIST},
  },
};

/****************************************************************************
 * XOP instructions
 *
 * Since large parts of the opcode space are empty, we save space by having
 * tables of 256 indices instead of tables of 256 instr_info_t structs.
 */
/* N.B.: all XOP 0x08 are assumed to have an immediate.  If this becomes
 * untrue we'll have to add an xop_8_extra[] table in decode_fast.c.
 */
const byte xop_8_index[256] = {
  /* 0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 0 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 1 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 2 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 3 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 4 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 5 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 6 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 7 */
     0, 0, 0, 0,  0, 1, 2, 3,  0, 0, 0, 0,  0, 0, 4, 5,  /* 8 */
     0, 0, 0, 0,  0, 6, 7, 8,  0, 0, 0, 0,  0, 0, 9,10,  /* 9 */
     0, 0,11,12,  0, 0,13, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* A */
     0, 0, 0, 0,  0, 0,14, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* B */
    15,16,17,18,  0, 0, 0, 0,  0, 0, 0, 0, 19,20,21,22,  /* C */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* D */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 23,24,25,26,  /* E */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0   /* F */
};
const byte xop_9_index[256] = {
  /* 0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
     0,58,59, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 0 */
     0, 0,61, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 1 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 2 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 3 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 4 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 5 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 6 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 7 */
    27,28,29,30,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 8 */
    31,32,33,34, 35,36,37,38, 39,40,41,42,  0, 0, 0, 0,  /* 9 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* A */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* B */
     0,43,44,45,  0, 0,46,47,  0, 0, 0,48,  0, 0, 0, 0,  /* C */
     0,49,50,51,  0, 0,52,53,  0, 0, 0,54,  0, 0, 0, 0,  /* D */
     0,55,56,57,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* E */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0   /* F */
};
/* N.B.: nothing here for initial XOP but upcoming TBM and LWP have opcodes here */
const byte xop_a_index[256] = {
  /* 0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 0 */
    60, 0,62, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 1 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 2 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 3 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 4 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 5 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 6 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 7 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 8 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 9 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* A */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* B */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* C */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* D */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* E */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0   /* F */
};

const instr_info_t xop_extensions[] = {
  {INVALID,     0x000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},              /* 0*/
  /* We are out of flags, and we want to share a lot of REQUIRES_VEX, so to
   * distinguish XOP we just rely on the XOP.map_select being disjoint from
   * the VEX.m-mmm field.
   */
  /* XOP.map_select = 0x08 */
  {OP_vpmacssww, 0x088518,"vpmacssww", Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 1*/
  {OP_vpmacsswd, 0x088618,"vpmacsswd", Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 2*/
  {OP_vpmacssdql,0x088718,"vpmacssdql",Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 3*/
  {OP_vpmacssdd, 0x088e18,"vpmacssdd", Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 4*/
  {OP_vpmacssdqh,0x088f18,"vpmacssdqh",Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 5*/
  {OP_vpmacsww,  0x089518,"vpmacsww",  Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 6*/
  {OP_vpmacswd,  0x089618,"vpmacswd",  Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 7*/
  {OP_vpmacsdql, 0x089718,"vpmacsdql", Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 8*/
  {OP_vpmacsdd,  0x089e18,"vpmacsdd",  Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 9*/
  {OP_vpmacsdqh, 0x089f18,"vpmacsdqh", Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /*10*/
  {VEX_W_EXT,    0x08a218, "(vex_W ext 50)", xx,xx,xx,xx,xx, mrm|vex, x,  50},  /*11*/
  {VEX_W_EXT,    0x08a318, "(vex_W ext 51)", xx,xx,xx,xx,xx, mrm|vex, x,  51},  /*12*/
  {OP_vpmadcsswd,0x08a618,"vpmadcsswd",Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /*13*/
  {OP_vpmadcswd, 0x08b618,"vpmadcswd", Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /*14*/
  {OP_vprotb,    0x08c018,"vprotb",    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[52][0]},/*15*/
  {OP_vprotw,    0x08c118,"vprotw",    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[53][0]},/*16*/
  {OP_vprotd,    0x08c218,"vprotd",    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[54][0]},/*17*/
  {OP_vprotq,    0x08c318,"vprotq",    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[55][0]},/*18*/
  {OP_vpcomb,    0x08cc18,"vpcomb",    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*19*/
  {OP_vpcomw,    0x08cd18,"vpcomw",    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*20*/
  {OP_vpcomd,    0x08ce18,"vpcomd",    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*21*/
  {OP_vpcomq,    0x08cf18,"vpcomq",    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*22*/
  {OP_vpcomub,   0x08ec18,"vpcomub",   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*23*/
  {OP_vpcomuw,   0x08ed18,"vpcomuw",   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*24*/
  {OP_vpcomud,   0x08ee18,"vpcomud",   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*25*/
  {OP_vpcomuq,   0x08ef18,"vpcomuq",   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*26*/
  /* XOP.map_select = 0x09 */
  {OP_vfrczps,   0x098018,"vfrczps",   Vvs,xx,Wvs,xx,xx,mrm|vex,x,END_LIST},    /*27*/
  {OP_vfrczpd,   0x098118,"vfrczpd",   Vvs,xx,Wvs,xx,xx,mrm|vex,x,END_LIST},    /*28*/
  {OP_vfrczss,   0x098218,"vfrczss",   Vss,xx,Wss,xx,xx,mrm|vex,x,END_LIST},    /*29*/
  {OP_vfrczsd,   0x098318,"vfrczsd",   Vsd,xx,Wsd,xx,xx,mrm|vex,x,END_LIST},    /*30*/
  {VEX_W_EXT,    0x099018, "(vex_W ext 52)", xx,xx,xx,xx,xx, mrm|vex, x,  52},  /*31*/
  {VEX_W_EXT,    0x099118, "(vex_W ext 53)", xx,xx,xx,xx,xx, mrm|vex, x,  53},  /*32*/
  {VEX_W_EXT,    0x099218, "(vex_W ext 54)", xx,xx,xx,xx,xx, mrm|vex, x,  54},  /*33*/
  {VEX_W_EXT,    0x099318, "(vex_W ext 55)", xx,xx,xx,xx,xx, mrm|vex, x,  55},  /*34*/
  {VEX_W_EXT,    0x099418, "(vex_W ext 56)", xx,xx,xx,xx,xx, mrm|vex, x,  56},  /*35*/
  {VEX_W_EXT,    0x099518, "(vex_W ext 57)", xx,xx,xx,xx,xx, mrm|vex, x,  57},  /*36*/
  {VEX_W_EXT,    0x099618, "(vex_W ext 58)", xx,xx,xx,xx,xx, mrm|vex, x,  58},  /*37*/
  {VEX_W_EXT,    0x099718, "(vex_W ext 59)", xx,xx,xx,xx,xx, mrm|vex, x,  59},  /*38*/
  {VEX_W_EXT,    0x099818, "(vex_W ext 60)", xx,xx,xx,xx,xx, mrm|vex, x,  60},  /*39*/
  {VEX_W_EXT,    0x099918, "(vex_W ext 61)", xx,xx,xx,xx,xx, mrm|vex, x,  61},  /*40*/
  {VEX_W_EXT,    0x099a18, "(vex_W ext 62)", xx,xx,xx,xx,xx, mrm|vex, x,  62},  /*41*/
  {VEX_W_EXT,    0x099b18, "(vex_W ext 63)", xx,xx,xx,xx,xx, mrm|vex, x,  63},  /*42*/
  {OP_vphaddbw,  0x09c118,"vphaddbw",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*43*/
  {OP_vphaddbd,  0x09c218,"vphaddbd",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*44*/
  {OP_vphaddbq,  0x09c318,"vphaddbq",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*45*/
  {OP_vphaddwd,  0x09c618,"vphaddwd",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*46*/
  {OP_vphaddwq,  0x09c718,"vphaddwq",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*47*/
  {OP_vphadddq,  0x09cb18,"vphadddq",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*48*/
  /* AMD decode table erroneously lists this as "vphaddubwd" */
  {OP_vphaddubw, 0x09d118,"vphaddubw",  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*49*/
  {OP_vphaddubd, 0x09d218,"vphaddubd",  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*50*/
  {OP_vphaddubq, 0x09d318,"vphaddubq",  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*51*/
  {OP_vphadduwd, 0x09d618,"vphadduwd",  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*52*/
  {OP_vphadduwq, 0x09d718,"vphadduwq",  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*53*/
  {OP_vphaddudq, 0x09db18,"vphaddudq",  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*54*/
  {OP_vphsubbw,  0x09e118,"vphsubbw",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*55*/
  {OP_vphsubwd,  0x09e218,"vphsubwd",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*56*/
  {OP_vphsubdq,  0x09e318,"vphsubdq",   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*57*/
  {EXTENSION,    0x090118, "(XOP group 1)", xx,xx, xx,xx,xx, mrm|vex, x, 27},   /*58*/
  {EXTENSION,    0x090218, "(XOP group 2)", xx,xx, xx,xx,xx, mrm|vex, x, 28},   /*59*/
  /* XOP.map_select = 0x0a */
  {OP_bextr,     0x0a1018, "bextr",  Gy,xx,Ey,Id,xx, mrm|vex, fW6, END_LIST},   /*60*/
  /* Later-added instrs, from various tables */
  {EXTENSION,    0x091218, "(XOP group 3)", xx,xx, xx,xx,xx, mrm|vex, x, 29},   /*61*/
  {EXTENSION,    0x0a1218, "(XOP group 4)", xx,xx, xx,xx,xx, mrm|vex, x, 30},   /*62*/
};

/****************************************************************************
 * String instructions that differ depending on rep/repne prefix
 *
 * Note that Intel manuals prior to May 2011 claim that for x64 the count
 * register for ins and outs is rcx by default, but for all other rep* is ecx.
 * The AMD manual, and experimental evidence, contradicts this and has rcx
 * as the default count register for all rep*.
 * Furthermore, the Intel manual implies that w/o rex.w edi/esi are used
 * rather than rdi/rsi: which again the AMD manual and experimental
 * evidence contradict.
 */
const instr_info_t rep_extensions[][4] = {
    /* FIXME: ins and outs access "I/O ports", are these memory addresses?
     * if so, change Ib to Ob and change dx to i_dx (move to dest for outs)
     */
  { /* rep extension 0 */
    {OP_ins,      0x6c0000, "ins",       Yb, axDI, dx, axDI, xx, no, fRD, END_LIST},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_ins,  0xf36c0000, "rep ins", Yb, axDI, dx, axDI, axCX, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf36c0000, "rep ins", axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 1 */
    {OP_ins,      0x6d0000, "ins",       Yz, axDI, dx, axDI, xx, no, fRD, tre[0][0]},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_ins,  0xf36d0000, "rep ins", Yz, axDI, dx, axDI, axCX, xop_next, fRD, tre[0][2]},
    {OP_CONTD,  0xf36d0000, "rep ins", axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 2 */
    {OP_outs,      0x6e0000, "outs",       axSI, xx, Xb, dx, axSI, no, fRD, END_LIST},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_outs,  0xf36e0000, "rep outs", axSI, axCX, Xb, dx, axSI, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf36e0000, "rep outs", xx, xx, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 3 */
    {OP_outs,      0x6f0000, "outs",       axSI, xx, Xz, dx, axSI, no, fRD, tre[2][0]},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_outs,  0xf36f0000, "rep outs", axSI, axCX, Xz, dx, axSI, xop_next, fRD, tre[2][2]},
    {OP_CONTD,  0xf36f0000, "rep outs", xx, xx, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 4 */
    {OP_movs,      0xa40000, "movs",       Yb, axSI, Xb, axSI, axDI, xop_next, fRD, END_LIST},
    {OP_CONTD,      0xa40000, "movs",       axDI, xx, xx, xx, xx, no, fRD, END_LIST},
    {OP_rep_movs,  0xf3a40000, "rep movs", Yb, axSI, Xb, axSI, axDI, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf3a40000, "rep movs", axDI, axCX, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 5 */
    {OP_movs,      0xa50000, "movs",       Yv, axSI, Xv, axSI, axDI, xop_next, fRD, tre[4][0]},
    {OP_CONTD,      0xa50000, "movs",       axDI, xx, xx, xx, xx, no, fRD, END_LIST},
    {OP_rep_movs,  0xf3a50000, "rep movs", Yv, axSI, Xv, axSI, axDI, xop_next, fRD, tre[4][2]},
    {OP_CONTD,  0xf3a50000, "rep movs", axDI, axCX, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 6 */
    {OP_stos,      0xaa0000, "stos",       Yb, axDI, al, axDI, xx, no, fRD, END_LIST},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_stos,  0xf3aa0000, "rep stos", Yb, axDI, al, axDI, axCX, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf3aa0000, "rep stos", axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 7 */
    {OP_stos,      0xab0000, "stos",       Yv, axDI, eAX, axDI, xx, no, fRD, tre[6][0]},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_stos,  0xf3ab0000, "rep stos", Yv, axDI, eAX, axDI, axCX, xop_next, fRD, tre[6][2]},
    {OP_CONTD,  0xf3ab0000, "rep stos", axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 8 */
    {OP_lods,      0xac0000, "lods",       al, axSI, Xb, axSI, xx, no, fRD, END_LIST},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_lods,  0xf3ac0000, "rep lods", al, axSI, Xb, axSI, axCX, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf3ac0000, "rep lods", axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 9 */
    {OP_lods,      0xad0000, "lods",       eAX, axSI, Xv, axSI, xx, no, fRD, tre[8][0]},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_lods,  0xf3ad0000, "rep lods", eAX, axSI, Xv, axSI, axCX, xop_next, fRD, tre[8][2]},
    {OP_CONTD,  0xf3ad0000, "rep lods", axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
};

const instr_info_t repne_extensions[][6] = {
  { /* repne extension 0 */
    {OP_cmps,       0xa60000, "cmps",         axSI, axDI, Xb, Yb, axSI, xop_next, (fW6|fRD), END_LIST},
    {OP_CONTD,      0xa60000, "cmps",         xx, xx, axDI, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_rep_cmps,   0xf3a60000, "rep cmps",   axSI, axDI, Xb, Yb, axSI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf3a60000, "rep cmps",   axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_cmps, 0xf2a60000, "repne cmps", axSI, axDI, Xb, Yb, axSI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf2a60000, "repne cmps", axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
  },
  { /* repne extension 1 */
    {OP_cmps,       0xa70000, "cmps",         axSI, axDI, Xv, Yv, axSI, xop_next, (fW6|fRD), tne[0][0]},
    {OP_CONTD,      0xa70000, "cmps",         xx, xx, axDI, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_rep_cmps,   0xf3a70000, "rep cmps",   axSI, axDI, Xv, Yv, axSI, xop_next, (fW6|fRD|fRZ), tne[0][2]},
    {OP_CONTD,      0xf3a70000, "rep cmps",   axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_cmps, 0xf2a70000, "repne cmps", axSI, axDI, Xv, Yv, axSI, xop_next, (fW6|fRD|fRZ), tne[0][4]},
    {OP_CONTD,      0xf2a70000, "repne cmps", axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
  },
  { /* repne extension 2 */
    {OP_scas,       0xae0000, "scas",         axDI, xx, Yb, al, axDI, no, (fW6|fRD), END_LIST},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_scas,   0xf3ae0000, "rep scas",   axDI, axCX, Yb, al, axDI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf3ae0000, "rep scas",   xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_scas, 0xf2ae0000, "repne scas", axDI, axCX, Yb, al, axDI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf2ae0000, "repne scas", xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
  },
  { /* repne extension 3 */
    {OP_scas,       0xaf0000, "scas",         axDI, xx, Yv, eAX, axDI, no, (fW6|fRD), tne[2][0]},
    {INVALID,   0x00000000, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_scas,   0xf3af0000, "rep scas",   axDI, axCX, Yv, eAX, axDI, xop_next, (fW6|fRD|fRZ), tne[2][2]},
    {OP_CONTD,      0xf3af0000, "rep scas",   xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_scas, 0xf2af0000, "repne scas", axDI, axCX, Yv, eAX, axDI, xop_next, (fW6|fRD|fRZ), tne[2][4]},
    {OP_CONTD,      0xf2af0000, "repne scas", xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
  }
};

/****************************************************************************
 * Float instructions with ModR/M from 0x00 to 0xbf
 * This is from Tables A-7, A-9, A-11, A-13, A-15, A-17, A-19, A-21
 * I've added my own symbol '+' to indicate a float, and:
 *   'x' to indicate extended real (80 bits)
 *   'y' to indicate 14/28 byte value in memory
 *   'z' to indicate 98/108 byte value in memory
 */
/* FIXME: I ignore fp stack changes, should we model that? */
const instr_info_t float_low_modrm[] = {
  /* d8 */
  {OP_fadd,  0xd80020, "fadd",  st0, xx, Kd, st0, xx, mrm, x, tfl[0x20]}, /* 00 */
  {OP_fmul,  0xd80021, "fmul",  st0, xx, Kd, st0, xx, mrm, x, tfl[0x21]},
  {OP_fcom,  0xd80022, "fcom",  xx, xx, Kd, st0, xx, mrm, x, tfl[0x22]},
  {OP_fcomp, 0xd80023, "fcomp", xx, xx, Kd, st0, xx, mrm, x, tfl[0x23]},
  {OP_fsub,  0xd80024, "fsub",  st0, xx, Kd, st0, xx, mrm, x, tfl[0x24]},
  {OP_fsubr, 0xd80025, "fsubr", st0, xx, Kd, st0, xx, mrm, x, tfl[0x25]},
  {OP_fdiv,  0xd80026, "fdiv",  st0, xx, Kd, st0, xx, mrm, x, tfl[0x26]},
  {OP_fdivr, 0xd80027, "fdivr", st0, xx, Kd, st0, xx, mrm, x, tfl[0x27]},
  /*  d9 */
  {OP_fld,    0xd90020, "fld",    st0, xx, Kd, xx, xx, mrm, x, tfl[0x1d]}, /* 08 */
  {INVALID,   0xd90021, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fst,    0xd90022, "fst",    Kd, xx, st0, xx, xx, mrm, x, tfl[0x2a]},
  {OP_fstp,   0xd90023, "fstp",   Kd, xx, st0, xx, xx, mrm, x, tfl[0x1f]},
  {OP_fldenv, 0xd90024, "fldenv", xx, xx, Ky, xx, xx, mrm, x, END_LIST},
  {OP_fldcw,  0xd90025, "fldcw",  xx, xx, Kw, xx, xx, mrm, x, END_LIST},
  {OP_fnstenv, 0xd90026, "fnstenv", Ky, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, this is "fstenv"*/
  {OP_fnstcw,  0xd90027, "fnstcw",  Kw, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, this is "fstcw"*/
  /* da */
  {OP_fiadd,  0xda0020, "fiadd",  st0, xx, Md, st0, xx, mrm, x, tfl[0x30]}, /* 10 */
  {OP_fimul,  0xda0021, "fimul",  st0, xx, Md, st0, xx, mrm, x, tfl[0x31]},
  {OP_ficom,  0xda0022, "ficom",  st0, xx, Md, st0, xx, mrm, x, tfl[0x32]},
  {OP_ficomp, 0xda0023, "ficomp", st0, xx, Md, st0, xx, mrm, x, tfl[0x33]},
  {OP_fisub,  0xda0024, "fisub",  st0, xx, Md, st0, xx, mrm, x, tfl[0x34]},
  {OP_fisubr, 0xda0025, "fisubr", st0, xx, Md, st0, xx, mrm, x, tfl[0x35]},
  {OP_fidiv,  0xda0026, "fidiv",  st0, xx, Md, st0, xx, mrm, x, tfl[0x36]},
  {OP_fidivr, 0xda0027, "fidivr", st0, xx, Md, st0, xx, mrm, x, tfl[0x37]},
  /* db */
  {OP_fild,  0xdb0020, "fild",  st0, xx, Md, xx, xx, mrm, x, tfl[0x38]}, /* 18 */
  {OP_fisttp, 0xdb0021, "fisttp",  Md, xx, st0, xx, xx, no, x, tfl[0x39]},
  {OP_fist,  0xdb0022, "fist",  Md, xx, st0, xx, xx, mrm, x, tfl[0x3a]},
  {OP_fistp, 0xdb0023, "fistp", Md, xx, st0, xx, xx, mrm, x, tfl[0x3b]},
  {INVALID,  0xdb0024, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fld,   0xdb0025, "fld",   st0, xx, Kx, xx, xx, mrm, x, tfl[0x28]},
  {INVALID,  0xdb0026, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fstp,  0xdb0027, "fstp",  Kx, xx, st0, xx, xx, mrm, x, tfl[0x2b]},
  /* dc */
  {OP_fadd,  0xdc0020, "fadd",  st0, xx, Kq, st0, xx, mrm, x, tfh[0][0x00]}, /* 20 */
  {OP_fmul,  0xdc0021, "fmul",  st0, xx, Kq, st0, xx, mrm, x, tfh[0][0x08]},
  {OP_fcom,  0xdc0022, "fcom",  xx, xx, Kq, st0, xx, mrm, x, tfh[0][0x10]},
  {OP_fcomp, 0xdc0023, "fcomp", xx, xx, Kq, st0, xx, mrm, x, tfh[0][0x18]},
  {OP_fsub,  0xdc0024, "fsub",  st0, xx, Kq, st0, xx, mrm, x, tfh[0][0x20]},
  {OP_fsubr, 0xdc0025, "fsubr", st0, xx, Kq, st0, xx, mrm, x, tfh[0][0x28]},
  {OP_fdiv,  0xdc0026, "fdiv",  st0, xx, Kq, st0, xx, mrm, x, tfh[0][0x30]},
  {OP_fdivr, 0xdc0027, "fdivr", st0, xx, Kq, st0, xx, mrm, x, tfh[0][0x38]},
  /* dd */
  {OP_fld,   0xdd0020, "fld",    st0, xx, Kq, xx, xx, mrm, x, tfh[1][0x00]}, /* 28 */
  {OP_fisttp, 0xdd0021, "fisttp",  Mq, xx, st0, xx, xx, no, x, tfl[0x19]},
  {OP_fst,   0xdd0022, "fst",    Kq, xx, st0, xx, xx, mrm, x, tfh[5][0x10]},
  {OP_fstp,  0xdd0023, "fstp",   Kq, xx, st0, xx, xx, mrm, x, tfh[5][0x18]},
  {OP_frstor,0xdd0024, "frstor", xx, xx, Kz, xx, xx, mrm, x, END_LIST},
  {INVALID,  0xdd0025, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fnsave, 0xdd0026, "fnsave",  Kz, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME:w/ preceding fwait instr, this is "fsave"*/
  {OP_fnstsw, 0xdd0027, "fnstsw",  Kw, xx, xx, xx, xx, mrm, x, tfh[7][0x20]},/*FIXME:w/ preceding fwait instr, this is "fstsw"*/
  /* de */
  {OP_fiadd,  0xde0020, "fiadd",  st0, xx, Kw, st0, xx, mrm, x, END_LIST}, /* 30 */
  {OP_fimul,  0xde0021, "fimul",  st0, xx, Kw, st0, xx, mrm, x, END_LIST},
  {OP_ficom,  0xde0022, "ficom",  xx, xx, Kw, st0, xx, mrm, x, END_LIST},
  {OP_ficomp, 0xde0023, "ficomp", xx, xx, Kw, st0, xx, mrm, x, END_LIST},
  {OP_fisub,  0xde0024, "fisub",  st0, xx, Kw, st0, xx, mrm, x, END_LIST},
  {OP_fisubr, 0xde0025, "fisubr", st0, xx, Kw, st0, xx, mrm, x, END_LIST},
  {OP_fidiv,  0xde0026, "fidiv",  st0, xx, Kw, st0, xx, mrm, x, END_LIST},
  {OP_fidivr, 0xde0027, "fidivr", st0, xx, Kw, st0, xx, mrm, x, END_LIST},
  /* df */
  {OP_fild,   0xdf0020, "fild",    st0, xx, Kw, xx, xx, mrm, x, tfl[0x3d]}, /* 38 */
  {OP_fisttp, 0xdf0021, "fisttp",  Mw, xx, st0, xx, xx, no, x, END_LIST},
  {OP_fist,   0xdf0022, "fist",    Kw, xx, st0, xx, xx, mrm, x, END_LIST},
  {OP_fistp,  0xdf0023, "fistp",   Kw, xx, st0, xx, xx, mrm, x, tfl[0x3f]},
  {OP_fbld,   0xdf0024, "fbld",    st0, xx, Kx, xx, xx, mrm, x, END_LIST},
  {OP_fild,   0xdf0025, "fild",    st0, xx, Kq, xx, xx, mrm, x, END_LIST},
  {OP_fbstp,  0xdf0026, "fbstp",   Kx, xx, st0, xx, xx, mrm, x, END_LIST},
  {OP_fistp,  0xdf0027, "fistp",   Kq, xx, st0, xx, xx, mrm, x, END_LIST},
};

/****************************************************************************
 * Float instructions with ModR/M above 0xbf
 * This is from Tables A-8, A-10, A-12, A-14, A-16, A-18, A-20, A-22
 */
const instr_info_t float_high_modrm[][64] = {
    { /* d8 = [0] */
        {OP_fadd, 0xd8c010, "fadd", st0, xx, st0, st0, xx, mrm, x, tfh[0][0x01]}, /* c0 = [0x00] */
        {OP_fadd, 0xd8c110, "fadd", st0, xx, st1, st0, xx, mrm, x, tfh[0][0x02]},
        {OP_fadd, 0xd8c210, "fadd", st0, xx, st2, st0, xx, mrm, x, tfh[0][0x03]},
        {OP_fadd, 0xd8c310, "fadd", st0, xx, st3, st0, xx, mrm, x, tfh[0][0x04]},
        {OP_fadd, 0xd8c410, "fadd", st0, xx, st4, st0, xx, mrm, x, tfh[0][0x05]},
        {OP_fadd, 0xd8c510, "fadd", st0, xx, st5, st0, xx, mrm, x, tfh[0][0x06]},
        {OP_fadd, 0xd8c610, "fadd", st0, xx, st6, st0, xx, mrm, x, tfh[0][0x07]},
        {OP_fadd, 0xd8c710, "fadd", st0, xx, st7, st0, xx, mrm, x, tfh[4][0x00]},
        {OP_fmul, 0xd8c810, "fmul", st0, xx, st0, st0, xx, mrm, x, tfh[0][0x09]}, /* c8 = [0x08] */
        {OP_fmul, 0xd8c910, "fmul", st0, xx, st1, st0, xx, mrm, x, tfh[0][0x0a]},
        {OP_fmul, 0xd8ca10, "fmul", st0, xx, st2, st0, xx, mrm, x, tfh[0][0x0b]},
        {OP_fmul, 0xd8cb10, "fmul", st0, xx, st3, st0, xx, mrm, x, tfh[0][0x0c]},
        {OP_fmul, 0xd8cc10, "fmul", st0, xx, st4, st0, xx, mrm, x, tfh[0][0x0d]},
        {OP_fmul, 0xd8cd10, "fmul", st0, xx, st5, st0, xx, mrm, x, tfh[0][0x0e]},
        {OP_fmul, 0xd8ce10, "fmul", st0, xx, st6, st0, xx, mrm, x, tfh[0][0x0f]},
        {OP_fmul, 0xd8cf10, "fmul", st0, xx, st7, st0, xx, mrm, x, tfh[4][0x08]},
        {OP_fcom, 0xd8d010, "fcom", xx, xx, st0, st0, xx, mrm, x, tfh[0][0x11]}, /* d0 = [0x10] */
        {OP_fcom, 0xd8d110, "fcom", xx, xx, st0, st1, xx, mrm, x, tfh[0][0x12]},
        {OP_fcom, 0xd8d210, "fcom", xx, xx, st0, st2, xx, mrm, x, tfh[0][0x13]},
        {OP_fcom, 0xd8d310, "fcom", xx, xx, st0, st3, xx, mrm, x, tfh[0][0x14]},
        {OP_fcom, 0xd8d410, "fcom", xx, xx, st0, st4, xx, mrm, x, tfh[0][0x15]},
        {OP_fcom, 0xd8d510, "fcom", xx, xx, st0, st5, xx, mrm, x, tfh[0][0x16]},
        {OP_fcom, 0xd8d610, "fcom", xx, xx, st0, st6, xx, mrm, x, tfh[0][0x17]},
        {OP_fcom, 0xd8d710, "fcom", xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xd8d810, "fcomp", xx, xx, st0, st0, xx, mrm, x, tfh[0][0x19]}, /* d8 = [0x18] */
        {OP_fcomp, 0xd8d910, "fcomp", xx, xx, st0, st1, xx, mrm, x, tfh[0][0x1a]},
        {OP_fcomp, 0xd8da10, "fcomp", xx, xx, st0, st2, xx, mrm, x, tfh[0][0x1b]},
        {OP_fcomp, 0xd8db10, "fcomp", xx, xx, st0, st3, xx, mrm, x, tfh[0][0x1c]},
        {OP_fcomp, 0xd8dc10, "fcomp", xx, xx, st0, st4, xx, mrm, x, tfh[0][0x1d]},
        {OP_fcomp, 0xd8dd10, "fcomp", xx, xx, st0, st5, xx, mrm, x, tfh[0][0x1e]},
        {OP_fcomp, 0xd8de10, "fcomp", xx, xx, st0, st6, xx, mrm, x, tfh[0][0x1f]},
        {OP_fcomp, 0xd8df10, "fcomp", xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsub, 0xd8e010, "fsub", st0, xx, st0, st0, xx, mrm, x, tfh[0][0x21]}, /* e0 = [0x20] */
        {OP_fsub, 0xd8e110, "fsub", st0, xx, st1, st0, xx, mrm, x, tfh[0][0x22]},
        {OP_fsub, 0xd8e210, "fsub", st0, xx, st2, st0, xx, mrm, x, tfh[0][0x23]},
        {OP_fsub, 0xd8e310, "fsub", st0, xx, st3, st0, xx, mrm, x, tfh[0][0x24]},
        {OP_fsub, 0xd8e410, "fsub", st0, xx, st4, st0, xx, mrm, x, tfh[0][0x25]},
        {OP_fsub, 0xd8e510, "fsub", st0, xx, st5, st0, xx, mrm, x, tfh[0][0x26]},
        {OP_fsub, 0xd8e610, "fsub", st0, xx, st6, st0, xx, mrm, x, tfh[0][0x27]},
        {OP_fsub, 0xd8e710, "fsub", st0, xx, st7, st0, xx, mrm, x, tfh[4][0x28]},
        {OP_fsubr, 0xd8e810, "fsubr", st0, xx, st0, st0, xx, mrm, x, tfh[0][0x29]}, /* e8 = [0x28] */
        {OP_fsubr, 0xd8e910, "fsubr", st0, xx, st1, st0, xx, mrm, x, tfh[0][0x2a]},
        {OP_fsubr, 0xd8ea10, "fsubr", st0, xx, st2, st0, xx, mrm, x, tfh[0][0x2b]},
        {OP_fsubr, 0xd8eb10, "fsubr", st0, xx, st3, st0, xx, mrm, x, tfh[0][0x2c]},
        {OP_fsubr, 0xd8ec10, "fsubr", st0, xx, st4, st0, xx, mrm, x, tfh[0][0x2d]},
        {OP_fsubr, 0xd8ed10, "fsubr", st0, xx, st5, st0, xx, mrm, x, tfh[0][0x2e]},
        {OP_fsubr, 0xd8ee10, "fsubr", st0, xx, st6, st0, xx, mrm, x, tfh[0][0x2f]},
        {OP_fsubr, 0xd8ef10, "fsubr", st0, xx, st7, st0, xx, mrm, x, tfh[4][0x20]},
        {OP_fdiv, 0xd8f010, "fdiv", st0, xx, st0, st0, xx, mrm, x, tfh[0][0x31]}, /* f0 = [0x30] */
        {OP_fdiv, 0xd8f110, "fdiv", st0, xx, st1, st0, xx, mrm, x, tfh[0][0x32]},
        {OP_fdiv, 0xd8f210, "fdiv", st0, xx, st2, st0, xx, mrm, x, tfh[0][0x33]},
        {OP_fdiv, 0xd8f310, "fdiv", st0, xx, st3, st0, xx, mrm, x, tfh[0][0x34]},
        {OP_fdiv, 0xd8f410, "fdiv", st0, xx, st4, st0, xx, mrm, x, tfh[0][0x35]},
        {OP_fdiv, 0xd8f510, "fdiv", st0, xx, st5, st0, xx, mrm, x, tfh[0][0x36]},
        {OP_fdiv, 0xd8f610, "fdiv", st0, xx, st6, st0, xx, mrm, x, tfh[0][0x37]},
        {OP_fdiv, 0xd8f710, "fdiv", st0, xx, st7, st0, xx, mrm, x, tfh[4][0x38]},
        {OP_fdivr, 0xd8f810, "fdivr", st0, xx, st0, st0, xx, mrm, x, tfh[0][0x39]}, /* f8 = [0x38] */
        {OP_fdivr, 0xd8f910, "fdivr", st0, xx, st1, st0, xx, mrm, x, tfh[0][0x3a]},
        {OP_fdivr, 0xd8fa10, "fdivr", st0, xx, st2, st0, xx, mrm, x, tfh[0][0x3b]},
        {OP_fdivr, 0xd8fb10, "fdivr", st0, xx, st3, st0, xx, mrm, x, tfh[0][0x3c]},
        {OP_fdivr, 0xd8fc10, "fdivr", st0, xx, st4, st0, xx, mrm, x, tfh[0][0x3d]},
        {OP_fdivr, 0xd8fd10, "fdivr", st0, xx, st5, st0, xx, mrm, x, tfh[0][0x3e]},
        {OP_fdivr, 0xd8fe10, "fdivr", st0, xx, st6, st0, xx, mrm, x, tfh[0][0x3f]},
        {OP_fdivr, 0xd8ff10, "fdivr", st0, xx, st7, st0, xx, mrm, x, tfh[4][0x30]},
   },
    { /* d9 = [1] */
        {OP_fld, 0xd9c010, "fld", st0, xx, st0, xx, xx, mrm, x, tfh[1][0x01]}, /* c0 = [0x00] */
        {OP_fld, 0xd9c110, "fld", st0, xx, st1, xx, xx, mrm, x, tfh[1][0x02]},
        {OP_fld, 0xd9c210, "fld", st0, xx, st2, xx, xx, mrm, x, tfh[1][0x03]},
        {OP_fld, 0xd9c310, "fld", st0, xx, st3, xx, xx, mrm, x, tfh[1][0x04]},
        {OP_fld, 0xd9c410, "fld", st0, xx, st4, xx, xx, mrm, x, tfh[1][0x05]},
        {OP_fld, 0xd9c510, "fld", st0, xx, st5, xx, xx, mrm, x, tfh[1][0x06]},
        {OP_fld, 0xd9c610, "fld", st0, xx, st6, xx, xx, mrm, x, tfh[1][0x07]},
        {OP_fld, 0xd9c710, "fld", st0, xx, st7, xx, xx, mrm, x, END_LIST},
        {OP_fxch, 0xd9c810, "fxch", st0, st0, st0, st0, xx, mrm, x, tfh[1][0x09]}, /* c8 = [0x08] */
        {OP_fxch, 0xd9c910, "fxch", st0, st1, st0, st1, xx, mrm, x, tfh[1][0x0a]},
        {OP_fxch, 0xd9ca10, "fxch", st0, st2, st0, st2, xx, mrm, x, tfh[1][0x0b]},
        {OP_fxch, 0xd9cb10, "fxch", st0, st3, st0, st3, xx, mrm, x, tfh[1][0x0c]},
        {OP_fxch, 0xd9cc10, "fxch", st0, st4, st0, st4, xx, mrm, x, tfh[1][0x0d]},
        {OP_fxch, 0xd9cd10, "fxch", st0, st5, st0, st5, xx, mrm, x, tfh[1][0x0e]},
        {OP_fxch, 0xd9ce10, "fxch", st0, st6, st0, st6, xx, mrm, x, tfh[1][0x0f]},
        {OP_fxch, 0xd9cf10, "fxch", st0, st7, st0, st7, xx, mrm, x, END_LIST},
        {OP_fnop, 0xd9d010, "fnop", xx, xx, xx, xx, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {INVALID, 0xd9d110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        /* Undocumented.  On sandpile.org as "fstp1".  We assume an alias for fstp
         * and do not include in the encode chain.
         */
        {OP_fstp, 0xd9d810, "fstp", st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* d8 = [0x18] */
        {OP_fstp, 0xd9d910, "fstp", st1, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9da10, "fstp", st2, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9db10, "fstp", st3, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9dc10, "fstp", st4, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9dd10, "fstp", st5, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9de10, "fstp", st6, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9df10, "fstp", st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fchs,   0xd9e010, "fchs",   st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* e0 = [0x20] */
        {OP_fabs,   0xd9e110, "fabs",   st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {INVALID,   0xd9e210, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
        {INVALID,   0xd9e310, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
        {OP_ftst,   0xd9e410, "ftst",   st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fxam,   0xd9e510, "fxam",   xx, xx, st0, xx, xx, mrm, x, END_LIST},
        {INVALID,   0xd9e610, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
        {INVALID,   0xd9e710, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
        {OP_fld1,   0xd9e810, "fld1",   st0, xx, cF, xx, xx, mrm, x, END_LIST}, /* e8 = [0x28] */
        {OP_fldl2t, 0xd9e910, "fldl2t", st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldl2e, 0xd9ea10, "fldl2e", st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldpi,  0xd9eb10, "fldpi",  st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldlg2, 0xd9ec10, "fldlg2", st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldln2, 0xd9ed10, "fldln2", st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldz,   0xd9ee10, "fldz",   st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {INVALID,   0xd9ef10, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
        {OP_f2xm1,  0xd9f010, "f2xm1",  st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* f0 = [0x30] */
        {OP_fyl2x,  0xd9f110, "fyl2x",  st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fptan,  0xd9f210, "fptan",  st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fpatan, 0xd9f310, "fpatan", st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fxtract,0xd9f410, "fxtract",st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fprem1, 0xd9f510, "fprem1", st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fdecstp,0xd9f610, "fdecstp", xx, xx, xx, xx, xx, mrm, x, END_LIST},
        {OP_fincstp,0xd9f710, "fincstp", xx, xx, xx, xx, xx, mrm, x, END_LIST},
        {OP_fprem,  0xd9f810, "fprem",  st0, st1, st0, st1, xx, mrm, x, END_LIST}, /* f8 = [0x38] */
        {OP_fyl2xp1,0xd9f910, "fyl2xp1",st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fsqrt,  0xd9fa10, "fsqrt",  st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fsincos,0xd9fb10, "fsincos",st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_frndint,0xd9fc10, "frndint",st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fscale, 0xd9fd10, "fscale", st0, xx, st1, st0, xx, mrm, x, END_LIST},
        {OP_fsin,   0xd9fe10, "fsin",   st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fcos,   0xd9ff10, "fcos",   st0, xx, st0, xx, xx, mrm, x, END_LIST},
   },
    { /* da = [2] */
        {OP_fcmovb, 0xdac010, "fcmovb", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x01]}, /* c0 = [0x00] */
        {OP_fcmovb, 0xdac110, "fcmovb", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x02]},
        {OP_fcmovb, 0xdac210, "fcmovb", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x03]},
        {OP_fcmovb, 0xdac310, "fcmovb", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x04]},
        {OP_fcmovb, 0xdac410, "fcmovb", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x05]},
        {OP_fcmovb, 0xdac510, "fcmovb", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x06]},
        {OP_fcmovb, 0xdac610, "fcmovb", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x07]},
        {OP_fcmovb, 0xdac710, "fcmovb", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmove, 0xdac810, "fcmove", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x09]}, /* c8 = [0x08] */
        {OP_fcmove, 0xdac910, "fcmove", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0a]},
        {OP_fcmove, 0xdaca10, "fcmove", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0b]},
        {OP_fcmove, 0xdacb10, "fcmove", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0c]},
        {OP_fcmove, 0xdacc10, "fcmove", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0d]},
        {OP_fcmove, 0xdacd10, "fcmove", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0e]},
        {OP_fcmove, 0xdace10, "fcmove", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0f]},
        {OP_fcmove, 0xdacf10, "fcmove", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovbe, 0xdad010, "fcmovbe", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x11]}, /* d0 = [0x10] */
        {OP_fcmovbe, 0xdad110, "fcmovbe", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x12]},
        {OP_fcmovbe, 0xdad210, "fcmovbe", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x13]},
        {OP_fcmovbe, 0xdad310, "fcmovbe", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x14]},
        {OP_fcmovbe, 0xdad410, "fcmovbe", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x15]},
        {OP_fcmovbe, 0xdad510, "fcmovbe", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x16]},
        {OP_fcmovbe, 0xdad610, "fcmovbe", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x17]},
        {OP_fcmovbe, 0xdad710, "fcmovbe", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovu, 0xdad810, "fcmovu", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x19]}, /* d8 = [0x18] */
        {OP_fcmovu, 0xdad910, "fcmovu", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1a]},
        {OP_fcmovu, 0xdada10, "fcmovu", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1b]},
        {OP_fcmovu, 0xdadb10, "fcmovu", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1c]},
        {OP_fcmovu, 0xdadc10, "fcmovu", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1d]},
        {OP_fcmovu, 0xdadd10, "fcmovu", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1e]},
        {OP_fcmovu, 0xdade10, "fcmovu", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1f]},
        {OP_fcmovu, 0xdadf10, "fcmovu", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {INVALID, 0xdae010, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* e0 = [0x20] */
        {INVALID, 0xdae110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae810, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* e8 = [0x28] */
        {OP_fucompp, 0xdae910, "fucompp", xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {INVALID, 0xdaea10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaeb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaec10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaed10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaee10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaef10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf010, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* f0 = [0x30] */
        {INVALID, 0xdaf110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf810, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xdaf910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaff10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
   },
    { /* db = [3] */
        {OP_fcmovnb, 0xdbc010, "fcmovnb", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x01]}, /* c0 = [0x00] */
        {OP_fcmovnb, 0xdbc110, "fcmovnb", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x02]},
        {OP_fcmovnb, 0xdbc210, "fcmovnb", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x03]},
        {OP_fcmovnb, 0xdbc310, "fcmovnb", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x04]},
        {OP_fcmovnb, 0xdbc410, "fcmovnb", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x05]},
        {OP_fcmovnb, 0xdbc510, "fcmovnb", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x06]},
        {OP_fcmovnb, 0xdbc610, "fcmovnb", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x07]},
        {OP_fcmovnb, 0xdbc710, "fcmovnb", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovne, 0xdbc810, "fcmovne", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x09]}, /* c8 = [0x08] */
        {OP_fcmovne, 0xdbc910, "fcmovne", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0a]},
        {OP_fcmovne, 0xdbca10, "fcmovne", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0b]},
        {OP_fcmovne, 0xdbcb10, "fcmovne", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0c]},
        {OP_fcmovne, 0xdbcc10, "fcmovne", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0d]},
        {OP_fcmovne, 0xdbcd10, "fcmovne", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0e]},
        {OP_fcmovne, 0xdbce10, "fcmovne", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0f]},
        {OP_fcmovne, 0xdbcf10, "fcmovne", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovnbe, 0xdbd010, "fcmovnbe", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x12]}, /* d0 = [0x10] */
        {OP_fcmovnbe, 0xdbd110, "fcmovnbe", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x12]},
        {OP_fcmovnbe, 0xdbd210, "fcmovnbe", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x13]},
        {OP_fcmovnbe, 0xdbd310, "fcmovnbe", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x14]},
        {OP_fcmovnbe, 0xdbd410, "fcmovnbe", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x15]},
        {OP_fcmovnbe, 0xdbd510, "fcmovnbe", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x16]},
        {OP_fcmovnbe, 0xdbd610, "fcmovnbe", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x17]},
        {OP_fcmovnbe, 0xdbd710, "fcmovnbe", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovnu, 0xdbd810, "fcmovnu", st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x19]}, /* d8 = [0x18] */
        {OP_fcmovnu, 0xdbd910, "fcmovnu", st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1a]},
        {OP_fcmovnu, 0xdbda10, "fcmovnu", st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1b]},
        {OP_fcmovnu, 0xdbdb10, "fcmovnu", st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1c]},
        {OP_fcmovnu, 0xdbdc10, "fcmovnu", st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1d]},
        {OP_fcmovnu, 0xdbdd10, "fcmovnu", st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1e]},
        {OP_fcmovnu, 0xdbde10, "fcmovnu", st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1f]},
        {OP_fcmovnu, 0xdbdf10, "fcmovnu", st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {INVALID, 0xdbe010, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* e0 = [0x20] */
        {INVALID, 0xdbe110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {OP_fnclex, 0xdbe210, "fnclex", xx, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, called "fclex"*/
        {OP_fninit, 0xdbe310, "fninit", xx, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, called "finit"*/
        {INVALID, 0xdbe410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbe510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbe610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbe710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {OP_fucomi, 0xdbe810, "fucomi", xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x29]}, /* e8 = [0x28] */
        {OP_fucomi, 0xdbe910, "fucomi", xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2a]},
        {OP_fucomi, 0xdbea10, "fucomi", xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2b]},
        {OP_fucomi, 0xdbeb10, "fucomi", xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2c]},
        {OP_fucomi, 0xdbec10, "fucomi", xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2d]},
        {OP_fucomi, 0xdbed10, "fucomi", xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2e]},
        {OP_fucomi, 0xdbee10, "fucomi", xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2f]},
        {OP_fucomi, 0xdbef10, "fucomi", xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {OP_fcomi, 0xdbf010, "fcomi", xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x31]}, /* f0 = [0x30] */
        {OP_fcomi, 0xdbf110, "fcomi", xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x32]},
        {OP_fcomi, 0xdbf210, "fcomi", xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x33]},
        {OP_fcomi, 0xdbf310, "fcomi", xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x34]},
        {OP_fcomi, 0xdbf410, "fcomi", xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x35]},
        {OP_fcomi, 0xdbf510, "fcomi", xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x36]},
        {OP_fcomi, 0xdbf610, "fcomi", xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x37]},
        {OP_fcomi, 0xdbf710, "fcomi", xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {INVALID, 0xdbf810, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xdbf910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbff10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
   },
    { /* dc = [4] */
        {OP_fadd, 0xdcc010, "fadd", st0, xx, st0, st0, xx, mrm, x, tfh[4][0x01]}, /* c0 = [0x00] */
        {OP_fadd, 0xdcc110, "fadd", st1, xx, st0, st1, xx, mrm, x, tfh[4][0x02]},
        {OP_fadd, 0xdcc210, "fadd", st2, xx, st0, st2, xx, mrm, x, tfh[4][0x03]},
        {OP_fadd, 0xdcc310, "fadd", st3, xx, st0, st3, xx, mrm, x, tfh[4][0x04]},
        {OP_fadd, 0xdcc410, "fadd", st4, xx, st0, st4, xx, mrm, x, tfh[4][0x05]},
        {OP_fadd, 0xdcc510, "fadd", st5, xx, st0, st5, xx, mrm, x, tfh[4][0x06]},
        {OP_fadd, 0xdcc610, "fadd", st6, xx, st0, st6, xx, mrm, x, tfh[4][0x07]},
        {OP_fadd, 0xdcc710, "fadd", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fmul, 0xdcc810, "fmul", st0, xx, st0, st0, xx, mrm, x, tfh[4][0x09]}, /* c8 = [0x08] */
        {OP_fmul, 0xdcc910, "fmul", st1, xx, st0, st1, xx, mrm, x, tfh[4][0x0a]},
        {OP_fmul, 0xdcca10, "fmul", st2, xx, st0, st2, xx, mrm, x, tfh[4][0x0b]},
        {OP_fmul, 0xdccb10, "fmul", st3, xx, st0, st3, xx, mrm, x, tfh[4][0x0c]},
        {OP_fmul, 0xdccc10, "fmul", st4, xx, st0, st4, xx, mrm, x, tfh[4][0x0d]},
        {OP_fmul, 0xdccd10, "fmul", st5, xx, st0, st5, xx, mrm, x, tfh[4][0x0e]},
        {OP_fmul, 0xdcce10, "fmul", st6, xx, st0, st6, xx, mrm, x, tfh[4][0x0f]},
        {OP_fmul, 0xdccf10, "fmul", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fcom2".  We assume an alias for fcom
         * and do not include in the encode chain.
         */
        {OP_fcom, 0xdcd010, "fcom", xx, xx, st0, st0, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {OP_fcom, 0xdcd110, "fcom", xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd210, "fcom", xx, xx, st0, st2, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd310, "fcom", xx, xx, st0, st3, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd410, "fcom", xx, xx, st0, st4, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd510, "fcom", xx, xx, st0, st5, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd610, "fcom", xx, xx, st0, st6, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd710, "fcom", xx, xx, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fcomp3".  We assume an alias for fcomp
         * and do not include in the encode chain.
         */
        {OP_fcomp, 0xdcd810, "fcomp", xx, xx, st0, st0, xx, mrm, x, END_LIST}, /* d8 = [0x18] */
        {OP_fcomp, 0xdcd910, "fcomp", xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcda10, "fcomp", xx, xx, st0, st2, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdb10, "fcomp", xx, xx, st0, st3, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdc10, "fcomp", xx, xx, st0, st4, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdd10, "fcomp", xx, xx, st0, st5, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcde10, "fcomp", xx, xx, st0, st6, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdf10, "fcomp", xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsubr, 0xdce010, "fsubr", st0, xx, st0, st0, xx, mrm, x, tfh[4][0x21]}, /* e0 = [0x20] */
        {OP_fsubr, 0xdce110, "fsubr", st1, xx, st0, st1, xx, mrm, x, tfh[4][0x22]},
        {OP_fsubr, 0xdce210, "fsubr", st2, xx, st0, st2, xx, mrm, x, tfh[4][0x23]},
        {OP_fsubr, 0xdce310, "fsubr", st3, xx, st0, st3, xx, mrm, x, tfh[4][0x24]},
        {OP_fsubr, 0xdce410, "fsubr", st4, xx, st0, st4, xx, mrm, x, tfh[4][0x25]},
        {OP_fsubr, 0xdce510, "fsubr", st5, xx, st0, st5, xx, mrm, x, tfh[4][0x26]},
        {OP_fsubr, 0xdce610, "fsubr", st6, xx, st0, st6, xx, mrm, x, tfh[4][0x27]},
        {OP_fsubr, 0xdce710, "fsubr", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsub, 0xdce810, "fsub", st0, xx, st0, st0, xx, mrm, x, tfh[4][0x29]}, /* e8 = [0x28] */
        {OP_fsub, 0xdce910, "fsub", st1, xx, st0, st1, xx, mrm, x, tfh[4][0x2a]},
        {OP_fsub, 0xdcea10, "fsub", st2, xx, st0, st2, xx, mrm, x, tfh[4][0x2b]},
        {OP_fsub, 0xdceb10, "fsub", st3, xx, st0, st3, xx, mrm, x, tfh[4][0x2c]},
        {OP_fsub, 0xdcec10, "fsub", st4, xx, st0, st4, xx, mrm, x, tfh[4][0x2d]},
        {OP_fsub, 0xdced10, "fsub", st5, xx, st0, st5, xx, mrm, x, tfh[4][0x2e]},
        {OP_fsub, 0xdcee10, "fsub", st6, xx, st0, st6, xx, mrm, x, tfh[4][0x2f]},
        {OP_fsub, 0xdcef10, "fsub", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdivr, 0xdcf010, "fdivr", st0, xx, st0, st0, xx, mrm, x, tfh[4][0x31]}, /* f0 = [0x30] */
        {OP_fdivr, 0xdcf110, "fdivr", st1, xx, st0, st1, xx, mrm, x, tfh[4][0x32]},
        {OP_fdivr, 0xdcf210, "fdivr", st2, xx, st0, st2, xx, mrm, x, tfh[4][0x33]},
        {OP_fdivr, 0xdcf310, "fdivr", st3, xx, st0, st3, xx, mrm, x, tfh[4][0x34]},
        {OP_fdivr, 0xdcf410, "fdivr", st4, xx, st0, st4, xx, mrm, x, tfh[4][0x35]},
        {OP_fdivr, 0xdcf510, "fdivr", st5, xx, st0, st5, xx, mrm, x, tfh[4][0x36]},
        {OP_fdivr, 0xdcf610, "fdivr", st6, xx, st0, st6, xx, mrm, x, tfh[4][0x37]},
        {OP_fdivr, 0xdcf710, "fdivr", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdiv, 0xdcf810, "fdiv", st0, xx, st0, st0, xx, mrm, x, tfh[4][0x39]}, /* f8 = [0x38] */
        {OP_fdiv, 0xdcf910, "fdiv", st1, xx, st0, st1, xx, mrm, x, tfh[4][0x3a]},
        {OP_fdiv, 0xdcfa10, "fdiv", st2, xx, st0, st2, xx, mrm, x, tfh[4][0x3b]},
        {OP_fdiv, 0xdcfb10, "fdiv", st3, xx, st0, st3, xx, mrm, x, tfh[4][0x3c]},
        {OP_fdiv, 0xdcfc10, "fdiv", st4, xx, st0, st4, xx, mrm, x, tfh[4][0x3d]},
        {OP_fdiv, 0xdcfd10, "fdiv", st5, xx, st0, st5, xx, mrm, x, tfh[4][0x3e]},
        {OP_fdiv, 0xdcfe10, "fdiv", st6, xx, st0, st6, xx, mrm, x, tfh[4][0x3f]},
        {OP_fdiv, 0xdcff10, "fdiv", st7, xx, st0, st7, xx, mrm, x, END_LIST},
   },
    { /* dd = [5] */
        {OP_ffree, 0xddc010, "ffree", st0, xx, xx, xx, xx, mrm, x, tfh[5][0x01]}, /* c0 = [0x00] */
        {OP_ffree, 0xddc110, "ffree", st1, xx, xx, xx, xx, mrm, x, tfh[5][0x02]},
        {OP_ffree, 0xddc210, "ffree", st2, xx, xx, xx, xx, mrm, x, tfh[5][0x03]},
        {OP_ffree, 0xddc310, "ffree", st3, xx, xx, xx, xx, mrm, x, tfh[5][0x04]},
        {OP_ffree, 0xddc410, "ffree", st4, xx, xx, xx, xx, mrm, x, tfh[5][0x05]},
        {OP_ffree, 0xddc510, "ffree", st5, xx, xx, xx, xx, mrm, x, tfh[5][0x06]},
        {OP_ffree, 0xddc610, "ffree", st6, xx, xx, xx, xx, mrm, x, tfh[5][0x07]},
        {OP_ffree, 0xddc710, "ffree", st7, xx, xx, xx, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fxch4".  We assume an alias for fxch
         * and do not include in the encode chain.
         */
        {OP_fxch, 0xddc810, "fxch", st0, st0, st0, st0, xx, mrm, x, END_LIST}, /* c8 = [0x08] */
        {OP_fxch, 0xddc910, "fxch", st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddca10, "fxch", st0, st2, st0, st2, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcb10, "fxch", st0, st3, st0, st3, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcc10, "fxch", st0, st4, st0, st4, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcd10, "fxch", st0, st5, st0, st5, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddce10, "fxch", st0, st6, st0, st6, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcf10, "fxch", st0, st7, st0, st7, xx, mrm, x, END_LIST},
        {OP_fst, 0xddd010, "fst", st0, xx, st0, xx, xx, mrm, x, tfh[5][0x11]}, /* d0 = [0x10] */
        {OP_fst, 0xddd110, "fst", st1, xx, st0, xx, xx, mrm, x, tfh[5][0x12]},
        {OP_fst, 0xddd210, "fst", st2, xx, st0, xx, xx, mrm, x, tfh[5][0x13]},
        {OP_fst, 0xddd310, "fst", st3, xx, st0, xx, xx, mrm, x, tfh[5][0x14]},
        {OP_fst, 0xddd410, "fst", st4, xx, st0, xx, xx, mrm, x, tfh[5][0x15]},
        {OP_fst, 0xddd510, "fst", st5, xx, st0, xx, xx, mrm, x, tfh[5][0x16]},
        {OP_fst, 0xddd610, "fst", st6, xx, st0, xx, xx, mrm, x, tfh[5][0x17]},
        {OP_fst, 0xddd710, "fst", st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xddd810, "fstp", st0, xx, st0, xx, xx, mrm, x, tfh[5][0x19]}, /* d8 = [0x18] */
        {OP_fstp, 0xddd910, "fstp", st1, xx, st0, xx, xx, mrm, x, tfh[5][0x1a]},
        {OP_fstp, 0xddda10, "fstp", st2, xx, st0, xx, xx, mrm, x, tfh[5][0x1b]},
        {OP_fstp, 0xdddb10, "fstp", st3, xx, st0, xx, xx, mrm, x, tfh[5][0x1c]},
        {OP_fstp, 0xdddc10, "fstp", st4, xx, st0, xx, xx, mrm, x, tfh[5][0x1d]},
        {OP_fstp, 0xdddd10, "fstp", st5, xx, st0, xx, xx, mrm, x, tfh[5][0x1e]},
        {OP_fstp, 0xddde10, "fstp", st6, xx, st0, xx, xx, mrm, x, tfh[5][0x1f]},
        {OP_fstp, 0xdddf10, "fstp", st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fucom, 0xdde010, "fucom", xx, xx, st0, st0, xx, mrm, x, tfh[5][0x21]}, /* e0 = [0x20] */
        {OP_fucom, 0xdde110, "fucom", xx, xx, st1, st0, xx, mrm, x, tfh[5][0x22]},
        {OP_fucom, 0xdde210, "fucom", xx, xx, st2, st0, xx, mrm, x, tfh[5][0x23]},
        {OP_fucom, 0xdde310, "fucom", xx, xx, st3, st0, xx, mrm, x, tfh[5][0x24]},
        {OP_fucom, 0xdde410, "fucom", xx, xx, st4, st0, xx, mrm, x, tfh[5][0x25]},
        {OP_fucom, 0xdde510, "fucom", xx, xx, st5, st0, xx, mrm, x, tfh[5][0x26]},
        {OP_fucom, 0xdde610, "fucom", xx, xx, st6, st0, xx, mrm, x, tfh[5][0x27]},
        {OP_fucom, 0xdde710, "fucom", xx, xx, st7, st0, xx, mrm, x, END_LIST},
        {OP_fucomp, 0xdde810, "fucomp", xx, xx, st0, st0, xx, mrm, x, tfh[5][0x29]}, /* e8 = [0x28] */
        {OP_fucomp, 0xdde910, "fucomp", xx, xx, st1, st0, xx, mrm, x, tfh[5][0x2a]},
        {OP_fucomp, 0xddea10, "fucomp", xx, xx, st2, st0, xx, mrm, x, tfh[5][0x2b]},
        {OP_fucomp, 0xddeb10, "fucomp", xx, xx, st3, st0, xx, mrm, x, tfh[5][0x2c]},
        {OP_fucomp, 0xddec10, "fucomp", xx, xx, st4, st0, xx, mrm, x, tfh[5][0x2d]},
        {OP_fucomp, 0xdded10, "fucomp", xx, xx, st5, st0, xx, mrm, x, tfh[5][0x2e]},
        {OP_fucomp, 0xddee10, "fucomp", xx, xx, st6, st0, xx, mrm, x, tfh[5][0x2f]},
        {OP_fucomp, 0xddef10, "fucomp", xx, xx, st7, st0, xx, mrm, x, END_LIST},
        {INVALID, 0xddf010, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* f0 = [0x30] */
        {INVALID, 0xddf110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf810, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xddf910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddff10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
   },
    { /* de = [6]*/
        {OP_faddp, 0xdec010, "faddp", st0, xx, st0, st0, xx, mrm, x, tfh[6][0x01]}, /* c0 = [0x00] */
        {OP_faddp, 0xdec110, "faddp", st1, xx, st0, st1, xx, mrm, x, tfh[6][0x02]},
        {OP_faddp, 0xdec210, "faddp", st2, xx, st0, st2, xx, mrm, x, tfh[6][0x03]},
        {OP_faddp, 0xdec310, "faddp", st3, xx, st0, st3, xx, mrm, x, tfh[6][0x04]},
        {OP_faddp, 0xdec410, "faddp", st4, xx, st0, st4, xx, mrm, x, tfh[6][0x05]},
        {OP_faddp, 0xdec510, "faddp", st5, xx, st0, st5, xx, mrm, x, tfh[6][0x06]},
        {OP_faddp, 0xdec610, "faddp", st6, xx, st0, st6, xx, mrm, x, tfh[6][0x07]},
        {OP_faddp, 0xdec710, "faddp", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fmulp, 0xdec810, "fmulp", st0, xx, st0, st0, xx, mrm, x, tfh[6][0x09]}, /* c8 = [0x08] */
        {OP_fmulp, 0xdec910, "fmulp", st1, xx, st0, st1, xx, mrm, x, tfh[6][0x0a]},
        {OP_fmulp, 0xdeca10, "fmulp", st2, xx, st0, st2, xx, mrm, x, tfh[6][0x0b]},
        {OP_fmulp, 0xdecb10, "fmulp", st3, xx, st0, st3, xx, mrm, x, tfh[6][0x0c]},
        {OP_fmulp, 0xdecc10, "fmulp", st4, xx, st0, st4, xx, mrm, x, tfh[6][0x0d]},
        {OP_fmulp, 0xdecd10, "fmulp", st5, xx, st0, st5, xx, mrm, x, tfh[6][0x0e]},
        {OP_fmulp, 0xdece10, "fmulp", st6, xx, st0, st6, xx, mrm, x, tfh[6][0x0f]},
        {OP_fmulp, 0xdecf10, "fmulp", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fcomp5".  We assume an alias for fcomp
         * and do not include in the encode chain.
         */
        {OP_fcomp, 0xded010, "fcomp", xx, xx, st0, st0, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {OP_fcomp, 0xded110, "fcomp", xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded210, "fcomp", xx, xx, st0, st2, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded310, "fcomp", xx, xx, st0, st3, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded410, "fcomp", xx, xx, st0, st4, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded510, "fcomp", xx, xx, st0, st5, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded610, "fcomp", xx, xx, st0, st6, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded710, "fcomp", xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {INVALID, 0xded810, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* d8 = [0x18] */
        {OP_fcompp, 0xded910, "fcompp", xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {INVALID, 0xdeda10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdede10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedf10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {OP_fsubrp, 0xdee010, "fsubrp", st0, xx, st0, st0, xx, mrm, x, tfh[6][0x21]}, /* e0 = [0x20] */
        {OP_fsubrp, 0xdee110, "fsubrp", st1, xx, st0, st1, xx, mrm, x, tfh[6][0x22]},
        {OP_fsubrp, 0xdee210, "fsubrp", st2, xx, st0, st2, xx, mrm, x, tfh[6][0x23]},
        {OP_fsubrp, 0xdee310, "fsubrp", st3, xx, st0, st3, xx, mrm, x, tfh[6][0x24]},
        {OP_fsubrp, 0xdee410, "fsubrp", st4, xx, st0, st4, xx, mrm, x, tfh[6][0x25]},
        {OP_fsubrp, 0xdee510, "fsubrp", st5, xx, st0, st5, xx, mrm, x, tfh[6][0x26]},
        {OP_fsubrp, 0xdee610, "fsubrp", st6, xx, st0, st6, xx, mrm, x, tfh[6][0x27]},
        {OP_fsubrp, 0xdee710, "fsubrp", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsubp, 0xdee810, "fsubp", st0, xx, st0, st0, xx, mrm, x, tfh[6][0x29]}, /* e8 = [0x28] */
        {OP_fsubp, 0xdee910, "fsubp", st1, xx, st0, st1, xx, mrm, x, tfh[6][0x2a]},
        {OP_fsubp, 0xdeea10, "fsubp", st2, xx, st0, st2, xx, mrm, x, tfh[6][0x2b]},
        {OP_fsubp, 0xdeeb10, "fsubp", st3, xx, st0, st3, xx, mrm, x, tfh[6][0x2c]},
        {OP_fsubp, 0xdeec10, "fsubp", st4, xx, st0, st4, xx, mrm, x, tfh[6][0x2d]},
        {OP_fsubp, 0xdeed10, "fsubp", st5, xx, st0, st5, xx, mrm, x, tfh[6][0x2e]},
        {OP_fsubp, 0xdeee10, "fsubp", st6, xx, st0, st6, xx, mrm, x, tfh[6][0x2f]},
        {OP_fsubp, 0xdeef10, "fsubp", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdivrp, 0xdef010, "fdivrp", st0, xx, st0, st0, xx, mrm, x, tfh[6][0x31]}, /* f0 = [0x30] */
        {OP_fdivrp, 0xdef110, "fdivrp", st1, xx, st0, st1, xx, mrm, x, tfh[6][0x32]},
        {OP_fdivrp, 0xdef210, "fdivrp", st2, xx, st0, st2, xx, mrm, x, tfh[6][0x33]},
        {OP_fdivrp, 0xdef310, "fdivrp", st3, xx, st0, st3, xx, mrm, x, tfh[6][0x34]},
        {OP_fdivrp, 0xdef410, "fdivrp", st4, xx, st0, st4, xx, mrm, x, tfh[6][0x35]},
        {OP_fdivrp, 0xdef510, "fdivrp", st5, xx, st0, st5, xx, mrm, x, tfh[6][0x36]},
        {OP_fdivrp, 0xdef610, "fdivrp", st6, xx, st0, st6, xx, mrm, x, tfh[6][0x37]},
        {OP_fdivrp, 0xdef710, "fdivrp", st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdivp, 0xdef810, "fdivp", st0, xx, st0, st0, xx, mrm, x, tfh[6][0x39]}, /* f8 = [0x38] */
        {OP_fdivp, 0xdef910, "fdivp", st1, xx, st0, st1, xx, mrm, x, tfh[6][0x3a]},
        {OP_fdivp, 0xdefa10, "fdivp", st2, xx, st0, st2, xx, mrm, x, tfh[6][0x3b]},
        {OP_fdivp, 0xdefb10, "fdivp", st3, xx, st0, st3, xx, mrm, x, tfh[6][0x3c]},
        {OP_fdivp, 0xdefc10, "fdivp", st4, xx, st0, st4, xx, mrm, x, tfh[6][0x3d]},
        {OP_fdivp, 0xdefd10, "fdivp", st5, xx, st0, st5, xx, mrm, x, tfh[6][0x3e]},
        {OP_fdivp, 0xdefe10, "fdivp", st6, xx, st0, st6, xx, mrm, x, tfh[6][0x3f]},
        {OP_fdivp, 0xdeff10, "fdivp", st7, xx, st0, st7, xx, mrm, x, END_LIST},
   },
    { /* df = [7] */
        /* Undocumented by Intel, but is on p152 of "AMD Athlon
         * Processor x86 Code Optimization Guide."
         */
        {OP_ffreep, 0xdfc010, "ffreep", st0, xx, xx, xx, xx, mrm, x, tfh[7][0x01]}, /* c0 = [0x00] */
        {OP_ffreep, 0xdfc110, "ffreep", st1, xx, xx, xx, xx, mrm, x, tfh[7][0x02]},
        {OP_ffreep, 0xdfc210, "ffreep", st2, xx, xx, xx, xx, mrm, x, tfh[7][0x03]},
        {OP_ffreep, 0xdfc310, "ffreep", st3, xx, xx, xx, xx, mrm, x, tfh[7][0x04]},
        {OP_ffreep, 0xdfc410, "ffreep", st4, xx, xx, xx, xx, mrm, x, tfh[7][0x05]},
        {OP_ffreep, 0xdfc510, "ffreep", st5, xx, xx, xx, xx, mrm, x, tfh[7][0x06]},
        {OP_ffreep, 0xdfc610, "ffreep", st6, xx, xx, xx, xx, mrm, x, tfh[7][0x07]},
        {OP_ffreep, 0xdfc710, "ffreep", st7, xx, xx, xx, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fxch7".  We assume an alias for fxch
         * and do not include in the encode chain.
         */
        {OP_fxch, 0xdfc810, "fxch", st0, st0, st0, st0, xx, mrm, x, END_LIST}, /* c8 = [0x08] */
        {OP_fxch, 0xdfc910, "fxch", st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfca10, "fxch", st0, st2, st0, st2, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcb10, "fxch", st0, st3, st0, st3, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcc10, "fxch", st0, st4, st0, st4, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcd10, "fxch", st0, st5, st0, st5, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfce10, "fxch", st0, st6, st0, st6, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcf10, "fxch", st0, st7, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fstp8".  We assume an alias for fstp
         * and do not include in the encode chain.
         */
        {OP_fstp, 0xdfd010, "fstp", st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {OP_fstp, 0xdfd110, "fstp", st1, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd210, "fstp", st2, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd310, "fstp", st3, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd410, "fstp", st4, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd510, "fstp", st5, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd610, "fstp", st6, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd710, "fstp", st7, xx, st0, xx, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fstp9".  We assume an alias for fstp
         * and do not include in the encode chain.
         */
        {OP_fstp, 0xdfd810, "fstp", st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* d8 = [0x18] */
        {OP_fstp, 0xdfd910, "fstp", st1, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfda10, "fstp", st2, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdb10, "fstp", st3, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdc10, "fstp", st4, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdd10, "fstp", st5, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfde10, "fstp", st6, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdf10, "fstp", st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fnstsw, 0xdfe010, "fnstsw", ax, xx, xx, xx, xx, mrm, x, END_LIST}, /* e0 = [0x20] */ /*FIXME:w/ preceding fwait instr, this is "fstsw"*/
        {INVALID, 0xdfe110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {OP_fucomip, 0xdfe810, "fucomip", xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x29]}, /* e8 = [0x28] */
        {OP_fucomip, 0xdfe910, "fucomip", xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2a]},
        {OP_fucomip, 0xdfea10, "fucomip", xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2b]},
        {OP_fucomip, 0xdfeb10, "fucomip", xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2c]},
        {OP_fucomip, 0xdfec10, "fucomip", xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2d]},
        {OP_fucomip, 0xdfed10, "fucomip", xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2e]},
        {OP_fucomip, 0xdfee10, "fucomip", xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2f]},
        {OP_fucomip, 0xdfef10, "fucomip", xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {OP_fcomip, 0xdff010, "fcomip", xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x31]}, /* f0 = [0x30] */
        {OP_fcomip, 0xdff110, "fcomip", xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x32]},
        {OP_fcomip, 0xdff210, "fcomip", xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x33]},
        {OP_fcomip, 0xdff310, "fcomip", xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x34]},
        {OP_fcomip, 0xdff410, "fcomip", xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x35]},
        {OP_fcomip, 0xdff510, "fcomip", xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x36]},
        {OP_fcomip, 0xdff610, "fcomip", xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x37]},
        {OP_fcomip, 0xdff710, "fcomip", xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {INVALID, 0xdff810, "(bad)", xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xdff910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfff10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
   },
};

/****************************************************************************
 * Suffix extensions: 3DNow! and 3DNow!+
 * Since there are only 24 of them, we save space by having a
 * table of 256 indices instead of 256 instr_info_t structs.
 */
const byte suffix_index[256] = {
  /* 0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 20,18, 0, 0,  /* 0 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 21,19, 0, 0,  /* 1 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 2 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 3 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 4 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 5 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 6 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 7 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0,22, 0,  0, 0,23, 0,  /* 8 */
     4, 0, 0, 0,  7, 0,10,13,  0, 0,16, 0,  0, 0, 2, 0,  /* 9 */
     5, 0, 0, 0,  8, 0,11,14,  0, 0,17, 0,  0, 0, 3, 0,  /* A */
     6, 0, 0, 0,  9, 0,12,15,  0, 0, 0,24,  0, 0, 0, 1,  /* B */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* C */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* D */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* E */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0   /* F */
};
const instr_info_t suffix_extensions[] = {
    /* Rather than forging an exception let's anticipate future additions: we know
     * (pretty sure anyway) that they'll have the same length and operand structure.
     * Won't encode properly from Level 4 but that's ok.
     */
    {OP_unknown_3dnow, 0x000f0f90, "unknown 3DNow",
                                          Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 0*/
    {OP_pavgusb , 0xbf0f0f90, "pavgusb",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 1*/
    {OP_pfadd   , 0x9e0f0f90, "pfadd",    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 2*/
    {OP_pfacc   , 0xae0f0f90, "pfacc",    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 3*/
    {OP_pfcmpge , 0x900f0f90, "pfcmpge",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 4*/
    {OP_pfcmpgt , 0xa00f0f90, "pfcmpgt",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 5*/
    {OP_pfcmpeq , 0xb00f0f90, "pfcmpeq",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 6*/
    {OP_pfmin   , 0x940f0f90, "pfmin"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 7*/
    {OP_pfmax   , 0xa40f0f90, "pfmax"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 8*/
    {OP_pfmul   , 0xb40f0f90, "pfmul"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 9*/
    {OP_pfrcp   , 0x960f0f90, "pfrcp"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*10*/
    {OP_pfrcpit1, 0xa60f0f90, "pfrcpit1", Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*11*/
    {OP_pfrcpit2, 0xb60f0f90, "pfrcpit2", Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*12*/
    {OP_pfrsqrt , 0x970f0f90, "pfrsqrt",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*13*/
    {OP_pfrsqit1, 0xa70f0f90, "pfrsqit1", Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*14*/
    {OP_pmulhrw , 0xb70f0f90, "pmulhrw",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*15*/
    {OP_pfsub   , 0x9a0f0f90, "pfsub"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*16*/
    {OP_pfsubr  , 0xaa0f0f90, "pfsubr" ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*17*/
    {OP_pi2fd   , 0x0d0f0f90, "pi2fd"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*18*/
    {OP_pf2id   , 0x1d0f0f90, "pf2id",    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*19*/
    {OP_pi2fw   , 0x0c0f0f90, "pi2fw"  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*20*/
    {OP_pf2iw   , 0x1c0f0f90, "pf2iw",    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*21*/
    {OP_pfnacc  , 0x8a0f0f90, "pfnacc" ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*22*/
    {OP_pfpnacc , 0x8e0f0f90, "pfpnacc",  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*23*/
    {OP_pswapd  , 0xbb0f0f90, "pswapd" ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*24*/
};

/****************************************************************************
 * To handle more than 2 dests or 3 sources we chain on extra instructions.
 * All cases where we have extra operands are single-encoding-only instructions,
 * so we use the list field to point to here.
 * N.B.: the size of this table is hardcoded in decode.c.
 * Also, only implicit operands are in these instruction extensions!!!
 */
const instr_info_t extra_operands[] =
{
    /* 0x00 */
    {OP_CONTD, 0x000000, "<pusha cont'd>", xx, xx, eCX, eDX, eBP, xop, x, exop[0x01]},
    {OP_CONTD, 0x000000, "<pusha cont'd>", xx, xx, eSI, eDI, xx, no, x, END_LIST},
    /* 0x02 */
    {OP_CONTD, 0x000000, "<popa cont'd>", eBX, eCX, xx, xx, xx, xop, x, exop[0x03]},
    {OP_CONTD, 0x000000, "<popa cont'd>", eDX, eBP, xx, xx, xx, xop, x, exop[0x04]},
    {OP_CONTD, 0x000000, "<popa cont'd>", eSI, eDI, xx, xx, xx, no, x, END_LIST},
    /* 0x05 */
    {OP_CONTD, 0x000000, "<enter cont'd>", xbp, xx, xbp, xx, xx, no, x, END_LIST},
    /* 0x06 */
    {OP_CONTD, 0x000000, "<cpuid cont'd>", ecx, edx, xx, xx, xx, no, x, END_LIST},
    /* 0x07 */
    {OP_CONTD, 0x000000, "<cmpxchg8b cont'd>", eDX, xx, eCX, eBX, xx, mrm, fWZ, END_LIST},
    {OP_CONTD,0x663a6018, "<pcmpestrm cont'd", xx, xx, eax, edx, xx, mrm|reqp, fW6, END_LIST},
    {OP_CONTD,0x663a6018, "<pcmpestri cont'd", xx, xx, eax, edx, xx, mrm|reqp, fW6, END_LIST},
    /* 10 */
    {OP_CONTD,0xf90f0177, "<rdtscp cont'd>", ecx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD,0x663a6018, "<vpcmpestrm cont'd", xx, xx, eax, edx, xx, mrm|vex|reqp, fW6, END_LIST},
    {OP_CONTD,0x663a6018, "<vpcmpestri cont'd", xx, xx, eax, edx, xx, mrm|vex|reqp, fW6, END_LIST},
    {OP_CONTD,0x0f3710, "<getsec cont'd", ecx, xx, xx, xx, xx, predcx, x, END_LIST},
};

