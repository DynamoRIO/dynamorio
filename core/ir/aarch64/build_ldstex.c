/* **********************************************************
 * Copyright (c) 2020 Google, Inc. All rights reserved.
 * Copyright (c) 2017 ARM Limited. All rights reserved.
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

/* A wrapper round the decoder that recognises single-entry single-exit blocks
 * of contiguous instructions containing an exclusive load/store pair and bundles
 * them into a macro-instruction, OP_ldstex. This is a temporary solution for
 * i#1698 and is likely to be fragile. Known problems:
 *
 * - We only handle single-entry single-exit contiguous code blocks. (Usually
 *   they are written as inline assembler so they do fit this pattern.)
 * - If the block uses all of X0-X5 and the stolen register then we cannot
 *   mangle it (so it is better not to recognise it at all).
 * - The contents of an OP_ldstex cannot be instrumented.
 * - If execution remains in an OP_ldstex then signal delivery may be delayed.
 * - Bad things might happen if there is a SIGSEGV or SIGBUS in an OP_ldstex.
 * - Code flushing.
 *
 * This is currently modularised as a layer between the normal decoder and the
 * block builder. It might be better to merge it with the block builder.
 *
 * If this solution can be made robust then it might be worth porting it to
 * ARM/AArch32.
 */

#include "../globals.h"
#include "arch.h"
#include "decode.h"
#include "disassemble.h"
#include "instr.h"
#include "instr_create_shared.h"

#include "build_ldstex.h"

static bool
instr_is_nonbranch_pcrel(instr_t *instr)
{
    int i, n;
    n = instr_num_dsts(instr);
    for (i = 0; i < n; i++)
        ASSERT(!OPND_IS_REL_ADDR(instr_get_dst(instr, i)));
    n = instr_num_srcs(instr);
    for (i = 0; i < n; i++) {
        if (OPND_IS_REL_ADDR(instr_get_src(instr, i)))
            return true;
    }
    return false;
}

static void
instr_create_ldstex(dcontext_t *dcontext, int len, uint *pc, instr_t *instr,
                    OUT instr_t *instr_ldstex)
{
    int num_dsts = 0;
    int num_srcs = 0;
    int i, d, s, j;

    for (i = 0; i < len; i++) {
        ASSERT(instr[i].length == AARCH64_INSTR_SIZE &&
               instr[i].bytes == instr[0].bytes + AARCH64_INSTR_SIZE * i);
        num_dsts += instr_num_dsts(&instr[i]);
        num_srcs += instr_num_srcs(&instr[i]);
    }
    instr_set_opcode(instr_ldstex, OP_ldstex);
    instr_set_num_opnds(dcontext, instr_ldstex, num_dsts, num_srcs);
    d = 0;
    s = 0;
    for (i = 0; i < len; i++) {
        int dsts = instr_num_dsts(&instr[i]);
        int srcs = instr_num_srcs(&instr[i]);
        for (j = 0; j < dsts; j++)
            instr_set_dst(instr_ldstex, d++, instr_get_dst(&instr[i], j));
        for (j = 0; j < srcs; j++)
            instr_set_src(instr_ldstex, s++, instr_get_src(&instr[i], j));
    }
    ASSERT(d == num_dsts && s == num_srcs);
    /* Set raw bits to original encoding. */
    instr_set_raw_bits(instr_ldstex, instr[0].bytes, len * AARCH64_INSTR_SIZE);
    /* Conservatively assume all flags are read and written. */
    instr_ldstex->eflags = EFLAGS_READ_ALL | EFLAGS_WRITE_ALL;
    instr_set_eflags_valid(instr_ldstex, true);
}

/* Here we attempt to combine a loop involving ldex (load exclusive) and
 * stex (store exclusive) into an OP_ldstex macro-instruction. The algorithm
 * is roughly this:
 *
 * Decode up to (2 * N) instructions while:
 * - none of them are indirect branches or system calls
 * - none of them is a direct branch out of these (2 * N) instructions
 * - none of them is OP_xx (to be safe)
 * - there is, or might yet be, both ldex and stex in the first N
 * - none of them is a non-branch PC-relative instruction: ADR, ADRP,
 *   PC-relative PRFM, literal load (this last condition could be removed
 *   if we mangled such instructions as we encountered them)
 *
 * To save time, give up if the first instruction is neither ldex nor stex
 * and there is no branch to it.
 * Take a sub-block containing both ldex and stex from the first N instructions.
 * Expand this sub-block to a minimal single-entry single-exit block.
 * Give up if the sub-block grows beyond N instructions.
 * Finally, give up if the sub-block does not contain the first instruction.
 * Also give up if the sub-block uses all of X0-X5 and the stolen register
 * because we would be unable to mangle such a block.
 *
 * XXX: This function uses a lot of CPU time. It could be made faster in
 * several ways, for example by caching decoded instructions or using a
 * custom decoder to recognise the particular instructions that we care
 * about here.
 */
