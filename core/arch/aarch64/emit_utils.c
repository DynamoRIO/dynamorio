/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
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
        /* Stub starts out unlinked. */
        cache_pc exit_target = get_unlinked_entry(dcontext,
                                                  EXIT_TARGET_TAG(dcontext, f, l));
        /* stp x0, x1, [x(stolen), #(offs)] */
        *pc++ = (0xa9000000 | 0 | 1 << 10 | (dr_reg_stolen - DR_REG_X0) << 5 |
                 TLS_REG0_SLOT >> 3 << 15);
        /* mov x0, ... */
        pc = insert_mov_imm(pc, DR_REG_X0, (ptr_int_t)l);
        /* ldr x1, [x(stolen), #(offs)] */
        *pc++ = (0xf9400000 | 1 | (dr_reg_stolen - DR_REG_X0) << 5 |
                 get_ibl_entry_tls_offs(dcontext, exit_target) >> 3 << 10);
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
    /* Restore the stp x0, x1, [x28] instruction. */
    *(uint *)stub_pc = (0xa9000000 | 0 | 1 << 10 | (dr_reg_stolen - DR_REG_X0) << 5 |
                        TLS_REG0_SLOT >> 3 << 15);
    if (hot_patch)
        machine_cache_sync(stub_pc, stub_pc + AARCH64_INSTR_SIZE, true);
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
    uint *pc;
    cache_pc exit_target;
    ibl_type_t ibl_type = {0};
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type_ex(dcontext, target_tag, &ibl_type);
    ASSERT(is_ibl);
    if (IS_IBL_LINKED(ibl_type.link_state))
        exit_target = target_tag;
    else
        exit_target = get_linked_entry(dcontext, target_tag);

    pc = (uint *)(stub_pc + exit_stub_size(dcontext, target_tag, f->flags) -
                  (2 * AARCH64_INSTR_SIZE));

    /* ldr x1, [x(stolen), #(offs)] */
    pc[0] = (0xf9400000 | 1 | (dr_reg_stolen - DR_REG_X0) << 5 |
             get_ibl_entry_tls_offs(dcontext, exit_target) >> 3 << 10);
    /* br x1 */
    pc[1] = 0xd61f0000 | 1 << 5;

    if (hot_patch)
        machine_cache_sync(pc, pc + 2, true);
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
    byte *stub_pc = (byte *)EXIT_STUB_PC(dcontext, f, l);
    uint *pc;
    cache_pc exit_target;
    ibl_code_t *ibl_code = NULL;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    /* Target is always the same, so if it's already unlinked, this is a nop. */
    if (!TEST(LINK_LINKED, l->flags))
        return;
    ibl_code = get_ibl_routine_code(dcontext,
                                    extract_branchtype(l->flags), f->flags);
    exit_target = ibl_code->unlinked_ibl_entry;

    pc = (uint *)(stub_pc +
                  exit_stub_size(dcontext, ibl_code->indirect_branch_lookup_routine,
                                 f->flags) - (2 * AARCH64_INSTR_SIZE));

    /* ldr x1, [x(stolen), #(offs)] */
    *pc = (0xf9400000 | 1 | (dr_reg_stolen - DR_REG_X0) << 5 |
           get_ibl_entry_tls_offs(dcontext, exit_target) >> 3 << 10);

    machine_cache_sync(pc, pc + 1, true);
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
    /* FIXME i#1575: coarse-grain NYI on AArch64 */
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
    PRE(ilist, where, INSTR_CREATE_mrs(dcontext, opnd_create_reg(reg_base),
                                       opnd_create_reg(DR_REG_TPIDR_EL0)));
    /* ldr dr_reg_stolen, [reg_base, DR_TLS_BASE_OFFSET] */
    PRE(ilist, where,
        XINST_CREATE_load(dcontext, opnd_create_reg(dr_reg_stolen),
                          OPND_CREATE_MEMPTR(reg_base, DR_TLS_BASE_OFFSET)));
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
    APP(ilist, XINST_CREATE_move(dcontext, opnd_create_reg(REG_DCXT),
                                 opnd_create_reg(DR_REG_X0)));

    /* FIXME i#2019: check dcontext->signals_pending.  First we need a way to
     * create a conditional branch b.le.
     */

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
    APP(ilist, INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_NZCV),
                                opnd_create_reg(DR_REG_X0)));
    APP(ilist, INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_FPCR),
                                opnd_create_reg(DR_REG_X1)));
    APP(ilist, INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_FPSR),
                                opnd_create_reg(DR_REG_X2)));
}

