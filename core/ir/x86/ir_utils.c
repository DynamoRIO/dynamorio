/* ******************************************************************************
 * Copyright (c) 2010-2020 Google, Inc.  All rights reserved.
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

/* file "ir_utils.c": multi-instruction manipulation shared between the core
 * and drdecode.
 */

#include "../globals.h"
#include "decode.h"
#include "instr_create_shared.h"
#include "instrument.h"

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE instrlist_meta_preinsert

/* If src_inst != NULL, uses it (and assumes it will be encoded at
 * encode_estimate to determine whether > 32 bits or not: so if unsure where
 * it will be encoded, pass a high address) as the immediate; else
 * uses val.
 * Keep this in sync with patch_mov_immed_arch().
 */
void
insert_mov_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                      ptr_int_t val, opnd_t dst, instrlist_t *ilist, instr_t *instr,
                      OUT instr_t **first, OUT instr_t **last)
{
    instr_t *mov1, *mov2;
    if (src_inst != NULL)
        val = (ptr_int_t)encode_estimate;
#ifdef X64
    if (X64_MODE_DC(dcontext) && !opnd_is_reg(dst)) {
        if (val <= INT_MAX && val >= INT_MIN) {
            /* mov is sign-extended, so we can use one move if it is all
             * 0 or 1 in top 33 bits
             */
            mov1 = INSTR_CREATE_mov_imm(dcontext, dst,
                                        (src_inst == NULL)
                                            ? OPND_CREATE_INT32((int)val)
                                            : opnd_create_instr_ex(src_inst, OPSZ_4, 0));
            PRE(ilist, instr, mov1);
            mov2 = NULL;
        } else {
            /* do mov-64-bit-immed in two pieces.  tiny corner-case risk of racy
             * access to [dst] if this thread is suspended in between or another
             * thread is trying to read [dst], but o/w we have to spill and
             * restore a register.
             */
            CLIENT_ASSERT(opnd_is_memory_reference(dst), "invalid dst opnd");
            /* mov low32 => [mem32] */
            opnd_set_size(&dst, OPSZ_4);
            mov1 = INSTR_CREATE_mov_st(dcontext, dst,
                                       (src_inst == NULL)
                                           ? OPND_CREATE_INT32((int)val)
                                           : opnd_create_instr_ex(src_inst, OPSZ_4, 0));
            PRE(ilist, instr, mov1);
            /* mov high32 => [mem32+4] */
            if (opnd_is_base_disp(dst)) {
                int disp = opnd_get_disp(dst);
                CLIENT_ASSERT(disp + 4 > disp, "disp overflow");
                opnd_set_disp(&dst, disp + 4);
            } else {
                byte *addr = opnd_get_addr(dst);
                CLIENT_ASSERT(!POINTER_OVERFLOW_ON_ADD(addr, 4), "addr overflow");
                dst = OPND_CREATE_ABSMEM(addr + 4, OPSZ_4);
            }
            mov2 = INSTR_CREATE_mov_st(dcontext, dst,
                                       (src_inst == NULL)
                                           ? OPND_CREATE_INT32((int)(val >> 32))
                                           : opnd_create_instr_ex(src_inst, OPSZ_4, 32));
            PRE(ilist, instr, mov2);
        }
    } else {
#endif
        mov1 = INSTR_CREATE_mov_imm(dcontext, dst,
                                    (src_inst == NULL)
                                        ? OPND_CREATE_INTPTR(val)
                                        : opnd_create_instr_ex(src_inst, OPSZ_PTR, 0));
        PRE(ilist, instr, mov1);
        mov2 = NULL;
#ifdef X64
    }
#endif
    if (first != NULL)
        *first = mov1;
    if (last != NULL)
        *last = mov2;
}

/* If src_inst != NULL, uses it (and assumes it will be encoded at
 * encode_estimate to determine whether > 32 bits or not: so if unsure where
 * it will be encoded, pass a high address) as the immediate; else
 * uses val.
 */
