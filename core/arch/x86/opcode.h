/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "opcode.h" -- opcode definitions and utilities */

#ifndef _OPCODE_H_
#define _OPCODE_H_ 1

/* DR_API EXPORT TOFILE dr_ir_opcodes_x86.h */
/* DR_API EXPORT BEGIN */

/****************************************************************************
 * OPCODES
 */
/**
 * @file dr_ir_opcodes_x86.h
 * @brief Instruction opcode constants for IA-32 and AMD64.
 */
#ifdef AVOID_API_EXPORT
/*
 * This enum corresponds with the array in decode_table.c
 * IF YOU CHANGE ONE YOU MUST CHANGE THE OTHER
 * The Perl script tools/x86opnums.pl is useful for re-numbering these
 * if you add or delete in the middle (feed it the array from decode_table.c).
 * When adding new instructions, be sure to update all of these places:
 *   1) decode_table op_instr array
 *   2) decode_table decoding table entries
 *   3) OP_ enum (here) via x86opnums.pl
 *   4) update OP_LAST at end of enum here
 *   5) decode_fast tables if necessary (they are conservative)
 *   6) instr_create macros
 *   7) suite/tests/api/ir* tests
 */
#endif
/** Opcode constants for use in the instr_t data structure. */
enum {
/*   0 */     OP_INVALID,  /* NULL, */ /**< INVALID opcode */
/*   1 */     OP_UNDECODED,  /* NULL, */ /**< UNDECODED opcode */
/*   2 */     OP_CONTD,    /* NULL, */ /**< CONTD opcode */
/*   3 */     OP_LABEL,    /* NULL, */ /**< LABEL opcode */

/*   4 */     OP_add,      /* &first_byte[0x05], */ /**< add opcode */
/*   5 */     OP_or,       /* &first_byte[0x0d], */ /**< or opcode */
/*   6 */     OP_adc,      /* &first_byte[0x15], */ /**< adc opcode */
/*   7 */     OP_sbb,      /* &first_byte[0x1d], */ /**< sbb opcode */
/*   8 */     OP_and,      /* &first_byte[0x25], */ /**< and opcode */
/*   9 */     OP_daa,      /* &first_byte[0x27], */ /**< daa opcode */
/*  10 */     OP_sub,      /* &first_byte[0x2d], */ /**< sub opcode */
/*  11 */     OP_das,      /* &first_byte[0x2f], */ /**< das opcode */
/*  12 */     OP_xor,      /* &first_byte[0x35], */ /**< xor opcode */
/*  13 */     OP_aaa,      /* &first_byte[0x37], */ /**< aaa opcode */
/*  14 */     OP_cmp,      /* &first_byte[0x3d], */ /**< cmp opcode */
/*  15 */     OP_aas,      /* &first_byte[0x3f], */ /**< aas opcode */
/*  16 */     OP_inc,      /* &x64_extensions[0][0], */ /**< inc opcode */
/*  17 */     OP_dec,      /* &x64_extensions[8][0], */ /**< dec opcode */
/*  18 */     OP_push,     /* &first_byte[0x50], */ /**< push opcode */
/*  19 */     OP_push_imm, /* &first_byte[0x68], */ /**< push_imm opcode */
/*  20 */     OP_pop,      /* &first_byte[0x58], */ /**< pop opcode */
/*  21 */     OP_pusha,    /* &first_byte[0x60], */ /**< pusha opcode */
/*  22 */     OP_popa,     /* &first_byte[0x61], */ /**< popa opcode */
/*  23 */     OP_bound,    /* &first_byte[0x62], */ /**< bound opcode */
/*  24 */     OP_arpl,     /* &x64_extensions[16][0], */ /**< arpl opcode */
/*  25 */     OP_imul,     /* &extensions[10][5], */ /**< imul opcode */

/*  26 */     OP_jo_short,     /* &first_byte[0x70], */ /**< jo_short opcode */
/*  27 */     OP_jno_short,    /* &first_byte[0x71], */ /**< jno_short opcode */
/*  28 */     OP_jb_short,     /* &first_byte[0x72], */ /**< jb_short opcode */
/*  29 */     OP_jnb_short,    /* &first_byte[0x73], */ /**< jnb_short opcode */
/*  30 */     OP_jz_short,     /* &first_byte[0x74], */ /**< jz_short opcode */
/*  31 */     OP_jnz_short,    /* &first_byte[0x75], */ /**< jnz_short opcode */
/*  32 */     OP_jbe_short,    /* &first_byte[0x76], */ /**< jbe_short opcode */
/*  33 */     OP_jnbe_short,   /* &first_byte[0x77], */ /**< jnbe_short opcode */
/*  34 */     OP_js_short,     /* &first_byte[0x78], */ /**< js_short opcode */
/*  35 */     OP_jns_short,    /* &first_byte[0x79], */ /**< jns_short opcode */
/*  36 */     OP_jp_short,     /* &first_byte[0x7a], */ /**< jp_short opcode */
/*  37 */     OP_jnp_short,    /* &first_byte[0x7b], */ /**< jnp_short opcode */
/*  38 */     OP_jl_short,     /* &first_byte[0x7c], */ /**< jl_short opcode */
/*  39 */     OP_jnl_short,    /* &first_byte[0x7d], */ /**< jnl_short opcode */
/*  40 */     OP_jle_short,    /* &first_byte[0x7e], */ /**< jle_short opcode */
/*  41 */     OP_jnle_short,   /* &first_byte[0x7f], */ /**< jnle_short opcode */

/*  42 */     OP_call,           /* &first_byte[0xe8], */ /**< call opcode */
/*  43 */     OP_call_ind,       /* &extensions[12][2], */ /**< call_ind opcode */
/*  44 */     OP_call_far,       /* &first_byte[0x9a], */ /**< call_far opcode */
/*  45 */     OP_call_far_ind,   /* &extensions[12][3], */ /**< call_far_ind opcode */
/*  46 */     OP_jmp,            /* &first_byte[0xe9], */ /**< jmp opcode */
/*  47 */     OP_jmp_short,      /* &first_byte[0xeb], */ /**< jmp_short opcode */
/*  48 */     OP_jmp_ind,        /* &extensions[12][4], */ /**< jmp_ind opcode */
/*  49 */     OP_jmp_far,        /* &first_byte[0xea], */ /**< jmp_far opcode */
/*  50 */     OP_jmp_far_ind,    /* &extensions[12][5], */ /**< jmp_far_ind opcode */

/*  51 */     OP_loopne,   /* &first_byte[0xe0], */ /**< loopne opcode */
/*  52 */     OP_loope,    /* &first_byte[0xe1], */ /**< loope opcode */
/*  53 */     OP_loop,     /* &first_byte[0xe2], */ /**< loop opcode */
/*  54 */     OP_jecxz,    /* &first_byte[0xe3], */ /**< jecxz opcode */

