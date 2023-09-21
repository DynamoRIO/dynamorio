/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

#ifndef _DR_IR_OPCODES_X86_H_
#define _DR_IR_OPCODES_X86_H_ 1

/****************************************************************************
 * OPCODES
 */
/**
 * @file dr_ir_opcodes_x86.h
 * @brief Instruction opcode constants for IA-32 and AMD64.
 */
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
 *   8) add binutils tests in third_party/binutils/test_decenc
 */
/** Opcode constants for use in the instr_t data structure. */
enum {
    /*   0 */ OP_INVALID,
    /* NULL, */ /**< INVALID opcode */
    /*   1 */ OP_UNDECODED,
    /* NULL, */ /**< UNDECODED opcode */
    /*   2 */ OP_CONTD,
    /* NULL, */ /**< CONTD opcode */
    /*   3 */ OP_LABEL,
    /* NULL, */ /**< LABEL opcode */

    /*   4 */ OP_add,      /**< IA-32/AMD64 add opcode. */
    /*   5 */ OP_or,       /**< IA-32/AMD64 or opcode. */
    /*   6 */ OP_adc,      /**< IA-32/AMD64 adc opcode. */
    /*   7 */ OP_sbb,      /**< IA-32/AMD64 sbb opcode. */
    /*   8 */ OP_and,      /**< IA-32/AMD64 and opcode. */
    /*   9 */ OP_daa,      /**< IA-32/AMD64 daa opcode. */
    /*  10 */ OP_sub,      /**< IA-32/AMD64 sub opcode. */
    /*  11 */ OP_das,      /**< IA-32/AMD64 das opcode. */
    /*  12 */ OP_xor,      /**< IA-32/AMD64 xor opcode. */
    /*  13 */ OP_aaa,      /**< IA-32/AMD64 aaa opcode. */
    /*  14 */ OP_cmp,      /**< IA-32/AMD64 cmp opcode. */
    /*  15 */ OP_aas,      /**< IA-32/AMD64 aas opcode. */
    /*  16 */ OP_inc,      /**< IA-32/AMD64 inc opcode. */
    /*  17 */ OP_dec,      /**< IA-32/AMD64 dec opcode. */
    /*  18 */ OP_push,     /**< IA-32/AMD64 push opcode. */
    /*  19 */ OP_push_imm, /**< IA-32/AMD64 push_imm opcode. */
    /*  20 */ OP_pop,      /**< IA-32/AMD64 pop opcode. */
    /*  21 */ OP_pusha,    /**< IA-32/AMD64 pusha opcode. */
    /*  22 */ OP_popa,     /**< IA-32/AMD64 popa opcode. */
    /*  23 */ OP_bound,    /**< IA-32/AMD64 bound opcode. */
    /*  24 */ OP_arpl,     /**< IA-32/AMD64 arpl opcode. */
    /*  25 */ OP_imul,     /**< IA-32/AMD64 imul opcode. */

    /*  26 */ OP_jo_short,   /**< IA-32/AMD64 jo_short opcode. */
    /*  27 */ OP_jno_short,  /**< IA-32/AMD64 jno_short opcode. */
    /*  28 */ OP_jb_short,   /**< IA-32/AMD64 jb_short opcode. */
    /*  29 */ OP_jnb_short,  /**< IA-32/AMD64 jnb_short opcode. */
    /*  30 */ OP_jz_short,   /**< IA-32/AMD64 jz_short opcode. */
    /*  31 */ OP_jnz_short,  /**< IA-32/AMD64 jnz_short opcode. */
    /*  32 */ OP_jbe_short,  /**< IA-32/AMD64 jbe_short opcode. */
    /*  33 */ OP_jnbe_short, /**< IA-32/AMD64 jnbe_short opcode. */
    /*  34 */ OP_js_short,   /**< IA-32/AMD64 js_short opcode. */
    /*  35 */ OP_jns_short,  /**< IA-32/AMD64 jns_short opcode. */
    /*  36 */ OP_jp_short,   /**< IA-32/AMD64 jp_short opcode. */
    /*  37 */ OP_jnp_short,  /**< IA-32/AMD64 jnp_short opcode. */
    /*  38 */ OP_jl_short,   /**< IA-32/AMD64 jl_short opcode. */
    /*  39 */ OP_jnl_short,  /**< IA-32/AMD64 jnl_short opcode. */
    /*  40 */ OP_jle_short,  /**< IA-32/AMD64 jle_short opcode. */
    /*  41 */ OP_jnle_short, /**< IA-32/AMD64 jnle_short opcode. */

    /*  42 */ OP_call,         /**< IA-32/AMD64 call opcode. */
    /*  43 */ OP_call_ind,     /**< IA-32/AMD64 call_ind opcode. */
    /*  44 */ OP_call_far,     /**< IA-32/AMD64 call_far opcode. */
    /*  45 */ OP_call_far_ind, /**< IA-32/AMD64 call_far_ind opcode. */
    /*  46 */ OP_jmp,          /**< IA-32/AMD64 jmp opcode. */
    /*  47 */ OP_jmp_short,    /**< IA-32/AMD64 jmp_short opcode. */
    /*  48 */ OP_jmp_ind,      /**< IA-32/AMD64 jmp_ind opcode. */
    /*  49 */ OP_jmp_far,      /**< IA-32/AMD64 jmp_far opcode. */
    /*  50 */ OP_jmp_far_ind,  /**< IA-32/AMD64 jmp_far_ind opcode. */

    /*  51 */ OP_loopne, /**< IA-32/AMD64 loopne opcode. */
    /*  52 */ OP_loope,  /**< IA-32/AMD64 loope opcode. */
    /*  53 */ OP_loop,   /**< IA-32/AMD64 loop opcode. */
    /*  54 */ OP_jecxz,  /**< IA-32/AMD64 jecxz opcode. */

    /* point ld & st at eAX & al instrs, they save 1 byte (no modrm),
     * hopefully time taken considering them doesn't offset that */
    /*  55 */ OP_mov_ld, /**< IA-32/AMD64 mov_ld opcode. */
    /*  56 */ OP_mov_st, /**< IA-32/AMD64 mov_st opcode. */
    /* PR 250397: store of immed is mov_st not mov_imm, even though can be immed->reg,
     * which we address by sharing part of the mov_st template chain */
    /*  57 */ OP_mov_imm,  /**< IA-32/AMD64 mov_imm opcode. */
    /*  58 */ OP_mov_seg,  /**< IA-32/AMD64 mov_seg opcode. */
    /*  59 */ OP_mov_priv, /**< IA-32/AMD64 mov_priv opcode. */

    /*  60 */ OP_test,  /**< IA-32/AMD64 test opcode. */
    /*  61 */ OP_lea,   /**< IA-32/AMD64 lea opcode. */
    /*  62 */ OP_xchg,  /**< IA-32/AMD64 xchg opcode. */
    /*  63 */ OP_cwde,  /**< IA-32/AMD64 cwde opcode. */
    /*  64 */ OP_cdq,   /**< IA-32/AMD64 cdq opcode. */
    /*  65 */ OP_fwait, /**< IA-32/AMD64 fwait opcode. */
    /*  66 */ OP_pushf, /**< IA-32/AMD64 pushf opcode. */
    /*  67 */ OP_popf,  /**< IA-32/AMD64 popf opcode. */
    /*  68 */ OP_sahf,  /**< IA-32/AMD64 sahf opcode. */
    /*  69 */ OP_lahf,  /**< IA-32/AMD64 lahf opcode. */

    /*  70 */ OP_ret,     /**< IA-32/AMD64 ret opcode. */
    /*  71 */ OP_ret_far, /**< IA-32/AMD64 ret_far opcode. */

    /*  72 */ OP_les,   /**< IA-32/AMD64 les opcode. */
    /*  73 */ OP_lds,   /**< IA-32/AMD64 lds opcode. */
    /*  74 */ OP_enter, /**< IA-32/AMD64 enter opcode. */
    /*  75 */ OP_leave, /**< IA-32/AMD64 leave opcode. */
    /*  76 */ OP_int3,  /**< IA-32/AMD64 int3 opcode. */
    /*  77 */ OP_int,   /**< IA-32/AMD64 int opcode. */
    /*  78 */ OP_into,  /**< IA-32/AMD64 into opcode. */
    /*  79 */ OP_iret,  /**< IA-32/AMD64 iret opcode. */
    /*  80 */ OP_aam,   /**< IA-32/AMD64 aam opcode. */
    /*  81 */ OP_aad,   /**< IA-32/AMD64 aad opcode. */
    /*  82 */ OP_xlat,  /**< IA-32/AMD64 xlat opcode. */
    /*  83 */ OP_in,    /**< IA-32/AMD64 in opcode. */
    /*  84 */ OP_out,   /**< IA-32/AMD64 out opcode. */
    /*  85 */ OP_hlt,   /**< IA-32/AMD64 hlt opcode. */
    /*  86 */ OP_cmc,   /**< IA-32/AMD64 cmc opcode. */
    /*  87 */ OP_clc,   /**< IA-32/AMD64 clc opcode. */
    /*  88 */ OP_stc,   /**< IA-32/AMD64 stc opcode. */
    /*  89 */ OP_cli,   /**< IA-32/AMD64 cli opcode. */
    /*  90 */ OP_sti,   /**< IA-32/AMD64 sti opcode. */
    /*  91 */ OP_cld,   /**< IA-32/AMD64 cld opcode. */
    /*  92 */ OP_std,   /**< IA-32/AMD64 std opcode. */

    /*  93 */ OP_lar,       /**< IA-32/AMD64 lar opcode. */
    /*  94 */ OP_lsl,       /**< IA-32/AMD64 lsl opcode. */
    /*  95 */ OP_syscall,   /**< IA-32/AMD64 syscall opcode. */
    /*  96 */ OP_clts,      /**< IA-32/AMD64 clts opcode. */
    /*  97 */ OP_sysret,    /**< IA-32/AMD64 sysret opcode. */
    /*  98 */ OP_invd,      /**< IA-32/AMD64 invd opcode. */
    /*  99 */ OP_wbinvd,    /**< IA-32/AMD64 wbinvd opcode. */
    /* 100 */ OP_ud2,       /**< IA-32/AMD64 ud2 opcode. */
    /* 101 */ OP_nop_modrm, /**< IA-32/AMD64 nop_modrm opcode. */
    /* 102 */ OP_movntps,   /**< IA-32/AMD64 movntps opcode. */
    /* 103 */ OP_movntpd,   /**< IA-32/AMD64 movntpd opcode. */
    /* 104 */ OP_wrmsr,     /**< IA-32/AMD64 wrmsr opcode. */
    /* 105 */ OP_rdtsc,     /**< IA-32/AMD64 rdtsc opcode. */
    /* 106 */ OP_rdmsr,     /**< IA-32/AMD64 rdmsr opcode. */
    /* 107 */ OP_rdpmc,     /**< IA-32/AMD64 rdpmc opcode. */
    /* 108 */ OP_sysenter,  /**< IA-32/AMD64 sysenter opcode. */
    /* 109 */ OP_sysexit,   /**< IA-32/AMD64 sysexit opcode. */

    /* 110 */ OP_cmovo,   /**< IA-32/AMD64 cmovo opcode. */
    /* 111 */ OP_cmovno,  /**< IA-32/AMD64 cmovno opcode. */
    /* 112 */ OP_cmovb,   /**< IA-32/AMD64 cmovb opcode. */
    /* 113 */ OP_cmovnb,  /**< IA-32/AMD64 cmovnb opcode. */
    /* 114 */ OP_cmovz,   /**< IA-32/AMD64 cmovz opcode. */
    /* 115 */ OP_cmovnz,  /**< IA-32/AMD64 cmovnz opcode. */
    /* 116 */ OP_cmovbe,  /**< IA-32/AMD64 cmovbe opcode. */
    /* 117 */ OP_cmovnbe, /**< IA-32/AMD64 cmovnbe opcode. */
    /* 118 */ OP_cmovs,   /**< IA-32/AMD64 cmovs opcode. */
    /* 119 */ OP_cmovns,  /**< IA-32/AMD64 cmovns opcode. */
    /* 120 */ OP_cmovp,   /**< IA-32/AMD64 cmovp opcode. */
    /* 121 */ OP_cmovnp,  /**< IA-32/AMD64 cmovnp opcode. */
    /* 122 */ OP_cmovl,   /**< IA-32/AMD64 cmovl opcode. */
    /* 123 */ OP_cmovnl,  /**< IA-32/AMD64 cmovnl opcode. */
    /* 124 */ OP_cmovle,  /**< IA-32/AMD64 cmovle opcode. */
    /* 125 */ OP_cmovnle, /**< IA-32/AMD64 cmovnle opcode. */

    /* 126 */ OP_punpcklbw,  /**< IA-32/AMD64 punpcklbw opcode. */
    /* 127 */ OP_punpcklwd,  /**< IA-32/AMD64 punpcklwd opcode. */
    /* 128 */ OP_punpckldq,  /**< IA-32/AMD64 punpckldq opcode. */
    /* 129 */ OP_packsswb,   /**< IA-32/AMD64 packsswb opcode. */
    /* 130 */ OP_pcmpgtb,    /**< IA-32/AMD64 pcmpgtb opcode. */
    /* 131 */ OP_pcmpgtw,    /**< IA-32/AMD64 pcmpgtw opcode. */
    /* 132 */ OP_pcmpgtd,    /**< IA-32/AMD64 pcmpgtd opcode. */
    /* 133 */ OP_packuswb,   /**< IA-32/AMD64 packuswb opcode. */
    /* 134 */ OP_punpckhbw,  /**< IA-32/AMD64 punpckhbw opcode. */
    /* 135 */ OP_punpckhwd,  /**< IA-32/AMD64 punpckhwd opcode. */
    /* 136 */ OP_punpckhdq,  /**< IA-32/AMD64 punpckhdq opcode. */
    /* 137 */ OP_packssdw,   /**< IA-32/AMD64 packssdw opcode. */
    /* 138 */ OP_punpcklqdq, /**< IA-32/AMD64 punpcklqdq opcode. */
    /* 139 */ OP_punpckhqdq, /**< IA-32/AMD64 punpckhqdq opcode. */
    /* 140 */ OP_movd,       /**< IA-32/AMD64 movd opcode. */
    /* 141 */ OP_movq,       /**< IA-32/AMD64 movq opcode. */
    /* 142 */ OP_movdqu,     /**< IA-32/AMD64 movdqu opcode. */
    /* 143 */ OP_movdqa,     /**< IA-32/AMD64 movdqa opcode. */
    /* 144 */ OP_pshufw,     /**< IA-32/AMD64 pshufw opcode. */
    /* 145 */ OP_pshufd,     /**< IA-32/AMD64 pshufd opcode. */
    /* 146 */ OP_pshufhw,    /**< IA-32/AMD64 pshufhw opcode. */
    /* 147 */ OP_pshuflw,    /**< IA-32/AMD64 pshuflw opcode. */
    /* 148 */ OP_pcmpeqb,    /**< IA-32/AMD64 pcmpeqb opcode. */
    /* 149 */ OP_pcmpeqw,    /**< IA-32/AMD64 pcmpeqw opcode. */
    /* 150 */ OP_pcmpeqd,    /**< IA-32/AMD64 pcmpeqd opcode. */
    /* 151 */ OP_emms,       /**< IA-32/AMD64 emms opcode. */

    /* 152 */ OP_jo,   /**< IA-32/AMD64 jo opcode. */
    /* 153 */ OP_jno,  /**< IA-32/AMD64 jno opcode. */
    /* 154 */ OP_jb,   /**< IA-32/AMD64 jb opcode. */
    /* 155 */ OP_jnb,  /**< IA-32/AMD64 jnb opcode. */
    /* 156 */ OP_jz,   /**< IA-32/AMD64 jz opcode. */
    /* 157 */ OP_jnz,  /**< IA-32/AMD64 jnz opcode. */
    /* 158 */ OP_jbe,  /**< IA-32/AMD64 jbe opcode. */
    /* 159 */ OP_jnbe, /**< IA-32/AMD64 jnbe opcode. */
    /* 160 */ OP_js,   /**< IA-32/AMD64 js opcode. */
    /* 161 */ OP_jns,  /**< IA-32/AMD64 jns opcode. */
    /* 162 */ OP_jp,   /**< IA-32/AMD64 jp opcode. */
    /* 163 */ OP_jnp,  /**< IA-32/AMD64 jnp opcode. */
    /* 164 */ OP_jl,   /**< IA-32/AMD64 jl opcode. */
    /* 165 */ OP_jnl,  /**< IA-32/AMD64 jnl opcode. */
    /* 166 */ OP_jle,  /**< IA-32/AMD64 jle opcode. */
    /* 167 */ OP_jnle, /**< IA-32/AMD64 jnle opcode. */

    /* 168 */ OP_seto,   /**< IA-32/AMD64 seto opcode. */
    /* 169 */ OP_setno,  /**< IA-32/AMD64 setno opcode. */
    /* 170 */ OP_setb,   /**< IA-32/AMD64 setb opcode. */
    /* 171 */ OP_setnb,  /**< IA-32/AMD64 setnb opcode. */
    /* 172 */ OP_setz,   /**< IA-32/AMD64 setz opcode. */
    /* 173 */ OP_setnz,  /**< IA-32/AMD64 setnz opcode. */
    /* 174 */ OP_setbe,  /**< IA-32/AMD64 setbe opcode. */
    /* 175 */ OP_setnbe, /**< IA-32/AMD64 setnbe opcode. */
    /* 176 */ OP_sets,   /**< IA-32/AMD64 sets opcode. */
    /* 177 */ OP_setns,  /**< IA-32/AMD64 setns opcode. */
    /* 178 */ OP_setp,   /**< IA-32/AMD64 setp opcode. */
    /* 179 */ OP_setnp,  /**< IA-32/AMD64 setnp opcode. */
    /* 180 */ OP_setl,   /**< IA-32/AMD64 setl opcode. */
    /* 181 */ OP_setnl,  /**< IA-32/AMD64 setnl opcode. */
    /* 182 */ OP_setle,  /**< IA-32/AMD64 setle opcode. */
    /* 183 */ OP_setnle, /**< IA-32/AMD64 setnle opcode. */

