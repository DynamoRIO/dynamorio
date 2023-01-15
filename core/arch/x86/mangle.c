/* ******************************************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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

/* file "mangle.c" */

#include "../globals.h"
#include "../link.h"
#include "../fragment.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "decode.h"
#include "decode_fast.h"
#include "decode_private.h"
#include "disassemble.h"
#include "../hashtable.h"
#include "instrument.h" /* for dr_insert_call */
#include "../fcache.h"  /* for in_fcache */
#include "../translate.h"

#ifdef RCT_IND_BRANCH
#    include "../rct.h" /* rct_add_rip_rel_addr */
#endif

#ifdef UNIX
#    include <sys/syscall.h>
#endif

#ifdef ANNOTATIONS
#    include "../annotations.h"
#endif

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE instrlist_meta_preinsert

/***************************************************************************/

void
mangle_arch_init(void)
{
    /* Nothing yet. */
}

int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save, byte *encode_pc)
{
    if (save) {
        /* We adjust the stack so the return address will not be clobbered,
         * so we can have call/return pair to take advantage of hardware
         * call return stack for better performance.
         * Xref emit_clean_call_save @ x86/emit_utils.c
         * The precise adjustment amount is relied upon in
         * find_next_fragment_from_gencode()'s handling of in_clean_call_save().
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(
                dcontext, opnd_create_reg(DR_REG_XSP),
                opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                      -(int)(get_clean_call_switch_stack_size() +
                                             get_clean_call_temp_stack_size()),
                                      OPSZ_lea)));
    }
    /* We document to clients that we use r11 if we need an indirect call here. */
    insert_reachable_cti(dcontext, ilist, instr, encode_pc,
                         save ? get_clean_call_save(dcontext _IF_X64(GENCODE_X64))
                              : get_clean_call_restore(dcontext _IF_X64(GENCODE_X64)),
                         false /*call*/, true /*returns*/, false /*!precise*/,
                         CALL_SCRATCH_REG, NULL);
    return get_clean_call_switch_stack_size();
}

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                    instr_t *instr)
{
    /* clear eflags for callee's usage */
    if (cci == NULL || !cci->skip_clear_flags) {
        if (dynamo_options.cleancall_ignore_eflags) {
            /* we still clear DF since some compiler assumes
             * DF is cleared at each function.
             */
            PRE(ilist, instr, INSTR_CREATE_cld(dcontext));
        } else {
            /* on x64 a push immed is sign-extended to 64-bit */
            PRE(ilist, instr, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));
            PRE(ilist, instr, INSTR_CREATE_popf(dcontext));
        }
    }
}

/* Pushes not only the GPRs but also xmm/ymm, xip, and xflags, in
 * priv_mcontext_t order.
 * The current stack pointer alignment should be passed.  Use 1 if
 * unknown (NOT 0).
 * Returns the amount of data pushed.  Does NOT fix up the xsp value pushed
 * to be the value prior to any pushes for x64 as no caller needs that
 * currently (they all build a priv_mcontext_t and have to do further xsp
 * fixups anyway).
 * Includes xmm0-5 for PR 264138.
 */
uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr, uint alignment,
                          opnd_t push_pc, reg_id_t scratch /*optional*/)
{
    uint dstack_offs = 0;
    int offs_beyond_xmm = 0;
    if (cci == NULL)
        cci = &default_clean_call_info;
    ASSERT(proc_num_simd_registers() == MCXT_NUM_SIMD_SLOTS ||
           proc_num_simd_registers() == MCXT_NUM_SIMD_SSE_AVX_SLOTS);
    if (clean_call_needs_simd(cci)) {
        int offs =
            MCXT_TOTAL_SIMD_SLOTS_SIZE + MCXT_TOTAL_OPMASK_SLOTS_SIZE + PRE_XMM_PADDING;
        if (cci->preserve_mcontext && cci->skip_save_flags) {
            offs_beyond_xmm = 2 * XSP_SZ; /* pc and flags */
            offs += offs_beyond_xmm;
        }
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -offs)));
        dstack_offs += offs;
    }

    /* pc and aflags */
    if (!cci->skip_save_flags) {
        ASSERT(offs_beyond_xmm == 0);
        if (opnd_is_immed_int(push_pc))
            PRE(ilist, instr, INSTR_CREATE_push_imm(dcontext, push_pc));
        else
            PRE(ilist, instr, INSTR_CREATE_push(dcontext, push_pc));
        dstack_offs += XSP_SZ;
        offs_beyond_xmm += XSP_SZ;
        PRE(ilist, instr, INSTR_CREATE_pushf(dcontext));
        dstack_offs += XSP_SZ;
        offs_beyond_xmm += XSP_SZ;
    } else {
        ASSERT(offs_beyond_xmm == 2 * XSP_SZ || !cci->preserve_mcontext);
        /* for cci->preserve_mcontext we added to the lea above so we ignore push_pc */
    }

    /* No processor will support AVX-512 but no SSE/AVX. */
    ASSERT(preserve_xmm_caller_saved() || !ZMM_ENABLED());

    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel */
        int i;
        ASSERT(proc_has_feature(FEATURE_SSE));
        ASSERT(proc_num_simd_saved() == proc_num_simd_registers() ||
               proc_num_simd_saved() == proc_num_simd_sse_avx_registers());
        instr_t *post_push = NULL;
        instr_t *pre_avx512_push = NULL;
        if (ZMM_ENABLED()) {
            post_push = INSTR_CREATE_label(dcontext);
            pre_avx512_push = INSTR_CREATE_label(dcontext);
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext,
                                 OPND_CREATE_ABSMEM(vmcode_get_executable_addr(
                                                        (byte *)d_r_avx512_code_in_use),
                                                    OPSZ_1),
                                 OPND_CREATE_INT8(0)));
            PRE(ilist, instr,
                INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(pre_avx512_push)));
        }
        uint opcode = move_mm_reg_opcode(ALIGNED(alignment, 16), ALIGNED(alignment, 32));
        for (i = 0; i < proc_num_simd_sse_avx_saved(); i++) {
            /* PR 266305: see discussion in emit_fcache_enter_shared on
             * which opcode is better.  Note that the AMD optimization
             * guide says to use movlps+movhps for unaligned stores, but
             * for simplicity and smaller code I'm using movups anyway.
             */
            /* XXX i#438: once have SandyBridge processor need to measure
             * cost of vmovdqu and whether worth arranging 32-byte alignment
             * for all callers.  B/c we put ymm at end of priv_mcontext_t, we do
             * currently have 32-byte alignment for clean calls.
             */
            if (!cci->simd_skip[i]) {
                PRE(ilist, instr,
                    instr_create_1dst_1src(
                        dcontext, opcode,
                        opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                              PRE_XMM_PADDING + i * MCXT_SIMD_SLOT_SIZE +
                                                  offs_beyond_xmm,
                                              OPSZ_SAVED_XMM),
                        opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i)));
            }
        }
        if (ZMM_ENABLED()) {
            PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(post_push)));
            PRE(ilist, instr, pre_avx512_push /*label*/);
            uint opcode_avx512 = move_mm_avx512_reg_opcode(ALIGNED(alignment, 64));
            for (i = 0; i < proc_num_simd_registers(); i++) {
                if (!cci->simd_skip[i]) {
                    instr_t *simdmov = instr_create_1dst_2src(
                        dcontext, opcode_avx512,
                        opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                              PRE_XMM_PADDING + i * MCXT_SIMD_SLOT_SIZE +
                                                  offs_beyond_xmm,
                                              OPSZ_SAVED_ZMM),
                        opnd_create_reg(DR_REG_K0),
                        opnd_create_reg(DR_REG_START_ZMM + (reg_id_t)i));
                    PRE(ilist, instr, simdmov);
                }
            }
            for (i = 0; i < proc_num_opmask_registers(); i++) {
                if (!cci->opmask_skip[i]) {
                    instr_t *maskmov = instr_create_1dst_1src(
                        dcontext,
                        proc_has_feature(FEATURE_AVX512BW) ? OP_kmovq : OP_kmovw,
                        opnd_create_base_disp(
                            REG_XSP, REG_NULL, 0,
                            PRE_XMM_PADDING + MCXT_TOTAL_SIMD_SLOTS_SIZE +
                                i * OPMASK_AVX512BW_REG_SIZE + offs_beyond_xmm,
                            OPSZ_SAVED_OPMASK),
                        opnd_create_reg(DR_REG_START_OPMASK + (reg_id_t)i));
                    PRE(ilist, instr, maskmov);
                }
            }
            PRE(ilist, instr, post_push /*label*/);
        }
    }

#ifdef X64
    /* keep priv_mcontext_t order */
    if (!cci->reg_skip[REG_R15 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R15)));
    if (!cci->reg_skip[REG_R14 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R14)));
    if (!cci->reg_skip[REG_R13 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R13)));
    if (!cci->reg_skip[REG_R12 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R12)));
    if (!cci->reg_skip[REG_R11 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R11)));
    if (!cci->reg_skip[REG_R10 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R10)));
    if (!cci->reg_skip[REG_R9 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R9)));
    if (!cci->reg_skip[REG_R8 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R8)));
    if (!cci->reg_skip[REG_RAX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RAX)));
    if (!cci->reg_skip[REG_RCX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RCX)));
    if (!cci->reg_skip[REG_RDX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RDX)));
    if (!cci->reg_skip[REG_RBX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RBX)));
    /* we do NOT match pusha xsp value */
    if (!cci->reg_skip[REG_RSP - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RSP)));
    if (!cci->reg_skip[REG_RBP - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RBP)));
    if (!cci->reg_skip[REG_RSI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RSI)));
    if (!cci->reg_skip[REG_RDI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RDI)));
    dstack_offs += (DR_NUM_GPR_REGS - cci->num_regs_skip) * XSP_SZ;
#else
    PRE(ilist, instr, INSTR_CREATE_pusha(dcontext));
    dstack_offs += 8 * XSP_SZ;
#endif
    ASSERT(cci->skip_save_flags || cci->num_simd_skip != 0 || cci->num_opmask_skip != 0 ||
           cci->num_regs_skip != 0 ||
           dstack_offs == (uint)get_clean_call_switch_stack_size());
    return dstack_offs;
}

/* User should pass the alignment from insert_push_all_registers: i.e., the
 * alignment at the end of all the popping, not the alignment prior to
 * the popping.
 */
void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, uint alignment)
{
    int offs_beyond_xmm = 0;
    if (cci == NULL)
        cci = &default_clean_call_info;

#ifdef X64
    /* in priv_mcontext_t order */
    if (!cci->reg_skip[REG_RDI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RDI)));
    if (!cci->reg_skip[REG_RSI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RSI)));
    if (!cci->reg_skip[REG_RBP - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBP)));
    /* skip xsp by popping into dead rbx */
    if (!cci->reg_skip[REG_RSP - REG_XAX]) {
        ASSERT(!cci->reg_skip[REG_RBX - REG_XAX]);
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBX)));
    }
    if (!cci->reg_skip[REG_RBX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBX)));
    if (!cci->reg_skip[REG_RDX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RDX)));
    if (!cci->reg_skip[REG_RCX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RCX)));
    if (!cci->reg_skip[REG_RAX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RAX)));
    if (!cci->reg_skip[REG_R8 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R8)));
    if (!cci->reg_skip[REG_R9 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R9)));
    if (!cci->reg_skip[REG_R10 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R10)));
    if (!cci->reg_skip[REG_R11 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R11)));
    if (!cci->reg_skip[REG_R12 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R12)));
    if (!cci->reg_skip[REG_R13 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R13)));
    if (!cci->reg_skip[REG_R14 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R14)));
    if (!cci->reg_skip[REG_R15 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R15)));
#else
    PRE(ilist, instr, INSTR_CREATE_popa(dcontext));
#endif

    /* aflags + pc */
    offs_beyond_xmm = 2 * XSP_SZ;

    /* No processor will support AVX-512 but no SSE/AVX. */
    ASSERT(preserve_xmm_caller_saved() || !ZMM_ENABLED());

    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel */
        int i;
        /* See discussion in emit_fcache_enter_shared on which opcode
         * is better. */
        uint opcode = move_mm_reg_opcode(ALIGNED(alignment, 16), ALIGNED(alignment, 32));
        ASSERT(proc_has_feature(FEATURE_SSE));
        ASSERT(proc_num_simd_saved() == proc_num_simd_registers() ||
               proc_num_simd_saved() == proc_num_simd_sse_avx_registers());
        instr_t *post_pop = NULL;
        instr_t *pre_avx512_pop = NULL;
        if (ZMM_ENABLED()) {
            post_pop = INSTR_CREATE_label(dcontext);
            pre_avx512_pop = INSTR_CREATE_label(dcontext);
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext,
                                 OPND_CREATE_ABSMEM(vmcode_get_executable_addr(
                                                        (byte *)d_r_avx512_code_in_use),
                                                    OPSZ_1),
                                 OPND_CREATE_INT8(0)));
            PRE(ilist, instr,
                INSTR_CREATE_jcc(dcontext, OP_jnz, opnd_create_instr(pre_avx512_pop)));
        }
        for (i = 0; i < proc_num_simd_sse_avx_saved(); i++) {
            if (!cci->simd_skip[i]) {
                PRE(ilist, instr,
                    instr_create_1dst_1src(
                        dcontext, opcode, opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i),
                        opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                              PRE_XMM_PADDING + i * MCXT_SIMD_SLOT_SIZE +
                                                  offs_beyond_xmm,
                                              OPSZ_SAVED_XMM)));
            }
        }
        if (ZMM_ENABLED()) {
            PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(post_pop)));
            PRE(ilist, instr, pre_avx512_pop /*label*/);
            uint opcode_avx512 = move_mm_avx512_reg_opcode(ALIGNED(alignment, 64));
            for (i = 0; i < proc_num_simd_registers(); i++) {
                if (!cci->simd_skip[i]) {
                    instr_t *simdmov = instr_create_1dst_2src(
                        dcontext, opcode_avx512,
                        opnd_create_reg(DR_REG_START_ZMM + (reg_id_t)i),
                        opnd_create_reg(DR_REG_K0),
                        opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                              PRE_XMM_PADDING + i * MCXT_SIMD_SLOT_SIZE +
                                                  offs_beyond_xmm,
                                              OPSZ_SAVED_ZMM));
                    PRE(ilist, instr, simdmov);
                }
            }
            for (i = 0; i < proc_num_opmask_registers(); i++) {
                if (!cci->opmask_skip[i]) {
                    instr_t *maskmov = instr_create_1dst_1src(
                        dcontext,
                        proc_has_feature(FEATURE_AVX512BW) ? OP_kmovq : OP_kmovw,
                        opnd_create_reg(DR_REG_START_OPMASK + (reg_id_t)i),
                        opnd_create_base_disp(
                            REG_XSP, REG_NULL, 0,
                            PRE_XMM_PADDING + MCXT_TOTAL_SIMD_SLOTS_SIZE +
                                i * OPMASK_AVX512BW_REG_SIZE + offs_beyond_xmm,
                            OPSZ_SAVED_OPMASK));
                    PRE(ilist, instr, maskmov);
                }
            }
            PRE(ilist, instr, post_pop /*label*/);
        }
    }

    if (!cci->skip_save_flags) {
        PRE(ilist, instr, INSTR_CREATE_popf(dcontext));
        offs_beyond_xmm = XSP_SZ; /* pc */
    }

    PRE(ilist, instr,
        INSTR_CREATE_lea(
            dcontext, opnd_create_reg(REG_XSP),
            OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0,
                                PRE_XMM_PADDING + MCXT_TOTAL_SIMD_SLOTS_SIZE +
                                    MCXT_TOTAL_OPMASK_SLOTS_SIZE + offs_beyond_xmm)));
}

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
#ifdef X64
    if (opnd_get_size(arg) == OPSZ_4) { /* we ignore var-sized */
        /* PR 250976 #2: leave 64-bit only if an immed w/ top bit set (we
         * assume user wants sign-extension; that is after all what happens
         * on a push of a 32-bit immed) */
        if (!opnd_is_immed_int(arg) || (opnd_get_immed_int(arg) & 0x80000000) == 0)
            return reg_64_to_32(regular);
    }
#endif
    return regular;
}

/* Returns the change in the stack pointer.
 * N.B.: due to stack alignment and minimum stack reservation, do
 * not use parameters involving esp/rsp, as its value can change!
 *
 * This routine only supports passing arguments that are integers or
 * pointers of a size equal or smaller than the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 *
 * For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * Arguments that reference REG_XSP will work for clean calls, but are not guaranteed
 * to work for non-clean, especially in the presence of stack alignment.  Arguments that
 * reference sub-register portions of REG_XSP are not supported.
 *
 * XXX PR 307874: w/ a post optimization pass, or perhaps more clever use of
 * existing passes, we could do much better on calling convention and xsp conflicting
 * args.  We should also really consider inlining client callees (PR 218907), since
 * clean calls for 64-bit are enormous (71 instrs/264 bytes for 2-arg x64; 26
 * instrs/99 bytes for x86) and we could avoid all the xmm saves and replace pushf w/
 * lahf.
 */
uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    uint i;
    int r;
    uint preparm_padding = 0;
    uint param_stack = 0, total_stack = 0;
    bool push = true;
    bool restore_xax = false;
    bool restore_xsp = false;
    /* we need two passes for PR 250976 optimization */
    /* Push/mov in reverse order.  We need a label so we can also add
     * instrs prior to the regular param prep.  So params are POST-mark, while
     * pre-param-prep is POST-prev or PRE-mark.
     */
#ifdef X64
    uint arg_pre_push = 0, total_pre_push = 0;