byte *
decode_ldstex(dcontext_t *dcontext, byte *pc_, byte *orig_pc_, instr_t *instr_ldstex)
{
#define N (MAX_INSTR_LENGTH / AARCH64_INSTR_SIZE)
    instr_t ibuf[2 * N];
    uint *pc = (uint *)pc_;
    uint *orig_pc = (uint *)orig_pc_;
    bool seen_ldex = false;
    bool seen_stex = false;
    bool seen_branch_to_start = false;
    bool failed = false;
    int ldstex_beg = -1;
    int ldstex_end = -1;
    int i, len;

    /* Decode up to 2 * N instructions. */
    for (i = 0; i < N; i++) {
        instr_t *instr = &ibuf[i];
        instr_init(dcontext, instr);
        decode_from_copy(dcontext, (byte *)(pc + i), (byte *)(orig_pc + i), instr);
        if (instr_is_mbr_arch(instr) || instr_is_syscall(instr) ||
            instr_get_opcode(instr) == OP_xx || instr_is_nonbranch_pcrel(instr))
            break;
        if (instr_is_ubr_arch(instr) || instr_is_cbr_arch(instr)) {
            ptr_uint_t target = (ptr_uint_t)instr_get_branch_target_pc(instr);
            if (target < (ptr_uint_t)pc || target > (ptr_uint_t)(pc + 2 * N))
                break;
            if (target == (ptr_uint_t)pc)
                seen_branch_to_start = true;
        }
        if (instr_is_exclusive_load(instr))
            seen_ldex = true;
        if (instr_is_exclusive_store(instr))
            seen_stex = true;
        if (i + 1 >= N && !(seen_ldex && seen_stex))
            break;
        if (ldstex_beg == -1 && (seen_ldex || seen_stex))
            ldstex_beg = i;
        if (ldstex_end == -1 && (seen_ldex && seen_stex))
            ldstex_end = i + 1;
    }
    if (i < N) {
        instr_reset(dcontext, &ibuf[i]);
        len = i;
    } else
        len = N;

    /* Quick check for hopeless situations. */
    if (len == 0 || !(seen_ldex && seen_stex) ||
        !(seen_branch_to_start ||
          (instr_is_exclusive_load(&ibuf[0]) || instr_is_exclusive_store(&ibuf[0])))) {
        for (i = 0; i < len; i++)
            instr_reset(dcontext, &ibuf[i]);
        return NULL;
    }

    /* There are several ways we could choose a sub-block containing both ldex
     * and stex from the first N instructions. Investigate further, perhaps.
     * We have already set ldstex_beg and ldstex_end.
     */
    ASSERT(ldstex_beg != -1 && ldstex_end != -1 && ldstex_beg < ldstex_end);

    /* Expand ldstex sub-block until it is a single-entry single-exit block. */
    for (;;) {
        int new_beg = ldstex_beg;
        int new_end = ldstex_end;
        for (i = ldstex_beg; i < ldstex_end; i++) {
            instr_t *instr = &ibuf[i];
            if (instr_is_ubr_arch(instr) || instr_is_cbr_arch(instr)) {
                int target = (uint *)instr_get_branch_target_pc(instr) - pc;
                if (target > len) {
                    failed = true;
                    break;
                }
                if (target < new_beg)
                    new_beg = target;
                if (target > new_end)
                    new_end = target;
            }
        }
        if (new_beg == ldstex_beg && new_end == ldstex_end)
            break;
        ldstex_beg = new_beg;
        ldstex_end = new_end;
    }

    if (ldstex_beg != 0)
        failed = true;

    if (!failed) {
        /* Check whether the sub-block uses the stolen register and all of X0-X5.
         * If it does, it would be impossible to mangle it so it is better not to
         * create an OP_ldstex.
         */
        reg_id_t regs[] = { dr_reg_stolen, DR_REG_X0, DR_REG_X1, DR_REG_X2,
                            DR_REG_X3,     DR_REG_X4, DR_REG_X5 };
        int r;
        for (r = 0; r < sizeof(regs) / sizeof(*regs); r++) {
            for (i = ldstex_beg; i < ldstex_end; i++) {
                if (instr_uses_reg(&ibuf[i], regs[r]))
                    break;
            }
            if (i >= ldstex_end)
                break;
        }
        if (r >= sizeof(regs) / sizeof(*regs))
            failed = true;
    }

    if (!failed) {
        instr_create_ldstex(dcontext, ldstex_end - ldstex_beg, pc + ldstex_beg,
                            &ibuf[ldstex_beg], instr_ldstex);
    }

    for (i = 0; i < len; i++)
        instr_reset(dcontext, &ibuf[i]);
    return failed ? NULL : (byte *)(pc + ldstex_end);
}

static byte *
decode_common_with_ldstex(dcontext_t *dcontext, byte *pc, byte *orig_pc, instr_t *instr)
{
    if (INTERNAL_OPTION(unsafe_build_ldstex)) {
        byte *pc_next = decode_ldstex(dcontext, pc, orig_pc, instr);
        if (pc_next != NULL)
            return pc_next;
    }
    return decode_from_copy(dcontext, pc, orig_pc, instr);
}

byte *
decode_with_ldstex(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    return decode_common_with_ldstex(dcontext, pc, pc, instr);
}

byte *
decode_cti_with_ldstex(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    return decode_with_ldstex(dcontext, pc, instr);
}
