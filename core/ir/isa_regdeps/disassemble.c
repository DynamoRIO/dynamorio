/* **********************************************************
 * Copyright (c) 2024 Google, Inc.  All rights reserved.
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

#include "../globals.h"
#include "disassemble.h"

/* DR_ISA_REGDEPS instruction encodings can be at most 16 bytes and are 4 byte aligned,
 * hence we can have at most 4 lines of 4 bytes each.
 */
#define REGDEPS_BYTES_PER_LINE 4

int
print_regdeps_encoding_bytes_to_buffer(char *buf, size_t bufsz,
                                       size_t *sofar DR_PARAM_INOUT, byte *pc,
                                       byte *next_pc)
{
    int extra_sz = 0;
    int sz = (int)(next_pc - pc);
    if (sz > REGDEPS_BYTES_PER_LINE)
        extra_sz = sz - REGDEPS_BYTES_PER_LINE;
    /* DR_ISA_REGDEPS instruction encodings are 4 byte aligned, so casting (byte *)
     * to (uint *) is safe.
     */
    print_to_buffer(buf, bufsz, sofar, " %08x ", *((uint *)pc));
    return extra_sz;
}

void
print_extra_regdeps_encoding_bytes_to_buffer(char *buf, size_t bufsz,
                                             size_t *sofar DR_PARAM_INOUT, byte *pc,
                                             byte *next_pc, int extra_sz,
                                             const char *extra_bytes_prefix)
{
    /* + 1 accounts for line 0 printed by print_regdeps_encoding_bytes_to_buffer().
     */
    int num_lines =
        (ALIGN_FORWARD(extra_sz, REGDEPS_BYTES_PER_LINE) / REGDEPS_BYTES_PER_LINE) + 1;
    /* We start from line 1 because line 0 has already been printed by
     * print_regdeps_encoding_bytes_to_buffer().
     */
    for (int line = 1; line < num_lines; ++line) {
        /* DR_ISA_REGDEPS instruction encodings are 4 byte aligned, so casting (byte *)
         * to (uint *) is safe.
         */
        print_to_buffer(buf, bufsz, sofar, "%s %08x\n", extra_bytes_prefix,
                        *((uint *)(pc + line * REGDEPS_BYTES_PER_LINE)));
    }
}