    /* 184 */ OP_cpuid,      /**< IA-32/AMD64 cpuid opcode. */
    /* 185 */ OP_bt,         /**< IA-32/AMD64 bt opcode. */
    /* 186 */ OP_shld,       /**< IA-32/AMD64 shld opcode. */
    /* 187 */ OP_rsm,        /**< IA-32/AMD64 rsm opcode. */
    /* 188 */ OP_bts,        /**< IA-32/AMD64 bts opcode. */
    /* 189 */ OP_shrd,       /**< IA-32/AMD64 shrd opcode. */
    /* 190 */ OP_cmpxchg,    /**< IA-32/AMD64 cmpxchg opcode. */
    /* 191 */ OP_lss,        /**< IA-32/AMD64 lss opcode. */
    /* 192 */ OP_btr,        /**< IA-32/AMD64 btr opcode. */
    /* 193 */ OP_lfs,        /**< IA-32/AMD64 lfs opcode. */
    /* 194 */ OP_lgs,        /**< IA-32/AMD64 lgs opcode. */
    /* 195 */ OP_movzx,      /**< IA-32/AMD64 movzx opcode. */
    /* 196 */ OP_ud1,        /**< IA-32/AMD64 ud1 opcode. */
    /* 197 */ OP_btc,        /**< IA-32/AMD64 btc opcode. */
    /* 198 */ OP_bsf,        /**< IA-32/AMD64 bsf opcode. */
    /* 199 */ OP_bsr,        /**< IA-32/AMD64 bsr opcode. */
    /* 200 */ OP_movsx,      /**< IA-32/AMD64 movsx opcode. */
    /* 201 */ OP_xadd,       /**< IA-32/AMD64 xadd opcode. */
    /* 202 */ OP_movnti,     /**< IA-32/AMD64 movnti opcode. */
    /* 203 */ OP_pinsrw,     /**< IA-32/AMD64 pinsrw opcode. */
    /* 204 */ OP_pextrw,     /**< IA-32/AMD64 pextrw opcode. */
    /* 205 */ OP_bswap,      /**< IA-32/AMD64 bswap opcode. */
    /* 206 */ OP_psrlw,      /**< IA-32/AMD64 psrlw opcode. */
    /* 207 */ OP_psrld,      /**< IA-32/AMD64 psrld opcode. */
    /* 208 */ OP_psrlq,      /**< IA-32/AMD64 psrlq opcode. */
    /* 209 */ OP_paddq,      /**< IA-32/AMD64 paddq opcode. */
    /* 210 */ OP_pmullw,     /**< IA-32/AMD64 pmullw opcode. */
    /* 211 */ OP_pmovmskb,   /**< IA-32/AMD64 pmovmskb opcode. */
    /* 212 */ OP_psubusb,    /**< IA-32/AMD64 psubusb opcode. */
    /* 213 */ OP_psubusw,    /**< IA-32/AMD64 psubusw opcode. */
    /* 214 */ OP_pminub,     /**< IA-32/AMD64 pminub opcode. */
    /* 215 */ OP_pand,       /**< IA-32/AMD64 pand opcode. */
    /* 216 */ OP_paddusb,    /**< IA-32/AMD64 paddusb opcode. */
    /* 217 */ OP_paddusw,    /**< IA-32/AMD64 paddusw opcode. */
    /* 218 */ OP_pmaxub,     /**< IA-32/AMD64 pmaxub opcode. */
    /* 219 */ OP_pandn,      /**< IA-32/AMD64 pandn opcode. */
    /* 220 */ OP_pavgb,      /**< IA-32/AMD64 pavgb opcode. */
    /* 221 */ OP_psraw,      /**< IA-32/AMD64 psraw opcode. */
    /* 222 */ OP_psrad,      /**< IA-32/AMD64 psrad opcode. */
    /* 223 */ OP_pavgw,      /**< IA-32/AMD64 pavgw opcode. */
    /* 224 */ OP_pmulhuw,    /**< IA-32/AMD64 pmulhuw opcode. */
    /* 225 */ OP_pmulhw,     /**< IA-32/AMD64 pmulhw opcode. */
    /* 226 */ OP_movntq,     /**< IA-32/AMD64 movntq opcode. */
    /* 227 */ OP_movntdq,    /**< IA-32/AMD64 movntdq opcode. */
    /* 228 */ OP_psubsb,     /**< IA-32/AMD64 psubsb opcode. */
    /* 229 */ OP_psubsw,     /**< IA-32/AMD64 psubsw opcode. */
    /* 230 */ OP_pminsw,     /**< IA-32/AMD64 pminsw opcode. */
    /* 231 */ OP_por,        /**< IA-32/AMD64 por opcode. */
    /* 232 */ OP_paddsb,     /**< IA-32/AMD64 paddsb opcode. */
    /* 233 */ OP_paddsw,     /**< IA-32/AMD64 paddsw opcode. */
    /* 234 */ OP_pmaxsw,     /**< IA-32/AMD64 pmaxsw opcode. */
    /* 235 */ OP_pxor,       /**< IA-32/AMD64 pxor opcode. */
    /* 236 */ OP_psllw,      /**< IA-32/AMD64 psllw opcode. */
    /* 237 */ OP_pslld,      /**< IA-32/AMD64 pslld opcode. */
    /* 238 */ OP_psllq,      /**< IA-32/AMD64 psllq opcode. */
    /* 239 */ OP_pmuludq,    /**< IA-32/AMD64 pmuludq opcode. */
    /* 240 */ OP_pmaddwd,    /**< IA-32/AMD64 pmaddwd opcode. */
    /* 241 */ OP_psadbw,     /**< IA-32/AMD64 psadbw opcode. */
    /* 242 */ OP_maskmovq,   /**< IA-32/AMD64 maskmovq opcode. */
    /* 243 */ OP_maskmovdqu, /**< IA-32/AMD64 maskmovdqu opcode. */
    /* 244 */ OP_psubb,      /**< IA-32/AMD64 psubb opcode. */
    /* 245 */ OP_psubw,      /**< IA-32/AMD64 psubw opcode. */
    /* 246 */ OP_psubd,      /**< IA-32/AMD64 psubd opcode. */
    /* 247 */ OP_psubq,      /**< IA-32/AMD64 psubq opcode. */
    /* 248 */ OP_paddb,      /**< IA-32/AMD64 paddb opcode. */
    /* 249 */ OP_paddw,      /**< IA-32/AMD64 paddw opcode. */
    /* 250 */ OP_paddd,      /**< IA-32/AMD64 paddd opcode. */
    /* 251 */ OP_psrldq,     /**< IA-32/AMD64 psrldq opcode. */
    /* 252 */ OP_pslldq,     /**< IA-32/AMD64 pslldq opcode. */

    /* 253 */ OP_rol,         /**< IA-32/AMD64 rol opcode. */
    /* 254 */ OP_ror,         /**< IA-32/AMD64 ror opcode. */
    /* 255 */ OP_rcl,         /**< IA-32/AMD64 rcl opcode. */
    /* 256 */ OP_rcr,         /**< IA-32/AMD64 rcr opcode. */
    /* 257 */ OP_shl,         /**< IA-32/AMD64 shl opcode. */
    /* 258 */ OP_shr,         /**< IA-32/AMD64 shr opcode. */
    /* 259 */ OP_sar,         /**< IA-32/AMD64 sar opcode. */
    /* 260 */ OP_not,         /**< IA-32/AMD64 not opcode. */
    /* 261 */ OP_neg,         /**< IA-32/AMD64 neg opcode. */
    /* 262 */ OP_mul,         /**< IA-32/AMD64 mul opcode. */
    /* 263 */ OP_div,         /**< IA-32/AMD64 div opcode. */
    /* 264 */ OP_idiv,        /**< IA-32/AMD64 idiv opcode. */
    /* 265 */ OP_sldt,        /**< IA-32/AMD64 sldt opcode. */
    /* 266 */ OP_str,         /**< IA-32/AMD64 str opcode. */
    /* 267 */ OP_lldt,        /**< IA-32/AMD64 lldt opcode. */
    /* 268 */ OP_ltr,         /**< IA-32/AMD64 ltr opcode. */
    /* 269 */ OP_verr,        /**< IA-32/AMD64 verr opcode. */
    /* 270 */ OP_verw,        /**< IA-32/AMD64 verw opcode. */
    /* 271 */ OP_sgdt,        /**< IA-32/AMD64 sgdt opcode. */
    /* 272 */ OP_sidt,        /**< IA-32/AMD64 sidt opcode. */
    /* 273 */ OP_lgdt,        /**< IA-32/AMD64 lgdt opcode. */
    /* 274 */ OP_lidt,        /**< IA-32/AMD64 lidt opcode. */
    /* 275 */ OP_smsw,        /**< IA-32/AMD64 smsw opcode. */
    /* 276 */ OP_lmsw,        /**< IA-32/AMD64 lmsw opcode. */
    /* 277 */ OP_invlpg,      /**< IA-32/AMD64 invlpg opcode. */
    /* 278 */ OP_cmpxchg8b,   /**< IA-32/AMD64 cmpxchg8b opcode. */
    /* 279 */ OP_fxsave32,    /**< IA-32/AMD64 fxsave opcode. */
    /* 280 */ OP_fxrstor32,   /**< IA-32/AMD64 fxrstor opcode. */
    /* 281 */ OP_ldmxcsr,     /**< IA-32/AMD64 ldmxcsr opcode. */
    /* 282 */ OP_stmxcsr,     /**< IA-32/AMD64 stmxcsr opcode. */
    /* 283 */ OP_lfence,      /**< IA-32/AMD64 lfence opcode. */
    /* 284 */ OP_mfence,      /**< IA-32/AMD64 mfence opcode. */
    /* 285 */ OP_clflush,     /**< IA-32/AMD64 clflush opcode. */
    /* 286 */ OP_sfence,      /**< IA-32/AMD64 sfence opcode. */
    /* 287 */ OP_prefetchnta, /**< IA-32/AMD64 prefetchnta opcode. */
    /* 288 */ OP_prefetcht0,  /**< IA-32/AMD64 prefetcht0 opcode. */
    /* 289 */ OP_prefetcht1,  /**< IA-32/AMD64 prefetcht1 opcode. */
    /* 290 */ OP_prefetcht2,  /**< IA-32/AMD64 prefetcht2 opcode. */
    /* 291 */ OP_prefetch,    /**< IA-32/AMD64 prefetch opcode. */
    /* 292 */ OP_prefetchw,   /**< IA-32/AMD64 prefetchw opcode. */

    /* 293 */ OP_movups,    /**< IA-32/AMD64 movups opcode. */
    /* 294 */ OP_movss,     /**< IA-32/AMD64 movss opcode. */
    /* 295 */ OP_movupd,    /**< IA-32/AMD64 movupd opcode. */
    /* 296 */ OP_movsd,     /**< IA-32/AMD64 movsd opcode. */
    /* 297 */ OP_movlps,    /**< IA-32/AMD64 movlps opcode. */
    /* 298 */ OP_movlpd,    /**< IA-32/AMD64 movlpd opcode. */
    /* 299 */ OP_unpcklps,  /**< IA-32/AMD64 unpcklps opcode. */
    /* 300 */ OP_unpcklpd,  /**< IA-32/AMD64 unpcklpd opcode. */
    /* 301 */ OP_unpckhps,  /**< IA-32/AMD64 unpckhps opcode. */
    /* 302 */ OP_unpckhpd,  /**< IA-32/AMD64 unpckhpd opcode. */
    /* 303 */ OP_movhps,    /**< IA-32/AMD64 movhps opcode. */
    /* 304 */ OP_movhpd,    /**< IA-32/AMD64 movhpd opcode. */
    /* 305 */ OP_movaps,    /**< IA-32/AMD64 movaps opcode. */
    /* 306 */ OP_movapd,    /**< IA-32/AMD64 movapd opcode. */
    /* 307 */ OP_cvtpi2ps,  /**< IA-32/AMD64 cvtpi2ps opcode. */
    /* 308 */ OP_cvtsi2ss,  /**< IA-32/AMD64 cvtsi2ss opcode. */
    /* 309 */ OP_cvtpi2pd,  /**< IA-32/AMD64 cvtpi2pd opcode. */
    /* 310 */ OP_cvtsi2sd,  /**< IA-32/AMD64 cvtsi2sd opcode. */
    /* 311 */ OP_cvttps2pi, /**< IA-32/AMD64 cvttps2pi opcode. */
    /* 312 */ OP_cvttss2si, /**< IA-32/AMD64 cvttss2si opcode. */
    /* 313 */ OP_cvttpd2pi, /**< IA-32/AMD64 cvttpd2pi opcode. */
    /* 314 */ OP_cvttsd2si, /**< IA-32/AMD64 cvttsd2si opcode. */
    /* 315 */ OP_cvtps2pi,  /**< IA-32/AMD64 cvtps2pi opcode. */
    /* 316 */ OP_cvtss2si,  /**< IA-32/AMD64 cvtss2si opcode. */
    /* 317 */ OP_cvtpd2pi,  /**< IA-32/AMD64 cvtpd2pi opcode. */
    /* 318 */ OP_cvtsd2si,  /**< IA-32/AMD64 cvtsd2si opcode. */
    /* 319 */ OP_ucomiss,   /**< IA-32/AMD64 ucomiss opcode. */
    /* 320 */ OP_ucomisd,   /**< IA-32/AMD64 ucomisd opcode. */
    /* 321 */ OP_comiss,    /**< IA-32/AMD64 comiss opcode. */
    /* 322 */ OP_comisd,    /**< IA-32/AMD64 comisd opcode. */
    /* 323 */ OP_movmskps,  /**< IA-32/AMD64 movmskps opcode. */
    /* 324 */ OP_movmskpd,  /**< IA-32/AMD64 movmskpd opcode. */
    /* 325 */ OP_sqrtps,    /**< IA-32/AMD64 sqrtps opcode. */
    /* 326 */ OP_sqrtss,    /**< IA-32/AMD64 sqrtss opcode. */
    /* 327 */ OP_sqrtpd,    /**< IA-32/AMD64 sqrtpd opcode. */
    /* 328 */ OP_sqrtsd,    /**< IA-32/AMD64 sqrtsd opcode. */
    /* 329 */ OP_rsqrtps,   /**< IA-32/AMD64 rsqrtps opcode. */
    /* 330 */ OP_rsqrtss,   /**< IA-32/AMD64 rsqrtss opcode. */
    /* 331 */ OP_rcpps,     /**< IA-32/AMD64 rcpps opcode. */
    /* 332 */ OP_rcpss,     /**< IA-32/AMD64 rcpss opcode. */
    /* 333 */ OP_andps,     /**< IA-32/AMD64 andps opcode. */
    /* 334 */ OP_andpd,     /**< IA-32/AMD64 andpd opcode. */
    /* 335 */ OP_andnps,    /**< IA-32/AMD64 andnps opcode. */
    /* 336 */ OP_andnpd,    /**< IA-32/AMD64 andnpd opcode. */
    /* 337 */ OP_orps,      /**< IA-32/AMD64 orps opcode. */
    /* 338 */ OP_orpd,      /**< IA-32/AMD64 orpd opcode. */
    /* 339 */ OP_xorps,     /**< IA-32/AMD64 xorps opcode. */
    /* 340 */ OP_xorpd,     /**< IA-32/AMD64 xorpd opcode. */
    /* 341 */ OP_addps,     /**< IA-32/AMD64 addps opcode. */
    /* 342 */ OP_addss,     /**< IA-32/AMD64 addss opcode. */
    /* 343 */ OP_addpd,     /**< IA-32/AMD64 addpd opcode. */
    /* 344 */ OP_addsd,     /**< IA-32/AMD64 addsd opcode. */
    /* 345 */ OP_mulps,     /**< IA-32/AMD64 mulps opcode. */
    /* 346 */ OP_mulss,     /**< IA-32/AMD64 mulss opcode. */
    /* 347 */ OP_mulpd,     /**< IA-32/AMD64 mulpd opcode. */
    /* 348 */ OP_mulsd,     /**< IA-32/AMD64 mulsd opcode. */
    /* 349 */ OP_cvtps2pd,  /**< IA-32/AMD64 cvtps2pd opcode. */
    /* 350 */ OP_cvtss2sd,  /**< IA-32/AMD64 cvtss2sd opcode. */
    /* 351 */ OP_cvtpd2ps,  /**< IA-32/AMD64 cvtpd2ps opcode. */
    /* 352 */ OP_cvtsd2ss,  /**< IA-32/AMD64 cvtsd2ss opcode. */
    /* 353 */ OP_cvtdq2ps,  /**< IA-32/AMD64 cvtdq2ps opcode. */
    /* 354 */ OP_cvttps2dq, /**< IA-32/AMD64 cvttps2dq opcode. */
    /* 355 */ OP_cvtps2dq,  /**< IA-32/AMD64 cvtps2dq opcode. */
    /* 356 */ OP_subps,     /**< IA-32/AMD64 subps opcode. */
    /* 357 */ OP_subss,     /**< IA-32/AMD64 subss opcode. */
    /* 358 */ OP_subpd,     /**< IA-32/AMD64 subpd opcode. */
    /* 359 */ OP_subsd,     /**< IA-32/AMD64 subsd opcode. */
    /* 360 */ OP_minps,     /**< IA-32/AMD64 minps opcode. */
    /* 361 */ OP_minss,     /**< IA-32/AMD64 minss opcode. */
    /* 362 */ OP_minpd,     /**< IA-32/AMD64 minpd opcode. */
    /* 363 */ OP_minsd,     /**< IA-32/AMD64 minsd opcode. */
    /* 364 */ OP_divps,     /**< IA-32/AMD64 divps opcode. */
    /* 365 */ OP_divss,     /**< IA-32/AMD64 divss opcode. */
    /* 366 */ OP_divpd,     /**< IA-32/AMD64 divpd opcode. */
    /* 367 */ OP_divsd,     /**< IA-32/AMD64 divsd opcode. */
    /* 368 */ OP_maxps,     /**< IA-32/AMD64 maxps opcode. */
    /* 369 */ OP_maxss,     /**< IA-32/AMD64 maxss opcode. */
    /* 370 */ OP_maxpd,     /**< IA-32/AMD64 maxpd opcode. */
    /* 371 */ OP_maxsd,     /**< IA-32/AMD64 maxsd opcode. */
    /* 372 */ OP_cmpps,     /**< IA-32/AMD64 cmpps opcode. */
    /* 373 */ OP_cmpss,     /**< IA-32/AMD64 cmpss opcode. */
    /* 374 */ OP_cmppd,     /**< IA-32/AMD64 cmppd opcode. */
    /* 375 */ OP_cmpsd,     /**< IA-32/AMD64 cmpsd opcode. */
    /* 376 */ OP_shufps,    /**< IA-32/AMD64 shufps opcode. */
    /* 377 */ OP_shufpd,    /**< IA-32/AMD64 shufpd opcode. */
    /* 378 */ OP_cvtdq2pd,  /**< IA-32/AMD64 cvtdq2pd opcode. */
    /* 379 */ OP_cvttpd2dq, /**< IA-32/AMD64 cvttpd2dq opcode. */
    /* 380 */ OP_cvtpd2dq,  /**< IA-32/AMD64 cvtpd2dq opcode. */
    /* 381 */ OP_nop,       /**< IA-32/AMD64 nop opcode. */
    /* 382 */ OP_pause,     /**< IA-32/AMD64 pause opcode. */

    /* 383 */ OP_ins,        /**< IA-32/AMD64 ins opcode. */
    /* 384 */ OP_rep_ins,    /**< IA-32/AMD64 rep_ins opcode. */
    /* 385 */ OP_outs,       /**< IA-32/AMD64 outs opcode. */
    /* 386 */ OP_rep_outs,   /**< IA-32/AMD64 rep_outs opcode. */
    /* 387 */ OP_movs,       /**< IA-32/AMD64 movs opcode. */
    /* 388 */ OP_rep_movs,   /**< IA-32/AMD64 rep_movs opcode. */
    /* 389 */ OP_stos,       /**< IA-32/AMD64 stos opcode. */
    /* 390 */ OP_rep_stos,   /**< IA-32/AMD64 rep_stos opcode. */
    /* 391 */ OP_lods,       /**< IA-32/AMD64 lods opcode. */
    /* 392 */ OP_rep_lods,   /**< IA-32/AMD64 rep_lods opcode. */
    /* 393 */ OP_cmps,       /**< IA-32/AMD64 cmps opcode. */
    /* 394 */ OP_rep_cmps,   /**< IA-32/AMD64 rep_cmps opcode. */
    /* 395 */ OP_repne_cmps, /**< IA-32/AMD64 repne_cmps opcode. */
    /* 396 */ OP_scas,       /**< IA-32/AMD64 scas opcode. */
    /* 397 */ OP_rep_scas,   /**< IA-32/AMD64 rep_scas opcode. */
    /* 398 */ OP_repne_scas, /**< IA-32/AMD64 repne_scas opcode. */

    /* 399 */ OP_fadd,    /**< IA-32/AMD64 fadd opcode. */
    /* 400 */ OP_fmul,    /**< IA-32/AMD64 fmul opcode. */
    /* 401 */ OP_fcom,    /**< IA-32/AMD64 fcom opcode. */
    /* 402 */ OP_fcomp,   /**< IA-32/AMD64 fcomp opcode. */
    /* 403 */ OP_fsub,    /**< IA-32/AMD64 fsub opcode. */
    /* 404 */ OP_fsubr,   /**< IA-32/AMD64 fsubr opcode. */
    /* 405 */ OP_fdiv,    /**< IA-32/AMD64 fdiv opcode. */
    /* 406 */ OP_fdivr,   /**< IA-32/AMD64 fdivr opcode. */
    /* 407 */ OP_fld,     /**< IA-32/AMD64 fld opcode. */
    /* 408 */ OP_fst,     /**< IA-32/AMD64 fst opcode. */
    /* 409 */ OP_fstp,    /**< IA-32/AMD64 fstp opcode. */
    /* 410 */ OP_fldenv,  /**< IA-32/AMD64 fldenv opcode. */
    /* 411 */ OP_fldcw,   /**< IA-32/AMD64 fldcw opcode. */
    /* 412 */ OP_fnstenv, /**< IA-32/AMD64 fnstenv opcode. */
    /* 413 */ OP_fnstcw,  /**< IA-32/AMD64 fnstcw opcode. */
    /* 414 */ OP_fiadd,   /**< IA-32/AMD64 fiadd opcode. */
    /* 415 */ OP_fimul,   /**< IA-32/AMD64 fimul opcode. */
    /* 416 */ OP_ficom,   /**< IA-32/AMD64 ficom opcode. */
    /* 417 */ OP_ficomp,  /**< IA-32/AMD64 ficomp opcode. */
    /* 418 */ OP_fisub,   /**< IA-32/AMD64 fisub opcode. */
    /* 419 */ OP_fisubr,  /**< IA-32/AMD64 fisubr opcode. */
    /* 420 */ OP_fidiv,   /**< IA-32/AMD64 fidiv opcode. */
    /* 421 */ OP_fidivr,  /**< IA-32/AMD64 fidivr opcode. */
    /* 422 */ OP_fild,    /**< IA-32/AMD64 fild opcode. */
    /* 423 */ OP_fist,    /**< IA-32/AMD64 fist opcode. */
    /* 424 */ OP_fistp,   /**< IA-32/AMD64 fistp opcode. */
    /* 425 */ OP_frstor,  /**< IA-32/AMD64 frstor opcode. */
    /* 426 */ OP_fnsave,  /**< IA-32/AMD64 fnsave opcode. */
    /* 427 */ OP_fnstsw,  /**< IA-32/AMD64 fnstsw opcode. */

