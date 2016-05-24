/* **********************************************************
 * Copyright (c) 2014-2015 Google, Inc.  All rights reserved.
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
#include "instr_create.h"
#include "instrlist.h"
#include "instrument.h"

/* shorten code generation lines */
#define APP    instrlist_meta_append
#define PRE    instrlist_meta_preinsert
#define OPREG  opnd_create_reg

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
    /* AArch64 always uses shared gencode so we ignore FRAG_DB_SHARED(flags) */
    if (TEST(FRAG_COARSE_GRAIN, flags)) {
        /* FIXME i#1575: coarse-grain NYI on AArch64 */
        ASSERT_NOT_IMPLEMENTED(false);
        return 0;
    }
    return TLS_FCACHE_RETURN_SLOT;
}

size_t
get_ibl_entry_tls_offs(dcontext_t *dcontext, cache_pc ibl_entry)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
    return 0;
}

/* Generate move (immediate) of a 64-bit value using 4 instructions. */
static uint *
insert_mov_imm(uint *pc, reg_id_t dst, ptr_int_t val)
{
    uint rt = dst - DR_REG_X0;
    ASSERT(rt < 31);
    *pc++ = 0xd2800000 | rt | (val       & 0xffff) << 5; /* mov  x(rt), #x */
    *pc++ = 0xf2a00000 | rt | (val >> 16 & 0xffff) << 5; /* movk x(rt), #x, lsl #16 */
    *pc++ = 0xf2c00000 | rt | (val >> 32 & 0xffff) << 5; /* movk x(rt), #x, lsl #32 */
    *pc++ = 0xf2e00000 | rt | (val >> 48 & 0xffff) << 5; /* movk x(rt), #x, lsl #48 */
    return pc;
}

/* Emit code for the exit stub at stub_pc.  Return the size of the
 * emitted code in bytes.  This routine assumes that the caller will
 * take care of any cache synchronization necessary.
 * The stub is unlinked initially, except coarse grain indirect exits,
 * which are always linked.
 */
int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f,
                             linkstub_t *l, cache_pc stub_pc, ushort l_flags)
{
    uint *pc = (uint *)stub_pc;
    /* FIXME i#1575: coarse-grain NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(!TEST(FRAG_COARSE_GRAIN, f->flags));
    if (LINKSTUB_DIRECT(l_flags)) {
        /* stp x0, x1, [x(stolen), #(offs)] */
        *pc++ = (0xa9000000 | 0 | 1 << 10 | (dr_reg_stolen - DR_REG_X0) << 5 |
                 TLS_REG0_SLOT >> 3 << 15);
        /* mov x0, ... */
        pc = insert_mov_imm(pc, DR_REG_X0, (ptr_int_t)l);
        /* ldr x1, [x(stolen), #(offs)] */
        *pc++ = (0xf9400000 | 1 | (dr_reg_stolen - DR_REG_X0) << 5 |
                 get_fcache_return_tls_offs(dcontext, f->flags) >>
                 3 << 10);
        /* br x1 */
        *pc++ = 0xd61f0000 | 1 << 5;
    } else {
        spill_state_t state;
        size_t ibl_offs = (byte *)&state.bb_ibl[IBL_INDJMP].ibl - (byte *)&state;
        /* stp x0, x1, [x(stolen), #(offs)] */
        *pc++ = (0xa9000000 | 0 | 1 << 10 | (dr_reg_stolen - DR_REG_X0) << 5 |
                 TLS_REG0_SLOT >> 3 << 15);
        /* mov x0, ... */
        pc = insert_mov_imm(pc, DR_REG_X0, (ptr_int_t)l);
        /* ldr x1, [x(stolen), #(offs)] */
        *pc++ = (0xf9400000 | 1 | (dr_reg_stolen - DR_REG_X0) << 5 |
                 ibl_offs >> 3 << 10);
        /* br x1 */
        *pc++ = 0xd61f0000 | 1 << 5;
    }
    ASSERT((ptr_int_t)((byte *)pc - (byte *)stub_pc) ==
           DIRECT_EXIT_STUB_SIZE(l_flags));
    return (int)((byte *)pc - (byte *)stub_pc);
}

bool
exit_cti_reaches_target(dcontext_t *dcontext, fragment_t *f,
                        linkstub_t *l, cache_pc target_pc)
{
    cache_pc branch_pc = EXIT_CTI_PC(f, l);
    /* Compute offset as unsigned, modulo arithmetic. */
    ptr_uint_t off = (ptr_uint_t)target_pc - (ptr_uint_t)branch_pc;
    uint enc = *(uint *)branch_pc;
    ASSERT(ALIGNED(branch_pc, 4) && ALIGNED(target_pc, 4));
    if ((enc & 0xfc000000) == 0x14000000) /* B (OP_b)*/
        return (off + 0x8000000 < 0x10000000);
    else if ((enc & 0xff000010) == 0x54000000 ||
             (enc & 0x7e000000) == 0x34000000) /* B.cond, CBNZ, CBZ */
        return (off + 0x40000 < 0x80000);
    else if ((enc & 0x7e000000) == 0x36000000) /* TBNZ, TBZ */
        return (off + 0x2000 < 0x4000);
    ASSERT(false);
    return false;
}

