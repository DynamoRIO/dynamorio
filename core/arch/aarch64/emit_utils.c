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

/***************************************************************************/
/*                               EXIT STUB                                 */
/***************************************************************************/

byte *
insert_relative_target(byte *pc, cache_pc target, bool hot_patch)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

byte *
insert_relative_jump(byte *pc, cache_pc target, bool hot_patch)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

uint
nop_pad_ilist(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist, bool emitting)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

size_t
get_fcache_return_tls_offs(dcontext_t *dcontext, uint flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

size_t
get_ibl_entry_tls_offs(dcontext_t *dcontext, cache_pc ibl_entry)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f,
                             linkstub_t *l, cache_pc stub_pc, ushort l_flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

bool
exit_cti_reaches_target(dcontext_t *dcontext, fragment_t *f,
                        linkstub_t *l, cache_pc target_pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, bool hot_patch)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

bool
stub_is_patched(fragment_t *f, cache_pc stub_pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
unpatch_stub(fragment_t *f, cache_pc stub_pc, bool hot_patch)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
patch_branch(dr_isa_mode_t isa_mode, cache_pc branch_pc, cache_pc target_pc,
             bool hot_patch)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

cache_pc
exit_cti_disp_pc(cache_pc branch_pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

void
link_indirect_exit_arch(dcontext_t *dcontext, fragment_t *f,
                        linkstub_t *l, bool hot_patch,
                        app_pc target_tag)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

cache_pc
cbr_fallthrough_exit_cti(cache_pc prev_cti_pc)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

void
unlink_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT SUPPORT
 */

cache_pc
entrance_stub_jmp(cache_pc stub)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

bool
coarse_is_entrance_stub(cache_pc stub)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

/*###########################################################################
 *
 * fragment_t Prefixes
*/

int
fragment_ibt_prefix_size(uint flags)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

void
insert_fragment_prefix(dcontext_t *dcontext, fragment_t *f)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

/***************************************************************************/
/*             THREAD-PRIVATE/SHARED ROUTINE GENERATION                    */
/***************************************************************************/

void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist,
                         bool absolute, bool shared)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist,
                          bool ibl_end, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}

void
insert_save_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                   uint flags, bool tls, bool absolute _IF_X86_64(bool x86_to_x64_ibl_opt))
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

void
insert_restore_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                      uint flags, bool tls, bool absolute
                      _IF_X86_64(bool x86_to_x64_ibl_opt))
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

byte *
emit_inline_ibl_stub(dcontext_t *dcontext, byte *pc,
                     ibl_code_t *ibl_code, bool target_trace_table)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return pc;
}

byte *
emit_indirect_branch_lookup(dcontext_t *dc, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc,
                            bool target_trace_table,
                            bool inline_ibl_head,
                            ibl_code_t *ibl_code /* IN/OUT */)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return NULL;
}

void
relink_special_ibl_xfer(dcontext_t *dcontext, int index,
                        ibl_entry_point_type_t entry_type,
                        ibl_branch_type_t ibl_type)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
}

bool
fill_with_nops(dr_isa_mode_t isa_mode, byte *addr, size_t size)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return false;
}
