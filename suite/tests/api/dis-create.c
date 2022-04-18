/* **********************************************************
 * Copyright (c) 2015-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/* Helper app to create a small opcode-representative binary for
 * dis.c.  The idea is to run this on a very large random binary and
 * it will create from the large binary a small binary suitable for
 * checking in to the repository.
 * I ran it like this:
 *
 * LD_LIBRARY_PATH=lib32/release suite/tests/bin/api.dis-create /tmp/randombits
 * /tmp/OUT-dis -arm > opcs
 */

#include "configure.h"
#include "dr_api.h"
#include <assert.h>
#include <string.h>

#ifndef ARM
#    error NYI
#endif

/* Strategy: include NUM_INITIAL random bytes to get some invalid ones,
 * and then from the rest of the file cap each opcode at NUM_EACH.
 */
static uint count[OP_LAST + 1];
#define NUM_COUNT sizeof(count) / sizeof(count[0])

static uint num_tot;
#define NUM_INITIAL 2048
#define NUM_EACH 12

static void
read_data(void *drcontext, file_t outf, byte *start, size_t size)
{
    instr_t instr;
    byte *pc = start, *prev_pc;

    instr_init(drcontext, &instr);
    while (pc < start + size) {
        /* FIXME: want to cut it off instead of reading beyond for
         * end of file!  If weren't printing it out as go along could
         * mark invalid after seeing whether instr overflows.
         */
        prev_pc = pc;
        instr_reset(drcontext, &instr);
        pc = decode(drcontext, pc, &instr);
        num_tot++;
        if (pc == NULL) {
            /* we still know size */
            pc = decode_next_pc(drcontext, prev_pc);
            /* put invalid entries in if they are in first set of random entries */
            if (num_tot < NUM_INITIAL)
                dr_write_file(outf, prev_pc, pc - prev_pc);
        } else {
            count[instr_get_opcode(&instr)]++;
            if (count[instr_get_opcode(&instr)] < NUM_EACH || num_tot < NUM_INITIAL)
                dr_write_file(outf, prev_pc, pc - prev_pc);
        }
    }
    int i;
    for (i = OP_FIRST; i < NUM_COUNT; i++) {
        printf("  %d %s: %d\n", i, decode_opcode_name(i), count[i]);
    }

    if (dr_get_isa_mode(drcontext) == DR_ISA_ARM_A32) {
        /* Even a large random binary is often missing these so we just throw them
         * in at the end:
         */
        uint rare_opc[] = {
            0xe320f001, // yield
            0xf57ff06f, // isb    $0x0f
            0xf57ff01f, // clrex
            0xe320f004, // sev
        };
        dr_write_file(outf, rare_opc, sizeof(rare_opc));
    } else {
        uint rare_opc[] = {
            0x8f2ff3bf, // clrex
            0x8f4ff3bf, // dsb    $0x0f
            0x4fdfe8d7, // ldaex r4, [r7]
            0x8f1ff3bf, // enterx
            0x8f0ff3bf, // leavex
        };
        dr_write_file(outf, rare_opc, sizeof(rare_opc));
    }
}

int
main(int argc, char *argv[])
{
    file_t f, outf;
    bool ok;
    uint64 file_size;
    size_t map_size;
    byte *map_base;
    void *drcontext = dr_standalone_init();

    if (argc != 4) {
        dr_fprintf(STDERR, "Usage: %s <objfile> <outfile> <-arm|-thumb>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[3], "-arm") == 0)
        dr_set_isa_mode(drcontext, DR_ISA_ARM_A32, NULL);
    else
        dr_set_isa_mode(drcontext, DR_ISA_ARM_THUMB, NULL);

    f = dr_open_file(argv[1], DR_FILE_READ | DR_FILE_ALLOW_LARGE);
    if (f == INVALID_FILE) {
        dr_fprintf(STDERR, "Error opening input file %s\n", argv[1]);
        return 1;
    }
    ok = dr_file_size(f, &file_size);
    if (!ok) {
        dr_fprintf(STDERR, "Error getting file size for %s\n", argv[1]);
        dr_close_file(f);
        return 1;
    }
    map_size = (size_t)file_size;
    map_base = dr_map_file(f, &map_size, 0, NULL, DR_MEMPROT_READ, DR_MAP_PRIVATE);
    if (map_base == NULL || map_size < file_size) {
        dr_fprintf(STDERR, "Error mapping %s\n", argv[1]);
        dr_close_file(f);
        return 1;
    }

    outf = dr_open_file(argv[2], DR_FILE_WRITE_OVERWRITE);
    if (outf == INVALID_FILE) {
        dr_fprintf(STDERR, "Error opening output file %s\n", argv[2]);
        return 1;
    }

    read_data(drcontext, outf, map_base, (size_t)file_size);

    dr_unmap_file(map_base, map_size);
    dr_close_file(f);
    dr_close_file(outf);
    dr_standalone_exit();
    return 0;
}
