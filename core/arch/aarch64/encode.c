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
#include "decode_private.h"

/* Extra logging for encoding */
#define ENC_LEVEL 6

/* Order corresponds to DR_REG_ enum. */
const char * const reg_names[] = {
    "<NULL>",
    "<invalid>",
};

/* Maps sub-registers to their containing register. */
/* Order corresponds to DR_REG_ enum. */
const reg_id_t dr_reg_fixer[] = {
    REG_NULL,
    REG_NULL,
};

#ifdef DEBUG
void
encode_debug_checks(void)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}
#endif

bool
encoding_possible(decode_info_t *di, instr_t *in, const instr_info_t * ii)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
decode_info_init_for_instr(decode_info_t *di, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

byte *
instr_encode_arch(dcontext_t *dcontext, instr_t *instr, byte *copy_pc, byte *final_pc,
                  bool check_reachable, bool *has_instr_opnds/*OUT OPTIONAL*/
                  _IF_DEBUG(bool assert_reachable))
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr,
                                 byte *dst_pc, byte *final_pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}