void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, bool hot_patch)
{
    /* Compute offset as unsigned, modulo arithmetic. */
    ptr_uint_t off = (ptr_uint_t)target_pc - (ptr_uint_t)stub_pc;
    if (off + 0x8000000 < 0x10000000) {
        /* We can get there with a B (OP_b, 26-bit signed immediate offset). */
        *(uint *)stub_pc = (0x14000000 | (0x03ffffff & off >> 2));
        if (hot_patch)
            machine_cache_sync(stub_pc, stub_pc + 4, true);
        return;
    }
    /* We must use an indirect branch. */
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
    /* Compute offset as unsigned, modulo arithmetic. */
    ptr_uint_t off = (ptr_uint_t)target_pc - (ptr_uint_t)branch_pc;
    uint *p = (uint *)branch_pc;
    uint enc = *p;
    ASSERT(ALIGNED(branch_pc, 4) && ALIGNED(target_pc, 4));
    if ((enc & 0xfc000000) == 0x14000000) { /* B */
        ASSERT(off + 0x8000000 < 0x10000000);
        *p = (0x14000000 | (0x03ffffff & off >> 2));
    }
    else if ((enc & 0xff000010) == 0x54000000 ||
             (enc & 0x7e000000) == 0x34000000) { /* B.cond, CBNZ, CBZ */
        ASSERT(off + 0x40000 < 0x80000);
        *p = (enc & 0xff00001f) | (0x00ffffe0 & off >> 2 << 5);
    }
    else if ((enc & 0x7e000000) == 0x36000000) { /* TBNZ, TBZ */
        ASSERT(off + 0x2000 < 0x4000);
        *p = (enc & 0xfff8001f) | (0x0007ffe0 & off >> 2 << 5);
    }
    else
        ASSERT(false);
    if (hot_patch)
        machine_cache_sync(branch_pc, branch_pc + 4, true);
    return;
}

uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    return 0; /* always aligned */
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
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);
    byte *pc = stub_pc + exit_stub_size(dcontext, target_tag, f->flags) - 4;
    *(uint *)pc = 0xd61f0000 | 1 << 5; /* br x1 */
    /* XXX: since we need to sync, we should start out linked */
    if (hot_patch)
        machine_cache_sync(pc, pc + 4, true);
}

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    cache_pc cti = EXIT_CTI_PC(f, l);
    if (!EXIT_HAS_STUB(l->flags, f->flags))
        return NULL;
    ASSERT(decode_raw_is_jmp(dcontext, cti));
    return decode_raw_jmp_target(dcontext, cti);
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
    /* Nothing extra for ibt as we don't have flags to restore */
    return FRAGMENT_BASE_PREFIX_SIZE(flags);
}

void
insert_fragment_prefix(dcontext_t *dcontext, fragment_t *f)
{
    /* Always use prefix on AArch64 as there is no load to PC. */
    byte *pc = (byte *)f->start_pc;
    ASSERT(f->prefix_size == 0);
    /* ldr x0, [x(stolen), #(off)] */
    *(uint *)pc = (0xf9400000 | (ENTRY_PC_REG - DR_REG_X0) |
                   (dr_reg_stolen - DR_REG_X0) << 5 |
                   ENTRY_PC_SPILL_SLOT >> 3 << 10);
    pc += 4;
    f->prefix_size = (byte)(((cache_pc)pc) - f->start_pc);
    ASSERT(f->prefix_size == fragment_prefix_size(f->flags));
}

/***************************************************************************/
/*             THREAD-PRIVATE/SHARED ROUTINE GENERATION                    */
/***************************************************************************/

/* Load DR's TLS base to dr_reg_stolen via reg_base. */
static void
insert_load_dr_tls_base(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t reg_base)
{
    /* Load TLS base from user-mode thread pointer/ID register:
     * mrs reg_base, tpidr_el0
     */
    PRE(ilist, where, INSTR_CREATE_xx(dcontext,
                                      0xd53bd040 | (reg_base - DR_REG_X0)));
    /* ldr dr_reg_stolen, [reg_base, DR_TLS_BASE_OFFSET] */
    PRE(ilist, where, INSTR_CREATE_xx(dcontext, 0xf9400000 |
                                      (dr_reg_stolen - DR_REG_X0) |
                                      (reg_base - DR_REG_X0) << 5 |
                                      DR_TLS_BASE_OFFSET >> 3 << 10));
}

