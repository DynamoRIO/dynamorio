/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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
#include "arch.h"       /* need this to include decode.h (byte, etc. */
#include "instr.h"      /* for REG_ constants */
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

// We skip auto-formatting for the entire file to keep our aligned op_instr
// entries and our single-line table entries:
/* clang-format off */

/****************************************************************************
 * Operand pointers into tables
 * When there are multiple encodings of an opcode, this points to the first
 * entry in a linked list.
 * This array corresponds with the enum in opcode_api.h
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
    /* OP_bound   */   &evex_prefix_extensions[0][0],
    /* OP_arpl    */   &x64_extensions[16][0],
    /* OP_imul    */   &base_extensions[10][5],

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
    /* OP_call_ind      */   &base_extensions[12][2],
    /* OP_call_far      */   &first_byte[0x9a],
    /* OP_call_far_ind  */   &base_extensions[12][3],
    /* OP_jmp           */   &first_byte[0xe9],
    /* OP_jmp_short     */   &first_byte[0xeb],
    /* OP_jmp_ind       */   &base_extensions[12][4],
    /* OP_jmp_far       */   &first_byte[0xea],
    /* OP_jmp_far_ind   */   &base_extensions[12][5],

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
    /* OP_ud2         */   &second_byte[0x0b],
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
    /* OP_cmovno      */   &e_vex_extensions[83][0],
    /* OP_cmovb       */   &e_vex_extensions[84][0],
    /* OP_cmovnb      */   &second_byte[0x43],
    /* OP_cmovz       */   &e_vex_extensions[86][0],
    /* OP_cmovnz      */   &e_vex_extensions[87][0],
    /* OP_cmovbe      */   &e_vex_extensions[88][0],
    /* OP_cmovnbe     */   &e_vex_extensions[89][0],
    /* OP_cmovs       */   &second_byte[0x48],
    /* OP_cmovns      */   &second_byte[0x49],
    /* OP_cmovp       */   &e_vex_extensions[90][0],
    /* OP_cmovnp      */   &e_vex_extensions[85][0],
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

    /* OP_seto        */   &e_vex_extensions[79][0],
    /* OP_setno       */   &e_vex_extensions[80][0],
    /* OP_setb        */   &e_vex_extensions[81][0],
    /* OP_setnb       */   &e_vex_extensions[82][0],
    /* OP_setz        */   &second_byte[0x94],
    /* OP_setnz       */   &second_byte[0x95],
    /* OP_setbe       */   &second_byte[0x96],
    /* OP_setnbe      */   &second_byte[0x97],
    /* OP_sets        */   &e_vex_extensions[91][0],
    /* OP_setns       */   &e_vex_extensions[92][0],
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
    /* OP_ud1         */   &second_byte[0xb9],
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


    /* OP_rol          */   &base_extensions[ 4][0],
    /* OP_ror          */   &base_extensions[ 4][1],
    /* OP_rcl          */   &base_extensions[ 4][2],
    /* OP_rcr          */   &base_extensions[ 4][3],
    /* OP_shl          */   &base_extensions[ 4][4],
    /* OP_shr          */   &base_extensions[ 4][5],
    /* OP_sar          */   &base_extensions[ 4][7],
    /* OP_not          */   &base_extensions[10][2],
    /* OP_neg          */   &base_extensions[10][3],
    /* OP_mul          */   &base_extensions[10][4],
    /* OP_div          */   &base_extensions[10][6],
    /* OP_idiv         */   &base_extensions[10][7],
    /* OP_sldt         */   &base_extensions[13][0],
    /* OP_str          */   &base_extensions[13][1],
    /* OP_lldt         */   &base_extensions[13][2],
    /* OP_ltr          */   &base_extensions[13][3],
    /* OP_verr         */   &base_extensions[13][4],
    /* OP_verw         */   &base_extensions[13][5],
    /* OP_sgdt         */   &mod_extensions[0][0],
    /* OP_sidt         */   &mod_extensions[1][0],
    /* OP_lgdt         */   &mod_extensions[5][0],
    /* OP_lidt         */   &mod_extensions[4][0],
    /* OP_smsw         */   &base_extensions[14][4],
    /* OP_lmsw         */   &base_extensions[14][6],
    /* OP_invlpg       */   &mod_extensions[2][0],
    /* OP_cmpxchg8b    */   &base_extensions[16][1],
    /* OP_fxsave32     */   &rex_w_extensions[0][0],
    /* OP_fxrstor32    */   &rex_w_extensions[1][0],
    /* OP_ldmxcsr      */   &e_vex_extensions[61][0],
    /* OP_stmxcsr      */   &e_vex_extensions[62][0],
    /* OP_lfence       */   &mod_extensions[6][1],
    /* OP_mfence       */   &mod_extensions[7][1],
    /* OP_clflush      */   &mod_extensions[3][0],
    /* OP_sfence       */   &mod_extensions[3][1],
    /* OP_prefetchnta  */   &base_extensions[23][0],
    /* OP_prefetcht0   */   &base_extensions[23][1],
    /* OP_prefetcht1   */   &base_extensions[23][2],
    /* OP_prefetcht2   */   &base_extensions[23][3],
    /* OP_prefetch     */   &base_extensions[24][0],
    /* OP_prefetchw    */   &base_extensions[24][1],


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
    /* OP_pblendvb      */   &e_vex_extensions[132][0],
    /* OP_blendvps      */   &e_vex_extensions[130][0],
    /* OP_blendvpd      */   &e_vex_extensions[129][0],
    /* OP_ptest         */   &e_vex_extensions[3][0],
    /* OP_pmovsxbw      */   &e_vex_extensions[4][0],
    /* OP_pmovsxbd      */   &e_vex_extensions[5][0],
    /* OP_pmovsxbq      */   &e_vex_extensions[6][0],
    /* OP_pmovsxwd      */   &e_vex_extensions[7][0],
    /* OP_pmovsxwq      */   &e_vex_extensions[8][0],
    /* OP_pmovsxdq      */   &e_vex_extensions[9][0],
    /* OP_pmuldq        */   &e_vex_extensions[10][0],
    /* OP_pcmpeqq       */   &e_vex_extensions[11][0],
    /* OP_movntdqa      */   &e_vex_extensions[12][0],
    /* OP_packusdw      */   &e_vex_extensions[13][0],
    /* OP_pmovzxbw      */   &e_vex_extensions[14][0],
    /* OP_pmovzxbd      */   &e_vex_extensions[15][0],
    /* OP_pmovzxbq      */   &e_vex_extensions[16][0],
    /* OP_pmovzxwd      */   &e_vex_extensions[17][0],
    /* OP_pmovzxwq      */   &e_vex_extensions[18][0],
    /* OP_pmovzxdq      */   &e_vex_extensions[19][0],
    /* OP_pcmpgtq       */   &e_vex_extensions[20][0],
    /* OP_pminsb        */   &e_vex_extensions[21][0],
    /* OP_pminsd        */   &e_vex_extensions[22][0],
    /* OP_pminuw        */   &e_vex_extensions[23][0],
    /* OP_pminud        */   &e_vex_extensions[24][0],
    /* OP_pmaxsb        */   &e_vex_extensions[25][0],
    /* OP_pmaxsd        */   &e_vex_extensions[26][0],
    /* OP_pmaxuw        */   &e_vex_extensions[27][0],
    /* OP_pmaxud        */   &e_vex_extensions[28][0],
    /* OP_pmulld        */   &e_vex_extensions[29][0],
    /* OP_phminposuw    */   &e_vex_extensions[30][0],
    /* OP_crc32         */   &prefix_extensions[139][3],
    /* OP_pextrb        */   &e_vex_extensions[36][0],
    /* OP_pextrd        */   &e_vex_extensions[38][0],
    /* OP_extractps     */   &e_vex_extensions[39][0],
    /* OP_roundps       */   &e_vex_extensions[40][0],
    /* OP_roundpd       */   &e_vex_extensions[41][0],
    /* OP_roundss       */   &e_vex_extensions[42][0],
    /* OP_roundsd       */   &e_vex_extensions[43][0],
    /* OP_blendps       */   &e_vex_extensions[44][0],
    /* OP_blendpd       */   &e_vex_extensions[45][0],
    /* OP_pblendw       */   &e_vex_extensions[46][0],
    /* OP_pinsrb        */   &e_vex_extensions[47][0],
    /* OP_insertps      */   &e_vex_extensions[48][0],
    /* OP_pinsrd        */   &e_vex_extensions[49][0],
    /* OP_dpps          */   &e_vex_extensions[50][0],
    /* OP_dppd          */   &e_vex_extensions[51][0],
    /* OP_mpsadbw       */   &e_vex_extensions[52][0],
    /* OP_pcmpestrm     */   &e_vex_extensions[53][0],
    /* OP_pcmpestri     */   &e_vex_extensions[54][0],
    /* OP_pcmpistrm     */   &e_vex_extensions[55][0],
    /* OP_pcmpistri     */   &e_vex_extensions[56][0],

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
    /* OP_pclmulqdq     */   &e_vex_extensions[57][0],
    /* OP_aesimc        */   &e_vex_extensions[31][0],
    /* OP_aesenc        */   &e_vex_extensions[32][0],
    /* OP_aesenclast    */   &e_vex_extensions[33][0],
    /* OP_aesdec        */   &e_vex_extensions[34][0],
    /* OP_aesdeclast    */   &e_vex_extensions[35][0],
    /* OP_aeskeygenassist*/  &e_vex_extensions[58][0],

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
    /* OP_vmovd         */  &vex_W_extensions[108][0],
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
    /* OP_vpblendvb     */  &e_vex_extensions[ 2][1],
    /* OP_vblendvps     */  &e_vex_extensions[ 0][1],
    /* OP_vblendvpd     */  &e_vex_extensions[ 1][1],
    /* OP_vptest        */  &e_vex_extensions[ 3][1],
    /* OP_vpmovsxbw     */  &e_vex_extensions[ 4][1],
    /* OP_vpmovsxbd     */  &e_vex_extensions[ 5][1],
    /* OP_vpmovsxbq     */  &e_vex_extensions[ 6][1],
    /* OP_vpmovsxwd     */  &e_vex_extensions[ 7][1],
    /* OP_vpmovsxwq     */  &e_vex_extensions[ 8][1],
    /* OP_vpmovsxdq     */  &e_vex_extensions[ 9][1],
    /* OP_vpmuldq       */  &e_vex_extensions[10][1],
    /* OP_vpcmpeqq      */  &e_vex_extensions[11][1],
    /* OP_vmovntdqa     */  &e_vex_extensions[12][1],
    /* OP_vpackusdw     */  &e_vex_extensions[13][1],
    /* OP_vpmovzxbw     */  &e_vex_extensions[14][1],
    /* OP_vpmovzxbd     */  &e_vex_extensions[15][1],
    /* OP_vpmovzxbq     */  &e_vex_extensions[16][1],
    /* OP_vpmovzxwd     */  &e_vex_extensions[17][1],
    /* OP_vpmovzxwq     */  &e_vex_extensions[18][1],
    /* OP_vpmovzxdq     */  &e_vex_extensions[19][1],
    /* OP_vpcmpgtq      */  &e_vex_extensions[20][1],
    /* OP_vpminsb       */  &e_vex_extensions[21][1],
    /* OP_vpminsd       */  &e_vex_extensions[22][1],
    /* OP_vpminuw       */  &e_vex_extensions[23][1],
    /* OP_vpminud       */  &e_vex_extensions[24][1],
    /* OP_vpmaxsb       */  &e_vex_extensions[25][1],
    /* OP_vpmaxsd       */  &e_vex_extensions[26][1],
    /* OP_vpmaxuw       */  &e_vex_extensions[27][1],
    /* OP_vpmaxud       */  &e_vex_extensions[28][1],
    /* OP_vpmulld       */  &e_vex_extensions[29][1],
    /* OP_vphminposuw   */  &e_vex_extensions[30][1],
    /* OP_vaesimc       */  &e_vex_extensions[31][1],
    /* OP_vaesenc       */  &e_vex_extensions[32][1],
    /* OP_vaesenclast   */  &e_vex_extensions[33][1],
    /* OP_vaesdec       */  &e_vex_extensions[34][1],
    /* OP_vaesdeclast   */  &e_vex_extensions[35][1],
    /* OP_vpextrb       */  &e_vex_extensions[36][1],
    /* OP_vpextrd       */  &e_vex_extensions[38][1],
    /* OP_vextractps    */  &e_vex_extensions[39][1],
    /* OP_vroundps      */  &e_vex_extensions[40][1],
    /* OP_vroundpd      */  &e_vex_extensions[41][1],
    /* OP_vroundss      */  &e_vex_extensions[42][1],
    /* OP_vroundsd      */  &e_vex_extensions[43][1],
    /* OP_vblendps      */  &e_vex_extensions[44][1],
    /* OP_vblendpd      */  &e_vex_extensions[45][1],
    /* OP_vpblendw      */  &e_vex_extensions[46][1],
    /* OP_vpinsrb       */  &e_vex_extensions[47][1],
    /* OP_vinsertps     */  &e_vex_extensions[48][1],
    /* OP_vpinsrd       */  &e_vex_extensions[49][1],
    /* OP_vdpps         */  &e_vex_extensions[50][1],
    /* OP_vdppd         */  &e_vex_extensions[51][1],
    /* OP_vmpsadbw      */  &e_vex_extensions[52][1],
    /* OP_vpcmpestrm    */  &e_vex_extensions[53][1],
    /* OP_vpcmpestri    */  &e_vex_extensions[54][1],
    /* OP_vpcmpistrm    */  &e_vex_extensions[55][1],
    /* OP_vpcmpistri    */  &e_vex_extensions[56][1],
    /* OP_vpclmulqdq    */  &e_vex_extensions[57][1],
    /* OP_vaeskeygenassist*/ &e_vex_extensions[58][1],
    /* OP_vtestps       */  &e_vex_extensions[59][1],
    /* OP_vtestpd       */  &e_vex_extensions[60][1],
    /* OP_vzeroupper    */  &vex_L_extensions[0][1],
    /* OP_vzeroall      */  &vex_L_extensions[0][2],
    /* OP_vldmxcsr      */  &e_vex_extensions[61][1],
    /* OP_vstmxcsr      */  &e_vex_extensions[62][1],
    /* OP_vbroadcastss  */  &e_vex_extensions[64][1],
    /* OP_vbroadcastsd  */  &e_vex_extensions[65][1],
    /* OP_vbroadcastf128*/  &e_vex_extensions[66][1],
    /* OP_vmaskmovps    */  &e_vex_extensions[67][1],
    /* OP_vmaskmovpd    */  &e_vex_extensions[68][1],
    /* OP_vpermilps     */  &e_vex_extensions[71][1],
    /* OP_vpermilpd     */  &e_vex_extensions[72][1],
    /* OP_vperm2f128    */  &e_vex_extensions[73][1],
    /* OP_vinsertf128   */  &e_vex_extensions[74][1],
    /* OP_vextractf128  */  &e_vex_extensions[75][1],

    /* added in Ivy Bridge I believe, and covered by F16C cpuid flag */
    /* OP_vcvtph2ps     */  &e_vex_extensions[63][1],
    /* OP_vcvtps2ph     */  &e_vex_extensions[76][1],

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
    /* OP_blcfill       */   &base_extensions[27][1],
    /* OP_blci          */   &base_extensions[28][6],
    /* OP_blcic         */   &base_extensions[27][5],
    /* OP_blcmsk        */   &base_extensions[28][1],
    /* OP_blcs          */   &base_extensions[27][3],
    /* OP_blsfill       */   &base_extensions[27][2],
    /* OP_blsic         */   &base_extensions[27][6],
    /* OP_t1mskc        */   &base_extensions[27][7],
    /* OP_tzmsk         */   &base_extensions[27][4],

    /* AMD LWP */
    /* OP_llwpcb        */   &base_extensions[29][0],
    /* OP_slwpcb        */   &base_extensions[29][1],
    /* OP_lwpins        */   &base_extensions[30][0],
    /* OP_lwpval        */   &base_extensions[30][1],

    /* Intel BMI1 */
    /* (includes non-immed form of OP_bextr) */
    /* OP_andn          */   &third_byte_38[100],
    /* OP_blsr          */   &base_extensions[31][1],
    /* OP_blsmsk        */   &base_extensions[31][2],
    /* OP_blsi          */   &base_extensions[31][3],
    /* OP_tzcnt         */   &prefix_extensions[140][1],

    /* Intel BMI2 */
    /* OP_bzhi          */   &prefix_extensions[142][4],
    /* OP_pext          */   &prefix_extensions[142][5],
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
    /* OP_xabort        */   &base_extensions[17][7],
    /* OP_xbegin        */   &base_extensions[18][7],
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
    /* OP_vbroadcasti128 */  &e_vex_extensions[139][1],
    /* OP_vinserti128   */   &e_vex_extensions[128][1],
    /* OP_vextracti128  */   &e_vex_extensions[127][1],
    /* OP_vpmaskmovd    */   &vex_W_extensions[70][0],
    /* OP_vpmaskmovq    */   &vex_W_extensions[70][1],
    /* OP_vperm2i128    */   &third_byte_3a[62],
    /* OP_vpermd        */   &e_vex_extensions[123][1],
    /* OP_vpermps       */   &e_vex_extensions[124][1],
    /* OP_vpermq        */   &e_vex_extensions[125][1],
    /* OP_vpermpd       */   &e_vex_extensions[126][1],
    /* OP_vpblendd      */   &third_byte_3a[61],
    /* OP_vpsllvd       */   &vex_W_extensions[73][0],
    /* OP_vpsllvq       */   &vex_W_extensions[73][1],
    /* OP_vpsravd       */   &e_vex_extensions[131][1],
    /* OP_vpsrlvd       */   &vex_W_extensions[72][0],
    /* OP_vpsrlvq       */   &vex_W_extensions[72][1],
    /* OP_vpbroadcastb  */   &e_vex_extensions[135][1],
    /* OP_vpbroadcastw  */   &e_vex_extensions[136][1],
    /* OP_vpbroadcastd  */   &e_vex_extensions[137][1],
    /* OP_vpbroadcastq  */   &e_vex_extensions[138][1],

    /* added in Intel Skylake */
    /* OP_xsavec32      */   &rex_w_extensions[5][0],
    /* OP_xsavec64      */   &rex_w_extensions[5][1],

    /* Intel ADX */
    /* OP_adox          */   &prefix_extensions[143][1],
    /* OP_adcx          */   &prefix_extensions[143][2],

    /* AVX-512 VEX encoded (scalar opmask instructions) */
    /* OP_kmovw         */  &vex_W_extensions[74][0],
    /* OP_kmovb         */  &vex_W_extensions[75][0],
    /* OP_kmovq         */  &vex_W_extensions[74][1],
    /* OP_kmovd         */  &vex_W_extensions[75][1],
    /* OP_kandw         */  &vex_W_extensions[82][0],
    /* OP_kandb         */  &vex_W_extensions[83][0],
    /* OP_kandq         */  &vex_W_extensions[82][1],
    /* OP_kandd         */  &vex_W_extensions[83][1],
    /* OP_kandnw        */  &vex_W_extensions[84][0],
    /* OP_kandnb        */  &vex_W_extensions[85][0],
    /* OP_kandnq        */  &vex_W_extensions[84][1],
    /* OP_kandnd        */  &vex_W_extensions[85][1],
    /* OP_kunpckbw      */  &vex_W_extensions[87][0],
    /* OP_kunpckwd      */  &vex_W_extensions[86][0],
    /* OP_kunpckdq      */  &vex_W_extensions[86][1],
    /* OP_knotw         */  &vex_W_extensions[88][0],
    /* OP_knotb         */  &vex_W_extensions[89][0],
    /* OP_knotq         */  &vex_W_extensions[88][1],
    /* OP_knotd         */  &vex_W_extensions[89][1],
    /* OP_korw          */  &vex_W_extensions[90][0],
    /* OP_korb          */  &vex_W_extensions[91][0],
    /* OP_korq          */  &vex_W_extensions[90][1],
    /* OP_kord          */  &vex_W_extensions[91][1],
    /* OP_kxnorw        */  &vex_W_extensions[92][0],
    /* OP_kxnorb        */  &vex_W_extensions[93][0],
    /* OP_kxnorq        */  &vex_W_extensions[92][1],
    /* OP_kxnord        */  &vex_W_extensions[93][1],
    /* OP_kxorw         */  &vex_W_extensions[94][0],
    /* OP_kxorb         */  &vex_W_extensions[95][0],
    /* OP_kxorq         */  &vex_W_extensions[94][1],
    /* OP_kxord         */  &vex_W_extensions[95][1],
    /* OP_kaddw         */  &vex_W_extensions[96][0],
    /* OP_kaddb         */  &vex_W_extensions[97][0],
    /* OP_kaddq         */  &vex_W_extensions[96][1],
    /* OP_kaddd         */  &vex_W_extensions[97][1],
    /* OP_kortestw      */  &vex_W_extensions[98][0],
    /* OP_kortestb      */  &vex_W_extensions[99][0],
    /* OP_kortestq      */  &vex_W_extensions[98][1],
    /* OP_kortestd      */  &vex_W_extensions[99][1],
    /* OP_kshiftlw      */  &vex_W_extensions[100][1],
    /* OP_kshiftlb      */  &vex_W_extensions[100][0],
    /* OP_kshiftlq      */  &vex_W_extensions[101][1],
    /* OP_kshiftld      */  &vex_W_extensions[101][0],
    /* OP_kshiftrw      */  &vex_W_extensions[102][1],
    /* OP_kshiftrb      */  &vex_W_extensions[102][0],
    /* OP_kshiftrq      */  &vex_W_extensions[103][1],
    /* OP_kshiftrd      */  &vex_W_extensions[103][0],
    /* OP_ktestw        */  &vex_W_extensions[104][0],
    /* OP_ktestb        */  &vex_W_extensions[105][0],
    /* OP_ktestq        */  &vex_W_extensions[104][1],
    /* OP_ktestd        */  &vex_W_extensions[105][1],

    /* AVX-512 EVEX encoded */
    /* OP_valignd         */  &evex_Wb_extensions[155][0],
    /* OP_valignq         */  &evex_Wb_extensions[155][2],
    /* OP_vblendmpd       */  &evex_Wb_extensions[156][2],
    /* OP_vblendmps       */  &evex_Wb_extensions[156][0],
    /* OP_vbroadcastf32x2 */  &evex_Wb_extensions[148][0],
    /* OP_vbroadcastf32x4 */  &evex_Wb_extensions[149][0],
    /* OP_vbroadcastf32x8 */  &evex_Wb_extensions[150][0],
    /* OP_vbroadcastf64x2 */  &evex_Wb_extensions[149][2],
    /* OP_vbroadcastf64x4 */  &evex_Wb_extensions[150][2],
    /* OP_vbroadcasti32x2 */  &evex_Wb_extensions[152][0],
    /* OP_vbroadcasti32x4 */  &evex_Wb_extensions[153][0],
    /* OP_vbroadcasti32x8 */  &evex_Wb_extensions[154][0],
    /* OP_vbroadcasti64x2 */  &evex_Wb_extensions[153][2],
    /* OP_vbroadcasti64x4 */  &evex_Wb_extensions[154][2],
    /* OP_vcompresspd     */  &evex_Wb_extensions[157][2],
    /* OP_vcompressps     */  &evex_Wb_extensions[157][0],
    /* OP_vcvtpd2qq       */  &evex_Wb_extensions[46][2],
    /* OP_vcvtpd2udq      */  &evex_Wb_extensions[47][2],
    /* OP_vcvtpd2uqq      */  &evex_Wb_extensions[48][2],
    /* OP_vcvtps2qq       */  &evex_Wb_extensions[46][0],
    /* OP_vcvtps2udq      */  &evex_Wb_extensions[47][0],
    /* OP_vcvtps2uqq      */  &evex_Wb_extensions[48][0],
    /* OP_vcvtqq2pd       */  &evex_Wb_extensions[57][2],
    /* OP_vcvtqq2ps       */  &evex_Wb_extensions[56][2],
    /* OP_vcvtsd2usi      */  &evex_Wb_extensions[53][0],
    /* OP_vcvtss2usi      */  &evex_Wb_extensions[52][0],
    /* OP_vcvttpd2qq      */  &evex_Wb_extensions[50][2],
    /* OP_vcvttpd2udq     */  &evex_Wb_extensions[49][2],
    /* OP_vcvttpd2uqq     */  &evex_Wb_extensions[51][2],
    /* OP_vcvttps2qq      */  &evex_Wb_extensions[50][0],
    /* OP_vcvttps2udq     */  &evex_Wb_extensions[49][0],
    /* OP_vcvttps2uqq     */  &evex_Wb_extensions[51][0],
    /* OP_vcvttsd2usi     */  &evex_Wb_extensions[55][0],
    /* OP_vcvttss2usi     */  &evex_Wb_extensions[54][0],
    /* OP_vcvtudq2pd      */  &evex_Wb_extensions[61][0],
    /* OP_vcvtudq2ps      */  &evex_Wb_extensions[60][0],
    /* OP_vcvtuqq2pd      */  &evex_Wb_extensions[61][2],
    /* OP_vcvtuqq2ps      */  &evex_Wb_extensions[60][2],
    /* OP_vcvtusi2sd      */  &evex_Wb_extensions[59][0],
    /* OP_vcvtusi2ss      */  &evex_Wb_extensions[58][0],
    /* OP_vdbpsadbw       */  &e_vex_extensions[52][2],
    /* OP_vexp2pd         */  &evex_Wb_extensions[185][2],
    /* OP_vexp2ps         */  &evex_Wb_extensions[185][0],
    /* OP_vexpandpd       */  &evex_Wb_extensions[158][2],
    /* OP_vexpandps       */  &evex_Wb_extensions[158][0],
    /* OP_vextractf32x4   */  &evex_Wb_extensions[101][0],
    /* OP_vextractf32x8   */  &evex_Wb_extensions[102][0],
    /* OP_vextractf64x2   */  &evex_Wb_extensions[101][2],
    /* OP_vextractf64x4   */  &evex_Wb_extensions[102][2],
    /* OP_vextracti32x4   */  &evex_Wb_extensions[103][0],
    /* OP_vextracti32x8   */  &evex_Wb_extensions[104][0],
    /* OP_vextracti64x2   */  &evex_Wb_extensions[103][2],
    /* OP_vextracti64x4   */  &evex_Wb_extensions[104][2],
    /* OP_vfixupimmpd     */  &evex_Wb_extensions[159][2],
    /* OP_vfixupimmps     */  &evex_Wb_extensions[159][0],
    /* OP_vfixupimmsd     */  &evex_Wb_extensions[160][2],
    /* OP_vfixupimmss     */  &evex_Wb_extensions[160][0],
    /* OP_vfpclasspd      */  &evex_Wb_extensions[183][2],
    /* OP_vfpclassps      */  &evex_Wb_extensions[183][0],
    /* OP_vfpclasssd      */  &evex_Wb_extensions[184][2],
    /* OP_vfpclassss      */  &evex_Wb_extensions[184][0],
    /* OP_vgatherpf0dpd   */  &evex_Wb_extensions[197][2],
    /* OP_vgatherpf0dps   */  &evex_Wb_extensions[197][0],
    /* OP_vgatherpf0qpd   */  &evex_Wb_extensions[198][2],
    /* OP_vgatherpf0qps   */  &evex_Wb_extensions[198][0],
    /* OP_vgatherpf1dpd   */  &evex_Wb_extensions[199][2],
    /* OP_vgatherpf1dps   */  &evex_Wb_extensions[199][0],
    /* OP_vgatherpf1qpd   */  &evex_Wb_extensions[200][2],
    /* OP_vgatherpf1qps   */  &evex_Wb_extensions[200][0],
    /* OP_vgetexppd       */  &evex_Wb_extensions[161][2],
    /* OP_vgetexpps       */  &evex_Wb_extensions[161][0],
    /* OP_vgetexpsd       */  &evex_Wb_extensions[162][2],
    /* OP_vgetexpss       */  &evex_Wb_extensions[162][0],
    /* OP_vgetmantpd      */  &evex_Wb_extensions[163][2],
    /* OP_vgetmantps      */  &evex_Wb_extensions[163][0],
    /* OP_vgetmantsd      */  &evex_Wb_extensions[164][2],
    /* OP_vgetmantss      */  &evex_Wb_extensions[164][0],
    /* OP_vinsertf32x4    */  &evex_Wb_extensions[105][0],
    /* OP_vinsertf32x8    */  &evex_Wb_extensions[106][0],
    /* OP_vinsertf64x2    */  &evex_Wb_extensions[105][2],
    /* OP_vinsertf64x4    */  &evex_Wb_extensions[106][2],
    /* OP_vinserti32x4    */  &evex_Wb_extensions[107][0],
    /* OP_vinserti32x8    */  &evex_Wb_extensions[108][0],
    /* OP_vinserti64x2    */  &evex_Wb_extensions[107][2],
    /* OP_vinserti64x4    */  &evex_Wb_extensions[108][2],
    /* OP_vmovdqa32       */  &evex_Wb_extensions[8][0],
    /* OP_vmovdqa64       */  &evex_Wb_extensions[8][2],
    /* OP_vmovdqu16       */  &evex_Wb_extensions[10][2],
    /* OP_vmovdqu32       */  &evex_Wb_extensions[11][0],
    /* OP_vmovdqu64       */  &evex_Wb_extensions[11][2],
    /* OP_vmovdqu8        */  &evex_Wb_extensions[10][0],
    /* OP_vpabsq          */  &evex_Wb_extensions[147][2],
    /* OP_vpandd          */  &evex_Wb_extensions[41][0],
    /* OP_vpandnd         */  &evex_Wb_extensions[42][0],
    /* OP_vpandnq         */  &evex_Wb_extensions[42][2],
    /* OP_vpandq          */  &evex_Wb_extensions[41][2],
    /* OP_vpblendmb       */  &evex_Wb_extensions[165][0],
    /* OP_vpblendmd       */  &evex_Wb_extensions[166][0],
    /* OP_vpblendmq       */  &evex_Wb_extensions[166][2],
    /* OP_vpblendmw       */  &evex_Wb_extensions[165][2],
    /* OP_vpbroadcastmb2q */  &prefix_extensions[184][9],
    /* OP_vpbroadcastmw2d */  &prefix_extensions[185][9],
    /* OP_vpcmpb          */  &evex_Wb_extensions[110][0],
    /* OP_vpcmpd          */  &evex_Wb_extensions[112][0],
    /* OP_vpcmpq          */  &evex_Wb_extensions[112][2],
    /* OP_vpcmpub         */  &evex_Wb_extensions[109][0],
    /* OP_vpcmpud         */  &evex_Wb_extensions[111][0],
    /* OP_vpcmpuq         */  &evex_Wb_extensions[111][2],
    /* OP_vpcmpuw         */  &evex_Wb_extensions[109][2],
    /* OP_vpcmpw          */  &evex_Wb_extensions[110][2],
    /* OP_vpcompressd     */  &evex_Wb_extensions[167][0],
    /* OP_vpcompressq     */  &evex_Wb_extensions[167][2],
    /* OP_vpconflictd     */  &evex_Wb_extensions[186][0],
    /* OP_vpconflictq     */  &evex_Wb_extensions[186][2],
    /* OP_vpermb          */  &evex_Wb_extensions[92][0],
    /* OP_vpermi2b        */  &evex_Wb_extensions[97][0],
    /* OP_vpermi2d        */  &evex_Wb_extensions[96][0],
    /* OP_vpermi2pd       */  &evex_Wb_extensions[95][2],
    /* OP_vpermi2ps       */  &evex_Wb_extensions[95][0],
    /* OP_vpermi2q        */  &evex_Wb_extensions[96][2],
    /* OP_vpermi2w        */  &evex_Wb_extensions[97][2],
    /* OP_vpermt2b        */  &evex_Wb_extensions[98][0],
    /* OP_vpermt2d        */  &evex_Wb_extensions[99][0],
    /* OP_vpermt2pd       */  &evex_Wb_extensions[100][2],
    /* OP_vpermt2ps       */  &evex_Wb_extensions[100][0],
    /* OP_vpermt2q        */  &evex_Wb_extensions[99][2],
    /* OP_vpermt2w        */  &evex_Wb_extensions[98][2],
    /* OP_vpermw          */  &evex_Wb_extensions[92][2],
    /* OP_vpexpandd       */  &evex_Wb_extensions[168][0],
    /* OP_vpexpandq       */  &evex_Wb_extensions[168][2],
    /* OP_vpextrq         */  &evex_Wb_extensions[145][2],
    /* OP_vpinsrq         */  &evex_Wb_extensions[144][2],
    /* OP_vplzcntd        */  &evex_Wb_extensions[187][0],
    /* OP_vplzcntq        */  &evex_Wb_extensions[187][2],
    /* OP_vpmadd52huq     */  &evex_Wb_extensions[234][2],
    /* OP_vpmadd52luq     */  &evex_Wb_extensions[220][2],
    /* OP_vpmaxsq         */  &evex_Wb_extensions[114][2],
    /* OP_vpmaxuq         */  &evex_Wb_extensions[116][2],
    /* OP_vpminsq         */  &evex_Wb_extensions[113][2],
    /* OP_vpminuq         */  &evex_Wb_extensions[115][2],
    /* OP_vpmovb2m        */  &evex_Wb_extensions[140][0],
    /* OP_vpmovd2m        */  &evex_Wb_extensions[141][0],
    /* OP_vpmovdb         */  &prefix_extensions[169][9],
    /* OP_vpmovdw         */  &prefix_extensions[172][9],
    /* OP_vpmovm2b        */  &evex_Wb_extensions[138][0],
    /* OP_vpmovm2d        */  &evex_Wb_extensions[139][0],
    /* OP_vpmovm2q        */  &evex_Wb_extensions[139][2],
    /* OP_vpmovm2w        */  &evex_Wb_extensions[138][2],
    /* OP_vpmovq2m        */  &evex_Wb_extensions[141][2],
    /* OP_vpmovqb         */  &prefix_extensions[160][9],
    /* OP_vpmovqd         */  &prefix_extensions[166][9],
    /* OP_vpmovqw         */  &prefix_extensions[163][9],
    /* OP_vpmovsdb        */  &prefix_extensions[170][9],
    /* OP_vpmovsdw        */  &prefix_extensions[173][9],
    /* OP_vpmovsqb        */  &prefix_extensions[161][9],
    /* OP_vpmovsqd        */  &prefix_extensions[167][9],
    /* OP_vpmovsqw        */  &prefix_extensions[164][9],
    /* OP_vpmovswb        */  &prefix_extensions[176][9],
    /* OP_vpmovusdb       */  &prefix_extensions[171][9],
    /* OP_vpmovusdw       */  &prefix_extensions[174][9],
    /* OP_vpmovusqb       */  &prefix_extensions[162][9],
    /* OP_vpmovusqd       */  &prefix_extensions[168][9],
    /* OP_vpmovusqw       */  &prefix_extensions[165][9],
    /* OP_vpmovuswb       */  &prefix_extensions[177][9],
    /* OP_vpmovw2m        */  &evex_Wb_extensions[140][2],
    /* OP_vpmovwb         */  &prefix_extensions[175][9],
    /* OP_vpmullq         */  &evex_Wb_extensions[45][2],
    /* OP_vpord           */  &evex_Wb_extensions[43][0],
    /* OP_vporq           */  &evex_Wb_extensions[43][2],
    /* OP_vprold          */  &evex_Wb_extensions[118][0],
    /* OP_vprolq          */  &evex_Wb_extensions[118][2],
    /* OP_vprolvd         */  &evex_Wb_extensions[117][0],
    /* OP_vprolvq         */  &evex_Wb_extensions[117][2],
    /* OP_vprord          */  &evex_Wb_extensions[120][0],
    /* OP_vprorq          */  &evex_Wb_extensions[120][2],
    /* OP_vprorvd         */  &evex_Wb_extensions[119][0],
    /* OP_vprorvq         */  &evex_Wb_extensions[119][2],
    /* OP_vpscatterdd     */  &evex_Wb_extensions[193][0],
    /* OP_vpscatterdq     */  &evex_Wb_extensions[193][2],
    /* OP_vpscatterqd     */  &evex_Wb_extensions[194][0],
    /* OP_vpscatterqq     */  &evex_Wb_extensions[194][2],
    /* OP_vpsllvw         */  &evex_Wb_extensions[130][2],
    /* OP_vpsraq          */  &evex_Wb_extensions[121][2],
    /* OP_vpsravq         */  &evex_Wb_extensions[128][2],
    /* OP_vpsravw         */  &evex_Wb_extensions[127][2],
    /* OP_vpsrlvw         */  &prefix_extensions[177][10],
    /* OP_vpternlogd      */  &evex_Wb_extensions[188][0],
    /* OP_vpternlogq      */  &evex_Wb_extensions[188][2],
    /* OP_vptestmb        */  &evex_Wb_extensions[169][0],
    /* OP_vptestmd        */  &evex_Wb_extensions[170][0],
    /* OP_vptestmq        */  &evex_Wb_extensions[170][2],
    /* OP_vptestmw        */  &evex_Wb_extensions[169][2],
    /* OP_vptestnmb       */  &evex_Wb_extensions[171][0],
    /* OP_vptestnmd       */  &evex_Wb_extensions[172][0],
    /* OP_vptestnmq       */  &evex_Wb_extensions[172][2],
    /* OP_vptestnmw       */  &evex_Wb_extensions[171][2],
    /* OP_vpxord          */  &evex_Wb_extensions[44][0],
    /* OP_vpxorq          */  &evex_Wb_extensions[44][2],
    /* OP_vrangepd        */  &evex_Wb_extensions[173][2],
    /* OP_vrangeps        */  &evex_Wb_extensions[173][0],
    /* OP_vrangesd        */  &evex_Wb_extensions[174][2],
    /* OP_vrangess        */  &evex_Wb_extensions[174][0],
    /* OP_vrcp14pd        */  &evex_Wb_extensions[132][2],
    /* OP_vrcp14ps        */  &evex_Wb_extensions[132][0],
    /* OP_vrcp14sd        */  &evex_Wb_extensions[133][2],
    /* OP_vrcp14ss        */  &evex_Wb_extensions[133][0],
    /* OP_vrcp28pd        */  &evex_Wb_extensions[134][2],
    /* OP_vrcp28ps        */  &evex_Wb_extensions[134][0],
    /* OP_vrcp28sd        */  &evex_Wb_extensions[135][2],
    /* OP_vrcp28ss        */  &evex_Wb_extensions[135][0],
    /* OP_vreducepd       */  &evex_Wb_extensions[175][2],
    /* OP_vreduceps       */  &evex_Wb_extensions[175][0],
    /* OP_vreducesd       */  &evex_Wb_extensions[176][2],
    /* OP_vreducess       */  &evex_Wb_extensions[176][0],
    /* OP_vrndscalepd     */  &evex_Wb_extensions[218][2],
    /* OP_vrndscaleps     */  &evex_Wb_extensions[246][0],
    /* OP_vrndscalesd     */  &evex_Wb_extensions[254][2],
    /* OP_vrndscaless     */  &evex_Wb_extensions[253][0],
    /* OP_vrsqrt14pd      */  &evex_Wb_extensions[177][2],
    /* OP_vrsqrt14ps      */  &evex_Wb_extensions[177][0],
    /* OP_vrsqrt14sd      */  &evex_Wb_extensions[178][2],
    /* OP_vrsqrt14ss      */  &evex_Wb_extensions[178][0],
    /* OP_vrsqrt28pd      */  &evex_Wb_extensions[179][2],
    /* OP_vrsqrt28ps      */  &evex_Wb_extensions[179][0],
    /* OP_vrsqrt28sd      */  &evex_Wb_extensions[180][2],
    /* OP_vrsqrt28ss      */  &evex_Wb_extensions[180][0],
    /* OP_vscalefpd       */  &evex_Wb_extensions[181][2],
    /* OP_vscalefps       */  &evex_Wb_extensions[181][0],
    /* OP_vscalefsd       */  &evex_Wb_extensions[182][2],
    /* OP_vscalefss       */  &evex_Wb_extensions[182][0],
    /* OP_vscatterdpd     */  &evex_Wb_extensions[195][2],
    /* OP_vscatterdps     */  &evex_Wb_extensions[195][0],
    /* OP_vscatterqpd     */  &evex_Wb_extensions[196][2],
    /* OP_vscatterqps     */  &evex_Wb_extensions[196][0],
    /* OP_vscatterpf0dpd  */  &evex_Wb_extensions[201][2],
    /* OP_vscatterpf0dps  */  &evex_Wb_extensions[201][0],
    /* OP_vscatterpf0qpd  */  &evex_Wb_extensions[202][2],
    /* OP_vscatterpf0qps  */  &evex_Wb_extensions[202][0],
    /* OP_vscatterpf1dpd  */  &evex_Wb_extensions[203][2],
    /* OP_vscatterpf1dps  */  &evex_Wb_extensions[203][0],
    /* OP_vscatterpf1qpd  */  &evex_Wb_extensions[204][2],
    /* OP_vscatterpf1qps  */  &evex_Wb_extensions[204][0],
    /* OP_vshuff32x4      */  &evex_Wb_extensions[142][0],
    /* OP_vshuff64x2      */  &evex_Wb_extensions[142][2],
    /* OP_vshufi32x4      */  &evex_Wb_extensions[143][0],
    /* OP_vshufi64x2      */  &evex_Wb_extensions[143][2],

    /* Intel SHA extensions */
    /* OP_sha1msg1        */  &third_byte_38[165],
    /* OP_sha1msg2        */  &e_vex_extensions[144][0],
    /* OP_sha1nexte       */  &e_vex_extensions[145][0],
    /* OP_sha1rnds4       */  &third_byte_3a[89],
    /* OP_sha256msg1      */  &e_vex_extensions[147][0],
    /* OP_sha256msg2      */  &e_vex_extensions[148][0],
    /* OP_sha256rnds2     */  &e_vex_extensions[146][0],

    /* Intel MPX extensions */
    /* OP_bndcl           */ &prefix_extensions[186][1],
    /* OP_bndcn           */ &prefix_extensions[187][3],
    /* OP_bndcu           */ &prefix_extensions[186][3],
    /* OP_bndldx          */ &prefix_extensions[186][0],
    /* OP_bndmk           */ &prefix_extensions[187][1],
    /* OP_bndmov          */ &prefix_extensions[186][2],
    /* OP_bndstx          */ &prefix_extensions[187][0],

    /* Intel PT extensions */
    /* OP_ptwrite         */ &prefix_extensions[188][1],

   /* AMD monitor extensions */
   /* OP_monitorx      */   &rm_extensions[2][2],
   /* OP_mwaitx        */   &rm_extensions[2][3],

   /* Intel MPK extensions */
   /* OP_rdpkru       */    &rm_extensions[5][6],
   /* OP_wrpkru       */    &rm_extensions[5][7],

   /* Intel Software Guard eXtension. */
   /* OP_encls        */    &rm_extensions[1][7],
   /* OP_enclu        */    &rm_extensions[4][7],
   /* OP_enclv        */    &rm_extensions[0][0],

    /* AVX512 VNNI */
    /* OP_vpdpbusd        */  &vex_W_extensions[110][0],
    /* OP_vpdpbusds       */  &vex_W_extensions[111][0],
    /* OP_vpdpwssd        */  &vex_W_extensions[112][0],
    /* OP_vpdpwssds       */  &vex_W_extensions[113][0],

    /* AVX512 BF16 */
    /*  OP_vcvtne2ps2bf16, */ &evex_Wb_extensions[271][0],
    /*  OP_vcvtneps2bf16,  */ &evex_Wb_extensions[272][0],
    /*  OP_vdpbf16ps,      */ &evex_Wb_extensions[273][0],

    /* AVX512 VPOPCNTDQ */
    /* OP_vpopcntd, */ &evex_Wb_extensions[274][0],
    /* OP_vpopcntq, */ &evex_Wb_extensions[274][2],
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
#define Ey  TYPE_E, OPSZ_4_rex8
#define Rd_Mb TYPE_E, OPSZ_1_reg4
#define Rd_Mw TYPE_E, OPSZ_2_reg4
#define Gb  TYPE_G, OPSZ_1
#define Gw  TYPE_G, OPSZ_2
#define Gv  TYPE_G, OPSZ_4_rex8_short2
#define Gz  TYPE_G, OPSZ_4_short2
#define Gd  TYPE_G, OPSZ_4
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
#define Rd  TYPE_R, OPSZ_4
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
#define Ud_dq TYPE_V_MODRM, OPSZ_4_of_16
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
#define Wh_x TYPE_W, OPSZ_half_16_vex32
#define Wi_x TYPE_W, OPSZ_quarter_16_vex32
#define Wj_x TYPE_W, OPSZ_eighth_16_vex32
#define Wqq TYPE_W, OPSZ_32
#define Mvs TYPE_M, OPSZ_16_vex32
#define Mvd TYPE_M, OPSZ_16_vex32
#define Mx TYPE_M, OPSZ_16_vex32
#define Ldq TYPE_L, OPSZ_16 /* immed is 1 byte but reg is xmm */
#define Lx TYPE_L, OPSZ_16_vex32 /* immed is 1 byte but reg is xmm/ymm */
#define Lvs TYPE_L, OPSZ_16_vex32 /* immed is 1 byte but reg is xmm/ymm */
#define Lvd TYPE_L, OPSZ_16_vex32 /* immed is 1 byte but reg is xmm/ymm */
#define Lss TYPE_L, OPSZ_4_of_16 /* immed is 1 byte but reg is xmm/ymm */
#define Lsd TYPE_L, OPSZ_8_of_16 /* immed is 1 byte but reg is xmm/ymm */

/* AVX-512 additions */
#define KP1b TYPE_K_REG, OPSZ_1b
#define KPb TYPE_K_REG, OPSZ_1
#define KPw TYPE_K_REG, OPSZ_2
#define KPd TYPE_K_REG, OPSZ_4
#define KPq TYPE_K_REG, OPSZ_8
#define KRb TYPE_K_MODRM_R, OPSZ_1
#define KRw TYPE_K_MODRM_R, OPSZ_2
#define KRd TYPE_K_MODRM_R, OPSZ_4
#define KRq TYPE_K_MODRM_R, OPSZ_8
#define KQb TYPE_K_MODRM, OPSZ_1
#define KQw TYPE_K_MODRM, OPSZ_2
#define KQd TYPE_K_MODRM, OPSZ_4
#define KQq TYPE_K_MODRM, OPSZ_8
#define KVb TYPE_K_VEX, OPSZ_1
#define KVw TYPE_K_VEX, OPSZ_2
#define KVd TYPE_K_VEX, OPSZ_4
#define KVq TYPE_K_VEX, OPSZ_8
#define KE1b TYPE_K_EVEX, OPSZ_1b
#define KE2b TYPE_K_EVEX, OPSZ_2b
#define KE4b TYPE_K_EVEX, OPSZ_4b
#define KEb TYPE_K_EVEX, OPSZ_1
#define KEw TYPE_K_EVEX, OPSZ_2
#define KEd TYPE_K_EVEX, OPSZ_4
#define KEq TYPE_K_EVEX, OPSZ_8
#define Eq TYPE_E, OPSZ_8
#define Ves TYPE_V, OPSZ_16_vex32_evex64
#define Ved TYPE_V, OPSZ_16_vex32_evex64
#define Vf TYPE_V, OPSZ_vex32_evex64
#define Vfs TYPE_V, OPSZ_vex32_evex64
#define Vfd TYPE_V, OPSZ_vex32_evex64
#define Vdq_f TYPE_V, OPSZ_16_of_32_evex64
#define Vqq_oq TYPE_V, OPSZ_32_of_64
#define Voq TYPE_V, OPSZ_64
#define Wes TYPE_W, OPSZ_16_vex32_evex64
#define Wed TYPE_W, OPSZ_16_vex32_evex64
#define We TYPE_W, OPSZ_16_vex32_evex64
#define Wf TYPE_W, OPSZ_vex32_evex64
#define Wfs TYPE_W, OPSZ_vex32_evex64
#define Wfd TYPE_W, OPSZ_vex32_evex64
#define Wd_f TYPE_W, OPSZ_4_of_32_evex64
#define Wq_f TYPE_W, OPSZ_8_of_32_evex64
#define Ve TYPE_V, OPSZ_16_vex32_evex64
#define Vh_e TYPE_V, OPSZ_half_16_vex32_evex64
#define We TYPE_W, OPSZ_16_vex32_evex64
#define Wh_e TYPE_W, OPSZ_half_16_vex32_evex64
#define Wi_e TYPE_W, OPSZ_quarter_16_vex32_evex64
#define Wj_e TYPE_W, OPSZ_eighth_16_vex32_evex64
#define Woq TYPE_W, OPSZ_64
#define Hes TYPE_H, OPSZ_16_vex32_evex64
#define Hed TYPE_H, OPSZ_16_vex32_evex64
#define He TYPE_H, OPSZ_16_vex32_evex64
#define Hh_e TYPE_H, OPSZ_half_16_vex32_evex64
#define Hf TYPE_H, OPSZ_vex32_evex64
#define Hfs TYPE_H, OPSZ_vex32_evex64
#define Hfd TYPE_H, OPSZ_vex32_evex64
#define Hdq_f TYPE_H, OPSZ_16_of_32_evex64
#define Hoq TYPE_H, OPSZ_64
#define Mes TYPE_M, OPSZ_16_vex32_evex64
#define Med TYPE_M, OPSZ_16_vex32_evex64
#define Me TYPE_M, OPSZ_16_vex32_evex64
#define Ue TYPE_V_MODRM, OPSZ_16_vex32_evex64
#define Uqq TYPE_V_MODRM, OPSZ_32
#define Uoq TYPE_V_MODRM, OPSZ_64

/* MPX additions, own codes */
#define TRqdq TYPE_T_REG, OPSZ_8x16
#define TMqdq TYPE_T_MODRM, OPSZ_8x16
#define Er  TYPE_E, OPSZ_4x8
#define Mr  TYPE_M, OPSZ_4x8

/* my own codes
 * size m = 32 or 16 bit depending on addr size attribute
 * B=ds:eDI, Z=xlat's mem, K=float in mem, i_==indirect
 */
#define Mb  TYPE_M, OPSZ_1
#define Md  TYPE_M, OPSZ_4
#define My  TYPE_M, OPSZ_4_rex8
#define Mw  TYPE_M, OPSZ_2
#define Mm  TYPE_M, OPSZ_lea
#define Moq  TYPE_M, OPSZ_512
#define Mxsave TYPE_M, OPSZ_xsave
#define Mps  TYPE_M, OPSZ_16
#define Mpd  TYPE_M, OPSZ_16
#define Mss  TYPE_M, OPSZ_4
#define Msd  TYPE_M, OPSZ_8
#define Mq  TYPE_M, OPSZ_8
#define Mdq  TYPE_M, OPSZ_16
#define Mqq  TYPE_M, OPSZ_32
#define Mq_dq TYPE_M, OPSZ_8_rex16
#define Mv  TYPE_M, OPSZ_4_rex8_short2
#define MVd TYPE_VSIB, OPSZ_4
#define MVq TYPE_VSIB, OPSZ_8
#define Zb  TYPE_XLAT, OPSZ_1
#define Bq  TYPE_MASKMOVQ, OPSZ_8
#define Bdq  TYPE_MASKMOVQ, OPSZ_16
#define Fw  TYPE_FLOATMEM, OPSZ_2
#define Fd  TYPE_FLOATMEM, OPSZ_4
#define Fq  TYPE_FLOATMEM, OPSZ_8
#define Ffx  TYPE_FLOATMEM, OPSZ_10
#define Ffy  TYPE_FLOATMEM, OPSZ_28_short14 /* _14_ if data16 */
#define Ffz  TYPE_FLOATMEM, OPSZ_108_short94 /* _98_ if data16 */
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

#ifdef X64
#   define na_x11 TYPE_REG, DR_REG_R11
#else
#   define na_x11 TYPE_NONE, OPSZ_NA
#endif
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
#define evex     REQUIRES_EVEX
#define reqLL0   REQUIRES_EVEX_LL_0
#define vsiby    REQUIRES_VSIB_YMM
#define vsibz    REQUIRES_VSIB_ZMM
#define nok0     REQUIRES_NOT_K0
#define sae      EVEX_b_IS_SAE
/* er requires sae. */
#define er       (EVEX_b_IS_SAE | EVEX_L_LL_IS_ER)

/* flags used for AVX-512 compressed disp8 */
#define inopsz1  DR_EVEX_INPUT_OPSZ_1
#define inopsz2  DR_EVEX_INPUT_OPSZ_2
#define inopsz4  DR_EVEX_INPUT_OPSZ_4
#define inopsz8  DR_EVEX_INPUT_OPSZ_8

/* AVX-512 tupletype attributes. They are moved into the upper DR_TUPLE_TYPE_BITS
 * of the instr_info_t flags.
 */
#define ttnone ((uint)DR_TUPLE_TYPE_NONE << DR_TUPLE_TYPE_BITPOS)
#define ttfv   ((uint)DR_TUPLE_TYPE_FV << DR_TUPLE_TYPE_BITPOS)
#define tthv   ((uint)DR_TUPLE_TYPE_HV << DR_TUPLE_TYPE_BITPOS)
#define ttfvm  ((uint)DR_TUPLE_TYPE_FVM << DR_TUPLE_TYPE_BITPOS)
#define ttt1s  ((uint)DR_TUPLE_TYPE_T1S << DR_TUPLE_TYPE_BITPOS)
#define ttt1f  ((uint)DR_TUPLE_TYPE_T1F << DR_TUPLE_TYPE_BITPOS)
#define ttt2   ((uint)DR_TUPLE_TYPE_T2 << DR_TUPLE_TYPE_BITPOS)
#define ttt4   ((uint)DR_TUPLE_TYPE_T4 << DR_TUPLE_TYPE_BITPOS)
#define ttt8   ((uint)DR_TUPLE_TYPE_T8 << DR_TUPLE_TYPE_BITPOS)
#define tthvm  ((uint)DR_TUPLE_TYPE_HVM << DR_TUPLE_TYPE_BITPOS)
#define ttqvm  ((uint)DR_TUPLE_TYPE_QVM << DR_TUPLE_TYPE_BITPOS)
#define ttovm  ((uint)DR_TUPLE_TYPE_OVM << DR_TUPLE_TYPE_BITPOS)
#define ttm128 ((uint)DR_TUPLE_TYPE_M128 << DR_TUPLE_TYPE_BITPOS)
#define ttdup  ((uint)DR_TUPLE_TYPE_DUP << DR_TUPLE_TYPE_BITPOS)

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
#define tex (ptr_int_t)&base_extensions
#define t38 (ptr_int_t)&third_byte_38
#define t3a (ptr_int_t)&third_byte_3a
#define tpe (ptr_int_t)&prefix_extensions
#define tvex (ptr_int_t)&e_vex_extensions
#define modx (ptr_int_t)&mod_extensions
#define tre (ptr_int_t)&rep_extensions
#define tne (ptr_int_t)&repne_extensions
#define tfl (ptr_int_t)&float_low_modrm
#define tfh (ptr_int_t)&float_high_modrm
#define exop (ptr_int_t)&extra_operands
#define t64e (ptr_int_t)&x64_extensions
#define trexb (ptr_int_t)&rex_b_extensions
#define trexw (ptr_int_t)&rex_w_extensions
#define tvex (ptr_int_t)&e_vex_extensions
#define tvexw (ptr_int_t)&vex_W_extensions
#define txop (ptr_int_t)&xop_extensions
#define tevexwb (ptr_int_t)&evex_Wb_extensions

/****************************************************************************
 * One-byte opcodes
 * This is from Tables A-2 & A-3
 */
const instr_info_t first_byte[] = {
    /* {op/type, op encoding, name, dst1, dst2, src1, src2, src3, modrm?, eflags, code} */
    /* 00 */
    {OP_add,  0x000000, STROFF(add),  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][0]},
    {OP_add,  0x010000, STROFF(add),  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x00]},
    {OP_add,  0x020000, STROFF(add),  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x01]},
    {OP_add,  0x030000, STROFF(add),  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x02]},
    {OP_add,  0x040000, STROFF(add),  al, xx, Ib, al, xx, no,  fW6, tfb[0x03]},
    {OP_add,  0x050000, STROFF(add), eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x04]},
    {OP_push, 0x060000, STROFF(push), xsp, i_xSPo1, es, xsp, xx, i64, x, tfb[0x0e]},
    {OP_pop,  0x070000, STROFF(pop), es, xsp, xsp, i_xSP, xx, i64, x, tsb[0xa1]},
    /* 08 */
    {OP_or,  0x080000, STROFF(or),  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][1]},
    {OP_or,  0x090000, STROFF(or),  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x08]},
    {OP_or,  0x0a0000, STROFF(or),  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x09]},
    {OP_or,  0x0b0000, STROFF(or),  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x0a]},
    {OP_or,  0x0c0000, STROFF(or),  al, xx, Ib, al, xx, no,  fW6, tfb[0x0b]},
    {OP_or,  0x0d0000, STROFF(or), eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x0c]},
    {OP_push,0x0e0000, STROFF(push), xsp, i_xSPo1, cs, xsp, xx, i64, x, tfb[0x16]},
    {ESCAPE, 0x0f0000, STROFF(escape), xx, xx, xx, xx, xx, no, x, NA},
    /* 10 */
    {OP_adc,  0x100000, STROFF(adc),  Eb, xx, Gb, Eb, xx, mrm, (fW6|fRC), tex[1][2]},
    {OP_adc,  0x110000, STROFF(adc),  Ev, xx, Gv, Ev, xx, mrm, (fW6|fRC), tfb[0x10]},
    {OP_adc,  0x120000, STROFF(adc),  Gb, xx, Eb, Gb, xx, mrm, (fW6|fRC), tfb[0x11]},
    {OP_adc,  0x130000, STROFF(adc),  Gv, xx, Ev, Gv, xx, mrm, (fW6|fRC), tfb[0x12]},
    {OP_adc,  0x140000, STROFF(adc),  al, xx, Ib, al, xx, no,  (fW6|fRC), tfb[0x13]},
    {OP_adc,  0x150000, STROFF(adc), eAX, xx, Iz, eAX, xx, no,  (fW6|fRC), tfb[0x14]},
    {OP_push, 0x160000, STROFF(push), xsp, i_xSPo1, ss, xsp, xx, i64, x, tfb[0x1e]},
    {OP_pop,  0x170000, STROFF(pop), ss, xsp, xsp, i_xSP, xx, i64, x, tfb[0x1f]},
    /* 18 */
    {OP_sbb,  0x180000, STROFF(sbb),  Eb, xx, Gb, Eb, xx, mrm, (fW6|fRC), tex[1][3]},
    {OP_sbb,  0x190000, STROFF(sbb),  Ev, xx, Gv, Ev, xx, mrm, (fW6|fRC), tfb[0x18]},
    {OP_sbb,  0x1a0000, STROFF(sbb),  Gb, xx, Eb, Gb, xx, mrm, (fW6|fRC), tfb[0x19]},
    {OP_sbb,  0x1b0000, STROFF(sbb),  Gv, xx, Ev, Gv, xx, mrm, (fW6|fRC), tfb[0x1a]},
    {OP_sbb,  0x1c0000, STROFF(sbb),  al, xx, Ib, al, xx, no,  (fW6|fRC), tfb[0x1b]},
    {OP_sbb,  0x1d0000, STROFF(sbb), eAX, xx, Iz, eAX, xx, no,  (fW6|fRC), tfb[0x1c]},
    {OP_push, 0x1e0000, STROFF(push), xsp, i_xSPo1, ds, xsp, xx, i64, x, tsb[0xa0]},
    {OP_pop,  0x1f0000, STROFF(pop), ds, xsp, xsp, i_xSP, xx, i64, x, tfb[0x07]},
    /* 20 */
    {OP_and,  0x200000, STROFF(and),  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][4]},
    {OP_and,  0x210000, STROFF(and),  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x20]},
    {OP_and,  0x220000, STROFF(and),  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x21]},
    {OP_and,  0x230000, STROFF(and),  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x22]},
    {OP_and,  0x240000, STROFF(and),  al, xx, Ib, al, xx, no,  fW6, tfb[0x23]},
    {OP_and,  0x250000, STROFF(and), eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x24]},
    {PREFIX,  0x260000, STROFF(es), xx, xx, xx, xx, xx, no, x, SEG_ES},
    {OP_daa,  0x270000, STROFF(daa), al, xx, al, xx, xx, i64, (fW6|fRC|fRA), END_LIST},
    /* 28 */
    {OP_sub,  0x280000, STROFF(sub),  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][5]},
    {OP_sub,  0x290000, STROFF(sub),  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x28]},
    {OP_sub,  0x2a0000, STROFF(sub),  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x29]},
    {OP_sub,  0x2b0000, STROFF(sub),  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x2a]},
    {OP_sub,  0x2c0000, STROFF(sub),  al, xx, Ib, al, xx, no,  fW6, tfb[0x2b]},
    {OP_sub,  0x2d0000, STROFF(sub), eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x2c]},
    {PREFIX,  0x2e0000, STROFF(cs), xx, xx, xx, xx, xx, no, x, SEG_CS},
    {OP_das,  0x2f0000, STROFF(das), al, xx, al, xx, xx, i64, (fW6|fRC|fRA), END_LIST},
    /* 30 */
    {OP_xor,  0x300000, STROFF(xor),  Eb, xx, Gb, Eb, xx, mrm, fW6, tex[1][6]},
    {OP_xor,  0x310000, STROFF(xor),  Ev, xx, Gv, Ev, xx, mrm, fW6, tfb[0x30]},
    {OP_xor,  0x320000, STROFF(xor),  Gb, xx, Eb, Gb, xx, mrm, fW6, tfb[0x31]},
    {OP_xor,  0x330000, STROFF(xor),  Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x32]},
    {OP_xor,  0x340000, STROFF(xor),  al, xx, Ib, al, xx, no,  fW6, tfb[0x33]},
    {OP_xor,  0x350000, STROFF(xor), eAX, xx, Iz, eAX, xx, no,  fW6, tfb[0x34]},
    {PREFIX,  0x360000, STROFF(ss), xx, xx, xx, xx, xx, no, x, SEG_SS},
    {OP_aaa,  0x370000, STROFF(aaa), ax, xx, ax, xx, xx, i64, (fW6|fRA), END_LIST},
    /* 38 */
    {OP_cmp,  0x380000, STROFF(cmp), xx, xx,  Eb, Gb, xx, mrm, fW6, tex[1][7]},
    {OP_cmp,  0x390000, STROFF(cmp), xx, xx,  Ev, Gv, xx, mrm, fW6, tfb[0x38]},
    {OP_cmp,  0x3a0000, STROFF(cmp), xx, xx,  Gb, Eb, xx, mrm, fW6, tfb[0x39]},
    {OP_cmp,  0x3b0000, STROFF(cmp), xx, xx,  Gv, Ev, xx, mrm, fW6, tfb[0x3a]},
    {OP_cmp,  0x3c0000, STROFF(cmp), xx, xx,  al, Ib, xx, no,  fW6, tfb[0x3b]},
    {OP_cmp,  0x3d0000, STROFF(cmp), xx, xx, eAX, Iz, xx, no,  fW6, tfb[0x3c]},
    {PREFIX,  0x3e0000, STROFF(ds), xx, xx, xx, xx, xx, no, x, SEG_DS},
    {OP_aas,  0x3f0000, STROFF(aas), ax, xx, ax, xx, xx, i64, (fW6|fRA), END_LIST},
    /* 40 */
    {X64_EXT, 0x400000, STROFF(x64_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    {X64_EXT, 0x410000, STROFF(x64_ext_1), xx, xx, xx, xx, xx, no, x, 1},
    {X64_EXT, 0x420000, STROFF(x64_ext_2), xx, xx, xx, xx, xx, no, x, 2},
    {X64_EXT, 0x430000, STROFF(x64_ext_3), xx, xx, xx, xx, xx, no, x, 3},
    {X64_EXT, 0x440000, STROFF(x64_ext_4), xx, xx, xx, xx, xx, no, x, 4},
    {X64_EXT, 0x450000, STROFF(x64_ext_5), xx, xx, xx, xx, xx, no, x, 5},
    {X64_EXT, 0x460000, STROFF(x64_ext_6), xx, xx, xx, xx, xx, no, x, 6},
    {X64_EXT, 0x470000, STROFF(x64_ext_7), xx, xx, xx, xx, xx, no, x, 7},
    /* 48 */
    {X64_EXT, 0x480000, STROFF(x64_ext_8), xx, xx, xx, xx, xx, no, x, 8},
    {X64_EXT, 0x490000, STROFF(x64_ext_9), xx, xx, xx, xx, xx, no, x, 9},
    {X64_EXT, 0x4a0000, STROFF(x64_ext_10), xx, xx, xx, xx, xx, no, x, 10},
    {X64_EXT, 0x4b0000, STROFF(x64_ext_11), xx, xx, xx, xx, xx, no, x, 11},
    {X64_EXT, 0x4c0000, STROFF(x64_ext_12), xx, xx, xx, xx, xx, no, x, 12},
    {X64_EXT, 0x4d0000, STROFF(x64_ext_13), xx, xx, xx, xx, xx, no, x, 13},
    {X64_EXT, 0x4e0000, STROFF(x64_ext_14), xx, xx, xx, xx, xx, no, x, 14},
    {X64_EXT, 0x4f0000, STROFF(x64_ext_15), xx, xx, xx, xx, xx, no, x, 15},
    /* 50 */
    {OP_push,  0x500000, STROFF(push), xsp, i_xSPo1, xAX_x, xsp, xx, no, x, tfb[0x51]},
    {OP_push,  0x510000, STROFF(push), xsp, i_xSPo1, xCX_x, xsp, xx, no, x, tfb[0x52]},
    {OP_push,  0x520000, STROFF(push), xsp, i_xSPo1, xDX_x, xsp, xx, no, x, tfb[0x53]},
    {OP_push,  0x530000, STROFF(push), xsp, i_xSPo1, xBX_x, xsp, xx, no, x, tfb[0x54]},
    {OP_push,  0x540000, STROFF(push), xsp, i_xSPo1, xSP_x, xsp, xx, no, x, tfb[0x55]},
    {OP_push,  0x550000, STROFF(push), xsp, i_xSPo1, xBP_x, xsp, xx, no, x, tfb[0x56]},
    {OP_push,  0x560000, STROFF(push), xsp, i_xSPo1, xSI_x, xsp, xx, no, x, tfb[0x57]},
    {OP_push,  0x570000, STROFF(push), xsp, i_xSPo1, xDI_x, xsp, xx, no, x, tex[12][6]},
    /* 58 */
    {OP_pop,  0x580000, STROFF(pop), xAX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x59]},
    {OP_pop,  0x590000, STROFF(pop), xCX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5a]},
    {OP_pop,  0x5a0000, STROFF(pop), xDX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5b]},
    {OP_pop,  0x5b0000, STROFF(pop), xBX_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5c]},
    {OP_pop,  0x5c0000, STROFF(pop), xSP_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5d]},
    {OP_pop,  0x5d0000, STROFF(pop), xBP_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5e]},
    {OP_pop,  0x5e0000, STROFF(pop), xSI_x, xsp, xsp, i_xSP, xx, no, x, tfb[0x5f]},
    {OP_pop,  0x5f0000, STROFF(pop), xDI_x, xsp, xsp, i_xSP, xx, no, x, tex[26][0]},
    /* 60 */
    {OP_pusha, 0x600000, STROFF(pusha), xsp, i_xSPo8, xsp, eAX, eBX, xop|i64, x, exop[0x00]},
    {OP_popa,  0x610000, STROFF(popa), xsp, eAX, xsp, i_xSPs8, xx, xop|i64, x, exop[0x02]},
    {EVEX_PREFIX_EXT, 0x620000, STROFF(evex_prefix_ext), xx, xx, xx, xx, xx, no, x, END_LIST},
    {X64_EXT,  0x630000, STROFF(x64_ext_16), xx, xx, xx, xx, xx, no, x, 16},
    {PREFIX, 0x640000, STROFF(fs), xx, xx, xx, xx, xx, no, x, SEG_FS},
    {PREFIX, 0x650000, STROFF(gs), xx, xx, xx, xx, xx, no, x, SEG_GS},
    {PREFIX, 0x660000, STROFF(data_size), xx, xx, xx, xx, xx, no, x, PREFIX_DATA},
    {PREFIX, 0x670000, STROFF(addr_size), xx, xx, xx, xx, xx, no, x, PREFIX_ADDR},
    /* 68 */
    {OP_push_imm, 0x680000, STROFF(push), xsp, i_xSPo1, Iz, xsp, xx, no, x, tfb[0x6a]},
    {OP_imul,  0x690000, STROFF(imul), Gv, xx, Ev, Iz, xx, mrm, fW6, tfb[0x6b]},
    {OP_push_imm, 0x6a0000, STROFF(push), xsp, i_xSPo1, Ib, xsp, xx, no, x, END_LIST},/* sign-extend to push 2/4/8 bytes */
    {OP_imul,  0x6b0000, STROFF(imul), Gv, xx, Ev, Ib, xx, mrm, fW6, END_LIST},
    {REP_EXT,  0x6c0000, STROFF(rep_ins), Yb, xx, i_dx, xx, xx, no, fRD, 0},
    {REP_EXT,  0x6d0000, STROFF(rep_ins), Yz, xx, i_dx, xx, xx, no, fRD, 1},
    {REP_EXT,  0x6e0000, STROFF(rep_outs), i_dx, xx, Xb, xx, xx, no, fRD, 2},
    {REP_EXT,  0x6f0000, STROFF(rep_outs), i_dx, xx, Xz, xx, xx, no, fRD, 3},
    /* 70 */
    {OP_jo_short,  0x700000, STROFF(jo),  xx, xx, Jb, xx, xx, predcc, fRO, END_LIST},
    {OP_jno_short, 0x710000, STROFF(jno), xx, xx, Jb, xx, xx, predcc, fRO, END_LIST},
    {OP_jb_short,  0x720000, STROFF(jb),  xx, xx, Jb, xx, xx, predcc, fRC, END_LIST},
    {OP_jnb_short, 0x730000, STROFF(jnb), xx, xx, Jb, xx, xx, predcc, fRC, END_LIST},
    {OP_jz_short,  0x740000, STROFF(jz),  xx, xx, Jb, xx, xx, predcc, fRZ, END_LIST},
    {OP_jnz_short, 0x750000, STROFF(jnz), xx, xx, Jb, xx, xx, predcc, fRZ, END_LIST},
    {OP_jbe_short, 0x760000, STROFF(jbe), xx, xx, Jb, xx, xx, predcc, (fRC|fRZ), END_LIST},
    {OP_jnbe_short,0x770000, STROFF(jnbe),xx, xx, Jb, xx, xx, predcc, (fRC|fRZ), END_LIST},
    /* 78 */
    {OP_js_short,  0x780000, STROFF(js),  xx, xx, Jb, xx, xx, predcc, fRS, END_LIST},
    {OP_jns_short, 0x790000, STROFF(jns), xx, xx, Jb, xx, xx, predcc, fRS, END_LIST},
    {OP_jp_short,  0x7a0000, STROFF(jp),  xx, xx, Jb, xx, xx, predcc, fRP, END_LIST},
    {OP_jnp_short, 0x7b0000, STROFF(jnp), xx, xx, Jb, xx, xx, predcc, fRP, END_LIST},
    {OP_jl_short,  0x7c0000, STROFF(jl),  xx, xx, Jb, xx, xx, predcc, (fRS|fRO), END_LIST},
    {OP_jnl_short, 0x7d0000, STROFF(jnl), xx, xx, Jb, xx, xx, predcc, (fRS|fRO), END_LIST},
    {OP_jle_short, 0x7e0000, STROFF(jle), xx, xx, Jb, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
    {OP_jnle_short,0x7f0000, STROFF(jnle),xx, xx, Jb, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
    /* 80 */
    {EXTENSION, 0x800000, STROFF(group_1a), Eb, xx, Ib, xx, xx, mrm, x, 0},
    {EXTENSION, 0x810000, STROFF(group_1b), Ev, xx, Iz, xx, xx, mrm, x, 1},
    {EXTENSION, 0x820000, STROFF(group_1c), Ev, xx, Ib, xx, xx, mrm|i64, x, 25}, /* PR 235092: gnu tools (gdb, objdump) think this is a bad opcode but windbg and the hardware disagree */
    {EXTENSION, 0x830000, STROFF(group_1c), Ev, xx, Ib, xx, xx, mrm, x, 2},
    {OP_test,  0x840000, STROFF(test), xx, xx, Eb, Gb, xx, mrm, fW6, tex[10][0]},
    {OP_test,  0x850000, STROFF(test), xx, xx, Ev, Gv, xx, mrm, fW6, tfb[0x84]},
    {OP_xchg,  0x860000, STROFF(xchg), Eb, Gb, Eb, Gb, xx, mrm, x, END_LIST},
    {OP_xchg,  0x870000, STROFF(xchg), Ev, Gv, Ev, Gv, xx, mrm, x, tfb[0x86]},
    /* 88 */
    {OP_mov_st,  0x880000, STROFF(mov), Eb, xx, Gb, xx, xx, mrm, x, tex[18][0]},
    {OP_mov_st,  0x890000, STROFF(mov), Ev, xx, Gv, xx, xx, mrm, x, tfb[0x88]},
    {OP_mov_ld,  0x8a0000, STROFF(mov), Gb, xx, Eb, xx, xx, mrm, x, END_LIST},
    {OP_mov_ld,  0x8b0000, STROFF(mov), Gv, xx, Ev, xx, xx, mrm, x, tfb[0x8a]},
    {OP_mov_seg, 0x8c0000, STROFF(mov), Ev, xx, Sw, xx, xx, mrm, x, END_LIST},
    {OP_lea,  0x8d0000, STROFF(lea), Gv, xx, Mm, xx, xx, mrm, x, END_LIST}, /* Intel has just M */
    {OP_mov_seg, 0x8e0000, STROFF(mov), Sw, xx, Ev, xx, xx, mrm, x, tfb[0x8c]},
    {XOP_PREFIX_EXT, 0x8f0000, STROFF(xop_prefix_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    /* 90 */
    {PREFIX_EXT, 0x900000, STROFF(prefix_ext_103), xx, xx, xx, xx, xx, no, x, 103},
    {OP_xchg, 0x910000, STROFF(xchg), eCX_x, eAX, eCX_x, eAX, xx, no, x, tfb[0x92]},
    {OP_xchg, 0x920000, STROFF(xchg), eDX_x, eAX, eDX_x, eAX, xx, no, x, tfb[0x93]},
    {OP_xchg, 0x930000, STROFF(xchg), eBX_x, eAX, eBX_x, eAX, xx, no, x, tfb[0x94]},
    {OP_xchg, 0x940000, STROFF(xchg), eSP_x, eAX, eSP_x, eAX, xx, no, x, tfb[0x95]},
    {OP_xchg, 0x950000, STROFF(xchg), eBP_x, eAX, eBP_x, eAX, xx, no, x, tfb[0x96]},
    {OP_xchg, 0x960000, STROFF(xchg), eSI_x, eAX, eSI_x, eAX, xx, no, x, tfb[0x97]},
    {OP_xchg, 0x970000, STROFF(xchg), eDI_x, eAX, eDI_x, eAX, xx, no, x, tfb[0x87]},
    /* 98 */
    {OP_cwde, 0x980000, STROFF(cwde), eAX, xx, ax, xx, xx, no, x, END_LIST},/*16-bit=="cbw", src is al not ax; FIXME: newer gdb calls it "cwtl"?!?*/
    /* PR 354096: does not write to ax/eax/rax: sign-extends into dx/edx/rdx */
    {OP_cdq,  0x990000, STROFF(cdq), eDX, xx, eAX, xx, xx, no, x, END_LIST},/*16-bit=="cwd";64-bit=="cqo"*/
    {OP_call_far, 0x9a0000, STROFF(lcall),  xsp, i_vSPo2, Ap, xsp, xx, i64, x, END_LIST},
    {OP_fwait, 0x9b0000, STROFF(fwait), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pushf, 0x9c0000, STROFF(pushf), xsp, i_xSPo1, xsp, xx, xx, no, fRX, END_LIST},
    {OP_popf,  0x9d0000, STROFF(popf), xsp, xx, xsp, i_xSP, xx, no, fWX, END_LIST},
    {OP_sahf,  0x9e0000, STROFF(sahf), xx, xx, ah, xx, xx, no, (fW6&(~fWO)), END_LIST},
    {OP_lahf,  0x9f0000, STROFF(lahf), ah, xx, xx, xx, xx, no, (fR6&(~fRO)), END_LIST},
    /* a0 */
    {OP_mov_ld,  0xa00000, STROFF(mov), al, xx, Ob, xx, xx, no, x, tfb[0x8b]},
    {OP_mov_ld,  0xa10000, STROFF(mov), eAX, xx, Ov, xx, xx, no, x, tfb[0xa0]},
    {OP_mov_st,  0xa20000, STROFF(mov), Ob, xx, al, xx, xx, no, x, tfb[0x89]},
    {OP_mov_st,  0xa30000, STROFF(mov), Ov, xx, eAX, xx, xx, no, x, tfb[0xa2]},
    {REP_EXT, 0xa40000, STROFF(rep_movs), Yb, xx, Xb, xx, xx, no, fRD, 4},
    {REP_EXT, 0xa50000, STROFF(rep_movs), Yv, xx, Xv, xx, xx, no, fRD, 5},
    {REPNE_EXT, 0xa60000, STROFF(rep_ne_cmps), Xb, xx, Yb, xx, xx, no, (fW6|fRD|fRZ), 0},
    {REPNE_EXT, 0xa70000, STROFF(rep_ne_cmps), Xv, xx, Yv, xx, xx, no, (fW6|fRD|fRZ), 1},
    /* a8 */
    {OP_test,  0xa80000, STROFF(test), xx, xx,  al, Ib, xx, no, fW6, tfb[0x85]},
    {OP_test,  0xa90000, STROFF(test), xx, xx, eAX, Iz, xx, no, fW6, tfb[0xa8]},
    {REP_EXT, 0xaa0000, STROFF(rep_stos), Yb, xx, al, xx, xx, no, fRD, 6},
    {REP_EXT, 0xab0000, STROFF(rep_stos), Yv, xx, eAX, xx, xx, no, fRD, 7},
    {REP_EXT, 0xac0000, STROFF(rep_lods), al, xx, Xb, xx, xx, no, fRD, 8},
    {REP_EXT, 0xad0000, STROFF(rep_lods), eAX, xx, Xv, xx, xx, no, fRD, 9},
    {REPNE_EXT, 0xae0000, STROFF(rep_ne_scas), al, xx, Yb, xx, xx, no, (fW6|fRD|fRZ), 2},
    {REPNE_EXT, 0xaf0000, STROFF(rep_ne_scas), eAX, xx, Yv, xx, xx, no, (fW6|fRD|fRZ), 3},
    /* b0 */
    {OP_mov_imm, 0xb00000, STROFF(mov), al_x, xx, Ib, xx, xx, no, x, tfb[0xb1]},
    {OP_mov_imm, 0xb10000, STROFF(mov), cl_x, xx, Ib, xx, xx, no, x, tfb[0xb2]},
    {OP_mov_imm, 0xb20000, STROFF(mov), dl_x, xx, Ib, xx, xx, no, x, tfb[0xb3]},
    {OP_mov_imm, 0xb30000, STROFF(mov), bl_x, xx, Ib, xx, xx, no, x, tfb[0xb4]},
    {OP_mov_imm, 0xb40000, STROFF(mov), ah_x, xx, Ib, xx, xx, no, x, tfb[0xb5]},
    {OP_mov_imm, 0xb50000, STROFF(mov), ch_x, xx, Ib, xx, xx, no, x, tfb[0xb6]},
    {OP_mov_imm, 0xb60000, STROFF(mov), dh_x, xx, Ib, xx, xx, no, x, tfb[0xb7]},
    /* PR 250397: we point at the tail end of the mov_st templates */
    {OP_mov_imm, 0xb70000, STROFF(mov), bh_x, xx, Ib, xx, xx, no, x, tex[18][0]},
    /* b8 */
    {OP_mov_imm, 0xb80000, STROFF(mov), eAX_x, xx, Iv, xx, xx, no, x, tfb[0xb9]},
    {OP_mov_imm, 0xb90000, STROFF(mov), eCX_x, xx, Iv, xx, xx, no, x, tfb[0xba]},
    {OP_mov_imm, 0xba0000, STROFF(mov), eDX_x, xx, Iv, xx, xx, no, x, tfb[0xbb]},
    {OP_mov_imm, 0xbb0000, STROFF(mov), eBX_x, xx, Iv, xx, xx, no, x, tfb[0xbc]},
    {OP_mov_imm, 0xbc0000, STROFF(mov), eSP_x, xx, Iv, xx, xx, no, x, tfb[0xbd]},
    {OP_mov_imm, 0xbd0000, STROFF(mov), eBP_x, xx, Iv, xx, xx, no, x, tfb[0xbe]},
    {OP_mov_imm, 0xbe0000, STROFF(mov), eSI_x, xx, Iv, xx, xx, no, x, tfb[0xbf]},
    {OP_mov_imm, 0xbf0000, STROFF(mov), eDI_x, xx, Iv, xx, xx, no, x, tfb[0xb0]},
    /* c0 */
    {EXTENSION, 0xc00000, STROFF(group_2a), Eb, xx, Ib, xx, xx, mrm, x, 3},
    {EXTENSION, 0xc10000, STROFF(group_2b), Ev, xx, Ib, xx, xx, mrm, x, 4},
    {OP_ret,  0xc20000, STROFF(ret), xsp, xx, Iw, xsp, i_iSP, no, x, tfb[0xc3]},
    {OP_ret,  0xc30000, STROFF(ret), xsp, xx, xsp, i_iSP, xx, no, x, END_LIST},
    {VEX_PREFIX_EXT, 0xc40000, STROFF(vex_prefix_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    {VEX_PREFIX_EXT, 0xc50000, STROFF(vex_prefix_ext_1), xx, xx, xx, xx, xx, no, x, 1},
    {EXTENSION, 0xc60000, STROFF(group_11a), Eb, xx, Ib, xx, xx, mrm, x, 17},
    {EXTENSION, 0xc70000, STROFF(group_11b), Ev, xx, Iz, xx, xx, mrm, x, 18},
    /* c8 */
    {OP_enter,  0xc80000, STROFF(enter), xsp, i_xSPoN, Iw, Ib, xsp, xop, x, exop[0x05]},
    {OP_leave,  0xc90000, STROFF(leave), xsp, xbp, xbp, xsp, i_xBP, no, x, END_LIST},
    {OP_ret_far,  0xca0000, STROFF(lret), xsp, xx, Iw, xsp, i_vSPs2, no, x, tfb[0xcb]},
    {OP_ret_far,  0xcb0000, STROFF(lret), xsp, xx, xsp, i_vSPs2, xx, no, x, END_LIST},
    /* we ignore the operations on the kernel stack */
    {OP_int3, 0xcc0000, STROFF(int3), xx, xx, xx, xx, xx, no, fINT, END_LIST},
    {OP_int,  0xcd0000, STROFF(int),  xx, xx, Ib, xx, xx, no, fINT, END_LIST},
    {OP_into, 0xce0000, STROFF(into), xx, xx, xx, xx, xx, i64, fINT, END_LIST},
    {OP_iret, 0xcf0000, STROFF(iret), xsp, xx, xsp, i_vSPs3, xx, no, fWX, END_LIST},
    /* d0 */
    {EXTENSION, 0xd00000, STROFF(group_2c), Eb, xx, c1,  xx, xx, mrm, x, 5},
    {EXTENSION, 0xd10000, STROFF(group_2d), Ev, xx, c1,  xx, xx, mrm, x, 6},
    {EXTENSION, 0xd20000, STROFF(group_2e), Eb, xx, cl, xx, xx, mrm, x, 7},
    {EXTENSION, 0xd30000, STROFF(group_2f), Ev, xx, cl, xx, xx, mrm, x, 8},
    {OP_aam,  0xd40000, STROFF(aam), ax, xx, Ib, ax, xx, i64, fW6, END_LIST},
    {OP_aad,  0xd50000, STROFF(aad), ax, xx, Ib, ax, xx, i64, fW6, END_LIST},
    {OP_salc,  0xd60000, STROFF(salc), al, xx, xx, xx, xx, i64, fRC, END_LIST},/*undocumented*/
    {OP_xlat,  0xd70000, STROFF(xlat), al, xx, Zb, xx, xx, no, x, END_LIST},
    /* d8 */
    {FLOAT_EXT, 0xd80000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},/* all floats need modrm */
    {FLOAT_EXT, 0xd90000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xda0000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdb0000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdc0000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdd0000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xde0000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    {FLOAT_EXT, 0xdf0000, STROFF(float), xx, xx, xx, xx, xx, mrm, x, NA},
    /* e0 */
    {OP_loopne,0xe00000, STROFF(loopne), axCX, xx, Jb, axCX, xx, no, fRZ, END_LIST},
    {OP_loope, 0xe10000, STROFF(loope),  axCX, xx, Jb, axCX, xx, no, fRZ, END_LIST},
    {OP_loop,  0xe20000, STROFF(loop),   axCX, xx, Jb, axCX, xx, no, x, END_LIST},
    {OP_jecxz, 0xe30000, STROFF(jecxz),  xx, xx, Jb, axCX, xx, no, x, END_LIST},/*16-bit=="jcxz",64-bit="jrcxz"*/
    /* FIXME: in & out access "I/O ports", are these memory addresses?
     * if so, change Ib to Ob and change dx to i_dx (move to dest for out)
     */
    {OP_in,  0xe40000, STROFF(in), al, xx, Ib, xx, xx, no, x, tfb[0xed]},
    {OP_in,  0xe50000, STROFF(in), zAX, xx, Ib, xx, xx, no, x, tfb[0xe4]},
    {OP_out,  0xe60000, STROFF(out), xx, xx, Ib, al, xx, no, x, tfb[0xef]},
    {OP_out,  0xe70000, STROFF(out), xx, xx, Ib, zAX, xx, no, x, tfb[0xe6]},
    /* e8 */
    {OP_call,     0xe80000, STROFF(call),  xsp, i_iSPo1, Jz, xsp, xx, no, x, END_LIST},
    {OP_jmp,       0xe90000, STROFF(jmp), xx, xx, Jz, xx, xx, no, x, END_LIST},
    {OP_jmp_far,   0xea0000, STROFF(ljmp), xx, xx, Ap, xx, xx, i64, x, END_LIST},
    {OP_jmp_short, 0xeb0000, STROFF(jmp), xx, xx, Jb, xx, xx, no, x, END_LIST},
    {OP_in,  0xec0000, STROFF(in), al, xx, dx, xx, xx, no, x, END_LIST},
    {OP_in,  0xed0000, STROFF(in), zAX, xx, dx, xx, xx, no, x, tfb[0xec]},
    {OP_out,  0xee0000, STROFF(out), xx, xx, al, dx, xx, no, x, END_LIST},
    {OP_out,  0xef0000, STROFF(out), xx, xx, zAX, dx, xx, no, x, tfb[0xee]},
    /* f0 */
    {PREFIX, 0xf00000, STROFF(lock), xx, xx, xx, xx, xx, no, x, PREFIX_LOCK},
    /* Also called OP_icebp.  Undocumented.  I'm assuming looks like OP_int* */
    {OP_int1, 0xf10000, STROFF(int1), xx, xx, xx, xx, xx, no, fINT, END_LIST},
    {PREFIX, 0xf20000, STROFF(repne), xx, xx, xx, xx, xx, no, x, PREFIX_REPNE},
    {PREFIX, 0xf30000, STROFF(rep), xx, xx, xx, xx, xx, no, x, PREFIX_REP},
    {OP_hlt,  0xf40000, STROFF(hlt), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_cmc,  0xf50000, STROFF(cmc), xx, xx, xx, xx, xx, no, fWC, END_LIST},
    {EXTENSION, 0xf60000, STROFF(group_3a), Eb, xx, xx, xx, xx, mrm, x, 9},
    {EXTENSION, 0xf70000, STROFF(group_3b), Ev, xx, xx, xx, xx, mrm, x, 10},
    /* f8 */
    {OP_clc,  0xf80000, STROFF(clc), xx, xx, xx, xx, xx, no, fWC, END_LIST},
    {OP_stc,  0xf90000, STROFF(stc), xx, xx, xx, xx, xx, no, fWC, END_LIST},
    {OP_cli,  0xfa0000, STROFF(cli), xx, xx, xx, xx, xx, no, fWI, END_LIST},
    {OP_sti,  0xfb0000, STROFF(sti), xx, xx, xx, xx, xx, no, fWI, END_LIST},
    {OP_cld,  0xfc0000, STROFF(cld), xx, xx, xx, xx, xx, no, fWD, END_LIST},
    {OP_std,  0xfd0000, STROFF(std), xx, xx, xx, xx, xx, no, fWD, END_LIST},
    {EXTENSION, 0xfe0000, STROFF(group_4), xx, xx, xx, xx, xx, mrm, x, 11},
    {EXTENSION, 0xff0000, STROFF(group_5), xx, xx, xx, xx, xx, mrm, x, 12},
};
/****************************************************************************
 * Two-byte opcodes
 * This is from Tables A-4 & A-5
 */
const instr_info_t second_byte[] = {
  /* 00 */
  {EXTENSION, 0x0f0010, STROFF(group_6), xx, xx, xx, xx, xx, mrm, x, 13},
  {EXTENSION, 0x0f0110, STROFF(group_7), xx, xx, xx, xx, xx, mrm, x, 14},
  {OP_lar, 0x0f0210, STROFF(lar), Gv, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
  {OP_lsl, 0x0f0310, STROFF(lsl), Gv, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
  {INVALID, 0x0f0410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  /* XXX: writes ss and cs */
  {OP_syscall, 0x0f0510, STROFF(syscall), xcx, na_x11, xx, xx, xx, no, x, NA}, /* AMD/x64 only */
  {OP_clts, 0x0f0610, STROFF(clts), xx, xx, xx, xx, xx, no, x, END_LIST},
  /* XXX: writes ss and cs */
  {OP_sysret, 0x0f0710, STROFF(sysret), xx, xx, xcx, na_x11, xx, no, x, NA}, /* AMD/x64 only */
  /* 08 */
  {OP_invd, 0x0f0810, STROFF(invd), xx, xx, xx, xx, xx, no, x, END_LIST},
  {OP_wbinvd, 0x0f0910, STROFF(wbinvd), xx, xx, xx, xx, xx, no, x, END_LIST},
  {INVALID, 0x0f0a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {OP_ud2, 0x0f0b10, STROFF(ud2), xx, xx, xx, xx, xx, no, x, END_LIST}, /* "undefined instr" instr */
  {INVALID, 0x0f0c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {EXTENSION, 0x0f0d10, STROFF(group_amd), xx, xx, xx, xx, xx, mrm, x, 24}, /* AMD only */
  {OP_femms, 0x0f0e10, STROFF(femms), xx, xx, xx, xx, xx, no, x, END_LIST},
  {SUFFIX_EXT, 0x0f0f10, STROFF(group_3DNow), xx, xx, xx, xx, xx, mrm, x, 0},
  /* 10 */
  {PREFIX_EXT, 0x0f1010, STROFF(prefix_ext_0), xx, xx, xx, xx, xx, mrm, x, 0},
  {PREFIX_EXT, 0x0f1110, STROFF(prefix_ext_1), xx, xx, xx, xx, xx, mrm, x, 1},
  {PREFIX_EXT, 0x0f1210, STROFF(prefix_ext_2), xx, xx, xx, xx, xx, mrm, x, 2},
  {PREFIX_EXT, 0x0f1310, STROFF(prefix_ext_3), xx, xx, xx, xx, xx, mrm, x, 3},
  {PREFIX_EXT, 0x0f1410, STROFF(prefix_ext_4), xx, xx, xx, xx, xx, mrm, x, 4},
  {PREFIX_EXT, 0x0f1510, STROFF(prefix_ext_5), xx, xx, xx, xx, xx, mrm, x, 5},
  {PREFIX_EXT, 0x0f1610, STROFF(prefix_ext_6), xx, xx, xx, xx, xx, mrm, x, 6},
  {PREFIX_EXT, 0x0f1710, STROFF(prefix_ext_7), xx, xx, xx, xx, xx, mrm, x, 7},
  /* 18 */
  {EXTENSION, 0x0f1810, STROFF(group_16), xx, xx, xx, xx, xx, mrm, x, 23},
  /* xref case 9862/PR 214297 : 0f19-0f1e are "HINT_NOP": valid on P6+.
   * we treat them the same as 0f1f but do not put on encoding chain.
   * The operand is ignored but to support encoding it we must list it.
   * i453: analysis routines now special case nop_modrm to ignore src opnd */
  {OP_nop_modrm, 0x0f1910, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {PREFIX_EXT, 0x0f1a10, STROFF(prefix_ext_186), xx, xx, xx, xx, xx, mrm, x, 186},
  {PREFIX_EXT, 0x0f1b10, STROFF(prefix_ext_187), xx, xx, xx, xx, xx, mrm, x, 187},
  {OP_nop_modrm, 0x0f1c10, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1d10, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1e10, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  {OP_nop_modrm, 0x0f1f10, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
  /* 20 */
  {OP_mov_priv, 0x0f2010, STROFF(mov), Rr, xx, Cr, xx, xx, mrm, fW6, tsb[0x21]},
  {OP_mov_priv, 0x0f2110, STROFF(mov), Rr, xx, Dr, xx, xx, mrm, fW6, tsb[0x22]},
  {OP_mov_priv, 0x0f2210, STROFF(mov), Cr, xx, Rr, xx, xx, mrm, fW6, tsb[0x23]},
  {OP_mov_priv, 0x0f2310, STROFF(mov), Dr, xx, Rr, xx, xx, mrm, fW6, END_LIST},
  {INVALID, 0x0f2410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* FIXME: gdb thinks ok! */
  {INVALID, 0x0f2510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f2610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* FIXME: gdb thinks ok! */
  {INVALID, 0x0f2710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  /* 28 */
  {PREFIX_EXT, 0x0f2810, STROFF(prefix_ext_8), xx, xx, xx, xx, xx, mrm, x, 8},
  {PREFIX_EXT, 0x0f2910, STROFF(prefix_ext_9), xx, xx, xx, xx, xx, mrm, x, 9},
  {PREFIX_EXT, 0x0f2a10, STROFF(prefix_ext_10), xx, xx, xx, xx, xx, mrm, x, 10},
  {PREFIX_EXT, 0x0f2b10, STROFF(prefix_ext_11), xx, xx, xx, xx, xx, mrm, x, 11},
  {PREFIX_EXT, 0x0f2c10, STROFF(prefix_ext_12), xx, xx, xx, xx, xx, mrm, x, 12},
  {PREFIX_EXT, 0x0f2d10, STROFF(prefix_ext_13), xx, xx, xx, xx, xx, mrm, x, 13},
  {PREFIX_EXT, 0x0f2e10, STROFF(prefix_ext_14), xx, xx, xx, xx, xx, mrm, x, 14},
  {PREFIX_EXT, 0x0f2f10, STROFF(prefix_ext_15), xx, xx, xx, xx, xx, mrm, x, 15},
  /* 30 */
  {OP_wrmsr, 0x0f3010, STROFF(wrmsr), xx, xx, edx, eax, ecx, no, x, END_LIST},
  {OP_rdtsc, 0x0f3110, STROFF(rdtsc), edx, eax, xx, xx, xx, no, x, END_LIST},
  {OP_rdmsr, 0x0f3210, STROFF(rdmsr), edx, eax, ecx, xx, xx, no, x, END_LIST},
  {OP_rdpmc, 0x0f3310, STROFF(rdpmc), edx, eax, ecx, xx, xx, no, x, END_LIST},
  /* XXX: sysenter writes cs and ss */
  {OP_sysenter, 0x0f3410, STROFF(sysenter), xsp, xx, xx, xx, xx, no, x, END_LIST},
  /* XXX: sysexit writes cs and ss */
  {OP_sysexit, 0x0f3510, STROFF(sysexit), xsp, xx, xcx, xx, xx, no, x, END_LIST},
  {INVALID, 0x0f3610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  /* XXX i#1313: various getsec leaf funcs at CPL 0 write to all kinds of
   * processor state including eflags and eip.  Leaf funcs are indicated by eax
   * value, though.  Here we only model the CPL > 0 effects, which conditionally
   * write to ebx + ecx.
   */
  {OP_getsec, 0x0f3710, STROFF(getsec), eax, ebx, eax, ebx, xx, xop|predcx, x, exop[13]},
  /* 38 */
  {ESCAPE_3BYTE_38, 0x0f3810, STROFF(3byte_38), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {ESCAPE_3BYTE_3a, 0x0f3a10, STROFF(3byte_3a), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0f3f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  /* 40 */
  {OP_cmovo,   0x0f4010, STROFF(cmovo),  Gv, xx, Ev, xx, xx, mrm|predcc, fRO, END_LIST},
  {E_VEX_EXT, 0x0f4110, STROFF(e_vex_ext_83), xx, xx, xx, xx, xx, mrm, x, 83},
  {E_VEX_EXT, 0x0f4210, STROFF(e_vex_ext_84), xx, xx, xx, xx, xx, mrm, x, 84},
  {OP_cmovnb,  0x0f4310, STROFF(cmovnb), Gv, xx, Ev, xx, xx, mrm|predcc, fRC, END_LIST},
  {E_VEX_EXT, 0x0f4410, STROFF(e_vex_ext_86), xx, xx, xx, xx, xx, mrm, x, 86},
  {E_VEX_EXT, 0x0f4510, STROFF(e_vex_ext_87), xx, xx, xx, xx, xx, mrm, x, 87},
  {E_VEX_EXT, 0x0f4610, STROFF(e_vex_ext_88), xx, xx, xx, xx, xx, mrm, x, 88},
  {E_VEX_EXT, 0x0f4710, STROFF(e_vex_ext_89), xx, xx, xx, xx, xx, mrm, x, 89},
  /* 48 */
  {OP_cmovs,  0x0f4810, STROFF(cmovs),  Gv, xx, Ev, xx, xx, mrm|predcc, fRS, END_LIST},
  {OP_cmovns, 0x0f4910, STROFF(cmovns), Gv, xx, Ev, xx, xx, mrm|predcc, fRS, END_LIST},
  {E_VEX_EXT, 0x0f4a10, STROFF(e_vex_ext_90), xx, xx, xx, xx, xx, mrm, x, 90},
  {E_VEX_EXT, 0x0f4b10, STROFF(e_vex_ext_85), xx, xx, xx, xx, xx, mrm, x, 85},
  {OP_cmovl,  0x0f4c10, STROFF(cmovl),  Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO), END_LIST},
  {OP_cmovnl, 0x0f4d10, STROFF(cmovnl), Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO), END_LIST},
  {OP_cmovle, 0x0f4e10, STROFF(cmovle), Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO|fRZ), END_LIST},
  {OP_cmovnle,0x0f4f10, STROFF(cmovnle),Gv, xx, Ev, xx, xx, mrm|predcc, (fRS|fRO|fRZ), END_LIST},
  /* 50 */
  {PREFIX_EXT, 0x0f5010, STROFF(prefix_ext_16), xx, xx, xx, xx, xx, mrm, x, 16},
  {PREFIX_EXT, 0x0f5110, STROFF(prefix_ext_17), xx, xx, xx, xx, xx, mrm, x, 17},
  {PREFIX_EXT, 0x0f5210, STROFF(prefix_ext_18), xx, xx, xx, xx, xx, mrm, x, 18},
  {PREFIX_EXT, 0x0f5310, STROFF(prefix_ext_19), xx, xx, xx, xx, xx, mrm, x, 19},
  {PREFIX_EXT, 0x0f5410, STROFF(prefix_ext_20), xx, xx, xx, xx, xx, mrm, x, 20},
  {PREFIX_EXT, 0x0f5510, STROFF(prefix_ext_21), xx, xx, xx, xx, xx, mrm, x, 21},
  {PREFIX_EXT, 0x0f5610, STROFF(prefix_ext_22), xx, xx, xx, xx, xx, mrm, x, 22},
  {PREFIX_EXT, 0x0f5710, STROFF(prefix_ext_23), xx, xx, xx, xx, xx, mrm, x, 23},
  /* 58 */
  {PREFIX_EXT, 0x0f5810, STROFF(prefix_ext_24), xx, xx, xx, xx, xx, mrm, x, 24},
  {PREFIX_EXT, 0x0f5910, STROFF(prefix_ext_25), xx, xx, xx, xx, xx, mrm, x, 25},
  {PREFIX_EXT, 0x0f5a10, STROFF(prefix_ext_26), xx, xx, xx, xx, xx, mrm, x, 26},
  {PREFIX_EXT, 0x0f5b10, STROFF(prefix_ext_27), xx, xx, xx, xx, xx, mrm, x, 27},
  {PREFIX_EXT, 0x0f5c10, STROFF(prefix_ext_28), xx, xx, xx, xx, xx, mrm, x, 28},
  {PREFIX_EXT, 0x0f5d10, STROFF(prefix_ext_29), xx, xx, xx, xx, xx, mrm, x, 29},
  {PREFIX_EXT, 0x0f5e10, STROFF(prefix_ext_30), xx, xx, xx, xx, xx, mrm, x, 30},
  {PREFIX_EXT, 0x0f5f10, STROFF(prefix_ext_31), xx, xx, xx, xx, xx, mrm, x, 31},
  /* 60 */
  {PREFIX_EXT, 0x0f6010, STROFF(prefix_ext_32), xx, xx, xx, xx, xx, mrm, x, 32},
  {PREFIX_EXT, 0x0f6110, STROFF(prefix_ext_33), xx, xx, xx, xx, xx, mrm, x, 33},
  {PREFIX_EXT, 0x0f6210, STROFF(prefix_ext_34), xx, xx, xx, xx, xx, mrm, x, 34},
  {PREFIX_EXT, 0x0f6310, STROFF(prefix_ext_35), xx, xx, xx, xx, xx, mrm, x, 35},
  {PREFIX_EXT, 0x0f6410, STROFF(prefix_ext_36), xx, xx, xx, xx, xx, mrm, x, 36},
  {PREFIX_EXT, 0x0f6510, STROFF(prefix_ext_37), xx, xx, xx, xx, xx, mrm, x, 37},
  {PREFIX_EXT, 0x0f6610, STROFF(prefix_ext_38), xx, xx, xx, xx, xx, mrm, x, 38},
  {PREFIX_EXT, 0x0f6710, STROFF(prefix_ext_39), xx, xx, xx, xx, xx, mrm, x, 39},
  /* 68 */
  {PREFIX_EXT, 0x0f6810, STROFF(prefix_ext_40), xx, xx, xx, xx, xx, mrm, x, 40},
  {PREFIX_EXT, 0x0f6910, STROFF(prefix_ext_41), xx, xx, xx, xx, xx, mrm, x, 41},
  {PREFIX_EXT, 0x0f6a10, STROFF(prefix_ext_42), xx, xx, xx, xx, xx, mrm, x, 42},
  {PREFIX_EXT, 0x0f6b10, STROFF(prefix_ext_43), xx, xx, xx, xx, xx, mrm, x, 43},
  {PREFIX_EXT, 0x0f6c10, STROFF(prefix_ext_44), xx, xx, xx, xx, xx, mrm, x, 44},
  {PREFIX_EXT, 0x0f6d10, STROFF(prefix_ext_45), xx, xx, xx, xx, xx, mrm, x, 45},
  {PREFIX_EXT, 0x0f6e10, STROFF(prefix_ext_46), xx, xx, xx, xx, xx, mrm, x, 46},
  {PREFIX_EXT, 0x0f6f10, STROFF(prefix_ext_112), xx, xx, xx, xx, xx, mrm, x, 112},
  /* 70 */
  {PREFIX_EXT, 0x0f7010, STROFF(prefix_ext_47), xx, xx, xx, xx, xx, mrm, x, 47},
  {EXTENSION, 0x0f7110, STROFF(group_12), xx, xx, xx, xx, xx, mrm, x, 19},
  {EXTENSION, 0x0f7210, STROFF(group_13), xx, xx, xx, xx, xx, mrm, x, 20},
  {EXTENSION, 0x0f7310, STROFF(group_14), xx, xx, xx, xx, xx, mrm, x, 21},
  {PREFIX_EXT, 0x0f7410, STROFF(prefix_ext_48), xx, xx, xx, xx, xx, mrm, x, 48},
  {PREFIX_EXT, 0x0f7510, STROFF(prefix_ext_49), xx, xx, xx, xx, xx, mrm, x, 49},
  {PREFIX_EXT, 0x0f7610, STROFF(prefix_ext_50), xx, xx, xx, xx, xx, mrm, x, 50},
  {VEX_L_EXT,  0x0f7710, STROFF(vex_L_ext_0), xx, xx, xx, xx, xx, no, x, 0},
  /* 78 */
  {PREFIX_EXT, 0x0f7810, STROFF(prefix_ext_134), xx, xx, xx, xx, xx, mrm, x, 134},
  {PREFIX_EXT, 0x0f7910, STROFF(prefix_ext_135), xx, xx, xx, xx, xx, mrm, x, 135},
  {PREFIX_EXT, 0x0f7a10, STROFF(prefix_ext_159), xx, xx, xx, xx, xx, mrm, x, 159},
  {PREFIX_EXT, 0x0f7b10, STROFF(prefix_ext_158), xx, xx, xx, xx, xx, mrm, x, 158},
  {PREFIX_EXT, 0x0f7c10, STROFF(prefix_ext_114), xx, xx, xx, xx, xx, mrm, x, 114},
  {PREFIX_EXT, 0x0f7d10, STROFF(prefix_ext_115), xx, xx, xx, xx, xx, mrm, x, 115},
  {PREFIX_EXT, 0x0f7e10, STROFF(prefix_ext_51), xx, xx, xx, xx, xx, mrm, x, 51},
  {PREFIX_EXT, 0x0f7f10, STROFF(prefix_ext_113), xx, xx, xx, xx, xx, mrm, x, 113},
  /* 80 */
  {OP_jo,  0x0f8010, STROFF(jo),  xx, xx, Jz, xx, xx, predcc, fRO, END_LIST},
  {OP_jno, 0x0f8110, STROFF(jno), xx, xx, Jz, xx, xx, predcc, fRO, END_LIST},
  {OP_jb,  0x0f8210, STROFF(jb),  xx, xx, Jz, xx, xx, predcc, fRC, END_LIST},
  {OP_jnb, 0x0f8310, STROFF(jnb), xx, xx, Jz, xx, xx, predcc, fRC, END_LIST},
  {OP_jz,  0x0f8410, STROFF(jz),  xx, xx, Jz, xx, xx, predcc, fRZ, END_LIST},
  {OP_jnz, 0x0f8510, STROFF(jnz), xx, xx, Jz, xx, xx, predcc, fRZ, END_LIST},
  {OP_jbe, 0x0f8610, STROFF(jbe), xx, xx, Jz, xx, xx, predcc, (fRC|fRZ), END_LIST},
  {OP_jnbe,0x0f8710, STROFF(jnbe),xx, xx, Jz, xx, xx, predcc, (fRC|fRZ), END_LIST},
  /* 88 */
  {OP_js,  0x0f8810, STROFF(js),  xx, xx, Jz, xx, xx, predcc, fRS, END_LIST},
  {OP_jns, 0x0f8910, STROFF(jns), xx, xx, Jz, xx, xx, predcc, fRS, END_LIST},
  {OP_jp,  0x0f8a10, STROFF(jp),  xx, xx, Jz, xx, xx, predcc, fRP, END_LIST},
  {OP_jnp, 0x0f8b10, STROFF(jnp), xx, xx, Jz, xx, xx, predcc, fRP, END_LIST},
  {OP_jl,  0x0f8c10, STROFF(jl),  xx, xx, Jz, xx, xx, predcc, (fRS|fRO), END_LIST},
  {OP_jnl, 0x0f8d10, STROFF(jnl), xx, xx, Jz, xx, xx, predcc, (fRS|fRO), END_LIST},
  {OP_jle, 0x0f8e10, STROFF(jle), xx, xx, Jz, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
  {OP_jnle,0x0f8f10, STROFF(jnle),xx, xx, Jz, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
  /* 90 */
  {E_VEX_EXT, 0x0f9010, STROFF(e_vex_ext_79), xx, xx, xx, xx, xx, mrm, x, 79},
  {E_VEX_EXT, 0x0f9110, STROFF(e_vex_ext_80), xx, xx, xx, xx, xx, mrm, x, 80},
  {E_VEX_EXT, 0x0f9210, STROFF(e_vex_ext_81), xx, xx, xx, xx, xx, mrm, x, 81},
  {E_VEX_EXT, 0x0f9310, STROFF(e_vex_ext_82), xx, xx, xx, xx, xx, mrm, x, 82},
  {OP_setz,  0x0f9410, STROFF(setz),  Eb, xx, xx, xx, xx, mrm, fRZ, END_LIST},
  {OP_setnz, 0x0f9510, STROFF(setnz), Eb, xx, xx, xx, xx, mrm, fRZ, END_LIST},
  {OP_setbe, 0x0f9610, STROFF(setbe), Eb, xx, xx, xx, xx, mrm, (fRC|fRZ), END_LIST},
  {OP_setnbe,0x0f9710, STROFF(setnbe),Eb, xx, xx, xx, xx, mrm, (fRC|fRZ), END_LIST},
  /* 98 */
  {E_VEX_EXT, 0x0f9810, STROFF(e_vex_ext_91), xx, xx, xx, xx, xx, mrm, x, 91},
  {E_VEX_EXT, 0x0f9910, STROFF(e_vex_ext_92), xx, xx, xx, xx, xx, mrm, x, 92},
  {OP_setp,  0x0f9a10, STROFF(setp),  Eb, xx, xx, xx, xx, mrm, fRP, END_LIST},
  {OP_setnp, 0x0f9b10, STROFF(setnp), Eb, xx, xx, xx, xx, mrm, fRP, END_LIST},
  {OP_setl,  0x0f9c10, STROFF(setl),  Eb, xx, xx, xx, xx, mrm, (fRS|fRO), END_LIST},
  {OP_setnl, 0x0f9d10, STROFF(setnl), Eb, xx, xx, xx, xx, mrm, (fRS|fRO), END_LIST},
  {OP_setle, 0x0f9e10, STROFF(setle), Eb, xx, xx, xx, xx, mrm, (fRS|fRO|fRZ), END_LIST},
  {OP_setnle,0x0f9f10, STROFF(setnle),Eb, xx, xx, xx, xx, mrm, (fRS|fRO|fRZ), END_LIST},
  /* a0 */
  {OP_push, 0x0fa010, STROFF(push), xsp, i_xSPo1, fs, xsp, xx, no, x, tsb[0xa8]},
  {OP_pop,  0x0fa110, STROFF(pop), fs, xsp, xsp, i_xSP, xx, no, x, tsb[0xa9]},
  {OP_cpuid, 0x0fa210, STROFF(cpuid), eax, ebx, eax, ecx, xx, xop, x, exop[0x06]},
  {OP_bt,   0x0fa310, STROFF(bt),   xx, xx, Ev, Gv, xx, mrm, fW6, tex[15][4]},
  {OP_shld, 0x0fa410, STROFF(shld), Ev, xx, Gv, Ib, Ev, mrm, fW6, tsb[0xa5]},
  {OP_shld, 0x0fa510, STROFF(shld), Ev, xx, Gv, cl, Ev, mrm, fW6, END_LIST},
  {INVALID, 0x0fa610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  {INVALID, 0x0fa710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  /* a8 */
  {OP_push, 0x0fa810, STROFF(push), xsp, i_xSPo1, gs, xsp, xx, no, x, END_LIST},
  {OP_pop,  0x0fa910, STROFF(pop), gs, xsp, xsp, i_xSP, xx, no, x, END_LIST},
  {OP_rsm,  0x0faa10, STROFF(rsm), xx, xx, xx, xx, xx, no, fWX, END_LIST},
  {OP_bts,  0x0fab10, STROFF(bts), Ev, xx, Gv, Ev, xx, mrm, fW6, tex[15][5]},
  {OP_shrd, 0x0fac10, STROFF(shrd), Ev, xx, Gv, Ib, Ev, mrm, fW6, tsb[0xad]},
  {OP_shrd, 0x0fad10, STROFF(shrd), Ev, xx, Gv, cl, Ev, mrm, fW6, END_LIST},
  {EXTENSION, 0x0fae10, STROFF(group_15), xx, xx, xx, xx, xx, mrm, x, 22},
  {OP_imul, 0x0faf10, STROFF(imul), Gv, xx, Ev, Gv, xx, mrm, fW6, tfb[0x69]},
  /* b0 */
  {OP_cmpxchg, 0x0fb010, STROFF(cmpxchg), Eb, al, Gb, Eb, al, mrm, fW6, END_LIST},
  {OP_cmpxchg, 0x0fb110, STROFF(cmpxchg), Ev, eAX, Gv, Ev, eAX, mrm, fW6, tsb[0xb0]},
  {OP_lss, 0x0fb210, STROFF(lss), Gv, ss, Mp, xx, xx, mrm, x, END_LIST},
  {OP_btr, 0x0fb310, STROFF(btr), Ev, xx, Gv, Ev, xx, mrm, fW6, tex[15][6]},
  {OP_lfs, 0x0fb410, STROFF(lfs), Gv, fs, Mp, xx, xx, mrm, x, END_LIST},
  {OP_lgs, 0x0fb510, STROFF(lgs), Gv, gs, Mp, xx, xx, mrm, x, END_LIST},
  {OP_movzx, 0x0fb610, STROFF(movzx), Gv, xx, Eb, xx, xx, mrm, x, END_LIST},
  {OP_movzx, 0x0fb710, STROFF(movzx), Gv, xx, Ew, xx, xx, mrm, x, tsb[0xb6]},
  /* b8 */
  {OP_popcnt, 0xf30fb810, STROFF(popcnt), Gv, xx, Ev, xx, xx, mrm|reqp, fW6, END_LIST},
  /* This is Group 10, but all identical (ud1) so no reason to split opcode by /reg */
  {OP_ud1, 0x0fb910, STROFF(ud1), xx, xx, Gv, Ev, xx, mrm, x, END_LIST},
  {EXTENSION, 0x0fba10, STROFF(group_8), xx, xx, xx, xx, xx, mrm, x, 15},
  {OP_btc, 0x0fbb10, STROFF(btc), Ev, xx, Gv, Ev, xx, mrm, fW6, tex[15][7]},
  {PREFIX_EXT, 0x0fbc10, STROFF(prefix_ext_140), xx, xx, xx, xx, xx, mrm, x, 140},
  {PREFIX_EXT, 0x0fbd10, STROFF(prefix_ext_136), xx, xx, xx, xx, xx, mrm, x, 136},
  {OP_movsx, 0x0fbe10, STROFF(movsx), Gv, xx, Eb, xx, xx, mrm, x, END_LIST},
  {OP_movsx, 0x0fbf10, STROFF(movsx), Gv, xx, Ew, xx, xx, mrm, x, tsb[0xbe]},
  /* c0 */
  {OP_xadd, 0x0fc010, STROFF(xadd), Eb, Gb, Eb, Gb, xx, mrm, fW6, END_LIST},
  {OP_xadd, 0x0fc110, STROFF(xadd), Ev, Gv, Ev, Gv, xx, mrm, fW6, tsb[0xc0]},
  {PREFIX_EXT, 0x0fc210, STROFF(prefix_ext_52), xx, xx, xx, xx, xx, mrm, x, 52},
  {OP_movnti, 0x0fc310, STROFF(movnti), My, xx, Gy, xx, xx, mrm, x, END_LIST},
  {PREFIX_EXT, 0x0fc410, STROFF(prefix_ext_53), xx, xx, xx, xx, xx, mrm, x, 53},
  {PREFIX_EXT, 0x0fc510, STROFF(prefix_ext_54), xx, xx, xx, xx, xx, mrm, x, 54},
  {PREFIX_EXT, 0x0fc610, STROFF(prefix_ext_55), xx, xx, xx, xx, xx, mrm, x, 55},
  {EXTENSION, 0x0fc710, STROFF(group_9), xx, xx, xx, xx, xx, mrm, x, 16},
  /* c8 */
  {OP_bswap, 0x0fc810, STROFF(bswap), uAX_x, xx, uAX_x, xx, xx, no, x, tsb[0xc9]},
  {OP_bswap, 0x0fc910, STROFF(bswap), uCX_x, xx, uCX_x, xx, xx, no, x, tsb[0xca]},
  {OP_bswap, 0x0fca10, STROFF(bswap), uDX_x, xx, uDX_x, xx, xx, no, x, tsb[0xcb]},
  {OP_bswap, 0x0fcb10, STROFF(bswap), uBX_x, xx, uBX_x, xx, xx, no, x, tsb[0xcc]},
  {OP_bswap, 0x0fcc10, STROFF(bswap), uSP_x, xx, uSP_x, xx, xx, no, x, tsb[0xcd]},
  {OP_bswap, 0x0fcd10, STROFF(bswap), uBP_x, xx, uBP_x, xx, xx, no, x, tsb[0xce]},
  {OP_bswap, 0x0fce10, STROFF(bswap), uSI_x, xx, uSI_x, xx, xx, no, x, tsb[0xcf]},
  {OP_bswap, 0x0fcf10, STROFF(bswap), uDI_x, xx, uDI_x, xx, xx, no, x, END_LIST},
  /* d0 */
  {PREFIX_EXT, 0x0fd010, STROFF(prefix_ext_116), xx, xx, xx, xx, xx, mrm, x, 116},
  {PREFIX_EXT, 0x0fd110, STROFF(prefix_ext_56), xx, xx, xx, xx, xx, mrm, x, 56},
  {PREFIX_EXT, 0x0fd210, STROFF(prefix_ext_57), xx, xx, xx, xx, xx, mrm, x, 57},
  {PREFIX_EXT, 0x0fd310, STROFF(prefix_ext_58), xx, xx, xx, xx, xx, mrm, x, 58},
  {PREFIX_EXT, 0x0fd410, STROFF(prefix_ext_59), xx, xx, xx, xx, xx, mrm, x, 59},
  {PREFIX_EXT, 0x0fd510, STROFF(prefix_ext_60), xx, xx, xx, xx, xx, mrm, x, 60},
  {PREFIX_EXT, 0x0fd610, STROFF(prefix_ext_61), xx, xx, xx, xx, xx, mrm, x, 61},
  {PREFIX_EXT, 0x0fd710, STROFF(prefix_ext_62), xx, xx, xx, xx, xx, mrm, x, 62},
  /* d8 */
  {PREFIX_EXT, 0x0fd810, STROFF(prefix_ext_63), xx, xx, xx, xx, xx, mrm, x, 63},
  {PREFIX_EXT, 0x0fd910, STROFF(prefix_ext_64), xx, xx, xx, xx, xx, mrm, x, 64},
  {PREFIX_EXT, 0x0fda10, STROFF(prefix_ext_65), xx, xx, xx, xx, xx, mrm, x, 65},
  {PREFIX_EXT, 0x0fdb10, STROFF(prefix_ext_66), xx, xx, xx, xx, xx, mrm, x, 66},
  {PREFIX_EXT, 0x0fdc10, STROFF(prefix_ext_67), xx, xx, xx, xx, xx, mrm, x, 67},
  {PREFIX_EXT, 0x0fdd10, STROFF(prefix_ext_68), xx, xx, xx, xx, xx, mrm, x, 68},
  {PREFIX_EXT, 0x0fde10, STROFF(prefix_ext_69), xx, xx, xx, xx, xx, mrm, x, 69},
  {PREFIX_EXT, 0x0fdf10, STROFF(prefix_ext_70), xx, xx, xx, xx, xx, mrm, x, 70},
  /* e0 */
  {PREFIX_EXT, 0x0fe010, STROFF(prefix_ext_71), xx, xx, xx, xx, xx, mrm, x, 71},
  {PREFIX_EXT, 0x0fe110, STROFF(prefix_ext_72), xx, xx, xx, xx, xx, mrm, x, 72},
  {PREFIX_EXT, 0x0fe210, STROFF(prefix_ext_73), xx, xx, xx, xx, xx, mrm, x, 73},
  {PREFIX_EXT, 0x0fe310, STROFF(prefix_ext_74), xx, xx, xx, xx, xx, mrm, x, 74},
  {PREFIX_EXT, 0x0fe410, STROFF(prefix_ext_75), xx, xx, xx, xx, xx, mrm, x, 75},
  {PREFIX_EXT, 0x0fe510, STROFF(prefix_ext_76), xx, xx, xx, xx, xx, mrm, x, 76},
  {PREFIX_EXT, 0x0fe610, STROFF(prefix_ext_77), xx, xx, xx, xx, xx, mrm, x, 77},
  {PREFIX_EXT, 0x0fe710, STROFF(prefix_ext_78), xx, xx, xx, xx, xx, mrm, x, 78},
  /* e8 */
  {PREFIX_EXT, 0x0fe810, STROFF(prefix_ext_79), xx, xx, xx, xx, xx, mrm, x, 79},
  {PREFIX_EXT, 0x0fe910, STROFF(prefix_ext_80), xx, xx, xx, xx, xx, mrm, x, 80},
  {PREFIX_EXT, 0x0fea10, STROFF(prefix_ext_81), xx, xx, xx, xx, xx, mrm, x, 81},
  {PREFIX_EXT, 0x0feb10, STROFF(prefix_ext_82), xx, xx, xx, xx, xx, mrm, x, 82},
  {PREFIX_EXT, 0x0fec10, STROFF(prefix_ext_83), xx, xx, xx, xx, xx, mrm, x, 83},
  {PREFIX_EXT, 0x0fed10, STROFF(prefix_ext_84), xx, xx, xx, xx, xx, mrm, x, 84},
  {PREFIX_EXT, 0x0fee10, STROFF(prefix_ext_85), xx, xx, xx, xx, xx, mrm, x, 85},
  {PREFIX_EXT, 0x0fef10, STROFF(prefix_ext_86), xx, xx, xx, xx, xx, mrm, x, 86},
  /* f0 */
  {PREFIX_EXT, 0x0ff010, STROFF(prefix_ext_117), xx, xx, xx, xx, xx, mrm, x, 117},
  {PREFIX_EXT, 0x0ff110, STROFF(prefix_ext_87), xx, xx, xx, xx, xx, mrm, x, 87},
  {PREFIX_EXT, 0x0ff210, STROFF(prefix_ext_88), xx, xx, xx, xx, xx, mrm, x, 88},
  {PREFIX_EXT, 0x0ff310, STROFF(prefix_ext_89), xx, xx, xx, xx, xx, mrm, x, 89},
  {PREFIX_EXT, 0x0ff410, STROFF(prefix_ext_90), xx, xx, xx, xx, xx, mrm, x, 90},
  {PREFIX_EXT, 0x0ff510, STROFF(prefix_ext_91), xx, xx, xx, xx, xx, mrm, x, 91},
  {PREFIX_EXT, 0x0ff610, STROFF(prefix_ext_92), xx, xx, xx, xx, xx, mrm, x, 92},
  {PREFIX_EXT, 0x0ff710, STROFF(prefix_ext_93), xx, xx, xx, xx, xx, mrm, x, 93},
  /* f8 */
  {PREFIX_EXT, 0x0ff810, STROFF(prefix_ext_94), xx, xx, xx, xx, xx, mrm, x, 94},
  {PREFIX_EXT, 0x0ff910, STROFF(prefix_ext_95), xx, xx, xx, xx, xx, mrm, x, 95},
  {PREFIX_EXT, 0x0ffa10, STROFF(prefix_ext_96), xx, xx, xx, xx, xx, mrm, x, 96},
  {PREFIX_EXT, 0x0ffb10, STROFF(prefix_ext_97), xx, xx, xx, xx, xx, mrm, x, 97},
  {PREFIX_EXT, 0x0ffc10, STROFF(prefix_ext_98), xx, xx, xx, xx, xx, mrm, x, 98},
  {PREFIX_EXT, 0x0ffd10, STROFF(prefix_ext_99), xx, xx, xx, xx, xx, mrm, x, 99},
  {PREFIX_EXT, 0x0ffe10, STROFF(prefix_ext_100), xx, xx, xx, xx, xx, mrm, x, 100},
  {INVALID, 0x0fff10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
};

/****************************************************************************
 * Opcode extensions
 * This is from Table A-6
 */
const instr_info_t base_extensions[][8] = {
  /* group 1a -- first opcode byte 80: all assumed to have Ib */
  { /* extensions[0] */
    {OP_add, 0x800020, STROFF(add), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][0]},
    {OP_or,  0x800021, STROFF(or),  Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][1]},
    {OP_adc, 0x800022, STROFF(adc), Eb, xx, Ib, Eb, xx, mrm, (fW6|fRC), tex[25][2]},
    {OP_sbb, 0x800023, STROFF(sbb), Eb, xx, Ib, Eb, xx, mrm, (fW6|fRC), tex[25][3]},
    {OP_and, 0x800024, STROFF(and), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][4]},
    {OP_sub, 0x800025, STROFF(sub), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][5]},
    {OP_xor, 0x800026, STROFF(xor), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[25][6]},
    {OP_cmp, 0x800027, STROFF(cmp), xx, xx, Eb, Ib, xx, mrm, fW6,  tex[25][7]},
 },
  /* group 1b -- first opcode byte 81: all assumed to have Iz */
  { /* extensions[1] */
    {OP_add, 0x810020, STROFF(add), Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][0]},
    {OP_or,  0x810021, STROFF(or),  Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][1]},
    {OP_adc, 0x810022, STROFF(adc), Ev, xx, Iz, Ev, xx, mrm, (fW6|fRC), tex[2][2]},
    {OP_sbb, 0x810023, STROFF(sbb), Ev, xx, Iz, Ev, xx, mrm, (fW6|fRC), tex[2][3]},
    {OP_and, 0x810024, STROFF(and), Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][4]},
    {OP_sub, 0x810025, STROFF(sub), Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][5]},
    {OP_xor, 0x810026, STROFF(xor), Ev, xx, Iz, Ev, xx, mrm, fW6,  tex[2][6]},
    {OP_cmp, 0x810027, STROFF(cmp), xx, xx, Ev, Iz, xx, mrm, fW6,  tex[2][7]},
 },
  /* group 1c -- first opcode byte 83 (for 82, see below "group 1c*"):
   * all assumed to have Ib */
  { /* extensions[2] */
    {OP_add, 0x830020, STROFF(add), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][0]},
    {OP_or,  0x830021, STROFF(or),  Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][1]},
    {OP_adc, 0x830022, STROFF(adc), Ev, xx, Ib, Ev, xx, mrm, (fW6|fRC), tex[0][2]},
    {OP_sbb, 0x830023, STROFF(sbb), Ev, xx, Ib, Ev, xx, mrm, (fW6|fRC), tex[0][3]},
    {OP_and, 0x830024, STROFF(and), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][4]},
    {OP_sub, 0x830025, STROFF(sub), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][5]},
    {OP_xor, 0x830026, STROFF(xor), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[0][6]},
    {OP_cmp, 0x830027, STROFF(cmp), xx, xx, Ev, Ib, xx, mrm, fW6,  tex[0][7]},
 },
  /* group 2a -- first opcode byte c0: all assumed to have Ib */
  { /* extensions[3] */
    {OP_rol, 0xc00020, STROFF(rol), Eb, xx, Ib, Eb, xx, mrm, (fWC|fWO),  tex[5][0]},
    {OP_ror, 0xc00021, STROFF(ror), Eb, xx, Ib, Eb, xx, mrm, (fWC|fWO),  tex[5][1]},
    {OP_rcl, 0xc00022, STROFF(rcl), Eb, xx, Ib, Eb, xx, mrm, (fRC|fWC|fWO), tex[5][2]},
    {OP_rcr, 0xc00023, STROFF(rcr), Eb, xx, Ib, Eb, xx, mrm, (fRC|fWC|fWO), tex[5][3]},
    {OP_shl, 0xc00024, STROFF(shl), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[5][4]},
    {OP_shr, 0xc00025, STROFF(shr), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[5][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xc00026, STROFF(shl), Eb, xx, Ib, Eb, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xc00027, STROFF(sar), Eb, xx, Ib, Eb, xx, mrm, fW6,  tex[5][7]},
 },
  /* group 2b -- first opcode byte c1: all assumed to have Ib */
  { /* extensions[4] */
    {OP_rol, 0xc10020, STROFF(rol), Ev, xx, Ib, Ev, xx, mrm, (fWC|fWO),  tex[6][0]},
    {OP_ror, 0xc10021, STROFF(ror), Ev, xx, Ib, Ev, xx, mrm, (fWC|fWO),  tex[6][1]},
    {OP_rcl, 0xc10022, STROFF(rcl), Ev, xx, Ib, Ev, xx, mrm, (fRC|fWC|fWO), tex[6][2]},
    {OP_rcr, 0xc10023, STROFF(rcr), Ev, xx, Ib, Ev, xx, mrm, (fRC|fWC|fWO), tex[6][3]},
    {OP_shl, 0xc10024, STROFF(shl), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[6][4]},
    {OP_shr, 0xc10025, STROFF(shr), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[6][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xc10026, STROFF(shl), Ev, xx, Ib, Ev, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xc10027, STROFF(sar), Ev, xx, Ib, Ev, xx, mrm, fW6,  tex[6][7]},
 },
  /* group 2c -- first opcode byte d0 */
  { /* extensions[5] */
    {OP_rol, 0xd00020, STROFF(rol), Eb, xx, c1, Eb, xx, mrm, (fWC|fWO),  tex[8][0]},
    {OP_ror, 0xd00021, STROFF(ror), Eb, xx, c1, Eb, xx, mrm, (fWC|fWO),  tex[8][1]},
    {OP_rcl, 0xd00022, STROFF(rcl), Eb, xx, c1, Eb, xx, mrm, (fRC|fWC|fWO), tex[8][2]},
    {OP_rcr, 0xd00023, STROFF(rcr), Eb, xx, c1, Eb, xx, mrm, (fRC|fWC|fWO), tex[8][3]},
    {OP_shl, 0xd00024, STROFF(shl), Eb, xx, c1, Eb, xx, mrm, fW6,  tex[8][4]},
    {OP_shr, 0xd00025, STROFF(shr), Eb, xx, c1, Eb, xx, mrm, fW6,  tex[8][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd00026, STROFF(shl), Eb, xx, c1, Eb, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd00027, STROFF(sar), Eb, xx, c1, Eb, xx, mrm, fW6,  tex[8][7]},
 },
  /* group 2d -- first opcode byte d1 */
  { /* extensions[6] */
    {OP_rol, 0xd10020, STROFF(rol), Ev, xx, c1, Ev, xx, mrm, (fWC|fWO),  tex[3][0]},
    {OP_ror, 0xd10021, STROFF(ror), Ev, xx, c1, Ev, xx, mrm, (fWC|fWO),  tex[3][1]},
    {OP_rcl, 0xd10022, STROFF(rcl), Ev, xx, c1, Ev, xx, mrm, (fRC|fWC|fWO), tex[3][2]},
    {OP_rcr, 0xd10023, STROFF(rcr), Ev, xx, c1, Ev, xx, mrm, (fRC|fWC|fWO), tex[3][3]},
    {OP_shl, 0xd10024, STROFF(shl), Ev, xx, c1, Ev, xx, mrm, fW6,  tex[3][4]},
    {OP_shr, 0xd10025, STROFF(shr), Ev, xx, c1, Ev, xx, mrm, fW6,  tex[3][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd10026, STROFF(shl), Ev, xx, c1, Ev, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd10027, STROFF(sar), Ev, xx, c1, Ev, xx, mrm, fW6,  tex[3][7]},
 },
  /* group 2e -- first opcode byte d2 */
  { /* extensions[7] */
    {OP_rol, 0xd20020, STROFF(rol), Eb, xx, cl, Eb, xx, mrm, (fWC|fWO),  END_LIST},
    {OP_ror, 0xd20021, STROFF(ror), Eb, xx, cl, Eb, xx, mrm, (fWC|fWO),  END_LIST},
    {OP_rcl, 0xd20022, STROFF(rcl), Eb, xx, cl, Eb, xx, mrm, (fRC|fWC|fWO), END_LIST},
    {OP_rcr, 0xd20023, STROFF(rcr), Eb, xx, cl, Eb, xx, mrm, (fRC|fWC|fWO), END_LIST},
    {OP_shl, 0xd20024, STROFF(shl), Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
    {OP_shr, 0xd20025, STROFF(shr), Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd20026, STROFF(shl), Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd20027, STROFF(sar), Eb, xx, cl, Eb, xx, mrm, fW6,  END_LIST},
 },
  /* group 2f -- first opcode byte d3 */
  { /* extensions[8] */
    {OP_rol, 0xd30020, STROFF(rol), Ev, xx, cl, Ev, xx, mrm, (fWC|fWO),  tex[7][0]},
    {OP_ror, 0xd30021, STROFF(ror), Ev, xx, cl, Ev, xx, mrm, (fWC|fWO),  tex[7][1]},
    {OP_rcl, 0xd30022, STROFF(rcl), Ev, xx, cl, Ev, xx, mrm, (fRC|fWC|fWO), tex[7][2]},
    {OP_rcr, 0xd30023, STROFF(rcr), Ev, xx, cl, Ev, xx, mrm, (fRC|fWC|fWO), tex[7][3]},
    {OP_shl, 0xd30024, STROFF(shl), Ev, xx, cl, Ev, xx, mrm, fW6,  tex[7][4]},
    {OP_shr, 0xd30025, STROFF(shr), Ev, xx, cl, Ev, xx, mrm, fW6,  tex[7][5]},
    /* PR 332254: /6 is an alias for /4; we do not add to encoding chain though */
    {OP_shl, 0xd30026, STROFF(shl), Ev, xx, cl, Ev, xx, mrm, fW6,  END_LIST},
    {OP_sar, 0xd30027, STROFF(sar), Ev, xx, cl, Ev, xx, mrm, fW6,  tex[7][7]},
 },
  /* group 3a -- first opcode byte f6 */
  { /* extensions[9] */
    {OP_test, 0xf60020, STROFF(test), xx, xx, Eb, Ib, xx, mrm, fW6, END_LIST},
    /* PR 332254: /1 is an alias for /0; we do not add to encoding chain though */
    {OP_test, 0xf60021, STROFF(test), xx, xx, Eb, Ib, xx, mrm, fW6, END_LIST},
    {OP_not,  0xf60022, STROFF(not), Eb, xx, Eb, xx, xx, mrm, x, END_LIST},
    {OP_neg,  0xf60023, STROFF(neg), Eb, xx, Eb, xx, xx, mrm, fW6, END_LIST},
    {OP_mul,  0xf60024, STROFF(mul), ax, xx, Eb, al, xx, mrm, fW6, END_LIST},
    {OP_imul, 0xf60025, STROFF(imul), ax, xx, Eb, al, xx, mrm, fW6, tsb[0xaf]},
    {OP_div,  0xf60026, STROFF(div), ah, al, Eb, ax, xx, mrm, fW6, END_LIST},
    {OP_idiv, 0xf60027, STROFF(idiv), ah, al, Eb, ax, xx, mrm, fW6, END_LIST},
 },
  /* group 3b -- first opcode byte f7 */
  { /* extensions[10] */
    {OP_test, 0xf70020, STROFF(test), xx,  xx, Ev, Iz, xx, mrm, fW6, tex[9][0]},
    /* PR 332254: /1 is an alias for /0; we do not add to encoding chain though */
    {OP_test, 0xf70021, STROFF(test), xx,  xx, Ev, Iz, xx, mrm, fW6, END_LIST},
    {OP_not,  0xf70022, STROFF(not), Ev,  xx, Ev, xx, xx, mrm, x, tex[9][2]},
    {OP_neg,  0xf70023, STROFF(neg), Ev,  xx, Ev, xx, xx, mrm, fW6, tex[9][3]},
    {OP_mul,  0xf70024, STROFF(mul),   eDX, eAX, Ev, eAX, xx, mrm, fW6, tex[9][4]},
    {OP_imul, 0xf70025, STROFF(imul),  eDX, eAX, Ev, eAX, xx, mrm, fW6, tex[9][5]},
    {OP_div,  0xf70026, STROFF(div),   eDX, eAX, Ev, eDX, eAX, mrm, fW6, tex[9][6]},
    {OP_idiv, 0xf70027, STROFF(idiv),  eDX, eAX, Ev, eDX, eAX, mrm, fW6, tex[9][7]},
 },
  /* group 4 (first byte fe) */
  { /* extensions[11] */
    {OP_inc, 0xfe0020, STROFF(inc), Eb, xx, Eb, xx, xx, mrm, (fW6&(~fWC)), END_LIST},
    {OP_dec, 0xfe0021, STROFF(dec), Eb, xx, Eb, xx, xx, mrm, (fW6&(~fWC)), END_LIST},
    {INVALID, 0xfe0022, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0023, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0024, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0025, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0026, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xfe0027, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 5 (first byte ff) */
  { /* extensions[12] */
    {OP_inc, 0xff0020, STROFF(inc), Ev, xx, Ev, xx, xx, mrm, (fW6&(~fWC)), tex[11][0]},
    {OP_dec, 0xff0021, STROFF(dec), Ev, xx, Ev, xx, xx, mrm, (fW6&(~fWC)), tex[11][1]},
    {OP_call_ind,     0xff0022, STROFF(call),  xsp, i_iSPo1, i_Exi, xsp, xx, mrm, x, END_LIST},
    /* Note how a far call's stack operand size matches far ret rather than call */
    {OP_call_far_ind, 0xff0023, STROFF(lcall),  xsp, i_vSPo2, i_Ep, xsp, xx, mrm, x, END_LIST},
    {OP_jmp_ind,      0xff0024, STROFF(jmp),  xx, xx, i_Exi, xx, xx, mrm, x, END_LIST},
    {OP_jmp_far_ind,  0xff0025, STROFF(ljmp),  xx, xx, i_Ep, xx, xx, mrm, x, END_LIST},
    {OP_push, 0xff0026, STROFF(push), xsp, i_xSPo1, Esv, xsp, xx, mrm, x, tfb[0x06]},
    {INVALID, 0xff0027, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 6 (first bytes 0f 00) */
  { /* extensions[13] */
    {OP_sldt, 0x0f0030, STROFF(sldt), Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_str,  0x0f0031, STROFF(str), Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_lldt, 0x0f0032, STROFF(lldt), xx, xx, Ew, xx, xx, mrm, x, END_LIST},
    {OP_ltr,  0x0f0033, STROFF(ltr), xx, xx, Ew, xx, xx, mrm, x, END_LIST},
    {OP_verr, 0x0f0034, STROFF(verr), xx, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
    {OP_verw, 0x0f0035, STROFF(verw), xx, xx, Ew, xx, xx, mrm, fWZ, END_LIST},
    {INVALID, 0x0f0036, STROFF(BAD),xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0037, STROFF(BAD),xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 7 (first bytes 0f 01) */
  { /* extensions[14] */
    {MOD_EXT, 0x0f0130, STROFF(group_7_mod_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    {MOD_EXT, 0x0f0131, STROFF(group_7_mod_ext_1), xx, xx, xx, xx, xx, no, x, 1},
    {MOD_EXT, 0x0f0132, STROFF(group_7_mod_ext_5), xx, xx, xx, xx, xx, no, x, 5},
    {MOD_EXT, 0x0f0133, STROFF(group_7_mod_ext_4), xx, xx, xx, xx, xx, no, x, 4},
    {OP_smsw, 0x0f0134, STROFF(smsw),  Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {MOD_EXT, 0x0f0135, STROFF(group_7_mod_ext_120), xx, xx, xx, xx, xx, no, x, 120},
    {OP_lmsw, 0x0f0136, STROFF(lmsw),  xx, xx, Ew, xx, xx, mrm, x, END_LIST},
    {MOD_EXT, 0x0f0137, STROFF(group_7_mod_ext_2), xx, xx, xx, xx, xx, no, x, 2},
  },
  /* group 8 (first bytes 0f ba): all assumed to have Ib */
  { /* extensions[15] */
    {INVALID, 0x0fba30, STROFF(BAD),xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fba31, STROFF(BAD),xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fba32, STROFF(BAD),xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fba33, STROFF(BAD),xx, xx, xx, xx, xx, no, x, NA},
    {OP_bt,  0x0fba34, STROFF(bt),    xx, xx, Ev, Ib, xx, mrm, fW6, END_LIST},
    {OP_bts, 0x0fba35, STROFF(bts), Ev, xx, Ib, Ev, xx, mrm, fW6, END_LIST},
    {OP_btr, 0x0fba36, STROFF(btr), Ev, xx, Ib, Ev, xx, mrm, fW6, END_LIST},
    {OP_btc, 0x0fba37, STROFF(btc), Ev, xx, Ib, Ev, xx, mrm, fW6, END_LIST},
  },
  /* group 9 (first bytes 0f c7) */
  { /* extensions[16] */
    {INVALID, 0x0fc730, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_cmpxchg8b, 0x0fc731, STROFF(cmpxchg8b), Mq_dq, eAX, Mq_dq, eAX, eDX, mrm_xop, fWZ, exop[0x07]},/*"cmpxchg16b" w/ rex.w*/
    {INVALID, 0x0fc732, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0fc733, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {REX_W_EXT, 0x0fc734, STROFF(rexw_ext_5), xx, xx, xx, xx, xx, mrm, x, 5},
    {INVALID, 0x0fc735, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {MOD_EXT, 0x0fc736, STROFF(group_9_mod_ext_12), xx, xx, xx, xx, xx, mrm, x, 12},
    {MOD_EXT, 0x0fc737, STROFF(mod_ext_13), xx, xx, xx, xx, xx, mrm, x, 13},
  },
  /* group 10 is all ud1 and is not used by us since identical */
  /* group 11a (first byte c6) */
  { /* extensions[17] */
    {OP_mov_st, 0xc60020, STROFF(mov), Eb, xx, Ib, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xc60021, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60022, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60023, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60024, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60025, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc60026, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#1314: this also sets eip */
    {OP_xabort, 0xf8c60067, STROFF(xabort), eax, xx, Ib, xx, xx, mrm, x, END_LIST},
  },
  /* group 11b (first byte c7) */
  { /* extensions[18] */
    /* PR 250397: be aware that mov_imm shares this tail end of mov_st templates */
    {OP_mov_st, 0xc70020, STROFF(mov), Ev, xx, Iz, xx, xx, mrm, x, tex[17][0]},
    {INVALID, 0xc70021, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70022, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70023, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70024, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70025, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xc70026, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_xbegin, 0xf8c70067, STROFF(xbegin), xx, xx, Jz, xx, xx, mrm, x, END_LIST},
  },
  /* group 12 (first bytes 0f 71): all assumed to have Ib */
  { /* extensions[19] */
    {INVALID, 0x0f7130, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7132, STROFF(prefix_ext_104), xx, xx, xx, xx, xx, no, x, 104},
    {INVALID, 0x0f7133, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7134, STROFF(prefix_ext_105), xx, xx, xx, xx, xx, no, x, 105},
    {INVALID, 0x0f7135, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7136, STROFF(prefix_ext_106), xx, xx, xx, xx, xx, no, x, 106},
    {INVALID, 0x0f7137, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 13 (first bytes 0f 72): all assumed to have Ib */
  { /* extensions[20] */
    {EVEX_Wb_EXT, 0x660f7220, STROFF(evex_Wb_ext_120), xx, xx, xx, xx, xx, mrm|evex, x, 120},
    {EVEX_Wb_EXT, 0x660f7221, STROFF(evex_Wb_ext_118), xx, xx, xx, xx, xx, mrm|evex, x, 118},
    {PREFIX_EXT, 0x0f7232, STROFF(prefix_ext_107), xx, xx, xx, xx, xx, no, x, 107},
    {INVALID, 0x0f7233, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7234, STROFF(prefix_ext_108), xx, xx, xx, xx, xx, no, x, 108},
    {INVALID, 0x0f7235, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7236, STROFF(prefix_ext_109), xx, xx, xx, xx, xx, no, x, 109},
    {INVALID, 0x0f7237, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 14 (first bytes 0f 73): all assumed to have Ib */
  { /* extensions[21] */
    {INVALID, 0x0f7330, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7331, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7332, STROFF(prefix_ext_110), xx, xx, xx, xx, xx, no, x, 110},
    {PREFIX_EXT, 0x0f7333, STROFF(prefix_ext_101), xx, xx, xx, xx, xx, no, x, 101},
    {INVALID, 0x0f7334, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f7335, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x0f7336, STROFF(prefix_ext_111), xx, xx, xx, xx, xx, no, x, 111},
    {PREFIX_EXT, 0x0f7337, STROFF(prefix_ext_102), xx, xx, xx, xx, xx, no, x, 102},
  },
  /* group 15 (first bytes 0f ae) */
  { /* extensions[22] */
    /* Intel tables imply they may add opcodes in the mod=3 (non-mem) space in future */
    {MOD_EXT,    0x0fae30, STROFF(group_15_mod_ext_14), xx, xx, xx, xx, xx, mrm, x, 14},
    {MOD_EXT,    0x0fae31, STROFF(group_15_mod_ext_15), xx, xx, xx, xx, xx, mrm, x, 15},
    {MOD_EXT,    0x0fae32, STROFF(group_15_mod_ext_16), xx, xx, xx, xx, xx, mrm, x, 16},
    {MOD_EXT,    0x0fae33, STROFF(group_15_mod_ext_17), xx, xx, xx, xx, xx, mrm, x, 17},
    {PREFIX_EXT, 0x0fae34, STROFF(prefix_ext_188), xx, xx, xx, xx, xx, no, x, 188},
    {MOD_EXT,    0x0fae35, STROFF(group_15_mod_ext_6), xx, xx, xx, xx, xx, no, x, 6},
    {MOD_EXT,    0x0fae36, STROFF(group_15_mod_ext_7), xx, xx, xx, xx, xx, no, x, 7},
    {MOD_EXT,    0x0fae37, STROFF(group_15_mod_ext_3), xx, xx, xx, xx, xx, no, x, 3},
 },
  /* group 16 (first bytes 0f 18) */
  { /* extensions[23] */
    /* Intel tables imply they may add opcodes in the mod=3 (non-mem) space in future */
    {OP_prefetchnta, 0x0f1830, STROFF(prefetchnta), xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetcht0,  0x0f1831, STROFF(prefetcht0),  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetcht1,  0x0f1832, STROFF(prefetcht1),  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetcht2,  0x0f1833, STROFF(prefetcht2),  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1834, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1835, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1836, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
    {OP_nop_modrm, 0x0f1837, STROFF(nop), xx, xx, Ed, xx, xx, mrm, x, END_LIST},
 },
  /* group AMD (first bytes 0f 0d) */
  { /* extensions[24] */
    {OP_prefetch,  0x0f0d30, STROFF(prefetch),  xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_prefetchw, 0x0f0d31, STROFF(prefetchw), xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f0d32, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d33, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d34, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d35, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d36, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0d37, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 1c* -- first opcode byte 82
   * see PR 235092 for the discrepancies in what 0x82 should be: empirically
   * and according to recent Intel manuals it matches 0x80, not 0x83 (as old
   * Intel manuals implied) or invalid (as gnu tools claim).
   * not linked into any encode chain.
   */
  { /* extensions[25]: all assumed to have Ib */
    {OP_add, 0x820020, STROFF(add), Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_or,  0x820021, STROFF(or),  Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_adc, 0x820022, STROFF(adc), Eb, xx, Ib, Eb, xx, mrm|i64, (fW6|fRC), END_LIST},
    {OP_sbb, 0x820023, STROFF(sbb), Eb, xx, Ib, Eb, xx, mrm|i64, (fW6|fRC), END_LIST},
    {OP_and, 0x820024, STROFF(and), Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_sub, 0x820025, STROFF(sub), Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_xor, 0x820026, STROFF(xor), Eb, xx, Ib, Eb, xx, mrm|i64, fW6,  END_LIST},
    {OP_cmp, 0x820027, STROFF(cmp), xx, xx, Eb, Ib, xx, mrm|i64, fW6,  END_LIST},
  },
  /* group 1d (Intel now calling Group 1A) -- first opcode byte 8f */
  { /* extensions[26] */
    {OP_pop,  0x8f0020, STROFF(pop), Esv, xsp, xsp, i_xSP, xx, mrm, x, tfb[0x17]},
    /* we shouldn't ever get here for these, as this becomes an XOP prefix */
    {INVALID, 0x8f0021, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0022, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0023, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0024, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0025, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0026, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x8f0027, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  /* XOP group 1 */
  { /* extensions[27] */
    {INVALID,     0x090138, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_blcfill,  0x090139, STROFF(blcfill), By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsfill,  0x09013a, STROFF(blsfill), By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blcs,     0x09013b, STROFF(blcs),    By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_tzmsk,    0x09013c, STROFF(tzmsk),   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blcic,    0x09013d, STROFF(blcic),   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsic,    0x09013e, STROFF(blsic),   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_t1mskc,   0x09013f, STROFF(t1mskc),  By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
  },
  /* XOP group 2 */
  { /* extensions[28] */
    {INVALID,     0x090238, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_blcmsk,   0x090239, STROFF(blcmsk),By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {INVALID,     0x09023a, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09023b, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09023c, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09023d, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_blci,     0x09023e, STROFF(blci),  By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {INVALID,     0x09023f, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  /* XOP group 3 */
  { /* extensions[29] */
    /* XXX i#1311: these instrs implicitly write to memory which we should
     * find a way to encode into the IR.
     */
    {OP_llwpcb,   0x091238, STROFF(llwpcb), xx, xx, Ry, xx, xx, mrm|vex, x, END_LIST},
    {OP_slwpcb,   0x091239, STROFF(slwpcb), Ry, xx, xx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0x09123a, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123b, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123c, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123d, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123e, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x09123f, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  /* XOP group 4: all assumed to have a 4-byte immediate by xop_a_extra[] */
  { /* extensions[30] */
    /* XXX i#1311: these instrs implicitly write to memory which we should
     * find a way to encode into the IR.
     */
    {OP_lwpins,   0x0a1238, STROFF(lwpins), xx, xx, By, Ed, Id, mrm|vex, fWC, END_LIST},
    {OP_lwpval,   0x0a1239, STROFF(lwpval), xx, xx, By, Ed, Id, mrm|vex, x, END_LIST},
    {INVALID,     0x0a123a, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123b, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123c, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123d, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123e, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0a123f, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 17 */
  { /* extensions[31] */
    {INVALID,     0x38f338, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_blsr,     0x38f339, STROFF(blsr),   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsmsk,   0x38f33a, STROFF(blsmsk), By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {OP_blsi,     0x38f33b, STROFF(blsi),   By, xx, Ey, xx, xx, mrm|vex, fW6, END_LIST},
    {INVALID,     0x38f33c, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x38f33d, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x38f33e, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x38f33f, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 18 */
  { /* extensions[32] */
    {INVALID, 0x6638c638, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c639, STROFF(evex_Wb_ext_197), xx, xx, xx, xx, xx, mrm|reqp, x, 197},
    {EVEX_Wb_EXT, 0x6638c63a, STROFF(evex_Wb_ext_199), xx, xx, xx, xx, xx, mrm|reqp, x, 199},
    {INVALID, 0x6638c63b, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638c63c, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c63d, STROFF(evex_Wb_ext_201), xx, xx, xx, xx, xx, mrm|reqp, x, 201},
    {EVEX_Wb_EXT, 0x6638c63e, STROFF(evex_Wb_ext_203), xx, xx, xx, xx, xx, mrm|reqp, x, 203},
    {INVALID, 0x6638c63f, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 19 */
  { /* extensions[33] */
    {INVALID, 0x6638c738, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c739, STROFF(evex_Wb_ext_198), xx, xx, xx, xx, xx, mrm|reqp, x, 198},
    {EVEX_Wb_EXT, 0x6638c73a, STROFF(evex_Wb_ext_200), xx, xx, xx, xx, xx, mrm|reqp, x, 200},
    {INVALID, 0x6638c73b, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638c73c, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c73d, STROFF(evex_Wb_ext_202), xx, xx, xx, xx, xx, mrm|reqp, x, 202},
    {EVEX_Wb_EXT, 0x6638c73e, STROFF(evex_Wb_ext_204), xx, xx, xx, xx, xx, mrm|reqp, x, 204},
    {INVALID, 0x6638c73f, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  },
};

/****************************************************************************
 * Two-byte instructions that differ depending on presence of
 * prefixes, indexed in this order:
 *   none, 0xf3, 0x66, 0xf2
 * A second set is used for vex-encoded instructions, indexed in the
 * same order by prefix.
 * A third set is used for evex-encoded instructions, indexed in the
 * same order by prefix.
 *
 * N.B.: to avoid having a full entry here when there is only one
 * valid opcode prefix, use |reqp in the original entry instead of
 * pointing to this table.
 */
const instr_info_t prefix_extensions[][12] = {
  /* prefix extension 0 */
  {
    {OP_movups, 0x0f1010, STROFF(movups), Vps, xx, Wps, xx, xx, mrm, x, tpe[1][0]},
    {MOD_EXT,   0xf30f1010, STROFF(mod_ext_18),  xx, xx, xx, xx, xx, mrm, x, 18},
    {OP_movupd, 0x660f1010, STROFF(movupd), Vpd, xx, Wpd, xx, xx, mrm, x, tpe[1][2]},
    {MOD_EXT,   0xf20f1010, STROFF(mod_ext_19),  xx, xx, xx, xx, xx, mrm, x, 19},
    {OP_vmovups,   0x0f1010, STROFF(vmovups), Vvs, xx, Wvs, xx, xx, mrm|vex, x, tpe[1][4]},
    {MOD_EXT,    0xf30f1010, STROFF(mod_ext_8), xx, xx, xx, xx, xx, mrm|vex, x, 8},
    {OP_vmovupd, 0x660f1010, STROFF(vmovupd), Vvd, xx, Wvd, xx, xx, mrm|vex, x, tpe[1][6]},
    {MOD_EXT,    0xf20f1010, STROFF(mod_ext_9), xx, xx, xx, xx, xx, mrm|vex, x, 9},
    {EVEX_Wb_EXT, 0x0f1010, STROFF(evex_Wb_ext_0), xx, xx, xx, xx, xx, mrm|evex, x, 0},
    {MOD_EXT,    0xf30f1010, STROFF(mod_ext_20), xx, xx, xx, xx, xx, mrm|evex, x, 20},
    {EVEX_Wb_EXT, 0x660f1010, STROFF(evex_Wb_ext_2), xx, xx, xx, xx, xx, mrm|evex, x, 2},
    {MOD_EXT,    0xf20f1010, STROFF(mod_ext_21), xx, xx, xx, xx, xx, mrm|evex, x, 21},
  }, /* prefix extension 1 */
  {
    {OP_movups, 0x0f1110, STROFF(movups), Wps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {OP_movss,  0xf30f1110, STROFF(movss),  Wss, xx, Vss, xx, xx, mrm, x, END_LIST},
    {OP_movupd, 0x660f1110, STROFF(movupd), Wpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {OP_movsd,  0xf20f1110, STROFF(movsd),  Wsd, xx, Vsd, xx, xx, mrm, x, END_LIST},
    {OP_vmovups,   0x0f1110, STROFF(vmovups), Wvs, xx, Vvs, xx, xx, mrm|vex, x, tevexwb[0][0]},
    {MOD_EXT,    0xf30f1110, STROFF(mod_ext_10), xx, xx, xx, xx, xx, mrm|vex, x, 10},
    {OP_vmovupd, 0x660f1110, STROFF(vmovupd), Wvd, xx, Vvd, xx, xx, mrm|vex, x, tevexwb[2][2]},
    {MOD_EXT,    0xf20f1110, STROFF(mod_ext_11), xx, xx, xx, xx, xx, mrm|vex, x, 11},
    {EVEX_Wb_EXT, 0x0f1110, STROFF(evex_Wb_ext_1), xx, xx, xx, xx, xx, mrm|evex, x, 1},
    {MOD_EXT,    0xf30f1110, STROFF(mod_ext_22), xx, xx, xx, xx, xx, mrm|evex, x, 22},
    {EVEX_Wb_EXT, 0x660f1110, STROFF(evex_Wb_ext_3), xx, xx, xx, xx, xx, mrm|evex, x, 3},
    {MOD_EXT,    0xf20f1110, STROFF(mod_ext_23), xx, xx, xx, xx, xx, mrm|evex, x, 23},
  }, /* prefix extension 2 */
  {
    /* i#319: note that the reg-reg form of the load version (0f12) is legal
     * and has a separate pneumonic ("movhlps"), yet the reg-reg form of
     * the store version (0f13) is illegal
     */
    {OP_movlps, 0x0f1210, STROFF(movlps), Vq_dq, xx, Wq_dq, xx, xx, mrm, x, tpe[3][0]}, /*"movhlps" if reg-reg */
    {OP_movsldup, 0xf30f1210, STROFF(movsldup), Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_movlpd, 0x660f1210, STROFF(movlpd), Vq_dq, xx, Mq, xx, xx, mrm, x, tpe[3][2]},
    {OP_movddup, 0xf20f1210, STROFF(movddup), Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_vmovlps,    0x0f1210, STROFF(vmovlps), Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|vex|reqL0, x, tpe[3][4]}, /*"vmovhlps" if reg-reg */
    {OP_vmovsldup,0xf30f1210, STROFF(vmovsldup), Vvs, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[18][0]},
    {OP_vmovlpd,  0x660f1210, STROFF(vmovlpd), Vq_dq, xx, Hq_dq, Mq, xx, mrm|vex, x, tpe[3][6]},
    {OP_vmovddup, 0xf20f1210, STROFF(vmovddup), Vvd, xx, Wx, xx, xx, mrm|vex, x, tevexwb[19][2]},
    {EVEX_Wb_EXT, 0x0f1210, STROFF(evex_Wb_ext_14), xx, xx, xx, xx, xx, mrm|evex, x, 14},
    {EVEX_Wb_EXT, 0xf30f1210, STROFF(evex_Wb_ext_18), xx, xx, xx, xx, xx, mrm|evex, x, 18},
    {EVEX_Wb_EXT, 0x660f1210, STROFF(evex_Wb_ext_16), xx, xx, xx, xx, xx, mrm|evex, x, 16},
    {EVEX_Wb_EXT, 0xf20f1210, STROFF(evex_Wb_ext_19), xx, xx, xx, xx, xx, mrm|evex, x, 19},
  }, /* prefix extension 3 */
  {
    {OP_movlps, 0x0f1310, STROFF(movlps), Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_movlpd, 0x660f1310, STROFF(movlpd), Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovlps, 0x0f1310, STROFF(vmovlps), Mq, xx, Vq_dq, xx, xx, mrm|vex, x, tevexwb[14][0]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovlpd, 0x660f1310, STROFF(vmovlpd), Mq, xx, Vq_dq, xx, xx, mrm|vex, x, tevexwb[16][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1310, STROFF(evex_Wb_ext_15), xx, xx, xx, xx, xx, mrm|evex, x, 15},
    {INVALID, 0xf30f1310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1310, STROFF(evex_Wb_ext_17), xx, xx, xx, xx, xx, mrm|evex, x, 17},
    {INVALID, 0xf20f1310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 4 */
  {
    {OP_unpcklps, 0x0f1410, STROFF(unpcklps), Vps, xx, Wq_dq, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_unpcklpd, 0x660f1410, STROFF(unpcklpd), Vpd, xx, Wq_dq, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpcklps, 0x0f1410, STROFF(vunpcklps), Vvs, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[25][0]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpcklpd, 0x660f1410, STROFF(vunpcklpd), Vvd, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[26][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1410, STROFF(evex_Wb_ext_25), xx, xx, xx, xx, xx, mrm|evex, x, 25},
    {INVALID, 0xf30f1410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1410, STROFF(evex_Wb_ext_26), xx, xx, xx, xx, xx, mrm|evex, x, 26},
    {INVALID, 0xf20f1410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 5 */
  {
    {OP_unpckhps, 0x0f1510, STROFF(unpckhps), Vps, xx, Wdq, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_unpckhpd, 0x660f1510, STROFF(unpckhpd), Vpd, xx, Wdq, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpckhps, 0x0f1510, STROFF(vunpckhps), Vvs, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[27][0]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpckhpd, 0x660f1510, STROFF(vunpckhpd), Vvd, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[28][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1510, STROFF(evex_Wb_ext_27), xx, xx, xx, xx, xx, mrm|evex, x, 27},
    {INVALID, 0xf30f1510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1510, STROFF(evex_Wb_ext_28), xx, xx, xx, xx, xx, mrm|evex, x, 28},
    {INVALID, 0xf20f1510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 6 */
  {
    /* i#319: note that the reg-reg form of the load version (0f16) is legal
     * and has a separate pneumonic ("movhlps"), yet the reg-reg form of
     * the store version (0f17) is illegal
     */
    {OP_movhps, 0x0f1610, STROFF(movhps), Vq_dq, xx, Wq_dq, xx, xx, mrm, x, tpe[7][0]}, /*"movlhps" if reg-reg */
    {OP_movshdup, 0xf30f1610, STROFF(movshdup), Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_movhpd, 0x660f1610, STROFF(movhpd), Vq_dq, xx, Mq, xx, xx, mrm, x, tpe[7][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhps, 0x0f1610, STROFF(vmovhps), Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|vex|reqL0, x, tpe[7][4]}, /*"vmovlhps" if reg-reg */
    {OP_vmovshdup, 0xf30f1610, STROFF(vmovshdup), Vvs, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[24][0]},
    {OP_vmovhpd, 0x660f1610, STROFF(vmovhpd), Vq_dq, xx, Hq_dq, Mq, xx, mrm|vex|reqL0, x, tpe[7][6]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1610, STROFF(evex_Wb_ext_20), xx, xx, xx, xx, xx, mrm|evex, x, 20},
    {EVEX_Wb_EXT, 0xf30f1610, STROFF(evex_Wb_ext_24), xx, xx, xx, xx, xx, mrm|evex, x, 24},
    {EVEX_Wb_EXT, 0x660f1610, STROFF(evex_Wb_ext_22), xx, xx, xx, xx, xx, mrm|evex, x, 22},
    {INVALID, 0xf20f1610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 7 */
  {
    {OP_movhps, 0x0f1710, STROFF(movhps), Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_movhpd, 0x660f1710, STROFF(movhpd), Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhps, 0x0f1710, STROFF(vmovhps), Mq, xx, Vq_dq, xx, xx, mrm|vex|reqL0, x, tevexwb[20][0]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhpd, 0x660f1710, STROFF(vmovhpd), Mq, xx, Vq_dq, xx, xx, mrm|vex|reqL0, x, tevexwb[22][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1710, STROFF(evex_Wb_ext_21), xx, xx, xx, xx, xx, mrm|evex, x, 21},
    {INVALID, 0xf30f1710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1710, STROFF(evex_Wb_ext_23), xx, xx, xx, xx, xx, mrm|evex, x, 23},
    {INVALID, 0xf20f1710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 8 */
  {
    {OP_movaps, 0x0f2810, STROFF(movaps), Vps, xx, Wps, xx, xx, mrm, x, tpe[9][0]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_movapd, 0x660f2810, STROFF(movapd), Vpd, xx, Wpd, xx, xx, mrm, x, tpe[9][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovaps, 0x0f2810, STROFF(vmovaps), Vvs, xx, Wvs, xx, xx, mrm|vex, x, tpe[9][4]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovapd, 0x660f2810, STROFF(vmovapd), Vvd, xx, Wvd, xx, xx, mrm|vex, x, tpe[9][6]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0x0f2810, STROFF(evex_Wb_ext_4), xx, xx, xx, xx, xx, mrm|evex, x, 4},
    {INVALID,    0xf30f2810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2810, STROFF(evex_Wb_ext_6), xx, xx, xx, xx, xx, mrm|evex, x, 6},
    {INVALID,    0xf20f2810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 9 */
  {
    {OP_movaps, 0x0f2910, STROFF(movaps), Wps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_movapd, 0x660f2910, STROFF(movapd), Wpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovaps, 0x0f2910, STROFF(vmovaps), Wvs, xx, Vvs, xx, xx, mrm|vex, x, tevexwb[4][0]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovapd, 0x660f2910, STROFF(vmovapd), Wvd, xx, Vvd, xx, xx, mrm|vex, x, tevexwb[6][2]},
    {INVALID, 0x00000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0x0f2910, STROFF(evex_Wb_ext_5), xx, xx, xx, xx, xx, mrm|evex, x, 5},
    {INVALID,    0xf30f2910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2910, STROFF(evex_Wb_ext_7), xx, xx, xx, xx, xx, mrm|evex, x, 7},
    {INVALID,    0xf20f2910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 10 */
  {
    {OP_cvtpi2ps,  0x0f2a10, STROFF(cvtpi2ps), Vq_dq, xx, Qq, xx, xx, mrm, x, END_LIST},
    {OP_cvtsi2ss, 0xf30f2a10, STROFF(cvtsi2ss), Vss, xx, Ey, xx, xx, mrm, x, END_LIST},
    {OP_cvtpi2pd, 0x660f2a10, STROFF(cvtpi2pd), Vpd, xx, Qq, xx, xx, mrm, x, END_LIST},
    {OP_cvtsi2sd, 0xf20f2a10, STROFF(cvtsi2sd), Vsd, xx, Ey, xx, xx, mrm, x, END_LIST},
    {INVALID,  0x0f2a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsi2ss, 0xf30f2a10, STROFF(vcvtsi2ss), Vdq, xx, H12_dq, Ey, xx, mrm|vex, x, tevexwb[31][0]},
    {INVALID, 0x660f2a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsi2sd, 0xf20f2a10, STROFF(vcvtsi2sd), Vdq, xx, Hsd, Ey, xx, mrm|vex, x, tevexwb[32][0]},
    {INVALID,   0x0f2a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f2a10, STROFF(evex_Wb_ext_31), xx, xx, xx, xx, xx, mrm|evex, x, 31},
    {INVALID, 0x660f2a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f2a10, STROFF(evex_Wb_ext_32), xx, xx, xx, xx, xx, mrm|evex, x, 32},
  }, /* prefix extension 11 */
  {
    {OP_movntps,   0x0f2b10, STROFF(movntps), Mps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {OP_movntss, 0xf30f2b10, STROFF(movntss), Mss, xx, Vss, xx, xx, mrm, x, END_LIST},
    {OP_movntpd, 0x660f2b10, STROFF(movntpd), Mpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {OP_movntsd, 0xf20f2b10, STROFF(movntsd), Msd, xx, Vsd, xx, xx, mrm, x, END_LIST},
    {OP_vmovntps,   0x0f2b10, STROFF(vmovntps), Mvs, xx, Vvs, xx, xx, mrm|vex, x, tevexwb[33][0]},
    /* XXX: AMD doesn't list movntss in their new manual => assuming no vex version */
    {INVALID, 0xf30f2b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovntpd, 0x660f2b10, STROFF(vmovntpd), Mvd, xx, Vvd, xx, xx, mrm|vex, x, tevexwb[34][2]},
    {INVALID, 0xf20f2b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f2b10, STROFF(evex_Wb_ext_33), xx, xx, xx, xx, xx, mrm|evex, x, 33},
    {INVALID, 0xf30f2b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2b10, STROFF(evex_Wb_ext_34), xx, xx, xx, xx, xx, mrm|evex, x, 34},
    {INVALID, 0xf20f2b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 12 */
  {
    {OP_cvttps2pi, 0x0f2c10, STROFF(cvttps2pi), Pq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvttss2si, 0xf30f2c10, STROFF(cvttss2si), Gy, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvttpd2pi, 0x660f2c10, STROFF(cvttpd2pi), Pq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvttsd2si, 0xf20f2c10, STROFF(cvttsd2si), Gy, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f2c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvttss2si, 0xf30f2c10, STROFF(vcvttss2si), Gy, xx, Wss, xx, xx, mrm|vex, x, tevexwb[35][0]},
    {INVALID, 0x660f2c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvttsd2si, 0xf20f2c10, STROFF(vcvttsd2si), Gy, xx, Wsd, xx, xx, mrm|vex, x, tevexwb[36][0]},
    {INVALID,   0x0f2c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f2c10, STROFF(evex_Wb_ext_35), xx, xx, xx, xx, xx, mrm|evex, x, 35},
    {INVALID, 0x660f2c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f2c10, STROFF(evex_Wb_ext_36), xx, xx, xx, xx, xx, mrm|evex, x, 36},
  }, /* prefix extension 13 */
  {
    {OP_cvtps2pi, 0x0f2d10, STROFF(cvtps2pi), Pq, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvtss2si, 0xf30f2d10, STROFF(cvtss2si), Gy, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2pi, 0x660f2d10, STROFF(cvtpd2pi), Pq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtsd2si, 0xf20f2d10, STROFF(cvtsd2si), Gy, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f2d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtss2si, 0xf30f2d10, STROFF(vcvtss2si), Gy, xx, Wss, xx, xx, mrm|vex, x, tevexwb[29][0]},
    {INVALID, 0x660f2d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsd2si, 0xf20f2d10, STROFF(vcvtsd2si), Gy, xx, Wsd, xx, xx, mrm|vex, x, tevexwb[30][0]},
    {INVALID,   0x0f2d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f2d10, STROFF(evex_Wb_ext_29), xx, xx, xx, xx, xx, mrm|evex, x, 29},
    {INVALID, 0x660f2d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f2d10, STROFF(evex_Wb_ext_30), xx, xx, xx, xx, xx, mrm|evex, x, 30},
  }, /* prefix extension 14 */
  {
    {OP_ucomiss, 0x0f2e10, STROFF(ucomiss), xx, xx, Vss, Wss, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf30f2e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_ucomisd, 0x660f2e10, STROFF(ucomisd), xx, xx, Vsd, Wsd, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf20f2e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vucomiss, 0x0f2e10, STROFF(vucomiss), xx, xx, Vss, Wss, xx, mrm|vex, fW6, tevexwb[37][0]},
    {INVALID, 0xf30f2e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vucomisd, 0x660f2e10, STROFF(vucomisd), xx, xx, Vsd, Wsd, xx, mrm|vex, fW6, tevexwb[38][2]},
    {INVALID, 0xf20f2e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f2e10, STROFF(evex_Wb_ext_37), xx, xx, xx, xx, xx, mrm|evex, x, 37},
    {INVALID, 0xf30f2e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2e10, STROFF(evex_Wb_ext_38), xx, xx, xx, xx, xx, mrm|evex, x, 38},
    {INVALID, 0xf20f2e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 15 */
  {
    {OP_comiss,  0x0f2f10, STROFF(comiss),  xx, xx, Vss, Wss, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf30f2f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_comisd,  0x660f2f10, STROFF(comisd),  xx, xx, Vsd, Wsd, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf20f2f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcomiss,  0x0f2f10, STROFF(vcomiss),  xx, xx, Vss, Wss, xx, mrm|vex, fW6, tevexwb[39][0]},
    {INVALID, 0xf30f2f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vcomisd,  0x660f2f10, STROFF(vcomisd),  xx, xx, Vsd, Wsd, xx, mrm|vex, fW6, tevexwb[40][2]},
    {INVALID, 0xf20f2f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f2e10, STROFF(evex_Wb_ext_39), xx, xx, xx, xx, xx, mrm|evex, x, 39},
    {INVALID, 0xf30f2f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2e10, STROFF(evex_Wb_ext_40), xx, xx, xx, xx, xx, mrm|evex, x, 40},
    {INVALID, 0xf20f2f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 16 */
  {
    {OP_movmskps, 0x0f5010, STROFF(movmskps), Gr, xx, Ups, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_movmskpd, 0x660f5010, STROFF(movmskpd), Gr, xx, Upd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovmskps, 0x0f5010, STROFF(vmovmskps), Gr, xx, Uvs, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovmskpd, 0x660f5010, STROFF(vmovmskpd), Gr, xx, Uvd, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 17 */
  {
    {OP_sqrtps, 0x0f5110, STROFF(sqrtps), Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_sqrtss, 0xf30f5110, STROFF(sqrtss), Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_sqrtpd, 0x660f5110, STROFF(sqrtpd), Vpd, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_sqrtsd, 0xf20f5110, STROFF(sqrtsd), Vsd, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {OP_vsqrtps, 0x0f5110, STROFF(vsqrtps), Vvs, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[265][0]},
    {OP_vsqrtss, 0xf30f5110, STROFF(vsqrtss), Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, tevexwb[266][0]},
    {OP_vsqrtpd, 0x660f5110, STROFF(vsqrtpd), Vvd, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[265][2]},
    {OP_vsqrtsd, 0xf20f5110, STROFF(vsqrtsd), Vdq, xx, Hsd, Wsd, xx, mrm|vex, x, tevexwb[266][2]},
    {EVEX_Wb_EXT, 0x0f5110, STROFF(evex_Wb_ext_265), xx, xx, xx, xx, xx, mrm|evex, x, 265},
    {EVEX_Wb_EXT, 0xf30f5110, STROFF(evex_Wb_ext_266), xx, xx, xx, xx, xx, mrm|evex, x, 266},
    {EVEX_Wb_EXT, 0x660f5110, STROFF(evex_Wb_ext_265), xx, xx, xx, xx, xx, mrm|evex, x, 265},
    {EVEX_Wb_EXT, 0xf20f5110, STROFF(evex_Wb_ext_266), xx, xx, xx, xx, xx, mrm|evex, x, 266},
  }, /* prefix extension 18 */
  {
    {OP_rsqrtps, 0x0f5210, STROFF(rsqrtps), Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_rsqrtss, 0xf30f5210, STROFF(rsqrtss), Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x660f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrsqrtps, 0x0f5210, STROFF(vrsqrtps), Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vrsqrtss, 0xf30f5210, STROFF(vrsqrtss), Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 19 */
  {
    {OP_rcpps, 0x0f5310, STROFF(rcpps), Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_rcpss, 0xf30f5310, STROFF(rcpss), Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x660f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrcpps, 0x0f5310, STROFF(vrcpps), Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vrcpss, 0xf30f5310, STROFF(vrcpss), Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 20 */
  {
    {OP_andps,  0x0f5410, STROFF(andps),  Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_andpd,  0x660f5410, STROFF(andpd),  Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vandps,  0x0f5410, STROFF(vandps),  Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[205][0]},
    {INVALID, 0xf30f5410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vandpd,  0x660f5410, STROFF(vandpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[205][2]},
    {INVALID, 0xf20f5410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5410, STROFF(evex_Wb_ext_205), xx, xx, xx, xx, xx, mrm|evex, x, 205},
    {INVALID, 0xf30f5410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5450, STROFF(evex_Wb_ext_205), xx, xx, xx, xx, xx, mrm|evex, x, 205},
    {INVALID, 0xf20f5410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 21 */
  {
    {OP_andnps, 0x0f5510, STROFF(andnps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_andnpd, 0x660f5510, STROFF(andnpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vandnps, 0x0f5510, STROFF(vandnps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[206][0]},
    {INVALID, 0xf30f5510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vandnpd, 0x660f5510, STROFF(vandnpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[206][2]},
    {INVALID, 0xf20f5510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5510, STROFF(evex_Wb_ext_206), xx, xx, xx, xx, xx, mrm|evex, x, 206},
    {INVALID, 0xf30f5510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5550, STROFF(evex_Wb_ext_206), xx, xx, xx, xx, xx, mrm|evex, x, 206},
    {INVALID, 0xf20f5510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 22 */
  {
    {OP_orps,   0x0f5610, STROFF(orps),   Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_orpd,   0x660f5610, STROFF(orpd),   Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorps,   0x0f5610, STROFF(vorps),   Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[207][0]},
    {INVALID, 0xf30f5610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vorpd,   0x660f5610, STROFF(vorpd),   Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[207][2]},
    {INVALID, 0xf20f5610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5610, STROFF(evex_Wb_ext_207), xx, xx, xx, xx, xx, mrm|evex, x, 207},
    {INVALID, 0xf30f5610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5650, STROFF(evex_Wb_ext_207), xx, xx, xx, xx, xx, mrm|evex, x, 207},
    {INVALID, 0xf20f5610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 23 */
  {
    {OP_xorps,  0x0f5710, STROFF(xorps),  Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_xorpd,  0x660f5710, STROFF(xorpd),  Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vxorps,  0x0f5710, STROFF(vxorps),  Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[208][0]},
    {INVALID, 0xf30f5710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vxorpd,  0x660f5710, STROFF(vxorpd),  Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[208][2]},
    {INVALID, 0xf20f5710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5710, STROFF(evex_Wb_ext_208), xx, xx, xx, xx, xx, mrm|evex, x, 208},
    {INVALID, 0xf30f5710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5750, STROFF(evex_Wb_ext_208), xx, xx, xx, xx, xx, mrm|evex, x, 208},
    {INVALID, 0xf20f5710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 24 */
  {
    {OP_addps, 0x0f5810, STROFF(addps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_addss, 0xf30f5810, STROFF(addss), Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_addpd, 0x660f5810, STROFF(addpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_addsd, 0xf20f5810, STROFF(addsd), Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vaddps, 0x0f5810, STROFF(vaddps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[209][0]},
    {OP_vaddss, 0xf30f5810, STROFF(vaddss), Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[255][0]},
    {OP_vaddpd, 0x660f5810, STROFF(vaddpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[209][2]},
    {OP_vaddsd, 0xf20f5810, STROFF(vaddsd), Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[255][2]},
    {EVEX_Wb_EXT, 0x0f5810, STROFF(evex_Wb_ext_209), xx, xx, xx, xx, xx, mrm|evex, x, 209},
    {EVEX_Wb_EXT, 0xf30f5800, STROFF(evex_Wb_ext_255), xx, xx, xx, xx, xx, mrm|evex, x, 255},
    {EVEX_Wb_EXT, 0x660f5850, STROFF(evex_Wb_ext_209), xx, xx, xx, xx, xx, mrm|evex, x, 209},
    {EVEX_Wb_EXT, 0xf20f5840, STROFF(evex_Wb_ext_255), xx, xx, xx, xx, xx, mrm|evex, x, 255},
  }, /* prefix extension 25 */
  {
    {OP_mulps, 0x0f5910, STROFF(mulps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_mulss, 0xf30f5910, STROFF(mulss), Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_mulpd, 0x660f5910, STROFF(mulpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_mulsd, 0xf20f5910, STROFF(mulsd), Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vmulps, 0x0f5910, STROFF(vmulps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[210][0]},
    {OP_vmulss, 0xf30f5910, STROFF(vmulss), Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[256][0]},
    {OP_vmulpd, 0x660f5910, STROFF(vmulpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[210][2]},
    {OP_vmulsd, 0xf20f5910, STROFF(vmulsd), Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[256][2]},
    {EVEX_Wb_EXT, 0x0f5910, STROFF(evex_Wb_ext_210), xx, xx, xx, xx, xx, mrm|evex, x, 210},
    {EVEX_Wb_EXT, 0xf30f5900, STROFF(evex_Wb_ext_256), xx, xx, xx, xx, xx, mrm|evex, x, 256},
    {EVEX_Wb_EXT, 0x660f5950, STROFF(evex_Wb_ext_210), xx, xx, xx, xx, xx, mrm|evex, x, 210},
    {EVEX_Wb_EXT, 0xf20f5940, STROFF(evex_Wb_ext_256), xx, xx, xx, xx, xx, mrm|evex, x, 256},
  }, /* prefix extension 26 */
  {
    {OP_cvtps2pd, 0x0f5a10, STROFF(cvtps2pd), Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvtss2sd, 0xf30f5a10, STROFF(cvtss2sd), Vsd, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2ps, 0x660f5a10, STROFF(cvtpd2ps), Vps, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtsd2ss, 0xf20f5a10, STROFF(cvtsd2ss), Vss, xx, Wsd, xx, xx, mrm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2pd, 0x0f5a10, STROFF(vcvtps2pd), Vvd, xx, Wh_x, xx, xx, mrm|vex, x, tevexwb[211][0]},
    {OP_vcvtss2sd, 0xf30f5a10, STROFF(vcvtss2sd), Vdq, xx, Hsd, Wss, xx, mrm|vex, x, tevexwb[257][0]},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtpd2ps, 0x660f5a10, STROFF(vcvtpd2ps), Vvs, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[211][2]},
    {OP_vcvtsd2ss, 0xf20f5a10, STROFF(vcvtsd2ss), Vdq, xx, H12_dq, Wsd, xx, mrm|vex, x, tevexwb[257][2]},
    {EVEX_Wb_EXT, 0x0f5a10, STROFF(evex_Wb_ext_211), xx, xx, xx, xx, xx, mrm|evex, x, 211},
    {EVEX_Wb_EXT, 0xf30f5a10, STROFF(evex_Wb_ext_257), xx, xx, xx, xx, xx, mrm|evex, x, 257},
    {EVEX_Wb_EXT, 0x660f5a50, STROFF(evex_Wb_ext_211), xx, xx, xx, xx, xx, mrm|evex, x, 211},
    {EVEX_Wb_EXT, 0xf20f5a50, STROFF(evex_Wb_ext_257), xx, xx, xx, xx, xx, mrm|evex, x, 257},
  }, /* prefix extension 27 */
  {
    {OP_cvtdq2ps, 0x0f5b10, STROFF(cvtdq2ps), Vps, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {OP_cvttps2dq, 0xf30f5b10, STROFF(cvttps2dq), Vdq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvtps2dq, 0x660f5b10, STROFF(cvtps2dq), Vdq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtdq2ps, 0x0f5b10, STROFF(vcvtdq2ps), Vvs, xx, Wx, xx, xx, mrm|vex, x, tevexwb[56][0]},
    {OP_vcvttps2dq, 0xf30f5b10, STROFF(vcvttps2dq), Vx, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[250][0]},
    {OP_vcvtps2dq, 0x660f5b10, STROFF(vcvtps2dq), Vx, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[249][0]},
    {INVALID, 0xf20f5b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5b10, STROFF(evex_Wb_ext_56), xx, xx, xx, xx, xx, mrm|evex, x, 56},
    {EVEX_Wb_EXT, 0x660f5b00, STROFF(evex_Wb_ext_250), xx, xx, xx, xx, xx, mrm|evex, x, 250},
    {EVEX_Wb_EXT, 0x660f5b00, STROFF(evex_Wb_ext_249), xx, xx, xx, xx, xx, mrm|evex, x, 249},
    {INVALID, 0xf20f5b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 28 */
  {
    {OP_subps, 0x0f5c10, STROFF(subps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_subss, 0xf30f5c10, STROFF(subss), Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_subpd, 0x660f5c10, STROFF(subpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_subsd, 0xf20f5c10, STROFF(subsd), Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vsubps, 0x0f5c10, STROFF(vsubps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[212][0]},
    {OP_vsubss, 0xf30f5c10, STROFF(vsubss), Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[258][0]},
    {OP_vsubpd, 0x660f5c10, STROFF(vsubpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[212][2]},
    {OP_vsubsd, 0xf20f5c10, STROFF(vsubsd), Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[258][2]},
    {EVEX_Wb_EXT, 0x0f5c10, STROFF(evex_Wb_ext_212), xx, xx, xx, xx, xx, mrm|evex, x, 212},
    {EVEX_Wb_EXT, 0xf30f5c00, STROFF(evex_Wb_ext_258), xx, xx, xx, xx, xx, mrm|evex, x, 258},
    {EVEX_Wb_EXT, 0x660f5c50, STROFF(evex_Wb_ext_212), xx, xx, xx, xx, xx, mrm|evex, x, 212},
    {EVEX_Wb_EXT, 0xf20f5c40, STROFF(evex_Wb_ext_258), xx, xx, xx, xx, xx, mrm|evex, x, 258},
  }, /* prefix extension 29 */
  {
    {OP_minps, 0x0f5d10, STROFF(minps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_minss, 0xf30f5d10, STROFF(minss), Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_minpd, 0x660f5d10, STROFF(minpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_minsd, 0xf20f5d10, STROFF(minsd), Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vminps, 0x0f5d10, STROFF(vminps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[213][0]},
    {OP_vminss, 0xf30f5d10, STROFF(vminss), Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[259][0]},
    {OP_vminpd, 0x660f5d10, STROFF(vminpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[213][2]},
    {OP_vminsd, 0xf20f5d10, STROFF(vminsd), Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[259][2]},
    {EVEX_Wb_EXT, 0x0f5d10, STROFF(evex_Wb_ext_213), xx, xx, xx, xx, xx, mrm|evex, x, 213},
    {EVEX_Wb_EXT, 0xf30f5d00, STROFF(evex_Wb_ext_259), xx, xx, xx, xx, xx, mrm|evex, x, 259},
    {EVEX_Wb_EXT, 0x660f5d50, STROFF(evex_Wb_ext_213), xx, xx, xx, xx, xx, mrm|evex, x, 213},
    {EVEX_Wb_EXT, 0xf20f5d40, STROFF(evex_Wb_ext_259), xx, xx, xx, xx, xx, mrm|evex, x, 259},
  }, /* prefix extension 30 */
  {
    {OP_divps, 0x0f5e10, STROFF(divps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_divss, 0xf30f5e10, STROFF(divss), Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_divpd, 0x660f5e10, STROFF(divpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_divsd, 0xf20f5e10, STROFF(divsd), Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vdivps, 0x0f5e10, STROFF(vdivps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[214][0]},
    {OP_vdivss, 0xf30f5e10, STROFF(vdivss), Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[260][0]},
    {OP_vdivpd, 0x660f5e10, STROFF(vdivpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[214][2]},
    {OP_vdivsd, 0xf20f5e10, STROFF(vdivsd), Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[260][2]},
    {EVEX_Wb_EXT, 0x0f5e10, STROFF(evex_Wb_ext_214), xx, xx, xx, xx, xx, mrm|evex, x, 214},
    {EVEX_Wb_EXT, 0xf30f5e00, STROFF(evex_Wb_ext_260), xx, xx, xx, xx, xx, mrm|evex, x, 260},
    {EVEX_Wb_EXT, 0x660f5e50, STROFF(evex_Wb_ext_214), xx, xx, xx, xx, xx, mrm|evex, x, 214},
    {EVEX_Wb_EXT, 0xf20f5e40, STROFF(evex_Wb_ext_260), xx, xx, xx, xx, xx, mrm|evex, x, 260},
  }, /* prefix extension 31 */
  {
    {OP_maxps, 0x0f5f10, STROFF(maxps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_maxss, 0xf30f5f10, STROFF(maxss), Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_maxpd, 0x660f5f10, STROFF(maxpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_maxsd, 0xf20f5f10, STROFF(maxsd), Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vmaxps, 0x0f5f10, STROFF(vmaxps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[215][0]},
    {OP_vmaxss, 0xf30f5f10, STROFF(vmaxss), Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[261][0]},
    {OP_vmaxpd, 0x660f5f10, STROFF(vmaxpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[215][2]},
    {OP_vmaxsd, 0xf20f5f10, STROFF(vmaxsd), Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[261][2]},
    {EVEX_Wb_EXT, 0x0f5f10, STROFF(evex_Wb_ext_215), xx, xx, xx, xx, xx, mrm|evex, x, 215},
    {EVEX_Wb_EXT, 0xf30f5f00, STROFF(evex_Wb_ext_261), xx, xx, xx, xx, xx, mrm|evex, x, 261},
    {EVEX_Wb_EXT, 0x660f5f50, STROFF(evex_Wb_ext_215), xx, xx, xx, xx, xx, mrm|evex, x, 215},
    {EVEX_Wb_EXT, 0xf20f5f40, STROFF(evex_Wb_ext_261), xx, xx, xx, xx, xx, mrm|evex, x, 261},
  }, /* prefix extension 32 */
  {
    {OP_punpcklbw,   0x0f6010, STROFF(punpcklbw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[32][2]},
    {INVALID,      0xf30f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklbw, 0x660f6010, STROFF(punpcklbw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6010,   STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklbw, 0x660f6010, STROFF(vpunpcklbw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[32][10]},
    {INVALID,      0xf20f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpcklbw, 0x660f6000, STROFF(vpunpcklbw), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 33 */
  {
    {OP_punpcklwd,   0x0f6110, STROFF(punpcklwd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[33][2]},
    {INVALID,      0xf30f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklwd, 0x660f6110, STROFF(punpcklwd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklwd, 0x660f6110, STROFF(vpunpcklwd), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[33][10]},
    {INVALID,      0xf20f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpcklwd, 0x660f6100, STROFF(vpunpcklwd), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 34 */
  {
    {OP_punpckldq,   0x0f6210, STROFF(punpckldq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[34][2]},
    {INVALID,      0xf30f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckldq, 0x660f6210, STROFF(punpckldq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckldq, 0x660f6210, STROFF(vpunpckldq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[235][0]},
    {INVALID,      0xf20f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6200, STROFF(evex_Wb_ext_235), xx, xx, xx, xx, xx, mrm|evex, x, 235},
    {INVALID, 0xf20f6210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 35 */
  {
    {OP_packsswb,   0x0f6310, STROFF(packsswb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[35][2]},
    {INVALID,     0xf30f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packsswb, 0x660f6310, STROFF(packsswb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpacksswb, 0x660f6310, STROFF(vpacksswb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[35][10]},
    {INVALID,     0xf20f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpacksswb, 0x660f6300, STROFF(vpacksswb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 36 */
  {
    {OP_pcmpgtb,   0x0f6410, STROFF(pcmpgtb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[36][2]},
    {INVALID,    0xf30f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtb, 0x660f6410, STROFF(pcmpgtb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtb, 0x660f6410, STROFF(vpcmpgtb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[36][10]},
    {INVALID,    0xf20f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpgtb, 0x660f6400, STROFF(vpcmpgtb), KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 37 */
  {
    {OP_pcmpgtw,   0x0f6510, STROFF(pcmpgtw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[37][2]},
    {INVALID,    0xf30f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtw, 0x660f6510, STROFF(pcmpgtw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtw, 0x660f6510, STROFF(vpcmpgtw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[37][10]},
    {INVALID,    0xf20f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpgtw, 0x660f6500, STROFF(vpcmpgtw), KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 38 */
  {
    {OP_pcmpgtd,   0x0f6610, STROFF(pcmpgtd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[38][2]},
    {INVALID,    0xf30f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtd, 0x660f6610, STROFF(pcmpgtd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtd, 0x660f6610, STROFF(vpcmpgtd), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[236][0]},
    {INVALID,    0xf20f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6600, STROFF(evex_Wb_ext_236), xx, xx, xx, xx, xx, mrm|evex, x, 236},
    {INVALID, 0xf20f6610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 39 */
  {
    {OP_packuswb,   0x0f6710, STROFF(packuswb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[39][2]},
    {INVALID,     0xf30f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packuswb, 0x660f6710, STROFF(packuswb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpackuswb, 0x660f6710, STROFF(vpackuswb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[39][10]},
    {INVALID,     0xf20f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpackuswb, 0x660f6700, STROFF(vpackuswb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 40 */
  {
    {OP_punpckhbw,   0x0f6810, STROFF(punpckhbw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[40][2]},
    {INVALID,      0xf30f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhbw, 0x660f6810, STROFF(punpckhbw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhbw, 0x660f6810, STROFF(vpunpckhbw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[40][10]},
    {INVALID,      0xf20f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpckhbw, 0x660f6800, STROFF(vpunpckhbw), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 41 */
  {
    {OP_punpckhwd,   0x0f6910, STROFF(punpckhwd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[41][2]},
    {INVALID,      0xf30f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhwd, 0x660f6910, STROFF(punpckhwd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhwd, 0x660f6910, STROFF(vpunpckhwd), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[41][10]},
    {INVALID,      0xf20f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpckhwd, 0x660f6900, STROFF(vpunpckhwd), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 42 */
  {
    {OP_punpckhdq,   0x0f6a10, STROFF(punpckhdq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[42][2]},
    {INVALID,      0xf30f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhdq, 0x660f6a10, STROFF(punpckhdq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhdq, 0x660f6a10, STROFF(vpunpckhdq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[237][0]},
    {INVALID,      0xf20f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6a00, STROFF(evex_Wb_ext_237), xx, xx, xx, xx, xx, mrm|evex, x, 237},
    {INVALID, 0xf20f6a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 43 */
  {
    {OP_packssdw,   0x0f6b10, STROFF(packssdw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[43][2]},
    {INVALID,     0xf30f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packssdw, 0x660f6b10, STROFF(packssdw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpackssdw, 0x660f6b10, STROFF(vpackssdw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[238][0]},
    {INVALID,     0xf20f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6b00, STROFF(evex_Wb_ext_238), xx, xx, xx, xx, xx, mrm|evex, x, 238},
    {INVALID, 0xf20f6b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 44 */
  {
    {INVALID,         0x0f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklqdq, 0x660f6c10, STROFF(punpcklqdq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,       0xf20f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklqdq, 0x660f6c10, STROFF(vpunpcklqdq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[216][2]},
    {INVALID,       0xf20f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6c40, STROFF(evex_Wb_ext_216), xx, xx, xx, xx, xx, mrm|evex, x, 216},
    {INVALID, 0xf20f6c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 45 */
  {
    {INVALID,         0x0f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhqdq, 0x660f6d10, STROFF(punpckhqdq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,       0xf20f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhqdq, 0x660f6d10, STROFF(vpunpckhqdq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[219][2]},
    {INVALID,       0xf20f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6d40, STROFF(evex_Wb_ext_219), xx, xx, xx, xx, xx, mrm|evex, x, 219},
    {INVALID, 0xf20f6d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 46 */
  {
    /* movd zeroes the top bits when the destination is an mmx or xmm reg */
    {OP_movd,   0x0f6e10, STROFF(movd), Pq, xx, Ey, xx, xx, mrm, x, tpe[46][2]},
    {INVALID, 0xf30f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    /* XXX: the opcode is called movq with rex.w + 0f. */
    {OP_movd, 0x660f6e10, STROFF(movd), Vdq, xx, Ey, xx, xx, mrm, x, tpe[51][0]},
    {INVALID, 0xf20f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x660f6e10, STROFF(vex_W_ext_108), xx, xx, xx, xx, xx, mrm|vex, x, 108},
    {INVALID, 0xf20f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6e10, STROFF(evex_Wb_ext_136), xx, xx, xx, xx, xx, mrm|evex, x, 136},
    {INVALID, 0xf20f6e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 47: all assumed to have Ib */
  {
    {OP_pshufw,   0x0f7010, STROFF(pshufw),   Pq, xx, Qq, Ib, xx, mrm, x, END_LIST},
    {OP_pshufhw, 0xf30f7010, STROFF(pshufhw), Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {OP_pshufd,  0x660f7010, STROFF(pshufd),  Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {OP_pshuflw, 0xf20f7010, STROFF(pshuflw), Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {INVALID,       0x0f7010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpshufhw, 0xf30f7010, STROFF(vpshufhw), Vx, xx, Wx, Ib, xx, mrm|vex, x, tpe[47][9]},
    {OP_vpshufd,  0x660f7010, STROFF(vpshufd),  Vx, xx, Wx, Ib, xx, mrm|vex, x, tevexwb[239][0]},
    {OP_vpshuflw, 0xf20f7010, STROFF(vpshuflw), Vx, xx, Wx, Ib, xx, mrm|vex, x, tpe[47][11]},
    {INVALID,   0x0f7010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufhw, 0xf30f7000, STROFF(vpshufhw), Ve, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {EVEX_Wb_EXT, 0x660f7000, STROFF(evex_Wb_ext_239), xx, xx, xx, xx, xx, mrm|evex, x, 239},
    {OP_vpshuflw, 0xf20f7000, STROFF(vpshuflw), Ve, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
  }, /* prefix extension 48 */
  {
    {OP_pcmpeqb,   0x0f7410, STROFF(pcmpeqb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[48][2]},
    {INVALID,    0xf30f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqb, 0x660f7410, STROFF(pcmpeqb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqb, 0x660f7410, STROFF(vpcmpeqb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[48][10]},
    {INVALID,    0xf20f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpeqb, 0x660f7400, STROFF(vpcmpeqb), KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 49 */
  {
    {OP_pcmpeqw,   0x0f7510, STROFF(pcmpeqw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[49][2]},
    {INVALID,    0xf30f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqw, 0x660f7510, STROFF(pcmpeqw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqw, 0x660f7510, STROFF(vpcmpeqw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[49][10]},
    {INVALID,    0xf20f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpeqw, 0x660f7500, STROFF(vpcmpeqw), KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 50 */
  {
    {OP_pcmpeqd,   0x0f7610, STROFF(pcmpeqd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[50][2]},
    {INVALID,    0xf30f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqd, 0x660f7610, STROFF(pcmpeqd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqd, 0x660f7610, STROFF(vpcmpeqd), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[240][0]},
    {INVALID,    0xf20f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7600, STROFF(evex_Wb_ext_240), xx, xx, xx, xx, xx, mrm|evex, x, 240},
    {INVALID, 0xf20f7610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 51 */
  {
    {OP_movd,   0x0f7e10, STROFF(movd), Ey, xx, Pd_q, xx, xx, mrm, x, tpe[51][2]},
    /* movq zeroes the top bits when the destination is an mmx or xmm reg */
    {OP_movq, 0xf30f7e10, STROFF(movq), Vdq, xx, Wq_dq, xx, xx, mrm, x, tpe[61][2]},
    {OP_movd, 0x660f7e10, STROFF(movd), Ey, xx, Vd_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f7e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0xf30f7e10, STROFF(vmovq), Vdq, xx, Wq_dq, xx, xx, mrm|vex, x, tpe[51][9]},
    {VEX_W_EXT, 0x660f7e10, STROFF(vex_W_ext_109), xx, xx, xx, xx, xx, mrm|vex, x, 109},
    {INVALID, 0xf20f7e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0xf30f7e40, STROFF(vmovq), Vdq, xx, Wq_dq, xx, xx, mrm|evex, x, tpe[61][6]},
    {EVEX_Wb_EXT, 0x660f7e10, STROFF(evex_Wb_ext_137), xx, xx, xx, xx, xx, mrm|evex, x, 137},
    {INVALID, 0xf20f7e10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 52: all assumed to have Ib */
  {
    {OP_cmpps, 0x0fc210, STROFF(cmpps), Vps, xx, Wps, Ib, Vps, mrm, x, END_LIST},
    {OP_cmpss, 0xf30fc210, STROFF(cmpss), Vss, xx, Wss, Ib, Vss, mrm, x, END_LIST},
    {OP_cmppd, 0x660fc210, STROFF(cmppd), Vpd, xx, Wpd, Ib, Vpd, mrm, x, END_LIST},
    {OP_cmpsd, 0xf20fc210, STROFF(cmpsd), Vsd, xx, Wsd, Ib, Vsd, mrm, x, END_LIST},
    {OP_vcmpps, 0x0fc210, STROFF(vcmpps), Vvs, xx, Hvs, Wvs, Ib, mrm|vex, x, tevexwb[224][0]},
    {OP_vcmpss, 0xf30fc210, STROFF(vcmpss), Vdq, xx, Hdq, Wss, Ib, mrm|vex, x, tevexwb[262][0]},
    {OP_vcmppd, 0x660fc210, STROFF(vcmppd), Vvd, xx, Hvd, Wvd, Ib, mrm|vex, x, tevexwb[224][2]},
    {OP_vcmpsd, 0xf20fc210, STROFF(vcmpsd), Vdq, xx, Hdq, Wsd, Ib, mrm|vex, x, tevexwb[262][2]},
    {EVEX_Wb_EXT, 0x0fc200, STROFF(evex_Wb_ext_224), xx, xx, xx, xx, xx, mrm|evex, x, 224},
    {EVEX_Wb_EXT, 0xf30fc200, STROFF(evex_Wb_ext_262), xx, xx, xx, xx, xx, mrm|evex, x, 262},
    {EVEX_Wb_EXT, 0x660fc240, STROFF(evex_Wb_ext_224), xx, xx, xx, xx, xx, mrm|evex, x, 224},
    {EVEX_Wb_EXT, 0xf20fc240, STROFF(evex_Wb_ext_262), xx, xx, xx, xx, xx, mrm|evex, x, 262},
  }, /* prefix extension 53: all assumed to have Ib */
  { /* note that gnu tools print immed first: pinsrw $0x0,(%esp),%xmm0 */
    /* FIXME i#1388: pinsrw actually reads only bottom word of reg */
    {OP_pinsrw,   0x0fc410, STROFF(pinsrw), Pw_q, xx, Rd_Mw, Ib, xx, mrm, x, tpe[53][2]},
    {INVALID,   0xf30fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pinsrw, 0x660fc410, STROFF(pinsrw), Vw_dq, xx, Rd_Mw, Ib, xx, mrm, x, END_LIST},
    {INVALID,   0xf20fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0x0fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpinsrw, 0x660fc410, STROFF(vpinsrw), Vdq, xx, H14_dq, Rd_Mw, Ib, mrm|vex, x, tpe[53][10]},
    {INVALID,   0xf20fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpinsrw, 0x660fc400, STROFF(vpinsrw), Vdq, xx, H14_dq, Rd_Mw, Ib, mrm|evex|ttt1s|inopsz2, x, END_LIST},
    {INVALID, 0xf20fc410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 54: all assumed to have Ib */
  { /* note that gnu tools print immed first: pextrw $0x7,%xmm7,%edx */
    {OP_pextrw,   0x0fc510, STROFF(pextrw), Gd, xx, Nw_q, Ib, xx, mrm, x, tpe[54][2]},
    {INVALID,   0xf30fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pextrw, 0x660fc510, STROFF(pextrw), Gd, xx, Uw_dq, Ib, xx, mrm, x, tvex[37][0]},
    {INVALID,   0xf20fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0x0fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpextrw, 0x660fc510, STROFF(vpextrw), Gd, xx, Uw_dq, Ib, xx, mrm|vex, x, tvex[37][1]},
    {INVALID,   0xf20fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpextrw, 0x660fc500, STROFF(vpextrw), Gd, xx, Uw_dq, Ib, xx, mrm|evex|ttnone|inopsz2, x, tvex[37][2]},
    {INVALID, 0xf20fc510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 55: all assumed to have Ib */
  {
    {OP_shufps, 0x0fc610, STROFF(shufps), Vps, xx, Wps, Ib, Vps, mrm, x, END_LIST},
    {INVALID, 0xf30fc610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_shufpd, 0x660fc610, STROFF(shufpd), Vpd, xx, Wpd, Ib, Vpd, mrm, x, END_LIST},
    {INVALID, 0xf20fc610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshufps, 0x0fc610, STROFF(vshufps), Vvs, xx, Hvs, Wvs, Ib, mrm|vex, x, tevexwb[221][0]},
    {INVALID, 0xf30fc610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vshufpd, 0x660fc610, STROFF(vshufpd), Vvd, xx, Hvd, Wvd, Ib, mrm|vex, x, tevexwb[221][2]},
    {INVALID, 0xf20fc610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0fc600, STROFF(evex_Wb_ext_221), xx, xx, xx, xx, xx, mrm|evex, x, 221},
    {INVALID, 0xf30fc610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fc640, STROFF(evex_Wb_ext_221), xx, xx, xx, xx, xx, mrm|evex, x, 221},
    {INVALID, 0xf20fc610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 56 */
  {
    {OP_psrlw,   0x0fd110, STROFF(psrlw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[56][2]},
    {INVALID,  0xf30fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrlw, 0x660fd110, STROFF(psrlw), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[104][0]},
    {INVALID,  0xf20fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrlw,  0x660fd110, STROFF(vpsrlw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[104][6]},
    {INVALID,  0xf20fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw, 0x660fd100, STROFF(vpsrlw), Ve, xx, KEd, He, Wdq, mrm|evex|ttm128, x, tpe[104][10]},
    {INVALID, 0xf20fd110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 57 */
  {
    {OP_psrld,   0x0fd210, STROFF(psrld), Pq, xx, Qq, Pq, xx, mrm, x, tpe[57][2]},
    {INVALID,  0xf30fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrld, 0x660fd210, STROFF(psrld), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[107][0]},
    {INVALID,  0xf20fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrld, 0x660fd210, STROFF(vpsrld), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[107][6]},
    {INVALID,  0xf20fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fd210, STROFF(evex_Wb_ext_123), xx, xx, xx, xx, xx, mrm|evex, x, 123},
    {INVALID, 0xf20fd210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 58 */
  {
    {OP_psrlq,   0x0fd310, STROFF(psrlq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[58][2]},
    {INVALID,  0xf30fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrlq, 0x660fd310, STROFF(psrlq), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[110][0]},
    {INVALID,  0xf20fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrlq, 0x660fd310, STROFF(vpsrlq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[110][6]},
    {INVALID,  0xf20fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fd310, STROFF(evex_Wb_ext_125), xx, xx, xx, xx, xx, mrm|evex, x, 125},
    {INVALID, 0xf20fd310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 59 */
  {
    {OP_paddq,   0x0fd410, STROFF(paddq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[59][2]},
    {INVALID,  0xf30fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_paddq, 0x660fd410, STROFF(paddq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,  0xf20fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddq, 0x660fd410, STROFF(vpaddq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[225][2]},
    {INVALID,  0xf20fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fd440, STROFF(evex_Wb_ext_225), xx, xx, xx, xx, xx, mrm|evex, x, 225},
    {INVALID, 0xf20fd410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 60 */
  {
    {OP_pmullw,   0x0fd510, STROFF(pmullw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[60][2]},
    {INVALID,   0xf30fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmullw, 0x660fd510, STROFF(pmullw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmullw, 0x660fd510, STROFF(vpmullw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[60][10]},
    {INVALID,   0xf20fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmullw, 0x660fd500, STROFF(vpmullw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fd510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 61 */
  {
    {INVALID,   0x0fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_movq2dq, 0xf30fd610, STROFF(movq2dq), Vdq, xx, Nq, xx, xx, mrm, x, END_LIST},
    {OP_movq, 0x660fd610, STROFF(movq), Wq_dq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {OP_movdq2q, 0xf20fd610, STROFF(movdq2q), Pq, xx, Uq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID,   0x0fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0x660fd610, STROFF(vmovq), Wq_dq, xx, Vq_dq, xx, xx, mrm|vex, x, tpe[61][10]},
    {INVALID, 0xf20fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0x660fd640, STROFF(vmovq), Wq_dq, xx, Vq_dq, xx, xx, mrm|evex, x, tvexw[108][1]},
    {INVALID, 0xf20fd610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 62 */
  {
    {OP_pmovmskb,   0x0fd710, STROFF(pmovmskb), Gd, xx, Nq, xx, xx, mrm, x, tpe[62][2]},
    {INVALID,     0xf30fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmovmskb, 0x660fd710, STROFF(pmovmskb), Gd, xx, Udq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0xf20fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x0fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf30fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovmskb, 0x660fd710, STROFF(vpmovmskb), Gd, xx, Ux, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf20fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fd710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 63 */
  {
    {OP_psubusb,   0x0fd810, STROFF(psubusb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[63][2]},
    {INVALID,    0xf30fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubusb, 0x660fd810, STROFF(psubusb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubusb, 0x660fd810, STROFF(vpsubusb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[63][10]},
    {INVALID,    0xf20fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubusb, 0x660fd800, STROFF(vpsubusb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fd810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 64 */
  {
    {OP_psubusw,   0x0fd910, STROFF(psubusw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[64][2]},
    {INVALID,    0xf30fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubusw, 0x660fd910, STROFF(psubusw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubusw, 0x660fd910, STROFF(vpsubusw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[64][10]},
    {INVALID,    0xf20fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubusw, 0x660fd900, STROFF(vpsubusw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fd910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 65 */
  {
    {OP_pminub,   0x0fda10, STROFF(pminub), Pq, xx, Qq, Pq, xx, mrm, x, tpe[65][2]},
    {INVALID,    0xf30fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pminub, 0x660fda10, STROFF(pminub), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpminub, 0x660fda10, STROFF(vpminub), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[65][10]},
    {INVALID,    0xf20fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpminub, 0x660fda00, STROFF(vpminub), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 66 */
  {
    {OP_pand,   0x0fdb10, STROFF(pand), Pq, xx, Qq, Pq, xx, mrm, x, tpe[66][2]},
    {INVALID,    0xf30fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pand, 0x660fdb10, STROFF(pand), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpand, 0x660fdb10, STROFF(vpand), Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fdb10, STROFF(evex_Wb_ext_41), xx, xx, xx, xx, xx, mrm|evex, x, 41},
    {INVALID, 0xf20fdb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 67 */
  {
    {OP_paddusb,   0x0fdc10, STROFF(paddusb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[67][2]},
    {INVALID,    0xf30fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddusb, 0x660fdc10, STROFF(paddusb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddusb, 0x660fdc10, STROFF(vpaddusb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[67][10]},
    {INVALID,    0xf20fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddusb, 0x660fdc00, STROFF(vpaddusb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fdc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 68 */
  {
    {OP_paddusw,   0x0fdd10, STROFF(paddusw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[68][2]},
    {INVALID,    0xf30fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddusw, 0x660fdd10, STROFF(paddusw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddusw, 0x660fdd10, STROFF(vpaddusw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[68][10]},
    {INVALID,    0xf20fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddusw, 0x660fdd00, STROFF(vpaddusw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fdd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 69 */
  {
    {OP_pmaxub,   0x0fde10, STROFF(pmaxub), Pq, xx, Qq, Pq, xx, mrm, x, tpe[69][2]},
    {INVALID,    0xf30fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaxub, 0x660fde10, STROFF(pmaxub), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaxub, 0x660fde10, STROFF(vpmaxub), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[69][10]},
    {INVALID,    0xf20fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaxub, 0x660fde00, STROFF(vpmaxub), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fde10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 70 */
  {
    {OP_pandn,   0x0fdf10, STROFF(pandn), Pq, xx, Qq, Pq, xx, mrm, x, tpe[70][2]},
    {INVALID,    0xf30fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pandn, 0x660fdf10, STROFF(pandn), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpandn, 0x660fdf10, STROFF(vpandn), Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fdf10, STROFF(evex_Wb_ext_42), xx, xx, xx, xx, xx, mrm|evex, x, 42},
    {INVALID, 0xf20fdf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 71 */
  {
    {OP_pavgb,   0x0fe010, STROFF(pavgb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[71][2]},
    {INVALID,    0xf30fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pavgb, 0x660fe010, STROFF(pavgb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpavgb, 0x660fe010, STROFF(vpavgb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[71][10]},
    {INVALID,    0xf20fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpavgb, 0x660fe000, STROFF(vpavgb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 72 */
  {
    {OP_psraw,   0x0fe110, STROFF(psraw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[72][2]},
    {INVALID,    0xf30fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psraw, 0x660fe110, STROFF(psraw), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[105][0]},
    {INVALID,    0xf20fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsraw, 0x660fe110, STROFF(vpsraw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[105][6]},
    {INVALID,    0xf20fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw, 0x660fe100, STROFF(vpsraw), Ve, xx, KEd, He, Wdq, mrm|evex|ttm128, x, tpe[105][10]},
    {INVALID, 0xf20fe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 73 */
  {
    {OP_psrad,   0x0fe210, STROFF(psrad), Pq, xx, Qq, Pq, xx, mrm, x, tpe[73][2]},
    {INVALID,    0xf30fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrad, 0x660fe210, STROFF(psrad), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[108][0]},
    {INVALID,    0xf20fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrad, 0x660fe210, STROFF(vpsrad), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[108][6]},
    {INVALID,    0xf20fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x60fe210, STROFF(evex_Wb_ext_121), xx, xx, xx, xx, xx, mrm|evex, x, 121},
    {INVALID, 0xf20fe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 74 */
  {
    {OP_pavgw,   0x0fe310, STROFF(pavgw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[74][2]},
    {INVALID,    0xf30fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pavgw, 0x660fe310, STROFF(pavgw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpavgw, 0x660fe310, STROFF(vpavgw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[74][10]},
    {INVALID,    0xf20fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpavgw, 0x660fe300, STROFF(vpavgw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 75 */
  {
    {OP_pmulhuw,   0x0fe410, STROFF(pmulhuw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[75][2]},
    {INVALID,    0xf30fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmulhuw, 0x660fe410, STROFF(pmulhuw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmulhuw, 0x660fe410, STROFF(vpmulhuw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[75][10]},
    {INVALID,    0xf20fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhuw, 0x660fe400, STROFF(vpmulhuw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 76 */
  {
    {OP_pmulhw,   0x0fe510, STROFF(pmulhw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[76][2]},
    {INVALID,    0xf30fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmulhw, 0x660fe510, STROFF(pmulhw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmulhw, 0x660fe510, STROFF(vpmulhw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[76][10]},
    {INVALID,    0xf20fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhw, 0x660fe500, STROFF(vpmulhw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 77 */
  {
    {INVALID, 0x0fe610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_cvtdq2pd, 0xf30fe610, STROFF(cvtdq2pd),  Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvttpd2dq,0x660fe610, STROFF(cvttpd2dq), Vdq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2dq, 0xf20fe610, STROFF(cvtpd2dq),  Vdq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {INVALID,        0x0fe610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vcvtdq2pd, 0xf30fe610, STROFF(vcvtdq2pd),  Vvd, xx, Wh_e, xx, xx, mrm|vex, x, tevexwb[57][0]},
    {OP_vcvttpd2dq,0x660fe610, STROFF(vcvttpd2dq), Vx, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[222][2]},
    {OP_vcvtpd2dq, 0xf20fe610, STROFF(vcvtpd2dq),  Vx, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[223][2]},
    {INVALID,   0x0fe610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30fe610, STROFF(evex_Wb_ext_57), xx, xx, xx, xx, xx, mrm|evex, x, 57},
    {EVEX_Wb_EXT, 0x660fe650, STROFF(evex_Wb_ext_222), xx, xx, xx, xx, xx, mrm|evex, x, 222},
    {EVEX_Wb_EXT, 0xf20fe650, STROFF(evex_Wb_ext_223), xx, xx, xx, xx, xx, mrm|evex, x, 223},
  }, /* prefix extension 78 */
  {
    {OP_movntq,    0x0fe710, STROFF(movntq),  Mq, xx, Pq, xx, xx, mrm, x, END_LIST},
    {INVALID,    0xf30fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_movntdq, 0x660fe710, STROFF(movntdq), Mdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovntdq, 0x660fe710, STROFF(vmovntdq), Mx, xx, Vx, xx, xx, mrm|vex, x, tpe[78][10]},
    {INVALID,    0xf20fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovntdq, 0x660fe700, STROFF(vmovntdq), Me, xx, Ve, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,     0xf20fe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 79 */
  {
    {OP_psubsb,   0x0fe810, STROFF(psubsb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[79][2]},
    {INVALID,    0xf30fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubsb, 0x660fe810, STROFF(psubsb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubsb, 0x660fe810, STROFF(vpsubsb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[79][10]},
    {INVALID,    0xf20fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubsb, 0x660fe800, STROFF(vpsubsb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 80 */
  {
    {OP_psubsw,   0x0fe910, STROFF(psubsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[80][2]},
    {INVALID,    0xf30fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubsw, 0x660fe910, STROFF(psubsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubsw, 0x660fe910, STROFF(vpsubsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[80][10]},
    {INVALID,    0xf20fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubsw, 0x660fe900, STROFF(vpsubsw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 81 */
  {
    {OP_pminsw,   0x0fea10, STROFF(pminsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[81][2]},
    {INVALID,    0xf30fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pminsw, 0x660fea10, STROFF(pminsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpminsw, 0x660fea10, STROFF(vpminsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[81][10]},
    {INVALID,    0xf20fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpminsw, 0x660fea00, STROFF(vpminsw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 82 */
  {
    {OP_por,   0x0feb10, STROFF(por), Pq, xx, Qq, Pq, xx, mrm, x, tpe[82][2]},
    {INVALID,    0xf30feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_por, 0x660feb10, STROFF(por), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpor, 0x660feb10, STROFF(vpor), Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660feb10, STROFF(evex_Wb_ext_43), xx, xx, xx, xx, xx, mrm|evex, x, 43},
    {INVALID, 0xf20feb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 83 */
  {
    {OP_paddsb,   0x0fec10, STROFF(paddsb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[83][2]},
    {INVALID,    0xf30fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddsb, 0x660fec10, STROFF(paddsb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddsb, 0x660fec10, STROFF(vpaddsb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[83][10]},
    {INVALID,    0xf20fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddsb, 0x660fec00, STROFF(vpaddsb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 84 */
  {
    {OP_paddsw,   0x0fed10, STROFF(paddsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[84][2]},
    {INVALID,    0xf30fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddsw, 0x660fed10, STROFF(paddsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddsw, 0x660fed10, STROFF(vpaddsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[84][10]},
    {INVALID,    0xf20fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddsw, 0x660fed00, STROFF(vpaddsw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 85 */
  {
    {OP_pmaxsw,   0x0fee10, STROFF(pmaxsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[85][2]},
    {INVALID,    0xf30fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaxsw, 0x660fee10, STROFF(pmaxsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaxsw, 0x660fee10, STROFF(vpmaxsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[85][10]},
    {INVALID,    0xf20fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaxsw, 0x660fee00, STROFF(vpmaxsw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 86 */
  {
    {OP_pxor,   0x0fef10, STROFF(pxor), Pq, xx, Qq, Pq, xx, mrm, x, tpe[86][2]},
    {INVALID,    0xf30fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pxor, 0x660fef10, STROFF(pxor), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpxor, 0x660fef10, STROFF(vpxor), Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fef10, STROFF(evex_Wb_ext_44), xx, xx, xx, xx, xx, mrm|evex, x, 44},
    {INVALID, 0xf20fef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 87 */
  {
    {OP_psllw,   0x0ff110, STROFF(psllw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[87][2]},
    {INVALID,    0xf30ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psllw, 0x660ff110, STROFF(psllw), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[106][0]},
    {INVALID,    0xf20ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsllw,  0x660ff110, STROFF(vpsllw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[106][6]},
    {INVALID,    0xf20ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw, 0x660ff100, STROFF(vpsllw), Ve, xx, KEd, He, Wdq, mrm|evex|ttm128, x, tpe[106][10]},
    {INVALID, 0xf20ff110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 88 */
  {
    {OP_pslld,   0x0ff210, STROFF(pslld), Pq, xx, Qq, Pq, xx, mrm, x, tpe[88][2]},
    {INVALID,    0xf30ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pslld, 0x660ff210, STROFF(pslld), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[109][0]},
    {INVALID,    0xf20ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpslld, 0x660ff210, STROFF(vpslld), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[109][6]},
    {INVALID,    0xf20ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ff200, STROFF(evex_Wb_ext_243), xx, xx, xx, xx, xx, mrm|evex, x, 243},
    {INVALID, 0xf20ff210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 89 */
  {
    {OP_psllq,   0x0ff310, STROFF(psllq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[89][2]},
    {INVALID,    0xf30ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psllq, 0x660ff310, STROFF(psllq), Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[111][0]},
    {INVALID,    0xf20ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsllq, 0x660ff310, STROFF(vpsllq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[111][6]},
    {INVALID,    0xf20ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ff340, STROFF(evex_Wb_ext_228), xx, xx, xx, xx, xx, mrm|evex, x, 228},
    {INVALID, 0xf20ff310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 90 */
  {
    {OP_pmuludq,   0x0ff410, STROFF(pmuludq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[90][2]},
    {INVALID,    0xf30ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmuludq, 0x660ff410, STROFF(pmuludq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmuludq, 0x660ff410, STROFF(vpmuludq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[217][2]},
    {INVALID,    0xf20ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ff440, STROFF(evex_Wb_ext_217), xx, xx, xx, xx, xx, mrm|evex, x, 217},
    {INVALID, 0xf20ff410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 91 */
  {
    {OP_pmaddwd,   0x0ff510, STROFF(pmaddwd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[91][2]},
    {INVALID,    0xf30ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaddwd, 0x660ff510, STROFF(pmaddwd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaddwd, 0x660ff510, STROFF(vpmaddwd), Vx, xx, Hx, Wx, xx, mrm|vex|ttfvm, x, tpe[91][10]},
    {INVALID,    0xf20ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddwd, 0x660ff500, STROFF(vpmaddwd), Ve, xx, KEw, He, We, mrm|evex, x, END_LIST},
    {INVALID, 0xf20ff510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 92 */
  {
    {OP_psadbw,   0x0ff610, STROFF(psadbw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[92][2]},
    {INVALID,    0xf30ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psadbw, 0x660ff610, STROFF(psadbw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsadbw, 0x660ff610, STROFF(vpsadbw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[92][10]},
    {INVALID,    0xf20ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsadbw, 0x660ff600, STROFF(vpsadbw), Ve, xx, He, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ff610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 93 */
  {
    {OP_maskmovq,     0x0ff710, STROFF(maskmovq), Bq, xx, Pq, Nq, xx, mrm|predcx, x, END_LIST}, /* Intel table says "Ppi, Qpi" */
    {INVALID,       0xf30ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_maskmovdqu, 0x660ff710, STROFF(maskmovdqu), Bdq, xx, Vdq, Udq, xx, mrm|predcx, x, END_LIST},
    {INVALID,       0xf20ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmaskmovdqu, 0x660ff710, STROFF(vmaskmovdqu), Bdq, xx, Vdq, Udq, xx, mrm|vex|reqL0|predcx, x, END_LIST},
    {INVALID,       0xf20ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20ff710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 94 */
  {
    {OP_psubb,   0x0ff810, STROFF(psubb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[94][2]},
    {INVALID,    0xf30ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubb, 0x660ff810, STROFF(psubb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubb, 0x660ff810, STROFF(vpsubb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[94][10]},
    {INVALID,    0xf20ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubb, 0x660ff800, STROFF(vpsubb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 95 */
  {
    {OP_psubw,   0x0ff910, STROFF(psubw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[95][2]},
    {INVALID,    0xf30ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubw, 0x660ff910, STROFF(psubw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubw, 0x660ff910, STROFF(vpsubw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[95][10]},
    {INVALID,    0xf20ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubw, 0x660ff900, STROFF(vpsubw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 96 */
  {
    {OP_psubd,   0x0ffa10, STROFF(psubd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[96][2]},
    {INVALID,    0xf30ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubd, 0x660ffa10, STROFF(psubd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubd, 0x660ffa10, STROFF(vpsubd), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[241][0]},
    {INVALID,    0xf20ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ffa00, STROFF(evex_Wb_ext_241), xx, xx, xx, xx, xx, mrm|evex, x, 241},
    {INVALID, 0xf20ffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 97 */
  {
    {OP_psubq,   0x0ffb10, STROFF(psubq), Pq, xx, Qq, Pq, xx, mrm, x, tpe[97][2]},
    {INVALID,  0xf30ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psubq, 0x660ffb10, STROFF(psubq), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,  0xf20ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubq, 0x660ffb10, STROFF(vpsubq), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[226][2]},
    {INVALID,  0xf20ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ffb40, STROFF(evex_Wb_ext_226), xx, xx, xx, xx, xx, mrm|evex, x, 226},
    {INVALID, 0xf20ffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 98 */
  {
    {OP_paddb,   0x0ffc10, STROFF(paddb), Pq, xx, Qq, Pq, xx, mrm, x, tpe[98][2]},
    {INVALID,    0xf30ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddb, 0x660ffc10, STROFF(paddb), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddb, 0x660ffc10, STROFF(vpaddb), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[98][10]},
    {INVALID,    0xf20ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddb, 0x660ffc00, STROFF(vpaddb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 99 */
  {
    {OP_paddw,   0x0ffd10, STROFF(paddw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[99][2]},
    {INVALID,    0xf30ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddw, 0x660ffd10, STROFF(paddw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddw, 0x660ffd10, STROFF(vpaddw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[99][10]},
    {INVALID,    0xf20ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddw, 0x660ffd00, STROFF(vpaddw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 100 */
  {
    {OP_paddd,   0x0ffe10, STROFF(paddd), Pq, xx, Qq, Pq, xx, mrm, x, tpe[100][2]},
    {INVALID,    0xf30ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddd, 0x660ffe10, STROFF(paddd), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddd, 0x660ffe10, STROFF(vpaddd), Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[242][0]},
    {INVALID,    0xf20ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ffe00, STROFF(evex_Wb_ext_242), xx, xx, xx, xx, xx, mrm|evex, x, 242},
    {INVALID, 0xf20ffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 101: all assumed to have Ib */
  {
    {INVALID,     0x0f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrldq, 0x660f7333, STROFF(psrldq), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrldq, 0x660f7333, STROFF(vpsrldq), Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[101][10]},
    {INVALID,   0xf20f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrldq, 0x660f7323, STROFF(vpsrldq), He, xx, Ib, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7333, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 102: all assumed to have Ib */
  {
    {INVALID,     0x0f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_pslldq, 0x660f7337, STROFF(pslldq), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslldq, 0x660f7337, STROFF(vpslldq), Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[102][10]},
    {INVALID,   0xf20f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslldq, 0x660f7327, STROFF(vpslldq), He, xx, Ib, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7337, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 103 */
  {
    {REX_B_EXT,  0x900000, STROFF(rexb_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    {OP_pause,0xf3900000, STROFF(pause), xx, xx, xx, xx, xx, no, x, END_LIST},
    {REX_B_EXT, 0x900000, STROFF(rexb_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    {REX_B_EXT, 0xf2900000, STROFF(rexb_ext_0), xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,     0x900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf3900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x66900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf2900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2900000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 104: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrlw,    0x0f7132, STROFF(psrlw), Nq, xx, Ib, Nq, xx, mrm, x, tpe[104][2]},
    {INVALID,   0xf30f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrlw,  0x660f7132, STROFF(psrlw), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw,  0x660f7132, STROFF(vpsrlw), Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[56][10]},
    {INVALID,   0xf20f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw, 0x660f7122, STROFF(vpsrlw), He, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7132, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 105: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psraw,    0x0f7134, STROFF(psraw), Nq, xx, Ib, Nq, xx, mrm, x, tpe[105][2]},
    {INVALID,   0xf30f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psraw,  0x660f7134, STROFF(psraw), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw,  0x660f7134, STROFF(vpsraw), Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[72][10]},
    {INVALID,   0xf20f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw, 0x660f7124, STROFF(vpsraw), He, xx, KEw, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7134, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 106: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psllw,    0x0f7136, STROFF(psllw), Nq, xx, Ib, Nq, xx, mrm, x, tpe[106][2]},
    {INVALID,   0xf30f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psllw,  0x660f7136, STROFF(psllw), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw,  0x660f7136, STROFF(vpsllw), Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[87][10]},
    {INVALID,   0xf20f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw,  0x660f7126, STROFF(vpsllw), He, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7136, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 107: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrld,    0x0f7232, STROFF(psrld), Nq, xx, Ib, Nq, xx, mrm, x, tpe[107][2]},
    {INVALID,   0xf30f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrld,  0x660f7232, STROFF(psrld), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrld,  0x660f7232, STROFF(vpsrld), Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[123][0]},
    {INVALID,   0xf20f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7232, STROFF(evex_Wb_ext_124), xx, xx, xx, xx, xx, mrm|evex, x, 124},
    {INVALID, 0xf20f7232, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 108: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrad,    0x0f7234, STROFF(psrad), Nq, xx, Ib, Nq, xx, mrm, x, tpe[108][2]},
    {INVALID,   0xf30f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrad,  0x660f7234, STROFF(psrad), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrad,  0x660f7234, STROFF(vpsrad), Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[121][0]},
    {INVALID,   0xf20f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7234, STROFF(evex_Wb_ext_122), xx, xx, xx, xx, xx, mrm|evex, x, 122},
    {INVALID, 0xf20f7234, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 109: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_pslld,    0x0f7236, STROFF(pslld), Nq, xx, Ib, Nq, xx, mrm, x, tpe[109][2]},
    {INVALID,   0xf30f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_pslld,  0x660f7236, STROFF(pslld), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslld,  0x660f7236, STROFF(vpslld), Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[243][0]},
    {INVALID,   0xf20f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f7226, STROFF(evex_Wb_ext_244), xx, xx, xx, xx, xx, mrm|evex, x, 244},
    {INVALID, 0xf20f7236, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 110: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrlq,    0x0f7332, STROFF(psrlq), Nq, xx, Ib, Nq, xx, mrm, x, tpe[110][2]},
    {INVALID,   0xf30f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrlq,  0x660f7332, STROFF(psrlq), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlq,  0x660f7332, STROFF(vpsrlq), Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[125][2]},
    {INVALID,   0xf20f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f7332, STROFF(evex_Wb_ext_126), xx, xx, xx, xx, xx, mrm|evex, x, 126},
    {INVALID, 0xf20f7332, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 111: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psllq,    0x0f7336, STROFF(psllq), Nq, xx, Ib, Nq, xx, mrm, x, tpe[111][2]},
    {INVALID,   0xf30f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_psllq,  0x660f7336, STROFF(psllq), Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq,  0x660f7336, STROFF(vpsllq), Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[228][2]},
    {INVALID,   0xf20f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7366, STROFF(evex_Wb_ext_229), xx, xx, xx, xx, xx, mrm|evex, x, 229},
    {INVALID, 0xf20f7336, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 112 */
  {
    {OP_movq,     0x0f6f10, STROFF(movq), Pq, xx, Qq, xx, xx, mrm, x, tpe[113][0]},
    {OP_movdqu, 0xf30f6f10, STROFF(movdqu), Vdq, xx, Wdq, xx, xx, mrm, x, tpe[113][1]},
    {OP_movdqa, 0x660f6f10, STROFF(movdqa), Vdq, xx, Wdq, xx, xx, mrm, x, tpe[113][2]},
    {INVALID,   0xf20f6f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f6f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovdqu, 0xf30f6f10, STROFF(vmovdqu), Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[113][5]},
    {OP_vmovdqa, 0x660f6f10, STROFF(vmovdqa), Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[113][6]},
    {INVALID,   0xf20f6f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f6f10, STROFF(evex_Wb_ext_11), xx, xx, xx, xx, xx, mrm|evex, x, 11},
    {EVEX_Wb_EXT, 0x660f6f10, STROFF(evex_Wb_ext_8), xx, xx, xx, xx, xx, mrm|evex, x, 8},
    {EVEX_Wb_EXT, 0xf20f6f10, STROFF(evex_Wb_ext_10), xx, xx, xx, xx, xx, mrm|evex, x, 10},
  }, /* prefix extension 113 */
  {
    {OP_movq,     0x0f7f10, STROFF(movq), Qq, xx, Pq, xx, xx, mrm, x, tpe[51][1]},
    {OP_movdqu, 0xf30f7f10, STROFF(movdqu), Wdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {OP_movdqa, 0x660f7f10, STROFF(movdqa), Wdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovdqu, 0xf30f7f10, STROFF(vmovdqu), Wx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {OP_vmovdqa, 0x660f7f10, STROFF(vmovdqa), Wx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7f10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f7f10, STROFF(evex_Wb_ext_13), xx, xx, xx, xx, xx, mrm|evex, x, 13},
    {EVEX_Wb_EXT, 0x660f7f10, STROFF(evex_Wb_ext_9), xx, xx, xx, xx, xx, mrm|evex, x, 9},
    {EVEX_Wb_EXT, 0xf20f7f10, STROFF(evex_Wb_ext_12), xx, xx, xx, xx, xx, mrm|evex, x, 12},
  }, /* prefix extension 114 */
  {
    {INVALID,     0x0f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_haddpd, 0x660f7c10, STROFF(haddpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_haddps, 0xf20f7c10, STROFF(haddps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vhaddpd, 0x660f7c10, STROFF(vhaddpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vhaddps, 0xf20f7c10, STROFF(vhaddps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f7c10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 115 */
  {
    {INVALID,     0x0f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_hsubpd, 0x660f7d10, STROFF(hsubpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_hsubps, 0xf20f7d10, STROFF(hsubps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vhsubpd, 0x660f7d10, STROFF(vhsubpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vhsubps, 0xf20f7d10, STROFF(vhsubps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f7d10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 116 */
  {
    {INVALID,     0x0fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_addsubpd, 0x660fd010, STROFF(addsubpd), Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_addsubps, 0xf20fd010, STROFF(addsubps), Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vaddsubpd, 0x660fd010, STROFF(vaddsubpd), Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vaddsubps, 0xf20fd010, STROFF(vaddsubps), Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fd010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 117 */
  {
    {INVALID,     0x0ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x660ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_lddqu,  0xf20ff010, STROFF(lddqu), Vdq, xx, Mdq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0x0ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x660ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vlddqu,  0xf20ff010, STROFF(vlddqu), Vx, xx, Mx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20ff010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, /***************************************************
   * SSSE3
   */
  { /* prefix extension 118 */
    {OP_pshufb,     0x380018, STROFF(pshufb),   Pq, xx, Qq, Pq, xx, mrm, x, tpe[118][2]},
    {INVALID,     0xf3380018, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {OP_pshufb,   0x66380018, STROFF(pshufb),   Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf2380018, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x380018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3380018, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufb,   0x66380018, STROFF(vpshufb),   Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[118][10]},
    {INVALID,     0xf2380018, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufb,   0x66380008, STROFF(vpshufb),   Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2380018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 119 */
    {OP_phaddw,      0x380118, STROFF(phaddw),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[119][2]},
    {INVALID,      0xf3380118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddw,    0x66380118, STROFF(phaddw),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddw,    0x66380118, STROFF(vphaddw),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 120 */
    {OP_phaddd,      0x380218, STROFF(phaddd),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[120][2]},
    {INVALID,      0xf3380218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddd,    0x66380218, STROFF(phaddd),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380218, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddd,    0x66380218, STROFF(vphaddd),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380218, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380218, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380218, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380218, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 121 */
    {OP_phaddsw,     0x380318, STROFF(phaddsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[121][2]},
    {INVALID,      0xf3380318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddsw,   0x66380318, STROFF(phaddsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddsw,   0x66380318, STROFF(vphaddsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 122 */
    {OP_pmaddubsw,   0x380418, STROFF(pmaddubsw),Pq, xx, Qq, Pq, xx, mrm, x, tpe[122][2]},
    {INVALID,      0xf3380418, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmaddubsw, 0x66380418, STROFF(pmaddubsw),Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380418, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380418, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddubsw, 0x66380418, STROFF(vpmaddubsw),Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[122][10]},
    {INVALID,      0xf2380418, STROFF(BAD),    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddubsw, 0x66380408, STROFF(vpmaddubsw),Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2380418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 123 */
    {OP_phsubw,      0x380518, STROFF(phsubw),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[123][2]},
    {INVALID,      0xf3380518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubw,    0x66380518, STROFF(phsubw),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubw,    0x66380518, STROFF(vphsubw),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 124 */
    {OP_phsubd,      0x380618, STROFF(phsubd),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[124][2]},
    {INVALID,      0xf3380618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubd,    0x66380618, STROFF(phsubd),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubd,    0x66380618, STROFF(vphsubd),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 125 */
    {OP_phsubsw,     0x380718, STROFF(phsubsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[125][2]},
    {INVALID,      0xf3380718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubsw,   0x66380718, STROFF(phsubsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubsw,   0x66380718, STROFF(vphsubsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 126 */
    {OP_psignb,      0x380818, STROFF(psignb),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[126][2]},
    {INVALID,      0xf3380818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignb,    0x66380818, STROFF(psignb),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignb,    0x66380818, STROFF(vpsignb),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 127 */
    {OP_psignw,      0x380918, STROFF(psignw),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[127][2]},
    {INVALID,      0xf3380918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignw,    0x66380918, STROFF(psignw),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignw,    0x66380918, STROFF(vpsignw),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 128 */
    {OP_psignd,      0x380a18, STROFF(psignd),  Pq, xx, Qq, Pq, xx, mrm, x, tpe[128][2]},
    {INVALID,      0xf3380a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignd,    0x66380a18, STROFF(psignd),  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignd,    0x66380a18, STROFF(vpsignd),  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 129 */
    {OP_pmulhrsw,    0x380b18, STROFF(pmulhrsw), Pq, xx, Qq, Pq, xx, mrm, x, tpe[129][2]},
    {INVALID,      0xf3380b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmulhrsw,  0x66380b18, STROFF(pmulhrsw), Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380b18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhrsw,  0x66380b18, STROFF(vpmulhrsw), Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[129][10]},
    {INVALID,      0xf2380b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380b18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380b18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhrsw,  0x66380b08, STROFF(vpmulhrsw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2380b18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 130 */
    {OP_pabsb,       0x381c18, STROFF(pabsb),   Pq, xx, Qq, xx, xx, mrm, x, tpe[130][2]},
    {INVALID,      0xf3381c18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsb,     0x66381c18, STROFF(pabsb),   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381c18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381c18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsb,     0x66381c18, STROFF(vpabsb),   Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[130][10]},
    {INVALID,      0xf2381c18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x381c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3381c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsb, 0x66381c08, STROFF(vpabsb),   Ve, xx, KEq, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2381c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 131 */
    {OP_pabsw,       0x381d18, STROFF(pabsw),   Pq, xx, Qq, xx, xx, mrm, x, tpe[131][2]},
    {INVALID,      0xf3381d18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsw,     0x66381d18, STROFF(pabsw),   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381d18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381d18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsw,     0x66381d18, STROFF(vpabsw),   Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[131][10]},
    {INVALID,      0xf2381d18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x381d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3381d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsw, 0x66381d08, STROFF(vpabsw),   Ve, xx, KEd, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2381d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 132 */
    {OP_pabsd,       0x381e18, STROFF(pabsd),   Pq, xx, Qq, xx, xx, mrm, x, tpe[132][2]},
    {INVALID,      0xf3381e18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsd,     0x66381e18, STROFF(pabsd),   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381e18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381e18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsd,     0x66381e18, STROFF(vpabsd),   Vx, xx, Wx, xx, xx, mrm|vex, x, tevexwb[146][0]},
    {INVALID,      0xf2381e18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x381e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3381e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x66381e18, STROFF(evex_Wb_ext_146), xx, xx, xx, xx, xx, mrm|evex, x, 146},
    {INVALID, 0xf2381e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 133: all assumed to have Ib */
    {OP_palignr,     0x3a0f18, STROFF(palignr), Pq, xx, Qq, Ib, Pq, mrm, x, tpe[133][2]},
    {INVALID,      0xf33a0f18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_palignr,   0x663a0f18, STROFF(palignr), Vdq, xx, Wdq, Ib, Vdq, mrm, x, END_LIST},
    {INVALID,      0xf23a0f18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x3a0f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf33a0f18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpalignr,   0x663a0f18, STROFF(vpalignr), Vx, xx, Hx, Wx, Ib, mrm|vex, x, tpe[133][10]},
    {INVALID,      0xf23a0f18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x3a0f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf33a0f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpalignr, 0x663a0f08, STROFF(vpalignr), Ve, xx, KEq, Ib, He, xop|mrm|evex|ttfvm, x, exop[248]},
    {INVALID, 0xf23a0f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 134 */
    {OP_vmread,      0x0f7810, STROFF(vmread),  Ey, xx, Gy, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf30f7810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* FIXME PR 338279: this is listed as /0 but I'm not going to chain it into
     * the reg extensions table until I can verify, since gdb thinks it
     * does NOT need /0.  Waiting for a processor that actually supports it.
     * It's ok for DR proper to think a non-cti instr is valid when really it's not,
     * though for our decoding library use we should get it right.
     */
    {OP_extrq,     0x660f7810, STROFF(extrq),   Udq, xx, Ib, Ib, xx, mrm, x, tpe[135][2]},
    /* FIXME: is src or dst Udq? */
    {OP_insertq,   0xf20f7810, STROFF(insertq), Vdq, xx, Udq, Ib, Ib, mrm, x, tpe[135][3]},
    {INVALID,        0x0f7810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f7810, STROFF(evex_Wb_ext_49), xx, xx, xx, xx, xx, mrm|evex, x, 49},
    {EVEX_Wb_EXT, 0xf30f7810, STROFF(evex_Wb_ext_54), xx, xx, xx, xx, xx, mrm|evex, x, 54},
    {EVEX_Wb_EXT, 0x660f7810, STROFF(evex_Wb_ext_51), xx, xx, xx, xx, xx, mrm|evex, x, 51},
    {EVEX_Wb_EXT, 0xf20f7810, STROFF(evex_Wb_ext_55), xx, xx, xx, xx, xx, mrm|evex, x, 55},
  }, { /* prefix extension 135 */
    {OP_vmwrite,     0x0f7910, STROFF(vmwrite), Gy, xx, Ey, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf30f7910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* FIXME: is src or dst Udq? */
    {OP_extrq,     0x660f7910, STROFF(extrq),   Vdq, xx, Udq, xx, xx, mrm, x, END_LIST},
    {OP_insertq,   0xf20f7910, STROFF(insertq), Vdq, xx, Udq, xx, xx, mrm, x, END_LIST},
    {INVALID,        0x0f7910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f7910, STROFF(evex_Wb_ext_47), xx, xx, xx, xx, xx, mrm|evex, x, 47},
    {EVEX_Wb_EXT, 0xf30f7910, STROFF(evex_Wb_ext_52), xx, xx, xx, xx, xx, mrm|evex, x, 52},
    {EVEX_Wb_EXT, 0x660f7910, STROFF(evex_Wb_ext_48), xx, xx, xx, xx, xx, mrm|evex, x, 48},
    {EVEX_Wb_EXT, 0xf20f7910, STROFF(evex_Wb_ext_53), xx, xx, xx, xx, xx, mrm|evex, x, 53},
  }, { /* prefix extension 136 */
    {OP_bsr,         0x0fbd10, STROFF(bsr),     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, END_LIST},
    /* XXX: if cpuid doesn't show lzcnt support, this is treated as bsr */
    {OP_lzcnt,     0xf30fbd10, STROFF(lzcnt),   Gv, xx, Ev, xx, xx, mrm, fW6, END_LIST},
    /* This is bsr w/ DATA_PREFIX, which we indicate by omitting 0x66 (i#1118).
     * It's not in the encoding chain.  Ditto for 0xf2.  If we keep the "all
     * prefix ext marked invalid are really treated valid" we don't need these,
     * but better to be explicit where we have to so we can easily remove that.
     */
    {OP_bsr,         0x0fbd10, STROFF(bsr),     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {OP_bsr,         0x0fbd10, STROFF(bsr),     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {INVALID,        0x0fbd10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fbd10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fbd10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fbd10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fbd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fbd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fbd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fbd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 137 */
    {OP_vmptrld,     0x0fc736, STROFF(vmptrld), xx, xx, Mq, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmxon,     0xf30fc736, STROFF(vmxon),   xx, xx, Mq, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmclear,   0x660fc736, STROFF(vmclear), Mq, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf20fc736, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0fc736, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fc736, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fc736, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fc736, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fc736, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fc736, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fc736, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fc736, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 138 */
    {OP_movbe,   0x38f018, STROFF(movbe), Gv, xx, Mv, xx, xx, mrm, x, tpe[139][0]},
    {INVALID,  0xf338f018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* really this is regular data-size prefix */
    {OP_movbe, 0x6638f018, STROFF(movbe), Gw, xx, Mw, xx, xx, mrm, x, tpe[139][2]},
    {OP_crc32, 0xf238f018, STROFF(crc32), Gy, xx, Eb, Gy, xx, mrm, x, END_LIST},
    {INVALID,    0x38f018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf338f018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0x6638f018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf238f018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x38f018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 139 */
    {OP_movbe,   0x38f118, STROFF(movbe), Mv, xx, Gv, xx, xx, mrm, x, tpe[138][2]},
    {INVALID,  0xf338f118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* really this is regular data-size prefix */
    {OP_movbe, 0x6638f118, STROFF(movbe), Mw, xx, Gw, xx, xx, mrm, x, END_LIST},
    /* The Intel table separates out a data-size prefix into Ey and Ew: ours are combined,
     * and thus we want Ev.
     */
    {OP_crc32, 0xf238f118, STROFF(crc32), Gy, xx, Ev, Gy, xx, mrm, x, tpe[138][3]},
    {INVALID,    0x38f118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf338f118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0x6638f118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf238f118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x38f118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 140 */
    {OP_bsf,         0x0fbc10, STROFF(bsf),     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, END_LIST},
    /* XXX: if cpuid doesn't show tzcnt support, this is treated as bsf */
    {OP_tzcnt,     0xf30fbc10, STROFF(tzcnt),   Gv, xx, Ev, xx, xx, mrm, fW6, END_LIST},
    /* see OP_bsr comments above -- this is the same but for bsf: */
    {OP_bsf,         0x0fbc10, STROFF(bsf),     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {OP_bsf,         0x0fbc10, STROFF(bsf),     Gv, xx, Ev, xx, xx, mrm|predcx, fW6, NA},
    {INVALID,        0x0fbc10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fbc10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fbc10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fbc10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fbc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fbc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fbc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fbc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 141 */
    {INVALID,        0x38f718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_bextr,       0x38f718, STROFF(bextr),   Gy, xx, Ey, By, xx, mrm|vex, fW6, txop[60]},
    {OP_sarx,      0xf338f718, STROFF(sarx),    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_shlx,      0x6638f718, STROFF(shlx),    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_shrx,      0xf238f718, STROFF(shrx),    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x38f718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 142 */
    {INVALID,        0x38f518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_bzhi,        0x38f518, STROFF(bzhi),    Gy, xx, Ey, By, xx, mrm|vex, fW6, END_LIST},
    {OP_pext,      0xf338f518, STROFF(pext),    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {INVALID,      0x6638f518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pdep,      0xf238f518, STROFF(pdep),    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x38f518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 143 */
    {INVALID,        0x38f618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_adox,      0xf338f618, STROFF(adox),    Gy, xx, Ey, Gy, xx, mrm, (fWO|fRO), END_LIST},
    {OP_adcx,      0x6638f618, STROFF(adcx),    Gy, xx, Ey, Gy, xx, mrm, (fWC|fRC), END_LIST},
    {INVALID,      0xf238f618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x38f618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_mulx,      0xf238f618, STROFF(mulx),    By, Gy, Ey, uDX, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x38f618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 144 */
    {INVALID,        0x0f9010, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9010, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9010, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9010, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9010, STROFF(vex_W_ext_74), xx, xx, xx, xx, xx, mrm|vex, x, 74},
    {INVALID,      0xf30f9010, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9010, STROFF(vex_W_ext_75), xx, xx, xx, xx, xx, mrm|vex, x, 75},
    {INVALID,      0xf20f9010, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 145 */
    {INVALID,        0x0f9110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9110, STROFF(vex_W_ext_76), xx, xx, xx, xx, xx, mrm|vex, x, 76},
    {INVALID,      0xf30f9110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9110, STROFF(vex_W_ext_77), xx, xx, xx, xx, xx, mrm|vex, x, 77},
    {INVALID,      0xf20f9110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 146 */
    {INVALID,        0x0f9210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9210, STROFF(vex_W_ext_78), xx, xx, xx, xx, xx, mrm|vex, x, 78},
    {INVALID,      0xf30f9210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9210, STROFF(vex_W_ext_79), xx, xx, xx, xx, xx, mrm|vex, x, 79},
    {VEX_W_EXT,    0xf20f9210, STROFF(vex_W_ext_106),xx, xx, xx, xx, xx, mrm|vex, x, 106},
    {INVALID,   0x0f9210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 147 */
    {INVALID,        0x0f9310, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9310, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9310, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9310, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9310, STROFF(vex_W_ext_80), xx, xx, xx, xx, xx, mrm|vex, x, 80},
    {INVALID,      0xf30f9310, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9310, STROFF(vex_W_ext_81), xx, xx, xx, xx, xx, mrm|vex, x, 81},
    {VEX_W_EXT,    0xf20f9310, STROFF(vex_W_ext_107),xx, xx, xx, xx, xx, mrm|vex, x, 107},
    {INVALID,   0x0f9310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 148 */
    {INVALID,        0x0f4110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4110, STROFF(vex_W_ext_82), xx, xx, xx, xx, xx, mrm|vex, x, 82},
    {INVALID,      0xf30f4110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4110, STROFF(vex_W_ext_83), xx, xx, xx, xx, xx, mrm|vex, x, 83},
    {INVALID,      0xf20f4110, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 149 */
    {INVALID,        0x0f4210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4210, STROFF(vex_W_ext_84), xx, xx, xx, xx, xx, mrm|vex, x, 84},
    {INVALID,      0xf30f4210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4210, STROFF(vex_W_ext_85), xx, xx, xx, xx, xx, mrm|vex, x, 85},
    {INVALID,      0xf20f4210, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 150 */
    {INVALID,        0x0f4b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4b10, STROFF(vex_W_ext_86), xx, xx, xx, xx, xx, mrm|vex, x, 86},
    {INVALID,      0xf30f4b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4b10, STROFF(vex_W_ext_87), xx, xx, xx, xx, xx, mrm|vex, x, 87},
    {INVALID,      0xf20f4b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 151 */
    {INVALID,        0x0f4410, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4410, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4410, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4410, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4410, STROFF(vex_W_ext_88), xx, xx, xx, xx, xx, mrm|vex, x, 88},
    {INVALID,      0xf30f4410, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4410, STROFF(vex_W_ext_89), xx, xx, xx, xx, xx, mrm|vex, x, 89},
    {INVALID,      0xf20f4410, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 152 */
    {INVALID,        0x0f4510, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4510, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4510, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4510, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4510, STROFF(vex_W_ext_90), xx, xx, xx, xx, xx, mrm|vex, x, 90},
    {INVALID,      0xf30f4510, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4510, STROFF(vex_W_ext_91), xx, xx, xx, xx, xx, mrm|vex, x, 91},
    {INVALID,      0xf20f4510, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 153 */
    {INVALID,        0x0f4610, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4610, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4610, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4610, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4610, STROFF(vex_W_ext_92), xx, xx, xx, xx, xx, mrm|vex, x, 92},
    {INVALID,      0xf30f4610, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4610, STROFF(vex_W_ext_93), xx, xx, xx, xx, xx, mrm|vex, x, 93},
    {INVALID,      0xf20f4610, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 154 */
    {INVALID,        0x0f4710, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4710, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4710, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4710, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4710, STROFF(vex_W_ext_94), xx, xx, xx, xx, xx, mrm|vex, x, 94},
    {INVALID,      0xf30f4710, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4710, STROFF(vex_W_ext_95), xx, xx, xx, xx, xx, mrm|vex, x, 95},
    {INVALID,      0xf20f4710, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 155 */
    {INVALID,        0x0f4a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4a10, STROFF(vex_W_ext_96), xx, xx, xx, xx, xx, mrm|vex, x, 96},
    {INVALID,      0xf30f4a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4a10, STROFF(vex_W_ext_97), xx, xx, xx, xx, xx, mrm|vex, x, 97},
    {INVALID,      0xf20f4a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 156 */
    {INVALID,        0x0f9810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9810, STROFF(vex_W_ext_98), xx, xx, xx, xx, xx, mrm|vex, x, 98},
    {INVALID,      0xf30f9810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9810, STROFF(vex_W_ext_99), xx, xx, xx, xx, xx, mrm|vex, x, 99},
    {INVALID,      0xf20f9810, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 157 */
    {INVALID,        0x0f9910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9910, STROFF(vex_W_ext_104), xx, xx, xx, xx, xx, mrm|vex, x, 104},
    {INVALID,      0xf30f9910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9910, STROFF(vex_W_ext_105), xx, xx, xx, xx, xx, mrm|vex, x, 105},
    {INVALID,      0xf20f9910, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 158 */
    {INVALID,        0x0f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7b10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0xf30f7b10, STROFF(evex_Wb_ext_58), xx, xx, xx, xx, xx, mrm|evex, x, 58},
    {EVEX_Wb_EXT,   0x660f7b10, STROFF(evex_Wb_ext_46), xx, xx, xx, xx, xx, mrm|evex, x, 46},
    {EVEX_Wb_EXT,   0xf20f7b10, STROFF(evex_Wb_ext_59), xx, xx, xx, xx, xx, mrm|evex, x, 59},
  }, { /* prefix extension 159 */
    {INVALID,        0x0f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7a10, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0xf30f7a10, STROFF(evex_Wb_ext_61), xx, xx, xx, xx, xx, mrm|evex, x, 61},
    {EVEX_Wb_EXT,   0x660f7a10, STROFF(evex_Wb_ext_50), xx, xx, xx, xx, xx, mrm|evex, x, 50},
    {EVEX_Wb_EXT,   0xf20f7a10, STROFF(evex_Wb_ext_60), xx, xx, xx, xx, xx, mrm|evex, x, 60},
  }, { /* prefix extension 160 */
    {INVALID,        0x383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovqb,   0xf3383208, STROFF(vpmovqb), Wj_e, xx, KEb, Ve, xx, mrm|evex|ttovm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovzxbq, 0x66383208, STROFF(vpmovzxbq), Ve, xx, KEb, Wj_e, xx, mrm|evex|ttovm, x, END_LIST},
    {INVALID,      0xf2383218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 161 */
    {INVALID,        0x382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovsqb,  0xf3382208, STROFF(vpmovsqb), Wj_e, xx, KEb, Ve, xx, mrm|evex|ttovm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsxbq, 0x66382208, STROFF(vpmovsxbq), Ve, xx, KEb, Wj_e, xx, mrm|evex|ttovm, x, END_LIST},
    {INVALID,      0xf2382218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 162 */
    {INVALID,        0x381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovusqb, 0xf3381208, STROFF(vpmovusqb), Wj_e, xx, KEb, Ve, xx, mrm|evex|ttovm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381218, STROFF(evex_Wb_ext_130), xx, xx, xx, xx, xx, mrm|evex, x, 130},
    {INVALID,      0xf2381218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 163 */
    {INVALID,        0x383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovqw,   0xf3383408, STROFF(vpmovqw), Wi_e, xx, KEb, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovzxwq, 0x66383408, STROFF(vpmovzxwq), Ve, xx, KEb, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2383418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 164 */
    {INVALID,        0x382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovsqw,  0xf3382408, STROFF(vpmovsqw), Wi_e, xx, KEb, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsxwq, 0x66382408, STROFF(vpmovsxwq), Ve, xx, KEb, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2382418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 165 */
    {INVALID,        0x381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovusqw, 0xf3381408, STROFF(vpmovusqw), Wi_e, xx, KEb, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381418, STROFF(evex_Wb_ext_119), xx, xx, xx, xx, xx, mrm|evex, x, 119},
    {INVALID,      0xf2381418, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 166 */
    {INVALID,        0x383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovqd,   0xf3383508, STROFF(vpmovqd), Wh_e, xx, KEb, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovzxdq, 0x66383508, STROFF(vpmovzxdq), Ve, xx, KEb, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2383518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 167 */
    {INVALID,        0x382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsqd,  0xf3382508, STROFF(vpmovsqd), Wh_e, xx, KEb, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovsxdq, 0x66382508, STROFF(vpmovsxdq), Ve, xx, KEb, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2382518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 168 */
    {INVALID,        0x381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovusqd, 0xf3381508, STROFF(vpmovusqd), Wh_e, xx, KEb, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381518, STROFF(evex_Wb_ext_117), xx, xx, xx, xx, xx, mrm|evex, x, 117},
    {INVALID,      0xf2381518, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 169 */
    {INVALID,        0x383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovdb,   0xf3383108, STROFF(vpmovdb), Wi_e, xx, KEw, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {OP_vpmovzxbd, 0x66383108, STROFF(vpmovzxbd), Ve, xx, KEw, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2383118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 170 */
    {INVALID,        0x382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsdb,  0xf3382108, STROFF(vpmovsdb), Wi_e, xx, KEw, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {OP_vpmovsxbd, 0x66382108, STROFF(vpmovsxbd), Ve, xx, KEw, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2382118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 171 */
    {INVALID,        0x381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovusdb, 0xf3381108, STROFF(vpmovusdb), Wi_e, xx, KEw, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381118, STROFF(evex_Wb_ext_127), xx, xx, xx, xx, xx, mrm|evex, x, 127},
    {INVALID,      0xf2381118, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 172 */
    {INVALID,        0x383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovdw,   0xf3383308, STROFF(vpmovdw), Wh_e, xx, KEw, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovzxwd, 0x66383308, STROFF(vpmovzxwd), Ve, xx, KEw, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2383318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 173 */
    {INVALID,        0x382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsdw,  0xf3382308, STROFF(vpmovsdw), Wh_e, xx, KEw, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovsxwd, 0x66382308, STROFF(vpmovsxwd), Ve, xx, KEw, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2382318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 174 */
    {INVALID,        0x381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovusdw, 0xf3381308, STROFF(vpmovusdw), Wh_e, xx, KEw, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {EVEX_Wb_EXT,  0x66381308, STROFF(evex_Wb_ext_263), xx, xx, xx, xx, xx, mrm|evex, x, 263},
    {INVALID,      0xf2381318, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 175 */
    {INVALID,        0x383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovwb,   0xf3383008, STROFF(vpmovwb), Wh_e, xx, KEd, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovzxbw, 0x66383008, STROFF(vpmovzxbw), Ve, xx, KEd, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2383018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 176 */
    {INVALID,        0x382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovswb,  0xf3382008, STROFF(vpmovswb), Wh_e, xx, KEd, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovsxbw, 0x66382008, STROFF(vpmovsxbw), Ve, xx, KEd, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2382018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 177 */
    {INVALID,        0x381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovuswb, 0xf3381008, STROFF(vpmovuswb), Wh_e, xx, KEd, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpsrlvw,   0x66381048, STROFF(vpsrlvw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,      0xf2381018, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 178 */
    {INVALID,       0x382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,  0xf3382818, STROFF(evex_Wb_ext_138), xx, xx, xx, xx, xx, mrm|evex, x, 138},
    {EVEX_Wb_EXT,  0x66382848, STROFF(evex_Wb_ext_227), xx, xx, xx, xx, xx, mrm|evex, x, 227},
    {INVALID,     0xf2382818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 179 */
    {INVALID,      0x383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3383818, STROFF(evex_Wb_ext_139), xx, xx, xx, xx, xx, mrm|evex, x, 139},
    {OP_vpminsb, 0x66383808, STROFF(vpminsb), Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,    0xf2383818, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 180 */
    {INVALID,       0x382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,  0xf3382918, STROFF(evex_Wb_ext_140), xx, xx, xx, xx, xx, mrm|evex, x, 140},
    {EVEX_Wb_EXT,  0x66382948, STROFF(evex_Wb_ext_233), xx, xx, xx, xx, xx, mrm|evex, x, 233},
    {INVALID,     0xf2382918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 181 */
    {INVALID,      0x383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3383918, STROFF(evex_Wb_ext_141), xx, xx, xx, xx, xx, mrm|evex, x, 141},
    {EVEX_Wb_EXT, 0x66383918, STROFF(evex_Wb_ext_113), xx, xx, xx, xx, xx, mrm|evex, x, 113},
    {INVALID,    0xf2383918, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 182 */
    {INVALID,      0x382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3382618, STROFF(evex_Wb_ext_171), xx, xx, xx, xx, xx, mrm, x, 171},
    {EVEX_Wb_EXT, 0x66382618, STROFF(evex_Wb_ext_169), xx, xx, xx, xx, xx, mrm, x, 169},
    {INVALID,    0xf2382618, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 183 */
    {INVALID,      0x382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3382718, STROFF(evex_Wb_ext_172), xx, xx, xx, xx, xx, mrm, x, 172},
    {EVEX_Wb_EXT, 0x66382718, STROFF(evex_Wb_ext_170), xx, xx, xx, xx, xx, mrm, x, 170},
    {INVALID,    0xf2382718, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 184 */
    {INVALID,      0x382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastmb2q, 0xf3382a48, STROFF(vpbroadcastmb2q), Ve, xx, KQb, xx, xx, mrm|evex|ttnone, x, NA},
    {OP_vmovntdqa, 0x66382a08, STROFF(vmovntdqa), Me, xx, Ve, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,    0xf2382a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 185 */
    {INVALID,      0x383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastmw2d, 0xf3383a08, STROFF(vpbroadcastmw2d), Ve, xx, KQw, xx, xx, mrm|evex|ttnone, x, NA},
    {OP_vpminuw, 0x66383a08, STROFF(vpminuw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,    0xf2383a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 186 */
    {OP_bndldx,    0x0f1a10, STROFF(bndldx), TRqdq, xx, Mm, xx, xx, mrm, x, END_LIST},
    {OP_bndcl,   0xf30f1a10, STROFF(bndcl), TRqdq, xx, Er, xx, xx, mrm, x, END_LIST},
    {OP_bndmov,  0x660f1a10, STROFF(bndmov), TRqdq, xx, TMqdq, xx, xx, mrm, x, tpe[187][2]},
    {OP_bndcu,   0xf20f1a10, STROFF(bndcu), TRqdq, xx, Er, xx, xx, mrm, x, END_LIST},
    {INVALID,      0x0f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1a18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 187 */
    {OP_bndstx,    0x0f1b10, STROFF(bndstx), Mm, xx, TRqdq, xx, xx, mrm, x, END_LIST},
    {OP_bndmk,   0xf30f1b10, STROFF(bndmk), TRqdq, xx, Mr, xx, xx, mrm, x, END_LIST},
    {OP_bndmov,  0x660f1b10, STROFF(bndmov), TMqdq, xx, TRqdq, xx, xx, mrm, x, END_LIST},
    {OP_bndcn,   0xf20f1b10, STROFF(bndcn), TRqdq, xx, Er, xx, xx, mrm, x, END_LIST},
    {INVALID,      0x0f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1b18, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 188 */
    {REX_W_EXT,    0x0fae34, STROFF(rexw_ext_2), xx, xx, xx, xx, xx, mrm, x, 2},
    {OP_ptwrite, 0xf30fae34, STROFF(ptwrite),   xx, xx, Ey, xx, xx, mrm, x, END_LIST},
    {INVALID,    0x660fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20fae34, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 189 */
    {INVALID,      0x385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* vex */
    {INVALID,      0x385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,  0x66385218, STROFF(vex_W_ext_112),   xx, xx, xx, xx, xx, no, x, 112},
    {INVALID,    0xf2385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    /* evex */
    {INVALID,      0x385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,0xf3385218, STROFF(evex_Wb_ext_273),   xx, xx, xx, xx, xx, no, x, 273},
    {EVEX_Wb_EXT,0x66385218, STROFF(evex_Wb_ext_269),   xx, xx, xx, xx, xx, no, x, 269},
    {INVALID,    0xf2385218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
  } , { /* prefix extension 190 */
    {INVALID,      0x387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,0xf3387208, STROFF(evex_Wb_ext_272), xx, xx, xx, xx, xx, mrm|evex|ttnone, x, 272},
    {INVALID,    0x66387218, STROFF(BAD),   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,0xf2387218, STROFF(evex_Wb_ext_271),   xx, xx, xx, xx, xx, mrm|evex|ttnone, x, 271},
  }
};
/****************************************************************************
 * Instructions that differ based on whether vex-encoded or not.
 * Most of these require an 0x66 prefix but we use reqp for that
 * so there's nothing inherent here about prefixes.
 */
const instr_info_t e_vex_extensions[][3] = {
  {    /* e_vex ext  0 */
    {INVALID, 0x663a4a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vblendvps, 0x663a4a18, STROFF(vblendvps), Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  1 */
    {INVALID, 0x663a4b18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vblendvpd, 0x663a4b18, STROFF(vblendvpd), Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4b18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  2 */
    {INVALID, 0x663a4c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpblendvb, 0x663a4c18, STROFF(vpblendvb), Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  3 */
    {OP_ptest,  0x66381718, STROFF(ptest),    xx, xx,  Vdq,Wdq, xx, mrm|reqp, fW6, END_LIST},
    {OP_vptest, 0x66381718, STROFF(vptest),   xx, xx,    Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x66381718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  4 */
    {OP_pmovsxbw,  0x66382018, STROFF(pmovsxbw), Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbw, 0x66382018, STROFF(vpmovsxbw), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[176][10]},
    {PREFIX_EXT,     0x382018, STROFF(prefix_ext_176), xx, xx, xx, xx, xx, mrm|evex, x, 176},
  }, { /* e_vex ext  5 */
    {OP_pmovsxbd,  0x66382118, STROFF(pmovsxbd), Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbd, 0x66382118, STROFF(vpmovsxbd), Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[170][10]},
    {PREFIX_EXT,     0x382118, STROFF(prefix_ext_170), xx, xx, xx, xx, xx, mrm|evex, x, 170},
  }, { /* e_vex ext  6 */
    /* XXX i#1312: the SSE and VEX table entries could get moved to prefix_extensions and
     * this table here re-numbered.
     */
    {OP_pmovsxbq,  0x66382218, STROFF(pmovsxbq), Vdq, xx, Ww_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbq, 0x66382218, STROFF(vpmovsxbq), Vx, xx, Wj_x, xx, xx, mrm|vex|reqp, x, tpe[161][10]},
    {PREFIX_EXT,     0x382218, STROFF(prefix_ext_161), xx, xx, xx, xx, xx, mrm|evex, x, 161},
  }, { /* e_vex ext  7 */
    {OP_pmovsxwd,  0x66382318, STROFF(pmovsxwd), Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxwd, 0x66382318, STROFF(vpmovsxwd), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[173][10]},
    {PREFIX_EXT,     0x382318, STROFF(prefix_ext_173), xx, xx, xx, xx, xx, mrm|evex, x, 173},
  }, { /* e_vex ext  8 */
    {OP_pmovsxwq,  0x66382418, STROFF(pmovsxwq), Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxwq, 0x66382418, STROFF(vpmovsxwq), Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[164][10]},
    {PREFIX_EXT,     0x382418, STROFF(prefix_ext_164), xx, xx, xx, xx, xx, mrm|evex, x, 164},
  }, { /* e_vex ext  9 */
    {OP_pmovsxdq,  0x66382518, STROFF(pmovsxdq), Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxdq, 0x66382518, STROFF(vpmovsxdq), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[167][10]},
    {PREFIX_EXT,     0x382518, STROFF(prefix_ext_167), xx, xx, xx, xx, xx, mrm|evex, x, 167},
  }, { /* e_vex ext 10 */
    {OP_pmuldq,  0x66382818, STROFF(pmuldq),   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmuldq, 0x66382818, STROFF(vpmuldq),   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[227][2]},
    {PREFIX_EXT,   0x382818, STROFF(prefix_ext_178), xx, xx, xx, xx, xx, mrm|evex, x, 178},
  }, { /* e_vex ext 11 */
    {OP_pcmpeqq,  0x66382918, STROFF(pcmpeqq),  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpcmpeqq, 0x66382918, STROFF(vpcmpeqq),  Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[233][2]},
    {PREFIX_EXT,    0x382918, STROFF(prefix_ext_180), xx, xx, xx, xx, xx, mrm|evex, x, 180},
  }, { /* e_vex ext 12 */
    {OP_movntdqa,  0x66382a18, STROFF(movntdqa), Mdq, xx, Vdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vmovntdqa, 0x66382a18, STROFF(vmovntdqa), Mx, xx, Vx, xx, xx, mrm|vex|reqp, x, tpe[184][10]},
    {PREFIX_EXT,    0x382a18, STROFF(prefix_ext_184), xx, xx, xx, xx, xx, mrm|evex, x, 184},
  }, { /* e_vex ext 13 */
    {OP_packusdw,  0x66382b18, STROFF(packusdw), Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpackusdw, 0x66382b18, STROFF(vpackusdw), Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[245][0]},
    {EVEX_Wb_EXT,  0x66382b18, STROFF(evex_Wb_ext_245), xx, xx, xx, xx, xx, mrm|evex, x, 245},
  }, { /* e_vex ext 14 */
    {OP_pmovzxbw,  0x66383018, STROFF(pmovzxbw), Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbw, 0x66383018, STROFF(vpmovzxbw), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[175][10]},
    {PREFIX_EXT,     0x383018, STROFF(prefix_ext_175), xx, xx, xx, xx, xx, mrm|evex, x, 175},
  }, { /* e_vex ext 15 */
    {OP_pmovzxbd,  0x66383118, STROFF(pmovzxbd), Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbd, 0x66383118, STROFF(vpmovzxbd), Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[169][10]},
    {PREFIX_EXT,     0x383118, STROFF(prefix_ext_169), xx, xx, xx, xx, xx, mrm|evex, x, 169},
  }, { /* e_vex ext 16 */
    /* XXX i#1312: the SSE and VEX table entries could get moved to prefix_extensions and
     * this table here re-numbered.
     */
    {OP_pmovzxbq,  0x66383218, STROFF(pmovzxbq), Vdq, xx, Ww_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbq, 0x66383218, STROFF(vpmovzxbq), Vx, xx, Wj_x, xx, xx, mrm|vex|reqp, x, tpe[160][10]},
    {PREFIX_EXT, 0x383218, STROFF(prefix_ext_160), xx, xx, xx, xx, xx, mrm|evex, x, 160},
  }, { /* e_vex ext 17 */
    {OP_pmovzxwd,  0x66383318, STROFF(pmovzxwd), Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxwd, 0x66383318, STROFF(vpmovzxwd), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[172][10]},
    {PREFIX_EXT,     0x383318, STROFF(prefix_ext_172), xx, xx, xx, xx, xx, mrm|evex, x, 172},
  }, { /* e_vex ext 18 */
    {OP_pmovzxwq,  0x66383418, STROFF(pmovzxwq), Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxwq, 0x66383418, STROFF(vpmovzxwq), Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[163][10]},
    {PREFIX_EXT, 0x383418, STROFF(prefix_ext_163), xx, xx, xx, xx, xx, mrm|evex, x, 163},
  }, { /* e_vex ext 19 */
    {OP_pmovzxdq,  0x66383518, STROFF(pmovzxdq), Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxdq, 0x66383518, STROFF(vpmovzxdq), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[166][10]},
    {PREFIX_EXT,     0x383518, STROFF(prefix_ext_166), xx, xx, xx, xx, xx, mrm|evex, x, 166},
  }, { /* e_vex ext 20 */
    {OP_pcmpgtq,  0x66383718, STROFF(pcmpgtq),  Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpcmpgtq, 0x66383718, STROFF(vpcmpgtq),  Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[232][2]},
    {EVEX_Wb_EXT,  0x66383748, STROFF(evex_Wb_ext_232), xx, xx, xx, xx, xx, mrm|evex, x, 232},
  }, { /* e_vex ext 21 */
    {OP_pminsb,  0x66383818, STROFF(pminsb),   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminsb, 0x66383818, STROFF(vpminsb),   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tpe[179][10]},
    {PREFIX_EXT,   0x383818, STROFF(prefix_ext_179), xx, xx, xx, xx, xx, mrm|evex, x, 179},
  }, { /* e_vex ext 22 */
    {OP_pminsd,   0x66383918, STROFF(pminsd),   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminsd,  0x66383918, STROFF(vpminsd),   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[113][0]},
    {PREFIX_EXT,    0x383918, STROFF(prefix_ext_181), xx, xx, xx, xx, xx, mrm|evex, x, 181},
  }, { /* e_vex ext 23 */
    {OP_pminuw,   0x66383a18, STROFF(pminuw),   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminuw,  0x66383a18, STROFF(vpminuw),   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tpe[185][10]},
    {PREFIX_EXT,    0x383a18, STROFF(prefix_ext_185), xx, xx, xx, xx, xx, mrm|evex, x, 185},
  }, { /* e_vex ext 24 */
    {OP_pminud,   0x66383b18, STROFF(pminud),   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminud,  0x66383b18, STROFF(vpminud),   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[115][0]},
    {EVEX_Wb_EXT,  0x66383b18, STROFF(evex_Wb_ext_115), xx, xx, xx, xx, xx, mrm|evex, x, 115},
  }, { /* e_vex ext 25 */
    {OP_pmaxsb,   0x66383c18, STROFF(pmaxsb),   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxsb,  0x66383c18, STROFF(vpmaxsb),   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tvex[25][2]},
    {OP_vpmaxsb,  0x66383c08, STROFF(vpmaxsb),   Ve, xx, KEq, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
  }, { /* e_vex ext 26 */
    {OP_pmaxsd,   0x66383d18, STROFF(pmaxsd),   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxsd,  0x66383d18, STROFF(vpmaxsd),   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[114][0]},
    {EVEX_Wb_EXT,  0x66383d18, STROFF(evex_Wb_ext_114), xx, xx, xx, xx, xx, mrm|evex, x, 114},
  }, { /* e_vex ext 27 */
    {OP_pmaxuw,   0x66383e18, STROFF(pmaxuw),   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxuw,  0x66383e18, STROFF(vpmaxuw),   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tvex[27][2]},
    {OP_vpmaxuw,  0x66383e08, STROFF(vpmaxuw),   Ve, xx, KEd, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
  }, { /* e_vex ext 28 */
    {OP_pmaxud,   0x66383f18, STROFF(pmaxud),   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxud,  0x66383f18, STROFF(vpmaxud),   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[116][0]},
    {EVEX_Wb_EXT,  0x66383f18, STROFF(evex_Wb_ext_116), xx, xx, xx, xx, xx, mrm|evex, x, 116},
  }, { /* e_vex ext 29 */
    {OP_pmulld,   0x66384018, STROFF(pmulld),   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmulld,  0x66384018, STROFF(vpmulld),   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[45][0]},
    {EVEX_Wb_EXT, 0x66384018, STROFF(evex_Wb_ext_45), xx, xx, xx, xx, xx, mrm|evex, x, 45},
  }, { /* e_vex ext 30 */
    {OP_phminposuw,  0x66384118,STROFF(phminposuw),Vdq,xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vphminposuw, 0x66384118,STROFF(vphminposuw),Vdq,xx, Wdq, xx, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x66384118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 31 */
    {OP_aesimc,  0x6638db18, STROFF(aesimc),  Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vaesimc, 0x6638db18, STROFF(vaesimc),  Vdq, xx, Wdq, xx, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638db18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 32 */
    {OP_aesenc,  0x6638dc18, STROFF(aesenc),  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vaesenc, 0x6638dc18, STROFF(vaesenc),  Vdq, xx, Hdq,Wdq, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638dc18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 33 */
    {OP_aesenclast,  0x6638dd18,STROFF(aesenclast),Vdq,xx,Wdq,Vdq,xx, mrm|reqp, x, END_LIST},
    {OP_vaesenclast, 0x6638dd18,STROFF(vaesenclast),Vdq,xx,Hdq,Wdq,xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638dd18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 34 */
    {OP_aesdec,  0x6638de18, STROFF(aesdec),  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vaesdec, 0x6638de18, STROFF(vaesdec),  Vdq, xx, Hdq,Wdq, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638de18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 35 */
    {OP_aesdeclast,  0x6638df18,STROFF(aesdeclast),Vdq,xx,Wdq,Vdq,xx, mrm|reqp, x, END_LIST},
    {OP_vaesdeclast, 0x6638df18,STROFF(vaesdeclast),Vdq,xx,Hdq,Wdq,xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638df18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 36 */
    {OP_pextrb,   0x663a1418, STROFF(pextrb), Rd_Mb, xx, Vb_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpextrb,  0x663a1418, STROFF(vpextrb), Rd_Mb, xx, Vb_dq, Ib, xx, mrm|vex|reqp, x, tvex[36][2]},
    {OP_vpextrb,  0x663a1408, STROFF(vpextrb), Rd_Mb, xx, Vb_dq, Ib, xx, mrm|evex|reqp|ttt1s|inopsz1, x, END_LIST},
  }, { /* e_vex ext 37 */
    {OP_pextrw,   0x663a1518, STROFF(pextrw), Rd_Mw, xx, Vw_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpextrw,  0x663a1518, STROFF(vpextrw), Rd_Mw, xx, Vw_dq, Ib, xx, mrm|vex|reqp, x, tpe[54][10]},
    {OP_vpextrw,  0x663a1508, STROFF(vpextrw), Rd_Mw, xx, Vw_dq, Ib, xx, mrm|evex|reqp|ttt1s|inopsz2, x, END_LIST},
  }, { /* e_vex ext 38 */
    {OP_pextrd,   0x663a1618, STROFF(pextrd),  Ey, xx, Vd_q_dq, Ib, xx, mrm|reqp, x, END_LIST},/*"pextrq" with rex.w*/
    {OP_vpextrd,  0x663a1618, STROFF(vpextrd),  Ey, xx, Vd_q_dq, Ib, xx, mrm|vex|reqp, x, tevexwb[145][0]},/*"vpextrq" with rex.w*/
    {EVEX_Wb_EXT, 0x663a1618, STROFF(evex_Wb_ext_145), xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 145},
  }, { /* e_vex ext 39 */
    {OP_extractps,  0x663a1718, STROFF(extractps), Ed, xx, Vd_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vextractps, 0x663a1718, STROFF(vextractps), Ed, xx, Ib, Vd_dq, xx, mrm|vex|reqp, x, tvex[39][2]},
    {OP_vextractps, 0x663a1708, STROFF(vextractps), Ed, xx, Ib, Vd_dq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
  }, { /* e_vex ext 40 */
    {OP_roundps,  0x663a0818, STROFF(roundps),  Vdq, xx, Wdq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundps, 0x663a0818, STROFF(vroundps),  Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0808, STROFF(evex_Wb_ext_246), xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 246},
  }, { /* e_vex ext 41 */
    {OP_roundpd,  0x663a0918, STROFF(roundpd),  Vdq, xx, Wdq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundpd, 0x663a0918, STROFF(vroundpd),  Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0948, STROFF(evex_Wb_ext_218), xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 218},
  }, { /* e_vex ext 42 */
    {OP_roundss,  0x663a0a18, STROFF(roundss),  Vss, xx, Wss, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundss, 0x663a0a18, STROFF(vroundss),  Vdq, xx, H12_dq, Wss, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0a08, STROFF(evex_Wb_ext_253), xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 253},
  }, { /* e_vex ext 43 */
    {OP_roundsd,  0x663a0b18, STROFF(roundsd),  Vsd, xx, Wsd, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundsd, 0x663a0b18, STROFF(vroundsd),  Vdq, xx, Hsd, Wsd, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0b08, STROFF(evex_Wb_ext_254), xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 254},
  }, { /* e_vex ext 44 */
    {OP_blendps,  0x663a0c18, STROFF(blendps),  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vblendps, 0x663a0c18, STROFF(vblendps),  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 45 */
    {OP_blendpd,  0x663a0d18, STROFF(blendpd),  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vblendpd, 0x663a0d18, STROFF(vblendpd),  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 46 */
    {OP_pblendw,  0x663a0e18, STROFF(pblendw),  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vpblendw, 0x663a0e18, STROFF(vpblendw),  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 47 */
    /* FIXME i#1388: pinsrb actually reads only bottom byte of reg */
    {OP_pinsrb,   0x663a2018, STROFF(pinsrb),   Vb_dq, xx, Rd_Mb,  Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpinsrb,  0x663a2018, STROFF(vpinsrb),   Vdq, xx, H15_dq, Rd_Mb, Ib, mrm|vex|reqp, x, tvex[47][2]},
    {OP_vpinsrb,  0x663a2008, STROFF(vpinsrb),   Vdq, xx, H15_dq, Rd_Mb, Ib, mrm|evex|reqp|ttt1s|inopsz1, x, END_LIST},
  }, { /* e_vex ext 48 */
    {OP_insertps, 0x663a2118, STROFF(insertps), Vdq, xx, Udq_Md, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vinsertps,0x663a2118, STROFF(vinsertps), Vdq, xx, Ib, Hdq, Udq_Md, mrm|vex|reqp|reqL0, x, tvex[48][2]},
    {OP_vinsertps,0x663a2108, STROFF(vinsertps), Vdq, xx, Ib, Hdq, Udq_Md, mrm|evex|reqp|reqL0|ttt1s, x, END_LIST},
  }, { /* e_vex ext 49 */
    {OP_pinsrd,   0x663a2218, STROFF(pinsrd),   Vd_q_dq, xx, Ey,Ib, xx, mrm|reqp, x, END_LIST},/*"pinsrq" with rex.w*/
    {OP_vpinsrd,  0x663a2218, STROFF(vpinsrd),   Vdq, xx, H12_8_dq, Ey, Ib, mrm|vex|reqp, x, tevexwb[144][0]},/*"vpinsrq" with rex.w*/
    {EVEX_Wb_EXT, 0x663a2218, STROFF(evex_Wb_ext_144), xx, xx, xx, xx, xx, mrm|evex, x, 144},
  }, { /* e_vex ext 50 */
    {OP_dpps,     0x663a4018, STROFF(dpps),     Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vdpps,    0x663a4018, STROFF(vdpps),     Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 51 */
    {OP_dppd,     0x663a4118, STROFF(dppd),     Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vdppd,    0x663a4118, STROFF(vdppd),     Vdq, xx, Hdq, Wdq, Ib, mrm|vex|reqp|reqL0, x, END_LIST},
    {INVALID, 0x663a4118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 52 */
    {OP_mpsadbw,  0x663a4218, STROFF(mpsadbw),  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vmpsadbw, 0x663a4218, STROFF(vmpsadbw),  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {OP_vdbpsadbw, 0x663a4208, STROFF(vdbpsadbw),  Ve, xx, KEd, Ib, He, xop|mrm|evex|reqp|ttfvm, x, exop[249]},
  }, { /* e_vex ext 53 */
    {OP_pcmpestrm, 0x663a6018, STROFF(pcmpestrm),xmm0, xx, Vdq, Wdq, Ib, mrm|reqp|xop, fW6, exop[8]},
    {OP_vpcmpestrm,0x663a6018, STROFF(vpcmpestrm),xmm0, xx, Vdq, Wdq, Ib, mrm|vex|reqp|xop, fW6, exop[11]},
    {INVALID, 0x663a6018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 54 */
    {OP_pcmpestri, 0x663a6118, STROFF(pcmpestri),ecx, xx, Vdq, Wdq, Ib, mrm|reqp|xop, fW6, exop[9]},
    {OP_vpcmpestri,0x663a6118, STROFF(vpcmpestri),ecx, xx, Vdq, Wdq, Ib, mrm|vex|reqp|xop, fW6, exop[12]},
    {INVALID, 0x663a6118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 55 */
    {OP_pcmpistrm, 0x663a6218, STROFF(pcmpistrm),xmm0, xx, Vdq, Wdq, Ib, mrm|reqp, fW6, END_LIST},
    {OP_vpcmpistrm,0x663a6218, STROFF(vpcmpistrm),xmm0, xx, Vdq, Wdq, Ib, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x663a6218, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 56 */
    {OP_pcmpistri, 0x663a6318, STROFF(pcmpistri),ecx, xx, Vdq, Wdq, Ib, mrm|reqp, fW6, END_LIST},
    {OP_vpcmpistri,0x663a6318, STROFF(vpcmpistri),ecx, xx, Vdq, Wdq, Ib, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x663a6318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 57 */
    {OP_pclmulqdq, 0x663a4418, STROFF(pclmulqdq), Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vpclmulqdq,0x663a4418, STROFF(vpclmulqdq), Vdq, xx, Hdq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 58 */
    {OP_aeskeygenassist, 0x663adf18, STROFF(aeskeygenassist),Vdq,xx,Wdq,Ib,xx,mrm|reqp,x,END_LIST},
    {OP_vaeskeygenassist,0x663adf18, STROFF(vaeskeygenassist),Vdq,xx,Wdq,Ib,xx,mrm|vex|reqp,x,END_LIST},
    {INVALID, 0x663adf18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 59 */
    {INVALID,   0x66380e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtestps, 0x66380e18, STROFF(vtestps), xx, xx, Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x66380e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 60 */
    {INVALID,   0x66380f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtestpd, 0x66380f18, STROFF(vtestpd), xx, xx, Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x66380f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 61 */
    {OP_ldmxcsr, 0x0fae32, STROFF(ldmxcsr), xx, xx, Md, xx, xx, mrm, x, END_LIST},
    {OP_vldmxcsr, 0x0fae32, STROFF(vldmxcsr), xx, xx, Md, xx, xx, mrm|vex|reqL0, x, END_LIST},
    {INVALID, 0x0fae32, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 62 */
    {OP_stmxcsr, 0x0fae33, STROFF(stmxcsr), Md, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_vstmxcsr, 0x0fae33, STROFF(vstmxcsr), Md, xx, xx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x0fae33, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 63 */
    {INVALID,   0x66381318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtph2ps, 0x66381318, STROFF(vcvtph2ps), Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tevexwb[263][0]},
    {PREFIX_EXT,     0x381318, STROFF(prefix_ext_174), xx, xx, xx, xx, xx, mrm|evex, x, 174},
  }, { /* e_vex ext 64 */
    {INVALID,   0x66381818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastss, 0x66381818, STROFF(vbroadcastss), Vx, xx, Wd_dq, xx, xx, mrm|vex|reqp, x, tvex[64][2]},
    {OP_vbroadcastss, 0x66381808, STROFF(vbroadcastss), Ve, xx, KEw, Wd_dq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
  }, { /* e_vex ext 65 */
    {INVALID,   0x66381918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastsd, 0x66381918, STROFF(vbroadcastsd), Vqq, xx, Wq_dq, xx, xx, mrm|vex|reqp|reqL1, x, tevexwb[148][2]},
    {EVEX_Wb_EXT, 0x66381918, STROFF(evex_Wb_ext_148), xx, xx, xx, xx, xx, mrm|evex, x, 148},
  }, { /* e_vex ext 66 */
    {INVALID,   0x66381a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastf128, 0x66381a18, STROFF(vbroadcastf128), Vqq, xx, Mdq, xx, xx, mrm|vex|reqp|reqL1, x, END_LIST},
    {EVEX_Wb_EXT, 0x66381a18, STROFF(evex_Wb_ext_149), xx, xx, xx, xx, xx, mrm|evex, x, 149},
  }, { /* e_vex ext 67 */
    {INVALID,   0x66382c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovps, 0x66382c18, STROFF(vmaskmovps), Vx, xx, Hx,Mx, xx, mrm|vex|reqp|predcx, x, tvex[69][1]},
    {EVEX_Wb_EXT, 0x66382c18, STROFF(evex_Wb_ext_181), xx, xx, xx, xx, xx, mrm|evex, x, 181},
  }, { /* e_vex ext 68 */
    {INVALID,   0x66382d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovpd, 0x66382d18, STROFF(vmaskmovpd), Vx, xx, Hx,Mx, xx, mrm|vex|reqp|predcx, x, tvex[70][1]},
    {EVEX_Wb_EXT, 0x66382d18, STROFF(evex_Wb_ext_182), xx, xx, xx, xx, xx, mrm|evex, x, 182},
  }, { /* e_vex ext 69 */
    {INVALID,   0x66382e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovps, 0x66382e18, STROFF(vmaskmovps), Mx, xx, Hx,Vx, xx, mrm|vex|reqp|predcx, x, END_LIST},
    {INVALID, 0x66382e18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 70 */
    {INVALID,   0x66382f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovpd, 0x66382f18, STROFF(vmaskmovpd), Mx, xx, Hx,Vx, xx, mrm|vex|reqp|predcx, x, END_LIST},
    {INVALID, 0x66382f18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 71 */
    {INVALID,   0x663a0418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilps, 0x663a0418, STROFF(vpermilps), Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, tvex[77][1]},
    {EVEX_Wb_EXT, 0x663a0418, STROFF(evex_Wb_ext_247), xx, xx, xx, xx, xx, mrm|evex, x, 247},
  }, { /* e_vex ext 72 */
    {INVALID,   0x663a0518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x663a0518, STROFF(vpermilpd), Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, tvex[78][1]},
    {EVEX_Wb_EXT, 0x663a0548, STROFF(evex_Wb_ext_230), xx, xx, xx, xx, xx, mrm|evex, x, 230},
  }, { /* e_vex ext 73 */
    {INVALID,   0x663a0618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vperm2f128, 0x663a0618, STROFF(vperm2f128), Vqq, xx, Hqq, Wqq, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 74 */
    {INVALID,   0x663a1818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vinsertf128, 0x663a1818, STROFF(vinsertf128), Vqq, xx, Hqq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a1818, STROFF(evex_Wb_ext_105), xx, xx, xx, xx, xx, mrm|evex, x, 105},
  }, { /* e_vex ext 75 */
    {INVALID,   0x663a1918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vextractf128, 0x663a1918, STROFF(vextractf128), Wdq, xx, Vdq_qq, Ib, xx, mrm|vex|reqp|reqL1, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a1918, STROFF(evex_Wb_ext_101), xx, xx, xx, xx, xx, mrm|evex, x, 101},
  }, { /* e_vex ext 76 */
    {INVALID,   0x663a1d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtps2ph, 0x663a1d18, STROFF(vcvtps2ph), Wh_x, xx, Vx, Ib, xx, mrm|vex|reqp, x, tevexwb[264][0]},
    {EVEX_Wb_EXT,   0x663a1d08, STROFF(evex_Wb_ext_264), xx, xx, xx, xx, xx, mrm|evex, x, 264},
  }, { /* e_vex ext 77 */
    {INVALID,   0x66380c18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilps, 0x66380c18, STROFF(vpermilps), Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[247][0]},
    {EVEX_Wb_EXT, 0x66380c08, STROFF(evex_Wb_ext_248), xx, xx, xx, xx, xx, mrm|evex, x, 248},
  }, { /* e_vex ext 78 */
    {INVALID,   0x66380d18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x66380d18, STROFF(vpermilpd), Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[230][2]},
    {EVEX_Wb_EXT, 0x66380d48, STROFF(evex_Wb_ext_231), xx, xx, xx, xx, xx, mrm|evex, x, 231},
  }, { /* e_vex ext 79 */
    {OP_seto,    0x0f9010,             STROFF(seto), Eb, xx, xx, xx, xx, mrm, fRO, END_LIST},
    {PREFIX_EXT, 0x0f9010, STROFF(prefix_ext_144), xx, xx, xx, xx, xx, mrm,   x, 144},
    {INVALID, 0x0f9010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 80 */
    {OP_setno,   0x0f9110,            STROFF(setno), Eb, xx, xx, xx, xx, mrm, fRO, END_LIST},
    {PREFIX_EXT, 0x0f9110, STROFF(prefix_ext_145), xx, xx, xx, xx, xx, mrm,   x, 145},
    {INVALID, 0x0f9110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 81 */
    {OP_setb,    0x0f9210,             STROFF(setb), Eb, xx, xx, xx, xx, mrm, fRC, END_LIST},
    {PREFIX_EXT, 0x0f9210, STROFF(prefix_ext_146), xx, xx, xx, xx, xx, mrm,   x, 146},
    {INVALID, 0x0f9210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 82 */
    {OP_setnb,   0x0f9310,            STROFF(setnb), Eb, xx, xx, xx, xx, mrm, fRC, END_LIST},
    {PREFIX_EXT, 0x0f9310, STROFF(prefix_ext_147), xx, xx, xx, xx, xx, mrm,   x, 147},
    {INVALID, 0x0f9310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 83 */
    {OP_cmovno,  0x0f4110,           STROFF(cmovno), Gv, xx, Ev, xx, xx, mrm|predcc, fRO, END_LIST},
    {PREFIX_EXT, 0x0f4110, STROFF(prefix_ext_148), xx, xx, xx, xx, xx, mrm,         x, 148},
    {INVALID, 0x0f4110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 84 */
    {OP_cmovb,   0x0f4210,            STROFF(cmovb), Gv, xx, Ev, xx, xx, mrm|predcc, fRC, END_LIST},
    {PREFIX_EXT, 0x0f4210, STROFF(prefix_ext_149), xx, xx, xx, xx, xx, mrm,          x, 149},
    {INVALID, 0x0f4210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 85 */
    {OP_cmovnp,  0x0f4b10,           STROFF(cmovnp), Gv, xx, Ev, xx, xx, mrm|predcc, fRP, END_LIST},
    {PREFIX_EXT, 0x0f4b10, STROFF(prefix_ext_150), xx, xx, xx, xx, xx, mrm,          x, 150},
    {INVALID, 0x0f4b10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 86 */
    {OP_cmovz,   0x0f4410,            STROFF(cmovz), Gv, xx, Ev, xx, xx, mrm|predcc, fRZ, END_LIST},
    {PREFIX_EXT, 0x0f4410, STROFF(prefix_ext_151), xx, xx, xx, xx, xx, mrm,          x, 151},
    {INVALID, 0x0f4410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 87 */
    {OP_cmovnz,  0x0f4510,           STROFF(cmovnz), Gv, xx, Ev, xx, xx, mrm|predcc, fRZ, END_LIST},
    {PREFIX_EXT, 0x0f4510, STROFF(prefix_ext_152), xx, xx, xx, xx, xx, mrm,          x, 152},
    {INVALID, 0x0f4510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 88 */
    {OP_cmovbe,  0x0f4610,           STROFF(cmovbe), Gv, xx, Ev, xx, xx, mrm|predcc, (fRC|fRZ), END_LIST},
    {PREFIX_EXT, 0x0f4610, STROFF(prefix_ext_153), xx, xx, xx, xx, xx, mrm,                x, 153},
    {INVALID, 0x0f4610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 89 */
    {OP_cmovnbe, 0x0f4710,          STROFF(cmovnbe), Gv, xx, Ev, xx, xx, mrm|predcc, (fRC|fRZ), END_LIST},
    {PREFIX_EXT, 0x0f4710, STROFF(prefix_ext_154), xx, xx, xx, xx, xx, mrm,                x, 154},
    {INVALID, 0x0f4710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 90 */
    {OP_cmovp,   0x0f4a10,            STROFF(cmovp), Gv, xx, Ev, xx, xx, mrm|predcc, fRP, END_LIST},
    {PREFIX_EXT, 0x0f4a10, STROFF(prefix_ext_155), xx, xx, xx, xx, xx, mrm,          x, 155},
    {INVALID, 0x0f4a10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 91 */
    {OP_sets,    0x0f9810,             STROFF(sets), Eb, xx, xx, xx, xx, mrm, fRS, END_LIST},
    {PREFIX_EXT, 0x0f9810, STROFF(prefix_ext_156), xx, xx, xx, xx, xx, mrm,    x, 156},
    {INVALID, 0x0f9810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 92 */
    {OP_setns,   0x0f9910,            STROFF(setns), Eb, xx, xx, xx, xx, mrm, fRS, END_LIST},
    {PREFIX_EXT, 0x0f9910, STROFF(prefix_ext_157), xx, xx, xx, xx, xx, mrm,   x, 157},
    {INVALID, 0x0f9910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 93 */
    {INVALID,      0x66389810,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389818,   STROFF(vex_W_ext_0), xx, xx, xx, xx, xx, mrm|vex, x, 0},
    {EVEX_Wb_EXT,   0x66389818, STROFF(evex_Wb_ext_62), xx, xx, xx, xx, xx, mrm|evex, x, 62},
  }, { /* e_vex ext 94 */
    {INVALID,      0x6638a810,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a818,   STROFF(vex_W_ext_1), xx, xx, xx, xx, xx, mrm|vex, x, 1},
    {EVEX_Wb_EXT,   0x6638a818, STROFF(evex_Wb_ext_63), xx, xx, xx, xx, xx, mrm|evex, x, 63},
  }, { /* e_vex ext 95 */
    {INVALID,      0x6638b810,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b818,   STROFF(vex_W_ext_2), xx, xx, xx, xx, xx, mrm|vex, x, 2},
    {EVEX_Wb_EXT,   0x6638b818, STROFF(evex_Wb_ext_64), xx, xx, xx, xx, xx, mrm|evex, x, 64},
  }, { /* e_vex ext 96 */
    {INVALID,      0x66389910,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389918,   STROFF(vex_W_ext_3), xx, xx, xx, xx, xx, mrm|vex, x, 3},
    {EVEX_Wb_EXT,   0x66389918, STROFF(evex_Wb_ext_65), xx, xx, xx, xx, xx, mrm|evex, x, 65},
  }, { /* e_vex ext 97 */
    {INVALID,      0x6638a910,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a918,   STROFF(vex_W_ext_4), xx, xx, xx, xx, xx, mrm|vex, x, 4},
    {EVEX_Wb_EXT,   0x6638a918, STROFF(evex_Wb_ext_66), xx, xx, xx, xx, xx, mrm|evex, x, 66},
  }, { /* e_vex ext 98 */
    {INVALID,      0x6638b910,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b918,   STROFF(vex_W_ext_5), xx, xx, xx, xx, xx, mrm|vex, x, 5},
    {EVEX_Wb_EXT,   0x6638b918, STROFF(evex_Wb_ext_67), xx, xx, xx, xx, xx, mrm|evex, x, 67},
  }, { /* e_vex ext 99 */
    {INVALID,      0x66389610,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389618,   STROFF(vex_W_ext_6), xx, xx, xx, xx, xx, mrm|vex, x, 6},
    {EVEX_Wb_EXT,   0x66389618, STROFF(evex_Wb_ext_68), xx, xx, xx, xx, xx, mrm|evex, x, 68},
  }, { /* e_vex ext 100 */
    {INVALID,      0x6638a610,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a618,   STROFF(vex_W_ext_7), xx, xx, xx, xx, xx, mrm|vex, x, 7},
    {EVEX_Wb_EXT,   0x6638a618, STROFF(evex_Wb_ext_69), xx, xx, xx, xx, xx, mrm|evex, x, 69},
  }, { /* e_vex ext 101 */
    {INVALID,      0x6638b610,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b618,   STROFF(vex_W_ext_8), xx, xx, xx, xx, xx, mrm|vex, x, 8},
    {EVEX_Wb_EXT,   0x6638b618, STROFF(evex_Wb_ext_70), xx, xx, xx, xx, xx, mrm|evex, x, 70},
  }, { /* e_vex ext 102 */
    {INVALID,      0x66389710,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389718,   STROFF(vex_W_ext_9), xx, xx, xx, xx, xx, mrm|vex, x, 9},
    {EVEX_Wb_EXT,   0x66389718, STROFF(evex_Wb_ext_71), xx, xx, xx, xx, xx, mrm|evex, x, 71},
  }, { /* e_vex ext 103 */
    {INVALID,      0x6638a710,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a718,  STROFF(vex_W_ext_10), xx, xx, xx, xx, xx, mrm|vex, x, 10},
    {EVEX_Wb_EXT,   0x6638a718, STROFF(evex_Wb_ext_72), xx, xx, xx, xx, xx, mrm|evex, x, 72},
  }, { /* e_vex ext 104 */
    {INVALID,      0x6638b710,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b718,  STROFF(vex_W_ext_11), xx, xx, xx, xx, xx, mrm|vex, x, 11},
    {EVEX_Wb_EXT,   0x6638b718, STROFF(evex_Wb_ext_73), xx, xx, xx, xx, xx, mrm|evex, x, 73},
  }, { /* e_vex ext 105 */
    {INVALID,      0x66389a10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389a18,  STROFF(vex_W_ext_12), xx, xx, xx, xx, xx, mrm|vex, x, 12},
    {EVEX_Wb_EXT,   0x66389a18, STROFF(evex_Wb_ext_74), xx, xx, xx, xx, xx, mrm|evex, x, 74},
  }, { /* e_vex ext 106 */
    {INVALID,      0x6638aa10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638aa18,  STROFF(vex_W_ext_13), xx, xx, xx, xx, xx, mrm|vex, x, 13},
    {EVEX_Wb_EXT,   0x6638aa18, STROFF(evex_Wb_ext_75), xx, xx, xx, xx, xx, mrm|evex, x, 75},
  }, { /* e_vex ext 107 */
    {INVALID,      0x6638ba10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ba18,  STROFF(vex_W_ext_14), xx, xx, xx, xx, xx, mrm|vex, x, 14},
    {EVEX_Wb_EXT,   0x6638ba18, STROFF(evex_Wb_ext_76), xx, xx, xx, xx, xx, mrm|evex, x, 76},
  }, { /* e_vex ext 108 */
    {INVALID,      0x66389b10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389b18,  STROFF(vex_W_ext_15), xx, xx, xx, xx, xx, mrm|vex, x, 15},
    {EVEX_Wb_EXT,   0x66389b18, STROFF(evex_Wb_ext_77), xx, xx, xx, xx, xx, mrm|evex, x, 77},
  }, { /* e_vex ext 109 */
    {INVALID,      0x6638ab10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ab18,  STROFF(vex_W_ext_16), xx, xx, xx, xx, xx, mrm|vex, x, 16},
    {EVEX_Wb_EXT,   0x6638ab18, STROFF(evex_Wb_ext_78), xx, xx, xx, xx, xx, mrm|evex, x, 78},
  }, { /* e_vex ext 110 */
    {INVALID,      0x6638bb10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bb18,  STROFF(vex_W_ext_17), xx, xx, xx, xx, xx, mrm|vex, x, 17},
    {EVEX_Wb_EXT,   0x6638bb18, STROFF(evex_Wb_ext_79), xx, xx, xx, xx, xx, mrm|evex, x, 79},
  }, { /* e_vex ext 111 */
    {INVALID,      0x66389c10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389c18,  STROFF(vex_W_ext_18), xx, xx, xx, xx, xx, mrm|vex, x, 18},
    {EVEX_Wb_EXT,   0x66389c18, STROFF(evex_Wb_ext_80), xx, xx, xx, xx, xx, mrm|evex, x, 80},
  }, { /* e_vex ext 112 */
    {INVALID,      0x6638ac10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ac18,  STROFF(vex_W_ext_19), xx, xx, xx, xx, xx, mrm|vex, x, 19},
    {EVEX_Wb_EXT,   0x6638ac18, STROFF(evex_Wb_ext_81), xx, xx, xx, xx, xx, mrm|evex, x, 81},
  }, { /* e_vex ext 113 */
    {INVALID,      0x6638bc10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bc18,  STROFF(vex_W_ext_20), xx, xx, xx, xx, xx, mrm|vex, x, 20},
    {EVEX_Wb_EXT,   0x6638bc18, STROFF(evex_Wb_ext_82), xx, xx, xx, xx, xx, mrm|evex, x, 82},
  }, { /* e_vex ext 114 */
    {INVALID,      0x66389d10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389d18,  STROFF(vex_W_ext_21), xx, xx, xx, xx, xx, mrm|vex, x, 21},
    {EVEX_Wb_EXT,   0x66389d18, STROFF(evex_Wb_ext_83), xx, xx, xx, xx, xx, mrm|evex, x, 83},
  }, { /* e_vex ext 115 */
    {INVALID,      0x6638ad10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ad18,  STROFF(vex_W_ext_22), xx, xx, xx, xx, xx, mrm|vex, x, 22},
    {EVEX_Wb_EXT,   0x6638ad18, STROFF(evex_Wb_ext_84), xx, xx, xx, xx, xx, mrm|evex, x, 84},
  }, { /* e_vex ext 116 */
    {INVALID,      0x6638bd10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bd18,  STROFF(vex_W_ext_23), xx, xx, xx, xx, xx, mrm|vex, x, 23},
    {EVEX_Wb_EXT,   0x6638bd18, STROFF(evex_Wb_ext_85), xx, xx, xx, xx, xx, mrm|evex, x, 85},
  }, { /* e_vex ext 117 */
    {INVALID,      0x66389e10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389e18,  STROFF(vex_W_ext_24), xx, xx, xx, xx, xx, mrm|vex, x, 24},
    {EVEX_Wb_EXT,   0x66389e18, STROFF(evex_Wb_ext_86), xx, xx, xx, xx, xx, mrm|evex, x, 86},
  }, { /* e_vex ext 118 */
    {INVALID,      0x6638ae10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ae18,  STROFF(vex_W_ext_25), xx, xx, xx, xx, xx, mrm|vex, x, 25},
    {EVEX_Wb_EXT,   0x6638ae18, STROFF(evex_Wb_ext_87), xx, xx, xx, xx, xx, mrm|evex, x, 87},
  }, { /* e_vex ext 119 */
    {INVALID,      0x6638be10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638be18,  STROFF(vex_W_ext_26), xx, xx, xx, xx, xx, mrm|vex, x, 26},
    {EVEX_Wb_EXT,   0x6638be18, STROFF(evex_Wb_ext_88), xx, xx, xx, xx, xx, mrm|evex, x, 88},
  }, { /* e_vex ext 120 */
    {INVALID,      0x66389f10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389f18,  STROFF(vex_W_ext_27), xx, xx, xx, xx, xx, mrm|vex, x, 27},
    {EVEX_Wb_EXT,   0x66389f18, STROFF(evex_Wb_ext_89), xx, xx, xx, xx, xx, mrm|evex, x, 89},
  }, { /* e_vex ext 121 */
    {INVALID,      0x6638af10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638af18,  STROFF(vex_W_ext_28), xx, xx, xx, xx, xx, mrm|vex, x, 28},
    {EVEX_Wb_EXT,   0x6638af18, STROFF(evex_Wb_ext_90), xx, xx, xx, xx, xx, mrm|evex, x, 90},
  }, { /* e_vex ext 122 */
    {INVALID,      0x6638bf10,           STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bf18,  STROFF(vex_W_ext_29), xx, xx, xx, xx, xx, mrm|vex, x, 29},
    {EVEX_Wb_EXT,   0x6638bf18, STROFF(evex_Wb_ext_91), xx, xx, xx, xx, xx, mrm|evex, x, 91},
  }, { /* e_vex ext 123 */
    {INVALID, 0x66383610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermd, 0x66383618, STROFF(vpermd), Vqq, xx, Hqq, Wqq, xx, mrm|vex|reqp, x, tevexwb[93][0]},
    {EVEX_Wb_EXT, 0x66383618, STROFF(evex_Wb_ext_93), xx, xx, xx, xx, xx, mrm|evex, x, 93},
  }, { /* e_vex ext 124 */
    {INVALID, 0x66381610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermps, 0x66381618, STROFF(vpermps), Vqq, xx, Hqq, Wqq, xx, mrm|vex|reqp, x, tevexwb[94][0]},
    {EVEX_Wb_EXT, 0x66381618, STROFF(evex_Wb_ext_94), xx, xx, xx, xx, xx, mrm|evex, x, 94 },
  }, { /* e_vex ext 125 */
    {INVALID, 0x663a0010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermq, 0x663a0058, STROFF(vpermq), Vqq, xx, Wqq, Ib, xx, mrm|vex|reqp, x, tevexwb[251][2]},
    {EVEX_Wb_EXT, 0x663a0048, STROFF(evex_Wb_ext_251), xx, xx, xx, xx, xx, mrm|evex, x, 251 },
  }, { /* e_vex ext 126 */
    {INVALID, 0x663a0010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermpd, 0x663a0158, STROFF(vpermpd), Vqq, xx, Wqq, Ib, xx, mrm|vex|reqp, x, tevexwb[252][2]},
    {EVEX_Wb_EXT, 0x663a0148, STROFF(evex_Wb_ext_252), xx, xx, xx, xx, xx, mrm|evex, x, 252 },
  }, { /* e_vex ext 127 */
    {INVALID, 0x663a3918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vextracti128,0x663a3918, STROFF(vextracti128), Wdq, xx, Vqq, Ib, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a3918, STROFF(evex_Wb_ext_103), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 103},
  }, { /* e_vex ext 128 */
    {INVALID, 0x663a3818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vinserti128, 0x663a3818, STROFF(vinserti128), Vqq, xx, Hqq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a3818, STROFF(evex_Wb_ext_107), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 107},
  }, { /* e_vex ext 129 */
    {OP_blendvpd, 0x66381518, STROFF(blendvpd), Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp, x, END_LIST},
    {INVALID,     0x66381518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT,    0x381518, STROFF(prefix_ext_168), xx, xx, xx, xx, xx, mrm|evex, x, 168},
  }, { /* e_vex ext 130 */
    {OP_blendvps, 0x66381418, STROFF(blendvps), Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp, x, END_LIST},
    {INVALID, 0x66381418, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x381418, STROFF(prefix_ext_165), xx, xx, xx, xx, xx, mrm, x, 165},
  }, { /* e_vex ext 131 */
    {INVALID, 0x66384618, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsravd, 0x66384618, STROFF(vpsravd), Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[128][0]},
    {EVEX_Wb_EXT, 0x66384618, STROFF(evex_Wb_ext_128), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 128},
  }, { /* e_vex ext 132 */
    {OP_pblendvb, 0x66381018, STROFF(pblendvb), Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp,x, END_LIST},
    {INVALID, 0x66381018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x381018, STROFF(prefix_ext_177), xx, xx, xx, xx, xx, mrm|evex, x, 177},
  }, { /* e_vex ext 133 */
    {INVALID, 0x66384518, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66384518, STROFF(vex_W_ext_72), xx, xx, xx, xx, xx, mrm|vex|reqp, x, 72},
    {EVEX_Wb_EXT, 0x66384518, STROFF(evex_Wb_ext_129), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 129},
  }, { /* e_vex ext 134 */
    {INVALID, 0x66384718, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66384718, STROFF(vex_W_ext_73), xx, xx, xx, xx, xx, mrm|vex|reqp, x, 73},
    {EVEX_Wb_EXT, 0x66384718, STROFF(evex_Wb_ext_131), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 131},
  }, { /* e_vex ext 135 */
    {INVALID, 0x66387818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastb, 0x66387818, STROFF(vpbroadcastb), Vx, xx, Wb_dq, xx, xx, mrm|vex|reqp, x, tvex[135][2]},
    {OP_vpbroadcastb, 0x66387808, STROFF(vpbroadcastb), Ve, xx, KEq, Wb_dq, xx, mrm|evex|reqp|ttt1s|inopsz1, x, t38[135]},
  }, { /* e_vex ext 136 */
    {INVALID, 0x66387918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastw, 0x66387918, STROFF(vpbroadcastw), Vx, xx, Ww_dq, xx, xx, mrm|vex|reqp, x, tvex[136][2]},
    {OP_vpbroadcastw, 0x66387908, STROFF(vpbroadcastw), Ve, xx, KEd, Ww_dq, xx, mrm|evex|reqp|ttt1s|inopsz2, x, t38[136]},
  }, { /* e_vex ext 137 */
    {INVALID, 0x66385818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastd, 0x66385818, STROFF(vpbroadcastd), Vx, xx, Wd_dq, xx, xx, mrm|vex|reqp, x, tvex[137][2]},
    {OP_vpbroadcastd, 0x66385808, STROFF(vpbroadcastd), Ve, xx, KEw, Wd_dq, xx, mrm|evex|reqp|ttt1s, x, tevexwb[151][0]},
  }, { /* e_vex ext 138 */
    {INVALID, 0x66385918, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastq, 0x66385918, STROFF(vpbroadcastq), Vx, xx, Wq_dq, xx, xx, mrm|vex|reqp, x, tevexwb[152][2]},
    {EVEX_Wb_EXT, 0x66385918, STROFF(evex_Wb_ext_152), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 152},
  }, { /* e_vex ext 139 */
    {INVALID, 0x66385a18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcasti128, 0x66385a18, STROFF(vbroadcasti128), Vqq, xx, Mdq, xx, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x66385a18, STROFF(evex_Wb_ext_153), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 153},
  }, { /* e_vex ext 140 */
    {INVALID, 0x389018, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389018, STROFF(vex_W_ext_66), xx, xx, xx, xx, xx, mrm|vex, x, 66},
    {EVEX_Wb_EXT, 0x66389018, STROFF(evex_Wb_ext_189), xx, xx, xx, xx, xx, mrm|evex, x, 189},
  }, { /* e_vex ext 141 */
    {INVALID, 0x389118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389118, STROFF(vex_W_ext_67), xx, xx, xx, xx, xx, mrm|vex, x, 67},
    {EVEX_Wb_EXT, 0x66389118, STROFF(evex_Wb_ext_190), xx, xx, xx, xx, xx, mrm|evex, x, 190},
  }, { /* e_vex ext 142 */
    {INVALID, 0x389118, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389218, STROFF(vex_W_ext_68), xx, xx, xx, xx, xx, mrm|vex, x, 68},
    {EVEX_Wb_EXT, 0x66389218, STROFF(evex_Wb_ext_191), xx, xx, xx, xx, xx, mrm|evex, x, 191},
  }, { /* e_vex ext 143 */
    {INVALID, 0x389318, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389318, STROFF(vex_W_ext_69), xx, xx, xx, xx, xx, mrm|vex, x, 69},
    {EVEX_Wb_EXT, 0x66389318, STROFF(evex_Wb_ext_192), xx, xx, xx, xx, xx, mrm|evex, x, 192},
  }, { /* e_vex ext 144 */
    {OP_sha1msg2, 0x38ca18, STROFF(sha1msg2), Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38ca18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638ca18, STROFF(evex_Wb_ext_134), xx, xx, xx, xx, xx, mrm|reqp, x, 134},
  }, { /* e_vex ext 145 */
    {OP_sha1nexte, 0x38c818, STROFF(sha1nexte), Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38c818, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c818, STROFF(evex_Wb_ext_185), xx, xx, xx, xx, xx, mrm|reqp, x, 185},
  }, { /* e_vex ext 146 */
    {OP_sha256rnds2, 0x38cb18, STROFF(sha256rnds2), Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp, x, END_LIST},
    {INVALID, 0x38cb18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638cb18, STROFF(evex_Wb_ext_135), xx, xx, xx, xx, xx, mrm|reqp, x, 135},
  }, { /* e_vex ext 147 */
    {OP_sha256msg1, 0x38cc18, STROFF(sha256msg1), Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38cc18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638cc18, STROFF(evex_Wb_ext_179), xx, xx, xx, xx, xx, mrm|reqp, x, 179},
  }, { /* e_vex ext 148 */
    {OP_sha256msg2, 0x38cd18, STROFF(sha256msg2), Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38cd18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638cd18, STROFF(evex_Wb_ext_180), xx, xx, xx, xx, xx, mrm|reqp, x, 180},
  }, { /* e_vex ext 149 */
    {INVALID, 0x385008, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385008, STROFF(vex_W_ext_110), xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 110},
    {EVEX_Wb_EXT, 0x385008, STROFF(evex_Wb_ext_267), xx, xx, xx, xx, xx, mrm|reqp, x, 267},
  }, { /* e_vex ext 150 */
    {INVALID, 0x385108, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385108, STROFF(vex_W_ext_111), xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 111},
    {EVEX_Wb_EXT, 0x385108, STROFF(evex_Wb_ext_268), xx, xx, xx, xx, xx, mrm|reqp, x, 268},
  }, { /* e_vex ext 151 */
    {INVALID, 0x385208, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385208, STROFF(vex_W_ext_112), xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 112},
    {EVEX_Wb_EXT, 0x385208, STROFF(evex_Wb_ext_269), xx, xx, xx, xx, xx, mrm|reqp, x, 269},
  }, { /* e_vex ext 152 */
    {INVALID, 0x385308, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385308, STROFF(vex_W_ext_113), xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 113},
    {EVEX_Wb_EXT, 0x385308, STROFF(evex_Wb_ext_270), xx, xx, xx, xx, xx, mrm|reqp, x, 270},
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
    {OP_sgdt, 0x0f0130, STROFF(sgdt), Ms, xx, xx, xx, xx, mrm, x, END_LIST},
    {RM_EXT,  0x0f0171, STROFF(group_7_mod_rm_ext_0), xx, xx, xx, xx, xx, mrm, x, 0},
  },
  { /* mod extension 1 */
    {OP_sidt, 0x0f0131, STROFF(sidt),  Ms, xx, xx, xx, xx, mrm, x, END_LIST},
    {RM_EXT,  0x0f0171, STROFF(group_7_mod_rm_ext_1), xx, xx, xx, xx, xx, mrm, x, 1},
  },
  { /* mod extension 2 */
    {OP_invlpg, 0x0f0137, STROFF(invlpg), xx, xx, Mm, xx, xx, mrm, x, END_LIST},
    {RM_EXT,    0x0f0177, STROFF(group_7_mod_rm_ext_2), xx, xx, xx, xx, xx, mrm, x, 2},
  },
  { /* mod extension 3 */
    {OP_clflush, 0x0fae37, STROFF(clflush), xx, xx, Mb, xx, xx, mrm, x, END_LIST},
    {OP_sfence,  0xf80fae77, STROFF(sfence),  xx, xx, xx, xx, xx, mrm, x, END_LIST},
  },
  { /* mod extension 4 */
    {OP_lidt,   0x0f0133, STROFF(lidt),  xx, xx, Ms, xx, xx, mrm, x, END_LIST},
    {RM_EXT,    0x0f0173, STROFF(group_7_mod_rm_ext_3), xx, xx, xx, xx, xx, mrm, x, 3},
  },
  { /* mod extension 5 */
    {OP_lgdt,   0x0f0132, STROFF(lgdt),  xx, xx, Ms, xx, xx, mrm, x, END_LIST},
    {RM_EXT,    0x0f0172, STROFF(group_7_mod_rm_ext_4), xx, xx, xx, xx, xx, mrm, x, 4},
  },
  { /* mod extension 6 */
    {REX_W_EXT, 0x0fae35, STROFF(rexw_ext_3), xx, xx, xx, xx, xx, mrm, x, 3},
    /* note that gdb thinks e9-ef are "lfence (bad)" (PR 239920) */
    {OP_lfence, 0xe80fae75, STROFF(lfence), xx, xx, xx, xx, xx, mrm, x, END_LIST},
  },
  { /* mod extension 7 */
    {REX_W_EXT,   0x0fae36, STROFF(rexw_ext_4), xx, xx, xx, xx, xx, mrm, x, 4},
    {OP_mfence,   0xf00fae76, STROFF(mfence), xx, xx, xx, xx, xx, mrm, x, END_LIST},
  },
  { /* mod extension 8 */
    {OP_vmovss,  0xf30f1010, STROFF(vmovss),  Vdq, xx, Wss,  xx, xx, mrm|vex, x, modx[10][0]},
    {OP_vmovss,  0xf30f1010, STROFF(vmovss),  Vdq, xx, H12_dq, Uss, xx, mrm|vex, x, modx[10][1]},
  },
  { /* mod extension 9 */
    {OP_vmovsd,  0xf20f1010, STROFF(vmovsd),  Vdq, xx, Wsd,  xx, xx, mrm|vex, x, modx[11][0]},
    {OP_vmovsd,  0xf20f1010, STROFF(vmovsd),  Vdq, xx, Hsd, Usd, xx, mrm|vex, x, modx[11][1]},
  },
  { /* mod extension 10 */
    {OP_vmovss,  0xf30f1110, STROFF(vmovss),  Wss, xx, Vss,  xx, xx, mrm|vex, x, modx[ 8][1]},
    {OP_vmovss,  0xf30f1110, STROFF(vmovss),  Udq, xx, H12_dq, Vss, xx, mrm|vex, x, modx[20][0]},
  },
  { /* mod extension 11 */
    {OP_vmovsd,  0xf20f1110, STROFF(vmovsd),  Wsd, xx, Vsd,  xx, xx, mrm|vex, x, modx[ 9][1]},
    {OP_vmovsd,  0xf20f1110, STROFF(vmovsd),  Udq, xx, Hsd, Vsd, xx, mrm|vex, x, modx[21][0]},
  },
  { /* mod extension 12 */
    {PREFIX_EXT, 0x0fc736, STROFF(prefix_ext_137), xx, xx, xx, xx, xx, no, x, 137},
    {OP_rdrand,  0x0fc736, STROFF(rdrand), Rv, xx, xx, xx, xx, mrm, fW6, END_LIST},
  },
  { /* mod extension 13 */
    /* The latest Intel table implies 0x66 prefix makes invalid instr but not worth
     * explicitly encoding that until we have more information.
     */
    {OP_vmptrst, 0x0fc737, STROFF(vmptrst), Mq, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_rdseed,  0x0fc737, STROFF(rdseed), Rv, xx, xx, xx, xx, mrm, fW6, END_LIST},
  },
  { /* mod extension 14 */
    {REX_W_EXT,  0x0fae30, STROFF(rexw_ext_0), xx, xx, xx, xx, xx, mrm, x, 0},
    /* Using reqp to avoid having to create a whole prefix_ext entry for one opcode.
     * Ditto below.
     */
    {OP_rdfsbase,0xf30fae30, STROFF(rdfsbase), Ry, xx, xx, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 15 */
    {REX_W_EXT,  0x0fae31, STROFF(rexw_ext_1), xx, xx, xx, xx, xx, mrm, x, 1},
    {OP_rdgsbase,0xf30fae31, STROFF(rdgsbase), Ry, xx, xx, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 16 */
    {E_VEX_EXT,    0x0fae32, STROFF(e_vex_ext_61), xx, xx, xx, xx, xx, mrm, x, 61},
    {OP_wrfsbase,0xf30fae32, STROFF(wrfsbase), xx, xx, Ry, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 17 */
    {E_VEX_EXT,    0x0fae33, STROFF(e_vex_ext_62), xx, xx, xx, xx, xx, mrm, x, 62},
    {OP_wrgsbase,0xf30fae33, STROFF(wrgsbase), xx, xx, Ry, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 18 */
    /* load from memory zeroes top bits */
    {OP_movss,  0xf30f1010, STROFF(movss),  Vdq, xx, Mss, xx, xx, mrm, x, modx[18][1]},
    {OP_movss,  0xf30f1010, STROFF(movss),  Vss, xx, Uss, xx, xx, mrm, x, tpe[1][1]},
  },
  { /* mod extension 19 */
    /* load from memory zeroes top bits */
    {OP_movsd,  0xf20f1010, STROFF(movsd),  Vdq, xx, Msd, xx, xx, mrm, x, modx[19][1]},
    {OP_movsd,  0xf20f1010, STROFF(movsd),  Vsd, xx, Usd, xx, xx, mrm, x, tpe[1][3]},
  },
  { /* mod extension 20 */
    {OP_vmovss,  0xf30f1000, STROFF(vmovss),  Vdq, xx, KE1b, Wss,  xx, mrm|evex|ttt1s, x, modx[22][0]},
    {OP_vmovss,  0xf30f1000, STROFF(vmovss),  Vdq, xx, KE1b, H12_dq, Uss, mrm|evex|ttnone, x, modx[22][1]},
  },
  { /* mod extension 21 */
    {OP_vmovsd,  0xf20f1040, STROFF(vmovsd),  Vdq, xx, KE1b, Wsd,  xx, mrm|evex|ttt1s, x, modx[23][0]},
    {OP_vmovsd,  0xf20f1040, STROFF(vmovsd),  Vdq, xx, KE1b, Hsd, Usd, mrm|evex|ttnone, x, modx[23][1]},
  },
  { /* mod extension 22 */
    {OP_vmovss,  0xf30f1100, STROFF(vmovss),  Wss, xx, KE1b, Vss,  xx, mrm|evex|ttt1s, x, modx[20][1]},
    {OP_vmovss,  0xf30f1100, STROFF(vmovss),  Udq, xx, KE1b, H12_dq, Vss, mrm|evex|ttnone, x, END_LIST},
  },
  { /* mod extension 23 */
    {OP_vmovsd,  0xf20f1140, STROFF(vmovsd),  Wsd, xx, KE1b, Vsd,  xx, mrm|evex|ttt1s, x, modx[21][1]},
    {OP_vmovsd,  0xf20f1140, STROFF(vmovsd),  Udq, xx, KE1b, Hsd, Vsd, mrm|evex|ttnone, x, END_LIST},
  },
  { /* mod extension 24 */
    {OP_vcvtps2qq, 0x660f7b10, STROFF(vcvtps2qq), Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[24][1]},
    {OP_vcvtps2qq, 0x660f7b10, STROFF(vcvtps2qq), Voq, xx, KEb, Uqq, xx, mrm|evex|er|tthv, x, END_LIST},
  },
  { /* mod extension 25 */
    {OP_vcvtpd2qq, 0x660f7b50, STROFF(vcvtpd2qq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[25][1]},
    {OP_vcvtpd2qq, 0x660f7b50, STROFF(vcvtpd2qq), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 26 */
    {OP_vcvtps2udq, 0x0f7910, STROFF(vcvtps2udq), Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[26][1]},
    {OP_vcvtps2udq, 0x0f7910, STROFF(vcvtps2udq), Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 27 */
    {OP_vcvtpd2udq, 0x0f7950, STROFF(vcvtpd2udq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[27][1]},
    {OP_vcvtpd2udq, 0x0f7950, STROFF(vcvtpd2udq), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 28 */
    {OP_vcvtps2uqq, 0x660f7910, STROFF(vcvtps2uqq), Ve, xx, KEw, Md, xx, mrm|evex|tthv, x, modx[28][1]},
    {OP_vcvtps2uqq, 0x660f7910, STROFF(vcvtps2uqq), Voq, xx, KEw, Uqq, xx, mrm|evex|er|tthv, x, END_LIST},
  },
  { /* mod extension 29 */
    {OP_vcvtpd2uqq, 0x660f7950, STROFF(vcvtpd2uqq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[29][1]},
    {OP_vcvtpd2uqq, 0x660f7950, STROFF(vcvtpd2uqq), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 30 */
    {OP_vcvttps2udq, 0x0f7810, STROFF(vcvttps2udq), Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[30][1]},
    {OP_vcvttps2udq, 0x0f7810, STROFF(vcvttps2udq), Voq, xx, KEw, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 31 */
    {OP_vcvttpd2udq, 0x0f7850, STROFF(vcvttpd2udq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[31][1]},
    {OP_vcvttpd2udq, 0x0f7850, STROFF(vcvttpd2udq), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 32 */
    {OP_vcvttps2qq, 0x660f7a10, STROFF(vcvttps2qq), Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[32][1]},
    {OP_vcvttps2qq, 0x660f7a10, STROFF(vcvttps2qq), Voq, xx, KEb, Uqq, xx, mrm|evex|sae|tthv, x, END_LIST},
  },
  { /* mod extension 33 */
    {OP_vcvttpd2qq,0x660f7a50, STROFF(vcvttpd2qq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[33][1]},
    {OP_vcvttpd2qq,0x660f7a50, STROFF(vcvttpd2qq), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 34 */
    {OP_vcvttps2uqq, 0x660f7810, STROFF(vcvttps2uqq), Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[34][1]},
    {OP_vcvttps2uqq, 0x660f7810, STROFF(vcvttps2uqq), Voq, xx, KEb, Uqq, xx, mrm|evex|sae|tthv, x, END_LIST},
  },
  { /* mod extension 35 */
    {OP_vcvttpd2uqq, 0x660f7850, STROFF(vcvttpd2uqq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[35][1]},
    {OP_vcvttpd2uqq, 0x660f7850, STROFF(vcvttpd2uqq), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 36 */
    {OP_vcvtdq2ps, 0x0f5b10, STROFF(vcvtdq2ps), Ves, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[36][1]},
    {OP_vcvtdq2ps, 0x0f5b10, STROFF(vcvtdq2ps), Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 37 */
    {OP_vcvtqq2ps, 0x0f5b50, STROFF(vcvtqq2ps), Ves, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[37][1]},
    {OP_vcvtqq2ps, 0x0f5b50, STROFF(vcvtqq2ps), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 38 */
    {OP_vcvtqq2pd, 0xf30fe650, STROFF(vcvtqq2pd), Ved, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[38][1]},
    {OP_vcvtqq2pd, 0xf30fe650, STROFF(vcvtqq2pd), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 39 */
    {OP_vcvtudq2ps, 0xf20f7a10, STROFF(vcvtudq2ps), Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[39][1]},
    {OP_vcvtudq2ps, 0xf20f7a10, STROFF(vcvtudq2ps), Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 40 */
    {OP_vcvtuqq2ps, 0xf20f7a50, STROFF(vcvtuqq2ps), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[40][1]},
    {OP_vcvtuqq2ps, 0xf20f7a50, STROFF(vcvtuqq2ps), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 41 */
    {OP_vcvtuqq2pd, 0xf30f7a50, STROFF(vcvtuqq2pd), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[41][1]},
    {OP_vcvtuqq2pd, 0xf30f7a50, STROFF(vcvtuqq2pd), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 42 */
    {OP_vfmadd132ps,0x66389818,STROFF(vfmadd132ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[15]},
    {OP_vfmadd132ps,0x66389818,STROFF(vfmadd132ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[16]},
  },
  { /* mod extension 43 */
    {OP_vfmadd132pd,0x66389858,STROFF(vfmadd132pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[18]},
    {OP_vfmadd132pd,0x66389858,STROFF(vfmadd132pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[19]},
  },
  { /* mod extension 44 */
    {OP_vfmadd213ps,0x6638a818,STROFF(vfmadd213ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[21]},
    {OP_vfmadd213ps,0x6638a818,STROFF(vfmadd213ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[22]},
  },
  { /* mod extension 45 */
    {OP_vfmadd213pd,0x6638a858,STROFF(vfmadd213pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[24]},
    {OP_vfmadd213pd,0x6638a858,STROFF(vfmadd213pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[25]},
  },
  { /* mod extension 46 */
    {OP_vfmadd231ps,0x6638b818,STROFF(vfmadd231ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[27]},
    {OP_vfmadd231ps,0x6638b818,STROFF(vfmadd231ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[28]},
  },
  { /* mod extension 47 */
    {OP_vfmadd231pd,0x6638b858,STROFF(vfmadd231pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[30]},
    {OP_vfmadd231pd,0x6638b858,STROFF(vfmadd231pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[31]},
  },
  { /* mod extension 48 */
    {OP_vfmaddsub132ps,0x66389618,STROFF(vfmaddsub132ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[45]},
    {OP_vfmaddsub132ps,0x66389618,STROFF(vfmaddsub132ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[46]},
  },
  { /* mod extension 49 */
    {OP_vfmaddsub132pd,0x66389658,STROFF(vfmaddsub132pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[48]},
    {OP_vfmaddsub132pd,0x66389658,STROFF(vfmaddsub132pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[49]},
  },
  { /* mod extension 50 */
    {OP_vfmaddsub213ps,0x6638a618,STROFF(vfmaddsub213ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[51]},
    {OP_vfmaddsub213ps,0x6638a618,STROFF(vfmaddsub213ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[52]},
  },
  { /* mod extension 51 */
    {OP_vfmaddsub213pd,0x6638a658,STROFF(vfmaddsub213pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[54]},
    {OP_vfmaddsub213pd,0x6638a658,STROFF(vfmaddsub213pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[55]},
  },
  { /* mod extension 52 */
    {OP_vfmaddsub231ps,0x6638b618,STROFF(vfmaddsub231ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[57]},
    {OP_vfmaddsub231ps,0x6638b618,STROFF(vfmaddsub231ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[58]},
  },
  { /* mod extension 53 */
    {OP_vfmaddsub231pd,0x6638b658,STROFF(vfmaddsub231pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[60]},
    {OP_vfmaddsub231pd,0x6638b658,STROFF(vfmaddsub231pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[61]},
  },
  { /* mod extension 54 */
    {OP_vfmsubadd132ps,0x66389718,STROFF(vfmsubadd132ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[63]},
    {OP_vfmsubadd132ps,0x66389718,STROFF(vfmsubadd132ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[64]},
  },
  { /* mod extension 55 */
    {OP_vfmsubadd132pd,0x66389758,STROFF(vfmsubadd132pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[66]},
    {OP_vfmsubadd132pd,0x66389758,STROFF(vfmsubadd132pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[67]},
  },
  { /* mod extension 56 */
    {OP_vfmsubadd213ps,0x6638a718,STROFF(vfmsubadd213ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[69]},
    {OP_vfmsubadd213ps,0x6638a718,STROFF(vfmsubadd213ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[70]},
  },
  { /* mod extension 57 */
    {OP_vfmsubadd213pd,0x6638a758,STROFF(vfmsubadd213pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[72]},
    {OP_vfmsubadd213pd,0x6638a758,STROFF(vfmsubadd213pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[73]},
  },
  { /* mod extension 58 */
    {OP_vfmsubadd231ps,0x6638b718,STROFF(vfmsubadd231ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[75]},
    {OP_vfmsubadd231ps,0x6638b718,STROFF(vfmsubadd231ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[76]},
  },
  { /* mod extension 59 */
    {OP_vfmsubadd231pd,0x6638b758,STROFF(vfmsubadd231pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[78]},
    {OP_vfmsubadd231pd,0x6638b758,STROFF(vfmsubadd231pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[79]},
  },
  { /* mod extension 60 */
    {OP_vfmsub132ps,0x66389a18,STROFF(vfmsub132ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[81]},
    {OP_vfmsub132ps,0x66389a18,STROFF(vfmsub132ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[82]},
  },
  { /* mod extension 61 */
    {OP_vfmsub132pd,0x66389a58,STROFF(vfmsub132pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[84]},
    {OP_vfmsub132pd,0x66389a58,STROFF(vfmsub132pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[85]},
  },
  { /* mod extension 62 */
    {OP_vfmsub213ps,0x6638aa18,STROFF(vfmsub213ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[87]},
    {OP_vfmsub213ps,0x6638aa18,STROFF(vfmsub213ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[88]},
  },
  { /* mod extension 63 */
    {OP_vfmsub213pd,0x6638aa58,STROFF(vfmsub213pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[90]},
    {OP_vfmsub213pd,0x6638aa58,STROFF(vfmsub213pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[91]},
  },
  { /* mod extension 64 */
    {OP_vfmsub231ps,0x6638ba18,STROFF(vfmsub231ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[93]},
    {OP_vfmsub231ps,0x6638ba18,STROFF(vfmsub231ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[94]},
  },
  { /* mod extension 65 */
    {OP_vfmsub231pd,0x6638ba58,STROFF(vfmsub231pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[96]},
    {OP_vfmsub231pd,0x6638ba58,STROFF(vfmsub231pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[97]},
  },
  { /* mod extension 66 */
    {OP_vfnmadd132ps,0x66389c18,STROFF(vfnmadd132ps),Ves,xx,KEb,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[111]},
    {OP_vfnmadd132ps,0x66389c18,STROFF(vfnmadd132ps),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[112]},
  },
  { /* mod extension 67 */
    {OP_vfnmadd132pd,0x66389c58,STROFF(vfnmadd132pd),Ved,xx,KEw,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[114]},
    {OP_vfnmadd132pd,0x66389c58,STROFF(vfnmadd132pd),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[115]},
  },
  { /* mod extension 68 */
    {OP_vfnmadd213ps,0x6638ac18,STROFF(vfnmadd213ps),Ves,xx,KEb,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[117]},
    {OP_vfnmadd213ps,0x6638ac18,STROFF(vfnmadd213ps),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[118]},
  },
  { /* mod extension 69 */
    {OP_vfnmadd213pd,0x6638ac58,STROFF(vfnmadd213pd),Ved,xx,KEw,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[120]},
    {OP_vfnmadd213pd,0x6638ac58,STROFF(vfnmadd213pd),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[121]},
  },
  { /* mod extension 70 */
    {OP_vfnmadd231ps,0x6638bc18,STROFF(vfnmadd231ps),Ves,xx,KEb,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[123]},
    {OP_vfnmadd231ps,0x6638bc18,STROFF(vfnmadd231ps),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[124]},
  },
  { /* mod extension 71 */
    {OP_vfnmadd231pd,0x6638bc58,STROFF(vfnmadd231pd),Ved,xx,KEw,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[126]},
    {OP_vfnmadd231pd,0x6638bc58,STROFF(vfnmadd231pd),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[127]},
  },
  { /* mod extension 72 */
    {OP_vfnmsub132ps,0x66389e18,STROFF(vfnmsub132ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[141]},
    {OP_vfnmsub132ps,0x66389e18,STROFF(vfnmsub132ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[142]},
  },
  { /* mod extension 73 */
    {OP_vfnmsub132pd,0x66389e58,STROFF(vfnmsub132pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[144]},
    {OP_vfnmsub132pd,0x66389e58,STROFF(vfnmsub132pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[145]},
  },
  { /* mod extension 74 */
    {OP_vfnmsub213ps,0x6638ae18,STROFF(vfnmsub213ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[147]},
    {OP_vfnmsub213ps,0x6638ae18,STROFF(vfnmsub213ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[148]},
  },
  { /* mod extension 75 */
    {OP_vfnmsub213pd,0x6638ae58,STROFF(vfnmsub213pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[150]},
    {OP_vfnmsub213pd,0x6638ae58,STROFF(vfnmsub213pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[151]},
  },
  { /* mod extension 76 */
    {OP_vfnmsub231ps,0x6638be18,STROFF(vfnmsub231ps),Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[153]},
    {OP_vfnmsub231ps,0x6638be18,STROFF(vfnmsub231ps),Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[154]},
  },
  { /* mod extension 77 */
    {OP_vfnmsub231pd,0x6638be58,STROFF(vfnmsub231pd),Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[156]},
    {OP_vfnmsub231pd,0x6638be58,STROFF(vfnmsub231pd),Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[157]},
  },
  { /* mod extension 78 */
    {OP_vrcp28ps, 0x6638ca18, STROFF(vrcp28ps), Voq, xx, KEw, Md, xx, mrm|evex|reqp|ttfv,x,modx[78][1]},
    {OP_vrcp28ps, 0x6638ca18, STROFF(vrcp28ps), Voq, xx, KEw, Woq, xx, mrm|evex|sae|reqp|ttfv,x,END_LIST},
  },
  { /* mod extension 79 */
    {OP_vrcp28pd, 0x6638ca58, STROFF(vrcp28pd), Voq, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv,x,modx[79][1]},
    {OP_vrcp28pd, 0x6638ca58, STROFF(vrcp28pd), Voq, xx, KEb, Woq, xx, mrm|evex|sae|reqp|ttfv,x,END_LIST},
  },
  { /* mod extension 80 */
    {OP_vfixupimmps, 0x663a5418, STROFF(vfixupimmps), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[217]},
    {OP_vfixupimmps, 0x663a5418, STROFF(vfixupimmps), Voq, xx, KEw, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[218]},
  },
  { /* mod extension 81 */
    {OP_vfixupimmpd, 0x663a5458, STROFF(vfixupimmpd), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[220]},
    {OP_vfixupimmpd, 0x663a5458, STROFF(vfixupimmpd), Voq, xx, KEb, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[221]},
  },
  { /* mod extension 82 */
    {OP_vgetexpps, 0x66384218, STROFF(vgetexpps), Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, modx[82][1]},
    {OP_vgetexpps, 0x66384218, STROFF(vgetexpps), Voq, xx, KEw, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 83 */
    {OP_vgetexppd, 0x66384258, STROFF(vgetexppd), Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, modx[83][1]},
    {OP_vgetexppd, 0x66384258, STROFF(vgetexppd), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 84 */
    {OP_vgetmantps, 0x663a2618, STROFF(vgetmantps), Ve, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, modx[84][1]},
    {OP_vgetmantps, 0x663a2618, STROFF(vgetmantps), Voq, xx, KEw, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 85 */
    {OP_vgetmantpd, 0x663a2658, STROFF(vgetmantpd), Ve, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, modx[85][1]},
    {OP_vgetmantpd, 0x663a2658, STROFF(vgetmantpd), Voq, xx, KEb, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 86 */
    {OP_vrangeps, 0x663a5018, STROFF(vrangeps), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[231]},
    {OP_vrangeps, 0x663a5018, STROFF(vrangeps), Voq, xx, KEw, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[232]},
  },
  { /* mod extension 87 */
    {OP_vrangepd, 0x663a5058, STROFF(vrangepd), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[234]},
    {OP_vrangepd, 0x663a5058, STROFF(vrangepd), Voq, xx, KEb, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[235]},
  },
  { /* mod extension 88 */
    {OP_vreduceps, 0x663a5618, STROFF(vreduceps), Ve, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, modx[88][1]},
    {OP_vreduceps, 0x663a5618, STROFF(vreduceps), Voq, xx, KEw, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 89 */
    {OP_vreducepd, 0x663a5658, STROFF(vreducepd), Ve, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, modx[89][1]},
    {OP_vreducepd, 0x663a5658, STROFF(vreducepd), Voq, xx, KEb, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 90 */
    {OP_vrsqrt28ps, 0x6638cc18, STROFF(vrsqrt28ps), Voq, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, modx[90][1]},
    {OP_vrsqrt28ps, 0x6638cc18, STROFF(vrsqrt28ps), Voq, xx, KEw, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 91 */
    {OP_vrsqrt28pd, 0x6638cc58, STROFF(vrsqrt28pd), Voq, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, modx[91][1]},
    {OP_vrsqrt28pd, 0x6638cc58, STROFF(vrsqrt28pd), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 92 */
    {OP_vscalefps, 0x66382c18, STROFF(vscalefps), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, modx[92][1]},
    {OP_vscalefps, 0x66382c18, STROFF(vscalefps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 93 */
    {OP_vscalefpd, 0x66382c58, STROFF(vscalefpd), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, modx[93][1]},
    {OP_vscalefpd, 0x66382c58, STROFF(vscalefpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 94 */
    {OP_vexp2ps, 0x6638c818, STROFF(vexp2ps), Voq, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, modx[94][1]},
    {OP_vexp2ps, 0x6638c818, STROFF(vexp2ps), Voq, xx, KEw, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 95 */
    {OP_vexp2pd, 0x6638c858, STROFF(vexp2pd), Voq, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, modx[95][1]},
    {OP_vexp2pd, 0x6638c858, STROFF(vexp2pd), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 96 */
    {OP_vaddps, 0x0f5810, STROFF(vaddps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[96][1]},
    {OP_vaddps, 0x0f5810, STROFF(vaddps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 97 */
    {OP_vaddpd, 0x660f5850, STROFF(vaddpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[97][1]},
    {OP_vaddpd, 0x660f5850, STROFF(vaddpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 98 */
    {OP_vmulps, 0x0f5910, STROFF(vmulps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[98][1]},
    {OP_vmulps, 0x0f5910, STROFF(vmulps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 99 */
    {OP_vmulpd, 0x660f5950, STROFF(vmulpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[99][1]},
    {OP_vmulpd, 0x660f5950, STROFF(vmulpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 100 */
    {OP_vcvtps2pd, 0x0f5a10, STROFF(vcvtps2pd), Ved, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[100][1]},
    {OP_vcvtps2pd, 0x0f5a10, STROFF(vcvtps2pd), Voq, xx, KEb, Uqq, xx, mrm|evex|sae|tthv, x, END_LIST},
  },
  { /* mod extension 101 */
    {OP_vcvtpd2ps, 0x660f5a50, STROFF(vcvtpd2ps), Ves, xx, KEw, Mq, xx, mrm|evex|ttfv, x, modx[101][1]},
    {OP_vcvtpd2ps, 0x660f5a50, STROFF(vcvtpd2ps), Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 102 */
    {OP_vsubps, 0x0f5c10, STROFF(vsubps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[102][1]},
    {OP_vsubps, 0x0f5c10, STROFF(vsubps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 103 */
    {OP_vsubpd, 0x660f5c50, STROFF(vsubpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[103][1]},
    {OP_vsubpd, 0x660f5c50, STROFF(vsubpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 104 */
    {OP_vminps, 0x0f5d10, STROFF(vminps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[104][1]},
    {OP_vminps, 0x0f5d10, STROFF(vminps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 105 */
    {OP_vminpd, 0x660f5d50, STROFF(vminpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[105][1]},
    {OP_vminpd, 0x660f5d50, STROFF(vminpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 106 */
    {OP_vdivps, 0x0f5e10, STROFF(vdivps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[106][1]},
    {OP_vdivps, 0x0f5e10, STROFF(vdivps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 107 */
    {OP_vdivpd, 0x660f5e50, STROFF(vdivpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[107][1]},
    {OP_vdivpd, 0x660f5e50, STROFF(vdivpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 108 */
    {OP_vmaxps,   0x0f5f10, STROFF(vmaxps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[108][1]},
    {OP_vmaxps,   0x0f5f10, STROFF(vmaxps), Voq, xx, KEw, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 109 */
    {OP_vmaxpd, 0x660f5f50, STROFF(vmaxpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[109][1]},
    {OP_vmaxpd, 0x660f5f50, STROFF(vmaxpd), Voq, xx, KEb, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 110 */
    {OP_vrndscaleps, 0x663a0818, STROFF(vrndscaleps),  Ve, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, modx[110][1]},
    {OP_vrndscaleps, 0x663a0818, STROFF(vrndscaleps),  Voq, xx, KEw, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 111 */
    {OP_vrndscalepd, 0x663a0958, STROFF(vrndscalepd),  Ve, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, modx[111][1]},
    {OP_vrndscalepd, 0x663a0958, STROFF(vrndscalepd),  Voq, xx, KEb, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 112 */
    {OP_vcvtpd2dq, 0xf20fe650, STROFF(vcvtpd2dq),  Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[112][1]},
    {OP_vcvtpd2dq, 0xf20fe650, STROFF(vcvtpd2dq),  Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 113 */
    {OP_vcvttpd2dq,0x660fe650, STROFF(vcvttpd2dq), Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[113][1]},
    {OP_vcvttpd2dq,0x660fe650, STROFF(vcvttpd2dq), Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 114 */
    {OP_vcmpps, 0x0fc210, STROFF(vcmpps), KPw, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[191]},
    {OP_vcmpps, 0x0fc210, STROFF(vcmpps), KPw, xx, KEw, Ib, Hoq, xop|mrm|evex|sae|ttfv, x, exop[192]},
  },
  { /* mod extension 115 */
    {OP_vcmppd, 0x660fc250, STROFF(vcmppd), KPb, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[198]},
    {OP_vcmppd, 0x660fc250, STROFF(vcmppd), KPb, xx, KEb, Ib, Hoq, xop|mrm|evex|sae|ttfv, x, exop[199]},
  },
  { /* mod extension 116 */
    {OP_vcvtps2dq, 0x660f5b10, STROFF(vcvtps2dq), Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[116][1]},
    {OP_vcvtps2dq, 0x660f5b10, STROFF(vcvtps2dq), Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 117 */
    {OP_vcvttps2dq, 0xf30f5b10, STROFF(vcvttps2dq), Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[117][1]},
    {OP_vcvttps2dq, 0xf30f5b10, STROFF(vcvttps2dq), Voq, xx, KEw, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 118 */
    {OP_vsqrtps,0x0f5110, STROFF(vsqrtps), Ves, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[118][1]},
    {OP_vsqrtps,0x660f5110, STROFF(vsqrtps), Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 119 */
    {OP_vsqrtpd,0x660f5150, STROFF(vsqrtpd), Ved, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[119][1]},
    {OP_vsqrtpd,0x660f5150, STROFF(vsqrtpd), Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 120 */
    {INVALID, 0x0f0135, STROFF(BAD), xx, xx, xx, xx, xx, no, x, END_LIST},
    {RM_EXT,  0x0f0175, STROFF(group_7_mod_rm_ext_5), xx, xx, xx, xx, xx, mrm, x, 5},
  },
};

/* Naturally all of these have modrm bytes even if they have no explicit operands */
const instr_info_t rm_extensions[][8] = {
  { /* rm extension 0 */
    {OP_enclv,    0xc00f0171, STROFF(enclv), eax, ebx, eax, ebx, ecx, mrm|xop, x, exop[0x100]},
    {OP_vmcall,   0xc10f0171, STROFF(vmcall),   xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmlaunch, 0xc20f0171, STROFF(vmlaunch), xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmresume, 0xc30f0171, STROFF(vmresume), xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmxoff,   0xc40f0171, STROFF(vmxoff),   xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* rm extension 1 */
    /* XXX i#4013: Treat address in xax as IR memref? */
    {OP_monitor, 0xc80f0171, STROFF(monitor),  xx, xx, axAX, ecx, edx, mrm, x, END_LIST},
    {OP_mwait,   0xc90f0171, STROFF(mwait),  xx, xx, eax, ecx, xx, mrm, x, END_LIST},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_encls,  0xcf0f0171, STROFF(encls), eax, ebx, eax, ebx, ecx, mrm|xop, x, exop[0xfe]},
  },
  { /* rm extension 2 */
    {OP_swapgs, 0xf80f0177, STROFF(swapgs), xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_rdtscp, 0xf90f0177, STROFF(rdtscp), edx, eax, xx, xx, xx, mrm|xop, x, exop[10]},/*AMD-only*/
    /* XXX i#4013: Treat address in xax as IR memref? */
    {OP_monitorx, 0xfa0f0177, STROFF(monitorx),  xx, xx, axAX, ecx, edx, mrm, x, END_LIST},/*AMD-only*/
    {OP_mwaitx, 0xfb0f0177, STROFF(mwaitx),  xx, xx, eax, ecx, xx, mrm, x, END_LIST},/*AMD-only*/
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* rm extension 3 */
    {OP_vmrun,  0xd80f0173, STROFF(vmrun), xx, xx, axAX, xx, xx, mrm, x, END_LIST},
    {OP_vmmcall,0xd90f0173, STROFF(vmmcall), xx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_vmload, 0xda0f0173, STROFF(vmload), xx, xx, axAX, xx, xx, mrm, x, END_LIST},
    {OP_vmsave, 0xdb0f0173, STROFF(vmsave), xx, xx, axAX, xx, xx, mrm, x, END_LIST},
    {OP_stgi,   0xdc0f0173, STROFF(stgi), xx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_clgi,   0xdd0f0173, STROFF(clgi), xx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_skinit, 0xde0f0173, STROFF(skinit), xx, xx, eax, xx, xx, mrm, x, END_LIST},
    {OP_invlpga,0xdf0f0173, STROFF(invlpga), xx, xx, axAX, ecx, xx, mrm, x, END_LIST},
  },
  { /* rm extension 4 */
    {OP_xgetbv, 0xd00f0172, STROFF(xgetbv), edx, eax, ecx, xx, xx, mrm, x, END_LIST},
    {OP_xsetbv, 0xd10f0172, STROFF(xsetbv), xx, xx, ecx, edx, eax, mrm, x, END_LIST},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmfunc, 0xd40f0172, STROFF(vmfunc), xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    /* Only if the transaction fails does xend write to eax => predcx.
     * XXX i#1314: on failure eip is also written to.
     */
    {OP_xend,   0xd50f0172, STROFF(xend), eax, xx, xx, xx, xx, mrm|predcx, x, NA},
    {OP_xtest,  0xd60f0172, STROFF(xtest), xx, xx, xx, xx, xx, mrm, fW6, NA},
    {OP_enclu,  0xd70f0172, STROFF(enclu), eax, ebx, eax, ebx, ecx, mrm|xop, x, exop[0xff]},
  },
  { /* rm extension 5 */
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_rdpkru, 0xee0f0171, STROFF(rdpkru), eax, edx, ecx, xx, xx, mrm, x, END_LIST},
    {OP_wrpkru, 0xef0f0171, STROFF(wrpkru), xx, xx, ecx, edx, eax, mrm, x, END_LIST},
  },
};

/****************************************************************************
 * Instructions that differ depending on whether in 64-bit mode
 */

const instr_info_t x64_extensions[][2] = {
  {    /* x64_ext 0 */
    {OP_inc,  0x400000, STROFF(inc), zAX, xx, zAX, xx, xx, i64, (fW6&(~fWC)), t64e[1][0]},
    {PREFIX,  0x400000, STROFF(rex), xx, xx, xx, xx, xx, no, x, PREFIX_REX_GENERAL},
  }, { /* x64_ext 1 */
    {OP_inc,  0x410000, STROFF(inc), zCX, xx, zCX, xx, xx, i64, (fW6&(~fWC)), t64e[2][0]},
    {PREFIX,  0x410000, STROFF(rexb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_B},
  }, { /* x64_ext 2 */
    {OP_inc,  0x420000, STROFF(inc), zDX, xx, zDX, xx, xx, i64, (fW6&(~fWC)), t64e[3][0]},
    {PREFIX,  0x420000, STROFF(rexx), xx, xx, xx, xx, xx, no, x, PREFIX_REX_X},
  }, { /* x64_ext 3 */
    {OP_inc,  0x430000, STROFF(inc), zBX, xx, zBX, xx, xx, i64, (fW6&(~fWC)), t64e[4][0]},
    {PREFIX,  0x430000, STROFF(rexxb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 4 */
    {OP_inc,  0x440000, STROFF(inc), zSP, xx, zSP, xx, xx, i64, (fW6&(~fWC)), t64e[5][0]},
    {PREFIX,  0x440000, STROFF(rexr), xx, xx, xx, xx, xx, no, x, PREFIX_REX_R},
  }, { /* x64_ext 5 */
    {OP_inc,  0x450000, STROFF(inc), zBP, xx, zBP, xx, xx, i64, (fW6&(~fWC)), t64e[6][0]},
    {PREFIX,  0x450000, STROFF(rexrb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_R|PREFIX_REX_B},
  }, { /* x64_ext 6 */
    {OP_inc,  0x460000, STROFF(inc), zSI, xx, zSI, xx, xx, i64, (fW6&(~fWC)), t64e[7][0]},
    {PREFIX,  0x460000, STROFF(rexrx), xx, xx, xx, xx, xx, no, x, PREFIX_REX_R|PREFIX_REX_X},
  }, { /* x64_ext 7 */
    {OP_inc,  0x470000, STROFF(inc), zDI, xx, zDI, xx, xx, i64, (fW6&(~fWC)), tex[12][0]},
    {PREFIX,  0x470000, STROFF(rexrxb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_R|PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 8 */
    {OP_dec,  0x480000, STROFF(dec), zAX, xx, zAX, xx, xx, i64, (fW6&(~fWC)), t64e[9][0]},
    {PREFIX,  0x480000, STROFF(rexw), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W},
  }, { /* x64_ext 9 */
    {OP_dec,  0x490000, STROFF(dec), zCX, xx, zCX, xx, xx, i64, (fW6&(~fWC)), t64e[10][0]},
    {PREFIX,  0x490000, STROFF(rexwb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_B},
  }, { /* x64_ext 10 */
    {OP_dec,  0x4a0000, STROFF(dec), zDX, xx, zDX, xx, xx, i64, (fW6&(~fWC)), t64e[11][0]},
    {PREFIX,  0x4a0000, STROFF(rexwx), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_X},
  }, { /* x64_ext 11 */
    {OP_dec,  0x4b0000, STROFF(dec), zBX, xx, zBX, xx, xx, i64, (fW6&(~fWC)), t64e[12][0]},
    {PREFIX,  0x4b0000, STROFF(rexwxb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 12 */
    {OP_dec,  0x4c0000, STROFF(dec), zSP, xx, zSP, xx, xx, i64, (fW6&(~fWC)), t64e[13][0]},
    {PREFIX,  0x4c0000, STROFF(rexwr), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R},
  }, { /* x64_ext 13 */
    {OP_dec,  0x4d0000, STROFF(dec), zBP, xx, zBP, xx, xx, i64, (fW6&(~fWC)), t64e[14][0]},
    {PREFIX,  0x4d0000, STROFF(rexwrb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R|PREFIX_REX_B},
  }, { /* x64_ext 14 */
    {OP_dec,  0x4e0000, STROFF(dec), zSI, xx, zSI, xx, xx, i64, (fW6&(~fWC)), t64e[15][0]},
    {PREFIX,  0x4e0000, STROFF(rexwrx), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R|PREFIX_REX_X},
  }, { /* x64_ext 15 */
    {OP_dec,  0x4f0000, STROFF(dec), zDI, xx, zDI, xx, xx, i64, (fW6&(~fWC)), tex[12][1]},
    {PREFIX,  0x4f0000, STROFF(rexwrxb), xx, xx, xx, xx, xx, no, x, PREFIX_REX_W|PREFIX_REX_R|PREFIX_REX_X|PREFIX_REX_B},
  }, { /* x64_ext 16 */
    {OP_arpl,   0x630000, STROFF(arpl), Ew, xx, Gw, xx, xx, mrm|i64, fWZ, END_LIST},
    {OP_movsxd, 0x630000, STROFF(movsxd), Gv, xx, Ed, xx, xx, mrm|o64, x, END_LIST},
  },
};

/****************************************************************************
 * Instructions that differ depending on the first two bits of the 2nd byte,
 * or whether in x64 mode.
 */
const instr_info_t vex_prefix_extensions[][2] = {
  {    /* vex_prefix_ext 0 */
    {OP_les,  0xc40000, STROFF(les), Gz, es, Mp, xx, xx, mrm|i64, x, END_LIST},
    {PREFIX,  0xc40000, STROFF(vex_2b), xx, xx, xx, xx, xx, no, x, PREFIX_VEX_3B},
  }, { /* vex_prefix_ext 1 */
    {OP_lds,  0xc50000, STROFF(lds), Gz, ds, Mp, xx, xx, mrm|i64, x, END_LIST},
    {PREFIX,  0xc50000, STROFF(vex_1b), xx, xx, xx, xx, xx, no, x, PREFIX_VEX_2B},
  },
};

/****************************************************************************
 * Instructions that differ depending on bits 4 and 5 of the 2nd byte.
 */
const instr_info_t xop_prefix_extensions[][2] = {
  {    /* xop_prefix_ext 0 */
    {EXTENSION, 0x8f0000, STROFF(group_1d), xx, xx, xx, xx, xx, mrm, x, 26},
    {PREFIX,    0x8f0000, STROFF(xop), xx, xx, xx, xx, xx, no, x, PREFIX_XOP},
  },
};

/****************************************************************************
 * Instructions that differ depending on whether vex-encoded and vex.L
 * Index 0 = no vex, 1 = vex and vex.L=0, 2 = vex and vex.L=1
 */
const instr_info_t vex_L_extensions[][3] = {
  {    /* vex_L_ext 0 */
    {OP_emms,       0x0f7710, STROFF(emms), xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vzeroupper, 0x0f7710, STROFF(vzeroupper), xx, xx, xx, xx, xx, vex, x, END_LIST},
    {OP_vzeroall,   0x0f7790, STROFF(vzeroall), xx, xx, xx, xx, xx, vex, x, END_LIST},
  },
};

/****************************************************************************
* Instructions that differ depending on whether evex-encoded.
* Index 0 = no evex, 1 = evex
*/

const instr_info_t evex_prefix_extensions[][2] = {
  {   /* evex_prefix_ext */
    {OP_bound, 0x620000, STROFF(bound), xx, xx, Gv, Ma, xx, mrm|i64, x, END_LIST},
    {PREFIX,   0x620000, STROFF(evex_prefix), xx, xx, xx, xx, xx, no, x, PREFIX_EVEX},
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
    /* We chain these even though encoding won't find them. */
    {OP_nop,  0x900000, STROFF(nop), xx, xx, xx, xx, xx, no, x, tpe[103][3]},
    /* For decoding we avoid needing new operand types by only getting
     * here if rex.b is set.  For encode, we would need either to take
     * REQUIRES_REX + OPCODE_SUFFIX or a new operand type for registers that
     * must be extended (could also try to list r8 instead of eax but
     * have to make sure all decode/encode routines can handle that as most
     * assume the registers listed here are 32-bit base): that's too
     * much effort for a corner case that we're not 100% certain works on
     * all x64 processors, so we just don't list in the encoding chain.
     * See i#5446.
     */
    {OP_xchg, 0x900000, STROFF(xchg), eAX_x, eAX, eAX_x, eAX, xx, o64, x, END_LIST},
  },
};

/* Instructions that differ depending on whether rex.w in is present.
 * The table is indexed by rex.w: index 0 is for no rex.w.
 */
const instr_info_t rex_w_extensions[][2] = {
  { /* rex.w extension 0 */
    {OP_fxsave32, 0x0fae30, STROFF(fxsave),   Moq, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_fxsave64, 0x0fae30, STROFF(fxsave64), Moq, xx, xx, xx, xx, mrm|rex, x, END_LIST},
  },
  { /* rex.w extension 1 */
    {OP_fxrstor32, 0x0fae31, STROFF(fxrstor),   xx, xx, Moq, xx, xx, mrm, x, END_LIST},
    {OP_fxrstor64, 0x0fae31, STROFF(fxrstor64), xx, xx, Moq, xx, xx, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 2 */
    {OP_xsave32,   0x0fae34, STROFF(xsave),   Mxsave, xx, edx, eax, xx, mrm, x, END_LIST},
    {OP_xsave64,   0x0fae34, STROFF(xsave64), Mxsave, xx, edx, eax, xx, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 3 */
    {OP_xrstor32, 0x0fae35, STROFF(xrstor),   xx, xx, Mxsave, edx, eax, mrm, x, END_LIST},
    {OP_xrstor64, 0x0fae35, STROFF(xrstor64), xx, xx, Mxsave, edx, eax, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 4 */
    {OP_xsaveopt32, 0x0fae36, STROFF(xsaveopt),   Mxsave, xx, edx, eax, xx, mrm, x, END_LIST},
    {OP_xsaveopt64, 0x0fae36, STROFF(xsaveopt64), Mxsave, xx, edx, eax, xx, mrm|rex, o64, END_LIST},
  },
  { /* rex.w extension 5 */
    {OP_xsavec32, 0x0fc734, STROFF(xsavec),   Mxsave, xx, edx, eax, xx, mrm, x, END_LIST},
    {OP_xsavec64, 0x0fc734, STROFF(xsavec64), Mxsave, xx, edx, eax, xx, mrm|rex, o64, END_LIST},
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
    16,127,128, 88,  17, 18,111, 19,  89, 90, 91,134,  13, 14, 15,133,  /* 1 */
    20, 21, 22, 23,  24, 25,148,149,  26, 27, 28, 29,  92, 93, 94, 95,  /* 2 */
    30, 31, 32, 33,  34, 35,112, 36,  37, 38, 39, 40,  41, 42, 43, 44,  /* 3 */
    45, 46,142,143, 156,113,114,115,   0,  0,  0,  0, 129,130,150,151,  /* 4 */
   166,167,168,169,   0,171,  0,  0, 118,119,108,138,   0,  0,  0,  0,  /* 5 */
     0,  0,  0,  0, 145,139,144,  0,   0,  0,  0,  0,   0,  0,  0,  0,  /* 6 */
     0,  0,170,  0,   0,123,122,121, 116,117,135,136, 137,124,125,126,  /* 7 */
    49, 50,103,  0,   0,  0,  0,  0, 141,147,140,146, 109,120,110,  0,  /* 8 */
   104,105,106,107,   0,  0, 58, 59,  60, 61, 62, 63,  64, 65, 66, 67,  /* 9 */
   159,160,161,162,   0,  0, 68, 69,  70, 71, 72, 73,  74, 75, 76, 77,  /* A */
     0,  0,  0,  0, 157,158, 78, 79,  80, 81, 82, 83,  84, 85, 86, 87,  /* B */
     0,  0,  0,  0, 155,  0,163,164, 154,165,131,132, 152,153,  0,  0,  /* C */
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0, 51,  52, 53, 54, 55,  /* D */
     0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,   0,  0,  0,  0,  /* E */
    47, 48,100, 99,   0,101,102, 98,   0,  0,  0,  0,   0,  0,  0,  0   /* F */
};

const instr_info_t third_byte_38[] = {
  {INVALID,     0x38ff18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},              /* 0*/
  /**** SSSE3 ****/
  {PREFIX_EXT,  0x380018,   STROFF(prefix_ext_118), xx, xx, xx, xx, xx, mrm, x, 118},/* 1*/
  {PREFIX_EXT,  0x380118,   STROFF(prefix_ext_119), xx, xx, xx, xx, xx, mrm, x, 119},/* 2*/
  {PREFIX_EXT,  0x380218,   STROFF(prefix_ext_120), xx, xx, xx, xx, xx, mrm, x, 120},/* 3*/
  {PREFIX_EXT,  0x380318,   STROFF(prefix_ext_121), xx, xx, xx, xx, xx, mrm, x, 121},/* 4*/
  {PREFIX_EXT,  0x380418,   STROFF(prefix_ext_122), xx, xx, xx, xx, xx, mrm, x, 122},/* 5*/
  {PREFIX_EXT,  0x380518,   STROFF(prefix_ext_123), xx, xx, xx, xx, xx, mrm, x, 123},/* 6*/
  {PREFIX_EXT,  0x380618,   STROFF(prefix_ext_124), xx, xx, xx, xx, xx, mrm, x, 124},/* 7*/
  {PREFIX_EXT,  0x380718,   STROFF(prefix_ext_125), xx, xx, xx, xx, xx, mrm, x, 125},/* 8*/
  {PREFIX_EXT,  0x380818,   STROFF(prefix_ext_126), xx, xx, xx, xx, xx, mrm, x, 126},/* 9*/
  {PREFIX_EXT,  0x380918,   STROFF(prefix_ext_127), xx, xx, xx, xx, xx, mrm, x, 127},/*10*/
  {PREFIX_EXT,  0x380a18,   STROFF(prefix_ext_128), xx, xx, xx, xx, xx, mrm, x, 128},/*11*/
  {PREFIX_EXT,  0x380b18,   STROFF(prefix_ext_129), xx, xx, xx, xx, xx, mrm, x, 129},/*12*/
  {PREFIX_EXT,  0x381c18,   STROFF(prefix_ext_130), xx, xx, xx, xx, xx, mrm, x, 130},/*13*/
  {PREFIX_EXT,  0x381d18,   STROFF(prefix_ext_131), xx, xx, xx, xx, xx, mrm, x, 131},/*14*/
  {PREFIX_EXT,  0x381e18,   STROFF(prefix_ext_132), xx, xx, xx, xx, xx, mrm, x, 132},/*15*/
  /**** SSE4 ****/
  {E_VEX_EXT,  0x66381018, STROFF(e_vex_ext_132), xx, xx, xx, xx, xx, mrm, x, 132},/*16*/
  {E_VEX_EXT,    0x381418, STROFF(e_vex_ext_130), xx, xx, xx, xx, xx, mrm, x, 130},/*17*/
  {E_VEX_EXT,  0x66381518, STROFF(e_vex_ext_129), xx, xx, xx, xx, xx, mrm, x, 129},/*18*/
  {E_VEX_EXT,  0x66381718, STROFF(e_vex_ext_3), xx, xx, xx, xx, xx, mrm, x,  3},/*19*/
  /* 20 */
  {E_VEX_EXT,  0x66382018, STROFF(e_vex_ext__4), xx, xx, xx, xx, xx, mrm, x,  4},/*20*/
  {E_VEX_EXT,  0x66382118, STROFF(e_vex_ext__5), xx, xx, xx, xx, xx, mrm, x,  5},/*21*/
  {E_VEX_EXT,  0x66382218, STROFF(e_vex_ext__6), xx, xx, xx, xx, xx, mrm, x,  6},/*22*/
  {E_VEX_EXT,  0x66382318, STROFF(e_vex_ext__7), xx, xx, xx, xx, xx, mrm, x,  7},/*23*/
  {E_VEX_EXT,  0x66382418, STROFF(e_vex_ext__8), xx, xx, xx, xx, xx, mrm, x,  8},/*24*/
  {E_VEX_EXT,  0x66382518, STROFF(e_vex_ext__9), xx, xx, xx, xx, xx, mrm, x,  9},/*25*/
  {E_VEX_EXT,  0x66382818, STROFF(e_vex_ext_10), xx, xx, xx, xx, xx, mrm, x, 10},/*26*/
  {E_VEX_EXT,  0x66382918, STROFF(e_vex_ext_11), xx, xx, xx, xx, xx, mrm, x, 11},/*27*/
  {E_VEX_EXT,  0x66382a18, STROFF(e_vex_ext_12), xx, xx, xx, xx, xx, mrm, x, 12},/*28*/
  {E_VEX_EXT,  0x66382b18, STROFF(e_vex_ext_13), xx, xx, xx, xx, xx, mrm, x, 13},/*29*/
  /* 30 */
  {E_VEX_EXT,  0x66383018, STROFF(e_vex_ext_14), xx, xx, xx, xx, xx, mrm, x, 14},/*30*/
  {E_VEX_EXT,  0x66383118, STROFF(e_vex_ext_15), xx, xx, xx, xx, xx, mrm, x, 15},/*31*/
  {E_VEX_EXT,  0x66383218, STROFF(e_vex_ext_16), xx, xx, xx, xx, xx, mrm, x, 16},/*32*/
  {E_VEX_EXT,  0x66383318, STROFF(e_vex_ext_17), xx, xx, xx, xx, xx, mrm, x, 17},/*33*/
  {E_VEX_EXT,  0x66383418, STROFF(e_vex_ext_18), xx, xx, xx, xx, xx, mrm, x, 18},/*34*/
  {E_VEX_EXT,  0x66383518, STROFF(e_vex_ext_19), xx, xx, xx, xx, xx, mrm, x, 19},/*35*/
  {E_VEX_EXT,  0x66383718, STROFF(e_vex_ext_20), xx, xx, xx, xx, xx, mrm, x, 20},/*36*/
  {E_VEX_EXT,  0x66383818, STROFF(e_vex_ext_21), xx, xx, xx, xx, xx, mrm, x, 21},/*37*/
  {E_VEX_EXT,  0x66383918, STROFF(e_vex_ext_22), xx, xx, xx, xx, xx, mrm, x, 22},/*38*/
  {E_VEX_EXT,  0x66383a18, STROFF(e_vex_ext_23), xx, xx, xx, xx, xx, mrm, x, 23},/*39*/
  {E_VEX_EXT,  0x66383b18, STROFF(e_vex_ext_24), xx, xx, xx, xx, xx, mrm, x, 24},/*40*/
  {E_VEX_EXT,  0x66383c18, STROFF(e_vex_ext_25), xx, xx, xx, xx, xx, mrm, x, 25},/*41*/
  {E_VEX_EXT,  0x66383d18, STROFF(e_vex_ext_26), xx, xx, xx, xx, xx, mrm, x, 26},/*42*/
  {E_VEX_EXT,  0x66383e18, STROFF(e_vex_ext_27), xx, xx, xx, xx, xx, mrm, x, 27},/*43*/
  {E_VEX_EXT,  0x66383f18, STROFF(e_vex_ext_28), xx, xx, xx, xx, xx, mrm, x, 28},/*44*/
  /* 40 */
  {E_VEX_EXT,  0x66384018, STROFF(e_vex_ext_29), xx, xx, xx, xx, xx, mrm, x, 29},/*45*/
  {E_VEX_EXT,  0x66384118, STROFF(e_vex_ext_30), xx, xx, xx, xx, xx, mrm, x, 30},/*46*/
  /* f0 */
  {PREFIX_EXT,  0x38f018,   STROFF(prefix_ext_138), xx, xx, xx, xx, xx, mrm, x, 138},/*47*/
  {PREFIX_EXT,  0x38f118,   STROFF(prefix_ext_139), xx, xx, xx, xx, xx, mrm, x, 139},/*48*/
  /* 80 */
  {OP_invept,   0x66388018, STROFF(invept),   xx, xx, Gr, Mdq, xx, mrm|reqp, x, END_LIST},/*49*/
  {OP_invvpid,  0x66388118, STROFF(invvpid),  xx, xx, Gr, Mdq, xx, mrm|reqp, x, END_LIST},/*50*/
  /* db-df */
  {E_VEX_EXT,  0x6638db18, STROFF(e_vex_ext_31), xx, xx, xx, xx, xx, mrm, x, 31},/*51*/
  {E_VEX_EXT,  0x6638dc18, STROFF(e_vex_ext_32), xx, xx, xx, xx, xx, mrm, x, 32},/*52*/
  {E_VEX_EXT,  0x6638dd18, STROFF(e_vex_ext_33), xx, xx, xx, xx, xx, mrm, x, 33},/*53*/
  {E_VEX_EXT,  0x6638de18, STROFF(e_vex_ext_34), xx, xx, xx, xx, xx, mrm, x, 34},/*54*/
  {E_VEX_EXT,  0x6638df18, STROFF(e_vex_ext_35), xx, xx, xx, xx, xx, mrm, x, 35},/*55*/
  /* AVX */
  {E_VEX_EXT,  0x66380e18, STROFF(e_vex_ext_59), xx, xx, xx, xx, xx, mrm, x, 59},/*56*/
  {E_VEX_EXT,  0x66380f18, STROFF(e_vex_ext_60), xx, xx, xx, xx, xx, mrm, x, 60},/*57*/
  /* FMA 96-9f */
  {E_VEX_EXT,   0x389618,  STROFF(e_vex_ext_99), xx, xx, xx, xx, xx, mrm, x, 99},/*58*/
  {E_VEX_EXT,   0x389718, STROFF(e_vex_ext_102), xx, xx, xx, xx, xx, mrm, x, 102},/*59*/
  {E_VEX_EXT,   0x389818,  STROFF(e_vex_ext_93), xx, xx, xx, xx, xx, mrm, x, 93},/*60*/
  {E_VEX_EXT,   0x389918,  STROFF(e_vex_ext_96), xx, xx, xx, xx, xx, mrm, x, 96},/*61*/
  {E_VEX_EXT,   0x389a18, STROFF(e_vex_ext_105), xx, xx, xx, xx, xx, mrm, x, 105},/*62*/
  {E_VEX_EXT,   0x389b18, STROFF(e_vex_ext_108), xx, xx, xx, xx, xx, mrm, x, 108},/*63*/
  {E_VEX_EXT,   0x389c18, STROFF(e_vex_ext_111), xx, xx, xx, xx, xx, mrm, x, 111},/*64*/
  {E_VEX_EXT,   0x389d18, STROFF(e_vex_ext_114), xx, xx, xx, xx, xx, mrm, x, 114},/*65*/
  {E_VEX_EXT,   0x389e18, STROFF(e_vex_ext_117), xx, xx, xx, xx, xx, mrm, x, 117},/*66*/
  {E_VEX_EXT,   0x389f18, STROFF(e_vex_ext_120), xx, xx, xx, xx, xx, mrm, x, 120},/*67*/
  /* FMA a6-af */
  {E_VEX_EXT,   0x38a618, STROFF(e_vex_ext_100), xx, xx, xx, xx, xx, mrm, x, 100},/*68*/
  {E_VEX_EXT,   0x38a718, STROFF(e_vex_ext_103), xx, xx, xx, xx, xx, mrm, x, 103},/*69*/
  {E_VEX_EXT,   0x38a818,  STROFF(e_vex_ext_94), xx, xx, xx, xx, xx, mrm, x, 94},/*70*/
  {E_VEX_EXT,   0x38a918,  STROFF(e_vex_ext_97), xx, xx, xx, xx, xx, mrm, x, 97},/*71*/
  {E_VEX_EXT,   0x38aa18, STROFF(e_vex_ext_106), xx, xx, xx, xx, xx, mrm, x, 106},/*72*/
  {E_VEX_EXT,   0x38ab18, STROFF(e_vex_ext_109), xx, xx, xx, xx, xx, mrm, x, 109},/*73*/
  {E_VEX_EXT,   0x38ac18, STROFF(e_vex_ext_112), xx, xx, xx, xx, xx, mrm, x, 112},/*74*/
  {E_VEX_EXT,   0x38ad18, STROFF(e_vex_ext_115), xx, xx, xx, xx, xx, mrm, x, 115},/*75*/
  {E_VEX_EXT,   0x38ae18, STROFF(e_vex_ext_118), xx, xx, xx, xx, xx, mrm, x, 118},/*76*/
  {E_VEX_EXT,   0x38af18, STROFF(e_vex_ext_121), xx, xx, xx, xx, xx, mrm, x, 121},/*77*/
  /* FMA b6-bf */
  {E_VEX_EXT,   0x38b618, STROFF(e_vex_ext_101), xx, xx, xx, xx, xx, mrm, x, 101},/*78*/
  {E_VEX_EXT,   0x38b718, STROFF(e_vex_ext_104), xx, xx, xx, xx, xx, mrm, x, 104},/*79*/
  {E_VEX_EXT,   0x38b818,  STROFF(e_vex_ext_95), xx, xx, xx, xx, xx, mrm, x, 95},/*80*/
  {E_VEX_EXT,   0x38b918,  STROFF(e_vex_ext_98), xx, xx, xx, xx, xx, mrm, x, 98},/*81*/
  {E_VEX_EXT,   0x38ba18, STROFF(e_vex_ext_107), xx, xx, xx, xx, xx, mrm, x, 107},/*82*/
  {E_VEX_EXT,   0x38bb18, STROFF(e_vex_ext_110), xx, xx, xx, xx, xx, mrm, x, 110},/*83*/
  {E_VEX_EXT,   0x38bc18, STROFF(e_vex_ext_113), xx, xx, xx, xx, xx, mrm, x, 113},/*84*/
  {E_VEX_EXT,   0x38bd18, STROFF(e_vex_ext_116), xx, xx, xx, xx, xx, mrm, x, 116},/*85*/
  {E_VEX_EXT,   0x38be18, STROFF(e_vex_ext_119), xx, xx, xx, xx, xx, mrm, x, 119},/*86*/
  {E_VEX_EXT,   0x38bf18, STROFF(e_vex_ext_122), xx, xx, xx, xx, xx, mrm, x, 122},/*87*/
  /* AVX overlooked in original pass */
  {E_VEX_EXT, 0x66381318, STROFF(e_vex_ext_63), xx, xx, xx, xx, xx, mrm, x, 63},/*88*/
  {E_VEX_EXT, 0x66381818, STROFF(e_vex_ext_64), xx, xx, xx, xx, xx, mrm, x, 64},/*89*/
  {E_VEX_EXT, 0x66381918, STROFF(e_vex_ext_65), xx, xx, xx, xx, xx, mrm, x, 65},/*90*/
  {E_VEX_EXT, 0x66381a18, STROFF(e_vex_ext_66), xx, xx, xx, xx, xx, mrm, x, 66},/*91*/
  {E_VEX_EXT, 0x66382c18, STROFF(e_vex_ext_67), xx, xx, xx, xx, xx, mrm, x, 67},/*92*/
  {E_VEX_EXT, 0x66382d18, STROFF(e_vex_ext_68), xx, xx, xx, xx, xx, mrm, x, 68},/*93*/
  {E_VEX_EXT, 0x66382e18, STROFF(e_vex_ext_69), xx, xx, xx, xx, xx, mrm, x, 69},/*94*/
  {E_VEX_EXT, 0x66382f18, STROFF(e_vex_ext_70), xx, xx, xx, xx, xx, mrm, x, 70},/*95*/
  {E_VEX_EXT, 0x66380c18, STROFF(e_vex_ext_77), xx, xx, xx, xx, xx, mrm, x, 77},/*96*/
  {E_VEX_EXT, 0x66380d18, STROFF(e_vex_ext_78), xx, xx, xx, xx, xx, mrm, x, 78},/*97*/
  /* TBM */
  {PREFIX_EXT, 0x38f718, STROFF(prefix_ext_141), xx, xx, xx, xx, xx, mrm, x, 141},  /*98*/
  /* BMI1 */
  {EXTENSION, 0x38f318, STROFF(group_17), By, xx, Ey, xx, xx, mrm|vex, x, 31},      /*99*/
  /* marked reqp b/c it should have no prefix (prefixes for future opcodes) */
  {OP_andn, 0x38f218, STROFF(andn), Gy, xx, By, Ey, xx, mrm|vex|reqp, fW6, END_LIST},/*100*/
  /* BMI2 */
  {PREFIX_EXT, 0x38f518, STROFF(prefix_ext_142), xx, xx, xx, xx, xx, mrm, x, 142}, /*101*/
  {PREFIX_EXT, 0x38f618, STROFF(prefix_ext_143), xx, xx, xx, xx, xx, mrm, x, 143}, /*102*/
  {OP_invpcid, 0x66388218, STROFF(invpcid),  xx, xx, Gy, Mdq, xx, mrm|reqp, x, END_LIST},/*103*/
  /* AVX2 */
  {E_VEX_EXT,   0x389018, STROFF(e_vex_ext_140), xx, xx, xx, xx, xx, mrm, x, 140},/*104*/
  {E_VEX_EXT,   0x389118, STROFF(e_vex_ext_141), xx, xx, xx, xx, xx, mrm, x, 141},/*105*/
  {E_VEX_EXT,   0x389218, STROFF(e_vex_ext_142), xx, xx, xx, xx, xx, mrm, x, 142},/*106*/
  {E_VEX_EXT,   0x389318, STROFF(e_vex_ext_143), xx, xx, xx, xx, xx, mrm, x, 143},/*107*/
  {E_VEX_EXT, 0x66385a18, STROFF(e_vex_ext_139), xx, xx, xx, xx, xx, mrm, x, 139},/*108*/
  {VEX_W_EXT, 0x66388c18, STROFF(vex_W_ext_70), xx,xx,xx,xx,xx, mrm|vex|reqp, x, 70},/*109*/
  {VEX_W_EXT, 0x66388e18, STROFF(vex_W_ext_71), xx,xx,xx,xx,xx, mrm|vex|reqp, x, 71},/*110*/
  /* Following Intel and not marking as packed float vs ints: just "qq". */
  {E_VEX_EXT, 0x66381618, STROFF(e_vex_ext_124), xx, xx, xx, xx, xx, mrm|reqp, x, 124},/*111*/
  {E_VEX_EXT, 0x66383618, STROFF(e_vex_ext_123), xx, xx, xx, xx, xx, mrm|reqp, x, 123},/*112*/
  {E_VEX_EXT, 0x66384518, STROFF(e_vex_ext_133), xx, xx, xx, xx, xx, mrm|reqp, x, 133},/*113*/
  {E_VEX_EXT, 0x66384618, STROFF(e_vex_ext_131), xx, xx, xx, xx, xx, mrm|reqp, x, 131},/*114*/
  {E_VEX_EXT, 0x66384718, STROFF(e_vex_ext_134), xx, xx, xx, xx, xx, mrm|reqp, x, 134},/*115*/
  {E_VEX_EXT, 0x66387818, STROFF(e_vex_ext_135), xx, xx, xx, xx, xx, mrm|reqp, x, 135},/*116*/
  {E_VEX_EXT, 0x66387918, STROFF(e_vex_ext_136), xx, xx, xx, xx, xx, mrm|reqp, x, 136},/*117*/
  {E_VEX_EXT, 0x66385818, STROFF(e_vex_ext_137), xx, xx, xx, xx, xx, mrm|reqp, x, 137},/*118*/
  {E_VEX_EXT, 0x66385918, STROFF(e_vex_ext_138), xx, xx, xx, xx, xx, mrm|reqp, x, 138},/*119*/
  {EVEX_Wb_EXT, 0x66388d18, STROFF(evex_Wb_ext_92), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 92},/*120*/
  {EVEX_Wb_EXT, 0x66387718, STROFF(evex_Wb_ext_95), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 95},/*121*/
  {EVEX_Wb_EXT, 0x66387618, STROFF(evex_Wb_ext_96), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 96},/*122*/
  {EVEX_Wb_EXT, 0x66387518, STROFF(evex_Wb_ext_97), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 97},/*123*/
  {EVEX_Wb_EXT, 0x66387d18, STROFF(evex_Wb_ext_98), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 98},/*124*/
  {EVEX_Wb_EXT, 0x66387e18, STROFF(evex_Wb_ext_99), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 99},/*125*/
  {EVEX_Wb_EXT, 0x66387f18, STROFF(evex_Wb_ext_100), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 100},/*126*/
  {PREFIX_EXT, 0x381118, STROFF(prefix_ext_171), xx, xx, xx, xx, xx, mrm, x, 171},  /*127*/
  {PREFIX_EXT, 0x381218, STROFF(prefix_ext_162), xx, xx, xx, xx, xx, mrm, x, 162},  /*128*/
  {EVEX_Wb_EXT, 0x66384c18, STROFF(evex_Wb_ext_132), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 132},/*129*/
  {EVEX_Wb_EXT, 0x66384d18, STROFF(evex_Wb_ext_133), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 133},/*130*/
  {E_VEX_EXT, 0x38ca18, STROFF(e_vex_ext_144), xx, xx, xx, xx, xx, mrm|reqp, x, 144},/*131*/
  {E_VEX_EXT, 0x38cb18, STROFF(e_vex_ext_146), xx, xx, xx, xx, xx, mrm|reqp, x, 146},/*132*/
  {EVEX_Wb_EXT, 0x66381f58, STROFF(evex_Wb_ext_147), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 147},/*133*/
  {EVEX_Wb_EXT, 0x66381b18, STROFF(evex_Wb_ext_150), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 150},/*134*/
  {OP_vpbroadcastb, 0x66387a08, STROFF(vpbroadcastb), Ve, xx, KEq, Ed, xx, mrm|evex|reqp|ttt1s, x, END_LIST},/*135*/
  {OP_vpbroadcastw, 0x66387b08, STROFF(vpbroadcastw), Ve, xx, KEd, Ed, xx, mrm|evex|reqp|ttt1s|inopsz2, x, END_LIST},/*136*/
  {EVEX_Wb_EXT, 0x66387c18, STROFF(evex_Wb_ext_151), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 151},/*137*/
  {EVEX_Wb_EXT, 0x66385b18, STROFF(evex_Wb_ext_154), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 154},/*138*/
  {EVEX_Wb_EXT, 0x66386518, STROFF(evex_Wb_ext_156), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 156},/*139*/
  {EVEX_Wb_EXT, 0x66388a18, STROFF(evex_Wb_ext_157), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 157},/*140*/
  {EVEX_Wb_EXT, 0x66388818, STROFF(evex_Wb_ext_158), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 158},/*141*/
  {EVEX_Wb_EXT, 0x66384218, STROFF(evex_Wb_ext_161), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 161},/*142*/
  {EVEX_Wb_EXT, 0x66384318, STROFF(evex_Wb_ext_162), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 162},/*143*/
  {EVEX_Wb_EXT, 0x66386618, STROFF(evex_Wb_ext_165), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 165},/*144*/
  {EVEX_Wb_EXT, 0x66386418, STROFF(evex_Wb_ext_166), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 166},/*145*/
  {EVEX_Wb_EXT, 0x66388b18, STROFF(evex_Wb_ext_167), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 167},/*146*/
  {EVEX_Wb_EXT, 0x66388918, STROFF(evex_Wb_ext_168), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 168},/*147*/
  {PREFIX_EXT, 0x382618, STROFF(prefix_ext_182), xx, xx, xx, xx, xx, mrm, x, 182},/*148*/
  {PREFIX_EXT, 0x382718, STROFF(prefix_ext_183), xx, xx, xx, xx, xx, mrm, x, 183},/*149*/
  {EVEX_Wb_EXT, 0x66384e18, STROFF(evex_Wb_ext_177), xx, xx, xx, xx, xx, mrm|reqp, x, 177},/*150*/
  {EVEX_Wb_EXT, 0x66384f18, STROFF(evex_Wb_ext_178), xx, xx, xx, xx, xx, mrm|reqp, x, 178},/*151*/
  {E_VEX_EXT, 0x38cc18, STROFF(e_vex_ext_147), xx, xx, xx, xx, xx, mrm|reqp, x, 147},/*152*/
  {E_VEX_EXT, 0x38cd18, STROFF(e_vex_ext_148), xx, xx, xx, xx, xx, mrm|reqp, x, 148},/*153*/
  {E_VEX_EXT, 0x38c818, STROFF(e_vex_ext_145), xx, xx, xx, xx, xx, mrm|reqp, x, 145},/*154*/
  {EVEX_Wb_EXT, 0x6638c418, STROFF(evex_Wb_ext_186), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 186},/*155*/
  {EVEX_Wb_EXT, 0x66384418, STROFF(evex_Wb_ext_187), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 187},/*156*/
  {EVEX_Wb_EXT, 0x6638b448, STROFF(evex_Wb_ext_220), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 220},/*157*/
  {EVEX_Wb_EXT, 0x6638b548, STROFF(evex_Wb_ext_234), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 234},/*158*/
  {EVEX_Wb_EXT, 0x6638a018, STROFF(evex_Wb_ext_193), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 193},/*159*/
  {EVEX_Wb_EXT, 0x6638a118, STROFF(evex_Wb_ext_194), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 194},/*160*/
  {EVEX_Wb_EXT, 0x6638a218, STROFF(evex_Wb_ext_195), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 195},/*161*/
  {EVEX_Wb_EXT, 0x6638a318, STROFF(evex_Wb_ext_196), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 196},/*162*/
  {EXTENSION, 0x6638c618, STROFF(group_18), xx, xx, xx, xx, xx, mrm, x, 32},/*163*/
  {EXTENSION, 0x6638c718, STROFF(group_19), xx, xx, xx, xx, xx, mrm, x, 33},/*164*/
  {OP_sha1msg1, 0x38c918, STROFF(sha1msg1), Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},/*165*/
  {E_VEX_EXT, 0x66385008, STROFF(e_vex_ext_149), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 149},/*166*/
  {E_VEX_EXT, 0x66385108, STROFF(e_vex_ext_150), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 150},/*167*/
  {PREFIX_EXT, 0x385208, STROFF(prefix_ext_189), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 189},/*168*/
  {E_VEX_EXT, 0x66385308, STROFF(e_vex_ext_152), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 152},/*169*/
  {PREFIX_EXT, 0x387208, STROFF(prefix_ext_190), xx, xx, xx, xx, xx, mrm|evex, x, 190},/*170*/
  /* AVX512 VPOPCNTDQ */
  {EVEX_Wb_EXT, 0x66385518, STROFF(evex_Wb_ext_274), xx, xx, xx, xx, xx, mrm|evex|reqp, x, 274}/*171*/
};

/* N.B.: every 0x3a instr so far has an immediate.  If a version w/o an immed
 * comes along we'll have to add a threebyte_3a_vex_extra[] table to decode_fast.c.
 */
const byte third_byte_3a_index[256] = {
  /* 0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    59,60,61,77, 28,29,30, 0,  6, 7, 8, 9, 10,11,12, 1,  /* 0 */
     0, 0, 0, 0,  2, 3, 4, 5, 31,32,69,67,  0,33,73,74,  /* 1 */
    13,14,15,75,  0,88,80,81,  0, 0, 0, 0,  0, 0, 0, 0,  /* 2 */
    63,64,65,66,  0, 0, 0, 0, 57,58,70,68,  0, 0,71,72,  /* 3 */
    16,17,18,76, 23, 0,62, 0, 54,55,25,26, 27, 0, 0, 0,  /* 4 */
    82,83, 0, 0, 78,79,84,85,  0, 0, 0, 0, 34,35,36,37,  /* 5 */
    19,20,21,22,  0, 0,86,87, 38,39,40,41, 42,43,44,45,  /* 6 */
     0, 0, 0, 0,  0, 0, 0, 0, 46,47,48,49, 50,51,52,53,  /* 7 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 8 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* 9 */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* A */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* B */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 89, 0, 0, 0,  /* C */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0,24,  /* D */
     0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  /* E */
    56, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0   /* F */
};
const instr_info_t third_byte_3a[] = {
  {INVALID,     0x3aff18, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},                 /* 0*/
  /**** SSSE3 ****/
  {PREFIX_EXT,  0x3a0f18, STROFF(prefix_ext_133), xx, xx, xx, xx, xx, mrm, x, 133},    /* 1*/
  /**** SSE4 ****/
  {E_VEX_EXT,  0x663a1418, STROFF(e_vex_ext_36), xx, xx, xx, xx, xx, mrm, x, 36},/* 2*/
  {E_VEX_EXT,  0x663a1518, STROFF(e_vex_ext_37), xx, xx, xx, xx, xx, mrm, x, 37},/* 3*/
  {E_VEX_EXT,  0x663a1618, STROFF(e_vex_ext_38), xx, xx, xx, xx, xx, mrm, x, 38},/* 4*/
  {E_VEX_EXT,  0x663a1718, STROFF(e_vex_ext_39), xx, xx, xx, xx, xx, mrm, x, 39},/* 5*/
  {E_VEX_EXT,  0x663a0818, STROFF(e_vex_ext_40), xx, xx, xx, xx, xx, mrm, x, 40},/* 6*/
  {E_VEX_EXT,  0x663a0918, STROFF(e_vex_ext_41), xx, xx, xx, xx, xx, mrm, x, 41},/* 7*/
  {E_VEX_EXT,  0x663a0a18, STROFF(e_vex_ext_42), xx, xx, xx, xx, xx, mrm, x, 42},/* 8*/
  {E_VEX_EXT,  0x663a0b18, STROFF(e_vex_ext_43), xx, xx, xx, xx, xx, mrm, x, 43},/* 9*/
  {E_VEX_EXT,  0x663a0c18, STROFF(e_vex_ext_44), xx, xx, xx, xx, xx, mrm, x, 44},/*10*/
  {E_VEX_EXT,  0x663a0d18, STROFF(e_vex_ext_45), xx, xx, xx, xx, xx, mrm, x, 45},/*11*/
  {E_VEX_EXT,  0x663a0e18, STROFF(e_vex_ext_46), xx, xx, xx, xx, xx, mrm, x, 46},/*12*/
  /* 20 */
  {E_VEX_EXT,  0x663a2018, STROFF(e_vex_ext_47), xx, xx, xx, xx, xx, mrm, x, 47},/*13*/
  {E_VEX_EXT,  0x663a2118, STROFF(e_vex_ext_48), xx, xx, xx, xx, xx, mrm, x, 48},/*14*/
  {E_VEX_EXT,  0x663a2218, STROFF(e_vex_ext_49), xx, xx, xx, xx, xx, mrm, x, 49},/*15*/
  /* 40 */
  {E_VEX_EXT,  0x663a4018, STROFF(e_vex_ext_50), xx, xx, xx, xx, xx, mrm, x, 50},/*16*/
  {E_VEX_EXT,  0x663a4118, STROFF(e_vex_ext_51), xx, xx, xx, xx, xx, mrm, x, 51},/*17*/
  {E_VEX_EXT,  0x663a4218, STROFF(e_vex_ext_52), xx, xx, xx, xx, xx, mrm, x, 52},/*18*/
  /* 60 */
  {E_VEX_EXT,  0x663a6018, STROFF(e_vex_ext_53), xx, xx, xx, xx, xx, mrm, x, 53},/*19*/
  {E_VEX_EXT,  0x663a6118, STROFF(e_vex_ext_54), xx, xx, xx, xx, xx, mrm, x, 54},/*20*/
  {E_VEX_EXT,  0x663a6218, STROFF(e_vex_ext_55), xx, xx, xx, xx, xx, mrm, x, 55},/*21*/
  {E_VEX_EXT,  0x663a6318, STROFF(e_vex_ext_56), xx, xx, xx, xx, xx, mrm, x, 56},/*22*/
  {E_VEX_EXT,  0x663a4418, STROFF(e_vex_ext_57), xx, xx, xx, xx, xx, mrm, x, 57},/*23*/
  {E_VEX_EXT,  0x663adf18, STROFF(e_vex_ext_58), xx, xx, xx, xx, xx, mrm, x, 58},/*24*/
  /* AVX overlooked in original pass */
  {E_VEX_EXT,  0x663a4a18, STROFF(e_vex_ext__0), xx, xx, xx, xx, xx, mrm, x,  0},/*25*/
  {E_VEX_EXT,  0x663a4b18, STROFF(e_vex_ext__1), xx, xx, xx, xx, xx, mrm, x,  1},/*26*/
  {E_VEX_EXT,  0x663a4c18, STROFF(e_vex_ext__2), xx, xx, xx, xx, xx, mrm, x,  2},/*27*/
  {E_VEX_EXT,  0x663a0418, STROFF(e_vex_ext_71), xx, xx, xx, xx, xx, mrm, x, 71},/*28*/
  {E_VEX_EXT,  0x663a0518, STROFF(e_vex_ext_72), xx, xx, xx, xx, xx, mrm, x, 72},/*29*/
  {E_VEX_EXT,  0x663a0618, STROFF(e_vex_ext_73), xx, xx, xx, xx, xx, mrm, x, 73},/*30*/
  {E_VEX_EXT,  0x663a1818, STROFF(e_vex_ext_74), xx, xx, xx, xx, xx, mrm, x, 74},/*31*/
  {E_VEX_EXT,  0x663a1918, STROFF(e_vex_ext_75), xx, xx, xx, xx, xx, mrm, x, 75},/*32*/
  {E_VEX_EXT,  0x663a1d18, STROFF(e_vex_ext_76), xx, xx, xx, xx, xx, mrm, x, 76},/*33*/
  /* FMA4 */
  {VEX_W_EXT,0x663a5c18, STROFF(vex_W_ext_30), xx, xx, xx, xx, xx, mrm, x, 30},/*34*/
  {VEX_W_EXT,0x663a5d18, STROFF(vex_W_ext_31), xx, xx, xx, xx, xx, mrm, x, 31},/*35*/
  {VEX_W_EXT,0x663a5e18, STROFF(vex_W_ext_32), xx, xx, xx, xx, xx, mrm, x, 32},/*36*/
  {VEX_W_EXT,0x663a5f18, STROFF(vex_W_ext_33), xx, xx, xx, xx, xx, mrm, x, 33},/*37*/
  {VEX_W_EXT,0x663a6818, STROFF(vex_W_ext_34), xx, xx, xx, xx, xx, mrm, x, 34},/*38*/
  {VEX_W_EXT,0x663a6918, STROFF(vex_W_ext_35), xx, xx, xx, xx, xx, mrm, x, 35},/*39*/
  {VEX_W_EXT,0x663a6a18, STROFF(vex_W_ext_36), xx, xx, xx, xx, xx, mrm, x, 36},/*40*/
  {VEX_W_EXT,0x663a6b18, STROFF(vex_W_ext_37), xx, xx, xx, xx, xx, mrm, x, 37},/*41*/
  {VEX_W_EXT,0x663a6c18, STROFF(vex_W_ext_38), xx, xx, xx, xx, xx, mrm, x, 38},/*42*/
  {VEX_W_EXT,0x663a6d18, STROFF(vex_W_ext_39), xx, xx, xx, xx, xx, mrm, x, 39},/*43*/
  {VEX_W_EXT,0x663a6e18, STROFF(vex_W_ext_40), xx, xx, xx, xx, xx, mrm, x, 40},/*44*/
  {VEX_W_EXT,0x663a6f18, STROFF(vex_W_ext_41), xx, xx, xx, xx, xx, mrm, x, 41},/*45*/
  {VEX_W_EXT,0x663a7818, STROFF(vex_W_ext_42), xx, xx, xx, xx, xx, mrm, x, 42},/*46*/
  {VEX_W_EXT,0x663a7918, STROFF(vex_W_ext_43), xx, xx, xx, xx, xx, mrm, x, 43},/*47*/
  {VEX_W_EXT,0x663a7a18, STROFF(vex_W_ext_44), xx, xx, xx, xx, xx, mrm, x, 44},/*48*/
  {VEX_W_EXT,0x663a7b18, STROFF(vex_W_ext_45), xx, xx, xx, xx, xx, mrm, x, 45},/*49*/
  {VEX_W_EXT,0x663a7c18, STROFF(vex_W_ext_46), xx, xx, xx, xx, xx, mrm, x, 46},/*50*/
  {VEX_W_EXT,0x663a7d18, STROFF(vex_W_ext_47), xx, xx, xx, xx, xx, mrm, x, 47},/*51*/
  {VEX_W_EXT,0x663a7e18, STROFF(vex_W_ext_48), xx, xx, xx, xx, xx, mrm, x, 48},/*52*/
  {VEX_W_EXT,0x663a7f18, STROFF(vex_W_ext_49), xx, xx, xx, xx, xx, mrm, x, 49},/*53*/
  /* XOP */
  {VEX_W_EXT,0x663a4818, STROFF(vex_W_ext_64), xx, xx, xx, xx, xx, mrm, x, 64},/*54*/
  {VEX_W_EXT,0x663a4918, STROFF(vex_W_ext_65), xx, xx, xx, xx, xx, mrm, x, 65},/*55*/
  /* BMI2 */
  {OP_rorx,  0xf23af018, STROFF(rorx),  Gy, xx, Ey, Ib, xx, mrm|vex|reqp, x, END_LIST},/*56*/
  /* AVX2 */
  {E_VEX_EXT, 0x663a3818, STROFF(e_vex_ext_128), xx, xx, xx, xx, xx, mrm, x, 128},/*57*/
  {E_VEX_EXT, 0x663a3918, STROFF(e_vex_ext_127), xx, xx, xx, xx, xx, mrm, x, 127},/*58*/
  {E_VEX_EXT, 0x663a0058, STROFF(e_vex_ext_125), xx, xx, xx, xx, xx, mrm, x, 125},/*59*/
  /* Following Intel and not marking as packed float vs ints: just "qq". */
  {E_VEX_EXT, 0x663a0158, STROFF(e_vex_ext_126), xx, xx, xx, xx, xx, mrm, x, 126},/*60*/
  {OP_vpblendd,0x663a0218,STROFF(vpblendd),Vx,xx,Hx,Wx,Ib, mrm|vex|reqp,x,END_LIST},/*61*/
  {OP_vperm2i128,0x663a4618,STROFF(vperm2i128),Vqq,xx,Hqq,Wqq,Ib, mrm|vex|reqp,x,END_LIST},/*62*/
  /* AVX-512 */
  {VEX_W_EXT, 0x663a3010, STROFF(vex_W_ext_102), xx, xx, xx, xx, xx, mrm|reqp, x, 102 },/*63*/
  {VEX_W_EXT, 0x663a3110, STROFF(vex_W_ext_103), xx, xx, xx, xx, xx, mrm|reqp, x, 103 },/*64*/
  {VEX_W_EXT, 0x663a3210, STROFF(vex_W_ext_100), xx, xx, xx, xx, xx, mrm|reqp, x, 100 },/*65*/
  {VEX_W_EXT, 0x663a3310, STROFF(vex_W_ext_101), xx, xx, xx, xx, xx, mrm|reqp, x, 101 },/*66*/
  {EVEX_Wb_EXT, 0x663a1b18, STROFF(evex_Wb_ext_102), xx, xx, xx, xx, xx, mrm, x, 102},/*67*/
  {EVEX_Wb_EXT, 0x663a3b18, STROFF(evex_Wb_ext_104), xx, xx, xx, xx, xx, mrm, x, 104},/*68*/
  {EVEX_Wb_EXT, 0x663a1a18, STROFF(evex_Wb_ext_106), xx, xx, xx, xx, xx, mrm, x, 106},/*69*/
  {EVEX_Wb_EXT, 0x663a3a18, STROFF(evex_Wb_ext_108), xx, xx, xx, xx, xx, mrm, x, 108},/*70*/
  {EVEX_Wb_EXT, 0x663a3e18, STROFF(evex_Wb_ext_109), xx, xx, xx, xx, xx, mrm, x, 109},/*71*/
  {EVEX_Wb_EXT, 0x663a3f18, STROFF(evex_Wb_ext_110), xx, xx, xx, xx, xx, mrm, x, 110},/*72*/
  {EVEX_Wb_EXT, 0x663a1e18, STROFF(evex_Wb_ext_111), xx, xx, xx, xx, xx, mrm, x, 111},/*73*/
  {EVEX_Wb_EXT, 0x663a1f18, STROFF(evex_Wb_ext_112), xx, xx, xx, xx, xx, mrm, x, 112},/*74*/
  {EVEX_Wb_EXT, 0x663a2318, STROFF(evex_Wb_ext_142), xx, xx, xx, xx, xx, mrm, x, 142},/*75*/
  {EVEX_Wb_EXT, 0x663a4318, STROFF(evex_Wb_ext_143), xx, xx, xx, xx, xx, mrm, x, 143},/*76*/
  {EVEX_Wb_EXT, 0x663a0318, STROFF(evex_Wb_ext_155), xx, xx, xx, xx, xx, mrm, x, 155},/*77*/
  {EVEX_Wb_EXT, 0x663a5418, STROFF(evex_Wb_ext_159), xx, xx, xx, xx, xx, mrm, x, 159},/*78*/
  {EVEX_Wb_EXT, 0x663a5518, STROFF(evex_Wb_ext_160), xx, xx, xx, xx, xx, mrm, x, 160},/*79*/
  {EVEX_Wb_EXT, 0x663a2618, STROFF(evex_Wb_ext_163), xx, xx, xx, xx, xx, mrm, x, 163},/*80*/
  {EVEX_Wb_EXT, 0x663a2718, STROFF(evex_Wb_ext_164), xx, xx, xx, xx, xx, mrm, x, 164},/*81*/
  {EVEX_Wb_EXT, 0x663a5018, STROFF(evex_Wb_ext_173), xx, xx, xx, xx, xx, mrm, x, 173},/*82*/
  {EVEX_Wb_EXT, 0x663a5118, STROFF(evex_Wb_ext_174), xx, xx, xx, xx, xx, mrm, x, 174},/*83*/
  {EVEX_Wb_EXT, 0x663a5618, STROFF(evex_Wb_ext_175), xx, xx, xx, xx, xx, mrm, x, 175},/*84*/
  {EVEX_Wb_EXT, 0x663a5718, STROFF(evex_Wb_ext_176), xx, xx, xx, xx, xx, mrm, x, 176},/*85*/
  {EVEX_Wb_EXT, 0x663a6618, STROFF(evex_Wb_ext_183), xx, xx, xx, xx, xx, mrm, x, 183},/*86*/
  {EVEX_Wb_EXT, 0x663a6718, STROFF(evex_Wb_ext_184), xx, xx, xx, xx, xx, mrm, x, 184},/*87*/
  {EVEX_Wb_EXT, 0x663a2518, STROFF(evex_Wb_ext_188), xx, xx, xx, xx, xx, mrm, x, 188},/*88*/
  /* SHA */
  {OP_sha1rnds4, 0x3acc18, STROFF(sha1rnds4), Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},/*89*/
};

/****************************************************************************
 * Instructions that differ depending on vex.W
 * Index is vex.W value
 */
const instr_info_t vex_W_extensions[][2] = {
  {    /* vex_W_ext 0 */
    {OP_vfmadd132ps,0x66389818,STROFF(vfmadd132ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[62][0]},
    {OP_vfmadd132pd,0x66389858,STROFF(vfmadd132pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[62][2]},
  }, { /* vex_W_ext 1 */
    {OP_vfmadd213ps,0x6638a818,STROFF(vfmadd213ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[63][0]},
    {OP_vfmadd213pd,0x6638a858,STROFF(vfmadd213pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[63][2]},
  }, { /* vex_W_ext 2 */
    {OP_vfmadd231ps,0x6638b818,STROFF(vfmadd231ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[64][0]},
    {OP_vfmadd231pd,0x6638b858,STROFF(vfmadd231pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[64][2]},
  }, { /* vex_W_ext 3 */
    {OP_vfmadd132ss,0x66389918,STROFF(vfmadd132ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[65][0]},
    {OP_vfmadd132sd,0x66389958,STROFF(vfmadd132sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[65][2]},
  }, { /* vex_W_ext 4 */
    {OP_vfmadd213ss,0x6638a918,STROFF(vfmadd213ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[66][0]},
    {OP_vfmadd213sd,0x6638a958,STROFF(vfmadd213sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[66][2]},
  }, { /* vex_W_ext 5 */
    {OP_vfmadd231ss,0x6638b918,STROFF(vfmadd231ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[67][0]},
    {OP_vfmadd231sd,0x6638b958,STROFF(vfmadd231sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[67][2]},
  }, { /* vex_W_ext 6 */
    {OP_vfmaddsub132ps,0x66389618,STROFF(vfmaddsub132ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[68][0]},
    {OP_vfmaddsub132pd,0x66389658,STROFF(vfmaddsub132pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[68][2]},
  }, { /* vex_W_ext 7 */
    {OP_vfmaddsub213ps,0x6638a618,STROFF(vfmaddsub213ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[69][0]},
    {OP_vfmaddsub213pd,0x6638a658,STROFF(vfmaddsub213pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[69][2]},
  }, { /* vex_W_ext 8 */
    {OP_vfmaddsub231ps,0x6638b618,STROFF(vfmaddsub231ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[70][0]},
    {OP_vfmaddsub231pd,0x6638b658,STROFF(vfmaddsub231pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[70][2]},
  }, { /* vex_W_ext 9 */
    {OP_vfmsubadd132ps,0x66389718,STROFF(vfmsubadd132ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[71][0]},
    {OP_vfmsubadd132pd,0x66389758,STROFF(vfmsubadd132pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[71][2]},
  }, { /* vex_W_ext 10 */
    {OP_vfmsubadd213ps,0x6638a718,STROFF(vfmsubadd213ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[72][0]},
    {OP_vfmsubadd213pd,0x6638a758,STROFF(vfmsubadd213pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[72][2]},
  }, { /* vex_W_ext 11 */
    {OP_vfmsubadd231ps,0x6638b718,STROFF(vfmsubadd231ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[73][0]},
    {OP_vfmsubadd231pd,0x6638b758,STROFF(vfmsubadd231pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[73][2]},
  }, { /* vex_W_ext 12 */
    {OP_vfmsub132ps,0x66389a18,STROFF(vfmsub132ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[74][0]},
    {OP_vfmsub132pd,0x66389a58,STROFF(vfmsub132pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[74][2]},
  }, { /* vex_W_ext 13 */
    {OP_vfmsub213ps,0x6638aa18,STROFF(vfmsub213ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[75][0]},
    {OP_vfmsub213pd,0x6638aa58,STROFF(vfmsub213pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[75][2]},
  }, { /* vex_W_ext 14 */
    {OP_vfmsub231ps,0x6638ba18,STROFF(vfmsub231ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[76][0]},
    {OP_vfmsub231pd,0x6638ba58,STROFF(vfmsub231pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[76][2]},
  }, { /* vex_W_ext 15 */
    {OP_vfmsub132ss,0x66389b18,STROFF(vfmsub132ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[77][0]},
    {OP_vfmsub132sd,0x66389b58,STROFF(vfmsub132sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[77][2]},
  }, { /* vex_W_ext 16 */
    {OP_vfmsub213ss,0x6638ab18,STROFF(vfmsub213ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[78][0]},
    {OP_vfmsub213sd,0x6638ab58,STROFF(vfmsub213sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[78][2]},
  }, { /* vex_W_ext 17 */
    {OP_vfmsub231ss,0x6638bb18,STROFF(vfmsub231ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[79][0]},
    {OP_vfmsub231sd,0x6638bb58,STROFF(vfmsub231sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[79][2]},
  }, { /* vex_W_ext 18 */
    {OP_vfnmadd132ps,0x66389c18,STROFF(vfnmadd132ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[80][0]},
    {OP_vfnmadd132pd,0x66389c58,STROFF(vfnmadd132pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[80][2]},
  }, { /* vex_W_ext 19 */
    {OP_vfnmadd213ps,0x6638ac18,STROFF(vfnmadd213ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[81][0]},
    {OP_vfnmadd213pd,0x6638ac58,STROFF(vfnmadd213pd),Vvd,xx,Hvd,Wvd,Vvs,mrm|vex|reqp,x,tevexwb[81][2]},
  }, { /* vex_W_ext 20 */
    {OP_vfnmadd231ps,0x6638bc18,STROFF(vfnmadd231ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[82][0]},
    {OP_vfnmadd231pd,0x6638bc58,STROFF(vfnmadd231pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[82][2]},
  }, { /* vex_W_ext 21 */
    {OP_vfnmadd132ss,0x66389d18,STROFF(vfnmadd132ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[83][0]},
    {OP_vfnmadd132sd,0x66389d58,STROFF(vfnmadd132sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[83][2]},
  }, { /* vex_W_ext 22 */
    {OP_vfnmadd213ss,0x6638ad18,STROFF(vfnmadd213ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[84][0]},
    {OP_vfnmadd213sd,0x6638ad58,STROFF(vfnmadd213sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[84][2]},
  }, { /* vex_W_ext 23 */
    {OP_vfnmadd231ss,0x6638bd18,STROFF(vfnmadd231ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[85][0]},
    {OP_vfnmadd231sd,0x6638bd58,STROFF(vfnmadd231sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[85][2]},
  }, { /* vex_W_ext 24 */
    {OP_vfnmsub132ps,0x66389e18,STROFF(vfnmsub132ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[86][0]},
    {OP_vfnmsub132pd,0x66389e58,STROFF(vfnmsub132pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[86][2]},
  }, { /* vex_W_ext 25 */
    {OP_vfnmsub213ps,0x6638ae18,STROFF(vfnmsub213ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[87][0]},
    {OP_vfnmsub213pd,0x6638ae58,STROFF(vfnmsub213pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[87][2]},
  }, { /* vex_W_ext 26 */
    {OP_vfnmsub231ps,0x6638be18,STROFF(vfnmsub231ps),Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[88][0]},
    {OP_vfnmsub231pd,0x6638be58,STROFF(vfnmsub231pd),Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[88][2]},
  }, { /* vex_W_ext 27 */
    {OP_vfnmsub132ss,0x66389f18,STROFF(vfnmsub132ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[89][0]},
    {OP_vfnmsub132sd,0x66389f58,STROFF(vfnmsub132sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[89][2]},
  }, { /* vex_W_ext 28 */
    {OP_vfnmsub213ss,0x6638af18,STROFF(vfnmsub213ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[90][0]},
    {OP_vfnmsub213sd,0x6638af58,STROFF(vfnmsub213sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[90][2]},
  }, { /* vex_W_ext 29 */
    {OP_vfnmsub231ss,0x6638bf18,STROFF(vfnmsub231ss),Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[91][0]},
    {OP_vfnmsub231sd,0x6638bf58,STROFF(vfnmsub231sd),Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[91][2]},
  }, { /* vex_W_ext 30 */
    {OP_vfmaddsubps,0x663a5c18,STROFF(vfmaddsubps),Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[30][1]},
    {OP_vfmaddsubps,0x663a5c58,STROFF(vfmaddsubps),Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 31 */
    {OP_vfmaddsubpd,0x663a5d18,STROFF(vfmaddsubpd),Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[31][1]},
    {OP_vfmaddsubpd,0x663a5d58,STROFF(vfmaddsubpd),Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 32 */
    {OP_vfmsubaddps,0x663a5e18,STROFF(vfmsubaddps),Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[32][1]},
    {OP_vfmsubaddps,0x663a5e58,STROFF(vfmsubaddps),Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 33 */
    {OP_vfmsubaddpd,0x663a5f18,STROFF(vfmsubaddpd),Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[33][1]},
    {OP_vfmsubaddpd,0x663a5f58,STROFF(vfmsubaddpd),Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 34 */
    {OP_vfmaddps,0x663a6818,STROFF(vfmaddps),Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[34][1]},
    {OP_vfmaddps,0x663a6858,STROFF(vfmaddps),Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 35 */
    {OP_vfmaddpd,0x663a6918,STROFF(vfmaddpd),Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[35][1]},
    {OP_vfmaddpd,0x663a6958,STROFF(vfmaddpd),Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 36 */
    {OP_vfmaddss,0x663a6a18,STROFF(vfmaddss),Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[36][1]},
    {OP_vfmaddss,0x663a6a58,STROFF(vfmaddss),Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 37 */
    {OP_vfmaddsd,0x663a6b18,STROFF(vfmaddsd),Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[37][1]},
    {OP_vfmaddsd,0x663a6b58,STROFF(vfmaddsd),Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 38 */
    {OP_vfmsubps,0x663a6c18,STROFF(vfmsubps),Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[38][1]},
    {OP_vfmsubps,0x663a6c58,STROFF(vfmsubps),Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 39 */
    {OP_vfmsubpd,0x663a6d18,STROFF(vfmsubpd),Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[39][1]},
    {OP_vfmsubpd,0x663a6d58,STROFF(vfmsubpd),Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 40 */
    {OP_vfmsubss,0x663a6e18,STROFF(vfmsubss),Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[40][1]},
    {OP_vfmsubss,0x663a6e58,STROFF(vfmsubss),Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 41 */
    {OP_vfmsubsd,0x663a6f18,STROFF(vfmsubsd),Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[41][1]},
    {OP_vfmsubsd,0x663a6f58,STROFF(vfmsubsd),Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 42 */
    {OP_vfnmaddps,0x663a7818,STROFF(vfnmaddps),Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[42][1]},
    {OP_vfnmaddps,0x663a7858,STROFF(vfnmaddps),Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 43 */
    {OP_vfnmaddpd,0x663a7918,STROFF(vfnmaddpd),Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[43][1]},
    {OP_vfnmaddpd,0x663a7958,STROFF(vfnmaddpd),Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 44 */
    {OP_vfnmaddss,0x663a7a18,STROFF(vfnmaddss),Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[44][1]},
    {OP_vfnmaddss,0x663a7a58,STROFF(vfnmaddss),Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 45 */
    {OP_vfnmaddsd,0x663a7b18,STROFF(vfnmaddsd),Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[45][1]},
    {OP_vfnmaddsd,0x663a7b58,STROFF(vfnmaddsd),Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 46 */
    {OP_vfnmsubps,0x663a7c18,STROFF(vfnmsubps),Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[46][1]},
    {OP_vfnmsubps,0x663a7c58,STROFF(vfnmsubps),Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 47 */
    {OP_vfnmsubpd,0x663a7d18,STROFF(vfnmsubpd),Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[47][1]},
    {OP_vfnmsubpd,0x663a7d58,STROFF(vfnmsubpd),Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 48 */
    {OP_vfnmsubss,0x663a7e18,STROFF(vfnmsubss),Vdq,xx,Lss,Wss,Hss,mrm|vex|reqp,x,tvexw[48][1]},
    {OP_vfnmsubss,0x663a7e58,STROFF(vfnmsubss),Vdq,xx,Lss,Hss,Wss,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 49 */
    {OP_vfnmsubsd,0x663a7f18,STROFF(vfnmsubsd),Vdq,xx,Lsd,Wsd,Hsd,mrm|vex|reqp,x,tvexw[49][1]},
    {OP_vfnmsubsd,0x663a7f58,STROFF(vfnmsubsd),Vdq,xx,Lsd,Hsd,Wsd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 50 */
    {OP_vpcmov,    0x08a218,STROFF(vpcmov),    Vvs,xx,Hvs,Wvs,Lvs,mrm|vex,x,tvexw[50][1]},
    {OP_vpcmov,    0x08a258,STROFF(vpcmov),    Vvs,xx,Hvs,Lvs,Wvs,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 51 */
    {OP_vpperm,    0x08a318,STROFF(vpperm),    Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,tvexw[51][1]},
    {OP_vpperm,    0x08a358,STROFF(vpperm),    Vdq,xx,Hdq,Ldq,Wdq,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 52 */
    {OP_vprotb,    0x099018,STROFF(vprotb),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[52][1]},
    {OP_vprotb,    0x099058,STROFF(vprotb),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 53 */
    {OP_vprotw,    0x099118,STROFF(vprotw),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[53][1]},
    {OP_vprotw,    0x099158,STROFF(vprotw),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 54 */
    {OP_vprotd,    0x099218,STROFF(vprotd),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[54][1]},
    {OP_vprotd,    0x099258,STROFF(vprotd),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 55 */
    {OP_vprotq,    0x099318,STROFF(vprotq),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[55][1]},
    {OP_vprotq,    0x099358,STROFF(vprotq),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 56 */
    {OP_vpshlb,    0x099418,STROFF(vpshlb),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[56][1]},
    {OP_vpshlb,    0x099458,STROFF(vpshlb),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 57 */
    {OP_vpshlw,    0x099518,STROFF(vpshlw),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[57][1]},
    {OP_vpshlw,    0x099558,STROFF(vpshlw),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 58 */
    {OP_vpshld,    0x099618,STROFF(vpshld),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[58][1]},
    {OP_vpshld,    0x099658,STROFF(vpshld),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 59 */
    {OP_vpshlq,    0x099718,STROFF(vpshlq),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[59][1]},
    {OP_vpshlq,    0x099758,STROFF(vpshlq),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 60 */
    {OP_vpshab,    0x099818,STROFF(vpshab),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[60][1]},
    {OP_vpshab,    0x099858,STROFF(vpshab),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 61 */
    {OP_vpshaw,    0x099918,STROFF(vpshaw),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[61][1]},
    {OP_vpshaw,    0x099958,STROFF(vpshaw),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 62 */
    {OP_vpshad,    0x099a18,STROFF(vpshad),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[62][1]},
    {OP_vpshad,    0x099a58,STROFF(vpshad),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 63 */
    {OP_vpshaq,    0x099b18,STROFF(vpshaq),    Vdq,xx,Wdq,Hdq,xx,mrm|vex,x,tvexw[63][1]},
    {OP_vpshaq,    0x099b58,STROFF(vpshaq),    Vdq,xx,Hdq,Wdq,xx,mrm|vex,x,END_LIST},
  }, { /* vex_W_ext 64 */
    {OP_vpermil2ps,0x663a4818,STROFF(vpermil2ps),Vvs,xx,Hvs,Wvs,Lvs,mrm|vex|reqp,x,tvexw[64][1]},
    {OP_vpermil2ps,0x663a4858,STROFF(vpermil2ps),Vvs,xx,Hvs,Lvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 65 */
    {OP_vpermil2pd,0x663a4918,STROFF(vpermil2pd),Vvs,xx,Hvs,Wvs,Lvs,mrm|vex|reqp,x,tvexw[65][1]},
    {OP_vpermil2pd,0x663a4958,STROFF(vpermil2pd),Vvs,xx,Hvs,Lvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 66 */
    /* XXX: OP_v*gather* raise #UD if any pair of the index, mask, or destination
     * registers are identical.  We don't bother trying to detect that.
     */
    {OP_vpgatherdd,0x66389018,STROFF(vpgatherdd),Vx,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[189][0]},
    {OP_vpgatherdq,0x66389058,STROFF(vpgatherdq),Vx,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[189][2]},
  }, { /* vex_W_ext 67 */
    {OP_vpgatherqd,0x66389118,STROFF(vpgatherqd),Vx,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[190][0]},
    {OP_vpgatherqq,0x66389158,STROFF(vpgatherqq),Vx,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[190][2]},
  }, { /* vex_W_ext 68 */
    {OP_vgatherdps,0x66389218,STROFF(vgatherdps),Vvs,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[191][0]},
    {OP_vgatherdpd,0x66389258,STROFF(vgatherdpd),Vvd,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[191][2]},
  }, { /* vex_W_ext 69 */
    {OP_vgatherqps,0x66389318,STROFF(vgatherqps),Vvs,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[192][0]},
    {OP_vgatherqpd,0x66389358,STROFF(vgatherqpd),Vvd,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[192][2]},
  }, { /* vex_W_ext 70 */
    {OP_vpmaskmovd,0x66388c18,STROFF(vpmaskmovd),Vx,xx,Hx,Mx,xx, mrm|vex|reqp|predcx,x,tvexw[71][0]},
    {OP_vpmaskmovq,0x66388c58,STROFF(vpmaskmovq),Vx,xx,Hx,Mx,xx, mrm|vex|reqp|predcx,x,tvexw[71][1]},
  }, { /* vex_W_ext 71 */
    /* Conditional store => predcx */
    {OP_vpmaskmovd,0x66388e18,STROFF(vpmaskmovd),Mx,xx,Vx,Hx,xx, mrm|vex|reqp|predcx,x,END_LIST},
    {OP_vpmaskmovq,0x66388e58,STROFF(vpmaskmovq),Mx,xx,Vx,Hx,xx, mrm|vex|reqp|predcx,x,END_LIST},
  }, { /* vex_W_ext 72 */
    {OP_vpsrlvd,0x66384518,STROFF(vpsrlvd),Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[129][0]},
    {OP_vpsrlvq,0x66384558,STROFF(vpsrlvq),Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[129][2]},
  }, { /* vex_W_ext 73 */
    {OP_vpsllvd,0x66384718,STROFF(vpsllvd),Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[131][0]},
    {OP_vpsllvq,0x66384758,STROFF(vpsllvq),Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[131][2]},
  }, { /* vex_W_ext 74 */
    {OP_kmovw,0x0f9010,STROFF(kmovw),KPw,xx,KQw,xx,xx, mrm|vex|reqL0,x,tvexw[76][0]},
    {OP_kmovq,0x0f9050,STROFF(kmovq),KPq,xx,KQq,xx,xx, mrm|vex|reqL0,x,tvexw[76][1]},
  }, { /* vex_W_ext 75 */
    {OP_kmovb,0x660f9010,STROFF(kmovb),KPb,xx,KQb,xx,xx, mrm|vex|reqL0,x,tvexw[77][0]},
    {OP_kmovd,0x660f9050,STROFF(kmovd),KPd,xx,KQd,xx,xx, mrm|vex|reqL0,x,tvexw[77][1]},
  }, { /* vex_W_ext 76 */
    {OP_kmovw,0x0f9110,STROFF(kmovw),KQw,xx,KPw,xx,xx, mrm|vex|reqL0,x,tvexw[78][0]},
    {OP_kmovq,0x0f9150,STROFF(kmovq),KQq,xx,KPq,xx,xx, mrm|vex|reqL0,x,tvexw[106][1]},
  }, { /* vex_W_ext 77 */
    {OP_kmovb,0x660f9110,STROFF(kmovb),KQb,xx,KPb,xx,xx, mrm|vex|reqL0,x,tvexw[79][0]},
    {OP_kmovd,0x660f9150,STROFF(kmovd),KQd,xx,KPd,xx,xx, mrm|vex|reqL0,x,tvexw[106][0]},
  }, { /* vex_W_ext 78 */
    {OP_kmovw,0x0f9210,STROFF(kmovw),KPw,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[80][0]},
    {INVALID, 0x0f9250,STROFF(BAD), xx,xx,xx,xx,xx,      no,x,NA},
  }, { /* vex_W_ext 79 */
    {OP_kmovb,0x660f9210,STROFF(kmovb),KPb,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[81][0]},
    {INVALID, 0x660f9250,STROFF(BAD), xx,xx,xx,xx,xx,      no,x,NA},
  }, { /* vex_W_ext 80 */
    {OP_kmovw,0x0f9310,STROFF(kmovw),  Gd,xx,KRw,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {INVALID, 0x0f9450,STROFF(BAD), xx,xx,xx,xx,xx,        no,x,NA},
  }, { /* vex_W_ext 81 */
    {OP_kmovb,0x660f9310,STROFF(kmovb),Gd,xx,KRb,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {INVALID, 0x660f9350,STROFF(BAD),xx,xx,xx,xx,xx,       no,x,NA},
  }, { /* vex_W_ext 82 */
    {OP_kandw,0x0f4110,STROFF(kandw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandq,0x0f4150,STROFF(kandq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 83 */
    {OP_kandb,0x660f4110,STROFF(kandb),KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandd,0x660f4150,STROFF(kandd),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 84 */
    {OP_kandnw,0x0f4210,STROFF(kandnw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandnq,0x0f4250,STROFF(kandnq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 85 */
    {OP_kandnb,0x660f4210,STROFF(kandnb),KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandnd,0x660f4250,STROFF(kandnd),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 86 */
    {OP_kunpckwd,0x0f4b10,STROFF(kunpckwd),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kunpckdq,0x0f4b50,STROFF(kunpckdq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 87 */
    {OP_kunpckbw,0x660f4b10,STROFF(kunpckbw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {INVALID,    0x660f4b50,   STROFF(BAD), xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 88 */
    {OP_knotw,0x0f4410,STROFF(knotw),KPw,xx,KRw,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_knotq,0x0f4450,STROFF(knotq),KPq,xx,KRq,xx,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 89 */
    {OP_knotb,0x660f4410,STROFF(knotb),KPb,xx,KRb,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_knotd,0x660f4450,STROFF(knotd),KPd,xx,KRd,xx,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 90 */
    {OP_korw,0x0f4510,STROFF(korw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_korq,0x0f4550,STROFF(korq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 91 */
    {OP_korb,0x660f4510,STROFF(korb),KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kord,0x660f4550,STROFF(kord),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 92 */
    {OP_kxnorw,0x0f4610,STROFF(kxnorw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxnorq,0x0f4650,STROFF(kxnorq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 93 */
    {OP_kxnorb,0x660f4610,STROFF(kxnorb),KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxnord,0x660f4650,STROFF(kxnord),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 94 */
    {OP_kxorw,0x0f4710,STROFF(kxorw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxorq,0x0f4750,STROFF(kxorq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 95 */
    {OP_kxorb,0x660f4710,STROFF(kxorb),KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxord,0x660f4750,STROFF(kxord),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 96 */
    {OP_kaddw,0x0f4a10,STROFF(kaddw),KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kaddq,0x0f4a50,STROFF(kaddq),KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 97 */
    {OP_kaddb,0x660f4a10,STROFF(kaddb),KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kaddd,0x660f4a50,STROFF(kaddd),KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 98 */
    {OP_kortestw,0x0f9810,STROFF(kortestw),KPw,xx,KRw,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
    {OP_kortestq,0x0f9850,STROFF(kortestq),KPq,xx,KRq,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
  }, { /* vex_W_ext 99 */
    {OP_kortestb,0x660f9810,STROFF(kortestb),KPb,xx,KRb,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
    {OP_kortestd,0x660f9850,STROFF(kortestd),KPd,xx,KRd,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
  }, { /* vex_W_ext 100 */
    {OP_kshiftlb,0x663a3208,STROFF(kshiftlb),KPb,xx,KRb,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftlw,0x663a3248,STROFF(kshiftlw),KPw,xx,KRw,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 101 */
    {OP_kshiftld,0x663a3308,STROFF(kshiftld),KPd,xx,KRd,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftlq,0x663a3348,STROFF(kshiftlq),KPq,xx,KRq,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 102 */
    {OP_kshiftrb,0x663a3008,STROFF(kshiftrb),KPb,xx,KRb,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftrw,0x663a3048,STROFF(kshiftrw),KPw,xx,KRw,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 103 */
    {OP_kshiftrd,0x663a3108,STROFF(kshiftrd),KPd,xx,KRd,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftrq,0x663a3148,STROFF(kshiftrq),KPq,xx,KRq,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 104 */
    {OP_ktestw,0x0f9910,STROFF(ktestw),KPw,xx,KRw,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
    {OP_ktestq,0x0f9950,STROFF(ktestq),KPq,xx,KRq,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
  }, { /* vex_W_ext 105 */
    {OP_ktestb,0x660f9910,STROFF(ktestb),KPb,xx,KRb,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
    {OP_ktestd,0x660f9950,STROFF(ktestd),KPd,xx,KRd,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
  }, { /* vex_W_ext 106 */
    {OP_kmovd,0xf20f9210,STROFF(kmovd),KPd,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[107][0]},
    {OP_kmovq,0xf20f9250,STROFF(kmovq),KPq,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[107][1]},
  }, { /* vex_W_ext 107 */
    {OP_kmovd,0xf20f9310,STROFF(kmovd),  Gd,xx,KRd,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kmovq,0xf20f9350,STROFF(kmovq),Gy,xx,KRq,xx,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 108 */
    {OP_vmovd, 0x660f6e10, STROFF(vmovd), Vdq, xx, Ed, xx, xx, mrm|vex, x, tvexw[109][0]},
    {OP_vmovq, 0x660f6e50, STROFF(vmovq), Vdq, xx, Ey, xx, xx, mrm|vex, x, tvexw[109][1]},
  }, { /* vex_W_ext 109 */
    {OP_vmovd, 0x660f7e10, STROFF(vmovd), Ed, xx, Vd_dq, xx, xx, mrm|vex, x, tevexwb[136][0]},
    {OP_vmovq, 0x660f7e50, STROFF(vmovq), Ey, xx, Vq_dq, xx, xx, mrm|vex, x, tevexwb[136][2]},
  }, { /* vex_W_ext 110 */
    {OP_vpdpbusd, 0x66385008, STROFF(vpdpbusd), Ve, xx, He, We, xx, mrm|vex|ttfvm|reqp, x, tevexwb[267][0]},
    {INVALID,    0x663850,   STROFF(BAD), xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 111 */
    {OP_vpdpbusds, 0x66385108, STROFF(vpdpbusds), Ve, xx, He, We, xx, mrm|vex|ttfvm|reqp, x, tevexwb[268][0]},
    {INVALID,    0x663850,   STROFF(BAD), xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 112 */
    {OP_vpdpwssd, 0x66385208, STROFF(vpdpwssd), Ve, xx, He, We, xx, mrm|vex|ttfvm, x, tevexwb[269][0]},
    {INVALID,    0x663850,   STROFF(BAD), xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 113 */
    {OP_vpdpwssds, 0x66385308, STROFF(vpdpwssds), Ve, xx, He, We, xx, mrm|vex|ttfvm|reqp, x, tevexwb[270][0]},
    {INVALID,    0x663850,   STROFF(BAD), xx,xx, xx,  xx,xx,     no,x,NA},
  },
};

/****************************************************************************
 * Instructions that differ depending on evex.W and evex.b
 * Index is evex.W value * 2 + evex.b value
 */
const instr_info_t evex_Wb_extensions[][4] = {
  {    /* evex_W_ext 0 */
    {OP_vmovups, 0x0f1000,STROFF(vmovups), Ves,xx,KEd,Wes,xx,mrm|evex|ttfvm,x,tevexwb[1][0]},
    {INVALID, 0x0f1010,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1040,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1050,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 1 */
    {OP_vmovups, 0x0f1100,STROFF(vmovups), Wes,xx,KEd,Ves,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x0f1110,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1140,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1150,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 2 */
    {INVALID, 0x660f1000,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1010,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovupd, 0x660f1040,STROFF(vmovupd), Ved,xx,KEd,Wed,xx,mrm|evex|ttfvm,x,tevexwb[3][2]},
    {INVALID, 0x660f1050,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 3 */
    {INVALID, 0x660f1100,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1110,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovupd, 0x660f1140,STROFF(vmovupd), Wed,xx,KEd,Ved,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f1150,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 4 */
    {OP_vmovaps, 0x0f2800,STROFF(vmovaps), Ves,xx,KEd,Wes,xx,mrm|evex|ttfvm,x,tevexwb[5][0]},
    {INVALID, 0x0f2810,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2840,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2850,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 5 */
    {OP_vmovaps, 0x0f2900,STROFF(vmovaps), Wes,xx,KEd,Ves,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x0f2910,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2940,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2950,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 6 */
    {INVALID, 0x660f2800,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2810,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovapd, 0x660f2840,STROFF(vmovapd), Ved,xx,KEd,Wed,xx,mrm|evex|ttfvm,x,tevexwb[7][2]},
    {INVALID, 0x660f2850,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 7 */
    {INVALID, 0x660f2900,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2910,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovapd, 0x660f2940,STROFF(vmovapd), Wed,xx,KEd,Ved,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f2950,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 8 */
    {OP_vmovdqa32, 0x660f6f00,STROFF(vmovdqa32),Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[9][0]},
    {INVALID, 0x660f6f10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqa64, 0x660f6f40,STROFF(vmovdqa64),Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[9][2]},
    {INVALID, 0x660f6f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 9 */
    {OP_vmovdqa32, 0x660f7f00,STROFF(vmovdqa32),We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f7f10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqa64, 0x660f7f40,STROFF(vmovdqa64),We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f7f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 10 */
    {OP_vmovdqu8, 0xf20f6f00,STROFF(vmovdqu8),Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[12][0]},
    {INVALID, 0xf20f6f10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu16, 0xf20f6f40,STROFF(vmovdqu16),Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[12][2]},
    {INVALID, 0xf20f6f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 11 */
    {OP_vmovdqu32, 0xf30f6f00,STROFF(vmovdqu32),Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[13][0]},
    {INVALID, 0xf30f6f10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu64, 0xf30f6f40,STROFF(vmovdqu64),Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[13][2]},
    {INVALID, 0xf30f6f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 12 */
    {OP_vmovdqu8, 0xf20f7f00,STROFF(vmovdqu8),We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf20f7f10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu16, 0xf20f7f40,STROFF(vmovdqu16),We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf20f7f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 13 */
    {OP_vmovdqu32, 0xf30f7f00,STROFF(vmovdqu32),We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf30f7f10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu64, 0xf30f7f40,STROFF(vmovdqu64),We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf30f7f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 14 */
    {OP_vmovlps, 0x0f1200, STROFF(vmovlps), Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|evex|reqL0|reqLL0|ttt2, x, tevexwb[15][0]}, /*"vmovhlps" if reg-reg */
    {INVALID, 0x0f1210,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1240,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1250,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 15 */
    {OP_vmovlps, 0x0f1300, STROFF(vmovlps), Mq, xx, Vq_dq, xx, xx, mrm|evex|ttt2, x, END_LIST},
    {INVALID, 0x0f1310,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1340,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1350,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 16 */
    {INVALID, 0x660f1200,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1210,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovlpd, 0x660f1240, STROFF(vmovlpd), Vq_dq, xx, Hq_dq, Mq, xx, mrm|evex|reqL0|reqLL0|ttt1s, x, tevexwb[17][2]},
    {INVALID, 0x660f1250,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 17 */
    {INVALID, 0x660f1300,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1310,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovlpd, 0x660f1340, STROFF(vmovlpd), Mq, xx, Vq_dq, xx, xx, mrm|evex|ttt1s, x, END_LIST},
    {INVALID, 0x660f1350,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 18 */
    {OP_vmovsldup, 0xf30f1200, STROFF(vmovsldup), Ves, xx, KEw, Wes, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf30f1210,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1240,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1250,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 19 */
    {INVALID, 0xf20f1200,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf20f1210,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovddup, 0xf20f1240, STROFF(vmovddup), Ved, xx, KEb, We, xx, mrm|evex|ttdup, x, END_LIST},
    {INVALID, 0xf20f1250,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 20 */
    {OP_vmovhps, 0x0f1600, STROFF(vmovhps), Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|evex|reqL0|reqLL0|ttt2, x, tevexwb[21][0]}, /*"vmovlhps" if reg-reg */
    {INVALID, 0x0f1610,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1640,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1650,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 21 */
    {OP_vmovhps, 0x0f1700, STROFF(vmovhps), Mq, xx, Vq_dq, xx, xx, mrm|evex|reqL0|reqLL0|ttt2, x, END_LIST},
    {INVALID, 0x0f1710,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1740,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1750,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 22 */
    {INVALID, 0x660f1600,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1610,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovhpd, 0x660f1640, STROFF(vmovhpd), Vq_dq, xx, Hq_dq, Mq, xx, mrm|evex|reqL0|reqLL0|ttt1s, x, tevexwb[23][2]},
    {INVALID, 0x660f1650,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 23 */
    {INVALID, 0x660f1700,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1710,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovhpd, 0x660f1740, STROFF(vmovhpd), Mq, xx, Vq_dq, xx, xx, mrm|evex|reqL0|reqLL0|ttt1s, x, END_LIST},
    {INVALID, 0x660f1750,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 24 */
    {OP_vmovshdup, 0xf30f1600, STROFF(vmovshdup), Ves, xx, KEw, Wes, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf30f1610,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1640,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1650,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 25 */
    {OP_vunpcklps, 0x0f1400, STROFF(vunpcklps), Ves, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[25][1]},
    {OP_vunpcklps, 0x0f1410, STROFF(vunpcklps), Ves, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x0f1440,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1450,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 26 */
    {INVALID, 0x660f1400,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1410,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vunpcklpd, 0x660f1440, STROFF(vunpcklpd), Ved, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[26][3]},
    {OP_vunpcklpd, 0x660f1450, STROFF(vunpcklpd), Ved, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 27 */
    {OP_vunpckhps, 0x0f1500, STROFF(vunpckhps), Ves, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[27][1]},
    {OP_vunpckhps, 0x0f1510, STROFF(vunpckhps), Ves, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x0f1540,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1550,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 28 */
    {INVALID, 0x660f1500,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1510,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vunpckhpd, 0x660f1540, STROFF(vunpckhpd), Ved, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[28][3]},
    {OP_vunpckhpd, 0x660f1550, STROFF(vunpckhpd), Ved, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 29 */
    {OP_vcvtss2si, 0xf30f2d00, STROFF(vcvtss2si), Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[29][1]},
    {OP_vcvtss2si, 0xf30f2d10, STROFF(vcvtss2si), Gd, xx, Ups, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, tevexwb[29][2]},
    {OP_vcvtss2si, 0xf30f2d40, STROFF(vcvtss2si), Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[29][3]},
    {OP_vcvtss2si, 0xf30f2d50, STROFF(vcvtss2si), Gy, xx, Ups, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 30 */
    {OP_vcvtsd2si, 0xf20f2d00, STROFF(vcvtsd2si), Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[30][1]},
    {OP_vcvtsd2si, 0xf20f2d10, STROFF(vcvtsd2si), Gd, xx, Upd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, tevexwb[30][2]},
    {OP_vcvtsd2si, 0xf20f2d40, STROFF(vcvtsd2si), Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[30][3]},
    {OP_vcvtsd2si, 0xf20f2d50, STROFF(vcvtsd2si), Gy, xx, Upd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 31 */
    {OP_vcvtsi2ss, 0xf30f2a00, STROFF(vcvtsi2ss), Vdq, xx, H12_dq, Ed, xx, mrm|evex|ttt1s, x, tevexwb[31][1]},
    {OP_vcvtsi2ss, 0xf30f2a10, STROFF(vcvtsi2ss), Vdq, xx, H12_dq, Rd, xx, mrm|evex|er|ttt1s, x, tevexwb[31][2]},
    {OP_vcvtsi2ss, 0xf30f2a40, STROFF(vcvtsi2ss), Vdq, xx, H12_dq, Ey, xx, mrm|evex|ttt1s, x, tevexwb[31][3]},
    {OP_vcvtsi2ss, 0xf30f2a50, STROFF(vcvtsi2ss), Vdq, xx, H12_dq, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 32 */
    {OP_vcvtsi2sd, 0xf20f2a00, STROFF(vcvtsi2sd), Vdq, xx, Hsd, Ed, xx, mrm|evex|ttt1s, x, tevexwb[32][2]},
    {INVALID, 0xf20f2a10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcvtsi2sd, 0xf20f2a40, STROFF(vcvtsi2sd), Vdq, xx, Hsd, Ey, xx, mrm|evex|ttt1s, x, tevexwb[32][3]},
    {OP_vcvtsi2sd, 0xf20f2a50, STROFF(vcvtsi2sd), Vdq, xx, Hsd, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 33 */
    {OP_vmovntps, 0x0f2b00, STROFF(vmovntps), Mes, xx, Ves, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x0f2b10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2b40,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2b50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 34 */
    {INVALID, 0x660f2b00,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2b10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovntpd, 0x660f2b40, STROFF(vmovntpd), Med, xx, Ved, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x660f2b50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 35 */
    {OP_vcvttss2si, 0xf30f2c00, STROFF(vcvttss2si), Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[35][1]},
    {OP_vcvttss2si, 0xf30f2c10, STROFF(vcvttss2si), Gd, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, tevexwb[35][2]},
    {OP_vcvttss2si, 0xf30f2c40, STROFF(vcvttss2si), Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[35][3]},
    {OP_vcvttss2si, 0xf30f2c50, STROFF(vcvttss2si), Gy, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 36 */
    {OP_vcvttsd2si, 0xf20f2c00, STROFF(vcvttsd2si), Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[36][1]},
    {OP_vcvttsd2si, 0xf20f2c10, STROFF(vcvttsd2si), Gd, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, tevexwb[36][2]},
    {OP_vcvttsd2si, 0xf20f2c40, STROFF(vcvttsd2si), Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[36][3]},
    {OP_vcvttsd2si, 0xf20f2c50, STROFF(vcvttsd2si), Gy, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 37 */
    {OP_vucomiss, 0x0f2e00, STROFF(vucomiss), xx, xx, Vss, Wss, xx, mrm|evex|ttt1s, fW6, tevexwb[37][1]},
    {OP_vucomiss, 0x0f2e10, STROFF(vucomiss), xx, xx, Vss, Uss, xx, mrm|evex|sae|ttt1s, fW6, END_LIST},
    {INVALID, 0x0f2e40,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2e50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 38 */
    {INVALID, 0x660f2e00,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2e10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vucomisd, 0x660f2e40, STROFF(vucomisd), xx, xx, Vsd, Wsd, xx, mrm|evex|ttt1s, fW6, tevexwb[38][1]},
    {OP_vucomisd, 0x660f2e50, STROFF(vucomisd), xx, xx, Vsd, Usd, xx, mrm|evex|sae|ttt1s, fW6, END_LIST},
  }, { /* evex_W_ext 39 */
    {OP_vcomiss,  0x0f2f00, STROFF(vcomiss),  xx, xx, Vss, Wss, xx, mrm|evex|ttt1f|inopsz4, fW6, tevexwb[39][1]},
    {OP_vcomiss,  0x0f2f10, STROFF(vcomiss),  xx, xx, Vss, Uss, xx, mrm|evex|sae|ttt1f|inopsz4, fW6, END_LIST},
    {INVALID, 0x0f2f40,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2f50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 40 */
    {INVALID, 0x660f2e00,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2e10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcomisd,  0x660f2f40, STROFF(vcomisd),  xx, xx, Vsd, Wsd, xx, mrm|evex|ttt1f|inopsz8, fW6, tevexwb[40][1]},
    {OP_vcomisd,  0x660f2f50, STROFF(vcomisd),  xx, xx, Vsd, Usd, xx, mrm|evex|sae|ttt1f|inopsz8, fW6, END_LIST},
  }, { /* evex_W_ext 41 */
    {OP_vpandd, 0x660fdb00, STROFF(vpandd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[41][1]},
    {OP_vpandd, 0x660fdb10, STROFF(vpandd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpandq, 0x660fdb40, STROFF(vpandq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[41][3]},
    {OP_vpandq, 0x660fdb50, STROFF(vpandq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 42 */
    {OP_vpandnd, 0x660fdf00, STROFF(vpandnd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[42][1]},
    {OP_vpandnd, 0x660fdf10, STROFF(vpandnd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpandnq, 0x660fdf40, STROFF(vpandnq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[42][3]},
    {OP_vpandnq, 0x660fdf50, STROFF(vpandnq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 43 */
    {OP_vpord, 0x660feb00, STROFF(vpord), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[43][1]},
    {OP_vpord, 0x660feb10, STROFF(vpord), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vporq, 0x660feb40, STROFF(vporq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[43][3]},
    {OP_vporq, 0x660feb50, STROFF(vporq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 44 */
    {OP_vpxord, 0x660fef00, STROFF(vpxord), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[44][1]},
    {OP_vpxord, 0x660fef10, STROFF(vpxord), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpxorq, 0x660fef40, STROFF(vpxorq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[44][3]},
    {OP_vpxorq, 0x660fef50, STROFF(vpxorq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 45 */
    {OP_vpmulld,  0x66384008, STROFF(vpmulld),   Ve, xx, KEw,He,We, mrm|evex|reqp|ttfv, x, tevexwb[45][1]},
    {OP_vpmulld,  0x66384018, STROFF(vpmulld),   Ve, xx, KEw,He,Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpmullq,  0x66384048, STROFF(vpmullq),   Ve, xx, KEb,He,We, mrm|evex|reqp|ttfv, x, tevexwb[45][3]},
    {OP_vpmullq,  0x66384058, STROFF(vpmullq),   Ve, xx, KEb,He,Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 46 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2qq, 0x660f7b00, STROFF(vcvtps2qq), Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[24][0]},
    {MOD_EXT, 0x660f7b10, STROFF(mod_ext_24), xx, xx, xx, xx, xx, mrm|evex, x, 24},
    {OP_vcvtpd2qq, 0x660f7b40, STROFF(vcvtpd2qq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[25][0]},
    {MOD_EXT, 0x660f7b50, STROFF(mod_ext_25), xx, xx, xx, xx, xx, mrm|evex, x, 25},
  }, { /* evex_W_ext 47 */
    {OP_vcvtps2udq, 0x0f7900, STROFF(vcvtps2udq), Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[26][0]},
    {MOD_EXT, 0x0f7910, STROFF(mod_ext_26), xx, xx, xx, xx, xx, mrm|evex, x, 26},
    {OP_vcvtpd2udq, 0x0f7940, STROFF(vcvtpd2udq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[27][0]},
    {MOD_EXT, 0x0f7950, STROFF(mod_ext_27), xx, xx, xx, xx, xx, mrm|evex, x, 27},
  }, { /* evex_W_ext 48 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2uqq, 0x660f7900, STROFF(vcvtps2uqq), Ve, xx, KEw, Wh_e, xx, mrm|evex|tthv, x, modx[28][0]},
    {MOD_EXT, 0x660f7910, STROFF(mod_ext_28), xx, xx, xx, xx, xx, mrm|evex, x, 28},
    {OP_vcvtpd2uqq, 0x660f7940, STROFF(vcvtpd2uqq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[29][0]},
    {MOD_EXT, 0x660f7950, STROFF(mod_ext_29), xx, xx, xx, xx, xx, mrm|evex, x, 29},
  }, { /* evex_W_ext 49 */
    {OP_vcvttps2udq, 0x0f7800, STROFF(vcvttps2udq), Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[30][0]},
    {MOD_EXT, 0x0f7810, STROFF(mod_ext_30), xx, xx, xx, xx, xx, mrm|evex, x, 30},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttpd2udq, 0x0f7840, STROFF(vcvttpd2udq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[31][0]},
    {MOD_EXT, 0x0f7840, STROFF(mod_ext_31), xx, xx, xx, xx, xx, mrm|evex, x, 31},
  }, { /* evex_W_ext 50 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttps2qq, 0x660f7a00, STROFF(vcvttps2qq), Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[32][0]},
    {MOD_EXT, 0x660f7a10, STROFF(mod_ext_32), xx, xx, xx, xx, xx, mrm|evex, x, 32},
    {OP_vcvttpd2qq, 0x660f7a40, STROFF(vcvttpd2qq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[33][0]},
    {MOD_EXT, 0x660f7a50, STROFF(mod_ext_33), xx, xx, xx, xx, xx, mrm|evex, x, 33},
  }, { /* evex_W_ext 51 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttps2uqq, 0x660f7800, STROFF(vcvttps2uqq), Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[34][0]},
    {MOD_EXT, 0x660f7810, STROFF(mod_ext_34), xx, xx, xx, xx, xx, mrm|evex, x, 34},
    {OP_vcvttpd2uqq, 0x660f7840, STROFF(vcvttpd2uqq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[35][0]},
    {MOD_EXT, 0x660f7850, STROFF(mod_ext_35), xx, xx, xx, xx, xx, mrm|evex, x, 35},
  }, { /* evex_W_ext 52 */
    {OP_vcvtss2usi, 0xf30f7900, STROFF(vcvtss2usi), Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[52][1]},
    {OP_vcvtss2usi, 0xf30f7910, STROFF(vcvtss2usi), Gd, xx, Uss, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, tevexwb[52][2]},
    {OP_vcvtss2usi, 0xf30f7940, STROFF(vcvtss2usi), Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[52][3]},
    {OP_vcvtss2usi, 0xf30f7950, STROFF(vcvtss2usi), Gy, xx, Uss, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 53 */
    {OP_vcvtsd2usi, 0xf20f7900, STROFF(vcvtsd2usi), Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[53][1]},
    {OP_vcvtsd2usi, 0xf20f7910, STROFF(vcvtsd2usi), Gd, xx, Usd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, tevexwb[53][2]},
    {OP_vcvtsd2usi, 0xf20f7940, STROFF(vcvtsd2usi), Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[53][1]},
    {OP_vcvtsd2usi, 0xf20f7950, STROFF(vcvtsd2usi), Gy, xx, Usd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 54 */
    {OP_vcvttss2usi, 0xf30f7800, STROFF(vcvttss2usi), Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[54][1]},
    {OP_vcvttss2usi, 0xf30f7810, STROFF(vcvttss2usi), Gd, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, tevexwb[54][2]},
    {OP_vcvttss2usi, 0xf30f7840, STROFF(vcvttss2usi), Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[54][3]},
    {OP_vcvttss2usi, 0xf30f7850, STROFF(vcvttss2usi), Gy, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 55 */
    {OP_vcvttsd2usi, 0xf20f7800, STROFF(vcvttsd2usi), Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[55][1]},
    {OP_vcvttsd2usi, 0xf20f7810, STROFF(vcvttsd2usi), Gd, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, tevexwb[55][2]},
    {OP_vcvttsd2usi, 0xf20f7840, STROFF(vcvttsd2usi), Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[55][3]},
    {OP_vcvttsd2usi, 0xf20f7850, STROFF(vcvttsd2usi), Gy, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 56 */
    {OP_vcvtdq2ps, 0x0f5b00, STROFF(vcvtdq2ps), Ves, xx, KEw, We, xx, mrm|evex|ttfv, x, modx[36][0]},
    {MOD_EXT, 0x0f5b10, STROFF(mod_ext_36), xx, xx, xx, xx, xx, mrm|evex, x, 36},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtqq2ps, 0x0f5b40, STROFF(vcvtqq2ps), Ves, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[37][0]},
    {MOD_EXT, 0x0f5b50, STROFF(mod_ext_37), xx, xx, xx, xx, xx, mrm|evex, x, 37},
  }, { /* evex_W_ext 57 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtdq2pd, 0xf30fe600, STROFF(vcvtdq2pd), Ved, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, tevexwb[57][1]},
    {OP_vcvtdq2pd, 0xf30fe610, STROFF(vcvtdq2pd), Ved, xx, KEb, Md, xx, mrm|evex|tthv, x, END_LIST},
    {OP_vcvtqq2pd, 0xf30fe640, STROFF(vcvtqq2pd), Ved, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[38][0]},
    {MOD_EXT, 0xf30fe650, STROFF(mod_ext_38), xx, xx, xx, xx, xx, mrm|evex, x, 38},
  }, { /* evex_W_ext 58 */
    {OP_vcvtusi2ss, 0xf30f7b00, STROFF(vcvtusi2ss), Vdq, xx, H12_dq, Ed, xx, mrm|evex|ttt1s, x, tevexwb[58][1]},
    {OP_vcvtusi2ss, 0xf30f7b10, STROFF(vcvtusi2ss), Vdq, xx, H12_dq, Rd, xx, mrm|evex|er|ttt1s, x, tevexwb[58][2]},
    {OP_vcvtusi2ss, 0xf30f7b40, STROFF(vcvtusi2ss), Vdq, xx, H12_dq, Ey, xx, mrm|evex|ttt1s, x, tevexwb[58][3]},
    {OP_vcvtusi2ss, 0xf30f7b50, STROFF(vcvtusi2ss), Vdq, xx, H12_dq, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 59 */
    {OP_vcvtusi2sd, 0xf20f7b00, STROFF(vcvtusi2sd), Vdq, xx, Hsd,   Ed, xx, mrm|evex|ttt1s, x, tevexwb[59][2]},
    {INVALID, 0xf20f7b10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcvtusi2sd, 0xf20f7b40, STROFF(vcvtusi2sd), Vdq, xx, Hsd, Ey, xx, mrm|evex|ttt1s, x, tevexwb[59][3]},
    {OP_vcvtusi2sd, 0xf20f7b50, STROFF(vcvtusi2sd), Vdq, xx, Hsd, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 60 */
    {OP_vcvtudq2ps, 0xf20f7a00, STROFF(vcvtudq2ps), Ve, xx, KEw, We, xx, mrm|evex|ttfv, x, modx[39][0]},
    {MOD_EXT, 0xf20f7a10, STROFF(mod_ext_39), xx, xx, xx, xx, xx, mrm|evex, x, 39},
    {OP_vcvtuqq2ps, 0xf20f7a40, STROFF(vcvtuqq2ps), Ve, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[40][0]},
    {MOD_EXT, 0xf20f7a50, STROFF(mod_ext_40), xx, xx, xx, xx, xx, mrm|evex, x, 40},
  }, { /* evex_W_ext 61 */
    {OP_vcvtudq2pd, 0xf30f7a00, STROFF(vcvtudq2pd), Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, tevexwb[61][1]},
    {OP_vcvtudq2pd, 0xf30f7a10, STROFF(vcvtudq2pd), Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, END_LIST},
    {OP_vcvtuqq2pd, 0xf30f7a40, STROFF(vcvtuqq2pd), Ve, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[41][0]},
    {MOD_EXT, 0xf30f7a50, STROFF(mod_ext_41), xx, xx, xx, xx, xx, mrm|evex, x, 41},
  }, { /* evex_W_ext 62 */
    {OP_vfmadd132ps,0x66389808,STROFF(vfmadd132ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[14]},
    {MOD_EXT, 0x66389818, STROFF(mod_ext_42), xx, xx, xx, xx, xx, mrm|evex, x, 42},
    {OP_vfmadd132pd,0x66389848,STROFF(vfmadd132pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[17]},
    {MOD_EXT, 0x66389858, STROFF(mod_ext_43), xx, xx, xx, xx, xx, mrm|evex, x, 43},
  }, { /* evex_W_ext 63 */
    {OP_vfmadd213ps,0x6638a808,STROFF(vfmadd213ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[20]},
    {MOD_EXT, 0x6638a818, STROFF(mod_ext_44), xx, xx, xx, xx, xx, mrm|evex, x, 44},
    {OP_vfmadd213pd,0x6638a848,STROFF(vfmadd213pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[23]},
    {MOD_EXT, 0x6638a858, STROFF(mod_ext_45), xx, xx, xx, xx, xx, mrm|evex, x, 45},
  }, { /* evex_W_ext 64 */
    {OP_vfmadd231ps,0x6638b808,STROFF(vfmadd231ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[26]},
    {MOD_EXT, 0x6638b818, STROFF(mod_ext_46), xx, xx, xx, xx, xx, mrm|evex, x, 46},
    {OP_vfmadd231pd,0x6638b848,STROFF(vfmadd231pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[29]},
    {MOD_EXT, 0x6638b858, STROFF(mod_ext_47), xx, xx, xx, xx, xx, mrm|evex, x, 47},
  }, { /* evex_W_ext 65 */
    {OP_vfmadd132ss,0x66389908,STROFF(vfmadd132ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[32]},
    {OP_vfmadd132ss,0x66389918,STROFF(vfmadd132ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[33]},
    {OP_vfmadd132sd,0x66389948,STROFF(vfmadd132sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[34]},
    {OP_vfmadd132sd,0x66389958,STROFF(vfmadd132sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[35]},
  }, { /* evex_W_ext 66 */
    {OP_vfmadd213ss,0x6638a908,STROFF(vfmadd213ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[36]},
    {OP_vfmadd213ss,0x6638a918,STROFF(vfmadd213ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[37]},
    {OP_vfmadd213sd,0x6638a948,STROFF(vfmadd213sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[38]},
    {OP_vfmadd213sd,0x6638a958,STROFF(vfmadd213sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[39]},
  }, { /* evex_W_ext 67 */
    {OP_vfmadd231ss,0x6638b908,STROFF(vfmadd231ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[40]},
    {OP_vfmadd231ss,0x6638b918,STROFF(vfmadd231ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[41]},
    {OP_vfmadd231sd,0x6638b948,STROFF(vfmadd231sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[42]},
    {OP_vfmadd231sd,0x6638b958,STROFF(vfmadd231sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[43]},
  }, { /* evex_W_ext 68 */
    {OP_vfmaddsub132ps,0x66389608,STROFF(vfmaddsub132ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[44]},
    {MOD_EXT, 0x66389618, STROFF(mod_ext_48), xx, xx, xx, xx, xx, mrm|evex, x, 48},
    {OP_vfmaddsub132pd,0x66389648,STROFF(vfmaddsub132pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[47]},
    {MOD_EXT, 0x66389658, STROFF(mod_ext_49), xx, xx, xx, xx, xx, mrm|evex, x, 49},
  }, { /* evex_W_ext 69 */
    {OP_vfmaddsub213ps,0x6638a608,STROFF(vfmaddsub213ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[50]},
    {MOD_EXT, 0x6638a618, STROFF(mod_ext_50), xx, xx, xx, xx, xx, mrm|evex, x, 50},
    {OP_vfmaddsub213pd,0x6638a648,STROFF(vfmaddsub213pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[53]},
    {MOD_EXT, 0x6638a658, STROFF(mod_ext_51), xx, xx, xx, xx, xx, mrm|evex, x, 51},
  }, { /* evex_W_ext 70 */
    {OP_vfmaddsub231ps,0x6638b608,STROFF(vfmaddsub231ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[56]},
    {MOD_EXT, 0x6638b618, STROFF(mod_ext_52), xx, xx, xx, xx, xx, mrm|evex, x, 52},
    {OP_vfmaddsub231pd,0x6638b648,STROFF(vfmaddsub231pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[59]},
    {MOD_EXT, 0x6638b658, STROFF(mod_ext_53), xx, xx, xx, xx, xx, mrm|evex, x, 53},
  }, { /* evex_W_ext 71 */
    {OP_vfmsubadd132ps,0x66389708,STROFF(vfmsubadd132ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[62]},
    {MOD_EXT, 0x66389718, STROFF(mod_ext_54), xx, xx, xx, xx, xx, mrm|evex, x, 54},
    {OP_vfmsubadd132pd,0x66389748,STROFF(vfmsubadd132pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[65]},
    {MOD_EXT, 0x66389758, STROFF(mod_ext_55), xx, xx, xx, xx, xx, mrm|evex, x, 55},
  }, { /* evex_W_ext 72 */
    {OP_vfmsubadd213ps,0x6638a708,STROFF(vfmsubadd213ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[68]},
    {MOD_EXT, 0x6638a718, STROFF(mod_ext_56), xx, xx, xx, xx, xx, mrm|evex, x, 56},
    {OP_vfmsubadd213pd,0x6638a748,STROFF(vfmsubadd213pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[71]},
    {MOD_EXT, 0x6638a758, STROFF(mod_ext_57), xx, xx, xx, xx, xx, mrm|evex, x, 57},
  }, { /* evex_W_ext 73 */
    {OP_vfmsubadd231ps,0x6638b708,STROFF(vfmsubadd231ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[74]},
    {MOD_EXT, 0x6638b718, STROFF(mod_ext_58), xx, xx, xx, xx, xx, mrm|evex, x, 58},
    {OP_vfmsubadd231pd,0x6638b748,STROFF(vfmsubadd231pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[77]},
    {MOD_EXT, 0x6638b758, STROFF(mod_ext_59), xx, xx, xx, xx, xx, mrm|evex, x, 59},
  }, { /* evex_W_ext 74 */
    {OP_vfmsub132ps,0x66389a08,STROFF(vfmsub132ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[80]},
    {MOD_EXT, 0x66389a18, STROFF(mod_ext_60), xx, xx, xx, xx, xx, mrm|evex, x, 60},
    {OP_vfmsub132pd,0x66389a48,STROFF(vfmsub132pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[83]},
    {MOD_EXT, 0x66389a58, STROFF(mod_ext_61), xx, xx, xx, xx, xx, mrm|evex, x, 61},
  }, { /* evex_W_ext 75 */
    {OP_vfmsub213ps,0x6638aa08,STROFF(vfmsub213ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[86]},
    {MOD_EXT, 0x6638aa18, STROFF(mod_ext_62), xx, xx, xx, xx, xx, mrm|evex, x, 62},
    {OP_vfmsub213pd,0x6638aa48,STROFF(vfmsub213pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[89]},
    {MOD_EXT, 0x6638aa58, STROFF(mod_ext_63), xx, xx, xx, xx, xx, mrm|evex, x, 63},
  }, { /* evex_W_ext 76 */
    {OP_vfmsub231ps,0x6638ba08,STROFF(vfmsub231ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[92]},
    {MOD_EXT, 0x6638ba18, STROFF(mod_ext_64), xx, xx, xx, xx, xx, mrm|evex, x, 64},
    {OP_vfmsub231pd,0x6638ba48,STROFF(vfmsub231pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[95]},
    {MOD_EXT, 0x6638ba58, STROFF(mod_ext_65), xx, xx, xx, xx, xx, mrm|evex, x, 65},
  }, { /* evex_W_ext 77 */
    {OP_vfmsub132ss,0x66389b08,STROFF(vfmsub132ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[98]},
    {OP_vfmsub132ss,0x66389b18,STROFF(vfmsub132ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[99]},
    {OP_vfmsub132sd,0x66389b48,STROFF(vfmsub132sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[100]},
    {OP_vfmsub132sd,0x66389b58,STROFF(vfmsub132sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[101]},
  }, { /* evex_W_ext 78 */
    {OP_vfmsub213ss,0x6638ab08,STROFF(vfmsub213ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[102]},
    {OP_vfmsub213ss,0x6638ab18,STROFF(vfmsub213ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[103]},
    {OP_vfmsub213sd,0x6638ab48,STROFF(vfmsub213sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[104]},
    {OP_vfmsub213sd,0x6638ab58,STROFF(vfmsub213sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[105]},
  }, { /* evex_W_ext 79 */
    {OP_vfmsub231ss,0x6638bb08,STROFF(vfmsub231ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[106]},
    {OP_vfmsub231ss,0x6638bb18,STROFF(vfmsub231ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[107]},
    {OP_vfmsub231sd,0x6638bb48,STROFF(vfmsub231sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[108]},
    {OP_vfmsub231sd,0x6638bb58,STROFF(vfmsub231sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[109]},
  }, { /* evex_W_ext 80 */
    {OP_vfnmadd132ps,0x66389c08,STROFF(vfnmadd132ps),Ves,xx,KEb,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[110]},
    {MOD_EXT, 0x66389c18, STROFF(mod_ext_66), xx, xx, xx, xx, xx, mrm|evex, x, 66},
    {OP_vfnmadd132pd,0x66389c48,STROFF(vfnmadd132pd),Ved,xx,KEw,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[113]},
    {MOD_EXT, 0x66389c58, STROFF(mod_ext_67), xx, xx, xx, xx, xx, mrm|evex, x, 67},
  }, { /* evex_W_ext 81 */
    {OP_vfnmadd213ps,0x6638ac08,STROFF(vfnmadd213ps),Ves,xx,KEb,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[116]},
    {MOD_EXT, 0x6638ac18, STROFF(mod_ext_68), xx, xx, xx, xx, xx, mrm|evex, x, 68},
    {OP_vfnmadd213pd,0x6638ac48,STROFF(vfnmadd213pd),Ved,xx,KEw,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[119]},
    {MOD_EXT, 0x6638ac58, STROFF(mod_ext_69), xx, xx, xx, xx, xx, mrm|evex, x, 69},
  }, { /* evex_W_ext 82 */
    {OP_vfnmadd231ps,0x6638bc08,STROFF(vfnmadd231ps),Ves,xx,KEb,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[122]},
    {MOD_EXT, 0x6638bc18, STROFF(mod_ext_70), xx, xx, xx, xx, xx, mrm|evex, x, 70},
    {OP_vfnmadd231pd,0x6638bc48,STROFF(vfnmadd231pd),Ved,xx,KEw,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[125]},
    {MOD_EXT, 0x6638bc58, STROFF(mod_ext_71), xx, xx, xx, xx, xx, mrm|evex, x, 71},
  }, { /* evex_W_ext 83 */
    {OP_vfnmadd132ss,0x66389d08,STROFF(vfnmadd132ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[128]},
    {OP_vfnmadd132ss,0x66389d18,STROFF(vfnmadd132ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[129]},
    {OP_vfnmadd132sd,0x66389d48,STROFF(vfnmadd132sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[130]},
    {OP_vfnmadd132sd,0x66389d58,STROFF(vfnmadd132sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[131]},
  }, { /* evex_W_ext 84 */
    {OP_vfnmadd213ss,0x6638ad08,STROFF(vfnmadd213ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[132]},
    {OP_vfnmadd213ss,0x6638ad18,STROFF(vfnmadd213ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[133]},
    {OP_vfnmadd213sd,0x6638ad48,STROFF(vfnmadd213sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[134]},
    {OP_vfnmadd213sd,0x6638ad58,STROFF(vfnmadd213sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[135]},
  }, { /* evex_W_ext 85 */
    {OP_vfnmadd231ss,0x6638bd08,STROFF(vfnmadd231ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[136]},
    {OP_vfnmadd231ss,0x6638bd18,STROFF(vfnmadd231ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[137]},
    {OP_vfnmadd231sd,0x6638bd48,STROFF(vfnmadd231sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[138]},
    {OP_vfnmadd231sd,0x6638bd58,STROFF(vfnmadd231sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[139]},
  }, { /* evex_W_ext 86 */
    {OP_vfnmsub132ps,0x66389e08,STROFF(vfnmsub132ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[140]},
    {MOD_EXT, 0x66389e18, STROFF(mod_ext_72), xx, xx, xx, xx, xx, mrm|evex, x, 72},
    {OP_vfnmsub132pd,0x66389e48,STROFF(vfnmsub132pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[143]},
    {MOD_EXT, 0x66389e58, STROFF(mod_ext_73), xx, xx, xx, xx, xx, mrm|evex, x, 73},
  }, { /* evex_W_ext 87 */
    {OP_vfnmsub213ps,0x6638ae08,STROFF(vfnmsub213ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[146]},
    {MOD_EXT, 0x6638ae18, STROFF(mod_ext_74), xx, xx, xx, xx, xx, mrm|evex, x, 74},
    {OP_vfnmsub213pd,0x6638ae48,STROFF(vfnmsub213pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[149]},
    {MOD_EXT, 0x6638ae58, STROFF(mod_ext_75), xx, xx, xx, xx, xx, mrm|evex, x, 75},
  }, { /* evex_W_ext 88 */
    {OP_vfnmsub231ps,0x6638be08,STROFF(vfnmsub231ps),Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[152]},
    {MOD_EXT, 0x6638be18, STROFF(mod_ext_76), xx, xx, xx, xx, xx, mrm|evex, x, 76},
    {OP_vfnmsub231pd,0x6638be48,STROFF(vfnmsub231pd),Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[155]},
    {MOD_EXT, 0x6638be58, STROFF(mod_ext_77), xx, xx, xx, xx, xx, mrm|evex, x, 77},
  }, { /* evex_W_ext 89 */
    {OP_vfnmsub132ss,0x66389f08,STROFF(vfnmsub132ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[158]},
    {OP_vfnmsub132ss,0x66389f18,STROFF(vfnmsub132ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[159]},
    {OP_vfnmsub132sd,0x66389f48,STROFF(vfnmsub132sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[160]},
    {OP_vfnmsub132sd,0x66389f58,STROFF(vfnmsub132sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[161]},
  }, { /* evex_W_ext 90 */
    {OP_vfnmsub213ss,0x6638af08,STROFF(vfnmsub213ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[162]},
    {OP_vfnmsub213ss,0x6638af18,STROFF(vfnmsub213ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[163]},
    {OP_vfnmsub213sd,0x6638af48,STROFF(vfnmsub213sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[164]},
    {OP_vfnmsub213sd,0x6638af58,STROFF(vfnmsub213sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[165]},
  }, { /* evex_W_ext 91 */
    {OP_vfnmsub231ss,0x6638bf08,STROFF(vfnmsub231ss),Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[166]},
    {OP_vfnmsub231ss,0x6638bf18,STROFF(vfnmsub231ss),Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[167]},
    {OP_vfnmsub231sd,0x6638bf48,STROFF(vfnmsub231sd),Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[168]},
    {OP_vfnmsub231sd,0x6638bf58,STROFF(vfnmsub231sd),Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[169]},
  }, { /* evex_W_ext 92 */
    {OP_vpermb,0x66388d08,STROFF(vpermb),Ve,xx,KEb,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66388d18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpermw,0x66388d48,STROFF(vpermw),Ve,xx,KEw,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66388d58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 93 */
    {OP_vpermd,0x66383608,STROFF(vpermd),Vf,xx,KEb,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[93][1]},
    {OP_vpermd,0x66383618,STROFF(vpermd),Vf,xx,KEb,Hf,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermq,0x66383648,STROFF(vpermq),Vf,xx,KEb,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[93][3]},
    {OP_vpermq,0x66383658,STROFF(vpermq),Vf,xx,KEb,Hf,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 94 */
    {OP_vpermps,0x66381608,STROFF(vpermps),Vf,xx,KEw,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[94][1]},
    {OP_vpermps,0x66381618,STROFF(vpermps),Vf,xx,KEw,Hf,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermpd,0x66381648,STROFF(vpermpd),Vf,xx,KEw,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[94][3]},
    {OP_vpermpd,0x66381658,STROFF(vpermpd),Vf,xx,KEw,Hf,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 95 */
    {OP_vpermi2ps,0x66387708,STROFF(vpermi2ps),Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[95][1]},
    {OP_vpermi2ps,0x66387718,STROFF(vpermi2ps),Ve,xx,KEw,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermi2pd,0x66387748,STROFF(vpermi2pd),Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[95][3]},
    {OP_vpermi2pd,0x66387758,STROFF(vpermi2pd),Ve,xx,KEw,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 96 */
    {OP_vpermi2d,0x66387608,STROFF(vpermi2d),Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[96][1]},
    {OP_vpermi2d,0x66387618,STROFF(vpermi2d),Ve,xx,KEw,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermi2q,0x66387648,STROFF(vpermi2q),Ve,xx,KEb,He,We,mrm|evex|reqp|ttfv,x,tevexwb[96][3]},
    {OP_vpermi2q,0x66387658,STROFF(vpermi2q),Ve,xx,KEb,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 97 */
    {OP_vpermi2b,0x66387508,STROFF(vpermi2b),Ve,xx,KEq,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387518,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpermi2w,0x66387548,STROFF(vpermi2w),Ve,xx,KEd,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387558,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 98 */
    {OP_vpermt2b,0x66387d08,STROFF(vpermt2b),Ve,xx,KEw,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387d18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpermt2w,0x66387d48,STROFF(vpermt2w),Ve,xx,KEb,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387d58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 99 */
    {OP_vpermt2d,0x66387e08,STROFF(vpermt2d),Ve,xx,KEq,He,We,mrm|evex|reqp|ttfv,x,tevexwb[99][1]},
    {OP_vpermt2d,0x66387e18,STROFF(vpermt2d),Ve,xx,KEq,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermt2q,0x66387e48,STROFF(vpermt2q),Ve,xx,KEd,He,We,mrm|evex|reqp|ttfv,x,tevexwb[99][3]},
    {OP_vpermt2q,0x66387e58,STROFF(vpermt2q),Ve,xx,KEd,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 100 */
    {OP_vpermt2ps,0x66387f08,STROFF(vpermt2ps),Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[100][1]},
    {OP_vpermt2ps,0x66387f18,STROFF(vpermt2ps),Ve,xx,KEw,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermt2pd,0x66387f48,STROFF(vpermt2pd),Ve,xx,KEb,He,We,mrm|evex|reqp|ttfv,x,tevexwb[100][3]},
    {OP_vpermt2pd,0x66387f58,STROFF(vpermt2pd),Ve,xx,KEb,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 101 */
    {OP_vextractf32x4, 0x663a1908, STROFF(vextractf32x4), Wdq, xx, KE4b, Ib, Vdq_f, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a1918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextractf64x2, 0x663a1948, STROFF(vextractf64x2), Wdq, xx, KE2b, Ib, Vdq_f, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x663a1958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 102 */
    {OP_vextractf32x8, 0x663a1b08, STROFF(vextractf32x8), Wqq, xx,  KEb, Ib, Vqq_oq, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x663a1b18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextractf64x4, 0x663a1b48, STROFF(vextractf64x4), Wqq, xx, KE4b, Ib, Vqq_oq, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a1b58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 103 */
    {OP_vextracti32x4, 0x663a3908, STROFF(vextracti32x4), Wdq, xx, KE4b, Ib, Vdq_f, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a3918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextracti64x2, 0x663a3948, STROFF(vextracti64x2), Wdq, xx, KE2b, Ib, Vdq_f, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x663a3958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 104 */
    {OP_vextracti32x8, 0x663a3b08, STROFF(vextracti32x8), Wqq, xx,  KEb, Ib, Vqq_oq, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x663a3b18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextracti64x4, 0x663a3b48, STROFF(vextracti64x4), Wqq, xx, KE4b, Ib, Vqq_oq, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a3b58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 105 */
    {OP_vinsertf32x4, 0x663a1808, STROFF(vinsertf32x4), Vf, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[170]},
    {INVALID, 0x663a1818,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinsertf64x2, 0x663a1848, STROFF(vinsertf64x2), Vf, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt2, x, exop[171]},
    {INVALID, 0x663a1858,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 106 */
    {OP_vinsertf32x8, 0x663a1a08, STROFF(vinsertf32x8), Voq, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt8, x, exop[172]},
    {INVALID, 0x663a1a18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinsertf64x4, 0x663a1a48, STROFF(vinsertf64x4), Voq, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[173]},
    {INVALID, 0x663a1858,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 107 */
    {OP_vinserti32x4, 0x663a3808, STROFF(vinserti32x4), Vf, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[174]},
    {INVALID, 0x663a3818,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinserti64x2, 0x663a3848, STROFF(vinserti64x2), Vf, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt2, x, exop[175]},
    {INVALID, 0x663a3858,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 108 */
    {OP_vinserti32x8, 0x663a3a08, STROFF(vinserti32x8), Voq, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt8, x, exop[176]},
    {INVALID, 0x663a3a18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinserti64x4, 0x663a3a48, STROFF(vinserti64x4), Voq, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[177]},
    {INVALID, 0x663a3a58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 109 */
    {OP_vpcmpub, 0x663a3e08, STROFF(vpcmpub), KPq, xx, KEq, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[178]},
    {INVALID, 0x663a3e18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpcmpuw, 0x663a3e48, STROFF(vpcmpuw), KPd, xx, KEd, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[180]},
    {INVALID, 0x663a3e58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 110 */
    {OP_vpcmpb, 0x663a3f08, STROFF(vpcmpb), KPq, xx, KEq, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[179]},
    {INVALID, 0x663a3f18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpcmpw, 0x663a3f48, STROFF(vpcmpw), KPd, xx, KEd, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[181]},
    {INVALID, 0x663a3f58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 111 */
    {OP_vpcmpud, 0x663a1e08, STROFF(vpcmpud), KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[182]},
    {OP_vpcmpud, 0x663a1e18, STROFF(vpcmpud), KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[183]},
    {OP_vpcmpuq, 0x663a1e48, STROFF(vpcmpuq), KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[186]},
    {OP_vpcmpuq, 0x663a1e58, STROFF(vpcmpuq), KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[187]},
  }, { /* evex_W_ext 112 */
    {OP_vpcmpd, 0x663a1f08, STROFF(vpcmpd), KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[184]},
    {OP_vpcmpd, 0x663a1f18, STROFF(vpcmpd), KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[185]},
    {OP_vpcmpq, 0x663a1f48, STROFF(vpcmpq), KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[188]},
    {OP_vpcmpq, 0x663a1f58, STROFF(vpcmpq), KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[189]},
  }, { /* evex_W_ext 113 */
    {OP_vpminsd, 0x66383908, STROFF(vpminsd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[113][1]},
    {OP_vpminsd, 0x66383918, STROFF(vpminsd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpminsq, 0x66383948, STROFF(vpminsq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[113][3]},
    {OP_vpminsq, 0x66383958, STROFF(vpminsq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 114 */
    {OP_vpmaxsd,  0x66383d08, STROFF(vpmaxsd), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[114][1]},
    {OP_vpmaxsd,  0x66383d18, STROFF(vpmaxsd), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpmaxsq,  0x66383d48, STROFF(vpmaxsq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[114][3]},
    {OP_vpmaxsq,  0x66383d58, STROFF(vpmaxsq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 115 */
    {OP_vpminud,  0x66383b08, STROFF(vpminud), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[115][1]},
    {OP_vpminud,  0x66383b18, STROFF(vpminud), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpminuq,  0x66383b48, STROFF(vpminuq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[115][3]},
    {OP_vpminuq,  0x66383b58, STROFF(vpminuq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 116 */
    {OP_vpmaxud,  0x66383f08, STROFF(vpmaxud), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[116][1]},
    {OP_vpmaxud,  0x66383f18, STROFF(vpmaxud), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpmaxuq,  0x66383f48, STROFF(vpmaxuq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[116][3]},
    {OP_vpmaxuq,  0x66383f58, STROFF(vpmaxuq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 117 */
    {OP_vprolvd, 0x66381508, STROFF(vprolvd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[117][1]},
    {OP_vprolvd, 0x66381518, STROFF(vprolvd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprolvq, 0x66381548, STROFF(vprolvq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[117][3]},
    {OP_vprolvq, 0x66381558, STROFF(vprolvq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 118 */
    {OP_vprold, 0x660f7221, STROFF(vprold), He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[118][1]},
    {OP_vprold, 0x660f7231, STROFF(vprold), He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprolq, 0x660f7261, STROFF(vprolq), He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[118][3]},
    {OP_vprolq, 0x660f7271, STROFF(vprolq), He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 119 */
    {OP_vprorvd, 0x66381408, STROFF(vprorvd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[119][1]},
    {OP_vprorvd, 0x66381418, STROFF(vprorvd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprorvq, 0x66381448, STROFF(vprorvq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[119][3]},
    {OP_vprorvq, 0x66381458, STROFF(vprorvq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 120 */
    {OP_vprord, 0x660f7220, STROFF(vprord), He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[120][1]},
    {OP_vprord, 0x660f7230, STROFF(vprord), He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprorq, 0x660f7260, STROFF(vprorq), He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[120][3]},
    {OP_vprorq, 0x660f7270, STROFF(vprorq), He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 121 */
    {OP_vpsrad, 0x660fe200, STROFF(vpsrad), Ve, xx, KEw, He, Wdq, mrm|evex|ttm128, x, tevexwb[122][0]},
    {INVALID, 0x660fe210,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsraq, 0x660fe240, STROFF(vpsraq), Ve, xx, KEb, He, Wdq, mrm|evex|ttm128, x, tevexwb[122][2]},
    {INVALID, 0x660fe250,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 122 */
    {OP_vpsrad, 0x660f7224, STROFF(vpsrad), He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[122][1]},
    {OP_vpsrad, 0x660f7234, STROFF(vpsrad), He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpsraq, 0x660f7264, STROFF(vpsraq), He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[122][3]},
    {OP_vpsraq, 0x660f7274, STROFF(vpsraq), He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 123 */
    {OP_vpsrld, 0x660fd200, STROFF(vpsrld), Ve, xx, KEw, He, Wdq, mrm|evex|ttm128, x, tevexwb[124][0]},
    {INVALID, 0x660fd210,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660fd240,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660fd250,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 124 */
    {OP_vpsrld, 0x660f7222, STROFF(vpsrld), He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[124][1]},
    {OP_vpsrld, 0x660f7232, STROFF(vpsrld), He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x660f7262,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f7272,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 125 */
    {INVALID, 0x660fd300,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660fd310,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsrlq, 0x660fd340, STROFF(vpsrlq), Ve, xx, KEb, He, Wdq, mrm|evex|ttm128, x, tevexwb[126][2]},
    {INVALID, 0x660fd350,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 126 */
    {INVALID, 0x660f7322,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f7332,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsrlq, 0x660f7362, STROFF(vpsrlq), He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[126][3]},
    {OP_vpsrlq, 0x660f7372, STROFF(vpsrlq), He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 127 */
    {INVALID, 0x66381108,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381118,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsravw, 0x66381148, STROFF(vpsravw), Ve, xx, KEb, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66381158,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 128 */
    {OP_vpsravd, 0x66384608, STROFF(vpsravd), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[128][1]},
    {OP_vpsravd, 0x66384618, STROFF(vpsravd), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpsravq, 0x66384648, STROFF(vpsravq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[128][3]},
    {OP_vpsravq, 0x66384658, STROFF(vpsravq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 129 */
    {OP_vpsrlvd,0x66384508, STROFF(vpsrlvd), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[129][1]},
    {OP_vpsrlvd,0x66384518, STROFF(vpsrlvd), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpsrlvq,0x66384548, STROFF(vpsrlvq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[129][3]},
    {OP_vpsrlvq,0x66384558, STROFF(vpsrlvq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 130 */
    {INVALID, 0x66381208,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381218,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsllvw, 0x66381248,STROFF(vpsllvw), Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66381258,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 131 */
    {OP_vpsllvd, 0x66384708, STROFF(vpsllvd), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv,x,tevexwb[131][1]},
    {OP_vpsllvd, 0x66384718, STROFF(vpsllvd), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpsllvq, 0x66384748, STROFF(vpsllvq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv,x,tevexwb[131][3]},
    {OP_vpsllvq, 0x66384758, STROFF(vpsllvq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 132 */
    {OP_vrcp14ps, 0x66384c08, STROFF(vrcp14ps), Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv,x,tevexwb[132][1]},
    {OP_vrcp14ps, 0x66384c18, STROFF(vrcp14ps), Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vrcp14pd, 0x66384c48, STROFF(vrcp14pd), Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv,x,tevexwb[132][3]},
    {OP_vrcp14pd, 0x66384c58, STROFF(vrcp14pd), Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 133 */
    {OP_vrcp14ss, 0x66384d08, STROFF(vrcp14ss), Vdq, xx, KE1b, H12_dq, Wss, mrm|evex|reqp|ttt1s,x,END_LIST},
    {INVALID, 0x66384d18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vrcp14sd, 0x66384d48, STROFF(vrcp14sd), Vdq, xx, KE1b, Hsd, Wsd, mrm|evex|reqp|ttt1s,x,END_LIST},
    {INVALID, 0x66384d58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 134 */
    {OP_vrcp28ps, 0x6638ca08, STROFF(vrcp28ps), Voq, xx, KEw, Woq, xx, mrm|evex|reqp|ttfv,x,modx[78][0]},
    {MOD_EXT, 0x6638ca18, STROFF(mod_ext_78), xx, xx, xx, xx, xx, mrm|evex, x, 78},
    {OP_vrcp28pd, 0x6638ca48, STROFF(vrcp28pd), Voq, xx, KEb, Woq, xx, mrm|evex|reqp|ttfv,x,modx[79][0]},
    {MOD_EXT, 0x6638ca58, STROFF(mod_ext_79), xx, xx, xx, xx, xx, mrm|evex, x, 79},
  }, { /* evex_W_ext 135 */
    {OP_vrcp28ss, 0x6638cb08, STROFF(vrcp28ss), Vdq, xx, KE1b, H12_dq, Wss, mrm|evex|reqp|ttt1s,x,tevexwb[135][1]},
    {OP_vrcp28ss, 0x6638cb18, STROFF(vrcp28ss), Vdq, xx, KE1b, H12_dq, Uss, mrm|evex|sae|reqp|ttt1s,x,END_LIST},
    {OP_vrcp28sd, 0x6638cb48, STROFF(vrcp28sd), Vdq, xx, KE1b, Hsd, Wsd, mrm|evex|reqp|ttt1s,x,tevexwb[135][3]},
    {OP_vrcp28sd, 0x6638cb58, STROFF(vrcp28sd), Vdq, xx, KE1b, Hsd, Usd, mrm|evex|sae|reqp|ttt1s,x,END_LIST},
  }, { /* evex_W_ext 136 */
    {OP_vmovd, 0x660f6e00, STROFF(vmovd), Vdq, xx, Ed, xx, xx, mrm|evex|ttt1s, x, tevexwb[137][0]},
    {INVALID, 0x660f6e10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovq, 0x660f6e40, STROFF(vmovq), Vdq, xx, Ey, xx, xx, mrm|evex|ttt1s, x, tevexwb[137][2]},
    {INVALID, 0x660f6e50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 137 */
    {OP_vmovd, 0x660f7e00, STROFF(vmovd), Ed, xx, Vd_dq, xx, xx, mrm|evex|ttt1s, x, END_LIST},
    {INVALID, 0x660f7e10,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovq, 0x660f7e40, STROFF(vmovq), Ey, xx, Vq_dq, xx, xx, mrm|evex|ttt1s, x, END_LIST},
    {INVALID, 0x660f7e50,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 138 */
    {OP_vpmovm2b, 0xf3382808, STROFF(vpmovm2b), Ve, xx, KQq, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382818,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovm2w, 0xf3382848, STROFF(vpmovm2w), Ve, xx, KQd, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382858,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 139 */
    {OP_vpmovm2d, 0xf3383808, STROFF(vpmovm2d), Ve, xx, KQw, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383818,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovm2q, 0xf3383848, STROFF(vpmovm2q), Ve, xx, KQb, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383858,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 140 */
    {OP_vpmovb2m, 0xf3382908, STROFF(vpmovb2m), KPq, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovw2m, 0xf3382948, STROFF(vpmovw2m), KPd, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 141 */
    {OP_vpmovd2m, 0xf3383908, STROFF(vpmovd2m), KPw, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovq2m, 0xf3383948, STROFF(vpmovq2m), KPb, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 142 */
    {OP_vshuff32x4, 0x663a2308, STROFF(vshuff32x4), Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[204]},
    {OP_vshuff32x4, 0x663a2318, STROFF(vshuff32x4), Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[205]},
    {OP_vshuff64x2, 0x663a2348, STROFF(vshuff64x2), Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[206]},
    {OP_vshuff64x2, 0x663a2358, STROFF(vshuff64x2), Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[207]},
  }, { /* evex_W_ext 143 */
    {OP_vshufi32x4, 0x663a4308, STROFF(vshufi32x4), Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[208]},
    {OP_vshufi32x4, 0x663a4318, STROFF(vshufi32x4), Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[209]},
    {OP_vshufi64x2, 0x663a4348, STROFF(vshufi64x2), Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[210]},
    {OP_vshufi64x2, 0x663a4358, STROFF(vshufi64x2), Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[211]},
  }, { /* evex_W_ext 144 */
    {OP_vpinsrd, 0x663a2208, STROFF(vpinsrd), Vdq, xx, H12_8_dq, Ey, Ib, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a2218,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpinsrq, 0x663a2248, STROFF(vpinsrq), Vdq, xx, H12_8_dq, Ey, Ib, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a2258,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 145 */
    {OP_vpextrd, 0x663a1608, STROFF(vpextrd),  Ey, xx, Vd_q_dq, Ib, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a1618,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpextrq, 0x663a1648, STROFF(vpextrq),  Ey, xx, Vd_q_dq, Ib, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a1658,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 146 */
    {OP_vpabsd, 0x66381e08, STROFF(vpabsd),   Ve, xx, KEw, We, xx, mrm|evex|ttfv, x, tevexwb[146][1]},
    {OP_vpabsd, 0x66381e18, STROFF(vpabsd),   Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x66381e48,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381e58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 147 */
    {INVALID, 0x66381f08,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381f18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpabsq, 0x66381f48, STROFF(vpabsq),   Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[147][3]},
    {OP_vpabsq, 0x66381f58, STROFF(vpabsq),   Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 148 */
    {OP_vbroadcastf32x2, 0x66381908, STROFF(vbroadcastf32x2), Vf, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66381918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcastsd, 0x66381948, STROFF(vbroadcastsd), Vf, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66381958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 149 */
    {OP_vbroadcastf32x4, 0x66381a08, STROFF(vbroadcastf32x4), Vf, xx, KEw, Mdq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66381a18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcastf64x2, 0x66381a48, STROFF(vbroadcastf64x2), Vf, xx, KEb, Mdq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66381a58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 150 */
    {OP_vbroadcastf32x8, 0x66381b08, STROFF(vbroadcastf32x8), Voq, xx, KEd, Mqq, xx, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x66381b18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcastf64x4, 0x66381b48, STROFF(vbroadcastf64x4), Voq, xx, KEb, Mqq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66381b58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 151 */
    {OP_vpbroadcastd, 0x66387c08, STROFF(vpbroadcastd), Ve, xx, KEw, Ed, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66387c18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpbroadcastq, 0x66387c48, STROFF(vpbroadcastq), Ve, xx, KEb, Eq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66387c58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 152 */
    {OP_vbroadcasti32x2, 0x66385908, STROFF(vbroadcasti32x2), Ve, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66385918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpbroadcastq, 0x66385948, STROFF(vpbroadcastq), Ve, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt1s, x, tevexwb[151][2]},
    {INVALID, 0x66385958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 153 */
    {OP_vbroadcasti32x4, 0x66385a08, STROFF(vbroadcasti32x4), Vf, xx, KEw, Mdq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66385a18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcasti64x2, 0x66385a48, STROFF(vbroadcasti64x2), Vf, xx, KEw, Mdq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66385a58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 154 */
    {OP_vbroadcasti32x8, 0x66385b08, STROFF(vbroadcasti32x8), Vf, xx, KEw, Mqq, xx, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x66385b18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcasti64x4, 0x66385b48, STROFF(vbroadcasti64x4), Vf, xx, KEb, Mqq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66385b58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 155 */
    {OP_valignd, 0x663a0308, STROFF(valignd), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[212]},
    {OP_valignd, 0x663a0318, STROFF(valignd), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[213]},
    {OP_valignq, 0x663a0348, STROFF(valignq), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[214]},
    {OP_valignq, 0x663a0358, STROFF(valignq), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[215]},
  }, { /* evex_W_ext 156 */
    {OP_vblendmps, 0x66386508, STROFF(vblendmps), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[156][1]},
    {OP_vblendmps, 0x66386518, STROFF(vblendmps), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vblendmpd, 0x66386548, STROFF(vblendmpd), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[156][3]},
    {OP_vblendmpd, 0x66386558, STROFF(vblendmpd), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 157 */
    {OP_vcompressps, 0x66388a08, STROFF(vcompressps), We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388a18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcompresspd, 0x66388a48, STROFF(vcompresspd), We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388a58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 158 */
    {OP_vexpandps, 0x66388808, STROFF(vexpandps), We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388818,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vexpandpd, 0x66388848, STROFF(vexpandpd), We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388858,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 159 */
    {OP_vfixupimmps, 0x663a5408, STROFF(vfixupimmps), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[216]},
    {MOD_EXT, 0x663a5418, STROFF(mod_ext_80), xx, xx, xx, xx, xx, mrm|evex, x, 80},
    {OP_vfixupimmpd, 0x663a5448, STROFF(vfixupimmpd), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[219]},
    {MOD_EXT, 0x663a5458, STROFF(mod_ext_81), xx, xx, xx, xx, xx, mrm|evex, x, 81},
  }, { /* evex_W_ext 160 */
    {OP_vfixupimmss, 0x663a5508, STROFF(vfixupimmss), Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|reqp|ttt1s, x, exop[222]},
    {OP_vfixupimmss, 0x663a5518, STROFF(vfixupimmss), Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[223]},
    {OP_vfixupimmsd, 0x663a5548, STROFF(vfixupimmsd), Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|reqp|ttt1s, x, exop[224]},
    {OP_vfixupimmsd, 0x663a5558, STROFF(vfixupimmsd), Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[225]},
  }, { /* evex_W_ext 161 */
    {OP_vgetexpps, 0x66384208, STROFF(vgetexpps), Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, modx[82][0]},
    {MOD_EXT, 0x66384218, STROFF(mod_ext_82), xx, xx, xx, xx, xx, mrm|evex, x, 82},
    {OP_vgetexppd, 0x66384248, STROFF(vgetexppd), Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, modx[83][0]},
    {MOD_EXT, 0x66384258, STROFF(mod_ext_83), xx, xx, xx, xx, xx, mrm|evex, x, 83},
  }, { /* evex_W_ext 162 */
    {OP_vgetexpss, 0x66384308, STROFF(vgetexpss), Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, tevexwb[162][1]},
    {OP_vgetexpss, 0x66384318, STROFF(vgetexpss), Vdq, xx, KE1b, H12_dq, Ud_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
    {OP_vgetexpsd, 0x66384348, STROFF(vgetexpsd), Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, tevexwb[162][3]},
    {OP_vgetexpsd, 0x66384358, STROFF(vgetexpsd), Vdq, xx, KE1b,    Hsd, Uq_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 163 */
    {OP_vgetmantps, 0x663a2608, STROFF(vgetmantps), Ve, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, modx[84][0]},
    {MOD_EXT, 0x663a2618, STROFF(mod_ext_84), xx, xx, xx, xx, xx, mrm|evex, x, 84},
    {OP_vgetmantpd, 0x663a2648, STROFF(vgetmantpd), Ve, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, modx[85][0]},
    {MOD_EXT, 0x663a2658, STROFF(mod_ext_85), xx, xx, xx, xx, xx, mrm|evex, x, 85},
  }, { /* evex_W_ext 164 */
    {OP_vgetmantss, 0x663a2708, STROFF(vgetmantss), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[226]},
    {OP_vgetmantss, 0x663a2718, STROFF(vgetmantss), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[227]},
    {OP_vgetmantsd, 0x663a2748, STROFF(vgetmantsd), Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[228]},
    {OP_vgetmantsd, 0x663a2758, STROFF(vgetmantsd), Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[229]},
  }, { /* evex_W_ext 165 */
    {OP_vpblendmb, 0x66386608, STROFF(vpblendmb), Ve, xx, KEq, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
    {INVALID, 0x66386618,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpblendmw, 0x66386648, STROFF(vpblendmw), Ve, xx, KEd, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
    {INVALID, 0x66386658,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 166 */
    {OP_vpblendmd, 0x66386408, STROFF(vpblendmd), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[166][1]},
    {OP_vpblendmd, 0x66386418, STROFF(vpblendmd), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpblendmq, 0x66386448, STROFF(vpblendmq), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[166][3]},
    {OP_vpblendmq, 0x66386458, STROFF(vpblendmq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 167 */
    {OP_vpcompressd, 0x66388b08, STROFF(vpcompressd), We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388b18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpcompressq, 0x66388b48, STROFF(vpcompressq), We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388b58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 168 */
    {OP_vpexpandd, 0x66388908, STROFF(vpexpandd), We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388918,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpexpandq, 0x66388948, STROFF(vpexpandq), We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388958,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 169 */
    {OP_vptestmb, 0x66382608, STROFF(vptestmb), KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66382618,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vptestmw, 0x66382648, STROFF(vptestmw), KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66382658,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 170 */
    {OP_vptestmd, 0x66382708, STROFF(vptestmd), KPw, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[170][1]},
    {OP_vptestmd, 0x66382718, STROFF(vptestmd), KPw, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vptestmq, 0x66382748, STROFF(vptestmq), KPb, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[170][3]},
    {OP_vptestmq, 0x66382758, STROFF(vptestmq), KPb, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 171 */
    {OP_vptestnmb, 0xf3382608, STROFF(vptestnmb), KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf3382618,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vptestnmw, 0xf3382648, STROFF(vptestnmw), KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf3382658,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 172 */
    {OP_vptestnmd, 0xf3382708, STROFF(vptestnmd), KPw, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[172][1]},
    {OP_vptestnmd, 0xf3382718, STROFF(vptestnmd), KPw, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vptestnmq, 0xf3382748, STROFF(vptestnmq), KPb, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[172][3]},
    {OP_vptestnmq, 0xf3382758, STROFF(vptestnmq), KPb, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 173 */
    {OP_vrangeps, 0x663a5008, STROFF(vrangeps), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[230]},
    {MOD_EXT, 0x663a5018, STROFF(mod_ext_86), xx, xx, xx, xx, xx, mrm|evex, x, 86},
    {OP_vrangepd, 0x663a5048, STROFF(vrangepd), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[233]},
    {MOD_EXT, 0x663a5058, STROFF(mod_ext_87), xx, xx, xx, xx, xx, mrm|evex, x, 87},
  }, { /* evex_W_ext 174 */
    {OP_vrangess, 0x663a5108, STROFF(vrangess), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[236]},
    {OP_vrangess, 0x663a5118, STROFF(vrangess), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[237]},
    {OP_vrangesd, 0x663a5148, STROFF(vrangesd), Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[238]},
    {OP_vrangesd, 0x663a5158, STROFF(vrangesd), Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[239]},
  }, { /* evex_W_ext 175 */
    {OP_vreduceps, 0x663a5608, STROFF(vreduceps), Ve, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, modx[88][0]},
    {MOD_EXT, 0x663a5618, STROFF(mod_ext_88), xx, xx, xx, xx, xx, mrm|evex, x, 88},
    {OP_vreducepd, 0x663a5648, STROFF(vreducepd), Ve, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, modx[89][0]},
    {MOD_EXT, 0x663a5658, STROFF(mod_ext_89), xx, xx, xx, xx, xx, mrm|evex, x, 89},
  }, { /* evex_W_ext 176 */
    {OP_vreducess, 0x663a5708, STROFF(vreducess), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[240]},
    {OP_vreducess, 0x663a5718, STROFF(vreducess), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[241]},
    {OP_vreducesd, 0x663a5748, STROFF(vreducesd), Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[242]},
    {OP_vreducesd, 0x663a5758, STROFF(vreducesd), Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[243]},
  }, { /* evex_W_ext 177 */
    {OP_vrsqrt14ps, 0x66384e08, STROFF(vrsqrt14ps), Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[177][1]},
    {OP_vrsqrt14ps, 0x66384e18, STROFF(vrsqrt14ps), Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vrsqrt14pd, 0x66384e48, STROFF(vrsqrt14pd), Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[177][3]},
    {OP_vrsqrt14pd, 0x66384e58, STROFF(vrsqrt14pd), Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 178 */
    {OP_vrsqrt14ss, 0x66384f08, STROFF(vrsqrt14ss), Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66384f18,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vrsqrt14sd, 0x66384f48, STROFF(vrsqrt14sd), Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66384f58,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 179 */
    {OP_vrsqrt28ps, 0x6638cc08, STROFF(vrsqrt28ps), Voq, xx, KEw, Woq, xx, mrm|evex|reqp|ttfv, x, modx[90][0]},
    {MOD_EXT, 0x6638cc18, STROFF(mod_ext_90), xx, xx, xx, xx, xx, mrm|evex, x, 90},
    {OP_vrsqrt28pd, 0x6638cc48, STROFF(vrsqrt28pd), Voq, xx, KEb, Woq, xx, mrm|evex|reqp|ttfv, x, modx[91][0]},
    {MOD_EXT, 0x6638cc58, STROFF(mod_ext_91), xx, xx, xx, xx, xx, mrm|evex, x, 91},
  }, { /* evex_W_ext 180 */
    {OP_vrsqrt28ss, 0x6638cd08, STROFF(vrsqrt28ss), Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, tevexwb[180][1]},
    {OP_vrsqrt28ss, 0x6638cd18, STROFF(vrsqrt28ss), Vdq, xx, KE1b, H12_dq, Ud_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
    {OP_vrsqrt28sd, 0x6638cd48, STROFF(vrsqrt28sd), Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, tevexwb[180][3]},
    {OP_vrsqrt28sd, 0x6638cd58, STROFF(vrsqrt28sd), Vdq, xx, KE1b,    Hsd, Uq_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 181 */
    {OP_vscalefps, 0x66382c08, STROFF(vscalefps), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, modx[92][0]},
    {MOD_EXT, 0x66382c18, STROFF(mod_ext_92), xx, xx, xx, xx, xx, mrm|evex, x, 92},
    {OP_vscalefpd, 0x66382c48, STROFF(vscalefpd), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, modx[93][0]},
    {MOD_EXT, 0x66382c58, STROFF(mod_ext_93), xx, xx, xx, xx, xx, mrm|evex, x, 93},
  }, { /* evex_W_ext 182 */
    {OP_vscalefss, 0x66382d08, STROFF(vscalefss), Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, tevexwb[182][1]},
    {OP_vscalefss, 0x66382d18, STROFF(vscalefss), Vdq, xx, KE1b, H12_dq, Ud_dq, mrm|evex|er|reqp|ttt1s, x, END_LIST},
    {OP_vscalefsd, 0x66382d48, STROFF(vscalefsd), Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, tevexwb[182][1]},
    {OP_vscalefsd, 0x66382d58, STROFF(vscalefsd), Vdq, xx, KE1b,    Hsd, Uq_dq, mrm|evex|er|reqp|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 183 */
    {OP_vfpclassps, 0x663a6608, STROFF(vfpclassps), KPw, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, tevexwb[183][1]},
    {OP_vfpclassps, 0x663a6618, STROFF(vfpclassps), KPw, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vfpclasspd, 0x663a6648, STROFF(vfpclasspd), KPb, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, tevexwb[183][3]},
    {OP_vfpclasspd, 0x663a6658, STROFF(vfpclasspd), KPb, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 184 */
    {OP_vfpclassss, 0x663a6708, STROFF(vfpclassss), KP1b, xx, KE1b, Ib, Wd_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a6718,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vfpclasssd, 0x663a6748, STROFF(vfpclasssd), KP1b, xx, KE1b, Ib, Wq_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a6758,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 185 */
    {OP_vexp2ps, 0x6638c808, STROFF(vexp2ps), Voq, xx, KEw, Woq, xx, mrm|evex|reqp|ttfv, x, modx[94][0]},
    {MOD_EXT, 0x6638c818, STROFF(mod_ext_94), xx, xx, xx, xx, xx, mrm|evex, x, 94},
    {OP_vexp2pd, 0x6638c848, STROFF(vexp2pd), Voq, xx, KEb, Woq, xx, mrm|evex|reqp|ttfv, x, modx[95][0]},
    {MOD_EXT, 0x6638c858, STROFF(mod_ext_95), xx, xx, xx, xx, xx, mrm|evex, x, 95},
  }, { /* evex_W_ext 186 */
    {OP_vpconflictd, 0x6638c408, STROFF(vpconflictd), Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[186][1]},
    {OP_vpconflictd, 0x6638c418, STROFF(vpconflictd), Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpconflictq, 0x6638c448, STROFF(vpconflictq), Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[186][3]},
    {OP_vpconflictq, 0x6638c458, STROFF(vpconflictq), Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 187 */
    {OP_vplzcntd, 0x66384408, STROFF(vplzcntd), Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[187][1]},
    {OP_vplzcntd, 0x66384418, STROFF(vplzcntd), Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vplzcntq, 0x66384448, STROFF(vplzcntq), Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[187][3]},
    {OP_vplzcntq, 0x66384458, STROFF(vplzcntq), Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 188 */
    {OP_vpternlogd, 0x663a2508, STROFF(vpternlogd), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[250]},
    {OP_vpternlogd, 0x663a2518, STROFF(vpternlogd), Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[251]},
    {OP_vpternlogq, 0x663a2548, STROFF(vpternlogq), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[252]},
    {OP_vpternlogq, 0x663a2558, STROFF(vpternlogq), Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[253]},
  }, { /* evex_W_ext 189 */
    /* XXX: OP_v*gather* raise #UD if any pair of the index, mask, or destination
     * registers are identical.  We don't bother trying to detect that.
     */
    {OP_vpgatherdd, 0x66389008, STROFF(vpgatherdd), Ve, KEw, KEw, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389018,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpgatherdq, 0x66389048, STROFF(vpgatherdq), Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389058,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 190 */
    {OP_vpgatherqd, 0x66389108, STROFF(vpgatherqd), Ve, KEb, KEb, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389118,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpgatherqq, 0x66389148, STROFF(vpgatherqq), Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389158,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 191 */
    {OP_vgatherdps, 0x66389208, STROFF(vgatherdps), Ve, KEw, KEw, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389218,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherdpd, 0x66389248, STROFF(vgatherdpd), Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389258,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 192 */
    {OP_vgatherqps, 0x66389308, STROFF(vgatherqps), Ve, KEb, KEb, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389318,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherqpd, 0x66389348, STROFF(vgatherqpd), Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389358,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 193 */
    /* XXX: OP_v*scatter* raise #UD if any pair of the index, mask, or destination
     * registers are identical.  We don't bother trying to detect that.
     */
    {OP_vpscatterdd, 0x6638a008, STROFF(vpscatterdd), MVd, KEw, KEw, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a018,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpscatterdq, 0x6638a048, STROFF(vpscatterdq), MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a058,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 194 */
    {OP_vpscatterqd, 0x6638a108, STROFF(vpscatterqd), MVd, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a118,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpscatterqq, 0x6638a148, STROFF(vpscatterqq), MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a158,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 195 */
    {OP_vscatterdps, 0x6638a208, STROFF(vscatterdps), MVd, KEw, KEw, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a218,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterdpd, 0x6638a248, STROFF(vscatterdpd), MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a258,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 196 */
    {OP_vscatterqps, 0x6638a308, STROFF(vscatterqps), MVd, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a318,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterqpd, 0x6638a348, STROFF(vscatterqpd), MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a358,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 197 */
       /* XXX i#1312: The encoding of this and the following gather prefetch instructions
        * is not clear. AVX-512PF seems to be specific to the Knights Landing architecture
        * (Xeon Phi). Our current encoding works with binutils but fails llvm-mc.
        * Open-source XED at the time of this writing doesn't support AVX-512PF. Neither
        * do I have hardware available that supports AVX-512PF. Looks like llvm-mc expects
        * EVEX.L', even though the index vector lengths are not ZMM. Before we implement
        * an exception using reqLL1 and fix the lengths in decoder/encoder or file a bug
        * on LLVM, this needs to be clarified.
        */
    {OP_vgatherpf0dps, 0x6638c629, STROFF(vgatherpf0dps), xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c639,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf0dpd, 0x6638c669, STROFF(vgatherpf0dpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c679,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 198 */
    {OP_vgatherpf0qps, 0x6638c729, STROFF(vgatherpf0qps), xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c739,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf0qpd, 0x6638c769, STROFF(vgatherpf0qpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c779,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 199 */
    {OP_vgatherpf1dps, 0x6638c62a, STROFF(vgatherpf1dps), xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c6ea,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf1dpd, 0x6638c66a, STROFF(vgatherpf1dpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c67a,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 200  */
    {OP_vgatherpf1qps, 0x6638c72a, STROFF(vgatherpf1qps), xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c731,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf1qpd, 0x6638c76a, STROFF(vgatherpf1qpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c77a,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 201 */
    {OP_vscatterpf0dps, 0x6638c62d, STROFF(vscatterpf0dps), xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c63e,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf0dpd, 0x6638c66d, STROFF(vscatterpf0dpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c67d,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 202 */
    {OP_vscatterpf0qps, 0x6638c72d, STROFF(vscatterpf0qps), xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c73d,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf0qpd, 0x6638c76d, STROFF(vscatterpf0qpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c77d,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 203 */
    {OP_vscatterpf1dps, 0x6638c62e, STROFF(vscatterpf1dps), xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c63e,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf1dpd, 0x6638c66e, STROFF(vscatterpf1dpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c67e,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 204  */
    {OP_vscatterpf1qps, 0x6638c72e, STROFF(vscatterpf1qps), xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c7e3,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf1qpd, 0x6638c76e, STROFF(vscatterpf1qpd), xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c77e,STROFF(BAD), xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 205 */
    {OP_vandps,  0x0f5400, STROFF(vandps),  Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[205][1]},
    {OP_vandps,  0x0f5410, STROFF(vandps),  Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vandpd,  0x660f5440, STROFF(vandpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[205][3]},
    {OP_vandpd,  0x660f5450, STROFF(vandpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 206 */
    {OP_vandnps, 0x0f5500, STROFF(vandnps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[206][1]},
    {OP_vandnps, 0x0f5510, STROFF(vandnps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vandnpd, 0x660f5540, STROFF(vandnpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[206][3]},
    {OP_vandnpd, 0x660f5550, STROFF(vandnpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 207 */
    {OP_vorps, 0x0f5600, STROFF(vorps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[207][1]},
    {OP_vorps, 0x0f5610, STROFF(vorps), Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vorpd, 0x660f5640, STROFF(vorpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[207][3]},
    {OP_vorpd, 0x660f5650, STROFF(vorpd), Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 208 */
    {OP_vxorps, 0x0f5700, STROFF(vxorps),  Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[208][1]},
    {OP_vxorps, 0x0f5710, STROFF(vxorps),  Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vxorpd, 0x660f5740, STROFF(vxorpd),  Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[208][3]},
    {OP_vxorpd, 0x660f5750, STROFF(vxorpd),  Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 209 */
    {OP_vaddps, 0x0f5800, STROFF(vaddps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[96][0]},
    {MOD_EXT, 0x0f5810, STROFF(mod_ext_96), xx, xx, xx, xx, xx, mrm|evex, x, 96},
    {OP_vaddpd, 0x660f5840, STROFF(vaddpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[97][0]},
    {MOD_EXT, 0x660f5850, STROFF(mod_ext_97), xx, xx, xx, xx, xx, mrm|evex, x, 97},
  }, { /* evex_W_ext 210 */
    {OP_vmulps, 0x0f5900, STROFF(vmulps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[98][0]},
    {MOD_EXT, 0x0f5910, STROFF(mod_ext_98), xx, xx, xx, xx, xx, mrm|evex, x, 98},
    {OP_vmulpd, 0x660f5940, STROFF(vmulpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[99][0]},
    {MOD_EXT, 0x660f5950, STROFF(mod_ext_99), xx, xx, xx, xx, xx, mrm|evex, x, 99},
  }, { /* evex_W_ext 211 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2pd, 0x0f5a00, STROFF(vcvtps2pd), Ved, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[100][0]},
    {MOD_EXT, 0x0f5a10, STROFF(mod_ext_100), xx, xx, xx, xx, xx, mrm|evex, x, 100},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtpd2ps, 0x660f5a40, STROFF(vcvtpd2ps), Ves, xx, KEw, Wed, xx, mrm|evex|ttfv, x, modx[101][0]},
    {MOD_EXT, 0x660f5a50, STROFF(mod_ext_101), xx, xx, xx, xx, xx, mrm|evex, x, 101},
  }, { /* evex_W_ext 212 */
    {OP_vsubps, 0x0f5c00, STROFF(vsubps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[102][0]},
    {MOD_EXT, 0x0f5c10, STROFF(mod_ext_102), xx, xx, xx, xx, xx, mrm|evex, x, 102},
    {OP_vsubpd, 0x660f5c40, STROFF(vsubpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[103][0]},
    {MOD_EXT, 0x660f5c50, STROFF(mod_ext_103), xx, xx, xx, xx, xx, mrm|evex, x, 103},
  }, { /* evex_W_ext 213 */
    {OP_vminps, 0x0f5d00, STROFF(vminps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[104][0]},
    {MOD_EXT, 0x0f5d10, STROFF(mod_ext_104), xx, xx, xx, xx, xx, mrm|evex, x, 104},
    {OP_vminpd, 0x660f5d40, STROFF(vminpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[105][0]},
    {MOD_EXT, 0x660f5d50, STROFF(mod_ext_105), xx, xx, xx, xx, xx, mrm|evex, x, 105},
  }, { /* evex_W_ext 214 */
    {OP_vdivps, 0x0f5e00, STROFF(vdivps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[106][0]},
    {MOD_EXT, 0x0f5e10, STROFF(mod_ext_106), xx, xx, xx, xx, xx, mrm|evex, x, 106},
    {OP_vdivpd, 0x660f5e40, STROFF(vdivpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[107][0]},
    {MOD_EXT, 0x660f5e50, STROFF(mod_ext_107), xx, xx, xx, xx, xx, mrm|evex, x, 107},
  }, { /* evex_W_ext 215 */
    {OP_vmaxps,   0x0f5f00, STROFF(vmaxps), Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[108][0]},
    {MOD_EXT, 0x0f5f10, STROFF(mod_ext_108), xx, xx, xx, xx, xx, mrm|evex, x, 108},
    {OP_vmaxpd, 0x660f5f40, STROFF(vmaxpd), Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[109][0]},
    {MOD_EXT, 0x660f5f50, STROFF(mod_ext_109), xx, xx, xx, xx, xx, mrm|evex, x, 109},
  }, { /* evex_W_ext 216 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpcklqdq, 0x660f6c40, STROFF(vpunpcklqdq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[216][3]},
    {OP_vpunpcklqdq, 0x660f6c50, STROFF(vpunpcklqdq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 217 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmuludq, 0x660ff440, STROFF(vpmuludq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[217][3]},
    {OP_vpmuludq, 0x660ff450, STROFF(vpmuludq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 218 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrndscalepd, 0x663a0948, STROFF(vrndscalepd),  Ve, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, modx[111][0]},
    {MOD_EXT, 0x663a0958, STROFF(mod_ext_111), xx, xx, xx, xx, xx, mrm|evex, x, 111},
  }, { /* evex_W_ext 219 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpckhqdq, 0x660f6d40, STROFF(vpunpckhqdq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[219][3]},
    {OP_vpunpckhqdq, 0x660f6d50, STROFF(vpunpckhqdq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 220 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmadd52luq, 0x6638b448, STROFF(vpmadd52luq), Ve, xx, KEb, He, Wed, mrm|evex|reqp|ttfv, x, tevexwb[220][3]},
    {OP_vpmadd52luq, 0x6638b458, STROFF(vpmadd52luq), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 221 */
    {OP_vshufps, 0x0fc600, STROFF(vshufps), Ves, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[200]},
    {OP_vshufps, 0x0fc610, STROFF(vshufps), Ves, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[201]},
    {OP_vshufpd, 0x660fc640, STROFF(vshufpd), Ved, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[202]},
    {OP_vshufpd, 0x660fc650, STROFF(vshufpd), Ved, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[203]},
  }, { /* evex_W_ext 222 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttpd2dq,0x660fe640, STROFF(vcvttpd2dq), Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[113][0]},
    {MOD_EXT, 0x660fe650, STROFF(mod_ext_113), xx, xx, xx, xx, xx, mrm|evex, x, 113},
  }, { /* evex_W_ext 223 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtpd2dq, 0xf20fe640, STROFF(vcvtpd2dq),  Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[112][0]},
    {MOD_EXT, 0xf20fe650, STROFF(mod_ext_112), xx, xx, xx, xx, xx, mrm|evex, x, 112},
  }, { /* evex_W_ext 224 */
    {OP_vcmpps, 0x0fc200, STROFF(vcmpps), KPw, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[190]},
    {MOD_EXT, 0x0fc210, STROFF(mod_ext_114), xx, xx, xx, xx, xx, mrm|evex, x, 114},
    {OP_vcmppd, 0x660fc240, STROFF(vcmppd), KPb, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[197]},
    {MOD_EXT, 0x660fc250, STROFF(mod_ext_115), xx, xx, xx, xx, xx, mrm|evex, x, 115},
  }, { /* evex_W_ext 225 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddq, 0x660fd440, STROFF(vpaddq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[225][3]},
    {OP_vpaddq, 0x660fd450, STROFF(vpaddq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 226 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubq, 0x660ffb40, STROFF(vpsubq), Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[226][3]},
    {OP_vpsubq, 0x660ffb50, STROFF(vpsubq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 227 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmuldq,  0x66382848, STROFF(vpmuldq), Ve, xx, KEb, He, Wed, mrm|evex|ttfv, x, tevexwb[227][3]},
    {OP_vpmuldq,  0x66382858, STROFF(vpmuldq), Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 228 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq, 0x660ff340, STROFF(vpsllq), Ve, xx, KEb, He, Wdq, mrm|evex|ttm128, x, tevexwb[229][2]},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 229 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq,  0x660f7366, STROFF(vpsllq), He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[229][3]},
    {OP_vpsllq,  0x660f7376, STROFF(vpsllq), He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 230 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x663a0548, STROFF(vpermilpd), Ve, xx, KEb, We, Ib, mrm|evex|reqp|ttfv, x, tevexwb[230][3]},
    {OP_vpermilpd, 0x663a0558, STROFF(vpermilpd), Ve, xx, KEb, Mq, Ib, mrm|evex|reqp|ttfv, x, tevexwb[231][2]},
  }, { /* evex_W_ext 231 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x66380d48, STROFF(vpermilpd), Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[231][3]},
    {OP_vpermilpd, 0x66380d58, STROFF(vpermilpd), Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 232 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpgtq, 0x66383748, STROFF(vpcmpgtq),  KPb, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[232][3]},
    {OP_vpcmpgtq, 0x66383758, STROFF(vpcmpgtq),  KPb, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 233 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpeqq, 0x66382948, STROFF(vpcmpeqq), KPb, xx, KEb, He, Wed, mrm|evex|ttfv, x, tevexwb[233][3]},
    {OP_vpcmpeqq, 0x66382958, STROFF(vpcmpeqq), KPb, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 234 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmadd52huq, 0x6638b548, STROFF(vpmadd52huq), Ve, xx, KEd, He, Wed, mrm|evex|reqp|ttfv, x, tevexwb[234][3]},
    {OP_vpmadd52huq, 0x6638b558, STROFF(vpmadd52huq), Ve, xx, KEd, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 235 */
    {OP_vpunpckldq, 0x660f6200, STROFF(vpunpckldq), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[235][1]},
    {OP_vpunpckldq, 0x660f6210, STROFF(vpunpckldq), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 236 */
    {OP_vpcmpgtd, 0x660f6600, STROFF(vpcmpgtd), KPb, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[236][1]},
    {OP_vpcmpgtd, 0x660f6610, STROFF(vpcmpgtd), KPb, xx, KEb, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 237 */
    {OP_vpunpckhdq, 0x660f6a00, STROFF(vpunpckhdq), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[237][1]},
    {OP_vpunpckhdq, 0x660f6a10, STROFF(vpunpckhdq), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 238 */
    {OP_vpackssdw, 0x660f6b00, STROFF(vpackssdw), Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[238][1]},
    {OP_vpackssdw, 0x660f6b10, STROFF(vpackssdw), Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 239 */
    {OP_vpshufd,  0x660f7000, STROFF(vpshufd),  Ve, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[239][1]},
    {OP_vpshufd,  0x660f7010, STROFF(vpshufd),  Ve, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 240 */
    {OP_vpcmpeqd, 0x660f7600, STROFF(vpcmpeqd), KPw, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[240][1]},
    {OP_vpcmpeqd, 0x660f7610, STROFF(vpcmpeqd), KPw, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 241 */
    {OP_vpsubd, 0x660ffa00, STROFF(vpsubd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[241][1]},
    {OP_vpsubd, 0x660ffa10, STROFF(vpsubd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 242 */
    {OP_vpaddd, 0x660ffe00, STROFF(vpaddd), Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[242][1]},
    {OP_vpaddd, 0x660ffe10, STROFF(vpaddd), Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 243 */
    {OP_vpslld, 0x660ff200, STROFF(vpslld), Ve, xx, KEw, He, Wdq, mrm|evex|ttm128, x, tevexwb[244][0]},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 244 */
    {OP_vpslld,  0x660f7226, STROFF(vpslld), He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[244][1]},
    {OP_vpslld,  0x660f7236, STROFF(vpslld), He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 245 */
    {OP_vpackusdw, 0x66382b08, STROFF(vpackusdw), Ve, xx, KEd, He, We, mrm|evex|reqp|ttfv, x, tevexwb[245][1]},
    {OP_vpackusdw, 0x66382b18, STROFF(vpackusdw), Ve, xx, KEd, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 246 */
    {OP_vrndscaleps, 0x663a0808, STROFF(vrndscaleps),  Ve, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, modx[110][0]},
    {MOD_EXT, 0x663a0918, STROFF(mod_ext_110), xx, xx, xx, xx, xx, mrm|evex, x, 110},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 247 */
    {OP_vpermilps, 0x663a0408, STROFF(vpermilps), Ve, xx, KEw, We, Ib, mrm|evex|reqp|ttfv, x, tevexwb[247][1]},
    {OP_vpermilps, 0x663a0418, STROFF(vpermilps), Ve, xx, KEw, Md, Ib, mrm|evex|reqp|ttfv, x, tevexwb[248][0]},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 248 */
    {OP_vpermilps, 0x66380c08, STROFF(vpermilps), Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[248][1]},
    {OP_vpermilps, 0x66380c18, STROFF(vpermilps), Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 249 */
    {OP_vcvtps2dq, 0x660f5b00, STROFF(vcvtps2dq), Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[116][0]},
    {MOD_EXT, 0x660f5b10, STROFF(mod_ext_116), xx, xx, xx, xx, xx, mrm|evex, x, 116},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 250 */
    {OP_vcvttps2dq, 0xf30f5b00, STROFF(vcvttps2dq), Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[117][0]},
    {MOD_EXT, 0xf30f5b10, STROFF(mod_ext_117), xx, xx, xx, xx, xx, mrm|evex, x, 117},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 251 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermq, 0x663a0048, STROFF(vpermq), Vf, xx, KEb, Wf, Ib, mrm|evex|reqp|ttfv, x, tevexwb[251][3]},
    {OP_vpermq, 0x663a0058, STROFF(vpermq), Vf, xx, KEb, Mq, Ib, mrm|evex|reqp|ttfv, x, tevexwb[93][2]},
  }, { /* evex_W_ext 252 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermpd, 0x663a0148, STROFF(vpermpd), Vf, xx, KEw, Wf, Ib, mrm|evex|reqp, x, tevexwb[252][3]},
    {OP_vpermpd, 0x663a0158, STROFF(vpermpd), Vf, xx, KEw, Mq, Ib, mrm|evex|reqp, x, tevexwb[94][2]},
  }, { /* evex_W_ext 253 */
    {OP_vrndscaless, 0x663a0a08, STROFF(vrndscaless), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[244]},
    {OP_vrndscaless, 0x663a0a18, STROFF(vrndscaless), Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[245]},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 254 */
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrndscalesd, 0x663a0b48, STROFF(vrndscalesd), Vdq, xx, KE1b, Ib, Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[246]},
    {OP_vrndscalesd, 0x663a0b58, STROFF(vrndscalesd), Vdq, xx, KE1b, Ib, Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[247]},
  }, { /* evex_W_ext 255 */
    {OP_vaddss, 0xf30f5800, STROFF(vaddss), Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[255][1]},
    {OP_vaddss, 0xf30f5810, STROFF(vaddss), Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vaddsd, 0xf20f5840, STROFF(vaddsd), Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[255][3]},
    {OP_vaddsd, 0xf20f5850, STROFF(vaddsd), Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 256 */
    {OP_vmulss, 0xf30f5900, STROFF(vmulss), Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[256][1]},
    {OP_vmulss, 0xf30f5910, STROFF(vmulss), Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vmulsd, 0xf20f5940, STROFF(vmulsd), Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[256][3]},
    {OP_vmulsd, 0xf20f5950, STROFF(vmulsd), Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 257 */
    {OP_vcvtss2sd, 0xf30f5a00, STROFF(vcvtss2sd), Vdq, xx, KE1b, Hsd, Wss, mrm|evex|ttt1s, x, tevexwb[257][1]},
    {OP_vcvtss2sd, 0xf30f5a10, STROFF(vcvtss2sd), Vdq, xx, KE1b, Hsd, Uss, mrm|evex|sae|ttt1s, x, END_LIST},
    {OP_vcvtsd2ss, 0xf20f5a40, STROFF(vcvtsd2ss), Vdq, xx, KE1b, H12_dq, Wsd, mrm|evex|ttt1s, x, tevexwb[257][3]},
    {OP_vcvtsd2ss, 0xf20f5a50, STROFF(vcvtsd2ss), Vdq, xx, KE1b, H12_dq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 258 */
    {OP_vsubss, 0xf30f5c00, STROFF(vsubss), Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[258][1]},
    {OP_vsubss, 0xf30f5c10, STROFF(vsubss), Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vsubsd, 0xf20f5c40, STROFF(vsubsd), Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[258][3]},
    {OP_vsubsd, 0xf20f5c50, STROFF(vsubsd), Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 259 */
    {OP_vminss, 0xf30f5d00, STROFF(vminss), Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[259][1]},
    {OP_vminss, 0xf30f5d10, STROFF(vminss), Vdq, xx, KE1b, Hdq, Uss, mrm|evex|sae|ttt1s, x, END_LIST},
    {OP_vminsd, 0xf20f5d40, STROFF(vminsd), Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[259][3]},
    {OP_vminsd, 0xf20f5d50, STROFF(vminsd), Vdq, xx, KE1b, Hdq, Usd, mrm|evex|sae|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 260 */
    {OP_vdivss, 0xf30f5e00, STROFF(vdivss), Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[260][1]},
    {OP_vdivss, 0xf30f5e10, STROFF(vdivss), Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vdivsd, 0xf20f5e40, STROFF(vdivsd), Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[260][3]},
    {OP_vdivsd, 0xf20f5e50, STROFF(vdivsd), Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 261 */
    {OP_vmaxss, 0xf30f5f00, STROFF(vmaxss), Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[261][1]},
    {OP_vmaxss, 0xf30f5f10, STROFF(vmaxss), Vdq, xx, KE1b, Hdq, Uss, mrm|evex|sae|ttt1s, x, END_LIST},
    {OP_vmaxsd, 0xf20f5f40, STROFF(vmaxsd), Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[261][3]},
    {OP_vmaxsd, 0xf20f5f50, STROFF(vmaxsd), Vdq, xx, KE1b, Hdq, Usd, mrm|evex|sae|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 262 */
    {OP_vcmpss, 0xf30fc200, STROFF(vcmpss), KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|ttt1s, x, exop[193]},
    {OP_vcmpss, 0xf30fc210, STROFF(vcmpss), KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|ttt1s, x, exop[194]},
    {OP_vcmpsd, 0xf20fc240, STROFF(vcmpsd), KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|ttt1s, x, exop[195]},
    {OP_vcmpsd, 0xf20fc250, STROFF(vcmpsd), KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|ttt1s, x, exop[196]},
  }, { /* evex_W_ext 263 */
    {OP_vcvtph2ps, 0x66381308, STROFF(vcvtph2ps), Ve, xx, KEw, Wh_e, xx, mrm|evex|tthvm, x, tevexwb[263][1]},
    {OP_vcvtph2ps, 0x66381318, STROFF(vcvtph2ps), Voq, xx, KEw, Uqq, xx, mrm|evex|sae|tthvm, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 264 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2ph, 0x663a1d08, STROFF(vcvtps2ph), Wh_e, xx, KEw, Ve, Ib, mrm|evex|reqp|tthvm, x, tevexwb[264][1]},
    {OP_vcvtps2ph, 0x663a1d18, STROFF(vcvtps2ph), Uqq, xx, KEw, Voq, Ib, mrm|evex|sae|reqp|tthvm, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 265 */
    {OP_vsqrtps, 0x0f5100, STROFF(vsqrtps), Ves, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[118][0]},
    {MOD_EXT, 0x0f5110, STROFF(mod_ext_118), xx, xx, xx, xx, xx, mrm|evex, x, 118},
    {OP_vsqrtpd, 0x660f5140, STROFF(vsqrtpd), Ved, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[119][0]},
    {MOD_EXT, 0x660f5150, STROFF(mod_ext_119), xx, xx, xx, xx, xx, mrm|evex, x, 119},
  }, { /* evex_W_ext 266 */
    {OP_vsqrtss, 0xf30f5100, STROFF(vsqrtss), Vdq, xx, KE1b, H12_dq, Wss, mrm|evex|ttt1s, x, tevexwb[266][1]},
    {OP_vsqrtss, 0xf30f5110, STROFF(vsqrtss), Vdq, xx, KE1b, H12_dq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vsqrtsd, 0xf20f5140, STROFF(vsqrtsd), Vdq, xx, KE1b, Hsd, Wsd, mrm|evex|ttt1s, x, tevexwb[266][3]},
    {OP_vsqrtsd, 0xf20f5150, STROFF(vsqrtsd), Vdq, xx, KE1b, Hsd, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 267 */
    {OP_vpdpbusd, 0x66385008, STROFF(vpdpbusd), Ve, xx, KEd, He, We, mrm|evex|ttfv|reqp, x, tevexwb[267][1]},
    {OP_vpdpbusd, 0x66385018, STROFF(vpdpbusd), Ve, xx, KEd, He, Md, mrm|evex|ttfv|reqp, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 268 */
    {OP_vpdpbusds, 0x66385108, STROFF(vpdpbusds), Ve, xx, KEd, He, We, mrm|evex|ttfv|reqp, x, tevexwb[268][1]},
    {OP_vpdpbusds, 0x66385118, STROFF(vpdpbusds), Ve, xx, KEd, He, Md, mrm|evex|ttfv|reqp, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 269 */
    {OP_vpdpwssd, 0x66385208, STROFF(vpdpwssd), Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[269][1]},
    {OP_vpdpwssd, 0x66385218, STROFF(vpdpwssd), Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 270 */
    {OP_vpdpwssds, 0x66385308, STROFF(vpdpwssds), Ve, xx, KEd, He, We, mrm|evex|ttfv|reqp, x, tevexwb[270][1]},
    {OP_vpdpwssds, 0x66385318, STROFF(vpdpwssds), Ve, xx, KEd, He, Md, mrm|evex|ttfv|reqp, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 271 */
    {OP_vcvtne2ps2bf16, 0xf2387208, STROFF(vcvtne2ps2bf16), Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[271][1]},
    {OP_vcvtne2ps2bf16, 0xf2387218, STROFF(vcvtne2ps2bf16), Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 272 */
    {OP_vcvtneps2bf16, 0xf3387208, STROFF(vcvtneps2bf16), Vh_e, xx, KEd, We, xx, mrm|evex|ttfv, x, tevexwb[272][1]},
    {OP_vcvtneps2bf16, 0xf3387218, STROFF(vcvtneps2bf16), Vh_e, xx, KEd, Md, xx, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 273 */
    {OP_vdpbf16ps, 0xf3385208, STROFF(vdpbf16ps), Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[273][1]},
    {OP_vdpbf16ps, 0xf3385218, STROFF(vdpbf16ps), Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 274 */
    {OP_vpopcntd, 0x66385508, STROFF(vpopcntd), Ve, xx, KEd, We, xx, mrm|evex|ttfv|reqp, x, tevexwb[274][1]},
    {OP_vpopcntd, 0x66385518, STROFF(vpopcntd), Ve, xx, KEd, Md, xx, mrm|evex|ttfv|reqp, x, END_LIST},
    {OP_vpopcntq, 0x66385548, STROFF(vpopcntq), Ve, xx, KEq, We, xx, mrm|evex|ttfv|reqp, x, tevexwb[274][3]},
    {OP_vpopcntq, 0x66385558, STROFF(vpopcntq), Ve, xx, KEq, Mq, xx, mrm|evex|ttfv|reqp, x, END_LIST},
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
  {INVALID,     0x000000, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},              /* 0*/
  /* We are out of flags, and we want to share a lot of REQUIRES_VEX, so to
   * distinguish XOP we just rely on the XOP.map_select being disjoint from
   * the VEX.m-mmm field.
   */
  /* XOP.map_select = 0x08 */
  {OP_vpmacssww, 0x088518,STROFF(vpmacssww), Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 1*/
  {OP_vpmacsswd, 0x088618,STROFF(vpmacsswd), Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 2*/
  {OP_vpmacssdql,0x088718,STROFF(vpmacssdql),Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 3*/
  {OP_vpmacssdd, 0x088e18,STROFF(vpmacssdd), Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 4*/
  {OP_vpmacssdqh,0x088f18,STROFF(vpmacssdqh),Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 5*/
  {OP_vpmacsww,  0x089518,STROFF(vpmacsww),  Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 6*/
  {OP_vpmacswd,  0x089618,STROFF(vpmacswd),  Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 7*/
  {OP_vpmacsdql, 0x089718,STROFF(vpmacsdql), Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 8*/
  {OP_vpmacsdd,  0x089e18,STROFF(vpmacsdd),  Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /* 9*/
  {OP_vpmacsdqh, 0x089f18,STROFF(vpmacsdqh), Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /*10*/
  {VEX_W_EXT,    0x08a218, STROFF(vex_W_ext_50), xx,xx,xx,xx,xx, mrm|vex, x,  50},  /*11*/
  {VEX_W_EXT,    0x08a318, STROFF(vex_W_ext_51), xx,xx,xx,xx,xx, mrm|vex, x,  51},  /*12*/
  {OP_vpmadcsswd,0x08a618,STROFF(vpmadcsswd),Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /*13*/
  {OP_vpmadcswd, 0x08b618,STROFF(vpmadcswd), Vdq,xx,Hdq,Wdq,Ldq,mrm|vex,x,END_LIST},  /*14*/
  {OP_vprotb,    0x08c018,STROFF(vprotb),    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[52][0]},/*15*/
  {OP_vprotw,    0x08c118,STROFF(vprotw),    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[53][0]},/*16*/
  {OP_vprotd,    0x08c218,STROFF(vprotd),    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[54][0]},/*17*/
  {OP_vprotq,    0x08c318,STROFF(vprotq),    Vdq,xx,Wdq,Ib,xx,mrm|vex,x,tvexw[55][0]},/*18*/
  {OP_vpcomb,    0x08cc18,STROFF(vpcomb),    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*19*/
  {OP_vpcomw,    0x08cd18,STROFF(vpcomw),    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*20*/
  {OP_vpcomd,    0x08ce18,STROFF(vpcomd),    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*21*/
  {OP_vpcomq,    0x08cf18,STROFF(vpcomq),    Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*22*/
  {OP_vpcomub,   0x08ec18,STROFF(vpcomub),   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*23*/
  {OP_vpcomuw,   0x08ed18,STROFF(vpcomuw),   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*24*/
  {OP_vpcomud,   0x08ee18,STROFF(vpcomud),   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*25*/
  {OP_vpcomuq,   0x08ef18,STROFF(vpcomuq),   Vdq,xx,Hdq,Wdq,Ib,mrm|vex,x,END_LIST},   /*26*/
  /* XOP.map_select = 0x09 */
  {OP_vfrczps,   0x098018,STROFF(vfrczps),   Vvs,xx,Wvs,xx,xx,mrm|vex,x,END_LIST},    /*27*/
  {OP_vfrczpd,   0x098118,STROFF(vfrczpd),   Vvs,xx,Wvs,xx,xx,mrm|vex,x,END_LIST},    /*28*/
  {OP_vfrczss,   0x098218,STROFF(vfrczss),   Vss,xx,Wss,xx,xx,mrm|vex,x,END_LIST},    /*29*/
  {OP_vfrczsd,   0x098318,STROFF(vfrczsd),   Vsd,xx,Wsd,xx,xx,mrm|vex,x,END_LIST},    /*30*/
  {VEX_W_EXT,    0x099018, STROFF(vex_W_ext_52), xx,xx,xx,xx,xx, mrm|vex, x,  52},  /*31*/
  {VEX_W_EXT,    0x099118, STROFF(vex_W_ext_53), xx,xx,xx,xx,xx, mrm|vex, x,  53},  /*32*/
  {VEX_W_EXT,    0x099218, STROFF(vex_W_ext_54), xx,xx,xx,xx,xx, mrm|vex, x,  54},  /*33*/
  {VEX_W_EXT,    0x099318, STROFF(vex_W_ext_55), xx,xx,xx,xx,xx, mrm|vex, x,  55},  /*34*/
  {VEX_W_EXT,    0x099418, STROFF(vex_W_ext_56), xx,xx,xx,xx,xx, mrm|vex, x,  56},  /*35*/
  {VEX_W_EXT,    0x099518, STROFF(vex_W_ext_57), xx,xx,xx,xx,xx, mrm|vex, x,  57},  /*36*/
  {VEX_W_EXT,    0x099618, STROFF(vex_W_ext_58), xx,xx,xx,xx,xx, mrm|vex, x,  58},  /*37*/
  {VEX_W_EXT,    0x099718, STROFF(vex_W_ext_59), xx,xx,xx,xx,xx, mrm|vex, x,  59},  /*38*/
  {VEX_W_EXT,    0x099818, STROFF(vex_W_ext_60), xx,xx,xx,xx,xx, mrm|vex, x,  60},  /*39*/
  {VEX_W_EXT,    0x099918, STROFF(vex_W_ext_61), xx,xx,xx,xx,xx, mrm|vex, x,  61},  /*40*/
  {VEX_W_EXT,    0x099a18, STROFF(vex_W_ext_62), xx,xx,xx,xx,xx, mrm|vex, x,  62},  /*41*/
  {VEX_W_EXT,    0x099b18, STROFF(vex_W_ext_63), xx,xx,xx,xx,xx, mrm|vex, x,  63},  /*42*/
  {OP_vphaddbw,  0x09c118,STROFF(vphaddbw),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*43*/
  {OP_vphaddbd,  0x09c218,STROFF(vphaddbd),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*44*/
  {OP_vphaddbq,  0x09c318,STROFF(vphaddbq),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*45*/
  {OP_vphaddwd,  0x09c618,STROFF(vphaddwd),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*46*/
  {OP_vphaddwq,  0x09c718,STROFF(vphaddwq),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*47*/
  {OP_vphadddq,  0x09cb18,STROFF(vphadddq),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*48*/
  /* AMD decode table erroneously lists this as "vphaddubwd" */
  {OP_vphaddubw, 0x09d118,STROFF(vphaddubw),  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*49*/
  {OP_vphaddubd, 0x09d218,STROFF(vphaddubd),  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*50*/
  {OP_vphaddubq, 0x09d318,STROFF(vphaddubq),  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*51*/
  {OP_vphadduwd, 0x09d618,STROFF(vphadduwd),  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*52*/
  {OP_vphadduwq, 0x09d718,STROFF(vphadduwq),  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*53*/
  {OP_vphaddudq, 0x09db18,STROFF(vphaddudq),  Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*54*/
  {OP_vphsubbw,  0x09e118,STROFF(vphsubbw),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*55*/
  {OP_vphsubwd,  0x09e218,STROFF(vphsubwd),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*56*/
  {OP_vphsubdq,  0x09e318,STROFF(vphsubdq),   Vdq,xx,Wdq,xx,xx,mrm|vex,x,END_LIST},   /*57*/
  {EXTENSION,    0x090118, STROFF(XOP_group_1), xx,xx, xx,xx,xx, mrm|vex, x, 27},   /*58*/
  {EXTENSION,    0x090218, STROFF(XOP_group_2), xx,xx, xx,xx,xx, mrm|vex, x, 28},   /*59*/
  /* XOP.map_select = 0x0a */
  {OP_bextr,     0x0a1018, STROFF(bextr),  Gy,xx,Ey,Id,xx, mrm|vex, fW6, END_LIST},   /*60*/
  /* Later-added instrs, from various tables */
  {EXTENSION,    0x091218, STROFF(XOP_group_3), xx,xx, xx,xx,xx, mrm|vex, x, 29},   /*61*/
  {EXTENSION,    0x0a1218, STROFF(XOP_group_4), xx,xx, xx,xx,xx, mrm|vex, x, 30},   /*62*/
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
    {OP_ins,      0x6c0000, STROFF(ins),       Yb, axDI, dx, axDI, xx, no, fRD, END_LIST},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_ins,  0xf36c0000, STROFF(rep_ins), Yb, axDI, dx, axDI, axCX, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf36c0000, STROFF(rep_ins), axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 1 */
    {OP_ins,      0x6d0000, STROFF(ins),       Yz, axDI, dx, axDI, xx, no, fRD, tre[0][0]},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_ins,  0xf36d0000, STROFF(rep_ins), Yz, axDI, dx, axDI, axCX, xop_next, fRD, tre[0][2]},
    {OP_CONTD,  0xf36d0000, STROFF(rep_ins), axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 2 */
    {OP_outs,      0x6e0000, STROFF(outs),       axSI, xx, Xb, dx, axSI, no, fRD, END_LIST},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_outs,  0xf36e0000, STROFF(rep_outs), axSI, axCX, Xb, dx, axSI, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf36e0000, STROFF(rep_outs), xx, xx, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 3 */
    {OP_outs,      0x6f0000, STROFF(outs),       axSI, xx, Xz, dx, axSI, no, fRD, tre[2][0]},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_outs,  0xf36f0000, STROFF(rep_outs), axSI, axCX, Xz, dx, axSI, xop_next, fRD, tre[2][2]},
    {OP_CONTD,  0xf36f0000, STROFF(rep_outs), xx, xx, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 4 */
    {OP_movs,      0xa40000, STROFF(movs),       Yb, axSI, Xb, axSI, axDI, xop_next, fRD, END_LIST},
    {OP_CONTD,      0xa40000, STROFF(movs),       axDI, xx, xx, xx, xx, no, fRD, END_LIST},
    {OP_rep_movs,  0xf3a40000, STROFF(rep_movs), Yb, axSI, Xb, axSI, axDI, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf3a40000, STROFF(rep_movs), axDI, axCX, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 5 */
    {OP_movs,      0xa50000, STROFF(movs),       Yv, axSI, Xv, axSI, axDI, xop_next, fRD, tre[4][0]},
    {OP_CONTD,      0xa50000, STROFF(movs),       axDI, xx, xx, xx, xx, no, fRD, END_LIST},
    {OP_rep_movs,  0xf3a50000, STROFF(rep_movs), Yv, axSI, Xv, axSI, axDI, xop_next, fRD, tre[4][2]},
    {OP_CONTD,  0xf3a50000, STROFF(rep_movs), axDI, axCX, axCX, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 6 */
    {OP_stos,      0xaa0000, STROFF(stos),       Yb, axDI, al, axDI, xx, no, fRD, END_LIST},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_stos,  0xf3aa0000, STROFF(rep_stos), Yb, axDI, al, axDI, axCX, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf3aa0000, STROFF(rep_stos), axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 7 */
    {OP_stos,      0xab0000, STROFF(stos),       Yv, axDI, eAX, axDI, xx, no, fRD, tre[6][0]},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_stos,  0xf3ab0000, STROFF(rep_stos), Yv, axDI, eAX, axDI, axCX, xop_next, fRD, tre[6][2]},
    {OP_CONTD,  0xf3ab0000, STROFF(rep_stos), axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 8 */
    {OP_lods,      0xac0000, STROFF(lods),       al, axSI, Xb, axSI, xx, no, fRD, END_LIST},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_lods,  0xf3ac0000, STROFF(rep_lods), al, axSI, Xb, axSI, axCX, xop_next, fRD, END_LIST},
    {OP_CONTD,  0xf3ac0000, STROFF(rep_lods), axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
  { /* rep extension 9 */
    {OP_lods,      0xad0000, STROFF(lods),       eAX, axSI, Xv, axSI, xx, no, fRD, tre[8][0]},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_lods,  0xf3ad0000, STROFF(rep_lods), eAX, axSI, Xv, axSI, axCX, xop_next, fRD, tre[8][2]},
    {OP_CONTD,  0xf3ad0000, STROFF(rep_lods), axCX, xx, xx, xx, xx, no, fRD, END_LIST},
  },
};

const instr_info_t repne_extensions[][6] = {
  { /* repne extension 0 */
    {OP_cmps,       0xa60000, STROFF(cmps),         axSI, axDI, Xb, Yb, axSI, xop_next, (fW6|fRD), END_LIST},
    {OP_CONTD,      0xa60000, STROFF(cmps),         xx, xx, axDI, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_rep_cmps,   0xf3a60000, STROFF(rep_cmps),   axSI, axDI, Xb, Yb, axSI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf3a60000, STROFF(rep_cmps),   axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_cmps, 0xf2a60000, STROFF(repne_cmps), axSI, axDI, Xb, Yb, axSI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf2a60000, STROFF(repne_cmps), axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
  },
  { /* repne extension 1 */
    {OP_cmps,       0xa70000, STROFF(cmps),         axSI, axDI, Xv, Yv, axSI, xop_next, (fW6|fRD), tne[0][0]},
    {OP_CONTD,      0xa70000, STROFF(cmps),         xx, xx, axDI, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_rep_cmps,   0xf3a70000, STROFF(rep_cmps),   axSI, axDI, Xv, Yv, axSI, xop_next, (fW6|fRD|fRZ), tne[0][2]},
    {OP_CONTD,      0xf3a70000, STROFF(rep_cmps),   axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_cmps, 0xf2a70000, STROFF(repne_cmps), axSI, axDI, Xv, Yv, axSI, xop_next, (fW6|fRD|fRZ), tne[0][4]},
    {OP_CONTD,      0xf2a70000, STROFF(repne_cmps), axCX, xx, axDI, axCX, xx, no, (fW6|fRD), END_LIST},
  },
  { /* repne extension 2 */
    {OP_scas,       0xae0000, STROFF(scas),         axDI, xx, Yb, al, axDI, no, (fW6|fRD), END_LIST},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_scas,   0xf3ae0000, STROFF(rep_scas),   axDI, axCX, Yb, al, axDI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf3ae0000, STROFF(rep_scas),   xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_scas, 0xf2ae0000, STROFF(repne_scas), axDI, axCX, Yb, al, axDI, xop_next, (fW6|fRD|fRZ), END_LIST},
    {OP_CONTD,      0xf2ae0000, STROFF(repne_scas), xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
  },
  { /* repne extension 3 */
    {OP_scas,       0xaf0000, STROFF(scas),         axDI, xx, Yv, eAX, axDI, no, (fW6|fRD), tne[2][0]},
    {INVALID,   0x00000000, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
    {OP_rep_scas,   0xf3af0000, STROFF(rep_scas),   axDI, axCX, Yv, eAX, axDI, xop_next, (fW6|fRD|fRZ), tne[2][2]},
    {OP_CONTD,      0xf3af0000, STROFF(rep_scas),   xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
    {OP_repne_scas, 0xf2af0000, STROFF(repne_scas), axDI, axCX, Yv, eAX, axDI, xop_next, (fW6|fRD|fRZ), tne[2][4]},
    {OP_CONTD,      0xf2af0000, STROFF(repne_scas), xx, xx, axCX, xx, xx, no, (fW6|fRD), END_LIST},
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
  {OP_fadd,  0xd80020, STROFF(fadd),  st0, xx, Fd, st0, xx, mrm, x, tfl[0x20]}, /* 00 */
  {OP_fmul,  0xd80021, STROFF(fmul),  st0, xx, Fd, st0, xx, mrm, x, tfl[0x21]},
  {OP_fcom,  0xd80022, STROFF(fcom),  xx, xx, Fd, st0, xx, mrm, x, tfl[0x22]},
  {OP_fcomp, 0xd80023, STROFF(fcomp), xx, xx, Fd, st0, xx, mrm, x, tfl[0x23]},
  {OP_fsub,  0xd80024, STROFF(fsub),  st0, xx, Fd, st0, xx, mrm, x, tfl[0x24]},
  {OP_fsubr, 0xd80025, STROFF(fsubr), st0, xx, Fd, st0, xx, mrm, x, tfl[0x25]},
  {OP_fdiv,  0xd80026, STROFF(fdiv),  st0, xx, Fd, st0, xx, mrm, x, tfl[0x26]},
  {OP_fdivr, 0xd80027, STROFF(fdivr), st0, xx, Fd, st0, xx, mrm, x, tfl[0x27]},
  /*  d9 */
  {OP_fld,    0xd90020, STROFF(fld),    st0, xx, Fd, xx, xx, mrm, x, tfl[0x1d]}, /* 08 */
  {INVALID,   0xd90021, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fst,    0xd90022, STROFF(fst),    Fd, xx, st0, xx, xx, mrm, x, tfl[0x2a]},
  {OP_fstp,   0xd90023, STROFF(fstp),   Fd, xx, st0, xx, xx, mrm, x, tfl[0x1f]},
  {OP_fldenv, 0xd90024, STROFF(fldenv), xx, xx, Ffy, xx, xx, mrm, x, END_LIST},
  {OP_fldcw,  0xd90025, STROFF(fldcw),  xx, xx, Fw, xx, xx, mrm, x, END_LIST},
  {OP_fnstenv, 0xd90026, STROFF(fnstenv), Ffy, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, this is "fstenv"*/
  {OP_fnstcw,  0xd90027, STROFF(fnstcw),  Fw, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, this is "fstcw"*/
  /* da */
  {OP_fiadd,  0xda0020, STROFF(fiadd),  st0, xx, Md, st0, xx, mrm, x, tfl[0x30]}, /* 10 */
  {OP_fimul,  0xda0021, STROFF(fimul),  st0, xx, Md, st0, xx, mrm, x, tfl[0x31]},
  {OP_ficom,  0xda0022, STROFF(ficom),  st0, xx, Md, st0, xx, mrm, x, tfl[0x32]},
  {OP_ficomp, 0xda0023, STROFF(ficomp), st0, xx, Md, st0, xx, mrm, x, tfl[0x33]},
  {OP_fisub,  0xda0024, STROFF(fisub),  st0, xx, Md, st0, xx, mrm, x, tfl[0x34]},
  {OP_fisubr, 0xda0025, STROFF(fisubr), st0, xx, Md, st0, xx, mrm, x, tfl[0x35]},
  {OP_fidiv,  0xda0026, STROFF(fidiv),  st0, xx, Md, st0, xx, mrm, x, tfl[0x36]},
  {OP_fidivr, 0xda0027, STROFF(fidivr), st0, xx, Md, st0, xx, mrm, x, tfl[0x37]},
  /* db */
  {OP_fild,  0xdb0020, STROFF(fild),  st0, xx, Md, xx, xx, mrm, x, tfl[0x38]}, /* 18 */
  {OP_fisttp, 0xdb0021, STROFF(fisttp),  Md, xx, st0, xx, xx, no, x, tfl[0x39]},
  {OP_fist,  0xdb0022, STROFF(fist),  Md, xx, st0, xx, xx, mrm, x, tfl[0x3a]},
  {OP_fistp, 0xdb0023, STROFF(fistp), Md, xx, st0, xx, xx, mrm, x, tfl[0x3b]},
  {INVALID,  0xdb0024, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fld,   0xdb0025, STROFF(fld),   st0, xx, Ffx, xx, xx, mrm, x, tfl[0x28]},
  {INVALID,  0xdb0026, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fstp,  0xdb0027, STROFF(fstp),  Ffx, xx, st0, xx, xx, mrm, x, tfl[0x2b]},
  /* dc */
  {OP_fadd,  0xdc0020, STROFF(fadd),  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x00]}, /* 20 */
  {OP_fmul,  0xdc0021, STROFF(fmul),  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x08]},
  {OP_fcom,  0xdc0022, STROFF(fcom),  xx, xx, Fq, st0, xx, mrm, x, tfh[0][0x10]},
  {OP_fcomp, 0xdc0023, STROFF(fcomp), xx, xx, Fq, st0, xx, mrm, x, tfh[0][0x18]},
  {OP_fsub,  0xdc0024, STROFF(fsub),  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x20]},
  {OP_fsubr, 0xdc0025, STROFF(fsubr), st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x28]},
  {OP_fdiv,  0xdc0026, STROFF(fdiv),  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x30]},
  {OP_fdivr, 0xdc0027, STROFF(fdivr), st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x38]},
  /* dd */
  {OP_fld,   0xdd0020, STROFF(fld),    st0, xx, Fq, xx, xx, mrm, x, tfh[1][0x00]}, /* 28 */
  {OP_fisttp, 0xdd0021, STROFF(fisttp),  Mq, xx, st0, xx, xx, no, x, tfl[0x19]},
  {OP_fst,   0xdd0022, STROFF(fst),    Fq, xx, st0, xx, xx, mrm, x, tfh[5][0x10]},
  {OP_fstp,  0xdd0023, STROFF(fstp),   Fq, xx, st0, xx, xx, mrm, x, tfh[5][0x18]},
  {OP_frstor,0xdd0024, STROFF(frstor), xx, xx, Ffz, xx, xx, mrm, x, END_LIST},
  {INVALID,  0xdd0025, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fnsave, 0xdd0026, STROFF(fnsave),  Ffz, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME:w/ preceding fwait instr, this is "fsave"*/
  {OP_fnstsw, 0xdd0027, STROFF(fnstsw),  Fw, xx, xx, xx, xx, mrm, x, tfh[7][0x20]},/*FIXME:w/ preceding fwait instr, this is "fstsw"*/
  /* de */
  {OP_fiadd,  0xde0020, STROFF(fiadd),  st0, xx, Fw, st0, xx, mrm, x, END_LIST}, /* 30 */
  {OP_fimul,  0xde0021, STROFF(fimul),  st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_ficom,  0xde0022, STROFF(ficom),  xx, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_ficomp, 0xde0023, STROFF(ficomp), xx, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fisub,  0xde0024, STROFF(fisub),  st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fisubr, 0xde0025, STROFF(fisubr), st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fidiv,  0xde0026, STROFF(fidiv),  st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fidivr, 0xde0027, STROFF(fidivr), st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  /* df */
  {OP_fild,   0xdf0020, STROFF(fild),    st0, xx, Fw, xx, xx, mrm, x, tfl[0x3d]}, /* 38 */
  {OP_fisttp, 0xdf0021, STROFF(fisttp),  Mw, xx, st0, xx, xx, no, x, END_LIST},
  {OP_fist,   0xdf0022, STROFF(fist),    Fw, xx, st0, xx, xx, mrm, x, END_LIST},
  {OP_fistp,  0xdf0023, STROFF(fistp),   Fw, xx, st0, xx, xx, mrm, x, tfl[0x3f]},
  {OP_fbld,   0xdf0024, STROFF(fbld),    st0, xx, Ffx, xx, xx, mrm, x, END_LIST},
  {OP_fild,   0xdf0025, STROFF(fild),    st0, xx, Fq, xx, xx, mrm, x, END_LIST},
  {OP_fbstp,  0xdf0026, STROFF(fbstp),   Ffx, xx, st0, xx, xx, mrm, x, END_LIST},
  {OP_fistp,  0xdf0027, STROFF(fistp),   Fq, xx, st0, xx, xx, mrm, x, END_LIST},
};

/****************************************************************************
 * Float instructions with ModR/M above 0xbf
 * This is from Tables A-8, A-10, A-12, A-14, A-16, A-18, A-20, A-22
 */
const instr_info_t float_high_modrm[][64] = {
    { /* d8 = [0] */
        {OP_fadd, 0xd8c010, STROFF(fadd), st0, xx, st0, st0, xx, mrm, x, tfh[0][0x01]}, /* c0 = [0x00] */
        {OP_fadd, 0xd8c110, STROFF(fadd), st0, xx, st1, st0, xx, mrm, x, tfh[0][0x02]},
        {OP_fadd, 0xd8c210, STROFF(fadd), st0, xx, st2, st0, xx, mrm, x, tfh[0][0x03]},
        {OP_fadd, 0xd8c310, STROFF(fadd), st0, xx, st3, st0, xx, mrm, x, tfh[0][0x04]},
        {OP_fadd, 0xd8c410, STROFF(fadd), st0, xx, st4, st0, xx, mrm, x, tfh[0][0x05]},
        {OP_fadd, 0xd8c510, STROFF(fadd), st0, xx, st5, st0, xx, mrm, x, tfh[0][0x06]},
        {OP_fadd, 0xd8c610, STROFF(fadd), st0, xx, st6, st0, xx, mrm, x, tfh[0][0x07]},
        {OP_fadd, 0xd8c710, STROFF(fadd), st0, xx, st7, st0, xx, mrm, x, tfh[4][0x00]},
        {OP_fmul, 0xd8c810, STROFF(fmul), st0, xx, st0, st0, xx, mrm, x, tfh[0][0x09]}, /* c8 = [0x08] */
        {OP_fmul, 0xd8c910, STROFF(fmul), st0, xx, st1, st0, xx, mrm, x, tfh[0][0x0a]},
        {OP_fmul, 0xd8ca10, STROFF(fmul), st0, xx, st2, st0, xx, mrm, x, tfh[0][0x0b]},
        {OP_fmul, 0xd8cb10, STROFF(fmul), st0, xx, st3, st0, xx, mrm, x, tfh[0][0x0c]},
        {OP_fmul, 0xd8cc10, STROFF(fmul), st0, xx, st4, st0, xx, mrm, x, tfh[0][0x0d]},
        {OP_fmul, 0xd8cd10, STROFF(fmul), st0, xx, st5, st0, xx, mrm, x, tfh[0][0x0e]},
        {OP_fmul, 0xd8ce10, STROFF(fmul), st0, xx, st6, st0, xx, mrm, x, tfh[0][0x0f]},
        {OP_fmul, 0xd8cf10, STROFF(fmul), st0, xx, st7, st0, xx, mrm, x, tfh[4][0x08]},
        {OP_fcom, 0xd8d010, STROFF(fcom), xx, xx, st0, st0, xx, mrm, x, tfh[0][0x11]}, /* d0 = [0x10] */
        {OP_fcom, 0xd8d110, STROFF(fcom), xx, xx, st0, st1, xx, mrm, x, tfh[0][0x12]},
        {OP_fcom, 0xd8d210, STROFF(fcom), xx, xx, st0, st2, xx, mrm, x, tfh[0][0x13]},
        {OP_fcom, 0xd8d310, STROFF(fcom), xx, xx, st0, st3, xx, mrm, x, tfh[0][0x14]},
        {OP_fcom, 0xd8d410, STROFF(fcom), xx, xx, st0, st4, xx, mrm, x, tfh[0][0x15]},
        {OP_fcom, 0xd8d510, STROFF(fcom), xx, xx, st0, st5, xx, mrm, x, tfh[0][0x16]},
        {OP_fcom, 0xd8d610, STROFF(fcom), xx, xx, st0, st6, xx, mrm, x, tfh[0][0x17]},
        {OP_fcom, 0xd8d710, STROFF(fcom), xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xd8d810, STROFF(fcomp), xx, xx, st0, st0, xx, mrm, x, tfh[0][0x19]}, /* d8 = [0x18] */
        {OP_fcomp, 0xd8d910, STROFF(fcomp), xx, xx, st0, st1, xx, mrm, x, tfh[0][0x1a]},
        {OP_fcomp, 0xd8da10, STROFF(fcomp), xx, xx, st0, st2, xx, mrm, x, tfh[0][0x1b]},
        {OP_fcomp, 0xd8db10, STROFF(fcomp), xx, xx, st0, st3, xx, mrm, x, tfh[0][0x1c]},
        {OP_fcomp, 0xd8dc10, STROFF(fcomp), xx, xx, st0, st4, xx, mrm, x, tfh[0][0x1d]},
        {OP_fcomp, 0xd8dd10, STROFF(fcomp), xx, xx, st0, st5, xx, mrm, x, tfh[0][0x1e]},
        {OP_fcomp, 0xd8de10, STROFF(fcomp), xx, xx, st0, st6, xx, mrm, x, tfh[0][0x1f]},
        {OP_fcomp, 0xd8df10, STROFF(fcomp), xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsub, 0xd8e010, STROFF(fsub), st0, xx, st0, st0, xx, mrm, x, tfh[0][0x21]}, /* e0 = [0x20] */
        {OP_fsub, 0xd8e110, STROFF(fsub), st0, xx, st1, st0, xx, mrm, x, tfh[0][0x22]},
        {OP_fsub, 0xd8e210, STROFF(fsub), st0, xx, st2, st0, xx, mrm, x, tfh[0][0x23]},
        {OP_fsub, 0xd8e310, STROFF(fsub), st0, xx, st3, st0, xx, mrm, x, tfh[0][0x24]},
        {OP_fsub, 0xd8e410, STROFF(fsub), st0, xx, st4, st0, xx, mrm, x, tfh[0][0x25]},
        {OP_fsub, 0xd8e510, STROFF(fsub), st0, xx, st5, st0, xx, mrm, x, tfh[0][0x26]},
        {OP_fsub, 0xd8e610, STROFF(fsub), st0, xx, st6, st0, xx, mrm, x, tfh[0][0x27]},
        {OP_fsub, 0xd8e710, STROFF(fsub), st0, xx, st7, st0, xx, mrm, x, tfh[4][0x28]},
        {OP_fsubr, 0xd8e810, STROFF(fsubr), st0, xx, st0, st0, xx, mrm, x, tfh[0][0x29]}, /* e8 = [0x28] */
        {OP_fsubr, 0xd8e910, STROFF(fsubr), st0, xx, st1, st0, xx, mrm, x, tfh[0][0x2a]},
        {OP_fsubr, 0xd8ea10, STROFF(fsubr), st0, xx, st2, st0, xx, mrm, x, tfh[0][0x2b]},
        {OP_fsubr, 0xd8eb10, STROFF(fsubr), st0, xx, st3, st0, xx, mrm, x, tfh[0][0x2c]},
        {OP_fsubr, 0xd8ec10, STROFF(fsubr), st0, xx, st4, st0, xx, mrm, x, tfh[0][0x2d]},
        {OP_fsubr, 0xd8ed10, STROFF(fsubr), st0, xx, st5, st0, xx, mrm, x, tfh[0][0x2e]},
        {OP_fsubr, 0xd8ee10, STROFF(fsubr), st0, xx, st6, st0, xx, mrm, x, tfh[0][0x2f]},
        {OP_fsubr, 0xd8ef10, STROFF(fsubr), st0, xx, st7, st0, xx, mrm, x, tfh[4][0x20]},
        {OP_fdiv, 0xd8f010, STROFF(fdiv), st0, xx, st0, st0, xx, mrm, x, tfh[0][0x31]}, /* f0 = [0x30] */
        {OP_fdiv, 0xd8f110, STROFF(fdiv), st0, xx, st1, st0, xx, mrm, x, tfh[0][0x32]},
        {OP_fdiv, 0xd8f210, STROFF(fdiv), st0, xx, st2, st0, xx, mrm, x, tfh[0][0x33]},
        {OP_fdiv, 0xd8f310, STROFF(fdiv), st0, xx, st3, st0, xx, mrm, x, tfh[0][0x34]},
        {OP_fdiv, 0xd8f410, STROFF(fdiv), st0, xx, st4, st0, xx, mrm, x, tfh[0][0x35]},
        {OP_fdiv, 0xd8f510, STROFF(fdiv), st0, xx, st5, st0, xx, mrm, x, tfh[0][0x36]},
        {OP_fdiv, 0xd8f610, STROFF(fdiv), st0, xx, st6, st0, xx, mrm, x, tfh[0][0x37]},
        {OP_fdiv, 0xd8f710, STROFF(fdiv), st0, xx, st7, st0, xx, mrm, x, tfh[4][0x38]},
        {OP_fdivr, 0xd8f810, STROFF(fdivr), st0, xx, st0, st0, xx, mrm, x, tfh[0][0x39]}, /* f8 = [0x38] */
        {OP_fdivr, 0xd8f910, STROFF(fdivr), st0, xx, st1, st0, xx, mrm, x, tfh[0][0x3a]},
        {OP_fdivr, 0xd8fa10, STROFF(fdivr), st0, xx, st2, st0, xx, mrm, x, tfh[0][0x3b]},
        {OP_fdivr, 0xd8fb10, STROFF(fdivr), st0, xx, st3, st0, xx, mrm, x, tfh[0][0x3c]},
        {OP_fdivr, 0xd8fc10, STROFF(fdivr), st0, xx, st4, st0, xx, mrm, x, tfh[0][0x3d]},
        {OP_fdivr, 0xd8fd10, STROFF(fdivr), st0, xx, st5, st0, xx, mrm, x, tfh[0][0x3e]},
        {OP_fdivr, 0xd8fe10, STROFF(fdivr), st0, xx, st6, st0, xx, mrm, x, tfh[0][0x3f]},
        {OP_fdivr, 0xd8ff10, STROFF(fdivr), st0, xx, st7, st0, xx, mrm, x, tfh[4][0x30]},
   },
    { /* d9 = [1] */
        {OP_fld, 0xd9c010, STROFF(fld), st0, xx, st0, xx, xx, mrm, x, tfh[1][0x01]}, /* c0 = [0x00] */
        {OP_fld, 0xd9c110, STROFF(fld), st0, xx, st1, xx, xx, mrm, x, tfh[1][0x02]},
        {OP_fld, 0xd9c210, STROFF(fld), st0, xx, st2, xx, xx, mrm, x, tfh[1][0x03]},
        {OP_fld, 0xd9c310, STROFF(fld), st0, xx, st3, xx, xx, mrm, x, tfh[1][0x04]},
        {OP_fld, 0xd9c410, STROFF(fld), st0, xx, st4, xx, xx, mrm, x, tfh[1][0x05]},
        {OP_fld, 0xd9c510, STROFF(fld), st0, xx, st5, xx, xx, mrm, x, tfh[1][0x06]},
        {OP_fld, 0xd9c610, STROFF(fld), st0, xx, st6, xx, xx, mrm, x, tfh[1][0x07]},
        {OP_fld, 0xd9c710, STROFF(fld), st0, xx, st7, xx, xx, mrm, x, END_LIST},
        {OP_fxch, 0xd9c810, STROFF(fxch), st0, st0, st0, st0, xx, mrm, x, tfh[1][0x09]}, /* c8 = [0x08] */
        {OP_fxch, 0xd9c910, STROFF(fxch), st0, st1, st0, st1, xx, mrm, x, tfh[1][0x0a]},
        {OP_fxch, 0xd9ca10, STROFF(fxch), st0, st2, st0, st2, xx, mrm, x, tfh[1][0x0b]},
        {OP_fxch, 0xd9cb10, STROFF(fxch), st0, st3, st0, st3, xx, mrm, x, tfh[1][0x0c]},
        {OP_fxch, 0xd9cc10, STROFF(fxch), st0, st4, st0, st4, xx, mrm, x, tfh[1][0x0d]},
        {OP_fxch, 0xd9cd10, STROFF(fxch), st0, st5, st0, st5, xx, mrm, x, tfh[1][0x0e]},
        {OP_fxch, 0xd9ce10, STROFF(fxch), st0, st6, st0, st6, xx, mrm, x, tfh[1][0x0f]},
        {OP_fxch, 0xd9cf10, STROFF(fxch), st0, st7, st0, st7, xx, mrm, x, END_LIST},
        {OP_fnop, 0xd9d010, STROFF(fnop), xx, xx, xx, xx, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {INVALID, 0xd9d110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xd9d710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        /* Undocumented.  On sandpile.org as "fstp1".  We assume an alias for fstp
         * and do not include in the encode chain.
         */
        {OP_fstp, 0xd9d810, STROFF(fstp), st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* d8 = [0x18] */
        {OP_fstp, 0xd9d910, STROFF(fstp), st1, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9da10, STROFF(fstp), st2, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9db10, STROFF(fstp), st3, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9dc10, STROFF(fstp), st4, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9dd10, STROFF(fstp), st5, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9de10, STROFF(fstp), st6, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xd9df10, STROFF(fstp), st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fchs,   0xd9e010, STROFF(fchs),   st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* e0 = [0x20] */
        {OP_fabs,   0xd9e110, STROFF(fabs),   st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {INVALID,   0xd9e210, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
        {INVALID,   0xd9e310, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
        {OP_ftst,   0xd9e410, STROFF(ftst),   st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fxam,   0xd9e510, STROFF(fxam),   xx, xx, st0, xx, xx, mrm, x, END_LIST},
        {INVALID,   0xd9e610, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
        {INVALID,   0xd9e710, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
        {OP_fld1,   0xd9e810, STROFF(fld1),   st0, xx, cF, xx, xx, mrm, x, END_LIST}, /* e8 = [0x28] */
        {OP_fldl2t, 0xd9e910, STROFF(fldl2t), st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldl2e, 0xd9ea10, STROFF(fldl2e), st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldpi,  0xd9eb10, STROFF(fldpi),  st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldlg2, 0xd9ec10, STROFF(fldlg2), st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldln2, 0xd9ed10, STROFF(fldln2), st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {OP_fldz,   0xd9ee10, STROFF(fldz),   st0, xx, cF, xx, xx, mrm, x, END_LIST},
        {INVALID,   0xd9ef10, STROFF(BAD),  xx, xx, xx, xx, xx, no, x, NA},
        {OP_f2xm1,  0xd9f010, STROFF(f2xm1),  st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* f0 = [0x30] */
        {OP_fyl2x,  0xd9f110, STROFF(fyl2x),  st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fptan,  0xd9f210, STROFF(fptan),  st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fpatan, 0xd9f310, STROFF(fpatan), st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fxtract,0xd9f410, STROFF(fxtract),st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fprem1, 0xd9f510, STROFF(fprem1), st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fdecstp,0xd9f610, STROFF(fdecstp), xx, xx, xx, xx, xx, mrm, x, END_LIST},
        {OP_fincstp,0xd9f710, STROFF(fincstp), xx, xx, xx, xx, xx, mrm, x, END_LIST},
        {OP_fprem,  0xd9f810, STROFF(fprem),  st0, st1, st0, st1, xx, mrm, x, END_LIST}, /* f8 = [0x38] */
        {OP_fyl2xp1,0xd9f910, STROFF(fyl2xp1),st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fsqrt,  0xd9fa10, STROFF(fsqrt),  st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fsincos,0xd9fb10, STROFF(fsincos),st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_frndint,0xd9fc10, STROFF(frndint),st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fscale, 0xd9fd10, STROFF(fscale), st0, xx, st1, st0, xx, mrm, x, END_LIST},
        {OP_fsin,   0xd9fe10, STROFF(fsin),   st0, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fcos,   0xd9ff10, STROFF(fcos),   st0, xx, st0, xx, xx, mrm, x, END_LIST},
   },
    { /* da = [2] */
        {OP_fcmovb, 0xdac010, STROFF(fcmovb), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x01]}, /* c0 = [0x00] */
        {OP_fcmovb, 0xdac110, STROFF(fcmovb), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x02]},
        {OP_fcmovb, 0xdac210, STROFF(fcmovb), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x03]},
        {OP_fcmovb, 0xdac310, STROFF(fcmovb), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x04]},
        {OP_fcmovb, 0xdac410, STROFF(fcmovb), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x05]},
        {OP_fcmovb, 0xdac510, STROFF(fcmovb), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x06]},
        {OP_fcmovb, 0xdac610, STROFF(fcmovb), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x07]},
        {OP_fcmovb, 0xdac710, STROFF(fcmovb), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmove, 0xdac810, STROFF(fcmove), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x09]}, /* c8 = [0x08] */
        {OP_fcmove, 0xdac910, STROFF(fcmove), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0a]},
        {OP_fcmove, 0xdaca10, STROFF(fcmove), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0b]},
        {OP_fcmove, 0xdacb10, STROFF(fcmove), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0c]},
        {OP_fcmove, 0xdacc10, STROFF(fcmove), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0d]},
        {OP_fcmove, 0xdacd10, STROFF(fcmove), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0e]},
        {OP_fcmove, 0xdace10, STROFF(fcmove), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x0f]},
        {OP_fcmove, 0xdacf10, STROFF(fcmove), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovbe, 0xdad010, STROFF(fcmovbe), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x11]}, /* d0 = [0x10] */
        {OP_fcmovbe, 0xdad110, STROFF(fcmovbe), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x12]},
        {OP_fcmovbe, 0xdad210, STROFF(fcmovbe), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x13]},
        {OP_fcmovbe, 0xdad310, STROFF(fcmovbe), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x14]},
        {OP_fcmovbe, 0xdad410, STROFF(fcmovbe), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x15]},
        {OP_fcmovbe, 0xdad510, STROFF(fcmovbe), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x16]},
        {OP_fcmovbe, 0xdad610, STROFF(fcmovbe), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x17]},
        {OP_fcmovbe, 0xdad710, STROFF(fcmovbe), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovu, 0xdad810, STROFF(fcmovu), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x19]}, /* d8 = [0x18] */
        {OP_fcmovu, 0xdad910, STROFF(fcmovu), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1a]},
        {OP_fcmovu, 0xdada10, STROFF(fcmovu), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1b]},
        {OP_fcmovu, 0xdadb10, STROFF(fcmovu), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1c]},
        {OP_fcmovu, 0xdadc10, STROFF(fcmovu), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1d]},
        {OP_fcmovu, 0xdadd10, STROFF(fcmovu), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1e]},
        {OP_fcmovu, 0xdade10, STROFF(fcmovu), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[2][0x1f]},
        {OP_fcmovu, 0xdadf10, STROFF(fcmovu), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {INVALID, 0xdae010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* e0 = [0x20] */
        {INVALID, 0xdae110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdae810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* e8 = [0x28] */
        {OP_fucompp, 0xdae910, STROFF(fucompp), xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {INVALID, 0xdaea10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaeb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaec10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaed10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaee10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaef10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* f0 = [0x30] */
        {INVALID, 0xdaf110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaf810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xdaf910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdafe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdaff10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
   },
    { /* db = [3] */
        {OP_fcmovnb, 0xdbc010, STROFF(fcmovnb), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x01]}, /* c0 = [0x00] */
        {OP_fcmovnb, 0xdbc110, STROFF(fcmovnb), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x02]},
        {OP_fcmovnb, 0xdbc210, STROFF(fcmovnb), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x03]},
        {OP_fcmovnb, 0xdbc310, STROFF(fcmovnb), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x04]},
        {OP_fcmovnb, 0xdbc410, STROFF(fcmovnb), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x05]},
        {OP_fcmovnb, 0xdbc510, STROFF(fcmovnb), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x06]},
        {OP_fcmovnb, 0xdbc610, STROFF(fcmovnb), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x07]},
        {OP_fcmovnb, 0xdbc710, STROFF(fcmovnb), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovne, 0xdbc810, STROFF(fcmovne), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x09]}, /* c8 = [0x08] */
        {OP_fcmovne, 0xdbc910, STROFF(fcmovne), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0a]},
        {OP_fcmovne, 0xdbca10, STROFF(fcmovne), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0b]},
        {OP_fcmovne, 0xdbcb10, STROFF(fcmovne), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0c]},
        {OP_fcmovne, 0xdbcc10, STROFF(fcmovne), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0d]},
        {OP_fcmovne, 0xdbcd10, STROFF(fcmovne), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0e]},
        {OP_fcmovne, 0xdbce10, STROFF(fcmovne), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x0f]},
        {OP_fcmovne, 0xdbcf10, STROFF(fcmovne), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovnbe, 0xdbd010, STROFF(fcmovnbe), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x12]}, /* d0 = [0x10] */
        {OP_fcmovnbe, 0xdbd110, STROFF(fcmovnbe), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x12]},
        {OP_fcmovnbe, 0xdbd210, STROFF(fcmovnbe), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x13]},
        {OP_fcmovnbe, 0xdbd310, STROFF(fcmovnbe), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x14]},
        {OP_fcmovnbe, 0xdbd410, STROFF(fcmovnbe), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x15]},
        {OP_fcmovnbe, 0xdbd510, STROFF(fcmovnbe), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x16]},
        {OP_fcmovnbe, 0xdbd610, STROFF(fcmovnbe), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x17]},
        {OP_fcmovnbe, 0xdbd710, STROFF(fcmovnbe), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {OP_fcmovnu, 0xdbd810, STROFF(fcmovnu), st0, xx, st0, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x19]}, /* d8 = [0x18] */
        {OP_fcmovnu, 0xdbd910, STROFF(fcmovnu), st0, xx, st1, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1a]},
        {OP_fcmovnu, 0xdbda10, STROFF(fcmovnu), st0, xx, st2, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1b]},
        {OP_fcmovnu, 0xdbdb10, STROFF(fcmovnu), st0, xx, st3, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1c]},
        {OP_fcmovnu, 0xdbdc10, STROFF(fcmovnu), st0, xx, st4, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1d]},
        {OP_fcmovnu, 0xdbdd10, STROFF(fcmovnu), st0, xx, st5, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1e]},
        {OP_fcmovnu, 0xdbde10, STROFF(fcmovnu), st0, xx, st6, xx, xx, mrm|predcc, (fRC|fRP|fRZ), tfh[3][0x1f]},
        {OP_fcmovnu, 0xdbdf10, STROFF(fcmovnu), st0, xx, st7, xx, xx, mrm|predcc, (fRC|fRP|fRZ), END_LIST},
        {INVALID, 0xdbe010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* e0 = [0x20] */
        {INVALID, 0xdbe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {OP_fnclex, 0xdbe210, STROFF(fnclex), xx, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, called "fclex"*/
        {OP_fninit, 0xdbe310, STROFF(fninit), xx, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, called "finit"*/
        {INVALID, 0xdbe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbe610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {OP_fucomi, 0xdbe810, STROFF(fucomi), xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x29]}, /* e8 = [0x28] */
        {OP_fucomi, 0xdbe910, STROFF(fucomi), xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2a]},
        {OP_fucomi, 0xdbea10, STROFF(fucomi), xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2b]},
        {OP_fucomi, 0xdbeb10, STROFF(fucomi), xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2c]},
        {OP_fucomi, 0xdbec10, STROFF(fucomi), xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2d]},
        {OP_fucomi, 0xdbed10, STROFF(fucomi), xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2e]},
        {OP_fucomi, 0xdbee10, STROFF(fucomi), xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x2f]},
        {OP_fucomi, 0xdbef10, STROFF(fucomi), xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {OP_fcomi, 0xdbf010, STROFF(fcomi), xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x31]}, /* f0 = [0x30] */
        {OP_fcomi, 0xdbf110, STROFF(fcomi), xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x32]},
        {OP_fcomi, 0xdbf210, STROFF(fcomi), xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x33]},
        {OP_fcomi, 0xdbf310, STROFF(fcomi), xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x34]},
        {OP_fcomi, 0xdbf410, STROFF(fcomi), xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x35]},
        {OP_fcomi, 0xdbf510, STROFF(fcomi), xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x36]},
        {OP_fcomi, 0xdbf610, STROFF(fcomi), xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[3][0x37]},
        {OP_fcomi, 0xdbf710, STROFF(fcomi), xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {INVALID, 0xdbf810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xdbf910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbfe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdbff10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
   },
    { /* dc = [4] */
        {OP_fadd, 0xdcc010, STROFF(fadd), st0, xx, st0, st0, xx, mrm, x, tfh[4][0x01]}, /* c0 = [0x00] */
        {OP_fadd, 0xdcc110, STROFF(fadd), st1, xx, st0, st1, xx, mrm, x, tfh[4][0x02]},
        {OP_fadd, 0xdcc210, STROFF(fadd), st2, xx, st0, st2, xx, mrm, x, tfh[4][0x03]},
        {OP_fadd, 0xdcc310, STROFF(fadd), st3, xx, st0, st3, xx, mrm, x, tfh[4][0x04]},
        {OP_fadd, 0xdcc410, STROFF(fadd), st4, xx, st0, st4, xx, mrm, x, tfh[4][0x05]},
        {OP_fadd, 0xdcc510, STROFF(fadd), st5, xx, st0, st5, xx, mrm, x, tfh[4][0x06]},
        {OP_fadd, 0xdcc610, STROFF(fadd), st6, xx, st0, st6, xx, mrm, x, tfh[4][0x07]},
        {OP_fadd, 0xdcc710, STROFF(fadd), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fmul, 0xdcc810, STROFF(fmul), st0, xx, st0, st0, xx, mrm, x, tfh[4][0x09]}, /* c8 = [0x08] */
        {OP_fmul, 0xdcc910, STROFF(fmul), st1, xx, st0, st1, xx, mrm, x, tfh[4][0x0a]},
        {OP_fmul, 0xdcca10, STROFF(fmul), st2, xx, st0, st2, xx, mrm, x, tfh[4][0x0b]},
        {OP_fmul, 0xdccb10, STROFF(fmul), st3, xx, st0, st3, xx, mrm, x, tfh[4][0x0c]},
        {OP_fmul, 0xdccc10, STROFF(fmul), st4, xx, st0, st4, xx, mrm, x, tfh[4][0x0d]},
        {OP_fmul, 0xdccd10, STROFF(fmul), st5, xx, st0, st5, xx, mrm, x, tfh[4][0x0e]},
        {OP_fmul, 0xdcce10, STROFF(fmul), st6, xx, st0, st6, xx, mrm, x, tfh[4][0x0f]},
        {OP_fmul, 0xdccf10, STROFF(fmul), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fcom2".  We assume an alias for fcom
         * and do not include in the encode chain.
         */
        {OP_fcom, 0xdcd010, STROFF(fcom), xx, xx, st0, st0, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {OP_fcom, 0xdcd110, STROFF(fcom), xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd210, STROFF(fcom), xx, xx, st0, st2, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd310, STROFF(fcom), xx, xx, st0, st3, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd410, STROFF(fcom), xx, xx, st0, st4, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd510, STROFF(fcom), xx, xx, st0, st5, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd610, STROFF(fcom), xx, xx, st0, st6, xx, mrm, x, END_LIST},
        {OP_fcom, 0xdcd710, STROFF(fcom), xx, xx, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fcomp3".  We assume an alias for fcomp
         * and do not include in the encode chain.
         */
        {OP_fcomp, 0xdcd810, STROFF(fcomp), xx, xx, st0, st0, xx, mrm, x, END_LIST}, /* d8 = [0x18] */
        {OP_fcomp, 0xdcd910, STROFF(fcomp), xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcda10, STROFF(fcomp), xx, xx, st0, st2, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdb10, STROFF(fcomp), xx, xx, st0, st3, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdc10, STROFF(fcomp), xx, xx, st0, st4, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdd10, STROFF(fcomp), xx, xx, st0, st5, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcde10, STROFF(fcomp), xx, xx, st0, st6, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xdcdf10, STROFF(fcomp), xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsubr, 0xdce010, STROFF(fsubr), st0, xx, st0, st0, xx, mrm, x, tfh[4][0x21]}, /* e0 = [0x20] */
        {OP_fsubr, 0xdce110, STROFF(fsubr), st1, xx, st0, st1, xx, mrm, x, tfh[4][0x22]},
        {OP_fsubr, 0xdce210, STROFF(fsubr), st2, xx, st0, st2, xx, mrm, x, tfh[4][0x23]},
        {OP_fsubr, 0xdce310, STROFF(fsubr), st3, xx, st0, st3, xx, mrm, x, tfh[4][0x24]},
        {OP_fsubr, 0xdce410, STROFF(fsubr), st4, xx, st0, st4, xx, mrm, x, tfh[4][0x25]},
        {OP_fsubr, 0xdce510, STROFF(fsubr), st5, xx, st0, st5, xx, mrm, x, tfh[4][0x26]},
        {OP_fsubr, 0xdce610, STROFF(fsubr), st6, xx, st0, st6, xx, mrm, x, tfh[4][0x27]},
        {OP_fsubr, 0xdce710, STROFF(fsubr), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsub, 0xdce810, STROFF(fsub), st0, xx, st0, st0, xx, mrm, x, tfh[4][0x29]}, /* e8 = [0x28] */
        {OP_fsub, 0xdce910, STROFF(fsub), st1, xx, st0, st1, xx, mrm, x, tfh[4][0x2a]},
        {OP_fsub, 0xdcea10, STROFF(fsub), st2, xx, st0, st2, xx, mrm, x, tfh[4][0x2b]},
        {OP_fsub, 0xdceb10, STROFF(fsub), st3, xx, st0, st3, xx, mrm, x, tfh[4][0x2c]},
        {OP_fsub, 0xdcec10, STROFF(fsub), st4, xx, st0, st4, xx, mrm, x, tfh[4][0x2d]},
        {OP_fsub, 0xdced10, STROFF(fsub), st5, xx, st0, st5, xx, mrm, x, tfh[4][0x2e]},
        {OP_fsub, 0xdcee10, STROFF(fsub), st6, xx, st0, st6, xx, mrm, x, tfh[4][0x2f]},
        {OP_fsub, 0xdcef10, STROFF(fsub), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdivr, 0xdcf010, STROFF(fdivr), st0, xx, st0, st0, xx, mrm, x, tfh[4][0x31]}, /* f0 = [0x30] */
        {OP_fdivr, 0xdcf110, STROFF(fdivr), st1, xx, st0, st1, xx, mrm, x, tfh[4][0x32]},
        {OP_fdivr, 0xdcf210, STROFF(fdivr), st2, xx, st0, st2, xx, mrm, x, tfh[4][0x33]},
        {OP_fdivr, 0xdcf310, STROFF(fdivr), st3, xx, st0, st3, xx, mrm, x, tfh[4][0x34]},
        {OP_fdivr, 0xdcf410, STROFF(fdivr), st4, xx, st0, st4, xx, mrm, x, tfh[4][0x35]},
        {OP_fdivr, 0xdcf510, STROFF(fdivr), st5, xx, st0, st5, xx, mrm, x, tfh[4][0x36]},
        {OP_fdivr, 0xdcf610, STROFF(fdivr), st6, xx, st0, st6, xx, mrm, x, tfh[4][0x37]},
        {OP_fdivr, 0xdcf710, STROFF(fdivr), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdiv, 0xdcf810, STROFF(fdiv), st0, xx, st0, st0, xx, mrm, x, tfh[4][0x39]}, /* f8 = [0x38] */
        {OP_fdiv, 0xdcf910, STROFF(fdiv), st1, xx, st0, st1, xx, mrm, x, tfh[4][0x3a]},
        {OP_fdiv, 0xdcfa10, STROFF(fdiv), st2, xx, st0, st2, xx, mrm, x, tfh[4][0x3b]},
        {OP_fdiv, 0xdcfb10, STROFF(fdiv), st3, xx, st0, st3, xx, mrm, x, tfh[4][0x3c]},
        {OP_fdiv, 0xdcfc10, STROFF(fdiv), st4, xx, st0, st4, xx, mrm, x, tfh[4][0x3d]},
        {OP_fdiv, 0xdcfd10, STROFF(fdiv), st5, xx, st0, st5, xx, mrm, x, tfh[4][0x3e]},
        {OP_fdiv, 0xdcfe10, STROFF(fdiv), st6, xx, st0, st6, xx, mrm, x, tfh[4][0x3f]},
        {OP_fdiv, 0xdcff10, STROFF(fdiv), st7, xx, st0, st7, xx, mrm, x, END_LIST},
   },
    { /* dd = [5] */
        {OP_ffree, 0xddc010, STROFF(ffree), st0, xx, xx, xx, xx, mrm, x, tfh[5][0x01]}, /* c0 = [0x00] */
        {OP_ffree, 0xddc110, STROFF(ffree), st1, xx, xx, xx, xx, mrm, x, tfh[5][0x02]},
        {OP_ffree, 0xddc210, STROFF(ffree), st2, xx, xx, xx, xx, mrm, x, tfh[5][0x03]},
        {OP_ffree, 0xddc310, STROFF(ffree), st3, xx, xx, xx, xx, mrm, x, tfh[5][0x04]},
        {OP_ffree, 0xddc410, STROFF(ffree), st4, xx, xx, xx, xx, mrm, x, tfh[5][0x05]},
        {OP_ffree, 0xddc510, STROFF(ffree), st5, xx, xx, xx, xx, mrm, x, tfh[5][0x06]},
        {OP_ffree, 0xddc610, STROFF(ffree), st6, xx, xx, xx, xx, mrm, x, tfh[5][0x07]},
        {OP_ffree, 0xddc710, STROFF(ffree), st7, xx, xx, xx, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fxch4".  We assume an alias for fxch
         * and do not include in the encode chain.
         */
        {OP_fxch, 0xddc810, STROFF(fxch), st0, st0, st0, st0, xx, mrm, x, END_LIST}, /* c8 = [0x08] */
        {OP_fxch, 0xddc910, STROFF(fxch), st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddca10, STROFF(fxch), st0, st2, st0, st2, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcb10, STROFF(fxch), st0, st3, st0, st3, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcc10, STROFF(fxch), st0, st4, st0, st4, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcd10, STROFF(fxch), st0, st5, st0, st5, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddce10, STROFF(fxch), st0, st6, st0, st6, xx, mrm, x, END_LIST},
        {OP_fxch, 0xddcf10, STROFF(fxch), st0, st7, st0, st7, xx, mrm, x, END_LIST},
        {OP_fst, 0xddd010, STROFF(fst), st0, xx, st0, xx, xx, mrm, x, tfh[5][0x11]}, /* d0 = [0x10] */
        {OP_fst, 0xddd110, STROFF(fst), st1, xx, st0, xx, xx, mrm, x, tfh[5][0x12]},
        {OP_fst, 0xddd210, STROFF(fst), st2, xx, st0, xx, xx, mrm, x, tfh[5][0x13]},
        {OP_fst, 0xddd310, STROFF(fst), st3, xx, st0, xx, xx, mrm, x, tfh[5][0x14]},
        {OP_fst, 0xddd410, STROFF(fst), st4, xx, st0, xx, xx, mrm, x, tfh[5][0x15]},
        {OP_fst, 0xddd510, STROFF(fst), st5, xx, st0, xx, xx, mrm, x, tfh[5][0x16]},
        {OP_fst, 0xddd610, STROFF(fst), st6, xx, st0, xx, xx, mrm, x, tfh[5][0x17]},
        {OP_fst, 0xddd710, STROFF(fst), st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xddd810, STROFF(fstp), st0, xx, st0, xx, xx, mrm, x, tfh[5][0x19]}, /* d8 = [0x18] */
        {OP_fstp, 0xddd910, STROFF(fstp), st1, xx, st0, xx, xx, mrm, x, tfh[5][0x1a]},
        {OP_fstp, 0xddda10, STROFF(fstp), st2, xx, st0, xx, xx, mrm, x, tfh[5][0x1b]},
        {OP_fstp, 0xdddb10, STROFF(fstp), st3, xx, st0, xx, xx, mrm, x, tfh[5][0x1c]},
        {OP_fstp, 0xdddc10, STROFF(fstp), st4, xx, st0, xx, xx, mrm, x, tfh[5][0x1d]},
        {OP_fstp, 0xdddd10, STROFF(fstp), st5, xx, st0, xx, xx, mrm, x, tfh[5][0x1e]},
        {OP_fstp, 0xddde10, STROFF(fstp), st6, xx, st0, xx, xx, mrm, x, tfh[5][0x1f]},
        {OP_fstp, 0xdddf10, STROFF(fstp), st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fucom, 0xdde010, STROFF(fucom), xx, xx, st0, st0, xx, mrm, x, tfh[5][0x21]}, /* e0 = [0x20] */
        {OP_fucom, 0xdde110, STROFF(fucom), xx, xx, st1, st0, xx, mrm, x, tfh[5][0x22]},
        {OP_fucom, 0xdde210, STROFF(fucom), xx, xx, st2, st0, xx, mrm, x, tfh[5][0x23]},
        {OP_fucom, 0xdde310, STROFF(fucom), xx, xx, st3, st0, xx, mrm, x, tfh[5][0x24]},
        {OP_fucom, 0xdde410, STROFF(fucom), xx, xx, st4, st0, xx, mrm, x, tfh[5][0x25]},
        {OP_fucom, 0xdde510, STROFF(fucom), xx, xx, st5, st0, xx, mrm, x, tfh[5][0x26]},
        {OP_fucom, 0xdde610, STROFF(fucom), xx, xx, st6, st0, xx, mrm, x, tfh[5][0x27]},
        {OP_fucom, 0xdde710, STROFF(fucom), xx, xx, st7, st0, xx, mrm, x, END_LIST},
        {OP_fucomp, 0xdde810, STROFF(fucomp), xx, xx, st0, st0, xx, mrm, x, tfh[5][0x29]}, /* e8 = [0x28] */
        {OP_fucomp, 0xdde910, STROFF(fucomp), xx, xx, st1, st0, xx, mrm, x, tfh[5][0x2a]},
        {OP_fucomp, 0xddea10, STROFF(fucomp), xx, xx, st2, st0, xx, mrm, x, tfh[5][0x2b]},
        {OP_fucomp, 0xddeb10, STROFF(fucomp), xx, xx, st3, st0, xx, mrm, x, tfh[5][0x2c]},
        {OP_fucomp, 0xddec10, STROFF(fucomp), xx, xx, st4, st0, xx, mrm, x, tfh[5][0x2d]},
        {OP_fucomp, 0xdded10, STROFF(fucomp), xx, xx, st5, st0, xx, mrm, x, tfh[5][0x2e]},
        {OP_fucomp, 0xddee10, STROFF(fucomp), xx, xx, st6, st0, xx, mrm, x, tfh[5][0x2f]},
        {OP_fucomp, 0xddef10, STROFF(fucomp), xx, xx, st7, st0, xx, mrm, x, END_LIST},
        {INVALID, 0xddf010, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* f0 = [0x30] */
        {INVALID, 0xddf110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddf810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xddf910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddfe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xddff10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
   },
    { /* de = [6]*/
        {OP_faddp, 0xdec010, STROFF(faddp), st0, xx, st0, st0, xx, mrm, x, tfh[6][0x01]}, /* c0 = [0x00] */
        {OP_faddp, 0xdec110, STROFF(faddp), st1, xx, st0, st1, xx, mrm, x, tfh[6][0x02]},
        {OP_faddp, 0xdec210, STROFF(faddp), st2, xx, st0, st2, xx, mrm, x, tfh[6][0x03]},
        {OP_faddp, 0xdec310, STROFF(faddp), st3, xx, st0, st3, xx, mrm, x, tfh[6][0x04]},
        {OP_faddp, 0xdec410, STROFF(faddp), st4, xx, st0, st4, xx, mrm, x, tfh[6][0x05]},
        {OP_faddp, 0xdec510, STROFF(faddp), st5, xx, st0, st5, xx, mrm, x, tfh[6][0x06]},
        {OP_faddp, 0xdec610, STROFF(faddp), st6, xx, st0, st6, xx, mrm, x, tfh[6][0x07]},
        {OP_faddp, 0xdec710, STROFF(faddp), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fmulp, 0xdec810, STROFF(fmulp), st0, xx, st0, st0, xx, mrm, x, tfh[6][0x09]}, /* c8 = [0x08] */
        {OP_fmulp, 0xdec910, STROFF(fmulp), st1, xx, st0, st1, xx, mrm, x, tfh[6][0x0a]},
        {OP_fmulp, 0xdeca10, STROFF(fmulp), st2, xx, st0, st2, xx, mrm, x, tfh[6][0x0b]},
        {OP_fmulp, 0xdecb10, STROFF(fmulp), st3, xx, st0, st3, xx, mrm, x, tfh[6][0x0c]},
        {OP_fmulp, 0xdecc10, STROFF(fmulp), st4, xx, st0, st4, xx, mrm, x, tfh[6][0x0d]},
        {OP_fmulp, 0xdecd10, STROFF(fmulp), st5, xx, st0, st5, xx, mrm, x, tfh[6][0x0e]},
        {OP_fmulp, 0xdece10, STROFF(fmulp), st6, xx, st0, st6, xx, mrm, x, tfh[6][0x0f]},
        {OP_fmulp, 0xdecf10, STROFF(fmulp), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fcomp5".  We assume an alias for fcomp
         * and do not include in the encode chain.
         */
        {OP_fcomp, 0xded010, STROFF(fcomp), xx, xx, st0, st0, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {OP_fcomp, 0xded110, STROFF(fcomp), xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded210, STROFF(fcomp), xx, xx, st0, st2, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded310, STROFF(fcomp), xx, xx, st0, st3, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded410, STROFF(fcomp), xx, xx, st0, st4, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded510, STROFF(fcomp), xx, xx, st0, st5, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded610, STROFF(fcomp), xx, xx, st0, st6, xx, mrm, x, END_LIST},
        {OP_fcomp, 0xded710, STROFF(fcomp), xx, xx, st0, st7, xx, mrm, x, END_LIST},
        {INVALID, 0xded810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* d8 = [0x18] */
        {OP_fcompp, 0xded910, STROFF(fcompp), xx, xx, st0, st1, xx, mrm, x, END_LIST},
        {INVALID, 0xdeda10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdede10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdedf10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {OP_fsubrp, 0xdee010, STROFF(fsubrp), st0, xx, st0, st0, xx, mrm, x, tfh[6][0x21]}, /* e0 = [0x20] */
        {OP_fsubrp, 0xdee110, STROFF(fsubrp), st1, xx, st0, st1, xx, mrm, x, tfh[6][0x22]},
        {OP_fsubrp, 0xdee210, STROFF(fsubrp), st2, xx, st0, st2, xx, mrm, x, tfh[6][0x23]},
        {OP_fsubrp, 0xdee310, STROFF(fsubrp), st3, xx, st0, st3, xx, mrm, x, tfh[6][0x24]},
        {OP_fsubrp, 0xdee410, STROFF(fsubrp), st4, xx, st0, st4, xx, mrm, x, tfh[6][0x25]},
        {OP_fsubrp, 0xdee510, STROFF(fsubrp), st5, xx, st0, st5, xx, mrm, x, tfh[6][0x26]},
        {OP_fsubrp, 0xdee610, STROFF(fsubrp), st6, xx, st0, st6, xx, mrm, x, tfh[6][0x27]},
        {OP_fsubrp, 0xdee710, STROFF(fsubrp), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fsubp, 0xdee810, STROFF(fsubp), st0, xx, st0, st0, xx, mrm, x, tfh[6][0x29]}, /* e8 = [0x28] */
        {OP_fsubp, 0xdee910, STROFF(fsubp), st1, xx, st0, st1, xx, mrm, x, tfh[6][0x2a]},
        {OP_fsubp, 0xdeea10, STROFF(fsubp), st2, xx, st0, st2, xx, mrm, x, tfh[6][0x2b]},
        {OP_fsubp, 0xdeeb10, STROFF(fsubp), st3, xx, st0, st3, xx, mrm, x, tfh[6][0x2c]},
        {OP_fsubp, 0xdeec10, STROFF(fsubp), st4, xx, st0, st4, xx, mrm, x, tfh[6][0x2d]},
        {OP_fsubp, 0xdeed10, STROFF(fsubp), st5, xx, st0, st5, xx, mrm, x, tfh[6][0x2e]},
        {OP_fsubp, 0xdeee10, STROFF(fsubp), st6, xx, st0, st6, xx, mrm, x, tfh[6][0x2f]},
        {OP_fsubp, 0xdeef10, STROFF(fsubp), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdivrp, 0xdef010, STROFF(fdivrp), st0, xx, st0, st0, xx, mrm, x, tfh[6][0x31]}, /* f0 = [0x30] */
        {OP_fdivrp, 0xdef110, STROFF(fdivrp), st1, xx, st0, st1, xx, mrm, x, tfh[6][0x32]},
        {OP_fdivrp, 0xdef210, STROFF(fdivrp), st2, xx, st0, st2, xx, mrm, x, tfh[6][0x33]},
        {OP_fdivrp, 0xdef310, STROFF(fdivrp), st3, xx, st0, st3, xx, mrm, x, tfh[6][0x34]},
        {OP_fdivrp, 0xdef410, STROFF(fdivrp), st4, xx, st0, st4, xx, mrm, x, tfh[6][0x35]},
        {OP_fdivrp, 0xdef510, STROFF(fdivrp), st5, xx, st0, st5, xx, mrm, x, tfh[6][0x36]},
        {OP_fdivrp, 0xdef610, STROFF(fdivrp), st6, xx, st0, st6, xx, mrm, x, tfh[6][0x37]},
        {OP_fdivrp, 0xdef710, STROFF(fdivrp), st7, xx, st0, st7, xx, mrm, x, END_LIST},
        {OP_fdivp, 0xdef810, STROFF(fdivp), st0, xx, st0, st0, xx, mrm, x, tfh[6][0x39]}, /* f8 = [0x38] */
        {OP_fdivp, 0xdef910, STROFF(fdivp), st1, xx, st0, st1, xx, mrm, x, tfh[6][0x3a]},
        {OP_fdivp, 0xdefa10, STROFF(fdivp), st2, xx, st0, st2, xx, mrm, x, tfh[6][0x3b]},
        {OP_fdivp, 0xdefb10, STROFF(fdivp), st3, xx, st0, st3, xx, mrm, x, tfh[6][0x3c]},
        {OP_fdivp, 0xdefc10, STROFF(fdivp), st4, xx, st0, st4, xx, mrm, x, tfh[6][0x3d]},
        {OP_fdivp, 0xdefd10, STROFF(fdivp), st5, xx, st0, st5, xx, mrm, x, tfh[6][0x3e]},
        {OP_fdivp, 0xdefe10, STROFF(fdivp), st6, xx, st0, st6, xx, mrm, x, tfh[6][0x3f]},
        {OP_fdivp, 0xdeff10, STROFF(fdivp), st7, xx, st0, st7, xx, mrm, x, END_LIST},
   },
    { /* df = [7] */
        /* Undocumented by Intel, but is on p152 of "AMD Athlon
         * Processor x86 Code Optimization Guide."
         */
        {OP_ffreep, 0xdfc010, STROFF(ffreep), st0, xx, xx, xx, xx, mrm, x, tfh[7][0x01]}, /* c0 = [0x00] */
        {OP_ffreep, 0xdfc110, STROFF(ffreep), st1, xx, xx, xx, xx, mrm, x, tfh[7][0x02]},
        {OP_ffreep, 0xdfc210, STROFF(ffreep), st2, xx, xx, xx, xx, mrm, x, tfh[7][0x03]},
        {OP_ffreep, 0xdfc310, STROFF(ffreep), st3, xx, xx, xx, xx, mrm, x, tfh[7][0x04]},
        {OP_ffreep, 0xdfc410, STROFF(ffreep), st4, xx, xx, xx, xx, mrm, x, tfh[7][0x05]},
        {OP_ffreep, 0xdfc510, STROFF(ffreep), st5, xx, xx, xx, xx, mrm, x, tfh[7][0x06]},
        {OP_ffreep, 0xdfc610, STROFF(ffreep), st6, xx, xx, xx, xx, mrm, x, tfh[7][0x07]},
        {OP_ffreep, 0xdfc710, STROFF(ffreep), st7, xx, xx, xx, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fxch7".  We assume an alias for fxch
         * and do not include in the encode chain.
         */
        {OP_fxch, 0xdfc810, STROFF(fxch), st0, st0, st0, st0, xx, mrm, x, END_LIST}, /* c8 = [0x08] */
        {OP_fxch, 0xdfc910, STROFF(fxch), st0, st1, st0, st1, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfca10, STROFF(fxch), st0, st2, st0, st2, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcb10, STROFF(fxch), st0, st3, st0, st3, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcc10, STROFF(fxch), st0, st4, st0, st4, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcd10, STROFF(fxch), st0, st5, st0, st5, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfce10, STROFF(fxch), st0, st6, st0, st6, xx, mrm, x, END_LIST},
        {OP_fxch, 0xdfcf10, STROFF(fxch), st0, st7, st0, st7, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fstp8".  We assume an alias for fstp
         * and do not include in the encode chain.
         */
        {OP_fstp, 0xdfd010, STROFF(fstp), st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* d0 = [0x10] */
        {OP_fstp, 0xdfd110, STROFF(fstp), st1, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd210, STROFF(fstp), st2, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd310, STROFF(fstp), st3, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd410, STROFF(fstp), st4, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd510, STROFF(fstp), st5, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd610, STROFF(fstp), st6, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfd710, STROFF(fstp), st7, xx, st0, xx, xx, mrm, x, END_LIST},
        /* Undocumented.  On sandpile.org as "fstp9".  We assume an alias for fstp
         * and do not include in the encode chain.
         */
        {OP_fstp, 0xdfd810, STROFF(fstp), st0, xx, st0, xx, xx, mrm, x, END_LIST}, /* d8 = [0x18] */
        {OP_fstp, 0xdfd910, STROFF(fstp), st1, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfda10, STROFF(fstp), st2, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdb10, STROFF(fstp), st3, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdc10, STROFF(fstp), st4, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdd10, STROFF(fstp), st5, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfde10, STROFF(fstp), st6, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fstp, 0xdfdf10, STROFF(fstp), st7, xx, st0, xx, xx, mrm, x, END_LIST},
        {OP_fnstsw, 0xdfe010, STROFF(fnstsw), ax, xx, xx, xx, xx, mrm, x, END_LIST}, /* e0 = [0x20] */ /*FIXME:w/ preceding fwait instr, this is "fstsw"*/
        {INVALID, 0xdfe110, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe210, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe310, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe410, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe510, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe610, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfe710, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {OP_fucomip, 0xdfe810, STROFF(fucomip), xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x29]}, /* e8 = [0x28] */
        {OP_fucomip, 0xdfe910, STROFF(fucomip), xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2a]},
        {OP_fucomip, 0xdfea10, STROFF(fucomip), xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2b]},
        {OP_fucomip, 0xdfeb10, STROFF(fucomip), xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2c]},
        {OP_fucomip, 0xdfec10, STROFF(fucomip), xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2d]},
        {OP_fucomip, 0xdfed10, STROFF(fucomip), xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2e]},
        {OP_fucomip, 0xdfee10, STROFF(fucomip), xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x2f]},
        {OP_fucomip, 0xdfef10, STROFF(fucomip), xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {OP_fcomip, 0xdff010, STROFF(fcomip), xx, xx, st0, st0, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x31]}, /* f0 = [0x30] */
        {OP_fcomip, 0xdff110, STROFF(fcomip), xx, xx, st0, st1, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x32]},
        {OP_fcomip, 0xdff210, STROFF(fcomip), xx, xx, st0, st2, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x33]},
        {OP_fcomip, 0xdff310, STROFF(fcomip), xx, xx, st0, st3, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x34]},
        {OP_fcomip, 0xdff410, STROFF(fcomip), xx, xx, st0, st4, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x35]},
        {OP_fcomip, 0xdff510, STROFF(fcomip), xx, xx, st0, st5, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x36]},
        {OP_fcomip, 0xdff610, STROFF(fcomip), xx, xx, st0, st6, xx, mrm, (fWC|fWP|fWZ), tfh[7][0x37]},
        {OP_fcomip, 0xdff710, STROFF(fcomip), xx, xx, st0, st7, xx, mrm, (fWC|fWP|fWZ), END_LIST},
        {INVALID, 0xdff810, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA}, /* f8 = [0x38] */
        {INVALID, 0xdff910, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffa10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffb10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffc10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffd10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdffe10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
        {INVALID, 0xdfff10, STROFF(BAD), xx, xx, xx, xx, xx, no, x, NA},
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
    {OP_unknown_3dnow, 0x000f0f90, STROFF(unknown_3DNow),
                                          Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 0*/
    {OP_pavgusb , 0xbf0f0f90, STROFF(pavgusb),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 1*/
    {OP_pfadd   , 0x9e0f0f90, STROFF(pfadd),    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 2*/
    {OP_pfacc   , 0xae0f0f90, STROFF(pfacc),    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 3*/
    {OP_pfcmpge , 0x900f0f90, STROFF(pfcmpge),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 4*/
    {OP_pfcmpgt , 0xa00f0f90, STROFF(pfcmpgt),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 5*/
    {OP_pfcmpeq , 0xb00f0f90, STROFF(pfcmpeq),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 6*/
    {OP_pfmin   , 0x940f0f90, STROFF(pfmin)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 7*/
    {OP_pfmax   , 0xa40f0f90, STROFF(pfmax)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 8*/
    {OP_pfmul   , 0xb40f0f90, STROFF(pfmul)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/* 9*/
    {OP_pfrcp   , 0x960f0f90, STROFF(pfrcp)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*10*/
    {OP_pfrcpit1, 0xa60f0f90, STROFF(pfrcpit1), Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*11*/
    {OP_pfrcpit2, 0xb60f0f90, STROFF(pfrcpit2), Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*12*/
    {OP_pfrsqrt , 0x970f0f90, STROFF(pfrsqrt),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*13*/
    {OP_pfrsqit1, 0xa70f0f90, STROFF(pfrsqit1), Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*14*/
    {OP_pmulhrw , 0xb70f0f90, STROFF(pmulhrw),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*15*/
    {OP_pfsub   , 0x9a0f0f90, STROFF(pfsub)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*16*/
    {OP_pfsubr  , 0xaa0f0f90, STROFF(pfsubr) ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*17*/
    {OP_pi2fd   , 0x0d0f0f90, STROFF(pi2fd)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*18*/
    {OP_pf2id   , 0x1d0f0f90, STROFF(pf2id),    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*19*/
    {OP_pi2fw   , 0x0c0f0f90, STROFF(pi2fw)  ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*20*/
    {OP_pf2iw   , 0x1c0f0f90, STROFF(pf2iw),    Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*21*/
    {OP_pfnacc  , 0x8a0f0f90, STROFF(pfnacc) ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*22*/
    {OP_pfpnacc , 0x8e0f0f90, STROFF(pfpnacc),  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*23*/
    {OP_pswapd  , 0xbb0f0f90, STROFF(pswapd) ,  Pq, xx, Qq, Pq, xx, mrm, x, END_LIST},/*24*/
};

/****************************************************************************
 * To handle more than 2 dests or 3 sources we chain on extra instructions.
 * All cases where we have extra operands are single-encoding-only instructions,
 * so we use the list field to point to here.
 * Also, only implicit operands are in these instruction extensions!!!
 */
const instr_info_t extra_operands[] =
{
    /* 0 */
    {OP_CONTD, 0x000000, STROFF(pusha_cont), xx, xx, eCX, eDX, eBP, xop, x, exop[0x01]},
    {OP_CONTD, 0x000000, STROFF(pusha_cont), xx, xx, eSI, eDI, xx, no, x, END_LIST},
    /* 2 */
    {OP_CONTD, 0x000000, STROFF(popa_cont), eBX, eCX, xx, xx, xx, xop, x, exop[0x03]},
    {OP_CONTD, 0x000000, STROFF(popa_cont), eDX, eBP, xx, xx, xx, xop, x, exop[0x04]},
    {OP_CONTD, 0x000000, STROFF(popa_cont), eSI, eDI, xx, xx, xx, no, x, END_LIST},
    /* 5 */
    {OP_CONTD, 0x000000, STROFF(enter_cont), xbp, xx, xbp, xx, xx, no, x, END_LIST},
    /* 6 */
    {OP_CONTD, 0x000000, STROFF(cpuid_cont), ecx, edx, xx, xx, xx, no, x, END_LIST},
    /* 7 */
    {OP_CONTD, 0x000000, STROFF(cmpxchg8b_cont), eDX, xx, eCX, eBX, xx, mrm, fWZ, END_LIST},
    {OP_CONTD,0x663a6018, STROFF(pcmpestrm_cont), xx, xx, eax, edx, xx, mrm|reqp, fW6, END_LIST},
    {OP_CONTD,0x663a6018, STROFF(pcmpestri_cont), xx, xx, eax, edx, xx, mrm|reqp, fW6, END_LIST},
    /* 10 */
    {OP_CONTD,0xf90f0177, STROFF(rdtscp_cont), ecx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD,0x663a6018, STROFF(vpcmpestrm_cont), xx, xx, eax, edx, xx, mrm|vex|reqp, fW6, END_LIST},
    {OP_CONTD,0x663a6018, STROFF(vpcmpestri_cont), xx, xx, eax, edx, xx, mrm|vex|reqp, fW6, END_LIST},
    {OP_CONTD,0x0f3710, STROFF(getsec_cont), ecx, xx, xx, xx, xx, predcx, x, END_LIST},
    /* 14 */
    {OP_CONTD,0x66389808, STROFF(vfmadd132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[42][0]},
    {OP_CONTD,0x66389818, STROFF(vfmadd132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[42][1]},
    {OP_CONTD,0x66389818, STROFF(vfmadd132ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 17 */
    {OP_CONTD,0x66389848, STROFF(vfmadd132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[43][0]},
    {OP_CONTD,0x66389858, STROFF(vfmadd132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[43][1]},
    {OP_CONTD,0x66389858, STROFF(vfmadd132pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 20 */
    {OP_CONTD,0x6638a808, STROFF(vfmadd213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[44][0]},
    {OP_CONTD,0x6638a818, STROFF(vfmadd213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[44][1]},
    {OP_CONTD,0x6638a818, STROFF(vfmadd213ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 23 */
    {OP_CONTD,0x6638a848, STROFF(vfmadd213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[45][0]},
    {OP_CONTD,0x6638a858, STROFF(vfmadd213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[45][1]},
    {OP_CONTD,0x6638a858, STROFF(vfmadd213pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 26 */
    {OP_CONTD,0x6638b808, STROFF(vfmadd231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[46][0]},
    {OP_CONTD,0x6638b818, STROFF(vfmadd231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[46][1]},
    {OP_CONTD,0x6638b818, STROFF(vfmadd231ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 29 */
    {OP_CONTD,0x6638b848, STROFF(vfmadd231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[47][0]},
    {OP_CONTD,0x6638b858, STROFF(vfmadd231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[47][1]},
    {OP_CONTD,0x6638b858, STROFF(vfmadd231pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 32 */
    {OP_CONTD,0x66389908, STROFF(vfmadd132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[65][1]},
    {OP_CONTD,0x66389918, STROFF(vfmadd132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 34 */
    {OP_CONTD,0x66389948, STROFF(vfmadd132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[65][3]},
    {OP_CONTD,0x66389958, STROFF(vfmadd132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 36 */
    {OP_CONTD,0x6638a908, STROFF(vfmadd213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[66][1]},
    {OP_CONTD,0x6638a918, STROFF(vfmadd213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 38 */
    {OP_CONTD,0x6638a948, STROFF(vfmadd213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[66][3]},
    {OP_CONTD,0x6638a958, STROFF(vfmadd213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 40 */
    {OP_CONTD,0x6638b908, STROFF(vfmadd231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[67][1]},
    {OP_CONTD,0x6638b918, STROFF(vfmadd231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 42 */
    {OP_CONTD,0x6638b948, STROFF(vfmadd231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[67][3]},
    {OP_CONTD,0x6638b958, STROFF(vfmadd231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 44 */
    {OP_CONTD,0x66389608, STROFF(vfmaddsub132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[48][0]},
    {OP_CONTD,0x66389618, STROFF(vfmaddsub132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[48][1]},
    {OP_CONTD,0x66389618, STROFF(vfmaddsub132ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 47 */
    {OP_CONTD,0x66389648, STROFF(vfmaddsub132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[49][0]},
    {OP_CONTD,0x66389658, STROFF(vfmaddsub132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[49][1]},
    {OP_CONTD,0x66389658, STROFF(vfmaddsub132pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 50 */
    {OP_CONTD,0x6638a608, STROFF(vfmaddsub213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[50][0]},
    {OP_CONTD,0x6638a618, STROFF(vfmaddsub213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[50][1]},
    {OP_CONTD,0x6638a618, STROFF(vfmaddsub213ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 53 */
    {OP_CONTD,0x6638a648, STROFF(vfmaddsub213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[51][0]},
    {OP_CONTD,0x6638a658, STROFF(vfmaddsub213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[51][1]},
    {OP_CONTD,0x6638a658, STROFF(vfmaddsub213pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 56 */
    {OP_CONTD,0x6638b608, STROFF(vfmaddsub231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[52][0]},
    {OP_CONTD,0x6638b618, STROFF(vfmaddsub231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[52][1]},
    {OP_CONTD,0x6638b618, STROFF(vfmaddsub231ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 59 */
    {OP_CONTD,0x6638b648, STROFF(vfmaddsub231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[53][0]},
    {OP_CONTD,0x6638b658, STROFF(vfmaddsub231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[53][1]},
    {OP_CONTD,0x6638b658, STROFF(vfmaddsub231pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 62 */
    {OP_CONTD,0x66389708, STROFF(vfmsubadd132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[54][0]},
    {OP_CONTD,0x66389718, STROFF(vfmsubadd132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[54][1]},
    {OP_CONTD,0x66389718, STROFF(vfmsubadd132ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 65 */
    {OP_CONTD,0x66389748, STROFF(vfmsubadd132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[55][0]},
    {OP_CONTD,0x66389758, STROFF(vfmsubadd132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[55][1]},
    {OP_CONTD,0x66389758, STROFF(vfmsubadd132pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 68 */
    {OP_CONTD,0x6638a708, STROFF(vfmsubadd213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[56][0]},
    {OP_CONTD,0x6638a718, STROFF(vfmsubadd213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[56][1]},
    {OP_CONTD,0x6638a718, STROFF(vfmsubadd213ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 71 */
    {OP_CONTD,0x6638a748, STROFF(vfmsubadd213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[57][0]},
    {OP_CONTD,0x6638a758, STROFF(vfmsubadd213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[57][1]},
    {OP_CONTD,0x6638a758, STROFF(vfmsubadd213pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 74 */
    {OP_CONTD,0x6638b708, STROFF(vfmsubadd231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[58][0]},
    {OP_CONTD,0x6638b718, STROFF(vfmsubadd231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[58][1]},
    {OP_CONTD,0x6638b718, STROFF(vfmsubadd231ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 77 */
    {OP_CONTD,0x6638b748, STROFF(vfmsubadd231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[59][0]},
    {OP_CONTD,0x6638b758, STROFF(vfmsubadd231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[59][1]},
    {OP_CONTD,0x6638b758, STROFF(vfmsubadd231pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 80 */
    {OP_CONTD,0x66389a08, STROFF(vfmsub132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[60][0]},
    {OP_CONTD,0x66389a18, STROFF(vfmsub132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[60][1]},
    {OP_CONTD,0x66389a18, STROFF(vfmsub132ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 83 */
    {OP_CONTD,0x66389a48, STROFF(vfmsub132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[61][0]},
    {OP_CONTD,0x66389a58, STROFF(vfmsub132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[61][1]},
    {OP_CONTD,0x66389a58, STROFF(vfmsub132pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 86 */
    {OP_CONTD,0x6638aa08, STROFF(vfmsub213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[62][0]},
    {OP_CONTD,0x6638aa18, STROFF(vfmsub213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[62][1]},
    {OP_CONTD,0x6638aa18, STROFF(vfmsub213ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 89 */
    {OP_CONTD,0x6638aa48, STROFF(vfmsub213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[63][0]},
    {OP_CONTD,0x6638aa58, STROFF(vfmsub213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[63][1]},
    {OP_CONTD,0x6638aa58, STROFF(vfmsub213pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 92 */
    {OP_CONTD,0x6638ba08, STROFF(vfmsub231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[64][0]},
    {OP_CONTD,0x6638ba18, STROFF(vfmsub231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[64][1]},
    {OP_CONTD,0x6638ba18, STROFF(vfmsub231ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 95 */
    {OP_CONTD,0x6638ba48, STROFF(vfmsub231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[65][0]},
    {OP_CONTD,0x6638ba58, STROFF(vfmsub231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[65][1]},
    {OP_CONTD,0x6638ba58, STROFF(vfmsub231pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 98 */
    {OP_CONTD,0x66389b08, STROFF(vfmsub132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[77][1]},
    {OP_CONTD,0x66389b18, STROFF(vfmsub132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 100 */
    {OP_CONTD,0x66389b48, STROFF(vfmsub132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[77][3]},
    {OP_CONTD,0x66389b58, STROFF(vfmsub132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 102 */
    {OP_CONTD,0x6638ab08, STROFF(vfmsub213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[78][1]},
    {OP_CONTD,0x6638ab18, STROFF(vfmsub213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 104 */
    {OP_CONTD,0x6638ab48, STROFF(vfmsub213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[78][3]},
    {OP_CONTD,0x6638ab58, STROFF(vfmsub213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 106 */
    {OP_CONTD,0x6638bb08, STROFF(vfmsub231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[79][1]},
    {OP_CONTD,0x6638bb18, STROFF(vfmsub231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 108 */
    {OP_CONTD,0x6638bb48, STROFF(vfmsub231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[79][3]},
    {OP_CONTD,0x6638bb58, STROFF(vfmsub231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 110 */
    {OP_CONTD,0x66389c08, STROFF(vfnmadd132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[66][0]},
    {OP_CONTD,0x66389c18, STROFF(vfnmadd132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[66][1]},
    {OP_CONTD,0x66389c18, STROFF(vfnmadd132ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 113 */
    {OP_CONTD,0x66389c48, STROFF(vfnmadd132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[67][0]},
    {OP_CONTD,0x66389c58, STROFF(vfnmadd132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[67][1]},
    {OP_CONTD,0x66389c58, STROFF(vfnmadd132pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 116 */
    {OP_CONTD,0x6638ac08, STROFF(vfnmadd213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[68][0]},
    {OP_CONTD,0x6638ac18, STROFF(vfnmadd213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[68][1]},
    {OP_CONTD,0x6638ac18, STROFF(vfnmadd213ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 119 */
    {OP_CONTD,0x6638ac48, STROFF(vfnmadd213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[69][0]},
    {OP_CONTD,0x6638ac58, STROFF(vfnmadd213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[69][1]},
    {OP_CONTD,0x6638ac58, STROFF(vfnmadd213pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 122 */
    {OP_CONTD,0x6638bc08, STROFF(vfnmadd231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[70][0]},
    {OP_CONTD,0x6638bc18, STROFF(vfnmadd231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[70][1]},
    {OP_CONTD,0x6638bc18, STROFF(vfnmadd231ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 125 */
    {OP_CONTD,0x6638bc48, STROFF(vfnmadd231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[71][0]},
    {OP_CONTD,0x6638bc58, STROFF(vfnmadd231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[71][1]},
    {OP_CONTD,0x6638bc58, STROFF(vfnmadd231pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 128 */
    {OP_CONTD,0x66389d08, STROFF(vfnmadd132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[83][1]},
    {OP_CONTD,0x66389d18, STROFF(vfnmadd132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 130 */
    {OP_CONTD,0x66389d48, STROFF(vfnmadd132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[83][3]},
    {OP_CONTD,0x66389d58, STROFF(vfnmadd132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 132 */
    {OP_CONTD,0x6638ad08, STROFF(vfnmadd213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[84][1]},
    {OP_CONTD,0x6638ad18, STROFF(vfnmadd213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 134 */
    {OP_CONTD,0x6638ad48, STROFF(vfnmadd213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[84][3]},
    {OP_CONTD,0x6638ad58, STROFF(vfnmadd213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 136 */
    {OP_CONTD,0x6638bd08, STROFF(vfnmadd231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[85][1]},
    {OP_CONTD,0x6638bd18, STROFF(vfnmadd231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 138 */
    {OP_CONTD,0x6638bd48, STROFF(vfnmadd231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[85][3]},
    {OP_CONTD,0x6638bd58, STROFF(vfnmadd231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 140 */
    {OP_CONTD,0x66389e08, STROFF(vfnmsub132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[72][0]},
    {OP_CONTD,0x66389e18, STROFF(vfnmsub132ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[72][1]},
    {OP_CONTD,0x66389e18, STROFF(vfnmsub132ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 143 */
    {OP_CONTD,0x66389e48, STROFF(vfnmsub132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[73][0]},
    {OP_CONTD,0x66389e58, STROFF(vfnmsub132pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[73][1]},
    {OP_CONTD,0x66389e58, STROFF(vfnmsub132pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 146 */
    {OP_CONTD,0x6638ae08, STROFF(vfnmsub213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[74][0]},
    {OP_CONTD,0x6638ae18, STROFF(vfnmsub213ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[74][1]},
    {OP_CONTD,0x6638ae18, STROFF(vfnmsub213ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 149 */
    {OP_CONTD,0x6638ae48, STROFF(vfnmsub213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[75][0]},
    {OP_CONTD,0x6638ae58, STROFF(vfnmsub213pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[75][1]},
    {OP_CONTD,0x6638ae58, STROFF(vfnmsub213pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 152 */
    {OP_CONTD,0x6638be08, STROFF(vfnmsub231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[76][0]},
    {OP_CONTD,0x6638be18, STROFF(vfnmsub231ps_cont), xx, xx, Ves, xx, xx, mrm|evex, x, modx[76][1]},
    {OP_CONTD,0x6638be18, STROFF(vfnmsub231ps_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 155 */
    {OP_CONTD,0x6638be48, STROFF(vfnmsub231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[77][0]},
    {OP_CONTD,0x6638be58, STROFF(vfnmsub231pd_cont), xx, xx, Ved, xx, xx, mrm|evex, x, modx[77][1]},
    {OP_CONTD,0x6638be58, STROFF(vfnmsub231pd_cont), xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 158 */
    {OP_CONTD,0x66389f08, STROFF(vfnmsub132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[89][1]},
    {OP_CONTD,0x66389f18, STROFF(vfnmsub132ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 160 */
    {OP_CONTD,0x66389f48, STROFF(vfnmsub132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[89][3]},
    {OP_CONTD,0x66389f58, STROFF(vfnmsub132sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 162 */
    {OP_CONTD,0x6638af08, STROFF(vfnmsub213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[90][1]},
    {OP_CONTD,0x6638af18, STROFF(vfnmsub213ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 164 */
    {OP_CONTD,0x6638af48, STROFF(vfnmsub213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[90][3]},
    {OP_CONTD,0x6638af58, STROFF(vfnmsub213sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 166 */
    {OP_CONTD,0x6638bf08, STROFF(vfnmsub231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[91][1]},
    {OP_CONTD,0x6638bf18, STROFF(vfnmsub231ss_cont), xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 168 */
    {OP_CONTD,0x6638bf48, STROFF(vfnmsub231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[91][3]},
    {OP_CONTD,0x6638bf58, STROFF(vfnmsub231sd_cont), xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 170 */
    {OP_CONTD, 0x663a1818, STROFF(vinsertf32x4_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a1858, STROFF(vinsertf64x2_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 172 */
    {OP_CONTD, 0x663a1a58, STROFF(vinsertf32x8_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a1a58, STROFF(vinsertf64x4_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 174 */
    {OP_CONTD, 0x663a3818, STROFF(vinserti32x4_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a3858, STROFF(vinserti64x2_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 176 */
    {OP_CONTD, 0x663a3a18, STROFF(vinserti32x8_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a3a58, STROFF(vinserti64x4_cont), xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 178 */
    {OP_CONTD, 0x663a3e18, STROFF(vpcmpub_cont), xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    {OP_CONTD, 0x663a3f18, STROFF(vpcmpb_cont), xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    /* 180 */
    {OP_CONTD, 0x663a3e18, STROFF(vpcmpuw_cont), xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    {OP_CONTD, 0x663a3f18, STROFF(vpcmpw_cont), xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    /* 182 */
    {OP_CONTD, 0x663a1e18, STROFF(vpcmpud_cont), xx, xx, We, xx, xx, evex|mrm, x, tevexwb[111][1]},
    {OP_CONTD, 0x663a1e18, STROFF(vpcmpud_cont), xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 184 */
    {OP_CONTD, 0x663a1f18, STROFF(vpcmpd_cont), xx, xx, We, xx, xx, evex|mrm, x, tevexwb[112][1]},
    {OP_CONTD, 0x663a1f18, STROFF(vpcmpd_cont), xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 186 */
    {OP_CONTD, 0x663a1e18, STROFF(vpcmpuq_cont), xx, xx, We, xx, xx, evex|mrm, x, tevexwb[111][3]},
    {OP_CONTD, 0x663a1e18, STROFF(vpcmpuq_cont), xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 188 */
    {OP_CONTD, 0x663a1f18, STROFF(vpcmpq_cont), xx, xx, We, xx, xx, evex|mrm, x, tevexwb[112][3]},
    {OP_CONTD, 0x663a1f18, STROFF(vpcmpq_cont), xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 190 */
    {OP_CONTD,   0x0fc200, STROFF(vcmpps_cont), xx, xx, Wes, xx, xx, evex|mrm, x, modx[114][0]},
    {OP_CONTD,   0x0fc210, STROFF(vcmpps_cont), xx, xx, Md, xx, xx, evex|mrm, x, modx[114][1]},
    {OP_CONTD,   0x0fc210, STROFF(vcmpps_cont), xx, xx, Uoq, xx, xx, evex|mrm, x, END_LIST},
    /* 193 */
    {OP_CONTD, 0xf30fc200, STROFF(vcmpss_cont), xx, xx, Wss, xx, xx, evex|mrm, x, tevexwb[262][1]},
    {OP_CONTD, 0xf30fc210, STROFF(vcmpss_cont), xx, xx, Uss, xx, xx, evex|mrm, x, END_LIST},
    /* 195 */
    {OP_CONTD, 0xf20fc200, STROFF(vcmpsd_cont), xx, xx, Wsd, xx, xx, evex|mrm, x, tevexwb[262][3]},
    {OP_CONTD, 0xf20fc210, STROFF(vcmpsd_cont), xx, xx, Usd, xx, xx, evex|mrm, x, END_LIST},
    /* 197 */
    {OP_CONTD, 0x660fc200, STROFF(vcmppd_cont), xx, xx, Wed, xx, xx, evex|mrm, x, modx[115][0]},
    {OP_CONTD, 0x660fc210, STROFF(vcmppd_cont), xx, xx, Mq, xx, xx, evex|mrm, x, modx[115][1]},
    {OP_CONTD, 0x660fc210, STROFF(vcmppd_cont), xx, xx, Uoq, xx, xx, evex|mrm, x, END_LIST},
    /* 200 */
    {OP_CONTD,   0x0fc610, STROFF(vshufps_cont), xx, xx, Wes, xx, xx, evex|mrm, x, tevexwb[221][1]},
    {OP_CONTD,   0x0fc610, STROFF(vshufps_cont), xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 202 */
    {OP_CONTD, 0x660fc650, STROFF(vshufpd_cont), xx, xx, Wed, xx, xx, evex|mrm, x, tevexwb[221][3]},
    {OP_CONTD, 0x660fc650, STROFF(vshufpd_cont), xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 204 */
    {OP_CONTD, 0x663a2318, STROFF(vshuff32x4_cont), xx, xx, Wfs, xx, xx, evex|mrm, x, tevexwb[142][1]},
    {OP_CONTD, 0x663a2318, STROFF(vshuff32x4_cont), xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 206 */
    {OP_CONTD, 0x663a2358, STROFF(vshuff64x2_cont), xx, xx, Wfd, xx, xx, evex|mrm, x, tevexwb[142][3]},
    {OP_CONTD, 0x663a2358, STROFF(vshuff64x2_cont), xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 208 */
    {OP_CONTD, 0x663a4318, STROFF(vshufi32x4_cont), xx, xx, Wfs, xx, xx, evex|mrm, x, tevexwb[143][1]},
    {OP_CONTD, 0x663a4318, STROFF(vshufi32x4_cont), xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 210 */
    {OP_CONTD, 0x663a4358, STROFF(vshufi64x2_cont), xx, xx, Wfd, xx, xx, evex|mrm, x, tevexwb[143][3]},
    {OP_CONTD, 0x663a4358, STROFF(vshufi64x2_cont), xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 212 */
    {OP_CONTD, 0x663a0318, STROFF(valignd_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[155][1]},
    {OP_CONTD, 0x663a0318, STROFF(valignd_cont), xx, xx, Md, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 214 */
    {OP_CONTD, 0x663a0358, STROFF(valignq_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[155][3]},
    {OP_CONTD, 0x663a0358, STROFF(valignq_cont), xx, xx, Mq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 216 */
    {OP_CONTD, 0x663a5408, STROFF(vfixupimmps_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[80][0]},
    {OP_CONTD, 0x663a5418, STROFF(vfixupimmps_cont), xx, xx, Md, xx, xx, mrm|evex|reqp, x, modx[80][1]},
    {OP_CONTD, 0x663a5418, STROFF(vfixupimmps_cont), xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 219 */
    {OP_CONTD, 0x663a5448, STROFF(vfixupimmpd_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[81][0]},
    {OP_CONTD, 0x663a5458, STROFF(vfixupimmpd_cont), xx, xx, Mq, xx, xx, mrm|evex|reqp, x, modx[81][1]},
    {OP_CONTD, 0x663a5458, STROFF(vfixupimmpd_cont), xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 222 */
    {OP_CONTD, 0x663a5508, STROFF(vfixupimmss_cont), xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[160][1]},
    {OP_CONTD, 0x663a5518, STROFF(vfixupimmss_cont), xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 224 */
    {OP_CONTD, 0x663a5548, STROFF(vfixupimmsd_cont), xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[161][3]},
    {OP_CONTD, 0x663a5558, STROFF(vfixupimmsd_cont), xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 226 */
    {OP_CONTD, 0x663a2708, STROFF(vgetmantss_cont), xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[164][1]},
    {OP_CONTD, 0x663a2718, STROFF(vgetmantss_cont), xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 228 */
    {OP_CONTD, 0x663a2748, STROFF(vgetmantsd_cont), xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[164][3]},
    {OP_CONTD, 0x663a2758, STROFF(vgetmantsd_cont), xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 230 */
    {OP_CONTD, 0x663a5008, STROFF(vrangeps_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[86][0]},
    {OP_CONTD, 0x663a5018, STROFF(vrangeps_cont), xx, xx, Md, xx, xx, mrm|evex|reqp, x, modx[86][1]},
    {OP_CONTD, 0x663a5018, STROFF(vrangeps_cont), xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 233 */
    {OP_CONTD, 0x663a5048, STROFF(vrangepd_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[87][0]},
    {OP_CONTD, 0x663a5058, STROFF(vrangepd_cont), xx, xx, Mq, xx, xx, mrm|evex|reqp, x, modx[87][1]},
    {OP_CONTD, 0x663a5058, STROFF(vrangepd_cont), xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 236 */
    {OP_CONTD, 0x663a5108, STROFF(vrangess_cont), xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[174][1]},
    {OP_CONTD, 0x663a5118, STROFF(vrangess_cont), xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 238 */
    {OP_CONTD, 0x663a5148, STROFF(vrangesd_cont), xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[174][3]},
    {OP_CONTD, 0x663a5158, STROFF(vrangesd_cont), xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 240 */
    {OP_CONTD, 0x663a5708, STROFF(vreducess_cont), xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[176][1]},
    {OP_CONTD, 0x663a5718, STROFF(vreducess_cont), xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 242 */
    {OP_CONTD, 0x663a5748, STROFF(vreducesd_cont), xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[176][3]},
    {OP_CONTD, 0x663a5758, STROFF(vreducesd_cont), xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 244 */
    {OP_CONTD, 0x663a0a08, STROFF(vrndscaless_cont), xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[253][1]},
    {OP_CONTD, 0x663a0a18, STROFF(vrndscaless_cont), xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 246 */
    {OP_CONTD, 0x663a0b08, STROFF(vrndscalesd_cont), xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[254][1]},
    {OP_CONTD, 0x663a0b18, STROFF(vrndscalesd_cont), xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 248 */
    {OP_CONTD, 0x663a0f18, STROFF(vpalignr_cont), xx, xx, We, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a4218, STROFF(vdbpsadbw_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 250 */
    {OP_CONTD, 0x663a2508, STROFF(vpternlogd_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[188][1]},
    {OP_CONTD, 0x663a2518, STROFF(vpternlogd_cont), xx, xx, Md, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 252 */
    {OP_CONTD, 0x663a2548, STROFF(vpternlogq_cont), xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[188][3]},
    {OP_CONTD, 0x663a2558, STROFF(vpternlogq_cont), xx, xx, Mq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 254 */
    {OP_CONTD, 0xcf0f0171, STROFF(encls_cont), ecx, edx, edx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD, 0xd70f0172, STROFF(enclu_cont), ecx, edx, edx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD, 0xc00f0171, STROFF(enclv_cont), ecx, edx, edx, xx, xx, mrm, x, END_LIST},
};

/* clang-format on */