    /* point ld & st at eAX & al instrs, they save 1 byte (no modrm),
     * hopefully time taken considering them doesn't offset that */
/*  55 */     OP_mov_ld,      /* &first_byte[0xa1], */ /**< mov_ld opcode */
/*  56 */     OP_mov_st,      /* &first_byte[0xa3], */ /**< mov_st opcode */
    /* PR 250397: store of immed is mov_st not mov_imm, even though can be immed->reg,
     * which we address by sharing part of the mov_st template chain */
/*  57 */     OP_mov_imm,     /* &first_byte[0xb8], */ /**< mov_imm opcode */
/*  58 */     OP_mov_seg,     /* &first_byte[0x8e], */ /**< mov_seg opcode */
/*  59 */     OP_mov_priv,    /* &second_byte[0x20], */ /**< mov_priv opcode */

/*  60 */     OP_test,     /* &first_byte[0xa9], */ /**< test opcode */
/*  61 */     OP_lea,      /* &first_byte[0x8d], */ /**< lea opcode */
/*  62 */     OP_xchg,     /* &first_byte[0x91], */ /**< xchg opcode */
/*  63 */     OP_cwde,     /* &first_byte[0x98], */ /**< cwde opcode */
/*  64 */     OP_cdq,      /* &first_byte[0x99], */ /**< cdq opcode */
/*  65 */     OP_fwait,    /* &first_byte[0x9b], */ /**< fwait opcode */
/*  66 */     OP_pushf,    /* &first_byte[0x9c], */ /**< pushf opcode */
/*  67 */     OP_popf,     /* &first_byte[0x9d], */ /**< popf opcode */
/*  68 */     OP_sahf,     /* &first_byte[0x9e], */ /**< sahf opcode */
/*  69 */     OP_lahf,     /* &first_byte[0x9f], */ /**< lahf opcode */

/*  70 */     OP_ret,       /* &first_byte[0xc2], */ /**< ret opcode */
/*  71 */     OP_ret_far,   /* &first_byte[0xca], */ /**< ret_far opcode */

/*  72 */     OP_les,      /* &vex_prefix_extensions[0][0], */ /**< les opcode */
/*  73 */     OP_lds,      /* &vex_prefix_extensions[1][0], */ /**< lds opcode */
/*  74 */     OP_enter,    /* &first_byte[0xc8], */ /**< enter opcode */
/*  75 */     OP_leave,    /* &first_byte[0xc9], */ /**< leave opcode */
/*  76 */     OP_int3,     /* &first_byte[0xcc], */ /**< int3 opcode */
/*  77 */     OP_int,      /* &first_byte[0xcd], */ /**< int opcode */
/*  78 */     OP_into,     /* &first_byte[0xce], */ /**< into opcode */
/*  79 */     OP_iret,     /* &first_byte[0xcf], */ /**< iret opcode */
/*  80 */     OP_aam,      /* &first_byte[0xd4], */ /**< aam opcode */
/*  81 */     OP_aad,      /* &first_byte[0xd5], */ /**< aad opcode */
/*  82 */     OP_xlat,     /* &first_byte[0xd7], */ /**< xlat opcode */
/*  83 */     OP_in,       /* &first_byte[0xe5], */ /**< in opcode */
/*  84 */     OP_out,      /* &first_byte[0xe7], */ /**< out opcode */
/*  85 */     OP_hlt,      /* &first_byte[0xf4], */ /**< hlt opcode */
/*  86 */     OP_cmc,      /* &first_byte[0xf5], */ /**< cmc opcode */
/*  87 */     OP_clc,      /* &first_byte[0xf8], */ /**< clc opcode */
/*  88 */     OP_stc,      /* &first_byte[0xf9], */ /**< stc opcode */
/*  89 */     OP_cli,      /* &first_byte[0xfa], */ /**< cli opcode */
/*  90 */     OP_sti,      /* &first_byte[0xfb], */ /**< sti opcode */
/*  91 */     OP_cld,      /* &first_byte[0xfc], */ /**< cld opcode */
/*  92 */     OP_std,      /* &first_byte[0xfd], */ /**< std opcode */


/*  93 */     OP_lar,          /* &second_byte[0x02], */ /**< lar opcode */
/*  94 */     OP_lsl,          /* &second_byte[0x03], */ /**< lsl opcode */
/*  95 */     OP_syscall,      /* &second_byte[0x05], */ /**< syscall opcode */
/*  96 */     OP_clts,         /* &second_byte[0x06], */ /**< clts opcode */
/*  97 */     OP_sysret,       /* &second_byte[0x07], */ /**< sysret opcode */
/*  98 */     OP_invd,         /* &second_byte[0x08], */ /**< invd opcode */
/*  99 */     OP_wbinvd,       /* &second_byte[0x09], */ /**< wbinvd opcode */
/* 100 */     OP_ud2a,         /* &second_byte[0x0b], */ /**< ud2a opcode */
/* 101 */     OP_nop_modrm,    /* &second_byte[0x1f], */ /**< nop_modrm opcode */
/* 102 */     OP_movntps,      /* &prefix_extensions[11][0], */ /**< movntps opcode */
/* 103 */     OP_movntpd,      /* &prefix_extensions[11][2], */ /**< movntpd opcode */
/* 104 */     OP_wrmsr,        /* &second_byte[0x30], */ /**< wrmsr opcode */
/* 105 */     OP_rdtsc,        /* &second_byte[0x31], */ /**< rdtsc opcode */
/* 106 */     OP_rdmsr,        /* &second_byte[0x32], */ /**< rdmsr opcode */
/* 107 */     OP_rdpmc,        /* &second_byte[0x33], */ /**< rdpmc opcode */
/* 108 */     OP_sysenter,     /* &second_byte[0x34], */ /**< sysenter opcode */
/* 109 */     OP_sysexit,      /* &second_byte[0x35], */ /**< sysexit opcode */

/* 110 */     OP_cmovo,        /* &second_byte[0x40], */ /**< cmovo opcode */
/* 111 */     OP_cmovno,       /* &second_byte[0x41], */ /**< cmovno opcode */
/* 112 */     OP_cmovb,        /* &second_byte[0x42], */ /**< cmovb opcode */
/* 113 */     OP_cmovnb,       /* &second_byte[0x43], */ /**< cmovnb opcode */
/* 114 */     OP_cmovz,        /* &second_byte[0x44], */ /**< cmovz opcode */
/* 115 */     OP_cmovnz,       /* &second_byte[0x45], */ /**< cmovnz opcode */
/* 116 */     OP_cmovbe,       /* &second_byte[0x46], */ /**< cmovbe opcode */
/* 117 */     OP_cmovnbe,      /* &second_byte[0x47], */ /**< cmovnbe opcode */
/* 118 */     OP_cmovs,        /* &second_byte[0x48], */ /**< cmovs opcode */
/* 119 */     OP_cmovns,       /* &second_byte[0x49], */ /**< cmovns opcode */
/* 120 */     OP_cmovp,        /* &second_byte[0x4a], */ /**< cmovp opcode */
/* 121 */     OP_cmovnp,       /* &second_byte[0x4b], */ /**< cmovnp opcode */
/* 122 */     OP_cmovl,        /* &second_byte[0x4c], */ /**< cmovl opcode */
/* 123 */     OP_cmovnl,       /* &second_byte[0x4d], */ /**< cmovnl opcode */
/* 124 */     OP_cmovle,       /* &second_byte[0x4e], */ /**< cmovle opcode */
/* 125 */     OP_cmovnle,      /* &second_byte[0x4f], */ /**< cmovnle opcode */

/* 126 */     OP_punpcklbw,    /* &prefix_extensions[32][0], */ /**< punpcklbw opcode */
/* 127 */     OP_punpcklwd,    /* &prefix_extensions[33][0], */ /**< punpcklwd opcode */
/* 128 */     OP_punpckldq,    /* &prefix_extensions[34][0], */ /**< punpckldq opcode */
/* 129 */     OP_packsswb,     /* &prefix_extensions[35][0], */ /**< packsswb opcode */
/* 130 */     OP_pcmpgtb,      /* &prefix_extensions[36][0], */ /**< pcmpgtb opcode */
/* 131 */     OP_pcmpgtw,      /* &prefix_extensions[37][0], */ /**< pcmpgtw opcode */
/* 132 */     OP_pcmpgtd,      /* &prefix_extensions[38][0], */ /**< pcmpgtd opcode */
/* 133 */     OP_packuswb,     /* &prefix_extensions[39][0], */ /**< packuswb opcode */
/* 134 */     OP_punpckhbw,    /* &prefix_extensions[40][0], */ /**< punpckhbw opcode */
/* 135 */     OP_punpckhwd,    /* &prefix_extensions[41][0], */ /**< punpckhwd opcode */
/* 136 */     OP_punpckhdq,    /* &prefix_extensions[42][0], */ /**< punpckhdq opcode */
/* 137 */     OP_packssdw,     /* &prefix_extensions[43][0], */ /**< packssdw opcode */
/* 138 */     OP_punpcklqdq,   /* &prefix_extensions[44][2], */ /**< punpcklqdq opcode */
/* 139 */     OP_punpckhqdq,   /* &prefix_extensions[45][2], */ /**< punpckhqdq opcode */
/* 140 */     OP_movd,         /* &prefix_extensions[46][0], */ /**< movd opcode */
/* 141 */     OP_movq,         /* &prefix_extensions[112][0], */ /**< movq opcode */
/* 142 */     OP_movdqu,       /* &prefix_extensions[112][1],  */ /**< movdqu opcode */
/* 143 */     OP_movdqa,       /* &prefix_extensions[112][2], */ /**< movdqa opcode */
/* 144 */     OP_pshufw,       /* &prefix_extensions[47][0], */ /**< pshufw opcode */
/* 145 */     OP_pshufd,       /* &prefix_extensions[47][2], */ /**< pshufd opcode */
/* 146 */     OP_pshufhw,      /* &prefix_extensions[47][1], */ /**< pshufhw opcode */
/* 147 */     OP_pshuflw,      /* &prefix_extensions[47][3], */ /**< pshuflw opcode */
/* 148 */     OP_pcmpeqb,      /* &prefix_extensions[48][0], */ /**< pcmpeqb opcode */
/* 149 */     OP_pcmpeqw,      /* &prefix_extensions[49][0], */ /**< pcmpeqw opcode */
/* 150 */     OP_pcmpeqd,      /* &prefix_extensions[50][0], */ /**< pcmpeqd opcode */
/* 151 */     OP_emms,         /* &vex_L_extensions[0][0], */ /**< emms opcode */

/* 152 */     OP_jo,       /* &second_byte[0x80], */ /**< jo opcode */
/* 153 */     OP_jno,      /* &second_byte[0x81], */ /**< jno opcode */
/* 154 */     OP_jb,       /* &second_byte[0x82], */ /**< jb opcode */
/* 155 */     OP_jnb,      /* &second_byte[0x83], */ /**< jnb opcode */
/* 156 */     OP_jz,       /* &second_byte[0x84], */ /**< jz opcode */
/* 157 */     OP_jnz,      /* &second_byte[0x85], */ /**< jnz opcode */
/* 158 */     OP_jbe,      /* &second_byte[0x86], */ /**< jbe opcode */
/* 159 */     OP_jnbe,     /* &second_byte[0x87], */ /**< jnbe opcode */
/* 160 */     OP_js,       /* &second_byte[0x88], */ /**< js opcode */
/* 161 */     OP_jns,      /* &second_byte[0x89], */ /**< jns opcode */
/* 162 */     OP_jp,       /* &second_byte[0x8a], */ /**< jp opcode */
/* 163 */     OP_jnp,      /* &second_byte[0x8b], */ /**< jnp opcode */
/* 164 */     OP_jl,       /* &second_byte[0x8c], */ /**< jl opcode */
/* 165 */     OP_jnl,      /* &second_byte[0x8d], */ /**< jnl opcode */
/* 166 */     OP_jle,      /* &second_byte[0x8e], */ /**< jle opcode */
/* 167 */     OP_jnle,     /* &second_byte[0x8f], */ /**< jnle opcode */

/* 168 */     OP_seto,         /* &second_byte[0x90], */ /**< seto opcode */
/* 169 */     OP_setno,        /* &second_byte[0x91], */ /**< setno opcode */
/* 170 */     OP_setb,         /* &second_byte[0x92], */ /**< setb opcode */
/* 171 */     OP_setnb,        /* &second_byte[0x93], */ /**< setnb opcode */
/* 172 */     OP_setz,         /* &second_byte[0x94], */ /**< setz opcode */
/* 173 */     OP_setnz,        /* &second_byte[0x95], */ /**< setnz opcode */
/* 174 */     OP_setbe,        /* &second_byte[0x96], */ /**< setbe opcode */
/* 175 */     OP_setnbe,         /* &second_byte[0x97], */ /**< setnbe opcode */
/* 176 */     OP_sets,         /* &second_byte[0x98], */ /**< sets opcode */
/* 177 */     OP_setns,        /* &second_byte[0x99], */ /**< setns opcode */
/* 178 */     OP_setp,         /* &second_byte[0x9a], */ /**< setp opcode */
/* 179 */     OP_setnp,        /* &second_byte[0x9b], */ /**< setnp opcode */
/* 180 */     OP_setl,         /* &second_byte[0x9c], */ /**< setl opcode */
/* 181 */     OP_setnl,        /* &second_byte[0x9d], */ /**< setnl opcode */
/* 182 */     OP_setle,        /* &second_byte[0x9e], */ /**< setle opcode */
/* 183 */     OP_setnle,         /* &second_byte[0x9f], */ /**< setnle opcode */

/* 184 */     OP_cpuid,        /* &second_byte[0xa2], */ /**< cpuid opcode */
/* 185 */     OP_bt,           /* &second_byte[0xa3], */ /**< bt opcode */
/* 186 */     OP_shld,         /* &second_byte[0xa4], */ /**< shld opcode */
/* 187 */     OP_rsm,          /* &second_byte[0xaa], */ /**< rsm opcode */
/* 188 */     OP_bts,          /* &second_byte[0xab], */ /**< bts opcode */
/* 189 */     OP_shrd,         /* &second_byte[0xac], */ /**< shrd opcode */
/* 190 */     OP_cmpxchg,      /* &second_byte[0xb1], */ /**< cmpxchg opcode */
/* 191 */     OP_lss,          /* &second_byte[0xb2], */ /**< lss opcode */
/* 192 */     OP_btr,          /* &second_byte[0xb3], */ /**< btr opcode */
/* 193 */     OP_lfs,          /* &second_byte[0xb4], */ /**< lfs opcode */
/* 194 */     OP_lgs,          /* &second_byte[0xb5], */ /**< lgs opcode */
/* 195 */     OP_movzx,        /* &second_byte[0xb7], */ /**< movzx opcode */
/* 196 */     OP_ud2b,         /* &second_byte[0xb9], */ /**< ud2b opcode */
/* 197 */     OP_btc,          /* &second_byte[0xbb], */ /**< btc opcode */
/* 198 */     OP_bsf,          /* &prefix_extensions[140][0], */ /**< bsf opcode */
/* 199 */     OP_bsr,          /* &prefix_extensions[136][0], */ /**< bsr opcode */
/* 200 */     OP_movsx,        /* &second_byte[0xbf], */ /**< movsx opcode */
/* 201 */     OP_xadd,         /* &second_byte[0xc1], */ /**< xadd opcode */
/* 202 */     OP_movnti,       /* &second_byte[0xc3], */ /**< movnti opcode */
/* 203 */     OP_pinsrw,       /* &prefix_extensions[53][0], */ /**< pinsrw opcode */
/* 204 */     OP_pextrw,       /* &prefix_extensions[54][0], */ /**< pextrw opcode */
/* 205 */     OP_bswap,        /* &second_byte[0xc8], */ /**< bswap opcode */
/* 206 */     OP_psrlw,        /* &prefix_extensions[56][0], */ /**< psrlw opcode */
/* 207 */     OP_psrld,        /* &prefix_extensions[57][0], */ /**< psrld opcode */
/* 208 */     OP_psrlq,        /* &prefix_extensions[58][0], */ /**< psrlq opcode */
/* 209 */     OP_paddq,        /* &prefix_extensions[59][0], */ /**< paddq opcode */
/* 210 */     OP_pmullw,       /* &prefix_extensions[60][0], */ /**< pmullw opcode */
/* 211 */     OP_pmovmskb,     /* &prefix_extensions[62][0], */ /**< pmovmskb opcode */
/* 212 */     OP_psubusb,      /* &prefix_extensions[63][0], */ /**< psubusb opcode */
/* 213 */     OP_psubusw,      /* &prefix_extensions[64][0], */ /**< psubusw opcode */
/* 214 */     OP_pminub,       /* &prefix_extensions[65][0], */ /**< pminub opcode */
/* 215 */     OP_pand,         /* &prefix_extensions[66][0], */ /**< pand opcode */
/* 216 */     OP_paddusb,      /* &prefix_extensions[67][0], */ /**< paddusb opcode */
/* 217 */     OP_paddusw,      /* &prefix_extensions[68][0], */ /**< paddusw opcode */
/* 218 */     OP_pmaxub,       /* &prefix_extensions[69][0], */ /**< pmaxub opcode */
/* 219 */     OP_pandn,        /* &prefix_extensions[70][0], */ /**< pandn opcode */
/* 220 */     OP_pavgb,        /* &prefix_extensions[71][0], */ /**< pavgb opcode */
/* 221 */     OP_psraw,        /* &prefix_extensions[72][0], */ /**< psraw opcode */
/* 222 */     OP_psrad,        /* &prefix_extensions[73][0], */ /**< psrad opcode */
/* 223 */     OP_pavgw,        /* &prefix_extensions[74][0], */ /**< pavgw opcode */
/* 224 */     OP_pmulhuw,      /* &prefix_extensions[75][0], */ /**< pmulhuw opcode */
/* 225 */     OP_pmulhw,       /* &prefix_extensions[76][0], */ /**< pmulhw opcode */
/* 226 */     OP_movntq,       /* &prefix_extensions[78][0], */ /**< movntq opcode */
/* 227 */     OP_movntdq,      /* &prefix_extensions[78][2], */ /**< movntdq opcode */
/* 228 */     OP_psubsb,       /* &prefix_extensions[79][0], */ /**< psubsb opcode */
/* 229 */     OP_psubsw,       /* &prefix_extensions[80][0], */ /**< psubsw opcode */
/* 230 */     OP_pminsw,       /* &prefix_extensions[81][0], */ /**< pminsw opcode */
/* 231 */     OP_por,          /* &prefix_extensions[82][0], */ /**< por opcode */
/* 232 */     OP_paddsb,       /* &prefix_extensions[83][0], */ /**< paddsb opcode */
/* 233 */     OP_paddsw,       /* &prefix_extensions[84][0], */ /**< paddsw opcode */
/* 234 */     OP_pmaxsw,       /* &prefix_extensions[85][0], */ /**< pmaxsw opcode */
/* 235 */     OP_pxor,         /* &prefix_extensions[86][0], */ /**< pxor opcode */
/* 236 */     OP_psllw,        /* &prefix_extensions[87][0], */ /**< psllw opcode */
/* 237 */     OP_pslld,        /* &prefix_extensions[88][0], */ /**< pslld opcode */
/* 238 */     OP_psllq,        /* &prefix_extensions[89][0], */ /**< psllq opcode */
/* 239 */     OP_pmuludq,      /* &prefix_extensions[90][0], */ /**< pmuludq opcode */
/* 240 */     OP_pmaddwd,      /* &prefix_extensions[91][0], */ /**< pmaddwd opcode */
/* 241 */     OP_psadbw,       /* &prefix_extensions[92][0], */ /**< psadbw opcode */
/* 242 */     OP_maskmovq,     /* &prefix_extensions[93][0], */ /**< maskmovq opcode */
/* 243 */     OP_maskmovdqu,   /* &prefix_extensions[93][2], */ /**< maskmovdqu opcode */
/* 244 */     OP_psubb,        /* &prefix_extensions[94][0], */ /**< psubb opcode */
/* 245 */     OP_psubw,        /* &prefix_extensions[95][0], */ /**< psubw opcode */
/* 246 */     OP_psubd,        /* &prefix_extensions[96][0], */ /**< psubd opcode */
/* 247 */     OP_psubq,        /* &prefix_extensions[97][0], */ /**< psubq opcode */
/* 248 */     OP_paddb,        /* &prefix_extensions[98][0], */ /**< paddb opcode */
/* 249 */     OP_paddw,        /* &prefix_extensions[99][0], */ /**< paddw opcode */
/* 250 */     OP_paddd,        /* &prefix_extensions[100][0], */ /**< paddd opcode */
/* 251 */     OP_psrldq,       /* &prefix_extensions[101][2], */ /**< psrldq opcode */
/* 252 */     OP_pslldq,       /* &prefix_extensions[102][2], */ /**< pslldq opcode */


/* 253 */     OP_rol,           /* &extensions[ 4][0], */ /**< rol opcode */
/* 254 */     OP_ror,           /* &extensions[ 4][1], */ /**< ror opcode */
/* 255 */     OP_rcl,           /* &extensions[ 4][2], */ /**< rcl opcode */
/* 256 */     OP_rcr,           /* &extensions[ 4][3], */ /**< rcr opcode */
/* 257 */     OP_shl,           /* &extensions[ 4][4], */ /**< shl opcode */
/* 258 */     OP_shr,           /* &extensions[ 4][5], */ /**< shr opcode */
/* 259 */     OP_sar,           /* &extensions[ 4][7], */ /**< sar opcode */
/* 260 */     OP_not,           /* &extensions[10][2], */ /**< not opcode */
/* 261 */     OP_neg,           /* &extensions[10][3], */ /**< neg opcode */
/* 262 */     OP_mul,           /* &extensions[10][4], */ /**< mul opcode */
/* 263 */     OP_div,           /* &extensions[10][6], */ /**< div opcode */
/* 264 */     OP_idiv,          /* &extensions[10][7], */ /**< idiv opcode */
/* 265 */     OP_sldt,          /* &extensions[13][0], */ /**< sldt opcode */
/* 266 */     OP_str,           /* &extensions[13][1], */ /**< str opcode */
/* 267 */     OP_lldt,          /* &extensions[13][2], */ /**< lldt opcode */
/* 268 */     OP_ltr,           /* &extensions[13][3], */ /**< ltr opcode */
/* 269 */     OP_verr,          /* &extensions[13][4], */ /**< verr opcode */
/* 270 */     OP_verw,          /* &extensions[13][5], */ /**< verw opcode */
/* 271 */     OP_sgdt,          /* &mod_extensions[0][0], */ /**< sgdt opcode */
/* 272 */     OP_sidt,          /* &mod_extensions[1][0], */ /**< sidt opcode */
/* 273 */     OP_lgdt,          /* &mod_extensions[5][0], */ /**< lgdt opcode */
/* 274 */     OP_lidt,          /* &mod_extensions[4][0], */ /**< lidt opcode */
/* 275 */     OP_smsw,          /* &extensions[14][4], */ /**< smsw opcode */
/* 276 */     OP_lmsw,          /* &extensions[14][6], */ /**< lmsw opcode */
/* 277 */     OP_invlpg,        /* &mod_extensions[2][0], */ /**< invlpg opcode */
/* 278 */     OP_cmpxchg8b,     /* &extensions[16][1], */ /**< cmpxchg8b opcode */
/* 279 */     OP_fxsave32,      /* &rex_w_extensions[0][0], */ /**< fxsave opcode */
/* 280 */     OP_fxrstor32,     /* &rex_w_extensions[1][0], */ /**< fxrstor opcode */
/* 281 */     OP_ldmxcsr,       /* &vex_extensions[61][0], */ /**< ldmxcsr opcode */
/* 282 */     OP_stmxcsr,       /* &vex_extensions[62][0], */ /**< stmxcsr opcode */
/* 283 */     OP_lfence,        /* &mod_extensions[6][1], */ /**< lfence opcode */
/* 284 */     OP_mfence,        /* &mod_extensions[7][1], */ /**< mfence opcode */
/* 285 */     OP_clflush,       /* &mod_extensions[3][0], */ /**< clflush opcode */
/* 286 */     OP_sfence,        /* &mod_extensions[3][1], */ /**< sfence opcode */
/* 287 */     OP_prefetchnta,   /* &extensions[23][0], */ /**< prefetchnta opcode */
/* 288 */     OP_prefetcht0,    /* &extensions[23][1], */ /**< prefetcht0 opcode */
/* 289 */     OP_prefetcht1,    /* &extensions[23][2], */ /**< prefetcht1 opcode */
/* 290 */     OP_prefetcht2,    /* &extensions[23][3], */ /**< prefetcht2 opcode */
/* 291 */     OP_prefetch,      /* &extensions[24][0], */ /**< prefetch opcode */
/* 292 */     OP_prefetchw,     /* &extensions[24][1], */ /**< prefetchw opcode */


/* 293 */     OP_movups,      /* &prefix_extensions[ 0][0], */ /**< movups opcode */
/* 294 */     OP_movss,       /* &prefix_extensions[ 0][1], */ /**< movss opcode */
/* 295 */     OP_movupd,      /* &prefix_extensions[ 0][2], */ /**< movupd opcode */
/* 296 */     OP_movsd,       /* &prefix_extensions[ 0][3], */ /**< movsd opcode */
/* 297 */     OP_movlps,      /* &prefix_extensions[ 2][0], */ /**< movlps opcode */
/* 298 */     OP_movlpd,      /* &prefix_extensions[ 2][2], */ /**< movlpd opcode */
/* 299 */     OP_unpcklps,    /* &prefix_extensions[ 4][0], */ /**< unpcklps opcode */
/* 300 */     OP_unpcklpd,    /* &prefix_extensions[ 4][2], */ /**< unpcklpd opcode */
/* 301 */     OP_unpckhps,    /* &prefix_extensions[ 5][0], */ /**< unpckhps opcode */
/* 302 */     OP_unpckhpd,    /* &prefix_extensions[ 5][2], */ /**< unpckhpd opcode */
/* 303 */     OP_movhps,      /* &prefix_extensions[ 6][0], */ /**< movhps opcode */
/* 304 */     OP_movhpd,      /* &prefix_extensions[ 6][2], */ /**< movhpd opcode */
/* 305 */     OP_movaps,      /* &prefix_extensions[ 8][0], */ /**< movaps opcode */
/* 306 */     OP_movapd,      /* &prefix_extensions[ 8][2], */ /**< movapd opcode */
/* 307 */     OP_cvtpi2ps,    /* &prefix_extensions[10][0], */ /**< cvtpi2ps opcode */
/* 308 */     OP_cvtsi2ss,    /* &prefix_extensions[10][1], */ /**< cvtsi2ss opcode */
/* 309 */     OP_cvtpi2pd,    /* &prefix_extensions[10][2], */ /**< cvtpi2pd opcode */
/* 310 */     OP_cvtsi2sd,    /* &prefix_extensions[10][3], */ /**< cvtsi2sd opcode */
/* 311 */     OP_cvttps2pi,   /* &prefix_extensions[12][0], */ /**< cvttps2pi opcode */
/* 312 */     OP_cvttss2si,   /* &prefix_extensions[12][1], */ /**< cvttss2si opcode */
/* 313 */     OP_cvttpd2pi,   /* &prefix_extensions[12][2], */ /**< cvttpd2pi opcode */
/* 314 */     OP_cvttsd2si,   /* &prefix_extensions[12][3], */ /**< cvttsd2si opcode */
/* 315 */     OP_cvtps2pi,    /* &prefix_extensions[13][0], */ /**< cvtps2pi opcode */
/* 316 */     OP_cvtss2si,    /* &prefix_extensions[13][1], */ /**< cvtss2si opcode */
/* 317 */     OP_cvtpd2pi,    /* &prefix_extensions[13][2], */ /**< cvtpd2pi opcode */
/* 318 */     OP_cvtsd2si,    /* &prefix_extensions[13][3], */ /**< cvtsd2si opcode */
/* 319 */     OP_ucomiss,     /* &prefix_extensions[14][0], */ /**< ucomiss opcode */
/* 320 */     OP_ucomisd,     /* &prefix_extensions[14][2], */ /**< ucomisd opcode */
/* 321 */     OP_comiss,      /* &prefix_extensions[15][0], */ /**< comiss opcode */
/* 322 */     OP_comisd,      /* &prefix_extensions[15][2], */ /**< comisd opcode */
/* 323 */     OP_movmskps,    /* &prefix_extensions[16][0], */ /**< movmskps opcode */
/* 324 */     OP_movmskpd,    /* &prefix_extensions[16][2], */ /**< movmskpd opcode */
/* 325 */     OP_sqrtps,      /* &prefix_extensions[17][0], */ /**< sqrtps opcode */
/* 326 */     OP_sqrtss,      /* &prefix_extensions[17][1], */ /**< sqrtss opcode */
/* 327 */     OP_sqrtpd,      /* &prefix_extensions[17][2], */ /**< sqrtpd opcode */
/* 328 */     OP_sqrtsd,      /* &prefix_extensions[17][3], */ /**< sqrtsd opcode */
/* 329 */     OP_rsqrtps,     /* &prefix_extensions[18][0], */ /**< rsqrtps opcode */
/* 330 */     OP_rsqrtss,     /* &prefix_extensions[18][1], */ /**< rsqrtss opcode */
/* 331 */     OP_rcpps,       /* &prefix_extensions[19][0], */ /**< rcpps opcode */
/* 332 */     OP_rcpss,       /* &prefix_extensions[19][1], */ /**< rcpss opcode */
/* 333 */     OP_andps,       /* &prefix_extensions[20][0], */ /**< andps opcode */
/* 334 */     OP_andpd,       /* &prefix_extensions[20][2], */ /**< andpd opcode */
/* 335 */     OP_andnps,      /* &prefix_extensions[21][0], */ /**< andnps opcode */
/* 336 */     OP_andnpd,      /* &prefix_extensions[21][2], */ /**< andnpd opcode */
/* 337 */     OP_orps,        /* &prefix_extensions[22][0], */ /**< orps opcode */
/* 338 */     OP_orpd,        /* &prefix_extensions[22][2], */ /**< orpd opcode */
/* 339 */     OP_xorps,       /* &prefix_extensions[23][0], */ /**< xorps opcode */
/* 340 */     OP_xorpd,       /* &prefix_extensions[23][2], */ /**< xorpd opcode */
/* 341 */     OP_addps,       /* &prefix_extensions[24][0], */ /**< addps opcode */
/* 342 */     OP_addss,       /* &prefix_extensions[24][1], */ /**< addss opcode */
/* 343 */     OP_addpd,       /* &prefix_extensions[24][2], */ /**< addpd opcode */
/* 344 */     OP_addsd,       /* &prefix_extensions[24][3], */ /**< addsd opcode */
/* 345 */     OP_mulps,       /* &prefix_extensions[25][0], */ /**< mulps opcode */
/* 346 */     OP_mulss,       /* &prefix_extensions[25][1], */ /**< mulss opcode */
/* 347 */     OP_mulpd,       /* &prefix_extensions[25][2], */ /**< mulpd opcode */
/* 348 */     OP_mulsd,       /* &prefix_extensions[25][3], */ /**< mulsd opcode */
/* 349 */     OP_cvtps2pd,    /* &prefix_extensions[26][0], */ /**< cvtps2pd opcode */
/* 350 */     OP_cvtss2sd,    /* &prefix_extensions[26][1], */ /**< cvtss2sd opcode */
/* 351 */     OP_cvtpd2ps,    /* &prefix_extensions[26][2], */ /**< cvtpd2ps opcode */
/* 352 */     OP_cvtsd2ss,    /* &prefix_extensions[26][3], */ /**< cvtsd2ss opcode */
/* 353 */     OP_cvtdq2ps,    /* &prefix_extensions[27][0], */ /**< cvtdq2ps opcode */
/* 354 */     OP_cvttps2dq,   /* &prefix_extensions[27][1], */ /**< cvttps2dq opcode */
/* 355 */     OP_cvtps2dq,    /* &prefix_extensions[27][2], */ /**< cvtps2dq opcode */
/* 356 */     OP_subps,       /* &prefix_extensions[28][0], */ /**< subps opcode */
/* 357 */     OP_subss,       /* &prefix_extensions[28][1], */ /**< subss opcode */
/* 358 */     OP_subpd,       /* &prefix_extensions[28][2], */ /**< subpd opcode */
/* 359 */     OP_subsd,       /* &prefix_extensions[28][3], */ /**< subsd opcode */
/* 360 */     OP_minps,       /* &prefix_extensions[29][0], */ /**< minps opcode */
/* 361 */     OP_minss,       /* &prefix_extensions[29][1], */ /**< minss opcode */
/* 362 */     OP_minpd,       /* &prefix_extensions[29][2], */ /**< minpd opcode */
/* 363 */     OP_minsd,       /* &prefix_extensions[29][3], */ /**< minsd opcode */
/* 364 */     OP_divps,       /* &prefix_extensions[30][0], */ /**< divps opcode */
/* 365 */     OP_divss,       /* &prefix_extensions[30][1], */ /**< divss opcode */
/* 366 */     OP_divpd,       /* &prefix_extensions[30][2], */ /**< divpd opcode */
/* 367 */     OP_divsd,       /* &prefix_extensions[30][3], */ /**< divsd opcode */
/* 368 */     OP_maxps,       /* &prefix_extensions[31][0], */ /**< maxps opcode */
/* 369 */     OP_maxss,       /* &prefix_extensions[31][1], */ /**< maxss opcode */
/* 370 */     OP_maxpd,       /* &prefix_extensions[31][2], */ /**< maxpd opcode */
/* 371 */     OP_maxsd,       /* &prefix_extensions[31][3], */ /**< maxsd opcode */
/* 372 */     OP_cmpps,       /* &prefix_extensions[52][0], */ /**< cmpps opcode */
/* 373 */     OP_cmpss,       /* &prefix_extensions[52][1], */ /**< cmpss opcode */
/* 374 */     OP_cmppd,       /* &prefix_extensions[52][2], */ /**< cmppd opcode */
/* 375 */     OP_cmpsd,       /* &prefix_extensions[52][3], */ /**< cmpsd opcode */
/* 376 */     OP_shufps,      /* &prefix_extensions[55][0], */ /**< shufps opcode */
/* 377 */     OP_shufpd,      /* &prefix_extensions[55][2], */ /**< shufpd opcode */
/* 378 */     OP_cvtdq2pd,    /* &prefix_extensions[77][1], */ /**< cvtdq2pd opcode */
/* 379 */     OP_cvttpd2dq,   /* &prefix_extensions[77][2], */ /**< cvttpd2dq opcode */
/* 380 */     OP_cvtpd2dq,    /* &prefix_extensions[77][3], */ /**< cvtpd2dq opcode */
/* 381 */     OP_nop,         /* &rex_b_extensions[0][0], */ /**< nop opcode */
/* 382 */     OP_pause,       /* &prefix_extensions[103][1], */ /**< pause opcode */

/* 383 */     OP_ins,          /* &rep_extensions[1][0], */ /**< ins opcode */
/* 384 */     OP_rep_ins,      /* &rep_extensions[1][2], */ /**< rep_ins opcode */
/* 385 */     OP_outs,         /* &rep_extensions[3][0], */ /**< outs opcode */
/* 386 */     OP_rep_outs,     /* &rep_extensions[3][2], */ /**< rep_outs opcode */
/* 387 */     OP_movs,         /* &rep_extensions[5][0], */ /**< movs opcode */
/* 388 */     OP_rep_movs,     /* &rep_extensions[5][2], */ /**< rep_movs opcode */
/* 389 */     OP_stos,         /* &rep_extensions[7][0], */ /**< stos opcode */
/* 390 */     OP_rep_stos,     /* &rep_extensions[7][2], */ /**< rep_stos opcode */
/* 391 */     OP_lods,         /* &rep_extensions[9][0], */ /**< lods opcode */
/* 392 */     OP_rep_lods,     /* &rep_extensions[9][2], */ /**< rep_lods opcode */
/* 393 */     OP_cmps,         /* &repne_extensions[1][0], */ /**< cmps opcode */
/* 394 */     OP_rep_cmps,     /* &repne_extensions[1][2], */ /**< rep_cmps opcode */
/* 395 */     OP_repne_cmps,   /* &repne_extensions[1][4], */ /**< repne_cmps opcode */
/* 396 */     OP_scas,         /* &repne_extensions[3][0], */ /**< scas opcode */
/* 397 */     OP_rep_scas,     /* &repne_extensions[3][2], */ /**< rep_scas opcode */
/* 398 */     OP_repne_scas,   /* &repne_extensions[3][4], */ /**< repne_scas opcode */


/* 399 */     OP_fadd,      /* &float_low_modrm[0x00], */ /**< fadd opcode */
/* 400 */     OP_fmul,      /* &float_low_modrm[0x01], */ /**< fmul opcode */
/* 401 */     OP_fcom,      /* &float_low_modrm[0x02], */ /**< fcom opcode */
/* 402 */     OP_fcomp,     /* &float_low_modrm[0x03], */ /**< fcomp opcode */
/* 403 */     OP_fsub,      /* &float_low_modrm[0x04], */ /**< fsub opcode */
/* 404 */     OP_fsubr,     /* &float_low_modrm[0x05], */ /**< fsubr opcode */
/* 405 */     OP_fdiv,      /* &float_low_modrm[0x06], */ /**< fdiv opcode */
/* 406 */     OP_fdivr,     /* &float_low_modrm[0x07], */ /**< fdivr opcode */
/* 407 */     OP_fld,       /* &float_low_modrm[0x08], */ /**< fld opcode */
/* 408 */     OP_fst,       /* &float_low_modrm[0x0a], */ /**< fst opcode */
/* 409 */     OP_fstp,      /* &float_low_modrm[0x0b], */ /**< fstp opcode */
/* 410 */     OP_fldenv,    /* &float_low_modrm[0x0c], */ /**< fldenv opcode */
/* 411 */     OP_fldcw,     /* &float_low_modrm[0x0d], */ /**< fldcw opcode */
/* 412 */     OP_fnstenv,   /* &float_low_modrm[0x0e], */ /**< fnstenv opcode */
/* 413 */     OP_fnstcw,    /* &float_low_modrm[0x0f], */ /**< fnstcw opcode */
/* 414 */     OP_fiadd,     /* &float_low_modrm[0x10], */ /**< fiadd opcode */
/* 415 */     OP_fimul,     /* &float_low_modrm[0x11], */ /**< fimul opcode */
/* 416 */     OP_ficom,     /* &float_low_modrm[0x12], */ /**< ficom opcode */
/* 417 */     OP_ficomp,    /* &float_low_modrm[0x13], */ /**< ficomp opcode */
/* 418 */     OP_fisub,     /* &float_low_modrm[0x14], */ /**< fisub opcode */
/* 419 */     OP_fisubr,    /* &float_low_modrm[0x15], */ /**< fisubr opcode */
/* 420 */     OP_fidiv,     /* &float_low_modrm[0x16], */ /**< fidiv opcode */
/* 421 */     OP_fidivr,    /* &float_low_modrm[0x17], */ /**< fidivr opcode */
/* 422 */     OP_fild,      /* &float_low_modrm[0x18], */ /**< fild opcode */
/* 423 */     OP_fist,      /* &float_low_modrm[0x1a], */ /**< fist opcode */
/* 424 */     OP_fistp,     /* &float_low_modrm[0x1b], */ /**< fistp opcode */
/* 425 */     OP_frstor,    /* &float_low_modrm[0x2c], */ /**< frstor opcode */
/* 426 */     OP_fnsave,    /* &float_low_modrm[0x2e], */ /**< fnsave opcode */
/* 427 */     OP_fnstsw,    /* &float_low_modrm[0x2f], */ /**< fnstsw opcode */

/* 428 */     OP_fbld,      /* &float_low_modrm[0x3c], */ /**< fbld opcode */
/* 429 */     OP_fbstp,     /* &float_low_modrm[0x3e], */ /**< fbstp opcode */


/* 430 */     OP_fxch,       /* &float_high_modrm[1][0x08], */ /**< fxch opcode */
/* 431 */     OP_fnop,       /* &float_high_modrm[1][0x10], */ /**< fnop opcode */
/* 432 */     OP_fchs,       /* &float_high_modrm[1][0x20], */ /**< fchs opcode */
/* 433 */     OP_fabs,       /* &float_high_modrm[1][0x21], */ /**< fabs opcode */
/* 434 */     OP_ftst,       /* &float_high_modrm[1][0x24], */ /**< ftst opcode */
/* 435 */     OP_fxam,       /* &float_high_modrm[1][0x25], */ /**< fxam opcode */
/* 436 */     OP_fld1,       /* &float_high_modrm[1][0x28], */ /**< fld1 opcode */
/* 437 */     OP_fldl2t,     /* &float_high_modrm[1][0x29], */ /**< fldl2t opcode */
/* 438 */     OP_fldl2e,     /* &float_high_modrm[1][0x2a], */ /**< fldl2e opcode */
/* 439 */     OP_fldpi,      /* &float_high_modrm[1][0x2b], */ /**< fldpi opcode */
/* 440 */     OP_fldlg2,     /* &float_high_modrm[1][0x2c], */ /**< fldlg2 opcode */
/* 441 */     OP_fldln2,     /* &float_high_modrm[1][0x2d], */ /**< fldln2 opcode */
/* 442 */     OP_fldz,       /* &float_high_modrm[1][0x2e], */ /**< fldz opcode */
/* 443 */     OP_f2xm1,      /* &float_high_modrm[1][0x30], */ /**< f2xm1 opcode */
/* 444 */     OP_fyl2x,      /* &float_high_modrm[1][0x31], */ /**< fyl2x opcode */
/* 445 */     OP_fptan,      /* &float_high_modrm[1][0x32], */ /**< fptan opcode */
/* 446 */     OP_fpatan,     /* &float_high_modrm[1][0x33], */ /**< fpatan opcode */
/* 447 */     OP_fxtract,    /* &float_high_modrm[1][0x34], */ /**< fxtract opcode */
/* 448 */     OP_fprem1,     /* &float_high_modrm[1][0x35], */ /**< fprem1 opcode */
/* 449 */     OP_fdecstp,    /* &float_high_modrm[1][0x36], */ /**< fdecstp opcode */
/* 450 */     OP_fincstp,    /* &float_high_modrm[1][0x37], */ /**< fincstp opcode */
/* 451 */     OP_fprem,      /* &float_high_modrm[1][0x38], */ /**< fprem opcode */
/* 452 */     OP_fyl2xp1,    /* &float_high_modrm[1][0x39], */ /**< fyl2xp1 opcode */
/* 453 */     OP_fsqrt,      /* &float_high_modrm[1][0x3a], */ /**< fsqrt opcode */
/* 454 */     OP_fsincos,    /* &float_high_modrm[1][0x3b], */ /**< fsincos opcode */
/* 455 */     OP_frndint,    /* &float_high_modrm[1][0x3c], */ /**< frndint opcode */
/* 456 */     OP_fscale,     /* &float_high_modrm[1][0x3d], */ /**< fscale opcode */
/* 457 */     OP_fsin,       /* &float_high_modrm[1][0x3e], */ /**< fsin opcode */
/* 458 */     OP_fcos,       /* &float_high_modrm[1][0x3f], */ /**< fcos opcode */
/* 459 */     OP_fcmovb,     /* &float_high_modrm[2][0x00], */ /**< fcmovb opcode */
/* 460 */     OP_fcmove,     /* &float_high_modrm[2][0x08], */ /**< fcmove opcode */
/* 461 */     OP_fcmovbe,    /* &float_high_modrm[2][0x10], */ /**< fcmovbe opcode */
/* 462 */     OP_fcmovu,     /* &float_high_modrm[2][0x18], */ /**< fcmovu opcode */
/* 463 */     OP_fucompp,    /* &float_high_modrm[2][0x29], */ /**< fucompp opcode */
/* 464 */     OP_fcmovnb,    /* &float_high_modrm[3][0x00], */ /**< fcmovnb opcode */
/* 465 */     OP_fcmovne,    /* &float_high_modrm[3][0x08], */ /**< fcmovne opcode */
/* 466 */     OP_fcmovnbe,   /* &float_high_modrm[3][0x10], */ /**< fcmovnbe opcode */
/* 467 */     OP_fcmovnu,    /* &float_high_modrm[3][0x18], */ /**< fcmovnu opcode */
/* 468 */     OP_fnclex,     /* &float_high_modrm[3][0x22], */ /**< fnclex opcode */
/* 469 */     OP_fninit,     /* &float_high_modrm[3][0x23], */ /**< fninit opcode */
/* 470 */     OP_fucomi,     /* &float_high_modrm[3][0x28], */ /**< fucomi opcode */
/* 471 */     OP_fcomi,      /* &float_high_modrm[3][0x30], */ /**< fcomi opcode */
/* 472 */     OP_ffree,      /* &float_high_modrm[5][0x00], */ /**< ffree opcode */
/* 473 */     OP_fucom,      /* &float_high_modrm[5][0x20], */ /**< fucom opcode */
/* 474 */     OP_fucomp,     /* &float_high_modrm[5][0x28], */ /**< fucomp opcode */
/* 475 */     OP_faddp,      /* &float_high_modrm[6][0x00], */ /**< faddp opcode */
/* 476 */     OP_fmulp,      /* &float_high_modrm[6][0x08], */ /**< fmulp opcode */
/* 477 */     OP_fcompp,     /* &float_high_modrm[6][0x19], */ /**< fcompp opcode */
/* 478 */     OP_fsubrp,     /* &float_high_modrm[6][0x20], */ /**< fsubrp opcode */
/* 479 */     OP_fsubp,      /* &float_high_modrm[6][0x28], */ /**< fsubp opcode */
/* 480 */     OP_fdivrp,     /* &float_high_modrm[6][0x30], */ /**< fdivrp opcode */
/* 481 */     OP_fdivp,      /* &float_high_modrm[6][0x38], */ /**< fdivp opcode */
/* 482 */     OP_fucomip,    /* &float_high_modrm[7][0x28], */ /**< fucomip opcode */
/* 483 */     OP_fcomip,     /* &float_high_modrm[7][0x30], */ /**< fcomip opcode */