/* Having only one thread register (TPIDR_EL0) shared between app and DR,
 * we steal a register for DR's TLS base in the code cache, and store
 * DR's TLS base into an private lib's TLS slot for accessing in C code.
 * On entering the code cache (fcache_enter):
 * - grab gen routine's parameter dcontext and put it into REG_DCXT
 * - load DR's TLS base into dr_reg_stolen from privlib's TLS
 */
void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    ASSERT_NOT_IMPLEMENTED(!absolute &&
                           !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
    /* Grab gen routine's parameter dcontext and put it into REG_DCXT:
     * mov x(dxct), x0
     */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xaa0003e0 | (REG_DCXT - DR_REG_X0)));
    /* set up stolen reg */
    insert_load_dr_tls_base(dcontext, ilist, NULL/*append*/, SCRATCH_REG0);
}

void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist,
                         bool absolute, bool shared)
{
    /* i#1569: DR_HOOK is not supported on AArch64 */
    ASSERT_NOT_IMPLEMENTED(EXIT_DR_HOOK == NULL);
}

void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, RESTORE_FROM_DC(dcontext, DR_REG_W0, XFLAGS_OFFSET));
    APP(ilist, RESTORE_FROM_DC(dcontext, DR_REG_W1, XFLAGS_OFFSET + 4));
    APP(ilist, RESTORE_FROM_DC(dcontext, DR_REG_W2, XFLAGS_OFFSET + 8));
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xd51b4200)); /* msr nzcv,x0 */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xd51b4401)); /* msr fpcr,x1 */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xd51b4422)); /* msr fpsr,x2 */
}

/* dcontext is in REG_DCXT; other registers can be used as scratch.
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    int i;
    /* add x1, x(dcxt), #(off) */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0x91000000 | 1 |
                               (REG_DCXT - DR_REG_X0) << 5 |
                               offsetof(priv_mcontext_t, simd) << 10));
    for (i = 0; i < 32; i += 4) {
        /* ld1 {v(i).2d-v(i + 3).2d}, [x1], #64 */
        APP(ilist, INSTR_CREATE_xx(dcontext, 0x4cdf2c20 + i));
    }
}

/* Append instructions to restore gpr on fcache enter, to be executed
 * right before jump to fcache target.
 * - dcontext is in REG_DCXT
 * - DR's tls base is in dr_reg_stolen
 * - all other registers can be used as scratch, and we are using X0.
 */
void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    int i;

     /* FIXME i#1573: NYI on ARM with SELFPROT_DCONTEXT */
    ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
    ASSERT(dr_reg_stolen != SCRATCH_REG0);
    /* Store stolen reg value into TLS slot. */
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, REG_OFFSET(dr_reg_stolen)));
    APP(ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG0, TLS_REG_STOLEN_SLOT));

    /* Save DR's tls base into mcontext. */
    APP(ilist, SAVE_TO_DC(dcontext, dr_reg_stolen, REG_OFFSET(dr_reg_stolen)));

    i = (REG_DCXT == DR_REG_X0);
    /* ldp x30, x(i), [x(dcxt), #x30_offset] */
    APP(ilist, INSTR_CREATE_xx(dcontext,
                               0xa9400000 | 30 | i << 10 |
                               (REG_DCXT - DR_REG_X0) << 5 |
                               REG_OFFSET(DR_REG_X30) >> 3 << 15));
    /* mov sp, x(i) */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0x9100001f | i << 5));
    for (i = 0; i < 30; i += 2) {
        if ((REG_DCXT - DR_REG_X0) >> 1 != i >> 1) {
            /* ldp x(i), x(i+1), [x(dcxt), #xi_offset] */
            APP(ilist, INSTR_CREATE_xx(dcontext,
                                       0xa9400000 | i | (i + 1) << 10 |
                                       (REG_DCXT - DR_REG_X0) << 5 |
                                       REG_OFFSET(DR_REG_X0 + i) >> 3 << 15));
        }
    }
    i = (REG_DCXT - DR_REG_X0) & ~1;
    /* ldp x(i), x(i+1), [x(dcxt), #xi_offset] */
    APP(ilist, INSTR_CREATE_xx(dcontext,
                               0xa9400000 | i | (i + 1) << 10 |
                               (REG_DCXT - DR_REG_X0) << 5 |
                               REG_OFFSET(DR_REG_X0 + i) >> 3 << 15));
}