#endif
    instr_t *prev = (instr == NULL) ? instrlist_last(ilist) : instr_get_prev(instr);
    instr_t *mark = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, mark);

    /* For a clean call, xax is dead (clobbered by prepare_for_clean_call()).
     * Rather than use as scratch and restore prior to each param that uses it,
     * we restore once up front if any use it, and use d_r_regparms[0] as scratch,
     * which is symmetric with non-clean-calls: d_r_regparms[0] is dead since we're
     * doing args in reverse order.  However, we then can't use d_r_regparms[0]
     * directly if referenced in earlier params, but similarly for xax, so
     * there's no clear better way. (prepare_for_clean_call also clobbers xsp,
     * but we just disallow args that use it).
     */

    ASSERT(num_args == 0 || args != NULL);
    /* We can get away w/ one pass, except for PR 250976 we want calling conv
     * regs to be able to refer to priv_mcontext_t as well as potentially being
     * pushed: but we need to know the total # pushes ahead of time (since hard
     * to mark for post-patching)
     */
    for (i = 0; i < num_args; i++) {
        IF_X64(bool is_pre_push = false;)
        for (r = 0; r < opnd_num_regs_used(args[i]); r++) {
            reg_id_t used = opnd_get_reg_used(args[i], r);
            IF_X64(int parm;)
            LOG(THREAD, LOG_INTERP, 4, "ipp: considering arg %d reg %d == %s\n", i, r,
                reg_names[used]);
            if (clean_call && !restore_xax && reg_overlap(used, REG_XAX))
                restore_xax = true;
            if (reg_overlap(used, REG_XSP)) {
                IF_X64(CLIENT_ASSERT(clean_call,
                                     "Non-clean-call argument: REG_XSP not supported"));
                CLIENT_ASSERT(used == REG_XSP,
                              "Call argument: sub-reg-xsp not supported");
                if (clean_call && /*x64*/ parameters_stack_padded() && !restore_xsp)
                    restore_xsp = true;
            }
#ifdef X64
            /* PR 250976 #A: count the number of pre-pushes we need */
            parm = reg_parameter_num(used);
            /* We can read a register used in an earlier arg since we store that
             * arg later (we do reverse order), except arg0, which we use as
             * scratch (we don't always need it, but not worth another pre-pass
             * through all args to find out), and xsp.  Otherwise, if a plain reg,
             * we point at mcontext (we restore xsp slot in mcontext if nec.).
             * If a mem ref, we need to pre-push onto stack.
             * N.B.: this conditional is duplicated in 2nd loop.
             */
            if (!is_pre_push &&
                ((parm == 0 && num_args > 1) || parm > (int)i ||
                 reg_overlap(used, REG_XSP)) &&
                (!clean_call || !opnd_is_reg(args[i]))) {
                total_pre_push++;
                is_pre_push = true; /* ignore further regs in same arg */
            }
#endif
        }
    }

    if (parameters_stack_padded()) {
        /* For x64, supposed to reserve rsp space in function prologue; we
         * do next best thing and reserve it prior to setting up the args.
         */
        push = false; /* store args to xsp offsets instead of pushing them */
        total_stack = REGPARM_MINSTACK;
        if (num_args > NUM_REGPARM)
            total_stack += XSP_SZ * (num_args - NUM_REGPARM);
        param_stack = total_stack;
        IF_X64(total_stack += XSP_SZ * total_pre_push);
        /* We assume rsp is currently 16-byte aligned.  End of arguments is supposed
         * to be 16-byte aligned for x64 SysV (note that retaddr will then make
         * rsp 8-byte-aligned, which is ok: callee has to rectify that).
         * For clean calls, prepare_for_clean_call leaves rsp aligned for x64.
         * XXX PR 218790: we require users of dr_insert_call to ensure
         * alignment; should we put in support to dynamically align?
         */
        preparm_padding =
            ALIGN_FORWARD_UINT(total_stack, REGPARM_END_ALIGN) - total_stack;
        total_stack += preparm_padding;
        /* we have to wait to insert the xsp adjust */
    } else {
        ASSERT(NUM_REGPARM == 0);
        ASSERT(push);
        IF_X64(ASSERT(total_pre_push == 0));
        total_stack = XSP_SZ * num_args;
    }
    LOG(THREAD, LOG_INTERP, 3,
        "insert_parameter_preparation: %d args, %d in-reg, %d pre-push, %d/%d stack\n",
        num_args, NUM_REGPARM, IF_X64_ELSE(total_pre_push, 0), param_stack, total_stack);

    for (i = 0; i < num_args; i++) {
        /* FIXME PR 302951: we need to handle state restoration if any
         * of these args references app memory.  We should pull the state from
         * the priv_mcontext_t on the stack if in a clean call.  FIXME: what if not?
         */
        opnd_t arg = args[i];
        CLIENT_ASSERT(opnd_get_size(arg) == OPSZ_PTR ||
                          opnd_is_immed_int(arg) IF_X64(|| opnd_get_size(arg) == OPSZ_4),
                      "Clean call arg has unsupported size");

#ifdef X64
        /* PR 250976 #A: support args that reference param regs */
        for (r = 0; r < opnd_num_regs_used(arg); r++) {
            reg_id_t used = opnd_get_reg_used(arg, r);
            int parm = reg_parameter_num(used);
            /* See comments in loop above */
            if ((parm == 0 && num_args > 1) || parm > (int)i ||
                reg_overlap(used, REG_XSP)) {
                int disp = 0;
                if (clean_call && opnd_is_reg(arg)) {
                    /* We can point at the priv_mcontext_t slot.
                     * priv_mcontext_t is at the base of dstack: compute offset
                     * from xsp to the field we want and replace arg.
                     */
                    disp += opnd_get_reg_dcontext_offs(opnd_get_reg(arg));
                    /* skip rest of what prepare_for_clean_call adds */
                    disp += clean_call_beyond_mcontext();
                    /* skip what this routine added */
                    disp += total_stack;
                } else {
                    /* Push a temp on the stack and point at it.  We
                     * could try to optimize by juggling registers, but
                     * not worth it.
                     */
                    /* xsp was adjusted up above; we simply store to xsp offsets */
                    disp = param_stack + XSP_SZ * arg_pre_push;
                    if (opnd_is_reg(arg) && opnd_get_size(arg) == OPSZ_PTR) {
                        POST(ilist, prev,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, disp), arg));
                    } else {
                        reg_id_t xsp_scratch = d_r_regparms[0];
                        /* don't want to just change size since will read extra bytes.
                         * can't do mem-to-mem so go through scratch reg */
                        if (reg_overlap(used, REG_XSP)) {
                            /* Get original xsp into scratch[0] and replace in arg */
                            if (opnd_uses_reg(arg, d_r_regparms[0])) {
                                xsp_scratch = REG_XAX;
                                ASSERT(!opnd_uses_reg(arg, REG_XAX)); /* can't use 3 */
                                /* FIXME: rather than putting xsp into mcontext
                                 * slot, better to just do local get from dcontext
                                 * like we do for 32-bit below? */
                                POST(ilist, prev,
                                     instr_create_restore_from_tls(dcontext, REG_XAX,
                                                                   TLS_XAX_SLOT));
                            }
                            opnd_replace_reg(&arg, REG_XSP, xsp_scratch);
                        }
                        POST(ilist, prev,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, disp),
                                                 opnd_create_reg(d_r_regparms[0])));
                        /* If sub-ptr-size, zero-extend is what we want so no movsxd */
                        POST(ilist, prev,
                             INSTR_CREATE_mov_ld(dcontext,
                                                 opnd_create_reg(shrink_reg_for_param(
                                                     d_r_regparms[0], arg)),
                                                 arg));
                        if (reg_overlap(used, REG_XSP)) {
                            int xsp_disp = opnd_get_reg_dcontext_offs(REG_XSP) +
                                clean_call_beyond_mcontext() + total_stack;
                            POST(ilist, prev,
                                 INSTR_CREATE_mov_ld(
                                     dcontext, opnd_create_reg(xsp_scratch),
                                     OPND_CREATE_MEMPTR(REG_XSP, xsp_disp)));
                            if (xsp_scratch == REG_XAX) {
                                POST(ilist, prev,
                                     instr_create_save_to_tls(dcontext, REG_XAX,
                                                              TLS_XAX_SLOT));
                            }
                        }
                        if (opnd_uses_reg(arg, d_r_regparms[0])) {
                            /* must restore since earlier arg might have clobbered */
                            int mc_disp = opnd_get_reg_dcontext_offs(d_r_regparms[0]) +
                                clean_call_beyond_mcontext() + total_stack;
                            POST(ilist, prev,
                                 INSTR_CREATE_mov_ld(
                                     dcontext, opnd_create_reg(d_r_regparms[0]),
                                     OPND_CREATE_MEMPTR(REG_XSP, mc_disp)));
                        }
                    }
                    arg_pre_push++; /* running counter */
                }
                arg =
                    opnd_create_base_disp(REG_XSP, REG_NULL, 0, disp, opnd_get_size(arg));
                break; /* once we've handled arg ignore futher reg refs */
            }
        }
#endif

        if (i < NUM_REGPARM) {
            reg_id_t regparm = shrink_reg_for_param(d_r_regparms[i], arg);
            if (opnd_is_immed_int(arg) || opnd_is_instr(arg)) {
                POST(ilist, mark,
                     INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(regparm), arg));
            } else {
                POST(ilist, mark,
                     INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(regparm), arg));
            }
        } else {
            if (push) {
                IF_X64(ASSERT_NOT_REACHED()); /* no 64-bit push_imm! */
                if (opnd_is_immed_int(arg) || opnd_is_instr(arg))
                    POST(ilist, mark, INSTR_CREATE_push_imm(dcontext, arg));
                else {
                    if (clean_call && opnd_uses_reg(arg, REG_XSP)) {
                        /* We do a purely local expansion:
                         * spill eax, mc->eax, esp->eax, arg->eax, push eax, restore eax
                         */
                        reg_id_t scratch = REG_XAX;
                        if (opnd_uses_reg(arg, scratch)) {
                            scratch = REG_XCX;
                            ASSERT(!opnd_uses_reg(arg, scratch)); /* can't use 3 regs */
                        }
                        opnd_replace_reg(&arg, REG_XSP, scratch);
                        POST(ilist, mark,
                             instr_create_restore_from_tls(dcontext, scratch,
                                                           TLS_XAX_SLOT));
                        POST(ilist, mark, INSTR_CREATE_push(dcontext, arg));
                        POST(ilist, mark,
                             instr_create_restore_from_dc_via_reg(dcontext, scratch,
                                                                  scratch, XSP_OFFSET));
                        insert_get_mcontext_base(dcontext, ilist, instr_get_next(mark),
                                                 scratch);
                        POST(ilist, mark,
                             instr_create_save_to_tls(dcontext, scratch, TLS_XAX_SLOT));
                    } else
                        POST(ilist, mark, INSTR_CREATE_push(dcontext, arg));
                }
            } else {
                /* xsp was adjusted up above; we simply store to xsp offsets */
                uint offs = REGPARM_MINSTACK + XSP_SZ * (i - NUM_REGPARM);
#ifdef X64
                if (opnd_is_immed_int(arg) || opnd_is_instr(arg)) {
                    /* PR 250976 #3: there is no memory store of 64-bit-immediate,
                     * so go through scratch reg */
                    ASSERT(NUM_REGPARM > 0);
                    POST(ilist, mark,
                         INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEMPTR(REG_XSP, offs),
                                             opnd_create_reg(d_r_regparms[0])));
                    POST(ilist, mark,
                         INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(d_r_regparms[0]),
                                              arg));
                } else {
#endif
                    if (opnd_is_memory_reference(arg)) {
                        /* can't do mem-to-mem so go through scratch */
                        reg_id_t scratch;
                        if (NUM_REGPARM > 0)
                            scratch = d_r_regparms[0];
                        else {
                            /* This happens on Mac.
                             * FIXME i#1370: not safe if later arg uses xax:
                             * local spill?  Review how d_r_regparms[0] is preserved.
                             */
                            scratch = REG_XAX;
                        }
                        POST(ilist, mark,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, offs),
                                                 opnd_create_reg(scratch)));
                        POST(ilist, mark,
                             INSTR_CREATE_mov_ld(
                                 dcontext,
                                 opnd_create_reg(shrink_reg_for_param(scratch, arg)),
                                 arg));
                    } else {
                        POST(ilist, mark,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, offs), arg));
                    }
#ifdef X64
                }
#endif
            }
        }
    }
    if (!push && total_stack > 0) {
        POST(ilist, prev, /* before everything else: pre-push and args */
             /* can we use sub?  may as well preserve eflags */
             INSTR_CREATE_lea(
                 dcontext, opnd_create_reg(REG_XSP),
                 OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -(int)total_stack)));
    } else if (total_stack % get_ABI_stack_alignment() != 0) {
        int off = get_ABI_stack_alignment() - (total_stack % get_ABI_stack_alignment());
        total_stack += off;
        POST(ilist, prev, /* before everything */
             INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                              OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -off)));
    }
    if (restore_xsp) {
        /* before restore_xax, since we're going to clobber xax */
        int disp = opnd_get_reg_dcontext_offs(REG_XSP);
        instr_t *where = instr_get_next(prev);
        /* skip rest of what prepare_for_clean_call adds */
        disp += clean_call_beyond_mcontext();
        insert_get_mcontext_base(dcontext, ilist, where, REG_XAX);
        PRE(ilist, where,
            instr_create_restore_from_dc_via_reg(dcontext, REG_XAX, REG_XAX, XSP_OFFSET));
        PRE(ilist, where,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEMPTR(REG_XSP, disp),
                                opnd_create_reg(REG_XAX)));
        /* now we need restore_xax to be AFTER this */
        prev = instr_get_prev(where);
    }
    if (restore_xax) {
        int disp = opnd_get_reg_dcontext_offs(REG_XAX);
        /* skip rest of what prepare_for_clean_call adds */
        disp += clean_call_beyond_mcontext();
        POST(ilist, prev, /* before everything else: pre-push, args, and stack adjust */
             INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                 OPND_CREATE_MEMPTR(REG_XSP, disp)));
    }
    return total_stack;
}

/* If jmp_instr == NULL, uses jmp_tag, otherwise uses jmp_instr
 */
void
insert_clean_call_with_arg_jmp_if_ret_true(dcontext_t *dcontext, instrlist_t *ilist,
                                           instr_t *instr, void *callee, int arg,
                                           app_pc jmp_tag, instr_t *jmp_instr)
{
    instr_t *false_popa, *jcc;
    byte *encode_pc = vmcode_get_start();
    prepare_for_clean_call(dcontext, NULL, ilist, instr, encode_pc);

    dr_insert_call(dcontext, ilist, instr, callee, 1, OPND_CREATE_INT32(arg));

    /* if the return value (xax) is 0, then jmp to internal false path */
    PRE(ilist, instr, /* can't cmp w/ 64-bit immed so use test (shorter anyway) */
        INSTR_CREATE_test(dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XAX)));
    /* fill in jcc target once have false path */
    jcc = INSTR_CREATE_jcc(dcontext, OP_jz, opnd_create_pc(NULL));
    PRE(ilist, instr, jcc);

    /* if it falls through, then it's true, so restore and jmp to true tag
     * passed in by caller
     */
    cleanup_after_clean_call(dcontext, NULL, ilist, instr, encode_pc);
    if (jmp_instr == NULL) {
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr,
                            INSTR_CREATE_jmp(dcontext, opnd_create_pc(jmp_tag)));
    } else {
        PRE(ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_instr(jmp_instr)));
    }

    /* otherwise (if returned false), just do standard popf and continue */
    /* get 1st instr of cleanup path */
    false_popa = instr_get_prev(instr);
    cleanup_after_clean_call(dcontext, NULL, ilist, instr, encode_pc);
    false_popa = instr_get_next(false_popa);
    instr_set_target(jcc, opnd_create_instr(false_popa));
}

/* If !precise, encode_pc is treated as +- a page (meant for clients
 * writing an instrlist to gencode so not sure of exact placement but
 * within a page).
 * If encode_pc == vmcode_get_start(), checks reachability of whole
 * vmcode region (meant for code going somewhere not precisely known
 * in the code cache).
 * Returns whether ended up using a direct cti.  If inlined_tgt_instr != NULL,
 * and an inlined target was used, returns a pointer to that instruction
 * in *inlined_tgt_instr.
 */
bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool returns, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    byte *encode_start;
    byte *encode_end;
    if (precise) {
        encode_start = target + JMP_LONG_LENGTH;
        encode_end = encode_start;
    } else if (encode_pc == vmcode_get_start()) {
        /* consider whole vmcode region */
        encode_start = encode_pc;
        encode_end = vmcode_get_end();
    } else {
        encode_start = (byte *)PAGE_START(encode_pc - PAGE_SIZE);
        encode_end = (byte *)ALIGN_FORWARD(encode_pc + PAGE_SIZE, PAGE_SIZE);
    }
    if (REL32_REACHABLE(encode_start, target) && REL32_REACHABLE(encode_end, target)) {
        /* For precise, we could consider a short cti, but so far no
         * users are precise so we'll leave that for i#56.
         */
        if (jmp)
            PRE(ilist, where, INSTR_CREATE_jmp(dcontext, opnd_create_pc(target)));
        else
            PRE(ilist, where, INSTR_CREATE_call(dcontext, opnd_create_pc(target)));
        return true;
    } else {
        opnd_t ind_tgt;
        instr_t *inlined_tgt = NULL;
        if (scratch == DR_REG_NULL) {
            /* indirect through an inlined target */
            inlined_tgt = instr_build_bits(dcontext, OP_UNDECODED, sizeof(target));
            /* XXX: could use mov imm->xax and have target skip rex+opcode
             * for clean disassembly
             */
            instr_set_raw_bytes(inlined_tgt, (byte *)&target, sizeof(target));
            /* this will copy the bytes for us, so we don't have to worry about
             * the lifetime of the target param
             */
            instr_allocate_raw_bits(dcontext, inlined_tgt, sizeof(target));
            ind_tgt = opnd_create_mem_instr(inlined_tgt, 0, OPSZ_PTR);
            if (inlined_tgt_instr != NULL)
                *inlined_tgt_instr = inlined_tgt;
        } else {
            PRE(ilist, where,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(scratch),
                                     OPND_CREATE_INTPTR(target)));
            ind_tgt = opnd_create_reg(scratch);
            if (inlined_tgt_instr != NULL)
                *inlined_tgt_instr = NULL;
        }
        if (jmp)
            PRE(ilist, where, INSTR_CREATE_jmp_ind(dcontext, ind_tgt));
        else
            PRE(ilist, where, INSTR_CREATE_call_ind(dcontext, ind_tgt));
        if (inlined_tgt != NULL)
            PRE(ilist, where, inlined_tgt);
        return false;
    }
}

/*###########################################################################
 *###########################################################################
 *
 * MANGLING ROUTINES
 */

/* Updates the immediates used by insert_mov_immed_arch() to use the value "val".
 * The "first" and "last" from insert_mov_immed_arch() should be passed here,
 * along with the encoded start pc of "first" as "pc".
 * Keep this in sync with insert_mov_immed_arch().
 * This is *not* a hot-patchable patch: i.e., it is subject to races.
 */
void
patch_mov_immed_arch(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                     instr_t *last)
{
    byte *write_pc = vmcode_get_writable_addr(pc);
    byte *immed_pc;
    ASSERT(first != NULL);
#ifdef X64
    if (X64_MODE_DC(dcontext) && last != NULL) {
        immed_pc = write_pc + instr_length(dcontext, first) - sizeof(int);
        ATOMIC_4BYTE_WRITE(immed_pc, (int)val, NOT_HOT_PATCHABLE);
        immed_pc = write_pc + instr_length(dcontext, first) +
            instr_length(dcontext, last) - sizeof(int);
        ATOMIC_4BYTE_WRITE(immed_pc, (int)(val >> 32), NOT_HOT_PATCHABLE);
    } else {
#endif
        immed_pc = write_pc + instr_length(dcontext, first) - sizeof(val);
        ATOMIC_ADDR_WRITE(immed_pc, val, NOT_HOT_PATCHABLE);
        ASSERT(last == NULL);
#ifdef X64
    }
#endif
}

/* Far calls and rets have double total size */
static opnd_size_t
stack_entry_size(instr_t *instr, opnd_size_t opsize)
{
    if (instr_get_opcode(instr) == OP_call_far ||
        instr_get_opcode(instr) == OP_call_far_ind ||
        instr_get_opcode(instr) == OP_ret_far) {
        /* cut OPSZ_8_rex16_short4 in half */
        if (opsize == OPSZ_4)
            return OPSZ_2;
        else if (opsize == OPSZ_8)
            return OPSZ_4;
        else {
#ifdef X64
            ASSERT(opsize == OPSZ_16);
            return OPSZ_8;
#else
            ASSERT_NOT_REACHED();
#endif
        }
    } else if (instr_get_opcode(instr) == OP_iret) {
        /* convert OPSZ_12_rex40_short6 */
        if (opsize == OPSZ_6)
            return OPSZ_2;
        else if (opsize == OPSZ_12)
            return OPSZ_4;
        else {
#ifdef X64
            ASSERT(opsize == OPSZ_40);
            return OPSZ_8;
#else
            ASSERT_NOT_REACHED();
#endif
        }
    }
    return opsize;
}

