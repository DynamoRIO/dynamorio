/* **********************************************************
 * Copyright (c) 2021 Google, Inc.  All rights reserved.
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

static byte *generated_code;
static size_t code_size;

static jmp_buf mark;
static sigjmp_buf sig_mark;

void
sig_segv_fpe_handler(int signal)
{
    siglongjmp(sig_mark, 1);
    DR_ASSERT_MSG(false, "Shouldn't be reached");
}

void
sigill_handler(int signal, siginfo_t *siginfo, void *uctx)
{
    ucontext_t *context = (ucontext_t *)uctx;
    /* Skip illegal instructions. */
    context->uc_mcontext.pc += 4;
}

static void
print_instr_pc(instr_t *instr, byte *encode_pc)
{
#if VERBOSE > 0
    fprintf(stderr, "%p: ", encode_pc);
    instr_disassemble(GLOBAL_DCONTEXT, instr, STDERR);
    fprintf(stderr, "\n");
#endif
}

static byte *
append_instr(instr_t *instr, byte *encode_pc)
{
    print_instr_pc(instr, encode_pc);
    byte *nxt_pc = instr_encode(GLOBAL_DCONTEXT, instr, encode_pc);
    instr_destroy(GLOBAL_DCONTEXT, instr);
    assert(nxt_pc);
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
    uint32_t opcode_pick = rand() % FUZZ_INST_CNT;
    const opcode_opnd_pair_t opcode_opnd_pair = fuzz_opcode_opnd_pairs[opcode_pick];

    uint32_t opnd_mask = opcode_opnd_pair.opnd;
    /* Avoid registers 16-31 for Rd (special registers and callee-saved registers). */
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
generate_inst(byte *encode_pc)
{
    /* Pick a random side-effect-free and non-branch instruction. */
    uint32_t encoded_inst = generate_encoded_inst();
    byte encoded_inst_bytes[4];
    for (int i = 0; i < 4; ++i, encoded_inst >>= 8)
        encoded_inst_bytes[i] = (byte)(encoded_inst & 0xff);

    /* Try to decode the randomized encoding. */
    instr_t *decoded_inst = instr_create(GLOBAL_DCONTEXT);
    byte *nxt_pc = decode(GLOBAL_DCONTEXT, encoded_inst_bytes, decoded_inst);
    /* XXX: Ideally the decoder would report as erroneous any encoding leading to SIGILL.
     * Currently, several valid decodings are illegal instructions.
     */
    if (nxt_pc != NULL && check_decoded_inst(decoded_inst))
        encode_pc = append_instr(decoded_inst, encode_pc);
    else
        instr_destroy(GLOBAL_DCONTEXT, decoded_inst);

    return encode_pc;
}

static void
generate_code()
{
    /* Account for the generated insts and the final return. */
    code_size = (NUM_INSTS + 1) * 4;
    generated_code =
        (byte *)allocate_mem(code_size, ALLOW_EXEC | ALLOW_READ | ALLOW_WRITE);
    assert(generated_code != NULL);

    /* Synthesize code which includes a lot of side-effect-free instructions. Only one
     * basic block is created (linear control flow). To test clobbering of arithmetic
     * flags conditionally-executed instructions are included.
     */
    byte *encode_pc = generated_code;
    for (int i = 0; i < NUM_INSTS; ++i) {
        encode_pc = generate_inst(encode_pc);
    }

    /* The outer level is a function. */
    encode_pc = append_instr(XINST_CREATE_return(GLOBAL_DCONTEXT), encode_pc);
    assert(encode_pc && encode_pc <= generated_code + code_size);
    protect_mem(generated_code, code_size, ALLOW_EXEC | ALLOW_READ);
}

int
main(void)
{
    /* Produce fuzzing application code. */
    srand(time(NULL));
    fprintf(stderr, "Generate code\n");
    generate_code();

    /* Handle execution of illegal instructions that were decodable.
     * (fairly common).
     */
    struct sigaction act_ill;
    memset(&act_ill, 0, sizeof(act_ill));
    act_ill.sa_sigaction = (void (*)(int, siginfo_t *, void *))sigill_handler;
    act_ill.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &act_ill, NULL);

    /* Handle seg faults and floating-point exceptions caused by the fuzzed insts
     * (rarely occur).
     */
    struct sigaction act_segv_fpe;
    memset(&act_segv_fpe, 0, sizeof(act_segv_fpe));
    act_segv_fpe.sa_sigaction = (void (*)(int, siginfo_t *, void *))sig_segv_fpe_handler;
    act_segv_fpe.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act_segv_fpe, NULL);
    sigaction(SIGFPE, &act_segv_fpe, NULL);

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
    free_mem((char *)generated_code, code_size);
    fprintf(stderr, "All done\n");
    return 0;
}
