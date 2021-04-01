/* **********************************************************
 * Copyright (c) 2012-2017 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* file "x86_to_x64.c" -- translate 32-bit instr to 64-bit
 *
 * The translation uses r8 freely, but not the other registers.
 *
 * We try to preserve app transparency by not touching beyond top-of-stack.
 * An exception is pushf/popf, as commented below.
 * The current fault translation should be able to handle all but les/lds,
 * but we have not tested fault translation yet.
 */

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrument.h"

static const reg_id_t pushad_registers[] = { REG_EDI, REG_ESI, REG_EBP, REG_R8D,
                                             REG_EBX, REG_EDX, REG_ECX, REG_EAX };

/* inserts "inst" before "where", and sets its translation */
static void
pre(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_translation(inst, instr_get_translation(where));
    instrlist_preinsert(ilist, where, inst);
}

/* replaces old with new, destroys old inst, and lets old point to new */
static void
replace(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **old, instr_t *new)
{
    instr_set_translation(new, instr_get_translation(*old));
    instrlist_replace(ilist, *old, new);
    instr_destroy(dcontext, *old);
    *old = new;
}

static opnd_t
opnd_change_base_reg_to_64(opnd_t opnd)
{
    reg_id_t base_reg;
    reg_id_t index_reg;
    int disp;

    ASSERT(opnd_is_base_disp(opnd));

    base_reg = opnd_get_base(opnd);
    index_reg = opnd_get_index(opnd);
    disp = opnd_get_disp(opnd);

    /* If there's a negative index, then base+index may overflow.
     * So we only perform the optimization when there's no index.
     *
     * XXX: We assume that if disp is within +/-4K, then it is really a
     * displacement, and base+disp won't overflow because disp will be
     * sign-extended. However, if disp is outside that range, then it
     * may be used as a base address and base is used as a displacement,
     * in which case base+disp may overflow because base will be zero-
     * extended.
     */
    if (reg_is_32bit(base_reg) && index_reg == REG_NULL && disp > -4096 && disp < 4096) {
        opnd = opnd_create_far_base_disp_ex(
            opnd_get_segment(opnd), reg_32_to_64(base_reg), index_reg,
            opnd_get_scale(opnd), disp, opnd_get_size(opnd),
            opnd_is_disp_encode_zero(opnd), opnd_is_disp_force_full(opnd),
            opnd_is_disp_short_addr(opnd));
    }

    return opnd;
}

static bool
instr_is_string_operation(instr_t *instr)
{
    switch (instr_get_opcode(instr)) {
    case OP_ins:
    case OP_rep_ins:
    case OP_outs:
    case OP_rep_outs:
    case OP_movs:
    case OP_rep_movs:
    case OP_stos:
    case OP_rep_stos:
    case OP_lods:
    case OP_rep_lods:
    case OP_cmps:
    case OP_rep_cmps:
    case OP_repne_cmps:
    case OP_scas:
    case OP_rep_scas:
    case OP_repne_scas: return true;
    }
    return false;
}

/* make memory reference operands use 64-bit regs in order to save the
 * addr32 prefix, because the high 32 bits should be zero at this time.
 */
static void
instr_change_base_reg_to_64(instr_t *instr)
{
    int i;
    bool is_string_instr = instr_is_string_operation(instr);

    for (i = 0; i < instr_num_dsts(instr); ++i) {
        opnd_t opnd = instr_get_dst(instr, i);
        if (opnd_is_base_disp(opnd))
            instr_set_dst(instr, i, opnd_change_base_reg_to_64(opnd));
        else if (is_string_instr && opnd_is_reg(opnd)) {
            reg_id_t reg = opnd_get_reg(opnd);
            if (reg == REG_ESI || reg == REG_EDI || reg == REG_ECX)
                instr_set_dst(instr, i, opnd_create_reg(reg_32_to_64(reg)));
        }
    }
    for (i = 0; i < instr_num_srcs(instr); ++i) {
        opnd_t opnd = instr_get_src(instr, i);
        if (opnd_is_base_disp(opnd))
            instr_set_src(instr, i, opnd_change_base_reg_to_64(opnd));
        else if (is_string_instr && opnd_is_reg(opnd)) {
            reg_id_t reg = opnd_get_reg(opnd);
            if (reg == REG_ESI || reg == REG_EDI || reg == REG_ECX)
                instr_set_src(instr, i, opnd_create_reg(reg_32_to_64(reg)));
        }
    }
}