/* Used for fault translation */
bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust)
{
    ASSERT(xsp_adjust != NULL);
    if (instr_get_opcode(inst) == OP_push || instr_get_opcode(inst) == OP_push_imm) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: push or push_imm\n");
        *xsp_adjust -= opnd_size_in_bytes(opnd_get_size(instr_get_dst(inst, 1)));
    } else if (instr_get_opcode(inst) == OP_pop) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: pop\n");
        *xsp_adjust += opnd_size_in_bytes(opnd_get_size(instr_get_src(inst, 1)));
    }
    /* 1st part of push emulation from insert_push_retaddr */
    else if (instr_get_opcode(inst) == OP_lea &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_XSP &&
             opnd_get_base(instr_get_src(inst, 0)) == REG_XSP &&
             opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: lea xsp adjust\n");
        *xsp_adjust += opnd_get_disp(instr_get_src(inst, 0));
    }
    /* 2nd part of push emulation from insert_push_retaddr */
    else if (instr_get_opcode(inst) == OP_mov_st &&
             opnd_is_base_disp(instr_get_dst(inst, 0)) &&
             opnd_get_base(instr_get_dst(inst, 0)) == REG_XSP &&
             opnd_get_index(instr_get_dst(inst, 0)) == REG_NULL) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: store to stack\n");
        /* nothing to track: paired lea is what we undo */
    }
    /* retrieval of target for call* or jmp* */
    else if ((instr_get_opcode(inst) == OP_movzx &&
              reg_overlap(opnd_get_reg(instr_get_dst(inst, 0)), REG_XCX)) ||
             (instr_get_opcode(inst) == OP_mov_ld &&
              reg_overlap(opnd_get_reg(instr_get_dst(inst, 0)), REG_XCX))) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: ib tgt to *cx\n");
        /* nothing: our xcx spill restore will undo */
    }
    /* part of pop emulation for iretd/lretd in x64 mode */
    else if (instr_get_opcode(inst) == OP_mov_ld &&
             opnd_is_base_disp(instr_get_src(inst, 0)) &&
             opnd_get_base(instr_get_src(inst, 0)) == REG_XSP &&
             opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: load from stack\n");
        /* nothing to track: paired lea is what we undo */
    }
    /* part of data16 ret.  once we have cs preservation (PR 271317) we'll
     * need to not fail when walking over a movzx to a pop cs (right now we
     * do not read the stack for the pop cs).
     */
    else if (instr_get_opcode(inst) == OP_movzx &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_CX) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: movzx to cx\n");
        /* nothing: our xcx spill restore will undo */
    }
    /* fake pop of cs for iret */
    else if (instr_get_opcode(inst) == OP_add && opnd_is_reg(instr_get_dst(inst, 0)) &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_XSP &&
             opnd_is_immed_int(instr_get_src(inst, 0))) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: add to xsp\n");
        ASSERT(CHECK_TRUNCATE_TYPE_int(opnd_get_immed_int(instr_get_src(inst, 0))));
        *xsp_adjust += (int)opnd_get_immed_int(instr_get_src(inst, 0));
    }
    /* popf for iret */
    else if (instr_get_opcode(inst) == OP_popf) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: popf\n");
        *xsp_adjust += opnd_size_in_bytes(opnd_get_size(instr_get_src(inst, 1)));
    } else {
        return false;
    }
    return true;
}

/* Returns whether the instruction supports a simple mangling epilogue that is supported
 * to xl8 to the next program counter post original app instruction.
 */
bool
instr_supports_simple_mangling_epilogue(dcontext_t *dcontext, instr_t *inst)
{
    /* XXX we expect the check in translate_walk_restore to fail if any other type
     * of mangling overlaps with rip-rel mangling than the supported ones. Currently,
     * this are only rip-rel control-flow instructions which are excluded here.
     */
    return !instr_is_cti(inst);
}

/* N.B.: keep in synch with instr_check_xsp_mangling() */
void
insert_push_retaddr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                    ptr_int_t retaddr, opnd_size_t opsize)
{
    if (opsize == OPSZ_2) {
        ptr_int_t val = retaddr & (ptr_int_t)0x0000ffff;
        /* can't do a non-default operand size with a push immed so we emulate */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, -2, OPSZ_lea)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM16(REG_XSP, 2),
                                OPND_CREATE_INT16((short)val)));
    } else if (opsize ==
               OPSZ_PTR IF_X64(|| (!X64_CACHE_MODE_DC(dcontext) && opsize == OPSZ_4))) {
        insert_push_immed_ptrsz(dcontext, retaddr, ilist, instr, NULL, NULL);
    } else {
#ifdef X64
        ptr_int_t val = retaddr & (ptr_int_t)0xffffffff;
        ASSERT(opsize == OPSZ_4);
        /* can't do a non-default operand size with a push immed so we emulate */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, -4, OPSZ_lea)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 0),
                                OPND_CREATE_INT32((int)val)));
#else
        ASSERT_NOT_REACHED();
#endif
    }
}

/* N.B.: keep in synch with instr_check_xsp_mangling() */
static void
insert_mov_ptr_uint_beyond_TOS(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                               ptr_int_t value, opnd_size_t opsize)
{
    /* we insert non-meta b/c we want faults to go to app (should only fault
     * if the ret itself faulted, barring races) for simplicity: o/w our
     * our-mangling sequence gets broken up and more complex.
     */
    if (opsize == OPSZ_2) {
        ptr_int_t val = value & (ptr_int_t)0x0000ffff;
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM16(REG_XSP, -2),
                                OPND_CREATE_INT16(val)));
    } else if (opsize == OPSZ_4) {
        ptr_int_t val = value & (ptr_int_t)0xffffffff;
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, -4),
                                OPND_CREATE_INT32(val)));
    } else {
#ifdef X64
        ptr_int_t val_low = value & (ptr_int_t)0xffffffff;
        ASSERT(opsize == OPSZ_8);
        if (CHECK_TRUNCATE_TYPE_int(value)) {
            /* prefer a single write w/ sign-extension */
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM64(REG_XSP, -8),
                                    OPND_CREATE_INT32(val_low)));
        } else {
            /* we need two 32-bit writes */
            ptr_int_t val_high = (value >> 32);
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, -8),
                                    OPND_CREATE_INT32(val_low)));
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, -4),
                                    OPND_CREATE_INT32(val_high)));
        }
#else
        ASSERT_NOT_REACHED();
#endif
    }
}

static void
insert_push_cs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               ptr_int_t retaddr, opnd_size_t opsize)
{
#ifdef X64
    if (X64_CACHE_MODE_DC(dcontext)) {
        /* "push cs" is invalid; for now we push the typical cs values.
         * i#823 covers doing this more generally.
         */
        insert_push_retaddr(dcontext, ilist, instr,
                            X64_MODE_DC(dcontext) ? CS64_SELECTOR : CS32_SELECTOR,
                            opsize);
    } else {
#endif
        opnd_t stackop;
        /* we go ahead and push cs, but we won't pop into cs */
        instr_t *push = INSTR_CREATE_push(dcontext, opnd_create_reg(SEG_CS));
        /* 2nd dest is the stack operand size */
        stackop = instr_get_dst(push, 1);
        opnd_set_size(&stackop, opsize);
        instr_set_dst(push, 1, stackop);
        PRE(ilist, instr, push);
#ifdef X64
    }
#endif
}

/* We spill to XCX(private dcontext) slot for private fragments,
 * and to TLS MANGLE_XCX_SPILL_SLOT for shared fragments.
 * (Except for DYNAMO_OPTION(private_ib_in_tls), for which all use tls,
 *  but that has a performance hit because of the extra data cache line)
 * We can get away with the split by having the shared ibl routine copy
 * xcx to the private dcontext, and by having the private ibl never
 * target shared fragments.
 * We also have to modify the xcx spill from tls to private dcontext when
 * adding a shared basic block to a trace.
 *
 * FIXME: if we do make non-trace-head basic blocks valid indirect branch
 * targets, we should have the private ibl have special code to test the
 * flags and copy xcx to the tls slot if necessary.
 */
#define SAVE_TO_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs)          \
    ((DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, (flags))) \
         ? instr_create_save_to_tls(dc, reg, tls_offs)                \
         : instr_create_save_to_dcontext((dc), (reg), (dc_offs)))

#define SAVE_TO_DC_OR_TLS_OR_REG(dc, flags, reg, tls_offs, dc_offs, dest_reg)       \
    ((X64_CACHE_MODE_DC(dc) &&                                                      \
      !X64_MODE_DC(dc) IF_X64(&&DYNAMO_OPTION(x86_to_x64_ibl_opt)))                 \
         ? INSTR_CREATE_mov_ld(dc, opnd_create_reg(dest_reg), opnd_create_reg(reg)) \
         : SAVE_TO_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs))

#define RESTORE_FROM_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs)     \
    ((DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, (flags))) \
         ? instr_create_restore_from_tls(dc, reg, tls_offs)           \
         : instr_create_restore_from_dcontext((dc), (reg), (dc_offs)))

static void
mangle_far_direct_helper(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         instr_t *next_instr, uint flags)
{
    /* FIXME i#823: we do not support other than flat 0-based CS, DS, SS, and ES.
     * If the app wants to change segments in a WOW64 process, we will
     * do the right thing for standard cs selector values (xref i#49).
     * For other cs changes or in other modes, we do go through far_ibl
     * today although we do not enact the cs change (nor bother to pass
     * the selector in xbx).
     *
     * For WOW64, I tried keeping this a direct jmp for nice linking by doing the
     * mode change in-fragment and then using a 64-bit stub with a 32-bit fragment,
     * but that gets messy b/c a lot of code assumes it can create or calculate the
     * size of exit stubs given nothing but the fragment flags.  I tried adding
     * FRAG_ENDS_IN_FAR_DIRECT but still need to pass another param to all the stub
     * macros and routines for mid-trace exits and for prefixes for -disable_traces.
     * So, going for treating as indirect and using the far_ibl.  It's a trace
     * barrier anyway, and rare.  We treat it as indirect in all modes (including
     * x86 builds) for simplicity (and eventually for full i#823 we'll want
     * to issue cs changes there too).
     */
    app_pc pc = opnd_get_pc(instr_get_target(instr));

#ifdef X64
    if (!X64_MODE_DC(dcontext) &&
        opnd_get_segment_selector(instr_get_target(instr)) == CS64_SELECTOR) {
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XBX, MANGLE_FAR_SPILL_SLOT,
                                     XBX_OFFSET, REG_R10));
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                                 OPND_CREATE_INT32(CS64_SELECTOR)));
    }
#endif

    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT,
                                 XCX_OFFSET, REG_R9));
    ASSERT((ptr_uint_t)pc < UINT_MAX); /* 32-bit code! */
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX),
                             OPND_CREATE_INT32((ptr_uint_t)pc)));
}

/***************************************************************************
 * DIRECT CALL
 * Returns new next_instr
 */
instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    ptr_uint_t retaddr;
    app_pc target = NULL;
    opnd_t pushop = instr_get_dst(instr, 1);
    opnd_size_t pushsz = stack_entry_size(instr, opnd_get_size(pushop));
    if (opnd_is_near_pc(instr_get_target(instr)))
        target = opnd_get_pc(instr_get_target(instr));
    else if (opnd_is_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        /* assumption: target's raw bits are meaningful */
        target = instr_get_raw_bits(tgt);
        ASSERT(target != 0);
        /* FIXME case 6962: for far instr, we ignore the segment and
         * assume it matches current cs */
    } else if (opnd_is_far_pc(instr_get_target(instr))) {
        target = opnd_get_pc(instr_get_target(instr));
        /* FIXME case 6962: we ignore the segment and assume it matches current cs */
    } else
        ASSERT_NOT_REACHED();

    if (!mangle_calls) {
        /* off-trace call that will be executed natively */
        /* relative target must be re-encoded */
        instr_set_raw_bits_valid(instr, false);
        return next_instr;
    }

    retaddr = get_call_return_address(dcontext, ilist, instr);

#ifdef CHECK_RETURNS_SSE2
    /* ASSUMPTION: a call to the next instr is not going to ever have a
     * matching ret! */
    if (target == (app_pc)retaddr) {
        LOG(THREAD, LOG_INTERP, 3, "found call to next instruction " PFX "\n", target);
    } else {
        check_return_handle_call(dcontext, ilist, next_instr);
    }
    /* now do the normal thing for a call */
#endif

    if (instr_get_opcode(instr) == OP_call_far) {
        /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
         * if the app wants to change segments, we won't actually issue
         * a segment change, and so will only work properly if the new segment
         * is also 0-based.  To properly issue new segments, we'd need a special
         * ibl that ends in a far cti, and all prior address manipulations would
         * need to be relative to the new segment, w/o messing up current segment.
         * FIXME: can we do better without too much work?
         * XXX: yes, for wow64: i#823: TODO mangle this like a far direct jmp
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct call");
        STATS_INC(num_far_dir_calls);

        mangle_far_direct_helper(dcontext, ilist, instr, next_instr, flags);

        insert_push_cs(dcontext, ilist, instr, 0, pushsz);
    }

    /* convert a direct call to a push of the return address */
    insert_push_retaddr(dcontext, ilist, instr, retaddr, pushsz);

    /* remove the call */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
}

#ifdef UNIX
/***************************************************************************
 * Mangle the memory reference operand that uses fs/gs semgents,
 * get the segment base of fs/gs into reg, and
 * replace oldop with newop using reg instead of fs/gs
 * The reg must not be used in the oldop, otherwise, the reg value
 * is corrupted.
 */

static ushort tls_slots[4] = { TLS_XAX_SLOT, TLS_XCX_SLOT, TLS_XDX_SLOT, TLS_XBX_SLOT };

opnd_t
mangle_seg_ref_opnd(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                    opnd_t oldop, reg_id_t reg)
{
    opnd_t newop;
    reg_id_t seg;

    ASSERT(opnd_is_far_base_disp(oldop));
    seg = opnd_get_segment(oldop);

    /* We only mangle fs/gs, assuming that ds, es, and cs are flat
     * (an assumption throughout the code, and always true for x64).
     */
    if (seg != SEG_GS && seg != SEG_FS)
        return oldop;
    if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
        return oldop;
    /* The reg should not be used by the oldop */
    ASSERT(!opnd_uses_reg(oldop, reg));

    /* XXX: this mangling is pattern-matched in translation's instr_is_seg_ref_load() */
    /* get app's segment base into reg. */
    PRE(ilist, instr,
        instr_create_restore_from_tls(dcontext, reg, os_get_app_tls_base_offset(seg)));
    if ((opnd_get_base(oldop) != DR_REG_NULL &&
         reg_get_size(opnd_get_base(oldop)) == OPSZ_2) ||
        (opnd_get_index(oldop) != DR_REG_NULL &&
         reg_get_size(opnd_get_index(oldop)) == OPSZ_2)) {
        /* We can't combine our full-size seg base reg with addr16 regs so we
         * need another scratch reg to first compute thet 16-bit-reg address.
         */
        reg_id_t scratch2;
        for (scratch2 = REG_XAX; scratch2 <= REG_XBX; scratch2++) {
            if (!instr_uses_reg(instr, scratch2) && scratch2 != reg)
                break;
        }
        ASSERT(scratch2 <= REG_XBX);
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, scratch2, tls_slots[scratch2 - REG_XAX]));
        instr_set_our_mangling(instr, true);
        PRE(ilist, instr_get_next(instr),
            instr_set_translation_mangling_epilogue(
                dcontext, ilist,
                instr_create_restore_from_tls(dcontext, scratch2,
                                              tls_slots[scratch2 - REG_XAX])));
        /* We add addr16 to the lea, and remove from the instr, to make the disasm
         * easier to read (does not affect encoding or correctness).
         */
        PRE(ilist, instr,
            instr_set_prefix_flag(
                INSTR_CREATE_lea(dcontext, opnd_create_reg(scratch2),
                                 opnd_create_base_disp(opnd_get_base(oldop),
                                                       opnd_get_index(oldop),
                                                       opnd_get_scale(oldop),
                                                       opnd_get_disp(oldop), OPSZ_lea)),
                PREFIX_ADDR));
        uint prefixes = instr_get_prefixes(instr);
        instr_set_prefixes(instr, prefixes & (~PREFIX_ADDR));
        return opnd_create_base_disp(reg, scratch2, 1, 0, opnd_get_size(oldop));
    }
    if (opnd_get_index(oldop) != REG_NULL && opnd_get_base(oldop) != REG_NULL) {
        /* if both base and index are used, use
         * lea [base, reg, 1] => reg
         * to get the base + seg_base into reg.
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(
                dcontext, opnd_create_reg(reg),
                opnd_create_base_disp(opnd_get_base(oldop), reg, 1, 0, OPSZ_lea)));
    }
    if (opnd_get_index(oldop) != REG_NULL) {
        newop = opnd_create_base_disp(reg, opnd_get_index(oldop), opnd_get_scale(oldop),
                                      opnd_get_disp(oldop), opnd_get_size(oldop));
    } else {
        newop = opnd_create_base_disp(opnd_get_base(oldop), reg, 1, opnd_get_disp(oldop),
                                      opnd_get_size(oldop));
    }
    return newop;
}
#endif /* UNIX */

/***************************************************************************
 * INDIRECT CALL
 */

static reg_id_t
mangle_far_indirect_helper(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                           instr_t *next_instr, uint flags, opnd_t *target_out)
{
    opnd_t target = *target_out;
    opnd_size_t addr_size;
    reg_id_t reg_target = REG_NULL;
    ASSERT(instr_get_opcode(instr) == OP_jmp_far_ind ||
           instr_get_opcode(instr) == OP_call_far_ind);
    /* FIXME i#823: we do not support other than flat 0-based CS, DS, SS, and ES.
     * If the app wants to change segments in a WOW64 process, we will
     * do the right thing for standard cs selector values (xref i#49).
     * For other cs changes or in other modes, we do go through far_ibl
     * today although we do not enact the cs change (nor bother to pass
     * the selector in xbx).
     */
    /* opnd type is i_Ep, it's not a far base disp b/c segment is at
     * memory location, not specified as segment prefix on instr
     * we assume register operands are marked as invalid instrs long
     * before this point.
     */
    ASSERT(opnd_is_base_disp(target));
    /* Segment selector is the final 2 bytes.
     * For non-mixed-mode, we ignore it.
     * We assume DS base == target cti CS base.
     */
    /* if data16 then just 2 bytes for address
     * if x64 mode and Intel and rex then 8 bytes for address */
    ASSERT((X64_MODE_DC(dcontext) && opnd_get_size(target) == OPSZ_10 &&
            proc_get_vendor() != VENDOR_AMD) ||
           opnd_get_size(target) == OPSZ_6 || opnd_get_size(target) == OPSZ_4);
    if (opnd_get_size(target) == OPSZ_10) {
        addr_size = OPSZ_8;
        reg_target = REG_RCX;
    } else if (opnd_get_size(target) == OPSZ_6) {
        addr_size = OPSZ_4;
        reg_target = REG_ECX;
    } else /* target has OPSZ_4 */ {
        addr_size = OPSZ_2;
        reg_target = REG_XCX; /* caller uses movzx so size doesn't have to match */
    }
#ifdef X64
    if (mixed_mode_enabled()) {
        /* While we don't support arbitrary segments, we do support
         * mode changes using standard cs selector values (i#823).
         * We save the selector into xbx.
         */
        opnd_t sel = target;
        opnd_set_disp(&sel, opnd_get_disp(target) + opnd_size_in_bytes(addr_size));
        opnd_set_size(&sel, OPSZ_2);

        /* all scratch space should be in TLS only */
        ASSERT(TEST(FRAG_SHARED, flags) || DYNAMO_OPTION(private_ib_in_tls));
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XBX, MANGLE_FAR_SPILL_SLOT,
                                     XBX_OFFSET, REG_R10));
        PRE(ilist, instr, INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EBX), sel));
        if (instr_uses_reg(instr, REG_XBX)) {
            /* instr can't be both riprel (uses xax slot for mangling) and use
             * a register, so we spill to the riprel (== xax) slot
             */
            PRE(ilist, instr,
                SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XBX, MANGLE_RIPREL_SPILL_SLOT,
                                  XAX_OFFSET));
            POST(ilist, instr,
                 instr_create_restore_from_tls(dcontext, REG_XBX,
                                               MANGLE_RIPREL_SPILL_SLOT));
        }
    }