    /* 428 */ OP_fbld,  /**< IA-32/AMD64 fbld opcode. */
    /* 429 */ OP_fbstp, /**< IA-32/AMD64 fbstp opcode. */

    /* 430 */ OP_fxch,     /**< IA-32/AMD64 fxch opcode. */
    /* 431 */ OP_fnop,     /**< IA-32/AMD64 fnop opcode. */
    /* 432 */ OP_fchs,     /**< IA-32/AMD64 fchs opcode. */
    /* 433 */ OP_fabs,     /**< IA-32/AMD64 fabs opcode. */
    /* 434 */ OP_ftst,     /**< IA-32/AMD64 ftst opcode. */
    /* 435 */ OP_fxam,     /**< IA-32/AMD64 fxam opcode. */
    /* 436 */ OP_fld1,     /**< IA-32/AMD64 fld1 opcode. */
    /* 437 */ OP_fldl2t,   /**< IA-32/AMD64 fldl2t opcode. */
    /* 438 */ OP_fldl2e,   /**< IA-32/AMD64 fldl2e opcode. */
    /* 439 */ OP_fldpi,    /**< IA-32/AMD64 fldpi opcode. */
    /* 440 */ OP_fldlg2,   /**< IA-32/AMD64 fldlg2 opcode. */
    /* 441 */ OP_fldln2,   /**< IA-32/AMD64 fldln2 opcode. */
    /* 442 */ OP_fldz,     /**< IA-32/AMD64 fldz opcode. */
    /* 443 */ OP_f2xm1,    /**< IA-32/AMD64 f2xm1 opcode. */
    /* 444 */ OP_fyl2x,    /**< IA-32/AMD64 fyl2x opcode. */
    /* 445 */ OP_fptan,    /**< IA-32/AMD64 fptan opcode. */
    /* 446 */ OP_fpatan,   /**< IA-32/AMD64 fpatan opcode. */
    /* 447 */ OP_fxtract,  /**< IA-32/AMD64 fxtract opcode. */
    /* 448 */ OP_fprem1,   /**< IA-32/AMD64 fprem1 opcode. */
    /* 449 */ OP_fdecstp,  /**< IA-32/AMD64 fdecstp opcode. */
    /* 450 */ OP_fincstp,  /**< IA-32/AMD64 fincstp opcode. */
    /* 451 */ OP_fprem,    /**< IA-32/AMD64 fprem opcode. */
    /* 452 */ OP_fyl2xp1,  /**< IA-32/AMD64 fyl2xp1 opcode. */
    /* 453 */ OP_fsqrt,    /**< IA-32/AMD64 fsqrt opcode. */
    /* 454 */ OP_fsincos,  /**< IA-32/AMD64 fsincos opcode. */
    /* 455 */ OP_frndint,  /**< IA-32/AMD64 frndint opcode. */
    /* 456 */ OP_fscale,   /**< IA-32/AMD64 fscale opcode. */
    /* 457 */ OP_fsin,     /**< IA-32/AMD64 fsin opcode. */
    /* 458 */ OP_fcos,     /**< IA-32/AMD64 fcos opcode. */
    /* 459 */ OP_fcmovb,   /**< IA-32/AMD64 fcmovb opcode. */
    /* 460 */ OP_fcmove,   /**< IA-32/AMD64 fcmove opcode. */
    /* 461 */ OP_fcmovbe,  /**< IA-32/AMD64 fcmovbe opcode. */
    /* 462 */ OP_fcmovu,   /**< IA-32/AMD64 fcmovu opcode. */
    /* 463 */ OP_fucompp,  /**< IA-32/AMD64 fucompp opcode. */
    /* 464 */ OP_fcmovnb,  /**< IA-32/AMD64 fcmovnb opcode. */
    /* 465 */ OP_fcmovne,  /**< IA-32/AMD64 fcmovne opcode. */
    /* 466 */ OP_fcmovnbe, /**< IA-32/AMD64 fcmovnbe opcode. */
    /* 467 */ OP_fcmovnu,  /**< IA-32/AMD64 fcmovnu opcode. */
    /* 468 */ OP_fnclex,   /**< IA-32/AMD64 fnclex opcode. */
    /* 469 */ OP_fninit,   /**< IA-32/AMD64 fninit opcode. */
    /* 470 */ OP_fucomi,   /**< IA-32/AMD64 fucomi opcode. */
    /* 471 */ OP_fcomi,    /**< IA-32/AMD64 fcomi opcode. */
    /* 472 */ OP_ffree,    /**< IA-32/AMD64 ffree opcode. */
    /* 473 */ OP_fucom,    /**< IA-32/AMD64 fucom opcode. */
    /* 474 */ OP_fucomp,   /**< IA-32/AMD64 fucomp opcode. */
    /* 475 */ OP_faddp,    /**< IA-32/AMD64 faddp opcode. */
    /* 476 */ OP_fmulp,    /**< IA-32/AMD64 fmulp opcode. */
    /* 477 */ OP_fcompp,   /**< IA-32/AMD64 fcompp opcode. */
    /* 478 */ OP_fsubrp,   /**< IA-32/AMD64 fsubrp opcode. */
    /* 479 */ OP_fsubp,    /**< IA-32/AMD64 fsubp opcode. */
    /* 480 */ OP_fdivrp,   /**< IA-32/AMD64 fdivrp opcode. */
    /* 481 */ OP_fdivp,    /**< IA-32/AMD64 fdivp opcode. */
    /* 482 */ OP_fucomip,  /**< IA-32/AMD64 fucomip opcode. */
    /* 483 */ OP_fcomip,   /**< IA-32/AMD64 fcomip opcode. */

    /* SSE3 instructions */
    /* 484 */ OP_fisttp,   /**< IA-32/AMD64 fisttp opcode. */
    /* 485 */ OP_haddpd,   /**< IA-32/AMD64 haddpd opcode. */
    /* 486 */ OP_haddps,   /**< IA-32/AMD64 haddps opcode. */
    /* 487 */ OP_hsubpd,   /**< IA-32/AMD64 hsubpd opcode. */
    /* 488 */ OP_hsubps,   /**< IA-32/AMD64 hsubps opcode. */
    /* 489 */ OP_addsubpd, /**< IA-32/AMD64 addsubpd opcode. */
    /* 490 */ OP_addsubps, /**< IA-32/AMD64 addsubps opcode. */
    /* 491 */ OP_lddqu,    /**< IA-32/AMD64 lddqu opcode. */
    /* 492 */ OP_monitor,  /**< IA-32/AMD64 monitor opcode. */
    /* 493 */ OP_mwait,    /**< IA-32/AMD64 mwait opcode. */
    /* 494 */ OP_movsldup, /**< IA-32/AMD64 movsldup opcode. */
    /* 495 */ OP_movshdup, /**< IA-32/AMD64 movshdup opcode. */
    /* 496 */ OP_movddup,  /**< IA-32/AMD64 movddup opcode. */

    /* 3D-Now! instructions */
    /* 497 */ OP_femms,         /**< IA-32/AMD64 femms opcode. */
    /* 498 */ OP_unknown_3dnow, /**< IA-32/AMD64 unknown_3dnow opcode. */
    /* 499 */ OP_pavgusb,       /**< IA-32/AMD64 pavgusb opcode. */
    /* 500 */ OP_pfadd,         /**< IA-32/AMD64 pfadd opcode. */
    /* 501 */ OP_pfacc,         /**< IA-32/AMD64 pfacc opcode. */
    /* 502 */ OP_pfcmpge,       /**< IA-32/AMD64 pfcmpge opcode. */
    /* 503 */ OP_pfcmpgt,       /**< IA-32/AMD64 pfcmpgt opcode. */
    /* 504 */ OP_pfcmpeq,       /**< IA-32/AMD64 pfcmpeq opcode. */
    /* 505 */ OP_pfmin,         /**< IA-32/AMD64 pfmin opcode. */
    /* 506 */ OP_pfmax,         /**< IA-32/AMD64 pfmax opcode. */
    /* 507 */ OP_pfmul,         /**< IA-32/AMD64 pfmul opcode. */
    /* 508 */ OP_pfrcp,         /**< IA-32/AMD64 pfrcp opcode. */
    /* 509 */ OP_pfrcpit1,      /**< IA-32/AMD64 pfrcpit1 opcode. */
    /* 510 */ OP_pfrcpit2,      /**< IA-32/AMD64 pfrcpit2 opcode. */
    /* 511 */ OP_pfrsqrt,       /**< IA-32/AMD64 pfrsqrt opcode. */
    /* 512 */ OP_pfrsqit1,      /**< IA-32/AMD64 pfrsqit1 opcode. */
    /* 513 */ OP_pmulhrw,       /**< IA-32/AMD64 pmulhrw opcode. */
    /* 514 */ OP_pfsub,         /**< IA-32/AMD64 pfsub opcode. */
    /* 515 */ OP_pfsubr,        /**< IA-32/AMD64 pfsubr opcode. */
    /* 516 */ OP_pi2fd,         /**< IA-32/AMD64 pi2fd opcode. */
    /* 517 */ OP_pf2id,         /**< IA-32/AMD64 pf2id opcode. */
    /* 518 */ OP_pi2fw,         /**< IA-32/AMD64 pi2fw opcode. */
    /* 519 */ OP_pf2iw,         /**< IA-32/AMD64 pf2iw opcode. */
    /* 520 */ OP_pfnacc,        /**< IA-32/AMD64 pfnacc opcode. */
    /* 521 */ OP_pfpnacc,       /**< IA-32/AMD64 pfpnacc opcode. */
    /* 522 */ OP_pswapd,        /**< IA-32/AMD64 pswapd opcode. */

    /* SSSE3 */
    /* 523 */ OP_pshufb,    /**< IA-32/AMD64 pshufb opcode. */
    /* 524 */ OP_phaddw,    /**< IA-32/AMD64 phaddw opcode. */
    /* 525 */ OP_phaddd,    /**< IA-32/AMD64 phaddd opcode. */
    /* 526 */ OP_phaddsw,   /**< IA-32/AMD64 phaddsw opcode. */
    /* 527 */ OP_pmaddubsw, /**< IA-32/AMD64 pmaddubsw opcode. */
    /* 528 */ OP_phsubw,    /**< IA-32/AMD64 phsubw opcode. */
    /* 529 */ OP_phsubd,    /**< IA-32/AMD64 phsubd opcode. */
    /* 530 */ OP_phsubsw,   /**< IA-32/AMD64 phsubsw opcode. */
    /* 531 */ OP_psignb,    /**< IA-32/AMD64 psignb opcode. */
    /* 532 */ OP_psignw,    /**< IA-32/AMD64 psignw opcode. */
    /* 533 */ OP_psignd,    /**< IA-32/AMD64 psignd opcode. */
    /* 534 */ OP_pmulhrsw,  /**< IA-32/AMD64 pmulhrsw opcode. */
    /* 535 */ OP_pabsb,     /**< IA-32/AMD64 pabsb opcode. */
    /* 536 */ OP_pabsw,     /**< IA-32/AMD64 pabsw opcode. */
    /* 537 */ OP_pabsd,     /**< IA-32/AMD64 pabsd opcode. */
    /* 538 */ OP_palignr,   /**< IA-32/AMD64 palignr opcode. */

    /* SSE4 (incl AMD (SSE4A) and Intel-specific (SSE4.1, SSE4.2) extensions */
    /* 539 */ OP_popcnt,     /**< IA-32/AMD64 popcnt opcode. */
    /* 540 */ OP_movntss,    /**< IA-32/AMD64 movntss opcode. */
    /* 541 */ OP_movntsd,    /**< IA-32/AMD64 movntsd opcode. */
    /* 542 */ OP_extrq,      /**< IA-32/AMD64 extrq opcode. */
    /* 543 */ OP_insertq,    /**< IA-32/AMD64 insertq opcode. */
    /* 544 */ OP_lzcnt,      /**< IA-32/AMD64 lzcnt opcode. */
    /* 545 */ OP_pblendvb,   /**< IA-32/AMD64 pblendvb opcode. */
    /* 546 */ OP_blendvps,   /**< IA-32/AMD64 blendvps opcode. */
    /* 547 */ OP_blendvpd,   /**< IA-32/AMD64 blendvpd opcode. */
    /* 548 */ OP_ptest,      /**< IA-32/AMD64 ptest opcode. */
    /* 549 */ OP_pmovsxbw,   /**< IA-32/AMD64 pmovsxbw opcode. */
    /* 550 */ OP_pmovsxbd,   /**< IA-32/AMD64 pmovsxbd opcode. */
    /* 551 */ OP_pmovsxbq,   /**< IA-32/AMD64 pmovsxbq opcode. */
    /* 552 */ OP_pmovsxwd,   /**< IA-32/AMD64 pmovsxwd opcode. */
    /* 553 */ OP_pmovsxwq,   /**< IA-32/AMD64 pmovsxwq opcode. */
    /* 554 */ OP_pmovsxdq,   /**< IA-32/AMD64 pmovsxdq opcode. */
    /* 555 */ OP_pmuldq,     /**< IA-32/AMD64 pmuldq opcode. */
    /* 556 */ OP_pcmpeqq,    /**< IA-32/AMD64 pcmpeqq opcode. */
    /* 557 */ OP_movntdqa,   /**< IA-32/AMD64 movntdqa opcode. */
    /* 558 */ OP_packusdw,   /**< IA-32/AMD64 packusdw opcode. */
    /* 559 */ OP_pmovzxbw,   /**< IA-32/AMD64 pmovzxbw opcode. */
    /* 560 */ OP_pmovzxbd,   /**< IA-32/AMD64 pmovzxbd opcode. */
    /* 561 */ OP_pmovzxbq,   /**< IA-32/AMD64 pmovzxbq opcode. */
    /* 562 */ OP_pmovzxwd,   /**< IA-32/AMD64 pmovzxwd opcode. */
    /* 563 */ OP_pmovzxwq,   /**< IA-32/AMD64 pmovzxwq opcode. */
    /* 564 */ OP_pmovzxdq,   /**< IA-32/AMD64 pmovzxdq opcode. */
    /* 565 */ OP_pcmpgtq,    /**< IA-32/AMD64 pcmpgtq opcode. */
    /* 566 */ OP_pminsb,     /**< IA-32/AMD64 pminsb opcode. */
    /* 567 */ OP_pminsd,     /**< IA-32/AMD64 pminsd opcode. */
    /* 568 */ OP_pminuw,     /**< IA-32/AMD64 pminuw opcode. */
    /* 569 */ OP_pminud,     /**< IA-32/AMD64 pminud opcode. */
    /* 570 */ OP_pmaxsb,     /**< IA-32/AMD64 pmaxsb opcode. */
    /* 571 */ OP_pmaxsd,     /**< IA-32/AMD64 pmaxsd opcode. */
    /* 572 */ OP_pmaxuw,     /**< IA-32/AMD64 pmaxuw opcode. */
    /* 573 */ OP_pmaxud,     /**< IA-32/AMD64 pmaxud opcode. */
    /* 574 */ OP_pmulld,     /**< IA-32/AMD64 pmulld opcode. */
    /* 575 */ OP_phminposuw, /**< IA-32/AMD64 phminposuw opcode. */
    /* 576 */ OP_crc32,      /**< IA-32/AMD64 crc32 opcode. */
    /* 577 */ OP_pextrb,     /**< IA-32/AMD64 pextrb opcode. */
    /* 578 */ OP_pextrd,     /**< IA-32/AMD64 pextrd opcode. */
    /* 579 */ OP_extractps,  /**< IA-32/AMD64 extractps opcode. */
    /* 580 */ OP_roundps,    /**< IA-32/AMD64 roundps opcode. */
    /* 581 */ OP_roundpd,    /**< IA-32/AMD64 roundpd opcode. */
    /* 582 */ OP_roundss,    /**< IA-32/AMD64 roundss opcode. */
    /* 583 */ OP_roundsd,    /**< IA-32/AMD64 roundsd opcode. */
    /* 584 */ OP_blendps,    /**< IA-32/AMD64 blendps opcode. */
    /* 585 */ OP_blendpd,    /**< IA-32/AMD64 blendpd opcode. */
    /* 586 */ OP_pblendw,    /**< IA-32/AMD64 pblendw opcode. */
    /* 587 */ OP_pinsrb,     /**< IA-32/AMD64 pinsrb opcode. */
    /* 588 */ OP_insertps,   /**< IA-32/AMD64 insertps opcode. */
    /* 589 */ OP_pinsrd,     /**< IA-32/AMD64 pinsrd opcode. */
    /* 590 */ OP_dpps,       /**< IA-32/AMD64 dpps opcode. */
    /* 591 */ OP_dppd,       /**< IA-32/AMD64 dppd opcode. */
    /* 592 */ OP_mpsadbw,    /**< IA-32/AMD64 mpsadbw opcode. */
    /* 593 */ OP_pcmpestrm,  /**< IA-32/AMD64 pcmpestrm opcode. */
    /* 594 */ OP_pcmpestri,  /**< IA-32/AMD64 pcmpestri opcode. */
    /* 595 */ OP_pcmpistrm,  /**< IA-32/AMD64 pcmpistrm opcode. */
    /* 596 */ OP_pcmpistri,  /**< IA-32/AMD64 pcmpistri opcode. */

    /* x64 */
    /* 597 */ OP_movsxd, /**< IA-32/AMD64 movsxd opcode. */
    /* 598 */ OP_swapgs, /**< IA-32/AMD64 swapgs opcode. */

    /* VMX */
    /* 599 */ OP_vmcall,   /**< IA-32/AMD64 vmcall opcode. */
    /* 600 */ OP_vmlaunch, /**< IA-32/AMD64 vmlaunch opcode. */
    /* 601 */ OP_vmresume, /**< IA-32/AMD64 vmresume opcode. */
    /* 602 */ OP_vmxoff,   /**< IA-32/AMD64 vmxoff opcode. */
    /* 603 */ OP_vmptrst,  /**< IA-32/AMD64 vmptrst opcode. */
    /* 604 */ OP_vmptrld,  /**< IA-32/AMD64 vmptrld opcode. */
    /* 605 */ OP_vmxon,    /**< IA-32/AMD64 vmxon opcode. */
    /* 606 */ OP_vmclear,  /**< IA-32/AMD64 vmclear opcode. */
    /* 607 */ OP_vmread,   /**< IA-32/AMD64 vmread opcode. */
    /* 608 */ OP_vmwrite,  /**< IA-32/AMD64 vmwrite opcode. */

    /* undocumented */
    /* 609 */ OP_int1,   /**< IA-32/AMD64 int1 opcode. */
    /* 610 */ OP_salc,   /**< IA-32/AMD64 salc opcode. */
    /* 611 */ OP_ffreep, /**< IA-32/AMD64 ffreep opcode. */