void
insert_push_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                       OUT instr_t **first, OUT instr_t **last)
{
    instr_t *push, *mov;
    if (src_inst != NULL)
        val = (ptr_int_t)encode_estimate;
#ifdef X64
    if (X64_MODE_DC(dcontext)) {
        /* do push-64-bit-immed in two pieces.  tiny corner-case risk of racy
         * access to TOS if this thread is suspended in between or another
         * thread is trying to read its stack, but o/w we have to spill and
         * restore a register.
         */
        push = INSTR_CREATE_push_imm(dcontext,
                                     (src_inst == NULL)
                                         ? OPND_CREATE_INT32((int)val)
                                         : opnd_create_instr_ex(src_inst, OPSZ_4, 0));
        PRE(ilist, instr, push);
        /* push is sign-extended, so we can skip top half if it is all 0 or 1
         * in top 33 bits
         */
        if (val <= INT_MAX && val >= INT_MIN) {
            mov = NULL;
        } else {
            mov = INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 4),
                                      (src_inst == NULL)
                                          ? OPND_CREATE_INT32((int)(val >> 32))
                                          : opnd_create_instr_ex(src_inst, OPSZ_4, 32));
            PRE(ilist, instr, mov);
        }
    } else {
#endif
        push = INSTR_CREATE_push_imm(dcontext,
                                     (src_inst == NULL)
                                         ? OPND_CREATE_INT32(val)
                                         : opnd_create_instr_ex(src_inst, OPSZ_4, 0));
        PRE(ilist, instr, push);
        mov = NULL;
#ifdef X64
    }
#endif
    if (first != NULL)
        *first = push;
    if (last != NULL)
        *last = mov;
}

/* Convert a short-format CTI into an equivalent one using
 * near-rel-format.
 * Remember, the target is kept in the 0th src array position,
 * and has already been converted from an 8-bit offset to an
 * absolute PC, so we can just pretend instructions are longer
 * than they really are.
 */
instr_t *
convert_to_near_rel_arch(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    app_pc target = NULL;

    if (opcode == OP_jmp_short) {
        instr_set_opcode(instr, OP_jmp);
        return instr;
    }

    if (OP_jo_short <= opcode && opcode <= OP_jnle_short) {
        /* WARNING! following is OP_ enum order specific */
        instr_set_opcode(instr, opcode - OP_jo_short + OP_jo);
        return instr;
    }

    if (OP_loopne <= opcode && opcode <= OP_jecxz) {
        uint mangled_sz;
        uint offs;
        /*
         * from "info as" on GNU/linux system:
       Note that the `jcxz', `jecxz', `loop', `loopz', `loope', `loopnz'
       and `loopne' instructions only come in byte displacements, so that if
       you use these instructions (`gcc' does not use them) you may get an
       error message (and incorrect code).  The AT&T 80386 assembler tries to
       get around this problem by expanding `jcxz foo' to
                     jcxz cx_zero
                     jmp cx_nonzero
            cx_zero: jmp foo
            cx_nonzero:
        *
        * We use that same expansion, but we want to treat the entire
        * three-instruction sequence as a single conditional branch.
        * Thus we use a special instruction that stores the entire
        * instruction sequence as mangled bytes, yet w/ a valid target operand
        * (xref PR 251646).
        * patch_branch and instr_invert_cbr
        * know how to find the target pc (final 4 of 9 bytes).
        * When decoding anything we've written we know the only jcxz or
        * loop* instructions are part of these rewritten packages, and
        * we use remangle_short_rewrite to read back in the instr.
        * (have to do this everywhere call decode() except original
        * interp, plus in input_trace())
        *
        * An alternative is to change 'jcxz foo' to:
                    <save eflags>
                    cmpb %cx,$0
                    je   foo_restore
                    <restore eflags>
                    ...
       foo_restore: <restore eflags>
               foo:
        * However the added complications of restoring the eflags on
        * the taken-branch path made me choose the former solution.
        */

        /* SUMMARY:
         * expand 'shortjump foo' to:
                          shortjump taken
                          jmp-short nottaken
                   taken: jmp foo
                nottaken:
        */
        if (ilist != NULL) {
            /* PR 266292: for meta instrs, insert separate instrs */
            /* reverse order */
            opnd_t tgt = instr_get_target(instr);
            instr_t *nottaken = INSTR_CREATE_label(dcontext);
            instr_t *taken = INSTR_CREATE_jmp(dcontext, tgt);
            ASSERT(instr_is_meta(instr));
            instrlist_meta_postinsert(ilist, instr, nottaken);
            instrlist_meta_postinsert(ilist, instr, taken);
            instrlist_meta_postinsert(
                ilist, instr,
                INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(nottaken)));
            instr_set_target(instr, opnd_create_instr(taken));
            return taken;
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
        /* need 9 bytes + possible addr prefix */
        mangled_sz = CTI_SHORT_REWRITE_LENGTH;
        if (!reg_is_pointer_sized(opnd_get_reg(instr_get_src(instr, 1))))
            mangled_sz++; /* need addr prefix */
        instr_allocate_raw_bits(dcontext, instr, mangled_sz);
        offs = 0;
        if (mangled_sz > CTI_SHORT_REWRITE_LENGTH) {
            instr_set_raw_byte(instr, offs, ADDR_PREFIX_OPCODE);
            offs++;
        }
        /* first 2 bytes: jecxz 8-bit-offset */
        instr_set_raw_byte(instr, offs, decode_first_opcode_byte(opcode));
        offs++;
        /* remember pc-relative offsets are from start of next instr */
        instr_set_raw_byte(instr, offs, (byte)2);
        offs++;
        /* next 2 bytes: jmp-short 8-bit-offset */
        instr_set_raw_byte(instr, offs, decode_first_opcode_byte(OP_jmp_short));
        offs++;
        instr_set_raw_byte(instr, offs, (byte)5);
        offs++;
        /* next 5 bytes: jmp 32-bit-offset */
        instr_set_raw_byte(instr, offs, decode_first_opcode_byte(OP_jmp));
        offs++;
        /* for x64 we may not reach, but we go ahead and try */
        instr_set_raw_word(instr, offs, (int)(target - (instr->bytes + mangled_sz)));
        offs += sizeof(int);
        ASSERT(offs == mangled_sz);
        LOG(THREAD, LOG_INTERP, 2, "convert_to_near_rel: jecxz/loop* opcode\n");
        /* original target operand is still valid */
        instr_set_operands_valid(instr, true);
        return instr;
    }

    LOG(THREAD, LOG_INTERP, 1, "convert_to_near_rel: unknown opcode: %d %s\n", opcode,
        decode_opcode_name(opcode));
    ASSERT_NOT_REACHED(); /* conversion not possible OR not a short-form cti */
    return instr;
}