#endif
    opnd_set_size(target_out, addr_size);
    return reg_target;
}

instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    opnd_t target;
    ptr_uint_t retaddr;
    opnd_t pushop = instr_get_dst(instr, 1);
    opnd_size_t pushsz = stack_entry_size(instr, opnd_get_size(pushop));
    reg_id_t reg_target = REG_XCX;

    if (!mangle_calls)
        return next_instr;
    retaddr = get_call_return_address(dcontext, ilist, instr);

    /* Convert near, indirect calls.  The jump to the exit_stub that
     * jumps to indirect_branch_lookup was already inserted into the
     * instr list by interp EXCEPT for the case in which we're converting
     * an indirect call to a direct call. In that case, mangle later
     * inserts a direct exit stub.
     */
    /* If this call is marked for conversion, do minimal processing.
     * FIXME Just a note that converted calls are not subjected to any of
     * the specialized builds' processing further down.
     */
    if (TEST(INSTR_IND_CALL_DIRECT, instr->flags)) {
        /* convert the call to a push of the return address */
        insert_push_retaddr(dcontext, ilist, instr, retaddr, pushsz);
        /* remove the call */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        return next_instr;
    }

    /* put the push AFTER the instruction that calculates
     * the target, b/c if target depends on xsp we must use
     * the value of xsp prior to this call instruction!
     * we insert before next_instr to accomplish this.
     */
    if (instr_get_opcode(instr) == OP_call_far_ind) {
        /* goes right before the push of the ret addr */
        insert_push_cs(dcontext, ilist, next_instr, 0, pushsz);
        /* see notes below -- we don't really support switching segments,
         * though we do go ahead and push cs, we won't pop into cs
         */
    }
    insert_push_retaddr(dcontext, ilist, next_instr, retaddr, pushsz);

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT,
                                 XCX_OFFSET, REG_R9));

    /* change: call /2, Ev -> movl Ev, %xcx */
    target = instr_get_src(instr, 0);
    if (instr_get_opcode(instr) == OP_call_far_ind) {
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far indirect call");
        STATS_INC(num_far_ind_calls);
        reg_target = mangle_far_indirect_helper(dcontext, ilist, instr, next_instr, flags,
                                                &target);
    }
#ifdef UNIX
    /* i#107, mangle the memory reference opnd that uses segment register. */
    if (INTERNAL_OPTION(mangle_app_seg) && opnd_is_far_base_disp(target)) {
        /* TODO i#107: We use REG_XCX to store the segment base, which might be used
         * in "target" and cause failure in mangle_seg_ref_opnd.
         * We need to spill another register in that case.
         */
        ASSERT_BUG_NUM(107,
                       !opnd_uses_reg(target, REG_XCX) ||
                           (opnd_get_segment(target) != SEG_FS &&
                            opnd_get_segment(target) != SEG_GS));
        target = mangle_seg_ref_opnd(dcontext, ilist, instr, target, REG_XCX);
    }
#endif
    /* cannot call instr_reset, it will kill prev & next ptrs */
    instr_free(dcontext, instr);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_opcode(instr, opnd_get_size(target) == OPSZ_2 ? OP_movzx : OP_mov_ld);
    instr_set_dst(instr, 0, opnd_create_reg(reg_target));
    instr_set_src(instr, 0, target); /* src stays the same */
    if (instrlist_get_translation_target(ilist) != NULL) {
        /* make sure original raw bits are used for translation */
        instr_set_translation(instr, instr_get_raw_bits(instr));
    }
    instr_set_our_mangling(instr, true);

#ifdef CHECK_RETURNS_SSE2
    check_return_handle_call(dcontext, ilist, next_instr);
#endif
    return next_instr;
}

/***************************************************************************
 * RETURN
 */

#ifdef X64
/* Saves the selector from the top of the stack into xbx, after spilling xbx,
 * for far_ibl.
 */
static void
mangle_far_return_save_selector(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                uint flags)
{
    if (mixed_mode_enabled()) {
        /* While we don't support arbitrary segments, we do support
         * mode changes using standard cs selector values (i#823).
         * We save the selector into xbx.
         */
        /* We could do a pop but state xl8 is already set up to restore lea */
        /* all scratch space should be in TLS only */
        ASSERT(TEST(FRAG_SHARED, flags) || DYNAMO_OPTION(private_ib_in_tls));
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XBX, MANGLE_FAR_SPILL_SLOT,
                                     XBX_OFFSET, REG_R10));
        PRE(ilist, instr,
            INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EBX),
                               OPND_CREATE_MEM16(REG_XSP, 0)));
    }
}
#endif

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    instr_t *pop;
    opnd_t retaddr;
    opnd_size_t retsz;

#ifdef CHECK_RETURNS_SSE2
    check_return_handle_return(dcontext, ilist, next_instr);
    /* now do the normal ret mangling */
#endif

    /* Convert returns.  If aggressive we could take advantage of the
     * fact that xcx is dead at the return and not bother saving it?
     * The jump to the exit_stub that jumps to indirect_branch_lookup
     * was already inserted into the instr list by interp. */

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT,
                                 XCX_OFFSET, REG_R9));

    /* see if ret has an immed int operand, assumed to be 1st src */

    if (instr_num_srcs(instr) > 0 && opnd_is_immed_int(instr_get_src(instr, 0))) {
        /* if has an operand, return removes some stack space,
         * AFTER the return address is popped
         */
        int val = (int)opnd_get_immed_int(instr_get_src(instr, 0));
        IF_X64(ASSERT_TRUNCATE(val, int, opnd_get_immed_int(instr_get_src(instr, 0))));
        /* addl sizeof_param_area, %xsp
         * except that clobbers the flags, so we use lea */
        PRE(ilist, next_instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, val, OPSZ_lea)));
    }

    /* don't need to steal edi since return cannot use registers */

    /* the retaddr operand is always the final source for all OP_ret* instrs */
    retaddr = instr_get_src(instr, instr_num_srcs(instr) - 1);
    retsz = stack_entry_size(instr, opnd_get_size(retaddr));

    if (X64_CACHE_MODE_DC(dcontext) && retsz == OPSZ_4) {
        if (instr_get_opcode(instr) == OP_iret || instr_get_opcode(instr) == OP_ret_far) {
            /* N.B.: For some unfathomable reason iret and ret_far default to operand
             * size 4 in 64-bit mode (making them, along w/ call_far, the only stack
             * operation instructions to do so). So if we see an iret or far ret with
             * OPSZ_4 in 64-bit mode we need a 4-byte pop, but since we can't actually
             * generate a 4-byte pop we have to emulate it here. */
            SYSLOG_INTERNAL_WARNING_ONCE("Encountered iretd/lretd in 64-bit mode!");
        }
        /* Note moving into ecx automatically zero extends which is what we want. */
        PRE(ilist, instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                                OPND_CREATE_MEM32(REG_RSP, 0)));
        /* iret could use add since going to pop the eflags, but not lret.
         * lret could combine w/ segment lea below: but not perf-crit instr, and
         * anticipating cs preservation PR 271317 I'm leaving separate. */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, 4, OPSZ_lea)));
    } else {
        /* change RET into a POP, keeping the operand size */
        opnd_t memop = retaddr;
        pop = INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XCX));
        /* need per-entry size, not total size (double for far ret) */
        opnd_set_size(&memop, retsz);
        instr_set_src(pop, 1, memop);
        if (retsz == OPSZ_2)
            instr_set_dst(pop, 0, opnd_create_reg(REG_CX));
        /* We can't do a 4-byte pop in 64-bit mode, but excepting iretd and lretd
         * handled above we should never see one. */
        ASSERT(!X64_MODE_DC(dcontext) || retsz != OPSZ_4);
        PRE(ilist, instr, pop);
        if (retsz == OPSZ_2) {
            /* we need to zero out the top 2 bytes */
            PRE(ilist, instr,
                INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_ECX),
                                   opnd_create_reg(REG_CX)));
        }
    }

    if (TEST(INSTR_CLOBBER_RETADDR, instr->flags)) {
        /* we put the value in the offset field earlier */
        ptr_uint_t val = (ptr_uint_t)instr->offset;
        insert_mov_ptr_uint_beyond_TOS(dcontext, ilist, instr, val, retsz);
    }

    if (instr_get_opcode(instr) == OP_ret_far) {
        /* FIXME i#823: we do not support other than flat 0-based CS, DS, SS, and ES.
         * If the app wants to change segments in a WOW64 process, we will
         * do the right thing for standard cs selector values (xref i#49).
         * For other cs changes or in other modes, we do go through far_ibl
         * today although we do not enact the cs change (nor bother to pass
         * the selector in xbx).
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far ret");
        STATS_INC(num_far_rets);
#ifdef X64
        mangle_far_return_save_selector(dcontext, ilist, instr, flags);
#endif
        /* pop selector from stack, but not into cs, just junk it
         * (the 16-bit selector is expanded to 32 bits on the push, unless data16)
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                                   opnd_size_in_bytes(retsz), OPSZ_lea)));
    }

    if (instr_get_opcode(instr) == OP_iret) {
        instr_t *popf;

        /* Xref PR 215553 and PR 191977 - we actually see this on 64-bit Vista */
        LOG(THREAD, LOG_INTERP, 2, "Encountered iret at " PFX " - mangling\n",
            instr_get_translation(instr));
        STATS_INC(num_irets);

        /* In 32-bit mode this is a pop->EIP pop->CS pop->eflags.
         * 64-bit mode (with either 32-bit or 64-bit operand size,
         * despite the (wrong) Intel manual pseudocode: see i#833 and
         * the win32.mixedmode test) extends
         * the above and additionally adds pop->RSP pop->ss.  N.B.: like OP_far_ret we
         * ignore the CS (except mixed-mode WOW64) and SS segment changes
         * (see the comments there).
         */

#ifdef X64
        mangle_far_return_save_selector(dcontext, ilist, instr, flags);
#endif
        /* Return address is already popped, next up is CS segment which we ignore
         * (unless in mixed-mode, handled above) so
         * adjust stack pointer. Note we can use an add here since the eflags will
         * be written below. */
        PRE(ilist, instr,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_INT8(opnd_size_in_bytes(retsz))));

        /* Next up is xflags, we use a popf. Popf should be setting the right flags
         * (it's difficult to tell because in the docs iret lists the flags it does
         * set while popf lists the flags it doesn't set). The docs aren't entirely
         * clear, but any flag that we or a user mode program would care about should
         * be right. */
        popf = INSTR_CREATE_popf(dcontext);
        if (X64_CACHE_MODE_DC(dcontext) && retsz == OPSZ_4) {
            /* We can't actually create a 32-bit popf and there's no easy way to
             * simulate one.  For now we'll do a 64-bit popf and fixup the stack offset.
             * If AMD/INTEL ever start using the top half of the rflags register then
             * we could have problems here. We could also break stack transparency and
             * do a mov, push, popf to zero extend the value. */
            PRE(ilist, instr, popf);
            /* flags are already set, must use lea to fix stack */
            PRE(ilist, instr,
                INSTR_CREATE_lea(
                    dcontext, opnd_create_reg(REG_XSP),
                    opnd_create_base_disp(REG_XSP, REG_NULL, 0, -4, OPSZ_lea)));
        } else {
            /* get popf size right the same way we do it for the return address */
            opnd_t memop = retaddr;
            opnd_set_size(&memop, retsz);
            DOCHECK(1, {
                if (retsz == OPSZ_2)
                    ASSERT_NOT_TESTED();
            });
            instr_set_src(popf, 1, memop);
            PRE(ilist, instr, popf);
        }
        /* Mangles single step exception after a popf. */
        mangle_possible_single_step(dcontext, ilist, popf);

#ifdef X64
        /* In x64 mode, iret additionally does pop->RSP and pop->ss. */
        if (X64_MODE_DC(dcontext)) {
            if (retsz == OPSZ_8)
                PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RSP)));
            else if (retsz == OPSZ_4) {
                PRE(ilist, instr,
                    INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ESP),
                                        OPND_CREATE_MEM32(REG_RSP, 0)));
            } else {
                ASSERT_NOT_TESTED();
                PRE(ilist, instr,
                    INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_ESP),
                                       OPND_CREATE_MEM16(REG_RSP, 0)));
            }
            /* We're ignoring the set of SS and since we just set RSP we don't need
             * to do anything to adjust the stack for the pop (since the pop would have
             * occurred with the old RSP). */
        }
#endif
    }

    /* remove the ret */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
}

/***************************************************************************
 * INDIRECT JUMP
 */
instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    opnd_t target;
    reg_id_t reg_target = REG_XCX;

    /* Convert indirect branches (that are not returns).  Again, the
     * jump to the exit_stub that jumps to indirect_branch_lookup
     * was already inserted into the instr list by interp. */

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX, MANGLE_XCX_SPILL_SLOT,
                                 XCX_OFFSET, REG_R9));

    /* change: jmp /4, i_Ev -> movl i_Ev, %xcx */
    target = instr_get_target(instr);
    if (instr_get_opcode(instr) == OP_jmp_far_ind) {
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far indirect jump");
        STATS_INC(num_far_ind_jmps);
        reg_target = mangle_far_indirect_helper(dcontext, ilist, instr, next_instr, flags,
                                                &target);
    }
#ifdef UNIX
    /* i#107, mangle the memory reference opnd that uses segment register. */
    if (INTERNAL_OPTION(mangle_app_seg) && opnd_is_far_base_disp(target)) {
        /* TODO i#107: We use REG_XCX to store the segment base, which might be used
         * in "target" and cause failure in mangle_seg_ref_opnd.
         * We need to spill another register in that case.
         */
        ASSERT_BUG_NUM(107,
                       !opnd_uses_reg(target, REG_XCX) ||
                           (opnd_get_segment(target) != SEG_FS &&
                            opnd_get_segment(target) != SEG_GS));
        target = mangle_seg_ref_opnd(dcontext, ilist, instr, target, REG_XCX);
    }
#endif
    /* cannot call instr_reset, it will kill prev & next ptrs */
    instr_free(dcontext, instr);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_opcode(instr, opnd_get_size(target) == OPSZ_2 ? OP_movzx : OP_mov_ld);
    instr_set_dst(instr, 0, opnd_create_reg(reg_target));
    instr_set_src(instr, 0, target); /* src stays the same */
    if (instrlist_get_translation_target(ilist) != NULL) {
        /* make sure original raw bits are used for translation */
        instr_set_translation(instr, instr_get_raw_bits(instr));
    }
    instr_set_our_mangling(instr, true);

    /* It's impossible for our register stealing to use ecx
     * because no branch can simultaneously use 3 registers, right?
     * Maximum is 2, in something like "jmp *(edi,ebx,4)"?
     * If it is possible, need to make sure stealing's use of ecx
     * doesn't conflict w/ our use = FIXME
     */
    return next_instr;
}

/***************************************************************************
 * FAR DIRECT JUMP
 */
void
mangle_far_direct_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       instr_t *next_instr, uint flags)
{
    SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct jmp");
    STATS_INC(num_far_dir_jmps);

    mangle_far_direct_helper(dcontext, ilist, instr, next_instr, flags);
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
}

/***************************************************************************
 * SYSCALL
 */

#ifdef UNIX
/* Inserts code to handle clone into ilist.
 * instr is the syscall instr itself.
 * Assumes that instructions exist beyond instr in ilist.
 *
 * CAUTION: don't use a lot of stack in the generated code because
 *          get_clone_record() makes assumptions about the usage of stack being
 *          less than a page.
 */
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist,
                         instr_t *instr _IF_X64(gencode_mode_t mode))
{
    /*    int 0x80
     *    xchg xax,xcx
     *    jecxz child
     *    jmp parent
     *  child:
     *    xchg xax,xcx
     *    # i#149/PR 403015: the child is on the dstack so no need to swap stacks
     *    jmp new_thread_dynamo_start
     *  parent:
     *    xchg xax,xcx
     *    <post system call, etc.>
     */
    instr_t *in = instr_get_next(instr);
    instr_t *child = INSTR_CREATE_label(dcontext);
    instr_t *parent = INSTR_CREATE_label(dcontext);
    ASSERT(in != NULL);
    PRE(ilist, in,
        INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XCX)));
    PRE(ilist, in, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(child)));
    PRE(ilist, in, INSTR_CREATE_jmp(dcontext, opnd_create_instr(parent)));

    PRE(ilist, in, child);
    PRE(ilist, in,
        INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XCX)));
    /* We used to insert this directly into fragments for inlined system
     * calls, but not once we eliminated clean calls out of the DR cache
     * for security purposes.  Thus it can be a meta jmp, or an indirect jmp.
     */
    insert_reachable_cti(dcontext, ilist, in, vmcode_get_start(),
                         (byte *)get_new_thread_start(dcontext _IF_X64(mode)),
                         true /*jmp*/, false /*!returns*/, false /*!precise*/,
                         DR_REG_NULL /*no scratch*/, NULL);
    instr_set_meta(instr_get_prev(in));
    PRE(ilist, in, parent);
    PRE(ilist, in,
        INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XCX)));
}
#endif /* UNIX */

#ifdef WINDOWS
/* Note that ignore syscalls processing for XP and 2003 is a two-phase operation.
 * For this reason, mangle_syscall() might be called with a 'next_instr' that's
 * not an original app instruction but one inserted by the earlier mangling phase.
 */
#endif
/* XXX: any extra code here can interfere with mangle_syscall_code()
 * and interrupted_inlined_syscall() which have assumptions about the
 * exact code around inlined system calls.
 */