static void
translate_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    /* translate: jmp target  => movzx/mov target -> r8d
     *                           jmp r8
     */
    opnd_t target = instr_get_target(*instr);
    if (opnd_get_size(target) == OPSZ_2) {
        pre(ilist, *instr,
            INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_R8D), target));
    } else {
        ASSERT(opnd_get_size(target) == OPSZ_4);
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D), target));
    }
    replace(dcontext, ilist, instr,
            INSTR_CREATE_jmp_ind(dcontext, opnd_create_reg(REG_R8)));
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    /* translate: call target => movzx/mov target -> r8d
     *                           push retaddr as 32-bit
     *                           jmp r8
     */
    opnd_t target = instr_get_target(*instr);
    ptr_uint_t retaddr = get_call_return_address(dcontext, ilist, *instr);
    if (opnd_get_size(target) == OPSZ_2) {
        pre(ilist, *instr,
            INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_R8D), target));
    } else {
        ASSERT(opnd_get_size(target) == OPSZ_4);
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D), target));
    }
    insert_push_retaddr(dcontext, ilist, *instr, retaddr, OPSZ_4);
    replace(dcontext, ilist, instr,
            INSTR_CREATE_jmp_ind(dcontext, opnd_create_reg(REG_R8)));
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_push(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    opnd_t mem = instr_get_dst(*instr, 1);
    opnd_t src;

    /* x64 can handle 2-byte push; no need to translate. */
    if (opnd_get_size(mem) == OPSZ_2)
        return;

    /* 4-byte push */
    ASSERT(opnd_get_size(mem) == OPSZ_4);
    src = instr_get_src(*instr, 0);
    if (opnd_is_reg(src)) {
        reg_id_t reg = opnd_get_reg(src);
        if (reg == REG_ESP) {
            /* translate: push esp => mov esp -> r8d
             *                        lea -4(rsp) -> rsp
             *                        mov r8d -> (rsp)
             */
            pre(ilist, *instr,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D), src));
            pre(ilist, *instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                 OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -4)));
            replace(dcontext, ilist, instr,
                    INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_RSP, 0),
                                        opnd_create_reg(REG_R8D)));
        } else if (reg_is_32bit(reg)) {
            /* translate: push reg32 => lea -4(rsp) -> rsp
             *                          mov reg32 -> (rsp)
             */
            pre(ilist, *instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                 OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -4)));
            replace(dcontext, ilist, instr,
                    INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_RSP, 0), src));
        } else {
            ASSERT(reg == SEG_CS || reg == SEG_SS || reg == SEG_DS || reg == SEG_ES ||
                   reg == SEG_FS || reg == SEG_GS);
            /* translate: push sreg => mov sreg -> r8
             *                         lea -4(rsp) -> rsp
             *                         mov r8d -> (rsp)
             */
            pre(ilist, *instr,
                INSTR_CREATE_mov_seg(dcontext, opnd_create_reg(REG_R8), src));
            pre(ilist, *instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                 OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -4)));
            replace(dcontext, ilist, instr,
                    INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_RSP, 0),
                                        opnd_create_reg(REG_R8D)));
        }
    } else {
        ASSERT(opnd_is_base_disp(src) && opnd_get_size(src) == OPSZ_4);
        /* translate: push mem32 => mov mem32 -> r8d
         *                          lea -4(rsp) -> rsp
         *                          mov r8d -> (rsp)
         */
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D),
                                opnd_change_base_reg_to_64(src)));
        pre(ilist, *instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                             OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -4)));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_RSP, 0),
                                    opnd_create_reg(REG_R8D)));
    }
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_push_imm(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    opnd_t mem = instr_get_dst(*instr, 1);
    opnd_t src;

    /* x64 can handle 2-byte push; no need to translate. */
    if (opnd_get_size(mem) == OPSZ_2)
        return;

    /* 4-byte push
     * translate: push imm => mov imm -> r8l/r8w/r8d
     *                        (movsx r8l/r8w -> r8d)  # for imm8 and imm16
     *                        lea -4(rsp) -> rsp
     *                        mov r8d -> (rsp)
     */
    ASSERT(opnd_get_size(mem) == OPSZ_4);
    src = instr_get_src(*instr, 0);
    switch (opnd_get_size(src)) {
    case OPSZ_1:
        pre(ilist, *instr, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_R8L), src));
        pre(ilist, *instr,
            INSTR_CREATE_movsx(dcontext, opnd_create_reg(REG_R8D),
                               opnd_create_reg(REG_R8L)));
        break;
    case OPSZ_2:
        pre(ilist, *instr, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_R8W), src));
        pre(ilist, *instr,
            INSTR_CREATE_movsx(dcontext, opnd_create_reg(REG_R8D),
                               opnd_create_reg(REG_R8W)));
        break;
    default:
        ASSERT(opnd_get_size(src) == OPSZ_4);
        pre(ilist, *instr, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_R8D), src));
    }
    pre(ilist, *instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                         OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -4)));
    replace(dcontext, ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_RSP, 0),
                                opnd_create_reg(REG_R8D)));
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_pop(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    opnd_t mem = instr_get_src(*instr, 1);
    opnd_t dst;

    /* x64 can handle 2-byte pop; no need to translate. */
    if (opnd_get_size(mem) == OPSZ_2)
        return;

    /* 4-byte pop */
    ASSERT(opnd_get_size(mem) == OPSZ_4);
    dst = instr_get_dst(*instr, 0);
    if (opnd_is_reg(dst)) {
        reg_id_t reg = opnd_get_reg(dst);
        if (reg == REG_ESP) {
            /* translate: pop esp => mov (rsp) -> esp */
            replace(dcontext, ilist, instr,
                    INSTR_CREATE_mov_ld(dcontext, dst, OPND_CREATE_MEM32(REG_RSP, 0)));
        } else if (reg_is_32bit(reg)) {
            /* translate: pop reg32 => mov (rsp) -> reg32
             *                         lea 4(rsp) -> rsp
             */
            pre(ilist, *instr,
                INSTR_CREATE_mov_ld(dcontext, dst, OPND_CREATE_MEM32(REG_RSP, 0)));
            replace(dcontext, ilist, instr,
                    INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                     OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, 4)));
        } else {
            ASSERT(reg == SEG_DS || reg == SEG_ES || reg == SEG_SS || reg == SEG_FS ||
                   reg == SEG_GS);
            /* translate: pop sreg => mov (rsp) -> sreg
             *                        lea 4(rsp) -> rsp
             */
            pre(ilist, *instr,
                INSTR_CREATE_mov_seg(dcontext, dst, OPND_CREATE_MEM16(REG_RSP, 0)));
            replace(dcontext, ilist, instr,
                    INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                     OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, 4)));
        }
    } else {
        ASSERT(opnd_is_base_disp(dst) && opnd_get_size(dst) == OPSZ_4);
        /* translate: pop mem32 => mov (rsp) -> r8d
         *                         lea 4(rsp) -> rsp
         *                         mov r8d -> mem32
         */
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D),
                                OPND_CREATE_MEM32(REG_RSP, 0)));
        pre(ilist, *instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                             OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, 4)));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_mov_st(dcontext, opnd_change_base_reg_to_64(dst),
                                    opnd_create_reg(REG_R8D)));
    }
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_pusha(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    /* translate: pusha/pushad => mov rsp -> r8
     *                            lea -16(rsp)/-32(rsp) -> rsp
     *                            mov ax/eax -> 14(rsp)/28(rsp)
     *                            mov cx/ecx -> 12(rsp)/24(rsp)
     *                            mov dx/edx -> 10(rsp)/20(rsp)
     *                            mov bx/ebx -> 8(rsp)/16(rsp)
     *                            mov r8w/r8d -> 6(rsp)/12(rsp)
     *                            mov bp/ebp -> 4(rsp)/8(rsp)
     *                            mov si/esi -> 2(rsp)/4(rsp)
     *                            mov di/edi -> (rsp)
     */
    opnd_t src = instr_get_src(*instr, 0);
    opnd_size_t opsz = opnd_get_size(src);
    uint opsz_bytes = opnd_size_in_bytes(opsz);
    int i;

    pre(ilist, *instr,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8), opnd_create_reg(REG_RSP)));
    pre(ilist, *instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                         OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -8 * opsz_bytes)));
    for (i = 7; i >= 0; --i) {
        instr_t *mov;
        mov = INSTR_CREATE_mov_st(
            dcontext, opnd_create_base_disp(REG_RSP, REG_NULL, 0, i * opsz_bytes, opsz),
            opnd_create_reg(reg_32_to_opsz(pushad_registers[i], opsz)));
        if (i > 0)
            pre(ilist, *instr, mov);
        else
            replace(dcontext, ilist, instr, mov);
    }
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_popa(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    /* touch high and low memory up front to make sure no faults.
     * translate: popa/popad => mov 14(rsp)/28(rsp) -> r8w/r8d
     *                          mov (rsp) -> di/edi
     *                          mov 2(rsp)/4(rsp) -> si/esi
     *                          mov 4(rsp)/8(rsp) -> bp/ebp
     *                          mov 8(rsp)/16(rsp) -> bx/ebx
     *                          mov 10(rsp)/20(rsp) -> dx/edx
     *                          mov 12(rsp)/24(rsp) -> cx/ecx
     *                          mov 14(rsp)/28(rsp) -> ax/eax
     *                          lea 16(rsp)/32(rsp) -> rsp
     */
    opnd_t dst = instr_get_dst(*instr, 0);
    opnd_size_t opsz = opnd_get_size(dst);
    uint opsz_bytes = opnd_size_in_bytes(opsz);
    int i;

    pre(ilist, *instr,
        INSTR_CREATE_mov_ld(
            dcontext, opnd_create_reg(reg_32_to_opsz(REG_R8D, opsz)),
            opnd_create_base_disp(REG_RSP, REG_NULL, 0, 7 * opsz_bytes, opsz)));
    for (i = 0; i < 8; ++i) {
        if (pushad_registers[i] == REG_ESP) /* skip sp/esp */
            continue;
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(
                dcontext, opnd_create_reg(reg_32_to_opsz(pushad_registers[i], opsz)),
                opnd_create_base_disp(REG_RSP, REG_NULL, 0, i * opsz_bytes, opsz)));
    }
    replace(dcontext, ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                             OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, 8 * opsz_bytes)));
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_into(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    /* translate: into => jno_short next_instr
     *                    int 4
     */
    pre(ilist, *instr,
        INSTR_CREATE_jcc_short(dcontext, OP_jno_short,
                               opnd_create_instr(instr_get_next(*instr))));
    replace(dcontext, ilist, instr, INSTR_CREATE_int(dcontext, OPND_CREATE_INT8(4)));
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_load_far_pointer(dcontext_t *dcontext, instrlist_t *ilist,
                           INOUT instr_t **instr)
{
    /* translate: les (src) -> dst, sreg => mov (src) -> r8w/r8d
     *                                      mov 2(src)/4(src) -> sreg
     *                                      mov r8w/r8d -> dst
     */
    opnd_t dst = instr_get_dst(*instr, 0);
    opnd_t sreg = instr_get_dst(*instr, 1);
    opnd_t src = opnd_change_base_reg_to_64(instr_get_src(*instr, 0));
    if (opnd_get_size(dst) == OPSZ_2) {
        opnd_set_size(&src, OPSZ_2);
        pre(ilist, *instr, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8W), src));
        opnd_set_disp(&src, 2);
        pre(ilist, *instr, INSTR_CREATE_mov_seg(dcontext, sreg, src));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_mov_ld(dcontext, dst, opnd_create_reg(REG_R8W)));
    } else {
        ASSERT(opnd_get_size(dst) == OPSZ_4);
        opnd_set_size(&src, OPSZ_4);
        pre(ilist, *instr, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D), src));
        opnd_set_disp(&src, 4);
        opnd_set_size(&src, OPSZ_2);
        pre(ilist, *instr, INSTR_CREATE_mov_seg(dcontext, sreg, src));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_mov_ld(dcontext, dst, opnd_create_reg(REG_R8D)));
    }
    STATS_INC(num_32bit_instrs_translated);
}

