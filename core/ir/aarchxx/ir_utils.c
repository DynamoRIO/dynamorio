/* **********************************************************
 * Copyright (c) 2014-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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
 * * Neither the name of ARM Limited nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL ARM LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "../globals.h"
#include "instr_create_shared.h"
#include "instrument.h"

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE instrlist_meta_preinsert

/* XXX: Best to move DR-execution-related things like this out of core/ir/ and into
 * core/arch/, but untangling them all will take some work, so for now it lives here.
 */
byte *
remangle_short_rewrite(dcontext_t *dcontext, instr_t *instr, byte *pc, app_pc target)
{
#ifdef AARCH64
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
#else
    uint mangled_sz = CTI_SHORT_REWRITE_LENGTH;
    uint raw_jmp;
    ASSERT(instr_is_cti_short_rewrite(instr, pc));
    if (target == NULL)
        target = decode_raw_jmp_target(dcontext, pc + CTI_SHORT_REWRITE_B_OFFS);
    instr_set_target(instr, opnd_create_pc(target));
    instr_allocate_raw_bits(dcontext, instr, mangled_sz);
    instr_set_raw_bytes(instr, pc, mangled_sz);
    encode_raw_jmp(dr_get_isa_mode(dcontext), target, (byte *)&raw_jmp,
                   pc + CTI_SHORT_REWRITE_B_OFFS);
    instr_set_raw_word(instr, CTI_SHORT_REWRITE_B_OFFS, raw_jmp);
    instr_set_operands_valid(instr, true);
    return (pc + mangled_sz);
#endif
}

instr_t *
convert_to_near_rel_arch(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
#ifdef AARCH64
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
#else
    int opcode = instr_get_opcode(instr);
    if (opcode == OP_b_short) {
        instr_set_opcode(instr, OP_b);
        return instr;
    } else if (opcode == OP_cbz || opcode == OP_cbnz) {
        /* While for non-trace-mode we could get by w/o converting,
         * as we use local stubs with a far-away link-through-stub
         * soln needed even for regular branches and thus these would
         * reach the stub, they won't reach for traces.
         * Thus we mirror what x86 does for jecxz:
         *       cbz foo
         *  =>
         *       cbnz fall
         *       jmp foo
         *  fall:
         *
         * The fact that we invert the cbr ends up requiring extra logic
         * in linkstub_cbr_disambiguate().
         */
        app_pc target = NULL;
        uint mangled_sz, offs, raw_jmp;
        reg_id_t src_reg;

        if (ilist != NULL) {
            /* PR 266292: for meta instrs, insert separate instrs */
            opnd_t tgt = instr_get_target(instr);
            instr_t *fall = INSTR_CREATE_label(dcontext);
            instr_t *jmp = INSTR_CREATE_b(dcontext, tgt);
            ASSERT(instr_is_meta(instr));
            /* reverse order */
            instrlist_meta_postinsert(ilist, instr, fall);
            instrlist_meta_postinsert(ilist, instr, jmp);
            instrlist_meta_postinsert(ilist, instr, instr);
            instr_set_target(instr, opnd_create_instr(fall));
            instr_invert_cbr(instr);
            return jmp; /* API specifies we return the long-reach cti */
        }

        if (opnd_is_near_pc(instr_get_target(instr)))
            target = opnd_get_pc(instr_get_target(instr));
        else if (opnd_is_near_instr(instr_get_target(instr))) {
            instr_t *tgt = opnd_get_instr(instr_get_target(instr));
            /* XXX: not using get_app_instr_xl8() b/c drdecodelib doesn't link
             * mangle_shared.c.
             */
            target = instr_get_translation(tgt);
            if (target == NULL && instr_raw_bits_valid(tgt))
                target = instr_get_raw_bits(tgt);
            ASSERT(target != NULL);
        } else
            ASSERT_NOT_REACHED();

        /* PR 251646: cti_short_rewrite: target is in src0, so operands are
         * valid, but raw bits must also be valid, since they hide the multiple
         * instrs.  For x64, it is marked for re-relativization, but it's
         * special since the target must be obtained from src0 and not
         * from the raw bits (since that might not reach).
         */
        /* query IR before we set raw bits */
        ASSERT(opnd_is_reg(instr_get_src(instr, 1)));
        src_reg = opnd_get_reg(instr_get_src(instr, 1));
        /* need 6 bytes */
        mangled_sz = CTI_SHORT_REWRITE_LENGTH;
        instr_allocate_raw_bits(dcontext, instr, mangled_sz);
        offs = 0;
        /* first 2 bytes: cbz or cbnz to "cur pc" + 2 which means immed is 1 */
        instr_set_raw_byte(instr, offs, 0x08 | (src_reg - DR_REG_R0));
        offs++;
        instr_set_raw_byte(instr, offs, (opcode == OP_cbz) ? CBNZ_BYTE_A : CBZ_BYTE_A);
        offs++;
        /* next 4 bytes: b to target */
        ASSERT(offs == CTI_SHORT_REWRITE_B_OFFS);
        encode_raw_jmp(dr_get_isa_mode(dcontext),
                       instr->bytes + offs /*not target, b/c may not reach*/,
                       (byte *)&raw_jmp, instr->bytes + offs);
        instr_set_raw_word(instr, offs, raw_jmp);
        offs += sizeof(int);
        ASSERT(offs == mangled_sz);
        LOG(THREAD, LOG_INTERP, 2, "convert_to_near_rel: cbz/cbnz opcode\n");
        /* original target operand is still valid */
        instr_set_operands_valid(instr, true);
        return instr;
    }
    ASSERT_NOT_REACHED();
    return instr;
#endif
}