void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr,
                    instr_t *next_instr)
{
#ifdef UNIX
    /* Shared routine already checked method, handled INSTR_NI_SYSCALL*,
     * and inserted the signal barrier and non-auto-restart nop.
     * If we get here, we're dealing with an ignorable syscall.
     */

#    ifdef MACOS
    if (instr_get_opcode(instr) == OP_sysenter) {
        /* The kernel returns control to whatever user-mode places in edx.
         * We get control back here and then go to the ret ibl (since normally
         * there's a call to a shared routine that does "pop edx").
         */
        instr_t *post_sysenter = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
        instrlist_insert_mov_instr_addr(dcontext, post_sysenter, NULL /*in cache*/,
                                        opnd_create_reg(REG_XDX), ilist, instr, NULL,
                                        NULL);
        /* sysenter goes here */
        PRE(ilist, next_instr, post_sysenter);
        /* XXX i#3307: unimplemented, we can only support simple mangling cases in
         * mangling epilogue.
         */
        PRE(ilist, next_instr,
            RESTORE_FROM_DC_OR_TLS(dcontext, flags, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
        PRE(ilist, next_instr,
            SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
        PRE(ilist, next_instr,
            INSTR_CREATE_mov_st(dcontext, opnd_create_reg(REG_XCX),
                                opnd_create_reg(REG_XDX)));
    } else if (TEST(INSTR_BRANCH_SPECIAL_EXIT, instr->flags)) {
        int num = instr_get_interrupt_number(instr);
        ASSERT(instr_get_opcode(instr) == OP_int);
        if (num == 0x81 || num == 0x82) {
            int reason = (num == 0x81) ? EXIT_REASON_NI_SYSCALL_INT_0x81
                                       : EXIT_REASON_NI_SYSCALL_INT_0x82;
            if (DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, flags)) {
                insert_shared_get_dcontext(dcontext, ilist, instr, true /*save_xdi*/);
                PRE(ilist, instr,
                    INSTR_CREATE_mov_st(
                        dcontext,
                        opnd_create_dcontext_field_via_reg_sz(
                            dcontext, REG_NULL /*default*/, EXIT_REASON_OFFSET, OPSZ_2),
                        OPND_CREATE_INT16(reason)));
                insert_shared_restore_dcontext_reg(dcontext, ilist, instr);
            } else {
                PRE(ilist, instr,
                    instr_create_save_immed16_to_dcontext(dcontext, reason,
                                                          EXIT_REASON_OFFSET));
            }
        }
    }
#    endif

#else /* WINDOWS */
    /* special handling of system calls is performed in shared_syscall or
     * in do_syscall
     */

    /* FIXME: for ignorable syscalls,
     * do we need support for exiting mid-fragment prior to a syscall
     * like we do on Linux, to bound time in cache?
     */

    if (does_syscall_ret_to_callsite()) {
        uint len = instr_length(dcontext, instr);
        if (TEST(INSTR_SHARED_SYSCALL, instr->flags)) {
            ASSERT(DYNAMO_OPTION(shared_syscalls));
            /* this syscall will be performed by the shared_syscall code
             * we just need to place a return address into the dcontext
             * xsi slot or the mangle-next-tag tls slot
             */
            if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
#    ifdef X64
                ASSERT(instr_raw_bits_valid(instr));
                /* PR 244741: no 64-bit store-immed-to-mem
                 * FIXME: would be nice to move this to the stub and
                 * use the dead rbx register!
                 */
                PRE(ilist, instr,
                    instr_create_save_to_tls(dcontext, REG_XCX, MANGLE_NEXT_TAG_SLOT));
                PRE(ilist, instr,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                         OPND_CREATE_INTPTR((instr->bytes + len))));
                PRE(ilist, instr,
                    INSTR_CREATE_xchg(
                        dcontext,
                        opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT)),
                        opnd_create_reg(REG_XCX)));
#    else
                PRE(ilist, instr,
                    INSTR_CREATE_mov_st(
                        dcontext,
                        opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT)),
                        OPND_CREATE_INTPTR((instr->bytes + len))));
#    endif
            } else {
                PRE(ilist, instr,
                    instr_create_save_immed32_to_dcontext(
                        dcontext, (uint)(ptr_uint_t)(instr->bytes + len), XSI_OFFSET));
            }
        }
        /* Handle ignorable syscall.  non-ignorable system calls are
         * destroyed and removed from the list at the end of this func.
         */
        else if (!TEST(INSTR_NI_SYSCALL, instr->flags)) {
            if (get_syscall_method() == SYSCALL_METHOD_INT && DYNAMO_OPTION(sygate_int)) {
                /* for Sygate need to mangle into a call to int_syscall_addr
                 * is anyone going to get screwed up by this change
                 * (say flags change?) [-ignore_syscalls only]*/
                ASSERT_NOT_TESTED();
                instrlist_replace(ilist, instr, create_syscall_instr(dcontext));
                instr_destroy(dcontext, instr);
            } else if (get_syscall_method() == SYSCALL_METHOD_SYSCALL)
                ASSERT_NOT_TESTED();
            else if (get_syscall_method() == SYSCALL_METHOD_WOW64)
                ASSERT_NOT_TESTED();
            return;
        }
    } else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* on XP/2003 we have a choice between inserting a trampoline at the
         * return pt of the sysenter, which is 0x7ffe0304 (except for
         * SP2-patched XP), which is bad since it would clobber whatever's after
         * the ret there (unless we used a 0xcc, like Visual Studio 2005 debugger
         * does), or replacing the ret addr on the stack -- we choose the
         * latter as the lesser of two transparency evils. Note that the
         * page at 0x7ffe0000 can't be made writable anyway, so hooking
         * isn't possible.
         */
        if (TEST(INSTR_SHARED_SYSCALL, instr->flags)) {
            ASSERT(DYNAMO_OPTION(shared_syscalls));
        }
        /* Handle ignorable syscall.  non-ignorable system calls are
         * destroyed and removed from the list at the end of this func.
         */
        else if (!TEST(INSTR_NI_SYSCALL, instr->flags)) {

            instr_t *mov_imm;

            /* even w/ ignorable syscall, need to make sure regain control */
            ASSERT(next_instr != NULL);
            ASSERT(DYNAMO_OPTION(indcall2direct));
            /* for sygate hack need to basically duplicate what is done in
             * shared_syscall, but here we could be shared so would need to
             * grab dcontext first etc. */
            ASSERT_NOT_IMPLEMENTED(!DYNAMO_OPTION(sygate_sysenter));
            /* PR 253943: we don't support sysenter in x64 */
            IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* can't have 8-byte imm-to-mem */
            /* FIXME PR 303413: we won't properly translate a fault in our
             * app stack reference here.  It's marked as our own mangling
             * so we'll at least return failure from our translate routine.
             */
            mov_imm = INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 0),
                                          opnd_create_instr(next_instr));
            ASSERT(instr_is_mov_imm_to_tos(mov_imm));
            PRE(ilist, instr, mov_imm);
            /* do not let any encoding for length be cached!
             * o/w will lose pc-relative opnd
             */
            /* 'next_instr' is executed after the after-syscall vsyscall
             * 'ret', which is executed natively. */
            instr_set_meta(instr_get_prev(instr));
            return; /* leave syscall instr alone */
        }
    } else {
        SYSLOG_INTERNAL_ERROR("unsupported system call method");
        LOG(THREAD, LOG_INTERP, 1, "don't know convention for this syscall method\n");
        if (!TEST(INSTR_NI_SYSCALL, instr->flags))
            return;
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* destroy the syscall instruction */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
#endif /* WINDOWS */
}

/***************************************************************************
 * NON-SYSCALL INTERRUPT
 */
void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
#ifdef WINDOWS
    int num;
    if (instr_get_opcode(instr) != OP_int)
        return;
    num = instr_get_interrupt_number(instr);
    if (num == 0x2b) {
        /* A callback finishes and returns to the interruption
         * point of the thread with the instruction "int 2b".
         * The interrupt ends the block; remove the instruction
         * since we'll come back to dynamo to perform the
         * interrupt.
         */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    }
#endif /* WINDOWS */
}

/***************************************************************************
 * Single step exceptions catching
 */
void
mangle_possible_single_step(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* Simply inserts two nops so that next instruction where a single step
     * exception might occur is in the same basic block and so that the
     * translation of a single step exception points back to the instruction
     * which set the trap flag.
     * The single step exception is a problem because
     * the ExceptionAddress should be the next EIP.
     */
    POST(ilist, instr, INSTR_CREATE_nop(dcontext));
    /* Inserting two nops to get ExceptionAddress on the second one. */
    POST(ilist, instr, INSTR_CREATE_nop(dcontext));
}

/***************************************************************************
 * Single step exceptions generation
 */
void
mangle_single_step(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr)
{
    /* Sets exit reason dynamically. */
    if (DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, flags)) {
        insert_shared_get_dcontext(dcontext, ilist, instr, true /*save_xdi*/);
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(
                dcontext,
                opnd_create_dcontext_field_via_reg_sz(dcontext, REG_NULL /*default*/,
                                                      EXIT_REASON_OFFSET, OPSZ_2),
                OPND_CREATE_INT16(EXIT_REASON_SINGLE_STEP)));
        insert_shared_restore_dcontext_reg(dcontext, ilist, instr);
    } else {
        PRE(ilist, instr,
            instr_create_save_immed16_to_dcontext(dcontext, EXIT_REASON_SINGLE_STEP,
                                                  EXIT_REASON_OFFSET));
    }
}

/***************************************************************************
 * FLOATING POINT PC
 */

/* The offset of the last floating point PC in the saved state */
#define FNSAVE_PC_OFFS 12
#define FXSAVE_PC_OFFS 8
#define FXSAVE_SIZE 512

void
float_pc_update(dcontext_t *dcontext)
{
    byte *state = *(byte **)(((byte *)dcontext->local_state) + FLOAT_PC_STATE_SLOT);
    app_pc orig_pc, xl8_pc;
    uint offs = 0;
    LOG(THREAD, LOG_INTERP, 2, "%s: fp state " PFX "\n", __FUNCTION__, state);
    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE ||
        dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE64) {
        /* Check whether the FPU state was saved */
        uint64 header_bv = *(uint64 *)(state + FXSAVE_SIZE);
        if (!TEST(XCR0_FP, header_bv)) {
            LOG(THREAD, LOG_INTERP, 2, "%s: xsave did not save FP state => nop\n",
                __FUNCTION__);
        }
        return;
    }

    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_FNSAVE) {
        offs = FNSAVE_PC_OFFS;
    } else {
        offs = FXSAVE_PC_OFFS;
    }
    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_FXSAVE64 ||
        dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE64)
        orig_pc = *(app_pc *)(state + offs);
    else /* just bottom 32 bits of pc */
        orig_pc = (app_pc)(ptr_uint_t) * (uint *)(state + offs);
    if (orig_pc == NULL) {
        /* no fp instr yet */
        LOG(THREAD, LOG_INTERP, 2, "%s: pc is NULL\n", __FUNCTION__);
        return;
    }
    /* i#1211-c#1: the orig_pc might be an app pc restored from fldenv */
    if (!in_fcache(orig_pc) &&
        /* XXX: i#698: there might be fp instr neither in fcache nor in app */
        !(in_generated_routine(dcontext, orig_pc) || is_dynamo_address(orig_pc) ||
          is_in_dynamo_dll(orig_pc) || is_in_client_lib(orig_pc))) {
        bool no_xl8 = true;
#ifdef X64
        if (dcontext->upcontext.upcontext.exit_reason != EXIT_REASON_FLOAT_PC_FXSAVE64 &&
            dcontext->upcontext.upcontext.exit_reason != EXIT_REASON_FLOAT_PC_XSAVE64) {
            /* i#1427: try to fill in the top 32 bits */
            ptr_uint_t vmcode = (ptr_uint_t)vmcode_get_start();
            if ((vmcode & 0xffffffff00000000) > 0) {
                byte *orig_try =
                    (byte *)((vmcode & 0xffffffff00000000) | (ptr_uint_t)orig_pc);
                if (in_fcache(orig_try)) {
                    LOG(THREAD, LOG_INTERP, 2,
                        "%s: speculating: pc " PFX " + top half of vmcode = " PFX "\n",
                        __FUNCTION__, orig_pc, orig_try);
                    orig_pc = orig_try;
                    no_xl8 = false;
                }
            }
        }
#endif
        if (no_xl8) {
            LOG(THREAD, LOG_INTERP, 2, "%s: pc " PFX " is translated already\n",
                __FUNCTION__, orig_pc);
            return;
        }
    }
    /* We must either grab thread_initexit_lock or be couldbelinking to translate */
    d_r_mutex_lock(&thread_initexit_lock);
    xl8_pc = recreate_app_pc(dcontext, orig_pc, NULL);
    d_r_mutex_unlock(&thread_initexit_lock);
    LOG(THREAD, LOG_INTERP, 2, "%s: translated " PFX " to " PFX "\n", __FUNCTION__,
        orig_pc, xl8_pc);

    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_FXSAVE64 ||
        dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE64)
        *(app_pc *)(state + offs) = xl8_pc;
    else /* just bottom 32 bits of pc */
        *(uint *)(state + offs) = (uint)(ptr_uint_t)xl8_pc;
}

void
mangle_float_pc(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr, uint *flags INOUT)
{
    /* If there is a prior non-control float instr, we can inline the pc update.
     * Otherwise, we go back to d_r_dispatch.  In the latter case we do not support
     * building traces across the float pc save: we assume it's rare.
     */
    app_pc prior_float = NULL;
    bool exit_is_normal = false;
    int op = instr_get_opcode(instr);
    opnd_t memop = instr_get_dst(instr, 0);
    ASSERT(opnd_is_memory_reference(memop));

    /* To simplify the code here we don't support rip-rel for local handling.
     * We also don't support xsave, as it optionally writes the fpstate.
     */
    if (opnd_is_base_disp(memop) && op != OP_xsave32 && op != OP_xsaveopt32 &&
        op != OP_xsave64 && op != OP_xsaveopt64 && op != OP_xsavec32 &&
        op != OP_xsavec64) {
        instr_t *prev;
        for (prev = instr_get_prev_expanded(dcontext, ilist, instr); prev != NULL;
             prev = instr_get_prev_expanded(dcontext, ilist, prev)) {
            dr_fp_type_t type;
            if (instr_is_app(prev) && instr_is_floating_ex(prev, &type)) {
                bool control_instr = false;
                if (type == DR_FP_STATE /* quick check */ &&
                    /* Check the list from Intel Vol 1 8.1.8 */
                    (op == OP_fnclex || op == OP_fldcw || op == OP_fnstcw ||
                     op == OP_fnstsw || op == OP_fnstenv || op == OP_fldenv ||
                     op == OP_fwait))
                    control_instr = true;
                if (!control_instr) {
                    prior_float = get_app_instr_xl8(prev);
                    break;
                }
            }
        }
    }

    if (prior_float != NULL) {
        /* We can link this */
        exit_is_normal = true;
        STATS_INC(float_pc_from_cache);

        /* Replace the stored code cache pc with the original app pc.
         * If the app memory is unwritable, instr would have already crashed.
         */
        if (op == OP_fnsave || op == OP_fnstenv) {
            opnd_set_disp(&memop, opnd_get_disp(memop) + FNSAVE_PC_OFFS);
            opnd_set_size(&memop, OPSZ_4);
            PRE(ilist, next_instr,
                INSTR_CREATE_mov_st(dcontext, memop,
                                    OPND_CREATE_INT32((int)(ptr_int_t)prior_float)));
        } else if (op == OP_fxsave32) {
            opnd_set_disp(&memop, opnd_get_disp(memop) + FXSAVE_PC_OFFS);
            opnd_set_size(&memop, OPSZ_4);
            PRE(ilist, next_instr,
                INSTR_CREATE_mov_st(dcontext, memop,
                                    OPND_CREATE_INT32((int)(ptr_int_t)prior_float)));
        } else if (op == OP_fxsave64) {
            opnd_set_disp(&memop, opnd_get_disp(memop) + FXSAVE_PC_OFFS);
            opnd_set_size(&memop, OPSZ_8);
            insert_mov_immed_ptrsz(dcontext, (ptr_int_t)prior_float, memop, ilist,
                                   next_instr, NULL, NULL);
        } else
            ASSERT_NOT_REACHED();
    } else if (!DYNAMO_OPTION(translate_fpu_pc)) {
        /* We only support translating when inlined.
         * XXX: we can't recover the loss of coarse-grained: we live with that.
         */
        exit_is_normal = true;
        ASSERT_CURIOSITY(!TEST(FRAG_CANNOT_BE_TRACE, *flags) ||
                         /* i#1562: it could be marked as no-trace for other reasons */
                         TEST(FRAG_SELFMOD_SANDBOXED, *flags));
    } else {
        int reason = 0;
        CLIENT_ASSERT(!TEST(FRAG_IS_TRACE, *flags),
                      "removing an FPU instr in a trace with an FPU state save "
                      "is not supported");
        switch (op) {
        case OP_fnsave:
        case OP_fnstenv: reason = EXIT_REASON_FLOAT_PC_FNSAVE; break;
        case OP_fxsave32: reason = EXIT_REASON_FLOAT_PC_FXSAVE; break;
        case OP_fxsave64: reason = EXIT_REASON_FLOAT_PC_FXSAVE64; break;
        case OP_xsave32:
        case OP_xsavec32:
        case OP_xsaveopt32: reason = EXIT_REASON_FLOAT_PC_XSAVE; break;
        case OP_xsave64:
        case OP_xsavec64:
        case OP_xsaveopt64: reason = EXIT_REASON_FLOAT_PC_XSAVE64; break;
        default: ASSERT_NOT_REACHED();
        }
        if (DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, *flags)) {
            insert_shared_get_dcontext(dcontext, ilist, instr, true /*save_xdi*/);
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(
                    dcontext,
                    opnd_create_dcontext_field_via_reg_sz(dcontext, REG_NULL /*default*/,
                                                          EXIT_REASON_OFFSET, OPSZ_2),
                    OPND_CREATE_INT16(reason)));
        } else {
            PRE(ilist, instr,
                instr_create_save_immed16_to_dcontext(dcontext, reason,
                                                      EXIT_REASON_OFFSET));
            PRE(ilist, instr,
                instr_create_save_to_tls(dcontext, REG_XDI, DCONTEXT_BASE_SPILL_SLOT));
        }
        /* At this point, xdi is spilled into DCONTEXT_BASE_SPILL_SLOT */

        /* We pass the address in the xbx tls slot, which is untouched by fcache_return.
         *
         * XXX: handle far refs!  Xref drutil_insert_get_mem_addr(), and sandbox_write()
         * hitting this same issue.
         */
        ASSERT_CURIOSITY(!opnd_is_far_memory_reference(memop));
        if (opnd_is_base_disp(memop)) {
            opnd_set_size(&memop, OPSZ_lea);
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XDI), memop));
        } else {
            ASSERT(opnd_is_abs_addr(memop) IF_X64(|| opnd_is_rel_addr(memop)));
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                                     OPND_CREATE_INTPTR(opnd_get_addr(memop))));
        }
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, REG_XDI, FLOAT_PC_STATE_SLOT));

        /* Restore app %xdi */
        if (TEST(FRAG_SHARED, *flags))
            insert_shared_restore_dcontext_reg(dcontext, ilist, instr);
        else {
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, REG_XDI,
                                              DCONTEXT_BASE_SPILL_SLOT));
        }
    }

    if (exit_is_normal && DYNAMO_OPTION(translate_fpu_pc)) {
        instr_t *exit_jmp = next_instr;
        while (exit_jmp != NULL && !instr_is_exit_cti(exit_jmp))
            exit_jmp = instr_get_next(next_instr);
        ASSERT(exit_jmp != NULL);
        ASSERT(instr_branch_special_exit(exit_jmp));
        instr_branch_set_special_exit(exit_jmp, false);
        /* XXX: there could be some other reason this was marked
         * cannot-be-trace that we're undoing here...
         */
        if (TEST(FRAG_CANNOT_BE_TRACE, *flags))
            *flags &= ~FRAG_CANNOT_BE_TRACE;
    }
}

/***************************************************************************
 * CPUID FOOLING
 */
#ifdef FOOL_CPUID

/* values returned by cpuid for Mobile Pentium MMX processor (family 5, model 8)
 * minus mmx (==0x00800000 in CPUID_1_EDX)
 * FIXME: change model number to a Pentium w/o MMX!
 */
