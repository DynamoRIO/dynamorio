/* **********************************************************
 * Copyright (c) 2016 ARM Limited. All rights reserved.
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

#include "../globals.h"
#include "arch.h"
#include "instr.h"
#include "decode.h"
#include "disassemble.h"
#include <string.h>

#if defined(INTERNAL) || defined(DEBUG) || defined(CLIENT_INTERFACE)

int
print_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT,
                      byte *pc, byte *next_pc, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

void
print_extra_bytes_to_buffer(char *buf, size_t bufsz, size_t *sofar INOUT,
                            byte *pc, byte *next_pc, int extra_sz,
                            const char *extra_bytes_prefix)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
opnd_base_disp_scale_disassemble(char *buf, size_t bufsz, size_t *sofar INOUT,
                                 opnd_t opnd)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

bool
opnd_disassemble_arch(char *buf, size_t bufsz, size_t *sofar INOUT, opnd_t opnd)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

bool
opnd_disassemble_noimplicit(char *buf, size_t bufsz, size_t *sofar INOUT,
                            dcontext_t *dcontext, instr_t *instr,
                            byte optype, opnd_t opnd, bool prev, bool multiple_encodings,
                            bool dst, int *idx INOUT)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
print_instr_prefixes(dcontext_t *dcontext, instr_t *instr,
                     char *buf, size_t bufsz, size_t *sofar INOUT)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return;
}

void
print_opcode_name(instr_t *instr, const char *name,
                  char *buf, size_t bufsz, size_t *sofar INOUT)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

#endif /* INTERNAL || DEBUG || CLIENT_INTERFACE */