    /* SSE3 instructions */
/* 484 */     OP_fisttp,       /* &float_low_modrm[0x29], */ /**< fisttp opcode */
/* 485 */     OP_haddpd,       /* &prefix_extensions[114][2], */ /**< haddpd opcode */
/* 486 */     OP_haddps,       /* &prefix_extensions[114][3], */ /**< haddps opcode */
/* 487 */     OP_hsubpd,       /* &prefix_extensions[115][2], */ /**< hsubpd opcode */
/* 488 */     OP_hsubps,       /* &prefix_extensions[115][3], */ /**< hsubps opcode */
/* 489 */     OP_addsubpd,     /* &prefix_extensions[116][2], */ /**< addsubpd opcode */
/* 490 */     OP_addsubps,     /* &prefix_extensions[116][3], */ /**< addsubps opcode */
/* 491 */     OP_lddqu,        /* &prefix_extensions[117][3], */ /**< lddqu opcode */
/* 492 */     OP_monitor,      /* &rm_extensions[1][0], */ /**< monitor opcode */
/* 493 */     OP_mwait,        /* &rm_extensions[1][1], */ /**< mwait opcode */
/* 494 */     OP_movsldup,     /* &prefix_extensions[ 2][1], */ /**< movsldup opcode */
/* 495 */     OP_movshdup,     /* &prefix_extensions[ 6][1], */ /**< movshdup opcode */
/* 496 */     OP_movddup,      /* &prefix_extensions[ 2][3], */ /**< movddup opcode */