#    define CPUID_0_EAX 0x00000001
#    define CPUID_0_EBX 0x756e6547
#    define CPUID_0_ECX 0x6c65746e
#    define CPUID_0_EDX 0x49656e69
/* extended family, extended model, type, family, model, stepping id: */
/* 20:27,           16:19,          12:13, 8:11,  4:7,   0:3    */
#    define CPUID_1_EAX 0x00000581
#    define CPUID_1_EBX 0x00000000
#    define CPUID_1_ECX 0x00000000
#    define CPUID_1_EDX 0x000001bf

static void
mangle_cpuid(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
             instr_t *next_instr)
{
    /* assumption: input value is put in eax on prev instr, or
     * on instr prior to that and prev is an inc instr.
     * alternative is to insert conditional branch...and save eflags, etc.
     */
    instr_t *prev = instr_get_prev(instr);
    opnd_t op;
    int input, out_eax, out_ebx, out_ecx, out_edx;

    LOG(THREAD, LOG_INTERP, 1, "fooling cpuid instruction!\n");

    ASSERT(prev != NULL);
    prev = instr_get_prev_expanded(dcontext, ilist, instr);
    instr_decode(dcontext, instr);
    if (!instr_valid(instr))
        goto cpuid_give_up;
    d_r_loginst(dcontext, 2, prev, "prior to cpuid");

    /* FIXME: maybe should insert code to dispatch on eax, rather than
     * this hack, which is based on photoshop, which either does
     * "xor eax,eax" or "xor eax,eax; inc eax"
     */
    if (!instr_is_mov_constant(prev, &input)) {
        /* we only allow inc here */
        if (instr_get_opcode(prev) != OP_inc)
            goto cpuid_give_up;
        op = instr_get_dst(prev, 0);
        if (!opnd_is_reg(op) || opnd_get_reg(op) != REG_EAX)
            goto cpuid_give_up;
        /* now check instr before inc */
        prev = instr_get_prev(prev);
        if (!instr_is_mov_constant(prev, &input) || input != 0)
            goto cpuid_give_up;
        input = 1;
        /* now check that mov 0 is into eax */
    }
    if (instr_num_dsts(prev) == 0)
        goto cpuid_give_up;
    op = instr_get_dst(prev, 0);
    if (!opnd_is_reg(op) || opnd_get_reg(op) != REG_EAX)
        goto cpuid_give_up;

    if (input == 0) {
        out_eax = CPUID_0_EAX;
        out_ebx = CPUID_0_EBX;
        out_ecx = CPUID_0_ECX;
        out_edx = CPUID_0_EDX;
    } else {
        /* 1 or anything higher all return same info */
        out_eax = CPUID_1_EAX;
        out_ebx = CPUID_1_EBX;
        out_ecx = CPUID_1_ECX;
        out_edx = CPUID_1_EDX;
    }

    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                             OPND_CREATE_INT32(out_eax)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                             OPND_CREATE_INT32(out_ebx)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX),
                             OPND_CREATE_INT32(out_ecx)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EDX),
                             OPND_CREATE_INT32(out_edx)));

    /* destroy the cpuid instruction */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return;
cpuid_give_up:
    LOG(THREAD, LOG_INTERP, 1, "\tcpuid fool: giving up\n");
    return;
}
#endif /* FOOL_CPUID */

void
mangle_exit_cti_prefixes(dcontext_t *dcontext, instr_t *instr)
{
    uint prefixes = instr_get_prefixes(instr);
    if (prefixes != 0) {
        bool remove = false;
        /* Case 8738: while for transparency it would be best to maintain all
         * prefixes, our patching and other routines make assumptions about
         * the length of exit ctis.  Plus our elision removes the whole
         * instr in any case.
         */
        if (instr_is_cbr(instr)) {
            if (TESTANY(~(PREFIX_JCC_TAKEN | PREFIX_JCC_NOT_TAKEN), prefixes)) {
                remove = true;
                prefixes &= (PREFIX_JCC_TAKEN | PREFIX_JCC_NOT_TAKEN);
            }
        } else {
            /* prefixes on ubr or mbr should be nops and for ubr will mess up
             * our size assumptions so drop them (i#435)
             */
            remove = true;
            prefixes = 0;
        }
        if (remove) {
            LOG(THREAD, LOG_INTERP, 4,
                "\tremoving unknown prefixes " PFX " from " PFX "\n", prefixes,
                instr_get_raw_bits(instr));
            ASSERT(instr_operands_valid(instr)); /* ensure will encode w/o raw bits */
            instr_set_prefixes(instr, prefixes);
        }
    } else if ((instr_get_opcode(instr) == OP_jmp &&
                instr_length(dcontext, instr) > JMP_LONG_LENGTH) ||
               (instr_is_cbr(instr) && instr_length(dcontext, instr) > CBR_LONG_LENGTH)) {
        /* i#1988: remove MPX prefixes as they mess up our nop padding.
         * i#1312 covers marking as actual prefixes, and we should keep them.
         */
        LOG(THREAD, LOG_INTERP, 4, "\tremoving unknown jmp prefixes from " PFX "\n",
            instr_get_raw_bits(instr));
        instr_set_raw_bits_valid(instr, false);
    }
}

#ifdef X64
/* PR 215397: re-relativize rip-relative data addresses */
/* Should return NULL if it destroy "instr".  We don't support both destroying
 * (done only for x86: i#393) and changing next_instr (done only for ARM).
 */
instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    uint opc = instr_get_opcode(instr);
    app_pc tgt;
    opnd_t dst, src;
    ASSERT(instr_has_rel_addr_reference(instr));
    instr_get_rel_addr_target(instr, &tgt);
    STATS_INC(rip_rel_instrs);
#    ifdef RCT_IND_BRANCH
    if (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
        TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) {
        /* PR 215408: record addresses taken via rip-relative instrs */
        rct_add_rip_rel_addr(dcontext, tgt _IF_DEBUG(instr_get_translation(instr)));
    }
#    endif
    if (opc == OP_lea) {
        /* We leave this as rip-rel if it still reaches from the code cache. */
        if (!rel32_reachable_from_vmcode(tgt)) {
            /* segment overrides are ignored on lea */
            opnd_t immed;
            dst = instr_get_dst(instr, 0);
            src = instr_get_src(instr, 0);
            ASSERT(opnd_is_reg(dst));
            ASSERT(opnd_is_rel_addr(src));
            ASSERT(opnd_get_addr(src) == tgt);
            /* Replace w/ an absolute immed of the target app address, following Intel
             * Table 3-59 "64-bit Mode LEA Operation with Address and Operand Size
             * Attributes" */
            if (reg_get_size(opnd_get_reg(dst)) == OPSZ_8) {
                /* PR 253327: there is no explicit addr32 marker; we assume
                 * that decode or the user already zeroed out the top bits
                 * if there was an addr32 prefix byte or the user wants
                 * that effect */
                immed = OPND_CREATE_INTPTR((ptr_int_t)tgt);
            } else if (reg_get_size(opnd_get_reg(dst)) == OPSZ_4)
                immed = OPND_CREATE_INT32((int)(ptr_int_t)tgt);
            else {
                ASSERT(reg_get_size(opnd_get_reg(dst)) == OPSZ_2);
                immed = OPND_CREATE_INT16((short)(ptr_int_t)tgt);
            }
            PRE(ilist, instr, INSTR_CREATE_mov_imm(dcontext, dst, immed));
            instrlist_remove(ilist, instr);
            instr_destroy(dcontext, instr);
            STATS_INC(rip_rel_lea);
            DOSTATS({
                if (tgt >= get_application_base() && tgt < get_application_end())
                    STATS_INC(rip_rel_app_lea);
            });
            return NULL; /* == destroyed instr */
        }
        return next_instr;
    } else {
        /* PR 251479 will automatically re-relativize if it reaches,
         * but if it doesn't we need to handle that here (since that
         * involves an encoding length change, which complicates many
         * use cases if done at instr encode time).
         * We don't yet know exactly where we're going to encode this bb,
         * so we're conservative and check for all reachability from our
         * heap (assumed to be a single heap: xref PR 215395, and xref
         * potential secondary code caches PR 253446.
         */
        if (!rel32_reachable_from_vmcode(tgt)) {
            int si = -1, di = -1;
            opnd_t relop, newop;
            bool spill = true;
            /* FIXME PR 253446: for mbr, should share the xcx spill */
            reg_id_t scratch_reg = REG_XAX;
            si = instr_get_rel_addr_src_idx(instr);
            di = instr_get_rel_addr_dst_idx(instr);
            if (si >= 0) {
                relop = instr_get_src(instr, si);
                ASSERT(di < 0 || opnd_same(relop, instr_get_dst(instr, di)));
                /* If it's a load (OP_mov_ld, or OP_movzx, etc.), use dead reg */
                if (instr_num_srcs(instr) == 1 && /* src is the rip-rel opnd */
                    instr_num_dsts(instr) == 1 && /* only one dest: a register */
                    opnd_is_reg(instr_get_dst(instr, 0)) && !instr_is_predicated(instr)) {
                    opnd_size_t sz = opnd_get_size(instr_get_dst(instr, 0));
                    reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
                    /* if target is 16 or 8 bit sub-register the whole reg is not dead
                     * (for 32-bit, top 32 bits are cleared) */
                    if (reg_is_gpr(reg) && (reg_is_32bit(reg) || reg_is_64bit(reg))) {
                        spill = false;
                        scratch_reg = opnd_get_reg(instr_get_dst(instr, 0));
                        if (sz == OPSZ_4)
                            scratch_reg = reg_32_to_64(scratch_reg);
                        /* we checked all opnds: should not read reg */
                        ASSERT(
                            !instr_reads_from_reg(instr, scratch_reg, DR_QUERY_DEFAULT));
                        STATS_INC(rip_rel_unreachable_nospill);
                    }
                }
            } else {
                relop = instr_get_dst(instr, di);
            }
            /* PR 263369: we can't just look for instr_reads_from_reg here since
             * our no-spill optimization above may miss some writes.
             */
            if (spill && instr_uses_reg(instr, scratch_reg)) {
                /* mbr (for which we'll use xcx once we optimize) should not
                 * get here: can't use registers (except xsp) */
                ASSERT(scratch_reg == REG_XAX);
                do {
                    scratch_reg++;
                    ASSERT(scratch_reg <= REG_STOP_64);
                } while (instr_uses_reg(instr, scratch_reg));
            }
            ASSERT(!instr_reads_from_reg(instr, scratch_reg, DR_QUERY_DEFAULT));
            ASSERT(!spill || !instr_writes_to_reg(instr, scratch_reg, DR_QUERY_DEFAULT));
            /* XXX PR 253446: Optimize by looking ahead for dead registers, and
             * sharing single spill across whole bb, or possibly building local code
             * cache to avoid unreachability: all depending on how many rip-rel
             * instrs we see.  We'll watch the stats.
             */
            if (spill) {
                PRE(ilist, instr,
                    SAVE_TO_DC_OR_TLS(dcontext, 0, scratch_reg, MANGLE_RIPREL_SPILL_SLOT,
                                      XAX_OFFSET));
            }
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(scratch_reg),
                                     OPND_CREATE_INTPTR((ptr_int_t)tgt)));

            newop = opnd_create_far_base_disp(opnd_get_segment(relop), scratch_reg,
                                              REG_NULL, 0, 0, opnd_get_size(relop));
            if (si >= 0)
                instr_set_src(instr, si, newop);
            if (di >= 0)
                instr_set_dst(instr, di, newop);
            /* we need the whole spill...restore region to all be marked mangle */
            instr_set_our_mangling(instr, true);
            if (spill) {
                /* We are making several assumptions here. Firstly, we assume that any
                 * instruction in the mangling code of any control-flow app instruction
                 * is always before the last commit point of the app instruction, i.e.
                 * does not xl8 to a PC post app instruction. This should be safe to
                 * assume for any control-flow instruction. We therefore do not mark the
                 * rip-rel related restores here as 'epilogue'. Secondly, we assume that
                 * no instructions in mangled code that require xsp adjustment to xl8 app
                 * state are instructions that can be fully rolled back. There is a check
                 * in translate_walk_restore that makes sure there is no xsp_adjust for
                 * instructions in mangling epilogue. Both of this includes instructions
                 * with further mangling after the rip-rel mangling code that require
                 * roll-back. We assume here that this is supported for all such
                 * instructions.
                 */
                instr_t *restore = instr_create_restore_from_tls(
                    dcontext, scratch_reg, MANGLE_RIPREL_SPILL_SLOT);
                PRE(ilist, next_instr,
                    instr_supports_simple_mangling_epilogue(dcontext, instr)
                        ? instr_set_translation_mangling_epilogue(dcontext, ilist,
                                                                  restore)
                        : restore);
            }
            STATS_INC(rip_rel_unreachable);
            DOSTATS({
                if (tgt >= get_application_base() && tgt < get_application_end())
                    STATS_INC(rip_rel_app_unreachable);
            });
        }
    }
    return next_instr;
}
#endif

/***************************************************************************
 * Reference with segment register (fs/gs)
 */
#ifdef UNIX
static int
instr_get_seg_ref_dst_idx(instr_t *instr)
{
    int i;
    opnd_t opnd;
    if (!instr_valid(instr))
        return -1;
    /* must go to level 3 operands */
    for (i = 0; i < instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (opnd_is_far_base_disp(opnd) &&
            (opnd_get_segment(opnd) == SEG_GS || opnd_get_segment(opnd) == SEG_FS))
            return i;
    }
    return -1;
}

static int
instr_get_seg_ref_src_idx(instr_t *instr)
{
    int i;
    opnd_t opnd;
    if (!instr_valid(instr))
        return -1;
    /* must go to level 3 operands */
    for (i = 0; i < instr_num_srcs(instr); i++) {
        opnd = instr_get_src(instr, i);
        if (opnd_is_far_base_disp(opnd) &&
            (opnd_get_segment(opnd) == SEG_GS || opnd_get_segment(opnd) == SEG_FS))
            return i;
    }
    return -1;
}

/* mangle the instruction OP_mov_seg, i.e. the instruction that
 * read/update the segment register.
 */
void
mangle_mov_seg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr)
{
    reg_id_t seg;
    opnd_t opnd, dst;
    opnd_size_t dst_sz;

    ASSERT(instr_get_opcode(instr) == OP_mov_seg);
    ASSERT(instr_num_srcs(instr) == 1);
    ASSERT(instr_num_dsts(instr) == 1);

    STATS_INC(app_mov_seg_mangled);
    /* for update, we simply change it to a nop because we will
     * update it when dynamorio entering code cache to execute
     * this basic block.
     */
    dst = instr_get_dst(instr, 0);
    if (opnd_is_reg(dst) && reg_is_segment(opnd_get_reg(dst))) {
        app_pc xl8;
        seg = opnd_get_reg(dst);
        if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
            return;
        /* must use the original instr, which might be used by caller */
        xl8 = get_app_instr_xl8(instr);
        instr_reuse(dcontext, instr);
        instr_set_opcode(instr, OP_nop);
        instr_set_num_opnds(dcontext, instr, 0, 0);
        instr_set_translation(instr, xl8);
        /* With no spills and just a single instr, no reason to set as our_mangling. */
        return;
    }

    /* for read seg, we mangle it */
    opnd = instr_get_src(instr, 0);
    ASSERT(opnd_is_reg(opnd));
    seg = opnd_get_reg(opnd);
    ASSERT(reg_is_segment(seg));
    if (seg != SEG_FS && seg != SEG_GS)
        return;
    if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
        return;

    /* There are two possible mov_seg instructions:
     * 8C/r           MOV r/m16,Sreg   Move segment register to r/m16
     * REX.W + 8C/r   MOV r/m64,Sreg   Move zero extended 16-bit segment
     *                                 register to r/m64
     * Note, In 32-bit mode, the assembler may insert the 16-bit operand-size
     * prefix with this instruction.
     */
    /* we cannot replace the instruction but only change it. */
    dst = instr_get_dst(instr, 0);
    dst_sz = opnd_get_size(dst);
    opnd =
        opnd_create_sized_tls_slot(os_tls_offset(os_get_app_tls_reg_offset(seg)), OPSZ_2);
    if (opnd_is_reg(dst)) { /* dst is a register */
        /* mov %gs:off => reg */
        instr_set_src(instr, 0, opnd);
        instr_set_opcode(instr, OP_mov_ld);
        if (dst_sz != OPSZ_2)
            instr_set_opcode(instr, OP_movzx);
        /* With no spills and just a single instr, no reason to set as our_mangling. */
    } else { /* dst is memory, need steal a register. */
        reg_id_t reg;
        instr_t *ti;
        for (reg = REG_XAX; reg < REG_XBX; reg++) {
            if (!instr_uses_reg(instr, reg))
                break;
        }
        /* We need save the register to corresponding slot for correct restore,
         * so only use the first four registers.
         */
        ASSERT(reg <= REG_XBX);
        /* save reg */
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, reg, tls_slots[reg - REG_XAX]));
        /* restore reg */
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, reg, tls_slots[reg - REG_XAX]));
        switch (dst_sz) {
        case OPSZ_8: IF_NOT_X64(ASSERT(false);) break;
        case OPSZ_4: IF_X64(reg = reg_64_to_32(reg);) break;
        case OPSZ_2:
            IF_X64(reg = reg_64_to_32(reg);)
            reg = reg_32_to_16(reg);
            break;
        default: ASSERT(false);
        }
        /* mov %gs:off => reg */
        ti = INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(reg), opnd);
        if (dst_sz != OPSZ_2)
            instr_set_opcode(ti, OP_movzx);
        PRE(ilist, instr, ti);
        /* change mov_seg to mov_st: mov reg => [mem] */
        instr_set_src(instr, 0, opnd_create_reg(reg));
        instr_set_opcode(instr, OP_mov_st);
        /* To handle xl8 for the spill/restore we need the app instr to be marked. */
        instr_set_our_mangling(instr, true);
    }
}