/* dcontext is in REG_DCXT; other registers can be used as scratch.
 */
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    int i;
    /* add x1, x(dcxt), #(off) */
    APP(ilist,
        XINST_CREATE_add_2src(dcontext, opnd_create_reg(DR_REG_X1),
                              opnd_create_reg(REG_DCXT),
                              OPND_CREATE_INTPTR(offsetof(priv_mcontext_t, simd))));
    for (i = 0; i < 32; i += 2) {
        /* ldp q(i), q(i + 1), [x1, #(i * 16)] */
        APP(ilist, INSTR_CREATE_ldp(dcontext,
                                    opnd_create_reg(DR_REG_Q0 + i),
                                    opnd_create_reg(DR_REG_Q0 + i + 1),
                                    opnd_create_base_disp(DR_REG_X1, DR_REG_NULL, 0,
                                                          i * 16, OPSZ_32)));
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
    APP(ilist, INSTR_CREATE_ldp(dcontext,
                                opnd_create_reg(DR_REG_X30),
                                opnd_create_reg(DR_REG_X0 + i),
                                opnd_create_base_disp(REG_DCXT, DR_REG_NULL, 0,
                                                      REG_OFFSET(DR_REG_X30), OPSZ_16)));
    /* mov sp, x(i) */
    APP(ilist, XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_SP),
                                 opnd_create_reg(DR_REG_X0 + i)));
    for (i = 0; i < 30; i += 2) {
        if ((REG_DCXT - DR_REG_X0) >> 1 != i >> 1) {
            /* ldp x(i), x(i+1), [x(dcxt), #xi_offset] */
            APP(ilist, INSTR_CREATE_ldp(dcontext,
                                        opnd_create_reg(DR_REG_X0 + i),
                                        opnd_create_reg(DR_REG_X0 + i + 1),
                                        opnd_create_base_disp(REG_DCXT, DR_REG_NULL, 0,
                                                              REG_OFFSET(DR_REG_X0 + i),
                                                              OPSZ_16)));
        }
    }
    i = (REG_DCXT - DR_REG_X0) & ~1;
    /* ldp x(i), x(i+1), [x(dcxt), #xi_offset] */
    APP(ilist, INSTR_CREATE_ldp(dcontext,
                                opnd_create_reg(DR_REG_X0 + i),
                                opnd_create_reg(DR_REG_X0 + i + 1),
                                opnd_create_base_disp(REG_DCXT, DR_REG_NULL, 0,
                                                      REG_OFFSET(DR_REG_X0 + i),
                                                      OPSZ_16)));
}

