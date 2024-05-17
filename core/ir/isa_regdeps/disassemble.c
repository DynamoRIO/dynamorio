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
#include "encoding_common.h"

/* DR_ISA_REGDEPS instruction encodings can be at most 16 bytes, hence we can have at
 * most 2 lines of 8 bytes each.
 */
#define REGDEPS_BYTES_PER_LINE 8

/* We separate the 8 bytes per line in two 4 byte words.
 */
#define REGDEPS_BYTES_PER_WORD 4

/* Prints the first line of regdeps encoding bytes in one or two 4 byte words, depending
 * on the encoding length.
 * Returns the number of bytes that need to be printed on the second line.
 */
int
d_r_regdeps_print_encoding_first_line(char *buf, size_t bufsz,
                                      size_t *sofar DR_PARAM_INOUT, byte *pc,
                                      byte *next_pc)
{
    int sz = (int)(next_pc - pc);
    /* Sanity check. This should never happen.
     */
    if (sz <= 0)
        return 0;

    /* Compute the number of bytes present in the second line.
     */
    int extra_sz = 0;
    if (sz > REGDEPS_BYTES_PER_LINE)
        extra_sz = sz - REGDEPS_BYTES_PER_LINE;

    /* DR_ISA_REGDEPS instruction encodings are 4 byte aligned, so casting (byte *)
     * to (uint *) is safe. We always have a 4 byte header, so we print the first 4 byte
     * word.
     */
    print_to_buffer(buf, bufsz, sofar, " %08x", *((uint *)pc));

    /* Print second 4 byte word, if any.
     */
    if (sz > REGDEPS_BYTES_PER_WORD) {
        print_to_buffer(buf, bufsz, sofar, " %08x",
                        *((uint *)(pc + REGDEPS_BYTES_PER_WORD)));
    }

    /* Add a space at the end.
     */
    print_to_buffer(buf, bufsz, sofar, " ");

    return extra_sz;
}

/* Prints the second line of regdeps encoding bytes in one or two 4 byte words, depending
 * on the encoding length.
 */
void
d_r_regdeps_print_encoding_second_line(char *buf, size_t bufsz,
                                       size_t *sofar DR_PARAM_INOUT, byte *pc,
                                       byte *next_pc, int extra_sz,
                                       const char *extra_bytes_prefix)
{
    /* Sanity check. This should never happen.
     */
    if (extra_sz <= 0)
        return;

    /* If we are here we have a third 4 byte word to print.
     */
    print_to_buffer(buf, bufsz, sofar, " %08x", *((uint *)(pc + REGDEPS_BYTES_PER_LINE)));

    /* Print fourth 4 byte word, if any.
     */
    if (extra_sz > REGDEPS_BYTES_PER_WORD) {
        print_to_buffer(
            buf, bufsz, sofar, " %08x",
            *((uint *)(pc + REGDEPS_BYTES_PER_LINE + REGDEPS_BYTES_PER_WORD)));
    }

    /* Add a new line at the end.
     */
    print_to_buffer(buf, bufsz, sofar, "\n");
}