static void
translate_leave(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    opnd_t dst = instr_get_dst(*instr, 0);
    if (opnd_get_size(dst) == OPSZ_4) {
        /* translate: leave => mov ebp -> esp
         *                     mov (rsp) -> ebp
         *                     lea 4(rsp) -> rsp
         */
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ESP),
                                opnd_create_reg(REG_EBP)));
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EBP),
                                OPND_CREATE_MEM32(REG_RSP, 0)));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                 OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, 4)));
        STATS_INC(num_32bit_instrs_translated);
    }
}

static void
translate_pushf(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    opnd_t src = instr_get_src(*instr, 0);
    if (opnd_get_size(src) == OPSZ_4) {
        /* N.B.: here we assume that we can read and write the top-of-stack,
         * which may violate app transparency. this may fault or create a race
         * if esp is pointing to the base of an empty stack.
         *
         * translate: pushfd => mov (rsp) -> r8d
         *                      lea 4(rsp) -> rsp
         *                      pushfq
         *                      mov r8d -> 4(rsp)
         */
        pre(ilist, *instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8D),
                                OPND_CREATE_MEM32(REG_RSP, 0)));
        pre(ilist, *instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                             OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, 4)));
        pre(ilist, *instr, INSTR_CREATE_pushf(dcontext));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_RSP, 4),
                                    opnd_create_reg(REG_R8D)));
        STATS_INC(num_32bit_instrs_translated);
    }
}