    /* 3D-Now! instructions */
/* 497 */     OP_femms,          /* &second_byte[0x0e], */ /**< femms opcode */
/* 498 */     OP_unknown_3dnow,  /* &suffix_extensions[0], */ /**< unknown_3dnow opcode */
/* 499 */     OP_pavgusb,        /* &suffix_extensions[1], */ /**< pavgusb opcode */
/* 500 */     OP_pfadd,          /* &suffix_extensions[2], */ /**< pfadd opcode */
/* 501 */     OP_pfacc,          /* &suffix_extensions[3], */ /**< pfacc opcode */
/* 502 */     OP_pfcmpge,        /* &suffix_extensions[4], */ /**< pfcmpge opcode */
/* 503 */     OP_pfcmpgt,        /* &suffix_extensions[5], */ /**< pfcmpgt opcode */
/* 504 */     OP_pfcmpeq,        /* &suffix_extensions[6], */ /**< pfcmpeq opcode */
/* 505 */     OP_pfmin,          /* &suffix_extensions[7], */ /**< pfmin opcode */
/* 506 */     OP_pfmax,          /* &suffix_extensions[8], */ /**< pfmax opcode */
/* 507 */     OP_pfmul,          /* &suffix_extensions[9], */ /**< pfmul opcode */
/* 508 */     OP_pfrcp,          /* &suffix_extensions[10], */ /**< pfrcp opcode */
/* 509 */     OP_pfrcpit1,       /* &suffix_extensions[11], */ /**< pfrcpit1 opcode */
/* 510 */     OP_pfrcpit2,       /* &suffix_extensions[12], */ /**< pfrcpit2 opcode */
/* 511 */     OP_pfrsqrt,        /* &suffix_extensions[13], */ /**< pfrsqrt opcode */
/* 512 */     OP_pfrsqit1,       /* &suffix_extensions[14], */ /**< pfrsqit1 opcode */
/* 513 */     OP_pmulhrw,        /* &suffix_extensions[15], */ /**< pmulhrw opcode */
/* 514 */     OP_pfsub,          /* &suffix_extensions[16], */ /**< pfsub opcode */
/* 515 */     OP_pfsubr,         /* &suffix_extensions[17], */ /**< pfsubr opcode */
/* 516 */     OP_pi2fd,          /* &suffix_extensions[18], */ /**< pi2fd opcode */
/* 517 */     OP_pf2id,          /* &suffix_extensions[19], */ /**< pf2id opcode */
/* 518 */     OP_pi2fw,          /* &suffix_extensions[20], */ /**< pi2fw opcode */
/* 519 */     OP_pf2iw,          /* &suffix_extensions[21], */ /**< pf2iw opcode */
/* 520 */     OP_pfnacc,         /* &suffix_extensions[22], */ /**< pfnacc opcode */
/* 521 */     OP_pfpnacc,        /* &suffix_extensions[23], */ /**< pfpnacc opcode */
/* 522 */     OP_pswapd,         /* &suffix_extensions[24], */ /**< pswapd opcode */

    /* SSSE3 */
/* 523 */     OP_pshufb,         /* &prefix_extensions[118][0], */ /**< pshufb opcode */
/* 524 */     OP_phaddw,         /* &prefix_extensions[119][0], */ /**< phaddw opcode */
/* 525 */     OP_phaddd,         /* &prefix_extensions[120][0], */ /**< phaddd opcode */
/* 526 */     OP_phaddsw,        /* &prefix_extensions[121][0], */ /**< phaddsw opcode */
/* 527 */     OP_pmaddubsw,      /* &prefix_extensions[122][0], */ /**< pmaddubsw opcode */
/* 528 */     OP_phsubw,         /* &prefix_extensions[123][0], */ /**< phsubw opcode */
/* 529 */     OP_phsubd,         /* &prefix_extensions[124][0], */ /**< phsubd opcode */
/* 530 */     OP_phsubsw,        /* &prefix_extensions[125][0], */ /**< phsubsw opcode */
/* 531 */     OP_psignb,         /* &prefix_extensions[126][0], */ /**< psignb opcode */
/* 532 */     OP_psignw,         /* &prefix_extensions[127][0], */ /**< psignw opcode */
/* 533 */     OP_psignd,         /* &prefix_extensions[128][0], */ /**< psignd opcode */
/* 534 */     OP_pmulhrsw,       /* &prefix_extensions[129][0], */ /**< pmulhrsw opcode */
/* 535 */     OP_pabsb,          /* &prefix_extensions[130][0], */ /**< pabsb opcode */
/* 536 */     OP_pabsw,          /* &prefix_extensions[131][0], */ /**< pabsw opcode */
/* 537 */     OP_pabsd,          /* &prefix_extensions[132][0], */ /**< pabsd opcode */
/* 538 */     OP_palignr,        /* &prefix_extensions[133][0], */ /**< palignr opcode */

    /* SSE4 (incl AMD (SSE4A) and Intel-specific (SSE4.1, SSE4.2) extensions */
/* 539 */     OP_popcnt,         /* &second_byte[0xb8], */ /**< popcnt opcode */
/* 540 */     OP_movntss,        /* &prefix_extensions[11][1], */ /**< movntss opcode */
/* 541 */     OP_movntsd,        /* &prefix_extensions[11][3], */ /**< movntsd opcode */
/* 542 */     OP_extrq,          /* &prefix_extensions[134][2], */ /**< extrq opcode */
/* 543 */     OP_insertq,        /* &prefix_extensions[134][3], */ /**< insertq opcode */
/* 544 */     OP_lzcnt,          /* &prefix_extensions[136][1], */ /**< lzcnt opcode */
/* 545 */     OP_pblendvb,       /* &third_byte_38[16], */ /**< pblendvb opcode */
/* 546 */     OP_blendvps,       /* &third_byte_38[17], */ /**< blendvps opcode */
/* 547 */     OP_blendvpd,       /* &third_byte_38[18], */ /**< blendvpd opcode */
/* 548 */     OP_ptest,          /* &vex_extensions[3][0], */ /**< ptest opcode */
/* 549 */     OP_pmovsxbw,       /* &vex_extensions[4][0], */ /**< pmovsxbw opcode */
/* 550 */     OP_pmovsxbd,       /* &vex_extensions[5][0], */ /**< pmovsxbd opcode */
/* 551 */     OP_pmovsxbq,       /* &vex_extensions[6][0], */ /**< pmovsxbq opcode */
/* 552 */     OP_pmovsxwd,       /* &vex_extensions[7][0], */ /**< pmovsxwd opcode */
/* 553 */     OP_pmovsxwq,       /* &vex_extensions[8][0], */ /**< pmovsxwq opcode */
/* 554 */     OP_pmovsxdq,       /* &vex_extensions[9][0], */ /**< pmovsxdq opcode */
/* 555 */     OP_pmuldq,         /* &vex_extensions[10][0], */ /**< pmuldq opcode */
/* 556 */     OP_pcmpeqq,        /* &vex_extensions[11][0], */ /**< pcmpeqq opcode */
/* 557 */     OP_movntdqa,       /* &vex_extensions[12][0], */ /**< movntdqa opcode */
/* 558 */     OP_packusdw,       /* &vex_extensions[13][0], */ /**< packusdw opcode */
/* 559 */     OP_pmovzxbw,       /* &vex_extensions[14][0], */ /**< pmovzxbw opcode */
/* 560 */     OP_pmovzxbd,       /* &vex_extensions[15][0], */ /**< pmovzxbd opcode */
/* 561 */     OP_pmovzxbq,       /* &vex_extensions[16][0], */ /**< pmovzxbq opcode */
/* 562 */     OP_pmovzxwd,       /* &vex_extensions[17][0], */ /**< pmovzxwd opcode */
/* 563 */     OP_pmovzxwq,       /* &vex_extensions[18][0], */ /**< pmovzxwq opcode */
/* 564 */     OP_pmovzxdq,       /* &vex_extensions[19][0], */ /**< pmovzxdq opcode */
/* 565 */     OP_pcmpgtq,        /* &vex_extensions[20][0], */ /**< pcmpgtq opcode */
/* 566 */     OP_pminsb,         /* &vex_extensions[21][0], */ /**< pminsb opcode */
/* 567 */     OP_pminsd,         /* &vex_extensions[22][0], */ /**< pminsd opcode */
/* 568 */     OP_pminuw,         /* &vex_extensions[23][0], */ /**< pminuw opcode */
/* 569 */     OP_pminud,         /* &vex_extensions[24][0], */ /**< pminud opcode */
/* 570 */     OP_pmaxsb,         /* &vex_extensions[25][0], */ /**< pmaxsb opcode */
/* 571 */     OP_pmaxsd,         /* &vex_extensions[26][0], */ /**< pmaxsd opcode */
/* 572 */     OP_pmaxuw,         /* &vex_extensions[27][0], */ /**< pmaxuw opcode */
/* 573 */     OP_pmaxud,         /* &vex_extensions[28][0], */ /**< pmaxud opcode */
/* 574 */     OP_pmulld,         /* &vex_extensions[29][0], */ /**< pmulld opcode */
/* 575 */     OP_phminposuw,     /* &vex_extensions[30][0], */ /**< phminposuw opcode */
/* 576 */     OP_crc32,          /* &prefix_extensions[139][3], */ /**< crc32 opcode */
/* 577 */     OP_pextrb,         /* &vex_extensions[36][0], */ /**< pextrb opcode */
/* 578 */     OP_pextrd,         /* &vex_extensions[38][0], */ /**< pextrd opcode */
/* 579 */     OP_extractps,      /* &vex_extensions[39][0], */ /**< extractps opcode */
/* 580 */     OP_roundps,        /* &vex_extensions[40][0], */ /**< roundps opcode */
/* 581 */     OP_roundpd,        /* &vex_extensions[41][0], */ /**< roundpd opcode */
/* 582 */     OP_roundss,        /* &vex_extensions[42][0], */ /**< roundss opcode */
/* 583 */     OP_roundsd,        /* &vex_extensions[43][0], */ /**< roundsd opcode */
/* 584 */     OP_blendps,        /* &vex_extensions[44][0], */ /**< blendps opcode */
/* 585 */     OP_blendpd,        /* &vex_extensions[45][0], */ /**< blendpd opcode */
/* 586 */     OP_pblendw,        /* &vex_extensions[46][0], */ /**< pblendw opcode */
/* 587 */     OP_pinsrb,         /* &vex_extensions[47][0], */ /**< pinsrb opcode */
/* 588 */     OP_insertps,       /* &vex_extensions[48][0], */ /**< insertps opcode */
/* 589 */     OP_pinsrd,         /* &vex_extensions[49][0], */ /**< pinsrd opcode */
/* 590 */     OP_dpps,           /* &vex_extensions[50][0], */ /**< dpps opcode */
/* 591 */     OP_dppd,           /* &vex_extensions[51][0], */ /**< dppd opcode */
/* 592 */     OP_mpsadbw,        /* &vex_extensions[52][0], */ /**< mpsadbw opcode */
/* 593 */     OP_pcmpestrm,      /* &vex_extensions[53][0], */ /**< pcmpestrm opcode */
/* 594 */     OP_pcmpestri,      /* &vex_extensions[54][0], */ /**< pcmpestri opcode */
/* 595 */     OP_pcmpistrm,      /* &vex_extensions[55][0], */ /**< pcmpistrm opcode */
/* 596 */     OP_pcmpistri,      /* &vex_extensions[56][0], */ /**< pcmpistri opcode */

    /* x64 */
/* 597 */     OP_movsxd,         /* &x64_extensions[16][1], */ /**< movsxd opcode */
/* 598 */     OP_swapgs,         /* &rm_extensions[2][0], */ /**< swapgs opcode */

    /* VMX */
/* 599 */     OP_vmcall,         /* &rm_extensions[0][1], */ /**< vmcall opcode */
/* 600 */     OP_vmlaunch,       /* &rm_extensions[0][2], */ /**< vmlaunch opcode */
/* 601 */     OP_vmresume,       /* &rm_extensions[0][3], */ /**< vmresume opcode */
/* 602 */     OP_vmxoff,         /* &rm_extensions[0][4], */ /**< vmxoff opcode */
/* 603 */     OP_vmptrst,        /* &mod_extensions[13][0], */ /**< vmptrst opcode */
/* 604 */     OP_vmptrld,        /* &prefix_extensions[137][0], */ /**< vmptrld opcode */
/* 605 */     OP_vmxon,          /* &prefix_extensions[137][1], */ /**< vmxon opcode */
/* 606 */     OP_vmclear,        /* &prefix_extensions[137][2], */ /**< vmclear opcode */
/* 607 */     OP_vmread,         /* &prefix_extensions[134][0], */ /**< vmread opcode */
/* 608 */     OP_vmwrite,        /* &prefix_extensions[135][0], */ /**< vmwrite opcode */

    /* undocumented */
/* 609 */     OP_int1,           /* &first_byte[0xf1], */ /**< int1 opcode */
/* 610 */     OP_salc,           /* &first_byte[0xd6], */ /**< salc opcode */
/* 611 */     OP_ffreep,         /* &float_high_modrm[7][0x00], */ /**< ffreep opcode */

