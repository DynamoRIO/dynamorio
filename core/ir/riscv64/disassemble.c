/* **********************************************************
 * Copyright (c) 2022 Rivos, Inc.  All rights reserved.
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
 * * Neither the name of Rivos, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL RIVOS, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <stdint.h>

#include "../globals.h"
#include "globals_shared.h"
#include "disassemble_api.h"
#include "codec.h"

int
print_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                      byte *next_pc, instr_t *instr)
{
    if (instruction_width(*(int16_t *)pc) == 2)
        print_to_buffer(buf, bufsz, sofar, "     %04x   ", *(uint16_t *)pc);
    else
        print_to_buffer(buf, bufsz, sofar, " %08x   ", *(uint *)pc);
    return 0;
}

void
print_extra_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT, byte *pc,
                            byte *next_pc, int extra_sz, const char *extra_bytes_prefix)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
opnd_base_disp_scale_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                                 opnd_t opnd)
{
    /* There is no scaled addressing on RISC-V */
    ASSERT_NOT_REACHED();
}

bool
opnd_disassemble_arch(char *buf, size_t bufsz, size_t *sofar INOUT, opnd_t opnd)
{
    switch (opnd.kind) {
    case IMMED_INTEGER_kind: {
        /* Immediates are sign-extended at the decode time. */
        ptr_int_t val = opnd_get_immed_int(opnd);
        const char *fmt =
            TEST(opnd_get_flags(opnd), DR_OPND_IMM_PRINT_DECIMAL) ? "%d" : "0x%x";
        print_to_buffer(buf, bufsz, sofar, fmt, val);
        return true;
    }
    }
    return false;
}

bool
opnd_disassemble_noimplicit(char *buf, size_t bufsz, size_t *sofar INOUT,
                            dcontext_t *dcontext, instr_t *instr, byte optype,
                            opnd_t opnd, bool prev, bool multiple_encodings, bool dst,
                            int *idx INOUT)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr, char *buf, size_t bufsz,
                     size_t *sofar INOUT)
{
}

void
print_opcode_name(instr_t *instr, const char *name, char *buf, size_t bufsz,
                  size_t *sofar INOUT)
{
    print_to_buffer(buf, bufsz, sofar, "%s", name);
}
