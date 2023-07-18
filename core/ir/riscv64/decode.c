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

#include "../globals.h"
#include "instr.h"
#include "decode.h"
#include "codec.h"

bool
is_isa_mode_legal(dr_isa_mode_t mode)
{
    return (mode == DR_ISA_RV64IMAFDC);
}

app_pc
canonicalize_pc_target(dcontext_t *dcontext, app_pc pc)
{
    return pc;
}

DR_API
app_pc
dr_app_pc_as_jump_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    return pc;
}

DR_API
app_pc
dr_app_pc_as_load_target(dr_isa_mode_t isa_mode, app_pc pc)
{
    return pc;
}

byte *
decode_eflags_usage(void *drcontext, byte *pc, uint *usage, dr_opnd_query_flags_t flags)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte *
decode_opcode(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte *
decode(void *drcontext, byte *pc, instr_t *instr)
{
    return decode_common(drcontext, pc, pc, instr);
}

byte *
decode_from_copy(void *drcontext, byte *copy_pc, byte *orig_pc, instr_t *instr)
{
    return decode_common(drcontext, copy_pc, orig_pc, instr);
}

byte *
decode_cti(void *drcontext, byte *pc, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte *
decode_next_pc(void *dcontext, byte *pc)
{
    int width = instruction_width(*(int16_t *)pc);
    return pc + width;
}

int
decode_sizeof(void *drcontext, byte *pc, int *num_prefixes)
{
    return instruction_width(*(int16_t *)pc);
}

byte *
decode_raw(dcontext_t *dcontext, byte *pc, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

bool
decode_raw_is_jmp(dcontext_t *dcontext, byte *pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

byte *
decode_raw_jmp_target(dcontext_t *dcontext, byte *pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

bool
decode_raw_is_cond_branch_zero(dcontext_t *dcontext, byte *pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

byte *
decode_raw_cond_branch_zero_target(dcontext_t *dcontext, byte *pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

const instr_info_t *
instr_info_extra_opnds(const instr_info_t *info)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte
instr_info_opnd_type(const instr_info_t *info, bool src, int num)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

const instr_info_t *
get_next_instr_info(const instr_info_t *info)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

byte
decode_first_opcode_byte(int opcode)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

const instr_info_t *
opcode_to_encoding_info(uint opc, dr_isa_mode_t isa_mode)
{
    return get_instruction_info(opc);
}

DR_API
const char *
decode_opcode_name(int opcode)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

opnd_size_t
resolve_variable_size(decode_info_t *di, opnd_size_t sz, bool is_reg)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

bool
optype_is_indir_reg(int optype)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
optype_is_reg(int optype)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
optype_is_gpr(int optype)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

#ifdef DEBUG
#    ifndef STANDALONE_DECODER
void
check_encode_decode_consistency(dcontext_t *dcontext, instrlist_t *ilist)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
#    endif /* STANDALONE_DECODER */

void
decode_debug_checks_arch(void)
{
    /* FIXME i#3544: NYI */
}
#endif /* DEBUG */

#ifdef DECODE_UNIT_TEST

#    include "instr_create_shared.h"

int
main()
{
    bool res = true;
    standalone_init();
    standalone_exit();
    return res;
}

#endif /* DECODE_UNIT_TEST */