    /* AMD SVM */
/* 612 */     OP_vmrun,          /* &rm_extensions[3][0], */ /**< vmrun opcode */
/* 613 */     OP_vmmcall,        /* &rm_extensions[3][1], */ /**< vmmcall opcode */
/* 614 */     OP_vmload,         /* &rm_extensions[3][2], */ /**< vmload opcode */
/* 615 */     OP_vmsave,         /* &rm_extensions[3][3], */ /**< vmsave opcode */
/* 616 */     OP_stgi,           /* &rm_extensions[3][4], */ /**< stgi opcode */
/* 617 */     OP_clgi,           /* &rm_extensions[3][5], */ /**< clgi opcode */
/* 618 */     OP_skinit,         /* &rm_extensions[3][6], */ /**< skinit opcode */
/* 619 */     OP_invlpga,        /* &rm_extensions[3][7], */ /**< invlpga opcode */
    /* AMD though not part of SVM */
/* 620 */     OP_rdtscp,         /* &rm_extensions[2][1], */ /**< rdtscp opcode */

    /* Intel VMX additions */
/* 621 */     OP_invept,         /* &third_byte_38[49], */ /**< invept opcode */
/* 622 */     OP_invvpid,        /* &third_byte_38[50], */ /**< invvpid opcode */

    /* added in Intel Westmere */
/* 623 */     OP_pclmulqdq,      /* &vex_extensions[57][0], */ /**< pclmulqdq opcode */
/* 624 */     OP_aesimc,         /* &vex_extensions[31][0], */ /**< aesimc opcode */
/* 625 */     OP_aesenc,         /* &vex_extensions[32][0], */ /**< aesenc opcode */
/* 626 */     OP_aesenclast,     /* &vex_extensions[33][0], */ /**< aesenclast opcode */
/* 627 */     OP_aesdec,         /* &vex_extensions[34][0], */ /**< aesdec opcode */
/* 628 */     OP_aesdeclast,     /* &vex_extensions[35][0], */ /**< aesdeclast opcode */
/* 629 */     OP_aeskeygenassist, /* &vex_extensions[58][0], */ /**< aeskeygenassist opcode */

    /* added in Intel Atom */
/* 630 */     OP_movbe,          /* &prefix_extensions[138][0], */ /**< movbe opcode */

    /* added in Intel Sandy Bridge */
/* 631 */     OP_xgetbv,         /* &rm_extensions[4][0], */ /**< xgetbv opcode */
/* 632 */     OP_xsetbv,         /* &rm_extensions[4][1], */ /**< xsetbv opcode */
/* 633 */     OP_xsave32,        /* &rex_w_extensions[2][0], */ /**< xsave opcode */
/* 634 */     OP_xrstor32,       /* &rex_w_extensions[3][0], */ /**< xrstor opcode */
/* 635 */     OP_xsaveopt32,     /* &rex_w_extensions[4][0], */ /**< xsaveopt opcode */

