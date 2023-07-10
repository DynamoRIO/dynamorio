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
    {EVEX_PREFIX_EXT, 0x620000, "(evex_prefix_ext)", xx, xx, xx, xx, xx, no, x, END_LIST},
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
    {OP_jo_short,  0x700000, "jo",  xx, xx, Jb, xx, xx, predcc, fRO, END_LIST},
    {OP_jno_short, 0x710000, "jno", xx, xx, Jb, xx, xx, predcc, fRO, END_LIST},
    {OP_jb_short,  0x720000, "jb",  xx, xx, Jb, xx, xx, predcc, fRC, END_LIST},
    {OP_jnb_short, 0x730000, "jnb", xx, xx, Jb, xx, xx, predcc, fRC, END_LIST},
    {OP_jz_short,  0x740000, "jz",  xx, xx, Jb, xx, xx, predcc, fRZ, END_LIST},
    {OP_jnz_short, 0x750000, "jnz", xx, xx, Jb, xx, xx, predcc, fRZ, END_LIST},
    {OP_jbe_short, 0x760000, "jbe", xx, xx, Jb, xx, xx, predcc, (fRC|fRZ), END_LIST},
    {OP_jnbe_short,0x770000, "jnbe",xx, xx, Jb, xx, xx, predcc, (fRC|fRZ), END_LIST},
    /* 78 */
    {OP_js_short,  0x780000, "js",  xx, xx, Jb, xx, xx, predcc, fRS, END_LIST},
    {OP_jns_short, 0x790000, "jns", xx, xx, Jb, xx, xx, predcc, fRS, END_LIST},
    {OP_jp_short,  0x7a0000, "jp",  xx, xx, Jb, xx, xx, predcc, fRP, END_LIST},
    {OP_jnp_short, 0x7b0000, "jnp", xx, xx, Jb, xx, xx, predcc, fRP, END_LIST},
    {OP_jl_short,  0x7c0000, "jl",  xx, xx, Jb, xx, xx, predcc, (fRS|fRO), END_LIST},
    {OP_jnl_short, 0x7d0000, "jnl", xx, xx, Jb, xx, xx, predcc, (fRS|fRO), END_LIST},
    {OP_jle_short, 0x7e0000, "jle", xx, xx, Jb, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
    {OP_jnle_short,0x7f0000, "jnle",xx, xx, Jb, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
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
  {OP_syscall, 0x0f0510, "syscall", xcx, na_x11, xx, xx, xx, no, x, NA}, /* AMD/x64 only */
  {OP_clts, 0x0f0610, "clts", xx, xx, xx, xx, xx, no, x, END_LIST},
  /* XXX: writes ss and cs */
  {OP_sysret, 0x0f0710, "sysret", xx, xx, xcx, na_x11, xx, no, x, NA}, /* AMD/x64 only */
  /* 08 */
  {OP_invd, 0x0f0810, "invd", xx, xx, xx, xx, xx, no, x, END_LIST},
  {OP_wbinvd, 0x0f0910, "wbinvd", xx, xx, xx, xx, xx, no, x, END_LIST},
  {INVALID, 0x0f0a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  {OP_ud2, 0x0f0b10, "ud2", xx, xx, xx, xx, xx, no, x, END_LIST}, /* "undefined instr" instr */
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
  {PREFIX_EXT, 0x0f1a10, "(prefix ext 186)", xx, xx, xx, xx, xx, mrm, x, 186},
  {PREFIX_EXT, 0x0f1b10, "(prefix ext 187)", xx, xx, xx, xx, xx, mrm, x, 187},
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
  {OP_cmovo,   0x0f4010, "cmovo",  Gv, xx, Ev, xx, xx, mrm|predcc, fRO, END_LIST},
  {E_VEX_EXT, 0x0f4110, "(e_vex ext 83)", xx, xx, xx, xx, xx, mrm, x, 83},
  {E_VEX_EXT, 0x0f4210, "(e_vex ext 84)", xx, xx, xx, xx, xx, mrm, x, 84},
  {OP_cmovnb,  0x0f4310, "cmovnb", Gv, xx, Ev, xx, xx, mrm|predcc, fRC, END_LIST},
  {E_VEX_EXT, 0x0f4410, "(e_vex ext 86)", xx, xx, xx, xx, xx, mrm, x, 86},
  {E_VEX_EXT, 0x0f4510, "(e_vex ext 87)", xx, xx, xx, xx, xx, mrm, x, 87},
  {E_VEX_EXT, 0x0f4610, "(e_vex ext 88)", xx, xx, xx, xx, xx, mrm, x, 88},
  {E_VEX_EXT, 0x0f4710, "(e_vex ext 89)", xx, xx, xx, xx, xx, mrm, x, 89},
  /* 48 */
  {OP_cmovs,  0x0f4810, "cmovs",  Gv, xx, Ev, xx, xx, mrm|predcc, fRS, END_LIST},
  {OP_cmovns, 0x0f4910, "cmovns", Gv, xx, Ev, xx, xx, mrm|predcc, fRS, END_LIST},
  {E_VEX_EXT, 0x0f4a10, "(e_vex ext 90)", xx, xx, xx, xx, xx, mrm, x, 90},
  {E_VEX_EXT, 0x0f4b10, "(e_vex ext 85)", xx, xx, xx, xx, xx, mrm, x, 85},
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
  {PREFIX_EXT, 0x0f7a10, "(prefix ext 159)", xx, xx, xx, xx, xx, mrm, x, 159},
  {PREFIX_EXT, 0x0f7b10, "(prefix ext 158)", xx, xx, xx, xx, xx, mrm, x, 158},
  {PREFIX_EXT, 0x0f7c10, "(prefix ext 114)", xx, xx, xx, xx, xx, mrm, x, 114},
  {PREFIX_EXT, 0x0f7d10, "(prefix ext 115)", xx, xx, xx, xx, xx, mrm, x, 115},
  {PREFIX_EXT, 0x0f7e10, "(prefix ext 51)", xx, xx, xx, xx, xx, mrm, x, 51},
  {PREFIX_EXT, 0x0f7f10, "(prefix ext 113)", xx, xx, xx, xx, xx, mrm, x, 113},
  /* 80 */
  {OP_jo,  0x0f8010, "jo",  xx, xx, Jz, xx, xx, predcc, fRO, END_LIST},
  {OP_jno, 0x0f8110, "jno", xx, xx, Jz, xx, xx, predcc, fRO, END_LIST},
  {OP_jb,  0x0f8210, "jb",  xx, xx, Jz, xx, xx, predcc, fRC, END_LIST},
  {OP_jnb, 0x0f8310, "jnb", xx, xx, Jz, xx, xx, predcc, fRC, END_LIST},
  {OP_jz,  0x0f8410, "jz",  xx, xx, Jz, xx, xx, predcc, fRZ, END_LIST},
  {OP_jnz, 0x0f8510, "jnz", xx, xx, Jz, xx, xx, predcc, fRZ, END_LIST},
  {OP_jbe, 0x0f8610, "jbe", xx, xx, Jz, xx, xx, predcc, (fRC|fRZ), END_LIST},
  {OP_jnbe,0x0f8710, "jnbe",xx, xx, Jz, xx, xx, predcc, (fRC|fRZ), END_LIST},
  /* 88 */
  {OP_js,  0x0f8810, "js",  xx, xx, Jz, xx, xx, predcc, fRS, END_LIST},
  {OP_jns, 0x0f8910, "jns", xx, xx, Jz, xx, xx, predcc, fRS, END_LIST},
  {OP_jp,  0x0f8a10, "jp",  xx, xx, Jz, xx, xx, predcc, fRP, END_LIST},
  {OP_jnp, 0x0f8b10, "jnp", xx, xx, Jz, xx, xx, predcc, fRP, END_LIST},
  {OP_jl,  0x0f8c10, "jl",  xx, xx, Jz, xx, xx, predcc, (fRS|fRO), END_LIST},
  {OP_jnl, 0x0f8d10, "jnl", xx, xx, Jz, xx, xx, predcc, (fRS|fRO), END_LIST},
  {OP_jle, 0x0f8e10, "jle", xx, xx, Jz, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
  {OP_jnle,0x0f8f10, "jnle",xx, xx, Jz, xx, xx, predcc, (fRS|fRO|fRZ), END_LIST},
  /* 90 */
  {E_VEX_EXT, 0x0f9010, "(e_vex ext 79)", xx, xx, xx, xx, xx, mrm, x, 79},
  {E_VEX_EXT, 0x0f9110, "(e_vex ext 80)", xx, xx, xx, xx, xx, mrm, x, 80},
  {E_VEX_EXT, 0x0f9210, "(e_vex ext 81)", xx, xx, xx, xx, xx, mrm, x, 81},
  {E_VEX_EXT, 0x0f9310, "(e_vex ext 82)", xx, xx, xx, xx, xx, mrm, x, 82},
  {OP_setz,  0x0f9410, "setz",  Eb, xx, xx, xx, xx, mrm, fRZ, END_LIST},
  {OP_setnz, 0x0f9510, "setnz", Eb, xx, xx, xx, xx, mrm, fRZ, END_LIST},
  {OP_setbe, 0x0f9610, "setbe", Eb, xx, xx, xx, xx, mrm, (fRC|fRZ), END_LIST},
  {OP_setnbe,0x0f9710, "setnbe",Eb, xx, xx, xx, xx, mrm, (fRC|fRZ), END_LIST},
  /* 98 */
  {E_VEX_EXT, 0x0f9810, "(e_vex ext 91)", xx, xx, xx, xx, xx, mrm, x, 91},
  {E_VEX_EXT, 0x0f9910, "(e_vex ext 92)", xx, xx, xx, xx, xx, mrm, x, 92},
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
  {OP_popcnt, 0xf30fb810, "popcnt", Gv, xx, Ev, xx, xx, mrm|reqp, fW6, END_LIST},
  /* This is Group 10, but all identical (ud1) so no reason to split opcode by /reg */
  {OP_ud1, 0x0fb910, "ud1", xx, xx, Gv, Ev, xx, mrm, x, END_LIST},
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
  {OP_movnti, 0x0fc310, "movnti", My, xx, Gy, xx, xx, mrm, x, END_LIST},
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
const instr_info_t base_extensions[][8] = {
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
    {INVALID, 0x0f0036, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x0f0037, "(bad)",xx, xx, xx, xx, xx, no, x, NA},
 },
  /* group 7 (first bytes 0f 01) */
  { /* extensions[14] */
    {MOD_EXT, 0x0f0130, "(group 7 mod ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {MOD_EXT, 0x0f0131, "(group 7 mod ext 1)", xx, xx, xx, xx, xx, no, x, 1},
    {MOD_EXT, 0x0f0132, "(group 7 mod ext 5)", xx, xx, xx, xx, xx, no, x, 5},
    {MOD_EXT, 0x0f0133, "(group 7 mod ext 4)", xx, xx, xx, xx, xx, no, x, 4},
    {OP_smsw, 0x0f0134, "smsw",  Ew, xx, xx, xx, xx, mrm, x, END_LIST},
    {MOD_EXT, 0x0f0135, "(group 7 mod ext 120)", xx, xx, xx, xx, xx, no, x, 120},
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
    {REX_W_EXT, 0x0fc734, "(rex.w ext 5)", xx, xx, xx, xx, xx, mrm, x, 5},
    {INVALID, 0x0fc735, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {MOD_EXT, 0x0fc736, "(group 9 mod ext 12)", xx, xx, xx, xx, xx, mrm, x, 12},
    {MOD_EXT, 0x0fc737, "(mod ext 13)", xx, xx, xx, xx, xx, mrm, x, 13},
  },
  /* group 10 is all ud1 and is not used by us since identical */
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
    {EVEX_Wb_EXT, 0x660f7220, "(evex_Wb ext 120)", xx, xx, xx, xx, xx, mrm|evex, x, 120},
    {EVEX_Wb_EXT, 0x660f7221, "(evex_Wb ext 118)", xx, xx, xx, xx, xx, mrm|evex, x, 118},
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
    {PREFIX_EXT, 0x0fae34, "(prefix ext 188)", xx, xx, xx, xx, xx, no, x, 188},
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
  /* group 18 */
  { /* extensions[32] */
    {INVALID, 0x6638c638, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c639, "(evex_Wb ext 197)", xx, xx, xx, xx, xx, mrm|reqp, x, 197},
    {EVEX_Wb_EXT, 0x6638c63a, "(evex_Wb ext 199)", xx, xx, xx, xx, xx, mrm|reqp, x, 199},
    {INVALID, 0x6638c63b, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638c63c, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c63d, "(evex_Wb ext 201)", xx, xx, xx, xx, xx, mrm|reqp, x, 201},
    {EVEX_Wb_EXT, 0x6638c63e, "(evex_Wb ext 203)", xx, xx, xx, xx, xx, mrm|reqp, x, 203},
    {INVALID, 0x6638c63f, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  },
  /* group 19 */
  { /* extensions[33] */
    {INVALID, 0x6638c738, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c739, "(evex_Wb ext 198)", xx, xx, xx, xx, xx, mrm|reqp, x, 198},
    {EVEX_Wb_EXT, 0x6638c73a, "(evex_Wb ext 200)", xx, xx, xx, xx, xx, mrm|reqp, x, 200},
    {INVALID, 0x6638c73b, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638c73c, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c73d, "(evex_Wb ext 202)", xx, xx, xx, xx, xx, mrm|reqp, x, 202},
    {EVEX_Wb_EXT, 0x6638c73e, "(evex_Wb ext 204)", xx, xx, xx, xx, xx, mrm|reqp, x, 204},
    {INVALID, 0x6638c73f, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
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
    {OP_movups, 0x0f1010, "movups", Vps, xx, Wps, xx, xx, mrm, x, tpe[1][0]},
    {MOD_EXT,   0xf30f1010, "(mod ext 18)",  xx, xx, xx, xx, xx, mrm, x, 18},
    {OP_movupd, 0x660f1010, "movupd", Vpd, xx, Wpd, xx, xx, mrm, x, tpe[1][2]},
    {MOD_EXT,   0xf20f1010, "(mod ext 19)",  xx, xx, xx, xx, xx, mrm, x, 19},
    {OP_vmovups,   0x0f1010, "vmovups", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tpe[1][4]},
    {MOD_EXT,    0xf30f1010, "(mod ext 8)", xx, xx, xx, xx, xx, mrm|vex, x, 8},
    {OP_vmovupd, 0x660f1010, "vmovupd", Vvd, xx, Wvd, xx, xx, mrm|vex, x, tpe[1][6]},
    {MOD_EXT,    0xf20f1010, "(mod ext 9)", xx, xx, xx, xx, xx, mrm|vex, x, 9},
    {EVEX_Wb_EXT, 0x0f1010, "(evex_Wb ext 0)", xx, xx, xx, xx, xx, mrm|evex, x, 0},
    {MOD_EXT,    0xf30f1010, "(mod ext 20)", xx, xx, xx, xx, xx, mrm|evex, x, 20},
    {EVEX_Wb_EXT, 0x660f1010, "(evex_Wb ext 2)", xx, xx, xx, xx, xx, mrm|evex, x, 2},
    {MOD_EXT,    0xf20f1010, "(mod ext 21)", xx, xx, xx, xx, xx, mrm|evex, x, 21},
  }, /* prefix extension 1 */
  {
    {OP_movups, 0x0f1110, "movups", Wps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {OP_movss,  0xf30f1110, "movss",  Wss, xx, Vss, xx, xx, mrm, x, END_LIST},
    {OP_movupd, 0x660f1110, "movupd", Wpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {OP_movsd,  0xf20f1110, "movsd",  Wsd, xx, Vsd, xx, xx, mrm, x, END_LIST},
    {OP_vmovups,   0x0f1110, "vmovups", Wvs, xx, Vvs, xx, xx, mrm|vex, x, tevexwb[0][0]},
    {MOD_EXT,    0xf30f1110, "(mod ext 10)", xx, xx, xx, xx, xx, mrm|vex, x, 10},
    {OP_vmovupd, 0x660f1110, "vmovupd", Wvd, xx, Vvd, xx, xx, mrm|vex, x, tevexwb[2][2]},
    {MOD_EXT,    0xf20f1110, "(mod ext 11)", xx, xx, xx, xx, xx, mrm|vex, x, 11},
    {EVEX_Wb_EXT, 0x0f1110, "(evex_Wb ext 1)", xx, xx, xx, xx, xx, mrm|evex, x, 1},
    {MOD_EXT,    0xf30f1110, "(mod ext 22)", xx, xx, xx, xx, xx, mrm|evex, x, 22},
    {EVEX_Wb_EXT, 0x660f1110, "(evex_Wb ext 3)", xx, xx, xx, xx, xx, mrm|evex, x, 3},
    {MOD_EXT,    0xf20f1110, "(mod ext 23)", xx, xx, xx, xx, xx, mrm|evex, x, 23},
  }, /* prefix extension 2 */
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
    {OP_vmovsldup,0xf30f1210, "vmovsldup", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[18][0]},
    {OP_vmovlpd,  0x660f1210, "vmovlpd", Vq_dq, xx, Hq_dq, Mq, xx, mrm|vex, x, tpe[3][6]},
    {OP_vmovddup, 0xf20f1210, "vmovddup", Vvd, xx, Wx, xx, xx, mrm|vex, x, tevexwb[19][2]},
    {EVEX_Wb_EXT, 0x0f1210, "(evex_Wb ext 14)", xx, xx, xx, xx, xx, mrm|evex, x, 14},
    {EVEX_Wb_EXT, 0xf30f1210, "(evex_Wb ext 18)", xx, xx, xx, xx, xx, mrm|evex, x, 18},
    {EVEX_Wb_EXT, 0x660f1210, "(evex_Wb ext 16)", xx, xx, xx, xx, xx, mrm|evex, x, 16},
    {EVEX_Wb_EXT, 0xf20f1210, "(evex_Wb ext 19)", xx, xx, xx, xx, xx, mrm|evex, x, 19},
  }, /* prefix extension 3 */
  {
    {OP_movlps, 0x0f1310, "movlps", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movlpd, 0x660f1310, "movlpd", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovlps, 0x0f1310, "vmovlps", Mq, xx, Vq_dq, xx, xx, mrm|vex, x, tevexwb[14][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovlpd, 0x660f1310, "vmovlpd", Mq, xx, Vq_dq, xx, xx, mrm|vex, x, tevexwb[16][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1310, "(evex_Wb ext 15)", xx, xx, xx, xx, xx, mrm|evex, x, 15},
    {INVALID, 0xf30f1310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1310, "(evex_Wb ext 17)", xx, xx, xx, xx, xx, mrm|evex, x, 17},
    {INVALID, 0xf20f1310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 4 */
  {
    {OP_unpcklps, 0x0f1410, "unpcklps", Vps, xx, Wq_dq, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_unpcklpd, 0x660f1410, "unpcklpd", Vpd, xx, Wq_dq, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpcklps, 0x0f1410, "vunpcklps", Vvs, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[25][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpcklpd, 0x660f1410, "vunpcklpd", Vvd, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[26][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1410, "(evex_Wb ext 25)", xx, xx, xx, xx, xx, mrm|evex, x, 25},
    {INVALID, 0xf30f1410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1410, "(evex_Wb ext 26)", xx, xx, xx, xx, xx, mrm|evex, x, 26},
    {INVALID, 0xf20f1410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 5 */
  {
    {OP_unpckhps, 0x0f1510, "unpckhps", Vps, xx, Wdq, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_unpckhpd, 0x660f1510, "unpckhpd", Vpd, xx, Wdq, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpckhps, 0x0f1510, "vunpckhps", Vvs, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[27][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vunpckhpd, 0x660f1510, "vunpckhpd", Vvd, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[28][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1510, "(evex_Wb ext 27)", xx, xx, xx, xx, xx, mrm|evex, x, 27},
    {INVALID, 0xf30f1510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1510, "(evex_Wb ext 28)", xx, xx, xx, xx, xx, mrm|evex, x, 28},
    {INVALID, 0xf20f1510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 6 */
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
    {OP_vmovshdup, 0xf30f1610, "vmovshdup", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[24][0]},
    {OP_vmovhpd, 0x660f1610, "vmovhpd", Vq_dq, xx, Hq_dq, Mq, xx, mrm|vex|reqL0, x, tpe[7][6]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1610, "(evex_Wb ext 20)", xx, xx, xx, xx, xx, mrm|evex, x, 20},
    {EVEX_Wb_EXT, 0xf30f1610, "(evex_Wb ext 24)", xx, xx, xx, xx, xx, mrm|evex, x, 24},
    {EVEX_Wb_EXT, 0x660f1610, "(evex_Wb ext 22)", xx, xx, xx, xx, xx, mrm|evex, x, 22},
    {INVALID, 0xf20f1610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 7 */
  {
    {OP_movhps, 0x0f1710, "movhps", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movhpd, 0x660f1710, "movhpd", Mq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhps, 0x0f1710, "vmovhps", Mq, xx, Vq_dq, xx, xx, mrm|vex|reqL0, x, tevexwb[20][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovhpd, 0x660f1710, "vmovhpd", Mq, xx, Vq_dq, xx, xx, mrm|vex|reqL0, x, tevexwb[22][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f1710, "(evex_Wb ext 21)", xx, xx, xx, xx, xx, mrm|evex, x, 21},
    {INVALID, 0xf30f1710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f1710, "(evex_Wb ext 23)", xx, xx, xx, xx, xx, mrm|evex, x, 23},
    {INVALID, 0xf20f1710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 8 */
  {
    {OP_movaps, 0x0f2810, "movaps", Vps, xx, Wps, xx, xx, mrm, x, tpe[9][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movapd, 0x660f2810, "movapd", Vpd, xx, Wpd, xx, xx, mrm, x, tpe[9][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovaps, 0x0f2810, "vmovaps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tpe[9][4]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovapd, 0x660f2810, "vmovapd", Vvd, xx, Wvd, xx, xx, mrm|vex, x, tpe[9][6]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0x0f2810, "(evex_Wb ext 4)", xx, xx, xx, xx, xx, mrm|evex, x, 4},
    {INVALID,    0xf30f2810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2810, "(evex_Wb ext 6)", xx, xx, xx, xx, xx, mrm|evex, x, 6},
    {INVALID,    0xf20f2810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 9 */
  {
    {OP_movaps, 0x0f2910, "movaps", Wps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movapd, 0x660f2910, "movapd", Wpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovaps, 0x0f2910, "vmovaps", Wvs, xx, Vvs, xx, xx, mrm|vex, x, tevexwb[4][0]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovapd, 0x660f2910, "vmovapd", Wvd, xx, Vvd, xx, xx, mrm|vex, x, tevexwb[6][2]},
    {INVALID, 0x00000000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0x0f2910, "(evex_Wb ext 5)", xx, xx, xx, xx, xx, mrm|evex, x, 5},
    {INVALID,    0xf30f2910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2910, "(evex_Wb ext 7)", xx, xx, xx, xx, xx, mrm|evex, x, 7},
    {INVALID,    0xf20f2910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 10 */
  {
    {OP_cvtpi2ps,  0x0f2a10, "cvtpi2ps", Vq_dq, xx, Qq, xx, xx, mrm, x, END_LIST},
    {OP_cvtsi2ss, 0xf30f2a10, "cvtsi2ss", Vss, xx, Ey, xx, xx, mrm, x, END_LIST},
    {OP_cvtpi2pd, 0x660f2a10, "cvtpi2pd", Vpd, xx, Qq, xx, xx, mrm, x, END_LIST},
    {OP_cvtsi2sd, 0xf20f2a10, "cvtsi2sd", Vsd, xx, Ey, xx, xx, mrm, x, END_LIST},
    {INVALID,  0x0f2a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsi2ss, 0xf30f2a10, "vcvtsi2ss", Vdq, xx, H12_dq, Ey, xx, mrm|vex, x, tevexwb[31][0]},
    {INVALID, 0x660f2a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsi2sd, 0xf20f2a10, "vcvtsi2sd", Vdq, xx, Hsd, Ey, xx, mrm|vex, x, tevexwb[32][0]},
    {INVALID,   0x0f2a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f2a10, "(evex_Wb ext 31)", xx, xx, xx, xx, xx, mrm|evex, x, 31},
    {INVALID, 0x660f2a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f2a10, "(evex_Wb ext 32)", xx, xx, xx, xx, xx, mrm|evex, x, 32},
  }, /* prefix extension 11 */
  {
    {OP_movntps,   0x0f2b10, "movntps", Mps, xx, Vps, xx, xx, mrm, x, END_LIST},
    {OP_movntss, 0xf30f2b10, "movntss", Mss, xx, Vss, xx, xx, mrm, x, END_LIST},
    {OP_movntpd, 0x660f2b10, "movntpd", Mpd, xx, Vpd, xx, xx, mrm, x, END_LIST},
    {OP_movntsd, 0xf20f2b10, "movntsd", Msd, xx, Vsd, xx, xx, mrm, x, END_LIST},
    {OP_vmovntps,   0x0f2b10, "vmovntps", Mvs, xx, Vvs, xx, xx, mrm|vex, x, tevexwb[33][0]},
    /* XXX: AMD doesn't list movntss in their new manual => assuming no vex version */
    {INVALID, 0xf30f2b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovntpd, 0x660f2b10, "vmovntpd", Mvd, xx, Vvd, xx, xx, mrm|vex, x, tevexwb[34][2]},
    {INVALID, 0xf20f2b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f2b10, "(evex_Wb ext 33)", xx, xx, xx, xx, xx, mrm|evex, x, 33},
    {INVALID, 0xf30f2b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2b10, "(evex_Wb ext 34)", xx, xx, xx, xx, xx, mrm|evex, x, 34},
    {INVALID, 0xf20f2b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 12 */
  {
    {OP_cvttps2pi, 0x0f2c10, "cvttps2pi", Pq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvttss2si, 0xf30f2c10, "cvttss2si", Gy, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvttpd2pi, 0x660f2c10, "cvttpd2pi", Pq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvttsd2si, 0xf20f2c10, "cvttsd2si", Gy, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f2c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvttss2si, 0xf30f2c10, "vcvttss2si", Gy, xx, Wss, xx, xx, mrm|vex, x, tevexwb[35][0]},
    {INVALID, 0x660f2c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvttsd2si, 0xf20f2c10, "vcvttsd2si", Gy, xx, Wsd, xx, xx, mrm|vex, x, tevexwb[36][0]},
    {INVALID,   0x0f2c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f2c10, "(evex_Wb ext 35)", xx, xx, xx, xx, xx, mrm|evex, x, 35},
    {INVALID, 0x660f2c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f2c10, "(evex_Wb ext 36)", xx, xx, xx, xx, xx, mrm|evex, x, 36},
  }, /* prefix extension 13 */
  {
    {OP_cvtps2pi, 0x0f2d10, "cvtps2pi", Pq, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvtss2si, 0xf30f2d10, "cvtss2si", Gy, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2pi, 0x660f2d10, "cvtpd2pi", Pq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtsd2si, 0xf20f2d10, "cvtsd2si", Gy, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x0f2d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtss2si, 0xf30f2d10, "vcvtss2si", Gy, xx, Wss, xx, xx, mrm|vex, x, tevexwb[29][0]},
    {INVALID, 0x660f2d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtsd2si, 0xf20f2d10, "vcvtsd2si", Gy, xx, Wsd, xx, xx, mrm|vex, x, tevexwb[30][0]},
    {INVALID,   0x0f2d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f2d10, "(evex_Wb ext 29)", xx, xx, xx, xx, xx, mrm|evex, x, 29},
    {INVALID, 0x660f2d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f2d10, "(evex_Wb ext 30)", xx, xx, xx, xx, xx, mrm|evex, x, 30},
  }, /* prefix extension 14 */
  {
    {OP_ucomiss, 0x0f2e10, "ucomiss", xx, xx, Vss, Wss, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf30f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_ucomisd, 0x660f2e10, "ucomisd", xx, xx, Vsd, Wsd, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf20f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vucomiss, 0x0f2e10, "vucomiss", xx, xx, Vss, Wss, xx, mrm|vex, fW6, tevexwb[37][0]},
    {INVALID, 0xf30f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vucomisd, 0x660f2e10, "vucomisd", xx, xx, Vsd, Wsd, xx, mrm|vex, fW6, tevexwb[38][2]},
    {INVALID, 0xf20f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f2e10, "(evex_Wb ext 37)", xx, xx, xx, xx, xx, mrm|evex, x, 37},
    {INVALID, 0xf30f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2e10, "(evex_Wb ext 38)", xx, xx, xx, xx, xx, mrm|evex, x, 38},
    {INVALID, 0xf20f2e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 15 */
  {
    {OP_comiss,  0x0f2f10, "comiss",  xx, xx, Vss, Wss, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf30f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_comisd,  0x660f2f10, "comisd",  xx, xx, Vsd, Wsd, xx, mrm, fW6, END_LIST},
    {INVALID, 0xf20f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcomiss,  0x0f2f10, "vcomiss",  xx, xx, Vss, Wss, xx, mrm|vex, fW6, tevexwb[39][0]},
    {INVALID, 0xf30f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vcomisd,  0x660f2f10, "vcomisd",  xx, xx, Vsd, Wsd, xx, mrm|vex, fW6, tevexwb[40][2]},
    {INVALID, 0xf20f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f2e10, "(evex_Wb ext 39)", xx, xx, xx, xx, xx, mrm|evex, x, 39},
    {INVALID, 0xf30f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f2e10, "(evex_Wb ext 40)", xx, xx, xx, xx, xx, mrm|evex, x, 40},
    {INVALID, 0xf20f2f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 16 */
  {
    {OP_movmskps, 0x0f5010, "movmskps", Gr, xx, Ups, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_movmskpd, 0x660f5010, "movmskpd", Gr, xx, Upd, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovmskps, 0x0f5010, "vmovmskps", Gr, xx, Uvs, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf30f5010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovmskpd, 0x660f5010, "vmovmskpd", Gr, xx, Uvd, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0xf20f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 17 */
  {
    {OP_sqrtps, 0x0f5110, "sqrtps", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_sqrtss, 0xf30f5110, "sqrtss", Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_sqrtpd, 0x660f5110, "sqrtpd", Vpd, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_sqrtsd, 0xf20f5110, "sqrtsd", Vsd, xx, Wsd, xx, xx, mrm, x, END_LIST},
    {OP_vsqrtps, 0x0f5110, "vsqrtps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[265][0]},
    {OP_vsqrtss, 0xf30f5110, "vsqrtss", Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, tevexwb[266][0]},
    {OP_vsqrtpd, 0x660f5110, "vsqrtpd", Vvd, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[265][2]},
    {OP_vsqrtsd, 0xf20f5110, "vsqrtsd", Vdq, xx, Hsd, Wsd, xx, mrm|vex, x, tevexwb[266][2]},
    {EVEX_Wb_EXT, 0x0f5110, "(evex_Wb ext 265)", xx, xx, xx, xx, xx, mrm|evex, x, 265},
    {EVEX_Wb_EXT, 0xf30f5110, "(evex_Wb ext 266)", xx, xx, xx, xx, xx, mrm|evex, x, 266},
    {EVEX_Wb_EXT, 0x660f5110, "(evex_Wb ext 265)", xx, xx, xx, xx, xx, mrm|evex, x, 265},
    {EVEX_Wb_EXT, 0xf20f5110, "(evex_Wb ext 266)", xx, xx, xx, xx, xx, mrm|evex, x, 266},
  }, /* prefix extension 18 */
  {
    {OP_rsqrtps, 0x0f5210, "rsqrtps", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_rsqrtss, 0xf30f5210, "rsqrtss", Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x660f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrsqrtps, 0x0f5210, "vrsqrtps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vrsqrtss, 0xf30f5210, "vrsqrtss", Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 19 */
  {
    {OP_rcpps, 0x0f5310, "rcpps", Vps, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_rcpss, 0xf30f5310, "rcpss", Vss, xx, Wss, xx, xx, mrm, x, END_LIST},
    {INVALID, 0x660f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrcpps, 0x0f5310, "vrcpps", Vvs, xx, Wvs, xx, xx, mrm|vex, x, END_LIST},
    {OP_vrcpss, 0xf30f5310, "vrcpss", Vdq, xx, H12_dq, Wss, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x660f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f5310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 20 */
  {
    {OP_andps,  0x0f5410, "andps",  Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_andpd,  0x660f5410, "andpd",  Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vandps,  0x0f5410, "vandps",  Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[205][0]},
    {INVALID, 0xf30f5410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vandpd,  0x660f5410, "vandpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[205][2]},
    {INVALID, 0xf20f5410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5410, "(evex_Wb ext 205)", xx, xx, xx, xx, xx, mrm|evex, x, 205},
    {INVALID, 0xf30f5410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5450, "(evex_Wb ext 205)", xx, xx, xx, xx, xx, mrm|evex, x, 205},
    {INVALID, 0xf20f5410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 21 */
  {
    {OP_andnps, 0x0f5510, "andnps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_andnpd, 0x660f5510, "andnpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vandnps, 0x0f5510, "vandnps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[206][0]},
    {INVALID, 0xf30f5510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vandnpd, 0x660f5510, "vandnpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[206][2]},
    {INVALID, 0xf20f5510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5510, "(evex_Wb ext 206)", xx, xx, xx, xx, xx, mrm|evex, x, 206},
    {INVALID, 0xf30f5510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5550, "(evex_Wb ext 206)", xx, xx, xx, xx, xx, mrm|evex, x, 206},
    {INVALID, 0xf20f5510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 22 */
  {
    {OP_orps,   0x0f5610, "orps",   Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_orpd,   0x660f5610, "orpd",   Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vorps,   0x0f5610, "vorps",   Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[207][0]},
    {INVALID, 0xf30f5610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vorpd,   0x660f5610, "vorpd",   Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[207][2]},
    {INVALID, 0xf20f5610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5610, "(evex_Wb ext 207)", xx, xx, xx, xx, xx, mrm|evex, x, 207},
    {INVALID, 0xf30f5610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5650, "(evex_Wb ext 207)", xx, xx, xx, xx, xx, mrm|evex, x, 207},
    {INVALID, 0xf20f5610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 23 */
  {
    {OP_xorps,  0x0f5710, "xorps",  Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID, 0xf30f5710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_xorpd,  0x660f5710, "xorpd",  Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vxorps,  0x0f5710, "vxorps",  Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[208][0]},
    {INVALID, 0xf30f5710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vxorpd,  0x660f5710, "vxorpd",  Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[208][2]},
    {INVALID, 0xf20f5710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5710, "(evex_Wb ext 208)", xx, xx, xx, xx, xx, mrm|evex, x, 208},
    {INVALID, 0xf30f5710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f5750, "(evex_Wb ext 208)", xx, xx, xx, xx, xx, mrm|evex, x, 208},
    {INVALID, 0xf20f5710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 24 */
  {
    {OP_addps, 0x0f5810, "addps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_addss, 0xf30f5810, "addss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_addpd, 0x660f5810, "addpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_addsd, 0xf20f5810, "addsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vaddps, 0x0f5810, "vaddps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[209][0]},
    {OP_vaddss, 0xf30f5810, "vaddss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[255][0]},
    {OP_vaddpd, 0x660f5810, "vaddpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[209][2]},
    {OP_vaddsd, 0xf20f5810, "vaddsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[255][2]},
    {EVEX_Wb_EXT, 0x0f5810, "(evex_Wb ext 209)", xx, xx, xx, xx, xx, mrm|evex, x, 209},
    {EVEX_Wb_EXT, 0xf30f5800, "(evex_Wb ext 255)", xx, xx, xx, xx, xx, mrm|evex, x, 255},
    {EVEX_Wb_EXT, 0x660f5850, "(evex_Wb ext 209)", xx, xx, xx, xx, xx, mrm|evex, x, 209},
    {EVEX_Wb_EXT, 0xf20f5840, "(evex_Wb ext 255)", xx, xx, xx, xx, xx, mrm|evex, x, 255},
  }, /* prefix extension 25 */
  {
    {OP_mulps, 0x0f5910, "mulps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_mulss, 0xf30f5910, "mulss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_mulpd, 0x660f5910, "mulpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_mulsd, 0xf20f5910, "mulsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vmulps, 0x0f5910, "vmulps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[210][0]},
    {OP_vmulss, 0xf30f5910, "vmulss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[256][0]},
    {OP_vmulpd, 0x660f5910, "vmulpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[210][2]},
    {OP_vmulsd, 0xf20f5910, "vmulsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[256][2]},
    {EVEX_Wb_EXT, 0x0f5910, "(evex_Wb ext 210)", xx, xx, xx, xx, xx, mrm|evex, x, 210},
    {EVEX_Wb_EXT, 0xf30f5900, "(evex_Wb ext 256)", xx, xx, xx, xx, xx, mrm|evex, x, 256},
    {EVEX_Wb_EXT, 0x660f5950, "(evex_Wb ext 210)", xx, xx, xx, xx, xx, mrm|evex, x, 210},
    {EVEX_Wb_EXT, 0xf20f5940, "(evex_Wb ext 256)", xx, xx, xx, xx, xx, mrm|evex, x, 256},
  }, /* prefix extension 26 */
  {
    {OP_cvtps2pd, 0x0f5a10, "cvtps2pd", Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvtss2sd, 0xf30f5a10, "cvtss2sd", Vsd, xx, Wss, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2ps, 0x660f5a10, "cvtpd2ps", Vps, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtsd2ss, 0xf20f5a10, "cvtsd2ss", Vss, xx, Wsd, xx, xx, mrm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2pd, 0x0f5a10, "vcvtps2pd", Vvd, xx, Wh_x, xx, xx, mrm|vex, x, tevexwb[211][0]},
    {OP_vcvtss2sd, 0xf30f5a10, "vcvtss2sd", Vdq, xx, Hsd, Wss, xx, mrm|vex, x, tevexwb[257][0]},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtpd2ps, 0x660f5a10, "vcvtpd2ps", Vvs, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[211][2]},
    {OP_vcvtsd2ss, 0xf20f5a10, "vcvtsd2ss", Vdq, xx, H12_dq, Wsd, xx, mrm|vex, x, tevexwb[257][2]},
    {EVEX_Wb_EXT, 0x0f5a10, "(evex_Wb ext 211)", xx, xx, xx, xx, xx, mrm|evex, x, 211},
    {EVEX_Wb_EXT, 0xf30f5a10, "(evex_Wb ext 257)", xx, xx, xx, xx, xx, mrm|evex, x, 257},
    {EVEX_Wb_EXT, 0x660f5a50, "(evex_Wb ext 211)", xx, xx, xx, xx, xx, mrm|evex, x, 211},
    {EVEX_Wb_EXT, 0xf20f5a50, "(evex_Wb ext 257)", xx, xx, xx, xx, xx, mrm|evex, x, 257},
  }, /* prefix extension 27 */
  {
    {OP_cvtdq2ps, 0x0f5b10, "cvtdq2ps", Vps, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {OP_cvttps2dq, 0xf30f5b10, "cvttps2dq", Vdq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {OP_cvtps2dq, 0x660f5b10, "cvtps2dq", Vdq, xx, Wps, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f5b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtdq2ps, 0x0f5b10, "vcvtdq2ps", Vvs, xx, Wx, xx, xx, mrm|vex, x, tevexwb[56][0]},
    {OP_vcvttps2dq, 0xf30f5b10, "vcvttps2dq", Vx, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[250][0]},
    {OP_vcvtps2dq, 0x660f5b10, "vcvtps2dq", Vx, xx, Wvs, xx, xx, mrm|vex, x, tevexwb[249][0]},
    {INVALID, 0xf20f5b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f5b10, "(evex_Wb ext 56)", xx, xx, xx, xx, xx, mrm|evex, x, 56},
    {EVEX_Wb_EXT, 0x660f5b00, "(evex_Wb ext 250)", xx, xx, xx, xx, xx, mrm|evex, x, 250},
    {EVEX_Wb_EXT, 0x660f5b00, "(evex_Wb ext 249)", xx, xx, xx, xx, xx, mrm|evex, x, 249},
    {INVALID, 0xf20f5b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 28 */
  {
    {OP_subps, 0x0f5c10, "subps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_subss, 0xf30f5c10, "subss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_subpd, 0x660f5c10, "subpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_subsd, 0xf20f5c10, "subsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vsubps, 0x0f5c10, "vsubps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[212][0]},
    {OP_vsubss, 0xf30f5c10, "vsubss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[258][0]},
    {OP_vsubpd, 0x660f5c10, "vsubpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[212][2]},
    {OP_vsubsd, 0xf20f5c10, "vsubsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[258][2]},
    {EVEX_Wb_EXT, 0x0f5c10, "(evex_Wb ext 212)", xx, xx, xx, xx, xx, mrm|evex, x, 212},
    {EVEX_Wb_EXT, 0xf30f5c00, "(evex_Wb ext 258)", xx, xx, xx, xx, xx, mrm|evex, x, 258},
    {EVEX_Wb_EXT, 0x660f5c50, "(evex_Wb ext 212)", xx, xx, xx, xx, xx, mrm|evex, x, 212},
    {EVEX_Wb_EXT, 0xf20f5c40, "(evex_Wb ext 258)", xx, xx, xx, xx, xx, mrm|evex, x, 258},
  }, /* prefix extension 29 */
  {
    {OP_minps, 0x0f5d10, "minps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_minss, 0xf30f5d10, "minss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_minpd, 0x660f5d10, "minpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_minsd, 0xf20f5d10, "minsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vminps, 0x0f5d10, "vminps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[213][0]},
    {OP_vminss, 0xf30f5d10, "vminss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[259][0]},
    {OP_vminpd, 0x660f5d10, "vminpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[213][2]},
    {OP_vminsd, 0xf20f5d10, "vminsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[259][2]},
    {EVEX_Wb_EXT, 0x0f5d10, "(evex_Wb ext 213)", xx, xx, xx, xx, xx, mrm|evex, x, 213},
    {EVEX_Wb_EXT, 0xf30f5d00, "(evex_Wb ext 259)", xx, xx, xx, xx, xx, mrm|evex, x, 259},
    {EVEX_Wb_EXT, 0x660f5d50, "(evex_Wb ext 213)", xx, xx, xx, xx, xx, mrm|evex, x, 213},
    {EVEX_Wb_EXT, 0xf20f5d40, "(evex_Wb ext 259)", xx, xx, xx, xx, xx, mrm|evex, x, 259},
  }, /* prefix extension 30 */
  {
    {OP_divps, 0x0f5e10, "divps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_divss, 0xf30f5e10, "divss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_divpd, 0x660f5e10, "divpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_divsd, 0xf20f5e10, "divsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vdivps, 0x0f5e10, "vdivps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[214][0]},
    {OP_vdivss, 0xf30f5e10, "vdivss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[260][0]},
    {OP_vdivpd, 0x660f5e10, "vdivpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[214][2]},
    {OP_vdivsd, 0xf20f5e10, "vdivsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[260][2]},
    {EVEX_Wb_EXT, 0x0f5e10, "(evex_Wb ext 214)", xx, xx, xx, xx, xx, mrm|evex, x, 214},
    {EVEX_Wb_EXT, 0xf30f5e00, "(evex_Wb ext 260)", xx, xx, xx, xx, xx, mrm|evex, x, 260},
    {EVEX_Wb_EXT, 0x660f5e50, "(evex_Wb ext 214)", xx, xx, xx, xx, xx, mrm|evex, x, 214},
    {EVEX_Wb_EXT, 0xf20f5e40, "(evex_Wb ext 260)", xx, xx, xx, xx, xx, mrm|evex, x, 260},
  }, /* prefix extension 31 */
  {
    {OP_maxps, 0x0f5f10, "maxps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {OP_maxss, 0xf30f5f10, "maxss", Vss, xx, Wss, Vss, xx, mrm, x, END_LIST},
    {OP_maxpd, 0x660f5f10, "maxpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_maxsd, 0xf20f5f10, "maxsd", Vsd, xx, Wsd, Vsd, xx, mrm, x, END_LIST},
    {OP_vmaxps, 0x0f5f10, "vmaxps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, tevexwb[215][0]},
    {OP_vmaxss, 0xf30f5f10, "vmaxss", Vdq, xx, Hdq, Wss, xx, mrm|vex, x, tevexwb[261][0]},
    {OP_vmaxpd, 0x660f5f10, "vmaxpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, tevexwb[215][2]},
    {OP_vmaxsd, 0xf20f5f10, "vmaxsd", Vdq, xx, Hdq, Wsd, xx, mrm|vex, x, tevexwb[261][2]},
    {EVEX_Wb_EXT, 0x0f5f10, "(evex_Wb ext 215)", xx, xx, xx, xx, xx, mrm|evex, x, 215},
    {EVEX_Wb_EXT, 0xf30f5f00, "(evex_Wb ext 261)", xx, xx, xx, xx, xx, mrm|evex, x, 261},
    {EVEX_Wb_EXT, 0x660f5f50, "(evex_Wb ext 215)", xx, xx, xx, xx, xx, mrm|evex, x, 215},
    {EVEX_Wb_EXT, 0xf20f5f40, "(evex_Wb ext 261)", xx, xx, xx, xx, xx, mrm|evex, x, 261},
  }, /* prefix extension 32 */
  {
    {OP_punpcklbw,   0x0f6010, "punpcklbw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[32][2]},
    {INVALID,      0xf30f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklbw, 0x660f6010, "punpcklbw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6010,   "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklbw, 0x660f6010, "vpunpcklbw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[32][10]},
    {INVALID,      0xf20f6010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpcklbw, 0x660f6000, "vpunpcklbw", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 33 */
  {
    {OP_punpcklwd,   0x0f6110, "punpcklwd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[33][2]},
    {INVALID,      0xf30f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklwd, 0x660f6110, "punpcklwd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklwd, 0x660f6110, "vpunpcklwd", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[33][10]},
    {INVALID,      0xf20f6110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpcklwd, 0x660f6100, "vpunpcklwd", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 34 */
  {
    {OP_punpckldq,   0x0f6210, "punpckldq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[34][2]},
    {INVALID,      0xf30f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckldq, 0x660f6210, "punpckldq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckldq, 0x660f6210, "vpunpckldq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[235][0]},
    {INVALID,      0xf20f6210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6200, "(evex_Wb ext 235)", xx, xx, xx, xx, xx, mrm|evex, x, 235},
    {INVALID, 0xf20f6210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 35 */
  {
    {OP_packsswb,   0x0f6310, "packsswb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[35][2]},
    {INVALID,     0xf30f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packsswb, 0x660f6310, "packsswb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpacksswb, 0x660f6310, "vpacksswb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[35][10]},
    {INVALID,     0xf20f6310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpacksswb, 0x660f6300, "vpacksswb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 36 */
  {
    {OP_pcmpgtb,   0x0f6410, "pcmpgtb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[36][2]},
    {INVALID,    0xf30f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtb, 0x660f6410, "pcmpgtb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtb, 0x660f6410, "vpcmpgtb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[36][10]},
    {INVALID,    0xf20f6410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpgtb, 0x660f6400, "vpcmpgtb", KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 37 */
  {
    {OP_pcmpgtw,   0x0f6510, "pcmpgtw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[37][2]},
    {INVALID,    0xf30f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtw, 0x660f6510, "pcmpgtw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtw, 0x660f6510, "vpcmpgtw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[37][10]},
    {INVALID,    0xf20f6510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpgtw, 0x660f6500, "vpcmpgtw", KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 38 */
  {
    {OP_pcmpgtd,   0x0f6610, "pcmpgtd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[38][2]},
    {INVALID,    0xf30f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpgtd, 0x660f6610, "pcmpgtd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpgtd, 0x660f6610, "vpcmpgtd", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[236][0]},
    {INVALID,    0xf20f6610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6600, "(evex_Wb ext 236)", xx, xx, xx, xx, xx, mrm|evex, x, 236},
    {INVALID, 0xf20f6610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 39 */
  {
    {OP_packuswb,   0x0f6710, "packuswb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[39][2]},
    {INVALID,     0xf30f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packuswb, 0x660f6710, "packuswb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpackuswb, 0x660f6710, "vpackuswb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[39][10]},
    {INVALID,     0xf20f6710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpackuswb, 0x660f6700, "vpackuswb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 40 */
  {
    {OP_punpckhbw,   0x0f6810, "punpckhbw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[40][2]},
    {INVALID,      0xf30f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhbw, 0x660f6810, "punpckhbw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhbw, 0x660f6810, "vpunpckhbw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[40][10]},
    {INVALID,      0xf20f6810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpckhbw, 0x660f6800, "vpunpckhbw", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 41 */
  {
    {OP_punpckhwd,   0x0f6910, "punpckhwd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[41][2]},
    {INVALID,      0xf30f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhwd, 0x660f6910, "punpckhwd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhwd, 0x660f6910, "vpunpckhwd", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[41][10]},
    {INVALID,      0xf20f6910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpckhwd, 0x660f6900, "vpunpckhwd", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f6910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 42 */
  {
    {OP_punpckhdq,   0x0f6a10, "punpckhdq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[42][2]},
    {INVALID,      0xf30f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhdq, 0x660f6a10, "punpckhdq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf20f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,        0x0f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf30f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhdq, 0x660f6a10, "vpunpckhdq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[237][0]},
    {INVALID,      0xf20f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6a00, "(evex_Wb ext 237)", xx, xx, xx, xx, xx, mrm|evex, x, 237},
    {INVALID, 0xf20f6a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 43 */
  {
    {OP_packssdw,   0x0f6b10, "packssdw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[43][2]},
    {INVALID,     0xf30f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_packssdw, 0x660f6b10, "packssdw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf20f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0x0f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf30f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpackssdw, 0x660f6b10, "vpackssdw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[238][0]},
    {INVALID,     0xf20f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6b00, "(evex_Wb ext 238)", xx, xx, xx, xx, xx, mrm|evex, x, 238},
    {INVALID, 0xf20f6b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 44 */
  {
    {INVALID,         0x0f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpcklqdq, 0x660f6c10, "punpcklqdq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,       0xf20f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpcklqdq, 0x660f6c10, "vpunpcklqdq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[216][2]},
    {INVALID,       0xf20f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6c40, "(evex_Wb ext 216)", xx, xx, xx, xx, xx, mrm|evex, x, 216},
    {INVALID, 0xf20f6c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 45 */
  {
    {INVALID,         0x0f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_punpckhqdq, 0x660f6d10, "punpckhqdq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,       0xf20f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpunpckhqdq, 0x660f6d10, "vpunpckhqdq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[219][2]},
    {INVALID,       0xf20f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6d40, "(evex_Wb ext 219)", xx, xx, xx, xx, xx, mrm|evex, x, 219},
    {INVALID, 0xf20f6d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 46 */
  {
    /* movd zeroes the top bits when the destination is an mmx or xmm reg */
    {OP_movd,   0x0f6e10, "movd", Pq, xx, Ey, xx, xx, mrm, x, tpe[46][2]},
    {INVALID, 0xf30f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    /* XXX: the opcode is called movq with rex.w + 0f. */
    {OP_movd, 0x660f6e10, "movd", Vdq, xx, Ey, xx, xx, mrm, x, tpe[51][0]},
    {INVALID, 0xf20f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x660f6e10, "(vex_W ext 108)", xx, xx, xx, xx, xx, mrm|vex, x, 108},
    {INVALID, 0xf20f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f6e10, "(evex_Wb ext 136)", xx, xx, xx, xx, xx, mrm|evex, x, 136},
    {INVALID, 0xf20f6e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 47: all assumed to have Ib */
  {
    {OP_pshufw,   0x0f7010, "pshufw",   Pq, xx, Qq, Ib, xx, mrm, x, END_LIST},
    {OP_pshufhw, 0xf30f7010, "pshufhw", Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {OP_pshufd,  0x660f7010, "pshufd",  Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {OP_pshuflw, 0xf20f7010, "pshuflw", Vdq, xx, Wdq, Ib, xx, mrm, x, END_LIST},
    {INVALID,       0x0f7010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpshufhw, 0xf30f7010, "vpshufhw", Vx, xx, Wx, Ib, xx, mrm|vex, x, tpe[47][9]},
    {OP_vpshufd,  0x660f7010, "vpshufd",  Vx, xx, Wx, Ib, xx, mrm|vex, x, tevexwb[239][0]},
    {OP_vpshuflw, 0xf20f7010, "vpshuflw", Vx, xx, Wx, Ib, xx, mrm|vex, x, tpe[47][11]},
    {INVALID,   0x0f7010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufhw, 0xf30f7000, "vpshufhw", Ve, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {EVEX_Wb_EXT, 0x660f7000, "(evex_Wb ext 239)", xx, xx, xx, xx, xx, mrm|evex, x, 239},
    {OP_vpshuflw, 0xf20f7000, "vpshuflw", Ve, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
  }, /* prefix extension 48 */
  {
    {OP_pcmpeqb,   0x0f7410, "pcmpeqb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[48][2]},
    {INVALID,    0xf30f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqb, 0x660f7410, "pcmpeqb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqb, 0x660f7410, "vpcmpeqb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[48][10]},
    {INVALID,    0xf20f7410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f7410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpeqb, 0x660f7400, "vpcmpeqb", KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 49 */
  {
    {OP_pcmpeqw,   0x0f7510, "pcmpeqw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[49][2]},
    {INVALID,    0xf30f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqw, 0x660f7510, "pcmpeqw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqw, 0x660f7510, "vpcmpeqw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[49][10]},
    {INVALID,    0xf20f7510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f7510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpeqw, 0x660f7500, "vpcmpeqw", KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 50 */
  {
    {OP_pcmpeqd,   0x0f7610, "pcmpeqd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[50][2]},
    {INVALID,    0xf30f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pcmpeqd, 0x660f7610, "pcmpeqd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpcmpeqd, 0x660f7610, "vpcmpeqd", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[240][0]},
    {INVALID,    0xf20f7610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0f7610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7600, "(evex_Wb ext 240)", xx, xx, xx, xx, xx, mrm|evex, x, 240},
    {INVALID, 0xf20f7610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 51 */
  {
    {OP_movd,   0x0f7e10, "movd", Ey, xx, Pd_q, xx, xx, mrm, x, tpe[51][2]},
    /* movq zeroes the top bits when the destination is an mmx or xmm reg */
    {OP_movq, 0xf30f7e10, "movq", Vdq, xx, Wq_dq, xx, xx, mrm, x, tpe[61][2]},
    {OP_movd, 0x660f7e10, "movd", Ey, xx, Vd_dq, xx, xx, mrm, x, END_LIST},
    {INVALID, 0xf20f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0xf30f7e10, "vmovq", Vdq, xx, Wq_dq, xx, xx, mrm|vex, x, tpe[51][9]},
    {VEX_W_EXT, 0x660f7e10, "(vex_W ext 109)", xx, xx, xx, xx, xx, mrm|vex, x, 109},
    {INVALID, 0xf20f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0xf30f7e40, "vmovq", Vdq, xx, Wq_dq, xx, xx, mrm|evex, x, tpe[61][6]},
    {EVEX_Wb_EXT, 0x660f7e10, "(evex_Wb ext 137)", xx, xx, xx, xx, xx, mrm|evex, x, 137},
    {INVALID, 0xf20f7e10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 52: all assumed to have Ib */
  {
    {OP_cmpps, 0x0fc210, "cmpps", Vps, xx, Wps, Ib, Vps, mrm, x, END_LIST},
    {OP_cmpss, 0xf30fc210, "cmpss", Vss, xx, Wss, Ib, Vss, mrm, x, END_LIST},
    {OP_cmppd, 0x660fc210, "cmppd", Vpd, xx, Wpd, Ib, Vpd, mrm, x, END_LIST},
    {OP_cmpsd, 0xf20fc210, "cmpsd", Vsd, xx, Wsd, Ib, Vsd, mrm, x, END_LIST},
    {OP_vcmpps, 0x0fc210, "vcmpps", Vvs, xx, Hvs, Wvs, Ib, mrm|vex, x, tevexwb[224][0]},
    {OP_vcmpss, 0xf30fc210, "vcmpss", Vdq, xx, Hdq, Wss, Ib, mrm|vex, x, tevexwb[262][0]},
    {OP_vcmppd, 0x660fc210, "vcmppd", Vvd, xx, Hvd, Wvd, Ib, mrm|vex, x, tevexwb[224][2]},
    {OP_vcmpsd, 0xf20fc210, "vcmpsd", Vdq, xx, Hdq, Wsd, Ib, mrm|vex, x, tevexwb[262][2]},
    {EVEX_Wb_EXT, 0x0fc200, "(evex_Wb ext 224)", xx, xx, xx, xx, xx, mrm|evex, x, 224},
    {EVEX_Wb_EXT, 0xf30fc200, "(evex_Wb ext 262)", xx, xx, xx, xx, xx, mrm|evex, x, 262},
    {EVEX_Wb_EXT, 0x660fc240, "(evex_Wb ext 224)", xx, xx, xx, xx, xx, mrm|evex, x, 224},
    {EVEX_Wb_EXT, 0xf20fc240, "(evex_Wb ext 262)", xx, xx, xx, xx, xx, mrm|evex, x, 262},
  }, /* prefix extension 53: all assumed to have Ib */
  { /* note that gnu tools print immed first: pinsrw $0x0,(%esp),%xmm0 */
    /* FIXME i#1388: pinsrw actually reads only bottom word of reg */
    {OP_pinsrw,   0x0fc410, "pinsrw", Pw_q, xx, Rd_Mw, Ib, xx, mrm, x, tpe[53][2]},
    {INVALID,   0xf30fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pinsrw, 0x660fc410, "pinsrw", Vw_dq, xx, Rd_Mw, Ib, xx, mrm, x, END_LIST},
    {INVALID,   0xf20fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0x0fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpinsrw, 0x660fc410, "vpinsrw", Vdq, xx, H14_dq, Rd_Mw, Ib, mrm|vex, x, tpe[53][10]},
    {INVALID,   0xf20fc410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fc410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fc410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpinsrw, 0x660fc400, "vpinsrw", Vdq, xx, H14_dq, Rd_Mw, Ib, mrm|evex|ttt1s|inopsz2, x, END_LIST},
    {INVALID, 0xf20fc410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 54: all assumed to have Ib */
  { /* note that gnu tools print immed first: pextrw $0x7,%xmm7,%edx */
    {OP_pextrw,   0x0fc510, "pextrw", Gd, xx, Nw_q, Ib, xx, mrm, x, tpe[54][2]},
    {INVALID,   0xf30fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pextrw, 0x660fc510, "pextrw", Gd, xx, Uw_dq, Ib, xx, mrm, x, tvex[37][0]},
    {INVALID,   0xf20fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0x0fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpextrw, 0x660fc510, "vpextrw", Gd, xx, Uw_dq, Ib, xx, mrm|vex, x, tvex[37][1]},
    {INVALID,   0xf20fc510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fc510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fc510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpextrw, 0x660fc500, "vpextrw", Gd, xx, Uw_dq, Ib, xx, mrm|evex|ttnone|inopsz2, x, tvex[37][2]},
    {INVALID, 0xf20fc510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 55: all assumed to have Ib */
  {
    {OP_shufps, 0x0fc610, "shufps", Vps, xx, Wps, Ib, Vps, mrm, x, END_LIST},
    {INVALID, 0xf30fc610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_shufpd, 0x660fc610, "shufpd", Vpd, xx, Wpd, Ib, Vpd, mrm, x, END_LIST},
    {INVALID, 0xf20fc610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vshufps, 0x0fc610, "vshufps", Vvs, xx, Hvs, Wvs, Ib, mrm|vex, x, tevexwb[221][0]},
    {INVALID, 0xf30fc610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vshufpd, 0x660fc610, "vshufpd", Vvd, xx, Hvd, Wvd, Ib, mrm|vex, x, tevexwb[221][2]},
    {INVALID, 0xf20fc610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0fc600, "(evex_Wb ext 221)", xx, xx, xx, xx, xx, mrm|evex, x, 221},
    {INVALID, 0xf30fc610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fc640, "(evex_Wb ext 221)", xx, xx, xx, xx, xx, mrm|evex, x, 221},
    {INVALID, 0xf20fc610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 56 */
  {
    {OP_psrlw,   0x0fd110, "psrlw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[56][2]},
    {INVALID,  0xf30fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrlw, 0x660fd110, "psrlw", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[104][0]},
    {INVALID,  0xf20fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrlw,  0x660fd110, "vpsrlw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[104][6]},
    {INVALID,  0xf20fd110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw, 0x660fd100, "vpsrlw", Ve, xx, KEd, He, Wdq, mrm|evex|ttm128, x, tpe[104][10]},
    {INVALID, 0xf20fd110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 57 */
  {
    {OP_psrld,   0x0fd210, "psrld", Pq, xx, Qq, Pq, xx, mrm, x, tpe[57][2]},
    {INVALID,  0xf30fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrld, 0x660fd210, "psrld", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[107][0]},
    {INVALID,  0xf20fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrld, 0x660fd210, "vpsrld", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[107][6]},
    {INVALID,  0xf20fd210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fd210, "(evex_Wb ext 123)", xx, xx, xx, xx, xx, mrm|evex, x, 123},
    {INVALID, 0xf20fd210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 58 */
  {
    {OP_psrlq,   0x0fd310, "psrlq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[58][2]},
    {INVALID,  0xf30fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrlq, 0x660fd310, "psrlq", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[110][0]},
    {INVALID,  0xf20fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrlq, 0x660fd310, "vpsrlq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[110][6]},
    {INVALID,  0xf20fd310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fd310, "(evex_Wb ext 125)", xx, xx, xx, xx, xx, mrm|evex, x, 125},
    {INVALID, 0xf20fd310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 59 */
  {
    {OP_paddq,   0x0fd410, "paddq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[59][2]},
    {INVALID,  0xf30fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_paddq, 0x660fd410, "paddq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,  0xf20fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddq, 0x660fd410, "vpaddq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[225][2]},
    {INVALID,  0xf20fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fd440, "(evex_Wb ext 225)", xx, xx, xx, xx, xx, mrm|evex, x, 225},
    {INVALID, 0xf20fd410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 60 */
  {
    {OP_pmullw,   0x0fd510, "pmullw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[60][2]},
    {INVALID,   0xf30fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmullw, 0x660fd510, "pmullw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0xf30fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmullw, 0x660fd510, "vpmullw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[60][10]},
    {INVALID,   0xf20fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmullw, 0x660fd500, "vpmullw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fd510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 61 */
  {
    {INVALID,   0x0fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_movq2dq, 0xf30fd610, "movq2dq", Vdq, xx, Nq, xx, xx, mrm, x, END_LIST},
    {OP_movq, 0x660fd610, "movq", Wq_dq, xx, Vq_dq, xx, xx, mrm, x, END_LIST},
    {OP_movdq2q, 0xf20fd610, "movdq2q", Pq, xx, Uq_dq, xx, xx, mrm, x, END_LIST},
    {INVALID,   0x0fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0x660fd610, "vmovq", Wq_dq, xx, Vq_dq, xx, xx, mrm|vex, x, tpe[61][10]},
    {INVALID, 0xf20fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovq, 0x660fd640, "vmovq", Wq_dq, xx, Vq_dq, xx, xx, mrm|evex, x, tvexw[108][1]},
    {INVALID, 0xf20fd610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 62 */
  {
    {OP_pmovmskb,   0x0fd710, "pmovmskb", Gd, xx, Nq, xx, xx, mrm, x, tpe[62][2]},
    {INVALID,     0xf30fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmovmskb, 0x660fd710, "pmovmskb", Gd, xx, Udq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0xf20fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x0fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf30fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovmskb, 0x660fd710, "vpmovmskb", Gd, xx, Ux, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,     0xf20fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fd710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 63 */
  {
    {OP_psubusb,   0x0fd810, "psubusb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[63][2]},
    {INVALID,    0xf30fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubusb, 0x660fd810, "psubusb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubusb, 0x660fd810, "vpsubusb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[63][10]},
    {INVALID,    0xf20fd810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubusb, 0x660fd800, "vpsubusb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fd810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 64 */
  {
    {OP_psubusw,   0x0fd910, "psubusw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[64][2]},
    {INVALID,    0xf30fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubusw, 0x660fd910, "psubusw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubusw, 0x660fd910, "vpsubusw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[64][10]},
    {INVALID,    0xf20fd910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fd910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubusw, 0x660fd900, "vpsubusw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fd910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 65 */
  {
    {OP_pminub,   0x0fda10, "pminub", Pq, xx, Qq, Pq, xx, mrm, x, tpe[65][2]},
    {INVALID,    0xf30fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pminub, 0x660fda10, "pminub", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpminub, 0x660fda10, "vpminub", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[65][10]},
    {INVALID,    0xf20fda10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fda10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fda10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpminub, 0x660fda00, "vpminub", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fda10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 66 */
  {
    {OP_pand,   0x0fdb10, "pand", Pq, xx, Qq, Pq, xx, mrm, x, tpe[66][2]},
    {INVALID,    0xf30fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pand, 0x660fdb10, "pand", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpand, 0x660fdb10, "vpand", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fdb10, "(evex_Wb ext 41)", xx, xx, xx, xx, xx, mrm|evex, x, 41},
    {INVALID, 0xf20fdb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 67 */
  {
    {OP_paddusb,   0x0fdc10, "paddusb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[67][2]},
    {INVALID,    0xf30fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddusb, 0x660fdc10, "paddusb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddusb, 0x660fdc10, "vpaddusb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[67][10]},
    {INVALID,    0xf20fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddusb, 0x660fdc00, "vpaddusb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fdc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 68 */
  {
    {OP_paddusw,   0x0fdd10, "paddusw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[68][2]},
    {INVALID,    0xf30fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddusw, 0x660fdd10, "paddusw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddusw, 0x660fdd10, "vpaddusw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[68][10]},
    {INVALID,    0xf20fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddusw, 0x660fdd00, "vpaddusw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fdd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 69 */
  {
    {OP_pmaxub,   0x0fde10, "pmaxub", Pq, xx, Qq, Pq, xx, mrm, x, tpe[69][2]},
    {INVALID,    0xf30fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaxub, 0x660fde10, "pmaxub", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaxub, 0x660fde10, "vpmaxub", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[69][10]},
    {INVALID,    0xf20fde10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fde10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fde10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaxub, 0x660fde00, "vpmaxub", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fde10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 70 */
  {
    {OP_pandn,   0x0fdf10, "pandn", Pq, xx, Qq, Pq, xx, mrm, x, tpe[70][2]},
    {INVALID,    0xf30fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pandn, 0x660fdf10, "pandn", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpandn, 0x660fdf10, "vpandn", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fdf10, "(evex_Wb ext 42)", xx, xx, xx, xx, xx, mrm|evex, x, 42},
    {INVALID, 0xf20fdf10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 71 */
  {
    {OP_pavgb,   0x0fe010, "pavgb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[71][2]},
    {INVALID,    0xf30fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pavgb, 0x660fe010, "pavgb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpavgb, 0x660fe010, "vpavgb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[71][10]},
    {INVALID,    0xf20fe010, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpavgb, 0x660fe000, "vpavgb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 72 */
  {
    {OP_psraw,   0x0fe110, "psraw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[72][2]},
    {INVALID,    0xf30fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psraw, 0x660fe110, "psraw", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[105][0]},
    {INVALID,    0xf20fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsraw, 0x660fe110, "vpsraw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[105][6]},
    {INVALID,    0xf20fe110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw, 0x660fe100, "vpsraw", Ve, xx, KEd, He, Wdq, mrm|evex|ttm128, x, tpe[105][10]},
    {INVALID, 0xf20fe110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 73 */
  {
    {OP_psrad,   0x0fe210, "psrad", Pq, xx, Qq, Pq, xx, mrm, x, tpe[73][2]},
    {INVALID,    0xf30fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psrad, 0x660fe210, "psrad", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[108][0]},
    {INVALID,    0xf20fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsrad, 0x660fe210, "vpsrad", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[108][6]},
    {INVALID,    0xf20fe210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x60fe210, "(evex_Wb ext 121)", xx, xx, xx, xx, xx, mrm|evex, x, 121},
    {INVALID, 0xf20fe210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 74 */
  {
    {OP_pavgw,   0x0fe310, "pavgw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[74][2]},
    {INVALID,    0xf30fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pavgw, 0x660fe310, "pavgw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpavgw, 0x660fe310, "vpavgw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[74][10]},
    {INVALID,    0xf20fe310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpavgw, 0x660fe300, "vpavgw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 75 */
  {
    {OP_pmulhuw,   0x0fe410, "pmulhuw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[75][2]},
    {INVALID,    0xf30fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmulhuw, 0x660fe410, "pmulhuw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmulhuw, 0x660fe410, "vpmulhuw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[75][10]},
    {INVALID,    0xf20fe410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhuw, 0x660fe400, "vpmulhuw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 76 */
  {
    {OP_pmulhw,   0x0fe510, "pmulhw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[76][2]},
    {INVALID,    0xf30fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmulhw, 0x660fe510, "pmulhw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmulhw, 0x660fe510, "vpmulhw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[76][10]},
    {INVALID,    0xf20fe510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhw, 0x660fe500, "vpmulhw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 77 */
  {
    {INVALID, 0x0fe610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_cvtdq2pd, 0xf30fe610, "cvtdq2pd",  Vpd, xx, Wq_dq, xx, xx, mrm, x, END_LIST},
    {OP_cvttpd2dq,0x660fe610, "cvttpd2dq", Vdq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {OP_cvtpd2dq, 0xf20fe610, "cvtpd2dq",  Vdq, xx, Wpd, xx, xx, mrm, x, END_LIST},
    {INVALID,        0x0fe610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vcvtdq2pd, 0xf30fe610, "vcvtdq2pd",  Vvd, xx, Wh_e, xx, xx, mrm|vex, x, tevexwb[57][0]},
    {OP_vcvttpd2dq,0x660fe610, "vcvttpd2dq", Vx, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[222][2]},
    {OP_vcvtpd2dq, 0xf20fe610, "vcvtpd2dq",  Vx, xx, Wvd, xx, xx, mrm|vex, x, tevexwb[223][2]},
    {INVALID,   0x0fe610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30fe610, "(evex_Wb ext 57)", xx, xx, xx, xx, xx, mrm|evex, x, 57},
    {EVEX_Wb_EXT, 0x660fe650, "(evex_Wb ext 222)", xx, xx, xx, xx, xx, mrm|evex, x, 222},
    {EVEX_Wb_EXT, 0xf20fe650, "(evex_Wb ext 223)", xx, xx, xx, xx, xx, mrm|evex, x, 223},
  }, /* prefix extension 78 */
  {
    {OP_movntq,    0x0fe710, "movntq",  Mq, xx, Pq, xx, xx, mrm, x, END_LIST},
    {INVALID,    0xf30fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_movntdq, 0x660fe710, "movntdq", Mdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovntdq, 0x660fe710, "vmovntdq", Mx, xx, Vx, xx, xx, mrm|vex, x, tpe[78][10]},
    {INVALID,    0xf20fe710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30fe710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmovntdq, 0x660fe700, "vmovntdq", Me, xx, Ve, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,     0xf20fe710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 79 */
  {
    {OP_psubsb,   0x0fe810, "psubsb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[79][2]},
    {INVALID,    0xf30fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubsb, 0x660fe810, "psubsb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubsb, 0x660fe810, "vpsubsb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[79][10]},
    {INVALID,    0xf20fe810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubsb, 0x660fe800, "vpsubsb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 80 */
  {
    {OP_psubsw,   0x0fe910, "psubsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[80][2]},
    {INVALID,    0xf30fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubsw, 0x660fe910, "psubsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubsw, 0x660fe910, "vpsubsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[80][10]},
    {INVALID,    0xf20fe910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fe910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fe910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubsw, 0x660fe900, "vpsubsw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fe910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 81 */
  {
    {OP_pminsw,   0x0fea10, "pminsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[81][2]},
    {INVALID,    0xf30fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pminsw, 0x660fea10, "pminsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpminsw, 0x660fea10, "vpminsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[81][10]},
    {INVALID,    0xf20fea10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fea10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fea10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpminsw, 0x660fea00, "vpminsw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fea10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 82 */
  {
    {OP_por,   0x0feb10, "por", Pq, xx, Qq, Pq, xx, mrm, x, tpe[82][2]},
    {INVALID,    0xf30feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_por, 0x660feb10, "por", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpor, 0x660feb10, "vpor", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20feb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0feb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30feb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660feb10, "(evex_Wb ext 43)", xx, xx, xx, xx, xx, mrm|evex, x, 43},
    {INVALID, 0xf20feb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 83 */
  {
    {OP_paddsb,   0x0fec10, "paddsb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[83][2]},
    {INVALID,    0xf30fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddsb, 0x660fec10, "paddsb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddsb, 0x660fec10, "vpaddsb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[83][10]},
    {INVALID,    0xf20fec10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fec10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fec10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddsb, 0x660fec00, "vpaddsb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fec10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 84 */
  {
    {OP_paddsw,   0x0fed10, "paddsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[84][2]},
    {INVALID,    0xf30fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddsw, 0x660fed10, "paddsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddsw, 0x660fed10, "vpaddsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[84][10]},
    {INVALID,    0xf20fed10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fed10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fed10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddsw, 0x660fed00, "vpaddsw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fed10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 85 */
  {
    {OP_pmaxsw,   0x0fee10, "pmaxsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[85][2]},
    {INVALID,    0xf30fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaxsw, 0x660fee10, "pmaxsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaxsw, 0x660fee10, "vpmaxsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[85][10]},
    {INVALID,    0xf20fee10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fee10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fee10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaxsw, 0x660fee00, "vpmaxsw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20fee10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 86 */
  {
    {OP_pxor,   0x0fef10, "pxor", Pq, xx, Qq, Pq, xx, mrm, x, tpe[86][2]},
    {INVALID,    0xf30fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pxor, 0x660fef10, "pxor", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpxor, 0x660fef10, "vpxor", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,    0xf20fef10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0fef10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fef10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660fef10, "(evex_Wb ext 44)", xx, xx, xx, xx, xx, mrm|evex, x, 44},
    {INVALID, 0xf20fef10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 87 */
  {
    {OP_psllw,   0x0ff110, "psllw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[87][2]},
    {INVALID,    0xf30ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psllw, 0x660ff110, "psllw", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[106][0]},
    {INVALID,    0xf20ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsllw,  0x660ff110, "vpsllw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[106][6]},
    {INVALID,    0xf20ff110, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw, 0x660ff100, "vpsllw", Ve, xx, KEd, He, Wdq, mrm|evex|ttm128, x, tpe[106][10]},
    {INVALID, 0xf20ff110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 88 */
  {
    {OP_pslld,   0x0ff210, "pslld", Pq, xx, Qq, Pq, xx, mrm, x, tpe[88][2]},
    {INVALID,    0xf30ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pslld, 0x660ff210, "pslld", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[109][0]},
    {INVALID,    0xf20ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpslld, 0x660ff210, "vpslld", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[109][6]},
    {INVALID,    0xf20ff210, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ff200, "(evex_Wb ext 243)", xx, xx, xx, xx, xx, mrm|evex, x, 243},
    {INVALID, 0xf20ff210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 89 */
  {
    {OP_psllq,   0x0ff310, "psllq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[89][2]},
    {INVALID,    0xf30ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psllq, 0x660ff310, "psllq", Vdq, xx, Wdq, Vdq, xx, mrm, x, tpe[111][0]},
    {INVALID,    0xf20ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsllq, 0x660ff310, "vpsllq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[111][6]},
    {INVALID,    0xf20ff310, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ff340, "(evex_Wb ext 228)", xx, xx, xx, xx, xx, mrm|evex, x, 228},
    {INVALID, 0xf20ff310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 90 */
  {
    {OP_pmuludq,   0x0ff410, "pmuludq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[90][2]},
    {INVALID,    0xf30ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmuludq, 0x660ff410, "pmuludq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmuludq, 0x660ff410, "vpmuludq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[217][2]},
    {INVALID,    0xf20ff410, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ff440, "(evex_Wb ext 217)", xx, xx, xx, xx, xx, mrm|evex, x, 217},
    {INVALID, 0xf20ff410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 91 */
  {
    {OP_pmaddwd,   0x0ff510, "pmaddwd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[91][2]},
    {INVALID,    0xf30ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_pmaddwd, 0x660ff510, "pmaddwd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpmaddwd, 0x660ff510, "vpmaddwd", Vx, xx, Hx, Wx, xx, mrm|vex|ttfvm, x, tpe[91][10]},
    {INVALID,    0xf20ff510, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddwd, 0x660ff500, "vpmaddwd", Ve, xx, KEw, He, We, mrm|evex, x, END_LIST},
    {INVALID, 0xf20ff510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 92 */
  {
    {OP_psadbw,   0x0ff610, "psadbw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[92][2]},
    {INVALID,    0xf30ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psadbw, 0x660ff610, "psadbw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsadbw, 0x660ff610, "vpsadbw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[92][10]},
    {INVALID,    0xf20ff610, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsadbw, 0x660ff600, "vpsadbw", Ve, xx, He, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ff610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 93 */
  {
    {OP_maskmovq,     0x0ff710, "maskmovq", Bq, xx, Pq, Nq, xx, mrm|predcx, x, END_LIST}, /* Intel table says "Ppi, Qpi" */
    {INVALID,       0xf30ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_maskmovdqu, 0x660ff710, "maskmovdqu", Bdq, xx, Vdq, Udq, xx, mrm|predcx, x, END_LIST},
    {INVALID,       0xf20ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,         0x0ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,       0xf30ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmaskmovdqu, 0x660ff710, "vmaskmovdqu", Bdq, xx, Vdq, Udq, xx, mrm|vex|reqL0|predcx, x, END_LIST},
    {INVALID,       0xf20ff710, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660ff710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20ff710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 94 */
  {
    {OP_psubb,   0x0ff810, "psubb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[94][2]},
    {INVALID,    0xf30ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubb, 0x660ff810, "psubb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubb, 0x660ff810, "vpsubb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[94][10]},
    {INVALID,    0xf20ff810, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubb, 0x660ff800, "vpsubb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ff810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 95 */
  {
    {OP_psubw,   0x0ff910, "psubw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[95][2]},
    {INVALID,    0xf30ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubw, 0x660ff910, "psubw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubw, 0x660ff910, "vpsubw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[95][10]},
    {INVALID,    0xf20ff910, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ff910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubw, 0x660ff900, "vpsubw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ff910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 96 */
  {
    {OP_psubd,   0x0ffa10, "psubd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[96][2]},
    {INVALID,    0xf30ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_psubd, 0x660ffa10, "psubd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpsubd, 0x660ffa10, "vpsubd", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[241][0]},
    {INVALID,    0xf20ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ffa00, "(evex_Wb ext 241)", xx, xx, xx, xx, xx, mrm|evex, x, 241},
    {INVALID, 0xf20ffa10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 97 */
  {
    {OP_psubq,   0x0ffb10, "psubq", Pq, xx, Qq, Pq, xx, mrm, x, tpe[97][2]},
    {INVALID,  0xf30ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psubq, 0x660ffb10, "psubq", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,  0xf20ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x0ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,  0xf30ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubq, 0x660ffb10, "vpsubq", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[226][2]},
    {INVALID,  0xf20ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ffb40, "(evex_Wb ext 226)", xx, xx, xx, xx, xx, mrm|evex, x, 226},
    {INVALID, 0xf20ffb10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 98 */
  {
    {OP_paddb,   0x0ffc10, "paddb", Pq, xx, Qq, Pq, xx, mrm, x, tpe[98][2]},
    {INVALID,    0xf30ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddb, 0x660ffc10, "paddb", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddb, 0x660ffc10, "vpaddb", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[98][10]},
    {INVALID,    0xf20ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddb, 0x660ffc00, "vpaddb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ffc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 99 */
  {
    {OP_paddw,   0x0ffd10, "paddw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[99][2]},
    {INVALID,    0xf30ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddw, 0x660ffd10, "paddw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddw, 0x660ffd10, "vpaddw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[99][10]},
    {INVALID,    0xf20ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddw, 0x660ffd00, "vpaddw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20ffd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 100 */
  {
    {OP_paddd,   0x0ffe10, "paddd", Pq, xx, Qq, Pq, xx, mrm, x, tpe[100][2]},
    {INVALID,    0xf30ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_paddd, 0x660ffe10, "paddd", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,    0xf20ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0x0ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,    0xf30ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vpaddd, 0x660ffe10, "vpaddd", Vx, xx, Hx, Wx, xx, mrm|vex, x, tevexwb[242][0]},
    {INVALID,    0xf20ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,   0x0ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660ffe00, "(evex_Wb ext 242)", xx, xx, xx, xx, xx, mrm|evex, x, 242},
    {INVALID, 0xf20ffe10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 101: all assumed to have Ib */
  {
    {INVALID,     0x0f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrldq, 0x660f7333, "psrldq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrldq, 0x660f7333, "vpsrldq", Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[101][10]},
    {INVALID,   0xf20f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrldq, 0x660f7323, "vpsrldq", He, xx, Ib, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7333, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 102: all assumed to have Ib */
  {
    {INVALID,     0x0f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pslldq, 0x660f7337, "pslldq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslldq, 0x660f7337, "vpslldq", Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[102][10]},
    {INVALID,   0xf20f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslldq, 0x660f7327, "vpslldq", He, xx, Ib, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7337, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 103 */
  {
    {REX_B_EXT,  0x900000, "(rex.b ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {OP_pause,0xf3900000, "pause", xx, xx, xx, xx, xx, no, x, END_LIST},
    {REX_B_EXT, 0x900000, "(rex.b ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {REX_B_EXT, 0xf2900000, "(rex.b ext 0)", xx, xx, xx, xx, xx, no, x, 0},
    {INVALID,     0x900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf3900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x66900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf2900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2900000, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 104: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrlw,    0x0f7132, "psrlw", Nq, xx, Ib, Nq, xx, mrm, x, tpe[104][2]},
    {INVALID,   0xf30f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrlw,  0x660f7132, "psrlw", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw,  0x660f7132, "vpsrlw", Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[56][10]},
    {INVALID,   0xf20f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlw, 0x660f7122, "vpsrlw", He, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7132, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 105: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psraw,    0x0f7134, "psraw", Nq, xx, Ib, Nq, xx, mrm, x, tpe[105][2]},
    {INVALID,   0xf30f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psraw,  0x660f7134, "psraw", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw,  0x660f7134, "vpsraw", Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[72][10]},
    {INVALID,   0xf20f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsraw, 0x660f7124, "vpsraw", He, xx, KEw, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7134, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 106: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psllw,    0x0f7136, "psllw", Nq, xx, Ib, Nq, xx, mrm, x, tpe[106][2]},
    {INVALID,   0xf30f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psllw,  0x660f7136, "psllw", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw,  0x660f7136, "vpsllw", Hx, xx, Ib, Ux, xx, mrm|vex, x, tpe[87][10]},
    {INVALID,   0xf20f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllw,  0x660f7126, "vpsllw", He, xx, KEd, Ib, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf20f7136, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 107: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrld,    0x0f7232, "psrld", Nq, xx, Ib, Nq, xx, mrm, x, tpe[107][2]},
    {INVALID,   0xf30f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrld,  0x660f7232, "psrld", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrld,  0x660f7232, "vpsrld", Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[123][0]},
    {INVALID,   0xf20f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7232, "(evex_Wb ext 124)", xx, xx, xx, xx, xx, mrm|evex, x, 124},
    {INVALID, 0xf20f7232, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 108: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrad,    0x0f7234, "psrad", Nq, xx, Ib, Nq, xx, mrm, x, tpe[108][2]},
    {INVALID,   0xf30f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrad,  0x660f7234, "psrad", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrad,  0x660f7234, "vpsrad", Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[121][0]},
    {INVALID,   0xf20f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7234, "(evex_Wb ext 122)", xx, xx, xx, xx, xx, mrm|evex, x, 122},
    {INVALID, 0xf20f7234, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 109: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_pslld,    0x0f7236, "pslld", Nq, xx, Ib, Nq, xx, mrm, x, tpe[109][2]},
    {INVALID,   0xf30f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_pslld,  0x660f7236, "pslld", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpslld,  0x660f7236, "vpslld", Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[243][0]},
    {INVALID,   0xf20f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f7226, "(evex_Wb ext 244)", xx, xx, xx, xx, xx, mrm|evex, x, 244},
    {INVALID, 0xf20f7236, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 110: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psrlq,    0x0f7332, "psrlq", Nq, xx, Ib, Nq, xx, mrm, x, tpe[110][2]},
    {INVALID,   0xf30f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psrlq,  0x660f7332, "psrlq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsrlq,  0x660f7332, "vpsrlq", Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[125][2]},
    {INVALID,   0xf20f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf20f7332, "(evex_Wb ext 126)", xx, xx, xx, xx, xx, mrm|evex, x, 126},
    {INVALID, 0xf20f7332, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 111: all assumed to have Ib */
  {
    /* Intel tables imply they may add opcodes in the mod<3 (mem) space in future */
    {OP_psllq,    0x0f7336, "psllq", Nq, xx, Ib, Nq, xx, mrm, x, tpe[111][2]},
    {INVALID,   0xf30f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_psllq,  0x660f7336, "psllq", Udq, xx, Ib, Udq, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq,  0x660f7336, "vpsllq", Hx, xx, Ib, Ux, xx, mrm|vex, x, tevexwb[228][2]},
    {INVALID,   0xf20f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x660f7366, "(evex_Wb ext 229)", xx, xx, xx, xx, xx, mrm|evex, x, 229},
    {INVALID, 0xf20f7336, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 112 */
  {
    {OP_movq,     0x0f6f10, "movq", Pq, xx, Qq, xx, xx, mrm, x, tpe[113][0]},
    {OP_movdqu, 0xf30f6f10, "movdqu", Vdq, xx, Wdq, xx, xx, mrm, x, tpe[113][1]},
    {OP_movdqa, 0x660f6f10, "movdqa", Vdq, xx, Wdq, xx, xx, mrm, x, tpe[113][2]},
    {INVALID,   0xf20f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovdqu, 0xf30f6f10, "vmovdqu", Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[113][5]},
    {OP_vmovdqa, 0x660f6f10, "vmovdqa", Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[113][6]},
    {INVALID,   0xf20f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f6f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f6f10, "(evex_Wb ext 11)", xx, xx, xx, xx, xx, mrm|evex, x, 11},
    {EVEX_Wb_EXT, 0x660f6f10, "(evex_Wb ext 8)", xx, xx, xx, xx, xx, mrm|evex, x, 8},
    {EVEX_Wb_EXT, 0xf20f6f10, "(evex_Wb ext 10)", xx, xx, xx, xx, xx, mrm|evex, x, 10},
  }, /* prefix extension 113 */
  {
    {OP_movq,     0x0f7f10, "movq", Qq, xx, Pq, xx, xx, mrm, x, tpe[51][1]},
    {OP_movdqu, 0xf30f7f10, "movdqu", Wdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {OP_movdqa, 0x660f7f10, "movdqa", Wdq, xx, Vdq, xx, xx, mrm, x, END_LIST},
    {INVALID,   0xf20f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x0f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {OP_vmovdqu, 0xf30f7f10, "vmovdqu", Wx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {OP_vmovdqa, 0x660f7f10, "vmovdqa", Wx, xx, Vx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,   0xf20f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f7f10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf30f7f10, "(evex_Wb ext 13)", xx, xx, xx, xx, xx, mrm|evex, x, 13},
    {EVEX_Wb_EXT, 0x660f7f10, "(evex_Wb ext 9)", xx, xx, xx, xx, xx, mrm|evex, x, 9},
    {EVEX_Wb_EXT, 0xf20f7f10, "(evex_Wb ext 12)", xx, xx, xx, xx, xx, mrm|evex, x, 12},
  }, /* prefix extension 114 */
  {
    {INVALID,     0x0f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_haddpd, 0x660f7c10, "haddpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_haddps, 0xf20f7c10, "haddps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vhaddpd, 0x660f7c10, "vhaddpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vhaddps, 0xf20f7c10, "vhaddps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f7c10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 115 */
  {
    {INVALID,     0x0f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_hsubpd, 0x660f7d10, "hsubpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_hsubps, 0xf20f7d10, "hsubps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vhsubpd, 0x660f7d10, "vhsubpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vhsubps, 0xf20f7d10, "vhsubps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f7d10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 116 */
  {
    {INVALID,     0x0fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_addsubpd, 0x660fd010, "addsubpd", Vpd, xx, Wpd, Vpd, xx, mrm, x, END_LIST},
    {OP_addsubps, 0xf20fd010, "addsubps", Vps, xx, Wps, Vps, xx, mrm, x, END_LIST},
    {INVALID,     0x0fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vaddsubpd, 0x660fd010, "vaddsubpd", Vvd, xx, Hvd, Wvd, xx, mrm|vex, x, END_LIST},
    {OP_vaddsubps, 0xf20fd010, "vaddsubps", Vvs, xx, Hvs, Wvs, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fd010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /* prefix extension 117 */
  {
    {INVALID,     0x0ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x660ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_lddqu,  0xf20ff010, "lddqu", Vdq, xx, Mdq, xx, xx, mrm, x, END_LIST},
    {INVALID,     0x0ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0xf30ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x660ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vlddqu,  0xf20ff010, "vlddqu", Vx, xx, Mx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x0ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20ff010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, /***************************************************
   * SSSE3
   */
  { /* prefix extension 118 */
    {OP_pshufb,     0x380018, "pshufb",   Pq, xx, Qq, Pq, xx, mrm, x, tpe[118][2]},
    {INVALID,     0xf3380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_pshufb,   0x66380018, "pshufb",   Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,     0xf2380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x380018, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,     0xf3380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufb,   0x66380018, "vpshufb",   Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[118][10]},
    {INVALID,     0xf2380018, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpshufb,   0x66380008, "vpshufb",   Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2380018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 119 */
    {OP_phaddw,      0x380118, "phaddw",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[119][2]},
    {INVALID,      0xf3380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddw,    0x66380118, "phaddw",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380118, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddw,    0x66380118, "vphaddw",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 120 */
    {OP_phaddd,      0x380218, "phaddd",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[120][2]},
    {INVALID,      0xf3380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddd,    0x66380218, "phaddd",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380218, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddd,    0x66380218, "vphaddd",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380218, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380218, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380218, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380218, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 121 */
    {OP_phaddsw,     0x380318, "phaddsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[121][2]},
    {INVALID,      0xf3380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phaddsw,   0x66380318, "phaddsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380318, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphaddsw,   0x66380318, "vphaddsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 122 */
    {OP_pmaddubsw,   0x380418, "pmaddubsw",Pq, xx, Qq, Pq, xx, mrm, x, tpe[122][2]},
    {INVALID,      0xf3380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmaddubsw, 0x66380418, "pmaddubsw",Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380418, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddubsw, 0x66380418, "vpmaddubsw",Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[122][10]},
    {INVALID,      0xf2380418, "(bad)",    xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmaddubsw, 0x66380408, "vpmaddubsw",Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2380418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 123 */
    {OP_phsubw,      0x380518, "phsubw",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[123][2]},
    {INVALID,      0xf3380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubw,    0x66380518, "phsubw",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380518, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubw,    0x66380518, "vphsubw",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 124 */
    {OP_phsubd,      0x380618, "phsubd",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[124][2]},
    {INVALID,      0xf3380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubd,    0x66380618, "phsubd",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380618, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubd,    0x66380618, "vphsubd",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 125 */
    {OP_phsubsw,     0x380718, "phsubsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[125][2]},
    {INVALID,      0xf3380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_phsubsw,   0x66380718, "phsubsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380718, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vphsubsw,   0x66380718, "vphsubsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 126 */
    {OP_psignb,      0x380818, "psignb",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[126][2]},
    {INVALID,      0xf3380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignb,    0x66380818, "psignb",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380818, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignb,    0x66380818, "vpsignb",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 127 */
    {OP_psignw,      0x380918, "psignw",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[127][2]},
    {INVALID,      0xf3380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignw,    0x66380918, "psignw",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380918, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignw,    0x66380918, "vpsignw",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 128 */
    {OP_psignd,      0x380a18, "psignd",  Pq, xx, Qq, Pq, xx, mrm, x, tpe[128][2]},
    {INVALID,      0xf3380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_psignd,    0x66380a18, "psignd",  Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380a18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsignd,    0x66380a18, "vpsignd",  Vx, xx, Hx, Wx, xx, mrm|vex, x, END_LIST},
    {INVALID,      0xf2380a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x66380a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf2380a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 129 */
    {OP_pmulhrsw,    0x380b18, "pmulhrsw", Pq, xx, Qq, Pq, xx, mrm, x, tpe[129][2]},
    {INVALID,      0xf3380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pmulhrsw,  0x66380b18, "pmulhrsw", Vdq, xx, Wdq, Vdq, xx, mrm, x, END_LIST},
    {INVALID,      0xf2380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x380b18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhrsw,  0x66380b18, "vpmulhrsw", Vx, xx, Hx, Wx, xx, mrm|vex, x, tpe[129][10]},
    {INVALID,      0xf2380b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x380b18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3380b18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmulhrsw,  0x66380b08, "vpmulhrsw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2380b18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 130 */
    {OP_pabsb,       0x381c18, "pabsb",   Pq, xx, Qq, xx, xx, mrm, x, tpe[130][2]},
    {INVALID,      0xf3381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsb,     0x66381c18, "pabsb",   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381c18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsb,     0x66381c18, "vpabsb",   Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[130][10]},
    {INVALID,      0xf2381c18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x381c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3381c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsb, 0x66381c08, "vpabsb",   Ve, xx, KEq, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2381c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 131 */
    {OP_pabsw,       0x381d18, "pabsw",   Pq, xx, Qq, xx, xx, mrm, x, tpe[131][2]},
    {INVALID,      0xf3381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsw,     0x66381d18, "pabsw",   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381d18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsw,     0x66381d18, "vpabsw",   Vx, xx, Wx, xx, xx, mrm|vex, x, tpe[131][10]},
    {INVALID,      0xf2381d18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x381d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3381d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsw, 0x66381d08, "vpabsw",   Ve, xx, KEd, We, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf2381d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 132 */
    {OP_pabsd,       0x381e18, "pabsd",   Pq, xx, Qq, xx, xx, mrm, x, tpe[132][2]},
    {INVALID,      0xf3381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pabsd,     0x66381e18, "pabsd",   Vdq, xx, Wdq, xx, xx, mrm, x, END_LIST},
    {INVALID,      0xf2381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381e18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf3381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpabsd,     0x66381e18, "vpabsd",   Vx, xx, Wx, xx, xx, mrm|vex, x, tevexwb[146][0]},
    {INVALID,      0xf2381e18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x381e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf3381e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x66381e18, "(evex_Wb ext 146)", xx, xx, xx, xx, xx, mrm|evex, x, 146},
    {INVALID, 0xf2381e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 133: all assumed to have Ib */
    {OP_palignr,     0x3a0f18, "palignr", Pq, xx, Qq, Ib, Pq, mrm, x, tpe[133][2]},
    {INVALID,      0xf33a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_palignr,   0x663a0f18, "palignr", Vdq, xx, Wdq, Ib, Vdq, mrm, x, END_LIST},
    {INVALID,      0xf23a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x3a0f18, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {INVALID,      0xf33a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpalignr,   0x663a0f18, "vpalignr", Vx, xx, Hx, Wx, Ib, mrm|vex, x, tpe[133][10]},
    {INVALID,      0xf23a0f18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x3a0f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf33a0f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpalignr, 0x663a0f08, "vpalignr", Ve, xx, KEq, Ib, He, xop|mrm|evex|ttfvm, x, exop[248]},
    {INVALID, 0xf23a0f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 134 */
    {OP_vmread,      0x0f7810, "vmread",  Ey, xx, Gy, xx, xx, mrm|o64, x, END_LIST},
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
    {EVEX_Wb_EXT, 0x0f7810, "(evex_Wb ext 49)", xx, xx, xx, xx, xx, mrm|evex, x, 49},
    {EVEX_Wb_EXT, 0xf30f7810, "(evex_Wb ext 54)", xx, xx, xx, xx, xx, mrm|evex, x, 54},
    {EVEX_Wb_EXT, 0x660f7810, "(evex_Wb ext 51)", xx, xx, xx, xx, xx, mrm|evex, x, 51},
    {EVEX_Wb_EXT, 0xf20f7810, "(evex_Wb ext 55)", xx, xx, xx, xx, xx, mrm|evex, x, 55},
  }, { /* prefix extension 135 */
    {OP_vmwrite,     0x0f7910, "vmwrite", Gy, xx, Ey, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf30f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* FIXME: is src or dst Udq? */
    {OP_extrq,     0x660f7910, "extrq",   Vdq, xx, Udq, xx, xx, mrm, x, END_LIST},
    {OP_insertq,   0xf20f7910, "insertq", Vdq, xx, Udq, xx, xx, mrm, x, END_LIST},
    {INVALID,        0x0f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x0f7910, "(evex_Wb ext 47)", xx, xx, xx, xx, xx, mrm|evex, x, 47},
    {EVEX_Wb_EXT, 0xf30f7910, "(evex_Wb ext 52)", xx, xx, xx, xx, xx, mrm|evex, x, 52},
    {EVEX_Wb_EXT, 0x660f7910, "(evex_Wb ext 48)", xx, xx, xx, xx, xx, mrm|evex, x, 48},
    {EVEX_Wb_EXT, 0xf20f7910, "(evex_Wb ext 53)", xx, xx, xx, xx, xx, mrm|evex, x, 53},
  }, { /* prefix extension 136 */
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
    {INVALID,   0x0fbd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fbd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fbd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fbd10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 137 */
    {OP_vmptrld,     0x0fc736, "vmptrld", xx, xx, Mq, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmxon,     0xf30fc736, "vmxon",   xx, xx, Mq, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmclear,   0x660fc736, "vmclear", Mq, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,      0xf20fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20fc736, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0fc736, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fc736, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fc736, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fc736, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 138 */
    {OP_movbe,   0x38f018, "movbe", Gv, xx, Mv, xx, xx, mrm, x, tpe[139][0]},
    {INVALID,  0xf338f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* really this is regular data-size prefix */
    {OP_movbe, 0x6638f018, "movbe", Gw, xx, Mw, xx, xx, mrm, x, tpe[139][2]},
    {OP_crc32, 0xf238f018, "crc32", Gy, xx, Eb, Gy, xx, mrm, x, END_LIST},
    {INVALID,    0x38f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf338f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0x6638f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf238f018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x38f018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 139 */
    {OP_movbe,   0x38f118, "movbe", Mv, xx, Gv, xx, xx, mrm, x, tpe[138][2]},
    {INVALID,  0xf338f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* really this is regular data-size prefix */
    {OP_movbe, 0x6638f118, "movbe", Mw, xx, Gw, xx, xx, mrm, x, END_LIST},
    /* The Intel table separates out a data-size prefix into Ey and Ew: ours are combined,
     * and thus we want Ev.
     */
    {OP_crc32, 0xf238f118, "crc32", Gy, xx, Ev, Gy, xx, mrm, x, tpe[138][3]},
    {INVALID,    0x38f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf338f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0x6638f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,  0xf238f118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x38f118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 140 */
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
    {INVALID,   0x0fbc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30fbc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660fbc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20fbc10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 141 */
    {INVALID,        0x38f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_bextr,       0x38f718, "bextr",   Gy, xx, Ey, By, xx, mrm|vex, fW6, txop[60]},
    {OP_sarx,      0xf338f718, "sarx",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_shlx,      0x6638f718, "shlx",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {OP_shrx,      0xf238f718, "shrx",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x38f718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 142 */
    {INVALID,        0x38f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf238f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_bzhi,        0x38f518, "bzhi",    Gy, xx, Ey, By, xx, mrm|vex, fW6, END_LIST},
    {OP_pext,      0xf338f518, "pext",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {INVALID,      0x6638f518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_pdep,      0xf238f518, "pdep",    Gy, xx, Ey, By, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x38f518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 143 */
    {INVALID,        0x38f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_adox,      0xf338f618, "adox",    Gy, xx, Ey, Gy, xx, mrm, (fWO|fRO), END_LIST},
    {OP_adcx,      0x6638f618, "adcx",    Gy, xx, Ey, Gy, xx, mrm, (fWC|fRC), END_LIST},
    {INVALID,      0xf238f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x38f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf338f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x6638f618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_mulx,      0xf238f618, "mulx",    By, Gy, Ey, uDX, xx, mrm|vex, x, END_LIST},
    {INVALID,   0x38f618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf338f618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x6638f618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf238f618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 144 */
    {INVALID,        0x0f9010, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9010, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9010, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9010, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9010, "(vex_W ext 74)", xx, xx, xx, xx, xx, mrm|vex, x, 74},
    {INVALID,      0xf30f9010, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9010, "(vex_W ext 75)", xx, xx, xx, xx, xx, mrm|vex, x, 75},
    {INVALID,      0xf20f9010, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 145 */
    {INVALID,        0x0f9110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9110, "(vex_W ext 76)", xx, xx, xx, xx, xx, mrm|vex, x, 76},
    {INVALID,      0xf30f9110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9110, "(vex_W ext 77)", xx, xx, xx, xx, xx, mrm|vex, x, 77},
    {INVALID,      0xf20f9110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 146 */
    {INVALID,        0x0f9210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9210, "(vex_W ext 78)", xx, xx, xx, xx, xx, mrm|vex, x, 78},
    {INVALID,      0xf30f9210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9210, "(vex_W ext 79)", xx, xx, xx, xx, xx, mrm|vex, x, 79},
    {VEX_W_EXT,    0xf20f9210, "(vex_W ext 106)",xx, xx, xx, xx, xx, mrm|vex, x, 106},
    {INVALID,   0x0f9210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 147 */
    {INVALID,        0x0f9310, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9310, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9310, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9310, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9310, "(vex_W ext 80)", xx, xx, xx, xx, xx, mrm|vex, x, 80},
    {INVALID,      0xf30f9310, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9310, "(vex_W ext 81)", xx, xx, xx, xx, xx, mrm|vex, x, 81},
    {VEX_W_EXT,    0xf20f9310, "(vex_W ext 107)",xx, xx, xx, xx, xx, mrm|vex, x, 107},
    {INVALID,   0x0f9310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 148 */
    {INVALID,        0x0f4110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4110, "(vex_W ext 82)", xx, xx, xx, xx, xx, mrm|vex, x, 82},
    {INVALID,      0xf30f4110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4110, "(vex_W ext 83)", xx, xx, xx, xx, xx, mrm|vex, x, 83},
    {INVALID,      0xf20f4110, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 149 */
    {INVALID,        0x0f4210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4210, "(vex_W ext 84)", xx, xx, xx, xx, xx, mrm|vex, x, 84},
    {INVALID,      0xf30f4210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4210, "(vex_W ext 85)", xx, xx, xx, xx, xx, mrm|vex, x, 85},
    {INVALID,      0xf20f4210, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 150 */
    {INVALID,        0x0f4b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4b10, "(vex_W ext 86)", xx, xx, xx, xx, xx, mrm|vex, x, 86},
    {INVALID,      0xf30f4b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4b10, "(vex_W ext 87)", xx, xx, xx, xx, xx, mrm|vex, x, 87},
    {INVALID,      0xf20f4b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 151 */
    {INVALID,        0x0f4410, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4410, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4410, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4410, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4410, "(vex_W ext 88)", xx, xx, xx, xx, xx, mrm|vex, x, 88},
    {INVALID,      0xf30f4410, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4410, "(vex_W ext 89)", xx, xx, xx, xx, xx, mrm|vex, x, 89},
    {INVALID,      0xf20f4410, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 152 */
    {INVALID,        0x0f4510, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4510, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4510, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4510, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4510, "(vex_W ext 90)", xx, xx, xx, xx, xx, mrm|vex, x, 90},
    {INVALID,      0xf30f4510, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4510, "(vex_W ext 91)", xx, xx, xx, xx, xx, mrm|vex, x, 91},
    {INVALID,      0xf20f4510, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 153 */
    {INVALID,        0x0f4610, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4610, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4610, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4610, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4610, "(vex_W ext 92)", xx, xx, xx, xx, xx, mrm|vex, x, 92},
    {INVALID,      0xf30f4610, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4610, "(vex_W ext 93)", xx, xx, xx, xx, xx, mrm|vex, x, 93},
    {INVALID,      0xf20f4610, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 154 */
    {INVALID,        0x0f4710, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4710, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4710, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4710, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4710, "(vex_W ext 94)", xx, xx, xx, xx, xx, mrm|vex, x, 94},
    {INVALID,      0xf30f4710, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4710, "(vex_W ext 95)", xx, xx, xx, xx, xx, mrm|vex, x, 95},
    {INVALID,      0xf20f4710, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 155 */
    {INVALID,        0x0f4a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f4a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f4a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f4a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f4a10, "(vex_W ext 96)", xx, xx, xx, xx, xx, mrm|vex, x, 96},
    {INVALID,      0xf30f4a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f4a10, "(vex_W ext 97)", xx, xx, xx, xx, xx, mrm|vex, x, 97},
    {INVALID,      0xf20f4a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f4a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f4a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f4a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f4a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 156 */
    {INVALID,        0x0f9810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9810, "(vex_W ext 98)", xx, xx, xx, xx, xx, mrm|vex, x, 98},
    {INVALID,      0xf30f9810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9810, "(vex_W ext 99)", xx, xx, xx, xx, xx, mrm|vex, x, 99},
    {INVALID,      0xf20f9810, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 157 */
    {INVALID,        0x0f9910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f9910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f9910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f9910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,      0x0f9910, "(vex_W ext 104)", xx, xx, xx, xx, xx, mrm|vex, x, 104},
    {INVALID,      0xf30f9910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x660f9910, "(vex_W ext 105)", xx, xx, xx, xx, xx, mrm|vex, x, 105},
    {INVALID,      0xf20f9910, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f9910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf30f9910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0x660f9910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0xf20f9910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 158 */
    {INVALID,        0x0f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7b10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0xf30f7b10, "(evex_Wb ext 58)", xx, xx, xx, xx, xx, mrm|evex, x, 58},
    {EVEX_Wb_EXT,   0x660f7b10, "(evex_Wb ext 46)", xx, xx, xx, xx, xx, mrm|evex, x, 46},
    {EVEX_Wb_EXT,   0xf20f7b10, "(evex_Wb ext 59)", xx, xx, xx, xx, xx, mrm|evex, x, 59},
  }, { /* prefix extension 159 */
    {INVALID,        0x0f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf30f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x660f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf20f7a10, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x0f7a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,   0xf30f7a10, "(evex_Wb ext 61)", xx, xx, xx, xx, xx, mrm|evex, x, 61},
    {EVEX_Wb_EXT,   0x660f7a10, "(evex_Wb ext 50)", xx, xx, xx, xx, xx, mrm|evex, x, 50},
    {EVEX_Wb_EXT,   0xf20f7a10, "(evex_Wb ext 60)", xx, xx, xx, xx, xx, mrm|evex, x, 60},
  }, { /* prefix extension 160 */
    {INVALID,        0x383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovqb,   0xf3383208, "vpmovqb", Wj_e, xx, KEb, Ve, xx, mrm|evex|ttovm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovzxbq, 0x66383208, "vpmovzxbq", Ve, xx, KEb, Wj_e, xx, mrm|evex|ttovm, x, END_LIST},
    {INVALID,      0xf2383218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 161 */
    {INVALID,        0x382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovsqb,  0xf3382208, "vpmovsqb", Wj_e, xx, KEb, Ve, xx, mrm|evex|ttovm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsxbq, 0x66382208, "vpmovsxbq", Ve, xx, KEb, Wj_e, xx, mrm|evex|ttovm, x, END_LIST},
    {INVALID,      0xf2382218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 162 */
    {INVALID,        0x381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovusqb, 0xf3381208, "vpmovusqb", Wj_e, xx, KEb, Ve, xx, mrm|evex|ttovm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381218, "(evex_Wb ext 130)", xx, xx, xx, xx, xx, mrm|evex, x, 130},
    {INVALID,      0xf2381218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 163 */
    {INVALID,        0x383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovqw,   0xf3383408, "vpmovqw", Wi_e, xx, KEb, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovzxwq, 0x66383408, "vpmovzxwq", Ve, xx, KEb, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2383418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 164 */
    {INVALID,        0x382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovsqw,  0xf3382408, "vpmovsqw", Wi_e, xx, KEb, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsxwq, 0x66382408, "vpmovsxwq", Ve, xx, KEb, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2382418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 165 */
    {INVALID,        0x381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmovusqw, 0xf3381408, "vpmovusqw", Wi_e, xx, KEb, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381418, "(evex_Wb ext 119)", xx, xx, xx, xx, xx, mrm|evex, x, 119},
    {INVALID,      0xf2381418, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 166 */
    {INVALID,        0x383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovqd,   0xf3383508, "vpmovqd", Wh_e, xx, KEb, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovzxdq, 0x66383508, "vpmovzxdq", Ve, xx, KEb, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2383518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 167 */
    {INVALID,        0x382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsqd,  0xf3382508, "vpmovsqd", Wh_e, xx, KEb, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovsxdq, 0x66382508, "vpmovsxdq", Ve, xx, KEb, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2382518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 168 */
    {INVALID,        0x381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovusqd, 0xf3381508, "vpmovusqd", Wh_e, xx, KEb, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381518, "(evex_Wb ext 117)", xx, xx, xx, xx, xx, mrm|evex, x, 117},
    {INVALID,      0xf2381518, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 169 */
    {INVALID,        0x383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovdb,   0xf3383108, "vpmovdb", Wi_e, xx, KEw, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {OP_vpmovzxbd, 0x66383108, "vpmovzxbd", Ve, xx, KEw, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2383118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 170 */
    {INVALID,        0x382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsdb,  0xf3382108, "vpmovsdb", Wi_e, xx, KEw, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {OP_vpmovsxbd, 0x66382108, "vpmovsxbd", Ve, xx, KEw, Wi_e, xx, mrm|evex|ttqvm, x, END_LIST},
    {INVALID,      0xf2382118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 171 */
    {INVALID,        0x381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovusdb, 0xf3381108, "vpmovusdb", Wi_e, xx, KEw, Ve, xx, mrm|evex|ttqvm, x, END_LIST},
    {EVEX_Wb_EXT,   0x66381118, "(evex_Wb ext 127)", xx, xx, xx, xx, xx, mrm|evex, x, 127},
    {INVALID,      0xf2381118, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 172 */
    {INVALID,        0x383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovdw,   0xf3383308, "vpmovdw", Wh_e, xx, KEw, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovzxwd, 0x66383308, "vpmovzxwd", Ve, xx, KEw, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2383318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 173 */
    {INVALID,        0x382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovsdw,  0xf3382308, "vpmovsdw", Wh_e, xx, KEw, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovsxwd, 0x66382308, "vpmovsxwd", Ve, xx, KEw, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2382318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 174 */
    {INVALID,        0x381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovusdw, 0xf3381308, "vpmovusdw", Wh_e, xx, KEw, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {EVEX_Wb_EXT,  0x66381308, "(evex_Wb ext 263)", xx, xx, xx, xx, xx, mrm|evex, x, 263},
    {INVALID,      0xf2381318, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 175 */
    {INVALID,        0x383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovwb,   0xf3383008, "vpmovwb", Wh_e, xx, KEd, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovzxbw, 0x66383008, "vpmovzxbw", Ve, xx, KEd, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2383018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 176 */
    {INVALID,        0x382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovswb,  0xf3382008, "vpmovswb", Wh_e, xx, KEd, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpmovsxbw, 0x66382008, "vpmovsxbw", Ve, xx, KEd, Wh_e, xx, mrm|evex|tthvm, x, END_LIST},
    {INVALID,      0xf2382018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 177 */
    {INVALID,        0x381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf3381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x66381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0xf2381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,        0x381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vpmovuswb, 0xf3381008, "vpmovuswb", Wh_e, xx, KEd, Ve, xx, mrm|evex|tthvm, x, END_LIST},
    {OP_vpsrlvw,   0x66381048, "vpsrlvw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,      0xf2381018, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 178 */
    {INVALID,       0x382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,  0xf3382818, "(evex_Wb ext 138)", xx, xx, xx, xx, xx, mrm|evex, x, 138},
    {EVEX_Wb_EXT,  0x66382848, "(evex_Wb ext 227)", xx, xx, xx, xx, xx, mrm|evex, x, 227},
    {INVALID,     0xf2382818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 179 */
    {INVALID,      0x383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3383818, "(evex_Wb ext 139)", xx, xx, xx, xx, xx, mrm|evex, x, 139},
    {OP_vpminsb, 0x66383808, "vpminsb", Ve, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,    0xf2383818, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 180 */
    {INVALID,       0x382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf3382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0x66382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,     0xf2382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,       0x382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,  0xf3382918, "(evex_Wb ext 140)", xx, xx, xx, xx, xx, mrm|evex, x, 140},
    {EVEX_Wb_EXT,  0x66382948, "(evex_Wb ext 233)", xx, xx, xx, xx, xx, mrm|evex, x, 233},
    {INVALID,     0xf2382918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 181 */
    {INVALID,      0x383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3383918, "(evex_Wb ext 141)", xx, xx, xx, xx, xx, mrm|evex, x, 141},
    {EVEX_Wb_EXT, 0x66383918, "(evex_Wb ext 113)", xx, xx, xx, xx, xx, mrm|evex, x, 113},
    {INVALID,    0xf2383918, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 182 */
    {INVALID,      0x382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3382618, "(evex_Wb ext 171)", xx, xx, xx, xx, xx, mrm, x, 171},
    {EVEX_Wb_EXT, 0x66382618, "(evex_Wb ext 169)", xx, xx, xx, xx, xx, mrm, x, 169},
    {INVALID,    0xf2382618, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 183 */
    {INVALID,      0x382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0xf3382718, "(evex_Wb ext 172)", xx, xx, xx, xx, xx, mrm, x, 172},
    {EVEX_Wb_EXT, 0x66382718, "(evex_Wb ext 170)", xx, xx, xx, xx, xx, mrm, x, 170},
    {INVALID,    0xf2382718, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 184 */
    {INVALID,      0x382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastmb2q, 0xf3382a48, "vpbroadcastmb2q", Ve, xx, KQb, xx, xx, mrm|evex|ttnone, x, NA},
    {OP_vmovntdqa, 0x66382a08, "vmovntdqa", Me, xx, Ve, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,    0xf2382a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 185 */
    {INVALID,      0x383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastmw2d, 0xf3383a08, "vpbroadcastmw2d", Ve, xx, KQw, xx, xx, mrm|evex|ttnone, x, NA},
    {OP_vpminuw, 0x66383a08, "vpminuw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID,    0xf2383a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 186 */
    {OP_bndldx,    0x0f1a10, "bndldx", TRqdq, xx, Mm, xx, xx, mrm, x, END_LIST},
    {OP_bndcl,   0xf30f1a10, "bndcl", TRqdq, xx, Er, xx, xx, mrm, x, END_LIST},
    {OP_bndmov,  0x660f1a10, "bndmov", TRqdq, xx, TMqdq, xx, xx, mrm, x, tpe[187][2]},
    {OP_bndcu,   0xf20f1a10, "bndcu", TRqdq, xx, Er, xx, xx, mrm, x, END_LIST},
    {INVALID,      0x0f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1a18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 187 */
    {OP_bndstx,    0x0f1b10, "bndstx", Mm, xx, TRqdq, xx, xx, mrm, x, END_LIST},
    {OP_bndmk,   0xf30f1b10, "bndmk", TRqdq, xx, Mr, xx, xx, mrm, x, END_LIST},
    {OP_bndmov,  0x660f1b10, "bndmov", TMqdq, xx, TRqdq, xx, xx, mrm, x, END_LIST},
    {OP_bndcn,   0xf20f1b10, "bndcn", TRqdq, xx, Er, xx, xx, mrm, x, END_LIST},
    {INVALID,      0x0f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20f1b18, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 188 */
    {REX_W_EXT,    0x0fae34, "(rex.w ext 2)", xx, xx, xx, xx, xx, mrm, x, 2},
    {OP_ptwrite, 0xf30fae34, "ptwrite",   xx, xx, Ey, xx, xx, mrm, x, END_LIST},
    {INVALID,    0x660fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x0fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf30fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x660fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf20fae34, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  }, { /* prefix extension 189 */
    {INVALID,      0x385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* vex */
    {INVALID,      0x385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,  0x66385218, "(vex_W ext 112)",   xx, xx, xx, xx, xx, no, x, 112},
    {INVALID,    0xf2385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    /* evex */
    {INVALID,      0x385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,0xf3385218, "(evex_Wb ext 273)",   xx, xx, xx, xx, xx, no, x, 273},
    {EVEX_Wb_EXT,0x66385218, "(evex_Wb ext 269)",   xx, xx, xx, xx, xx, no, x, 269},
    {INVALID,    0xf2385218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
  } , { /* prefix extension 190 */
    {INVALID,      0x387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf3387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0x66387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,    0xf2387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,      0x387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,0xf3387208, "(evex_Wb ext 272)", xx, xx, xx, xx, xx, mrm|evex|ttnone, x, 272},
    {INVALID,    0x66387218, "(bad)",   xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT,0xf2387218, "(evex_Wb ext 271)",   xx, xx, xx, xx, xx, mrm|evex|ttnone, x, 271},
  }
};
/****************************************************************************
 * Instructions that differ based on whether vex-encoded or not.
 * Most of these require an 0x66 prefix but we use reqp for that
 * so there's nothing inherent here about prefixes.
 */
const instr_info_t e_vex_extensions[][3] = {
  {    /* e_vex ext  0 */
    {INVALID, 0x663a4a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vblendvps, 0x663a4a18, "vblendvps", Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  1 */
    {INVALID, 0x663a4b18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vblendvpd, 0x663a4b18, "vblendvpd", Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4b18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  2 */
    {INVALID, 0x663a4c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpblendvb, 0x663a4c18, "vpblendvb", Vx, xx, Hx,Wx,Lx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  3 */
    {OP_ptest,  0x66381718, "ptest",    xx, xx,  Vdq,Wdq, xx, mrm|reqp, fW6, END_LIST},
    {OP_vptest, 0x66381718, "vptest",   xx, xx,    Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x66381718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext  4 */
    {OP_pmovsxbw,  0x66382018, "pmovsxbw", Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbw, 0x66382018, "vpmovsxbw", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[176][10]},
    {PREFIX_EXT,     0x382018, "(prefix ext 176)", xx, xx, xx, xx, xx, mrm|evex, x, 176},
  }, { /* e_vex ext  5 */
    {OP_pmovsxbd,  0x66382118, "pmovsxbd", Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbd, 0x66382118, "vpmovsxbd", Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[170][10]},
    {PREFIX_EXT,     0x382118, "(prefix ext 170)", xx, xx, xx, xx, xx, mrm|evex, x, 170},
  }, { /* e_vex ext  6 */
    /* XXX i#1312: the SSE and VEX table entries could get moved to prefix_extensions and
     * this table here re-numbered.
     */
    {OP_pmovsxbq,  0x66382218, "pmovsxbq", Vdq, xx, Ww_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxbq, 0x66382218, "vpmovsxbq", Vx, xx, Wj_x, xx, xx, mrm|vex|reqp, x, tpe[161][10]},
    {PREFIX_EXT,     0x382218, "(prefix ext 161)", xx, xx, xx, xx, xx, mrm|evex, x, 161},
  }, { /* e_vex ext  7 */
    {OP_pmovsxwd,  0x66382318, "pmovsxwd", Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxwd, 0x66382318, "vpmovsxwd", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[173][10]},
    {PREFIX_EXT,     0x382318, "(prefix ext 173)", xx, xx, xx, xx, xx, mrm|evex, x, 173},
  }, { /* e_vex ext  8 */
    {OP_pmovsxwq,  0x66382418, "pmovsxwq", Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxwq, 0x66382418, "vpmovsxwq", Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[164][10]},
    {PREFIX_EXT,     0x382418, "(prefix ext 164)", xx, xx, xx, xx, xx, mrm|evex, x, 164},
  }, { /* e_vex ext  9 */
    {OP_pmovsxdq,  0x66382518, "pmovsxdq", Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovsxdq, 0x66382518, "vpmovsxdq", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[167][10]},
    {PREFIX_EXT,     0x382518, "(prefix ext 167)", xx, xx, xx, xx, xx, mrm|evex, x, 167},
  }, { /* e_vex ext 10 */
    {OP_pmuldq,  0x66382818, "pmuldq",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmuldq, 0x66382818, "vpmuldq",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[227][2]},
    {PREFIX_EXT,   0x382818, "(prefix ext 178)", xx, xx, xx, xx, xx, mrm|evex, x, 178},
  }, { /* e_vex ext 11 */
    {OP_pcmpeqq,  0x66382918, "pcmpeqq",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpcmpeqq, 0x66382918, "vpcmpeqq",  Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[233][2]},
    {PREFIX_EXT,    0x382918, "(prefix ext 180)", xx, xx, xx, xx, xx, mrm|evex, x, 180},
  }, { /* e_vex ext 12 */
    {OP_movntdqa,  0x66382a18, "movntdqa", Mdq, xx, Vdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vmovntdqa, 0x66382a18, "vmovntdqa", Mx, xx, Vx, xx, xx, mrm|vex|reqp, x, tpe[184][10]},
    {PREFIX_EXT,    0x382a18, "(prefix ext 184)", xx, xx, xx, xx, xx, mrm|evex, x, 184},
  }, { /* e_vex ext 13 */
    {OP_packusdw,  0x66382b18, "packusdw", Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpackusdw, 0x66382b18, "vpackusdw", Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[245][0]},
    {EVEX_Wb_EXT,  0x66382b18, "(evex_Wb ext 245)", xx, xx, xx, xx, xx, mrm|evex, x, 245},
  }, { /* e_vex ext 14 */
    {OP_pmovzxbw,  0x66383018, "pmovzxbw", Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbw, 0x66383018, "vpmovzxbw", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[175][10]},
    {PREFIX_EXT,     0x383018, "(prefix ext 175)", xx, xx, xx, xx, xx, mrm|evex, x, 175},
  }, { /* e_vex ext 15 */
    {OP_pmovzxbd,  0x66383118, "pmovzxbd", Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbd, 0x66383118, "vpmovzxbd", Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[169][10]},
    {PREFIX_EXT,     0x383118, "(prefix ext 169)", xx, xx, xx, xx, xx, mrm|evex, x, 169},
  }, { /* e_vex ext 16 */
    /* XXX i#1312: the SSE and VEX table entries could get moved to prefix_extensions and
     * this table here re-numbered.
     */
    {OP_pmovzxbq,  0x66383218, "pmovzxbq", Vdq, xx, Ww_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxbq, 0x66383218, "vpmovzxbq", Vx, xx, Wj_x, xx, xx, mrm|vex|reqp, x, tpe[160][10]},
    {PREFIX_EXT, 0x383218, "(prefix ext 160)", xx, xx, xx, xx, xx, mrm|evex, x, 160},
  }, { /* e_vex ext 17 */
    {OP_pmovzxwd,  0x66383318, "pmovzxwd", Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxwd, 0x66383318, "vpmovzxwd", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[172][10]},
    {PREFIX_EXT,     0x383318, "(prefix ext 172)", xx, xx, xx, xx, xx, mrm|evex, x, 172},
  }, { /* e_vex ext 18 */
    {OP_pmovzxwq,  0x66383418, "pmovzxwq", Vdq, xx, Wd_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxwq, 0x66383418, "vpmovzxwq", Vx, xx, Wi_x, xx, xx, mrm|vex|reqp, x, tpe[163][10]},
    {PREFIX_EXT, 0x383418, "(prefix ext 163)", xx, xx, xx, xx, xx, mrm|evex, x, 163},
  }, { /* e_vex ext 19 */
    {OP_pmovzxdq,  0x66383518, "pmovzxdq", Vdq, xx, Wq_dq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vpmovzxdq, 0x66383518, "vpmovzxdq", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tpe[166][10]},
    {PREFIX_EXT,     0x383518, "(prefix ext 166)", xx, xx, xx, xx, xx, mrm|evex, x, 166},
  }, { /* e_vex ext 20 */
    {OP_pcmpgtq,  0x66383718, "pcmpgtq",  Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpcmpgtq, 0x66383718, "vpcmpgtq",  Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[232][2]},
    {EVEX_Wb_EXT,  0x66383748, "(evex_Wb ext 232)", xx, xx, xx, xx, xx, mrm|evex, x, 232},
  }, { /* e_vex ext 21 */
    {OP_pminsb,  0x66383818, "pminsb",   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminsb, 0x66383818, "vpminsb",   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tpe[179][10]},
    {PREFIX_EXT,   0x383818, "(prefix ext 179)", xx, xx, xx, xx, xx, mrm|evex, x, 179},
  }, { /* e_vex ext 22 */
    {OP_pminsd,   0x66383918, "pminsd",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminsd,  0x66383918, "vpminsd",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[113][0]},
    {PREFIX_EXT,    0x383918, "(prefix ext 181)", xx, xx, xx, xx, xx, mrm|evex, x, 181},
  }, { /* e_vex ext 23 */
    {OP_pminuw,   0x66383a18, "pminuw",   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminuw,  0x66383a18, "vpminuw",   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tpe[185][10]},
    {PREFIX_EXT,    0x383a18, "(prefix ext 185)", xx, xx, xx, xx, xx, mrm|evex, x, 185},
  }, { /* e_vex ext 24 */
    {OP_pminud,   0x66383b18, "pminud",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpminud,  0x66383b18, "vpminud",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[115][0]},
    {EVEX_Wb_EXT,  0x66383b18, "(evex_Wb ext 115)", xx, xx, xx, xx, xx, mrm|evex, x, 115},
  }, { /* e_vex ext 25 */
    {OP_pmaxsb,   0x66383c18, "pmaxsb",   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxsb,  0x66383c18, "vpmaxsb",   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tvex[25][2]},
    {OP_vpmaxsb,  0x66383c08, "vpmaxsb",   Ve, xx, KEq, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
  }, { /* e_vex ext 26 */
    {OP_pmaxsd,   0x66383d18, "pmaxsd",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxsd,  0x66383d18, "vpmaxsd",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[114][0]},
    {EVEX_Wb_EXT,  0x66383d18, "(evex_Wb ext 114)", xx, xx, xx, xx, xx, mrm|evex, x, 114},
  }, { /* e_vex ext 27 */
    {OP_pmaxuw,   0x66383e18, "pmaxuw",   Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxuw,  0x66383e18, "vpmaxuw",   Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tvex[27][2]},
    {OP_vpmaxuw,  0x66383e08, "vpmaxuw",   Ve, xx, KEd, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
  }, { /* e_vex ext 28 */
    {OP_pmaxud,   0x66383f18, "pmaxud",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmaxud,  0x66383f18, "vpmaxud",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[116][0]},
    {EVEX_Wb_EXT,  0x66383f18, "(evex_Wb ext 116)", xx, xx, xx, xx, xx, mrm|evex, x, 116},
  }, { /* e_vex ext 29 */
    {OP_pmulld,   0x66384018, "pmulld",   Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vpmulld,  0x66384018, "vpmulld",   Vx, xx, Hx,Wx, xx, mrm|vex|reqp, x, tevexwb[45][0]},
    {EVEX_Wb_EXT, 0x66384018, "(evex_Wb ext 45)", xx, xx, xx, xx, xx, mrm|evex, x, 45},
  }, { /* e_vex ext 30 */
    {OP_phminposuw,  0x66384118,"phminposuw",Vdq,xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vphminposuw, 0x66384118,"vphminposuw",Vdq,xx, Wdq, xx, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x66384118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 31 */
    {OP_aesimc,  0x6638db18, "aesimc",  Vdq, xx, Wdq, xx, xx, mrm|reqp, x, END_LIST},
    {OP_vaesimc, 0x6638db18, "vaesimc",  Vdq, xx, Wdq, xx, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638db18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 32 */
    {OP_aesenc,  0x6638dc18, "aesenc",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vaesenc, 0x6638dc18, "vaesenc",  Vdq, xx, Hdq,Wdq, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638dc18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 33 */
    {OP_aesenclast,  0x6638dd18,"aesenclast",Vdq,xx,Wdq,Vdq,xx, mrm|reqp, x, END_LIST},
    {OP_vaesenclast, 0x6638dd18,"vaesenclast",Vdq,xx,Hdq,Wdq,xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638dd18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 34 */
    {OP_aesdec,  0x6638de18, "aesdec",  Vdq, xx, Wdq,Vdq, xx, mrm|reqp, x, END_LIST},
    {OP_vaesdec, 0x6638de18, "vaesdec",  Vdq, xx, Hdq,Wdq, xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638de18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 35 */
    {OP_aesdeclast,  0x6638df18,"aesdeclast",Vdq,xx,Wdq,Vdq,xx, mrm|reqp, x, END_LIST},
    {OP_vaesdeclast, 0x6638df18,"vaesdeclast",Vdq,xx,Hdq,Wdq,xx, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x6638df18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 36 */
    {OP_pextrb,   0x663a1418, "pextrb", Rd_Mb, xx, Vb_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpextrb,  0x663a1418, "vpextrb", Rd_Mb, xx, Vb_dq, Ib, xx, mrm|vex|reqp, x, tvex[36][2]},
    {OP_vpextrb,  0x663a1408, "vpextrb", Rd_Mb, xx, Vb_dq, Ib, xx, mrm|evex|reqp|ttt1s|inopsz1, x, END_LIST},
  }, { /* e_vex ext 37 */
    {OP_pextrw,   0x663a1518, "pextrw", Rd_Mw, xx, Vw_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpextrw,  0x663a1518, "vpextrw", Rd_Mw, xx, Vw_dq, Ib, xx, mrm|vex|reqp, x, tpe[54][10]},
    {OP_vpextrw,  0x663a1508, "vpextrw", Rd_Mw, xx, Vw_dq, Ib, xx, mrm|evex|reqp|ttt1s|inopsz2, x, END_LIST},
  }, { /* e_vex ext 38 */
    {OP_pextrd,   0x663a1618, "pextrd",  Ey, xx, Vd_q_dq, Ib, xx, mrm|reqp, x, END_LIST},/*"pextrq" with rex.w*/
    {OP_vpextrd,  0x663a1618, "vpextrd",  Ey, xx, Vd_q_dq, Ib, xx, mrm|vex|reqp, x, tevexwb[145][0]},/*"vpextrq" with rex.w*/
    {EVEX_Wb_EXT, 0x663a1618, "(evex_Wb ext 145)", xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 145},
  }, { /* e_vex ext 39 */
    {OP_extractps,  0x663a1718, "extractps", Ed, xx, Vd_dq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vextractps, 0x663a1718, "vextractps", Ed, xx, Ib, Vd_dq, xx, mrm|vex|reqp, x, tvex[39][2]},
    {OP_vextractps, 0x663a1708, "vextractps", Ed, xx, Ib, Vd_dq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
  }, { /* e_vex ext 40 */
    {OP_roundps,  0x663a0818, "roundps",  Vdq, xx, Wdq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundps, 0x663a0818, "vroundps",  Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0808, "(evex_Wb ext 246)", xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 246},
  }, { /* e_vex ext 41 */
    {OP_roundpd,  0x663a0918, "roundpd",  Vdq, xx, Wdq, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundpd, 0x663a0918, "vroundpd",  Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0948, "(evex_Wb ext 218)", xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 218},
  }, { /* e_vex ext 42 */
    {OP_roundss,  0x663a0a18, "roundss",  Vss, xx, Wss, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundss, 0x663a0a18, "vroundss",  Vdq, xx, H12_dq, Wss, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0a08, "(evex_Wb ext 253)", xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 253},
  }, { /* e_vex ext 43 */
    {OP_roundsd,  0x663a0b18, "roundsd",  Vsd, xx, Wsd, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vroundsd, 0x663a0b18, "vroundsd",  Vdq, xx, Hsd, Wsd, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a0b08, "(evex_Wb ext 254)", xx, xx, xx, xx, xx, mrm|evex|ttt1s, x, 254},
  }, { /* e_vex ext 44 */
    {OP_blendps,  0x663a0c18, "blendps",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vblendps, 0x663a0c18, "vblendps",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 45 */
    {OP_blendpd,  0x663a0d18, "blendpd",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vblendpd, 0x663a0d18, "vblendpd",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 46 */
    {OP_pblendw,  0x663a0e18, "pblendw",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vpblendw, 0x663a0e18, "vpblendw",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 47 */
    /* FIXME i#1388: pinsrb actually reads only bottom byte of reg */
    {OP_pinsrb,   0x663a2018, "pinsrb",   Vb_dq, xx, Rd_Mb,  Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vpinsrb,  0x663a2018, "vpinsrb",   Vdq, xx, H15_dq, Rd_Mb, Ib, mrm|vex|reqp, x, tvex[47][2]},
    {OP_vpinsrb,  0x663a2008, "vpinsrb",   Vdq, xx, H15_dq, Rd_Mb, Ib, mrm|evex|reqp|ttt1s|inopsz1, x, END_LIST},
  }, { /* e_vex ext 48 */
    {OP_insertps, 0x663a2118, "insertps", Vdq, xx, Udq_Md, Ib, xx, mrm|reqp, x, END_LIST},
    {OP_vinsertps,0x663a2118, "vinsertps", Vdq, xx, Ib, Hdq, Udq_Md, mrm|vex|reqp|reqL0, x, tvex[48][2]},
    {OP_vinsertps,0x663a2108, "vinsertps", Vdq, xx, Ib, Hdq, Udq_Md, mrm|evex|reqp|reqL0|ttt1s, x, END_LIST},
  }, { /* e_vex ext 49 */
    {OP_pinsrd,   0x663a2218, "pinsrd",   Vd_q_dq, xx, Ey,Ib, xx, mrm|reqp, x, END_LIST},/*"pinsrq" with rex.w*/
    {OP_vpinsrd,  0x663a2218, "vpinsrd",   Vdq, xx, H12_8_dq, Ey, Ib, mrm|vex|reqp, x, tevexwb[144][0]},/*"vpinsrq" with rex.w*/
    {EVEX_Wb_EXT, 0x663a2218, "(evex_Wb ext 144)", xx, xx, xx, xx, xx, mrm|evex, x, 144},
  }, { /* e_vex ext 50 */
    {OP_dpps,     0x663a4018, "dpps",     Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vdpps,    0x663a4018, "vdpps",     Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 51 */
    {OP_dppd,     0x663a4118, "dppd",     Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vdppd,    0x663a4118, "vdppd",     Vdq, xx, Hdq, Wdq, Ib, mrm|vex|reqp|reqL0, x, END_LIST},
    {INVALID, 0x663a4118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 52 */
    {OP_mpsadbw,  0x663a4218, "mpsadbw",  Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vmpsadbw, 0x663a4218, "vmpsadbw",  Vx, xx, Hx, Wx, Ib, mrm|vex|reqp, x, END_LIST},
    {OP_vdbpsadbw, 0x663a4208, "vdbpsadbw",  Ve, xx, KEd, Ib, He, xop|mrm|evex|reqp|ttfvm, x, exop[249]},
  }, { /* e_vex ext 53 */
    {OP_pcmpestrm, 0x663a6018, "pcmpestrm",xmm0, xx, Vdq, Wdq, Ib, mrm|reqp|xop, fW6, exop[8]},
    {OP_vpcmpestrm,0x663a6018, "vpcmpestrm",xmm0, xx, Vdq, Wdq, Ib, mrm|vex|reqp|xop, fW6, exop[11]},
    {INVALID, 0x663a6018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 54 */
    {OP_pcmpestri, 0x663a6118, "pcmpestri",ecx, xx, Vdq, Wdq, Ib, mrm|reqp|xop, fW6, exop[9]},
    {OP_vpcmpestri,0x663a6118, "vpcmpestri",ecx, xx, Vdq, Wdq, Ib, mrm|vex|reqp|xop, fW6, exop[12]},
    {INVALID, 0x663a6118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 55 */
    {OP_pcmpistrm, 0x663a6218, "pcmpistrm",xmm0, xx, Vdq, Wdq, Ib, mrm|reqp, fW6, END_LIST},
    {OP_vpcmpistrm,0x663a6218, "vpcmpistrm",xmm0, xx, Vdq, Wdq, Ib, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x663a6218, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 56 */
    {OP_pcmpistri, 0x663a6318, "pcmpistri",ecx, xx, Vdq, Wdq, Ib, mrm|reqp, fW6, END_LIST},
    {OP_vpcmpistri,0x663a6318, "vpcmpistri",ecx, xx, Vdq, Wdq, Ib, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x663a6318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 57 */
    {OP_pclmulqdq, 0x663a4418, "pclmulqdq", Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},
    {OP_vpclmulqdq,0x663a4418, "vpclmulqdq", Vdq, xx, Hdq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a4418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 58 */
    {OP_aeskeygenassist, 0x663adf18, "aeskeygenassist",Vdq,xx,Wdq,Ib,xx,mrm|reqp,x,END_LIST},
    {OP_vaeskeygenassist,0x663adf18, "vaeskeygenassist",Vdq,xx,Wdq,Ib,xx,mrm|vex|reqp,x,END_LIST},
    {INVALID, 0x663adf18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 59 */
    {INVALID,   0x66380e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtestps, 0x66380e18, "vtestps", xx, xx, Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x66380e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 60 */
    {INVALID,   0x66380f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vtestpd, 0x66380f18, "vtestpd", xx, xx, Vx,Wx, xx, mrm|vex|reqp, fW6, END_LIST},
    {INVALID, 0x66380f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 61 */
    {OP_ldmxcsr, 0x0fae32, "ldmxcsr", xx, xx, Md, xx, xx, mrm, x, END_LIST},
    {OP_vldmxcsr, 0x0fae32, "vldmxcsr", xx, xx, Md, xx, xx, mrm|vex|reqL0, x, END_LIST},
    {INVALID, 0x0fae32, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 62 */
    {OP_stmxcsr, 0x0fae33, "stmxcsr", Md, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_vstmxcsr, 0x0fae33, "vstmxcsr", Md, xx, xx, xx, xx, mrm|vex, x, END_LIST},
    {INVALID, 0x0fae33, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 63 */
    {INVALID,   0x66381318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtph2ps, 0x66381318, "vcvtph2ps", Vx, xx, Wh_x, xx, xx, mrm|vex|reqp, x, tevexwb[263][0]},
    {PREFIX_EXT,     0x381318, "(prefix ext 174)", xx, xx, xx, xx, xx, mrm|evex, x, 174},
  }, { /* e_vex ext 64 */
    {INVALID,   0x66381818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastss, 0x66381818, "vbroadcastss", Vx, xx, Wd_dq, xx, xx, mrm|vex|reqp, x, tvex[64][2]},
    {OP_vbroadcastss, 0x66381808, "vbroadcastss", Ve, xx, KEw, Wd_dq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
  }, { /* e_vex ext 65 */
    {INVALID,   0x66381918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastsd, 0x66381918, "vbroadcastsd", Vqq, xx, Wq_dq, xx, xx, mrm|vex|reqp|reqL1, x, tevexwb[148][2]},
    {EVEX_Wb_EXT, 0x66381918, "(evex_Wb ext 148)", xx, xx, xx, xx, xx, mrm|evex, x, 148},
  }, { /* e_vex ext 66 */
    {INVALID,   0x66381a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcastf128, 0x66381a18, "vbroadcastf128", Vqq, xx, Mdq, xx, xx, mrm|vex|reqp|reqL1, x, END_LIST},
    {EVEX_Wb_EXT, 0x66381a18, "(evex_Wb ext 149)", xx, xx, xx, xx, xx, mrm|evex, x, 149},
  }, { /* e_vex ext 67 */
    {INVALID,   0x66382c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovps, 0x66382c18, "vmaskmovps", Vx, xx, Hx,Mx, xx, mrm|vex|reqp|predcx, x, tvex[69][1]},
    {EVEX_Wb_EXT, 0x66382c18, "(evex_Wb ext 181)", xx, xx, xx, xx, xx, mrm|evex, x, 181},
  }, { /* e_vex ext 68 */
    {INVALID,   0x66382d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovpd, 0x66382d18, "vmaskmovpd", Vx, xx, Hx,Mx, xx, mrm|vex|reqp|predcx, x, tvex[70][1]},
    {EVEX_Wb_EXT, 0x66382d18, "(evex_Wb ext 182)", xx, xx, xx, xx, xx, mrm|evex, x, 182},
  }, { /* e_vex ext 69 */
    {INVALID,   0x66382e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovps, 0x66382e18, "vmaskmovps", Mx, xx, Hx,Vx, xx, mrm|vex|reqp|predcx, x, END_LIST},
    {INVALID, 0x66382e18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 70 */
    {INVALID,   0x66382f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vmaskmovpd, 0x66382f18, "vmaskmovpd", Mx, xx, Hx,Vx, xx, mrm|vex|reqp|predcx, x, END_LIST},
    {INVALID, 0x66382f18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 71 */
    {INVALID,   0x663a0418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilps, 0x663a0418, "vpermilps", Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, tvex[77][1]},
    {EVEX_Wb_EXT, 0x663a0418, "(evex_Wb ext 247)", xx, xx, xx, xx, xx, mrm|evex, x, 247},
  }, { /* e_vex ext 72 */
    {INVALID,   0x663a0518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x663a0518, "vpermilpd", Vx, xx, Wx, Ib, xx, mrm|vex|reqp, x, tvex[78][1]},
    {EVEX_Wb_EXT, 0x663a0548, "(evex_Wb ext 230)", xx, xx, xx, xx, xx, mrm|evex, x, 230},
  }, { /* e_vex ext 73 */
    {INVALID,   0x663a0618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vperm2f128, 0x663a0618, "vperm2f128", Vqq, xx, Hqq, Wqq, Ib, mrm|vex|reqp, x, END_LIST},
    {INVALID, 0x663a0618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 74 */
    {INVALID,   0x663a1818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vinsertf128, 0x663a1818, "vinsertf128", Vqq, xx, Hqq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a1818, "(evex_Wb ext 105)", xx, xx, xx, xx, xx, mrm|evex, x, 105},
  }, { /* e_vex ext 75 */
    {INVALID,   0x663a1918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vextractf128, 0x663a1918, "vextractf128", Wdq, xx, Vdq_qq, Ib, xx, mrm|vex|reqp|reqL1, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a1918, "(evex_Wb ext 101)", xx, xx, xx, xx, xx, mrm|evex, x, 101},
  }, { /* e_vex ext 76 */
    {INVALID,   0x663a1d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vcvtps2ph, 0x663a1d18, "vcvtps2ph", Wh_x, xx, Vx, Ib, xx, mrm|vex|reqp, x, tevexwb[264][0]},
    {EVEX_Wb_EXT,   0x663a1d08, "(evex_Wb ext 264)", xx, xx, xx, xx, xx, mrm|evex, x, 264},
  }, { /* e_vex ext 77 */
    {INVALID,   0x66380c18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilps, 0x66380c18, "vpermilps", Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[247][0]},
    {EVEX_Wb_EXT, 0x66380c08, "(evex_Wb ext 248)", xx, xx, xx, xx, xx, mrm|evex, x, 248},
  }, { /* e_vex ext 78 */
    {INVALID,   0x66380d18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x66380d18, "vpermilpd", Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[230][2]},
    {EVEX_Wb_EXT, 0x66380d48, "(evex_Wb ext 231)", xx, xx, xx, xx, xx, mrm|evex, x, 231},
  }, { /* e_vex ext 79 */
    {OP_seto,    0x0f9010,             "seto", Eb, xx, xx, xx, xx, mrm, fRO, END_LIST},
    {PREFIX_EXT, 0x0f9010, "(prefix ext 144)", xx, xx, xx, xx, xx, mrm,   x, 144},
    {INVALID, 0x0f9010, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 80 */
    {OP_setno,   0x0f9110,            "setno", Eb, xx, xx, xx, xx, mrm, fRO, END_LIST},
    {PREFIX_EXT, 0x0f9110, "(prefix ext 145)", xx, xx, xx, xx, xx, mrm,   x, 145},
    {INVALID, 0x0f9110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 81 */
    {OP_setb,    0x0f9210,             "setb", Eb, xx, xx, xx, xx, mrm, fRC, END_LIST},
    {PREFIX_EXT, 0x0f9210, "(prefix ext 146)", xx, xx, xx, xx, xx, mrm,   x, 146},
    {INVALID, 0x0f9210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 82 */
    {OP_setnb,   0x0f9310,            "setnb", Eb, xx, xx, xx, xx, mrm, fRC, END_LIST},
    {PREFIX_EXT, 0x0f9310, "(prefix ext 147)", xx, xx, xx, xx, xx, mrm,   x, 147},
    {INVALID, 0x0f9310, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 83 */
    {OP_cmovno,  0x0f4110,           "cmovno", Gv, xx, Ev, xx, xx, mrm|predcc, fRO, END_LIST},
    {PREFIX_EXT, 0x0f4110, "(prefix ext 148)", xx, xx, xx, xx, xx, mrm,         x, 148},
    {INVALID, 0x0f4110, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 84 */
    {OP_cmovb,   0x0f4210,            "cmovb", Gv, xx, Ev, xx, xx, mrm|predcc, fRC, END_LIST},
    {PREFIX_EXT, 0x0f4210, "(prefix ext 149)", xx, xx, xx, xx, xx, mrm,          x, 149},
    {INVALID, 0x0f4210, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 85 */
    {OP_cmovnp,  0x0f4b10,           "cmovnp", Gv, xx, Ev, xx, xx, mrm|predcc, fRP, END_LIST},
    {PREFIX_EXT, 0x0f4b10, "(prefix ext 150)", xx, xx, xx, xx, xx, mrm,          x, 150},
    {INVALID, 0x0f4b10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 86 */
    {OP_cmovz,   0x0f4410,            "cmovz", Gv, xx, Ev, xx, xx, mrm|predcc, fRZ, END_LIST},
    {PREFIX_EXT, 0x0f4410, "(prefix ext 151)", xx, xx, xx, xx, xx, mrm,          x, 151},
    {INVALID, 0x0f4410, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 87 */
    {OP_cmovnz,  0x0f4510,           "cmovnz", Gv, xx, Ev, xx, xx, mrm|predcc, fRZ, END_LIST},
    {PREFIX_EXT, 0x0f4510, "(prefix ext 152)", xx, xx, xx, xx, xx, mrm,          x, 152},
    {INVALID, 0x0f4510, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 88 */
    {OP_cmovbe,  0x0f4610,           "cmovbe", Gv, xx, Ev, xx, xx, mrm|predcc, (fRC|fRZ), END_LIST},
    {PREFIX_EXT, 0x0f4610, "(prefix ext 153)", xx, xx, xx, xx, xx, mrm,                x, 153},
    {INVALID, 0x0f4610, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 89 */
    {OP_cmovnbe, 0x0f4710,          "cmovnbe", Gv, xx, Ev, xx, xx, mrm|predcc, (fRC|fRZ), END_LIST},
    {PREFIX_EXT, 0x0f4710, "(prefix ext 154)", xx, xx, xx, xx, xx, mrm,                x, 154},
    {INVALID, 0x0f4710, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 90 */
    {OP_cmovp,   0x0f4a10,            "cmovp", Gv, xx, Ev, xx, xx, mrm|predcc, fRP, END_LIST},
    {PREFIX_EXT, 0x0f4a10, "(prefix ext 155)", xx, xx, xx, xx, xx, mrm,          x, 155},
    {INVALID, 0x0f4a10, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 91 */
    {OP_sets,    0x0f9810,             "sets", Eb, xx, xx, xx, xx, mrm, fRS, END_LIST},
    {PREFIX_EXT, 0x0f9810, "(prefix ext 156)", xx, xx, xx, xx, xx, mrm,    x, 156},
    {INVALID, 0x0f9810, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 92 */
    {OP_setns,   0x0f9910,            "setns", Eb, xx, xx, xx, xx, mrm, fRS, END_LIST},
    {PREFIX_EXT, 0x0f9910, "(prefix ext 157)", xx, xx, xx, xx, xx, mrm,   x, 157},
    {INVALID, 0x0f9910, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* e_vex ext 93 */
    {INVALID,      0x66389810,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389818,   "(vex_W ext 0)", xx, xx, xx, xx, xx, mrm|vex, x, 0},
    {EVEX_Wb_EXT,   0x66389818, "(evex_Wb ext 62)", xx, xx, xx, xx, xx, mrm|evex, x, 62},
  }, { /* e_vex ext 94 */
    {INVALID,      0x6638a810,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a818,   "(vex_W ext 1)", xx, xx, xx, xx, xx, mrm|vex, x, 1},
    {EVEX_Wb_EXT,   0x6638a818, "(evex_Wb ext 63)", xx, xx, xx, xx, xx, mrm|evex, x, 63},
  }, { /* e_vex ext 95 */
    {INVALID,      0x6638b810,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b818,   "(vex_W ext 2)", xx, xx, xx, xx, xx, mrm|vex, x, 2},
    {EVEX_Wb_EXT,   0x6638b818, "(evex_Wb ext 64)", xx, xx, xx, xx, xx, mrm|evex, x, 64},
  }, { /* e_vex ext 96 */
    {INVALID,      0x66389910,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389918,   "(vex_W ext 3)", xx, xx, xx, xx, xx, mrm|vex, x, 3},
    {EVEX_Wb_EXT,   0x66389918, "(evex_Wb ext 65)", xx, xx, xx, xx, xx, mrm|evex, x, 65},
  }, { /* e_vex ext 97 */
    {INVALID,      0x6638a910,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a918,   "(vex_W ext 4)", xx, xx, xx, xx, xx, mrm|vex, x, 4},
    {EVEX_Wb_EXT,   0x6638a918, "(evex_Wb ext 66)", xx, xx, xx, xx, xx, mrm|evex, x, 66},
  }, { /* e_vex ext 98 */
    {INVALID,      0x6638b910,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b918,   "(vex_W ext 5)", xx, xx, xx, xx, xx, mrm|vex, x, 5},
    {EVEX_Wb_EXT,   0x6638b918, "(evex_Wb ext 67)", xx, xx, xx, xx, xx, mrm|evex, x, 67},
  }, { /* e_vex ext 99 */
    {INVALID,      0x66389610,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389618,   "(vex_W ext 6)", xx, xx, xx, xx, xx, mrm|vex, x, 6},
    {EVEX_Wb_EXT,   0x66389618, "(evex_Wb ext 68)", xx, xx, xx, xx, xx, mrm|evex, x, 68},
  }, { /* e_vex ext 100 */
    {INVALID,      0x6638a610,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a618,   "(vex_W ext 7)", xx, xx, xx, xx, xx, mrm|vex, x, 7},
    {EVEX_Wb_EXT,   0x6638a618, "(evex_Wb ext 69)", xx, xx, xx, xx, xx, mrm|evex, x, 69},
  }, { /* e_vex ext 101 */
    {INVALID,      0x6638b610,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b618,   "(vex_W ext 8)", xx, xx, xx, xx, xx, mrm|vex, x, 8},
    {EVEX_Wb_EXT,   0x6638b618, "(evex_Wb ext 70)", xx, xx, xx, xx, xx, mrm|evex, x, 70},
  }, { /* e_vex ext 102 */
    {INVALID,      0x66389710,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389718,   "(vex_W ext 9)", xx, xx, xx, xx, xx, mrm|vex, x, 9},
    {EVEX_Wb_EXT,   0x66389718, "(evex_Wb ext 71)", xx, xx, xx, xx, xx, mrm|evex, x, 71},
  }, { /* e_vex ext 103 */
    {INVALID,      0x6638a710,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638a718,  "(vex_W ext 10)", xx, xx, xx, xx, xx, mrm|vex, x, 10},
    {EVEX_Wb_EXT,   0x6638a718, "(evex_Wb ext 72)", xx, xx, xx, xx, xx, mrm|evex, x, 72},
  }, { /* e_vex ext 104 */
    {INVALID,      0x6638b710,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638b718,  "(vex_W ext 11)", xx, xx, xx, xx, xx, mrm|vex, x, 11},
    {EVEX_Wb_EXT,   0x6638b718, "(evex_Wb ext 73)", xx, xx, xx, xx, xx, mrm|evex, x, 73},
  }, { /* e_vex ext 105 */
    {INVALID,      0x66389a10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389a18,  "(vex_W ext 12)", xx, xx, xx, xx, xx, mrm|vex, x, 12},
    {EVEX_Wb_EXT,   0x66389a18, "(evex_Wb ext 74)", xx, xx, xx, xx, xx, mrm|evex, x, 74},
  }, { /* e_vex ext 106 */
    {INVALID,      0x6638aa10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638aa18,  "(vex_W ext 13)", xx, xx, xx, xx, xx, mrm|vex, x, 13},
    {EVEX_Wb_EXT,   0x6638aa18, "(evex_Wb ext 75)", xx, xx, xx, xx, xx, mrm|evex, x, 75},
  }, { /* e_vex ext 107 */
    {INVALID,      0x6638ba10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ba18,  "(vex_W ext 14)", xx, xx, xx, xx, xx, mrm|vex, x, 14},
    {EVEX_Wb_EXT,   0x6638ba18, "(evex_Wb ext 76)", xx, xx, xx, xx, xx, mrm|evex, x, 76},
  }, { /* e_vex ext 108 */
    {INVALID,      0x66389b10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389b18,  "(vex_W ext 15)", xx, xx, xx, xx, xx, mrm|vex, x, 15},
    {EVEX_Wb_EXT,   0x66389b18, "(evex_Wb ext 77)", xx, xx, xx, xx, xx, mrm|evex, x, 77},
  }, { /* e_vex ext 109 */
    {INVALID,      0x6638ab10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ab18,  "(vex_W ext 16)", xx, xx, xx, xx, xx, mrm|vex, x, 16},
    {EVEX_Wb_EXT,   0x6638ab18, "(evex_Wb ext 78)", xx, xx, xx, xx, xx, mrm|evex, x, 78},
  }, { /* e_vex ext 110 */
    {INVALID,      0x6638bb10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bb18,  "(vex_W ext 17)", xx, xx, xx, xx, xx, mrm|vex, x, 17},
    {EVEX_Wb_EXT,   0x6638bb18, "(evex_Wb ext 79)", xx, xx, xx, xx, xx, mrm|evex, x, 79},
  }, { /* e_vex ext 111 */
    {INVALID,      0x66389c10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389c18,  "(vex_W ext 18)", xx, xx, xx, xx, xx, mrm|vex, x, 18},
    {EVEX_Wb_EXT,   0x66389c18, "(evex_Wb ext 80)", xx, xx, xx, xx, xx, mrm|evex, x, 80},
  }, { /* e_vex ext 112 */
    {INVALID,      0x6638ac10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ac18,  "(vex_W ext 19)", xx, xx, xx, xx, xx, mrm|vex, x, 19},
    {EVEX_Wb_EXT,   0x6638ac18, "(evex_Wb ext 81)", xx, xx, xx, xx, xx, mrm|evex, x, 81},
  }, { /* e_vex ext 113 */
    {INVALID,      0x6638bc10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bc18,  "(vex_W ext 20)", xx, xx, xx, xx, xx, mrm|vex, x, 20},
    {EVEX_Wb_EXT,   0x6638bc18, "(evex_Wb ext 82)", xx, xx, xx, xx, xx, mrm|evex, x, 82},
  }, { /* e_vex ext 114 */
    {INVALID,      0x66389d10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389d18,  "(vex_W ext 21)", xx, xx, xx, xx, xx, mrm|vex, x, 21},
    {EVEX_Wb_EXT,   0x66389d18, "(evex_Wb ext 83)", xx, xx, xx, xx, xx, mrm|evex, x, 83},
  }, { /* e_vex ext 115 */
    {INVALID,      0x6638ad10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ad18,  "(vex_W ext 22)", xx, xx, xx, xx, xx, mrm|vex, x, 22},
    {EVEX_Wb_EXT,   0x6638ad18, "(evex_Wb ext 84)", xx, xx, xx, xx, xx, mrm|evex, x, 84},
  }, { /* e_vex ext 116 */
    {INVALID,      0x6638bd10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bd18,  "(vex_W ext 23)", xx, xx, xx, xx, xx, mrm|vex, x, 23},
    {EVEX_Wb_EXT,   0x6638bd18, "(evex_Wb ext 85)", xx, xx, xx, xx, xx, mrm|evex, x, 85},
  }, { /* e_vex ext 117 */
    {INVALID,      0x66389e10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389e18,  "(vex_W ext 24)", xx, xx, xx, xx, xx, mrm|vex, x, 24},
    {EVEX_Wb_EXT,   0x66389e18, "(evex_Wb ext 86)", xx, xx, xx, xx, xx, mrm|evex, x, 86},
  }, { /* e_vex ext 118 */
    {INVALID,      0x6638ae10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638ae18,  "(vex_W ext 25)", xx, xx, xx, xx, xx, mrm|vex, x, 25},
    {EVEX_Wb_EXT,   0x6638ae18, "(evex_Wb ext 87)", xx, xx, xx, xx, xx, mrm|evex, x, 87},
  }, { /* e_vex ext 119 */
    {INVALID,      0x6638be10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638be18,  "(vex_W ext 26)", xx, xx, xx, xx, xx, mrm|vex, x, 26},
    {EVEX_Wb_EXT,   0x6638be18, "(evex_Wb ext 88)", xx, xx, xx, xx, xx, mrm|evex, x, 88},
  }, { /* e_vex ext 120 */
    {INVALID,      0x66389f10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x66389f18,  "(vex_W ext 27)", xx, xx, xx, xx, xx, mrm|vex, x, 27},
    {EVEX_Wb_EXT,   0x66389f18, "(evex_Wb ext 89)", xx, xx, xx, xx, xx, mrm|evex, x, 89},
  }, { /* e_vex ext 121 */
    {INVALID,      0x6638af10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638af18,  "(vex_W ext 28)", xx, xx, xx, xx, xx, mrm|vex, x, 28},
    {EVEX_Wb_EXT,   0x6638af18, "(evex_Wb ext 90)", xx, xx, xx, xx, xx, mrm|evex, x, 90},
  }, { /* e_vex ext 122 */
    {INVALID,      0x6638bf10,           "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT,    0x6638bf18,  "(vex_W ext 29)", xx, xx, xx, xx, xx, mrm|vex, x, 29},
    {EVEX_Wb_EXT,   0x6638bf18, "(evex_Wb ext 91)", xx, xx, xx, xx, xx, mrm|evex, x, 91},
  }, { /* e_vex ext 123 */
    {INVALID, 0x66383610, "(bad)", xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermd, 0x66383618, "vpermd", Vqq, xx, Hqq, Wqq, xx, mrm|vex|reqp, x, tevexwb[93][0]},
    {EVEX_Wb_EXT, 0x66383618, "(evex_Wb ext 93)", xx, xx, xx, xx, xx, mrm|evex, x, 93},
  }, { /* e_vex ext 124 */
    {INVALID, 0x66381610, "(bad)", xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermps, 0x66381618, "vpermps", Vqq, xx, Hqq, Wqq, xx, mrm|vex|reqp, x, tevexwb[94][0]},
    {EVEX_Wb_EXT, 0x66381618, "(evex_Wb ext 94)", xx, xx, xx, xx, xx, mrm|evex, x, 94 },
  }, { /* e_vex ext 125 */
    {INVALID, 0x663a0010, "(bad)", xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermq, 0x663a0058, "vpermq", Vqq, xx, Wqq, Ib, xx, mrm|vex|reqp, x, tevexwb[251][2]},
    {EVEX_Wb_EXT, 0x663a0048, "(evex_Wb ext 251)", xx, xx, xx, xx, xx, mrm|evex, x, 251 },
  }, { /* e_vex ext 126 */
    {INVALID, 0x663a0010, "(bad)", xx, xx, xx, xx, xx, no, x, NA },
    {OP_vpermpd, 0x663a0158, "vpermpd", Vqq, xx, Wqq, Ib, xx, mrm|vex|reqp, x, tevexwb[252][2]},
    {EVEX_Wb_EXT, 0x663a0148, "(evex_Wb ext 252)", xx, xx, xx, xx, xx, mrm|evex, x, 252 },
  }, { /* e_vex ext 127 */
    {INVALID, 0x663a3918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vextracti128,0x663a3918, "vextracti128", Wdq, xx, Vqq, Ib, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a3918, "(evex_Wb ext 103)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 103},
  }, { /* e_vex ext 128 */
    {INVALID, 0x663a3818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vinserti128, 0x663a3818, "vinserti128", Vqq, xx, Hqq, Wdq, Ib, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x663a3818, "(evex_Wb ext 107)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 107},
  }, { /* e_vex ext 129 */
    {OP_blendvpd, 0x66381518, "blendvpd", Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp, x, END_LIST},
    {INVALID,     0x66381518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT,    0x381518, "(prefix ext 168)", xx, xx, xx, xx, xx, mrm|evex, x, 168},
  }, { /* e_vex ext 130 */
    {OP_blendvps, 0x66381418, "blendvps", Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp, x, END_LIST},
    {INVALID, 0x66381418, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x381418, "(prefix ext 165)", xx, xx, xx, xx, xx, mrm, x, 165},
  }, { /* e_vex ext 131 */
    {INVALID, 0x66384618, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsravd, 0x66384618, "vpsravd", Vx, xx, Hx, Wx, xx, mrm|vex|reqp, x, tevexwb[128][0]},
    {EVEX_Wb_EXT, 0x66384618, "(evex_Wb ext 128)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 128},
  }, { /* e_vex ext 132 */
    {OP_pblendvb, 0x66381018, "pblendvb", Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp,x, END_LIST},
    {INVALID, 0x66381018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {PREFIX_EXT, 0x381018, "(prefix ext 177)", xx, xx, xx, xx, xx, mrm|evex, x, 177},
  }, { /* e_vex ext 133 */
    {INVALID, 0x66384518, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66384518, "(vex_W ext 72)", xx, xx, xx, xx, xx, mrm|vex|reqp, x, 72},
    {EVEX_Wb_EXT, 0x66384518, "(evex_Wb ext 129)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 129},
  }, { /* e_vex ext 134 */
    {INVALID, 0x66384718, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66384718, "(vex_W ext 73)", xx, xx, xx, xx, xx, mrm|vex|reqp, x, 73},
    {EVEX_Wb_EXT, 0x66384718, "(evex_Wb ext 131)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 131},
  }, { /* e_vex ext 135 */
    {INVALID, 0x66387818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastb, 0x66387818, "vpbroadcastb", Vx, xx, Wb_dq, xx, xx, mrm|vex|reqp, x, tvex[135][2]},
    {OP_vpbroadcastb, 0x66387808, "vpbroadcastb", Ve, xx, KEq, Wb_dq, xx, mrm|evex|reqp|ttt1s|inopsz1, x, t38[135]},
  }, { /* e_vex ext 136 */
    {INVALID, 0x66387918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastw, 0x66387918, "vpbroadcastw", Vx, xx, Ww_dq, xx, xx, mrm|vex|reqp, x, tvex[136][2]},
    {OP_vpbroadcastw, 0x66387908, "vpbroadcastw", Ve, xx, KEd, Ww_dq, xx, mrm|evex|reqp|ttt1s|inopsz2, x, t38[136]},
  }, { /* e_vex ext 137 */
    {INVALID, 0x66385818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastd, 0x66385818, "vpbroadcastd", Vx, xx, Wd_dq, xx, xx, mrm|vex|reqp, x, tvex[137][2]},
    {OP_vpbroadcastd, 0x66385808, "vpbroadcastd", Ve, xx, KEw, Wd_dq, xx, mrm|evex|reqp|ttt1s, x, tevexwb[151][0]},
  }, { /* e_vex ext 138 */
    {INVALID, 0x66385918, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpbroadcastq, 0x66385918, "vpbroadcastq", Vx, xx, Wq_dq, xx, xx, mrm|vex|reqp, x, tevexwb[152][2]},
    {EVEX_Wb_EXT, 0x66385918, "(evex_Wb ext 152)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 152},
  }, { /* e_vex ext 139 */
    {INVALID, 0x66385a18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vbroadcasti128, 0x66385a18, "vbroadcasti128", Vqq, xx, Mdq, xx, xx, mrm|vex|reqp, x, END_LIST},
    {EVEX_Wb_EXT, 0x66385a18, "(evex_Wb ext 153)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 153},
  }, { /* e_vex ext 140 */
    {INVALID, 0x389018, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389018, "(vex_W ext 66)", xx, xx, xx, xx, xx, mrm|vex, x, 66},
    {EVEX_Wb_EXT, 0x66389018, "(evex_Wb ext 189)", xx, xx, xx, xx, xx, mrm|evex, x, 189},
  }, { /* e_vex ext 141 */
    {INVALID, 0x389118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389118, "(vex_W ext 67)", xx, xx, xx, xx, xx, mrm|vex, x, 67},
    {EVEX_Wb_EXT, 0x66389118, "(evex_Wb ext 190)", xx, xx, xx, xx, xx, mrm|evex, x, 190},
  }, { /* e_vex ext 142 */
    {INVALID, 0x389118, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389218, "(vex_W ext 68)", xx, xx, xx, xx, xx, mrm|vex, x, 68},
    {EVEX_Wb_EXT, 0x66389218, "(evex_Wb ext 191)", xx, xx, xx, xx, xx, mrm|evex, x, 191},
  }, { /* e_vex ext 143 */
    {INVALID, 0x389318, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x66389318, "(vex_W ext 69)", xx, xx, xx, xx, xx, mrm|vex, x, 69},
    {EVEX_Wb_EXT, 0x66389318, "(evex_Wb ext 192)", xx, xx, xx, xx, xx, mrm|evex, x, 192},
  }, { /* e_vex ext 144 */
    {OP_sha1msg2, 0x38ca18, "sha1msg2", Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38ca18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638ca18, "(evex_Wb ext 134)", xx, xx, xx, xx, xx, mrm|reqp, x, 134},
  }, { /* e_vex ext 145 */
    {OP_sha1nexte, 0x38c818, "sha1nexte", Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38c818, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638c818, "(evex_Wb ext 185)", xx, xx, xx, xx, xx, mrm|reqp, x, 185},
  }, { /* e_vex ext 146 */
    {OP_sha256rnds2, 0x38cb18, "sha256rnds2", Vdq, xx, Wdq, xmm0, Vdq, mrm|reqp, x, END_LIST},
    {INVALID, 0x38cb18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638cb18, "(evex_Wb ext 135)", xx, xx, xx, xx, xx, mrm|reqp, x, 135},
  }, { /* e_vex ext 147 */
    {OP_sha256msg1, 0x38cc18, "sha256msg1", Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38cc18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638cc18, "(evex_Wb ext 179)", xx, xx, xx, xx, xx, mrm|reqp, x, 179},
  }, { /* e_vex ext 148 */
    {OP_sha256msg2, 0x38cd18, "sha256msg2", Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},
    {INVALID, 0x38cd18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {EVEX_Wb_EXT, 0x6638cd18, "(evex_Wb ext 180)", xx, xx, xx, xx, xx, mrm|reqp, x, 180},
  }, { /* e_vex ext 149 */
    {INVALID, 0x385008, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385008, "(vex_W ext 110)", xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 110},
    {EVEX_Wb_EXT, 0x385008, "(evex_Wb ext 267)", xx, xx, xx, xx, xx, mrm|reqp, x, 267},
  }, { /* e_vex ext 150 */
    {INVALID, 0x385108, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385108, "(vex_W ext 111)", xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 111},
    {EVEX_Wb_EXT, 0x385108, "(evex_Wb ext 268)", xx, xx, xx, xx, xx, mrm|reqp, x, 268},
  }, { /* e_vex ext 151 */
    {INVALID, 0x385208, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385208, "(vex_W ext 112)", xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 112},
    {EVEX_Wb_EXT, 0x385208, "(evex_Wb ext 269)", xx, xx, xx, xx, xx, mrm|reqp, x, 269},
  }, { /* e_vex ext 152 */
    {INVALID, 0x385308, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {VEX_W_EXT, 0x385308, "(vex_W ext 113)", xx, xx, xx, xx, xx, mrm|vex|reqp|ttfvm, x, 113},
    {EVEX_Wb_EXT, 0x385308, "(evex_Wb ext 270)", xx, xx, xx, xx, xx, mrm|reqp, x, 270},
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
    {OP_vmovss,  0xf30f1010, "vmovss",  Vdq, xx, Wss,  xx, xx, mrm|vex, x, modx[10][0]},
    {OP_vmovss,  0xf30f1010, "vmovss",  Vdq, xx, H12_dq, Uss, xx, mrm|vex, x, modx[10][1]},
  },
  { /* mod extension 9 */
    {OP_vmovsd,  0xf20f1010, "vmovsd",  Vdq, xx, Wsd,  xx, xx, mrm|vex, x, modx[11][0]},
    {OP_vmovsd,  0xf20f1010, "vmovsd",  Vdq, xx, Hsd, Usd, xx, mrm|vex, x, modx[11][1]},
  },
  { /* mod extension 10 */
    {OP_vmovss,  0xf30f1110, "vmovss",  Wss, xx, Vss,  xx, xx, mrm|vex, x, modx[ 8][1]},
    {OP_vmovss,  0xf30f1110, "vmovss",  Udq, xx, H12_dq, Vss, xx, mrm|vex, x, modx[20][0]},
  },
  { /* mod extension 11 */
    {OP_vmovsd,  0xf20f1110, "vmovsd",  Wsd, xx, Vsd,  xx, xx, mrm|vex, x, modx[ 9][1]},
    {OP_vmovsd,  0xf20f1110, "vmovsd",  Udq, xx, Hsd, Vsd, xx, mrm|vex, x, modx[21][0]},
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
    {E_VEX_EXT,    0x0fae32, "(e_vex ext 61)", xx, xx, xx, xx, xx, mrm, x, 61},
    {OP_wrfsbase,0xf30fae32, "wrfsbase", xx, xx, Ry, xx, xx, mrm|o64|reqp, x, END_LIST},
  },
  { /* mod extension 17 */
    {E_VEX_EXT,    0x0fae33, "(e_vex ext 62)", xx, xx, xx, xx, xx, mrm, x, 62},
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
  { /* mod extension 20 */
    {OP_vmovss,  0xf30f1000, "vmovss",  Vdq, xx, KE1b, Wss,  xx, mrm|evex|ttt1s, x, modx[22][0]},
    {OP_vmovss,  0xf30f1000, "vmovss",  Vdq, xx, KE1b, H12_dq, Uss, mrm|evex|ttnone, x, modx[22][1]},
  },
  { /* mod extension 21 */
    {OP_vmovsd,  0xf20f1040, "vmovsd",  Vdq, xx, KE1b, Wsd,  xx, mrm|evex|ttt1s, x, modx[23][0]},
    {OP_vmovsd,  0xf20f1040, "vmovsd",  Vdq, xx, KE1b, Hsd, Usd, mrm|evex|ttnone, x, modx[23][1]},
  },
  { /* mod extension 22 */
    {OP_vmovss,  0xf30f1100, "vmovss",  Wss, xx, KE1b, Vss,  xx, mrm|evex|ttt1s, x, modx[20][1]},
    {OP_vmovss,  0xf30f1100, "vmovss",  Udq, xx, KE1b, H12_dq, Vss, mrm|evex|ttnone, x, END_LIST},
  },
  { /* mod extension 23 */
    {OP_vmovsd,  0xf20f1140, "vmovsd",  Wsd, xx, KE1b, Vsd,  xx, mrm|evex|ttt1s, x, modx[21][1]},
    {OP_vmovsd,  0xf20f1140, "vmovsd",  Udq, xx, KE1b, Hsd, Vsd, mrm|evex|ttnone, x, END_LIST},
  },
  { /* mod extension 24 */
    {OP_vcvtps2qq, 0x660f7b10, "vcvtps2qq", Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[24][1]},
    {OP_vcvtps2qq, 0x660f7b10, "vcvtps2qq", Voq, xx, KEb, Uqq, xx, mrm|evex|er|tthv, x, END_LIST},
  },
  { /* mod extension 25 */
    {OP_vcvtpd2qq, 0x660f7b50, "vcvtpd2qq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[25][1]},
    {OP_vcvtpd2qq, 0x660f7b50, "vcvtpd2qq", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 26 */
    {OP_vcvtps2udq, 0x0f7910, "vcvtps2udq", Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[26][1]},
    {OP_vcvtps2udq, 0x0f7910, "vcvtps2udq", Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 27 */
    {OP_vcvtpd2udq, 0x0f7950, "vcvtpd2udq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[27][1]},
    {OP_vcvtpd2udq, 0x0f7950, "vcvtpd2udq", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 28 */
    {OP_vcvtps2uqq, 0x660f7910, "vcvtps2uqq", Ve, xx, KEw, Md, xx, mrm|evex|tthv, x, modx[28][1]},
    {OP_vcvtps2uqq, 0x660f7910, "vcvtps2uqq", Voq, xx, KEw, Uqq, xx, mrm|evex|er|tthv, x, END_LIST},
  },
  { /* mod extension 29 */
    {OP_vcvtpd2uqq, 0x660f7950, "vcvtpd2uqq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[29][1]},
    {OP_vcvtpd2uqq, 0x660f7950, "vcvtpd2uqq", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 30 */
    {OP_vcvttps2udq, 0x0f7810, "vcvttps2udq", Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[30][1]},
    {OP_vcvttps2udq, 0x0f7810, "vcvttps2udq", Voq, xx, KEw, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 31 */
    {OP_vcvttpd2udq, 0x0f7850, "vcvttpd2udq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[31][1]},
    {OP_vcvttpd2udq, 0x0f7850, "vcvttpd2udq", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 32 */
    {OP_vcvttps2qq, 0x660f7a10, "vcvttps2qq", Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[32][1]},
    {OP_vcvttps2qq, 0x660f7a10, "vcvttps2qq", Voq, xx, KEb, Uqq, xx, mrm|evex|sae|tthv, x, END_LIST},
  },
  { /* mod extension 33 */
    {OP_vcvttpd2qq,0x660f7a50, "vcvttpd2qq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[33][1]},
    {OP_vcvttpd2qq,0x660f7a50, "vcvttpd2qq", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 34 */
    {OP_vcvttps2uqq, 0x660f7810, "vcvttps2uqq", Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[34][1]},
    {OP_vcvttps2uqq, 0x660f7810, "vcvttps2uqq", Voq, xx, KEb, Uqq, xx, mrm|evex|sae|tthv, x, END_LIST},
  },
  { /* mod extension 35 */
    {OP_vcvttpd2uqq, 0x660f7850, "vcvttpd2uqq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[35][1]},
    {OP_vcvttpd2uqq, 0x660f7850, "vcvttpd2uqq", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 36 */
    {OP_vcvtdq2ps, 0x0f5b10, "vcvtdq2ps", Ves, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[36][1]},
    {OP_vcvtdq2ps, 0x0f5b10, "vcvtdq2ps", Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 37 */
    {OP_vcvtqq2ps, 0x0f5b50, "vcvtqq2ps", Ves, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[37][1]},
    {OP_vcvtqq2ps, 0x0f5b50, "vcvtqq2ps", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 38 */
    {OP_vcvtqq2pd, 0xf30fe650, "vcvtqq2pd", Ved, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[38][1]},
    {OP_vcvtqq2pd, 0xf30fe650, "vcvtqq2pd", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 39 */
    {OP_vcvtudq2ps, 0xf20f7a10, "vcvtudq2ps", Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[39][1]},
    {OP_vcvtudq2ps, 0xf20f7a10, "vcvtudq2ps", Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 40 */
    {OP_vcvtuqq2ps, 0xf20f7a50, "vcvtuqq2ps", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[40][1]},
    {OP_vcvtuqq2ps, 0xf20f7a50, "vcvtuqq2ps", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 41 */
    {OP_vcvtuqq2pd, 0xf30f7a50, "vcvtuqq2pd", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[41][1]},
    {OP_vcvtuqq2pd, 0xf30f7a50, "vcvtuqq2pd", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 42 */
    {OP_vfmadd132ps,0x66389818,"vfmadd132ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[15]},
    {OP_vfmadd132ps,0x66389818,"vfmadd132ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[16]},
  },
  { /* mod extension 43 */
    {OP_vfmadd132pd,0x66389858,"vfmadd132pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[18]},
    {OP_vfmadd132pd,0x66389858,"vfmadd132pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[19]},
  },
  { /* mod extension 44 */
    {OP_vfmadd213ps,0x6638a818,"vfmadd213ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[21]},
    {OP_vfmadd213ps,0x6638a818,"vfmadd213ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[22]},
  },
  { /* mod extension 45 */
    {OP_vfmadd213pd,0x6638a858,"vfmadd213pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[24]},
    {OP_vfmadd213pd,0x6638a858,"vfmadd213pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[25]},
  },
  { /* mod extension 46 */
    {OP_vfmadd231ps,0x6638b818,"vfmadd231ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[27]},
    {OP_vfmadd231ps,0x6638b818,"vfmadd231ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[28]},
  },
  { /* mod extension 47 */
    {OP_vfmadd231pd,0x6638b858,"vfmadd231pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[30]},
    {OP_vfmadd231pd,0x6638b858,"vfmadd231pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[31]},
  },
  { /* mod extension 48 */
    {OP_vfmaddsub132ps,0x66389618,"vfmaddsub132ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[45]},
    {OP_vfmaddsub132ps,0x66389618,"vfmaddsub132ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[46]},
  },
  { /* mod extension 49 */
    {OP_vfmaddsub132pd,0x66389658,"vfmaddsub132pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[48]},
    {OP_vfmaddsub132pd,0x66389658,"vfmaddsub132pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[49]},
  },
  { /* mod extension 50 */
    {OP_vfmaddsub213ps,0x6638a618,"vfmaddsub213ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[51]},
    {OP_vfmaddsub213ps,0x6638a618,"vfmaddsub213ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[52]},
  },
  { /* mod extension 51 */
    {OP_vfmaddsub213pd,0x6638a658,"vfmaddsub213pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[54]},
    {OP_vfmaddsub213pd,0x6638a658,"vfmaddsub213pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[55]},
  },
  { /* mod extension 52 */
    {OP_vfmaddsub231ps,0x6638b618,"vfmaddsub231ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[57]},
    {OP_vfmaddsub231ps,0x6638b618,"vfmaddsub231ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[58]},
  },
  { /* mod extension 53 */
    {OP_vfmaddsub231pd,0x6638b658,"vfmaddsub231pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[60]},
    {OP_vfmaddsub231pd,0x6638b658,"vfmaddsub231pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[61]},
  },
  { /* mod extension 54 */
    {OP_vfmsubadd132ps,0x66389718,"vfmsubadd132ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[63]},
    {OP_vfmsubadd132ps,0x66389718,"vfmsubadd132ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[64]},
  },
  { /* mod extension 55 */
    {OP_vfmsubadd132pd,0x66389758,"vfmsubadd132pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[66]},
    {OP_vfmsubadd132pd,0x66389758,"vfmsubadd132pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[67]},
  },
  { /* mod extension 56 */
    {OP_vfmsubadd213ps,0x6638a718,"vfmsubadd213ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[69]},
    {OP_vfmsubadd213ps,0x6638a718,"vfmsubadd213ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[70]},
  },
  { /* mod extension 57 */
    {OP_vfmsubadd213pd,0x6638a758,"vfmsubadd213pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[72]},
    {OP_vfmsubadd213pd,0x6638a758,"vfmsubadd213pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[73]},
  },
  { /* mod extension 58 */
    {OP_vfmsubadd231ps,0x6638b718,"vfmsubadd231ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[75]},
    {OP_vfmsubadd231ps,0x6638b718,"vfmsubadd231ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[76]},
  },
  { /* mod extension 59 */
    {OP_vfmsubadd231pd,0x6638b758,"vfmsubadd231pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[78]},
    {OP_vfmsubadd231pd,0x6638b758,"vfmsubadd231pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[79]},
  },
  { /* mod extension 60 */
    {OP_vfmsub132ps,0x66389a18,"vfmsub132ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[81]},
    {OP_vfmsub132ps,0x66389a18,"vfmsub132ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[82]},
  },
  { /* mod extension 61 */
    {OP_vfmsub132pd,0x66389a58,"vfmsub132pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[84]},
    {OP_vfmsub132pd,0x66389a58,"vfmsub132pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[85]},
  },
  { /* mod extension 62 */
    {OP_vfmsub213ps,0x6638aa18,"vfmsub213ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[87]},
    {OP_vfmsub213ps,0x6638aa18,"vfmsub213ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[88]},
  },
  { /* mod extension 63 */
    {OP_vfmsub213pd,0x6638aa58,"vfmsub213pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[90]},
    {OP_vfmsub213pd,0x6638aa58,"vfmsub213pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[91]},
  },
  { /* mod extension 64 */
    {OP_vfmsub231ps,0x6638ba18,"vfmsub231ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[93]},
    {OP_vfmsub231ps,0x6638ba18,"vfmsub231ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[94]},
  },
  { /* mod extension 65 */
    {OP_vfmsub231pd,0x6638ba58,"vfmsub231pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[96]},
    {OP_vfmsub231pd,0x6638ba58,"vfmsub231pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[97]},
  },
  { /* mod extension 66 */
    {OP_vfnmadd132ps,0x66389c18,"vfnmadd132ps",Ves,xx,KEb,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[111]},
    {OP_vfnmadd132ps,0x66389c18,"vfnmadd132ps",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[112]},
  },
  { /* mod extension 67 */
    {OP_vfnmadd132pd,0x66389c58,"vfnmadd132pd",Ved,xx,KEw,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[114]},
    {OP_vfnmadd132pd,0x66389c58,"vfnmadd132pd",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[115]},
  },
  { /* mod extension 68 */
    {OP_vfnmadd213ps,0x6638ac18,"vfnmadd213ps",Ves,xx,KEb,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[117]},
    {OP_vfnmadd213ps,0x6638ac18,"vfnmadd213ps",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[118]},
  },
  { /* mod extension 69 */
    {OP_vfnmadd213pd,0x6638ac58,"vfnmadd213pd",Ved,xx,KEw,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[120]},
    {OP_vfnmadd213pd,0x6638ac58,"vfnmadd213pd",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[121]},
  },
  { /* mod extension 70 */
    {OP_vfnmadd231ps,0x6638bc18,"vfnmadd231ps",Ves,xx,KEb,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[123]},
    {OP_vfnmadd231ps,0x6638bc18,"vfnmadd231ps",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[124]},
  },
  { /* mod extension 71 */
    {OP_vfnmadd231pd,0x6638bc58,"vfnmadd231pd",Ved,xx,KEw,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[126]},
    {OP_vfnmadd231pd,0x6638bc58,"vfnmadd231pd",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[127]},
  },
  { /* mod extension 72 */
    {OP_vfnmsub132ps,0x66389e18,"vfnmsub132ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[141]},
    {OP_vfnmsub132ps,0x66389e18,"vfnmsub132ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[142]},
  },
  { /* mod extension 73 */
    {OP_vfnmsub132pd,0x66389e58,"vfnmsub132pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[144]},
    {OP_vfnmsub132pd,0x66389e58,"vfnmsub132pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[145]},
  },
  { /* mod extension 74 */
    {OP_vfnmsub213ps,0x6638ae18,"vfnmsub213ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[147]},
    {OP_vfnmsub213ps,0x6638ae18,"vfnmsub213ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[148]},
  },
  { /* mod extension 75 */
    {OP_vfnmsub213pd,0x6638ae58,"vfnmsub213pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[150]},
    {OP_vfnmsub213pd,0x6638ae58,"vfnmsub213pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[151]},
  },
  { /* mod extension 76 */
    {OP_vfnmsub231ps,0x6638be18,"vfnmsub231ps",Ves,xx,KEw,Hes,Md,xop|mrm|evex|reqp|ttfv,x,exop[153]},
    {OP_vfnmsub231ps,0x6638be18,"vfnmsub231ps",Voq,xx,KEw,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[154]},
  },
  { /* mod extension 77 */
    {OP_vfnmsub231pd,0x6638be58,"vfnmsub231pd",Ved,xx,KEb,Hed,Mq,xop|mrm|evex|reqp|ttfv,x,exop[156]},
    {OP_vfnmsub231pd,0x6638be58,"vfnmsub231pd",Voq,xx,KEb,Hoq,Uoq,xop|mrm|evex|er|reqp|ttfv,x,exop[157]},
  },
  { /* mod extension 78 */
    {OP_vrcp28ps, 0x6638ca18, "vrcp28ps", Voq, xx, KEw, Md, xx, mrm|evex|reqp|ttfv,x,modx[78][1]},
    {OP_vrcp28ps, 0x6638ca18, "vrcp28ps", Voq, xx, KEw, Woq, xx, mrm|evex|sae|reqp|ttfv,x,END_LIST},
  },
  { /* mod extension 79 */
    {OP_vrcp28pd, 0x6638ca58, "vrcp28pd", Voq, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv,x,modx[79][1]},
    {OP_vrcp28pd, 0x6638ca58, "vrcp28pd", Voq, xx, KEb, Woq, xx, mrm|evex|sae|reqp|ttfv,x,END_LIST},
  },
  { /* mod extension 80 */
    {OP_vfixupimmps, 0x663a5418, "vfixupimmps", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[217]},
    {OP_vfixupimmps, 0x663a5418, "vfixupimmps", Voq, xx, KEw, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[218]},
  },
  { /* mod extension 81 */
    {OP_vfixupimmpd, 0x663a5458, "vfixupimmpd", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[220]},
    {OP_vfixupimmpd, 0x663a5458, "vfixupimmpd", Voq, xx, KEb, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[221]},
  },
  { /* mod extension 82 */
    {OP_vgetexpps, 0x66384218, "vgetexpps", Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, modx[82][1]},
    {OP_vgetexpps, 0x66384218, "vgetexpps", Voq, xx, KEw, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 83 */
    {OP_vgetexppd, 0x66384258, "vgetexppd", Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, modx[83][1]},
    {OP_vgetexppd, 0x66384258, "vgetexppd", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 84 */
    {OP_vgetmantps, 0x663a2618, "vgetmantps", Ve, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, modx[84][1]},
    {OP_vgetmantps, 0x663a2618, "vgetmantps", Voq, xx, KEw, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 85 */
    {OP_vgetmantpd, 0x663a2658, "vgetmantpd", Ve, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, modx[85][1]},
    {OP_vgetmantpd, 0x663a2658, "vgetmantpd", Voq, xx, KEb, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 86 */
    {OP_vrangeps, 0x663a5018, "vrangeps", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[231]},
    {OP_vrangeps, 0x663a5018, "vrangeps", Voq, xx, KEw, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[232]},
  },
  { /* mod extension 87 */
    {OP_vrangepd, 0x663a5058, "vrangepd", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[234]},
    {OP_vrangepd, 0x663a5058, "vrangepd", Voq, xx, KEb, Ib, Hoq, xop|mrm|evex|sae|reqp|ttfv, x, exop[235]},
  },
  { /* mod extension 88 */
    {OP_vreduceps, 0x663a5618, "vreduceps", Ve, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, modx[88][1]},
    {OP_vreduceps, 0x663a5618, "vreduceps", Voq, xx, KEw, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 89 */
    {OP_vreducepd, 0x663a5658, "vreducepd", Ve, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, modx[89][1]},
    {OP_vreducepd, 0x663a5658, "vreducepd", Voq, xx, KEb, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 90 */
    {OP_vrsqrt28ps, 0x6638cc18, "vrsqrt28ps", Voq, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, modx[90][1]},
    {OP_vrsqrt28ps, 0x6638cc18, "vrsqrt28ps", Voq, xx, KEw, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 91 */
    {OP_vrsqrt28pd, 0x6638cc58, "vrsqrt28pd", Voq, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, modx[91][1]},
    {OP_vrsqrt28pd, 0x6638cc58, "vrsqrt28pd", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 92 */
    {OP_vscalefps, 0x66382c18, "vscalefps", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, modx[92][1]},
    {OP_vscalefps, 0x66382c18, "vscalefps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 93 */
    {OP_vscalefpd, 0x66382c58, "vscalefpd", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, modx[93][1]},
    {OP_vscalefpd, 0x66382c58, "vscalefpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 94 */
    {OP_vexp2ps, 0x6638c818, "vexp2ps", Voq, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, modx[94][1]},
    {OP_vexp2ps, 0x6638c818, "vexp2ps", Voq, xx, KEw, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 95 */
    {OP_vexp2pd, 0x6638c858, "vexp2pd", Voq, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, modx[95][1]},
    {OP_vexp2pd, 0x6638c858, "vexp2pd", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 96 */
    {OP_vaddps, 0x0f5810, "vaddps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[96][1]},
    {OP_vaddps, 0x0f5810, "vaddps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 97 */
    {OP_vaddpd, 0x660f5850, "vaddpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[97][1]},
    {OP_vaddpd, 0x660f5850, "vaddpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 98 */
    {OP_vmulps, 0x0f5910, "vmulps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[98][1]},
    {OP_vmulps, 0x0f5910, "vmulps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 99 */
    {OP_vmulpd, 0x660f5950, "vmulpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[99][1]},
    {OP_vmulpd, 0x660f5950, "vmulpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 100 */
    {OP_vcvtps2pd, 0x0f5a10, "vcvtps2pd", Ved, xx, KEb, Md, xx, mrm|evex|tthv, x, modx[100][1]},
    {OP_vcvtps2pd, 0x0f5a10, "vcvtps2pd", Voq, xx, KEb, Uqq, xx, mrm|evex|sae|tthv, x, END_LIST},
  },
  { /* mod extension 101 */
    {OP_vcvtpd2ps, 0x660f5a50, "vcvtpd2ps", Ves, xx, KEw, Mq, xx, mrm|evex|ttfv, x, modx[101][1]},
    {OP_vcvtpd2ps, 0x660f5a50, "vcvtpd2ps", Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 102 */
    {OP_vsubps, 0x0f5c10, "vsubps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[102][1]},
    {OP_vsubps, 0x0f5c10, "vsubps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 103 */
    {OP_vsubpd, 0x660f5c50, "vsubpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[103][1]},
    {OP_vsubpd, 0x660f5c50, "vsubpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 104 */
    {OP_vminps, 0x0f5d10, "vminps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[104][1]},
    {OP_vminps, 0x0f5d10, "vminps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 105 */
    {OP_vminpd, 0x660f5d50, "vminpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[105][1]},
    {OP_vminpd, 0x660f5d50, "vminpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 106 */
    {OP_vdivps, 0x0f5e10, "vdivps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[106][1]},
    {OP_vdivps, 0x0f5e10, "vdivps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 107 */
    {OP_vdivpd, 0x660f5e50, "vdivpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[107][1]},
    {OP_vdivpd, 0x660f5e50, "vdivpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 108 */
    {OP_vmaxps,   0x0f5f10, "vmaxps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, modx[108][1]},
    {OP_vmaxps,   0x0f5f10, "vmaxps", Voq, xx, KEw, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 109 */
    {OP_vmaxpd, 0x660f5f50, "vmaxpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, modx[109][1]},
    {OP_vmaxpd, 0x660f5f50, "vmaxpd", Voq, xx, KEb, Hoq, Uoq, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 110 */
    {OP_vrndscaleps, 0x663a0818, "vrndscaleps",  Ve, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, modx[110][1]},
    {OP_vrndscaleps, 0x663a0818, "vrndscaleps",  Voq, xx, KEw, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 111 */
    {OP_vrndscalepd, 0x663a0958, "vrndscalepd",  Ve, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, modx[111][1]},
    {OP_vrndscalepd, 0x663a0958, "vrndscalepd",  Voq, xx, KEb, Ib, Uoq, mrm|evex|sae|reqp|ttfv, x, END_LIST},
  },
  { /* mod extension 112 */
    {OP_vcvtpd2dq, 0xf20fe650, "vcvtpd2dq",  Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[112][1]},
    {OP_vcvtpd2dq, 0xf20fe650, "vcvtpd2dq",  Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 113 */
    {OP_vcvttpd2dq,0x660fe650, "vcvttpd2dq", Ve, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[113][1]},
    {OP_vcvttpd2dq,0x660fe650, "vcvttpd2dq", Voq, xx, KEb, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 114 */
    {OP_vcmpps, 0x0fc210, "vcmpps", KPw, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[191]},
    {OP_vcmpps, 0x0fc210, "vcmpps", KPw, xx, KEw, Ib, Hoq, xop|mrm|evex|sae|ttfv, x, exop[192]},
  },
  { /* mod extension 115 */
    {OP_vcmppd, 0x660fc250, "vcmppd", KPb, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[198]},
    {OP_vcmppd, 0x660fc250, "vcmppd", KPb, xx, KEb, Ib, Hoq, xop|mrm|evex|sae|ttfv, x, exop[199]},
  },
  { /* mod extension 116 */
    {OP_vcvtps2dq, 0x660f5b10, "vcvtps2dq", Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[116][1]},
    {OP_vcvtps2dq, 0x660f5b10, "vcvtps2dq", Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 117 */
    {OP_vcvttps2dq, 0xf30f5b10, "vcvttps2dq", Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[117][1]},
    {OP_vcvttps2dq, 0xf30f5b10, "vcvttps2dq", Voq, xx, KEw, Uoq, xx, mrm|evex|sae|ttfv, x, END_LIST},
  },
  { /* mod extension 118 */
    {OP_vsqrtps,0x0f5110, "vsqrtps", Ves, xx, KEw, Md, xx, mrm|evex|ttfv, x, modx[118][1]},
    {OP_vsqrtps,0x660f5110, "vsqrtps", Voq, xx, KEw, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 119 */
    {OP_vsqrtpd,0x660f5150, "vsqrtpd", Ved, xx, KEb, Mq, xx, mrm|evex|ttfv, x, modx[119][1]},
    {OP_vsqrtpd,0x660f5150, "vsqrtpd", Voq, xx, KEb, Uoq, xx, mrm|evex|er|ttfv, x, END_LIST},
  },
  { /* mod extension 120 */
    {INVALID, 0x0f0135, "(bad)", xx, xx, xx, xx, xx, no, x, END_LIST},
    {RM_EXT,  0x0f0175, "(group 7 mod + rm ext 5)", xx, xx, xx, xx, xx, mrm, x, 5},
  },
};

/* Naturally all of these have modrm bytes even if they have no explicit operands */
const instr_info_t rm_extensions[][8] = {
  { /* rm extension 0 */
    {OP_enclv,    0xc00f0171, "enclv", eax, ebx, eax, ebx, ecx, mrm|xop, x, exop[0x100]},
    {OP_vmcall,   0xc10f0171, "vmcall",   xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmlaunch, 0xc20f0171, "vmlaunch", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmresume, 0xc30f0171, "vmresume", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_vmxoff,   0xc40f0171, "vmxoff",   xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },
  { /* rm extension 1 */
    /* XXX i#4013: Treat address in xax as IR memref? */
    {OP_monitor, 0xc80f0171, "monitor",  xx, xx, axAX, ecx, edx, mrm, x, END_LIST},
    {OP_mwait,   0xc90f0171, "mwait",  xx, xx, eax, ecx, xx, mrm, x, END_LIST},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_encls,  0xcf0f0171, "encls", eax, ebx, eax, ebx, ecx, mrm|xop, x, exop[0xfe]},
  },
  { /* rm extension 2 */
    {OP_swapgs, 0xf80f0177, "swapgs", xx, xx, xx, xx, xx, mrm|o64, x, END_LIST},
    {OP_rdtscp, 0xf90f0177, "rdtscp", edx, eax, xx, xx, xx, mrm|xop, x, exop[10]},/*AMD-only*/
    /* XXX i#4013: Treat address in xax as IR memref? */
    {OP_monitorx, 0xfa0f0177, "monitorx",  xx, xx, axAX, ecx, edx, mrm, x, END_LIST},/*AMD-only*/
    {OP_mwaitx, 0xfb0f0177, "mwaitx",  xx, xx, eax, ecx, xx, mrm, x, END_LIST},/*AMD-only*/
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
    {OP_enclu,  0xd70f0172, "enclu", eax, ebx, eax, ebx, ecx, mrm|xop, x, exop[0xff]},
  },
  { /* rm extension 5 */
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID,   0x0f0131, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_rdpkru, 0xee0f0171, "rdpkru", eax, edx, ecx, xx, xx, mrm, x, END_LIST},
    {OP_wrpkru, 0xef0f0171, "wrpkru", xx, xx, ecx, edx, eax, mrm, x, END_LIST},
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
* Instructions that differ depending on whether evex-encoded.
* Index 0 = no evex, 1 = evex
*/

const instr_info_t evex_prefix_extensions[][2] = {
  {   /* evex_prefix_ext */
    {OP_bound, 0x620000, "bound", xx, xx, Gv, Ma, xx, mrm|i64, x, END_LIST},
    {PREFIX,   0x620000, "(evex prefix)", xx, xx, xx, xx, xx, no, x, PREFIX_EVEX},
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
    {OP_nop,  0x900000, "nop", xx, xx, xx, xx, xx, no, x, tpe[103][3]},
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
    {OP_xchg, 0x900000, "xchg", eAX_x, eAX, eAX_x, eAX, xx, o64, x, END_LIST},
  },
};

/* Instructions that differ depending on whether rex.w in is present.
 * The table is indexed by rex.w: index 0 is for no rex.w.
 */
const instr_info_t rex_w_extensions[][2] = {
  { /* rex.w extension 0 */
    {OP_fxsave32, 0x0fae30, "fxsave",   Moq, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_fxsave64, 0x0fae30, "fxsave64", Moq, xx, xx, xx, xx, mrm|rex, x, END_LIST},
  },
  { /* rex.w extension 1 */
    {OP_fxrstor32, 0x0fae31, "fxrstor",   xx, xx, Moq, xx, xx, mrm, x, END_LIST},
    {OP_fxrstor64, 0x0fae31, "fxrstor64", xx, xx, Moq, xx, xx, mrm|rex, o64, END_LIST},
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
  { /* rex.w extension 5 */
    {OP_xsavec32, 0x0fc734, "xsavec",   Mxsave, xx, edx, eax, xx, mrm, x, END_LIST},
    {OP_xsavec64, 0x0fc734, "xsavec64", Mxsave, xx, edx, eax, xx, mrm|rex, o64, END_LIST},
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
  {E_VEX_EXT,  0x66381018, "(e_vex ext 132)", xx, xx, xx, xx, xx, mrm, x, 132},/*16*/
  {E_VEX_EXT,    0x381418, "(e_vex ext 130)", xx, xx, xx, xx, xx, mrm, x, 130},/*17*/
  {E_VEX_EXT,  0x66381518, "(e_vex ext 129)", xx, xx, xx, xx, xx, mrm, x, 129},/*18*/
  {E_VEX_EXT,  0x66381718, "(e_vex ext 3)", xx, xx, xx, xx, xx, mrm, x,  3},/*19*/
  /* 20 */
  {E_VEX_EXT,  0x66382018, "(e_vex ext  4)", xx, xx, xx, xx, xx, mrm, x,  4},/*20*/
  {E_VEX_EXT,  0x66382118, "(e_vex ext  5)", xx, xx, xx, xx, xx, mrm, x,  5},/*21*/
  {E_VEX_EXT,  0x66382218, "(e_vex ext  6)", xx, xx, xx, xx, xx, mrm, x,  6},/*22*/
  {E_VEX_EXT,  0x66382318, "(e_vex ext  7)", xx, xx, xx, xx, xx, mrm, x,  7},/*23*/
  {E_VEX_EXT,  0x66382418, "(e_vex ext  8)", xx, xx, xx, xx, xx, mrm, x,  8},/*24*/
  {E_VEX_EXT,  0x66382518, "(e_vex ext  9)", xx, xx, xx, xx, xx, mrm, x,  9},/*25*/
  {E_VEX_EXT,  0x66382818, "(e_vex ext 10)", xx, xx, xx, xx, xx, mrm, x, 10},/*26*/
  {E_VEX_EXT,  0x66382918, "(e_vex ext 11)", xx, xx, xx, xx, xx, mrm, x, 11},/*27*/
  {E_VEX_EXT,  0x66382a18, "(e_vex ext 12)", xx, xx, xx, xx, xx, mrm, x, 12},/*28*/
  {E_VEX_EXT,  0x66382b18, "(e_vex ext 13)", xx, xx, xx, xx, xx, mrm, x, 13},/*29*/
  /* 30 */
  {E_VEX_EXT,  0x66383018, "(e_vex ext 14)", xx, xx, xx, xx, xx, mrm, x, 14},/*30*/
  {E_VEX_EXT,  0x66383118, "(e_vex ext 15)", xx, xx, xx, xx, xx, mrm, x, 15},/*31*/
  {E_VEX_EXT,  0x66383218, "(e_vex ext 16)", xx, xx, xx, xx, xx, mrm, x, 16},/*32*/
  {E_VEX_EXT,  0x66383318, "(e_vex ext 17)", xx, xx, xx, xx, xx, mrm, x, 17},/*33*/
  {E_VEX_EXT,  0x66383418, "(e_vex ext 18)", xx, xx, xx, xx, xx, mrm, x, 18},/*34*/
  {E_VEX_EXT,  0x66383518, "(e_vex ext 19)", xx, xx, xx, xx, xx, mrm, x, 19},/*35*/
  {E_VEX_EXT,  0x66383718, "(e_vex ext 20)", xx, xx, xx, xx, xx, mrm, x, 20},/*36*/
  {E_VEX_EXT,  0x66383818, "(e_vex ext 21)", xx, xx, xx, xx, xx, mrm, x, 21},/*37*/
  {E_VEX_EXT,  0x66383918, "(e_vex ext 22)", xx, xx, xx, xx, xx, mrm, x, 22},/*38*/
  {E_VEX_EXT,  0x66383a18, "(e_vex ext 23)", xx, xx, xx, xx, xx, mrm, x, 23},/*39*/
  {E_VEX_EXT,  0x66383b18, "(e_vex ext 24)", xx, xx, xx, xx, xx, mrm, x, 24},/*40*/
  {E_VEX_EXT,  0x66383c18, "(e_vex ext 25)", xx, xx, xx, xx, xx, mrm, x, 25},/*41*/
  {E_VEX_EXT,  0x66383d18, "(e_vex ext 26)", xx, xx, xx, xx, xx, mrm, x, 26},/*42*/
  {E_VEX_EXT,  0x66383e18, "(e_vex ext 27)", xx, xx, xx, xx, xx, mrm, x, 27},/*43*/
  {E_VEX_EXT,  0x66383f18, "(e_vex ext 28)", xx, xx, xx, xx, xx, mrm, x, 28},/*44*/
  /* 40 */
  {E_VEX_EXT,  0x66384018, "(e_vex ext 29)", xx, xx, xx, xx, xx, mrm, x, 29},/*45*/
  {E_VEX_EXT,  0x66384118, "(e_vex ext 30)", xx, xx, xx, xx, xx, mrm, x, 30},/*46*/
  /* f0 */
  {PREFIX_EXT,  0x38f018,   "(prefix ext 138)", xx, xx, xx, xx, xx, mrm, x, 138},/*47*/
  {PREFIX_EXT,  0x38f118,   "(prefix ext 139)", xx, xx, xx, xx, xx, mrm, x, 139},/*48*/
  /* 80 */
  {OP_invept,   0x66388018, "invept",   xx, xx, Gr, Mdq, xx, mrm|reqp, x, END_LIST},/*49*/
  {OP_invvpid,  0x66388118, "invvpid",  xx, xx, Gr, Mdq, xx, mrm|reqp, x, END_LIST},/*50*/
  /* db-df */
  {E_VEX_EXT,  0x6638db18, "(e_vex ext 31)", xx, xx, xx, xx, xx, mrm, x, 31},/*51*/
  {E_VEX_EXT,  0x6638dc18, "(e_vex ext 32)", xx, xx, xx, xx, xx, mrm, x, 32},/*52*/
  {E_VEX_EXT,  0x6638dd18, "(e_vex ext 33)", xx, xx, xx, xx, xx, mrm, x, 33},/*53*/
  {E_VEX_EXT,  0x6638de18, "(e_vex ext 34)", xx, xx, xx, xx, xx, mrm, x, 34},/*54*/
  {E_VEX_EXT,  0x6638df18, "(e_vex ext 35)", xx, xx, xx, xx, xx, mrm, x, 35},/*55*/
  /* AVX */
  {E_VEX_EXT,  0x66380e18, "(e_vex ext 59)", xx, xx, xx, xx, xx, mrm, x, 59},/*56*/
  {E_VEX_EXT,  0x66380f18, "(e_vex ext 60)", xx, xx, xx, xx, xx, mrm, x, 60},/*57*/
  /* FMA 96-9f */
  {E_VEX_EXT,   0x389618,  "(e_vex ext 99)", xx, xx, xx, xx, xx, mrm, x, 99},/*58*/
  {E_VEX_EXT,   0x389718, "(e_vex ext 102)", xx, xx, xx, xx, xx, mrm, x, 102},/*59*/
  {E_VEX_EXT,   0x389818,  "(e_vex ext 93)", xx, xx, xx, xx, xx, mrm, x, 93},/*60*/
  {E_VEX_EXT,   0x389918,  "(e_vex ext 96)", xx, xx, xx, xx, xx, mrm, x, 96},/*61*/
  {E_VEX_EXT,   0x389a18, "(e_vex ext 105)", xx, xx, xx, xx, xx, mrm, x, 105},/*62*/
  {E_VEX_EXT,   0x389b18, "(e_vex ext 108)", xx, xx, xx, xx, xx, mrm, x, 108},/*63*/
  {E_VEX_EXT,   0x389c18, "(e_vex ext 111)", xx, xx, xx, xx, xx, mrm, x, 111},/*64*/
  {E_VEX_EXT,   0x389d18, "(e_vex ext 114)", xx, xx, xx, xx, xx, mrm, x, 114},/*65*/
  {E_VEX_EXT,   0x389e18, "(e_vex ext 117)", xx, xx, xx, xx, xx, mrm, x, 117},/*66*/
  {E_VEX_EXT,   0x389f18, "(e_vex ext 120)", xx, xx, xx, xx, xx, mrm, x, 120},/*67*/
  /* FMA a6-af */
  {E_VEX_EXT,   0x38a618, "(e_vex ext 100)", xx, xx, xx, xx, xx, mrm, x, 100},/*68*/
  {E_VEX_EXT,   0x38a718, "(e_vex ext 103)", xx, xx, xx, xx, xx, mrm, x, 103},/*69*/
  {E_VEX_EXT,   0x38a818,  "(e_vex ext 94)", xx, xx, xx, xx, xx, mrm, x, 94},/*70*/
  {E_VEX_EXT,   0x38a918,  "(e_vex ext 97)", xx, xx, xx, xx, xx, mrm, x, 97},/*71*/
  {E_VEX_EXT,   0x38aa18, "(e_vex ext 106)", xx, xx, xx, xx, xx, mrm, x, 106},/*72*/
  {E_VEX_EXT,   0x38ab18, "(e_vex ext 109)", xx, xx, xx, xx, xx, mrm, x, 109},/*73*/
  {E_VEX_EXT,   0x38ac18, "(e_vex ext 112)", xx, xx, xx, xx, xx, mrm, x, 112},/*74*/
  {E_VEX_EXT,   0x38ad18, "(e_vex ext 115)", xx, xx, xx, xx, xx, mrm, x, 115},/*75*/
  {E_VEX_EXT,   0x38ae18, "(e_vex ext 118)", xx, xx, xx, xx, xx, mrm, x, 118},/*76*/
  {E_VEX_EXT,   0x38af18, "(e_vex ext 121)", xx, xx, xx, xx, xx, mrm, x, 121},/*77*/
  /* FMA b6-bf */
  {E_VEX_EXT,   0x38b618, "(e_vex ext 101)", xx, xx, xx, xx, xx, mrm, x, 101},/*78*/
  {E_VEX_EXT,   0x38b718, "(e_vex ext 104)", xx, xx, xx, xx, xx, mrm, x, 104},/*79*/
  {E_VEX_EXT,   0x38b818,  "(e_vex ext 95)", xx, xx, xx, xx, xx, mrm, x, 95},/*80*/
  {E_VEX_EXT,   0x38b918,  "(e_vex ext 98)", xx, xx, xx, xx, xx, mrm, x, 98},/*81*/
  {E_VEX_EXT,   0x38ba18, "(e_vex ext 107)", xx, xx, xx, xx, xx, mrm, x, 107},/*82*/
  {E_VEX_EXT,   0x38bb18, "(e_vex ext 110)", xx, xx, xx, xx, xx, mrm, x, 110},/*83*/
  {E_VEX_EXT,   0x38bc18, "(e_vex ext 113)", xx, xx, xx, xx, xx, mrm, x, 113},/*84*/
  {E_VEX_EXT,   0x38bd18, "(e_vex ext 116)", xx, xx, xx, xx, xx, mrm, x, 116},/*85*/
  {E_VEX_EXT,   0x38be18, "(e_vex ext 119)", xx, xx, xx, xx, xx, mrm, x, 119},/*86*/
  {E_VEX_EXT,   0x38bf18, "(e_vex ext 122)", xx, xx, xx, xx, xx, mrm, x, 122},/*87*/
  /* AVX overlooked in original pass */
  {E_VEX_EXT, 0x66381318, "(e_vex ext 63)", xx, xx, xx, xx, xx, mrm, x, 63},/*88*/
  {E_VEX_EXT, 0x66381818, "(e_vex ext 64)", xx, xx, xx, xx, xx, mrm, x, 64},/*89*/
  {E_VEX_EXT, 0x66381918, "(e_vex ext 65)", xx, xx, xx, xx, xx, mrm, x, 65},/*90*/
  {E_VEX_EXT, 0x66381a18, "(e_vex ext 66)", xx, xx, xx, xx, xx, mrm, x, 66},/*91*/
  {E_VEX_EXT, 0x66382c18, "(e_vex ext 67)", xx, xx, xx, xx, xx, mrm, x, 67},/*92*/
  {E_VEX_EXT, 0x66382d18, "(e_vex ext 68)", xx, xx, xx, xx, xx, mrm, x, 68},/*93*/
  {E_VEX_EXT, 0x66382e18, "(e_vex ext 69)", xx, xx, xx, xx, xx, mrm, x, 69},/*94*/
  {E_VEX_EXT, 0x66382f18, "(e_vex ext 70)", xx, xx, xx, xx, xx, mrm, x, 70},/*95*/
  {E_VEX_EXT, 0x66380c18, "(e_vex ext 77)", xx, xx, xx, xx, xx, mrm, x, 77},/*96*/
  {E_VEX_EXT, 0x66380d18, "(e_vex ext 78)", xx, xx, xx, xx, xx, mrm, x, 78},/*97*/
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
  {E_VEX_EXT,   0x389018, "(e_vex ext 140)", xx, xx, xx, xx, xx, mrm, x, 140},/*104*/
  {E_VEX_EXT,   0x389118, "(e_vex ext 141)", xx, xx, xx, xx, xx, mrm, x, 141},/*105*/
  {E_VEX_EXT,   0x389218, "(e_vex ext 142)", xx, xx, xx, xx, xx, mrm, x, 142},/*106*/
  {E_VEX_EXT,   0x389318, "(e_vex ext 143)", xx, xx, xx, xx, xx, mrm, x, 143},/*107*/
  {E_VEX_EXT, 0x66385a18, "(e_vex ext 139)", xx, xx, xx, xx, xx, mrm, x, 139},/*108*/
  {VEX_W_EXT, 0x66388c18, "(vex_W ext 70)", xx,xx,xx,xx,xx, mrm|vex|reqp, x, 70},/*109*/
  {VEX_W_EXT, 0x66388e18, "(vex_W ext 71)", xx,xx,xx,xx,xx, mrm|vex|reqp, x, 71},/*110*/
  /* Following Intel and not marking as packed float vs ints: just "qq". */
  {E_VEX_EXT, 0x66381618, "(e_vex ext 124)", xx, xx, xx, xx, xx, mrm|reqp, x, 124},/*111*/
  {E_VEX_EXT, 0x66383618, "(e_vex ext 123)", xx, xx, xx, xx, xx, mrm|reqp, x, 123},/*112*/
  {E_VEX_EXT, 0x66384518, "(e_vex ext 133)", xx, xx, xx, xx, xx, mrm|reqp, x, 133},/*113*/
  {E_VEX_EXT, 0x66384618, "(e_vex ext 131)", xx, xx, xx, xx, xx, mrm|reqp, x, 131},/*114*/
  {E_VEX_EXT, 0x66384718, "(e_vex ext 134)", xx, xx, xx, xx, xx, mrm|reqp, x, 134},/*115*/
  {E_VEX_EXT, 0x66387818, "(e_vex ext 135)", xx, xx, xx, xx, xx, mrm|reqp, x, 135},/*116*/
  {E_VEX_EXT, 0x66387918, "(e_vex ext 136)", xx, xx, xx, xx, xx, mrm|reqp, x, 136},/*117*/
  {E_VEX_EXT, 0x66385818, "(e_vex ext 137)", xx, xx, xx, xx, xx, mrm|reqp, x, 137},/*118*/
  {E_VEX_EXT, 0x66385918, "(e_vex ext 138)", xx, xx, xx, xx, xx, mrm|reqp, x, 138},/*119*/
  {EVEX_Wb_EXT, 0x66388d18, "(evex_Wb ext 92)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 92},/*120*/
  {EVEX_Wb_EXT, 0x66387718, "(evex_Wb ext 95)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 95},/*121*/
  {EVEX_Wb_EXT, 0x66387618, "(evex_Wb ext 96)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 96},/*122*/
  {EVEX_Wb_EXT, 0x66387518, "(evex_Wb ext 97)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 97},/*123*/
  {EVEX_Wb_EXT, 0x66387d18, "(evex_Wb ext 98)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 98},/*124*/
  {EVEX_Wb_EXT, 0x66387e18, "(evex_Wb ext 99)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 99},/*125*/
  {EVEX_Wb_EXT, 0x66387f18, "(evex_Wb ext 100)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 100},/*126*/
  {PREFIX_EXT, 0x381118, "(prefix ext 171)", xx, xx, xx, xx, xx, mrm, x, 171},  /*127*/
  {PREFIX_EXT, 0x381218, "(prefix ext 162)", xx, xx, xx, xx, xx, mrm, x, 162},  /*128*/
  {EVEX_Wb_EXT, 0x66384c18, "(evex_Wb ext 132)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 132},/*129*/
  {EVEX_Wb_EXT, 0x66384d18, "(evex_Wb ext 133)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 133},/*130*/
  {E_VEX_EXT, 0x38ca18, "(e_vex ext 144)", xx, xx, xx, xx, xx, mrm|reqp, x, 144},/*131*/
  {E_VEX_EXT, 0x38cb18, "(e_vex ext 146)", xx, xx, xx, xx, xx, mrm|reqp, x, 146},/*132*/
  {EVEX_Wb_EXT, 0x66381f58, "(evex_Wb ext 147)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 147},/*133*/
  {EVEX_Wb_EXT, 0x66381b18, "(evex_Wb ext 150)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 150},/*134*/
  {OP_vpbroadcastb, 0x66387a08, "vpbroadcastb", Ve, xx, KEq, Ed, xx, mrm|evex|reqp|ttt1s, x, END_LIST},/*135*/
  {OP_vpbroadcastw, 0x66387b08, "vpbroadcastw", Ve, xx, KEd, Ed, xx, mrm|evex|reqp|ttt1s|inopsz2, x, END_LIST},/*136*/
  {EVEX_Wb_EXT, 0x66387c18, "(evex_Wb ext 151)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 151},/*137*/
  {EVEX_Wb_EXT, 0x66385b18, "(evex_Wb ext 154)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 154},/*138*/
  {EVEX_Wb_EXT, 0x66386518, "(evex_Wb ext 156)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 156},/*139*/
  {EVEX_Wb_EXT, 0x66388a18, "(evex_Wb ext 157)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 157},/*140*/
  {EVEX_Wb_EXT, 0x66388818, "(evex_Wb ext 158)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 158},/*141*/
  {EVEX_Wb_EXT, 0x66384218, "(evex_Wb ext 161)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 161},/*142*/
  {EVEX_Wb_EXT, 0x66384318, "(evex_Wb ext 162)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 162},/*143*/
  {EVEX_Wb_EXT, 0x66386618, "(evex_Wb ext 165)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 165},/*144*/
  {EVEX_Wb_EXT, 0x66386418, "(evex_Wb ext 166)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 166},/*145*/
  {EVEX_Wb_EXT, 0x66388b18, "(evex_Wb ext 167)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 167},/*146*/
  {EVEX_Wb_EXT, 0x66388918, "(evex_Wb ext 168)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 168},/*147*/
  {PREFIX_EXT, 0x382618, "(prefix ext 182)", xx, xx, xx, xx, xx, mrm, x, 182},/*148*/
  {PREFIX_EXT, 0x382718, "(prefix ext 183)", xx, xx, xx, xx, xx, mrm, x, 183},/*149*/
  {EVEX_Wb_EXT, 0x66384e18, "(evex_Wb ext 177)", xx, xx, xx, xx, xx, mrm|reqp, x, 177},/*150*/
  {EVEX_Wb_EXT, 0x66384f18, "(evex_Wb ext 178)", xx, xx, xx, xx, xx, mrm|reqp, x, 178},/*151*/
  {E_VEX_EXT, 0x38cc18, "(e_vex ext 147)", xx, xx, xx, xx, xx, mrm|reqp, x, 147},/*152*/
  {E_VEX_EXT, 0x38cd18, "(e_vex ext 148)", xx, xx, xx, xx, xx, mrm|reqp, x, 148},/*153*/
  {E_VEX_EXT, 0x38c818, "(e_vex ext 145)", xx, xx, xx, xx, xx, mrm|reqp, x, 145},/*154*/
  {EVEX_Wb_EXT, 0x6638c418, "(evex_Wb ext 186)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 186},/*155*/
  {EVEX_Wb_EXT, 0x66384418, "(evex_Wb ext 187)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 187},/*156*/
  {EVEX_Wb_EXT, 0x6638b448, "(evex_Wb ext 220)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 220},/*157*/
  {EVEX_Wb_EXT, 0x6638b548, "(evex_Wb ext 234)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 234},/*158*/
  {EVEX_Wb_EXT, 0x6638a018, "(evex_Wb ext 193)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 193},/*159*/
  {EVEX_Wb_EXT, 0x6638a118, "(evex_Wb ext 194)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 194},/*160*/
  {EVEX_Wb_EXT, 0x6638a218, "(evex_Wb ext 195)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 195},/*161*/
  {EVEX_Wb_EXT, 0x6638a318, "(evex_Wb ext 196)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 196},/*162*/
  {EXTENSION, 0x6638c618, "group 18", xx, xx, xx, xx, xx, mrm, x, 32},/*163*/
  {EXTENSION, 0x6638c718, "group 19", xx, xx, xx, xx, xx, mrm, x, 33},/*164*/
  {OP_sha1msg1, 0x38c918, "sha1msg1", Vdq, xx, Wdq, Vdq, xx, mrm|reqp, x, END_LIST},/*165*/
  {E_VEX_EXT, 0x66385008, "(e_vex ext 149)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 149},/*166*/
  {E_VEX_EXT, 0x66385108, "(e_vex ext 150)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 150},/*167*/
  {PREFIX_EXT, 0x385208, "(prefix ext 189)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 189},/*168*/
  {E_VEX_EXT, 0x66385308, "(e_vex ext 152)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 152},/*169*/
  {PREFIX_EXT, 0x387208, "(prefix ext 190)", xx, xx, xx, xx, xx, mrm|evex, x, 190},/*170*/
  /* AVX512 VPOPCNTDQ */
  {EVEX_Wb_EXT, 0x66385518, "(evex_Wb ext 274)", xx, xx, xx, xx, xx, mrm|evex|reqp, x, 274}/*171*/
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
  {INVALID,     0x3aff18, "(bad)", xx, xx, xx, xx, xx, no, x, NA},                 /* 0*/
  /**** SSSE3 ****/
  {PREFIX_EXT,  0x3a0f18, "(prefix ext 133)", xx, xx, xx, xx, xx, mrm, x, 133},    /* 1*/
  /**** SSE4 ****/
  {E_VEX_EXT,  0x663a1418, "(e_vex ext 36)", xx, xx, xx, xx, xx, mrm, x, 36},/* 2*/
  {E_VEX_EXT,  0x663a1518, "(e_vex ext 37)", xx, xx, xx, xx, xx, mrm, x, 37},/* 3*/
  {E_VEX_EXT,  0x663a1618, "(e_vex ext 38)", xx, xx, xx, xx, xx, mrm, x, 38},/* 4*/
  {E_VEX_EXT,  0x663a1718, "(e_vex ext 39)", xx, xx, xx, xx, xx, mrm, x, 39},/* 5*/
  {E_VEX_EXT,  0x663a0818, "(e_vex ext 40)", xx, xx, xx, xx, xx, mrm, x, 40},/* 6*/
  {E_VEX_EXT,  0x663a0918, "(e_vex ext 41)", xx, xx, xx, xx, xx, mrm, x, 41},/* 7*/
  {E_VEX_EXT,  0x663a0a18, "(e_vex ext 42)", xx, xx, xx, xx, xx, mrm, x, 42},/* 8*/
  {E_VEX_EXT,  0x663a0b18, "(e_vex ext 43)", xx, xx, xx, xx, xx, mrm, x, 43},/* 9*/
  {E_VEX_EXT,  0x663a0c18, "(e_vex ext 44)", xx, xx, xx, xx, xx, mrm, x, 44},/*10*/
  {E_VEX_EXT,  0x663a0d18, "(e_vex ext 45)", xx, xx, xx, xx, xx, mrm, x, 45},/*11*/
  {E_VEX_EXT,  0x663a0e18, "(e_vex ext 46)", xx, xx, xx, xx, xx, mrm, x, 46},/*12*/
  /* 20 */
  {E_VEX_EXT,  0x663a2018, "(e_vex ext 47)", xx, xx, xx, xx, xx, mrm, x, 47},/*13*/
  {E_VEX_EXT,  0x663a2118, "(e_vex ext 48)", xx, xx, xx, xx, xx, mrm, x, 48},/*14*/
  {E_VEX_EXT,  0x663a2218, "(e_vex ext 49)", xx, xx, xx, xx, xx, mrm, x, 49},/*15*/
  /* 40 */
  {E_VEX_EXT,  0x663a4018, "(e_vex ext 50)", xx, xx, xx, xx, xx, mrm, x, 50},/*16*/
  {E_VEX_EXT,  0x663a4118, "(e_vex ext 51)", xx, xx, xx, xx, xx, mrm, x, 51},/*17*/
  {E_VEX_EXT,  0x663a4218, "(e_vex ext 52)", xx, xx, xx, xx, xx, mrm, x, 52},/*18*/
  /* 60 */
  {E_VEX_EXT,  0x663a6018, "(e_vex ext 53)", xx, xx, xx, xx, xx, mrm, x, 53},/*19*/
  {E_VEX_EXT,  0x663a6118, "(e_vex ext 54)", xx, xx, xx, xx, xx, mrm, x, 54},/*20*/
  {E_VEX_EXT,  0x663a6218, "(e_vex ext 55)", xx, xx, xx, xx, xx, mrm, x, 55},/*21*/
  {E_VEX_EXT,  0x663a6318, "(e_vex ext 56)", xx, xx, xx, xx, xx, mrm, x, 56},/*22*/
  {E_VEX_EXT,  0x663a4418, "(e_vex ext 57)", xx, xx, xx, xx, xx, mrm, x, 57},/*23*/
  {E_VEX_EXT,  0x663adf18, "(e_vex ext 58)", xx, xx, xx, xx, xx, mrm, x, 58},/*24*/
  /* AVX overlooked in original pass */
  {E_VEX_EXT,  0x663a4a18, "(e_vex ext  0)", xx, xx, xx, xx, xx, mrm, x,  0},/*25*/
  {E_VEX_EXT,  0x663a4b18, "(e_vex ext  1)", xx, xx, xx, xx, xx, mrm, x,  1},/*26*/
  {E_VEX_EXT,  0x663a4c18, "(e_vex ext  2)", xx, xx, xx, xx, xx, mrm, x,  2},/*27*/
  {E_VEX_EXT,  0x663a0418, "(e_vex ext 71)", xx, xx, xx, xx, xx, mrm, x, 71},/*28*/
  {E_VEX_EXT,  0x663a0518, "(e_vex ext 72)", xx, xx, xx, xx, xx, mrm, x, 72},/*29*/
  {E_VEX_EXT,  0x663a0618, "(e_vex ext 73)", xx, xx, xx, xx, xx, mrm, x, 73},/*30*/
  {E_VEX_EXT,  0x663a1818, "(e_vex ext 74)", xx, xx, xx, xx, xx, mrm, x, 74},/*31*/
  {E_VEX_EXT,  0x663a1918, "(e_vex ext 75)", xx, xx, xx, xx, xx, mrm, x, 75},/*32*/
  {E_VEX_EXT,  0x663a1d18, "(e_vex ext 76)", xx, xx, xx, xx, xx, mrm, x, 76},/*33*/
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
  {E_VEX_EXT, 0x663a3818, "(e_vex ext 128)", xx, xx, xx, xx, xx, mrm, x, 128},/*57*/
  {E_VEX_EXT, 0x663a3918, "(e_vex ext 127)", xx, xx, xx, xx, xx, mrm, x, 127},/*58*/
  {E_VEX_EXT, 0x663a0058, "(e_vex ext 125)", xx, xx, xx, xx, xx, mrm, x, 125},/*59*/
  /* Following Intel and not marking as packed float vs ints: just "qq". */
  {E_VEX_EXT, 0x663a0158, "(e_vex ext 126)", xx, xx, xx, xx, xx, mrm, x, 126},/*60*/
  {OP_vpblendd,0x663a0218,"vpblendd",Vx,xx,Hx,Wx,Ib, mrm|vex|reqp,x,END_LIST},/*61*/
  {OP_vperm2i128,0x663a4618,"vperm2i128",Vqq,xx,Hqq,Wqq,Ib, mrm|vex|reqp,x,END_LIST},/*62*/
  /* AVX-512 */
  {VEX_W_EXT, 0x663a3010, "(vex_W ext 102)", xx, xx, xx, xx, xx, mrm|reqp, x, 102 },/*63*/
  {VEX_W_EXT, 0x663a3110, "(vex_W ext 103)", xx, xx, xx, xx, xx, mrm|reqp, x, 103 },/*64*/
  {VEX_W_EXT, 0x663a3210, "(vex_W ext 100)", xx, xx, xx, xx, xx, mrm|reqp, x, 100 },/*65*/
  {VEX_W_EXT, 0x663a3310, "(vex_W ext 101)", xx, xx, xx, xx, xx, mrm|reqp, x, 101 },/*66*/
  {EVEX_Wb_EXT, 0x663a1b18, "(evex_Wb ext 102)", xx, xx, xx, xx, xx, mrm, x, 102},/*67*/
  {EVEX_Wb_EXT, 0x663a3b18, "(evex_Wb ext 104)", xx, xx, xx, xx, xx, mrm, x, 104},/*68*/
  {EVEX_Wb_EXT, 0x663a1a18, "(evex_Wb ext 106)", xx, xx, xx, xx, xx, mrm, x, 106},/*69*/
  {EVEX_Wb_EXT, 0x663a3a18, "(evex_Wb ext 108)", xx, xx, xx, xx, xx, mrm, x, 108},/*70*/
  {EVEX_Wb_EXT, 0x663a3e18, "(evex_Wb ext 109)", xx, xx, xx, xx, xx, mrm, x, 109},/*71*/
  {EVEX_Wb_EXT, 0x663a3f18, "(evex_Wb ext 110)", xx, xx, xx, xx, xx, mrm, x, 110},/*72*/
  {EVEX_Wb_EXT, 0x663a1e18, "(evex_Wb ext 111)", xx, xx, xx, xx, xx, mrm, x, 111},/*73*/
  {EVEX_Wb_EXT, 0x663a1f18, "(evex_Wb ext 112)", xx, xx, xx, xx, xx, mrm, x, 112},/*74*/
  {EVEX_Wb_EXT, 0x663a2318, "(evex_Wb ext 142)", xx, xx, xx, xx, xx, mrm, x, 142},/*75*/
  {EVEX_Wb_EXT, 0x663a4318, "(evex_Wb ext 143)", xx, xx, xx, xx, xx, mrm, x, 143},/*76*/
  {EVEX_Wb_EXT, 0x663a0318, "(evex_Wb ext 155)", xx, xx, xx, xx, xx, mrm, x, 155},/*77*/
  {EVEX_Wb_EXT, 0x663a5418, "(evex_Wb ext 159)", xx, xx, xx, xx, xx, mrm, x, 159},/*78*/
  {EVEX_Wb_EXT, 0x663a5518, "(evex_Wb ext 160)", xx, xx, xx, xx, xx, mrm, x, 160},/*79*/
  {EVEX_Wb_EXT, 0x663a2618, "(evex_Wb ext 163)", xx, xx, xx, xx, xx, mrm, x, 163},/*80*/
  {EVEX_Wb_EXT, 0x663a2718, "(evex_Wb ext 164)", xx, xx, xx, xx, xx, mrm, x, 164},/*81*/
  {EVEX_Wb_EXT, 0x663a5018, "(evex_Wb ext 173)", xx, xx, xx, xx, xx, mrm, x, 173},/*82*/
  {EVEX_Wb_EXT, 0x663a5118, "(evex_Wb ext 174)", xx, xx, xx, xx, xx, mrm, x, 174},/*83*/
  {EVEX_Wb_EXT, 0x663a5618, "(evex_Wb ext 175)", xx, xx, xx, xx, xx, mrm, x, 175},/*84*/
  {EVEX_Wb_EXT, 0x663a5718, "(evex_Wb ext 176)", xx, xx, xx, xx, xx, mrm, x, 176},/*85*/
  {EVEX_Wb_EXT, 0x663a6618, "(evex_Wb ext 183)", xx, xx, xx, xx, xx, mrm, x, 183},/*86*/
  {EVEX_Wb_EXT, 0x663a6718, "(evex_Wb ext 184)", xx, xx, xx, xx, xx, mrm, x, 184},/*87*/
  {EVEX_Wb_EXT, 0x663a2518, "(evex_Wb ext 188)", xx, xx, xx, xx, xx, mrm, x, 188},/*88*/
  /* SHA */
  {OP_sha1rnds4, 0x3acc18, "sha1rnds4", Vdq, xx, Wdq, Ib, Vdq, mrm|reqp, x, END_LIST},/*89*/
};

/****************************************************************************
 * Instructions that differ depending on vex.W
 * Index is vex.W value
 */
const instr_info_t vex_W_extensions[][2] = {
  {    /* vex_W_ext 0 */
    {OP_vfmadd132ps,0x66389818,"vfmadd132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[62][0]},
    {OP_vfmadd132pd,0x66389858,"vfmadd132pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[62][2]},
  }, { /* vex_W_ext 1 */
    {OP_vfmadd213ps,0x6638a818,"vfmadd213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[63][0]},
    {OP_vfmadd213pd,0x6638a858,"vfmadd213pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[63][2]},
  }, { /* vex_W_ext 2 */
    {OP_vfmadd231ps,0x6638b818,"vfmadd231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[64][0]},
    {OP_vfmadd231pd,0x6638b858,"vfmadd231pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[64][2]},
  }, { /* vex_W_ext 3 */
    {OP_vfmadd132ss,0x66389918,"vfmadd132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[65][0]},
    {OP_vfmadd132sd,0x66389958,"vfmadd132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[65][2]},
  }, { /* vex_W_ext 4 */
    {OP_vfmadd213ss,0x6638a918,"vfmadd213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[66][0]},
    {OP_vfmadd213sd,0x6638a958,"vfmadd213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[66][2]},
  }, { /* vex_W_ext 5 */
    {OP_vfmadd231ss,0x6638b918,"vfmadd231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[67][0]},
    {OP_vfmadd231sd,0x6638b958,"vfmadd231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[67][2]},
  }, { /* vex_W_ext 6 */
    {OP_vfmaddsub132ps,0x66389618,"vfmaddsub132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[68][0]},
    {OP_vfmaddsub132pd,0x66389658,"vfmaddsub132pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[68][2]},
  }, { /* vex_W_ext 7 */
    {OP_vfmaddsub213ps,0x6638a618,"vfmaddsub213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[69][0]},
    {OP_vfmaddsub213pd,0x6638a658,"vfmaddsub213pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[69][2]},
  }, { /* vex_W_ext 8 */
    {OP_vfmaddsub231ps,0x6638b618,"vfmaddsub231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[70][0]},
    {OP_vfmaddsub231pd,0x6638b658,"vfmaddsub231pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[70][2]},
  }, { /* vex_W_ext 9 */
    {OP_vfmsubadd132ps,0x66389718,"vfmsubadd132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[71][0]},
    {OP_vfmsubadd132pd,0x66389758,"vfmsubadd132pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[71][2]},
  }, { /* vex_W_ext 10 */
    {OP_vfmsubadd213ps,0x6638a718,"vfmsubadd213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[72][0]},
    {OP_vfmsubadd213pd,0x6638a758,"vfmsubadd213pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[72][2]},
  }, { /* vex_W_ext 11 */
    {OP_vfmsubadd231ps,0x6638b718,"vfmsubadd231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[73][0]},
    {OP_vfmsubadd231pd,0x6638b758,"vfmsubadd231pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[73][2]},
  }, { /* vex_W_ext 12 */
    {OP_vfmsub132ps,0x66389a18,"vfmsub132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[74][0]},
    {OP_vfmsub132pd,0x66389a58,"vfmsub132pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[74][2]},
  }, { /* vex_W_ext 13 */
    {OP_vfmsub213ps,0x6638aa18,"vfmsub213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[75][0]},
    {OP_vfmsub213pd,0x6638aa58,"vfmsub213pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[75][2]},
  }, { /* vex_W_ext 14 */
    {OP_vfmsub231ps,0x6638ba18,"vfmsub231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[76][0]},
    {OP_vfmsub231pd,0x6638ba58,"vfmsub231pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[76][2]},
  }, { /* vex_W_ext 15 */
    {OP_vfmsub132ss,0x66389b18,"vfmsub132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[77][0]},
    {OP_vfmsub132sd,0x66389b58,"vfmsub132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[77][2]},
  }, { /* vex_W_ext 16 */
    {OP_vfmsub213ss,0x6638ab18,"vfmsub213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[78][0]},
    {OP_vfmsub213sd,0x6638ab58,"vfmsub213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[78][2]},
  }, { /* vex_W_ext 17 */
    {OP_vfmsub231ss,0x6638bb18,"vfmsub231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[79][0]},
    {OP_vfmsub231sd,0x6638bb58,"vfmsub231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[79][2]},
  }, { /* vex_W_ext 18 */
    {OP_vfnmadd132ps,0x66389c18,"vfnmadd132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[80][0]},
    {OP_vfnmadd132pd,0x66389c58,"vfnmadd132pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[80][2]},
  }, { /* vex_W_ext 19 */
    {OP_vfnmadd213ps,0x6638ac18,"vfnmadd213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[81][0]},
    {OP_vfnmadd213pd,0x6638ac58,"vfnmadd213pd",Vvd,xx,Hvd,Wvd,Vvs,mrm|vex|reqp,x,tevexwb[81][2]},
  }, { /* vex_W_ext 20 */
    {OP_vfnmadd231ps,0x6638bc18,"vfnmadd231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[82][0]},
    {OP_vfnmadd231pd,0x6638bc58,"vfnmadd231pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[82][2]},
  }, { /* vex_W_ext 21 */
    {OP_vfnmadd132ss,0x66389d18,"vfnmadd132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[83][0]},
    {OP_vfnmadd132sd,0x66389d58,"vfnmadd132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[83][2]},
  }, { /* vex_W_ext 22 */
    {OP_vfnmadd213ss,0x6638ad18,"vfnmadd213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[84][0]},
    {OP_vfnmadd213sd,0x6638ad58,"vfnmadd213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[84][2]},
  }, { /* vex_W_ext 23 */
    {OP_vfnmadd231ss,0x6638bd18,"vfnmadd231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[85][0]},
    {OP_vfnmadd231sd,0x6638bd58,"vfnmadd231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[85][2]},
  }, { /* vex_W_ext 24 */
    {OP_vfnmsub132ps,0x66389e18,"vfnmsub132ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[86][0]},
    {OP_vfnmsub132pd,0x66389e58,"vfnmsub132pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[86][2]},
  }, { /* vex_W_ext 25 */
    {OP_vfnmsub213ps,0x6638ae18,"vfnmsub213ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[87][0]},
    {OP_vfnmsub213pd,0x6638ae58,"vfnmsub213pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[87][2]},
  }, { /* vex_W_ext 26 */
    {OP_vfnmsub231ps,0x6638be18,"vfnmsub231ps",Vvs,xx,Hvs,Wvs,Vvs,mrm|vex|reqp,x,tevexwb[88][0]},
    {OP_vfnmsub231pd,0x6638be58,"vfnmsub231pd",Vvd,xx,Hvd,Wvd,Vvd,mrm|vex|reqp,x,tevexwb[88][2]},
  }, { /* vex_W_ext 27 */
    {OP_vfnmsub132ss,0x66389f18,"vfnmsub132ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[89][0]},
    {OP_vfnmsub132sd,0x66389f58,"vfnmsub132sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[89][2]},
  }, { /* vex_W_ext 28 */
    {OP_vfnmsub213ss,0x6638af18,"vfnmsub213ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[90][0]},
    {OP_vfnmsub213sd,0x6638af58,"vfnmsub213sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[90][2]},
  }, { /* vex_W_ext 29 */
    {OP_vfnmsub231ss,0x6638bf18,"vfnmsub231ss",Vss,xx,Hss,Wss,Vss,mrm|vex|reqp,x,tevexwb[91][0]},
    {OP_vfnmsub231sd,0x6638bf58,"vfnmsub231sd",Vsd,xx,Hsd,Wsd,Vsd,mrm|vex|reqp,x,tevexwb[91][2]},
  }, { /* vex_W_ext 30 */
    {OP_vfmaddsubps,0x663a5c18,"vfmaddsubps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[30][1]},
    {OP_vfmaddsubps,0x663a5c58,"vfmaddsubps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 31 */
    {OP_vfmaddsubpd,0x663a5d18,"vfmaddsubpd",Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[31][1]},
    {OP_vfmaddsubpd,0x663a5d58,"vfmaddsubpd",Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 32 */
    {OP_vfmsubaddps,0x663a5e18,"vfmsubaddps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[32][1]},
    {OP_vfmsubaddps,0x663a5e58,"vfmsubaddps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 33 */
    {OP_vfmsubaddpd,0x663a5f18,"vfmsubaddpd",Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[33][1]},
    {OP_vfmsubaddpd,0x663a5f58,"vfmsubaddpd",Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 34 */
    {OP_vfmaddps,0x663a6818,"vfmaddps",Vvs,xx,Lvs,Wvs,Hvs,mrm|vex|reqp,x,tvexw[34][1]},
    {OP_vfmaddps,0x663a6858,"vfmaddps",Vvs,xx,Lvs,Hvs,Wvs,mrm|vex|reqp,x,END_LIST},
  }, { /* vex_W_ext 35 */
    {OP_vfmaddpd,0x663a6918,"vfmaddpd",Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[35][1]},
    {OP_vfmaddpd,0x663a6958,"vfmaddpd",Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
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
    {OP_vfmsubpd,0x663a6d18,"vfmsubpd",Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[39][1]},
    {OP_vfmsubpd,0x663a6d58,"vfmsubpd",Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
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
    {OP_vfnmaddpd,0x663a7918,"vfnmaddpd",Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[43][1]},
    {OP_vfnmaddpd,0x663a7958,"vfnmaddpd",Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
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
    {OP_vfnmsubpd,0x663a7d18,"vfnmsubpd",Vvd,xx,Lvd,Wvd,Hvd,mrm|vex|reqp,x,tvexw[47][1]},
    {OP_vfnmsubpd,0x663a7d58,"vfnmsubpd",Vvd,xx,Lvd,Hvd,Wvd,mrm|vex|reqp,x,END_LIST},
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
    {OP_vpgatherdd,0x66389018,"vpgatherdd",Vx,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[189][0]},
    {OP_vpgatherdq,0x66389058,"vpgatherdq",Vx,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[189][2]},
  }, { /* vex_W_ext 67 */
    {OP_vpgatherqd,0x66389118,"vpgatherqd",Vx,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[190][0]},
    {OP_vpgatherqq,0x66389158,"vpgatherqq",Vx,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[190][2]},
  }, { /* vex_W_ext 68 */
    {OP_vgatherdps,0x66389218,"vgatherdps",Vvs,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[191][0]},
    {OP_vgatherdpd,0x66389258,"vgatherdpd",Vvd,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[191][2]},
  }, { /* vex_W_ext 69 */
    {OP_vgatherqps,0x66389318,"vgatherqps",Vvs,Hx,MVd,Hx,xx, mrm|vex|reqp,x,tevexwb[192][0]},
    {OP_vgatherqpd,0x66389358,"vgatherqpd",Vvd,Hx,MVq,Hx,xx, mrm|vex|reqp,x,tevexwb[192][2]},
  }, { /* vex_W_ext 70 */
    {OP_vpmaskmovd,0x66388c18,"vpmaskmovd",Vx,xx,Hx,Mx,xx, mrm|vex|reqp|predcx,x,tvexw[71][0]},
    {OP_vpmaskmovq,0x66388c58,"vpmaskmovq",Vx,xx,Hx,Mx,xx, mrm|vex|reqp|predcx,x,tvexw[71][1]},
  }, { /* vex_W_ext 71 */
    /* Conditional store => predcx */
    {OP_vpmaskmovd,0x66388e18,"vpmaskmovd",Mx,xx,Vx,Hx,xx, mrm|vex|reqp|predcx,x,END_LIST},
    {OP_vpmaskmovq,0x66388e58,"vpmaskmovq",Mx,xx,Vx,Hx,xx, mrm|vex|reqp|predcx,x,END_LIST},
  }, { /* vex_W_ext 72 */
    {OP_vpsrlvd,0x66384518,"vpsrlvd",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[129][0]},
    {OP_vpsrlvq,0x66384558,"vpsrlvq",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[129][2]},
  }, { /* vex_W_ext 73 */
    {OP_vpsllvd,0x66384718,"vpsllvd",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[131][0]},
    {OP_vpsllvq,0x66384758,"vpsllvq",Vx,xx,Hx,Wx,xx, mrm|vex|reqp,x,tevexwb[131][2]},
  }, { /* vex_W_ext 74 */
    {OP_kmovw,0x0f9010,"kmovw",KPw,xx,KQw,xx,xx, mrm|vex|reqL0,x,tvexw[76][0]},
    {OP_kmovq,0x0f9050,"kmovq",KPq,xx,KQq,xx,xx, mrm|vex|reqL0,x,tvexw[76][1]},
  }, { /* vex_W_ext 75 */
    {OP_kmovb,0x660f9010,"kmovb",KPb,xx,KQb,xx,xx, mrm|vex|reqL0,x,tvexw[77][0]},
    {OP_kmovd,0x660f9050,"kmovd",KPd,xx,KQd,xx,xx, mrm|vex|reqL0,x,tvexw[77][1]},
  }, { /* vex_W_ext 76 */
    {OP_kmovw,0x0f9110,"kmovw",KQw,xx,KPw,xx,xx, mrm|vex|reqL0,x,tvexw[78][0]},
    {OP_kmovq,0x0f9150,"kmovq",KQq,xx,KPq,xx,xx, mrm|vex|reqL0,x,tvexw[106][1]},
  }, { /* vex_W_ext 77 */
    {OP_kmovb,0x660f9110,"kmovb",KQb,xx,KPb,xx,xx, mrm|vex|reqL0,x,tvexw[79][0]},
    {OP_kmovd,0x660f9150,"kmovd",KQd,xx,KPd,xx,xx, mrm|vex|reqL0,x,tvexw[106][0]},
  }, { /* vex_W_ext 78 */
    {OP_kmovw,0x0f9210,"kmovw",KPw,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[80][0]},
    {INVALID, 0x0f9250,"(bad)", xx,xx,xx,xx,xx,      no,x,NA},
  }, { /* vex_W_ext 79 */
    {OP_kmovb,0x660f9210,"kmovb",KPb,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[81][0]},
    {INVALID, 0x660f9250,"(bad)", xx,xx,xx,xx,xx,      no,x,NA},
  }, { /* vex_W_ext 80 */
    {OP_kmovw,0x0f9310,"kmovw",  Gd,xx,KRw,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {INVALID, 0x0f9450,"(bad)", xx,xx,xx,xx,xx,        no,x,NA},
  }, { /* vex_W_ext 81 */
    {OP_kmovb,0x660f9310,"kmovb",Gd,xx,KRb,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {INVALID, 0x660f9350,"(bad)",xx,xx,xx,xx,xx,       no,x,NA},
  }, { /* vex_W_ext 82 */
    {OP_kandw,0x0f4110,"kandw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandq,0x0f4150,"kandq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 83 */
    {OP_kandb,0x660f4110,"kandb",KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandd,0x660f4150,"kandd",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 84 */
    {OP_kandnw,0x0f4210,"kandnw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandnq,0x0f4250,"kandnq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 85 */
    {OP_kandnb,0x660f4210,"kandnb",KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kandnd,0x660f4250,"kandnd",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 86 */
    {OP_kunpckwd,0x0f4b10,"kunpckwd",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kunpckdq,0x0f4b50,"kunpckdq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 87 */
    {OP_kunpckbw,0x660f4b10,"kunpckbw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {INVALID,    0x660f4b50,   "(bad)", xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 88 */
    {OP_knotw,0x0f4410,"knotw",KPw,xx,KRw,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_knotq,0x0f4450,"knotq",KPq,xx,KRq,xx,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 89 */
    {OP_knotb,0x660f4410,"knotb",KPb,xx,KRb,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_knotd,0x660f4450,"knotd",KPd,xx,KRd,xx,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 90 */
    {OP_korw,0x0f4510,"korw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_korq,0x0f4550,"korq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 91 */
    {OP_korb,0x660f4510,"korb",KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kord,0x660f4550,"kord",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 92 */
    {OP_kxnorw,0x0f4610,"kxnorw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxnorq,0x0f4650,"kxnorq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 93 */
    {OP_kxnorb,0x660f4610,"kxnorb",KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxnord,0x660f4650,"kxnord",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 94 */
    {OP_kxorw,0x0f4710,"kxorw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxorq,0x0f4750,"kxorq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 95 */
    {OP_kxorb,0x660f4710,"kxorb",KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kxord,0x660f4750,"kxord",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 96 */
    {OP_kaddw,0x0f4a10,"kaddw",KPw,xx,KVw,KRw,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kaddq,0x0f4a50,"kaddq",KPq,xx,KVq,KRq,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 97 */
    {OP_kaddb,0x660f4a10,"kaddb",KPb,xx,KVb,KRb,xx, mrm|vex|reqL1,x,END_LIST},
    {OP_kaddd,0x660f4a50,"kaddd",KPd,xx,KVd,KRd,xx, mrm|vex|reqL1,x,END_LIST},
  }, { /* vex_W_ext 98 */
    {OP_kortestw,0x0f9810,"kortestw",KPw,xx,KRw,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
    {OP_kortestq,0x0f9850,"kortestq",KPq,xx,KRq,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
  }, { /* vex_W_ext 99 */
    {OP_kortestb,0x660f9810,"kortestb",KPb,xx,KRb,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
    {OP_kortestd,0x660f9850,"kortestd",KPd,xx,KRd,xx,xx, mrm|vex|reqL0,(fWC|fWZ),END_LIST},
  }, { /* vex_W_ext 100 */
    {OP_kshiftlb,0x663a3208,"kshiftlb",KPb,xx,KRb,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftlw,0x663a3248,"kshiftlw",KPw,xx,KRw,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 101 */
    {OP_kshiftld,0x663a3308,"kshiftld",KPd,xx,KRd,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftlq,0x663a3348,"kshiftlq",KPq,xx,KRq,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 102 */
    {OP_kshiftrb,0x663a3008,"kshiftrb",KPb,xx,KRb,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftrw,0x663a3048,"kshiftrw",KPw,xx,KRw,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 103 */
    {OP_kshiftrd,0x663a3108,"kshiftrd",KPd,xx,KRd,Ib,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kshiftrq,0x663a3148,"kshiftrq",KPq,xx,KRq,Ib,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 104 */
    {OP_ktestw,0x0f9910,"ktestw",KPw,xx,KRw,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
    {OP_ktestq,0x0f9950,"ktestq",KPq,xx,KRq,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
  }, { /* vex_W_ext 105 */
    {OP_ktestb,0x660f9910,"ktestb",KPb,xx,KRb,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
    {OP_ktestd,0x660f9950,"ktestd",KPd,xx,KRd,xx,xx, mrm|vex|reqL0,fW6,END_LIST},
  }, { /* vex_W_ext 106 */
    {OP_kmovd,0xf20f9210,"kmovd",KPd,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[107][0]},
    {OP_kmovq,0xf20f9250,"kmovq",KPq,xx,Ry,xx,xx, mrm|vex|reqL0,x,tvexw[107][1]},
  }, { /* vex_W_ext 107 */
    {OP_kmovd,0xf20f9310,"kmovd",  Gd,xx,KRd,xx,xx, mrm|vex|reqL0,x,END_LIST},
    {OP_kmovq,0xf20f9350,"kmovq",Gy,xx,KRq,xx,xx, mrm|vex|reqL0,x,END_LIST},
  }, { /* vex_W_ext 108 */
    {OP_vmovd, 0x660f6e10, "vmovd", Vdq, xx, Ed, xx, xx, mrm|vex, x, tvexw[109][0]},
    {OP_vmovq, 0x660f6e50, "vmovq", Vdq, xx, Ey, xx, xx, mrm|vex, x, tvexw[109][1]},
  }, { /* vex_W_ext 109 */
    {OP_vmovd, 0x660f7e10, "vmovd", Ed, xx, Vd_dq, xx, xx, mrm|vex, x, tevexwb[136][0]},
    {OP_vmovq, 0x660f7e50, "vmovq", Ey, xx, Vq_dq, xx, xx, mrm|vex, x, tevexwb[136][2]},
  }, { /* vex_W_ext 110 */
    {OP_vpdpbusd, 0x66385008, "vpdpbusd", Ve, xx, He, We, xx, mrm|vex|ttfvm|reqp, x, tevexwb[267][0]},
    {INVALID,    0x663850,   "(bad)", xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 111 */
    {OP_vpdpbusds, 0x66385108, "vpdpbusds", Ve, xx, He, We, xx, mrm|vex|ttfvm|reqp, x, tevexwb[268][0]},
    {INVALID,    0x663850,   "(bad)", xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 112 */
    {OP_vpdpwssd, 0x66385208, "vpdpwssd", Ve, xx, He, We, xx, mrm|vex|ttfvm, x, tevexwb[269][0]},
    {INVALID,    0x663850,   "(bad)", xx,xx, xx,  xx,xx,     no,x,NA},
  }, { /* vex_W_ext 113 */
    {OP_vpdpwssds, 0x66385308, "vpdpwssds", Ve, xx, He, We, xx, mrm|vex|ttfvm|reqp, x, tevexwb[270][0]},
    {INVALID,    0x663850,   "(bad)", xx,xx, xx,  xx,xx,     no,x,NA},
  },
};

/****************************************************************************
 * Instructions that differ depending on evex.W and evex.b
 * Index is evex.W value * 2 + evex.b value
 */
const instr_info_t evex_Wb_extensions[][4] = {
  {    /* evex_W_ext 0 */
    {OP_vmovups, 0x0f1000,"vmovups", Ves,xx,KEd,Wes,xx,mrm|evex|ttfvm,x,tevexwb[1][0]},
    {INVALID, 0x0f1010,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1040,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1050,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 1 */
    {OP_vmovups, 0x0f1100,"vmovups", Wes,xx,KEd,Ves,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x0f1110,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1140,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1150,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 2 */
    {INVALID, 0x660f1000,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1010,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovupd, 0x660f1040,"vmovupd", Ved,xx,KEd,Wed,xx,mrm|evex|ttfvm,x,tevexwb[3][2]},
    {INVALID, 0x660f1050,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 3 */
    {INVALID, 0x660f1100,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1110,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovupd, 0x660f1140,"vmovupd", Wed,xx,KEd,Ved,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f1150,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 4 */
    {OP_vmovaps, 0x0f2800,"vmovaps", Ves,xx,KEd,Wes,xx,mrm|evex|ttfvm,x,tevexwb[5][0]},
    {INVALID, 0x0f2810,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2840,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2850,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 5 */
    {OP_vmovaps, 0x0f2900,"vmovaps", Wes,xx,KEd,Ves,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x0f2910,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2940,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2950,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 6 */
    {INVALID, 0x660f2800,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2810,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovapd, 0x660f2840,"vmovapd", Ved,xx,KEd,Wed,xx,mrm|evex|ttfvm,x,tevexwb[7][2]},
    {INVALID, 0x660f2850,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 7 */
    {INVALID, 0x660f2900,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2910,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovapd, 0x660f2940,"vmovapd", Wed,xx,KEd,Ved,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f2950,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 8 */
    {OP_vmovdqa32, 0x660f6f00,"vmovdqa32",Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[9][0]},
    {INVALID, 0x660f6f10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqa64, 0x660f6f40,"vmovdqa64",Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[9][2]},
    {INVALID, 0x660f6f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 9 */
    {OP_vmovdqa32, 0x660f7f00,"vmovdqa32",We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f7f10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqa64, 0x660f7f40,"vmovdqa64",We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0x660f7f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 10 */
    {OP_vmovdqu8, 0xf20f6f00,"vmovdqu8",Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[12][0]},
    {INVALID, 0xf20f6f10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu16, 0xf20f6f40,"vmovdqu16",Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[12][2]},
    {INVALID, 0xf20f6f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 11 */
    {OP_vmovdqu32, 0xf30f6f00,"vmovdqu32",Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[13][0]},
    {INVALID, 0xf30f6f10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu64, 0xf30f6f40,"vmovdqu64",Ve,xx,KEw,We,xx,mrm|evex|ttfvm,x,tevexwb[13][2]},
    {INVALID, 0xf30f6f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 12 */
    {OP_vmovdqu8, 0xf20f7f00,"vmovdqu8",We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf20f7f10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu16, 0xf20f7f40,"vmovdqu16",We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf20f7f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 13 */
    {OP_vmovdqu32, 0xf30f7f00,"vmovdqu32",We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf30f7f10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovdqu64, 0xf30f7f40,"vmovdqu64",We,xx,KEw,Ve,xx,mrm|evex|ttfvm,x,END_LIST},
    {INVALID, 0xf30f7f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 14 */
    {OP_vmovlps, 0x0f1200, "vmovlps", Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|evex|reqL0|reqLL0|ttt2, x, tevexwb[15][0]}, /*"vmovhlps" if reg-reg */
    {INVALID, 0x0f1210,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1240,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1250,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 15 */
    {OP_vmovlps, 0x0f1300, "vmovlps", Mq, xx, Vq_dq, xx, xx, mrm|evex|ttt2, x, END_LIST},
    {INVALID, 0x0f1310,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1340,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1350,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 16 */
    {INVALID, 0x660f1200,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1210,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovlpd, 0x660f1240, "vmovlpd", Vq_dq, xx, Hq_dq, Mq, xx, mrm|evex|reqL0|reqLL0|ttt1s, x, tevexwb[17][2]},
    {INVALID, 0x660f1250,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 17 */
    {INVALID, 0x660f1300,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1310,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovlpd, 0x660f1340, "vmovlpd", Mq, xx, Vq_dq, xx, xx, mrm|evex|ttt1s, x, END_LIST},
    {INVALID, 0x660f1350,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 18 */
    {OP_vmovsldup, 0xf30f1200, "vmovsldup", Ves, xx, KEw, Wes, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf30f1210,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1240,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1250,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 19 */
    {INVALID, 0xf20f1200,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf20f1210,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovddup, 0xf20f1240, "vmovddup", Ved, xx, KEb, We, xx, mrm|evex|ttdup, x, END_LIST},
    {INVALID, 0xf20f1250,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 20 */
    {OP_vmovhps, 0x0f1600, "vmovhps", Vq_dq, xx, Hq_dq, Wq_dq, xx, mrm|evex|reqL0|reqLL0|ttt2, x, tevexwb[21][0]}, /*"vmovlhps" if reg-reg */
    {INVALID, 0x0f1610,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1640,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1650,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 21 */
    {OP_vmovhps, 0x0f1700, "vmovhps", Mq, xx, Vq_dq, xx, xx, mrm|evex|reqL0|reqLL0|ttt2, x, END_LIST},
    {INVALID, 0x0f1710,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1740,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1750,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 22 */
    {INVALID, 0x660f1600,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1610,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovhpd, 0x660f1640, "vmovhpd", Vq_dq, xx, Hq_dq, Mq, xx, mrm|evex|reqL0|reqLL0|ttt1s, x, tevexwb[23][2]},
    {INVALID, 0x660f1650,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 23 */
    {INVALID, 0x660f1700,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1710,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovhpd, 0x660f1740, "vmovhpd", Mq, xx, Vq_dq, xx, xx, mrm|evex|reqL0|reqLL0|ttt1s, x, END_LIST},
    {INVALID, 0x660f1750,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 24 */
    {OP_vmovshdup, 0xf30f1600, "vmovshdup", Ves, xx, KEw, Wes, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf30f1610,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1640,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0xf30f1650,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 25 */
    {OP_vunpcklps, 0x0f1400, "vunpcklps", Ves, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[25][1]},
    {OP_vunpcklps, 0x0f1410, "vunpcklps", Ves, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x0f1440,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1450,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 26 */
    {INVALID, 0x660f1400,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1410,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vunpcklpd, 0x660f1440, "vunpcklpd", Ved, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[26][3]},
    {OP_vunpcklpd, 0x660f1450, "vunpcklpd", Ved, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 27 */
    {OP_vunpckhps, 0x0f1500, "vunpckhps", Ves, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[27][1]},
    {OP_vunpckhps, 0x0f1510, "vunpckhps", Ves, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x0f1540,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f1550,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 28 */
    {INVALID, 0x660f1500,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f1510,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vunpckhpd, 0x660f1540, "vunpckhpd", Ved, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[28][3]},
    {OP_vunpckhpd, 0x660f1550, "vunpckhpd", Ved, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 29 */
    {OP_vcvtss2si, 0xf30f2d00, "vcvtss2si", Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[29][1]},
    {OP_vcvtss2si, 0xf30f2d10, "vcvtss2si", Gd, xx, Ups, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, tevexwb[29][2]},
    {OP_vcvtss2si, 0xf30f2d40, "vcvtss2si", Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[29][3]},
    {OP_vcvtss2si, 0xf30f2d50, "vcvtss2si", Gy, xx, Ups, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 30 */
    {OP_vcvtsd2si, 0xf20f2d00, "vcvtsd2si", Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[30][1]},
    {OP_vcvtsd2si, 0xf20f2d10, "vcvtsd2si", Gd, xx, Upd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, tevexwb[30][2]},
    {OP_vcvtsd2si, 0xf20f2d40, "vcvtsd2si", Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[30][3]},
    {OP_vcvtsd2si, 0xf20f2d50, "vcvtsd2si", Gy, xx, Upd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 31 */
    {OP_vcvtsi2ss, 0xf30f2a00, "vcvtsi2ss", Vdq, xx, H12_dq, Ed, xx, mrm|evex|ttt1s, x, tevexwb[31][1]},
    {OP_vcvtsi2ss, 0xf30f2a10, "vcvtsi2ss", Vdq, xx, H12_dq, Rd, xx, mrm|evex|er|ttt1s, x, tevexwb[31][2]},
    {OP_vcvtsi2ss, 0xf30f2a40, "vcvtsi2ss", Vdq, xx, H12_dq, Ey, xx, mrm|evex|ttt1s, x, tevexwb[31][3]},
    {OP_vcvtsi2ss, 0xf30f2a50, "vcvtsi2ss", Vdq, xx, H12_dq, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 32 */
    {OP_vcvtsi2sd, 0xf20f2a00, "vcvtsi2sd", Vdq, xx, Hsd, Ed, xx, mrm|evex|ttt1s, x, tevexwb[32][2]},
    {INVALID, 0xf20f2a10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcvtsi2sd, 0xf20f2a40, "vcvtsi2sd", Vdq, xx, Hsd, Ey, xx, mrm|evex|ttt1s, x, tevexwb[32][3]},
    {OP_vcvtsi2sd, 0xf20f2a50, "vcvtsi2sd", Vdq, xx, Hsd, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 33 */
    {OP_vmovntps, 0x0f2b00, "vmovntps", Mes, xx, Ves, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x0f2b10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2b40,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2b50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 34 */
    {INVALID, 0x660f2b00,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2b10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovntpd, 0x660f2b40, "vmovntpd", Med, xx, Ved, xx, xx, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x660f2b50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 35 */
    {OP_vcvttss2si, 0xf30f2c00, "vcvttss2si", Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[35][1]},
    {OP_vcvttss2si, 0xf30f2c10, "vcvttss2si", Gd, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, tevexwb[35][2]},
    {OP_vcvttss2si, 0xf30f2c40, "vcvttss2si", Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[35][3]},
    {OP_vcvttss2si, 0xf30f2c50, "vcvttss2si", Gy, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 36 */
    {OP_vcvttsd2si, 0xf20f2c00, "vcvttsd2si", Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[36][1]},
    {OP_vcvttsd2si, 0xf20f2c10, "vcvttsd2si", Gd, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, tevexwb[36][2]},
    {OP_vcvttsd2si, 0xf20f2c40, "vcvttsd2si", Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[36][3]},
    {OP_vcvttsd2si, 0xf20f2c50, "vcvttsd2si", Gy, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 37 */
    {OP_vucomiss, 0x0f2e00, "vucomiss", xx, xx, Vss, Wss, xx, mrm|evex|ttt1s, fW6, tevexwb[37][1]},
    {OP_vucomiss, 0x0f2e10, "vucomiss", xx, xx, Vss, Uss, xx, mrm|evex|sae|ttt1s, fW6, END_LIST},
    {INVALID, 0x0f2e40,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2e50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 38 */
    {INVALID, 0x660f2e00,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2e10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vucomisd, 0x660f2e40, "vucomisd", xx, xx, Vsd, Wsd, xx, mrm|evex|ttt1s, fW6, tevexwb[38][1]},
    {OP_vucomisd, 0x660f2e50, "vucomisd", xx, xx, Vsd, Usd, xx, mrm|evex|sae|ttt1s, fW6, END_LIST},
  }, { /* evex_W_ext 39 */
    {OP_vcomiss,  0x0f2f00, "vcomiss",  xx, xx, Vss, Wss, xx, mrm|evex|ttt1f|inopsz4, fW6, tevexwb[39][1]},
    {OP_vcomiss,  0x0f2f10, "vcomiss",  xx, xx, Vss, Uss, xx, mrm|evex|sae|ttt1f|inopsz4, fW6, END_LIST},
    {INVALID, 0x0f2f40,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x0f2f50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 40 */
    {INVALID, 0x660f2e00,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f2e10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcomisd,  0x660f2f40, "vcomisd",  xx, xx, Vsd, Wsd, xx, mrm|evex|ttt1f|inopsz8, fW6, tevexwb[40][1]},
    {OP_vcomisd,  0x660f2f50, "vcomisd",  xx, xx, Vsd, Usd, xx, mrm|evex|sae|ttt1f|inopsz8, fW6, END_LIST},
  }, { /* evex_W_ext 41 */
    {OP_vpandd, 0x660fdb00, "vpandd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[41][1]},
    {OP_vpandd, 0x660fdb10, "vpandd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpandq, 0x660fdb40, "vpandq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[41][3]},
    {OP_vpandq, 0x660fdb50, "vpandq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 42 */
    {OP_vpandnd, 0x660fdf00, "vpandnd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[42][1]},
    {OP_vpandnd, 0x660fdf10, "vpandnd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpandnq, 0x660fdf40, "vpandnq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[42][3]},
    {OP_vpandnq, 0x660fdf50, "vpandnq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 43 */
    {OP_vpord, 0x660feb00, "vpord", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[43][1]},
    {OP_vpord, 0x660feb10, "vpord", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vporq, 0x660feb40, "vporq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[43][3]},
    {OP_vporq, 0x660feb50, "vporq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 44 */
    {OP_vpxord, 0x660fef00, "vpxord", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[44][1]},
    {OP_vpxord, 0x660fef10, "vpxord", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpxorq, 0x660fef40, "vpxorq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[44][3]},
    {OP_vpxorq, 0x660fef50, "vpxorq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 45 */
    {OP_vpmulld,  0x66384008, "vpmulld",   Ve, xx, KEw,He,We, mrm|evex|reqp|ttfv, x, tevexwb[45][1]},
    {OP_vpmulld,  0x66384018, "vpmulld",   Ve, xx, KEw,He,Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpmullq,  0x66384048, "vpmullq",   Ve, xx, KEb,He,We, mrm|evex|reqp|ttfv, x, tevexwb[45][3]},
    {OP_vpmullq,  0x66384058, "vpmullq",   Ve, xx, KEb,He,Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 46 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2qq, 0x660f7b00, "vcvtps2qq", Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[24][0]},
    {MOD_EXT, 0x660f7b10, "(mod ext 24)", xx, xx, xx, xx, xx, mrm|evex, x, 24},
    {OP_vcvtpd2qq, 0x660f7b40, "vcvtpd2qq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[25][0]},
    {MOD_EXT, 0x660f7b50, "(mod ext 25)", xx, xx, xx, xx, xx, mrm|evex, x, 25},
  }, { /* evex_W_ext 47 */
    {OP_vcvtps2udq, 0x0f7900, "vcvtps2udq", Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[26][0]},
    {MOD_EXT, 0x0f7910, "(mod ext 26)", xx, xx, xx, xx, xx, mrm|evex, x, 26},
    {OP_vcvtpd2udq, 0x0f7940, "vcvtpd2udq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[27][0]},
    {MOD_EXT, 0x0f7950, "(mod ext 27)", xx, xx, xx, xx, xx, mrm|evex, x, 27},
  }, { /* evex_W_ext 48 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2uqq, 0x660f7900, "vcvtps2uqq", Ve, xx, KEw, Wh_e, xx, mrm|evex|tthv, x, modx[28][0]},
    {MOD_EXT, 0x660f7910, "(mod ext 28)", xx, xx, xx, xx, xx, mrm|evex, x, 28},
    {OP_vcvtpd2uqq, 0x660f7940, "vcvtpd2uqq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[29][0]},
    {MOD_EXT, 0x660f7950, "(mod ext 29)", xx, xx, xx, xx, xx, mrm|evex, x, 29},
  }, { /* evex_W_ext 49 */
    {OP_vcvttps2udq, 0x0f7800, "vcvttps2udq", Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[30][0]},
    {MOD_EXT, 0x0f7810, "(mod ext 30)", xx, xx, xx, xx, xx, mrm|evex, x, 30},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttpd2udq, 0x0f7840, "vcvttpd2udq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[31][0]},
    {MOD_EXT, 0x0f7840, "(mod ext 31)", xx, xx, xx, xx, xx, mrm|evex, x, 31},
  }, { /* evex_W_ext 50 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttps2qq, 0x660f7a00, "vcvttps2qq", Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[32][0]},
    {MOD_EXT, 0x660f7a10, "(mod ext 32)", xx, xx, xx, xx, xx, mrm|evex, x, 32},
    {OP_vcvttpd2qq, 0x660f7a40, "vcvttpd2qq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[33][0]},
    {MOD_EXT, 0x660f7a50, "(mod ext 33)", xx, xx, xx, xx, xx, mrm|evex, x, 33},
  }, { /* evex_W_ext 51 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttps2uqq, 0x660f7800, "vcvttps2uqq", Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[34][0]},
    {MOD_EXT, 0x660f7810, "(mod ext 34)", xx, xx, xx, xx, xx, mrm|evex, x, 34},
    {OP_vcvttpd2uqq, 0x660f7840, "vcvttpd2uqq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[35][0]},
    {MOD_EXT, 0x660f7850, "(mod ext 35)", xx, xx, xx, xx, xx, mrm|evex, x, 35},
  }, { /* evex_W_ext 52 */
    {OP_vcvtss2usi, 0xf30f7900, "vcvtss2usi", Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[52][1]},
    {OP_vcvtss2usi, 0xf30f7910, "vcvtss2usi", Gd, xx, Uss, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, tevexwb[52][2]},
    {OP_vcvtss2usi, 0xf30f7940, "vcvtss2usi", Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[52][3]},
    {OP_vcvtss2usi, 0xf30f7950, "vcvtss2usi", Gy, xx, Uss, xx, xx, mrm|evex|er|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 53 */
    {OP_vcvtsd2usi, 0xf20f7900, "vcvtsd2usi", Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[53][1]},
    {OP_vcvtsd2usi, 0xf20f7910, "vcvtsd2usi", Gd, xx, Usd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, tevexwb[53][2]},
    {OP_vcvtsd2usi, 0xf20f7940, "vcvtsd2usi", Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[53][1]},
    {OP_vcvtsd2usi, 0xf20f7950, "vcvtsd2usi", Gy, xx, Usd, xx, xx, mrm|evex|er|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 54 */
    {OP_vcvttss2usi, 0xf30f7800, "vcvttss2usi", Gd, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[54][1]},
    {OP_vcvttss2usi, 0xf30f7810, "vcvttss2usi", Gd, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, tevexwb[54][2]},
    {OP_vcvttss2usi, 0xf30f7840, "vcvttss2usi", Gy, xx, Wss, xx, xx, mrm|evex|ttt1f|inopsz4, x, tevexwb[54][3]},
    {OP_vcvttss2usi, 0xf30f7850, "vcvttss2usi", Gy, xx, Uss, xx, xx, mrm|evex|sae|ttt1f|inopsz4, x, END_LIST},
  }, { /* evex_W_ext 55 */
    {OP_vcvttsd2usi, 0xf20f7800, "vcvttsd2usi", Gd, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[55][1]},
    {OP_vcvttsd2usi, 0xf20f7810, "vcvttsd2usi", Gd, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, tevexwb[55][2]},
    {OP_vcvttsd2usi, 0xf20f7840, "vcvttsd2usi", Gy, xx, Wsd, xx, xx, mrm|evex|ttt1f|inopsz8, x, tevexwb[55][3]},
    {OP_vcvttsd2usi, 0xf20f7850, "vcvttsd2usi", Gy, xx, Usd, xx, xx, mrm|evex|sae|ttt1f|inopsz8, x, END_LIST},
  }, { /* evex_W_ext 56 */
    {OP_vcvtdq2ps, 0x0f5b00, "vcvtdq2ps", Ves, xx, KEw, We, xx, mrm|evex|ttfv, x, modx[36][0]},
    {MOD_EXT, 0x0f5b10, "(mod ext 36)", xx, xx, xx, xx, xx, mrm|evex, x, 36},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtqq2ps, 0x0f5b40, "vcvtqq2ps", Ves, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[37][0]},
    {MOD_EXT, 0x0f5b50, "(mod ext 37)", xx, xx, xx, xx, xx, mrm|evex, x, 37},
  }, { /* evex_W_ext 57 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtdq2pd, 0xf30fe600, "vcvtdq2pd", Ved, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, tevexwb[57][1]},
    {OP_vcvtdq2pd, 0xf30fe610, "vcvtdq2pd", Ved, xx, KEb, Md, xx, mrm|evex|tthv, x, END_LIST},
    {OP_vcvtqq2pd, 0xf30fe640, "vcvtqq2pd", Ved, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[38][0]},
    {MOD_EXT, 0xf30fe650, "(mod ext 38)", xx, xx, xx, xx, xx, mrm|evex, x, 38},
  }, { /* evex_W_ext 58 */
    {OP_vcvtusi2ss, 0xf30f7b00, "vcvtusi2ss", Vdq, xx, H12_dq, Ed, xx, mrm|evex|ttt1s, x, tevexwb[58][1]},
    {OP_vcvtusi2ss, 0xf30f7b10, "vcvtusi2ss", Vdq, xx, H12_dq, Rd, xx, mrm|evex|er|ttt1s, x, tevexwb[58][2]},
    {OP_vcvtusi2ss, 0xf30f7b40, "vcvtusi2ss", Vdq, xx, H12_dq, Ey, xx, mrm|evex|ttt1s, x, tevexwb[58][3]},
    {OP_vcvtusi2ss, 0xf30f7b50, "vcvtusi2ss", Vdq, xx, H12_dq, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 59 */
    {OP_vcvtusi2sd, 0xf20f7b00, "vcvtusi2sd", Vdq, xx, Hsd,   Ed, xx, mrm|evex|ttt1s, x, tevexwb[59][2]},
    {INVALID, 0xf20f7b10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcvtusi2sd, 0xf20f7b40, "vcvtusi2sd", Vdq, xx, Hsd, Ey, xx, mrm|evex|ttt1s, x, tevexwb[59][3]},
    {OP_vcvtusi2sd, 0xf20f7b50, "vcvtusi2sd", Vdq, xx, Hsd, Ry, xx, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 60 */
    {OP_vcvtudq2ps, 0xf20f7a00, "vcvtudq2ps", Ve, xx, KEw, We, xx, mrm|evex|ttfv, x, modx[39][0]},
    {MOD_EXT, 0xf20f7a10, "(mod ext 39)", xx, xx, xx, xx, xx, mrm|evex, x, 39},
    {OP_vcvtuqq2ps, 0xf20f7a40, "vcvtuqq2ps", Ve, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[40][0]},
    {MOD_EXT, 0xf20f7a50, "(mod ext 40)", xx, xx, xx, xx, xx, mrm|evex, x, 40},
  }, { /* evex_W_ext 61 */
    {OP_vcvtudq2pd, 0xf30f7a00, "vcvtudq2pd", Ve, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, tevexwb[61][1]},
    {OP_vcvtudq2pd, 0xf30f7a10, "vcvtudq2pd", Ve, xx, KEb, Md, xx, mrm|evex|tthv, x, END_LIST},
    {OP_vcvtuqq2pd, 0xf30f7a40, "vcvtuqq2pd", Ve, xx, KEb, We, xx, mrm|evex|ttfv, x, modx[41][0]},
    {MOD_EXT, 0xf30f7a50, "(mod ext 41)", xx, xx, xx, xx, xx, mrm|evex, x, 41},
  }, { /* evex_W_ext 62 */
    {OP_vfmadd132ps,0x66389808,"vfmadd132ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[14]},
    {MOD_EXT, 0x66389818, "(mod ext 42)", xx, xx, xx, xx, xx, mrm|evex, x, 42},
    {OP_vfmadd132pd,0x66389848,"vfmadd132pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[17]},
    {MOD_EXT, 0x66389858, "(mod ext 43)", xx, xx, xx, xx, xx, mrm|evex, x, 43},
  }, { /* evex_W_ext 63 */
    {OP_vfmadd213ps,0x6638a808,"vfmadd213ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[20]},
    {MOD_EXT, 0x6638a818, "(mod ext 44)", xx, xx, xx, xx, xx, mrm|evex, x, 44},
    {OP_vfmadd213pd,0x6638a848,"vfmadd213pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[23]},
    {MOD_EXT, 0x6638a858, "(mod ext 45)", xx, xx, xx, xx, xx, mrm|evex, x, 45},
  }, { /* evex_W_ext 64 */
    {OP_vfmadd231ps,0x6638b808,"vfmadd231ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[26]},
    {MOD_EXT, 0x6638b818, "(mod ext 46)", xx, xx, xx, xx, xx, mrm|evex, x, 46},
    {OP_vfmadd231pd,0x6638b848,"vfmadd231pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[29]},
    {MOD_EXT, 0x6638b858, "(mod ext 47)", xx, xx, xx, xx, xx, mrm|evex, x, 47},
  }, { /* evex_W_ext 65 */
    {OP_vfmadd132ss,0x66389908,"vfmadd132ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[32]},
    {OP_vfmadd132ss,0x66389918,"vfmadd132ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[33]},
    {OP_vfmadd132sd,0x66389948,"vfmadd132sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[34]},
    {OP_vfmadd132sd,0x66389958,"vfmadd132sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[35]},
  }, { /* evex_W_ext 66 */
    {OP_vfmadd213ss,0x6638a908,"vfmadd213ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[36]},
    {OP_vfmadd213ss,0x6638a918,"vfmadd213ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[37]},
    {OP_vfmadd213sd,0x6638a948,"vfmadd213sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[38]},
    {OP_vfmadd213sd,0x6638a958,"vfmadd213sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[39]},
  }, { /* evex_W_ext 67 */
    {OP_vfmadd231ss,0x6638b908,"vfmadd231ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[40]},
    {OP_vfmadd231ss,0x6638b918,"vfmadd231ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[41]},
    {OP_vfmadd231sd,0x6638b948,"vfmadd231sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[42]},
    {OP_vfmadd231sd,0x6638b958,"vfmadd231sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[43]},
  }, { /* evex_W_ext 68 */
    {OP_vfmaddsub132ps,0x66389608,"vfmaddsub132ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[44]},
    {MOD_EXT, 0x66389618, "(mod ext 48)", xx, xx, xx, xx, xx, mrm|evex, x, 48},
    {OP_vfmaddsub132pd,0x66389648,"vfmaddsub132pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[47]},
    {MOD_EXT, 0x66389658, "(mod ext 49)", xx, xx, xx, xx, xx, mrm|evex, x, 49},
  }, { /* evex_W_ext 69 */
    {OP_vfmaddsub213ps,0x6638a608,"vfmaddsub213ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[50]},
    {MOD_EXT, 0x6638a618, "(mod ext 50)", xx, xx, xx, xx, xx, mrm|evex, x, 50},
    {OP_vfmaddsub213pd,0x6638a648,"vfmaddsub213pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[53]},
    {MOD_EXT, 0x6638a658, "(mod ext 51)", xx, xx, xx, xx, xx, mrm|evex, x, 51},
  }, { /* evex_W_ext 70 */
    {OP_vfmaddsub231ps,0x6638b608,"vfmaddsub231ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[56]},
    {MOD_EXT, 0x6638b618, "(mod ext 52)", xx, xx, xx, xx, xx, mrm|evex, x, 52},
    {OP_vfmaddsub231pd,0x6638b648,"vfmaddsub231pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[59]},
    {MOD_EXT, 0x6638b658, "(mod ext 53)", xx, xx, xx, xx, xx, mrm|evex, x, 53},
  }, { /* evex_W_ext 71 */
    {OP_vfmsubadd132ps,0x66389708,"vfmsubadd132ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[62]},
    {MOD_EXT, 0x66389718, "(mod ext 54)", xx, xx, xx, xx, xx, mrm|evex, x, 54},
    {OP_vfmsubadd132pd,0x66389748,"vfmsubadd132pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[65]},
    {MOD_EXT, 0x66389758, "(mod ext 55)", xx, xx, xx, xx, xx, mrm|evex, x, 55},
  }, { /* evex_W_ext 72 */
    {OP_vfmsubadd213ps,0x6638a708,"vfmsubadd213ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[68]},
    {MOD_EXT, 0x6638a718, "(mod ext 56)", xx, xx, xx, xx, xx, mrm|evex, x, 56},
    {OP_vfmsubadd213pd,0x6638a748,"vfmsubadd213pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[71]},
    {MOD_EXT, 0x6638a758, "(mod ext 57)", xx, xx, xx, xx, xx, mrm|evex, x, 57},
  }, { /* evex_W_ext 73 */
    {OP_vfmsubadd231ps,0x6638b708,"vfmsubadd231ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[74]},
    {MOD_EXT, 0x6638b718, "(mod ext 58)", xx, xx, xx, xx, xx, mrm|evex, x, 58},
    {OP_vfmsubadd231pd,0x6638b748,"vfmsubadd231pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[77]},
    {MOD_EXT, 0x6638b758, "(mod ext 59)", xx, xx, xx, xx, xx, mrm|evex, x, 59},
  }, { /* evex_W_ext 74 */
    {OP_vfmsub132ps,0x66389a08,"vfmsub132ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[80]},
    {MOD_EXT, 0x66389a18, "(mod ext 60)", xx, xx, xx, xx, xx, mrm|evex, x, 60},
    {OP_vfmsub132pd,0x66389a48,"vfmsub132pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[83]},
    {MOD_EXT, 0x66389a58, "(mod ext 61)", xx, xx, xx, xx, xx, mrm|evex, x, 61},
  }, { /* evex_W_ext 75 */
    {OP_vfmsub213ps,0x6638aa08,"vfmsub213ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[86]},
    {MOD_EXT, 0x6638aa18, "(mod ext 62)", xx, xx, xx, xx, xx, mrm|evex, x, 62},
    {OP_vfmsub213pd,0x6638aa48,"vfmsub213pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[89]},
    {MOD_EXT, 0x6638aa58, "(mod ext 63)", xx, xx, xx, xx, xx, mrm|evex, x, 63},
  }, { /* evex_W_ext 76 */
    {OP_vfmsub231ps,0x6638ba08,"vfmsub231ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[92]},
    {MOD_EXT, 0x6638ba18, "(mod ext 64)", xx, xx, xx, xx, xx, mrm|evex, x, 64},
    {OP_vfmsub231pd,0x6638ba48,"vfmsub231pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[95]},
    {MOD_EXT, 0x6638ba58, "(mod ext 65)", xx, xx, xx, xx, xx, mrm|evex, x, 65},
  }, { /* evex_W_ext 77 */
    {OP_vfmsub132ss,0x66389b08,"vfmsub132ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[98]},
    {OP_vfmsub132ss,0x66389b18,"vfmsub132ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[99]},
    {OP_vfmsub132sd,0x66389b48,"vfmsub132sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[100]},
    {OP_vfmsub132sd,0x66389b58,"vfmsub132sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[101]},
  }, { /* evex_W_ext 78 */
    {OP_vfmsub213ss,0x6638ab08,"vfmsub213ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[102]},
    {OP_vfmsub213ss,0x6638ab18,"vfmsub213ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[103]},
    {OP_vfmsub213sd,0x6638ab48,"vfmsub213sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[104]},
    {OP_vfmsub213sd,0x6638ab58,"vfmsub213sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[105]},
  }, { /* evex_W_ext 79 */
    {OP_vfmsub231ss,0x6638bb08,"vfmsub231ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[106]},
    {OP_vfmsub231ss,0x6638bb18,"vfmsub231ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[107]},
    {OP_vfmsub231sd,0x6638bb48,"vfmsub231sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[108]},
    {OP_vfmsub231sd,0x6638bb58,"vfmsub231sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[109]},
  }, { /* evex_W_ext 80 */
    {OP_vfnmadd132ps,0x66389c08,"vfnmadd132ps",Ves,xx,KEb,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[110]},
    {MOD_EXT, 0x66389c18, "(mod ext 66)", xx, xx, xx, xx, xx, mrm|evex, x, 66},
    {OP_vfnmadd132pd,0x66389c48,"vfnmadd132pd",Ved,xx,KEw,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[113]},
    {MOD_EXT, 0x66389c58, "(mod ext 67)", xx, xx, xx, xx, xx, mrm|evex, x, 67},
  }, { /* evex_W_ext 81 */
    {OP_vfnmadd213ps,0x6638ac08,"vfnmadd213ps",Ves,xx,KEb,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[116]},
    {MOD_EXT, 0x6638ac18, "(mod ext 68)", xx, xx, xx, xx, xx, mrm|evex, x, 68},
    {OP_vfnmadd213pd,0x6638ac48,"vfnmadd213pd",Ved,xx,KEw,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[119]},
    {MOD_EXT, 0x6638ac58, "(mod ext 69)", xx, xx, xx, xx, xx, mrm|evex, x, 69},
  }, { /* evex_W_ext 82 */
    {OP_vfnmadd231ps,0x6638bc08,"vfnmadd231ps",Ves,xx,KEb,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[122]},
    {MOD_EXT, 0x6638bc18, "(mod ext 70)", xx, xx, xx, xx, xx, mrm|evex, x, 70},
    {OP_vfnmadd231pd,0x6638bc48,"vfnmadd231pd",Ved,xx,KEw,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[125]},
    {MOD_EXT, 0x6638bc58, "(mod ext 71)", xx, xx, xx, xx, xx, mrm|evex, x, 71},
  }, { /* evex_W_ext 83 */
    {OP_vfnmadd132ss,0x66389d08,"vfnmadd132ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[128]},
    {OP_vfnmadd132ss,0x66389d18,"vfnmadd132ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[129]},
    {OP_vfnmadd132sd,0x66389d48,"vfnmadd132sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[130]},
    {OP_vfnmadd132sd,0x66389d58,"vfnmadd132sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[131]},
  }, { /* evex_W_ext 84 */
    {OP_vfnmadd213ss,0x6638ad08,"vfnmadd213ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[132]},
    {OP_vfnmadd213ss,0x6638ad18,"vfnmadd213ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[133]},
    {OP_vfnmadd213sd,0x6638ad48,"vfnmadd213sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[134]},
    {OP_vfnmadd213sd,0x6638ad58,"vfnmadd213sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[135]},
  }, { /* evex_W_ext 85 */
    {OP_vfnmadd231ss,0x6638bd08,"vfnmadd231ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[136]},
    {OP_vfnmadd231ss,0x6638bd18,"vfnmadd231ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[137]},
    {OP_vfnmadd231sd,0x6638bd48,"vfnmadd231sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[138]},
    {OP_vfnmadd231sd,0x6638bd58,"vfnmadd231sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[139]},
  }, { /* evex_W_ext 86 */
    {OP_vfnmsub132ps,0x66389e08,"vfnmsub132ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[140]},
    {MOD_EXT, 0x66389e18, "(mod ext 72)", xx, xx, xx, xx, xx, mrm|evex, x, 72},
    {OP_vfnmsub132pd,0x66389e48,"vfnmsub132pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[143]},
    {MOD_EXT, 0x66389e58, "(mod ext 73)", xx, xx, xx, xx, xx, mrm|evex, x, 73},
  }, { /* evex_W_ext 87 */
    {OP_vfnmsub213ps,0x6638ae08,"vfnmsub213ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[146]},
    {MOD_EXT, 0x6638ae18, "(mod ext 74)", xx, xx, xx, xx, xx, mrm|evex, x, 74},
    {OP_vfnmsub213pd,0x6638ae48,"vfnmsub213pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[149]},
    {MOD_EXT, 0x6638ae58, "(mod ext 75)", xx, xx, xx, xx, xx, mrm|evex, x, 75},
  }, { /* evex_W_ext 88 */
    {OP_vfnmsub231ps,0x6638be08,"vfnmsub231ps",Ves,xx,KEw,Hes,Wes,xop|mrm|evex|reqp|ttfv,x,exop[152]},
    {MOD_EXT, 0x6638be18, "(mod ext 76)", xx, xx, xx, xx, xx, mrm|evex, x, 76},
    {OP_vfnmsub231pd,0x6638be48,"vfnmsub231pd",Ved,xx,KEb,Hed,Wed,xop|mrm|evex|reqp|ttfv,x,exop[155]},
    {MOD_EXT, 0x6638be58, "(mod ext 77)", xx, xx, xx, xx, xx, mrm|evex, x, 77},
  }, { /* evex_W_ext 89 */
    {OP_vfnmsub132ss,0x66389f08,"vfnmsub132ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[158]},
    {OP_vfnmsub132ss,0x66389f18,"vfnmsub132ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[159]},
    {OP_vfnmsub132sd,0x66389f48,"vfnmsub132sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[160]},
    {OP_vfnmsub132sd,0x66389f58,"vfnmsub132sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[161]},
  }, { /* evex_W_ext 90 */
    {OP_vfnmsub213ss,0x6638af08,"vfnmsub213ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[162]},
    {OP_vfnmsub213ss,0x6638af18,"vfnmsub213ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[163]},
    {OP_vfnmsub213sd,0x6638af48,"vfnmsub213sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[164]},
    {OP_vfnmsub213sd,0x6638af58,"vfnmsub213sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[165]},
  }, { /* evex_W_ext 91 */
    {OP_vfnmsub231ss,0x6638bf08,"vfnmsub231ss",Vss,xx,KE1b,Hss,Wss,xop|mrm|evex|reqp|ttt1s,x,exop[166]},
    {OP_vfnmsub231ss,0x6638bf18,"vfnmsub231ss",Vss,xx,KE1b,Hss,Uss,xop|mrm|evex|er|reqp|ttt1s,x,exop[167]},
    {OP_vfnmsub231sd,0x6638bf48,"vfnmsub231sd",Vsd,xx,KE1b,Hsd,Wsd,xop|mrm|evex|reqp|ttt1s,x,exop[168]},
    {OP_vfnmsub231sd,0x6638bf58,"vfnmsub231sd",Vsd,xx,KE1b,Hsd,Usd,xop|mrm|evex|er|reqp|ttt1s,x,exop[169]},
  }, { /* evex_W_ext 92 */
    {OP_vpermb,0x66388d08,"vpermb",Ve,xx,KEb,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66388d18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpermw,0x66388d48,"vpermw",Ve,xx,KEw,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66388d58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 93 */
    {OP_vpermd,0x66383608,"vpermd",Vf,xx,KEb,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[93][1]},
    {OP_vpermd,0x66383618,"vpermd",Vf,xx,KEb,Hf,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermq,0x66383648,"vpermq",Vf,xx,KEb,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[93][3]},
    {OP_vpermq,0x66383658,"vpermq",Vf,xx,KEb,Hf,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 94 */
    {OP_vpermps,0x66381608,"vpermps",Vf,xx,KEw,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[94][1]},
    {OP_vpermps,0x66381618,"vpermps",Vf,xx,KEw,Hf,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermpd,0x66381648,"vpermpd",Vf,xx,KEw,Hf,Wf,mrm|evex|reqp|ttfv,x,tevexwb[94][3]},
    {OP_vpermpd,0x66381658,"vpermpd",Vf,xx,KEw,Hf,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 95 */
    {OP_vpermi2ps,0x66387708,"vpermi2ps",Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[95][1]},
    {OP_vpermi2ps,0x66387718,"vpermi2ps",Ve,xx,KEw,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermi2pd,0x66387748,"vpermi2pd",Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[95][3]},
    {OP_vpermi2pd,0x66387758,"vpermi2pd",Ve,xx,KEw,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 96 */
    {OP_vpermi2d,0x66387608,"vpermi2d",Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[96][1]},
    {OP_vpermi2d,0x66387618,"vpermi2d",Ve,xx,KEw,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermi2q,0x66387648,"vpermi2q",Ve,xx,KEb,He,We,mrm|evex|reqp|ttfv,x,tevexwb[96][3]},
    {OP_vpermi2q,0x66387658,"vpermi2q",Ve,xx,KEb,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 97 */
    {OP_vpermi2b,0x66387508,"vpermi2b",Ve,xx,KEq,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387518,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpermi2w,0x66387548,"vpermi2w",Ve,xx,KEd,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387558,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 98 */
    {OP_vpermt2b,0x66387d08,"vpermt2b",Ve,xx,KEw,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387d18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpermt2w,0x66387d48,"vpermt2w",Ve,xx,KEb,He,We,mrm|evex|reqp|ttfvm,x,END_LIST},
    {INVALID, 0x66387d58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 99 */
    {OP_vpermt2d,0x66387e08,"vpermt2d",Ve,xx,KEq,He,We,mrm|evex|reqp|ttfv,x,tevexwb[99][1]},
    {OP_vpermt2d,0x66387e18,"vpermt2d",Ve,xx,KEq,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermt2q,0x66387e48,"vpermt2q",Ve,xx,KEd,He,We,mrm|evex|reqp|ttfv,x,tevexwb[99][3]},
    {OP_vpermt2q,0x66387e58,"vpermt2q",Ve,xx,KEd,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 100 */
    {OP_vpermt2ps,0x66387f08,"vpermt2ps",Ve,xx,KEw,He,We,mrm|evex|reqp|ttfv,x,tevexwb[100][1]},
    {OP_vpermt2ps,0x66387f18,"vpermt2ps",Ve,xx,KEw,He,Md,mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpermt2pd,0x66387f48,"vpermt2pd",Ve,xx,KEb,He,We,mrm|evex|reqp|ttfv,x,tevexwb[100][3]},
    {OP_vpermt2pd,0x66387f58,"vpermt2pd",Ve,xx,KEb,He,Mq,mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 101 */
    {OP_vextractf32x4, 0x663a1908, "vextractf32x4", Wdq, xx, KE4b, Ib, Vdq_f, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a1918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextractf64x2, 0x663a1948, "vextractf64x2", Wdq, xx, KE2b, Ib, Vdq_f, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x663a1958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 102 */
    {OP_vextractf32x8, 0x663a1b08, "vextractf32x8", Wqq, xx,  KEb, Ib, Vqq_oq, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x663a1b18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextractf64x4, 0x663a1b48, "vextractf64x4", Wqq, xx, KE4b, Ib, Vqq_oq, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a1b58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 103 */
    {OP_vextracti32x4, 0x663a3908, "vextracti32x4", Wdq, xx, KE4b, Ib, Vdq_f, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a3918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextracti64x2, 0x663a3948, "vextracti64x2", Wdq, xx, KE2b, Ib, Vdq_f, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x663a3958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 104 */
    {OP_vextracti32x8, 0x663a3b08, "vextracti32x8", Wqq, xx,  KEb, Ib, Vqq_oq, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x663a3b18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vextracti64x4, 0x663a3b48, "vextracti64x4", Wqq, xx, KE4b, Ib, Vqq_oq, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x663a3b58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 105 */
    {OP_vinsertf32x4, 0x663a1808, "vinsertf32x4", Vf, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[170]},
    {INVALID, 0x663a1818,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinsertf64x2, 0x663a1848, "vinsertf64x2", Vf, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt2, x, exop[171]},
    {INVALID, 0x663a1858,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 106 */
    {OP_vinsertf32x8, 0x663a1a08, "vinsertf32x8", Voq, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt8, x, exop[172]},
    {INVALID, 0x663a1a18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinsertf64x4, 0x663a1a48, "vinsertf64x4", Voq, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[173]},
    {INVALID, 0x663a1858,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 107 */
    {OP_vinserti32x4, 0x663a3808, "vinserti32x4", Vf, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[174]},
    {INVALID, 0x663a3818,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinserti64x2, 0x663a3848, "vinserti64x2", Vf, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt2, x, exop[175]},
    {INVALID, 0x663a3858,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 108 */
    {OP_vinserti32x8, 0x663a3a08, "vinserti32x8", Voq, xx, KEw, Ib, Hdq_f, xop|mrm|evex|reqp|ttt8, x, exop[176]},
    {INVALID, 0x663a3a18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vinserti64x4, 0x663a3a48, "vinserti64x4", Voq, xx, KEb, Ib, Hdq_f, xop|mrm|evex|reqp|ttt4, x, exop[177]},
    {INVALID, 0x663a3a58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 109 */
    {OP_vpcmpub, 0x663a3e08, "vpcmpub", KPq, xx, KEq, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[178]},
    {INVALID, 0x663a3e18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpcmpuw, 0x663a3e48, "vpcmpuw", KPd, xx, KEd, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[180]},
    {INVALID, 0x663a3e58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 110 */
    {OP_vpcmpb, 0x663a3f08, "vpcmpb", KPq, xx, KEq, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[179]},
    {INVALID, 0x663a3f18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpcmpw, 0x663a3f48, "vpcmpw", KPd, xx, KEd, Ib, He, xop|evex|mrm|reqp|ttfvm, x, exop[181]},
    {INVALID, 0x663a3f58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 111 */
    {OP_vpcmpud, 0x663a1e08, "vpcmpud", KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[182]},
    {OP_vpcmpud, 0x663a1e18, "vpcmpud", KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[183]},
    {OP_vpcmpuq, 0x663a1e48, "vpcmpuq", KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[186]},
    {OP_vpcmpuq, 0x663a1e58, "vpcmpuq", KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[187]},
  }, { /* evex_W_ext 112 */
    {OP_vpcmpd, 0x663a1f08, "vpcmpd", KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[184]},
    {OP_vpcmpd, 0x663a1f18, "vpcmpd", KPw, xx, KEw, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[185]},
    {OP_vpcmpq, 0x663a1f48, "vpcmpq", KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[188]},
    {OP_vpcmpq, 0x663a1f58, "vpcmpq", KPb, xx, KEb, Ib, He, xop|evex|mrm|reqp|ttfv, x, exop[189]},
  }, { /* evex_W_ext 113 */
    {OP_vpminsd, 0x66383908, "vpminsd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[113][1]},
    {OP_vpminsd, 0x66383918, "vpminsd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpminsq, 0x66383948, "vpminsq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[113][3]},
    {OP_vpminsq, 0x66383958, "vpminsq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 114 */
    {OP_vpmaxsd,  0x66383d08, "vpmaxsd", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[114][1]},
    {OP_vpmaxsd,  0x66383d18, "vpmaxsd", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpmaxsq,  0x66383d48, "vpmaxsq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[114][3]},
    {OP_vpmaxsq,  0x66383d58, "vpmaxsq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 115 */
    {OP_vpminud,  0x66383b08, "vpminud", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[115][1]},
    {OP_vpminud,  0x66383b18, "vpminud", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpminuq,  0x66383b48, "vpminuq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[115][3]},
    {OP_vpminuq,  0x66383b58, "vpminuq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 116 */
    {OP_vpmaxud,  0x66383f08, "vpmaxud", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[116][1]},
    {OP_vpmaxud,  0x66383f18, "vpmaxud", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpmaxuq,  0x66383f48, "vpmaxuq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[116][3]},
    {OP_vpmaxuq,  0x66383f58, "vpmaxuq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 117 */
    {OP_vprolvd, 0x66381508, "vprolvd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[117][1]},
    {OP_vprolvd, 0x66381518, "vprolvd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprolvq, 0x66381548, "vprolvq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[117][3]},
    {OP_vprolvq, 0x66381558, "vprolvq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 118 */
    {OP_vprold, 0x660f7221, "vprold", He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[118][1]},
    {OP_vprold, 0x660f7231, "vprold", He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprolq, 0x660f7261, "vprolq", He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[118][3]},
    {OP_vprolq, 0x660f7271, "vprolq", He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 119 */
    {OP_vprorvd, 0x66381408, "vprorvd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[119][1]},
    {OP_vprorvd, 0x66381418, "vprorvd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprorvq, 0x66381448, "vprorvq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[119][3]},
    {OP_vprorvq, 0x66381458, "vprorvq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 120 */
    {OP_vprord, 0x660f7220, "vprord", He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[120][1]},
    {OP_vprord, 0x660f7230, "vprord", He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vprorq, 0x660f7260, "vprorq", He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[120][3]},
    {OP_vprorq, 0x660f7270, "vprorq", He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 121 */
    {OP_vpsrad, 0x660fe200, "vpsrad", Ve, xx, KEw, He, Wdq, mrm|evex|ttm128, x, tevexwb[122][0]},
    {INVALID, 0x660fe210,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsraq, 0x660fe240, "vpsraq", Ve, xx, KEb, He, Wdq, mrm|evex|ttm128, x, tevexwb[122][2]},
    {INVALID, 0x660fe250,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 122 */
    {OP_vpsrad, 0x660f7224, "vpsrad", He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[122][1]},
    {OP_vpsrad, 0x660f7234, "vpsrad", He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vpsraq, 0x660f7264, "vpsraq", He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[122][3]},
    {OP_vpsraq, 0x660f7274, "vpsraq", He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 123 */
    {OP_vpsrld, 0x660fd200, "vpsrld", Ve, xx, KEw, He, Wdq, mrm|evex|ttm128, x, tevexwb[124][0]},
    {INVALID, 0x660fd210,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660fd240,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660fd250,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 124 */
    {OP_vpsrld, 0x660f7222, "vpsrld", He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[124][1]},
    {OP_vpsrld, 0x660f7232, "vpsrld", He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x660f7262,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f7272,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 125 */
    {INVALID, 0x660fd300,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660fd310,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsrlq, 0x660fd340, "vpsrlq", Ve, xx, KEb, He, Wdq, mrm|evex|ttm128, x, tevexwb[126][2]},
    {INVALID, 0x660fd350,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 126 */
    {INVALID, 0x660f7322,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x660f7332,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsrlq, 0x660f7362, "vpsrlq", He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[126][3]},
    {OP_vpsrlq, 0x660f7372, "vpsrlq", He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 127 */
    {INVALID, 0x66381108,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381118,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsravw, 0x66381148, "vpsravw", Ve, xx, KEb, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66381158,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 128 */
    {OP_vpsravd, 0x66384608, "vpsravd", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[128][1]},
    {OP_vpsravd, 0x66384618, "vpsravd", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpsravq, 0x66384648, "vpsravq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[128][3]},
    {OP_vpsravq, 0x66384658, "vpsravq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 129 */
    {OP_vpsrlvd,0x66384508, "vpsrlvd", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[129][1]},
    {OP_vpsrlvd,0x66384518, "vpsrlvd", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpsrlvq,0x66384548, "vpsrlvq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[129][3]},
    {OP_vpsrlvq,0x66384558, "vpsrlvq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 130 */
    {INVALID, 0x66381208,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381218,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpsllvw, 0x66381248,"vpsllvw", Ve, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66381258,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 131 */
    {OP_vpsllvd, 0x66384708, "vpsllvd", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv,x,tevexwb[131][1]},
    {OP_vpsllvd, 0x66384718, "vpsllvd", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vpsllvq, 0x66384748, "vpsllvq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv,x,tevexwb[131][3]},
    {OP_vpsllvq, 0x66384758, "vpsllvq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 132 */
    {OP_vrcp14ps, 0x66384c08, "vrcp14ps", Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv,x,tevexwb[132][1]},
    {OP_vrcp14ps, 0x66384c18, "vrcp14ps", Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv,x,END_LIST},
    {OP_vrcp14pd, 0x66384c48, "vrcp14pd", Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv,x,tevexwb[132][3]},
    {OP_vrcp14pd, 0x66384c58, "vrcp14pd", Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv,x,END_LIST},
  }, { /* evex_W_ext 133 */
    {OP_vrcp14ss, 0x66384d08, "vrcp14ss", Vdq, xx, KE1b, H12_dq, Wss, mrm|evex|reqp|ttt1s,x,END_LIST},
    {INVALID, 0x66384d18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vrcp14sd, 0x66384d48, "vrcp14sd", Vdq, xx, KE1b, Hsd, Wsd, mrm|evex|reqp|ttt1s,x,END_LIST},
    {INVALID, 0x66384d58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 134 */
    {OP_vrcp28ps, 0x6638ca08, "vrcp28ps", Voq, xx, KEw, Woq, xx, mrm|evex|reqp|ttfv,x,modx[78][0]},
    {MOD_EXT, 0x6638ca18, "(mod ext 78)", xx, xx, xx, xx, xx, mrm|evex, x, 78},
    {OP_vrcp28pd, 0x6638ca48, "vrcp28pd", Voq, xx, KEb, Woq, xx, mrm|evex|reqp|ttfv,x,modx[79][0]},
    {MOD_EXT, 0x6638ca58, "(mod ext 79)", xx, xx, xx, xx, xx, mrm|evex, x, 79},
  }, { /* evex_W_ext 135 */
    {OP_vrcp28ss, 0x6638cb08, "vrcp28ss", Vdq, xx, KE1b, H12_dq, Wss, mrm|evex|reqp|ttt1s,x,tevexwb[135][1]},
    {OP_vrcp28ss, 0x6638cb18, "vrcp28ss", Vdq, xx, KE1b, H12_dq, Uss, mrm|evex|sae|reqp|ttt1s,x,END_LIST},
    {OP_vrcp28sd, 0x6638cb48, "vrcp28sd", Vdq, xx, KE1b, Hsd, Wsd, mrm|evex|reqp|ttt1s,x,tevexwb[135][3]},
    {OP_vrcp28sd, 0x6638cb58, "vrcp28sd", Vdq, xx, KE1b, Hsd, Usd, mrm|evex|sae|reqp|ttt1s,x,END_LIST},
  }, { /* evex_W_ext 136 */
    {OP_vmovd, 0x660f6e00, "vmovd", Vdq, xx, Ed, xx, xx, mrm|evex|ttt1s, x, tevexwb[137][0]},
    {INVALID, 0x660f6e10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovq, 0x660f6e40, "vmovq", Vdq, xx, Ey, xx, xx, mrm|evex|ttt1s, x, tevexwb[137][2]},
    {INVALID, 0x660f6e50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 137 */
    {OP_vmovd, 0x660f7e00, "vmovd", Ed, xx, Vd_dq, xx, xx, mrm|evex|ttt1s, x, END_LIST},
    {INVALID, 0x660f7e10,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vmovq, 0x660f7e40, "vmovq", Ey, xx, Vq_dq, xx, xx, mrm|evex|ttt1s, x, END_LIST},
    {INVALID, 0x660f7e50,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 138 */
    {OP_vpmovm2b, 0xf3382808, "vpmovm2b", Ve, xx, KQq, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382818,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovm2w, 0xf3382848, "vpmovm2w", Ve, xx, KQd, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382858,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 139 */
    {OP_vpmovm2d, 0xf3383808, "vpmovm2d", Ve, xx, KQw, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383818,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovm2q, 0xf3383848, "vpmovm2q", Ve, xx, KQb, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383858,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 140 */
    {OP_vpmovb2m, 0xf3382908, "vpmovb2m", KPq, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovw2m, 0xf3382948, "vpmovw2m", KPd, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3382958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 141 */
    {OP_vpmovd2m, 0xf3383908, "vpmovd2m", KPw, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpmovq2m, 0xf3383948, "vpmovq2m", KPb, xx, Ue, xx, xx, mrm|evex|ttnone, x, END_LIST},
    {INVALID, 0xf3383958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 142 */
    {OP_vshuff32x4, 0x663a2308, "vshuff32x4", Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[204]},
    {OP_vshuff32x4, 0x663a2318, "vshuff32x4", Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[205]},
    {OP_vshuff64x2, 0x663a2348, "vshuff64x2", Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[206]},
    {OP_vshuff64x2, 0x663a2358, "vshuff64x2", Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[207]},
  }, { /* evex_W_ext 143 */
    {OP_vshufi32x4, 0x663a4308, "vshufi32x4", Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[208]},
    {OP_vshufi32x4, 0x663a4318, "vshufi32x4", Vfs, xx, KEw, Ib, Hfs, xop|mrm|evex|reqp|ttfv, x, exop[209]},
    {OP_vshufi64x2, 0x663a4348, "vshufi64x2", Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[210]},
    {OP_vshufi64x2, 0x663a4358, "vshufi64x2", Vfd, xx, KEb, Ib, Hfd, xop|mrm|evex|reqp|ttfv, x, exop[211]},
  }, { /* evex_W_ext 144 */
    {OP_vpinsrd, 0x663a2208, "vpinsrd", Vdq, xx, H12_8_dq, Ey, Ib, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a2218,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpinsrq, 0x663a2248, "vpinsrq", Vdq, xx, H12_8_dq, Ey, Ib, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a2258,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 145 */
    {OP_vpextrd, 0x663a1608, "vpextrd",  Ey, xx, Vd_q_dq, Ib, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a1618,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpextrq, 0x663a1648, "vpextrq",  Ey, xx, Vd_q_dq, Ib, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a1658,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 146 */
    {OP_vpabsd, 0x66381e08, "vpabsd",   Ve, xx, KEw, We, xx, mrm|evex|ttfv, x, tevexwb[146][1]},
    {OP_vpabsd, 0x66381e18, "vpabsd",   Ve, xx, KEw, Md, xx, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0x66381e48,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381e58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 147 */
    {INVALID, 0x66381f08,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {INVALID, 0x66381f18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpabsq, 0x66381f48, "vpabsq",   Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[147][3]},
    {OP_vpabsq, 0x66381f58, "vpabsq",   Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 148 */
    {OP_vbroadcastf32x2, 0x66381908, "vbroadcastf32x2", Vf, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66381918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcastsd, 0x66381948, "vbroadcastsd", Vf, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66381958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 149 */
    {OP_vbroadcastf32x4, 0x66381a08, "vbroadcastf32x4", Vf, xx, KEw, Mdq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66381a18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcastf64x2, 0x66381a48, "vbroadcastf64x2", Vf, xx, KEb, Mdq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66381a58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 150 */
    {OP_vbroadcastf32x8, 0x66381b08, "vbroadcastf32x8", Voq, xx, KEd, Mqq, xx, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x66381b18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcastf64x4, 0x66381b48, "vbroadcastf64x4", Voq, xx, KEb, Mqq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66381b58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 151 */
    {OP_vpbroadcastd, 0x66387c08, "vpbroadcastd", Ve, xx, KEw, Ed, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66387c18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpbroadcastq, 0x66387c48, "vpbroadcastq", Ve, xx, KEb, Eq, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66387c58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 152 */
    {OP_vbroadcasti32x2, 0x66385908, "vbroadcasti32x2", Ve, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66385918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpbroadcastq, 0x66385948, "vpbroadcastq", Ve, xx, KEb, Wq_dq, xx, mrm|evex|reqp|ttt1s, x, tevexwb[151][2]},
    {INVALID, 0x66385958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 153 */
    {OP_vbroadcasti32x4, 0x66385a08, "vbroadcasti32x4", Vf, xx, KEw, Mdq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66385a18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcasti64x2, 0x66385a48, "vbroadcasti64x2", Vf, xx, KEw, Mdq, xx, mrm|evex|reqp|ttt2, x, END_LIST},
    {INVALID, 0x66385a58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 154 */
    {OP_vbroadcasti32x8, 0x66385b08, "vbroadcasti32x8", Vf, xx, KEw, Mqq, xx, mrm|evex|reqp|ttt8, x, END_LIST},
    {INVALID, 0x66385b18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vbroadcasti64x4, 0x66385b48, "vbroadcasti64x4", Vf, xx, KEb, Mqq, xx, mrm|evex|reqp|ttt4, x, END_LIST},
    {INVALID, 0x66385b58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 155 */
    {OP_valignd, 0x663a0308, "valignd", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[212]},
    {OP_valignd, 0x663a0318, "valignd", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[213]},
    {OP_valignq, 0x663a0348, "valignq", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[214]},
    {OP_valignq, 0x663a0358, "valignq", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[215]},
  }, { /* evex_W_ext 156 */
    {OP_vblendmps, 0x66386508, "vblendmps", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[156][1]},
    {OP_vblendmps, 0x66386518, "vblendmps", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vblendmpd, 0x66386548, "vblendmpd", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[156][3]},
    {OP_vblendmpd, 0x66386558, "vblendmpd", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 157 */
    {OP_vcompressps, 0x66388a08, "vcompressps", We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388a18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vcompresspd, 0x66388a48, "vcompresspd", We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388a58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 158 */
    {OP_vexpandps, 0x66388808, "vexpandps", We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388818,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vexpandpd, 0x66388848, "vexpandpd", We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388858,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 159 */
    {OP_vfixupimmps, 0x663a5408, "vfixupimmps", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[216]},
    {MOD_EXT, 0x663a5418, "(mod ext 80)", xx, xx, xx, xx, xx, mrm|evex, x, 80},
    {OP_vfixupimmpd, 0x663a5448, "vfixupimmpd", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[219]},
    {MOD_EXT, 0x663a5458, "(mod ext 81)", xx, xx, xx, xx, xx, mrm|evex, x, 81},
  }, { /* evex_W_ext 160 */
    {OP_vfixupimmss, 0x663a5508, "vfixupimmss", Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|reqp|ttt1s, x, exop[222]},
    {OP_vfixupimmss, 0x663a5518, "vfixupimmss", Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[223]},
    {OP_vfixupimmsd, 0x663a5548, "vfixupimmsd", Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|reqp|ttt1s, x, exop[224]},
    {OP_vfixupimmsd, 0x663a5558, "vfixupimmsd", Vdq, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[225]},
  }, { /* evex_W_ext 161 */
    {OP_vgetexpps, 0x66384208, "vgetexpps", Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, modx[82][0]},
    {MOD_EXT, 0x66384218, "(mod ext 82)", xx, xx, xx, xx, xx, mrm|evex, x, 82},
    {OP_vgetexppd, 0x66384248, "vgetexppd", Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, modx[83][0]},
    {MOD_EXT, 0x66384258, "(mod ext 83)", xx, xx, xx, xx, xx, mrm|evex, x, 83},
  }, { /* evex_W_ext 162 */
    {OP_vgetexpss, 0x66384308, "vgetexpss", Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, tevexwb[162][1]},
    {OP_vgetexpss, 0x66384318, "vgetexpss", Vdq, xx, KE1b, H12_dq, Ud_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
    {OP_vgetexpsd, 0x66384348, "vgetexpsd", Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, tevexwb[162][3]},
    {OP_vgetexpsd, 0x66384358, "vgetexpsd", Vdq, xx, KE1b,    Hsd, Uq_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 163 */
    {OP_vgetmantps, 0x663a2608, "vgetmantps", Ve, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, modx[84][0]},
    {MOD_EXT, 0x663a2618, "(mod ext 84)", xx, xx, xx, xx, xx, mrm|evex, x, 84},
    {OP_vgetmantpd, 0x663a2648, "vgetmantpd", Ve, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, modx[85][0]},
    {MOD_EXT, 0x663a2658, "(mod ext 85)", xx, xx, xx, xx, xx, mrm|evex, x, 85},
  }, { /* evex_W_ext 164 */
    {OP_vgetmantss, 0x663a2708, "vgetmantss", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[226]},
    {OP_vgetmantss, 0x663a2718, "vgetmantss", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[227]},
    {OP_vgetmantsd, 0x663a2748, "vgetmantsd", Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[228]},
    {OP_vgetmantsd, 0x663a2758, "vgetmantsd", Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[229]},
  }, { /* evex_W_ext 165 */
    {OP_vpblendmb, 0x66386608, "vpblendmb", Ve, xx, KEq, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
    {INVALID, 0x66386618,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpblendmw, 0x66386648, "vpblendmw", Ve, xx, KEd, He, We, mrm|evex|reqp|ttfvm, x, END_LIST},
    {INVALID, 0x66386658,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 166 */
    {OP_vpblendmd, 0x66386408, "vpblendmd", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[166][1]},
    {OP_vpblendmd, 0x66386418, "vpblendmd", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpblendmq, 0x66386448, "vpblendmq", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[166][3]},
    {OP_vpblendmq, 0x66386458, "vpblendmq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 167 */
    {OP_vpcompressd, 0x66388b08, "vpcompressd", We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388b18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpcompressq, 0x66388b48, "vpcompressq", We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388b58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 168 */
    {OP_vpexpandd, 0x66388908, "vpexpandd", We, xx, KEw, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388918,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpexpandq, 0x66388948, "vpexpandq", We, xx, KEb, Ve, xx, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66388958,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 169 */
    {OP_vptestmb, 0x66382608, "vptestmb", KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66382618,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vptestmw, 0x66382648, "vptestmw", KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0x66382658,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 170 */
    {OP_vptestmd, 0x66382708, "vptestmd", KPw, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[170][1]},
    {OP_vptestmd, 0x66382718, "vptestmd", KPw, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vptestmq, 0x66382748, "vptestmq", KPb, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[170][3]},
    {OP_vptestmq, 0x66382758, "vptestmq", KPb, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 171 */
    {OP_vptestnmb, 0xf3382608, "vptestnmb", KPq, xx, KEq, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf3382618,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vptestnmw, 0xf3382648, "vptestnmw", KPd, xx, KEd, He, We, mrm|evex|ttfvm, x, END_LIST},
    {INVALID, 0xf3382658,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 172 */
    {OP_vptestnmd, 0xf3382708, "vptestnmd", KPw, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[172][1]},
    {OP_vptestnmd, 0xf3382718, "vptestnmd", KPw, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vptestnmq, 0xf3382748, "vptestnmq", KPb, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[172][3]},
    {OP_vptestnmq, 0xf3382758, "vptestnmq", KPb, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 173 */
    {OP_vrangeps, 0x663a5008, "vrangeps", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[230]},
    {MOD_EXT, 0x663a5018, "(mod ext 86)", xx, xx, xx, xx, xx, mrm|evex, x, 86},
    {OP_vrangepd, 0x663a5048, "vrangepd", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[233]},
    {MOD_EXT, 0x663a5058, "(mod ext 87)", xx, xx, xx, xx, xx, mrm|evex, x, 87},
  }, { /* evex_W_ext 174 */
    {OP_vrangess, 0x663a5108, "vrangess", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[236]},
    {OP_vrangess, 0x663a5118, "vrangess", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[237]},
    {OP_vrangesd, 0x663a5148, "vrangesd", Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[238]},
    {OP_vrangesd, 0x663a5158, "vrangesd", Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[239]},
  }, { /* evex_W_ext 175 */
    {OP_vreduceps, 0x663a5608, "vreduceps", Ve, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, modx[88][0]},
    {MOD_EXT, 0x663a5618, "(mod ext 88)", xx, xx, xx, xx, xx, mrm|evex, x, 88},
    {OP_vreducepd, 0x663a5648, "vreducepd", Ve, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, modx[89][0]},
    {MOD_EXT, 0x663a5658, "(mod ext 89)", xx, xx, xx, xx, xx, mrm|evex, x, 89},
  }, { /* evex_W_ext 176 */
    {OP_vreducess, 0x663a5708, "vreducess", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[240]},
    {OP_vreducess, 0x663a5718, "vreducess", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[241]},
    {OP_vreducesd, 0x663a5748, "vreducesd", Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[242]},
    {OP_vreducesd, 0x663a5758, "vreducesd", Vdq, xx, KE1b, Ib,    Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[243]},
  }, { /* evex_W_ext 177 */
    {OP_vrsqrt14ps, 0x66384e08, "vrsqrt14ps", Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[177][1]},
    {OP_vrsqrt14ps, 0x66384e18, "vrsqrt14ps", Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vrsqrt14pd, 0x66384e48, "vrsqrt14pd", Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[177][3]},
    {OP_vrsqrt14pd, 0x66384e58, "vrsqrt14pd", Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 178 */
    {OP_vrsqrt14ss, 0x66384f08, "vrsqrt14ss", Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66384f18,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vrsqrt14sd, 0x66384f48, "vrsqrt14sd", Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x66384f58,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 179 */
    {OP_vrsqrt28ps, 0x6638cc08, "vrsqrt28ps", Voq, xx, KEw, Woq, xx, mrm|evex|reqp|ttfv, x, modx[90][0]},
    {MOD_EXT, 0x6638cc18, "(mod ext 90)", xx, xx, xx, xx, xx, mrm|evex, x, 90},
    {OP_vrsqrt28pd, 0x6638cc48, "vrsqrt28pd", Voq, xx, KEb, Woq, xx, mrm|evex|reqp|ttfv, x, modx[91][0]},
    {MOD_EXT, 0x6638cc58, "(mod ext 91)", xx, xx, xx, xx, xx, mrm|evex, x, 91},
  }, { /* evex_W_ext 180 */
    {OP_vrsqrt28ss, 0x6638cd08, "vrsqrt28ss", Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, tevexwb[180][1]},
    {OP_vrsqrt28ss, 0x6638cd18, "vrsqrt28ss", Vdq, xx, KE1b, H12_dq, Ud_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
    {OP_vrsqrt28sd, 0x6638cd48, "vrsqrt28sd", Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, tevexwb[180][3]},
    {OP_vrsqrt28sd, 0x6638cd58, "vrsqrt28sd", Vdq, xx, KE1b,    Hsd, Uq_dq, mrm|evex|sae|reqp|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 181 */
    {OP_vscalefps, 0x66382c08, "vscalefps", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, modx[92][0]},
    {MOD_EXT, 0x66382c18, "(mod ext 92)", xx, xx, xx, xx, xx, mrm|evex, x, 92},
    {OP_vscalefpd, 0x66382c48, "vscalefpd", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, modx[93][0]},
    {MOD_EXT, 0x66382c58, "(mod ext 93)", xx, xx, xx, xx, xx, mrm|evex, x, 93},
  }, { /* evex_W_ext 182 */
    {OP_vscalefss, 0x66382d08, "vscalefss", Vdq, xx, KE1b, H12_dq, Wd_dq, mrm|evex|reqp|ttt1s, x, tevexwb[182][1]},
    {OP_vscalefss, 0x66382d18, "vscalefss", Vdq, xx, KE1b, H12_dq, Ud_dq, mrm|evex|er|reqp|ttt1s, x, END_LIST},
    {OP_vscalefsd, 0x66382d48, "vscalefsd", Vdq, xx, KE1b,    Hsd, Wq_dq, mrm|evex|reqp|ttt1s, x, tevexwb[182][1]},
    {OP_vscalefsd, 0x66382d58, "vscalefsd", Vdq, xx, KE1b,    Hsd, Uq_dq, mrm|evex|er|reqp|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 183 */
    {OP_vfpclassps, 0x663a6608, "vfpclassps", KPw, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, tevexwb[183][1]},
    {OP_vfpclassps, 0x663a6618, "vfpclassps", KPw, xx, KEw, Ib, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vfpclasspd, 0x663a6648, "vfpclasspd", KPb, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, tevexwb[183][3]},
    {OP_vfpclasspd, 0x663a6658, "vfpclasspd", KPb, xx, KEb, Ib, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 184 */
    {OP_vfpclassss, 0x663a6708, "vfpclassss", KP1b, xx, KE1b, Ib, Wd_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a6718,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vfpclasssd, 0x663a6748, "vfpclasssd", KP1b, xx, KE1b, Ib, Wq_dq, mrm|evex|reqp|ttt1s, x, END_LIST},
    {INVALID, 0x663a6758,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 185 */
    {OP_vexp2ps, 0x6638c808, "vexp2ps", Voq, xx, KEw, Woq, xx, mrm|evex|reqp|ttfv, x, modx[94][0]},
    {MOD_EXT, 0x6638c818, "(mod ext 94)", xx, xx, xx, xx, xx, mrm|evex, x, 94},
    {OP_vexp2pd, 0x6638c848, "vexp2pd", Voq, xx, KEb, Woq, xx, mrm|evex|reqp|ttfv, x, modx[95][0]},
    {MOD_EXT, 0x6638c858, "(mod ext 95)", xx, xx, xx, xx, xx, mrm|evex, x, 95},
  }, { /* evex_W_ext 186 */
    {OP_vpconflictd, 0x6638c408, "vpconflictd", Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[186][1]},
    {OP_vpconflictd, 0x6638c418, "vpconflictd", Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vpconflictq, 0x6638c448, "vpconflictq", Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[186][3]},
    {OP_vpconflictq, 0x6638c458, "vpconflictq", Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 187 */
    {OP_vplzcntd, 0x66384408, "vplzcntd", Ve, xx, KEw, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[187][1]},
    {OP_vplzcntd, 0x66384418, "vplzcntd", Ve, xx, KEw, Md, xx, mrm|evex|reqp|ttfv, x, END_LIST},
    {OP_vplzcntq, 0x66384448, "vplzcntq", Ve, xx, KEb, We, xx, mrm|evex|reqp|ttfv, x, tevexwb[187][3]},
    {OP_vplzcntq, 0x66384458, "vplzcntq", Ve, xx, KEb, Mq, xx, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 188 */
    {OP_vpternlogd, 0x663a2508, "vpternlogd", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[250]},
    {OP_vpternlogd, 0x663a2518, "vpternlogd", Ve, xx, KEw, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[251]},
    {OP_vpternlogq, 0x663a2548, "vpternlogq", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[252]},
    {OP_vpternlogq, 0x663a2558, "vpternlogq", Ve, xx, KEb, Ib, He, xop|mrm|evex|reqp|ttfv, x, exop[253]},
  }, { /* evex_W_ext 189 */
    /* XXX: OP_v*gather* raise #UD if any pair of the index, mask, or destination
     * registers are identical.  We don't bother trying to detect that.
     */
    {OP_vpgatherdd, 0x66389008, "vpgatherdd", Ve, KEw, KEw, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389018,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpgatherdq, 0x66389048, "vpgatherdq", Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389058,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 190 */
    {OP_vpgatherqd, 0x66389108, "vpgatherqd", Ve, KEb, KEb, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389118,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpgatherqq, 0x66389148, "vpgatherqq", Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389158,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 191 */
    {OP_vgatherdps, 0x66389208, "vgatherdps", Ve, KEw, KEw, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389218,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherdpd, 0x66389248, "vgatherdpd", Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389258,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 192 */
    {OP_vgatherqps, 0x66389308, "vgatherqps", Ve, KEb, KEb, MVd, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389318,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherqpd, 0x66389348, "vgatherqpd", Ve, KEb, KEb, MVq, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x66389358,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 193 */
    /* XXX: OP_v*scatter* raise #UD if any pair of the index, mask, or destination
     * registers are identical.  We don't bother trying to detect that.
     */
    {OP_vpscatterdd, 0x6638a008, "vpscatterdd", MVd, KEw, KEw, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a018,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpscatterdq, 0x6638a048, "vpscatterdq", MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a058,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 194 */
    {OP_vpscatterqd, 0x6638a108, "vpscatterqd", MVd, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a118,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vpscatterqq, 0x6638a148, "vpscatterqq", MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a158,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 195 */
    {OP_vscatterdps, 0x6638a208, "vscatterdps", MVd, KEw, KEw, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a218,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterdpd, 0x6638a248, "vscatterdpd", MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a258,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 196 */
    {OP_vscatterqps, 0x6638a308, "vscatterqps", MVd, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a318,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterqpd, 0x6638a348, "vscatterqpd", MVq, KEb, KEb, Ve, xx, mrm|evex|reqp|ttnone|nok0, x, END_LIST},
    {INVALID, 0x6638a358,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
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
    {OP_vgatherpf0dps, 0x6638c629, "vgatherpf0dps", xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c639,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf0dpd, 0x6638c669, "vgatherpf0dpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c679,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 198 */
    {OP_vgatherpf0qps, 0x6638c729, "vgatherpf0qps", xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c739,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf0qpd, 0x6638c769, "vgatherpf0qpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c779,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 199 */
    {OP_vgatherpf1dps, 0x6638c62a, "vgatherpf1dps", xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c6ea,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf1dpd, 0x6638c66a, "vgatherpf1dpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c67a,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 200  */
    {OP_vgatherpf1qps, 0x6638c72a, "vgatherpf1qps", xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c731,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vgatherpf1qpd, 0x6638c76a, "vgatherpf1qpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c77a,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 201 */
    {OP_vscatterpf0dps, 0x6638c62d, "vscatterpf0dps", xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c63e,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf0dpd, 0x6638c66d, "vscatterpf0dpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c67d,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 202 */
    {OP_vscatterpf0qps, 0x6638c72d, "vscatterpf0qps", xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c73d,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf0qpd, 0x6638c76d, "vscatterpf0qpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c77d,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 203 */
    {OP_vscatterpf1dps, 0x6638c62e, "vscatterpf1dps", xx, xx, KEw, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c63e,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf1dpd, 0x6638c66e, "vscatterpf1dpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsiby|ttt1s, x, END_LIST},
    {INVALID, 0x6638c67e,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 204  */
    {OP_vscatterpf1qps, 0x6638c72e, "vscatterpf1qps", xx, xx, KEb, MVd, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c7e3,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
    {OP_vscatterpf1qpd, 0x6638c76e, "vscatterpf1qpd", xx, xx, KEb, MVq, xx, mrm|evex|reqp|vsibz|ttt1s, x, END_LIST},
    {INVALID, 0x6638c77e,"(bad)", xx,xx,xx,xx,xx,no,x,NA},
  }, { /* evex_W_ext 205 */
    {OP_vandps,  0x0f5400, "vandps",  Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[205][1]},
    {OP_vandps,  0x0f5410, "vandps",  Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vandpd,  0x660f5440, "vandpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[205][3]},
    {OP_vandpd,  0x660f5450, "vandpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 206 */
    {OP_vandnps, 0x0f5500, "vandnps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[206][1]},
    {OP_vandnps, 0x0f5510, "vandnps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vandnpd, 0x660f5540, "vandnpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[206][3]},
    {OP_vandnpd, 0x660f5550, "vandnpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 207 */
    {OP_vorps, 0x0f5600, "vorps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[207][1]},
    {OP_vorps, 0x0f5610, "vorps", Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vorpd, 0x660f5640, "vorpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[207][3]},
    {OP_vorpd, 0x660f5650, "vorpd", Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 208 */
    {OP_vxorps, 0x0f5700, "vxorps",  Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, tevexwb[208][1]},
    {OP_vxorps, 0x0f5710, "vxorps",  Ves, xx, KEw, Hes, Md, mrm|evex|ttfv, x, END_LIST},
    {OP_vxorpd, 0x660f5740, "vxorpd",  Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, tevexwb[208][3]},
    {OP_vxorpd, 0x660f5750, "vxorpd",  Ved, xx, KEb, Hed, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 209 */
    {OP_vaddps, 0x0f5800, "vaddps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[96][0]},
    {MOD_EXT, 0x0f5810, "(mod ext 96)", xx, xx, xx, xx, xx, mrm|evex, x, 96},
    {OP_vaddpd, 0x660f5840, "vaddpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[97][0]},
    {MOD_EXT, 0x660f5850, "(mod ext 97)", xx, xx, xx, xx, xx, mrm|evex, x, 97},
  }, { /* evex_W_ext 210 */
    {OP_vmulps, 0x0f5900, "vmulps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[98][0]},
    {MOD_EXT, 0x0f5910, "(mod ext 98)", xx, xx, xx, xx, xx, mrm|evex, x, 98},
    {OP_vmulpd, 0x660f5940, "vmulpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[99][0]},
    {MOD_EXT, 0x660f5950, "(mod ext 99)", xx, xx, xx, xx, xx, mrm|evex, x, 99},
  }, { /* evex_W_ext 211 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2pd, 0x0f5a00, "vcvtps2pd", Ved, xx, KEb, Wh_e, xx, mrm|evex|tthv, x, modx[100][0]},
    {MOD_EXT, 0x0f5a10, "(mod ext 100)", xx, xx, xx, xx, xx, mrm|evex, x, 100},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtpd2ps, 0x660f5a40, "vcvtpd2ps", Ves, xx, KEw, Wed, xx, mrm|evex|ttfv, x, modx[101][0]},
    {MOD_EXT, 0x660f5a50, "(mod ext 101)", xx, xx, xx, xx, xx, mrm|evex, x, 101},
  }, { /* evex_W_ext 212 */
    {OP_vsubps, 0x0f5c00, "vsubps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[102][0]},
    {MOD_EXT, 0x0f5c10, "(mod ext 102)", xx, xx, xx, xx, xx, mrm|evex, x, 102},
    {OP_vsubpd, 0x660f5c40, "vsubpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[103][0]},
    {MOD_EXT, 0x660f5c50, "(mod ext 103)", xx, xx, xx, xx, xx, mrm|evex, x, 103},
  }, { /* evex_W_ext 213 */
    {OP_vminps, 0x0f5d00, "vminps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[104][0]},
    {MOD_EXT, 0x0f5d10, "(mod ext 104)", xx, xx, xx, xx, xx, mrm|evex, x, 104},
    {OP_vminpd, 0x660f5d40, "vminpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[105][0]},
    {MOD_EXT, 0x660f5d50, "(mod ext 105)", xx, xx, xx, xx, xx, mrm|evex, x, 105},
  }, { /* evex_W_ext 214 */
    {OP_vdivps, 0x0f5e00, "vdivps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[106][0]},
    {MOD_EXT, 0x0f5e10, "(mod ext 106)", xx, xx, xx, xx, xx, mrm|evex, x, 106},
    {OP_vdivpd, 0x660f5e40, "vdivpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[107][0]},
    {MOD_EXT, 0x660f5e50, "(mod ext 107)", xx, xx, xx, xx, xx, mrm|evex, x, 107},
  }, { /* evex_W_ext 215 */
    {OP_vmaxps,   0x0f5f00, "vmaxps", Ves, xx, KEw, Hes, Wes, mrm|evex|ttfv, x, modx[108][0]},
    {MOD_EXT, 0x0f5f10, "(mod ext 108)", xx, xx, xx, xx, xx, mrm|evex, x, 108},
    {OP_vmaxpd, 0x660f5f40, "vmaxpd", Ved, xx, KEb, Hed, Wed, mrm|evex|ttfv, x, modx[109][0]},
    {MOD_EXT, 0x660f5f50, "(mod ext 109)", xx, xx, xx, xx, xx, mrm|evex, x, 109},
  }, { /* evex_W_ext 216 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpcklqdq, 0x660f6c40, "vpunpcklqdq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[216][3]},
    {OP_vpunpcklqdq, 0x660f6c50, "vpunpcklqdq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 217 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmuludq, 0x660ff440, "vpmuludq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[217][3]},
    {OP_vpmuludq, 0x660ff450, "vpmuludq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 218 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrndscalepd, 0x663a0948, "vrndscalepd",  Ve, xx, KEb, Ib, We, mrm|evex|reqp|ttfv, x, modx[111][0]},
    {MOD_EXT, 0x663a0958, "(mod ext 111)", xx, xx, xx, xx, xx, mrm|evex, x, 111},
  }, { /* evex_W_ext 219 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpunpckhqdq, 0x660f6d40, "vpunpckhqdq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[219][3]},
    {OP_vpunpckhqdq, 0x660f6d50, "vpunpckhqdq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 220 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmadd52luq, 0x6638b448, "vpmadd52luq", Ve, xx, KEb, He, Wed, mrm|evex|reqp|ttfv, x, tevexwb[220][3]},
    {OP_vpmadd52luq, 0x6638b458, "vpmadd52luq", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 221 */
    {OP_vshufps, 0x0fc600, "vshufps", Ves, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[200]},
    {OP_vshufps, 0x0fc610, "vshufps", Ves, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[201]},
    {OP_vshufpd, 0x660fc640, "vshufpd", Ved, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[202]},
    {OP_vshufpd, 0x660fc650, "vshufpd", Ved, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[203]},
  }, { /* evex_W_ext 222 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvttpd2dq,0x660fe640, "vcvttpd2dq", Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[113][0]},
    {MOD_EXT, 0x660fe650, "(mod ext 113)", xx, xx, xx, xx, xx, mrm|evex, x, 113},
  }, { /* evex_W_ext 223 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtpd2dq, 0xf20fe640, "vcvtpd2dq",  Ve, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[112][0]},
    {MOD_EXT, 0xf20fe650, "(mod ext 112)", xx, xx, xx, xx, xx, mrm|evex, x, 112},
  }, { /* evex_W_ext 224 */
    {OP_vcmpps, 0x0fc200, "vcmpps", KPw, xx, KEw, Ib, Hes, xop|mrm|evex|ttfv, x, exop[190]},
    {MOD_EXT, 0x0fc210, "(mod ext 114)", xx, xx, xx, xx, xx, mrm|evex, x, 114},
    {OP_vcmppd, 0x660fc240, "vcmppd", KPb, xx, KEb, Ib, Hed, xop|mrm|evex|ttfv, x, exop[197]},
    {MOD_EXT, 0x660fc250, "(mod ext 115)", xx, xx, xx, xx, xx, mrm|evex, x, 115},
  }, { /* evex_W_ext 225 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpaddq, 0x660fd440, "vpaddq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[225][3]},
    {OP_vpaddq, 0x660fd450, "vpaddq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 226 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsubq, 0x660ffb40, "vpsubq", Ve, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[226][3]},
    {OP_vpsubq, 0x660ffb50, "vpsubq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 227 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmuldq,  0x66382848, "vpmuldq", Ve, xx, KEb, He, Wed, mrm|evex|ttfv, x, tevexwb[227][3]},
    {OP_vpmuldq,  0x66382858, "vpmuldq", Ve, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 228 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq, 0x660ff340, "vpsllq", Ve, xx, KEb, He, Wdq, mrm|evex|ttm128, x, tevexwb[229][2]},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 229 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpsllq,  0x660f7366, "vpsllq", He, xx, KEb, Ib, We, mrm|evex|ttfv, x, tevexwb[229][3]},
    {OP_vpsllq,  0x660f7376, "vpsllq", He, xx, KEb, Ib, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 230 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x663a0548, "vpermilpd", Ve, xx, KEb, We, Ib, mrm|evex|reqp|ttfv, x, tevexwb[230][3]},
    {OP_vpermilpd, 0x663a0558, "vpermilpd", Ve, xx, KEb, Mq, Ib, mrm|evex|reqp|ttfv, x, tevexwb[231][2]},
  }, { /* evex_W_ext 231 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermilpd, 0x66380d48, "vpermilpd", Ve, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[231][3]},
    {OP_vpermilpd, 0x66380d58, "vpermilpd", Ve, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 232 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpgtq, 0x66383748, "vpcmpgtq",  KPb, xx, KEb, He, We, mrm|evex|reqp|ttfv, x, tevexwb[232][3]},
    {OP_vpcmpgtq, 0x66383758, "vpcmpgtq",  KPb, xx, KEb, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 233 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpcmpeqq, 0x66382948, "vpcmpeqq", KPb, xx, KEb, He, Wed, mrm|evex|ttfv, x, tevexwb[233][3]},
    {OP_vpcmpeqq, 0x66382958, "vpcmpeqq", KPb, xx, KEb, He, Mq, mrm|evex|ttfv, x, END_LIST},
  }, { /* evex_W_ext 234 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpmadd52huq, 0x6638b548, "vpmadd52huq", Ve, xx, KEd, He, Wed, mrm|evex|reqp|ttfv, x, tevexwb[234][3]},
    {OP_vpmadd52huq, 0x6638b558, "vpmadd52huq", Ve, xx, KEd, He, Mq, mrm|evex|reqp|ttfv, x, END_LIST},
  }, { /* evex_W_ext 235 */
    {OP_vpunpckldq, 0x660f6200, "vpunpckldq", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[235][1]},
    {OP_vpunpckldq, 0x660f6210, "vpunpckldq", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 236 */
    {OP_vpcmpgtd, 0x660f6600, "vpcmpgtd", KPb, xx, KEb, He, We, mrm|evex|ttfv, x, tevexwb[236][1]},
    {OP_vpcmpgtd, 0x660f6610, "vpcmpgtd", KPb, xx, KEb, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 237 */
    {OP_vpunpckhdq, 0x660f6a00, "vpunpckhdq", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[237][1]},
    {OP_vpunpckhdq, 0x660f6a10, "vpunpckhdq", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 238 */
    {OP_vpackssdw, 0x660f6b00, "vpackssdw", Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[238][1]},
    {OP_vpackssdw, 0x660f6b10, "vpackssdw", Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 239 */
    {OP_vpshufd,  0x660f7000, "vpshufd",  Ve, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[239][1]},
    {OP_vpshufd,  0x660f7010, "vpshufd",  Ve, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 240 */
    {OP_vpcmpeqd, 0x660f7600, "vpcmpeqd", KPw, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[240][1]},
    {OP_vpcmpeqd, 0x660f7610, "vpcmpeqd", KPw, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 241 */
    {OP_vpsubd, 0x660ffa00, "vpsubd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[241][1]},
    {OP_vpsubd, 0x660ffa10, "vpsubd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 242 */
    {OP_vpaddd, 0x660ffe00, "vpaddd", Ve, xx, KEw, He, We, mrm|evex|ttfv, x, tevexwb[242][1]},
    {OP_vpaddd, 0x660ffe10, "vpaddd", Ve, xx, KEw, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 243 */
    {OP_vpslld, 0x660ff200, "vpslld", Ve, xx, KEw, He, Wdq, mrm|evex|ttm128, x, tevexwb[244][0]},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 244 */
    {OP_vpslld,  0x660f7226, "vpslld", He, xx, KEw, Ib, We, mrm|evex|ttfv, x, tevexwb[244][1]},
    {OP_vpslld,  0x660f7236, "vpslld", He, xx, KEw, Ib, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 245 */
    {OP_vpackusdw, 0x66382b08, "vpackusdw", Ve, xx, KEd, He, We, mrm|evex|reqp|ttfv, x, tevexwb[245][1]},
    {OP_vpackusdw, 0x66382b18, "vpackusdw", Ve, xx, KEd, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 246 */
    {OP_vrndscaleps, 0x663a0808, "vrndscaleps",  Ve, xx, KEw, Ib, We, mrm|evex|reqp|ttfv, x, modx[110][0]},
    {MOD_EXT, 0x663a0918, "(mod ext 110)", xx, xx, xx, xx, xx, mrm|evex, x, 110},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 247 */
    {OP_vpermilps, 0x663a0408, "vpermilps", Ve, xx, KEw, We, Ib, mrm|evex|reqp|ttfv, x, tevexwb[247][1]},
    {OP_vpermilps, 0x663a0418, "vpermilps", Ve, xx, KEw, Md, Ib, mrm|evex|reqp|ttfv, x, tevexwb[248][0]},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 248 */
    {OP_vpermilps, 0x66380c08, "vpermilps", Ve, xx, KEw, He, We, mrm|evex|reqp|ttfv, x, tevexwb[248][1]},
    {OP_vpermilps, 0x66380c18, "vpermilps", Ve, xx, KEw, He, Md, mrm|evex|reqp|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 249 */
    {OP_vcvtps2dq, 0x660f5b00, "vcvtps2dq", Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[116][0]},
    {MOD_EXT, 0x660f5b10, "(mod ext 116)", xx, xx, xx, xx, xx, mrm|evex, x, 116},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 250 */
    {OP_vcvttps2dq, 0xf30f5b00, "vcvttps2dq", Ve, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[117][0]},
    {MOD_EXT, 0xf30f5b10, "(mod ext 117)", xx, xx, xx, xx, xx, mrm|evex, x, 117},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 251 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermq, 0x663a0048, "vpermq", Vf, xx, KEb, Wf, Ib, mrm|evex|reqp|ttfv, x, tevexwb[251][3]},
    {OP_vpermq, 0x663a0058, "vpermq", Vf, xx, KEb, Mq, Ib, mrm|evex|reqp|ttfv, x, tevexwb[93][2]},
  }, { /* evex_W_ext 252 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vpermpd, 0x663a0148, "vpermpd", Vf, xx, KEw, Wf, Ib, mrm|evex|reqp, x, tevexwb[252][3]},
    {OP_vpermpd, 0x663a0158, "vpermpd", Vf, xx, KEw, Mq, Ib, mrm|evex|reqp, x, tevexwb[94][2]},
  }, { /* evex_W_ext 253 */
    {OP_vrndscaless, 0x663a0a08, "vrndscaless", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|reqp|ttt1s, x, exop[244]},
    {OP_vrndscaless, 0x663a0a18, "vrndscaless", Vdq, xx, KE1b, Ib, H12_dq, xop|mrm|evex|sae|reqp|ttt1s, x, exop[245]},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 254 */
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {OP_vrndscalesd, 0x663a0b48, "vrndscalesd", Vdq, xx, KE1b, Ib, Hsd, xop|mrm|evex|reqp|ttt1s, x, exop[246]},
    {OP_vrndscalesd, 0x663a0b58, "vrndscalesd", Vdq, xx, KE1b, Ib, Hsd, xop|mrm|evex|sae|reqp|ttt1s, x, exop[247]},
  }, { /* evex_W_ext 255 */
    {OP_vaddss, 0xf30f5800, "vaddss", Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[255][1]},
    {OP_vaddss, 0xf30f5810, "vaddss", Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vaddsd, 0xf20f5840, "vaddsd", Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[255][3]},
    {OP_vaddsd, 0xf20f5850, "vaddsd", Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 256 */
    {OP_vmulss, 0xf30f5900, "vmulss", Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[256][1]},
    {OP_vmulss, 0xf30f5910, "vmulss", Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vmulsd, 0xf20f5940, "vmulsd", Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[256][3]},
    {OP_vmulsd, 0xf20f5950, "vmulsd", Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 257 */
    {OP_vcvtss2sd, 0xf30f5a00, "vcvtss2sd", Vdq, xx, KE1b, Hsd, Wss, mrm|evex|ttt1s, x, tevexwb[257][1]},
    {OP_vcvtss2sd, 0xf30f5a10, "vcvtss2sd", Vdq, xx, KE1b, Hsd, Uss, mrm|evex|sae|ttt1s, x, END_LIST},
    {OP_vcvtsd2ss, 0xf20f5a40, "vcvtsd2ss", Vdq, xx, KE1b, H12_dq, Wsd, mrm|evex|ttt1s, x, tevexwb[257][3]},
    {OP_vcvtsd2ss, 0xf20f5a50, "vcvtsd2ss", Vdq, xx, KE1b, H12_dq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 258 */
    {OP_vsubss, 0xf30f5c00, "vsubss", Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[258][1]},
    {OP_vsubss, 0xf30f5c10, "vsubss", Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vsubsd, 0xf20f5c40, "vsubsd", Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[258][3]},
    {OP_vsubsd, 0xf20f5c50, "vsubsd", Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 259 */
    {OP_vminss, 0xf30f5d00, "vminss", Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[259][1]},
    {OP_vminss, 0xf30f5d10, "vminss", Vdq, xx, KE1b, Hdq, Uss, mrm|evex|sae|ttt1s, x, END_LIST},
    {OP_vminsd, 0xf20f5d40, "vminsd", Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[259][3]},
    {OP_vminsd, 0xf20f5d50, "vminsd", Vdq, xx, KE1b, Hdq, Usd, mrm|evex|sae|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 260 */
    {OP_vdivss, 0xf30f5e00, "vdivss", Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[260][1]},
    {OP_vdivss, 0xf30f5e10, "vdivss", Vdq, xx, KE1b, Hdq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vdivsd, 0xf20f5e40, "vdivsd", Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[260][3]},
    {OP_vdivsd, 0xf20f5e50, "vdivsd", Vdq, xx, KE1b, Hdq, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 261 */
    {OP_vmaxss, 0xf30f5f00, "vmaxss", Vdq, xx, KE1b, Hdq, Wss, mrm|evex|ttt1s, x, tevexwb[261][1]},
    {OP_vmaxss, 0xf30f5f10, "vmaxss", Vdq, xx, KE1b, Hdq, Uss, mrm|evex|sae|ttt1s, x, END_LIST},
    {OP_vmaxsd, 0xf20f5f40, "vmaxsd", Vdq, xx, KE1b, Hdq, Wsd, mrm|evex|ttt1s, x, tevexwb[261][3]},
    {OP_vmaxsd, 0xf20f5f50, "vmaxsd", Vdq, xx, KE1b, Hdq, Usd, mrm|evex|sae|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 262 */
    {OP_vcmpss, 0xf30fc200, "vcmpss", KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|ttt1s, x, exop[193]},
    {OP_vcmpss, 0xf30fc210, "vcmpss", KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|ttt1s, x, exop[194]},
    {OP_vcmpsd, 0xf20fc240, "vcmpsd", KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|ttt1s, x, exop[195]},
    {OP_vcmpsd, 0xf20fc250, "vcmpsd", KP1b, xx, KE1b, Ib, Hdq, xop|mrm|evex|sae|ttt1s, x, exop[196]},
  }, { /* evex_W_ext 263 */
    {OP_vcvtph2ps, 0x66381308, "vcvtph2ps", Ve, xx, KEw, Wh_e, xx, mrm|evex|tthvm, x, tevexwb[263][1]},
    {OP_vcvtph2ps, 0x66381318, "vcvtph2ps", Voq, xx, KEw, Uqq, xx, mrm|evex|sae|tthvm, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 264 */
    /* XXX i#3639: tools tend to accept different source/destination register mnemonics.
     * This also affects the existing VEX version if it exists.
     */
    {OP_vcvtps2ph, 0x663a1d08, "vcvtps2ph", Wh_e, xx, KEw, Ve, Ib, mrm|evex|reqp|tthvm, x, tevexwb[264][1]},
    {OP_vcvtps2ph, 0x663a1d18, "vcvtps2ph", Uqq, xx, KEw, Voq, Ib, mrm|evex|sae|reqp|tthvm, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 265 */
    {OP_vsqrtps, 0x0f5100, "vsqrtps", Ves, xx, KEw, Wes, xx, mrm|evex|ttfv, x, modx[118][0]},
    {MOD_EXT, 0x0f5110, "(mod ext 118)", xx, xx, xx, xx, xx, mrm|evex, x, 118},
    {OP_vsqrtpd, 0x660f5140, "vsqrtpd", Ved, xx, KEb, Wed, xx, mrm|evex|ttfv, x, modx[119][0]},
    {MOD_EXT, 0x660f5150, "(mod ext 119)", xx, xx, xx, xx, xx, mrm|evex, x, 119},
  }, { /* evex_W_ext 266 */
    {OP_vsqrtss, 0xf30f5100, "vsqrtss", Vdq, xx, KE1b, H12_dq, Wss, mrm|evex|ttt1s, x, tevexwb[266][1]},
    {OP_vsqrtss, 0xf30f5110, "vsqrtss", Vdq, xx, KE1b, H12_dq, Uss, mrm|evex|er|ttt1s, x, END_LIST},
    {OP_vsqrtsd, 0xf20f5140, "vsqrtsd", Vdq, xx, KE1b, Hsd, Wsd, mrm|evex|ttt1s, x, tevexwb[266][3]},
    {OP_vsqrtsd, 0xf20f5150, "vsqrtsd", Vdq, xx, KE1b, Hsd, Usd, mrm|evex|er|ttt1s, x, END_LIST},
  }, { /* evex_W_ext 267 */
    {OP_vpdpbusd, 0x66385008, "vpdpbusd", Ve, xx, KEd, He, We, mrm|evex|ttfv|reqp, x, tevexwb[267][1]},
    {OP_vpdpbusd, 0x66385018, "vpdpbusd", Ve, xx, KEd, He, Md, mrm|evex|ttfv|reqp, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 268 */
    {OP_vpdpbusds, 0x66385108, "vpdpbusds", Ve, xx, KEd, He, We, mrm|evex|ttfv|reqp, x, tevexwb[268][1]},
    {OP_vpdpbusds, 0x66385118, "vpdpbusds", Ve, xx, KEd, He, Md, mrm|evex|ttfv|reqp, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 269 */
    {OP_vpdpwssd, 0x66385208, "vpdpwssd", Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[269][1]},
    {OP_vpdpwssd, 0x66385218, "vpdpwssd", Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  }, { /* evex_W_ext 270 */
    {OP_vpdpwssds, 0x66385308, "vpdpwssds", Ve, xx, KEd, He, We, mrm|evex|ttfv|reqp, x, tevexwb[270][1]},
    {OP_vpdpwssds, 0x66385318, "vpdpwssds", Ve, xx, KEd, He, Md, mrm|evex|ttfv|reqp, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 271 */
    {OP_vcvtne2ps2bf16, 0xf2387208, "vcvtne2ps2bf16", Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[271][1]},
    {OP_vcvtne2ps2bf16, 0xf2387218, "vcvtne2ps2bf16", Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 272 */
    {OP_vcvtneps2bf16, 0xf3387208, "vcvtneps2bf16", Vh_e, xx, KEd, We, xx, mrm|evex|ttfv, x, tevexwb[272][1]},
    {OP_vcvtneps2bf16, 0xf3387218, "vcvtneps2bf16", Vh_e, xx, KEd, Md, xx, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 273 */
    {OP_vdpbf16ps, 0xf3385208, "vdpbf16ps", Ve, xx, KEd, He, We, mrm|evex|ttfv, x, tevexwb[273][1]},
    {OP_vdpbf16ps, 0xf3385218, "vdpbf16ps", Ve, xx, KEd, He, Md, mrm|evex|ttfv, x, END_LIST},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
    {INVALID, 0, "(bad)", xx, xx, xx, xx, xx, no, x, NA},
  },{ /* evex_W_ext 274 */
    {OP_vpopcntd, 0x66385508, "vpopcntd", Ve, xx, KEd, We, xx, mrm|evex|ttfv|reqp, x, tevexwb[274][1]},
    {OP_vpopcntd, 0x66385518, "vpopcntd", Ve, xx, KEd, Md, xx, mrm|evex|ttfv|reqp, x, END_LIST},
    {OP_vpopcntq, 0x66385548, "vpopcntq", Ve, xx, KEq, We, xx, mrm|evex|ttfv|reqp, x, tevexwb[274][3]},
    {OP_vpopcntq, 0x66385558, "vpopcntq", Ve, xx, KEq, Mq, xx, mrm|evex|ttfv|reqp, x, END_LIST},
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
  {OP_fadd,  0xd80020, "fadd",  st0, xx, Fd, st0, xx, mrm, x, tfl[0x20]}, /* 00 */
  {OP_fmul,  0xd80021, "fmul",  st0, xx, Fd, st0, xx, mrm, x, tfl[0x21]},
  {OP_fcom,  0xd80022, "fcom",  xx, xx, Fd, st0, xx, mrm, x, tfl[0x22]},
  {OP_fcomp, 0xd80023, "fcomp", xx, xx, Fd, st0, xx, mrm, x, tfl[0x23]},
  {OP_fsub,  0xd80024, "fsub",  st0, xx, Fd, st0, xx, mrm, x, tfl[0x24]},
  {OP_fsubr, 0xd80025, "fsubr", st0, xx, Fd, st0, xx, mrm, x, tfl[0x25]},
  {OP_fdiv,  0xd80026, "fdiv",  st0, xx, Fd, st0, xx, mrm, x, tfl[0x26]},
  {OP_fdivr, 0xd80027, "fdivr", st0, xx, Fd, st0, xx, mrm, x, tfl[0x27]},
  /*  d9 */
  {OP_fld,    0xd90020, "fld",    st0, xx, Fd, xx, xx, mrm, x, tfl[0x1d]}, /* 08 */
  {INVALID,   0xd90021, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fst,    0xd90022, "fst",    Fd, xx, st0, xx, xx, mrm, x, tfl[0x2a]},
  {OP_fstp,   0xd90023, "fstp",   Fd, xx, st0, xx, xx, mrm, x, tfl[0x1f]},
  {OP_fldenv, 0xd90024, "fldenv", xx, xx, Ffy, xx, xx, mrm, x, END_LIST},
  {OP_fldcw,  0xd90025, "fldcw",  xx, xx, Fw, xx, xx, mrm, x, END_LIST},
  {OP_fnstenv, 0xd90026, "fnstenv", Ffy, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, this is "fstenv"*/
  {OP_fnstcw,  0xd90027, "fnstcw",  Fw, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME: w/ preceding fwait instr, this is "fstcw"*/
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
  {OP_fld,   0xdb0025, "fld",   st0, xx, Ffx, xx, xx, mrm, x, tfl[0x28]},
  {INVALID,  0xdb0026, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fstp,  0xdb0027, "fstp",  Ffx, xx, st0, xx, xx, mrm, x, tfl[0x2b]},
  /* dc */
  {OP_fadd,  0xdc0020, "fadd",  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x00]}, /* 20 */
  {OP_fmul,  0xdc0021, "fmul",  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x08]},
  {OP_fcom,  0xdc0022, "fcom",  xx, xx, Fq, st0, xx, mrm, x, tfh[0][0x10]},
  {OP_fcomp, 0xdc0023, "fcomp", xx, xx, Fq, st0, xx, mrm, x, tfh[0][0x18]},
  {OP_fsub,  0xdc0024, "fsub",  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x20]},
  {OP_fsubr, 0xdc0025, "fsubr", st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x28]},
  {OP_fdiv,  0xdc0026, "fdiv",  st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x30]},
  {OP_fdivr, 0xdc0027, "fdivr", st0, xx, Fq, st0, xx, mrm, x, tfh[0][0x38]},
  /* dd */
  {OP_fld,   0xdd0020, "fld",    st0, xx, Fq, xx, xx, mrm, x, tfh[1][0x00]}, /* 28 */
  {OP_fisttp, 0xdd0021, "fisttp",  Mq, xx, st0, xx, xx, no, x, tfl[0x19]},
  {OP_fst,   0xdd0022, "fst",    Fq, xx, st0, xx, xx, mrm, x, tfh[5][0x10]},
  {OP_fstp,  0xdd0023, "fstp",   Fq, xx, st0, xx, xx, mrm, x, tfh[5][0x18]},
  {OP_frstor,0xdd0024, "frstor", xx, xx, Ffz, xx, xx, mrm, x, END_LIST},
  {INVALID,  0xdd0025, "(bad)",  xx, xx, xx, xx, xx, no, x, NA},
  {OP_fnsave, 0xdd0026, "fnsave",  Ffz, xx, xx, xx, xx, mrm, x, END_LIST},/*FIXME:w/ preceding fwait instr, this is "fsave"*/
  {OP_fnstsw, 0xdd0027, "fnstsw",  Fw, xx, xx, xx, xx, mrm, x, tfh[7][0x20]},/*FIXME:w/ preceding fwait instr, this is "fstsw"*/
  /* de */
  {OP_fiadd,  0xde0020, "fiadd",  st0, xx, Fw, st0, xx, mrm, x, END_LIST}, /* 30 */
  {OP_fimul,  0xde0021, "fimul",  st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_ficom,  0xde0022, "ficom",  xx, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_ficomp, 0xde0023, "ficomp", xx, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fisub,  0xde0024, "fisub",  st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fisubr, 0xde0025, "fisubr", st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fidiv,  0xde0026, "fidiv",  st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  {OP_fidivr, 0xde0027, "fidivr", st0, xx, Fw, st0, xx, mrm, x, END_LIST},
  /* df */
  {OP_fild,   0xdf0020, "fild",    st0, xx, Fw, xx, xx, mrm, x, tfl[0x3d]}, /* 38 */
  {OP_fisttp, 0xdf0021, "fisttp",  Mw, xx, st0, xx, xx, no, x, END_LIST},
  {OP_fist,   0xdf0022, "fist",    Fw, xx, st0, xx, xx, mrm, x, END_LIST},
  {OP_fistp,  0xdf0023, "fistp",   Fw, xx, st0, xx, xx, mrm, x, tfl[0x3f]},
  {OP_fbld,   0xdf0024, "fbld",    st0, xx, Ffx, xx, xx, mrm, x, END_LIST},
  {OP_fild,   0xdf0025, "fild",    st0, xx, Fq, xx, xx, mrm, x, END_LIST},
  {OP_fbstp,  0xdf0026, "fbstp",   Ffx, xx, st0, xx, xx, mrm, x, END_LIST},
  {OP_fistp,  0xdf0027, "fistp",   Fq, xx, st0, xx, xx, mrm, x, END_LIST},
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
 * Also, only implicit operands are in these instruction extensions!!!
 */
const instr_info_t extra_operands[] =
{
    /* 0 */
    {OP_CONTD, 0x000000, "<pusha cont'd>", xx, xx, eCX, eDX, eBP, xop, x, exop[0x01]},
    {OP_CONTD, 0x000000, "<pusha cont'd>", xx, xx, eSI, eDI, xx, no, x, END_LIST},
    /* 2 */
    {OP_CONTD, 0x000000, "<popa cont'd>", eBX, eCX, xx, xx, xx, xop, x, exop[0x03]},
    {OP_CONTD, 0x000000, "<popa cont'd>", eDX, eBP, xx, xx, xx, xop, x, exop[0x04]},
    {OP_CONTD, 0x000000, "<popa cont'd>", eSI, eDI, xx, xx, xx, no, x, END_LIST},
    /* 5 */
    {OP_CONTD, 0x000000, "<enter cont'd>", xbp, xx, xbp, xx, xx, no, x, END_LIST},
    /* 6 */
    {OP_CONTD, 0x000000, "<cpuid cont'd>", ecx, edx, xx, xx, xx, no, x, END_LIST},
    /* 7 */
    {OP_CONTD, 0x000000, "<cmpxchg8b cont'd>", eDX, xx, eCX, eBX, xx, mrm, fWZ, END_LIST},
    {OP_CONTD,0x663a6018, "<pcmpestrm cont'd", xx, xx, eax, edx, xx, mrm|reqp, fW6, END_LIST},
    {OP_CONTD,0x663a6018, "<pcmpestri cont'd", xx, xx, eax, edx, xx, mrm|reqp, fW6, END_LIST},
    /* 10 */
    {OP_CONTD,0xf90f0177, "<rdtscp cont'd>", ecx, xx, xx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD,0x663a6018, "<vpcmpestrm cont'd", xx, xx, eax, edx, xx, mrm|vex|reqp, fW6, END_LIST},
    {OP_CONTD,0x663a6018, "<vpcmpestri cont'd", xx, xx, eax, edx, xx, mrm|vex|reqp, fW6, END_LIST},
    {OP_CONTD,0x0f3710, "<getsec cont'd", ecx, xx, xx, xx, xx, predcx, x, END_LIST},
    /* 14 */
    {OP_CONTD,0x66389808, "<vfmadd132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[42][0]},
    {OP_CONTD,0x66389818, "<vfmadd132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[42][1]},
    {OP_CONTD,0x66389818, "<vfmadd132ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 17 */
    {OP_CONTD,0x66389848, "<vfmadd132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[43][0]},
    {OP_CONTD,0x66389858, "<vfmadd132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[43][1]},
    {OP_CONTD,0x66389858, "<vfmadd132pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 20 */
    {OP_CONTD,0x6638a808, "<vfmadd213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[44][0]},
    {OP_CONTD,0x6638a818, "<vfmadd213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[44][1]},
    {OP_CONTD,0x6638a818, "<vfmadd213ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 23 */
    {OP_CONTD,0x6638a848, "<vfmadd213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[45][0]},
    {OP_CONTD,0x6638a858, "<vfmadd213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[45][1]},
    {OP_CONTD,0x6638a858, "<vfmadd213pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 26 */
    {OP_CONTD,0x6638b808, "<vfmadd231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[46][0]},
    {OP_CONTD,0x6638b818, "<vfmadd231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[46][1]},
    {OP_CONTD,0x6638b818, "<vfmadd231ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 29 */
    {OP_CONTD,0x6638b848, "<vfmadd231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[47][0]},
    {OP_CONTD,0x6638b858, "<vfmadd231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[47][1]},
    {OP_CONTD,0x6638b858, "<vfmadd231pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 32 */
    {OP_CONTD,0x66389908, "<vfmadd132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[65][1]},
    {OP_CONTD,0x66389918, "<vfmadd132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 34 */
    {OP_CONTD,0x66389948, "<vfmadd132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[65][3]},
    {OP_CONTD,0x66389958, "<vfmadd132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 36 */
    {OP_CONTD,0x6638a908, "<vfmadd213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[66][1]},
    {OP_CONTD,0x6638a918, "<vfmadd213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 38 */
    {OP_CONTD,0x6638a948, "<vfmadd213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[66][3]},
    {OP_CONTD,0x6638a958, "<vfmadd213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 40 */
    {OP_CONTD,0x6638b908, "<vfmadd231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[67][1]},
    {OP_CONTD,0x6638b918, "<vfmadd231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 42 */
    {OP_CONTD,0x6638b948, "<vfmadd231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[67][3]},
    {OP_CONTD,0x6638b958, "<vfmadd231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 44 */
    {OP_CONTD,0x66389608, "<vfmaddsub132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[48][0]},
    {OP_CONTD,0x66389618, "<vfmaddsub132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[48][1]},
    {OP_CONTD,0x66389618, "<vfmaddsub132ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 47 */
    {OP_CONTD,0x66389648, "<vfmaddsub132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[49][0]},
    {OP_CONTD,0x66389658, "<vfmaddsub132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[49][1]},
    {OP_CONTD,0x66389658, "<vfmaddsub132pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 50 */
    {OP_CONTD,0x6638a608, "<vfmaddsub213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[50][0]},
    {OP_CONTD,0x6638a618, "<vfmaddsub213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[50][1]},
    {OP_CONTD,0x6638a618, "<vfmaddsub213ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 53 */
    {OP_CONTD,0x6638a648, "<vfmaddsub213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[51][0]},
    {OP_CONTD,0x6638a658, "<vfmaddsub213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[51][1]},
    {OP_CONTD,0x6638a658, "<vfmaddsub213pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 56 */
    {OP_CONTD,0x6638b608, "<vfmaddsub231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[52][0]},
    {OP_CONTD,0x6638b618, "<vfmaddsub231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[52][1]},
    {OP_CONTD,0x6638b618, "<vfmaddsub231ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 59 */
    {OP_CONTD,0x6638b648, "<vfmaddsub231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[53][0]},
    {OP_CONTD,0x6638b658, "<vfmaddsub231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[53][1]},
    {OP_CONTD,0x6638b658, "<vfmaddsub231pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 62 */
    {OP_CONTD,0x66389708, "<vfmsubadd132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[54][0]},
    {OP_CONTD,0x66389718, "<vfmsubadd132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[54][1]},
    {OP_CONTD,0x66389718, "<vfmsubadd132ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 65 */
    {OP_CONTD,0x66389748, "<vfmsubadd132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[55][0]},
    {OP_CONTD,0x66389758, "<vfmsubadd132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[55][1]},
    {OP_CONTD,0x66389758, "<vfmsubadd132pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 68 */
    {OP_CONTD,0x6638a708, "<vfmsubadd213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[56][0]},
    {OP_CONTD,0x6638a718, "<vfmsubadd213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[56][1]},
    {OP_CONTD,0x6638a718, "<vfmsubadd213ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 71 */
    {OP_CONTD,0x6638a748, "<vfmsubadd213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[57][0]},
    {OP_CONTD,0x6638a758, "<vfmsubadd213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[57][1]},
    {OP_CONTD,0x6638a758, "<vfmsubadd213pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 74 */
    {OP_CONTD,0x6638b708, "<vfmsubadd231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[58][0]},
    {OP_CONTD,0x6638b718, "<vfmsubadd231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[58][1]},
    {OP_CONTD,0x6638b718, "<vfmsubadd231ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 77 */
    {OP_CONTD,0x6638b748, "<vfmsubadd231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[59][0]},
    {OP_CONTD,0x6638b758, "<vfmsubadd231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[59][1]},
    {OP_CONTD,0x6638b758, "<vfmsubadd231pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 80 */
    {OP_CONTD,0x66389a08, "<vfmsub132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[60][0]},
    {OP_CONTD,0x66389a18, "<vfmsub132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[60][1]},
    {OP_CONTD,0x66389a18, "<vfmsub132ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 83 */
    {OP_CONTD,0x66389a48, "<vfmsub132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[61][0]},
    {OP_CONTD,0x66389a58, "<vfmsub132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[61][1]},
    {OP_CONTD,0x66389a58, "<vfmsub132pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 86 */
    {OP_CONTD,0x6638aa08, "<vfmsub213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[62][0]},
    {OP_CONTD,0x6638aa18, "<vfmsub213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[62][1]},
    {OP_CONTD,0x6638aa18, "<vfmsub213ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 89 */
    {OP_CONTD,0x6638aa48, "<vfmsub213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[63][0]},
    {OP_CONTD,0x6638aa58, "<vfmsub213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[63][1]},
    {OP_CONTD,0x6638aa58, "<vfmsub213pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 92 */
    {OP_CONTD,0x6638ba08, "<vfmsub231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[64][0]},
    {OP_CONTD,0x6638ba18, "<vfmsub231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[64][1]},
    {OP_CONTD,0x6638ba18, "<vfmsub231ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 95 */
    {OP_CONTD,0x6638ba48, "<vfmsub231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[65][0]},
    {OP_CONTD,0x6638ba58, "<vfmsub231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[65][1]},
    {OP_CONTD,0x6638ba58, "<vfmsub231pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 98 */
    {OP_CONTD,0x66389b08, "<vfmsub132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[77][1]},
    {OP_CONTD,0x66389b18, "<vfmsub132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 100 */
    {OP_CONTD,0x66389b48, "<vfmsub132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[77][3]},
    {OP_CONTD,0x66389b58, "<vfmsub132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 102 */
    {OP_CONTD,0x6638ab08, "<vfmsub213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[78][1]},
    {OP_CONTD,0x6638ab18, "<vfmsub213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 104 */
    {OP_CONTD,0x6638ab48, "<vfmsub213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[78][3]},
    {OP_CONTD,0x6638ab58, "<vfmsub213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 106 */
    {OP_CONTD,0x6638bb08, "<vfmsub231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[79][1]},
    {OP_CONTD,0x6638bb18, "<vfmsub231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 108 */
    {OP_CONTD,0x6638bb48, "<vfmsub231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[79][3]},
    {OP_CONTD,0x6638bb58, "<vfmsub231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 110 */
    {OP_CONTD,0x66389c08, "<vfnmadd132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[66][0]},
    {OP_CONTD,0x66389c18, "<vfnmadd132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[66][1]},
    {OP_CONTD,0x66389c18, "<vfnmadd132ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 113 */
    {OP_CONTD,0x66389c48, "<vfnmadd132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[67][0]},
    {OP_CONTD,0x66389c58, "<vfnmadd132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[67][1]},
    {OP_CONTD,0x66389c58, "<vfnmadd132pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 116 */
    {OP_CONTD,0x6638ac08, "<vfnmadd213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[68][0]},
    {OP_CONTD,0x6638ac18, "<vfnmadd213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[68][1]},
    {OP_CONTD,0x6638ac18, "<vfnmadd213ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 119 */
    {OP_CONTD,0x6638ac48, "<vfnmadd213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[69][0]},
    {OP_CONTD,0x6638ac58, "<vfnmadd213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[69][1]},
    {OP_CONTD,0x6638ac58, "<vfnmadd213pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 122 */
    {OP_CONTD,0x6638bc08, "<vfnmadd231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[70][0]},
    {OP_CONTD,0x6638bc18, "<vfnmadd231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[70][1]},
    {OP_CONTD,0x6638bc18, "<vfnmadd231ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 125 */
    {OP_CONTD,0x6638bc48, "<vfnmadd231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[71][0]},
    {OP_CONTD,0x6638bc58, "<vfnmadd231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[71][1]},
    {OP_CONTD,0x6638bc58, "<vfnmadd231pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 128 */
    {OP_CONTD,0x66389d08, "<vfnmadd132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[83][1]},
    {OP_CONTD,0x66389d18, "<vfnmadd132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 130 */
    {OP_CONTD,0x66389d48, "<vfnmadd132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[83][3]},
    {OP_CONTD,0x66389d58, "<vfnmadd132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 132 */
    {OP_CONTD,0x6638ad08, "<vfnmadd213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[84][1]},
    {OP_CONTD,0x6638ad18, "<vfnmadd213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 134 */
    {OP_CONTD,0x6638ad48, "<vfnmadd213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[84][3]},
    {OP_CONTD,0x6638ad58, "<vfnmadd213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 136 */
    {OP_CONTD,0x6638bd08, "<vfnmadd231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[85][1]},
    {OP_CONTD,0x6638bd18, "<vfnmadd231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 138 */
    {OP_CONTD,0x6638bd48, "<vfnmadd231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[85][3]},
    {OP_CONTD,0x6638bd58, "<vfnmadd231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 140 */
    {OP_CONTD,0x66389e08, "<vfnmsub132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[72][0]},
    {OP_CONTD,0x66389e18, "<vfnmsub132ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[72][1]},
    {OP_CONTD,0x66389e18, "<vfnmsub132ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 143 */
    {OP_CONTD,0x66389e48, "<vfnmsub132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[73][0]},
    {OP_CONTD,0x66389e58, "<vfnmsub132pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[73][1]},
    {OP_CONTD,0x66389e58, "<vfnmsub132pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 146 */
    {OP_CONTD,0x6638ae08, "<vfnmsub213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[74][0]},
    {OP_CONTD,0x6638ae18, "<vfnmsub213ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[74][1]},
    {OP_CONTD,0x6638ae18, "<vfnmsub213ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 149 */
    {OP_CONTD,0x6638ae48, "<vfnmsub213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[75][0]},
    {OP_CONTD,0x6638ae58, "<vfnmsub213pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[75][1]},
    {OP_CONTD,0x6638ae58, "<vfnmsub213pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 152 */
    {OP_CONTD,0x6638be08, "<vfnmsub231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[76][0]},
    {OP_CONTD,0x6638be18, "<vfnmsub231ps cont'd", xx, xx, Ves, xx, xx, mrm|evex, x, modx[76][1]},
    {OP_CONTD,0x6638be18, "<vfnmsub231ps cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 155 */
    {OP_CONTD,0x6638be48, "<vfnmsub231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[77][0]},
    {OP_CONTD,0x6638be58, "<vfnmsub231pd cont'd", xx, xx, Ved, xx, xx, mrm|evex, x, modx[77][1]},
    {OP_CONTD,0x6638be58, "<vfnmsub231pd cont'd", xx, xx, Voq, xx, xx, mrm|evex, x, END_LIST},
    /* 158 */
    {OP_CONTD,0x66389f08, "<vfnmsub132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[89][1]},
    {OP_CONTD,0x66389f18, "<vfnmsub132ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 160 */
    {OP_CONTD,0x66389f48, "<vfnmsub132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[89][3]},
    {OP_CONTD,0x66389f58, "<vfnmsub132sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 162 */
    {OP_CONTD,0x6638af08, "<vfnmsub213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[90][1]},
    {OP_CONTD,0x6638af18, "<vfnmsub213ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 164 */
    {OP_CONTD,0x6638af48, "<vfnmsub213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[90][3]},
    {OP_CONTD,0x6638af58, "<vfnmsub213sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 166 */
    {OP_CONTD,0x6638bf08, "<vfnmsub231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, tevexwb[91][1]},
    {OP_CONTD,0x6638bf18, "<vfnmsub231ss cont'd", xx, xx, Vss, xx, xx, mrm|evex, x, END_LIST},
    /* 168 */
    {OP_CONTD,0x6638bf48, "<vfnmsub231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, tevexwb[91][3]},
    {OP_CONTD,0x6638bf58, "<vfnmsub231sd cont'd", xx, xx, Vsd, xx, xx, mrm|evex, x, END_LIST},
    /* 170 */
    {OP_CONTD, 0x663a1818, "vinsertf32x4 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a1858, "vinsertf64x2 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 172 */
    {OP_CONTD, 0x663a1a58, "vinsertf32x8 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a1a58, "vinsertf64x4 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 174 */
    {OP_CONTD, 0x663a3818, "vinserti32x4 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a3858, "vinserti64x2 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 176 */
    {OP_CONTD, 0x663a3a18, "vinserti32x8 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a3a58, "vinserti64x4 cont'd", xx, xx, Wdq, xx, xx, mrm|evex, x, END_LIST},
    /* 178 */
    {OP_CONTD, 0x663a3e18, "vpcmpub cont'd", xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    {OP_CONTD, 0x663a3f18, "vpcmpb cont'd", xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    /* 180 */
    {OP_CONTD, 0x663a3e18, "vpcmpuw cont'd", xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    {OP_CONTD, 0x663a3f18, "vpcmpw cont'd", xx, xx, We, xx, xx, evex|mrm, x, END_LIST},
    /* 182 */
    {OP_CONTD, 0x663a1e18, "vpcmpud cont'd", xx, xx, We, xx, xx, evex|mrm, x, tevexwb[111][1]},
    {OP_CONTD, 0x663a1e18, "vpcmpud cont'd", xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 184 */
    {OP_CONTD, 0x663a1f18, "vpcmpd cont'd", xx, xx, We, xx, xx, evex|mrm, x, tevexwb[112][1]},
    {OP_CONTD, 0x663a1f18, "vpcmpd cont'd", xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 186 */
    {OP_CONTD, 0x663a1e18, "vpcmpuq cont'd", xx, xx, We, xx, xx, evex|mrm, x, tevexwb[111][3]},
    {OP_CONTD, 0x663a1e18, "vpcmpuq cont'd", xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 188 */
    {OP_CONTD, 0x663a1f18, "vpcmpq cont'd", xx, xx, We, xx, xx, evex|mrm, x, tevexwb[112][3]},
    {OP_CONTD, 0x663a1f18, "vpcmpq cont'd", xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 190 */
    {OP_CONTD,   0x0fc200, "vcmpps cont'd", xx, xx, Wes, xx, xx, evex|mrm, x, modx[114][0]},
    {OP_CONTD,   0x0fc210, "vcmpps cont'd", xx, xx, Md, xx, xx, evex|mrm, x, modx[114][1]},
    {OP_CONTD,   0x0fc210, "vcmpps cont'd", xx, xx, Uoq, xx, xx, evex|mrm, x, END_LIST},
    /* 193 */
    {OP_CONTD, 0xf30fc200, "vcmpss cont'd", xx, xx, Wss, xx, xx, evex|mrm, x, tevexwb[262][1]},
    {OP_CONTD, 0xf30fc210, "vcmpss cont'd", xx, xx, Uss, xx, xx, evex|mrm, x, END_LIST},
    /* 195 */
    {OP_CONTD, 0xf20fc200, "vcmpsd cont'd", xx, xx, Wsd, xx, xx, evex|mrm, x, tevexwb[262][3]},
    {OP_CONTD, 0xf20fc210, "vcmpsd cont'd", xx, xx, Usd, xx, xx, evex|mrm, x, END_LIST},
    /* 197 */
    {OP_CONTD, 0x660fc200, "vcmppd cont'd", xx, xx, Wed, xx, xx, evex|mrm, x, modx[115][0]},
    {OP_CONTD, 0x660fc210, "vcmppd cont'd", xx, xx, Mq, xx, xx, evex|mrm, x, modx[115][1]},
    {OP_CONTD, 0x660fc210, "vcmppd cont'd", xx, xx, Uoq, xx, xx, evex|mrm, x, END_LIST},
    /* 200 */
    {OP_CONTD,   0x0fc610, "vshufps cont'd", xx, xx, Wes, xx, xx, evex|mrm, x, tevexwb[221][1]},
    {OP_CONTD,   0x0fc610, "vshufps cont'd", xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 202 */
    {OP_CONTD, 0x660fc650, "vshufpd cont'd", xx, xx, Wed, xx, xx, evex|mrm, x, tevexwb[221][3]},
    {OP_CONTD, 0x660fc650, "vshufpd cont'd", xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 204 */
    {OP_CONTD, 0x663a2318, "vshuff32x4 cont'd", xx, xx, Wfs, xx, xx, evex|mrm, x, tevexwb[142][1]},
    {OP_CONTD, 0x663a2318, "vshuff32x4 cont'd", xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 206 */
    {OP_CONTD, 0x663a2358, "vshuff64x2 cont'd", xx, xx, Wfd, xx, xx, evex|mrm, x, tevexwb[142][3]},
    {OP_CONTD, 0x663a2358, "vshuff64x2 cont'd", xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 208 */
    {OP_CONTD, 0x663a4318, "vshufi32x4 cont'd", xx, xx, Wfs, xx, xx, evex|mrm, x, tevexwb[143][1]},
    {OP_CONTD, 0x663a4318, "vshufi32x4 cont'd", xx, xx, Md, xx, xx, evex|mrm, x, END_LIST},
    /* 210 */
    {OP_CONTD, 0x663a4358, "vshufi64x2 cont'd", xx, xx, Wfd, xx, xx, evex|mrm, x, tevexwb[143][3]},
    {OP_CONTD, 0x663a4358, "vshufi64x2 cont'd", xx, xx, Mq, xx, xx, evex|mrm, x, END_LIST},
    /* 212 */
    {OP_CONTD, 0x663a0318, "valignd cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[155][1]},
    {OP_CONTD, 0x663a0318, "valignd cont'd", xx, xx, Md, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 214 */
    {OP_CONTD, 0x663a0358, "valignq cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[155][3]},
    {OP_CONTD, 0x663a0358, "valignq cont'd", xx, xx, Mq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 216 */
    {OP_CONTD, 0x663a5408, "vfixupimmps cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[80][0]},
    {OP_CONTD, 0x663a5418, "vfixupimmps cont'd", xx, xx, Md, xx, xx, mrm|evex|reqp, x, modx[80][1]},
    {OP_CONTD, 0x663a5418, "vfixupimmps cont'd", xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 219 */
    {OP_CONTD, 0x663a5448, "vfixupimmpd cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[81][0]},
    {OP_CONTD, 0x663a5458, "vfixupimmpd cont'd", xx, xx, Mq, xx, xx, mrm|evex|reqp, x, modx[81][1]},
    {OP_CONTD, 0x663a5458, "vfixupimmpd cont'd", xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 222 */
    {OP_CONTD, 0x663a5508, "vfixupimmss cont'd", xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[160][1]},
    {OP_CONTD, 0x663a5518, "vfixupimmss cont'd", xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 224 */
    {OP_CONTD, 0x663a5548, "vfixupimmsd cont'd", xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[161][3]},
    {OP_CONTD, 0x663a5558, "vfixupimmsd cont'd", xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 226 */
    {OP_CONTD, 0x663a2708, "vgetmantss cont'd", xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[164][1]},
    {OP_CONTD, 0x663a2718, "vgetmantss cont'd", xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 228 */
    {OP_CONTD, 0x663a2748, "vgetmantsd cont'd", xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[164][3]},
    {OP_CONTD, 0x663a2758, "vgetmantsd cont'd", xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 230 */
    {OP_CONTD, 0x663a5008, "vrangeps cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[86][0]},
    {OP_CONTD, 0x663a5018, "vrangeps cont'd", xx, xx, Md, xx, xx, mrm|evex|reqp, x, modx[86][1]},
    {OP_CONTD, 0x663a5018, "vrangeps cont'd", xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 233 */
    {OP_CONTD, 0x663a5048, "vrangepd cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, modx[87][0]},
    {OP_CONTD, 0x663a5058, "vrangepd cont'd", xx, xx, Mq, xx, xx, mrm|evex|reqp, x, modx[87][1]},
    {OP_CONTD, 0x663a5058, "vrangepd cont'd", xx, xx, Uoq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 236 */
    {OP_CONTD, 0x663a5108, "vrangess cont'd", xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[174][1]},
    {OP_CONTD, 0x663a5118, "vrangess cont'd", xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 238 */
    {OP_CONTD, 0x663a5148, "vrangesd cont'd", xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[174][3]},
    {OP_CONTD, 0x663a5158, "vrangesd cont'd", xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 240 */
    {OP_CONTD, 0x663a5708, "vreducess cont'd", xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[176][1]},
    {OP_CONTD, 0x663a5718, "vreducess cont'd", xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 242 */
    {OP_CONTD, 0x663a5748, "vreducesd cont'd", xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[176][3]},
    {OP_CONTD, 0x663a5758, "vreducesd cont'd", xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 244 */
    {OP_CONTD, 0x663a0a08, "vrndscaless cont'd", xx, xx, Wd_dq, xx, xx, mrm|evex|reqp, x, tevexwb[253][1]},
    {OP_CONTD, 0x663a0a18, "vrndscaless cont'd", xx, xx, Ud_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 246 */
    {OP_CONTD, 0x663a0b08, "vrndscalesd cont'd", xx, xx, Wq_dq, xx, xx, mrm|evex|reqp, x, tevexwb[254][1]},
    {OP_CONTD, 0x663a0b18, "vrndscalesd cont'd", xx, xx, Uq_dq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 248 */
    {OP_CONTD, 0x663a0f18, "vpalignr cont'd", xx, xx, We, xx, xx, mrm|evex, x, END_LIST},
    {OP_CONTD, 0x663a4218, "vdbpsadbw cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 250 */
    {OP_CONTD, 0x663a2508, "vpternlogd cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[188][1]},
    {OP_CONTD, 0x663a2518, "vpternlogd cont'd", xx, xx, Md, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 252 */
    {OP_CONTD, 0x663a2548, "vpternlogq cont'd", xx, xx, We, xx, xx, mrm|evex|reqp, x, tevexwb[188][3]},
    {OP_CONTD, 0x663a2558, "vpternlogq cont'd", xx, xx, Mq, xx, xx, mrm|evex|reqp, x, END_LIST},
    /* 254 */
    {OP_CONTD, 0xcf0f0171, "<encls cont'd>", ecx, edx, edx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD, 0xd70f0172, "<enclu cont'd>", ecx, edx, edx, xx, xx, mrm, x, END_LIST},
    {OP_CONTD, 0xc00f0171, "<enclv cont'd>", ecx, edx, edx, xx, xx, mrm, x, END_LIST},
};

/* clang-format on */
