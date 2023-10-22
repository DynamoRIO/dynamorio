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
#include "arch.h"
#include "instr_create_shared.h"
#include "instrument.h" /* instrlist_meta_preinsert */
/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define PRE instrlist_meta_preinsert

void
mangle_arch_init(void)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                    instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr, uint alignment,
                          opnd_t push_pc,
                          reg_id_t scratch /*optional*/
                              _IF_AARCH64(bool out_of_line))
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, uint alignment _IF_AARCH64(bool out_of_line))
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return regular;
}

uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool returns, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save, byte *encode_pc)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

/*###########################################################################
 *   MANGLING ROUTINES
 */

/* This is *not* a hot-patchable patch: i.e., it is subject to races. */
void
patch_mov_immed_arch(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                     instr_t *last)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* Used for fault translation */
bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr,
                    instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

#ifdef UNIX
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* UNIX */

void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    ASSERT(instr_get_opcode(instr) == OP_jal);
    ASSERT(opnd_is_pc(instr_get_target(instr)));
    insert_mov_immed_ptrsz(dcontext, get_call_return_address(dcontext, ilist, instr),
                           instr_get_dst(instr, 0), ilist, instr, NULL, NULL);
    instrlist_remove(ilist, instr); /* remove OP_jal */
    instr_destroy(dcontext, instr);
    return next_instr;
}

instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* The mangling is identical. */
    return mangle_indirect_jump(dcontext, ilist, instr, next_instr, flags);
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    /* The mangling is identical. */
    mangle_indirect_jump(dcontext, ilist, instr, next_instr, flags);
}

instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    ASSERT(instr_is_mbr(instr));
    opnd_t dst = instr_get_dst(instr, 0), target = instr_get_target(instr);
    PRE(ilist, instr,
        instr_create_save_to_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    ASSERT(opnd_is_reg(target));

    ASSERT_NOT_IMPLEMENTED(!opnd_same(target, opnd_create_reg(DR_REG_TP)));
    ASSERT_NOT_IMPLEMENTED(!opnd_same(dst, opnd_create_reg(DR_REG_TP)));
    ASSERT_NOT_IMPLEMENTED(!opnd_same(dst, opnd_create_reg(dr_reg_stolen)));

    if (opnd_same(target, opnd_create_reg(dr_reg_stolen))) {
        /* If the target reg is dr_reg_stolen, the app value is in TLS. */
        PRE(ilist, instr,
            instr_create_restore_from_tls(dcontext, IBL_TARGET_REG, TLS_REG_STOLEN_SLOT));
        if (opnd_get_immed_int(instr_get_src(instr, 1)) != 0)
            PRE(ilist, instr,
                XINST_CREATE_add(dcontext, opnd_create_reg(IBL_TARGET_REG),
                                 instr_get_src(instr, 1)));
    } else
        PRE(ilist, instr,
            XINST_CREATE_add_2src(dcontext, opnd_create_reg(IBL_TARGET_REG), target,
                                  instr_get_src(instr, 1)));

    if (opnd_get_reg(dst) != DR_REG_ZERO)
        insert_mov_immed_ptrsz(dcontext, get_call_return_address(dcontext, ilist, instr),
                               dst, ilist, next_instr, NULL, NULL);

    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
}

instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

instr_t *
mangle_reads_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             instr_t *next_instr)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
float_pc_update(dcontext_t *dcontext)
{
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* END OF MANGLING ROUTINES
 *###########################################################################
 */