    /* AMD SVM */
    /* 612 */ OP_vmrun,   /**< IA-32/AMD64 vmrun opcode. */
    /* 613 */ OP_vmmcall, /**< IA-32/AMD64 vmmcall opcode. */
    /* 614 */ OP_vmload,  /**< IA-32/AMD64 vmload opcode. */
    /* 615 */ OP_vmsave,  /**< IA-32/AMD64 vmsave opcode. */
    /* 616 */ OP_stgi,    /**< IA-32/AMD64 stgi opcode. */
    /* 617 */ OP_clgi,    /**< IA-32/AMD64 clgi opcode. */
    /* 618 */ OP_skinit,  /**< IA-32/AMD64 skinit opcode. */
    /* 619 */ OP_invlpga, /**< IA-32/AMD64 invlpga opcode. */
                          /* AMD though not part of SVM */
    /* 620 */ OP_rdtscp,  /**< IA-32/AMD64 rdtscp opcode. */

    /* Intel VMX additions */
    /* 621 */ OP_invept,  /**< IA-32/AMD64 invept opcode. */
    /* 622 */ OP_invvpid, /**< IA-32/AMD64 invvpid opcode. */

    /* added in Intel Westmere */
    /* 623 */ OP_pclmulqdq,       /**< IA-32/AMD64 pclmulqdq opcode. */
    /* 624 */ OP_aesimc,          /**< IA-32/AMD64 aesimc opcode. */
    /* 625 */ OP_aesenc,          /**< IA-32/AMD64 aesenc opcode. */
    /* 626 */ OP_aesenclast,      /**< IA-32/AMD64 aesenclast opcode. */
    /* 627 */ OP_aesdec,          /**< IA-32/AMD64 aesdec opcode. */
    /* 628 */ OP_aesdeclast,      /**< IA-32/AMD64 aesdeclast opcode. */
    /* 629 */ OP_aeskeygenassist, /**< IA-32/AMD64 aeskeygenassist opcode. */

    /* added in Intel Atom */
    /* 630 */ OP_movbe, /**< IA-32/AMD64 movbe opcode. */

    /* added in Intel Sandy Bridge */
    /* 631 */ OP_xgetbv,     /**< IA-32/AMD64 xgetbv opcode. */
    /* 632 */ OP_xsetbv,     /**< IA-32/AMD64 xsetbv opcode. */
    /* 633 */ OP_xsave32,    /**< IA-32/AMD64 xsave opcode. */
    /* 634 */ OP_xrstor32,   /**< IA-32/AMD64 xrstor opcode. */
    /* 635 */ OP_xsaveopt32, /**< IA-32/AMD64 xsaveopt opcode. */

    /* AVX */
    /* 636 */ OP_vmovss,           /**< IA-32/AMD64 vmovss opcode. */
    /* 637 */ OP_vmovsd,           /**< IA-32/AMD64 vmovsd opcode. */
    /* 638 */ OP_vmovups,          /**< IA-32/AMD64 vmovups opcode. */
    /* 639 */ OP_vmovupd,          /**< IA-32/AMD64 vmovupd opcode. */
    /* 640 */ OP_vmovlps,          /**< IA-32/AMD64 vmovlps opcode. */
    /* 641 */ OP_vmovsldup,        /**< IA-32/AMD64 vmovsldup opcode. */
    /* 642 */ OP_vmovlpd,          /**< IA-32/AMD64 vmovlpd opcode. */
    /* 643 */ OP_vmovddup,         /**< IA-32/AMD64 vmovddup opcode. */
    /* 644 */ OP_vunpcklps,        /**< IA-32/AMD64 vunpcklps opcode. */
    /* 645 */ OP_vunpcklpd,        /**< IA-32/AMD64 vunpcklpd opcode. */
    /* 646 */ OP_vunpckhps,        /**< IA-32/AMD64 vunpckhps opcode. */
    /* 647 */ OP_vunpckhpd,        /**< IA-32/AMD64 vunpckhpd opcode. */
    /* 648 */ OP_vmovhps,          /**< IA-32/AMD64 vmovhps opcode. */
    /* 649 */ OP_vmovshdup,        /**< IA-32/AMD64 vmovshdup opcode. */
    /* 650 */ OP_vmovhpd,          /**< IA-32/AMD64 vmovhpd opcode. */
    /* 651 */ OP_vmovaps,          /**< IA-32/AMD64 vmovaps opcode. */
    /* 652 */ OP_vmovapd,          /**< IA-32/AMD64 vmovapd opcode. */
    /* 653 */ OP_vcvtsi2ss,        /**< IA-32/AMD64 vcvtsi2ss opcode. */
    /* 654 */ OP_vcvtsi2sd,        /**< IA-32/AMD64 vcvtsi2sd opcode. */
    /* 655 */ OP_vmovntps,         /**< IA-32/AMD64 vmovntps opcode. */
    /* 656 */ OP_vmovntpd,         /**< IA-32/AMD64 vmovntpd opcode. */
    /* 657 */ OP_vcvttss2si,       /**< IA-32/AMD64 vcvttss2si opcode. */
    /* 658 */ OP_vcvttsd2si,       /**< IA-32/AMD64 vcvttsd2si opcode. */
    /* 659 */ OP_vcvtss2si,        /**< IA-32/AMD64 vcvtss2si opcode. */
    /* 660 */ OP_vcvtsd2si,        /**< IA-32/AMD64 vcvtsd2si opcode. */
    /* 661 */ OP_vucomiss,         /**< IA-32/AMD64 vucomiss opcode. */
    /* 662 */ OP_vucomisd,         /**< IA-32/AMD64 vucomisd opcode. */
    /* 663 */ OP_vcomiss,          /**< IA-32/AMD64 vcomiss opcode. */
    /* 664 */ OP_vcomisd,          /**< IA-32/AMD64 vcomisd opcode. */
    /* 665 */ OP_vmovmskps,        /**< IA-32/AMD64 vmovmskps opcode. */
    /* 666 */ OP_vmovmskpd,        /**< IA-32/AMD64 vmovmskpd opcode. */
    /* 667 */ OP_vsqrtps,          /**< IA-32/AMD64 vsqrtps opcode. */
    /* 668 */ OP_vsqrtss,          /**< IA-32/AMD64 vsqrtss opcode. */
    /* 669 */ OP_vsqrtpd,          /**< IA-32/AMD64 vsqrtpd opcode. */
    /* 670 */ OP_vsqrtsd,          /**< IA-32/AMD64 vsqrtsd opcode. */
    /* 671 */ OP_vrsqrtps,         /**< IA-32/AMD64 vrsqrtps opcode. */
    /* 672 */ OP_vrsqrtss,         /**< IA-32/AMD64 vrsqrtss opcode. */
    /* 673 */ OP_vrcpps,           /**< IA-32/AMD64 vrcpps opcode. */
    /* 674 */ OP_vrcpss,           /**< IA-32/AMD64 vrcpss opcode. */
    /* 675 */ OP_vandps,           /**< IA-32/AMD64 vandps opcode. */
    /* 676 */ OP_vandpd,           /**< IA-32/AMD64 vandpd opcode. */
    /* 677 */ OP_vandnps,          /**< IA-32/AMD64 vandnps opcode. */
    /* 678 */ OP_vandnpd,          /**< IA-32/AMD64 vandnpd opcode. */
    /* 679 */ OP_vorps,            /**< IA-32/AMD64 vorps opcode. */
    /* 680 */ OP_vorpd,            /**< IA-32/AMD64 vorpd opcode. */
    /* 681 */ OP_vxorps,           /**< IA-32/AMD64 vxorps opcode. */
    /* 682 */ OP_vxorpd,           /**< IA-32/AMD64 vxorpd opcode. */
    /* 683 */ OP_vaddps,           /**< IA-32/AMD64 vaddps opcode. */
    /* 684 */ OP_vaddss,           /**< IA-32/AMD64 vaddss opcode. */
    /* 685 */ OP_vaddpd,           /**< IA-32/AMD64 vaddpd opcode. */
    /* 686 */ OP_vaddsd,           /**< IA-32/AMD64 vaddsd opcode. */
    /* 687 */ OP_vmulps,           /**< IA-32/AMD64 vmulps opcode. */
    /* 688 */ OP_vmulss,           /**< IA-32/AMD64 vmulss opcode. */
    /* 689 */ OP_vmulpd,           /**< IA-32/AMD64 vmulpd opcode. */
    /* 690 */ OP_vmulsd,           /**< IA-32/AMD64 vmulsd opcode. */
    /* 691 */ OP_vcvtps2pd,        /**< IA-32/AMD64 vcvtps2pd opcode. */
    /* 692 */ OP_vcvtss2sd,        /**< IA-32/AMD64 vcvtss2sd opcode. */
    /* 693 */ OP_vcvtpd2ps,        /**< IA-32/AMD64 vcvtpd2ps opcode. */
    /* 694 */ OP_vcvtsd2ss,        /**< IA-32/AMD64 vcvtsd2ss opcode. */
    /* 695 */ OP_vcvtdq2ps,        /**< IA-32/AMD64 vcvtdq2ps opcode. */
    /* 696 */ OP_vcvttps2dq,       /**< IA-32/AMD64 vcvttps2dq opcode. */
    /* 697 */ OP_vcvtps2dq,        /**< IA-32/AMD64 vcvtps2dq opcode. */
    /* 698 */ OP_vsubps,           /**< IA-32/AMD64 vsubps opcode. */
    /* 699 */ OP_vsubss,           /**< IA-32/AMD64 vsubss opcode. */
    /* 700 */ OP_vsubpd,           /**< IA-32/AMD64 vsubpd opcode. */
    /* 701 */ OP_vsubsd,           /**< IA-32/AMD64 vsubsd opcode. */
    /* 702 */ OP_vminps,           /**< IA-32/AMD64 vminps opcode. */
    /* 703 */ OP_vminss,           /**< IA-32/AMD64 vminss opcode. */
    /* 704 */ OP_vminpd,           /**< IA-32/AMD64 vminpd opcode. */
    /* 705 */ OP_vminsd,           /**< IA-32/AMD64 vminsd opcode. */
    /* 706 */ OP_vdivps,           /**< IA-32/AMD64 vdivps opcode. */
    /* 707 */ OP_vdivss,           /**< IA-32/AMD64 vdivss opcode. */
    /* 708 */ OP_vdivpd,           /**< IA-32/AMD64 vdivpd opcode. */
    /* 709 */ OP_vdivsd,           /**< IA-32/AMD64 vdivsd opcode. */
    /* 710 */ OP_vmaxps,           /**< IA-32/AMD64 vmaxps opcode. */
    /* 711 */ OP_vmaxss,           /**< IA-32/AMD64 vmaxss opcode. */
    /* 712 */ OP_vmaxpd,           /**< IA-32/AMD64 vmaxpd opcode. */
    /* 713 */ OP_vmaxsd,           /**< IA-32/AMD64 vmaxsd opcode. */
    /* 714 */ OP_vpunpcklbw,       /**< IA-32/AMD64 vpunpcklbw opcode. */
    /* 715 */ OP_vpunpcklwd,       /**< IA-32/AMD64 vpunpcklwd opcode. */
    /* 716 */ OP_vpunpckldq,       /**< IA-32/AMD64 vpunpckldq opcode. */
    /* 717 */ OP_vpacksswb,        /**< IA-32/AMD64 vpacksswb opcode. */
    /* 718 */ OP_vpcmpgtb,         /**< IA-32/AMD64 vpcmpgtb opcode. */
    /* 719 */ OP_vpcmpgtw,         /**< IA-32/AMD64 vpcmpgtw opcode. */
    /* 720 */ OP_vpcmpgtd,         /**< IA-32/AMD64 vpcmpgtd opcode. */
    /* 721 */ OP_vpackuswb,        /**< IA-32/AMD64 vpackuswb opcode. */
    /* 722 */ OP_vpunpckhbw,       /**< IA-32/AMD64 vpunpckhbw opcode. */
    /* 723 */ OP_vpunpckhwd,       /**< IA-32/AMD64 vpunpckhwd opcode. */
    /* 724 */ OP_vpunpckhdq,       /**< IA-32/AMD64 vpunpckhdq opcode. */
    /* 725 */ OP_vpackssdw,        /**< IA-32/AMD64 vpackssdw opcode. */
    /* 726 */ OP_vpunpcklqdq,      /**< IA-32/AMD64 vpunpcklqdq opcode. */
    /* 727 */ OP_vpunpckhqdq,      /**< IA-32/AMD64 vpunpckhqdq opcode. */
    /* 728 */ OP_vmovd,            /**< IA-32/AMD64 vmovd opcode. */
    /* 729 */ OP_vpshufhw,         /**< IA-32/AMD64 vpshufhw opcode. */
    /* 730 */ OP_vpshufd,          /**< IA-32/AMD64 vpshufd opcode. */
    /* 731 */ OP_vpshuflw,         /**< IA-32/AMD64 vpshuflw opcode. */
    /* 732 */ OP_vpcmpeqb,         /**< IA-32/AMD64 vpcmpeqb opcode. */
    /* 733 */ OP_vpcmpeqw,         /**< IA-32/AMD64 vpcmpeqw opcode. */
    /* 734 */ OP_vpcmpeqd,         /**< IA-32/AMD64 vpcmpeqd opcode. */
    /* 735 */ OP_vmovq,            /**< IA-32/AMD64 vmovq opcode. */
    /* 736 */ OP_vcmpps,           /**< IA-32/AMD64 vcmpps opcode. */
    /* 737 */ OP_vcmpss,           /**< IA-32/AMD64 vcmpss opcode. */
    /* 738 */ OP_vcmppd,           /**< IA-32/AMD64 vcmppd opcode. */
    /* 739 */ OP_vcmpsd,           /**< IA-32/AMD64 vcmpsd opcode. */
    /* 740 */ OP_vpinsrw,          /**< IA-32/AMD64 vpinsrw opcode. */
    /* 741 */ OP_vpextrw,          /**< IA-32/AMD64 vpextrw opcode. */
    /* 742 */ OP_vshufps,          /**< IA-32/AMD64 vshufps opcode. */
    /* 743 */ OP_vshufpd,          /**< IA-32/AMD64 vshufpd opcode. */
    /* 744 */ OP_vpsrlw,           /**< IA-32/AMD64 vpsrlw opcode. */
    /* 745 */ OP_vpsrld,           /**< IA-32/AMD64 vpsrld opcode. */
    /* 746 */ OP_vpsrlq,           /**< IA-32/AMD64 vpsrlq opcode. */
    /* 747 */ OP_vpaddq,           /**< IA-32/AMD64 vpaddq opcode. */
    /* 748 */ OP_vpmullw,          /**< IA-32/AMD64 vpmullw opcode. */
    /* 749 */ OP_vpmovmskb,        /**< IA-32/AMD64 vpmovmskb opcode. */
    /* 750 */ OP_vpsubusb,         /**< IA-32/AMD64 vpsubusb opcode. */
    /* 751 */ OP_vpsubusw,         /**< IA-32/AMD64 vpsubusw opcode. */
    /* 752 */ OP_vpminub,          /**< IA-32/AMD64 vpminub opcode. */
    /* 753 */ OP_vpand,            /**< IA-32/AMD64 vpand opcode. */
    /* 754 */ OP_vpaddusb,         /**< IA-32/AMD64 vpaddusb opcode. */
    /* 755 */ OP_vpaddusw,         /**< IA-32/AMD64 vpaddusw opcode. */
    /* 756 */ OP_vpmaxub,          /**< IA-32/AMD64 vpmaxub opcode. */
    /* 757 */ OP_vpandn,           /**< IA-32/AMD64 vpandn opcode. */
    /* 758 */ OP_vpavgb,           /**< IA-32/AMD64 vpavgb opcode. */
    /* 759 */ OP_vpsraw,           /**< IA-32/AMD64 vpsraw opcode. */
    /* 760 */ OP_vpsrad,           /**< IA-32/AMD64 vpsrad opcode. */
    /* 761 */ OP_vpavgw,           /**< IA-32/AMD64 vpavgw opcode. */
    /* 762 */ OP_vpmulhuw,         /**< IA-32/AMD64 vpmulhuw opcode. */
    /* 763 */ OP_vpmulhw,          /**< IA-32/AMD64 vpmulhw opcode. */
    /* 764 */ OP_vcvtdq2pd,        /**< IA-32/AMD64 vcvtdq2pd opcode. */
    /* 765 */ OP_vcvttpd2dq,       /**< IA-32/AMD64 vcvttpd2dq opcode. */
    /* 766 */ OP_vcvtpd2dq,        /**< IA-32/AMD64 vcvtpd2dq opcode. */
    /* 767 */ OP_vmovntdq,         /**< IA-32/AMD64 vmovntdq opcode. */
    /* 768 */ OP_vpsubsb,          /**< IA-32/AMD64 vpsubsb opcode. */
    /* 769 */ OP_vpsubsw,          /**< IA-32/AMD64 vpsubsw opcode. */
    /* 770 */ OP_vpminsw,          /**< IA-32/AMD64 vpminsw opcode. */
    /* 771 */ OP_vpor,             /**< IA-32/AMD64 vpor opcode. */
    /* 772 */ OP_vpaddsb,          /**< IA-32/AMD64 vpaddsb opcode. */
    /* 773 */ OP_vpaddsw,          /**< IA-32/AMD64 vpaddsw opcode. */
    /* 774 */ OP_vpmaxsw,          /**< IA-32/AMD64 vpmaxsw opcode. */
    /* 775 */ OP_vpxor,            /**< IA-32/AMD64 vpxor opcode. */
    /* 776 */ OP_vpsllw,           /**< IA-32/AMD64 vpsllw opcode. */
    /* 777 */ OP_vpslld,           /**< IA-32/AMD64 vpslld opcode. */
    /* 778 */ OP_vpsllq,           /**< IA-32/AMD64 vpsllq opcode. */
    /* 779 */ OP_vpmuludq,         /**< IA-32/AMD64 vpmuludq opcode. */
    /* 780 */ OP_vpmaddwd,         /**< IA-32/AMD64 vpmaddwd opcode. */
    /* 781 */ OP_vpsadbw,          /**< IA-32/AMD64 vpsadbw opcode. */
    /* 782 */ OP_vmaskmovdqu,      /**< IA-32/AMD64 vmaskmovdqu opcode. */
    /* 783 */ OP_vpsubb,           /**< IA-32/AMD64 vpsubb opcode. */
    /* 784 */ OP_vpsubw,           /**< IA-32/AMD64 vpsubw opcode. */
    /* 785 */ OP_vpsubd,           /**< IA-32/AMD64 vpsubd opcode. */
    /* 786 */ OP_vpsubq,           /**< IA-32/AMD64 vpsubq opcode. */
    /* 787 */ OP_vpaddb,           /**< IA-32/AMD64 vpaddb opcode. */
    /* 788 */ OP_vpaddw,           /**< IA-32/AMD64 vpaddw opcode. */
    /* 789 */ OP_vpaddd,           /**< IA-32/AMD64 vpaddd opcode. */
    /* 790 */ OP_vpsrldq,          /**< IA-32/AMD64 vpsrldq opcode. */
    /* 791 */ OP_vpslldq,          /**< IA-32/AMD64 vpslldq opcode. */
    /* 792 */ OP_vmovdqu,          /**< IA-32/AMD64 vmovdqu opcode. */
    /* 793 */ OP_vmovdqa,          /**< IA-32/AMD64 vmovdqa opcode. */
    /* 794 */ OP_vhaddpd,          /**< IA-32/AMD64 vhaddpd opcode. */
    /* 795 */ OP_vhaddps,          /**< IA-32/AMD64 vhaddps opcode. */
    /* 796 */ OP_vhsubpd,          /**< IA-32/AMD64 vhsubpd opcode. */
    /* 797 */ OP_vhsubps,          /**< IA-32/AMD64 vhsubps opcode. */
    /* 798 */ OP_vaddsubpd,        /**< IA-32/AMD64 vaddsubpd opcode. */
    /* 799 */ OP_vaddsubps,        /**< IA-32/AMD64 vaddsubps opcode. */
    /* 800 */ OP_vlddqu,           /**< IA-32/AMD64 vlddqu opcode. */
    /* 801 */ OP_vpshufb,          /**< IA-32/AMD64 vpshufb opcode. */
    /* 802 */ OP_vphaddw,          /**< IA-32/AMD64 vphaddw opcode. */
    /* 803 */ OP_vphaddd,          /**< IA-32/AMD64 vphaddd opcode. */
    /* 804 */ OP_vphaddsw,         /**< IA-32/AMD64 vphaddsw opcode. */
    /* 805 */ OP_vpmaddubsw,       /**< IA-32/AMD64 vpmaddubsw opcode. */
    /* 806 */ OP_vphsubw,          /**< IA-32/AMD64 vphsubw opcode. */
    /* 807 */ OP_vphsubd,          /**< IA-32/AMD64 vphsubd opcode. */
    /* 808 */ OP_vphsubsw,         /**< IA-32/AMD64 vphsubsw opcode. */
    /* 809 */ OP_vpsignb,          /**< IA-32/AMD64 vpsignb opcode. */
    /* 810 */ OP_vpsignw,          /**< IA-32/AMD64 vpsignw opcode. */
    /* 811 */ OP_vpsignd,          /**< IA-32/AMD64 vpsignd opcode. */
    /* 812 */ OP_vpmulhrsw,        /**< IA-32/AMD64 vpmulhrsw opcode. */
    /* 813 */ OP_vpabsb,           /**< IA-32/AMD64 vpabsb opcode. */
    /* 814 */ OP_vpabsw,           /**< IA-32/AMD64 vpabsw opcode. */
    /* 815 */ OP_vpabsd,           /**< IA-32/AMD64 vpabsd opcode. */
    /* 816 */ OP_vpalignr,         /**< IA-32/AMD64 vpalignr opcode. */
    /* 817 */ OP_vpblendvb,        /**< IA-32/AMD64 vpblendvb opcode. */
    /* 818 */ OP_vblendvps,        /**< IA-32/AMD64 vblendvps opcode. */
    /* 819 */ OP_vblendvpd,        /**< IA-32/AMD64 vblendvpd opcode. */
    /* 820 */ OP_vptest,           /**< IA-32/AMD64 vptest opcode. */
    /* 821 */ OP_vpmovsxbw,        /**< IA-32/AMD64 vpmovsxbw opcode. */
    /* 822 */ OP_vpmovsxbd,        /**< IA-32/AMD64 vpmovsxbd opcode. */
    /* 823 */ OP_vpmovsxbq,        /**< IA-32/AMD64 vpmovsxbq opcode. */
    /* 824 */ OP_vpmovsxwd,        /**< IA-32/AMD64 vpmovsxwd opcode. */
    /* 825 */ OP_vpmovsxwq,        /**< IA-32/AMD64 vpmovsxwq opcode. */
    /* 826 */ OP_vpmovsxdq,        /**< IA-32/AMD64 vpmovsxdq opcode. */
    /* 827 */ OP_vpmuldq,          /**< IA-32/AMD64 vpmuldq opcode. */
    /* 828 */ OP_vpcmpeqq,         /**< IA-32/AMD64 vpcmpeqq opcode. */
    /* 829 */ OP_vmovntdqa,        /**< IA-32/AMD64 vmovntdqa opcode. */
    /* 830 */ OP_vpackusdw,        /**< IA-32/AMD64 vpackusdw opcode. */
    /* 831 */ OP_vpmovzxbw,        /**< IA-32/AMD64 vpmovzxbw opcode. */
    /* 832 */ OP_vpmovzxbd,        /**< IA-32/AMD64 vpmovzxbd opcode. */
    /* 833 */ OP_vpmovzxbq,        /**< IA-32/AMD64 vpmovzxbq opcode. */
    /* 834 */ OP_vpmovzxwd,        /**< IA-32/AMD64 vpmovzxwd opcode. */
    /* 835 */ OP_vpmovzxwq,        /**< IA-32/AMD64 vpmovzxwq opcode. */
    /* 836 */ OP_vpmovzxdq,        /**< IA-32/AMD64 vpmovzxdq opcode. */
    /* 837 */ OP_vpcmpgtq,         /**< IA-32/AMD64 vpcmpgtq opcode. */
    /* 838 */ OP_vpminsb,          /**< IA-32/AMD64 vpminsb opcode. */
    /* 839 */ OP_vpminsd,          /**< IA-32/AMD64 vpminsd opcode. */
    /* 840 */ OP_vpminuw,          /**< IA-32/AMD64 vpminuw opcode. */
    /* 841 */ OP_vpminud,          /**< IA-32/AMD64 vpminud opcode. */
    /* 842 */ OP_vpmaxsb,          /**< IA-32/AMD64 vpmaxsb opcode. */
    /* 843 */ OP_vpmaxsd,          /**< IA-32/AMD64 vpmaxsd opcode. */
    /* 844 */ OP_vpmaxuw,          /**< IA-32/AMD64 vpmaxuw opcode. */
    /* 845 */ OP_vpmaxud,          /**< IA-32/AMD64 vpmaxud opcode. */
    /* 846 */ OP_vpmulld,          /**< IA-32/AMD64 vpmulld opcode. */
    /* 847 */ OP_vphminposuw,      /**< IA-32/AMD64 vphminposuw opcode. */
    /* 848 */ OP_vaesimc,          /**< IA-32/AMD64 vaesimc opcode. */
    /* 849 */ OP_vaesenc,          /**< IA-32/AMD64 vaesenc opcode. */
    /* 850 */ OP_vaesenclast,      /**< IA-32/AMD64 vaesenclast opcode. */
    /* 851 */ OP_vaesdec,          /**< IA-32/AMD64 vaesdec opcode. */
    /* 852 */ OP_vaesdeclast,      /**< IA-32/AMD64 vaesdeclast opcode. */
    /* 853 */ OP_vpextrb,          /**< IA-32/AMD64 vpextrb opcode. */
    /* 854 */ OP_vpextrd,          /**< IA-32/AMD64 vpextrd opcode. */
    /* 855 */ OP_vextractps,       /**< IA-32/AMD64 vextractps opcode. */
    /* 856 */ OP_vroundps,         /**< IA-32/AMD64 vroundps opcode. */
    /* 857 */ OP_vroundpd,         /**< IA-32/AMD64 vroundpd opcode. */
    /* 858 */ OP_vroundss,         /**< IA-32/AMD64 vroundss opcode. */
    /* 859 */ OP_vroundsd,         /**< IA-32/AMD64 vroundsd opcode. */
    /* 860 */ OP_vblendps,         /**< IA-32/AMD64 vblendps opcode. */
    /* 861 */ OP_vblendpd,         /**< IA-32/AMD64 vblendpd opcode. */
    /* 862 */ OP_vpblendw,         /**< IA-32/AMD64 vpblendw opcode. */
    /* 863 */ OP_vpinsrb,          /**< IA-32/AMD64 vpinsrb opcode. */
    /* 864 */ OP_vinsertps,        /**< IA-32/AMD64 vinsertps opcode. */
    /* 865 */ OP_vpinsrd,          /**< IA-32/AMD64 vpinsrd opcode. */
    /* 866 */ OP_vdpps,            /**< IA-32/AMD64 vdpps opcode. */
    /* 867 */ OP_vdppd,            /**< IA-32/AMD64 vdppd opcode. */
    /* 868 */ OP_vmpsadbw,         /**< IA-32/AMD64 vmpsadbw opcode. */
    /* 869 */ OP_vpcmpestrm,       /**< IA-32/AMD64 vpcmpestrm opcode. */
    /* 870 */ OP_vpcmpestri,       /**< IA-32/AMD64 vpcmpestri opcode. */
    /* 871 */ OP_vpcmpistrm,       /**< IA-32/AMD64 vpcmpistrm opcode. */
    /* 872 */ OP_vpcmpistri,       /**< IA-32/AMD64 vpcmpistri opcode. */
    /* 873 */ OP_vpclmulqdq,       /**< IA-32/AMD64 vpclmulqdq opcode. */
    /* 874 */ OP_vaeskeygenassist, /**< IA-32/AMD64 vaeskeygenassist opcode. */
    /* 875 */ OP_vtestps,          /**< IA-32/AMD64 vtestps opcode. */
    /* 876 */ OP_vtestpd,          /**< IA-32/AMD64 vtestpd opcode. */
    /* 877 */ OP_vzeroupper,       /**< IA-32/AMD64 vzeroupper opcode. */
    /* 878 */ OP_vzeroall,         /**< IA-32/AMD64 vzeroall opcode. */
    /* 879 */ OP_vldmxcsr,         /**< IA-32/AMD64 vldmxcsr opcode. */
    /* 880 */ OP_vstmxcsr,         /**< IA-32/AMD64 vstmxcsr opcode. */
    /* 881 */ OP_vbroadcastss,     /**< IA-32/AMD64 vbroadcastss opcode. */
    /* 882 */ OP_vbroadcastsd,     /**< IA-32/AMD64 vbroadcastsd opcode. */
    /* 883 */ OP_vbroadcastf128,   /**< IA-32/AMD64 vbroadcastf128 opcode. */
    /* 884 */ OP_vmaskmovps,       /**< IA-32/AMD64 vmaskmovps opcode. */
    /* 885 */ OP_vmaskmovpd,       /**< IA-32/AMD64 vmaskmovpd opcode. */
    /* 886 */ OP_vpermilps,        /**< IA-32/AMD64 vpermilps opcode. */
    /* 887 */ OP_vpermilpd,        /**< IA-32/AMD64 vpermilpd opcode. */
    /* 888 */ OP_vperm2f128,       /**< IA-32/AMD64 vperm2f128 opcode. */
    /* 889 */ OP_vinsertf128,      /**< IA-32/AMD64 vinsertf128 opcode. */
    /* 890 */ OP_vextractf128,     /**< IA-32/AMD64 vextractf128 opcode. */

