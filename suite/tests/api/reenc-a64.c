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

/* Test the AArch64 encoder and decoder by decoding and reencoding all words
 * in a given range. The user is expected to run multiple instances of this
 * program on a multicore system or cluster using whatever tools are locally
 * available. For example, on a single machine with two cores one could run:
 *    api.reenc-a64 0x00000000 0x7fffffff > log0 &
 *    api.reenc-a64 0x80000000 0xffffffff > log1 &
 */

#include "configure.h"
#include "dr_api.h"

#include <stdlib.h>

#define ORIG_PC ((byte *)0x10000000)

void
test1(void *dc, uint enc, int len)
{
    instr_t instr;
    byte *pc1, *pc2;
    uint enc2 = 0;

    instr_init(dc, &instr);
    pc1 = decode_from_copy(dc, (byte *)&enc, ORIG_PC, &instr);
    if (pc1 == NULL && instr_get_opcode(&instr) == OP_INVALID)
        goto done;
    if (pc1 != (byte *)&enc + len || instr_get_opcode(&instr) < OP_FIRST ||
        instr_get_opcode(&instr) > OP_LAST) {
        dr_printf("%08x  Decode failed\n", enc);
        goto done;
    }
    pc2 = instr_encode_to_copy(dc, &instr, (byte *)&enc2, ORIG_PC);
    if (pc2 != (byte *)&enc2 + len) {
        dr_printf("%08x  Encode failed: ", enc);
        disassemble_from_copy(dc, (byte *)&enc, ORIG_PC, STDOUT, false, false);
        goto done;
    }
    if (enc2 != enc) {
        /* Digits are to protect line order if output is sorted. */
        dr_printf("%08x  1: Encode gave different bits:\n", enc);
        dr_printf("%08x  2:    %08x  ", enc, enc);
        disassemble_from_copy(dc, (byte *)&enc, ORIG_PC, STDOUT, false, false);
        dr_printf("%08x  3: -> %08x  ", enc2);
        disassemble_from_copy(dc, (byte *)&enc2, ORIG_PC, STDOUT, false, false);
    }

done:
    instr_free(dc, &instr);
}

int
main(int argc, char *argv[])
{
    uint a, b, i;
    void *dc;

    if (argc != 3) {
        dr_printf("Usage: %s FIRST LAST\n", argv[0]);
        return 1;
    }

    a = (uint)strtoll(argv[1], NULL, 0);
    b = (uint)strtoll(argv[2], NULL, 0);
    dc = dr_standalone_init();

    i = a;
    do {
        test1(dc, i, sizeof(uint));
    } while (i++ != b);

    dr_standalone_exit();
    return 0;
}