/* Keep this in sync with patch_mov_immed_arch(). */
void
insert_mov_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                      ptr_int_t val, opnd_t dst, instrlist_t *ilist, instr_t *instr,
                      OUT instr_t **first, OUT instr_t **last)
{
#ifdef AARCH64
    instr_t *mov;
    int i;

    CLIENT_ASSERT(opnd_is_reg(dst), "AArch64 cannot store an immediate direct to memory");

    if (opnd_get_reg(dst) == DR_REG_XZR) {
        /* Moving a value to the zero register is a no-op. We insert nothing,
         * so *first and *last are set to NULL. Caller beware!
         */
        if (first != NULL)
            *first = NULL;
        if (last != NULL)
            *last = NULL;
        return;
    }

    ASSERT((uint)(opnd_get_reg(dst) - DR_REG_X0) < 31);
    if (src_inst != NULL)
        val = (ptr_int_t)encode_estimate;

    /* movz x(dst), #(val & 0xffff) */
    mov = INSTR_CREATE_movz(dcontext, dst,
                            src_inst == NULL ? OPND_CREATE_INT16(val & 0xffff)
                                             : opnd_create_instr_ex(src_inst, OPSZ_2, 0),
                            OPND_CREATE_INT8(0));
    PRE(ilist, instr, mov);
    if (first != NULL)
        *first = mov;
    for (i = 1; i < 4; i++) {
        if ((val >> (16 * i) & 0xffff) != 0) {
            /* movk x(dst), #(val >> sh & 0xffff), lsl #(sh) */
            mov = INSTR_CREATE_movk(dcontext, dst,
                                    src_inst == NULL
                                        ? OPND_CREATE_INT16((val >> 16 * i) & 0xffff)
                                        : opnd_create_instr_ex(src_inst, OPSZ_2, 16 * i),
                                    OPND_CREATE_INT8(i * 16));
            PRE(ilist, instr, mov);
        }
    }
    if (last != NULL)
        *last = mov;
#else
    instr_t *mov1, *mov2;
    if (src_inst != NULL)
        val = (ptr_int_t)encode_estimate;
    CLIENT_ASSERT(opnd_is_reg(dst), "ARM cannot store an immediate direct to memory");
    /* MVN writes the bitwise inverse of an immediate value to the dst register */
    /* XXX: we could check for larger tile/rotate immed patterns */
    if (src_inst == NULL && ~val >= 0 && ~val <= 0xff) {
        mov1 = INSTR_CREATE_mvn(dcontext, dst, OPND_CREATE_INT(~val));
        PRE(ilist, instr, mov1);
        mov2 = NULL;
    } else {
        /* To use INT16 here and pass the size checks in opnd_create_immed_int
         * we'd have to add UINT16 (or sign-extend the bottom half again):
         * simpler to use INT, and our general ARM philosophy is to use INT and
         * ignore immed sizes at instr creation time (only at encode time do we
         * check them).
         */
        mov1 = INSTR_CREATE_movw(dcontext, dst,
                                 (src_inst == NULL)
                                     ? OPND_CREATE_INT(val & 0xffff)
                                     : opnd_create_instr_ex(src_inst, OPSZ_2, 0));
        PRE(ilist, instr, mov1);
        val = (val >> 16) & 0xffff;
        if (val == 0) {
            /* movw zero-extends so we're done */
            mov2 = NULL;
        } else {
            mov2 = INSTR_CREATE_movt(dcontext, dst,
                                     (src_inst == NULL)
                                         ? OPND_CREATE_INT(val)
                                         : opnd_create_instr_ex(src_inst, OPSZ_2, 16));
            PRE(ilist, instr, mov2);
        }
    }
    if (first != NULL)
        *first = mov1;
    if (last != NULL)
        *last = mov2;
#endif
}

void
insert_push_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                       OUT instr_t **first, OUT instr_t **last)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1551, i#1569 */
}