    /* added in Ivy Bridge I believe, and covered by F16C cpuid flag */
    /* 891 */ OP_vcvtph2ps, /**< IA-32/AMD64 vcvtph2ps opcode. */
    /* 892 */ OP_vcvtps2ph, /**< IA-32/AMD64 vcvtps2ph opcode. */

    /* FMA */
    /* 893 */ OP_vfmadd132ps,    /**< IA-32/AMD64 vfmadd132ps opcode. */
    /* 894 */ OP_vfmadd132pd,    /**< IA-32/AMD64 vfmadd132pd opcode. */
    /* 895 */ OP_vfmadd213ps,    /**< IA-32/AMD64 vfmadd213ps opcode. */
    /* 896 */ OP_vfmadd213pd,    /**< IA-32/AMD64 vfmadd213pd opcode. */
    /* 897 */ OP_vfmadd231ps,    /**< IA-32/AMD64 vfmadd231ps opcode. */
    /* 898 */ OP_vfmadd231pd,    /**< IA-32/AMD64 vfmadd231pd opcode. */
    /* 899 */ OP_vfmadd132ss,    /**< IA-32/AMD64 vfmadd132ss opcode. */
    /* 900 */ OP_vfmadd132sd,    /**< IA-32/AMD64 vfmadd132sd opcode. */
    /* 901 */ OP_vfmadd213ss,    /**< IA-32/AMD64 vfmadd213ss opcode. */
    /* 902 */ OP_vfmadd213sd,    /**< IA-32/AMD64 vfmadd213sd opcode. */
    /* 903 */ OP_vfmadd231ss,    /**< IA-32/AMD64 vfmadd231ss opcode. */
    /* 904 */ OP_vfmadd231sd,    /**< IA-32/AMD64 vfmadd231sd opcode. */
    /* 905 */ OP_vfmaddsub132ps, /**< IA-32/AMD64 vfmaddsub132ps opcode. */
    /* 906 */ OP_vfmaddsub132pd, /**< IA-32/AMD64 vfmaddsub132pd opcode. */
    /* 907 */ OP_vfmaddsub213ps, /**< IA-32/AMD64 vfmaddsub213ps opcode. */
    /* 908 */ OP_vfmaddsub213pd, /**< IA-32/AMD64 vfmaddsub213pd opcode. */
    /* 909 */ OP_vfmaddsub231ps, /**< IA-32/AMD64 vfmaddsub231ps opcode. */
    /* 910 */ OP_vfmaddsub231pd, /**< IA-32/AMD64 vfmaddsub231pd opcode. */
    /* 911 */ OP_vfmsubadd132ps, /**< IA-32/AMD64 vfmsubadd132ps opcode. */
    /* 912 */ OP_vfmsubadd132pd, /**< IA-32/AMD64 vfmsubadd132pd opcode. */
    /* 913 */ OP_vfmsubadd213ps, /**< IA-32/AMD64 vfmsubadd213ps opcode. */
    /* 914 */ OP_vfmsubadd213pd, /**< IA-32/AMD64 vfmsubadd213pd opcode. */
    /* 915 */ OP_vfmsubadd231ps, /**< IA-32/AMD64 vfmsubadd231ps opcode. */
    /* 916 */ OP_vfmsubadd231pd, /**< IA-32/AMD64 vfmsubadd231pd opcode. */
    /* 917 */ OP_vfmsub132ps,    /**< IA-32/AMD64 vfmsub132ps opcode. */
    /* 918 */ OP_vfmsub132pd,    /**< IA-32/AMD64 vfmsub132pd opcode. */
    /* 919 */ OP_vfmsub213ps,    /**< IA-32/AMD64 vfmsub213ps opcode. */
    /* 920 */ OP_vfmsub213pd,    /**< IA-32/AMD64 vfmsub213pd opcode. */
    /* 921 */ OP_vfmsub231ps,    /**< IA-32/AMD64 vfmsub231ps opcode. */
    /* 922 */ OP_vfmsub231pd,    /**< IA-32/AMD64 vfmsub231pd opcode. */
    /* 923 */ OP_vfmsub132ss,    /**< IA-32/AMD64 vfmsub132ss opcode. */
    /* 924 */ OP_vfmsub132sd,    /**< IA-32/AMD64 vfmsub132sd opcode. */
    /* 925 */ OP_vfmsub213ss,    /**< IA-32/AMD64 vfmsub213ss opcode. */
    /* 926 */ OP_vfmsub213sd,    /**< IA-32/AMD64 vfmsub213sd opcode. */
    /* 927 */ OP_vfmsub231ss,    /**< IA-32/AMD64 vfmsub231ss opcode. */
    /* 928 */ OP_vfmsub231sd,    /**< IA-32/AMD64 vfmsub231sd opcode. */
    /* 929 */ OP_vfnmadd132ps,   /**< IA-32/AMD64 vfnmadd132ps opcode. */
    /* 930 */ OP_vfnmadd132pd,   /**< IA-32/AMD64 vfnmadd132pd opcode. */
    /* 931 */ OP_vfnmadd213ps,   /**< IA-32/AMD64 vfnmadd213ps opcode. */
    /* 932 */ OP_vfnmadd213pd,   /**< IA-32/AMD64 vfnmadd213pd opcode. */
    /* 933 */ OP_vfnmadd231ps,   /**< IA-32/AMD64 vfnmadd231ps opcode. */
    /* 934 */ OP_vfnmadd231pd,   /**< IA-32/AMD64 vfnmadd231pd opcode. */
    /* 935 */ OP_vfnmadd132ss,   /**< IA-32/AMD64 vfnmadd132ss opcode. */
    /* 936 */ OP_vfnmadd132sd,   /**< IA-32/AMD64 vfnmadd132sd opcode. */
    /* 937 */ OP_vfnmadd213ss,   /**< IA-32/AMD64 vfnmadd213ss opcode. */
    /* 938 */ OP_vfnmadd213sd,   /**< IA-32/AMD64 vfnmadd213sd opcode. */
    /* 939 */ OP_vfnmadd231ss,   /**< IA-32/AMD64 vfnmadd231ss opcode. */
    /* 940 */ OP_vfnmadd231sd,   /**< IA-32/AMD64 vfnmadd231sd opcode. */
    /* 941 */ OP_vfnmsub132ps,   /**< IA-32/AMD64 vfnmsub132ps opcode. */
    /* 942 */ OP_vfnmsub132pd,   /**< IA-32/AMD64 vfnmsub132pd opcode. */
    /* 943 */ OP_vfnmsub213ps,   /**< IA-32/AMD64 vfnmsub213ps opcode. */
    /* 944 */ OP_vfnmsub213pd,   /**< IA-32/AMD64 vfnmsub213pd opcode. */
    /* 945 */ OP_vfnmsub231ps,   /**< IA-32/AMD64 vfnmsub231ps opcode. */
    /* 946 */ OP_vfnmsub231pd,   /**< IA-32/AMD64 vfnmsub231pd opcode. */
    /* 947 */ OP_vfnmsub132ss,   /**< IA-32/AMD64 vfnmsub132ss opcode. */
    /* 948 */ OP_vfnmsub132sd,   /**< IA-32/AMD64 vfnmsub132sd opcode. */
    /* 949 */ OP_vfnmsub213ss,   /**< IA-32/AMD64 vfnmsub213ss opcode. */
    /* 950 */ OP_vfnmsub213sd,   /**< IA-32/AMD64 vfnmsub213sd opcode. */
    /* 951 */ OP_vfnmsub231ss,   /**< IA-32/AMD64 vfnmsub231ss opcode. */
    /* 952 */ OP_vfnmsub231sd,   /**< IA-32/AMD64 vfnmsub231sd opcode. */

    /* 953 */ OP_movq2dq, /**< IA-32/AMD64 movq2dq opcode. */
    /* 954 */ OP_movdq2q, /**< IA-32/AMD64 movdq2q opcode. */

    /* 955 */ OP_fxsave64,   /**< IA-32/AMD64 fxsave64 opcode. */
    /* 956 */ OP_fxrstor64,  /**< IA-32/AMD64 fxrstor64 opcode. */
    /* 957 */ OP_xsave64,    /**< IA-32/AMD64 xsave64 opcode. */
    /* 958 */ OP_xrstor64,   /**< IA-32/AMD64 xrstor64 opcode. */
    /* 959 */ OP_xsaveopt64, /**< IA-32/AMD64 xsaveopt64 opcode. */

    /* added in Intel Ivy Bridge: RDRAND and FSGSBASE cpuid flags */
    /* 960 */ OP_rdrand,   /**< IA-32/AMD64 rdrand opcode. */
    /* 961 */ OP_rdfsbase, /**< IA-32/AMD64 rdfsbase opcode. */
    /* 962 */ OP_rdgsbase, /**< IA-32/AMD64 rdgsbase opcode. */
    /* 963 */ OP_wrfsbase, /**< IA-32/AMD64 wrfsbase opcode. */
    /* 964 */ OP_wrgsbase, /**< IA-32/AMD64 wrgsbase opcode. */

    /* coming in the future but adding now since enough details are known */
    /* 965 */ OP_rdseed, /**< IA-32/AMD64 rdseed opcode. */

    /* AMD FMA4 */
    /* 966 */ OP_vfmaddsubps, /**< IA-32/AMD64 vfmaddsubps opcode. */
    /* 967 */ OP_vfmaddsubpd, /**< IA-32/AMD64 vfmaddsubpd opcode. */
    /* 968 */ OP_vfmsubaddps, /**< IA-32/AMD64 vfmsubaddps opcode. */
    /* 969 */ OP_vfmsubaddpd, /**< IA-32/AMD64 vfmsubaddpd opcode. */
    /* 970 */ OP_vfmaddps,    /**< IA-32/AMD64 vfmaddps opcode. */
    /* 971 */ OP_vfmaddpd,    /**< IA-32/AMD64 vfmaddpd opcode. */
    /* 972 */ OP_vfmaddss,    /**< IA-32/AMD64 vfmaddss opcode. */
    /* 973 */ OP_vfmaddsd,    /**< IA-32/AMD64 vfmaddsd opcode. */
    /* 974 */ OP_vfmsubps,    /**< IA-32/AMD64 vfmsubps opcode. */
    /* 975 */ OP_vfmsubpd,    /**< IA-32/AMD64 vfmsubpd opcode. */
    /* 976 */ OP_vfmsubss,    /**< IA-32/AMD64 vfmsubss opcode. */
    /* 977 */ OP_vfmsubsd,    /**< IA-32/AMD64 vfmsubsd opcode. */
    /* 978 */ OP_vfnmaddps,   /**< IA-32/AMD64 vfnmaddps opcode. */
    /* 979 */ OP_vfnmaddpd,   /**< IA-32/AMD64 vfnmaddpd opcode. */
    /* 980 */ OP_vfnmaddss,   /**< IA-32/AMD64 vfnmaddss opcode. */
    /* 981 */ OP_vfnmaddsd,   /**< IA-32/AMD64 vfnmaddsd opcode. */
    /* 982 */ OP_vfnmsubps,   /**< IA-32/AMD64 vfnmsubps opcode. */
    /* 983 */ OP_vfnmsubpd,   /**< IA-32/AMD64 vfnmsubpd opcode. */
    /* 984 */ OP_vfnmsubss,   /**< IA-32/AMD64 vfnmsubss opcode. */
    /* 985 */ OP_vfnmsubsd,   /**< IA-32/AMD64 vfnmsubsd opcode. */