static void
translate_popf(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    opnd_t dst = instr_get_dst(*instr, 0);
    if (opnd_get_size(dst) == OPSZ_4) {
        /* N.B.: here we assume that we can read and write the top-of-stack,
         * which may violate app transparency. this may fault or create a race
         * if esp is pointing to the base of an empty stack.
         *
         * translate: popfd => popfq
         *                     lea -4(rsp) -> rsp
         */
        pre(ilist, *instr, INSTR_CREATE_popf(dcontext));
        replace(dcontext, ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_RSP),
                                 OPND_CREATE_MEM_lea(REG_RSP, REG_NULL, 0, -4)));
        STATS_INC(num_32bit_instrs_translated);
    }
}

void
translate_x86_to_x64(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr)
{
    int opc = instr_get_opcode(*instr);
    ASSERT(instrlist_get_our_mangling(ilist));
    ASSERT(instr_get_x86_mode(*instr));
    switch (opc) {
    case OP_call_ind: translate_indirect_call(dcontext, ilist, instr); break;
    case OP_jmp_ind: translate_indirect_jump(dcontext, ilist, instr); break;
    case OP_push: translate_push(dcontext, ilist, instr); break;
    case OP_push_imm: translate_push_imm(dcontext, ilist, instr); break;
    case OP_pop: translate_pop(dcontext, ilist, instr); break;
    case OP_pusha: translate_pusha(dcontext, ilist, instr); break;
    case OP_popa: translate_popa(dcontext, ilist, instr); break;
    case OP_into: translate_into(dcontext, ilist, instr); break;
    case OP_les:
    case OP_lds: translate_load_far_pointer(dcontext, ilist, instr); break;
    case OP_leave: translate_leave(dcontext, ilist, instr); break;
    case OP_enter:
        /* NYI. shoule be similar to leave. */
        ASSERT_NOT_IMPLEMENTED(false);
        break;
    case OP_pushf: translate_pushf(dcontext, ilist, instr); break;
    case OP_popf: translate_popf(dcontext, ilist, instr); break;
    case OP_daa:
    case OP_das:
    case OP_aaa:
    case OP_aas:
    case OP_aam:
    case OP_aad:
    case OP_bound:
    case OP_arpl:
    case OP_salc:
    case OP_mov_priv:
    case OP_sgdt:
    case OP_sidt:
    case OP_lidt:
    case OP_lgdt:
        /* NYI. should just bail -- leave the instr as x86. */
        ASSERT_NOT_IMPLEMENTED(false);
        return;
    default:
        /* instr is valid in x64; no need to translate. */
        /* make memory reference operands use 64-bit regs in order to save the
         * addr32 prefix, because the high 32 bits should be zero at this time.
         */
        instr_change_base_reg_to_64(*instr);
        break;
    }
    instr_set_our_mangling(*instr, true);
    instr_set_raw_bits_valid(*instr, false);
    instr_set_x86_mode(*instr, false);
}
