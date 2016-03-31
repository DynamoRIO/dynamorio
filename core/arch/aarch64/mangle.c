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
#include "instr_create.h"
#include "instrument.h" /* instrlist_meta_preinsert */
#include "disassemble.h"

byte *
remangle_short_rewrite(dcontext_t *dcontext,
                       instr_t *instr, byte *pc, app_pc target)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

instr_t *
convert_to_near_rel_arch(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

/***************************************************************************/
#if !defined(STANDALONE_DECODER)

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci,
                    instrlist_t *ilist, instr_t *instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr,
                          uint alignment, opnd_t push_pc, reg_id_t scratch/*optional*/)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *instr,
                         uint alignment)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool returns, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

/*###########################################################################
 *###########################################################################
 *
 *   M A N G L I N G   R O U T I N E S
 */

void
insert_mov_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                      ptr_int_t val, opnd_t dst,
                      instrlist_t *ilist, instr_t *instr,
                      instr_t **first, instr_t **second)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
insert_push_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                       instr_t **first, instr_t **second)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

/* Used for fault translation */
bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
                    instr_t *instr, instr_t *next_instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

#ifdef UNIX
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr
                         _IF_X86_64(gencode_mode_t mode))
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}
#endif /* UNIX */

void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

instr_t *
mangle_reads_thread_register(dcontext_t *dcontext, instrlist_t *ilist,
                             instr_t *instr, instr_t *next_instr)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

void
float_pc_update(dcontext_t *dcontext)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

#endif /* !STANDALONE_DECODER */
/***************************************************************************/