    /* AMD XOP */
    /* 986 */ OP_vfrczps,     /**< IA-32/AMD64 vfrczps opcode. */
    /* 987 */ OP_vfrczpd,     /**< IA-32/AMD64 vfrczpd opcode. */
    /* 988 */ OP_vfrczss,     /**< IA-32/AMD64 vfrczss opcode. */
    /* 989 */ OP_vfrczsd,     /**< IA-32/AMD64 vfrczsd opcode. */
    /* 990 */ OP_vpcmov,      /**< IA-32/AMD64 vpcmov opcode. */
    /* 991 */ OP_vpcomb,      /**< IA-32/AMD64 vpcomb opcode. */
    /* 992 */ OP_vpcomw,      /**< IA-32/AMD64 vpcomw opcode. */
    /* 993 */ OP_vpcomd,      /**< IA-32/AMD64 vpcomd opcode. */
    /* 994 */ OP_vpcomq,      /**< IA-32/AMD64 vpcomq opcode. */
    /* 995 */ OP_vpcomub,     /**< IA-32/AMD64 vpcomub opcode. */
    /* 996 */ OP_vpcomuw,     /**< IA-32/AMD64 vpcomuw opcode. */
    /* 997 */ OP_vpcomud,     /**< IA-32/AMD64 vpcomud opcode. */
    /* 998 */ OP_vpcomuq,     /**< IA-32/AMD64 vpcomuq opcode. */
    /* 999 */ OP_vpermil2pd,  /**< IA-32/AMD64 vpermil2pd opcode. */
    /* 1000 */ OP_vpermil2ps, /**< IA-32/AMD64 vpermil2ps opcode. */
    /* 1001 */ OP_vphaddbw,   /**< IA-32/AMD64 vphaddbw opcode. */
    /* 1002 */ OP_vphaddbd,   /**< IA-32/AMD64 vphaddbd opcode. */
    /* 1003 */ OP_vphaddbq,   /**< IA-32/AMD64 vphaddbq opcode. */
    /* 1004 */ OP_vphaddwd,   /**< IA-32/AMD64 vphaddwd opcode. */
    /* 1005 */ OP_vphaddwq,   /**< IA-32/AMD64 vphaddwq opcode. */
    /* 1006 */ OP_vphadddq,   /**< IA-32/AMD64 vphadddq opcode. */
    /* 1007 */ OP_vphaddubw,  /**< IA-32/AMD64 vphaddubw opcode. */
    /* 1008 */ OP_vphaddubd,  /**< IA-32/AMD64 vphaddubd opcode. */
    /* 1009 */ OP_vphaddubq,  /**< IA-32/AMD64 vphaddubq opcode. */
    /* 1010 */ OP_vphadduwd,  /**< IA-32/AMD64 vphadduwd opcode. */
    /* 1011 */ OP_vphadduwq,  /**< IA-32/AMD64 vphadduwq opcode. */
    /* 1012 */ OP_vphaddudq,  /**< IA-32/AMD64 vphaddudq opcode. */
    /* 1013 */ OP_vphsubbw,   /**< IA-32/AMD64 vphsubbw opcode. */
    /* 1014 */ OP_vphsubwd,   /**< IA-32/AMD64 vphsubwd opcode. */
    /* 1015 */ OP_vphsubdq,   /**< IA-32/AMD64 vphsubdq opcode. */
    /* 1016 */ OP_vpmacssww,  /**< IA-32/AMD64 vpmacssww opcode. */
    /* 1017 */ OP_vpmacsswd,  /**< IA-32/AMD64 vpmacsswd opcode. */
    /* 1018 */ OP_vpmacssdql, /**< IA-32/AMD64 vpmacssdql opcode. */
    /* 1019 */ OP_vpmacssdd,  /**< IA-32/AMD64 vpmacssdd opcode. */
    /* 1020 */ OP_vpmacssdqh, /**< IA-32/AMD64 vpmacssdqh opcode. */
    /* 1021 */ OP_vpmacsww,   /**< IA-32/AMD64 vpmacsww opcode. */
    /* 1022 */ OP_vpmacswd,   /**< IA-32/AMD64 vpmacswd opcode. */
    /* 1023 */ OP_vpmacsdql,  /**< IA-32/AMD64 vpmacsdql opcode. */
    /* 1024 */ OP_vpmacsdd,   /**< IA-32/AMD64 vpmacsdd opcode. */
    /* 1025 */ OP_vpmacsdqh,  /**< IA-32/AMD64 vpmacsdqh opcode. */
    /* 1026 */ OP_vpmadcsswd, /**< IA-32/AMD64 vpmadcsswd opcode. */
    /* 1027 */ OP_vpmadcswd,  /**< IA-32/AMD64 vpmadcswd opcode. */
    /* 1028 */ OP_vpperm,     /**< IA-32/AMD64 vpperm opcode. */
    /* 1029 */ OP_vprotb,     /**< IA-32/AMD64 vprotb opcode. */
    /* 1030 */ OP_vprotw,     /**< IA-32/AMD64 vprotw opcode. */
    /* 1031 */ OP_vprotd,     /**< IA-32/AMD64 vprotd opcode. */
    /* 1032 */ OP_vprotq,     /**< IA-32/AMD64 vprotq opcode. */
    /* 1033 */ OP_vpshlb,     /**< IA-32/AMD64 vpshlb opcode. */
    /* 1034 */ OP_vpshlw,     /**< IA-32/AMD64 vpshlw opcode. */
    /* 1035 */ OP_vpshld,     /**< IA-32/AMD64 vpshld opcode. */
    /* 1036 */ OP_vpshlq,     /**< IA-32/AMD64 vpshlq opcode. */
    /* 1037 */ OP_vpshab,     /**< IA-32/AMD64 vpshab opcode. */
    /* 1038 */ OP_vpshaw,     /**< IA-32/AMD64 vpshaw opcode. */
    /* 1039 */ OP_vpshad,     /**< IA-32/AMD64 vpshad opcode. */
    /* 1040 */ OP_vpshaq,     /**< IA-32/AMD64 vpshaq opcode. */

    /* AMD TBM */
    /* 1041 */ OP_bextr,   /**< IA-32/AMD64 bextr opcode. */
    /* 1042 */ OP_blcfill, /**< IA-32/AMD64 blcfill opcode. */
    /* 1043 */ OP_blci,    /**< IA-32/AMD64 blci opcode. */
    /* 1044 */ OP_blcic,   /**< IA-32/AMD64 blcic opcode. */
    /* 1045 */ OP_blcmsk,  /**< IA-32/AMD64 blcmsk opcode. */
    /* 1046 */ OP_blcs,    /**< IA-32/AMD64 blcs opcode. */
    /* 1047 */ OP_blsfill, /**< IA-32/AMD64 blsfill opcode. */
    /* 1048 */ OP_blsic,   /**< IA-32/AMD64 blsic opcode. */
    /* 1049 */ OP_t1mskc,  /**< IA-32/AMD64 t1mskc opcode. */
    /* 1050 */ OP_tzmsk,   /**< IA-32/AMD64 tzmsk opcode. */

    /* AMD LWP */
    /* 1051 */ OP_llwpcb, /**< IA-32/AMD64 llwpcb opcode. */
    /* 1052 */ OP_slwpcb, /**< IA-32/AMD64 slwpcb opcode. */
    /* 1053 */ OP_lwpins, /**< IA-32/AMD64 lwpins opcode. */
    /* 1054 */ OP_lwpval, /**< IA-32/AMD64 lwpval opcode. */

    /* Intel BMI1 */
    /* (includes non-immed form of OP_bextr) */
    /* 1055 */ OP_andn,   /**< IA-32/AMD64 andn opcode. */
    /* 1056 */ OP_blsr,   /**< IA-32/AMD64 blsr opcode. */
    /* 1057 */ OP_blsmsk, /**< IA-32/AMD64 blsmsk opcode. */
    /* 1058 */ OP_blsi,   /**< IA-32/AMD64 blsi opcode. */
    /* 1059 */ OP_tzcnt,  /**< IA-32/AMD64 tzcnt opcode. */

    /* Intel BMI2 */
    /* 1060 */ OP_bzhi, /**< IA-32/AMD64 bzhi opcode. */
    /* 1061 */ OP_pext, /**< IA-32/AMD64 pext opcode. */
    /* 1062 */ OP_pdep, /**< IA-32/AMD64 pdep opcode. */
    /* 1063 */ OP_sarx, /**< IA-32/AMD64 sarx opcode. */
    /* 1064 */ OP_shlx, /**< IA-32/AMD64 shlx opcode. */
    /* 1065 */ OP_shrx, /**< IA-32/AMD64 shrx opcode. */
    /* 1066 */ OP_rorx, /**< IA-32/AMD64 rorx opcode. */
    /* 1067 */ OP_mulx, /**< IA-32/AMD64 mulx opcode. */

    /* Intel Safer Mode Extensions */
    /* 1068 */ OP_getsec, /**< IA-32/AMD64 getsec opcode. */

    /* Misc Intel additions */
    /* 1069 */ OP_vmfunc,  /**< IA-32/AMD64 vmfunc opcode. */
    /* 1070 */ OP_invpcid, /**< IA-32/AMD64 invpcid opcode. */

    /* Intel TSX */
    /* 1071 */ OP_xabort, /**< IA-32/AMD64 xabort opcode. */
    /* 1072 */ OP_xbegin, /**< IA-32/AMD64 xbegin opcode. */
    /* 1073 */ OP_xend,   /**< IA-32/AMD64 xend opcode. */
    /* 1074 */ OP_xtest,  /**< IA-32/AMD64 xtest opcode. */

    /* AVX2 */
    /* 1075 */ OP_vpgatherdd,     /**< IA-32/AMD64 vpgatherdd opcode. */
    /* 1076 */ OP_vpgatherdq,     /**< IA-32/AMD64 vpgatherdq opcode. */
    /* 1077 */ OP_vpgatherqd,     /**< IA-32/AMD64 vpgatherqd opcode. */
    /* 1078 */ OP_vpgatherqq,     /**< IA-32/AMD64 vpgatherqq opcode. */
    /* 1079 */ OP_vgatherdps,     /**< IA-32/AMD64 vgatherdps opcode. */
    /* 1080 */ OP_vgatherdpd,     /**< IA-32/AMD64 vgatherdpd opcode. */
    /* 1081 */ OP_vgatherqps,     /**< IA-32/AMD64 vgatherqps opcode. */
    /* 1082 */ OP_vgatherqpd,     /**< IA-32/AMD64 vgatherqpd opcode. */
    /* 1083 */ OP_vbroadcasti128, /**< IA-32/AMD64 vbroadcasti128 opcode. */
    /* 1084 */ OP_vinserti128,    /**< IA-32/AMD64 vinserti128 opcode. */
    /* 1085 */ OP_vextracti128,   /**< IA-32/AMD64 vextracti128 opcode. */
    /* 1086 */ OP_vpmaskmovd,     /**< IA-32/AMD64 vpmaskmovd opcode. */
    /* 1087 */ OP_vpmaskmovq,     /**< IA-32/AMD64 vpmaskmovq opcode. */
    /* 1088 */ OP_vperm2i128,     /**< IA-32/AMD64 vperm2i128 opcode. */
    /* 1089 */ OP_vpermd,         /**< IA-32/AMD64 vpermd opcode. */
    /* 1090 */ OP_vpermps,        /**< IA-32/AMD64 vpermps opcode. */
    /* 1091 */ OP_vpermq,         /**< IA-32/AMD64 vpermq opcode. */
    /* 1092 */ OP_vpermpd,        /**< IA-32/AMD64 vpermpd opcode. */
    /* 1093 */ OP_vpblendd,       /**< IA-32/AMD64 vpblendd opcode. */
    /* 1094 */ OP_vpsllvd,        /**< IA-32/AMD64 vpsllvd opcode. */
    /* 1095 */ OP_vpsllvq,        /**< IA-32/AMD64 vpsllvq opcode. */
    /* 1096 */ OP_vpsravd,        /**< IA-32/AMD64 vpsravd opcode. */
    /* 1097 */ OP_vpsrlvd,        /**< IA-32/AMD64 vpsrlvd opcode. */
    /* 1098 */ OP_vpsrlvq,        /**< IA-32/AMD64 vpsrlvq opcode. */
    /* 1099 */ OP_vpbroadcastb,   /**< IA-32/AMD64 vpbroadcastb opcode. */
    /* 1100 */ OP_vpbroadcastw,   /**< IA-32/AMD64 vpbroadcastw opcode. */
    /* 1101 */ OP_vpbroadcastd,   /**< IA-32/AMD64 vpbroadcastd opcode. */
    /* 1102 */ OP_vpbroadcastq,   /**< IA-32/AMD64 vpbroadcastq opcode. */

    /* added in Intel Skylake */
    /* 1103 */ OP_xsavec32, /**< IA-32/AMD64 xsavec opcode. */
    /* 1104 */ OP_xsavec64, /**< IA-32/AMD64 xsavec64 opcode. */

    /* Intel ADX */
    /* 1105 */ OP_adox, /**< IA-32/AMD64 adox opcode. */
    /* 1106 */ OP_adcx, /**< IA-32/AMD64 adox opcode. */

    /* Intel AVX-512 VEX */
    /* 1107 */ OP_kmovw,    /**< IA-32/AMD64 AVX-512 kmovw opcode. */
    /* 1108 */ OP_kmovb,    /**< IA-32/AMD64 AVX-512 kmovb opcode. */
    /* 1109 */ OP_kmovq,    /**< IA-32/AMD64 AVX-512 kmovq opcode. */
    /* 1110 */ OP_kmovd,    /**< IA-32/AMD64 AVX-512 kmovd opcode. */
    /* 1111 */ OP_kandw,    /**< IA-32/AMD64 AVX-512 kandw opcode. */
    /* 1112 */ OP_kandb,    /**< IA-32/AMD64 AVX-512 kandb opcode. */
    /* 1113 */ OP_kandq,    /**< IA-32/AMD64 AVX-512 kandq opcode. */
    /* 1114 */ OP_kandd,    /**< IA-32/AMD64 AVX-512 kandd opcode. */
    /* 1115 */ OP_kandnw,   /**< IA-32/AMD64 AVX-512 kandnw opcode. */
    /* 1116 */ OP_kandnb,   /**< IA-32/AMD64 AVX-512 kandnb opcode. */
    /* 1117 */ OP_kandnq,   /**< IA-32/AMD64 AVX-512 kandnq opcode. */
    /* 1118 */ OP_kandnd,   /**< IA-32/AMD64 AVX-512 kandnd opcode. */
    /* 1119 */ OP_kunpckbw, /**< IA-32/AMD64 AVX-512 kunpckbw opcode. */
    /* 1120 */ OP_kunpckwd, /**< IA-32/AMD64 AVX-512 kunpckwd opcode. */
    /* 1121 */ OP_kunpckdq, /**< IA-32/AMD64 AVX-512 kunpckdq opcode. */
    /* 1122 */ OP_knotw,    /**< IA-32/AMD64 AVX-512 knotw opcode. */
    /* 1123 */ OP_knotb,    /**< IA-32/AMD64 AVX-512 knotb opcode. */
    /* 1124 */ OP_knotq,    /**< IA-32/AMD64 AVX-512 knotq opcode. */
    /* 1125 */ OP_knotd,    /**< IA-32/AMD64 AVX-512 knotd opcode. */
    /* 1126 */ OP_korw,     /**< IA-32/AMD64 AVX-512 korw opcode. */
    /* 1127 */ OP_korb,     /**< IA-32/AMD64 AVX-512 korb opcode. */
    /* 1128 */ OP_korq,     /**< IA-32/AMD64 AVX-512 korq opcode. */
    /* 1129 */ OP_kord,     /**< IA-32/AMD64 AVX-512 kord opcode. */
    /* 1130 */ OP_kxnorw,   /**< IA-32/AMD64 AVX-512 kxnorw opcode. */
    /* 1131 */ OP_kxnorb,   /**< IA-32/AMD64 AVX-512 kxnorb opcode. */
    /* 1132 */ OP_kxnorq,   /**< IA-32/AMD64 AVX-512 kxnorq opcode. */
    /* 1133 */ OP_kxnord,   /**< IA-32/AMD64 AVX-512 kxnord opcode. */
    /* 1134 */ OP_kxorw,    /**< IA-32/AMD64 AVX-512 kxorw opcode. */
    /* 1135 */ OP_kxorb,    /**< IA-32/AMD64 AVX-512 kxorb opcode. */
    /* 1136 */ OP_kxorq,    /**< IA-32/AMD64 AVX-512 kxorq opcode. */
    /* 1137 */ OP_kxord,    /**< IA-32/AMD64 AVX-512 kxord opcode. */
    /* 1138 */ OP_kaddw,    /**< IA-32/AMD64 AVX-512 kaddw opcode. */
    /* 1139 */ OP_kaddb,    /**< IA-32/AMD64 AVX-512 kaddb opcode. */
    /* 1140 */ OP_kaddq,    /**< IA-32/AMD64 AVX-512 kaddq opcode. */
    /* 1141 */ OP_kaddd,    /**< IA-32/AMD64 AVX-512 kaddd opcode. */
    /* 1142 */ OP_kortestw, /**< IA-32/AMD64 AVX-512 kortestw opcode. */
    /* 1143 */ OP_kortestb, /**< IA-32/AMD64 AVX-512 kortestb opcode. */
    /* 1144 */ OP_kortestq, /**< IA-32/AMD64 AVX-512 kortestq opcode. */
    /* 1145 */ OP_kortestd, /**< IA-32/AMD64 AVX-512 kortestd opcode. */
    /* 1146 */ OP_kshiftlw, /**< IA-32/AMD64 AVX-512 kshiftlw opcode. */
    /* 1147 */ OP_kshiftlb, /**< IA-32/AMD64 AVX-512 kshiftlb opcode. */
    /* 1148 */ OP_kshiftlq, /**< IA-32/AMD64 AVX-512 kshiftlq opcode. */
    /* 1149 */ OP_kshiftld, /**< IA-32/AMD64 AVX-512 kshiftld opcode. */
    /* 1150 */ OP_kshiftrw, /**< IA-32/AMD64 AVX-512 kshiftrw opcode. */
    /* 1151 */ OP_kshiftrb, /**< IA-32/AMD64 AVX-512 kshiftrb opcode. */
    /* 1152 */ OP_kshiftrq, /**< IA-32/AMD64 AVX-512 kshiftrq opcode. */
    /* 1153 */ OP_kshiftrd, /**< IA-32/AMD64 AVX-512 kshiftrd opcode. */
    /* 1154 */ OP_ktestw,   /**< IA-32/AMD64 AVX-512 ktestd opcode. */
    /* 1155 */ OP_ktestb,   /**< IA-32/AMD64 AVX-512 ktestd opcode. */
    /* 1156 */ OP_ktestq,   /**< IA-32/AMD64 AVX-512 ktestd opcode. */
    /* 1157 */ OP_ktestd,   /**< IA-32/AMD64 AVX-512 ktestd opcode. */

