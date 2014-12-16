/* ******************************************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
 * ******************************************************************************/

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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Google, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* file "mangle.c" */

#include "../globals.h"
#include "arch.h"
#include "instr_create.h"
#include "instrument.h"

/* Make code more readable by shortening long lines.
 * We mark everything we add as non-app instr.
 */
#define POST instrlist_meta_postinsert
#define PRE  instrlist_meta_preinsert

byte *
remangle_short_rewrite(dcontext_t *dcontext,
                       instr_t *instr, byte *pc, app_pc target)
{
    /* FIXME i#1551: refactor the caller and make this routine x86-only. */
    ASSERT_NOT_REACHED();
    return NULL;
}

void
convert_to_near_rel(dcontext_t *dcontext, instr_t *instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}
/***************************************************************************/
#if !defined(STANDALONE_DECODER)

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci,
                    instrlist_t *ilist, instr_t *instr)
{
    /* clear eflags for callee's usage */
    if (cci == NULL || !cci->skip_clear_eflags) {
        if (!dynamo_options.cleancall_ignore_eflags) {
            /* FIXME i#1551: NYI on ARM */
            ASSERT_NOT_IMPLEMENTED(false);
        }
    }
}

/* Pushes not only the GPRs but also simd regs, xip, and xflags, in
 * priv_mcontext_t order.
 * The current stack pointer alignment should be passed.  Use 1 if
 * unknown (NOT 0).
 * Returns the amount of data pushed.  Does NOT fix up the xsp value pushed
 * to be the value prior to any pushes for x64 as no caller needs that
 * currently (they all build a priv_mcontext_t and have to do further xsp
 * fixups anyway).
 */
uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr,
                          uint alignment, instr_t *push_pc)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

/* User should pass the alignment from insert_push_all_registers: i.e., the
 * alignment at the end of all the popping, not the alignment prior to
 * the popping.
 */
void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *instr,
                         uint alignment)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
#ifdef X64
    /* FIXME i#1551: NYI on AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    return regular;
}

uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    uint i, j;
    instr_t *mark = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, mark);

    ASSERT(num_args == 0 || args != NULL);
    /* FIXME i#1551: we only support limited number of args for now. */
    ASSERT_NOT_IMPLEMENTED(num_args <= NUM_REGPARM);
    for (i = 0; i < num_args; i++) {
        /* FIXME i#1551: we only implement naive parameter preparation,
         * where args are all regs and do not conflict with param regs.
         */
        ASSERT_NOT_IMPLEMENTED(opnd_is_reg(args[i]) &&
                               opnd_get_size(args[i]) == OPSZ_PTR);
        DODEBUG({
            /* assume no reg used by arg conflicts with regparms */
            for (j = 0; j < i; j++)
                ASSERT_NOT_IMPLEMENTED(!opnd_uses_reg(args[j], regparms[i]));
        });
        if (regparms[i] != opnd_get_reg(args[i])) {
            POST(ilist, mark, XINST_CREATE_move(dcontext,
                                                opnd_create_reg(regparms[i]),
                                                args[i]));
        }
    }
    return 0;
}

bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
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
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_push_immed_arch(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                       instr_t **first, instr_t **second)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
mangle_syscall(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
               instr_t *instr, instr_t *next_instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

#ifdef UNIX
void
mangle_clone_code(dcontext_t *dcontext, byte *pc, bool skip)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
mangle_syscall_code(dcontext_t *dcontext, fragment_t *f, byte *pc, bool skip)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         bool skip _IF_X64(gencode_mode_t mode))
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* UNIX */

void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

void
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
float_pc_update(dcontext_t *dcontext)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_REACHED();
}

int
find_syscall_num(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
    return -1;
}
/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

#endif /* !STANDALONE_DECODER */
/***************************************************************************/