    /* AVX */
/* 636 */     OP_vmovss,         /* &mod_extensions[ 8][0], */ /**< vmovss opcode */
/* 637 */     OP_vmovsd,         /* &mod_extensions[ 9][0], */ /**< vmovsd opcode */
/* 638 */     OP_vmovups,        /* &prefix_extensions[ 0][4], */ /**< vmovups opcode */
/* 639 */     OP_vmovupd,        /* &prefix_extensions[ 0][6], */ /**< vmovupd opcode */
/* 640 */     OP_vmovlps,        /* &prefix_extensions[ 2][4], */ /**< vmovlps opcode */
/* 641 */     OP_vmovsldup,      /* &prefix_extensions[ 2][5], */ /**< vmovsldup opcode */
/* 642 */     OP_vmovlpd,        /* &prefix_extensions[ 2][6], */ /**< vmovlpd opcode */
/* 643 */     OP_vmovddup,       /* &prefix_extensions[ 2][7], */ /**< vmovddup opcode */
/* 644 */     OP_vunpcklps,      /* &prefix_extensions[ 4][4], */ /**< vunpcklps opcode */
/* 645 */     OP_vunpcklpd,      /* &prefix_extensions[ 4][6], */ /**< vunpcklpd opcode */
/* 646 */     OP_vunpckhps,      /* &prefix_extensions[ 5][4], */ /**< vunpckhps opcode */
/* 647 */     OP_vunpckhpd,      /* &prefix_extensions[ 5][6], */ /**< vunpckhpd opcode */
/* 648 */     OP_vmovhps,        /* &prefix_extensions[ 6][4], */ /**< vmovhps opcode */
/* 649 */     OP_vmovshdup,      /* &prefix_extensions[ 6][5], */ /**< vmovshdup opcode */
/* 650 */     OP_vmovhpd,        /* &prefix_extensions[ 6][6], */ /**< vmovhpd opcode */
/* 651 */     OP_vmovaps,        /* &prefix_extensions[ 8][4], */ /**< vmovaps opcode */
/* 652 */     OP_vmovapd,        /* &prefix_extensions[ 8][6], */ /**< vmovapd opcode */
/* 653 */     OP_vcvtsi2ss,      /* &prefix_extensions[10][5], */ /**< vcvtsi2ss opcode */
/* 654 */     OP_vcvtsi2sd,      /* &prefix_extensions[10][7], */ /**< vcvtsi2sd opcode */
/* 655 */     OP_vmovntps,       /* &prefix_extensions[11][4], */ /**< vmovntps opcode */
/* 656 */     OP_vmovntpd,       /* &prefix_extensions[11][6], */ /**< vmovntpd opcode */
/* 657 */     OP_vcvttss2si,     /* &prefix_extensions[12][5], */ /**< vcvttss2si opcode */
/* 658 */     OP_vcvttsd2si,     /* &prefix_extensions[12][7], */ /**< vcvttsd2si opcode */
/* 659 */     OP_vcvtss2si,      /* &prefix_extensions[13][5], */ /**< vcvtss2si opcode */
/* 660 */     OP_vcvtsd2si,      /* &prefix_extensions[13][7], */ /**< vcvtsd2si opcode */
/* 661 */     OP_vucomiss,       /* &prefix_extensions[14][4], */ /**< vucomiss opcode */
/* 662 */     OP_vucomisd,       /* &prefix_extensions[14][6], */ /**< vucomisd opcode */
/* 663 */     OP_vcomiss,        /* &prefix_extensions[15][4], */ /**< vcomiss opcode */
/* 664 */     OP_vcomisd,        /* &prefix_extensions[15][6], */ /**< vcomisd opcode */
/* 665 */     OP_vmovmskps,      /* &prefix_extensions[16][4], */ /**< vmovmskps opcode */
/* 666 */     OP_vmovmskpd,      /* &prefix_extensions[16][6], */ /**< vmovmskpd opcode */
/* 667 */     OP_vsqrtps,        /* &prefix_extensions[17][4], */ /**< vsqrtps opcode */
/* 668 */     OP_vsqrtss,        /* &prefix_extensions[17][5], */ /**< vsqrtss opcode */
/* 669 */     OP_vsqrtpd,        /* &prefix_extensions[17][6], */ /**< vsqrtpd opcode */
/* 670 */     OP_vsqrtsd,        /* &prefix_extensions[17][7], */ /**< vsqrtsd opcode */
/* 671 */     OP_vrsqrtps,       /* &prefix_extensions[18][4], */ /**< vrsqrtps opcode */
/* 672 */     OP_vrsqrtss,       /* &prefix_extensions[18][5], */ /**< vrsqrtss opcode */
/* 673 */     OP_vrcpps,         /* &prefix_extensions[19][4], */ /**< vrcpps opcode */
/* 674 */     OP_vrcpss,         /* &prefix_extensions[19][5], */ /**< vrcpss opcode */
/* 675 */     OP_vandps,         /* &prefix_extensions[20][4], */ /**< vandps opcode */
/* 676 */     OP_vandpd,         /* &prefix_extensions[20][6], */ /**< vandpd opcode */
/* 677 */     OP_vandnps,        /* &prefix_extensions[21][4], */ /**< vandnps opcode */
/* 678 */     OP_vandnpd,        /* &prefix_extensions[21][6], */ /**< vandnpd opcode */
/* 679 */     OP_vorps,          /* &prefix_extensions[22][4], */ /**< vorps opcode */
/* 680 */     OP_vorpd,          /* &prefix_extensions[22][6], */ /**< vorpd opcode */
/* 681 */     OP_vxorps,         /* &prefix_extensions[23][4], */ /**< vxorps opcode */
/* 682 */     OP_vxorpd,         /* &prefix_extensions[23][6], */ /**< vxorpd opcode */
/* 683 */     OP_vaddps,         /* &prefix_extensions[24][4], */ /**< vaddps opcode */
/* 684 */     OP_vaddss,         /* &prefix_extensions[24][5], */ /**< vaddss opcode */
/* 685 */     OP_vaddpd,         /* &prefix_extensions[24][6], */ /**< vaddpd opcode */
/* 686 */     OP_vaddsd,         /* &prefix_extensions[24][7], */ /**< vaddsd opcode */
/* 687 */     OP_vmulps,         /* &prefix_extensions[25][4], */ /**< vmulps opcode */
/* 688 */     OP_vmulss,         /* &prefix_extensions[25][5], */ /**< vmulss opcode */
/* 689 */     OP_vmulpd,         /* &prefix_extensions[25][6], */ /**< vmulpd opcode */
/* 690 */     OP_vmulsd,         /* &prefix_extensions[25][7], */ /**< vmulsd opcode */
/* 691 */     OP_vcvtps2pd,      /* &prefix_extensions[26][4], */ /**< vcvtps2pd opcode */
/* 692 */     OP_vcvtss2sd,      /* &prefix_extensions[26][5], */ /**< vcvtss2sd opcode */
/* 693 */     OP_vcvtpd2ps,      /* &prefix_extensions[26][6], */ /**< vcvtpd2ps opcode */
/* 694 */     OP_vcvtsd2ss,      /* &prefix_extensions[26][7], */ /**< vcvtsd2ss opcode */
/* 695 */     OP_vcvtdq2ps,      /* &prefix_extensions[27][4], */ /**< vcvtdq2ps opcode */
/* 696 */     OP_vcvttps2dq,     /* &prefix_extensions[27][5], */ /**< vcvttps2dq opcode */
/* 697 */     OP_vcvtps2dq,      /* &prefix_extensions[27][6], */ /**< vcvtps2dq opcode */
/* 698 */     OP_vsubps,         /* &prefix_extensions[28][4], */ /**< vsubps opcode */
/* 699 */     OP_vsubss,         /* &prefix_extensions[28][5], */ /**< vsubss opcode */
/* 700 */     OP_vsubpd,         /* &prefix_extensions[28][6], */ /**< vsubpd opcode */
/* 701 */     OP_vsubsd,         /* &prefix_extensions[28][7], */ /**< vsubsd opcode */
/* 702 */     OP_vminps,         /* &prefix_extensions[29][4], */ /**< vminps opcode */
/* 703 */     OP_vminss,         /* &prefix_extensions[29][5], */ /**< vminss opcode */
/* 704 */     OP_vminpd,         /* &prefix_extensions[29][6], */ /**< vminpd opcode */
/* 705 */     OP_vminsd,         /* &prefix_extensions[29][7], */ /**< vminsd opcode */
/* 706 */     OP_vdivps,         /* &prefix_extensions[30][4], */ /**< vdivps opcode */
/* 707 */     OP_vdivss,         /* &prefix_extensions[30][5], */ /**< vdivss opcode */
/* 708 */     OP_vdivpd,         /* &prefix_extensions[30][6], */ /**< vdivpd opcode */
/* 709 */     OP_vdivsd,         /* &prefix_extensions[30][7], */ /**< vdivsd opcode */
/* 710 */     OP_vmaxps,         /* &prefix_extensions[31][4], */ /**< vmaxps opcode */
/* 711 */     OP_vmaxss,         /* &prefix_extensions[31][5], */ /**< vmaxss opcode */
/* 712 */     OP_vmaxpd,         /* &prefix_extensions[31][6], */ /**< vmaxpd opcode */
/* 713 */     OP_vmaxsd,         /* &prefix_extensions[31][7], */ /**< vmaxsd opcode */
/* 714 */     OP_vpunpcklbw,     /* &prefix_extensions[32][6], */ /**< vpunpcklbw opcode */
/* 715 */     OP_vpunpcklwd,     /* &prefix_extensions[33][6], */ /**< vpunpcklwd opcode */
/* 716 */     OP_vpunpckldq,     /* &prefix_extensions[34][6], */ /**< vpunpckldq opcode */
/* 717 */     OP_vpacksswb,      /* &prefix_extensions[35][6], */ /**< vpacksswb opcode */
/* 718 */     OP_vpcmpgtb,       /* &prefix_extensions[36][6], */ /**< vpcmpgtb opcode */
/* 719 */     OP_vpcmpgtw,       /* &prefix_extensions[37][6], */ /**< vpcmpgtw opcode */
/* 720 */     OP_vpcmpgtd,       /* &prefix_extensions[38][6], */ /**< vpcmpgtd opcode */
/* 721 */     OP_vpackuswb,      /* &prefix_extensions[39][6], */ /**< vpackuswb opcode */
/* 722 */     OP_vpunpckhbw,     /* &prefix_extensions[40][6], */ /**< vpunpckhbw opcode */
/* 723 */     OP_vpunpckhwd,     /* &prefix_extensions[41][6], */ /**< vpunpckhwd opcode */
/* 724 */     OP_vpunpckhdq,     /* &prefix_extensions[42][6], */ /**< vpunpckhdq opcode */
/* 725 */     OP_vpackssdw,      /* &prefix_extensions[43][6], */ /**< vpackssdw opcode */
/* 726 */     OP_vpunpcklqdq,    /* &prefix_extensions[44][6], */ /**< vpunpcklqdq opcode */
/* 727 */     OP_vpunpckhqdq,    /* &prefix_extensions[45][6], */ /**< vpunpckhqdq opcode */
/* 728 */     OP_vmovd,          /* &prefix_extensions[46][6], */ /**< vmovd opcode */
/* 729 */     OP_vpshufhw,       /* &prefix_extensions[47][5], */ /**< vpshufhw opcode */
/* 730 */     OP_vpshufd,        /* &prefix_extensions[47][6], */ /**< vpshufd opcode */
/* 731 */     OP_vpshuflw,       /* &prefix_extensions[47][7], */ /**< vpshuflw opcode */
/* 732 */     OP_vpcmpeqb,       /* &prefix_extensions[48][6], */ /**< vpcmpeqb opcode */
/* 733 */     OP_vpcmpeqw,       /* &prefix_extensions[49][6], */ /**< vpcmpeqw opcode */
/* 734 */     OP_vpcmpeqd,       /* &prefix_extensions[50][6], */ /**< vpcmpeqd opcode */
/* 735 */     OP_vmovq,          /* &prefix_extensions[51][5], */ /**< vmovq opcode */
/* 736 */     OP_vcmpps,         /* &prefix_extensions[52][4], */ /**< vcmpps opcode */
/* 737 */     OP_vcmpss,         /* &prefix_extensions[52][5], */ /**< vcmpss opcode */
/* 738 */     OP_vcmppd,         /* &prefix_extensions[52][6], */ /**< vcmppd opcode */
/* 739 */     OP_vcmpsd,         /* &prefix_extensions[52][7], */ /**< vcmpsd opcode */
/* 740 */     OP_vpinsrw,        /* &prefix_extensions[53][6], */ /**< vpinsrw opcode */
/* 741 */     OP_vpextrw,        /* &prefix_extensions[54][6], */ /**< vpextrw opcode */
/* 742 */     OP_vshufps,        /* &prefix_extensions[55][4], */ /**< vshufps opcode */
/* 743 */     OP_vshufpd,        /* &prefix_extensions[55][6], */ /**< vshufpd opcode */
/* 744 */     OP_vpsrlw,         /* &prefix_extensions[56][6], */ /**< vpsrlw opcode */
/* 745 */     OP_vpsrld,         /* &prefix_extensions[57][6], */ /**< vpsrld opcode */
/* 746 */     OP_vpsrlq,         /* &prefix_extensions[58][6], */ /**< vpsrlq opcode */
/* 747 */     OP_vpaddq,         /* &prefix_extensions[59][6], */ /**< vpaddq opcode */
/* 748 */     OP_vpmullw,        /* &prefix_extensions[60][6], */ /**< vpmullw opcode */
/* 749 */     OP_vpmovmskb,      /* &prefix_extensions[62][6], */ /**< vpmovmskb opcode */
/* 750 */     OP_vpsubusb,       /* &prefix_extensions[63][6], */ /**< vpsubusb opcode */
/* 751 */     OP_vpsubusw,       /* &prefix_extensions[64][6], */ /**< vpsubusw opcode */
/* 752 */     OP_vpminub,        /* &prefix_extensions[65][6], */ /**< vpminub opcode */
/* 753 */     OP_vpand,          /* &prefix_extensions[66][6], */ /**< vpand opcode */
/* 754 */     OP_vpaddusb,       /* &prefix_extensions[67][6], */ /**< vpaddusb opcode */
/* 755 */     OP_vpaddusw,       /* &prefix_extensions[68][6], */ /**< vpaddusw opcode */
/* 756 */     OP_vpmaxub,        /* &prefix_extensions[69][6], */ /**< vpmaxub opcode */
/* 757 */     OP_vpandn,         /* &prefix_extensions[70][6], */ /**< vpandn opcode */
/* 758 */     OP_vpavgb,         /* &prefix_extensions[71][6], */ /**< vpavgb opcode */
/* 759 */     OP_vpsraw,         /* &prefix_extensions[72][6], */ /**< vpsraw opcode */
/* 760 */     OP_vpsrad,         /* &prefix_extensions[73][6], */ /**< vpsrad opcode */
/* 761 */     OP_vpavgw,         /* &prefix_extensions[74][6], */ /**< vpavgw opcode */
/* 762 */     OP_vpmulhuw,       /* &prefix_extensions[75][6], */ /**< vpmulhuw opcode */
/* 763 */     OP_vpmulhw,        /* &prefix_extensions[76][6], */ /**< vpmulhw opcode */
/* 764 */     OP_vcvtdq2pd,      /* &prefix_extensions[77][5], */ /**< vcvtdq2pd opcode */
/* 765 */     OP_vcvttpd2dq,     /* &prefix_extensions[77][6], */ /**< vcvttpd2dq opcode */
/* 766 */     OP_vcvtpd2dq,      /* &prefix_extensions[77][7], */ /**< vcvtpd2dq opcode */
/* 767 */     OP_vmovntdq,       /* &prefix_extensions[78][6], */ /**< vmovntdq opcode */
/* 768 */     OP_vpsubsb,        /* &prefix_extensions[79][6], */ /**< vpsubsb opcode */
/* 769 */     OP_vpsubsw,        /* &prefix_extensions[80][6], */ /**< vpsubsw opcode */
/* 770 */     OP_vpminsw,        /* &prefix_extensions[81][6], */ /**< vpminsw opcode */
/* 771 */     OP_vpor,           /* &prefix_extensions[82][6], */ /**< vpor opcode */
/* 772 */     OP_vpaddsb,        /* &prefix_extensions[83][6], */ /**< vpaddsb opcode */
/* 773 */     OP_vpaddsw,        /* &prefix_extensions[84][6], */ /**< vpaddsw opcode */
/* 774 */     OP_vpmaxsw,        /* &prefix_extensions[85][6], */ /**< vpmaxsw opcode */
/* 775 */     OP_vpxor,          /* &prefix_extensions[86][6], */ /**< vpxor opcode */
/* 776 */     OP_vpsllw,         /* &prefix_extensions[87][6], */ /**< vpsllw opcode */
/* 777 */     OP_vpslld,         /* &prefix_extensions[88][6], */ /**< vpslld opcode */
/* 778 */     OP_vpsllq,         /* &prefix_extensions[89][6], */ /**< vpsllq opcode */
/* 779 */     OP_vpmuludq,       /* &prefix_extensions[90][6], */ /**< vpmuludq opcode */
/* 780 */     OP_vpmaddwd,       /* &prefix_extensions[91][6], */ /**< vpmaddwd opcode */
/* 781 */     OP_vpsadbw,        /* &prefix_extensions[92][6], */ /**< vpsadbw opcode */
/* 782 */     OP_vmaskmovdqu,    /* &prefix_extensions[93][6], */ /**< vmaskmovdqu opcode */
/* 783 */     OP_vpsubb,         /* &prefix_extensions[94][6], */ /**< vpsubb opcode */
/* 784 */     OP_vpsubw,         /* &prefix_extensions[95][6], */ /**< vpsubw opcode */
/* 785 */     OP_vpsubd,         /* &prefix_extensions[96][6], */ /**< vpsubd opcode */
/* 786 */     OP_vpsubq,         /* &prefix_extensions[97][6], */ /**< vpsubq opcode */
/* 787 */     OP_vpaddb,         /* &prefix_extensions[98][6], */ /**< vpaddb opcode */
/* 788 */     OP_vpaddw,         /* &prefix_extensions[99][6], */ /**< vpaddw opcode */
/* 789 */     OP_vpaddd,         /* &prefix_extensions[100][6], */ /**< vpaddd opcode */
/* 790 */     OP_vpsrldq,        /* &prefix_extensions[101][6], */ /**< vpsrldq opcode */
/* 791 */     OP_vpslldq,        /* &prefix_extensions[102][6], */ /**< vpslldq opcode */
/* 792 */     OP_vmovdqu,        /* &prefix_extensions[112][5], */ /**< vmovdqu opcode */
/* 793 */     OP_vmovdqa,        /* &prefix_extensions[112][6], */ /**< vmovdqa opcode */
/* 794 */     OP_vhaddpd,        /* &prefix_extensions[114][6], */ /**< vhaddpd opcode */
/* 795 */     OP_vhaddps,        /* &prefix_extensions[114][7], */ /**< vhaddps opcode */
/* 796 */     OP_vhsubpd,        /* &prefix_extensions[115][6], */ /**< vhsubpd opcode */
/* 797 */     OP_vhsubps,        /* &prefix_extensions[115][7], */ /**< vhsubps opcode */
/* 798 */     OP_vaddsubpd,      /* &prefix_extensions[116][6], */ /**< vaddsubpd opcode */
/* 799 */     OP_vaddsubps,      /* &prefix_extensions[116][7], */ /**< vaddsubps opcode */
/* 800 */     OP_vlddqu,         /* &prefix_extensions[117][7], */ /**< vlddqu opcode */
/* 801 */     OP_vpshufb,        /* &prefix_extensions[118][6], */ /**< vpshufb opcode */
/* 802 */     OP_vphaddw,        /* &prefix_extensions[119][6], */ /**< vphaddw opcode */
/* 803 */     OP_vphaddd,        /* &prefix_extensions[120][6], */ /**< vphaddd opcode */
/* 804 */     OP_vphaddsw,       /* &prefix_extensions[121][6], */ /**< vphaddsw opcode */
/* 805 */     OP_vpmaddubsw,     /* &prefix_extensions[122][6], */ /**< vpmaddubsw opcode */
/* 806 */     OP_vphsubw,        /* &prefix_extensions[123][6], */ /**< vphsubw opcode */
/* 807 */     OP_vphsubd,        /* &prefix_extensions[124][6], */ /**< vphsubd opcode */
/* 808 */     OP_vphsubsw,       /* &prefix_extensions[125][6], */ /**< vphsubsw opcode */
/* 809 */     OP_vpsignb,        /* &prefix_extensions[126][6], */ /**< vpsignb opcode */
/* 810 */     OP_vpsignw,        /* &prefix_extensions[127][6], */ /**< vpsignw opcode */
/* 811 */     OP_vpsignd,        /* &prefix_extensions[128][6], */ /**< vpsignd opcode */
/* 812 */     OP_vpmulhrsw,      /* &prefix_extensions[129][6], */ /**< vpmulhrsw opcode */
/* 813 */     OP_vpabsb,         /* &prefix_extensions[130][6], */ /**< vpabsb opcode */
/* 814 */     OP_vpabsw,         /* &prefix_extensions[131][6], */ /**< vpabsw opcode */
/* 815 */     OP_vpabsd,         /* &prefix_extensions[132][6], */ /**< vpabsd opcode */
/* 816 */     OP_vpalignr,       /* &prefix_extensions[133][6], */ /**< vpalignr opcode */
/* 817 */     OP_vpblendvb,      /* &vex_extensions[ 2][1], */ /**< vpblendvb opcode */
/* 818 */     OP_vblendvps,      /* &vex_extensions[ 0][1], */ /**< vblendvps opcode */
/* 819 */     OP_vblendvpd,      /* &vex_extensions[ 1][1], */ /**< vblendvpd opcode */
/* 820 */     OP_vptest,         /* &vex_extensions[ 3][1], */ /**< vptest opcode */
/* 821 */     OP_vpmovsxbw,      /* &vex_extensions[ 4][1], */ /**< vpmovsxbw opcode */
/* 822 */     OP_vpmovsxbd,      /* &vex_extensions[ 5][1], */ /**< vpmovsxbd opcode */
/* 823 */     OP_vpmovsxbq,      /* &vex_extensions[ 6][1], */ /**< vpmovsxbq opcode */
/* 824 */     OP_vpmovsxwd,      /* &vex_extensions[ 7][1], */ /**< vpmovsxwd opcode */
/* 825 */     OP_vpmovsxwq,      /* &vex_extensions[ 8][1], */ /**< vpmovsxwq opcode */
/* 826 */     OP_vpmovsxdq,      /* &vex_extensions[ 9][1], */ /**< vpmovsxdq opcode */
/* 827 */     OP_vpmuldq,        /* &vex_extensions[10][1], */ /**< vpmuldq opcode */
/* 828 */     OP_vpcmpeqq,       /* &vex_extensions[11][1], */ /**< vpcmpeqq opcode */
/* 829 */     OP_vmovntdqa,      /* &vex_extensions[12][1], */ /**< vmovntdqa opcode */
/* 830 */     OP_vpackusdw,      /* &vex_extensions[13][1], */ /**< vpackusdw opcode */
/* 831 */     OP_vpmovzxbw,      /* &vex_extensions[14][1], */ /**< vpmovzxbw opcode */
/* 832 */     OP_vpmovzxbd,      /* &vex_extensions[15][1], */ /**< vpmovzxbd opcode */
/* 833 */     OP_vpmovzxbq,      /* &vex_extensions[16][1], */ /**< vpmovzxbq opcode */
/* 834 */     OP_vpmovzxwd,      /* &vex_extensions[17][1], */ /**< vpmovzxwd opcode */
/* 835 */     OP_vpmovzxwq,      /* &vex_extensions[18][1], */ /**< vpmovzxwq opcode */
/* 836 */     OP_vpmovzxdq,      /* &vex_extensions[19][1], */ /**< vpmovzxdq opcode */
/* 837 */     OP_vpcmpgtq,       /* &vex_extensions[20][1], */ /**< vpcmpgtq opcode */
/* 838 */     OP_vpminsb,        /* &vex_extensions[21][1], */ /**< vpminsb opcode */
/* 839 */     OP_vpminsd,        /* &vex_extensions[22][1], */ /**< vpminsd opcode */
/* 840 */     OP_vpminuw,        /* &vex_extensions[23][1], */ /**< vpminuw opcode */
/* 841 */     OP_vpminud,        /* &vex_extensions[24][1], */ /**< vpminud opcode */
/* 842 */     OP_vpmaxsb,        /* &vex_extensions[25][1], */ /**< vpmaxsb opcode */
/* 843 */     OP_vpmaxsd,        /* &vex_extensions[26][1], */ /**< vpmaxsd opcode */
/* 844 */     OP_vpmaxuw,        /* &vex_extensions[27][1], */ /**< vpmaxuw opcode */
/* 845 */     OP_vpmaxud,        /* &vex_extensions[28][1], */ /**< vpmaxud opcode */
/* 846 */     OP_vpmulld,        /* &vex_extensions[29][1], */ /**< vpmulld opcode */
/* 847 */     OP_vphminposuw,    /* &vex_extensions[30][1], */ /**< vphminposuw opcode */
/* 848 */     OP_vaesimc,        /* &vex_extensions[31][1], */ /**< vaesimc opcode */
/* 849 */     OP_vaesenc,        /* &vex_extensions[32][1], */ /**< vaesenc opcode */
/* 850 */     OP_vaesenclast,    /* &vex_extensions[33][1], */ /**< vaesenclast opcode */
/* 851 */     OP_vaesdec,        /* &vex_extensions[34][1], */ /**< vaesdec opcode */
/* 852 */     OP_vaesdeclast,    /* &vex_extensions[35][1], */ /**< vaesdeclast opcode */
/* 853 */     OP_vpextrb,        /* &vex_extensions[36][1], */ /**< vpextrb opcode */
/* 854 */     OP_vpextrd,        /* &vex_extensions[38][1], */ /**< vpextrd opcode */
/* 855 */     OP_vextractps,     /* &vex_extensions[39][1], */ /**< vextractps opcode */
/* 856 */     OP_vroundps,       /* &vex_extensions[40][1], */ /**< vroundps opcode */
/* 857 */     OP_vroundpd,       /* &vex_extensions[41][1], */ /**< vroundpd opcode */
/* 858 */     OP_vroundss,       /* &vex_extensions[42][1], */ /**< vroundss opcode */
/* 859 */     OP_vroundsd,       /* &vex_extensions[43][1], */ /**< vroundsd opcode */
/* 860 */     OP_vblendps,       /* &vex_extensions[44][1], */ /**< vblendps opcode */
/* 861 */     OP_vblendpd,       /* &vex_extensions[45][1], */ /**< vblendpd opcode */
/* 862 */     OP_vpblendw,       /* &vex_extensions[46][1], */ /**< vpblendw opcode */
/* 863 */     OP_vpinsrb,        /* &vex_extensions[47][1], */ /**< vpinsrb opcode */
/* 864 */     OP_vinsertps,      /* &vex_extensions[48][1], */ /**< vinsertps opcode */
/* 865 */     OP_vpinsrd,        /* &vex_extensions[49][1], */ /**< vpinsrd opcode */
/* 866 */     OP_vdpps,          /* &vex_extensions[50][1], */ /**< vdpps opcode */
/* 867 */     OP_vdppd,          /* &vex_extensions[51][1], */ /**< vdppd opcode */
/* 868 */     OP_vmpsadbw,       /* &vex_extensions[52][1], */ /**< vmpsadbw opcode */
/* 869 */     OP_vpcmpestrm,     /* &vex_extensions[53][1], */ /**< vpcmpestrm opcode */
/* 870 */     OP_vpcmpestri,     /* &vex_extensions[54][1], */ /**< vpcmpestri opcode */
/* 871 */     OP_vpcmpistrm,     /* &vex_extensions[55][1], */ /**< vpcmpistrm opcode */
/* 872 */     OP_vpcmpistri,     /* &vex_extensions[56][1], */ /**< vpcmpistri opcode */
/* 873 */     OP_vpclmulqdq,     /* &vex_extensions[57][1], */ /**< vpclmulqdq opcode */
/* 874 */     OP_vaeskeygenassist, /* &vex_extensions[58][1], */ /**< vaeskeygenassist opcode */
/* 875 */     OP_vtestps,        /* &vex_extensions[59][1], */ /**< vtestps opcode */
/* 876 */     OP_vtestpd,        /* &vex_extensions[60][1], */ /**< vtestpd opcode */
/* 877 */     OP_vzeroupper,     /* &vex_L_extensions[0][1], */ /**< vzeroupper opcode */
/* 878 */     OP_vzeroall,       /* &vex_L_extensions[0][2], */ /**< vzeroall opcode */
/* 879 */     OP_vldmxcsr,       /* &vex_extensions[61][1], */ /**< vldmxcsr opcode */
/* 880 */     OP_vstmxcsr,       /* &vex_extensions[62][1], */ /**< vstmxcsr opcode */
/* 881 */     OP_vbroadcastss,   /* &vex_extensions[64][1], */ /**< vbroadcastss opcode */
/* 882 */     OP_vbroadcastsd,   /* &vex_extensions[65][1], */ /**< vbroadcastsd opcode */
/* 883 */     OP_vbroadcastf128, /* &vex_extensions[66][1], */ /**< vbroadcastf128 opcode */
/* 884 */     OP_vmaskmovps,     /* &vex_extensions[67][1], */ /**< vmaskmovps opcode */
/* 885 */     OP_vmaskmovpd,     /* &vex_extensions[68][1], */ /**< vmaskmovpd opcode */
/* 886 */     OP_vpermilps,      /* &vex_extensions[71][1], */ /**< vpermilps opcode */
/* 887 */     OP_vpermilpd,      /* &vex_extensions[72][1], */ /**< vpermilpd opcode */
/* 888 */     OP_vperm2f128,     /* &vex_extensions[73][1], */ /**< vperm2f128 opcode */
/* 889 */     OP_vinsertf128,    /* &vex_extensions[74][1], */ /**< vinsertf128 opcode */
/* 890 */     OP_vextractf128,   /* &vex_extensions[75][1], */ /**< vextractf128 opcode */

    /* added in Ivy Bridge I believe, and covered by F16C cpuid flag */
/* 891 */     OP_vcvtph2ps,      /* &vex_extensions[63][1], */ /**< vcvtph2ps opcode */
/* 892 */     OP_vcvtps2ph,      /* &vex_extensions[76][1], */ /**< vcvtps2ph opcode */