    /* Intel AVX-512 EVEX */
    /* XXX i#1312: The opcode enum numbers here are changing as long as
     * AVX-512 instructions are being added to DynamoRIO. Users are advised
     * not to rely on the numerical value until i#1312 has been completed.
     */
    /* 1158 */ OP_valignd,         /**< IA-32/AMD64 AVX-512 OP_valignd opcode. */
    /* 1159 */ OP_valignq,         /**< IA-32/AMD64 AVX-512 OP_valignq opcode. */
    /* 1160 */ OP_vblendmpd,       /**< IA-32/AMD64 AVX-512 OP_vblendmpd opcode. */
    /* 1161 */ OP_vblendmps,       /**< IA-32/AMD64 AVX-512 OP_vblendmps opcode. */
    /* 1162 */ OP_vbroadcastf32x2, /**< IA-32/AMD64 AVX-512 OP_vbroadcastf32x2 opcode. */
    /* 1163 */ OP_vbroadcastf32x4, /**< IA-32/AMD64 AVX-512 OP_vbroadcastf32x4 opcode. */
    /* 1164 */ OP_vbroadcastf32x8, /**< IA-32/AMD64 AVX-512 OP_vbroadcastf32x8 opcode. */
    /* 1165 */ OP_vbroadcastf64x2, /**< IA-32/AMD64 AVX-512 OP_vbroadcastf64x2 opcode. */
    /* 1166 */ OP_vbroadcastf64x4, /**< IA-32/AMD64 AVX-512 OP_vbroadcastf64x4 opcode. */
    /* 1167 */ OP_vbroadcasti32x2, /**< IA-32/AMD64 AVX-512 OP_vbroadcasti32x2 opcode. */
    /* 1168 */ OP_vbroadcasti32x4, /**< IA-32/AMD64 AVX-512 OP_vbroadcasti32x4 opcode. */
    /* 1169 */ OP_vbroadcasti32x8, /**< IA-32/AMD64 AVX-512 OP_vbroadcasti32x8 opcode. */
    /* 1170 */ OP_vbroadcasti64x2, /**< IA-32/AMD64 AVX-512 OP_vbroadcasti64x2 opcode. */
    /* 1171 */ OP_vbroadcasti64x4, /**< IA-32/AMD64 AVX-512 OP_vbroadcasti64x4 opcode. */
    /* 1172 */ OP_vcompresspd,     /**< IA-32/AMD64 AVX-512 OP_vcompresspd opcode. */
    /* 1173 */ OP_vcompressps,     /**< IA-32/AMD64 AVX-512 OP_vcompressps opcode. */
    /* 1174 */ OP_vcvtpd2qq,       /**< IA-32/AMD64 AVX-512 OP_vcvtpd2qq opcode. */
    /* 1175 */ OP_vcvtpd2udq,      /**< IA-32/AMD64 AVX-512 OP_vcvtpd2udq opcode. */
    /* 1176 */ OP_vcvtpd2uqq,      /**< IA-32/AMD64 AVX-512 OP_vcvtpd2uqq opcode. */
    /* 1177 */ OP_vcvtps2qq,       /**< IA-32/AMD64 AVX-512 OP_vcvtps2qq opcode. */
    /* 1178 */ OP_vcvtps2udq,      /**< IA-32/AMD64 AVX-512 OP_vcvtps2udq opcode. */
    /* 1179 */ OP_vcvtps2uqq,      /**< IA-32/AMD64 AVX-512 OP_vcvtps2uqq opcode. */
    /* 1180 */ OP_vcvtqq2pd,       /**< IA-32/AMD64 AVX-512 OP_vcvtqq2pd opcode. */
    /* 1181 */ OP_vcvtqq2ps,       /**< IA-32/AMD64 AVX-512 OP_vcvtqq2ps opcode. */
    /* 1182 */ OP_vcvtsd2usi,      /**< IA-32/AMD64 AVX-512 OP_vcvtsd2usi opcode. */
    /* 1183 */ OP_vcvtss2usi,      /**< IA-32/AMD64 AVX-512 OP_vcvtss2usi opcode. */
    /* 1184 */ OP_vcvttpd2qq,      /**< IA-32/AMD64 AVX-512 OP_vcvttpd2qq opcode. */
    /* 1185 */ OP_vcvttpd2udq,     /**< IA-32/AMD64 AVX-512 OP_vcvttpd2udq opcode. */
    /* 1186 */ OP_vcvttpd2uqq,     /**< IA-32/AMD64 AVX-512 OP_vcvttpd2uqq opcode. */
    /* 1187 */ OP_vcvttps2qq,      /**< IA-32/AMD64 AVX-512 OP_vcvttps2qq opcode. */
    /* 1188 */ OP_vcvttps2udq,     /**< IA-32/AMD64 AVX-512 OP_vcvttps2udq opcode. */
    /* 1189 */ OP_vcvttps2uqq,     /**< IA-32/AMD64 AVX-512 OP_vcvttps2uqq opcode. */
    /* 1190 */ OP_vcvttsd2usi,     /**< IA-32/AMD64 AVX-512 OP_vcvttsd2usi opcode. */
    /* 1191 */ OP_vcvttss2usi,     /**< IA-32/AMD64 AVX-512 OP_vcvttss2usi opcode. */
    /* 1192 */ OP_vcvtudq2pd,      /**< IA-32/AMD64 AVX-512 OP_vcvtudq2pd opcode. */
    /* 1193 */ OP_vcvtudq2ps,      /**< IA-32/AMD64 AVX-512 OP_vcvtudq2ps opcode. */
    /* 1194 */ OP_vcvtuqq2pd,      /**< IA-32/AMD64 AVX-512 OP_vcvtuqq2pd opcode. */
    /* 1195 */ OP_vcvtuqq2ps,      /**< IA-32/AMD64 AVX-512 OP_vcvtuqq2ps opcode. */
    /* 1196 */ OP_vcvtusi2sd,      /**< IA-32/AMD64 AVX-512 OP_vcvtusi2sd opcode. */
    /* 1197 */ OP_vcvtusi2ss,      /**< IA-32/AMD64 AVX-512 OP_vcvtusi2ss opcode. */
    /* 1198 */ OP_vdbpsadbw,       /**< IA-32/AMD64 AVX-512 OP_vdbpsadbw opcode. */
    /* 1199 */ OP_vexp2pd,         /**< IA-32/AMD64 AVX-512 OP_vexp2pd opcode. */
    /* 1200 */ OP_vexp2ps,         /**< IA-32/AMD64 AVX-512 OP_vexp2ps opcode. */
    /* 1201 */ OP_vexpandpd,       /**< IA-32/AMD64 AVX-512 OP_vexpandpd opcode. */
    /* 1202 */ OP_vexpandps,       /**< IA-32/AMD64 AVX-512 OP_vexpandps opcode. */
    /* 1203 */ OP_vextractf32x4,   /**< IA-32/AMD64 AVX-512 OP_vextractf32x4 opcode. */
    /* 1204 */ OP_vextractf32x8,   /**< IA-32/AMD64 AVX-512 OP_vextractf32x8 opcode. */
    /* 1205 */ OP_vextractf64x2,   /**< IA-32/AMD64 AVX-512 OP_vextractf64x2 opcode. */
    /* 1206 */ OP_vextractf64x4,   /**< IA-32/AMD64 AVX-512 OP_vextractf64x4 opcode. */
    /* 1207 */ OP_vextracti32x4,   /**< IA-32/AMD64 AVX-512 OP_vextracti32x4 opcode. */
    /* 1208 */ OP_vextracti32x8,   /**< IA-32/AMD64 AVX-512 OP_vextracti32x8 opcode. */
    /* 1209 */ OP_vextracti64x2,   /**< IA-32/AMD64 AVX-512 OP_vextracti64x2 opcode. */
    /* 1210 */ OP_vextracti64x4,   /**< IA-32/AMD64 AVX-512 OP_vextracti64x4 opcode. */
    /* 1211 */ OP_vfixupimmpd,     /**< IA-32/AMD64 AVX-512 OP_vfixupimmpd opcode. */
    /* 1212 */ OP_vfixupimmps,     /**< IA-32/AMD64 AVX-512 OP_vfixupimmps opcode. */
    /* 1213 */ OP_vfixupimmsd,     /**< IA-32/AMD64 AVX-512 OP_vfixupimmsd opcode. */
    /* 1214 */ OP_vfixupimmss,     /**< IA-32/AMD64 AVX-512 OP_vfixupimmss opcode. */
    /* 1215 */ OP_vfpclasspd,      /**< IA-32/AMD64 AVX-512 OP_vfpclasspd opcode. */
    /* 1216 */ OP_vfpclassps,      /**< IA-32/AMD64 AVX-512 OP_vfpclassps opcode. */
    /* 1217 */ OP_vfpclasssd,      /**< IA-32/AMD64 AVX-512 OP_vfpclasssd opcode. */
    /* 1218 */ OP_vfpclassss,      /**< IA-32/AMD64 AVX-512 OP_vfpclassss opcode. */
    /* 1219 */ OP_vgatherpf0dpd,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf0dps opcode. */
    /* 1220 */ OP_vgatherpf0dps,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf0dps opcode. */
    /* 1221 */ OP_vgatherpf0qpd,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf0qpd opcode. */
    /* 1222 */ OP_vgatherpf0qps,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf0qps opcode. */
    /* 1223 */ OP_vgatherpf1dpd,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf1dpd opcode. */
    /* 1224 */ OP_vgatherpf1dps,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf1dps opcode. */
    /* 1225 */ OP_vgatherpf1qpd,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf1qpd opcode. */
    /* 1226 */ OP_vgatherpf1qps,   /**< IA-32/AMD64 AVX-512 OP_vgatherpf1qps opcode. */
    /* 1227 */ OP_vgetexppd,       /**< IA-32/AMD64 AVX-512 OP_vgetexppd opcode. */
    /* 1228 */ OP_vgetexpps,       /**< IA-32/AMD64 AVX-512 OP_vgetexpps opcode. */
    /* 1229 */ OP_vgetexpsd,       /**< IA-32/AMD64 AVX-512 OP_vgetexpsd opcode. */
    /* 1230 */ OP_vgetexpss,       /**< IA-32/AMD64 AVX-512 OP_vgetexpss opcode. */
    /* 1231 */ OP_vgetmantpd,      /**< IA-32/AMD64 AVX-512 OP_vgetmantpd opcode. */
    /* 1232 */ OP_vgetmantps,      /**< IA-32/AMD64 AVX-512 OP_vgetmantps opcode. */
    /* 1233 */ OP_vgetmantsd,      /**< IA-32/AMD64 AVX-512 OP_vgetmantsd opcode. */
    /* 1234 */ OP_vgetmantss,      /**< IA-32/AMD64 AVX-512 OP_vgetmantss opcode. */
    /* 1235 */ OP_vinsertf32x4,    /**< IA-32/AMD64 AVX-512 OP_vinsertf32x4 opcode. */
    /* 1236 */ OP_vinsertf32x8,    /**< IA-32/AMD64 AVX-512 OP_vinsertf32x8 opcode. */
    /* 1237 */ OP_vinsertf64x2,    /**< IA-32/AMD64 AVX-512 OP_vinsertf64x2 opcode. */
    /* 1238 */ OP_vinsertf64x4,    /**< IA-32/AMD64 AVX-512 OP_vinsertf64x4 opcode. */
    /* 1239 */ OP_vinserti32x4,    /**< IA-32/AMD64 AVX-512 OP_vinserti32x4 opcode. */
    /* 1240 */ OP_vinserti32x8,    /**< IA-32/AMD64 AVX-512 OP_vinserti32x8 opcode. */
    /* 1241 */ OP_vinserti64x2,    /**< IA-32/AMD64 AVX-512 OP_vinserti64x2 opcode. */
    /* 1242 */ OP_vinserti64x4,    /**< IA-32/AMD64 AVX-512 OP_vinserti64x4 opcode. */
    /* 1243 */ OP_vmovdqa32,       /**< IA-32/AMD64 AVX-512 OP_vmovdqa32 opcode. */
    /* 1244 */ OP_vmovdqa64,       /**< IA-32/AMD64 AVX-512 OP_vmovdqa64 opcode. */
    /* 1245 */ OP_vmovdqu16,       /**< IA-32/AMD64 AVX-512 OP_vmovdqu16 opcode. */
    /* 1246 */ OP_vmovdqu32,       /**< IA-32/AMD64 AVX-512 OP_vmovdqu32 opcode. */
    /* 1247 */ OP_vmovdqu64,       /**< IA-32/AMD64 AVX-512 OP_vmovdqu64 opcode. */
    /* 1248 */ OP_vmovdqu8,        /**< IA-32/AMD64 AVX-512 OP_vmovdqu8 opcode. */
    /* 1249 */ OP_vpabsq,          /**< IA-32/AMD64 AVX-512 OP_vpabsq opcode. */
    /* 1250 */ OP_vpandd,          /**< IA-32/AMD64 AVX-512 OP_vpandd opcode. */
    /* 1251 */ OP_vpandnd,         /**< IA-32/AMD64 AVX-512 OP_vpandnd opcode. */
    /* 1252 */ OP_vpandnq,         /**< IA-32/AMD64 AVX-512 OP_vpandnq opcode. */
    /* 1253 */ OP_vpandq,          /**< IA-32/AMD64 AVX-512 OP_vpandq opcode. */
    /* 1254 */ OP_vpblendmb,       /**< IA-32/AMD64 AVX-512 OP_vpblendmb opcode. */
    /* 1255 */ OP_vpblendmd,       /**< IA-32/AMD64 AVX-512 OP_vpblendmd opcode. */
    /* 1256 */ OP_vpblendmq,       /**< IA-32/AMD64 AVX-512 OP_vpblendmq opcode. */
    /* 1257 */ OP_vpblendmw,       /**< IA-32/AMD64 AVX-512 OP_vpblendmw opcode. */
    /* 1258 */ OP_vpbroadcastmb2q, /**< IA-32/AMD64 AVX-512 OP_vpbroadcastmb2q opcode. */
    /* 1259 */ OP_vpbroadcastmw2d, /**< IA-32/AMD64 AVX-512 OP_vpbroadcastmw2d opcode. */
    /* 1260 */ OP_vpcmpb,          /**< IA-32/AMD64 AVX-512 OP_vpcmpb opcode. */
    /* 1261 */ OP_vpcmpd,          /**< IA-32/AMD64 AVX-512 OP_vpcmpd opcode. */
    /* 1262 */ OP_vpcmpq,          /**< IA-32/AMD64 AVX-512 OP_vpcmpq opcode. */
    /* 1263 */ OP_vpcmpub,         /**< IA-32/AMD64 AVX-512 OP_vpcmpub opcode. */
    /* 1264 */ OP_vpcmpud,         /**< IA-32/AMD64 AVX-512 OP_vpcmpud opcode. */
    /* 1265 */ OP_vpcmpuq,         /**< IA-32/AMD64 AVX-512 OP_vpcmpuq opcode. */
    /* 1266 */ OP_vpcmpuw,         /**< IA-32/AMD64 AVX-512 OP_vpcmpuw opcode. */
    /* 1267 */ OP_vpcmpw,          /**< IA-32/AMD64 AVX-512 OP_vpcmpw opcode. */
    /* 1268 */ OP_vpcompressd,     /**< IA-32/AMD64 AVX-512 OP_vpcompressd opcode. */
    /* 1269 */ OP_vpcompressq,     /**< IA-32/AMD64 AVX-512 OP_vpcompressq opcode. */
    /* 1270 */ OP_vpconflictd,     /**< IA-32/AMD64 AVX-512 OP_vpconflictd opcode. */
    /* 1271 */ OP_vpconflictq,     /**< IA-32/AMD64 AVX-512 OP_vpconflictq opcode. */
    /* 1272 */ OP_vpermb,          /**< IA-32/AMD64 AVX-512 OP_vpermb opcode. */
    /* 1273 */ OP_vpermi2b,        /**< IA-32/AMD64 AVX-512 OP_vpermi2b opcode. */
    /* 1274 */ OP_vpermi2d,        /**< IA-32/AMD64 AVX-512 OP_vpermi2d opcode. */
    /* 1275 */ OP_vpermi2pd,       /**< IA-32/AMD64 AVX-512 OP_vpermi2pd opcode. */
    /* 1276 */ OP_vpermi2ps,       /**< IA-32/AMD64 AVX-512 OP_vpermi2ps opcode. */
    /* 1277 */ OP_vpermi2q,        /**< IA-32/AMD64 AVX-512 OP_vpermi2q opcode. */
    /* 1278 */ OP_vpermi2w,        /**< IA-32/AMD64 AVX-512 OP_vpermi2w opcode. */
    /* 1279 */ OP_vpermt2b,        /**< IA-32/AMD64 AVX-512 OP_vpermt2b opcode. */
    /* 1280 */ OP_vpermt2d,        /**< IA-32/AMD64 AVX-512 OP_vpermt2d opcode. */
    /* 1281 */ OP_vpermt2pd,       /**< IA-32/AMD64 AVX-512 OP_vpermt2pd opcode. */
    /* 1282 */ OP_vpermt2ps,       /**< IA-32/AMD64 AVX-512 OP_vpermt2ps opcode. */
    /* 1283 */ OP_vpermt2q,        /**< IA-32/AMD64 AVX-512 OP_vpermt2q opcode. */
    /* 1284 */ OP_vpermt2w,        /**< IA-32/AMD64 AVX-512 OP_vpermt2w opcode. */
    /* 1285 */ OP_vpermw,          /**< IA-32/AMD64 AVX-512 OP_vpermw opcode. */
    /* 1286 */ OP_vpexpandd,       /**< IA-32/AMD64 AVX-512 OP_vpexpandd opcode. */
    /* 1287 */ OP_vpexpandq,       /**< IA-32/AMD64 AVX-512 OP_vpexpandq opcode. */
    /* 1288 */ OP_vpextrq,         /**< IA-32/AMD64 AVX-512 OP_vpextrq opcode. */
    /* 1289 */ OP_vpinsrq,         /**< IA-32/AMD64 AVX-512 OP_vpinsrq opcode. */
    /* 1290 */ OP_vplzcntd,        /**< IA-32/AMD64 AVX-512 OP_vplzcntd opcode. */
    /* 1291 */ OP_vplzcntq,        /**< IA-32/AMD64 AVX-512 OP_vplzcntq opcode. */
    /* 1292 */ OP_vpmadd52huq,     /**< IA-32/AMD64 AVX-512 OP_vpmadd52huq opcode. */
    /* 1293 */ OP_vpmadd52luq,     /**< IA-32/AMD64 AVX-512 OP_vpmadd52luq opcode. */
    /* 1294 */ OP_vpmaxsq,         /**< IA-32/AMD64 AVX-512 OP_vpmaxsq opcode. */
    /* 1295 */ OP_vpmaxuq,         /**< IA-32/AMD64 AVX-512 OP_vpmaxuq opcode. */
    /* 1296 */ OP_vpminsq,         /**< IA-32/AMD64 AVX-512 OP_vpminsq opcode. */
    /* 1297 */ OP_vpminuq,         /**< IA-32/AMD64 AVX-512 OP_vpminuq opcode. */
    /* 1298 */ OP_vpmovb2m,        /**< IA-32/AMD64 AVX-512 OP_vpmovb2m opcode. */
    /* 1299 */ OP_vpmovd2m,        /**< IA-32/AMD64 AVX-512 OP_vpmovd2m opcode. */
    /* 1300 */ OP_vpmovdb,         /**< IA-32/AMD64 AVX-512 OP_vpmovdb opcode. */
    /* 1301 */ OP_vpmovdw,         /**< IA-32/AMD64 AVX-512 OP_vpmovdw opcode. */
    /* 1302 */ OP_vpmovm2b,        /**< IA-32/AMD64 AVX-512 OP_vpmovm2b opcode. */
    /* 1303 */ OP_vpmovm2d,        /**< IA-32/AMD64 AVX-512 OP_vpmovm2d opcode. */
    /* 1304 */ OP_vpmovm2q,        /**< IA-32/AMD64 AVX-512 OP_vpmovm2q opcode. */
    /* 1305 */ OP_vpmovm2w,        /**< IA-32/AMD64 AVX-512 OP_vpmovm2w opcode. */
    /* 1306 */ OP_vpmovq2m,        /**< IA-32/AMD64 AVX-512 OP_vpmovq2m opcode. */
    /* 1307 */ OP_vpmovqb,         /**< IA-32/AMD64 AVX-512 OP_vpmovqb opcode. */
    /* 1308 */ OP_vpmovqd,         /**< IA-32/AMD64 AVX-512 OP_vpmovqd opcode. */
    /* 1309 */ OP_vpmovqw,         /**< IA-32/AMD64 AVX-512 OP_vpmovqw opcode. */
    /* 1310 */ OP_vpmovsdb,        /**< IA-32/AMD64 AVX-512 OP_vpmovsdb opcode. */
    /* 1311 */ OP_vpmovsdw,        /**< IA-32/AMD64 AVX-512 OP_vpmovsdw opcode. */
    /* 1312 */ OP_vpmovsqb,        /**< IA-32/AMD64 AVX-512 OP_vpmovsqb opcode. */
    /* 1313 */ OP_vpmovsqd,        /**< IA-32/AMD64 AVX-512 OP_vpmovsqd opcode. */
    /* 1314 */ OP_vpmovsqw,        /**< IA-32/AMD64 AVX-512 OP_vpmovsqw opcode. */
    /* 1315 */ OP_vpmovswb,        /**< IA-32/AMD64 AVX-512 OP_vpmovswb opcode. */
    /* 1316 */ OP_vpmovusdb,       /**< IA-32/AMD64 AVX-512 OP_vpmovusdb opcode. */
    /* 1317 */ OP_vpmovusdw,       /**< IA-32/AMD64 AVX-512 OP_vpmovusdw opcode. */
    /* 1318 */ OP_vpmovusqb,       /**< IA-32/AMD64 AVX-512 OP_vpmovusqb opcode. */
    /* 1319 */ OP_vpmovusqd,       /**< IA-32/AMD64 AVX-512 OP_vpmovusqd opcode. */
    /* 1320 */ OP_vpmovusqw,       /**< IA-32/AMD64 AVX-512 OP_vpmovusqw opcode. */
    /* 1321 */ OP_vpmovuswb,       /**< IA-32/AMD64 AVX-512 OP_vpmovuswb opcode. */
    /* 1322 */ OP_vpmovw2m,        /**< IA-32/AMD64 AVX-512 OP_vpmovw2m opcode. */
    /* 1323 */ OP_vpmovwb,         /**< IA-32/AMD64 AVX-512 OP_vpmovwb opcode. */
    /* 1324 */ OP_vpmullq,         /**< IA-32/AMD64 AVX-512 OP_vpmullq opcode. */
    /* 1325 */ OP_vpord,           /**< IA-32/AMD64 AVX-512 OP_vpord opcode. */
    /* 1326 */ OP_vporq,           /**< IA-32/AMD64 AVX-512 OP_vporq opcode. */
    /* 1327 */ OP_vprold,          /**< IA-32/AMD64 AVX-512 OP_vprold opcode. */
    /* 1328 */ OP_vprolq,          /**< IA-32/AMD64 AVX-512 OP_vprolq opcode. */
    /* 1329 */ OP_vprolvd,         /**< IA-32/AMD64 AVX-512 OP_vprolvd opcode. */
    /* 1330 */ OP_vprolvq,         /**< IA-32/AMD64 AVX-512 OP_vprolvq opcode. */
    /* 1331 */ OP_vprord,          /**< IA-32/AMD64 AVX-512 OP_vprord opcode. */
    /* 1332 */ OP_vprorq,          /**< IA-32/AMD64 AVX-512 OP_vprorq opcode. */
    /* 1333 */ OP_vprorvd,         /**< IA-32/AMD64 AVX-512 OP_vprorvd opcode. */
    /* 1334 */ OP_vprorvq,         /**< IA-32/AMD64 AVX-512 OP_vprorvq opcode. */
    /* 1335 */ OP_vpscatterdd,     /**< IA-32/AMD64 AVX-512 OP_vpscatterdd opcode. */
    /* 1336 */ OP_vpscatterdq,     /**< IA-32/AMD64 AVX-512 OP_vpscatterdq opcode. */
    /* 1337 */ OP_vpscatterqd,     /**< IA-32/AMD64 AVX-512 OP_vpscatterqd opcode. */
    /* 1338 */ OP_vpscatterqq,     /**< IA-32/AMD64 AVX-512 OP_vpscatterqq opcode. */
    /* 1339 */ OP_vpsllvw,         /**< IA-32/AMD64 AVX-512 OP_vpsllvw opcode. */
    /* 1340 */ OP_vpsraq,          /**< IA-32/AMD64 AVX-512 OP_vpsraq opcode. */
    /* 1341 */ OP_vpsravq,         /**< IA-32/AMD64 AVX-512 OP_vpsravq opcode. */
    /* 1342 */ OP_vpsravw,         /**< IA-32/AMD64 AVX-512 OP_vpsravw opcode. */
    /* 1343 */ OP_vpsrlvw,         /**< IA-32/AMD64 AVX-512 OP_vpsrlvw opcode. */
    /* 1344 */ OP_vpternlogd,      /**< IA-32/AMD64 AVX-512 OP_vpternlogd opcode. */
    /* 1345 */ OP_vpternlogq,      /**< IA-32/AMD64 AVX-512 OP_vpternlogd opcode. */
    /* 1346 */ OP_vptestmb,        /**< IA-32/AMD64 AVX-512 OP_vptestmb opcode. */
    /* 1347 */ OP_vptestmd,        /**< IA-32/AMD64 AVX-512 OP_vptestmd opcode. */
    /* 1348 */ OP_vptestmq,        /**< IA-32/AMD64 AVX-512 OP_vptestmq opcode. */
    /* 1349 */ OP_vptestmw,        /**< IA-32/AMD64 AVX-512 OP_vptestmw opcode. */
    /* 1350 */ OP_vptestnmb,       /**< IA-32/AMD64 AVX-512 OP_vptestnmb opcode. */
    /* 1351 */ OP_vptestnmd,       /**< IA-32/AMD64 AVX-512 OP_vptestnmd opcode. */
    /* 1352 */ OP_vptestnmq,       /**< IA-32/AMD64 AVX-512 OP_vptestnmq opcode. */
    /* 1353 */ OP_vptestnmw,       /**< IA-32/AMD64 AVX-512 OP_vptestnmw opcode. */
    /* 1354 */ OP_vpxord,          /**< IA-32/AMD64 AVX-512 OP_vpxordvpxord opcode. */
    /* 1355 */ OP_vpxorq,          /**< IA-32/AMD64 AVX-512 OP_vpxorq opcode. */
    /* 1356 */ OP_vrangepd,        /**< IA-32/AMD64 AVX-512 OP_vrangepd opcode. */
    /* 1357 */ OP_vrangeps,        /**< IA-32/AMD64 AVX-512 OP_vrangeps opcode. */
    /* 1358 */ OP_vrangesd,        /**< IA-32/AMD64 AVX-512 OP_vrangesd opcode. */
    /* 1359 */ OP_vrangess,        /**< IA-32/AMD64 AVX-512 OP_vrangess opcode. */
    /* 1360 */ OP_vrcp14pd,        /**< IA-32/AMD64 AVX-512 OP_vrcp14pd opcode. */
    /* 1361 */ OP_vrcp14ps,        /**< IA-32/AMD64 AVX-512 OP_vrcp14ps opcode. */
    /* 1362 */ OP_vrcp14sd,        /**< IA-32/AMD64 AVX-512 OP_vrcp14sd opcode. */
    /* 1363 */ OP_vrcp14ss,        /**< IA-32/AMD64 AVX-512 OP_vrcp14ss opcode. */
    /* 1364 */ OP_vrcp28pd,        /**< IA-32/AMD64 AVX-512 OP_vrcp28pd opcode. */
    /* 1365 */ OP_vrcp28ps,        /**< IA-32/AMD64 AVX-512 OP_vrcp28ps opcode. */
    /* 1366 */ OP_vrcp28sd,        /**< IA-32/AMD64 AVX-512 OP_vrcp28sd opcode. */
    /* 1367 */ OP_vrcp28ss,        /**< IA-32/AMD64 AVX-512 OP_vrcp28ss opcode. */
    /* 1368 */ OP_vreducepd,       /**< IA-32/AMD64 AVX-512 OP_vreducepd opcode. */
    /* 1369 */ OP_vreduceps,       /**< IA-32/AMD64 AVX-512 OP_vreduceps opcode. */
    /* 1370 */ OP_vreducesd,       /**< IA-32/AMD64 AVX-512 OP_vreducesd opcode. */
    /* 1371 */ OP_vreducess,       /**< IA-32/AMD64 AVX-512 OP_vreducess opcode. */
    /* 1372 */ OP_vrndscalepd,     /**< IA-32/AMD64 AVX-512 OP_vrndscalepd opcode. */
    /* 1373 */ OP_vrndscaleps,     /**< IA-32/AMD64 AVX-512 OP_vrndscaleps opcode. */
    /* 1374 */ OP_vrndscalesd,     /**< IA-32/AMD64 AVX-512 OP_vrndscalesd opcode. */
    /* 1375 */ OP_vrndscaless,     /**< IA-32/AMD64 AVX-512 OP_vrndscaless opcode. */
    /* 1376 */ OP_vrsqrt14pd,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt14pd opcode. */
    /* 1377 */ OP_vrsqrt14ps,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt14ps opcode. */
    /* 1378 */ OP_vrsqrt14sd,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt14sd opcode. */
    /* 1379 */ OP_vrsqrt14ss,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt14ss opcode. */
    /* 1380 */ OP_vrsqrt28pd,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt28pd opcode. */
    /* 1381 */ OP_vrsqrt28ps,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt28ps opcode. */
    /* 1382 */ OP_vrsqrt28sd,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt28sd opcode. */
    /* 1383 */ OP_vrsqrt28ss,      /**< IA-32/AMD64 AVX-512 OP_vrsqrt28ss opcode. */
    /* 1384 */ OP_vscalefpd,       /**< IA-32/AMD64 AVX-512 OP_vscalepd opcode. */
    /* 1385 */ OP_vscalefps,       /**< IA-32/AMD64 AVX-512 OP_vscaleps opcode. */
    /* 1386 */ OP_vscalefsd,       /**< IA-32/AMD64 AVX-512 OP_vscalesd opcode. */
    /* 1387 */ OP_vscalefss,       /**< IA-32/AMD64 AVX-512 OP_vscalesss opcode. */
    /* 1388 */ OP_vscatterdpd,     /**< IA-32/AMD64 AVX-512 OP_vscatterdpd opcode. */
    /* 1389 */ OP_vscatterdps,     /**< IA-32/AMD64 AVX-512 OP_vscatterdps opcode. */
    /* 1390 */ OP_vscatterqpd,     /**< IA-32/AMD64 AVX-512 OP_vscatterqpd opcode. */
    /* 1391 */ OP_vscatterqps,     /**< IA-32/AMD64 AVX-512 OP_vscatterqps opcode. */
    /* 1392 */ OP_vscatterpf0dpd,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf0dpd opcode. */
    /* 1393 */ OP_vscatterpf0dps,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf0dps opcode. */
    /* 1394 */ OP_vscatterpf0qpd,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf0qpd opcode. */
    /* 1395 */ OP_vscatterpf0qps,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf0qps opcode. */
    /* 1396 */ OP_vscatterpf1dpd,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf1dpd opcode. */
    /* 1397 */ OP_vscatterpf1dps,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf1dps opcode. */
    /* 1398 */ OP_vscatterpf1qpd,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf1qpd opcode. */
    /* 1399 */ OP_vscatterpf1qps,  /**< IA-32/AMD64 AVX-512 OP_vscatterpf1qps opcode. */
    /* 1400 */ OP_vshuff32x4,      /**< IA-32/AMD64 AVX-512 OP_vshuff32x4 opcode. */
    /* 1401 */ OP_vshuff64x2,      /**< IA-32/AMD64 AVX-512 OP_vshuff64x2 opcode. */
    /* 1402 */ OP_vshufi32x4,      /**< IA-32/AMD64 AVX-512 OP_vshufi32x4 opcode. */
    /* 1403 */ OP_vshufi64x2,      /**< IA-32/AMD64 AVX-512 OP_vshufi64x2 opcode. */