/* XXX: Best to move DR-execution-related things like this out of core/ir/ and into
 * core/arch/, but untangling them all will take some work, so for now it lives here.
 */
/* For jecxz and loop*, we create 3 instructions in a single
 * instr that we treat like a single conditional branch.
 * On re-decoding our own output we need to recreate that instr.
 * This routine assumes that the instructions encoded at pc
 * are indeed a mangled cti short.
 * Assumes that the first instr has already been decoded into instr,
 * that pc points to the start of that instr.
 * Converts instr into a new 3-raw-byte-instr with a private copy of the
 * original raw bits.
 * Optionally modifies the target to "target" if "target" is non-null.
 * Returns the pc of the instruction after the remangled sequence.
 */
byte *
remangle_short_rewrite(dcontext_t *dcontext, instr_t *instr, byte *pc, app_pc target)
{
    uint mangled_sz = CTI_SHORT_REWRITE_LENGTH;
    ASSERT(instr_is_cti_short_rewrite(instr, pc));
    if (*pc == ADDR_PREFIX_OPCODE)
        mangled_sz++;

    /* first set the target in the actual operand src0 */
    if (target == NULL) {
        /* acquire existing absolute target */
        int rel_target = *((int *)(pc + mangled_sz - 4));
        target = pc + mangled_sz + rel_target;
    }
    instr_set_target(instr, opnd_create_pc(target));
    /* now set up the bundle of raw instructions
     * we've already read the first 2-byte instruction, jecxz/loop*
     * they all take up mangled_sz bytes
     */
    instr_allocate_raw_bits(dcontext, instr, mangled_sz);
    instr_set_raw_bytes(instr, pc, mangled_sz);
    /* for x64 we may not reach, but we go ahead and try */
    instr_set_raw_word(instr, mangled_sz - 4, (int)(target - (pc + mangled_sz)));
    /* now make operands valid */
    instr_set_operands_valid(instr, true);
    return (pc + mangled_sz);
}
