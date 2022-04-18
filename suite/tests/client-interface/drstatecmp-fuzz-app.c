/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
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

/* Fuzzing application to stress-test DR with the drstatecmp library.
 * Only AArch64 is currently supported.
 */

#include "configure.h"
#include "dr_api.h"
/* The opcode_opnd_pairs.h is generated for this fuzzer by codec.py from codec.txt. */
#include "opcode_opnd_pairs.h"
#include "tools.h"
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define NUM_INSTS 10000

#define VERBOSE 0

#ifdef AARCH64

static byte *generated_code;
static size_t max_code_size;

static jmp_buf mark;
static sigjmp_buf sig_mark;

void
sig_segv_fpe_handler(int signal, siginfo_t *siginfo, ucontext_t *ucxt)
{
    siglongjmp(sig_mark, 1);
    DR_ASSERT_MSG(false, "Shouldn't be reached");
}

void
sigill_handler(int signal, siginfo_t *siginfo, ucontext_t *uctx)
{
    /* Skip illegal instructions. */
    uctx->uc_mcontext.pc += 4;
}

static void
print_instr_pc(instr_t *instr, byte *encode_pc)
{
    fprintf(stderr, "%p: ", encode_pc);
    instr_disassemble(GLOBAL_DCONTEXT, instr, STDERR);
    fprintf(stderr, "\n");
}

static byte *
append_instr(instr_t *instr, byte *encode_pc)
{
#    if VERBOSE > 0
    print_instr_pc(instr, encode_pc);
#    endif
    byte *nxt_pc = instr_encode(GLOBAL_DCONTEXT, instr, encode_pc);
    assert(nxt_pc);
    instr_destroy(GLOBAL_DCONTEXT, instr);
    return nxt_pc;
}

static uint32_t
rand_32b(void)
{
    uint32_t r = rand() & 0xff;
    r |= (rand() & 0xff) << 8;
    r |= (rand() & 0xff) << 16;
    r |= (rand() & 0xff) << 24;
    return r;
}

static uint32_t
generate_encoded_inst(void)
{
    /* Pick one of the available (side-effect-free and non-branch) opcodes
     * and randomize the non-fixed bits.
     */
    uint32_t opcode_pick = rand() % DR_FUZZ_INST_CNT;
    const dr_opcode_opnd_pair_t opcode_opnd_pair = dr_fuzz_opcode_opnd_pairs[opcode_pick];

    uint32_t opnd_mask = opcode_opnd_pair.opnd;
    /* Avoid registers 16-31 for Rd (special registers and callee-saved registers) to
     * avoid causing segmentation faults and skipping checks.
     * XXX: Could relax this constraint by pushing all callee-saved registers in a new
     * preceding basic block and popping them in a succeeding basic block (need
     * to be separate basic blocks since drstatecmp does not support memory operations).
     * Encoding for register 31 for Rd should still be avoided since this encoding is used
     * for special purposes and sometimes refers to the stack pointer or the zero
     * register.
     */
    opnd_mask &= 0b01111;
    /* Fuzz the operand bits. */
    uint32_t fuzzed_opnd = rand_32b() & opnd_mask;

    uint32_t encoded_inst = opcode_opnd_pair.opcode | fuzzed_opnd;
    return encoded_inst;
}

static int
check_decoded_inst(instr_t *decoded_inst)
{
    return instr_valid(decoded_inst) && instr_get_opcode(decoded_inst) != OP_xx &&
        instr_raw_bits_valid(decoded_inst) && instr_operands_valid(decoded_inst);
}

static byte *
generate_inst(byte *encode_pc, size_t *skipped_insts)
{
    /* Pick a random side-effect-free and non-branch instruction. */
    uint32_t encoded_inst = generate_encoded_inst();

    /* Try to decode the randomized encoding. */
    instr_t *decoded_inst = instr_create(GLOBAL_DCONTEXT);
    byte *nxt_pc = decode(GLOBAL_DCONTEXT, (byte *)&encoded_inst, decoded_inst);
    /* XXX: Ideally the decoder would report as erroneous any encoding leading to SIGILL.
     * Currently, several valid decodings are illegal instructions.
     */
    if (nxt_pc != NULL && check_decoded_inst(decoded_inst)) {
        encode_pc = append_instr(decoded_inst, encode_pc);
    } else {
        (*skipped_insts)++;
        instr_destroy(GLOBAL_DCONTEXT, decoded_inst);
    }

    return encode_pc;
}

static void
generate_code()
{
    /* Account for the generated insts and the final return. */
    max_code_size = (NUM_INSTS + 1) * 4;
    generated_code =
        (byte *)allocate_mem(max_code_size, ALLOW_EXEC | ALLOW_READ | ALLOW_WRITE);
    assert(generated_code != NULL);

    /* Synthesize code which includes a lot of side-effect-free instructions. Only one
     * basic block is created (linear control flow). To test clobbering of arithmetic
     * flags conditionally-executed instructions are included.
     */
    byte *encode_pc = generated_code;
    size_t skipped_insts = 0;
    for (int i = 0; i < NUM_INSTS; ++i) {
        encode_pc = generate_inst(encode_pc, &skipped_insts);
    }
    size_t actual_code_size = max_code_size - skipped_insts * 4;

    /* The outer level is a function. */
    encode_pc = append_instr(XINST_CREATE_return(GLOBAL_DCONTEXT), encode_pc);
    assert(encode_pc && encode_pc <= generated_code + actual_code_size);
    protect_mem(generated_code, actual_code_size, ALLOW_EXEC | ALLOW_READ);
}

int
main(void)
{
    /* XXX: this app should take in the rand seed as a parameter and print it
     * out on an error to allow reproducibility of the exact same instruction
     * sequence in subsequent runs.
     */
    srand(time(NULL));
    /* Produce fuzzing application code. */
    generate_code();

    /* Handle execution of illegal instructions that were decodable.
     * (fairly common).
     */
    intercept_signal(SIGILL, sigill_handler, false);

    /* Handle seg faults and floating-point exceptions caused by the fuzzed insts
     * (rarely occur).
     */
    intercept_signal(SIGSEGV, sig_segv_fpe_handler, false);
    intercept_signal(SIGFPE, sig_segv_fpe_handler, false);

    /* Execute generated code. */
    int executed = setjmp(mark);
    int sig_segv_fpe_received = sigsetjmp(sig_mark, 1);
    if (!executed && !sig_segv_fpe_received) {
        fprintf(stderr, "Execute generated code\n");
        ((void (*)(void))generated_code)();
        /* Restore the environment before the execution of the generated code. */
        longjmp(mark, 1);
    }

    /* Cleanup generated code. */
    free_mem((char *)generated_code, max_code_size);
    fprintf(stderr, "All done\n");
    return 0;
}

#else
/* XXX: only AArch64 supported. */
#    error NYI
#endif