    /* Intel SHA extensions */
    /* 1404 */ OP_sha1msg1,    /**< IA-32/AMD64 SHA OP_sha1msg1 opcode. */
    /* 1405 */ OP_sha1msg2,    /**< IA-32/AMD64 SHA OP_sha1msg2 opcode. */
    /* 1406 */ OP_sha1nexte,   /**< IA-32/AMD64 SHA OP_sha1nexte opcode. */
    /* 1407 */ OP_sha1rnds4,   /**< IA-32/AMD64 SHA OP_sha1rnds4 opcode. */
    /* 1408 */ OP_sha256msg1,  /**< IA-32/AMD64 SHA OP_sha2msg1 opcode. */
    /* 1409 */ OP_sha256msg2,  /**< IA-32/AMD64 SHA OP_sha2msg2 opcode. */
    /* 1410 */ OP_sha256rnds2, /**< IA-32/AMD64 SHA OP_sha2rnds2 opcode. */

    /* Intel MPX extensions */
    /* 1411 */ OP_bndcl,  /**< IA-32/AMD64 MPX OP_bndcl opcode. */
    /* 1412 */ OP_bndcn,  /**< IA-32/AMD64 MPX OP_bndcn opcode. */
    /* 1413 */ OP_bndcu,  /**< IA-32/AMD64 MPX OP_bndcu opcode. */
    /* 1414 */ OP_bndldx, /**< IA-32/AMD64 MPX OP_bndldx opcode. */
    /* 1415 */ OP_bndmk,  /**< IA-32/AMD64 MPX OP_bndmk opcode. */
    /* 1416 */ OP_bndmov, /**< IA-32/AMD64 MPX OP_bndmov opcode. */
    /* 1417 */ OP_bndstx, /**< IA-32/AMD64 MPX OP_bndstx opcode. */

    /* Intel PT extensions */
    /* 1418 */ OP_ptwrite, /**< IA-32/AMD64 PT OP_ptwrite opcode. */

    /* AMD monitor extensions */
    /* 1419 */ OP_monitorx, /**< AMD64 monitorx opcode. */
    /* 1420 */ OP_mwaitx,   /**< AMD64 mwaitx opcode. */

    /* Intel MPK extensions */
    /* 1421 */ OP_rdpkru, /**< IA-32/AMD64 MPK rdpkru opcode. */
    /* 1422 */ OP_wrpkru, /**< IA-32/AMD64 MPK wrpkru opcode. */

    /* Intel Software Guard eXtension. */
    /* 1423 */ OP_encls, /**< IA-32/AMD64 SGX encls opcode. */
    /* 1424 */ OP_enclu, /**< IA-32/AMD64 SGX enclu opcode. */
    /* 1425 */ OP_enclv, /**< IA-32/AMD64 SGX enclv opcode. */

    /* AVX512 VNNI */
    /* 1426 */ OP_vpdpbusd,  /**< IA-32/AMD64 vpdpbusd opcode. */
    /* 1427 */ OP_vpdpbusds, /**< IA-32/AMD64 vpdpbusds opcode. */
    /* 1428 */ OP_vpdpwssd,  /**< IA-32/AMD64 vpdpwssd opcode. */
    /* 1429 */ OP_vpdpwssds, /**< IA-32/AMD64 vpdpwssds opcode. */

    /* AVX512 BF16 */
    /* 1430 */ OP_vcvtne2ps2bf16, /**< IA-32/AMD64 vcvtne2ps2bf16 opcode. */
    /* 1431 */ OP_vcvtneps2bf16,  /**< IA-32/AMD64 vcvtneps2bf16 opcode. */
    /* 1432 */ OP_vdpbf16ps,      /**< IA-32/AMD64 vdpbf16ps opcode. */

    /* AVX512 VPOPCNTDQ */
    /* 1433 */ OP_vpopcntd, /**< IA-32/AMD64 vpopcntd opcode. */
    /* 1434 */ OP_vpopcntq, /**< IA-32/AMD64 vpopcntd opcode. */

    OP_AFTER_LAST,
    OP_FIRST = OP_add,           /**< First real opcode. */
    OP_LAST = OP_AFTER_LAST - 1, /**< Last real opcode. */
};

/* alternative names */
/* we do not equate the fwait+op opcodes
 *   fstsw, fstcw, fstenv, finit, fclex
 * for us that has to be a sequence of instructions: a separate fwait
 */
/* XXX i#1307: we could add extra decode table layers to print the proper name
 * when we disassemble these, but it's not clear it's worth the effort.
 */
/* 16-bit versions that have different names */
#define OP_cbw OP_cwde   /**< Alternative opcode name for 16-bit version. */
#define OP_cwd OP_cdq    /**< Alternative opcode name for 16-bit version. */
#define OP_jcxz OP_jecxz /**< Alternative opcode name for 16-bit version. */
/* 64-bit versions that have different names */
#define OP_cdqe OP_cwde            /**< Alternative opcode name for 64-bit version. */
#define OP_cqo OP_cdq              /**< Alternative opcode name for 64-bit version. */
#define OP_jrcxz OP_jecxz          /**< Alternative opcode name for 64-bit version. */
#define OP_cmpxchg16b OP_cmpxchg8b /**< Alternative opcode name for 64-bit version. */
#define OP_pextrq OP_pextrd        /**< Alternative opcode name for 64-bit version. */
#define OP_pinsrq OP_pinsrd        /**< Alternative opcode name for 64-bit version. */
#define OP_vpextrq OP_vpextrd      /**< Alternative opcode name for 64-bit version. */
#define OP_vpinsrq OP_vpinsrd      /**< Alternative opcode name for 64-bit version. */
/* reg-reg version has different name */
#define OP_movhlps OP_movlps   /**< Alternative opcode name for reg-reg version. */
#define OP_movlhps OP_movhps   /**< Alternative opcode name for reg-reg version. */
#define OP_vmovhlps OP_vmovlps /**< Alternative opcode name for reg-reg version. */
#define OP_vmovlhps OP_vmovhps /**< Alternative opcode name for reg-reg version. */
/* condition codes */
#define OP_jae_short OP_jnb_short /**< Alternative opcode name. */
#define OP_jnae_short OP_jb_short /**< Alternative opcode name. */
#define OP_ja_short OP_jnbe_short /**< Alternative opcode name. */
#define OP_jna_short OP_jbe_short /**< Alternative opcode name. */
#define OP_je_short OP_jz_short   /**< Alternative opcode name. */
#define OP_jne_short OP_jnz_short /**< Alternative opcode name. */
#define OP_jge_short OP_jnl_short /**< Alternative opcode name. */
#define OP_jg_short OP_jnle_short /**< Alternative opcode name. */
#define OP_jae OP_jnb             /**< Alternative opcode name. */
#define OP_jnae OP_jb             /**< Alternative opcode name. */
#define OP_ja OP_jnbe             /**< Alternative opcode name. */
#define OP_jna OP_jbe             /**< Alternative opcode name. */
#define OP_je OP_jz               /**< Alternative opcode name. */
#define OP_jne OP_jnz             /**< Alternative opcode name. */
#define OP_jge OP_jnl             /**< Alternative opcode name. */
#define OP_jg OP_jnle             /**< Alternative opcode name. */
#define OP_setae OP_setnb         /**< Alternative opcode name. */
#define OP_setnae OP_setb         /**< Alternative opcode name. */
#define OP_seta OP_setnbe         /**< Alternative opcode name. */
#define OP_setna OP_setbe         /**< Alternative opcode name. */
#define OP_sete OP_setz           /**< Alternative opcode name. */
#define OP_setne OP_setnz         /**< Alternative opcode name. */
#define OP_setge OP_setnl         /**< Alternative opcode name. */
#define OP_setg OP_setnle         /**< Alternative opcode name. */
#define OP_cmovae OP_cmovnb       /**< Alternative opcode name. */
#define OP_cmovnae OP_cmovb       /**< Alternative opcode name. */
#define OP_cmova OP_cmovnbe       /**< Alternative opcode name. */
#define OP_cmovna OP_cmovbe       /**< Alternative opcode name. */
#define OP_cmove OP_cmovz         /**< Alternative opcode name. */
#define OP_cmovne OP_cmovnz       /**< Alternative opcode name. */
#define OP_cmovge OP_cmovnl       /**< Alternative opcode name. */
#define OP_cmovg OP_cmovnle       /**< Alternative opcode name. */
#ifndef X64
#    define OP_fxsave OP_fxsave32     /**< Alternative opcode name. */
#    define OP_fxrstor OP_fxrstor32   /**< Alternative opcode name. */
#    define OP_xsave OP_xsave32       /**< Alternative opcode name. */
#    define OP_xrstor OP_xrstor32     /**< Alternative opcode name. */
#    define OP_xsaveopt OP_xsaveopt32 /**< Alternative opcode name. */
#    define OP_xsavec OP_xsavec32     /**< Alternative opcode name. */
#endif
#define OP_wait OP_fwait /**< Alternative opcode name. */
#define OP_sal OP_shl    /**< Alternative opcode name. */

#define OP_load OP_mov_ld  /**< Platform-independent opcode name for load. */
#define OP_store OP_mov_st /**< Platform-independent opcode name for store. */

/* undocumented opcodes */
#define OP_icebp OP_int1
#define OP_setalc OP_salc

/* Renamed opcodes. */
#define OP_ud2a OP_ud2 /**< Deprecated opcode name for ud2. */
#define OP_ud2b OP_ud1 /**< Deprecated opcode name for ud1. */

#endif /* _DR_IR_OPCODES_X86_H_ */