/* Append instructions to save gpr on fcache return, called after
 * append_fcache_return_prologue.
 * Assuming the execution comes from an exit stub via br DR_REG_X1,
 * dcontext base is held in REG_DCXT, and exit stub in X0.
 * App's x0 and x1 is stored in TLS_REG0_SLOT and TLS_REG1_SLOT
 * - store all registers into dcontext's mcontext
 * - restore REG_DCXT app value from TLS slot to mcontext
 * - restore dr_reg_stolen app value from TLS slot to mcontext
 */
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info)
{
    int i;

    /* X0 and X1 will always have been saved in TLS slots before executing
     * the code generated here. See, for example:
     * emit_do_syscall_common, emit_indirect_branch_lookup, handle_sigreturn,
     * insert_exit_stub_other_flags, execute_handler_from_{cache,dispatch},
     * transfer_from_sig_handler_to_fcache_return
     */
    for (i = 2; i < 30; i += 2) {
        /* stp x(i), x(i+1), [x(dcxt), #xi_offset] */
        APP(ilist, INSTR_CREATE_stp(dcontext,
                                    opnd_create_base_disp(REG_DCXT, DR_REG_NULL, 0,
                                                          REG_OFFSET(DR_REG_X0 + i),
                                                          OPSZ_16),
                                    opnd_create_reg(DR_REG_X0 + i),
                                    opnd_create_reg(DR_REG_X0 + i + 1)));
    }
    /* mov x1, sp */
    APP(ilist, XINST_CREATE_move(dcontext, opnd_create_reg(DR_REG_X1),
                                 opnd_create_reg(DR_REG_SP)));
    /* stp x30, x1, [x(dcxt), #x30_offset] */
    APP(ilist, INSTR_CREATE_stp(dcontext,
                                opnd_create_base_disp(REG_DCXT, DR_REG_NULL, 0,
                                                      REG_OFFSET(DR_REG_X30), OPSZ_16),
                                opnd_create_reg(DR_REG_X30),
                                opnd_create_reg(DR_REG_X1)));

    /* ldp x1, x2, [x(stolen)]
     * stp x1, x2, [x(dcxt)]
     */
    APP(ilist, INSTR_CREATE_ldp(dcontext,
                                opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X2),
                                opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                                      0, OPSZ_16)));
    APP(ilist, INSTR_CREATE_stp(dcontext,
                                opnd_create_base_disp(REG_DCXT, DR_REG_NULL, 0,
                                                      0, OPSZ_16),
                                opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X2)));

    if (linkstub != NULL) {
        /* FIXME i#1575: NYI for coarse-grain stub */
        ASSERT_NOT_IMPLEMENTED(false);
    }

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
    APP(ilist,
        XINST_CREATE_add_2src(dcontext, opnd_create_reg(DR_REG_X1),
                              opnd_create_reg(REG_DCXT),
                              OPND_CREATE_INTPTR(offsetof(priv_mcontext_t, simd))));
    for (i = 0; i < 32; i += 2) {
        /* stp q(i), q(i + 1), [x1, #(i * 16)] */
        APP(ilist, INSTR_CREATE_stp(dcontext,
                                    opnd_create_base_disp(DR_REG_X1, DR_REG_NULL, 0,
                                                          i * 16, OPSZ_32),
                                    opnd_create_reg(DR_REG_Q0 + i),
                                    opnd_create_reg(DR_REG_Q0 + i + 1)));
    }
}

