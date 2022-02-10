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

/* Code Manipulation API Sample:
 * dis.c
 *
 * Disassembles a binary file containing nothing but code.
 */

#include "configure.h"
#include "dr_api.h"
#include <assert.h>
#include <string.h>

#define VERBOSE 1

/* arbitrary pc for pc-relative operands for consistent output */
#define ORIG_PC ((app_pc)0x10000000)

static void
read_data(void *drcontext, byte *start, size_t size)
{
    byte *pc = start, *prev_pc;
    while (pc < start + size) {
        /* FIXME: want to cut it off instead of reading beyond for
         * end of file!  If weren't printing it out as go along could
         * mark invalid after seeing whether instr overflows.
         */
        prev_pc = pc;
#if VERBOSE
        dr_printf("+0x%04x  ", prev_pc - start);
#endif
        pc = disassemble_from_copy(drcontext, pc, ORIG_PC + (pc - start), STDOUT,
                                   false /*don't show pc*/,
#if VERBOSE
                                   true /*show bytes*/
#else
                                   false /*do not show bytes*/
#endif
        );
#ifdef ARM
        if (pc == NULL) /* we still know size */
            pc = decode_next_pc(drcontext, prev_pc);
#else
        /* If invalid, try next byte */
        /* FIXME: udis86 is going to byte after the one that makes it
         * invalid: so if 1st byte is invalid opcode, go to 2nd;
         * if modrm makes it invalid (0xc5 0xc5), go to 3rd.
         * not clear that's nec. better but we need to reconcile that w/
         * their diff for automated testing.
         */
        if (pc == NULL)
            pc = prev_pc + 1;
#endif
    }
}

int
main(int argc, char *argv[])
{
    file_t f;
    bool ok;
    uint64 file_size;
    size_t map_size;
    byte *map_base;

    /* Test i#2499: heap alloc prior to standalone init. */
    void *temp = dr_global_alloc(sizeof(void *));
    dr_global_free(temp, sizeof(void *));

    void *drcontext = dr_standalone_init();

#ifdef ARM
    if (argc != 3) {
        dr_fprintf(STDERR, "Usage: %s <objfile> <-arm|-thumb>\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[2], "-arm") == 0)
        dr_set_isa_mode(drcontext, DR_ISA_ARM_A32, NULL);
    else
        dr_set_isa_mode(drcontext, DR_ISA_ARM_THUMB, NULL);
#else
    if (argc != 2) {
        dr_fprintf(STDERR, "Usage: %s <objfile>\n", argv[0]);
        return 1;
    }
#endif

    f = dr_open_file(argv[1], DR_FILE_READ | DR_FILE_ALLOW_LARGE);
    if (f == INVALID_FILE) {
        dr_fprintf(STDERR, "Error opening %s\n", argv[1]);
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

    /* XXX: re-run 64-bit asking for 32-bit mode */

    read_data(drcontext, map_base, (size_t)file_size);

    dr_unmap_file(map_base, map_size);
    dr_close_file(f);
    dr_standalone_exit();
    return 0;
}
