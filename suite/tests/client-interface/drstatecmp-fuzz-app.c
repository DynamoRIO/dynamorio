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

/* Fuzzing application to stress-test DR with the drstatecmp library. Only AArch64
 * is currently supported.
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

#define NUM_INSTS 2
#define TRIES 10000

static byte *generated_code;
static size_t code_size;

static jmp_buf mark;
static sigjmp_buf sig_mark;

static byte *
append_ilist(instrlist_t *ilist, byte *encode_pc, instr_t *instr)
{
    instrlist_append(ilist, instr);
    return encode_pc + instr_length(GLOBAL_DCONTEXT, instr);
}

void
sigill_handler(int signal)
{
    siglongjmp(sig_mark, 1);
    DR_ASSERT_MSG(false, "Shouldn't be reached");
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
    instrlist_t *ilist = instrlist_create(GLOBAL_DCONTEXT);
    byte *encode_pc = generated_code;
    uint32_t encoded_inst = 0b00011110001001000001010001001111;
    byte encoded_inst_bytes[4];
    uint32_t low_bits = 0b11111111;
    for (int i = 0; i < 4; ++i) {
        encoded_inst_bytes[i] = (byte)(encoded_inst & low_bits);
        encoded_inst >>= 8;
    }
    instr_t *decoded_inst = instr_create(GLOBAL_DCONTEXT);
    byte *nxt_pc = decode(GLOBAL_DCONTEXT, encoded_inst_bytes, decoded_inst);
    if (nxt_pc != NULL && instr_valid(decoded_inst) &&
        instr_get_opcode(decoded_inst) != OP_xx) {
        encode_pc = append_ilist(ilist, encode_pc, decoded_inst);
    } else {
        instr_destroy(GLOBAL_DCONTEXT, decoded_inst);
    }

    /* The outer level is a function. */
    encode_pc = append_ilist(ilist, encode_pc, XINST_CREATE_return(GLOBAL_DCONTEXT));
    byte *end_pc = instrlist_encode(GLOBAL_DCONTEXT, ilist, generated_code, true);
    assert(end_pc <= generated_code + code_size);
    protect_mem(generated_code, code_size, ALLOW_EXEC | ALLOW_READ);
    instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
}

int
main(void)
{
    /* Produce fuzzing application code. */
    srand(time(NULL));
    fprintf(stderr, "Generate code\n");
    generate_code();
    /* Handle execution of illegal instructions that were decodable. */
    struct sigaction act, oact;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))sigill_handler;
    act.sa_flags = SA_SIGINFO || SA_ONSTACK || SA_RESTART;
    sigaction(SIGILL, &act, &oact);
    /* Execute generated code. */
    int executed = setjmp(mark);
    if (!executed) {
        fprintf(stderr, "Execute generated code\n");
        ((void (*)(void))generated_code)();
        /* Skip execution of generated code once a illegal instruction is encountered. */
        sigsetjmp(sig_mark, 1);
        /* Restore the enviroment before the execution of the generated code. */
        longjmp(mark, 1);
    }
    /* Cleanup generated code. */
    free_mem((char *)generated_code, code_size);
    fprintf(stderr, "All done\n");
    return 0;
}