/* Scratch reg0 is holding exit stub. */
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    APP(ilist, INSTR_CREATE_mrs(dcontext, opnd_create_reg(DR_REG_X1),
                                opnd_create_reg(DR_REG_NZCV)));
    APP(ilist, INSTR_CREATE_mrs(dcontext, opnd_create_reg(DR_REG_X2),
                                opnd_create_reg(DR_REG_FPCR)));
    APP(ilist, INSTR_CREATE_mrs(dcontext, opnd_create_reg(DR_REG_X3),
                                opnd_create_reg(DR_REG_FPSR)));
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
                   uint flags, bool tls, bool absolute
                   _IF_X86_64(bool x86_to_x64_ibl_opt))
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
    instrlist_init(&ilist);
    patch_list_t *patch = &ibl_code->ibl_patch;
    init_patch_list(patch, PATCH_TYPE_INDIRECT_TLS);

    instr_t *load_tag = INSTR_CREATE_label(dc);
    instr_t *compare_tag = INSTR_CREATE_label(dc);
    instr_t *try_next = INSTR_CREATE_label(dc);
    instr_t *miss = INSTR_CREATE_label(dc);
    instr_t *not_hit = INSTR_CREATE_label(dc);
    instr_t *target_delete_entry = INSTR_CREATE_label(dc);
    instr_t *unlinked = INSTR_CREATE_label(dc);

    /* FIXME i#1569: Use INSTR_CREATE macros when encoder is implemented. */

    /* On entry we expect:
     *     x0: link_stub entry
     *     x1: scratch reg, arrived from br x1
     *     x2: indirect branch target
     *     TLS_REG0_SLOT: app's x0
     *     TLS_REG1_SLOT: app's x1
     *     TLS_REG2_SLOT: app's x2
     *     TLS_REG3_SLOT: scratch space
     * There are three entries with the same context:
     *     indirect_branch_lookup
     *     target_delete_entry
     *     unlink_stub_entry
     * On miss exit we output:
     *     x0: the dcontext->last_exit
     *     x1: br x1
     *     x2: app's x2
     *     TLS_REG0_SLOT: app's x0 (recovered by fcache_return)
     *     TLS_REG1_SLOT: app's x1 (recovered by fcache_return)
     * On hit exit we output:
     *     x0: fragment_start_pc (points to the fragment prefix)
     *     x1: app's x1
     *     x2: app's x2
     *     TLS_REG1_SLOT: app's x0 (recovered by fragment_prefix)
     */

    /* Spill x0. */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R0, TLS_REG3_SLOT));
    /* Load hash mask and base. */
    /* ldp x1, x0, [x28, hash_mask] */
    APP(&ilist,
        INSTR_CREATE_ldp(dc, opnd_create_reg(DR_REG_X1), opnd_create_reg(DR_REG_X0),
                         opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                               TLS_MASK_SLOT(ibl_code->branch_type),
                                               OPSZ_16)));
    /* and x1, x1, x2 */
    APP(&ilist, INSTR_CREATE_and(dc, opnd_create_reg(DR_REG_X1),
                                 opnd_create_reg(DR_REG_X1),
                                 opnd_create_reg(DR_REG_X2)));
    /* Get table entry. */
    /* add x1, x0, x1, LSL #4 */
    APP(&ilist, INSTR_CREATE_add_shift
        (dc, opnd_create_reg(DR_REG_X1),
         opnd_create_reg(DR_REG_X0),
         opnd_create_reg(DR_REG_X1),
         OPND_CREATE_INT8(DR_SHIFT_LSL),
         OPND_CREATE_INT8(4 - HASHTABLE_IBL_OFFSET(ibl_code->branch_type))));
    /* x1 now holds the fragment_entry_t* in the hashtable. */
    APP(&ilist, load_tag);
    /* Load tag from fragment_entry_t* in the hashtable to x0. */
    /* ldr x0, [x1, #tag_fragment_offset] */
    APP(&ilist,
        INSTR_CREATE_ldr(dc, opnd_create_reg(DR_REG_X0),
                         OPND_CREATE_MEMPTR(DR_REG_X1,
                                            offsetof(fragment_entry_t, tag_fragment))));
    /* Did we hit? */
    APP(&ilist, compare_tag);
    /* cbz x0, not_hit */
    APP(&ilist, INSTR_CREATE_cbz(dc, opnd_create_instr(not_hit),
                                 opnd_create_reg(DR_REG_X0)));
    /* sub x0, x0, x2 */
    APP(&ilist, XINST_CREATE_sub(dc, opnd_create_reg(DR_REG_X0),
                                 opnd_create_reg(DR_REG_X2)));
    /* cbnz x0, try_next */
    APP(&ilist, INSTR_CREATE_cbnz(dc, opnd_create_instr(try_next),
                                  opnd_create_reg(DR_REG_X0)));

    /* Hit path: load the app's original value of x0 and x1. */
    /* ldp x0, x2, [x28] */
    APP(&ilist, INSTR_CREATE_ldp(dc,
                                 opnd_create_reg(DR_REG_X0), opnd_create_reg(DR_REG_X2),
                                 opnd_create_base_disp(dr_reg_stolen, DR_REG_NULL, 0,
                                                       TLS_REG0_SLOT, OPSZ_16)));
    /* Store x0 in TLS_REG1_SLOT as requied in the fragment prefix. */
    APP(&ilist, instr_create_save_to_tls(dc, DR_REG_R0, TLS_REG1_SLOT));
    /* ldr x0, [x1, #start_pc_fragment_offset] */
    APP(&ilist, INSTR_CREATE_ldr
        (dc, opnd_create_reg(DR_REG_X0),
         OPND_CREATE_MEMPTR(DR_REG_X1,
                            offsetof(fragment_entry_t, start_pc_fragment))));
    /* mov x1, x2 */
    APP(&ilist, XINST_CREATE_move(dc, opnd_create_reg(DR_REG_X1),
                                  opnd_create_reg(DR_REG_X2)));
    /* Recover app's original x2. */
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R2, TLS_REG2_SLOT));
    /* br x0 */
    APP(&ilist, INSTR_CREATE_br(dc, opnd_create_reg(DR_REG_X0)));

    APP(&ilist, try_next);

    /* Try next entry, in case of collision. No wraparound check is needed
     * because of the sentinel at the end.
     * ldr x0, [x1, #tag_fragment_offset]! */
    APP(&ilist,
        instr_create_2dst_3src(dc, OP_ldr, opnd_create_reg(DR_REG_X0),
                               opnd_create_reg(DR_REG_X1),
                               OPND_CREATE_MEMPTR(DR_REG_X1, sizeof(fragment_entry_t)),
                               opnd_create_reg(DR_REG_X1),
                               OPND_CREATE_INTPTR(sizeof(fragment_entry_t))));
    /* b compare_tag */
    APP(&ilist, INSTR_CREATE_b(dc, opnd_create_instr(compare_tag)));

    APP(&ilist, not_hit);

    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* Load start_pc from fragment_entry_t* in the hashtable to x0. */
        /* ldr x0, [x1, #start_pc_fragment] */
        APP(&ilist, XINST_CREATE_load(dc, opnd_create_reg(DR_REG_X0),
                                      OPND_CREATE_MEMPTR(DR_REG_X1,
                                                         offsetof(fragment_entry_t,
                                                                  start_pc_fragment))));
        /* To compare with an arbitrary constant we'd need a 4th scratch reg.
         * Instead we rely on the sentinel start PC being 1.
         */
        ASSERT(HASHLOOKUP_SENTINEL_START_PC == (cache_pc)PTR_UINT_1);
        /* sub x0, x0, #1 */
        APP(&ilist, XINST_CREATE_sub(dc, opnd_create_reg(DR_REG_X0),
                                     OPND_CREATE_INT8(1)));
        /* cbnz x0, miss */
        APP(&ilist, INSTR_CREATE_cbnz(dc, opnd_create_instr(miss),
                                      opnd_create_reg(DR_REG_R0)));
        /* Point at the first table slot and then go load and compare its tag */
        /* ldr x1, [x28, #table_base] */
        APP(&ilist,
            XINST_CREATE_load(dc, opnd_create_reg(DR_REG_X1),
                              OPND_CREATE_MEMPTR(dr_reg_stolen,
                                                 TLS_TABLE_SLOT(ibl_code->branch_type))));
        /* branch to load_tag */
        APP(&ilist, INSTR_CREATE_b(dc, opnd_create_instr(load_tag)));
    }

    APP(&ilist, miss);
    /* Recover the dcontext->last_exit to x0 */
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R0, TLS_REG3_SLOT));

    /* Target delete entry */
    APP(&ilist, target_delete_entry);
    add_patch_marker(patch, target_delete_entry, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t*)&ibl_code->target_delete_entry);

    /* Unlink path: entry from stub */
    APP(&ilist, unlinked);
    add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t*)&ibl_code->unlinked_ibl_entry);

    /* Put ib tgt into dcontext->next_tag */
    insert_shared_get_dcontext(dc, &ilist, NULL, true);
    APP(&ilist, SAVE_TO_DC(dc, DR_REG_R2, NEXT_TAG_OFFSET));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R5, DCONTEXT_BASE_SPILL_SLOT));
    APP(&ilist, instr_create_restore_from_tls(dc, DR_REG_R2, TLS_REG2_SLOT));

    /* ldr x1, [x(stolen), #(offs)] */
    APP(&ilist, INSTR_CREATE_ldr(dc, opnd_create_reg(DR_REG_X1),
                                 OPND_TLS_FIELD(TLS_FCACHE_RETURN_SLOT)));
    /* br x1 */
    APP(&ilist, INSTR_CREATE_br(dc, opnd_create_reg(DR_REG_X1)));

    ibl_code->ibl_routine_length = encode_with_patch_list(dc, patch, &ilist, pc);
    instrlist_clear(dc, &ilist);
    return pc + ibl_code->ibl_routine_length;
}