    /* FMA */
/* 893 */     OP_vfmadd132ps,    /* &vex_W_extensions[ 0][0], */ /**< vfmadd132ps opcode */
/* 894 */     OP_vfmadd132pd,    /* &vex_W_extensions[ 0][1], */ /**< vfmadd132pd opcode */
/* 895 */     OP_vfmadd213ps,    /* &vex_W_extensions[ 1][0], */ /**< vfmadd213ps opcode */
/* 896 */     OP_vfmadd213pd,    /* &vex_W_extensions[ 1][1], */ /**< vfmadd213pd opcode */
/* 897 */     OP_vfmadd231ps,    /* &vex_W_extensions[ 2][0], */ /**< vfmadd231ps opcode */
/* 898 */     OP_vfmadd231pd,    /* &vex_W_extensions[ 2][1], */ /**< vfmadd231pd opcode */
/* 899 */     OP_vfmadd132ss,    /* &vex_W_extensions[ 3][0], */ /**< vfmadd132ss opcode */
/* 900 */     OP_vfmadd132sd,    /* &vex_W_extensions[ 3][1], */ /**< vfmadd132sd opcode */
/* 901 */     OP_vfmadd213ss,    /* &vex_W_extensions[ 4][0], */ /**< vfmadd213ss opcode */
/* 902 */     OP_vfmadd213sd,    /* &vex_W_extensions[ 4][1], */ /**< vfmadd213sd opcode */
/* 903 */     OP_vfmadd231ss,    /* &vex_W_extensions[ 5][0], */ /**< vfmadd231ss opcode */
/* 904 */     OP_vfmadd231sd,    /* &vex_W_extensions[ 5][1], */ /**< vfmadd231sd opcode */
/* 905 */     OP_vfmaddsub132ps, /* &vex_W_extensions[ 6][0], */ /**< vfmaddsub132ps opcode */
/* 906 */     OP_vfmaddsub132pd, /* &vex_W_extensions[ 6][1], */ /**< vfmaddsub132pd opcode */
/* 907 */     OP_vfmaddsub213ps, /* &vex_W_extensions[ 7][0], */ /**< vfmaddsub213ps opcode */
/* 908 */     OP_vfmaddsub213pd, /* &vex_W_extensions[ 7][1], */ /**< vfmaddsub213pd opcode */
/* 909 */     OP_vfmaddsub231ps, /* &vex_W_extensions[ 8][0], */ /**< vfmaddsub231ps opcode */
/* 910 */     OP_vfmaddsub231pd, /* &vex_W_extensions[ 8][1], */ /**< vfmaddsub231pd opcode */
/* 911 */     OP_vfmsubadd132ps, /* &vex_W_extensions[ 9][0], */ /**< vfmsubadd132ps opcode */
/* 912 */     OP_vfmsubadd132pd, /* &vex_W_extensions[ 9][1], */ /**< vfmsubadd132pd opcode */
/* 913 */     OP_vfmsubadd213ps, /* &vex_W_extensions[10][0], */ /**< vfmsubadd213ps opcode */
/* 914 */     OP_vfmsubadd213pd, /* &vex_W_extensions[10][1], */ /**< vfmsubadd213pd opcode */
/* 915 */     OP_vfmsubadd231ps, /* &vex_W_extensions[11][0], */ /**< vfmsubadd231ps opcode */
/* 916 */     OP_vfmsubadd231pd, /* &vex_W_extensions[11][1], */ /**< vfmsubadd231pd opcode */
/* 917 */     OP_vfmsub132ps,    /* &vex_W_extensions[12][0], */ /**< vfmsub132ps opcode */
/* 918 */     OP_vfmsub132pd,    /* &vex_W_extensions[12][1], */ /**< vfmsub132pd opcode */
/* 919 */     OP_vfmsub213ps,    /* &vex_W_extensions[13][0], */ /**< vfmsub213ps opcode */
/* 920 */     OP_vfmsub213pd,    /* &vex_W_extensions[13][1], */ /**< vfmsub213pd opcode */
/* 921 */     OP_vfmsub231ps,    /* &vex_W_extensions[14][0], */ /**< vfmsub231ps opcode */
/* 922 */     OP_vfmsub231pd,    /* &vex_W_extensions[14][1], */ /**< vfmsub231pd opcode */
/* 923 */     OP_vfmsub132ss,    /* &vex_W_extensions[15][0], */ /**< vfmsub132ss opcode */
/* 924 */     OP_vfmsub132sd,    /* &vex_W_extensions[15][1], */ /**< vfmsub132sd opcode */
/* 925 */     OP_vfmsub213ss,    /* &vex_W_extensions[16][0], */ /**< vfmsub213ss opcode */
/* 926 */     OP_vfmsub213sd,    /* &vex_W_extensions[16][1], */ /**< vfmsub213sd opcode */
/* 927 */     OP_vfmsub231ss,    /* &vex_W_extensions[17][0], */ /**< vfmsub231ss opcode */
/* 928 */     OP_vfmsub231sd,    /* &vex_W_extensions[17][1], */ /**< vfmsub231sd opcode */
/* 929 */     OP_vfnmadd132ps,   /* &vex_W_extensions[18][0], */ /**< vfnmadd132ps opcode */
/* 930 */     OP_vfnmadd132pd,   /* &vex_W_extensions[18][1], */ /**< vfnmadd132pd opcode */
/* 931 */     OP_vfnmadd213ps,   /* &vex_W_extensions[19][0], */ /**< vfnmadd213ps opcode */
/* 932 */     OP_vfnmadd213pd,   /* &vex_W_extensions[19][1], */ /**< vfnmadd213pd opcode */
/* 933 */     OP_vfnmadd231ps,   /* &vex_W_extensions[20][0], */ /**< vfnmadd231ps opcode */
/* 934 */     OP_vfnmadd231pd,   /* &vex_W_extensions[20][1], */ /**< vfnmadd231pd opcode */
/* 935 */     OP_vfnmadd132ss,   /* &vex_W_extensions[21][0], */ /**< vfnmadd132ss opcode */
/* 936 */     OP_vfnmadd132sd,   /* &vex_W_extensions[21][1], */ /**< vfnmadd132sd opcode */
/* 937 */     OP_vfnmadd213ss,   /* &vex_W_extensions[22][0], */ /**< vfnmadd213ss opcode */
/* 938 */     OP_vfnmadd213sd,   /* &vex_W_extensions[22][1], */ /**< vfnmadd213sd opcode */
/* 939 */     OP_vfnmadd231ss,   /* &vex_W_extensions[23][0], */ /**< vfnmadd231ss opcode */
/* 940 */     OP_vfnmadd231sd,   /* &vex_W_extensions[23][1], */ /**< vfnmadd231sd opcode */
/* 941 */     OP_vfnmsub132ps,   /* &vex_W_extensions[24][0], */ /**< vfnmsub132ps opcode */
/* 942 */     OP_vfnmsub132pd,   /* &vex_W_extensions[24][1], */ /**< vfnmsub132pd opcode */
/* 943 */     OP_vfnmsub213ps,   /* &vex_W_extensions[25][0], */ /**< vfnmsub213ps opcode */
/* 944 */     OP_vfnmsub213pd,   /* &vex_W_extensions[25][1], */ /**< vfnmsub213pd opcode */
/* 945 */     OP_vfnmsub231ps,   /* &vex_W_extensions[26][0], */ /**< vfnmsub231ps opcode */
/* 946 */     OP_vfnmsub231pd,   /* &vex_W_extensions[26][1], */ /**< vfnmsub231pd opcode */
/* 947 */     OP_vfnmsub132ss,   /* &vex_W_extensions[27][0], */ /**< vfnmsub132ss opcode */
/* 948 */     OP_vfnmsub132sd,   /* &vex_W_extensions[27][1], */ /**< vfnmsub132sd opcode */
/* 949 */     OP_vfnmsub213ss,   /* &vex_W_extensions[28][0], */ /**< vfnmsub213ss opcode */
/* 950 */     OP_vfnmsub213sd,   /* &vex_W_extensions[28][1], */ /**< vfnmsub213sd opcode */
/* 951 */     OP_vfnmsub231ss,   /* &vex_W_extensions[29][0], */ /**< vfnmsub231ss opcode */
/* 952 */     OP_vfnmsub231sd,   /* &vex_W_extensions[29][1], */ /**< vfnmsub231sd opcode */

/* 953 */     OP_movq2dq,        /* &prefix_extensions[61][1], */ /**< movq2dq opcode */
/* 954 */     OP_movdq2q,        /* &prefix_extensions[61][3], */ /**< movdq2q opcode */

/* 955 */     OP_fxsave64,       /* &rex_w_extensions[0][1], */ /**< fxsave64 opcode */
/* 956 */     OP_fxrstor64,      /* &rex_w_extensions[1][1], */ /**< fxrstor64 opcode */
/* 957 */     OP_xsave64,        /* &rex_w_extensions[2][1], */ /**< xsave64 opcode */
/* 958 */     OP_xrstor64,       /* &rex_w_extensions[3][1], */ /**< xrstor64 opcode */
/* 959 */     OP_xsaveopt64,     /* &rex_w_extensions[4][1], */ /**< xsaveopt64 opcode */

    /* added in Intel Ivy Bridge: RDRAND and FSGSBASE cpuid flags */
/* 960 */     OP_rdrand,         /* &mod_extensions[12][1], */ /**< rdrand opcode */
/* 961 */     OP_rdfsbase,       /* &mod_extensions[14][1], */ /**< rdfsbase opcode */
/* 962 */     OP_rdgsbase,       /* &mod_extensions[15][1], */ /**< rdgsbase opcode */
/* 963 */     OP_wrfsbase,       /* &mod_extensions[16][1], */ /**< wrfsbase opcode */
/* 964 */     OP_wrgsbase,       /* &mod_extensions[17][1], */ /**< wrgsbase opcode */

    /* coming in the future but adding now since enough details are known */
/* 965 */     OP_rdseed,         /* &mod_extensions[13][1], */ /**< rdseed opcode */

    /* AMD FMA4 */
/* 966 */     OP_vfmaddsubps,    /* &vex_W_extensions[30][0], */ /**< vfmaddsubps opcode */
/* 967 */     OP_vfmaddsubpd,    /* &vex_W_extensions[31][0], */ /**< vfmaddsubpd opcode */
/* 968 */     OP_vfmsubaddps,    /* &vex_W_extensions[32][0], */ /**< vfmsubaddps opcode */
/* 969 */     OP_vfmsubaddpd,    /* &vex_W_extensions[33][0], */ /**< vfmsubaddpd opcode */
/* 970 */     OP_vfmaddps,       /* &vex_W_extensions[34][0], */ /**< vfmaddps opcode */
/* 971 */     OP_vfmaddpd,       /* &vex_W_extensions[35][0], */ /**< vfmaddpd opcode */
/* 972 */     OP_vfmaddss,       /* &vex_W_extensions[36][0], */ /**< vfmaddss opcode */
/* 973 */     OP_vfmaddsd,       /* &vex_W_extensions[37][0], */ /**< vfmaddsd opcode */
/* 974 */     OP_vfmsubps,       /* &vex_W_extensions[38][0], */ /**< vfmsubps opcode */
/* 975 */     OP_vfmsubpd,       /* &vex_W_extensions[39][0], */ /**< vfmsubpd opcode */
/* 976 */     OP_vfmsubss,       /* &vex_W_extensions[40][0], */ /**< vfmsubss opcode */
/* 977 */     OP_vfmsubsd,       /* &vex_W_extensions[41][0], */ /**< vfmsubsd opcode */
/* 978 */     OP_vfnmaddps,      /* &vex_W_extensions[42][0], */ /**< vfnmaddps opcode */
/* 979 */     OP_vfnmaddpd,      /* &vex_W_extensions[43][0], */ /**< vfnmaddpd opcode */
/* 980 */     OP_vfnmaddss,      /* &vex_W_extensions[44][0], */ /**< vfnmaddss opcode */
/* 981 */     OP_vfnmaddsd,      /* &vex_W_extensions[45][0], */ /**< vfnmaddsd opcode */
/* 982 */     OP_vfnmsubps,      /* &vex_W_extensions[46][0], */ /**< vfnmsubps opcode */
/* 983 */     OP_vfnmsubpd,      /* &vex_W_extensions[47][0], */ /**< vfnmsubpd opcode */
/* 984 */     OP_vfnmsubss,      /* &vex_W_extensions[48][0], */ /**< vfnmsubss opcode */
/* 985 */     OP_vfnmsubsd,      /* &vex_W_extensions[49][0], */ /**< vfnmsubsd opcode */

    /* AMD XOP */
/* 986 */     OP_vfrczps,        /* &xop_extensions[27], */ /**< vfrczps opcode */
/* 987 */     OP_vfrczpd,        /* &xop_extensions[28], */ /**< vfrczpd opcode */
/* 988 */     OP_vfrczss,        /* &xop_extensions[29], */ /**< vfrczss opcode */
/* 989 */     OP_vfrczsd,        /* &xop_extensions[30], */ /**< vfrczsd opcode */
/* 990 */     OP_vpcmov,         /* &vex_W_extensions[50][0], */ /**< vpcmov opcode */
/* 991 */     OP_vpcomb,         /* &xop_extensions[19], */ /**< vpcomb opcode */
/* 992 */     OP_vpcomw,         /* &xop_extensions[20], */ /**< vpcomw opcode */
/* 993 */     OP_vpcomd,         /* &xop_extensions[21], */ /**< vpcomd opcode */
/* 994 */     OP_vpcomq,         /* &xop_extensions[22], */ /**< vpcomq opcode */
/* 995 */     OP_vpcomub,        /* &xop_extensions[23], */ /**< vpcomub opcode */
/* 996 */     OP_vpcomuw,        /* &xop_extensions[24], */ /**< vpcomuw opcode */
/* 997 */     OP_vpcomud,        /* &xop_extensions[25], */ /**< vpcomud opcode */
/* 998 */     OP_vpcomuq,        /* &xop_extensions[26], */ /**< vpcomuq opcode */
/* 999 */     OP_vpermil2pd,     /* &vex_W_extensions[65][0], */ /**< vpermil2pd opcode */
/* 1000 */     OP_vpermil2ps,     /* &vex_W_extensions[64][0], */ /**< vpermil2ps opcode */
/* 1001 */     OP_vphaddbw,       /* &xop_extensions[43], */ /**< vphaddbw opcode */
/* 1002 */     OP_vphaddbd,       /* &xop_extensions[44], */ /**< vphaddbd opcode */
/* 1003 */     OP_vphaddbq,       /* &xop_extensions[45], */ /**< vphaddbq opcode */
/* 1004 */     OP_vphaddwd,       /* &xop_extensions[46], */ /**< vphaddwd opcode */
/* 1005 */     OP_vphaddwq,       /* &xop_extensions[47], */ /**< vphaddwq opcode */
/* 1006 */     OP_vphadddq,       /* &xop_extensions[48], */ /**< vphadddq opcode */
/* 1007 */     OP_vphaddubw,      /* &xop_extensions[49], */ /**< vphaddubw opcode */
/* 1008 */     OP_vphaddubd,      /* &xop_extensions[50], */ /**< vphaddubd opcode */
/* 1009 */     OP_vphaddubq,      /* &xop_extensions[51], */ /**< vphaddubq opcode */
/* 1010 */     OP_vphadduwd,      /* &xop_extensions[52], */ /**< vphadduwd opcode */
/* 1011 */     OP_vphadduwq,      /* &xop_extensions[53], */ /**< vphadduwq opcode */
/* 1012 */     OP_vphaddudq,      /* &xop_extensions[54], */ /**< vphaddudq opcode */
/* 1013 */     OP_vphsubbw,       /* &xop_extensions[55], */ /**< vphsubbw opcode */
/* 1014 */     OP_vphsubwd,       /* &xop_extensions[56], */ /**< vphsubwd opcode */
/* 1015 */     OP_vphsubdq,       /* &xop_extensions[57], */ /**< vphsubdq opcode */
/* 1016 */     OP_vpmacssww,      /* &xop_extensions[ 1], */ /**< vpmacssww opcode */
/* 1017 */     OP_vpmacsswd,      /* &xop_extensions[ 2], */ /**< vpmacsswd opcode */
/* 1018 */     OP_vpmacssdql,     /* &xop_extensions[ 3], */ /**< vpmacssdql opcode */
/* 1019 */     OP_vpmacssdd,      /* &xop_extensions[ 4], */ /**< vpmacssdd opcode */
/* 1020 */     OP_vpmacssdqh,     /* &xop_extensions[ 5], */ /**< vpmacssdqh opcode */
/* 1021 */     OP_vpmacsww,       /* &xop_extensions[ 6], */ /**< vpmacsww opcode */
/* 1022 */     OP_vpmacswd,       /* &xop_extensions[ 7], */ /**< vpmacswd opcode */
/* 1023 */     OP_vpmacsdql,      /* &xop_extensions[ 8], */ /**< vpmacsdql opcode */
/* 1024 */     OP_vpmacsdd,       /* &xop_extensions[ 9], */ /**< vpmacsdd opcode */
/* 1025 */     OP_vpmacsdqh,      /* &xop_extensions[10], */ /**< vpmacsdqh opcode */
/* 1026 */     OP_vpmadcsswd,     /* &xop_extensions[13], */ /**< vpmadcsswd opcode */
/* 1027 */     OP_vpmadcswd,      /* &xop_extensions[14], */ /**< vpmadcswd opcode */
/* 1028 */     OP_vpperm,         /* &vex_W_extensions[51][0], */ /**< vpperm opcode */
/* 1029 */     OP_vprotb,         /* &xop_extensions[15], */ /**< vprotb opcode */
/* 1030 */     OP_vprotw,         /* &xop_extensions[16], */ /**< vprotw opcode */
/* 1031 */     OP_vprotd,         /* &xop_extensions[17], */ /**< vprotd opcode */
/* 1032 */     OP_vprotq,         /* &xop_extensions[18], */ /**< vprotq opcode */
/* 1033 */     OP_vpshlb,         /* &vex_W_extensions[56][0], */ /**< vpshlb opcode */
/* 1034 */     OP_vpshlw,         /* &vex_W_extensions[57][0], */ /**< vpshlw opcode */
/* 1035 */     OP_vpshld,         /* &vex_W_extensions[58][0], */ /**< vpshld opcode */
/* 1036 */     OP_vpshlq,         /* &vex_W_extensions[59][0], */ /**< vpshlq opcode */
/* 1037 */     OP_vpshab,         /* &vex_W_extensions[60][0], */ /**< vpshab opcode */
/* 1038 */     OP_vpshaw,         /* &vex_W_extensions[61][0], */ /**< vpshaw opcode */
/* 1039 */     OP_vpshad,         /* &vex_W_extensions[62][0], */ /**< vpshad opcode */
/* 1040 */     OP_vpshaq,         /* &vex_W_extensions[63][0], */ /**< vpshaq opcode */