/* Append instructions to save gpr on fcache return, called after
 * append_fcache_return_prologue.
 * Assuming the execution comes from an exit stub,
 * dcontext base is held in REG_DCXT, and exit stub in X0.
 * - store all registers into dcontext's mcontext
 * - restore REG_DCXT app value from TLS slot to mcontext
 * - restore dr_reg_stolen app value from TLS slot to mcontext
 */
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    int i;

    for (i = 0; i < 30; i += 2) {
        /* stp x(i), x(i+1), [x(dcxt), #xi_offset] */
        APP(ilist, INSTR_CREATE_xx(dcontext,
                                   0xa9000000 | i | (i + 1) << 10 |
                                   (REG_DCXT - DR_REG_X0) << 5 |
                                   REG_OFFSET(DR_REG_X0 + i) >> 3 << 15));
    }
    /* mov x1, sp */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0x910003e0 | 1));
    /* stp x30, x1, [x(dcxt), #x30_offset] */
    APP(ilist, INSTR_CREATE_xx(dcontext,
                               0xa9000000 | 30 | 1 << 10 |
                               (REG_DCXT - DR_REG_X0) << 5 |
                               REG_OFFSET(DR_REG_X30) >> 3 << 15));

    /* It would be a bit more efficient to use LDP + STP here:
     * app's x0 was spilled to DIRECT_STUB_SPILL_SLOT by exit stub.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DIRECT_STUB_SPILL_SLOT));
    if (linkstub != NULL) {
        /* FIXME i#1575: NYI for coarse-grain stub */
        ASSERT_NOT_IMPLEMENTED(false);
    } else {
        APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, R0_OFFSET));
    }
    /* App's x1 was spilled to DIRECT_STUB_SPILL_SLOT2. */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DIRECT_STUB_SPILL_SLOT2));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, R1_OFFSET));

    /* REG_DCXT's app value is stored in DCONTEXT_BASE_SPILL_SLOT by
     * append_prepare_fcache_return, so copy it to mcontext.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, DCONTEXT_BASE_SPILL_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_DCXT_OFFS));
    /* dr_reg_stolen's app value is always stored in the TLS spill slot,
     * and we restore its value back to mcontext on fcache return.
     */
    APP(ilist, RESTORE_FROM_TLS(dcontext, SCRATCH_REG1, TLS_REG_STOLEN_SLOT));
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG1, REG_OFFSET(dr_reg_stolen)));
}

/* dcontext base is held in REG_DCXT, and exit stub in X0.
 * GPR's are already saved.
 */
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    int i;
    /* add x1, x(DCXT), #(off) */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0x91000000 | 1 |
                               (REG_DCXT - DR_REG_X0) << 5 |
                               offsetof(priv_mcontext_t, simd) << 10));
    for (i = 0; i < 32; i += 4) {
        /* st1 {v(i).2d-v(i + 3).2d}, [x1], #64 */
        APP(ilist, INSTR_CREATE_xx(dcontext, 0x4c9f2c20 + i));
    }
}

/* Scratch reg0 is holding exit stub. */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xd53b4201)); /* mrs x1, nzcv */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xd53b4402)); /* mrs x2, fpcr */
    APP(ilist, INSTR_CREATE_xx(dcontext, 0xd53b4423)); /* mrs x3, fpsr */
    APP(ilist, SAVE_TO_DC(dcontext, DR_REG_W1, XFLAGS_OFFSET));
    APP(ilist, SAVE_TO_DC(dcontext, DR_REG_W2, XFLAGS_OFFSET + 4));
    APP(ilist, SAVE_TO_DC(dcontext, DR_REG_W3, XFLAGS_OFFSET + 8));
}

bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist,
                          bool ibl_end, bool absolute)
{
    /* i#1569: DR_HOOK is not supported on AArch64 */
    ASSERT_NOT_IMPLEMENTED(EXIT_DR_HOOK == NULL);
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
    bool absolute = false;
    instrlist_t ilist;
    byte *pc1;
    instrlist_init(&ilist);

    insert_shared_get_dcontext(dc, &ilist, NULL, true);
    APP(&ilist, SAVE_TO_DC(dc, DR_REG_R2, NEXT_TAG_OFFSET));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R5, DCONTEXT_BASE_SPILL_SLOT));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R2, TLS_REG2_SLOT));

    /* ldr x1, [x(stolen), #(offs)] */
    APP(&ilist, INSTR_CREATE_xx(dc, 0xf9400000 | 1 | (dr_reg_stolen - DR_REG_R0) << 5 |
                                get_fcache_return_tls_offs(dc, 0) >>
                                3 << 10));
    /* br x1 */
    APP(&ilist, INSTR_CREATE_xx(dc, 0xd61f0000 | 1 << 5));

    pc1 = instrlist_encode(dc, &ilist, pc, false);
    instrlist_clear(dc, &ilist);
    ibl_code->ibl_routine_length = pc1 - pc;
    return pc1;
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
    byte *pc;
    if (!ALIGNED(addr, 4) || !ALIGNED(addr + size, 4)) {
        ASSERT_NOT_REACHED();
        return false;
    }
    for (pc = addr; pc < addr + size; pc += 4)
        *(uint *)pc = 0xd503201f; /* nop */
    return true;
}