void
relink_special_ibl_xfer(dcontext_t *dcontext, int index,
                        ibl_entry_point_type_t entry_type,
                        ibl_branch_type_t ibl_type)
{
    generated_code_t *code;
    byte *ibl_tgt;
    uint *pc;
    if (dcontext == GLOBAL_DCONTEXT) {
        ASSERT(!special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = SHARED_GENCODE_MATCH_THREAD(get_thread_private_dcontext());
    } else {
        ASSERT(special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = THREAD_GENCODE(dcontext);
    }
    if (code == NULL) /* thread private that we don't need */
        return;
    ibl_tgt = special_ibl_xfer_tgt(dcontext, code, entry_type, ibl_type);
    ASSERT(code->special_ibl_xfer[index] != NULL);
    pc = (uint *)(code->special_ibl_xfer[index] + code->special_ibl_unlink_offs[index]);

    protect_generated_code(code, WRITABLE);

    /* ldr x1, [x(stolen), #(offs)] */
    pc[0] = (0xf9400000 | 1 | (dr_reg_stolen - DR_REG_X0) << 5 |
             get_ibl_entry_tls_offs(dcontext, ibl_tgt) >> 3 << 10);

    /* br x1 */
    pc[1] = 0xd61f0000 | 1 << 5;

    machine_cache_sync(pc, pc + 2, true);
    protect_generated_code(code, READONLY);
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