/* mangle the instruction that reference memory via segment register */
void
mangle_seg_ref(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr)
{
    int si = -1, di = -1;
    opnd_t segop, newop;
    bool spill = true;
    reg_id_t scratch_reg = REG_XAX, seg = REG_NULL;

    /* exit cti won't be seg ref */
    if (instr_is_exit_cti(instr))
        return;
    /* mbr will be handled separatly */
    if (instr_is_mbr(instr))
        return;
    if (instr_get_opcode(instr) == OP_lea)
        return;

    /* XXX: maybe using decode_cti and then a check on prefix could be
     * more efficient as it only examines a few byte and avoid fully decoding
     * the instruction. For simplicity, we examine every operands instead.
     */
    /* 1. get ref opnd */
    si = instr_get_seg_ref_src_idx(instr);
    di = instr_get_seg_ref_dst_idx(instr);
    if (si < 0 && di < 0)
        return;
    if (si >= 0) {
        segop = instr_get_src(instr, si);
        ASSERT(di < 0 || opnd_same(segop, instr_get_dst(instr, di)));
    } else {
        segop = instr_get_dst(instr, di);
    }
    seg = opnd_get_segment(segop);
    if (seg != SEG_GS && seg != SEG_FS)
        return;
    if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
        return;
    STATS_INC(app_seg_refs_mangled);

    DOLOG(3, LOG_INTERP,
          { d_r_loginst(dcontext, 3, instr, "reference with fs/gs segment"); });
    /* 2. decide the scratch reg */
    /* Opt: if it's a load (OP_mov_ld, or OP_movzx, etc.), use dead reg */
    if (si >= 0 && instr_num_srcs(instr) == 1 && /* src is the seg ref opnd */
        instr_num_dsts(instr) == 1 &&            /* only one dest: a register */
        opnd_is_reg(instr_get_dst(instr, 0)) && !instr_is_predicated(instr)) {
        reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
        /* if target is 16 or 8 bit sub-register the whole reg is not dead
         * (for 32-bit, top 32 bits are cleared) */
        if (reg_is_gpr(reg) && (reg_is_32bit(reg) || reg_is_64bit(reg)) &&
            /* mov [%fs:%xax] => %xax */
            !instr_reads_from_reg(instr, reg, DR_QUERY_DEFAULT) &&
            /* xsp cannot be an index reg. */
            reg != DR_REG_XSP) {
            spill = false;
            scratch_reg = reg;
#    ifdef X64
            if (opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_4)
                scratch_reg = reg_32_to_64(reg);
#    endif
        }
    }
    if (spill) {
        /* we pick a scratch register from XAX, XBX, XCX, or XDX
         * that has direct TLS slots.
         */
        for (scratch_reg = REG_XAX; scratch_reg <= REG_XBX; scratch_reg++) {
            /* the register must not be used by the instr, either read or write,
             * because we will mangle it when executing the instr (no read from),
             * and restore it after that instr (no write to).
             */
            if (!instr_uses_reg(instr, scratch_reg))
                break;
        }
        ASSERT(scratch_reg <= REG_XBX);
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, scratch_reg,
                                     tls_slots[scratch_reg - REG_XAX]));
    }
    newop = mangle_seg_ref_opnd(dcontext, ilist, instr, segop, scratch_reg);
    if (si >= 0)
        instr_set_src(instr, si, newop);
    if (di >= 0)
        instr_set_dst(instr, di, newop);
    /* we need the whole spill...restore region to all be marked mangle */
    instr_set_our_mangling(instr, true);
    /* FIXME: i#107 we should check the bound and raise signal if out of bound. */
    DOLOG(3, LOG_INTERP,
          { d_r_loginst(dcontext, 3, instr, "re-wrote app tls reference"); });

    if (spill) {
        PRE(ilist, next_instr,
            /* XXX i#3307: needs test. */
            instr_set_translation_mangling_epilogue(
                dcontext, ilist,
                instr_create_restore_from_tls(dcontext, scratch_reg,
                                              tls_slots[scratch_reg - REG_XAX])));
    }
}
#endif /* UNIX */

#ifdef ANNOTATIONS
/***************************************************************************
 * DR and Valgrind annotations
 */
void
mangle_annotation_helper(dcontext_t *dcontext, instr_t *label, instrlist_t *ilist)
{
    dr_instr_label_data_t *label_data = instr_get_label_data_area(label);
    dr_annotation_handler_t *handler = GET_ANNOTATION_HANDLER(label_data);
    dr_annotation_receiver_t *receiver = handler->receiver_list;
    opnd_t *args = NULL;

    ASSERT(handler->type == DR_ANNOTATION_HANDLER_CALL);
    LOG(THREAD, LOG_INTERP, 3, "inserting call to annotation handler\n");

    while (receiver != NULL) {
        if (handler->num_args != 0) {
            args = HEAP_ARRAY_ALLOC(dcontext, opnd_t, handler->num_args, ACCT_CLEANCALL,
                                    UNPROTECTED);
            memcpy(args, handler->args, sizeof(opnd_t) * handler->num_args);
        }
        if (handler->pass_pc_in_slot) {
            app_pc pc = GET_ANNOTATION_APP_PC(label_data);
            instrlist_insert_mov_immed_ptrsz(
                dcontext, (ptr_int_t)pc, dr_reg_spill_slot_opnd(dcontext, SPILL_SLOT_2),
                ilist, label, NULL, NULL);
        }
        dr_insert_clean_call_ex_varg(
            dcontext, ilist, label, receiver->instrumentation.callback,
            (receiver->save_fpstate ? DR_CLEANCALL_SAVE_FLOAT : 0) |
                /* Setting a return value is already handled with an inserted app
                 * instruction, so we do not set the DR_CLEANCALL_WRITES_APP_CONTEXT flag.
                 */
                DR_CLEANCALL_READS_APP_CONTEXT,
            handler->num_args, args);
        if (handler->num_args != 0) {
            HEAP_ARRAY_FREE(dcontext, args, opnd_t, handler->num_args, ACCT_CLEANCALL,
                            UNPROTECTED);
        }
        receiver = receiver->next;
    }
}
#endif

/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

/* SELF-MODIFYING-CODE SANDBOXING
 *
 * When we detect it, we take an exit that targets our own routine
 * fragment_self_write.  Dispatch checks for that target and if it finds it,
 * it calls that routine, so don't worry about building a bb for it.
 * Returns false if the bb has invalid instrs in the middle and it should
 * be rebuilt from scratch.
 */

#undef SAVE_TO_DC_OR_TLS
#undef RESTORE_FROM_DC_OR_TLS
/* PR 244737: x64 uses tls to avoid reachability issues w/ absolute addresses */
#ifdef X64
#    define SAVE_TO_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
        instr_create_save_to_tls((dc), (reg), (tls_offs))
#    define RESTORE_FROM_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
        instr_create_restore_from_tls((dc), (reg), (tls_offs))
#else
#    define SAVE_TO_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
        instr_create_save_to_dcontext((dc), (reg), (dc_offs))
#    define RESTORE_FROM_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
        instr_create_restore_from_dcontext((dc), (reg), (dc_offs))
#endif

static void
sandbox_rep_instr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, instr_t *next,
                  app_pc start_pc, app_pc end_pc /* end is open */)
{
    /* put checks before instr, set some reg as a flag, act on it
     * after instr (even if overwrite self will execute rep to completion)
     * want to read DF to find direction (0=inc xsi/xdi, 1=dec),
     * but only way to read is to do a pushf!
     * Solution: if cld or std right before rep instr, use that info,
     * otherwise check for BOTH directions!
     * xcx is a pre-check, xsi/xdi are inc/dec after memory op, so
     * xdi+xcx*opndsize == instr of NEXT write, so open-ended there:
     * if DF==0:
     *   if (xdi < end_pc && xdi+xcx*opndsize > start_pc) => self-write
     * if DF==1:
     *   if (xdi > start_pc && xdi-xcx*opndsize > end_pc) => self-write
     * both:
     *   if (xdi-xcx*opndsize < end_pc && xdi+xcx*opndsize > start_pc) => self-write
     * opndsize is 1,2, or 4 => use lea for mul
     *   lea (xdi,xcx,opndsize),xcx
     *
     *   save flags and xax
     *   save xbx
     *   lea (xdi,xcx,opndsize),xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): save xdx
     * if x64 && start_pc > 4GB: mov start_pc, xdx
     *   cmp xbx, IF_X64_>4GB_ELSE(xdx, start_pc)
     *   mov $0,xbx # for if ok
     *   jle ok # open b/c address of next rep write
     *   lea (,xcx,opndsize),xbx
     *   neg xbx # sub does dst - src
     *   add xdi,xbx
     * if x64 && end_pc > 4GB: mov end_pc, xdx
     *   cmp xbx, IF_X64_>4GB_ELSE(xdx, end_pc)
     *   mov $0,xbx # for if ok
     *   jge ok    # end is open
     *   mov $1,xbx
     * ok:
     *   restore flags and xax (xax used by stos)
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xdx
     *   <rep instr> # doesn't use xbx
     *     (PR 267764/i#398: we special-case restore xbx on cxt xl8 if this instr faults)
     *   mov xbx,xcx # we can use xcx, it's dead since 0 after rep
     *   restore xbx
     *   jecxz ok2  # if xbx was 1 we'll fall through and exit
     *   mov $0,xcx
     *   jmp <instr after write, flag as INSTR_BRANCH_SPECIAL_EXIT>
     * ok2:
     *   <label> # ok2 can't == next, b/c next may be ind br -> mangled w/ instrs
     *           # inserted before it, so jecxz would target too far
     */
    instr_t *ok = INSTR_CREATE_label(dcontext);
    instr_t *ok2 = INSTR_CREATE_label(dcontext);
    instr_t *jmp;
    app_pc after_write;
    uint opndsize = opnd_size_in_bytes(opnd_get_size(instr_get_dst(instr, 0)));
    uint flags =
        instr_eflags_to_fragment_eflags(forward_eflags_analysis(dcontext, ilist, next));
    bool use_tls = IF_X64_ELSE(true, false);
    IF_X64(bool x86_to_x64_ibl_opt = DYNAMO_OPTION(x86_to_x64_ibl_opt);)
    instr_t *next_app = next;
    DOLOG(3, LOG_INTERP, { d_r_loginst(dcontext, 3, instr, "writes memory"); });

    ASSERT(!instr_is_call_indirect(instr)); /* FIXME: can you have REP on on CALL's */

    /* skip meta instrs to find next app instr (xref PR 472190) */
    while (next_app != NULL && instr_is_meta(next_app))
        next_app = instr_get_next(next_app);

    if (next_app != NULL) {
        /* client may have inserted non-meta instrs, so use translation first
         * (xref PR 472190)
         */
        if (instr_get_app_pc(next_app) != NULL)
            after_write = instr_get_app_pc(next_app);
        else if (!instr_raw_bits_valid(next_app)) {
            /* next must be the final jmp! */
            ASSERT(instr_is_ubr(next_app) && instr_get_next(next_app) == NULL);
            after_write = opnd_get_pc(instr_get_target(next_app));
        } else
            after_write = instr_get_raw_bits(next_app);
    } else {
        after_write = end_pc;
    }

    insert_save_eflags(dcontext, ilist, instr, flags, use_tls,
                       !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                        !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    PRE(ilist, instr, SAVE_TO_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_XDI, REG_XCX, opndsize, 0, OPSZ_lea)));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr, SAVE_TO_DC_OR_TLS(dcontext, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
    }
    if ((ptr_uint_t)start_pc > UINT_MAX) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                 OPND_CREATE_INTPTR(start_pc)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XDX)));
    } else {
#endif
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(0)));
    PRE(ilist, instr, INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(ok)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(
            dcontext, opnd_create_reg(REG_XBX),
            opnd_create_base_disp(REG_NULL, REG_XCX, opndsize, 0, OPSZ_lea)));
    PRE(ilist, instr, INSTR_CREATE_neg(dcontext, opnd_create_reg(REG_XBX)));
    PRE(ilist, instr,
        INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XBX), opnd_create_reg(REG_XDI)));
#ifdef X64
    if ((ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XDX)));
    } else {
#endif
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)end_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(0)));
    PRE(ilist, instr, INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(ok)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(1)));
    PRE(ilist, instr, ok);
    insert_restore_eflags(dcontext, ilist, instr, flags, use_tls,
                          !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                           !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
    }
#endif
    /* instr goes here */
    PRE(ilist, next,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                            opnd_create_reg(REG_XBX)));
    PRE(ilist, next, RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    PRE(ilist, next, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(ok2)));
    PRE(ilist, next,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                             OPND_CREATE_INT32(0))); /* on x64 top 32 bits zeroed */
    jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(after_write));
    instr_branch_set_special_exit(jmp, true);
    /* an exit cti, not a meta instr */
    instrlist_preinsert(ilist, next, jmp);
    PRE(ilist, next, ok2);
}

static void
sandbox_write(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, instr_t *next,
              opnd_t op, app_pc start_pc, app_pc end_pc /* end is open */)
{
    /* can only test for equality w/o modifying flags, so save them
     * if (addr < end_pc && addr+opndsize > start_pc) => self-write
     *   <write memory>
     *   save xbx
     *   lea memory,xbx
     *   save flags and xax # after lea of memory in case memory includes xax
     * if x64 && (start_pc > 4GB || end_pc > 4GB): save xcx
     * if x64 && end_pc > 4GB: mov end_pc, xcx
     *   cmp xbx, IF_X64_>4GB_ELSE(xcx, end_pc)
     *   jge ok    # end is open
     *   lea opndsize(xbx),xbx
     * if x64 && start_pc > 4GB: mov start_pc, xcx
     *   cmp xbx, IF_X64_>4GB_ELSE(xcx, start_pc)
     *   jle ok    # open since added size
     *   restore flags (using xbx) and xax
     *   restore xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xcx
     *   jmp <instr after write, flag as INSTR_BRANCH_SPECIAL_EXIT>
     * ok:
     *   restore flags and xax
     *   restore xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xcx
     */
    instr_t *ok = INSTR_CREATE_label(dcontext), *jmp;
    app_pc after_write = NULL;
    uint opndsize = opnd_size_in_bytes(opnd_get_size(op));
    uint flags =
        instr_eflags_to_fragment_eflags(forward_eflags_analysis(dcontext, ilist, next));
    bool use_tls = IF_X64_ELSE(true, false);
    IF_X64(bool x86_to_x64_ibl_opt = DYNAMO_OPTION(x86_to_x64_ibl_opt);)
    instr_t *next_app = next;
    instr_t *get_addr_at = next;
    int opcode = instr_get_opcode(instr);
    DOLOG(3, LOG_INTERP, { d_r_loginst(dcontext, 3, instr, "writes memory"); });

    /* skip meta instrs to find next app instr (xref PR 472190) */
    while (next_app != NULL && instr_is_meta(next_app))
        next_app = instr_get_next(next_app);

    if (next_app != NULL) {
        /* client may have inserted non-meta instrs, so use translation first
         * (xref PR 472190)
         */
        if (instr_get_app_pc(next_app) != NULL)
            after_write = instr_get_app_pc(next_app);
        else if (!instr_raw_bits_valid(next_app)) {
            /* next must be the final artificially added jmp! */
            ASSERT(instr_is_ubr(next_app) && instr_get_next(next_app) == NULL);
            /* for sure this is the last jmp out, but it
             * doesn't have to be a direct jmp but instead
             * it could be the exit branch we add as an
             * for an indirect call - which is the only ind branch
             * that writes to memory
             * CALL* already means that we're leaving the block and it cannot be a selfmod
             * instruction even though it writes to memory
             */
            DOLOG(4, LOG_INTERP,
                  { d_r_loginst(dcontext, 4, next_app, "next app instr"); });
            after_write = opnd_get_pc(instr_get_target(next_app));
            LOG(THREAD, LOG_INTERP, 4, "after_write = " PFX " next should be final jmp\n",
                after_write);
        } else
            after_write = instr_get_raw_bits(next_app);
    } else {
        ASSERT_NOT_TESTED();
        after_write = end_pc;
    }

    if (opcode == OP_ins || opcode == OP_movs || opcode == OP_stos) {
        /* These instrs modify their own addressing register so we must
         * get the address pre-write.  None of them touch xbx.
         */
        get_addr_at = instr;
        ASSERT(!instr_writes_to_reg(instr, REG_XBX, DR_QUERY_DEFAULT) &&
               !instr_reads_from_reg(instr, REG_XBX, DR_QUERY_DEFAULT));
    }

    PRE(ilist, get_addr_at,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    /* XXX: Basically reimplementing drutil_insert_get_mem_addr(). */
    /* FIXME i#986: Sandbox far writes.  Not a hypothetical problem!  NaCl uses
     * segments for its x86 sandbox, although they are 0 based with a limit.
     * qq.exe has them in sandboxed code.
     */
    ASSERT_CURIOSITY(!opnd_is_far_memory_reference(op) ||
                     /* Standard far refs */
                     opcode == OP_ins || opcode == OP_movs || opcode == OP_stos);
    if (opnd_is_base_disp(op)) {
        /* change to OPSZ_lea for lea */
        opnd_set_size(&op, OPSZ_lea);
        PRE(ilist, get_addr_at, INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX), op));
        if ((opcode == OP_push && opnd_is_base_disp(op) &&
             opnd_get_index(op) == DR_REG_NULL &&
             reg_to_pointer_sized(opnd_get_base(op)) == DR_REG_XSP) ||
            opcode == OP_push_imm || opcode == OP_pushf || opcode == OP_pusha ||
            opcode == OP_pop /* pop into stack slot */ || opcode == OP_call ||
            opcode == OP_call_ind || opcode == OP_call_far || opcode == OP_call_far_ind) {
            /* Undo xsp adjustment made by the instruction itself.
             * We could use get_addr_at to acquire the address pre-instruction
             * for some of these, but some can read or write ebx.
             */
            PRE(ilist, next,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                                 opnd_create_base_disp(REG_NULL, REG_XBX, 1,
                                                       -opnd_get_disp(op), OPSZ_lea)));
        }
    } else {
        /* handle abs addr pointing within fragment */
        /* XXX: Can optimize this by doing address comparison at translation
         * time.  Might happen frequently if a JIT stores data on the same page
         * as its code.  For now we hook into existing sandboxing code.
         */
        app_pc abs_addr;
        ASSERT(opnd_is_abs_addr(op) IF_X64(|| opnd_is_rel_addr(op)));
        abs_addr = opnd_get_addr(op);
        PRE(ilist, get_addr_at,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX),
                                 OPND_CREATE_INTPTR(abs_addr)));
    }
    insert_save_eflags(dcontext, ilist, next, flags, use_tls,
                       !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                        !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next, SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
    if ((ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XCX)));
    } else {
#endif
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)end_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, next, INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(ok)));
    PRE(ilist, next,
        INSTR_CREATE_lea(
            dcontext, opnd_create_reg(REG_XBX),
            opnd_create_base_disp(REG_XBX, REG_NULL, 0, opndsize, OPSZ_lea)));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX) {
        PRE(ilist, next,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(start_pc)));
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XCX)));
    } else {
#endif
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, next, INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(ok)));
    insert_restore_eflags(dcontext, ilist, next, flags, use_tls,
                          !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                           !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    PRE(ilist, next, RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
#endif
    jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(after_write));
    instr_branch_set_special_exit(jmp, true);
    /* an exit cti, not a meta instr */
    instrlist_preinsert(ilist, next, jmp);
    PRE(ilist, next, ok);
    insert_restore_eflags(dcontext, ilist, next, flags, use_tls,
                          !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                           !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    PRE(ilist, next, RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
#endif
}

static bool
sandbox_top_of_bb_check_s2ro(dcontext_t *dcontext, app_pc start_pc)
{
    return (DYNAMO_OPTION(sandbox2ro_threshold) > 0 &&
            /* we can't make stack regions ro so don't put in the instrumentation */
            !is_address_on_stack(dcontext, start_pc) &&
            /* case 9098 we don't want to ever make RO untrackable driver areas */
            !is_driver_address(start_pc));
}

static void
sandbox_top_of_bb(dcontext_t *dcontext, instrlist_t *ilist, bool s2ro, uint flags,
                  app_pc start_pc, app_pc end_pc, /* end is open */
                  bool for_cache,
                  /* for obtaining the two patch locations: */
                  patch_list_t *patchlist, cache_pc *copy_start_loc,
                  cache_pc *copy_end_loc)
{
    /* add check at top of ilist that compares actual app instructions versus
     * copy we saved, stored in cache right after fragment itself.  leave its
     * start address blank here, will be touched up after emitting this ilist.
     *
     * FIXME case 8165/PR 212600: optimize this: move reg restores to
     * custom fcache_return, use cmpsd instead of cmpsb, etc.
     *
     * if eflags live entering this bb:
     *   save xax
     *   lahf
     *   seto  %al
     * endif
     * if (-sandbox2ro_threshold > 0)
     *  if x64: save xcx
     *     incl  &vm_area_t->exec_count (for x64, via xcx)
     *     cmp   sandbox2ro_threshold, vm_area_t->exec_count (for x64, via xcx)
     *  if eflags live entering this bb, or x64:
     *     jl    past_threshold
     *   if x64: restore xcx
     *   if eflags live entering this bb:
     *     jmp restore_eflags_and_exit
     *   else
     *     jmp   start_pc marked as selfmod exit
     *   endif
     *   past_threshold:
     *  else
     *     jge   start_pc marked as selfmod exit
     *  endif
     * endif
     * if (-sandbox2ro_threshold == 0) && !x64)
     *   save xcx
     * endif
     *   save xsi
     *   save xdi
     * if stats:
     *   inc num_sandbox_execs stat (for x64, via xsi)
     * endif
     *   mov start_pc,xsi
     *   mov copy_start_pc,xdi  # 1 opcode byte, then offset
     *       # => patch point 1
     *   cmpsb
     * if copy_size > 1 # not an opt: for correctness: if "repe cmpsb" has xcx==0, it
     *                  # doesn't touch eflags and we treat cmp results as cmpsb results
     *     jne check_results
     *   if x64 && start_pc > 4GB
     *     mov start_pc, xcx
     *     cmp xsi, xcx
     *   else
     *     cmp xsi, start_pc
     *   endif
     *     mov copy_size-1, xcx # -1 b/c we already checked 1st byte
     *     jge forward
     *     mov copy_end_pc - 1, xdi # -1 b/c it is the end of this basic block
     *         # => patch point 2
     *     mov end_pc - 1, xsi
     *   forward:
     *     repe cmpsb
     * endif # copy_size > 1
     *   check_results:
     *     restore xcx
     *     restore xsi
     *     restore xdi
     * if eflags live:
     *   je start_bb
     *  restore_eflags_and_exit:
     *   add   $0x7f,%al
     *   sahf
     *   restore xax
     *   jmp start_pc marked as selfmod exit
     * else
     *   jne start_pc marked as selfmod exit
     * endif
     * start_bb:
     * if eflags live:
     *   add   $0x7f,%al
     *   sahf
     *   restore xax
     * endif
     */
    instr_t *instr, *jmp;
    instr_t *restore_eflags_and_exit = NULL;
    bool use_tls = IF_X64_ELSE(true, false);
    IF_X64(bool x86_to_x64_ibl_opt = DYNAMO_OPTION(x86_to_x64_ibl_opt);)
    bool saved_xcx = false;
    instr_t *check_results = INSTR_CREATE_label(dcontext);

    instr = instrlist_first_expanded(dcontext, ilist);

    insert_save_eflags(dcontext, ilist, instr, flags, use_tls,
                       !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                        !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));

    if (s2ro) {
        /* It's difficult to use lea/jecxz here as we want to use a shared
         * counter but no lock, and thus need a relative comparison, while
         * lea/jecxz can only do an exact comparison.  We could be exact by
         * having a separate counter per (private) fragment but by spilling
         * eflags we can inc memory, making the scheme here not inefficient.
         */
        uint thresh = DYNAMO_OPTION(sandbox2ro_threshold);
        uint *counter;
        if (for_cache)
            counter = get_selfmod_exec_counter(start_pc);
        else {
            /* Won't find exec area since not a real fragment (probably
             * a recreation post-flush).  Won't execute, so NULL is fine.
             */
            counter = NULL;
        }
#ifdef X64
        PRE(ilist, instr, SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
        saved_xcx = true;
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(counter)));
        PRE(ilist, instr, INSTR_CREATE_inc(dcontext, OPND_CREATE_MEM32(REG_XCX, 0)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, OPND_CREATE_MEM32(REG_XCX, 0),
                             OPND_CREATE_INT_32OR8((int)thresh)));
#else
        PRE(ilist, instr,
            INSTR_CREATE_inc(dcontext, OPND_CREATE_ABSMEM(counter, OPSZ_4)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, OPND_CREATE_ABSMEM(counter, OPSZ_4),
                             OPND_CREATE_INT_32OR8(thresh)));
#endif
        if (TEST(FRAG_WRITES_EFLAGS_6, flags) IF_X64(&&false)) {
            jmp = INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_pc(start_pc));
            instr_branch_set_special_exit(jmp, true);
            /* an exit cti, not a meta instr */
            instrlist_preinsert(ilist, instr, jmp);
        } else {
            instr_t *past_threshold = INSTR_CREATE_label(dcontext);
            PRE(ilist, instr,
                INSTR_CREATE_jcc_short(dcontext, OP_jl_short,
                                       opnd_create_instr(past_threshold)));
#ifdef X64
            PRE(ilist, instr,
                RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
#endif
            if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
                ASSERT(restore_eflags_and_exit == NULL);
                restore_eflags_and_exit = INSTR_CREATE_label(dcontext);
                PRE(ilist, instr,
                    INSTR_CREATE_jmp(dcontext,
                                     opnd_create_instr(restore_eflags_and_exit)));
            }
#ifdef X64
            else {
                jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(start_pc));
                instr_branch_set_special_exit(jmp, true);
                /* an exit cti, not a meta instr */
                instrlist_preinsert(ilist, instr, jmp);
            }
#endif
            PRE(ilist, instr, past_threshold);
        }
    }

    if (!saved_xcx) {
        PRE(ilist, instr, SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
    PRE(ilist, instr, SAVE_TO_DC_OR_TLS(dcontext, REG_XSI, TLS_XBX_SLOT, XSI_OFFSET));
    PRE(ilist, instr, SAVE_TO_DC_OR_TLS(dcontext, REG_XDI, TLS_XDX_SLOT, XDI_OFFSET));
    DOSTATS({
        if (GLOBAL_STATS_ON()) {
            /* We only do global inc, not bothering w/ thread-private stats.
             * We don't care about races: ballpark figure is good enough.
             * We could do a direct inc of memory for 32-bit.
             */
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(
                    dcontext, opnd_create_reg(REG_XSI),
                    OPND_CREATE_INTPTR(GLOBAL_STAT_ADDR(num_sandbox_execs))));
            PRE(ilist, instr,
                INSTR_CREATE_inc(
                    dcontext,
                    opnd_create_base_disp(REG_XSI, REG_NULL, 0, 0, OPSZ_STATS)));
        }
    });
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XSI),
                             OPND_CREATE_INTPTR(start_pc)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                             /* will become copy start */
                             OPND_CREATE_INTPTR(start_pc)));
    if (patchlist != NULL) {
        ASSERT(copy_start_loc != NULL);
        add_patch_marker(patchlist, instr_get_prev(instr), PATCH_ASSEMBLE_ABSOLUTE,
                         -(short)sizeof(cache_pc), (ptr_uint_t *)copy_start_loc);
    }
    PRE(ilist, instr, INSTR_CREATE_cmps_1(dcontext));
    /* For a 1-byte copy size we cannot use "repe cmpsb" as it won't
     * touch eflags and we'll treat the cmp results as cmpsb results, which
     * doesn't work (cmp will never be equal)
     */
    if (end_pc - start_pc > 1) {
        instr_t *forward = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_instr(check_results)));