    /* AMD TBM */
/* 1041 */     OP_bextr,          /* &prefix_extensions[141][4], */ /**< bextr opcode */
/* 1042 */     OP_blcfill,        /* &extensions[27][1], */ /**< blcfill opcode */
/* 1043 */     OP_blci,           /* &extensions[28][6], */ /**< blci opcode */
/* 1044 */     OP_blcic,          /* &extensions[27][5], */ /**< blcic opcode */
/* 1045 */     OP_blcmsk,         /* &extensions[28][1], */ /**< blcmsk opcode */
/* 1046 */     OP_blcs,           /* &extensions[27][3], */ /**< blcs opcode */
/* 1047 */     OP_blsfill,        /* &extensions[27][2], */ /**< blsfill opcode */
/* 1048 */     OP_blsic,          /* &extensions[27][6], */ /**< blsic opcode */
/* 1049 */     OP_t1mskc,         /* &extensions[27][7], */ /**< t1mskc opcode */
/* 1050 */     OP_tzmsk,          /* &extensions[27][4], */ /**< tzmsk opcode */

    /* AMD LWP */
/* 1051 */     OP_llwpcb,         /* &extensions[29][0], */ /**< llwpcb opcode */
/* 1052 */     OP_slwpcb,         /* &extensions[29][1], */ /**< slwpcb opcode */
/* 1053 */     OP_lwpins,         /* &extensions[30][0], */ /**< lwpins opcode */
/* 1054 */     OP_lwpval,         /* &extensions[30][1], */ /**< lwpval opcode */

    /* Intel BMI1 */
    /* (includes non-immed form of OP_bextr) */
/* 1055 */     OP_andn,           /* &third_byte_38[100], */ /**< andn opcode */
/* 1056 */     OP_blsr,           /* &extensions[31][1], */ /**< blsr opcode */
/* 1057 */     OP_blsmsk,         /* &extensions[31][2], */ /**< blsmsk opcode */
/* 1058 */     OP_blsi,           /* &extensions[31][3], */ /**< blsi opcode */
/* 1059 */     OP_tzcnt,          /* &prefix_extensions[140][1], */ /**< tzcnt opcode */

    /* Intel BMI2 */
/* 1060 */     OP_bzhi,           /* &prefix_extensions[142][4], */ /**< bzhi opcode */
/* 1061 */     OP_pext,           /* &prefix_extensions[142][6], */ /**< pext opcode */
/* 1062 */     OP_pdep,           /* &prefix_extensions[142][7], */ /**< pdep opcode */
/* 1063 */     OP_sarx,           /* &prefix_extensions[141][5], */ /**< sarx opcode */
/* 1064 */     OP_shlx,           /* &prefix_extensions[141][6], */ /**< shlx opcode */
/* 1065 */     OP_shrx,           /* &prefix_extensions[141][7], */ /**< shrx opcode */
/* 1066 */     OP_rorx,           /* &third_byte_3a[56], */ /**< rorx opcode */
/* 1067 */     OP_mulx,           /* &prefix_extensions[143][7], */ /**< mulx opcode */

    /* Intel Safer Mode Extensions */
/* 1068 */     OP_getsec,         /* &second_byte[0x37], */ /**< getsec opcode */

    /* Misc Intel additions */
/* 1069 */     OP_vmfunc,         /* &rm_extensions[4][4], */ /**< vmfunc opcode */
/* 1070 */     OP_invpcid,        /* &third_byte_38[103], */ /**< invpcid opcode */

    /* Intel TSX */
/* 1071 */     OP_xabort,         /* &extensions[17][7], */ /**< xabort opcode */
/* 1072 */     OP_xbegin,         /* &extensions[18][7], */ /**< xbegin opcode */
/* 1073 */     OP_xend,           /* &rm_extensions[4][5], */ /**< xend opcode */
/* 1074 */     OP_xtest,          /* &rm_extensions[4][6], */ /**< xtest opcode */

    /* AVX2 */
/* 1075 */     OP_vpgatherdd,     /* &vex_W_extensions[66][0], */ /**< vpgatherdd opcode */
/* 1076 */     OP_vpgatherdq,     /* &vex_W_extensions[66][1], */ /**< vpgatherdq opcode */
/* 1077 */     OP_vpgatherqd,     /* &vex_W_extensions[67][0], */ /**< vpgatherqd opcode */
/* 1078 */     OP_vpgatherqq,     /* &vex_W_extensions[67][1], */ /**< vpgatherqq opcode */
/* 1079 */     OP_vgatherdps,     /* &vex_W_extensions[68][0], */ /**< vgatherdps opcode */
/* 1080 */     OP_vgatherdpd,     /* &vex_W_extensions[68][1], */ /**< vgatherdpd opcode */
/* 1081 */     OP_vgatherqps,     /* &vex_W_extensions[69][0], */ /**< vgatherqps opcode */
/* 1082 */     OP_vgatherqpd,     /* &vex_W_extensions[69][1], */ /**< vgatherqpd opcode */
/* 1083 */     OP_vbroadcasti128,  /* &third_byte_38[108], */ /**< vbroadcasti128 opcode */
/* 1084 */     OP_vinserti128,    /* &third_byte_3a[57], */ /**< vinserti128 opcode */
/* 1085 */     OP_vextracti128,   /* &third_byte_3a[58], */ /**< vextracti128 opcode */
/* 1086 */     OP_vpmaskmovd,     /* &vex_W_extensions[70][0], */ /**< vpmaskmovd opcode */
/* 1087 */     OP_vpmaskmovq,     /* &vex_W_extensions[70][1], */ /**< vpmaskmovq opcode */
/* 1088 */     OP_vperm2i128,     /* &vex_W_extensions[69][1], */ /**< vperm2i128 opcode */
/* 1089 */     OP_vpermd,         /* &third_byte_38[112], */ /**< vpermd opcode */
/* 1090 */     OP_vpermps,        /* &third_byte_38[111], */ /**< vpermps opcode */
/* 1091 */     OP_vpermq,         /* &third_byte_3a[59], */ /**< vpermq opcode */
/* 1092 */     OP_vpermpd,        /* &third_byte_3a[60], */ /**< vpermpd opcode */
/* 1093 */     OP_vpblendd,       /* &third_byte_3a[61], */ /**< vpblendd opcode */
/* 1094 */     OP_vpsllvd,        /* &vex_W_extensions[73][0], */ /**< vpsllvd opcode */
/* 1095 */     OP_vpsllvq,        /* &vex_W_extensions[73][1], */ /**< vpsllvq opcode */
/* 1096 */     OP_vpsravd,        /* &third_byte_38[114], */ /**< vpsravd opcode */
/* 1097 */     OP_vpsrlvd,        /* &vex_W_extensions[72][0], */ /**< vpsrlvd opcode */
/* 1098 */     OP_vpsrlvq,        /* &vex_W_extensions[72][1], */ /**< vpsrlvq opcode */

    /* Keep these at the end so that ifdefs don't change internal enum values */
#ifdef IA32_ON_IA64
/* 1099 */     OP_jmpe,       /* &extensions[13][6], */ /**< jmpe opcode */
/* 1100 */     OP_jmpe_abs,   /* &second_byte[0xb8], */ /**< jmpe_abs opcode */
#endif

    OP_AFTER_LAST,
    OP_FIRST = OP_add,            /**< First real opcode. */
    OP_LAST  = OP_AFTER_LAST - 1, /**< Last real opcode. */
};

#ifdef IA32_ON_IA64
/* redefine instead of if else so works with genapi.pl script */
#define OP_LAST OP_jmpe_abs
#endif

/* alternative names */
/* we do not equate the fwait+op opcodes
 *   fstsw, fstcw, fstenv, finit, fclex
 * for us that has to be a sequence of instructions: a separate fwait
 */
/* XXX i#1307: we could add extra decode table layers to print the proper name
 * when we disassemble these, but it's not clear it's worth the effort.
 */
/* 16-bit versions that have different names */
#define OP_cbw        OP_cwde /**< Alternative opcode name for 16-bit version. */
#define OP_cwd        OP_cdq /**< Alternative opcode name for 16-bit version. */
#define OP_jcxz       OP_jecxz /**< Alternative opcode name for 16-bit version. */
/* 64-bit versions that have different names */
#define OP_cdqe       OP_cwde /**< Alternative opcode name for 64-bit version. */
#define OP_cqo        OP_cdq /**< Alternative opcode name for 64-bit version. */
#define OP_jrcxz      OP_jecxz     /**< Alternative opcode name for 64-bit version. */
#define OP_cmpxchg16b OP_cmpxchg8b /**< Alternative opcode name for 64-bit version. */
#define OP_pextrq     OP_pextrd    /**< Alternative opcode name for 64-bit version. */
#define OP_pinsrq     OP_pinsrd    /**< Alternative opcode name for 64-bit version. */
#define OP_vpextrq     OP_vpextrd    /**< Alternative opcode name for 64-bit version. */
#define OP_vpinsrq     OP_vpinsrd    /**< Alternative opcode name for 64-bit version. */
/* reg-reg version has different name */
#define OP_movhlps    OP_movlps /**< Alternative opcode name for reg-reg version. */
#define OP_movlhps    OP_movhps /**< Alternative opcode name for reg-reg version. */
#define OP_vmovhlps    OP_vmovlps /**< Alternative opcode name for reg-reg version. */
#define OP_vmovlhps    OP_vmovhps /**< Alternative opcode name for reg-reg version. */
/* condition codes */
#define OP_jae_short  OP_jnb_short  /**< Alternative opcode name. */
#define OP_jnae_short OP_jb_short   /**< Alternative opcode name. */
#define OP_ja_short   OP_jnbe_short /**< Alternative opcode name. */
#define OP_jna_short  OP_jbe_short  /**< Alternative opcode name. */
#define OP_je_short   OP_jz_short   /**< Alternative opcode name. */
#define OP_jne_short  OP_jnz_short  /**< Alternative opcode name. */
#define OP_jge_short  OP_jnl_short  /**< Alternative opcode name. */
#define OP_jg_short   OP_jnle_short /**< Alternative opcode name. */
#define OP_jae  OP_jnb        /**< Alternative opcode name. */
#define OP_jnae OP_jb         /**< Alternative opcode name. */
#define OP_ja   OP_jnbe       /**< Alternative opcode name. */
#define OP_jna  OP_jbe        /**< Alternative opcode name. */
#define OP_je   OP_jz         /**< Alternative opcode name. */
#define OP_jne  OP_jnz        /**< Alternative opcode name. */
#define OP_jge  OP_jnl        /**< Alternative opcode name. */
#define OP_jg   OP_jnle       /**< Alternative opcode name. */
#define OP_setae  OP_setnb    /**< Alternative opcode name. */
#define OP_setnae OP_setb     /**< Alternative opcode name. */
#define OP_seta   OP_setnbe   /**< Alternative opcode name. */
#define OP_setna  OP_setbe    /**< Alternative opcode name. */
#define OP_sete   OP_setz     /**< Alternative opcode name. */
#define OP_setne  OP_setnz    /**< Alternative opcode name. */
#define OP_setge  OP_setnl    /**< Alternative opcode name. */
#define OP_setg   OP_setnle   /**< Alternative opcode name. */
#define OP_cmovae  OP_cmovnb  /**< Alternative opcode name. */
#define OP_cmovnae OP_cmovb   /**< Alternative opcode name. */
#define OP_cmova   OP_cmovnbe /**< Alternative opcode name. */
#define OP_cmovna  OP_cmovbe  /**< Alternative opcode name. */
#define OP_cmove   OP_cmovz   /**< Alternative opcode name. */
#define OP_cmovne  OP_cmovnz  /**< Alternative opcode name. */
#define OP_cmovge  OP_cmovnl  /**< Alternative opcode name. */
#define OP_cmovg   OP_cmovnle /**< Alternative opcode name. */
#ifndef X64
# define OP_fxsave   OP_fxsave32   /**< Alternative opcode name. */
# define OP_fxrstor  OP_fxrstor32  /**< Alternative opcode name. */
# define OP_xsave    OP_xsave32    /**< Alternative opcode name. */
# define OP_xrstor   OP_xrstor32   /**< Alternative opcode name. */
# define OP_xsaveopt OP_xsaveopt32 /**< Alternative opcode name. */
#endif
#define OP_wait   OP_fwait /**< Alternative opcode name. */
#define OP_sal    OP_shl /**< Alternative opcode name. */
/* undocumented opcodes */
#define OP_icebp OP_int1
#define OP_setalc OP_salc

/****************************************************************************/
/* DR_API EXPORT END */

enum {
    RAW_OPCODE_nop             = 0x90,
    RAW_OPCODE_jmp_short       = 0xeb,
    RAW_OPCODE_call            = 0xe8,
    RAW_OPCODE_ret             = 0xc3,
    RAW_OPCODE_jmp             = 0xe9,
    RAW_OPCODE_push_imm32      = 0x68,
    RAW_OPCODE_pop_eax         = 0x58,
    RAW_OPCODE_jcc_short_start = 0x70,
    RAW_OPCODE_jcc_short_end   = 0x7f,
    RAW_OPCODE_jcc_byte1       = 0x0f,
    RAW_OPCODE_jcc_byte2_start = 0x80,
    RAW_OPCODE_jcc_byte2_end   = 0x8f,
    RAW_OPCODE_loop_start      = 0xe0,
    RAW_OPCODE_loop_end        = 0xe3,
    RAW_OPCODE_lea             = 0x8d,
    RAW_PREFIX_jcc_not_taken   = 0x2e,
    RAW_PREFIX_jcc_taken       = 0x3e,
    RAW_PREFIX_lock            = 0xf0,
    RAW_PREFIX_xacquire        = 0xf2,
    RAW_PREFIX_xrelease        = 0xf3,
};

enum { /* FIXME: vs RAW_OPCODE_* enum */
    FS_SEG_OPCODE        = 0x64,
    GS_SEG_OPCODE        = 0x65,

    /* For Windows, we piggyback on native TLS via gs for x64 and fs for x86.
     * For Linux, we steal a segment register, and so use fs for x86 (where
     * pthreads uses gs) and gs for x64 (where pthreads uses fs) (presumably
     * to avoid conflicts w/ wine).
     */
#ifdef X64
    TLS_SEG_OPCODE       = GS_SEG_OPCODE,
#else
    TLS_SEG_OPCODE       = FS_SEG_OPCODE,
#endif

    DATA_PREFIX_OPCODE   = 0x66,
    ADDR_PREFIX_OPCODE   = 0x67,
    REPNE_PREFIX_OPCODE  = 0xf2,
    REP_PREFIX_OPCODE    = 0xf3,
    REX_PREFIX_BASE_OPCODE = 0x40,
    REX_PREFIX_W_OPFLAG    = 0x8,
    REX_PREFIX_R_OPFLAG    = 0x4,
    REX_PREFIX_X_OPFLAG    = 0x2,
    REX_PREFIX_B_OPFLAG    = 0x1,
    REX_PREFIX_ALL_OPFLAGS = 0xf,
    MOV_REG2MEM_OPCODE   = 0x89,
    MOV_MEM2REG_OPCODE   = 0x8b,
    MOV_XAX2MEM_OPCODE   = 0xa3, /* no ModRm */
    MOV_MEM2XAX_OPCODE   = 0xa1, /* no ModRm */
    MOV_IMM2XAX_OPCODE   = 0xb8, /* no ModRm */
    MOV_IMM2XBX_OPCODE   = 0xbb, /* no ModRm */
    MOV_IMM2MEM_OPCODE   = 0xc7, /* has ModRm */
    JECXZ_OPCODE         = 0xe3,
    JMP_SHORT_OPCODE     = 0xeb,
    JMP_OPCODE           = 0xe9,
    JNE_OPCODE_1         = 0x0f,
    SAHF_OPCODE          = 0x9e,
    LAHF_OPCODE          = 0x9f,
    SETO_OPCODE_1        = 0x0f,
    SETO_OPCODE_2        = 0x90,
    ADD_AL_OPCODE        = 0x04,
    INC_MEM32_OPCODE_1   = 0xff, /* has /0 as well */
    MODRM16_DISP16       = 0x06, /* see vol.2 Table 2-1 for modR/M */
    SIB_DISP32           = 0x25, /* see vol.2 Table 2-1 for modR/M */
};

#endif /* _OPCODE_H_ */