#ifdef X64
        if ((ptr_uint_t)start_pc > UINT_MAX) {
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                     OPND_CREATE_INTPTR(start_pc)));
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSI),
                                 opnd_create_reg(REG_XCX)));
        } else {
#endif
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSI),
                                 OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
        }
#endif
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(end_pc - (start_pc + 1))));
        /* i#2155: In the case where the direction flag is set, xsi will be lesser
         * than start_pc after cmps, and the jump branch will not be taken.
         */
        PRE(ilist, instr, INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(forward)));
        /* i#2155: The immediate value is only psychological
         * since it will be modified in finalize_selfmod_sandbox.
         */
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                                 /* will become copy end */
                                 OPND_CREATE_INTPTR(end_pc - 1)));
        if (patchlist != NULL) {
            ASSERT(copy_end_loc != NULL);
            add_patch_marker(patchlist, instr_get_prev(instr), PATCH_ASSEMBLE_ABSOLUTE,
                             -(short)sizeof(cache_pc), (ptr_uint_t *)copy_end_loc);
        }
        /* i#2155: The next rep cmps comparison will be done backward,
         * and thus it should be started at end_pc - 1
         * because current basic block is [start_pc:end_pc-1].
         */
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XSI),
                                 OPND_CREATE_INTPTR(end_pc - 1)));
        PRE(ilist, instr, forward);
        PRE(ilist, instr, INSTR_CREATE_rep_cmps_1(dcontext));
    }
    PRE(ilist, instr, check_results);
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XSI, TLS_XBX_SLOT, XSI_OFFSET));
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XDI, TLS_XDX_SLOT, XDI_OFFSET));
    if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
        instr_t *start_bb = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr, INSTR_CREATE_jcc(dcontext, OP_je, opnd_create_instr(start_bb)));
        if (restore_eflags_and_exit != NULL) /* somebody needs this label */
            PRE(ilist, instr, restore_eflags_and_exit);
        insert_restore_eflags(dcontext, ilist, instr, flags, use_tls,
                              !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                               !X64_MODE_DC(dcontext) &&
                                               x86_to_x64_ibl_opt));
        jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(start_pc));
        instr_branch_set_special_exit(jmp, true);
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr, jmp);
        PRE(ilist, instr, start_bb);
    } else {
        jmp = INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_pc(start_pc));
        instr_branch_set_special_exit(jmp, true);
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr, jmp);
    }
    insert_restore_eflags(dcontext, ilist, instr, flags, use_tls,
                          !use_tls _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                           !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    /* fall-through to bb start */
}

/* returns false if failed to add sandboxing b/c of a problematic ilist --
 * invalid instrs, elided ctis, etc.
 */
bool
insert_selfmod_sandbox(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
                       app_pc start_pc, app_pc end_pc, /* end is open */
                       bool record_translation, bool for_cache)
{
    instr_t *instr, *next;

    if (!INTERNAL_OPTION(hw_cache_consistency))
        return true; /* nothing to do */

    /* this code assumes bb covers single, contiguous region */
    ASSERT((flags & FRAG_HAS_DIRECT_CTI) == 0);

    /* store first instr so loop below will skip top check */
    instr = instrlist_first_expanded(dcontext, ilist);
    instrlist_set_our_mangling(ilist, true); /* PR 267260 */
    if (record_translation) {
        /* skip client instrumentation, if any, as is done below */
        while (instr != NULL && instr_is_meta(instr))
            instr = instr_get_next_expanded(dcontext, ilist, instr);
        /* make sure inserted instrs translate to the proper original instr */
        ASSERT(instr != NULL && instr_get_translation(instr) != NULL);
        instrlist_set_translation_target(ilist, instr_get_translation(instr));
    }

    sandbox_top_of_bb(dcontext, ilist, sandbox_top_of_bb_check_s2ro(dcontext, start_pc),
                      flags, start_pc, end_pc, for_cache, NULL, NULL, NULL);

    if (INTERNAL_OPTION(sandbox_writes)) {
        for (; instr != NULL; instr = next) {
            int i, opcode;
            opnd_t op;

            opcode = instr_get_opcode(instr);
            if (!instr_valid(instr)) {
                /* invalid instr -- best to truncate block here, easiest way
                 * to do that and get all flags right is to re-build it,
                 * but this time we'll use full decode so we'll avoid the discrepancy
                 * between fast and full decode on invalid instr detection.
                 */
                if (record_translation)
                    instrlist_set_translation_target(ilist, NULL);
                instrlist_set_our_mangling(ilist, false); /* PR 267260 */
                return false;
            }

            /* don't mangle anything that mangle inserts! */
            next = instr_get_next_expanded(dcontext, ilist, instr);
            if (instr_is_meta(instr))
                continue;
            if (record_translation) {
                /* make sure inserted instrs translate to the proper original instr */
                ASSERT(instr_get_translation(instr) != NULL);
                instrlist_set_translation_target(ilist, instr_get_translation(instr));
            }

            if (opcode == OP_rep_ins || opcode == OP_rep_movs || opcode == OP_rep_stos) {
                sandbox_rep_instr(dcontext, ilist, instr, next, start_pc, end_pc);
                continue;
            }

            /* FIXME case 8165: optimize for multiple push/pop */
            for (i = 0; i < instr_num_dsts(instr); i++) {
                op = instr_get_dst(instr, i);
                if (opnd_is_memory_reference(op)) {
                    /* ignore CALL* since last anyways */
                    if (instr_is_call_indirect(instr)) {
                        ASSERT(next != NULL && !instr_raw_bits_valid(next));
                        /* FIXME case 8165: why do we ever care about the last
                         * instruction modifying anything?
                         */
                        /* conversion of IAT calls (but not elision)
                         * transforms this into a direct CALL,
                         * in that case 'next' is a direct jmp
                         * fall through, so has no exit flags
                         */
                        ASSERT(EXIT_IS_CALL(instr_exit_branch_type(next)) ||
                               (DYNAMO_OPTION(IAT_convert) &&
                                TEST(INSTR_IND_CALL_DIRECT, instr->flags)));

                        LOG(THREAD, LOG_INTERP, 3,
                            " ignoring CALL* at end of fragment\n");
                        /* This test could be done outside of this loop on
                         * destinations, but since it is rare it is faster
                         * to do it here.  Using continue instead of break in case
                         * it gets moved out.
                         */
                        continue;
                    }
                    if (opnd_is_abs_addr(op) IF_X64(|| opnd_is_rel_addr(op))) {
                        app_pc abs_addr = opnd_get_addr(op);
                        uint size = opnd_size_in_bytes(opnd_get_size(op));
                        if (!POINTER_OVERFLOW_ON_ADD(abs_addr, size) &&
                            (abs_addr + size < start_pc || abs_addr >= end_pc)) {
                            /* This is an absolute memory reference that points
                             * outside the current basic block and doesn't need
                             * sandboxing.
                             */
                            continue;
                        }
                    }
                    sandbox_write(dcontext, ilist, instr, next, op, start_pc, end_pc);
                }
            }
        }
    }
    if (record_translation)
        instrlist_set_translation_target(ilist, NULL);
    instrlist_set_our_mangling(ilist, false); /* PR 267260 */
    return true;
}

/* Offsets within selfmod sandbox top-of-bb code that we patch once
 * the code is emitted, as the values depend on the emitted address.
 * These vary by whether sandbox_top_of_bb_check_s2ro() and whether
 * eflags are not written, all written, or just OF is written.
 * For the copy_size == 1 variation, we simply ignore the 2nd patch point.
 */
static bool selfmod_s2ro[] = { false, true };
static uint selfmod_eflags[] = { FRAG_WRITES_EFLAGS_6, FRAG_WRITES_EFLAGS_OF, 0 };
#define SELFMOD_NUM_S2RO (sizeof(selfmod_s2ro) / sizeof(selfmod_s2ro[0]))
#define SELFMOD_NUM_EFLAGS (sizeof(selfmod_eflags) / sizeof(selfmod_eflags[0]))
#ifdef X64 /* additional complexity: start_pc > 4GB? */
static app_pc selfmod_gt4G[] = { NULL, (app_pc)(POINTER_MAX - 2) /*so end can be +2*/ };
#    define SELFMOD_NUM_GT4G (sizeof(selfmod_gt4G) / sizeof(selfmod_gt4G[0]))
#endif
uint selfmod_copy_start_offs[SELFMOD_NUM_S2RO]
                            [SELFMOD_NUM_EFLAGS] IF_X64([SELFMOD_NUM_GT4G]);
uint selfmod_copy_end_offs[SELFMOD_NUM_S2RO]
                          [SELFMOD_NUM_EFLAGS] IF_X64([SELFMOD_NUM_GT4G]);

void
set_selfmod_sandbox_offsets(dcontext_t *dcontext)
{
    int i, j;
#ifdef X64
    int k;
#endif
    instrlist_t ilist;
    patch_list_t patch;
    static byte buf[256];
    uint len;
    /* We assume this is called at init, when .data is +w and we need no
     * synch on accessing buf */
    ASSERT(!dynamo_initialized);
    for (i = 0; i < SELFMOD_NUM_S2RO; i++) {
        for (j = 0; j < SELFMOD_NUM_EFLAGS; j++) {
#ifdef X64
            for (k = 0; k < SELFMOD_NUM_GT4G; k++) {
#endif
                cache_pc start_pc, end_pc;
                app_pc app_start;
                instr_t *inst;
                instrlist_init(&ilist);
                /* sandbox_top_of_bb assumes there's an instr there */
                instrlist_append(&ilist, INSTR_CREATE_label(dcontext));
                init_patch_list(&patch, PATCH_TYPE_ABSOLUTE);
                app_start = IF_X64_ELSE(selfmod_gt4G[k], NULL);
                sandbox_top_of_bb(dcontext, &ilist, selfmod_s2ro[i], selfmod_eflags[j],
                                  /* we must have a >1-byte region to get
                                   * both patch points */
                                  app_start, app_start + 2, false, &patch, &start_pc,
                                  &end_pc);
                /* The exit cti's may not reachably encode (normally
                 * they'd be mangled away) so we munge them first
                 */
                for (inst = instrlist_first(&ilist); inst != NULL;
                     inst = instr_get_next(inst)) {
                    if (instr_is_exit_cti(inst)) {
                        instr_set_target(inst, opnd_create_pc(buf));
                    }
                }
                len = encode_with_patch_list(dcontext, &patch, &ilist, buf);
                ASSERT(len < BUFFER_SIZE_BYTES(buf));
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(start_pc - buf)));
                selfmod_copy_start_offs[i][j] IF_X64([k]) = (uint)(start_pc - buf);
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(end_pc - buf)));
                selfmod_copy_end_offs[i][j] IF_X64([k]) = (uint)(end_pc - buf);
                LOG(THREAD, LOG_EMIT, 3, "selfmod offs %d %d" IF_X64(" %d") ": %u %u\n",
                    i, j, IF_X64_(k) selfmod_copy_start_offs[i][j] IF_X64([k]),
                    selfmod_copy_end_offs[i][j] IF_X64([k]));
                /* free the instrlist_t elements */
                instrlist_clear(dcontext, &ilist);
#ifdef X64
            }
#endif
        }
    }
}

void
finalize_selfmod_sandbox(dcontext_t *dcontext, fragment_t *f)
{
    cache_pc copy_pc = FRAGMENT_SELFMOD_COPY_PC(f);
    byte *pc;
    int i, j;
#ifdef X64
    int k = ((ptr_uint_t)f->tag) > UINT_MAX ? 1 : 0;
#endif
    i = (sandbox_top_of_bb_check_s2ro(dcontext, f->tag)) ? 1 : 0;
    j = (TEST(FRAG_WRITES_EFLAGS_6, f->flags)
             ? 0
             : (TEST(FRAG_WRITES_EFLAGS_OF, f->flags) ? 1 : 2));
    pc = FCACHE_ENTRY_PC(f) + selfmod_copy_start_offs[i][j] IF_X64([k]);
    /* The copy start gets updated after sandbox_top_of_bb. */
    *((cache_pc *)vmcode_get_writable_addr(pc)) = copy_pc;
    if (FRAGMENT_SELFMOD_COPY_CODE_SIZE(f) > 1) {
        pc = FCACHE_ENTRY_PC(f) + selfmod_copy_end_offs[i][j] IF_X64([k]);
        /* i#2155: The copy end gets updated.
         * This value will be used in the case where the direction flag is set.
         * It will then be the starting point for the backward repe cmps.
         */
        *((cache_pc *)vmcode_get_writable_addr(pc)) =
            (copy_pc + FRAGMENT_SELFMOD_COPY_CODE_SIZE(f) - 1);
    } /* else, no 2nd patch point */
}
