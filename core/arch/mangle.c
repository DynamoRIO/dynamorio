/* ******************************************************************************
 * Copyright (c) 2010-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2010 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/* file "mangle.c" */

#include "../globals.h"
#include "arch.h"
#include "../link.h"
#include "../fragment.h"
#include "arch.h"
#include "instr.h"
#include "instr_create.h"
#include "instrlist.h"
#include "decode.h"
#include "decode_fast.h"
#include "disassemble.h"
#include "../hashtable.h"
#include "../fcache.h"  /* for in_fcache */
#ifdef STEAL_REGISTER
#include "steal_reg.h"
#endif
#include "instrument.h" /* for dr_insert_call */
#include "../translate.h"

#ifdef RCT_IND_BRANCH
# include "../rct.h" /* rct_add_rip_rel_addr */
#endif

#ifdef UNIX
#include <sys/syscall.h>
#endif

#include <string.h> /* for memset */

#ifdef ANNOTATIONS
# include "../annotations.h"
#endif

/* make code more readable by shortening long lines
 * we mark everything we add as a meta-instr to avoid hitting
 * client asserts on setting translation fields
 */
#define POST instrlist_meta_postinsert
#define PRE  instrlist_meta_preinsert

#ifndef STANDALONE_DECODER
/****************************************************************************
 * clean call callee info table for i#42 and i#43
 */

/* Describes usage of a scratch slot. */
enum {
    SLOT_NONE = 0,
    SLOT_REG,
    SLOT_LOCAL,
    SLOT_FLAGS,
};
typedef byte slot_kind_t;

/* If kind is:
 * SLOT_REG: value is a reg_id_t
 * SLOT_LOCAL: value is meaningless, may change to support multiple locals
 * SLOT_FLAGS: value is meaningless
 */
typedef struct _slot_t {
    slot_kind_t kind;
    reg_id_t value;
} slot_t;

/* data structure of clean call callee information. */
typedef struct _callee_info_t {
    bool bailout;             /* if we bail out on function analysis */
    uint num_args;            /* number of args that will passed in */
    int num_instrs;           /* total number of instructions of a function */
    app_pc start;             /* entry point of a function  */
    app_pc bwd_tgt;           /* earliest backward branch target */
    app_pc fwd_tgt;           /* last forward branch target */
    int num_xmms_used;        /* number of xmms used by callee */
    bool xmm_used[NUM_XMM_REGS];  /* xmm/ymm registers usage */
    bool reg_used[NUM_GP_REGS];   /* general purpose registers usage */
    int num_callee_save_regs; /* number of regs callee saved */
    bool callee_save_regs[NUM_GP_REGS]; /* callee-save registers */
    bool has_locals;          /* if reference local via stack */
    bool xbp_is_fp;           /* if xbp is used as frame pointer */
    bool opt_inline;          /* can be inlined or not */
    bool write_aflags;        /* if the function changes aflags */
    bool read_aflags;         /* if the function reads aflags from caller */
    bool tls_used;            /* application accesses TLS (errno, etc.) */
    reg_id_t spill_reg;       /* base register for spill slots */
    uint slots_used;          /* scratch slots needed after analysis */
    slot_t scratch_slots[CLEANCALL_NUM_INLINE_SLOTS];  /* scratch slot allocation */
    instrlist_t *ilist;       /* instruction list of function for inline. */
} callee_info_t;
static callee_info_t     default_callee_info;
static clean_call_info_t default_clean_call_info;

#ifdef CLIENT_INTERFACE
/* hashtable for storing analyzed callee info */
static generic_table_t  *callee_info_table;
/* we only free callee info at exit, when callee_info_table_exit is true. */
static bool callee_info_table_exit = false;
#define INIT_HTABLE_SIZE_CALLEE 6 /* should remain small */

static void
callee_info_init(callee_info_t *ci)
{
    uint i;
    memset(ci, 0, sizeof(*ci));
    ci->bailout = true;
    /* to be conservative */
    ci->has_locals   = true;
    ci->write_aflags = true;
    ci->read_aflags  = true;
    ci->tls_used   = true;
    /* We use loop here and memset in analyze_callee_regs_usage later.
     * We could reverse the logic and use memset to set the value below,
     * but then later in analyze_callee_regs_usage, we have to use the loop.
     */
    /* assuming all xmm registers are used */
    ci->num_xmms_used = NUM_XMM_REGS;
    for (i = 0; i < NUM_XMM_REGS; i++)
        ci->xmm_used[i] = true;
    for (i = 0; i < NUM_GP_REGS; i++)
        ci->reg_used[i] = true;
    ci->spill_reg = DR_REG_INVALID;
}

static void
callee_info_free(callee_info_t *ci)
{
    ASSERT(callee_info_table_exit);
    if (ci->ilist != NULL) {
        ASSERT(ci->opt_inline);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci, callee_info_t,
                   ACCT_CLEANCALL, PROTECTED);
}

static callee_info_t *
callee_info_create(app_pc start, uint num_args)
{
    callee_info_t *info;

    info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, callee_info_t,
                           ACCT_CLEANCALL, PROTECTED);
    callee_info_init(info);
    info->start = start;
    info->num_args = num_args;
    return info;
}

static void
callee_info_reserve_slot(callee_info_t *ci, slot_kind_t kind, reg_id_t value)
{
    if (ci->slots_used < BUFFER_SIZE_ELEMENTS(ci->scratch_slots)) {
        if (kind == SLOT_REG)
            value = dr_reg_fixer[value];
        ci->scratch_slots[ci->slots_used].kind = kind;
        ci->scratch_slots[ci->slots_used].value = value;
    } else {
        LOG(THREAD_GET, LOG_CLEANCALL, 2,
            "CLEANCALL: unable to fulfill callee_info_reserve_slot for "
            "kind %d value %d\n", kind, value);
    }
    /* We check if slots_used > CLEANCALL_NUM_INLINE_SLOTS to detect failure. */
    ci->slots_used++;
}

static opnd_t
callee_info_slot_opnd(callee_info_t *ci, slot_kind_t kind, reg_id_t value)
{
    uint i;
    if (kind == SLOT_REG)
        value = dr_reg_fixer[value];
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(ci->scratch_slots); i++) {
        if (ci->scratch_slots[i].kind  == kind &&
            ci->scratch_slots[i].value == value) {
            int disp = (int)offsetof(unprotected_context_t,
                                     inline_spill_slots[i]);
            return opnd_create_base_disp(ci->spill_reg, DR_REG_NULL, 0, disp,
                                         OPSZ_PTR);
        }
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS, "Tried to find scratch slot for value "
                   "without calling callee_info_reserve_slot for it", false);
    return opnd_create_null();
}

static void
callee_info_table_init(void)
{
    callee_info_table =
        generic_hash_create(GLOBAL_DCONTEXT,
                            INIT_HTABLE_SIZE_CALLEE,
                            80 /* load factor: not perf-critical */,
                            HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
                            (void(*)(void*)) callee_info_free
                            _IF_DEBUG("callee-info table"));
}

static void
callee_info_table_destroy(void)
{
    callee_info_table_exit = true;
    generic_hash_destroy(GLOBAL_DCONTEXT, callee_info_table);
}

static callee_info_t *
callee_info_table_lookup(void *callee)
{
    callee_info_t *ci;
    TABLE_RWLOCK(callee_info_table, read, lock);
    ci = generic_hash_lookup(GLOBAL_DCONTEXT, callee_info_table,
                             (ptr_uint_t)callee);
    TABLE_RWLOCK(callee_info_table, read, unlock);
    /* We only delete the callee_info from the callee_info_table
     * when destroy the table on exit, so we can keep the ci
     * without holding the lock.
     */
    return ci;
}

static callee_info_t *
callee_info_table_add(callee_info_t *ci)
{
    callee_info_t *info;
    TABLE_RWLOCK(callee_info_table, write, lock);
    info = generic_hash_lookup(GLOBAL_DCONTEXT, callee_info_table,
                               (ptr_uint_t)ci->start);
    if (info == NULL) {
        info = ci;
        generic_hash_add(GLOBAL_DCONTEXT, callee_info_table,
                         (ptr_uint_t)ci->start, (void *)ci);
    } else {
        /* Have one in the table, free the new one and use existing one.
         * We cannot free the existing one in the table as it might be used by
         * other thread without holding the lock.
         * Since we assume callee should never be changed, they should have
         * the same content of ci.
         */
        callee_info_free(ci);
    }
    TABLE_RWLOCK(callee_info_table, write, unlock);
    return info;
}
#endif /* CLIENT_INTERFACE */

static void
clean_call_info_init(clean_call_info_t *cci, void *callee,
                     bool save_fpstate, uint num_args)
{
    memset(cci, 0, sizeof(*cci));
    cci->callee        = callee;
    cci->num_args      = num_args;
    cci->save_fpstate  = save_fpstate;
    cci->save_all_regs = true;
    cci->should_align  = true;
    cci->callee_info   = &default_callee_info;
}
#endif /* !STANDALONE_DECODER */
/***************************************************************************/

#ifdef X86 /* XXX i#1551: if we split up mangle.c, move to x86/ */
/* Convert a short-format CTI into an equivalent one using
 * near-rel-format.
 * Remember, the target is kept in the 0th src array position,
 * and has already been converted from an 8-bit offset to an
 * absolute PC, so we can just pretend instructions are longer
 * than they really are.
 */
static instr_t *
convert_to_near_rel_common(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    int opcode = instr_get_opcode(instr);
    DEBUG_DECLARE(const instr_info_t * info = instr_get_instr_info(instr);)
    app_pc target = NULL;

    if (opcode == OP_jmp_short) {
        instr_set_opcode(instr, OP_jmp);
        return instr;
    }

    if (OP_jo_short <= opcode && opcode <= OP_jnle_short) {
        /* WARNING! following is OP_ enum order specific */
        instr_set_opcode(instr, opcode - OP_jo_short + OP_jo);
        return instr;
    }

    if (OP_loopne <= opcode && opcode <= OP_jecxz) {
        uint mangled_sz;
        uint offs;
        /*
         * from "info as" on GNU/linux system:
       Note that the `jcxz', `jecxz', `loop', `loopz', `loope', `loopnz'
       and `loopne' instructions only come in byte displacements, so that if
       you use these instructions (`gcc' does not use them) you may get an
       error message (and incorrect code).  The AT&T 80386 assembler tries to
       get around this problem by expanding `jcxz foo' to
                     jcxz cx_zero
                     jmp cx_nonzero
            cx_zero: jmp foo
            cx_nonzero:
        *
        * We use that same expansion, but we want to treat the entire
        * three-instruction sequence as a single conditional branch.
        * Thus we use a special instruction that stores the entire
        * instruction sequence as mangled bytes, yet w/ a valid target operand
        * (xref PR 251646).
        * patch_branch and instr_invert_cbr
        * know how to find the target pc (final 4 of 9 bytes).
        * When decoding anything we've written we know the only jcxz or
        * loop* instructions are part of these rewritten packages, and
        * we use remangle_short_rewrite to read back in the instr.
        * (have to do this everywhere call decode() except original
        * interp, plus in input_trace())
        *
        * An alternative is to change 'jcxz foo' to:
                    <save eflags>
                    cmpb %cx,$0
                    je   foo_restore
                    <restore eflags>
                    ...
       foo_restore: <restore eflags>
               foo:
        * However the added complications of restoring the eflags on
        * the taken-branch path made me choose the former solution.
        */

        /* SUMMARY:
         * expand 'shortjump foo' to:
                          shortjump taken
                          jmp-short nottaken
                   taken: jmp foo
                nottaken:
        */
        if (ilist != NULL) {
            /* PR 266292: for meta instrs, insert separate instrs */
            /* reverse order */
            opnd_t tgt = instr_get_target(instr);
            instr_t *nottaken = INSTR_CREATE_label(dcontext);
            instr_t *taken = INSTR_CREATE_jmp(dcontext, tgt);
            ASSERT(instr_is_meta(instr));
            instrlist_meta_postinsert(ilist, instr, nottaken);
            instrlist_meta_postinsert(ilist, instr, taken);
            instrlist_meta_postinsert(ilist, instr, INSTR_CREATE_jmp_short
                                      (dcontext, opnd_create_instr(nottaken)));
            instr_set_target(instr, opnd_create_instr(taken));
            return taken;
        }

        if (opnd_is_near_pc(instr_get_target(instr)))
            target = opnd_get_pc(instr_get_target(instr));
        else if (opnd_is_near_instr(instr_get_target(instr))) {
            instr_t *tgt = opnd_get_instr(instr_get_target(instr));
            /* assumption: target's translation or raw bits are set properly */
            target = instr_get_translation(tgt);
            if (target == NULL && instr_raw_bits_valid(tgt))
                target = instr_get_raw_bits(tgt);
            ASSERT(target != NULL);
        } else
            ASSERT_NOT_REACHED();

        /* PR 251646: cti_short_rewrite: target is in src0, so operands are
         * valid, but raw bits must also be valid, since they hide the multiple
         * instrs.  For x64, it is marked for re-relativization, but it's
         * special since the target must be obtained from src0 and not
         * from the raw bits (since that might not reach).
         */
        /* need 9 bytes + possible addr prefix */
        mangled_sz = CTI_SHORT_REWRITE_LENGTH;
        if (!reg_is_pointer_sized(opnd_get_reg(instr_get_src(instr, 1))))
            mangled_sz++; /* need addr prefix */
        instr_allocate_raw_bits(dcontext, instr, mangled_sz);
        offs = 0;
        if (mangled_sz > CTI_SHORT_REWRITE_LENGTH) {
            instr_set_raw_byte(instr, offs, ADDR_PREFIX_OPCODE);
            offs++;
        }
        /* first 2 bytes: jecxz 8-bit-offset */
        instr_set_raw_byte(instr, offs, decode_first_opcode_byte(opcode));
        offs++;
        /* remember pc-relative offsets are from start of next instr */
        instr_set_raw_byte(instr, offs, (byte)2);
        offs++;
        /* next 2 bytes: jmp-short 8-bit-offset */
        instr_set_raw_byte(instr, offs, decode_first_opcode_byte(OP_jmp_short));
        offs++;
        instr_set_raw_byte(instr, offs, (byte)5);
        offs++;
        /* next 5 bytes: jmp 32-bit-offset */
        instr_set_raw_byte(instr, offs, decode_first_opcode_byte(OP_jmp));
        offs++;
        /* for x64 we may not reach, but we go ahead and try */
        instr_set_raw_word(instr, offs, (int)
                           (target - (instr->bytes + mangled_sz)));
        offs += sizeof(int);
        ASSERT(offs == mangled_sz);
        LOG(THREAD, LOG_INTERP, 2, "convert_to_near_rel: jecxz/loop* opcode\n");
        /* original target operand is still valid */
        instr_set_operands_valid(instr, true);
        return instr;
    }

    LOG(THREAD, LOG_INTERP, 1, "convert_to_near_rel: unknown opcode: %d %s\n",
        opcode, info->name);
    ASSERT_NOT_REACHED();      /* conversion not possible OR not a short-form cti */
    return instr;
}

instr_t *
convert_to_near_rel_meta(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    return convert_to_near_rel_common(dcontext, ilist, instr);
}

void
convert_to_near_rel(dcontext_t *dcontext, instr_t *instr)
{
    convert_to_near_rel_common(dcontext, NULL, instr);
}

/* For jecxz and loop*, we create 3 instructions in a single
 * instr that we treat like a single conditional branch.
 * On re-decoding our own output we need to recreate that instr.
 * This routine assumes that the instructions encoded at pc
 * are indeed a mangled cti short.
 * Assumes that the first instr has already been decoded into instr,
 * that pc points to the start of that instr.
 * Converts instr into a new 3-raw-byte-instr with a private copy of the
 * original raw bits.
 * Optionally modifies the target to "target" if "target" is non-null.
 * Returns the pc of the instruction after the remangled sequence.
 */
byte *
remangle_short_rewrite(dcontext_t *dcontext,
                       instr_t *instr, byte *pc, app_pc target)
{
    uint mangled_sz = CTI_SHORT_REWRITE_LENGTH;
    ASSERT(instr_is_cti_short_rewrite(instr, pc));
    if (*pc == ADDR_PREFIX_OPCODE)
        mangled_sz++;

    /* first set the target in the actual operand src0 */
    if (target == NULL) {
        /* acquire existing absolute target */
        int rel_target = *((int *)(pc + mangled_sz - 4));
        target = pc + mangled_sz + rel_target;
    }
    instr_set_target(instr, opnd_create_pc(target));
    /* now set up the bundle of raw instructions
     * we've already read the first 2-byte instruction, jecxz/loop*
     * they all take up mangled_sz bytes
     */
    instr_allocate_raw_bits(dcontext, instr, mangled_sz);
    instr_set_raw_bytes(instr, pc, mangled_sz);
    /* for x64 we may not reach, but we go ahead and try */
    instr_set_raw_word(instr, mangled_sz - 4, (int)(target - (pc + mangled_sz)));
    /* now make operands valid */
    instr_set_operands_valid(instr, true);
    return (pc+mangled_sz);
}
#elif defined (ARM)

byte *
remangle_short_rewrite(dcontext_t *dcontext,
                       instr_t *instr, byte *pc, app_pc target)
{
    /* FIXME i#1551: refactor the caller and make this routine x86-only. */
    ASSERT_NOT_REACHED();
    return NULL;
}

#endif /* X86/ARM */

/***************************************************************************/
#if !defined(STANDALONE_DECODER)

/* the stack size of a full context switch for clean call */
int
get_clean_call_switch_stack_size(void)
{
    return sizeof(priv_mcontext_t);
}

/* extra temporarily-used stack usage beyond
 * get_clean_call_switch_stack_size()
 */
int
get_clean_call_temp_stack_size(void)
{
    return XSP_SZ; /* for eflags clear code: push 0; popf */
}

static int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save)
{
    if (save) {
        /* We adjust the stack so the return address will not be clobbered,
         * so we can have call/return pair to take advantage of hardware
         * call return stack for better performance.
         * xref emit_clean_call_save @ x86/emit_utils.c
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea
            (dcontext,
             opnd_create_reg(DR_REG_XSP),
             opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                                   -(int)(get_clean_call_switch_stack_size() +
                                          get_clean_call_temp_stack_size()),
                                   OPSZ_lea)));
    }
    PRE(ilist, instr,
        INSTR_CREATE_call
        (dcontext, save ?
         opnd_create_pc(get_clean_call_save(dcontext _IF_X64(GENCODE_X64))) :
         opnd_create_pc(get_clean_call_restore(dcontext _IF_X64(GENCODE_X64)))));
    return get_clean_call_switch_stack_size();
}

void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci,
                    instrlist_t *ilist, instr_t *instr)
{
    /* clear eflags for callee's usage */
    if (cci == NULL || !cci->skip_clear_eflags) {
        if (dynamo_options.cleancall_ignore_eflags) {
            /* we still clear DF since some compiler assumes
             * DF is cleared at each function.
             */
            PRE(ilist, instr, INSTR_CREATE_cld(dcontext));
        } else {
            /* on x64 a push immed is sign-extended to 64-bit */
            PRE(ilist, instr,
                INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));
            PRE(ilist, instr, INSTR_CREATE_popf(dcontext));
        }
    }
}

/* Pushes not only the GPRs but also xmm/ymm, xip, and xflags, in
 * priv_mcontext_t order.
 * The current stack pointer alignment should be passed.  Use 1 if
 * unknown (NOT 0).
 * Returns the amount of data pushed.  Does NOT fix up the xsp value pushed
 * to be the value prior to any pushes for x64 as no caller needs that
 * currently (they all build a priv_mcontext_t and have to do further xsp
 * fixups anyway).
 * Includes xmm0-5 for PR 264138.
 */
uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr,
                          uint alignment, instr_t *push_pc)
{
    uint dstack_offs = 0;
    int  offs_beyond_xmm = 0;
    if (cci == NULL)
        cci = &default_clean_call_info;
    if (cci->preserve_mcontext || cci->num_xmms_skip != NUM_XMM_REGS) {
        int offs = XMM_SLOTS_SIZE + PRE_XMM_PADDING;
        if (cci->preserve_mcontext && cci->skip_save_aflags) {
            offs_beyond_xmm = 2*XSP_SZ; /* pc and flags */
            offs += offs_beyond_xmm;
        }
        PRE(ilist, instr, INSTR_CREATE_lea
            (dcontext, opnd_create_reg(REG_XSP),
             OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -offs)));
        dstack_offs += offs;
    }
    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel */
        int i;
        /* PR 266305: see discussion in emit_fcache_enter_shared on
         * which opcode is better.  Note that the AMD optimization
         * guide says to use movlps+movhps for unaligned stores, but
         * for simplicity and smaller code I'm using movups anyway.
         */
        /* XXX i#438: once have SandyBridge processor need to measure
         * cost of vmovdqu and whether worth arranging 32-byte alignment
         * for all callers.  B/c we put ymm at end of priv_mcontext_t, we do
         * currently have 32-byte alignment for clean calls.
         */
        uint opcode = move_mm_reg_opcode(ALIGNED(alignment, 16), ALIGNED(alignment, 32));
        ASSERT(proc_has_feature(FEATURE_SSE));
        for (i=0; i<NUM_XMM_SAVED; i++) {
            if (!cci->xmm_skip[i]) {
                PRE(ilist, instr, instr_create_1dst_1src
                    (dcontext, opcode,
                     opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                           PRE_XMM_PADDING + i*XMM_SAVED_REG_SIZE +
                                           offs_beyond_xmm,
                                           OPSZ_SAVED_XMM),
                     opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i)));
            }
        }
        ASSERT(i*XMM_SAVED_REG_SIZE == XMM_SAVED_SIZE);
        ASSERT(XMM_SAVED_SIZE <= XMM_SLOTS_SIZE);
    }
    /* pc and aflags */
    if (!cci->skip_save_aflags) {
        ASSERT(offs_beyond_xmm == 0);
        PRE(ilist, instr, push_pc);
        dstack_offs += XSP_SZ;
        PRE(ilist, instr, INSTR_CREATE_pushf(dcontext));
        dstack_offs += XSP_SZ;
    } else {
        ASSERT(offs_beyond_xmm == 2*XSP_SZ || !cci->preserve_mcontext);
        /* for cci->preserve_mcontext we added to the lea above */
        instr_destroy(dcontext, push_pc);
    }

#ifdef X64
    /* keep priv_mcontext_t order */
    if (!cci->reg_skip[REG_R15 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R15)));
    if (!cci->reg_skip[REG_R14 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R14)));
    if (!cci->reg_skip[REG_R13 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R13)));
    if (!cci->reg_skip[REG_R12 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R12)));
    if (!cci->reg_skip[REG_R11 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R11)));
    if (!cci->reg_skip[REG_R10 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R10)));
    if (!cci->reg_skip[REG_R9 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R9)));
    if (!cci->reg_skip[REG_R8 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_R8)));
    if (!cci->reg_skip[REG_RAX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RAX)));
    if (!cci->reg_skip[REG_RCX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RCX)));
    if (!cci->reg_skip[REG_RDX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RDX)));
    if (!cci->reg_skip[REG_RBX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RBX)));
    /* we do NOT match pusha xsp value */
    if (!cci->reg_skip[REG_RSP - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RSP)));
    if (!cci->reg_skip[REG_RBP - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RBP)));
    if (!cci->reg_skip[REG_RSI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RSI)));
    if (!cci->reg_skip[REG_RDI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_RDI)));
    dstack_offs += (NUM_GP_REGS - cci->num_regs_skip) * XSP_SZ;
#else
    PRE(ilist, instr, INSTR_CREATE_pusha(dcontext));
    dstack_offs += 8 * XSP_SZ;
#endif
    ASSERT(cci->skip_save_aflags   ||
           cci->num_xmms_skip != 0 ||
           cci->num_regs_skip != 0 ||
           dstack_offs == (uint)get_clean_call_switch_stack_size());
    return dstack_offs;
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
    int offs_beyond_xmm = 0;
    if (cci == NULL)
        cci = &default_clean_call_info;

#ifdef X64
    /* in priv_mcontext_t order */
    if (!cci->reg_skip[REG_RDI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RDI)));
    if (!cci->reg_skip[REG_RSI - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RSI)));
    if (!cci->reg_skip[REG_RBP - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBP)));
    /* skip xsp by popping into dead rbx */
    if (!cci->reg_skip[REG_RSP - REG_XAX]) {
        ASSERT(!cci->reg_skip[REG_RBX - REG_XAX]);
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBX)));
    }
    if (!cci->reg_skip[REG_RBX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RBX)));
    if (!cci->reg_skip[REG_RDX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RDX)));
    if (!cci->reg_skip[REG_RCX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RCX)));
    if (!cci->reg_skip[REG_RAX - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RAX)));
    if (!cci->reg_skip[REG_R8 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R8)));
    if (!cci->reg_skip[REG_R9 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R9)));
    if (!cci->reg_skip[REG_R10 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R10)));
    if (!cci->reg_skip[REG_R11 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R11)));
    if (!cci->reg_skip[REG_R12 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R12)));
    if (!cci->reg_skip[REG_R13 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R13)));
    if (!cci->reg_skip[REG_R14 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R14)));
    if (!cci->reg_skip[REG_R15 - REG_XAX])
        PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_R15)));
#else
    PRE(ilist, instr, INSTR_CREATE_popa(dcontext));
#endif
    if (!cci->skip_save_aflags) {
        PRE(ilist, instr, INSTR_CREATE_popf(dcontext));
        offs_beyond_xmm = XSP_SZ; /* pc */;
    } else if (cci->preserve_mcontext) {
        offs_beyond_xmm = 2*XSP_SZ; /* aflags + pc */
    }

    if (preserve_xmm_caller_saved()) {
        /* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel */
        int i;
        /* See discussion in emit_fcache_enter_shared on which opcode
         * is better. */
        uint opcode = move_mm_reg_opcode(ALIGNED(alignment, 32), ALIGNED(alignment, 16));
        ASSERT(proc_has_feature(FEATURE_SSE));
        for (i=0; i<NUM_XMM_SAVED; i++) {
            if (!cci->xmm_skip[i]) {
                PRE(ilist, instr, instr_create_1dst_1src
                    (dcontext, opcode, opnd_create_reg(REG_SAVED_XMM0 + (reg_id_t)i),
                     opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                           PRE_XMM_PADDING + i*XMM_SAVED_REG_SIZE +
                                           offs_beyond_xmm,
                                           OPSZ_SAVED_XMM)));
            }
        }
        ASSERT(i*XMM_SAVED_REG_SIZE == XMM_SAVED_SIZE);
        ASSERT(XMM_SAVED_SIZE <= XMM_SLOTS_SIZE);
    }

    PRE(ilist, instr, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XSP),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0,
                             PRE_XMM_PADDING + XMM_SLOTS_SIZE +
                             offs_beyond_xmm)));
}

/* utility routines for inserting clean calls to an instrumentation routine
 * strategy is very similar to fcache_enter/return
 * FIXME: try to share code with fcache_enter/return?
 *
 * first swap stacks to DynamoRIO stack:
 *      SAVE_TO_UPCONTEXT %xsp,xsp_OFFSET
 *      RESTORE_FROM_DCONTEXT dstack_OFFSET,%xsp
 * swap peb/teb fields
 * now save app eflags and registers, being sure to lay them out on
 * the stack in priv_mcontext_t order:
 *      push $0 # for priv_mcontext_t.pc; wasted, for now
 *      pushf
 *      pusha # xsp is dstack-XSP_SZ*2; rest are app values
 * clear the eflags for our usage
 * ASSUMPTION (also made in x86.asm): 0 ok, reserved bits are not set by popf,
 *                                    and clearing, not preserving, is good enough
 *      push   $0
 *      popf
 * make the call
 *      call routine
 * restore app regs and eflags
 *      popa
 *      popf
 *      lea XSP_SZ(xsp),xsp # clear priv_mcontext_t.pc slot
 * swap peb/teb fields
 * restore app stack
 *      RESTORE_FROM_UPCONTEXT xsp_OFFSET,%xsp
 */

void
insert_get_mcontext_base(dcontext_t *dcontext, instrlist_t *ilist,
                         instr_t *where, reg_id_t reg)
{
    PRE(ilist, where, instr_create_restore_from_tls
        (dcontext, reg, TLS_DCONTEXT_SLOT));

    /* An extra level of indirection with SELFPROT_DCONTEXT */
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        ASSERT_NOT_TESTED();
        PRE(ilist, where, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(reg),
             OPND_CREATE_MEMPTR(reg, offsetof(dcontext_t, upcontext))));
    }
}

/* What prepare_for_clean_call() adds to xsp beyond sizeof(priv_mcontext_t) */
static inline int
clean_call_beyond_mcontext(void)
{
    return 0; /* no longer adding anything */
}

/* prepare_for and cleanup_after assume that the stack looks the same after
 * the call to the instrumentation routine, since it stores the app state
 * on the stack.
 * Returns the size of the data stored on the DR stack.
 * WARNING: this routine does NOT save the fp/mmx/sse state, to do that the
 * instrumentation routine should call proc_save_fpstate() and then
 * proc_restore_fpstate()
 * (This is because of expense:
 *   fsave takes 118 cycles!
 *   frstor (separated by 6 instrs from fsave) takes 89 cycles
 *   fxsave and fxrstor are not available on HP machine!
 *   supposedly they came out in PII
 *   on balrog: fxsave 91 cycles, fxrstor 173)
 *
 * For x64, changes the stack pointer by a multiple of 16.
 *
 * NOTE: The client interface's get/set mcontext functions and the
 * hotpatching gateway rely on the app's context being available
 * on the dstack in a particular format.  Do not corrupt this data
 * unless you update all users of this data!
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot.
 * We guarantee to clients that all other slots (except the XAX mcontext slot)
 * will remain untouched.
 *
 * N.B.: insert_parameter_preparation (and our documentation for
 * dr_prepare_for_call) assumes that this routine only modifies xsp
 * and xax and no other registers.
 */
/* number of extra slots in addition to register slots. */
#define NUM_EXTRA_SLOTS 2 /* pc, aflags */
uint
prepare_for_clean_call(dcontext_t *dcontext, clean_call_info_t *cci,
                       instrlist_t *ilist, instr_t *instr)
{
    uint dstack_offs = 0;

    if (cci == NULL)
        cci = &default_clean_call_info;

    /* Swap stacks.  For thread-shared, we need to get the dcontext
     * dynamically rather than use the constant passed in here.  Save
     * away xax in a TLS slot and then load the dcontext there.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, instr, instr_create_save_to_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));

        insert_get_mcontext_base(dcontext, ilist, instr,
                                 REG_XAX);

        PRE(ilist, instr, instr_create_save_to_dc_via_reg
            (dcontext, REG_XAX, REG_XSP, XSP_OFFSET));

        /* DSTACK_OFFSET isn't within the upcontext so if it's separate this won't
         * work right.  FIXME - the dcontext accessing routines are a mess of shared
         * vs. no shared support, separate context vs. no separate context support etc. */
        ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
        /* i#249: swap PEB pointers while we have dcxt in reg.  We risk "silent
         * death" by using xsp as scratch but don't have simple alternative.
         * We don't support non-SCRATCH_ALWAYS_TLS.
         */
        /* XXX: should use clean callee analysis to remove pieces of this
         * such as errno preservation
         */
        if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer() &&
            !cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX/*dc*/, REG_XSP/*scratch*/, true/*to priv*/);
        }
#endif
        PRE(ilist, instr, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_XSP, DSTACK_OFFSET));

        /* restore xax before pushing the context on the dstack */
        PRE(ilist, instr, instr_create_restore_from_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));
    }
    else {
        PRE(ilist, instr, instr_create_save_to_dcontext(dcontext, REG_XSP, XSP_OFFSET));
#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
        if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer() &&
            !cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX/*unused*/, REG_XSP/*scratch*/, true/*to priv*/);
        }
#endif
        PRE(ilist, instr, instr_create_restore_dynamo_stack(dcontext));
    }

    /* Save flags and all registers, in priv_mcontext_t order.
     * We're at base of dstack so should be nicely aligned.
     */
    ASSERT(ALIGNED(dcontext->dstack, PAGE_SIZE));

    /* Note that we do NOT bother to put the correct pre-push app xsp value on the
     * stack here, as an optimization for callees who never ask for it: instead we
     * rely on dr_[gs]et_mcontext() to fix it up if asked for.  We can get away w/
     * this while hotpatching cannot (hotp_inject_gateway_call() fixes it up every
     * time) b/c the callee has to ask for the priv_mcontext_t.
     */
    if (cci->out_of_line_swap) {
        dstack_offs +=
            insert_out_of_line_context_switch(dcontext, ilist, instr, true);
    } else {
        dstack_offs +=
            insert_push_all_registers(dcontext, cci, ilist, instr, PAGE_SIZE,
                                      INSTR_CREATE_push_imm
                                      (dcontext, OPND_CREATE_INT32(0)));
        insert_clear_eflags(dcontext, cci, ilist, instr);
    }

    /* We no longer need to preserve the app's errno on Windows except
     * when using private libraries, so its preservation is in
     * preinsert_swap_peb().
     * We do not need to preserve DR's Linux errno across app execution.
     */

#if defined(X64) || defined(MACOS)
    /* PR 218790: maintain 16-byte rsp alignment.
     * insert_parameter_preparation() currently assumes we leave rsp aligned.
     */
    /* check if need adjust stack for alignment. */
    if (cci->should_align) {
        uint num_slots = NUM_GP_REGS + NUM_EXTRA_SLOTS;
        if (cci->skip_save_aflags)
            num_slots -= 2;
        num_slots -= cci->num_regs_skip; /* regs that not saved */
        if ((num_slots % 2) == 1) {
            ASSERT((dstack_offs % 16) == 8);
            PRE(ilist, instr, INSTR_CREATE_lea
                (dcontext, opnd_create_reg(REG_XSP),
                 OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, -(int)XSP_SZ)));
            dstack_offs += XSP_SZ;
        } else {
            ASSERT((dstack_offs % 16) == 0);
        }
    }
#endif
    ASSERT(cci->skip_save_aflags   ||
           cci->num_xmms_skip != 0 ||
           cci->num_regs_skip != 0 ||
           dstack_offs == sizeof(priv_mcontext_t) + clean_call_beyond_mcontext());
    return dstack_offs;
}

void
cleanup_after_clean_call(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *instr)
{
    if (cci == NULL)
        cci = &default_clean_call_info;
    /* saved error code is currently on the top of the stack */

#if defined(X64) || defined(MACOS)
    /* PR 218790: remove the padding we added for 16-byte rsp alignment */
    if (cci->should_align) {
        uint num_slots = NUM_GP_REGS + NUM_EXTRA_SLOTS;
        if (cci->skip_save_aflags)
            num_slots += 2;
        num_slots -= cci->num_regs_skip; /* regs that not saved */
        if ((num_slots % 2) == 1) {
            PRE(ilist, instr, INSTR_CREATE_lea
                (dcontext, opnd_create_reg(REG_XSP),
                 OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, XSP_SZ)));
        }
    }
#endif

    /* now restore everything */
    if (cci->out_of_line_swap) {
        insert_out_of_line_context_switch(dcontext, ilist, instr, false);
    } else {
        insert_pop_all_registers(dcontext, cci, ilist, instr,
                                 /* see notes in prepare_for_clean_call() */
                                 PAGE_SIZE);
    }

    /* Swap stacks back.  For thread-shared, we need to get the dcontext
     * dynamically.  Save xax in TLS so we can use it as scratch.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        PRE(ilist, instr, instr_create_save_to_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));

        insert_get_mcontext_base(dcontext, ilist, instr,
                                 REG_XAX);

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
        /* i#249: swap PEB pointers while we have dcxt in reg.  We risk "silent
         * death" by using xsp as scratch but don't have simple alternative.
         * We don't support non-SCRATCH_ALWAYS_TLS.
         */
        if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer() &&
            !cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX/*dc*/, REG_XSP/*scratch*/, false/*to app*/);
        }
#endif

        PRE(ilist, instr, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_XSP, XSP_OFFSET));

        PRE(ilist, instr, instr_create_restore_from_tls
            (dcontext, REG_XAX, TLS_XAX_SLOT));
    }
    else {
#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
        if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer() &&
            !cci->out_of_line_swap) {
            preinsert_swap_peb(dcontext, ilist, instr, !SCRATCH_ALWAYS_TLS(),
                               REG_XAX/*unused*/, REG_XSP/*scratch*/, false/*to app*/);
        }
#endif
        PRE(ilist, instr,
            instr_create_restore_from_dcontext(dcontext, REG_XSP, XSP_OFFSET));
    }
}

bool
parameters_stack_padded(void)
{
    return (REGPARM_MINSTACK > 0 || REGPARM_END_ALIGN > XSP_SZ);
}

static reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg)
{
#ifdef X64
    if (opnd_get_size(arg) == OPSZ_4) { /* we ignore var-sized */
        /* PR 250976 #2: leave 64-bit only if an immed w/ top bit set (we
         * assume user wants sign-extension; that is after all what happens
         * on a push of a 32-bit immed) */
        if (!opnd_is_immed_int(arg) ||
            (opnd_get_immed_int(arg) & 0x80000000) == 0)
            return reg_64_to_32(regular);
    }
#endif
    return regular;
}


/* Returns the change in the stack pointer.
 * N.B.: due to stack alignment and minimum stack reservation, do
 * not use parameters involving esp/rsp, as its value can change!
 *
 * This routine only supports passing arguments that are integers or
 * pointers of a size equal or smaller than the register size: i.e., no
 * floating-point, multimedia, or aggregate data types.
 *
 * For 64-bit mode, if a 32-bit immediate integer is specified as an
 * argument and it has its top bit set, we assume it is intended to be
 * sign-extended to 64-bits; otherwise we zero-extend it.
 *
 * For 64-bit mode, variable-sized argument operands may not work
 * properly.
 *
 * Arguments that reference REG_XSP will work for clean calls, but are not guaranteed
 * to work for non-clean, especially for 64-bit where we align, etc.  Arguments that
 * reference sub-register portions of REG_XSP are not supported.
 *
 * XXX PR 307874: w/ a post optimization pass, or perhaps more clever use of
 * existing passes, we could do much better on calling convention and xsp conflicting
 * args.  We should also really consider inlining client callees (PR 218907), since
 * clean calls for 64-bit are enormous (71 instrs/264 bytes for 2-arg x64; 26
 * instrs/99 bytes for x86) and we could avoid all the xmm saves and replace pushf w/
 * lahf.
 */
static uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args)
{
    uint i;
    int r;
    uint preparm_padding = 0;
    uint param_stack = 0, total_stack = 0;
    bool push = true;
    bool restore_xax = false;
    bool restore_xsp = false;
    /* we need two passes for PR 250976 optimization */
    /* Push/mov in reverse order.  We need a label so we can also add
     * instrs prior to the regular param prep.  So params are POST-mark, while
     * pre-param-prep is POST-prev or PRE-mark.
     */
#ifdef X64
    uint arg_pre_push = 0, total_pre_push = 0;
#endif
    instr_t *prev = (instr == NULL) ? instrlist_last(ilist) : instr_get_prev(instr);
    instr_t *mark = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, mark);

    /* For a clean call, xax is dead (clobbered by prepare_for_clean_call()).
     * Rather than use as scratch and restore prior to each param that uses it,
     * we restore once up front if any use it, and use regparms[0] as scratch,
     * which is symmetric with non-clean-calls: regparms[0] is dead since we're
     * doing args in reverse order.  However, we then can't use regparms[0]
     * directly if referenced in earlier params, but similarly for xax, so
     * there's no clear better way. (prepare_for_clean_call also clobbers xsp,
     * but we just disallow args that use it).
     */

    ASSERT(num_args == 0 || args != NULL);
    /* We can get away w/ one pass, except for PR 250976 we want calling conv
     * regs to be able to refer to priv_mcontext_t as well as potentially being
     * pushed: but we need to know the total # pushes ahead of time (since hard
     * to mark for post-patching)
     */
    for (i = 0; i < num_args; i++) {
        IF_X64(bool is_pre_push = false;)
        for (r = 0; r < opnd_num_regs_used(args[i]); r++) {
            reg_id_t used = opnd_get_reg_used(args[i], r);
            IF_X64(int parm;)
            LOG(THREAD, LOG_INTERP, 4,
                "ipp: considering arg %d reg %d == %s\n", i, r, reg_names[used]);
            if (clean_call && !restore_xax && reg_overlap(used, REG_XAX))
                restore_xax = true;
            if (reg_overlap(used, REG_XSP)) {
                IF_X64(CLIENT_ASSERT(clean_call,
                                     "Non-clean-call argument: REG_XSP not supported"));
                CLIENT_ASSERT(used == REG_XSP,
                              "Call argument: sub-reg-xsp not supported");
                if (clean_call && /*x64*/parameters_stack_padded() && !restore_xsp)
                    restore_xsp = true;
            }
#ifdef X64
            /* PR 250976 #A: count the number of pre-pushes we need */
            parm = reg_parameter_num(used);
            /* We can read a register used in an earlier arg since we store that
             * arg later (we do reverse order), except arg0, which we use as
             * scratch (we don't always need it, but not worth another pre-pass
             * through all args to find out), and xsp.  Otherwise, if a plain reg,
             * we point at mcontext (we restore xsp slot in mcontext if nec.).
             * If a mem ref, we need to pre-push onto stack.
             * N.B.: this conditional is duplicated in 2nd loop.
             */
            if (!is_pre_push &&
                ((parm == 0 && num_args > 1) || parm > (int)i ||
                 reg_overlap(used, REG_XSP)) &&
                (!clean_call || !opnd_is_reg(args[i]))) {
                total_pre_push++;
                is_pre_push = true; /* ignore further regs in same arg */
            }
#endif
        }
    }

    if (parameters_stack_padded()) {
        /* For x64, supposed to reserve rsp space in function prologue; we
         * do next best thing and reserve it prior to setting up the args.
         */
        push = false; /* store args to xsp offsets instead of pushing them */
        total_stack = REGPARM_MINSTACK;
        if (num_args > NUM_REGPARM)
            total_stack += XSP_SZ * (num_args - NUM_REGPARM);
        param_stack = total_stack;
        IF_X64(total_stack += XSP_SZ * total_pre_push);
        /* We assume rsp is currently 16-byte aligned.  End of arguments is supposed
         * to be 16-byte aligned for x64 SysV (note that retaddr will then make
         * rsp 8-byte-aligned, which is ok: callee has to rectify that).
         * For clean calls, prepare_for_clean_call leaves rsp aligned for x64.
         * XXX PR 218790: we require users of dr_insert_call to ensure
         * alignment; should we put in support to dynamically align?
         */
        preparm_padding =
            ALIGN_FORWARD_UINT(total_stack, REGPARM_END_ALIGN) - total_stack;
        total_stack += preparm_padding;
        /* we have to wait to insert the xsp adjust */
    } else {
        ASSERT(NUM_REGPARM == 0);
        ASSERT(push);
        IF_X64(ASSERT(total_pre_push == 0));
        total_stack = XSP_SZ * num_args;
    }
    LOG(THREAD, LOG_INTERP, 3,
        "insert_parameter_preparation: %d args, %d in-reg, %d pre-push, %d/%d stack\n",
        num_args, NUM_REGPARM, IF_X64_ELSE(total_pre_push, 0), param_stack, total_stack);

    for (i = 0; i < num_args; i++) {
        /* FIXME PR 302951: we need to handle state restoration if any
         * of these args references app memory.  We should pull the state from
         * the priv_mcontext_t on the stack if in a clean call.  FIXME: what if not?
         */
        opnd_t arg = args[i];
        CLIENT_ASSERT(opnd_get_size(arg) == OPSZ_PTR || opnd_is_immed_int(arg)
                      IF_X64(|| opnd_get_size(arg) == OPSZ_4),
                      "Clean call arg has unsupported size");

#ifdef X64
        /* PR 250976 #A: support args that reference param regs */
        for (r = 0; r < opnd_num_regs_used(arg); r++) {
            reg_id_t used = opnd_get_reg_used(arg, r);
            int parm = reg_parameter_num(used);
            /* See comments in loop above */
            if ((parm == 0 && num_args > 1) || parm > (int)i ||
                 reg_overlap(used, REG_XSP)) {
                int disp = 0;
                if (clean_call && opnd_is_reg(arg)) {
                    /* We can point at the priv_mcontext_t slot.
                     * priv_mcontext_t is at the base of dstack: compute offset
                     * from xsp to the field we want and replace arg.
                     */
                    disp += opnd_get_reg_dcontext_offs(opnd_get_reg(arg));
                    /* skip rest of what prepare_for_clean_call adds */
                    disp += clean_call_beyond_mcontext();
                    /* skip what this routine added */
                    disp += total_stack;
                } else {
                    /* Push a temp on the stack and point at it.  We
                     * could try to optimize by juggling registers, but
                     * not worth it.
                     */
                    /* xsp was adjusted up above; we simply store to xsp offsets */
                    disp = param_stack + XSP_SZ * arg_pre_push;
                    if (opnd_is_reg(arg) && opnd_get_size(arg) == OPSZ_PTR) {
                        POST(ilist, prev, INSTR_CREATE_mov_st
                             (dcontext, OPND_CREATE_MEMPTR(REG_XSP, disp), arg));
                    } else {
                        reg_id_t xsp_scratch = regparms[0];
                        /* don't want to just change size since will read extra bytes.
                         * can't do mem-to-mem so go through scratch reg */
                        if (reg_overlap(used, REG_XSP)) {
                            /* Get original xsp into scratch[0] and replace in arg */
                            if (opnd_uses_reg(arg, regparms[0])) {
                                xsp_scratch = REG_XAX;
                                ASSERT(!opnd_uses_reg(arg, REG_XAX)); /* can't use 3 */
                                /* FIXME: rather than putting xsp into mcontext
                                 * slot, better to just do local get from dcontext
                                 * like we do for 32-bit below? */
                                POST(ilist, prev, instr_create_restore_from_tls
                                     (dcontext, REG_XAX, TLS_XAX_SLOT));
                            }
                            opnd_replace_reg(&arg, REG_XSP, xsp_scratch);
                        }
                        POST(ilist, prev,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, disp),
                                                 opnd_create_reg(regparms[0])));
                        /* If sub-ptr-size, zero-extend is what we want so no movsxd */
                        POST(ilist, prev, INSTR_CREATE_mov_ld
                             (dcontext, opnd_create_reg
                              (shrink_reg_for_param(regparms[0], arg)), arg));
                        if (reg_overlap(used, REG_XSP)) {
                            int xsp_disp = opnd_get_reg_dcontext_offs(REG_XSP) +
                                clean_call_beyond_mcontext() + total_stack;
                            POST(ilist, prev, INSTR_CREATE_mov_ld
                                 (dcontext, opnd_create_reg(xsp_scratch),
                                  OPND_CREATE_MEMPTR(REG_XSP, xsp_disp)));
                            if (xsp_scratch == REG_XAX) {
                                POST(ilist, prev, instr_create_save_to_tls
                                     (dcontext, REG_XAX, TLS_XAX_SLOT));
                            }
                        }
                        if (opnd_uses_reg(arg, regparms[0])) {
                            /* must restore since earlier arg might have clobbered */
                            int mc_disp = opnd_get_reg_dcontext_offs(regparms[0]) +
                                clean_call_beyond_mcontext() + total_stack;
                            POST(ilist, prev, INSTR_CREATE_mov_ld
                                 (dcontext, opnd_create_reg(regparms[0]),
                                  OPND_CREATE_MEMPTR(REG_XSP, mc_disp)));
                        }
                    }
                    arg_pre_push++; /* running counter */
                }
                arg = opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                            disp, opnd_get_size(arg));
                break; /* once we've handled arg ignore futher reg refs */
            }
        }
#endif

        if (i < NUM_REGPARM) {
            reg_id_t regparm = shrink_reg_for_param(regparms[i], arg);
            if (opnd_is_immed_int(arg) || opnd_is_instr(arg)) {
                POST(ilist, mark,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(regparm), arg));
            } else {
                POST(ilist, mark,
                    INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(regparm), arg));
            }
        } else {
            if (push) {
                IF_X64(ASSERT_NOT_REACHED()); /* no 64-bit push_imm! */
                if (opnd_is_immed_int(arg) || opnd_is_instr(arg))
                    POST(ilist, mark, INSTR_CREATE_push_imm(dcontext, arg));
                else {
                    if (clean_call && opnd_uses_reg(arg, REG_XSP)) {
                        /* We do a purely local expansion:
                         * spill eax, mc->eax, esp->eax, arg->eax, push eax, restore eax
                         */
                        reg_id_t scratch = REG_XAX;
                        if (opnd_uses_reg(arg, scratch)) {
                            scratch = REG_XCX;
                            ASSERT(!opnd_uses_reg(arg, scratch)); /* can't use 3 regs */
                        }
                        opnd_replace_reg(&arg, REG_XSP, scratch);
                        POST(ilist, mark, instr_create_restore_from_tls
                             (dcontext, scratch, TLS_XAX_SLOT));
                        POST(ilist, mark, INSTR_CREATE_push(dcontext, arg));
                        POST(ilist, mark, instr_create_restore_from_dc_via_reg
                            (dcontext, scratch, scratch, XSP_OFFSET));
                        insert_get_mcontext_base
                            (dcontext, ilist, instr_get_next(mark), scratch);
                        POST(ilist, mark, instr_create_save_to_tls
                             (dcontext, scratch, TLS_XAX_SLOT));
                    } else
                        POST(ilist, mark, INSTR_CREATE_push(dcontext, arg));
                }
            } else {
                /* xsp was adjusted up above; we simply store to xsp offsets */
                uint offs = REGPARM_MINSTACK + XSP_SZ * (i - NUM_REGPARM);
#ifdef X64
                if (opnd_is_immed_int(arg) || opnd_is_instr(arg)) {
                    /* PR 250976 #3: there is no memory store of 64-bit-immediate,
                     * so go through scratch reg */
                    ASSERT(NUM_REGPARM > 0);
                    POST(ilist, mark,
                         INSTR_CREATE_mov_st(dcontext,
                                             OPND_CREATE_MEMPTR(REG_XSP, offs),
                                             opnd_create_reg(regparms[0])));
                    POST(ilist, mark,
                         INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(regparms[0]),
                                              arg));
                } else {
#endif
                    if (opnd_is_memory_reference(arg)) {
                        /* can't do mem-to-mem so go through scratch */
                        reg_id_t scratch;
                        if (NUM_REGPARM > 0)
                            scratch = regparms[0];
                        else {
                            /* This happens on Mac.
                             * FIXME i#1370: not safe if later arg uses xax:
                             * local spill?  Review how regparms[0] is preserved.
                             */
                            scratch = REG_XAX;
                        }
                        POST(ilist, mark,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, offs),
                                                 opnd_create_reg(scratch)));
                        POST(ilist, mark,
                             INSTR_CREATE_mov_ld(dcontext, opnd_create_reg
                                                 (shrink_reg_for_param(scratch, arg)),
                                                 arg));
                    } else {
                        POST(ilist, mark,
                             INSTR_CREATE_mov_st(dcontext,
                                                 OPND_CREATE_MEMPTR(REG_XSP, offs), arg));
                    }
#ifdef X64
                }
#endif
            }
        }
    }
    if (!push && total_stack > 0) {
        POST(ilist, prev, /* before everything else: pre-push and args */
            /* can we use sub?  may as well preserve eflags */
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0,
                                                 -(int)total_stack)));
    }
    if (restore_xsp) {
        /* before restore_xax, since we're going to clobber xax */
        int disp = opnd_get_reg_dcontext_offs(REG_XSP);
        instr_t *where = instr_get_next(prev);
        /* skip rest of what prepare_for_clean_call adds */
        disp += clean_call_beyond_mcontext();
        insert_get_mcontext_base(dcontext, ilist, where, REG_XAX);
        PRE(ilist, where, instr_create_restore_from_dc_via_reg
            (dcontext, REG_XAX, REG_XAX, XSP_OFFSET));
        PRE(ilist, where,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEMPTR(REG_XSP, disp),
                                opnd_create_reg(REG_XAX)));
        /* now we need restore_xax to be AFTER this */
        prev = instr_get_prev(where);
    }
    if (restore_xax) {
        int disp = opnd_get_reg_dcontext_offs(REG_XAX);
        /* skip rest of what prepare_for_clean_call adds */
        disp += clean_call_beyond_mcontext();
        POST(ilist, prev, /* before everything else: pre-push, args, and stack adjust */
             INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                 OPND_CREATE_MEMPTR(REG_XSP, disp)));
    }
    return total_stack;
}

/* Inserts a complete call to callee with the passed-in arguments.
 * For x64, assumes the stack pointer is currently 16-byte aligned.
 * Clean calls ensure this by using clean base of dstack and having
 * dr_prepare_for_call pad to 16 bytes.
 * Returns whether the call is direct.
 */
bool
insert_meta_call_vargs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       bool clean_call, byte *encode_pc, void *callee,
                       uint num_args, opnd_t *args)
{
    instr_t *in = (instr == NULL) ? instrlist_last(ilist) : instr_get_prev(instr);
    bool direct;
    uint stack_for_params =
        insert_parameter_preparation(dcontext, ilist, instr,
                                     clean_call, num_args, args);
    IF_X64(ASSERT(ALIGNED(stack_for_params, 16)));
    /* If we need an indirect call, we use r11 as the last of the scratch regs.
     * We document this to clients using dr_insert_call_ex() or DR_CLEANCALL_INDIRECT.
     */
    direct = insert_reachable_cti(dcontext, ilist, instr, encode_pc, (byte *)callee,
                                  false/*call*/, false/*!precise*/, DR_REG_R11, NULL);
    if (stack_for_params > 0) {
        /* XXX PR 245936: let user decide whether to clean up?
         * i.e., support calling a stdcall routine?
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0,
                                                   stack_for_params, OPSZ_lea)));
    }
    /* mark it all meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != instr) {
        instr_set_meta(in);
        in = instr_get_next(in);
    }
    return direct;
}

/* If jmp_instr == NULL, uses jmp_tag, otherwise uses jmp_instr
 */
void
insert_clean_call_with_arg_jmp_if_ret_true(dcontext_t *dcontext,
            instrlist_t *ilist, instr_t *instr,  void *callee, int arg,
                                           app_pc jmp_tag, instr_t *jmp_instr)
{
    instr_t *false_popa, *jcc;
    prepare_for_clean_call(dcontext, NULL, ilist, instr);

    dr_insert_call(dcontext, ilist, instr, callee, 1, OPND_CREATE_INT32(arg));

    /* if the return value (xax) is 0, then jmp to internal false path */
    PRE(ilist,instr, /* can't cmp w/ 64-bit immed so use test (shorter anyway) */
        INSTR_CREATE_test(dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XAX)));
    /* fill in jcc target once have false path */
    jcc = INSTR_CREATE_jcc(dcontext, OP_jz, opnd_create_pc(NULL));
    PRE(ilist, instr, jcc);

    /* if it falls through, then it's true, so restore and jmp to true tag
     * passed in by caller
     */
    cleanup_after_clean_call(dcontext, NULL, ilist, instr);
    if (jmp_instr == NULL) {
        /* an exit cti, not a meta instr */
        instrlist_preinsert
            (ilist, instr, INSTR_CREATE_jmp(dcontext, opnd_create_pc(jmp_tag)));
    } else {
        PRE(ilist, instr,
            INSTR_CREATE_jmp(dcontext, opnd_create_instr(jmp_instr)));
    }

    /* otherwise (if returned false), just do standard popf and continue */
    /* get 1st instr of cleanup path */
    false_popa = instr_get_prev(instr);
    cleanup_after_clean_call(dcontext, NULL, ilist, instr);
    false_popa = instr_get_next(false_popa);
    instr_set_target(jcc, opnd_create_instr(false_popa));
}

/* If !precise, encode_pc is treated as +- a page (meant for clients
 * writing an instrlist to gencode so not sure of exact placement but
 * within a page).
 * If encode_pc == vmcode_get_start(), checks reachability of whole
 * vmcode region (meant for code going somewhere not precisely known
 * in the code cache).
 * Returns whether ended up using a direct cti.  If inlined_tgt_instr != NULL,
 * and an inlined target was used, returns a pointer to that instruction
 * in *inlined_tgt_instr.
 */
bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr)
{
    byte *encode_start;
    byte *encode_end;
    if (precise) {
        encode_start = target + JMP_LONG_LENGTH;
        encode_end = encode_start;
    } else if (encode_pc == vmcode_get_start()) {
        /* consider whole vmcode region */
        encode_start = encode_pc;
        encode_end = vmcode_get_end();
    } else {
        encode_start = (byte *) PAGE_START(encode_pc - PAGE_SIZE);
        encode_end = (byte *) ALIGN_FORWARD(encode_pc + PAGE_SIZE, PAGE_SIZE);
    }
    if (REL32_REACHABLE(encode_start, target) &&
        REL32_REACHABLE(encode_end, target)) {
        /* For precise, we could consider a short cti, but so far no
         * users are precise so we'll leave that for i#56.
         */
        if (jmp)
            PRE(ilist, where, INSTR_CREATE_jmp(dcontext, opnd_create_pc(target)));
        else
            PRE(ilist, where, INSTR_CREATE_call(dcontext, opnd_create_pc(target)));
        return true;
    } else {
        opnd_t ind_tgt;
        instr_t *inlined_tgt = NULL;
        if (scratch == DR_REG_NULL) {
            /* indirect through an inlined target */
            inlined_tgt = instr_build_bits(dcontext, OP_UNDECODED, sizeof(target));
            /* XXX: could use mov imm->xax and have target skip rex+opcode
             * for clean disassembly
             */
            instr_set_raw_bytes(inlined_tgt, (byte *) &target, sizeof(target));
            /* this will copy the bytes for us, so we don't have to worry about
             * the lifetime of the target param
             */
            instr_allocate_raw_bits(dcontext, inlined_tgt, sizeof(target));
            ind_tgt = opnd_create_mem_instr(inlined_tgt, 0, OPSZ_PTR);
            if (inlined_tgt_instr != NULL)
                *inlined_tgt_instr = inlined_tgt;
        } else {
            PRE(ilist, where, INSTR_CREATE_mov_imm
                (dcontext, opnd_create_reg(scratch), OPND_CREATE_INTPTR(target)));
            ind_tgt = opnd_create_reg(scratch);
            if (inlined_tgt_instr != NULL)
                *inlined_tgt_instr = NULL;
        }
        if (jmp)
            PRE(ilist, where, INSTR_CREATE_jmp_ind(dcontext, ind_tgt));
        else
            PRE(ilist, where, INSTR_CREATE_call_ind(dcontext, ind_tgt));
        if (inlined_tgt != NULL)
            PRE(ilist, where, inlined_tgt);
        return false;
    }
}

/*###########################################################################
 *###########################################################################
 *
 *   M A N G L I N G   R O U T I N E S
 */

/* If src_inst != NULL, uses it (and assumes it will be encoded at
 * encode_estimate to determine whether > 32 bits or not: so if unsure where
 * it will be encoded, pass a high address) as the immediate; else
 * uses val.
 */
static void
insert_mov_immed_common(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                        ptr_int_t val, opnd_t dst,
                        instrlist_t *ilist, instr_t *instr,
                        instr_t **first, instr_t **second)
{
    instr_t *mov1, *mov2;
    if (src_inst != NULL)
        val = (ptr_int_t) encode_estimate;
#ifdef X64
    if (X64_MODE_DC(dcontext) && !opnd_is_reg(dst)) {
        if (val <= INT_MAX && val >= INT_MIN) {
            /* mov is sign-extended, so we can use one move if it is all
             * 0 or 1 in top 33 bits
             */
            mov1 = INSTR_CREATE_mov_imm(dcontext, dst,
                                        (src_inst == NULL) ?
                                        OPND_CREATE_INT32((int)val) :
                                        opnd_create_instr_ex(src_inst, OPSZ_4, 0));
            PRE(ilist, instr, mov1);
            mov2 = NULL;
        } else {
            /* do mov-64-bit-immed in two pieces.  tiny corner-case risk of racy
             * access to [dst] if this thread is suspended in between or another
             * thread is trying to read [dst], but o/w we have to spill and
             * restore a register.
             */
            CLIENT_ASSERT(opnd_is_memory_reference(dst), "invalid dst opnd");
            /* mov low32 => [mem32] */
            opnd_set_size(&dst, OPSZ_4);
            mov1 = INSTR_CREATE_mov_st(dcontext, dst,
                                       (src_inst == NULL) ?
                                       OPND_CREATE_INT32((int)val) :
                                       opnd_create_instr_ex(src_inst, OPSZ_4, 0));
            PRE(ilist, instr, mov1);
            /* mov high32 => [mem32+4] */
            if (opnd_is_base_disp(dst)) {
                int disp = opnd_get_disp(dst);
                CLIENT_ASSERT(disp + 4 > disp, "disp overflow");
                opnd_set_disp(&dst, disp+4);
            } else {
                byte *addr = opnd_get_addr(dst);
                CLIENT_ASSERT(!POINTER_OVERFLOW_ON_ADD(addr, 4),
                              "addr overflow");
                dst = OPND_CREATE_ABSMEM(addr+4, OPSZ_4);
            }
            mov2 = INSTR_CREATE_mov_st(dcontext, dst,
                                       (src_inst == NULL) ?
                                       OPND_CREATE_INT32((int)(val >> 32)) :
                                       opnd_create_instr_ex(src_inst, OPSZ_4, 32));
            PRE(ilist, instr, mov2);
        }
    } else {
#endif
        mov1 = INSTR_CREATE_mov_imm(dcontext, dst,
                                    (src_inst == NULL) ?
                                    OPND_CREATE_INTPTR(val) :
                                    opnd_create_instr_ex(src_inst, OPSZ_4, 0));
        PRE(ilist, instr, mov1);
        mov2 = NULL;
#ifdef X64
    }
#endif
    if (first != NULL)
        *first = mov1;
    if (second != NULL)
        *second = mov2;
}

void
insert_mov_immed_ptrsz(dcontext_t *dcontext, ptr_int_t val, opnd_t dst,
                       instrlist_t *ilist, instr_t *instr,
                       instr_t **first, instr_t **second)
{
    insert_mov_immed_common(dcontext, NULL, NULL, val, dst,
                            ilist, instr, first, second);
}

void
insert_mov_instr_addr(dcontext_t *dcontext, instr_t *src, byte *encode_estimate,
                      opnd_t dst, instrlist_t *ilist, instr_t *instr,
                      instr_t **first, instr_t **second)
{
    insert_mov_immed_common(dcontext, src, encode_estimate, 0, dst,
                            ilist, instr, first, second);
}



/* If src_inst != NULL, uses it (and assumes it will be encoded at
 * encode_estimate to determine whether > 32 bits or not: so if unsure where
 * it will be encoded, pass a high address) as the immediate; else
 * uses val.
 */
static void
insert_push_immed_common(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                         ptr_int_t val, instrlist_t *ilist, instr_t *instr,
                         instr_t **first, instr_t **second)
{
    instr_t *push, *mov;
    if (src_inst != NULL)
        val = (ptr_int_t) encode_estimate;
#ifdef X64
    if (X64_MODE_DC(dcontext)) {
        /* do push-64-bit-immed in two pieces.  tiny corner-case risk of racy
         * access to TOS if this thread is suspended in between or another
         * thread is trying to read its stack, but o/w we have to spill and
         * restore a register.
         */
        push = INSTR_CREATE_push_imm(dcontext,
                                     (src_inst == NULL) ?
                                     OPND_CREATE_INT32((int)val) :
                                     opnd_create_instr_ex(src_inst, OPSZ_4, 0));
        PRE(ilist, instr, push);
        /* push is sign-extended, so we can skip top half if it is all 0 or 1
         * in top 33 bits
         */
        if (val <= INT_MAX && val >= INT_MIN) {
            mov = NULL;
        } else {
            mov = INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 4),
                                      (src_inst == NULL) ?
                                      OPND_CREATE_INT32((int)(val >> 32)) :
                                      opnd_create_instr_ex(src_inst, OPSZ_4, 32));
            PRE(ilist, instr, mov);
        }
    } else {
#endif
        push = INSTR_CREATE_push_imm(dcontext,
                                     (src_inst == NULL) ?
                                     OPND_CREATE_INT32(val) :
                                     opnd_create_instr_ex(src_inst, OPSZ_4, 0));
        PRE(ilist, instr, push);
        mov  = NULL;
#ifdef X64
    }
#endif
    if (first != NULL)
        *first = push;
    if (second != NULL)
        *second = mov;
}

void
insert_push_immed_ptrsz(dcontext_t *dcontext, ptr_int_t val,
                        instrlist_t *ilist, instr_t *instr,
                        instr_t **first, instr_t **second)
{
    insert_push_immed_common(dcontext, NULL, NULL, val,
                             ilist, instr, first, second);
}

void
insert_push_instr_addr(dcontext_t *dcontext, instr_t *src_inst, byte *encode_estimate,
                       instrlist_t *ilist, instr_t *instr,
                       instr_t **first, instr_t **second)
{
    insert_push_immed_common(dcontext, src_inst, encode_estimate, 0,
                             ilist, instr, first, second);
}

/* Far calls and rets have double total size */
static opnd_size_t
stack_entry_size(instr_t *instr, opnd_size_t opsize)
{
    if (instr_get_opcode(instr) == OP_call_far ||
        instr_get_opcode(instr) == OP_call_far_ind ||
        instr_get_opcode(instr) == OP_ret_far) {
        /* cut OPSZ_8_rex16_short4 in half */
        if (opsize == OPSZ_4)
            return OPSZ_2;
        else if (opsize == OPSZ_8)
            return OPSZ_4;
        else {
#ifdef X64
            ASSERT(opsize == OPSZ_16);
            return OPSZ_8;
#else
            ASSERT_NOT_REACHED();
#endif
        }
    } else if (instr_get_opcode(instr) == OP_iret) {
        /* convert OPSZ_12_rex40_short6 */
        if (opsize == OPSZ_6)
            return OPSZ_2;
        else if (opsize == OPSZ_12)
            return OPSZ_4;
        else {
#ifdef X64
            ASSERT(opsize == OPSZ_40);
            return OPSZ_8;
#else
            ASSERT_NOT_REACHED();
#endif
        }
    }
    return opsize;
}

/* Used for fault translation */
bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust)
{
    ASSERT(xsp_adjust != NULL);
    if (instr_get_opcode(inst) == OP_push ||
        instr_get_opcode(inst) == OP_push_imm) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: push or push_imm\n");
        *xsp_adjust -= opnd_size_in_bytes(opnd_get_size(instr_get_dst(inst, 1)));
    } else if (instr_get_opcode(inst) == OP_pop) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: pop\n");
        *xsp_adjust += opnd_size_in_bytes(opnd_get_size(instr_get_src(inst, 1)));
    }
    /* 1st part of push emulation from insert_push_retaddr */
    else if (instr_get_opcode(inst) == OP_lea &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_XSP &&
             opnd_get_base(instr_get_src(inst, 0)) == REG_XSP &&
             opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: lea xsp adjust\n");
        *xsp_adjust += opnd_get_disp(instr_get_src(inst, 0));
    }
    /* 2nd part of push emulation from insert_push_retaddr */
    else if (instr_get_opcode(inst) == OP_mov_st &&
             opnd_is_base_disp(instr_get_dst(inst, 0)) &&
             opnd_get_base(instr_get_dst(inst, 0)) == REG_XSP &&
             opnd_get_index(instr_get_dst(inst, 0)) == REG_NULL) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: store to stack\n");
        /* nothing to track: paired lea is what we undo */
    }
    /* retrieval of target for call* or jmp* */
    else if ((instr_get_opcode(inst) == OP_movzx &&
              reg_overlap(opnd_get_reg(instr_get_dst(inst, 0)), REG_XCX)) ||
             (instr_get_opcode(inst) == OP_mov_ld &&
              reg_overlap(opnd_get_reg(instr_get_dst(inst, 0)), REG_XCX))) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: ib tgt to *cx\n");
        /* nothing: our xcx spill restore will undo */
    }
    /* part of pop emulation for iretd/lretd in x64 mode */
    else if (instr_get_opcode(inst) == OP_mov_ld &&
             opnd_is_base_disp(instr_get_src(inst, 0)) &&
             opnd_get_base(instr_get_src(inst, 0)) == REG_XSP &&
             opnd_get_index(instr_get_src(inst, 0)) == REG_NULL) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: load from stack\n");
        /* nothing to track: paired lea is what we undo */
    }
    /* part of data16 ret.  once we have cs preservation (PR 271317) we'll
     * need to not fail when walking over a movzx to a pop cs (right now we
     * do not read the stack for the pop cs).
     */
    else if (instr_get_opcode(inst) == OP_movzx &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_CX) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: movzx to cx\n");
        /* nothing: our xcx spill restore will undo */
    }
    /* fake pop of cs for iret */
    else if (instr_get_opcode(inst) == OP_add &&
             opnd_is_reg(instr_get_dst(inst, 0)) &&
             opnd_get_reg(instr_get_dst(inst, 0)) == REG_XSP &&
             opnd_is_immed_int(instr_get_src(inst, 0))) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: add to xsp\n");
        ASSERT(CHECK_TRUNCATE_TYPE_int(opnd_get_immed_int(instr_get_src(inst, 0))));
        *xsp_adjust += (int) opnd_get_immed_int(instr_get_src(inst, 0));
    }
    /* popf for iret */
    else if (instr_get_opcode(inst) == OP_popf) {
        LOG(THREAD_GET, LOG_INTERP, 4, "\tstate track: popf\n");
        *xsp_adjust += opnd_size_in_bytes(opnd_get_size(instr_get_src(inst, 1)));
    } else {
        return false;
    }
    return true;
}

/* N.B.: keep in synch with instr_check_xsp_mangling() */
void
insert_push_retaddr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                    ptr_int_t retaddr, opnd_size_t opsize)
{
    if (opsize == OPSZ_2) {
        ptr_int_t val = retaddr & (ptr_int_t) 0x0000ffff;
        /* can't do a non-default operand size with a push immed so we emulate */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, -2,
                                                   OPSZ_lea)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM16(REG_XSP, 2),
                                OPND_CREATE_INT16(val)));
    } else if (opsize == OPSZ_PTR
               IF_X64(|| (!X64_CACHE_MODE_DC(dcontext) && opsize == OPSZ_4))) {
        insert_push_immed_ptrsz(dcontext, retaddr, ilist, instr, NULL, NULL);
    } else {
#ifdef X64
        ptr_int_t val = retaddr & (ptr_int_t) 0xffffffff;
        ASSERT(opsize == OPSZ_4);
        /* can't do a non-default operand size with a push immed so we emulate */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, -4,
                                                   OPSZ_lea)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 0),
                                OPND_CREATE_INT32((int)val)));
#else
        ASSERT_NOT_REACHED();
#endif
    }
}

#ifdef CLIENT_INTERFACE
/* N.B.: keep in synch with instr_check_xsp_mangling() */
static void
insert_mov_ptr_uint_beyond_TOS(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                               ptr_int_t value, opnd_size_t opsize)
{
    /* we insert non-meta b/c we want faults to go to app (should only fault
     * if the ret itself faulted, barring races) for simplicity: o/w our
     * our-mangling sequence gets broken up and more complex.
     */
    if (opsize == OPSZ_2) {
        ptr_int_t val = value & (ptr_int_t) 0x0000ffff;
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM16(REG_XSP, -2),
                                OPND_CREATE_INT16(val)));
    } else if (opsize == OPSZ_4) {
        ptr_int_t val = value & (ptr_int_t) 0xffffffff;
        PRE(ilist, instr,
            INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, -4),
                                OPND_CREATE_INT32(val)));
    } else {
# ifdef X64
        ptr_int_t val_low = value & (ptr_int_t) 0xffffffff;
        ASSERT(opsize == OPSZ_8);
        if (CHECK_TRUNCATE_TYPE_int(value)) {
            /* prefer a single write w/ sign-extension */
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM64(REG_XSP, -8),
                                    OPND_CREATE_INT32(val_low)));
        } else {
            /* we need two 32-bit writes */
            ptr_int_t val_high = (value >> 32);
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, -8),
                                    OPND_CREATE_INT32(val_low)));
            PRE(ilist, instr,
                INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, -4),
                                    OPND_CREATE_INT32(val_high)));
        }
# else
        ASSERT_NOT_REACHED();
# endif
    }
}
#endif /* CLIENT_INTERFACE */

static void
insert_push_cs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               ptr_int_t retaddr, opnd_size_t opsize)
{
#ifdef X64
    if (X64_CACHE_MODE_DC(dcontext)) {
        /* "push cs" is invalid; for now we push the typical cs values.
         * i#823 covers doing this more generally.
         */
        insert_push_retaddr(dcontext, ilist, instr,
                            X64_MODE_DC(dcontext) ? CS64_SELECTOR : CS32_SELECTOR, opsize);
    } else {
#endif
        opnd_t stackop;
        /* we go ahead and push cs, but we won't pop into cs */
        instr_t *push = INSTR_CREATE_push(dcontext, opnd_create_reg(SEG_CS));
        /* 2nd dest is the stack operand size */
        stackop = instr_get_dst(push, 1);
        opnd_set_size(&stackop, opsize);
        instr_set_dst(push, 1, stackop);
        PRE(ilist, instr, push);
#ifdef X64
    }
#endif
}

ptr_uint_t
get_call_return_address(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    ptr_uint_t retaddr, curaddr;

    ASSERT(instr_is_call(instr));
#ifdef CLIENT_INTERFACE
    /* i#620: provide API to set fall-through and retaddr targets at end of bb */
    if (instrlist_get_return_target(ilist) != NULL) {
        retaddr = (ptr_uint_t)instrlist_get_return_target(ilist);
        LOG(THREAD, LOG_INTERP, 3, "set return target "PFX" by client\n", retaddr);
        return retaddr;
    }
#endif
    /* For CI builds, use the translation field so we can handle cases
     * where the client has changed the target and invalidated the raw
     * bits.  We'll make sure the translation is always set for direct
     * calls.
     *
     * If a client changes an instr, or our own mangle_rel_addr() does,
     * the raw bits won't be valid but the translation should be.
     */
    curaddr = (ptr_uint_t) instr_get_translation(instr);
    if (curaddr == 0 && instr_raw_bits_valid(instr))
        curaddr = (ptr_uint_t) instr_get_raw_bits(instr);
    ASSERT(curaddr != 0);
    /* we use the next app instruction as return address as the client
     * or DR may change the instruction and so its length.
     */
    if (instr_raw_bits_valid(instr) &&
        instr_get_translation(instr) == instr_get_raw_bits(instr)) {
        /* optimization, if nothing changes, use instr->length to avoid
         * calling decode_next_pc.
         */
        retaddr = curaddr + instr->length;
    } else {
        retaddr = (ptr_uint_t) decode_next_pc(dcontext, (byte *)curaddr);
    }
    return retaddr;
}

/* We spill to XCX(private dcontext) slot for private fragments,
 * and to TLS MANGLE_XCX_SPILL_SLOT for shared fragments.
 * (Except for DYNAMO_OPTION(private_ib_in_tls), for which all use tls,
 *  but that has a performance hit because of the extra data cache line)
 * We can get away with the split by having the shared ibl routine copy
 * xcx to the private dcontext, and by having the private ibl never
 * target shared fragments.
 * We also have to modify the xcx spill from tls to private dcontext when
 * adding a shared basic block to a trace.
 *
 * FIXME: if we do make non-trace-head basic blocks valid indirect branch
 * targets, we should have the private ibl have special code to test the
 * flags and copy xcx to the tls slot if necessary.
 */
#define SAVE_TO_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs)                      \
    ((DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, (flags))) ?           \
     instr_create_save_to_tls(dc, reg, tls_offs) :                                \
     instr_create_save_to_dcontext((dc), (reg), (dc_offs)))

#define SAVE_TO_DC_OR_TLS_OR_REG(dc, flags, reg, tls_offs, dc_offs, dest_reg)   \
    ((X64_CACHE_MODE_DC(dc) && !X64_MODE_DC(dc)                                 \
      IF_X64(&& DYNAMO_OPTION(x86_to_x64_ibl_opt))) ?                           \
     INSTR_CREATE_mov_ld(dc, opnd_create_reg(dest_reg), opnd_create_reg(reg)) : \
     SAVE_TO_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs))

#define RESTORE_FROM_DC_OR_TLS(dc, flags, reg, tls_offs, dc_offs)                 \
    ((DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, (flags))) ?           \
     instr_create_restore_from_tls(dc, reg, tls_offs) :                           \
     instr_create_restore_from_dcontext((dc), (reg), (dc_offs)))

static void
mangle_far_direct_helper(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         instr_t *next_instr, uint flags)
{
    /* FIXME i#823: we do not support other than flat 0-based CS, DS, SS, and ES.
     * If the app wants to change segments in a WOW64 process, we will
     * do the right thing for standard cs selector values (xref i#49).
     * For other cs changes or in other modes, we do go through far_ibl
     * today although we do not enact the cs change (nor bother to pass
     * the selector in xbx).
     *
     * For WOW64, I tried keeping this a direct jmp for nice linking by doing the
     * mode change in-fragment and then using a 64-bit stub with a 32-bit fragment,
     * but that gets messy b/c a lot of code assumes it can create or calculate the
     * size of exit stubs given nothing but the fragment flags.  I tried adding
     * FRAG_ENDS_IN_FAR_DIRECT but still need to pass another param to all the stub
     * macros and routines for mid-trace exits and for prefixes for -disable_traces.
     * So, going for treating as indirect and using the far_ibl.  It's a trace
     * barrier anyway, and rare.  We treat it as indirect in all modes (including
     * x86 builds) for simplicity (and eventually for full i#823 we'll want
     * to issue cs changes there too).
     */
    app_pc pc = opnd_get_pc(instr_get_target(instr));

#ifdef X64
    if (!X64_MODE_DC(dcontext) &&
        opnd_get_segment_selector(instr_get_target(instr)) == CS64_SELECTOR) {
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XBX,
                                     MANGLE_FAR_SPILL_SLOT, XBX_OFFSET, REG_R10));
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                                 OPND_CREATE_INT32(CS64_SELECTOR)));
    }
#endif

    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX,
                                 MANGLE_XCX_SPILL_SLOT, XCX_OFFSET, REG_R9));
    ASSERT((ptr_uint_t)pc < UINT_MAX); /* 32-bit code! */
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX),
                             OPND_CREATE_INT32((ptr_uint_t)pc)));
}

/***************************************************************************
 * DIRECT CALL
 * Returns new next_instr
 */
static instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags)
{
    ptr_uint_t retaddr;
    app_pc target = NULL;
    opnd_t pushop = instr_get_dst(instr, 1);
    opnd_size_t pushsz = stack_entry_size(instr, opnd_get_size(pushop));
    if (opnd_is_near_pc(instr_get_target(instr)))
        target = opnd_get_pc(instr_get_target(instr));
    else if (opnd_is_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        /* assumption: target's raw bits are meaningful */
        target = instr_get_raw_bits(tgt);
        ASSERT(target != 0);
        /* FIXME case 6962: for far instr, we ignore the segment and
         * assume it matches current cs */
    } else if (opnd_is_far_pc(instr_get_target(instr))) {
        target = opnd_get_pc(instr_get_target(instr));
        /* FIXME case 6962: we ignore the segment and assume it matches current cs */
    } else
        ASSERT_NOT_REACHED();

    if (!mangle_calls) {
        /* off-trace call that will be executed natively */

        /* relative target must be re-encoded */
        instr_set_raw_bits_valid(instr, false);

#ifdef STEAL_REGISTER
        /* FIXME: need to push edi prior to call and pop after.
         * However, need to push edi prior to any args to this call,
         * and it may be hard to find pre-arg-pushing spot...
         * edi is supposed to be callee-saved, we're trusting this
         * off-trace call to return, we may as well trust it to
         * not trash edi -- these no-inline calls are dynamo's
         * own routines, after all.
         */
#endif
        return next_instr;
    }

    retaddr = get_call_return_address(dcontext, ilist, instr);

#ifdef CHECK_RETURNS_SSE2
    /* ASSUMPTION: a call to the next instr is not going to ever have a
     * matching ret! */
    if (target == (app_pc)retaddr) {
        LOG(THREAD, LOG_INTERP, 3, "found call to next instruction "PFX"\n", target);
    } else {
        check_return_handle_call(dcontext, ilist, next_instr);
    }
    /* now do the normal thing for a call */
#endif

    if (instr_get_opcode(instr) == OP_call_far) {
        /* N.B.: we do not support other than flat 0-based CS, DS, SS, and ES.
         * if the app wants to change segments, we won't actually issue
         * a segment change, and so will only work properly if the new segment
         * is also 0-based.  To properly issue new segments, we'd need a special
         * ibl that ends in a far cti, and all prior address manipulations would
         * need to be relative to the new segment, w/o messing up current segment.
         * FIXME: can we do better without too much work?
         * XXX: yes, for wow64: i#823: TODO mangle this like a far direct jmp
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct call");
        STATS_INC(num_far_dir_calls);

        mangle_far_direct_helper(dcontext, ilist, instr, next_instr, flags);

        insert_push_cs(dcontext, ilist, instr, 0, pushsz);
    }

    /* convert a direct call to a push of the return address */
    insert_push_retaddr(dcontext, ilist, instr, retaddr, pushsz);

    /* remove the call */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return next_instr;
}


#ifdef UNIX
/***************************************************************************
 * Mangle the memory reference operand that uses fs/gs semgents,
 * get the segment base of fs/gs into reg, and
 * replace oldop with newop using reg instead of fs/gs
 * The reg must not be used in the oldop, otherwise, the reg value
 * is corrupted.
 */
static opnd_t
mangle_seg_ref_opnd(dcontext_t *dcontext, instrlist_t *ilist,
                    instr_t *where, opnd_t oldop, reg_id_t reg)
{
    opnd_t newop;
    reg_id_t seg;

    ASSERT(opnd_is_far_base_disp(oldop));
    seg = opnd_get_segment(oldop);
    /* we only mangle fs/gs */
    if (seg != SEG_GS && seg != SEG_FS)
        return oldop;
#ifdef CLIENT_INTERFACE
    if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
        return oldop;
#endif
    /* The reg should not be used by the oldop */
    ASSERT(!opnd_uses_reg(oldop, reg));

    /* XXX: this mangling is pattern-matched in translation's instr_is_seg_ref_load() */
    /* get app's segment base into reg. */
    PRE(ilist, where,
        instr_create_restore_from_tls(dcontext, reg,
                                      os_get_app_seg_base_offset(seg)));
    if (opnd_get_index(oldop) != REG_NULL &&
        opnd_get_base(oldop) != REG_NULL) {
        /* if both base and index are used, use
         * lea [base, reg, 1] => reg
         * to get the base + seg_base into reg.
         */
        PRE(ilist, where,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(reg),
                             opnd_create_base_disp(opnd_get_base(oldop),
                                                   reg, 1, 0, OPSZ_lea)));
    }
    if (opnd_get_index(oldop) != REG_NULL) {
        newop = opnd_create_base_disp(reg,
                                      opnd_get_index(oldop),
                                      opnd_get_scale(oldop),
                                      opnd_get_disp(oldop),
                                      opnd_get_size(oldop));
    } else {
        newop = opnd_create_base_disp(opnd_get_base(oldop),
                                      reg, 1,
                                      opnd_get_disp(oldop),
                                      opnd_get_size(oldop));
    }
    return newop;
}
#endif /* UNIX */

/***************************************************************************
 * INDIRECT CALL
 */

static reg_id_t
mangle_far_indirect_helper(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                           instr_t *next_instr, uint flags, opnd_t *target_out)
{
    opnd_t target = *target_out;
    opnd_size_t addr_size;
    reg_id_t reg_target = REG_NULL;
    ASSERT(instr_get_opcode(instr) == OP_jmp_far_ind ||
           instr_get_opcode(instr) == OP_call_far_ind);
    /* FIXME i#823: we do not support other than flat 0-based CS, DS, SS, and ES.
     * If the app wants to change segments in a WOW64 process, we will
     * do the right thing for standard cs selector values (xref i#49).
     * For other cs changes or in other modes, we do go through far_ibl
     * today although we do not enact the cs change (nor bother to pass
     * the selector in xbx).
     */
    /* opnd type is i_Ep, it's not a far base disp b/c segment is at
     * memory location, not specified as segment prefix on instr
     * we assume register operands are marked as invalid instrs long
     * before this point.
     */
    ASSERT(opnd_is_base_disp(target));
    /* Segment selector is the final 2 bytes.
     * For non-mixed-mode, we ignore it.
     * We assume DS base == target cti CS base.
     */
    /* if data16 then just 2 bytes for address
     * if x64 mode and Intel and rex then 8 bytes for address */
    ASSERT((X64_MODE_DC(dcontext) && opnd_get_size(target) == OPSZ_10 &&
            proc_get_vendor() != VENDOR_AMD) ||
           opnd_get_size(target) == OPSZ_6 || opnd_get_size(target) == OPSZ_4);
    if (opnd_get_size(target) == OPSZ_10) {
        addr_size = OPSZ_8;
        reg_target = REG_RCX;
    } else if (opnd_get_size(target) == OPSZ_6) {
        addr_size = OPSZ_4;
        reg_target = REG_ECX;
    } else /* target has OPSZ_4 */ {
        addr_size = OPSZ_2;
        reg_target = REG_XCX; /* caller uses movzx so size doesn't have to match */
    }
#ifdef X64
    if (mixed_mode_enabled()) {
        /* While we don't support arbitrary segments, we do support
         * mode changes using standard cs selector values (i#823).
         * We save the selector into xbx.
         */
        opnd_t sel = target;
        opnd_set_disp(&sel, opnd_get_disp(target) + opnd_size_in_bytes(addr_size));
        opnd_set_size(&sel, OPSZ_2);

        /* all scratch space should be in TLS only */
        ASSERT(TEST(FRAG_SHARED, flags) || DYNAMO_OPTION(private_ib_in_tls));
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XBX,
                                     MANGLE_FAR_SPILL_SLOT, XBX_OFFSET, REG_R10));
        PRE(ilist, instr,
            INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EBX), sel));
        if (instr_uses_reg(instr, REG_XBX)) {
            /* instr can't be both riprel (uses xax slot for mangling) and use
             * a register, so we spill to the riprel (== xax) slot
             */
            PRE(ilist, instr,
                SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XBX, MANGLE_RIPREL_SPILL_SLOT,
                                  XAX_OFFSET));
            POST(ilist, instr,
                 instr_create_restore_from_tls(dcontext, REG_XBX,
                                               MANGLE_RIPREL_SPILL_SLOT));
        }
    }
#endif
    opnd_set_size(target_out, addr_size);
    return reg_target;
}

static void
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags)
{
    opnd_t target;
    ptr_uint_t retaddr;
    opnd_t pushop = instr_get_dst(instr, 1);
    opnd_size_t pushsz = stack_entry_size(instr, opnd_get_size(pushop));
    reg_id_t reg_target = REG_XCX;

    if (!mangle_calls)
        return;
    retaddr = get_call_return_address(dcontext, ilist, instr);

    /* Convert near, indirect calls.  The jump to the exit_stub that
     * jumps to indirect_branch_lookup was already inserted into the
     * instr list by interp EXCEPT for the case in which we're converting
     * an indirect call to a direct call. In that case, mangle later
     * inserts a direct exit stub.
     */
    /* If this call is marked for conversion, do minimal processing.
     * FIXME Just a note that converted calls are not subjected to any of
     * the specialized builds' processing further down.
     */
    if (TEST(INSTR_IND_CALL_DIRECT, instr->flags)) {
        /* convert the call to a push of the return address */
        insert_push_retaddr(dcontext, ilist, instr, retaddr, pushsz);
        /* remove the call */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        return;
    }

    /* put the push AFTER the instruction that calculates
     * the target, b/c if target depends on xsp we must use
     * the value of xsp prior to this call instruction!
     * we insert before next_instr to accomplish this.
     */
    if (instr_get_opcode(instr) == OP_call_far_ind) {
        /* goes right before the push of the ret addr */
        insert_push_cs(dcontext, ilist, next_instr, 0, pushsz);
        /* see notes below -- we don't really support switching segments,
         * though we do go ahead and push cs, we won't pop into cs
         */
    }
    insert_push_retaddr(dcontext, ilist, next_instr, retaddr, pushsz);

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX,
                                 MANGLE_XCX_SPILL_SLOT, XCX_OFFSET, REG_R9));

#ifdef STEAL_REGISTER
    /* Steal edi if call uses it, using original call instruction */
    steal_reg(dcontext, instr, ilist);
    if (ilist->flags)
        restore_state(dcontext, next_instr, ilist);
    /* It's impossible for our register stealing to use ecx
     * because no call can simultaneously use 3 registers, right?
     * Maximum is 2, in something like "call *(edi,ecx,4)"?
     * If it is possible, need to make sure stealing's use of ecx
     * doesn't conflict w/ our use
     */
#endif

    /* change: call /2, Ev -> movl Ev, %xcx */
    target = instr_get_src(instr, 0);
    if (instr_get_opcode(instr) == OP_call_far_ind) {
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far indirect call");
        STATS_INC(num_far_ind_calls);
        reg_target = mangle_far_indirect_helper(dcontext, ilist, instr,
                                                next_instr, flags, &target);
    }
#ifdef UNIX
    /* i#107, mangle the memory reference opnd that uses segment register. */
    if (INTERNAL_OPTION(mangle_app_seg) && opnd_is_far_base_disp(target)) {
        /* FIXME: we use REG_XCX to store the segment base, which might be used
         * in target and cause assertion failure in mangle_seg_ref_opnd.
         */
        ASSERT_BUG_NUM(107, !opnd_uses_reg(target, REG_XCX));
        target = mangle_seg_ref_opnd(dcontext, ilist, instr, target, REG_XCX);
    }
#endif
    /* cannot call instr_reset, it will kill prev & next ptrs */
    instr_free(dcontext, instr);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_opcode(instr, opnd_get_size(target) == OPSZ_2 ? OP_movzx : OP_mov_ld);
    instr_set_dst(instr, 0, opnd_create_reg(reg_target));
    instr_set_src(instr, 0, target); /* src stays the same */
    if (instrlist_get_translation_target(ilist) != NULL) {
        /* make sure original raw bits are used for translation */
        instr_set_translation(instr, instr_get_raw_bits(instr));
    }
    instr_set_our_mangling(instr, true);

#ifdef CHECK_RETURNS_SSE2
    check_return_handle_call(dcontext, ilist, next_instr);
#endif
}

/***************************************************************************
 * RETURN
 */

#ifdef X64
/* Saves the selector from the top of the stack into xbx, after spilling xbx,
 * for far_ibl.
 */
static void
mangle_far_return_save_selector(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                uint flags)
{
    if (mixed_mode_enabled()) {
        /* While we don't support arbitrary segments, we do support
         * mode changes using standard cs selector values (i#823).
         * We save the selector into xbx.
         */
        /* We could do a pop but state xl8 is already set up to restore lea */
        /* all scratch space should be in TLS only */
        ASSERT(TEST(FRAG_SHARED, flags) || DYNAMO_OPTION(private_ib_in_tls));
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XBX,
                                     MANGLE_FAR_SPILL_SLOT, XBX_OFFSET, REG_R10));
        PRE(ilist, instr,
            INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EBX),
                               OPND_CREATE_MEM16(REG_XSP, 0)));
    }
}
#endif

static void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags)
{
    instr_t *pop;
    opnd_t retaddr;
    opnd_size_t retsz;

#ifdef CHECK_RETURNS_SSE2
    check_return_handle_return(dcontext, ilist, next_instr);
    /* now do the normal ret mangling */
#endif

    /* Convert returns.  If aggressive we could take advantage of the
     * fact that xcx is dead at the return and not bother saving it?
     * The jump to the exit_stub that jumps to indirect_branch_lookup
     * was already inserted into the instr list by interp. */

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX,
                                 MANGLE_XCX_SPILL_SLOT, XCX_OFFSET, REG_R9));

    /* see if ret has an immed int operand, assumed to be 1st src */

    if (instr_num_srcs(instr) > 0 && opnd_is_immed_int(instr_get_src(instr, 0))) {
        /* if has an operand, return removes some stack space,
         * AFTER the return address is popped
         */
        int val = (int) opnd_get_immed_int(instr_get_src(instr, 0));
        IF_X64(ASSERT_TRUNCATE(val, int, opnd_get_immed_int(instr_get_src(instr, 0))));
        /* addl sizeof_param_area, %xsp
         * except that clobbers the flags, so we use lea */
        PRE(ilist, next_instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, val, OPSZ_lea)));
    }

    /* don't need to steal edi since return cannot use registers */

    /* the retaddr operand is always the final source for all OP_ret* instrs */
    retaddr = instr_get_src(instr, instr_num_srcs(instr) - 1);
    retsz = stack_entry_size(instr, opnd_get_size(retaddr));

    if (X64_CACHE_MODE_DC(dcontext) && retsz == OPSZ_4) {
        if (instr_get_opcode(instr) == OP_iret || instr_get_opcode(instr) == OP_ret_far) {
            /* N.B.: For some unfathomable reason iret and ret_far default to operand
             * size 4 in 64-bit mode (making them, along w/ call_far, the only stack
             * operation instructions to do so). So if we see an iret or far ret with
             * OPSZ_4 in 64-bit mode we need a 4-byte pop, but since we can't actually
             * generate a 4-byte pop we have to emulate it here. */
            SYSLOG_INTERNAL_WARNING_ONCE("Encountered iretd/lretd in 64-bit mode!");
        }
        /* Note moving into ecx automatically zero extends which is what we want. */
        PRE(ilist, instr,
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                                OPND_CREATE_MEM32(REG_RSP, 0)));
        /* iret could use add since going to pop the eflags, but not lret.
         * lret could combine w/ segment lea below: but not perf-crit instr, and
         * anticipating cs preservation PR 271317 I'm leaving separate. */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp(REG_XSP, REG_NULL, 0, 4, OPSZ_lea)));
    } else {
        /* change RET into a POP, keeping the operand size */
        opnd_t memop = retaddr;
        pop = INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_XCX));
        /* need per-entry size, not total size (double for far ret) */
        opnd_set_size(&memop, retsz);
        instr_set_src(pop, 1, memop);
        if (retsz == OPSZ_2)
            instr_set_dst(pop, 0, opnd_create_reg(REG_CX));
        /* We can't do a 4-byte pop in 64-bit mode, but excepting iretd and lretd
         * handled above we should never see one. */
        ASSERT(!X64_MODE_DC(dcontext) || retsz != OPSZ_4);
        PRE(ilist, instr, pop);
        if (retsz == OPSZ_2) {
            /* we need to zero out the top 2 bytes */
            PRE(ilist, instr, INSTR_CREATE_movzx
                (dcontext,
                 opnd_create_reg(REG_ECX), opnd_create_reg(REG_CX)));
        }
    }

#ifdef CLIENT_INTERFACE
    if (TEST(INSTR_CLOBBER_RETADDR, instr->flags)) {
        /* we put the value in the note field earlier */
        ptr_uint_t val = (ptr_uint_t) instr->note;
        insert_mov_ptr_uint_beyond_TOS(dcontext, ilist, instr, val, retsz);
    }
#endif

    if (instr_get_opcode(instr) == OP_ret_far) {
        /* FIXME i#823: we do not support other than flat 0-based CS, DS, SS, and ES.
         * If the app wants to change segments in a WOW64 process, we will
         * do the right thing for standard cs selector values (xref i#49).
         * For other cs changes or in other modes, we do go through far_ibl
         * today although we do not enact the cs change (nor bother to pass
         * the selector in xbx).
         */
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far ret");
        STATS_INC(num_far_rets);
#ifdef X64
        mangle_far_return_save_selector(dcontext, ilist, instr, flags);
#endif
        /* pop selector from stack, but not into cs, just junk it
         * (the 16-bit selector is expanded to 32 bits on the push, unless data16)
         */
        PRE(ilist, instr,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                             opnd_create_base_disp
                             (REG_XSP, REG_NULL, 0,
                              opnd_size_in_bytes(retsz), OPSZ_lea)));
    }

    if (instr_get_opcode(instr) == OP_iret) {
        instr_t *popf;

        /* Xref PR 215553 and PR 191977 - we actually see this on 64-bit Vista */
        LOG(THREAD, LOG_INTERP, 2, "Encountered iret at "PFX" - mangling\n",
            instr_get_translation(instr));
        STATS_INC(num_irets);

        /* In 32-bit mode this is a pop->EIP pop->CS pop->eflags.
         * 64-bit mode (with either 32-bit or 64-bit operand size,
         * despite the (wrong) Intel manual pseudocode: see i#833 and
         * the win32.mixedmode test) extends
         * the above and additionally adds pop->RSP pop->ss.  N.B.: like OP_far_ret we
         * ignore the CS (except mixed-mode WOW64) and SS segment changes
         * (see the comments there).
         */

#ifdef X64
        mangle_far_return_save_selector(dcontext, ilist, instr, flags);
#endif
        /* Return address is already popped, next up is CS segment which we ignore
         * (unless in mixed-mode, handled above) so
         * adjust stack pointer. Note we can use an add here since the eflags will
         * be written below. */
        PRE(ilist, instr,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_INT8
                             (opnd_size_in_bytes(retsz))));

        /* Next up is xflags, we use a popf. Popf should be setting the right flags
         * (it's difficult to tell because in the docs iret lists the flags it does
         * set while popf lists the flags it doesn't set). The docs aren't entirely
         * clear, but any flag that we or a user mode program would care about should
         * be right. */
        popf = INSTR_CREATE_popf(dcontext);
        if (X64_CACHE_MODE_DC(dcontext) && retsz == OPSZ_4) {
            /* We can't actually create a 32-bit popf and there's no easy way to
             * simulate one.  For now we'll do a 64-bit popf and fixup the stack offset.
             * If AMD/INTEL ever start using the top half of the rflags register then
             * we could have problems here. We could also break stack transparency and
             * do a mov, push, popf to zero extend the value. */
            PRE(ilist, instr, popf);
            /* flags are already set, must use lea to fix stack */
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XSP),
                                 opnd_create_base_disp
                                 (REG_XSP, REG_NULL, 0, -4, OPSZ_lea)));
        } else {
            /* get popf size right the same way we do it for the return address */
            opnd_t memop = retaddr;
            opnd_set_size(&memop, retsz);
            DOCHECK(1, if (retsz == OPSZ_2)
                        ASSERT_NOT_TESTED(););
            instr_set_src(popf, 1, memop);
            PRE(ilist, instr, popf);
        }

#ifdef X64
        /* In x64 mode, iret additionally does pop->RSP and pop->ss. */
        if (X64_MODE_DC(dcontext)) {
            if (retsz == OPSZ_8)
                PRE(ilist, instr, INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_RSP)));
            else if (retsz == OPSZ_4) {
                PRE(ilist, instr, INSTR_CREATE_mov_ld
                    (dcontext, opnd_create_reg(REG_ESP), OPND_CREATE_MEM32(REG_RSP, 0)));
            } else {
                ASSERT_NOT_TESTED();
                PRE(ilist, instr, INSTR_CREATE_movzx
                    (dcontext, opnd_create_reg(REG_ESP), OPND_CREATE_MEM16(REG_RSP, 0)));
            }
            /* We're ignoring the set of SS and since we just set RSP we don't need
             * to do anything to adjust the stack for the pop (since the pop would have
             * occurred with the old RSP). */
        }
#endif
    }

    /* remove the ret */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
}

/***************************************************************************
 * INDIRECT JUMP
 */
static void
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags)
{
    opnd_t target;
    reg_id_t reg_target = REG_XCX;

    /* Convert indirect branches (that are not returns).  Again, the
     * jump to the exit_stub that jumps to indirect_branch_lookup
     * was already inserted into the instr list by interp. */

    /* save away xcx so that we can use it */
    /* (it's restored in x86.s (indirect_branch_lookup) */
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS_OR_REG(dcontext, flags, REG_XCX,
                                 MANGLE_XCX_SPILL_SLOT, XCX_OFFSET, REG_R9));

#ifdef STEAL_REGISTER
    /* Steal edi if branch uses it, using original instruction */
    steal_reg(dcontext, instr, ilist);
    if (ilist->flags)
        restore_state(dcontext, next_instr, ilist);
#endif

    /* change: jmp /4, i_Ev -> movl i_Ev, %xcx */
    target = instr_get_target(instr);
    if (instr_get_opcode(instr) == OP_jmp_far_ind) {
        SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far indirect jump");
        STATS_INC(num_far_ind_jmps);
        reg_target = mangle_far_indirect_helper(dcontext, ilist, instr,
                                                next_instr, flags, &target);
    }
#ifdef UNIX
    /* i#107, mangle the memory reference opnd that uses segment register. */
    if (INTERNAL_OPTION(mangle_app_seg) && opnd_is_far_base_disp(target)) {
        /* FIXME: we use REG_XCX to store segment base, which might be used
         * in target and cause assertion failure in mangle_seg_ref_opnd.
         */
        ASSERT_BUG_NUM(107, !opnd_uses_reg(target, REG_XCX));
        target = mangle_seg_ref_opnd(dcontext, ilist, instr, target, REG_XCX);
    }
#endif
    /* cannot call instr_reset, it will kill prev & next ptrs */
    instr_free(dcontext, instr);
    instr_set_num_opnds(dcontext, instr, 1, 1);
    instr_set_opcode(instr, opnd_get_size(target) == OPSZ_2 ? OP_movzx : OP_mov_ld);
    instr_set_dst(instr, 0, opnd_create_reg(reg_target));
    instr_set_src(instr, 0, target); /* src stays the same */
    if (instrlist_get_translation_target(ilist) != NULL) {
        /* make sure original raw bits are used for translation */
        instr_set_translation(instr, instr_get_raw_bits(instr));
    }
    instr_set_our_mangling(instr, true);

    /* It's impossible for our register stealing to use ecx
     * because no branch can simultaneously use 3 registers, right?
     * Maximum is 2, in something like "jmp *(edi,ebx,4)"?
     * If it is possible, need to make sure stealing's use of ecx
     * doesn't conflict w/ our use = FIXME
     */
}

/***************************************************************************
 * FAR DIRECT JUMP
 */
static void
mangle_far_direct_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       instr_t *next_instr, uint flags)
{
    SYSLOG_INTERNAL_WARNING_ONCE("Encountered a far direct jmp");
    STATS_INC(num_far_dir_jmps);

    mangle_far_direct_helper(dcontext, ilist, instr, next_instr, flags);
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
}

/***************************************************************************
 * SYSCALL
 */
#ifdef CLIENT_INTERFACE
static bool
cti_is_normal_elision(instr_t *instr)
{
    instr_t *next;
    opnd_t   tgt;
    app_pc   next_pc;
    if (instr == NULL || instr_is_meta(instr))
        return false;
    if (!instr_is_ubr(instr) && !instr_is_call_direct(instr))
        return false;
    next = instr_get_next(instr);
    if (next == NULL || instr_is_meta(next))
        return false;
    tgt = instr_get_target(instr);
    next_pc = instr_get_translation(next);
    if (next_pc == NULL && instr_raw_bits_valid(next))
        next_pc = instr_get_raw_bits(next);
    if (opnd_is_pc(tgt) && next_pc != NULL && opnd_get_pc(tgt) == next_pc)
        return true;
    return false;
}
#endif

/* Tries to statically find the syscall number for the
 * syscall instruction instr.
 * Returns -1 upon failure.
 * Note that on MacOS, 32-bit Mach syscalls are encoded using negative numbers
 * (although -1 is invalid), so be sure to test for -1 and not just <0 as a failure
 * code.
 */
int
find_syscall_num(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    int syscall = -1;
    instr_t *prev = instr_get_prev(instr);
    if (prev != NULL) {
        prev = instr_get_prev_expanded(dcontext, ilist, instr);
        /* walk backwards looking for "mov_imm imm->xax"
         * may be other instrs placing operands into registers
         * for the syscall in between
         */
        while (prev != NULL &&
               instr_num_dsts(prev) > 0 &&
               opnd_is_reg(instr_get_dst(prev, 0)) &&
#ifdef X64
               opnd_get_reg(instr_get_dst(prev, 0)) != REG_RAX &&
#endif
               opnd_get_reg(instr_get_dst(prev, 0)) != REG_EAX) {
#ifdef CLIENT_INTERFACE
            /* if client added cti in between, bail and assume non-ignorable */
            if (instr_is_cti(prev) &&
                !(cti_is_normal_elision(prev)
                  IF_WINDOWS(|| instr_is_call_sysenter_pattern
                             (prev, instr_get_next(prev), instr))))
                return -1;
#endif
            prev = instr_get_prev_expanded(dcontext, ilist, prev);
        }
        if (prev != NULL &&
            instr_get_opcode(prev) == OP_mov_imm &&
            (IF_X64_ELSE(opnd_get_reg(instr_get_dst(prev, 0)) == REG_RAX, true) ||
             opnd_get_reg(instr_get_dst(prev, 0)) == REG_EAX)) {
#ifdef CLIENT_INTERFACE
            instr_t *walk, *tgt;
#endif
            IF_X64(ASSERT_TRUNCATE(int, int,
                                   opnd_get_immed_int(instr_get_src(prev, 0))));
            syscall = (int) opnd_get_immed_int(instr_get_src(prev, 0));
#ifdef CLIENT_INTERFACE
            /* if client added cti target in between, bail and assume non-ignorable */
            for (walk = instrlist_first_expanded(dcontext, ilist);
                 walk != NULL;
                 walk = instr_get_next_expanded(dcontext, ilist, walk)) {
                if (instr_is_cti(walk) && opnd_is_instr(instr_get_target(walk))) {
                    for (tgt = opnd_get_instr(instr_get_target(walk));
                         tgt != NULL;
                         tgt = instr_get_next_expanded(dcontext, ilist, tgt)) {
                        if (tgt == prev)
                            break;
                        if (tgt == instr)
                            return -1;
                    }
                }
            }
#endif
        }
    }
    return syscall;
}

#ifdef UNIX
/* Inserts code to handle clone into ilist.
 * instr is the syscall instr itself.
 * Assumes that instructions exist beyond instr in ilist.
 * pc_to_ecx is an instr that puts the pc after the app's syscall instr
 * into xcx.
 * skip decides whether the clone code is skipped by default or not.
 *
 * N.B.: mangle_clone_code() makes assumptions about this exact code layout
 *
 * CAUTION: don't use a lot of stack in the generated code because
 *          get_clone_record() makes assumptions about the usage of stack being
 *          less than a page.
 */
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         bool skip _IF_X64(gencode_mode_t mode))
{
            /*
                int 0x80
            .if don't know sysnum statically:
                jmp ignore  <-- modifiable jmp
            .else
                jmp xchg    # need this so can jmp to ignore if !CLONE_VM
            .endif
              xchg:
                xchg xax,xcx
                jecxz child
                jmp parent
              child:
                # i#149/PR 403015: the child is on the dstack so no need to swap stacks
                jmp new_thread_dynamo_start
              parent:
                xchg xax,xcx
              ignore:
                <post system call, etc.>
            */
    instr_t *in = instr_get_next(instr);
    instr_t *xchg = INSTR_CREATE_label(dcontext);
    instr_t *child = INSTR_CREATE_label(dcontext);
    instr_t *parent = INSTR_CREATE_label(dcontext);
    ASSERT(in != NULL);
    /* we have to dynamically skip or not skip the clone code
     * see mangle_clone_code below
     */
    if (skip) {
        /* Insert a jmp that normally skips the clone stuff,
         * pre_system_call will modify it if it really is SYS_clone.
         */
        PRE(ilist, in,
            INSTR_CREATE_jmp(dcontext, opnd_create_instr(in)));
    } else {
        /* We have to do this even if we statically know the sysnum
         * because if CLONE_VM is not set this is a fork, and we then
         * want to skip our clone code.
         */
        PRE(ilist, in,
            INSTR_CREATE_jmp(dcontext, opnd_create_instr(xchg)));
    }
    PRE(ilist, in, xchg);
    PRE(ilist, in, INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX),
                                     opnd_create_reg(REG_XCX)));
    PRE(ilist, in,
        INSTR_CREATE_jecxz(dcontext, opnd_create_instr(child)));
    PRE(ilist, in,
        INSTR_CREATE_jmp(dcontext, opnd_create_instr(parent)));

    PRE(ilist, in, child);
    /* We used to insert this directly into fragments for inlined system
     * calls, but not once we eliminated clean calls out of the DR cache
     * for security purposes.  Thus it can be a meta jmp, or an indirect jmp.
     */
    insert_reachable_cti(dcontext, ilist, in, vmcode_get_start(),
                         (byte *) get_new_thread_start(dcontext _IF_X64(mode)),
                         true/*jmp*/, false/*!precise*/, DR_REG_NULL/*no scratch*/,
                         NULL);
    instr_set_meta(instr_get_prev(in));
    PRE(ilist, in, parent);
    PRE(ilist, in, INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XAX),
                                     opnd_create_reg(REG_XCX)));
}

/* find the system call number in instrlist for an inlined system call
 * by simpling walking the ilist backward and finding "mov immed => %eax"
 * without checking cti or expanding instr
 */
int
ilist_find_sysnum(instrlist_t *ilist, instr_t *instr)
{
    for (; instr != NULL; instr = instr_get_prev(instr)) {
        if (instr_is_app(instr) &&
            instr_get_opcode(instr) == OP_mov_imm &&
            opnd_is_reg(instr_get_dst(instr, 0)) &&
            opnd_get_reg(instr_get_dst(instr, 0)) == REG_EAX &&
            opnd_is_immed_int(instr_get_src(instr, 0)))
            return (int) opnd_get_immed_int(instr_get_src(instr, 0));
    }
    ASSERT_NOT_REACHED();
    return -1;
}

#endif /* UNIX */

#ifdef WINDOWS
/* Note that ignore syscalls processing for XP and 2003 is a two-phase operation.
 * For this reason, mangle_syscall() might be called with a 'next_instr' that's
 * not an original app instruction but one inserted by the earlier mangling phase.
 */
#endif
static void
mangle_syscall(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
               instr_t *instr, instr_t *next_instr)
{
#ifdef UNIX
    if (get_syscall_method() != SYSCALL_METHOD_INT &&
        get_syscall_method() != SYSCALL_METHOD_SYSCALL &&
        get_syscall_method() != SYSCALL_METHOD_SYSENTER) {
        /* don't know convention on return address from kernel mode! */
        SYSLOG_INTERNAL_ERROR("unsupported system call method");
        LOG(THREAD, LOG_INTERP, 1, "don't know convention for this syscall method\n");
        CLIENT_ASSERT(false, "Unsupported system call method detected. Please "
                      "reboot with the nosep kernel option if this is a 32-bit "
                      "2.5 or 2.6 version Linux kernel.");
    }
    /* cannot use dynamo stack in code cache, so we cannot insert a
     * call -- instead we have interp end bbs at interrupts unless
     * we can identify them as ignorable system calls.  Otherwise,
     * we just remove the instruction and jump back to dynamo to
     * handle it.
     */
    if (TESTANY(INSTR_NI_SYSCALL_ALL, instr->flags)) {
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        return;
    }

    /* signal barrier: need to be able to exit fragment immediately
     * prior to syscall, so we set up an exit cti with a jmp right beforehand
     * that by default hops over the exit cti.
     * when we want to exit right before the syscall, we call the
     * mangle_syscall_code() routine below.
     */
    instr_t *skip_exit = INSTR_CREATE_label(dcontext);
    PRE(ilist, instr, INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(skip_exit)));
    /* assumption: raw bits of instr == app pc */
    ASSERT(instr_get_raw_bits(instr) != NULL);
    /* this should NOT be a meta-instr so we don't use PRE */
    /* note that it's ok if this gets linked: we unlink all outgoing exits in
     * addition to changing the skip_exit jmp upon receiving a signal
     */
    instrlist_preinsert(ilist, instr, INSTR_CREATE_jmp
                        (dcontext, opnd_create_pc(instr_get_raw_bits(instr))));
    PRE(ilist, instr, skip_exit);

    if (does_syscall_ret_to_callsite() &&
        sysnum_is_not_restartable(ilist_find_sysnum(ilist, instr))) {
        /* i#1216: we insert a nop instr right after inlined non-auto-restart
         * syscall to make it a safe point for suspending.
         * XXX-i#1216-c#2: we still need handle auto-restart syscall
         */
        instr_t *nop = INSTR_CREATE_nop(dcontext);
        /* We make a fake app nop instr for easy handling in recreate_app_state.
         * XXX: it is cleaner to mark our-mangling and handle it, but it seems
         * ok to use a fake app nop instr, since the client won't see it.
         */
        INSTR_XL8(nop, (instr_get_translation(instr) +
                        instr_length(dcontext, instr)));
        instr_set_app(instr);
        instrlist_postinsert(ilist, instr, nop);
    }

# ifdef MACOS
    if (instr_get_opcode(instr) == OP_sysenter) {
        /* The kernel returns control to whatever user-mode places in edx.
         * We get control back here and then go to the ret ibl (since normally
         * there's a call to a shared routine that does "pop edx").
         */
        instr_t *post_sysenter = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
        instrlist_insert_mov_instr_addr(dcontext, post_sysenter, NULL/*in cache*/,
                                        opnd_create_reg(REG_XDX),
                                        ilist, instr, NULL, NULL);
        /* sysenter goes here */
        PRE(ilist, next_instr, post_sysenter);
        PRE(ilist, next_instr,
            RESTORE_FROM_DC_OR_TLS(dcontext, flags, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
        PRE(ilist, next_instr,
            SAVE_TO_DC_OR_TLS(dcontext, flags, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
        PRE(ilist, next_instr,
            INSTR_CREATE_mov_st(dcontext, opnd_create_reg(REG_XCX),
                                opnd_create_reg(REG_XDX)));
    } else if (TEST(INSTR_BRANCH_SPECIAL_EXIT, instr->flags)) {
        int num = instr_get_interrupt_number(instr);
        ASSERT(instr_get_opcode(instr) == OP_int);
        if (num == 0x81 || num == 0x82) {
            int reason = (num == 0x81) ? EXIT_REASON_NI_SYSCALL_INT_0x81 :
                EXIT_REASON_NI_SYSCALL_INT_0x82;
            if (DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, flags)) {
                insert_shared_get_dcontext(dcontext, ilist, instr, true/*save_xdi*/);
                PRE(ilist, instr, INSTR_CREATE_mov_st
                    (dcontext,
                     opnd_create_dcontext_field_via_reg_sz(dcontext, REG_NULL/*default*/,
                                                           EXIT_REASON_OFFSET, OPSZ_4),
                     OPND_CREATE_INT32(reason)));
                insert_shared_restore_dcontext_reg(dcontext, ilist, instr);
            } else {
                PRE(ilist, instr,
                    instr_create_save_immed_to_dcontext(dcontext, reason,
                                                        EXIT_REASON_OFFSET));
            }
        }
    }
# endif

# ifdef STEAL_REGISTER
    /* in linux, system calls get their parameters via registers.
     * edi is the last one used, but there are system calls that
     * use it, so we put the real value into edi.  plus things
     * like fork() should get the real register values.
     * it's also a good idea to put the real edi into %edi for
     * debugger interrupts (int3).
     */

    /* the only way we can save and then restore our dc
     * ptr is to use the stack!
     * this should be fine, all interrupt instructions push
     * both eflags and return address on stack, so esp must
     * be valid at this point.  there could be an application
     * assuming only 2 slots on stack will be used, we use a 3rd
     * slot, could mess up that app...but what can we do?
     * also, if kernel examines user stack, we could have problems.
     *   push edi          # push dcontext ptr
     *   restore edi       # restore app edi
     *   <syscall>
     *   push ebx
     *   mov edi, ebx
     *   mov 4(esp), edi   # get dcontext ptr
     *   save ebx to edi slot
     *   pop ebx
     *   add 4,esp         # clean up push of dcontext ptr
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    PRE(ilist, instr,
        INSTR_CREATE_push(dcontext, opnd_create_reg(REG_EDI)));
    PRE(ilist, instr,
        instr_create_restore_from_dcontext(dcontext, REG_EDI, XDI_OFFSET));

    /* insert after in reverse order: */
    POST(ilist, instr,
         INSTR_CREATE_add(dcontext, opnd_create_reg(REG_ESP),
                          OPND_CREATE_INT8(4)));
    POST(ilist, instr,
         INSTR_CREATE_pop(dcontext, opnd_create_reg(REG_EBX)));
    POST(ilist, instr,
         instr_create_save_to_dcontext(dcontext, REG_EBX, XDI_OFFSET));
    POST(ilist, instr,
         INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EDI),
                             OPND_CREATE_MEM32(REG_ESP, 4)));
    POST(ilist, instr,
         INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EBX),
                             opnd_create_reg(REG_EDI)));
    POST(ilist, instr,
         INSTR_CREATE_push(dcontext, opnd_create_reg(REG_EBX)));
# endif /* STEAL_REGISTER */

#else /* WINDOWS */
    /* special handling of system calls is performed in shared_syscall or
     * in do_syscall
     */

    /* FIXME: for ignorable syscalls,
     * do we need support for exiting mid-fragment prior to a syscall
     * like we do on Linux, to bound time in cache?
     */

    if (does_syscall_ret_to_callsite()) {
        uint len = instr_length(dcontext, instr);
        if (TEST(INSTR_SHARED_SYSCALL, instr->flags)) {
            ASSERT(DYNAMO_OPTION(shared_syscalls));
            /* this syscall will be performed by the shared_syscall code
             * we just need to place a return address into the dcontext
             * xsi slot or the mangle-next-tag tls slot
             */
            if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
# ifdef X64
                ASSERT(instr_raw_bits_valid(instr));
                /* PR 244741: no 64-bit store-immed-to-mem
                 * FIXME: would be nice to move this to the stub and
                 * use the dead rbx register!
                 */
                PRE(ilist, instr,
                    instr_create_save_to_tls(dcontext, REG_XCX, MANGLE_NEXT_TAG_SLOT));
                PRE(ilist, instr,
                    INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                         OPND_CREATE_INTPTR((instr->bytes + len))));
                PRE(ilist, instr, INSTR_CREATE_xchg
                    (dcontext, opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT)),
                     opnd_create_reg(REG_XCX)));
# else
                PRE(ilist, instr, INSTR_CREATE_mov_st
                    (dcontext, opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT)),
                     OPND_CREATE_INTPTR((instr->bytes + len))));
# endif
            }
            else {
                PRE(ilist, instr, instr_create_save_immed_to_dcontext
                    (dcontext, (uint)(ptr_uint_t)(instr->bytes + len), XSI_OFFSET));
            }
        }
        /* Handle ignorable syscall.  non-ignorable system calls are
         * destroyed and removed from the list at the end of this func.
         */
        else if (!TEST(INSTR_NI_SYSCALL, instr->flags)) {
            if (get_syscall_method() == SYSCALL_METHOD_INT &&
                DYNAMO_OPTION(sygate_int)) {
                /* for Sygate need to mangle into a call to int_syscall_addr
                 * is anyone going to get screwed up by this change
                 * (say flags change?) [-ignore_syscalls only]*/
                ASSERT_NOT_TESTED();
                instrlist_replace(ilist, instr, create_syscall_instr(dcontext));
                instr_destroy(dcontext, instr);
            } else if (get_syscall_method() == SYSCALL_METHOD_SYSCALL)
                ASSERT_NOT_TESTED();
            else if (get_syscall_method() == SYSCALL_METHOD_WOW64)
                ASSERT_NOT_TESTED();
            return;
        }
    } else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* on XP/2003 we have a choice between inserting a trampoline at the
         * return pt of the sysenter, which is 0x7ffe0304 (except for
         * SP2-patched XP), which is bad since it would clobber whatever's after
         * the ret there (unless we used a 0xcc, like Visual Studio 2005 debugger
         * does), or replacing the ret addr on the stack -- we choose the
         * latter as the lesser of two transparency evils. Note that the
         * page at 0x7ffe0000 can't be made writable anyway, so hooking
         * isn't possible.
         */
        if (TEST(INSTR_SHARED_SYSCALL, instr->flags)) {
            ASSERT(DYNAMO_OPTION(shared_syscalls));
        }
        /* Handle ignorable syscall.  non-ignorable system calls are
         * destroyed and removed from the list at the end of this func.
         */
        else if (!TEST(INSTR_NI_SYSCALL, instr->flags)) {

            instr_t *mov_imm;

            /* even w/ ignorable syscall, need to make sure regain control */
            ASSERT(next_instr != NULL);
            ASSERT(DYNAMO_OPTION(indcall2direct));
            /* for sygate hack need to basically duplicate what is done in
             * shared_syscall, but here we could be shared so would need to
             * grab dcontext first etc. */
            ASSERT_NOT_IMPLEMENTED(!DYNAMO_OPTION(sygate_sysenter));
            /* PR 253943: we don't support sysenter in x64 */
            IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* can't have 8-byte imm-to-mem */
            /* FIXME PR 303413: we won't properly translate a fault in our
             * app stack reference here.  It's marked as our own mangling
             * so we'll at least return failure from our translate routine.
             */
            mov_imm = INSTR_CREATE_mov_st(dcontext, OPND_CREATE_MEM32(REG_XSP, 0),
                                          opnd_create_instr(next_instr));
            ASSERT(instr_is_mov_imm_to_tos(mov_imm));
            PRE(ilist, instr, mov_imm);
            /* do not let any encoding for length be cached!
             * o/w will lose pc-relative opnd
             */
            /* 'next_instr' is executed after the after-syscall vsyscall
             * 'ret', which is executed natively. */
            instr_set_meta(instr_get_prev(instr));
            return; /* leave syscall instr alone */
        }
    } else {
        SYSLOG_INTERNAL_ERROR("unsupported system call method");
        LOG(THREAD, LOG_INTERP, 1, "don't know convention for this syscall method\n");
        if (!TEST(INSTR_NI_SYSCALL, instr->flags))
            return;
        ASSERT_NOT_IMPLEMENTED(false);
    }

    /* destroy the syscall instruction */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
#endif /* WINDOWS */
}

#ifdef UNIX

/* Makes sure the jmp immediately after the syscall instruction
 * either skips or doesn't skip the clone code following it,
 * as indicated by the parameter skip.
 * pc must be either the return address of pre_system_call or
 * the address of do_syscall.
 */
void
mangle_clone_code(dcontext_t *dcontext, byte *pc, bool skip)
{
    byte *target, *prev_pc;
    instr_t instr;
    instr_init(dcontext, &instr);
    LOG(THREAD, LOG_SYSCALLS, 3,
        "mangle_clone_code: pc="PFX", skip=%d\n", pc, skip);
    do {
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        ASSERT(pc != NULL); /* our own code! */
    } while (!instr_is_syscall(&instr));
    /* jmp is right after syscall */
    instr_reset(dcontext, &instr);
    prev_pc = pc;
    pc = decode(dcontext, pc, &instr);
    ASSERT(pc != NULL); /* our own code! */
    ASSERT(instr_get_opcode(&instr) == OP_jmp);
    if (skip) {
        /* target is after 3rd xchg */
        instr_t tmp_instr;
        int num_xchg = 0;
        target = pc;
        instr_init(dcontext, &tmp_instr);
        while (num_xchg <= 2) {
            instr_reset(dcontext, &tmp_instr);
            target = decode(dcontext, target, &tmp_instr);
            ASSERT(target != NULL); /* our own code! */
            if (instr_get_opcode(&tmp_instr) == OP_xchg)
                num_xchg++;
        }
        instr_free(dcontext, &tmp_instr);
    } else {
        target = pc;
    }
    if (opnd_get_pc(instr_get_target(&instr)) != target) {
        DEBUG_DECLARE(byte *nxt_pc;)
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\tmodifying target of after-clone jmp to "PFX"\n", target);
        instr_set_target(&instr, opnd_create_pc(target));
        DEBUG_DECLARE(nxt_pc = ) instr_encode(dcontext, &instr, prev_pc);
        ASSERT(nxt_pc != NULL && nxt_pc == pc);
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\ttarget of after-clone jmp is already "PFX"\n", target);
    }
    instr_free(dcontext, &instr);
}

/* If skip is false:
 *   changes the jmp right before the next syscall (after pc) to target the
 *   exit cti immediately following it;
 * If skip is true:
 *   changes back to the default, where skip hops over the exit cti,
 *   which is assumed to be located at pc.
 */
bool
mangle_syscall_code(dcontext_t *dcontext, fragment_t *f, byte *pc, bool skip)
{
    byte *stop_pc = fragment_body_end_pc(dcontext, f);
    byte *target, *prev_pc, *cti_pc = NULL, *skip_pc = NULL;
    instr_t instr;
    DEBUG_DECLARE(instr_t cti;)
    instr_init(dcontext, &instr);
    DODEBUG({ instr_init(dcontext, &cti); });
    LOG(THREAD, LOG_SYSCALLS, 3,
        "mangle_syscall_code: pc="PFX", skip=%d\n", pc, skip);
    do {
        instr_reset(dcontext, &instr);
        prev_pc = pc;
        pc = decode(dcontext, pc, &instr);
        ASSERT(pc != NULL); /* our own code! */
        if (instr_get_opcode(&instr) == OP_jmp_short)
            skip_pc = prev_pc;
        else if (instr_get_opcode(&instr) == OP_jmp)
            cti_pc = prev_pc;
        if (pc >= stop_pc) {
            LOG(THREAD, LOG_SYSCALLS, 3, "\tno syscalls found\n");
            instr_free(dcontext, &instr);
            return false;
        }
    } while (!instr_is_syscall(&instr));
    if (skip_pc != NULL) {
        /* signal happened after skip jmp: nothing we can do here
         *
         * FIXME PR 213040: we should tell caller difference between
         * "no syscalls" and "too-close syscall" and have it take
         * other actions to bound signal delay
         */
        instr_free(dcontext, &instr);
        return false;
    }
    ASSERT(skip_pc != NULL && cti_pc != NULL);
    /* jmps are right before syscall, but there can be nops to pad exit cti */
    ASSERT(cti_pc == prev_pc - JMP_LONG_LENGTH);
    ASSERT(skip_pc < cti_pc);
    ASSERT(skip_pc == cti_pc - JMP_SHORT_LENGTH ||
           *(cti_pc - JMP_SHORT_LENGTH) == RAW_OPCODE_nop);
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, skip_pc, &instr);
    ASSERT(pc != NULL); /* our own code! */
    ASSERT(instr_get_opcode(&instr) == OP_jmp_short);
    ASSERT(pc <= cti_pc); /* could be nops */
    DOCHECK(1, {
        pc = decode(dcontext, cti_pc, &cti);
        ASSERT(pc != NULL); /* our own code! */
        ASSERT(instr_get_opcode(&cti) == OP_jmp);
        ASSERT(pc == prev_pc);
        instr_reset(dcontext, &cti);
    });
    if (skip) {
        /* target is syscall itself */
        target = prev_pc;
    } else {
        /* target is exit cti */
        target = cti_pc;
    }
    /* FIXME : this should work out to just a 1 byte write, but let's make
     * it more clear that this is atomic! */
    if (opnd_get_pc(instr_get_target(&instr)) != target) {
        DEBUG_DECLARE(byte *nxt_pc;)
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\tmodifying target of syscall jmp to "PFX"\n", target);
        instr_set_target(&instr, opnd_create_pc(target));
        DEBUG_DECLARE(nxt_pc = ) instr_encode(dcontext, &instr, skip_pc);
        ASSERT(nxt_pc != NULL && nxt_pc == cti_pc);
    } else {
        LOG(THREAD, LOG_SYSCALLS, 3,
            "\ttarget of syscall jmp is already "PFX"\n", target);
    }
    instr_free(dcontext, &instr);
    return true;
}
#endif /* UNIX */

/***************************************************************************
 * NON-SYSCALL INTERRUPT
 */
static void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr)
{
#ifdef WINDOWS
    int num;
    if (instr_get_opcode(instr) != OP_int)
        return;
    num = instr_get_interrupt_number(instr);
    if (num == 0x2b) {
        /* A callback finishes and returns to the interruption
         * point of the thread with the instruction "int 2b".
         * The interrupt ends the block; remove the instruction
         * since we'll come back to dynamo to perform the
         * interrupt.
         */
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
    }
#endif /* WINDOWS */
}

/***************************************************************************
 * FLOATING POINT PC
 */

/* The offset of the last floating point PC in the saved state */
#define FNSAVE_PC_OFFS  12
#define FXSAVE_PC_OFFS   8
#define FXSAVE_SIZE    512

void
float_pc_update(dcontext_t *dcontext)
{
    byte *state = *(byte **)(((byte *)dcontext->local_state) + FLOAT_PC_STATE_SLOT);
    app_pc orig_pc, xl8_pc;
    uint offs = 0;
    LOG(THREAD, LOG_INTERP, 2, "%s: fp state "PFX"\n", __FUNCTION__, state);
    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE ||
        dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE64) {
        /* Check whether the FPU state was saved */
        uint64 header_bv = *(uint64 *)(state + FXSAVE_SIZE);
        if (!TEST(XCR0_FP, header_bv)) {
            LOG(THREAD, LOG_INTERP, 2, "%s: xsave did not save FP state => nop\n",
                __FUNCTION__);
        }
        return;
    }

    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_FNSAVE) {
        offs = FNSAVE_PC_OFFS;
    } else {
        offs = FXSAVE_PC_OFFS;
    }
    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_FXSAVE64 ||
        dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE64)
        orig_pc = *(app_pc *)(state + offs);
    else /* just bottom 32 bits of pc */
        orig_pc = (app_pc)(ptr_uint_t) *(uint *)(state + offs);
    if (orig_pc == NULL) {
        /* no fp instr yet */
        LOG(THREAD, LOG_INTERP, 2, "%s: pc is NULL\n", __FUNCTION__);
        return;
    }
    /* i#1211-c#1: the orig_pc might be an app pc restored from fldenv */
    if (!in_fcache(orig_pc) &&
        /* XXX: i#698: there might be fp instr neither in fcache nor in app */
        !(in_generated_routine(dcontext, orig_pc) ||
          is_dynamo_address(orig_pc) ||
          is_in_dynamo_dll(orig_pc)
          IF_CLIENT_INTERFACE(|| is_in_client_lib(orig_pc)))) {
        bool no_xl8 = true;
#ifdef X64
        if (dcontext->upcontext.upcontext.exit_reason != EXIT_REASON_FLOAT_PC_FXSAVE64 &&
            dcontext->upcontext.upcontext.exit_reason != EXIT_REASON_FLOAT_PC_XSAVE64) {
            /* i#1427: try to fill in the top 32 bits */
            ptr_uint_t vmcode = (ptr_uint_t) vmcode_get_start();
            if ((vmcode & 0xffffffff00000000) > 0) {
                byte *orig_try = (byte *)
                    ((vmcode & 0xffffffff00000000) | (ptr_uint_t)orig_pc);
                if (in_fcache(orig_try)) {
                    LOG(THREAD, LOG_INTERP, 2,
                        "%s: speculating: pc "PFX" + top half of vmcode = "PFX"\n",
                        __FUNCTION__, orig_pc, orig_try);
                    orig_pc = orig_try;
                    no_xl8 = false;
                }
            }
        }
#endif
        if (no_xl8) {
            LOG(THREAD, LOG_INTERP, 2, "%s: pc "PFX" is translated already\n",
                __FUNCTION__, orig_pc);
            return;
        }
    }
    /* We must either grab thread_initexit_lock or be couldbelinking to translate */
    mutex_lock(&thread_initexit_lock);
    xl8_pc = recreate_app_pc(dcontext, orig_pc, NULL);
    mutex_unlock(&thread_initexit_lock);
    LOG(THREAD, LOG_INTERP, 2, "%s: translated "PFX" to "PFX"\n", __FUNCTION__,
        orig_pc, xl8_pc);

    if (dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_FXSAVE64 ||
        dcontext->upcontext.upcontext.exit_reason == EXIT_REASON_FLOAT_PC_XSAVE64)
        *(app_pc *)(state + offs) = xl8_pc;
    else /* just bottom 32 bits of pc */
        *(uint *)(state + offs) = (uint)(ptr_uint_t) xl8_pc;
}

static void
mangle_float_pc(dcontext_t *dcontext, instrlist_t *ilist,
                instr_t *instr, instr_t *next_instr, uint *flags INOUT)
{
    /* If there is a prior non-control float instr, we can inline the pc update.
     * Otherwise, we go back to dispatch.  In the latter case we do not support
     * building traces across the float pc save: we assume it's rare.
     */
    app_pc prior_float = NULL;
    bool exit_is_normal = false;
    int op = instr_get_opcode(instr);
    opnd_t memop = instr_get_dst(instr, 0);
    ASSERT(opnd_is_memory_reference(memop));

    /* To simplify the code here we don't support rip-rel for local handling.
     * We also don't support xsave, as it optionally writes the fpstate.
     */
    if (opnd_is_base_disp(memop) && op != OP_xsave32 && op != OP_xsaveopt32 &&
        op != OP_xsave64 && op != OP_xsaveopt64) {
        instr_t *prev;
        for (prev = instr_get_prev_expanded(dcontext, ilist, instr);
             prev != NULL;
             prev = instr_get_prev_expanded(dcontext, ilist, prev)) {
            dr_fp_type_t type;
            if (instr_is_app(prev) &&
                instr_is_floating_ex(prev, &type)) {
                bool control_instr = false;
                if (type == DR_FP_STATE /* quick check */ &&
                    /* Check the list from Intel Vol 1 8.1.8 */
                    (op == OP_fnclex || op == OP_fldcw || op == OP_fnstcw ||
                     op == OP_fnstsw || op == OP_fnstenv || op == OP_fldenv ||
                     op == OP_fwait))
                    control_instr = true;
                if (!control_instr) {
                    prior_float = instr_get_translation(prev);
                    if (prior_float == NULL && instr_raw_bits_valid(prev))
                        prior_float = instr_get_raw_bits(prev);
                    break;
                }
            }
        }
    }

    if (prior_float != NULL) {
        /* We can link this */
        exit_is_normal = true;
        STATS_INC(float_pc_from_cache);

        /* Replace the stored code cache pc with the original app pc.
         * If the app memory is unwritable, instr  would have already crashed.
         */
        if (op == OP_fnsave || op == OP_fnstenv) {
            opnd_set_disp(&memop, opnd_get_disp(memop) + FNSAVE_PC_OFFS);
            opnd_set_size(&memop, OPSZ_4);
            PRE(ilist, next_instr,
                INSTR_CREATE_mov_st(dcontext, memop,
                                    OPND_CREATE_INT32((int)(ptr_int_t)prior_float)));
        } else if (op == OP_fxsave32) {
            opnd_set_disp(&memop, opnd_get_disp(memop) + FXSAVE_PC_OFFS);
            opnd_set_size(&memop, OPSZ_4);
            PRE(ilist, next_instr,
                INSTR_CREATE_mov_st(dcontext, memop,
                                    OPND_CREATE_INT32((int)(ptr_int_t)prior_float)));
        } else if (op == OP_fxsave64) {
            opnd_set_disp(&memop, opnd_get_disp(memop) + FXSAVE_PC_OFFS);
            opnd_set_size(&memop, OPSZ_8);
            insert_mov_immed_ptrsz(dcontext, (ptr_int_t)prior_float, memop,
                                   ilist, next_instr, NULL, NULL);
        } else
            ASSERT_NOT_REACHED();
    } else if (!DYNAMO_OPTION(translate_fpu_pc)) {
        /* We only support translating when inlined.
         * XXX: we can't recover the loss of coarse-grained: we live with that.
         */
        exit_is_normal = true;
        ASSERT(!TEST(FRAG_CANNOT_BE_TRACE, *flags));
    } else {
        int reason = 0;
        CLIENT_ASSERT(!TEST(FRAG_IS_TRACE, *flags),
                      "removing an FPU instr in a trace with an FPU state save "
                      "is not supported");
        switch (op) {
        case OP_fnsave:
        case OP_fnstenv:    reason = EXIT_REASON_FLOAT_PC_FNSAVE;  break;
        case OP_fxsave32:   reason = EXIT_REASON_FLOAT_PC_FXSAVE;  break;
        case OP_fxsave64:   reason = EXIT_REASON_FLOAT_PC_FXSAVE64;break;
        case OP_xsave32:
        case OP_xsaveopt32: reason = EXIT_REASON_FLOAT_PC_XSAVE;   break;
        case OP_xsave64:
        case OP_xsaveopt64: reason = EXIT_REASON_FLOAT_PC_XSAVE64; break;
        default: ASSERT_NOT_REACHED();
        }
        if (DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, *flags)) {
            insert_shared_get_dcontext(dcontext, ilist, instr, true/*save_xdi*/);
            PRE(ilist, instr, INSTR_CREATE_mov_st
                (dcontext,
                 opnd_create_dcontext_field_via_reg_sz(dcontext, REG_NULL/*default*/,
                                                       EXIT_REASON_OFFSET, OPSZ_4),
                 OPND_CREATE_INT32(reason)));
        } else {
            PRE(ilist, instr,
                instr_create_save_immed_to_dcontext(dcontext, reason,
                                                    EXIT_REASON_OFFSET));
            PRE(ilist, instr,
                instr_create_save_to_tls(dcontext, REG_XDI, DCONTEXT_BASE_SPILL_SLOT));
        }
        /* At this point, xdi is spilled into DCONTEXT_BASE_SPILL_SLOT */

        /* We pass the address in the xbx tls slot, which is untouched by fcache_return.
         *
         * XXX: handle far refs!  Xref drutil_insert_get_mem_addr(), and sandbox_write()
         * hitting this same issue.
         */
        ASSERT_CURIOSITY(!opnd_is_far_memory_reference(memop));
        if (opnd_is_base_disp(memop)) {
            opnd_set_size(&memop, OPSZ_lea);
            PRE(ilist, instr,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XDI), memop));
        } else {
            ASSERT(opnd_is_abs_addr(memop) IF_X64( || opnd_is_rel_addr(memop)));
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                                     OPND_CREATE_INTPTR(opnd_get_addr(memop))));
        }
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, REG_XDI, FLOAT_PC_STATE_SLOT));

        /* Restore app %xdi */
        if (TEST(FRAG_SHARED, *flags))
            insert_shared_restore_dcontext_reg(dcontext, ilist, instr);
        else {
            PRE(ilist, instr,
                instr_create_restore_from_tls(dcontext, REG_XDI,
                                              DCONTEXT_BASE_SPILL_SLOT));
        }
    }

    if (exit_is_normal && DYNAMO_OPTION(translate_fpu_pc)) {
        instr_t *exit_jmp = next_instr;
        while (exit_jmp != NULL && !instr_is_exit_cti(exit_jmp))
            exit_jmp = instr_get_next(next_instr);
        ASSERT(exit_jmp != NULL);
        ASSERT(instr_branch_special_exit(exit_jmp));
        instr_branch_set_special_exit(exit_jmp, false);
        /* XXX: there could be some other reason this was marked
         * cannot-be-trace that we're undoing here...
         */
        if (TEST(FRAG_CANNOT_BE_TRACE, *flags))
            *flags &= ~FRAG_CANNOT_BE_TRACE;
    }
}

/***************************************************************************
 * CPUID FOOLING
 */
#ifdef FOOL_CPUID

/* values returned by cpuid for Mobile Pentium MMX processor (family 5, model 8)
 * minus mmx (==0x00800000 in CPUID_1_EDX)
 * FIXME: change model number to a Pentium w/o MMX!
 */
#define CPUID_0_EAX 0x00000001
#define CPUID_0_EBX 0x756e6547
#define CPUID_0_ECX 0x6c65746e
#define CPUID_0_EDX 0x49656e69
/* extended family, extended model, type, family, model, stepping id: */
/* 20:27,           16:19,          12:13, 8:11,  4:7,   0:3    */
#define CPUID_1_EAX 0x00000581
#define CPUID_1_EBX 0x00000000
#define CPUID_1_ECX 0x00000000
#define CPUID_1_EDX 0x000001bf

static void
mangle_cpuid(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
             instr_t *next_instr)
{
    /* assumption: input value is put in eax on prev instr, or
     * on instr prior to that and prev is an inc instr.
     * alternative is to insert conditional branch...and save eflags, etc.
     */
    instr_t *prev = instr_get_prev(instr);
    opnd_t op;
    int input, out_eax, out_ebx, out_ecx, out_edx;

    LOG(THREAD, LOG_INTERP, 1, "fooling cpuid instruction!\n");

    ASSERT(prev != NULL);
    prev = instr_get_prev_expanded(dcontext, ilist, instr);
    instr_decode(dcontext, instr);
    if (!instr_valid(instr))
        goto cpuid_give_up;
    loginst(dcontext, 2, prev, "prior to cpuid");

    /* FIXME: maybe should insert code to dispatch on eax, rather than
     * this hack, which is based on photoshop, which either does
     * "xor eax,eax" or "xor eax,eax; inc eax"
     */
    if (!instr_is_mov_constant(prev, &input)) {
        /* we only allow inc here */
        if (instr_get_opcode(prev) != OP_inc)
            goto cpuid_give_up;
        op = instr_get_dst(prev, 0);
        if (!opnd_is_reg(op) || opnd_get_reg(op) != REG_EAX)
            goto cpuid_give_up;
        /* now check instr before inc */
        prev = instr_get_prev(prev);
        if (!instr_is_mov_constant(prev, &input) || input != 0)
            goto cpuid_give_up;
        input = 1;
        /* now check that mov 0 is into eax */
    }
    if (instr_num_dsts(prev) == 0)
        goto cpuid_give_up;
    op = instr_get_dst(prev, 0);
    if (!opnd_is_reg(op) || opnd_get_reg(op) != REG_EAX)
        goto cpuid_give_up;

    if (input == 0) {
        out_eax = CPUID_0_EAX;
        out_ebx = CPUID_0_EBX;
        out_ecx = CPUID_0_ECX;
        out_edx = CPUID_0_EDX;
    } else {
        /* 1 or anything higher all return same info */
        out_eax = CPUID_1_EAX;
        out_ebx = CPUID_1_EBX;
        out_ecx = CPUID_1_ECX;
        out_edx = CPUID_1_EDX;
    }

    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                             OPND_CREATE_INT32(out_eax)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                             OPND_CREATE_INT32(out_ebx)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX),
                             OPND_CREATE_INT32(out_ecx)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EDX),
                             OPND_CREATE_INT32(out_edx)));

    /* destroy the cpuid instruction */
    instrlist_remove(ilist, instr);
    instr_destroy(dcontext, instr);
    return;
 cpuid_give_up:
    LOG(THREAD, LOG_INTERP, 1, "\tcpuid fool: giving up\n");
    return;
}
#endif /* FOOL_CPUID */

static void
mangle_exit_cti_prefixes(dcontext_t *dcontext, instr_t *instr)
{
    uint prefixes = instr_get_prefixes(instr);
    if (prefixes != 0) {
        bool remove = false;
        /* Case 8738: while for transparency it would be best to maintain all
         * prefixes, our patching and other routines make assumptions about
         * the length of exit ctis.  Plus our elision removes the whole
         * instr in any case.
         */
        if (instr_is_cbr(instr)) {
            if (TESTANY(~(PREFIX_JCC_TAKEN|PREFIX_JCC_NOT_TAKEN), prefixes)) {
                remove = true;
                prefixes &= (PREFIX_JCC_TAKEN|PREFIX_JCC_NOT_TAKEN);
            }
        } else {
            /* prefixes on ubr or mbr should be nops and for ubr will mess up
             * our size assumptions so drop them (i#435)
             */
            remove = true;
            prefixes = 0;
        }
        if (remove) {
            LOG(THREAD, LOG_INTERP, 4,
                "\tremoving unknown prefixes "PFX" from "PFX"\n",
                prefixes, instr_get_raw_bits(instr));
            ASSERT(instr_operands_valid(instr)); /* ensure will encode w/o raw bits */
            instr_set_prefixes(instr, prefixes);
        }
    }
}

#ifdef X64
/* PR 215397: re-relativize rip-relative data addresses */
/* i#393, returned bool indicates if the instr is destroyed. */
static bool
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr)
{
    uint opc = instr_get_opcode(instr);
    app_pc tgt;
    opnd_t dst, src;
    ASSERT(instr_has_rel_addr_reference(instr));
    instr_get_rel_addr_target(instr, &tgt);
    STATS_INC(rip_rel_instrs);
# ifdef RCT_IND_BRANCH
    if (TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_call)) ||
        TEST(OPTION_ENABLED, DYNAMO_OPTION(rct_ind_jump))) {
        /* PR 215408: record addresses taken via rip-relative instrs */
        rct_add_rip_rel_addr(dcontext, tgt _IF_DEBUG(instr_get_translation(instr)));
    }
# endif
    if (opc == OP_lea) {
        /* segment overrides are ignored on lea */
        opnd_t immed;
        dst = instr_get_dst(instr, 0);
        src = instr_get_src(instr, 0);
        ASSERT(opnd_is_reg(dst));
        ASSERT(opnd_is_rel_addr(src));
        ASSERT(opnd_get_addr(src) == tgt);
        /* Replace w/ an absolute immed of the target app address, following Intel
         * Table 3-59 "64-bit Mode LEA Operation with Address and Operand Size
         * Attributes" */
        /* FIXME PR 253446: optimization: we could leave this as rip-rel if it
         * still reaches from the code cache. */
        if (reg_get_size(opnd_get_reg(dst)) == OPSZ_8) {
            /* PR 253327: there is no explicit addr32 marker; we assume
             * that decode or the user already zeroed out the top bits
             * if there was an addr32 prefix byte or the user wants
             * that effect */
            immed = OPND_CREATE_INTPTR((ptr_int_t)tgt);
        } else if (reg_get_size(opnd_get_reg(dst)) == OPSZ_4)
            immed = OPND_CREATE_INT32((int)(ptr_int_t)tgt);
        else {
            ASSERT(reg_get_size(opnd_get_reg(dst)) == OPSZ_2);
            immed = OPND_CREATE_INT16((short)(ptr_int_t)tgt);
        }
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, dst, immed));
        instrlist_remove(ilist, instr);
        instr_destroy(dcontext, instr);
        STATS_INC(rip_rel_lea);
        return true;
    } else {
        /* PR 251479 will automatically re-relativize if it reaches,
         * but if it doesn't we need to handle that here (since that
         * involves an encoding length change, which complicates many
         * use cases if done at instr encode time).
         * We don't yet know exactly where we're going to encode this bb,
         * so we're conservative and check for all reachability from our
         * heap (assumed to be a single heap: xref PR 215395, and xref
         * potential secondary code caches PR 253446.
         */
        if (!rel32_reachable_from_vmcode(tgt)) {
            int si = -1, di = -1;
            opnd_t relop, newop;
            bool spill = true;
            /* FIXME PR 253446: for mbr, should share the xcx spill */
            reg_id_t scratch_reg = REG_XAX;
            si = instr_get_rel_addr_src_idx(instr);
            di = instr_get_rel_addr_dst_idx(instr);
            if (si >= 0) {
                relop = instr_get_src(instr, si);
                ASSERT(di < 0 || opnd_same(relop, instr_get_dst(instr, di)));
                /* If it's a load (OP_mov_ld, or OP_movzx, etc.), use dead reg */
                if (instr_num_srcs(instr) == 1 && /* src is the rip-rel opnd */
                    instr_num_dsts(instr) == 1 && /* only one dest: a register */
                    opnd_is_reg(instr_get_dst(instr, 0))) {
                    opnd_size_t sz = opnd_get_size(instr_get_dst(instr, 0));
                    reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
                    /* if target is 16 or 8 bit sub-register the whole reg is not dead
                     * (for 32-bit, top 32 bits are cleared) */
                    if (reg_is_gpr(reg) && (reg_is_32bit(reg) || reg_is_64bit(reg))) {
                        spill = false;
                        scratch_reg = opnd_get_reg(instr_get_dst(instr, 0));
                        if (sz == OPSZ_4)
                            scratch_reg = reg_32_to_64(scratch_reg);
                        /* we checked all opnds: should not read reg */
                        ASSERT(!instr_reads_from_reg(instr, scratch_reg));
                        STATS_INC(rip_rel_unreachable_nospill);
                    }
                }
            } else {
                relop = instr_get_dst(instr, di);
            }
            /* PR 263369: we can't just look for instr_reads_from_reg here since
             * our no-spill optimization above may miss some writes.
             */
            if (spill && instr_uses_reg(instr, scratch_reg)) {
                /* mbr (for which we'll use xcx once we optimize) should not
                 * get here: can't use registers (except xsp) */
                ASSERT(scratch_reg == REG_XAX);
                do {
                    scratch_reg++;
                    ASSERT(scratch_reg <= REG_STOP_64);
                } while (instr_uses_reg(instr, scratch_reg));
            }
            ASSERT(!instr_reads_from_reg(instr, scratch_reg));
            ASSERT(!spill || !instr_writes_to_reg(instr, scratch_reg));
            /* XXX PR 253446: Optimize by looking ahead for dead registers, and
             * sharing single spill across whole bb, or possibly building local code
             * cache to avoid unreachability: all depending on how many rip-rel
             * instrs we see.  We'll watch the stats.
             */
            if (spill) {
                PRE(ilist, instr,
                    SAVE_TO_DC_OR_TLS(dcontext, 0, scratch_reg, MANGLE_RIPREL_SPILL_SLOT,
                                      XAX_OFFSET));
            }
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(scratch_reg),
                                     OPND_CREATE_INTPTR((ptr_int_t)tgt)));

            newop = opnd_create_far_base_disp(opnd_get_segment(relop), scratch_reg,
                                              REG_NULL, 0, 0, opnd_get_size(relop));
            if (si >= 0)
                instr_set_src(instr, si, newop);
            if (di >= 0)
                instr_set_dst(instr, di, newop);
            /* we need the whole spill...restore region to all be marked mangle */
            instr_set_our_mangling(instr, true);
            if (spill) {
                PRE(ilist, next_instr,
                    instr_create_restore_from_tls(dcontext, scratch_reg,
                                                  MANGLE_RIPREL_SPILL_SLOT));
            }
            STATS_INC(rip_rel_unreachable);
        }
    }
    return false;
}
#endif

/***************************************************************************
 * Reference with segment register (fs/gs)
 */
#ifdef UNIX
static int
instr_get_seg_ref_dst_idx(instr_t *instr)
{
    int i;
    opnd_t opnd;
    if (!instr_valid(instr))
        return -1;
    /* must go to level 3 operands */
    for (i=0; i<instr_num_dsts(instr); i++) {
        opnd = instr_get_dst(instr, i);
        if (opnd_is_far_base_disp(opnd) &&
            (opnd_get_segment(opnd) == SEG_GS ||
             opnd_get_segment(opnd) == SEG_FS))
            return i;
    }
    return -1;
}

static int
instr_get_seg_ref_src_idx(instr_t *instr)
{
    int i;
    opnd_t opnd;
    if (!instr_valid(instr))
        return -1;
    /* must go to level 3 operands */
    for (i=0; i<instr_num_srcs(instr); i++) {
        opnd = instr_get_src(instr, i);
        if (opnd_is_far_base_disp(opnd) &&
            (opnd_get_segment(opnd) == SEG_GS ||
             opnd_get_segment(opnd) == SEG_FS))
            return i;
    }
    return -1;
}

static ushort tls_slots[4] =
    {TLS_XAX_SLOT, TLS_XCX_SLOT, TLS_XDX_SLOT, TLS_XBX_SLOT};

/* mangle the instruction OP_mov_seg, i.e. the instruction that
 * read/update the segment register.
 */
static void
mangle_mov_seg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr)
{
    reg_id_t seg;
    opnd_t opnd, dst;
    opnd_size_t dst_sz;

    ASSERT(instr_get_opcode(instr) == OP_mov_seg);
    ASSERT(instr_num_srcs(instr) == 1);
    ASSERT(instr_num_dsts(instr) == 1);

    STATS_INC(app_mov_seg_mangled);
    /* for update, we simply change it to a nop because we will
     * update it when dynamorio entering code cache to execute
     * this basic block.
     */
    dst = instr_get_dst(instr, 0);
    if (opnd_is_reg(dst) && reg_is_segment(opnd_get_reg(dst))) {
        seg = opnd_get_reg(dst);
#ifdef CLIENT_INTERFACE
        if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
            return;
#endif
        /* must use the original instr, which might be used by caller */
        instr_reuse(dcontext, instr);
        instr_set_opcode(instr, OP_nop);
        instr_set_num_opnds(dcontext, instr, 0, 0);
        return;
    }

    /* for read seg, we mangle it */
    opnd = instr_get_src(instr, 0);
    ASSERT(opnd_is_reg(opnd));
    seg = opnd_get_reg(opnd);
    ASSERT(reg_is_segment(seg));
    if (seg != SEG_FS && seg != SEG_GS)
        return;
#ifdef CLIENT_INTERFACE
    if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
        return;
#endif

    /* There are two possible mov_seg instructions:
     * 8C/r           MOV r/m16,Sreg   Move segment register to r/m16
     * REX.W + 8C/r   MOV r/m64,Sreg   Move zero extended 16-bit segment
     *                                 register to r/m64
     * Note, In 32-bit mode, the assembler may insert the 16-bit operand-size
     * prefix with this instruction.
     */
    /* we cannot replace the instruction but only change it. */
    dst = instr_get_dst(instr, 0);
    dst_sz = opnd_get_size(dst);
    opnd = opnd_create_sized_tls_slot
        (os_tls_offset(os_get_app_seg_offset(seg)), dst_sz);
    if (opnd_is_reg(dst)) { /* dst is a register */
        /* mov %gs:off => reg */
        instr_set_src(instr, 0, opnd);
        instr_set_opcode(instr, OP_mov_ld);
        if (IF_X64_ELSE((dst_sz == OPSZ_8), false))
            instr_set_opcode(instr, OP_movzx);
    } else { /* dst is memory, need steal a register. */
        reg_id_t reg;
        instr_t *ti;
        for (reg = REG_XAX; reg < REG_XBX; reg++) {
            if (!instr_uses_reg(instr, reg))
                break;
        }
        /* We need save the register to corresponding slot for correct restore,
         * so only use the first four registers.
         */
        ASSERT(reg <= REG_XBX);
        /* save reg */
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, reg,
                                     tls_slots[reg - REG_XAX]));
        /* restore reg */
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, reg,
                                          tls_slots[reg - REG_XAX]));
        switch (dst_sz) {
        case OPSZ_8:
            IF_NOT_X64(ASSERT(false);)
            break;
        case OPSZ_4:
            IF_X64(reg = reg_64_to_32(reg);)
            break;
        case OPSZ_2:
            IF_X64(reg = reg_64_to_32(reg);)
            reg = reg_32_to_16(reg);
            break;
        case OPSZ_1:
            IF_X64(reg = reg_64_to_32(reg);)
            reg = reg_32_to_8(reg);
        default:
            ASSERT(false);
        }
        /* mov %gs:off => reg */
        ti = INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(reg), opnd);
        if (IF_X64_ELSE((dst_sz == OPSZ_8), false))
            instr_set_opcode(ti, OP_movzx);
        PRE(ilist, instr, ti);
        /* change mov_seg to mov_st: mov reg => [mem] */
        instr_set_src(instr, 0, opnd_create_reg(reg));
        instr_set_opcode(instr, OP_mov_st);
    }
}

/* mangle the instruction that reference memory via segment register */
static void
mangle_seg_ref(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr)
{
    int si = -1, di = -1;
    opnd_t segop, newop;
    bool spill = true;
    reg_id_t scratch_reg = REG_XAX, seg = REG_NULL;

    /* exit cti won't be seg ref */
    if (instr_is_exit_cti(instr))
        return;
    /* mbr will be handled separatly */
    if (instr_is_mbr(instr))
        return;
    if (instr_get_opcode(instr) == OP_lea)
        return;

    /* XXX: maybe using decode_cti and then a check on prefix could be
     * more efficient as it only examines a few byte and avoid fully decoding
     * the instruction. For simplicity, we examine every operands instead.
     */
    /* 1. get ref opnd */
    si = instr_get_seg_ref_src_idx(instr);
    di = instr_get_seg_ref_dst_idx(instr);
    if (si < 0 && di < 0)
        return;
    if (si >= 0) {
        segop = instr_get_src(instr, si);
        ASSERT(di < 0 || opnd_same(segop, instr_get_dst(instr, di)));
    } else {
        segop = instr_get_dst(instr, di);
    }
    seg = opnd_get_segment(segop);
    if (seg != SEG_GS && seg != SEG_FS)
        return;
#ifdef CLIENT_INTERFACE
    if (seg == LIB_SEG_TLS && !INTERNAL_OPTION(private_loader))
        return;
#endif
    STATS_INC(app_seg_refs_mangled);

    DOLOG(3, LOG_INTERP, {
        loginst(dcontext, 3, instr, "reference with fs/gs segment");
    });
    /* 2. decide the scratch reg */
    /* Opt: if it's a load (OP_mov_ld, or OP_movzx, etc.), use dead reg */
    if (si >= 0 &&
        instr_num_srcs(instr) == 1 && /* src is the seg ref opnd */
        instr_num_dsts(instr) == 1 && /* only one dest: a register */
        opnd_is_reg(instr_get_dst(instr, 0))) {
        reg_id_t reg = opnd_get_reg(instr_get_dst(instr, 0));
        /* if target is 16 or 8 bit sub-register the whole reg is not dead
         * (for 32-bit, top 32 bits are cleared) */
        if (reg_is_gpr(reg) && (reg_is_32bit(reg) || reg_is_64bit(reg)) &&
            !instr_reads_from_reg(instr, reg) /* mov [%fs:%xax] => %xax */) {
            spill = false;
            scratch_reg = reg;
# ifdef X64
            if (opnd_get_size(instr_get_dst(instr, 0)) == OPSZ_4)
                scratch_reg = reg_32_to_64(reg);
# endif
        }
    }
    if (spill) {
        /* we pick a scratch register from XAX, XBX, XCX, or XDX
         * that has direct TLS slots.
         */
        for (scratch_reg = REG_XAX; scratch_reg <= REG_XBX; scratch_reg++) {
            /* the register must not be used by the instr, either read or write,
             * because we will mangle it when executing the instr (no read from),
             * and restore it after that instr (no write to).
             */
            if (!instr_uses_reg(instr, scratch_reg))
                break;
        }
        ASSERT(scratch_reg <= REG_XBX);
        PRE(ilist, instr,
            instr_create_save_to_tls(dcontext, scratch_reg,
                                     tls_slots[scratch_reg - REG_XAX]));
    }
    newop = mangle_seg_ref_opnd(dcontext, ilist, instr, segop, scratch_reg);
    if (si >= 0)
        instr_set_src(instr, si, newop);
    if (di >= 0)
        instr_set_dst(instr, di, newop);
    /* we need the whole spill...restore region to all be marked mangle */
    instr_set_our_mangling(instr, true);
    /* FIXME: i#107 we should check the bound and raise signal if out of bound. */
    DOLOG(3, LOG_INTERP, {
        loginst(dcontext, 3, instr, "re-wrote app tls reference");
    });

    if (spill) {
        PRE(ilist, next_instr,
            instr_create_restore_from_tls(dcontext, scratch_reg,
                                          tls_slots[scratch_reg - REG_XAX]));
    }
}
#endif /* UNIX */

#ifdef ANNOTATIONS
/***************************************************************************
 * DR and Valgrind annotations
 */
static void
mangle_annotation_helper(dcontext_t *dcontext, instr_t *instr, instrlist_t *ilist)
{
    dr_instr_label_data_t *label_data = instr_get_label_data_area(instr);
    dr_annotation_handler_t *handler = (dr_annotation_handler_t *) label_data->data[0];
    dr_annotation_receiver_t *receiver = handler->receiver_list;
    opnd_t *args = NULL;

    ASSERT(handler->type == DR_ANNOTATION_HANDLER_CALL);

    while (receiver != NULL) {
        if (handler->num_args != 0) {
            args = HEAP_ARRAY_ALLOC(dcontext, opnd_t, handler->num_args,
                                    ACCT_CLEANCALL, UNPROTECTED);
            memcpy(args, handler->args, sizeof(opnd_t) * handler->num_args);
        }
        dr_insert_clean_call_ex_varg(dcontext, ilist, instr,
            receiver->instrumentation.callback,
                receiver->save_fpstate ? DR_CLEANCALL_SAVE_FLOAT : 0,
            handler->num_args, args);
        if (handler->num_args != 0) {
            HEAP_ARRAY_FREE(dcontext, args, opnd_t, handler->num_args,
                            ACCT_CLEANCALL, UNPROTECTED);
        }
        receiver = receiver->next;
    }
}
#endif

/* TOP-LEVEL MANGLE
 * This routine is responsible for mangling a fragment into the form
 * we'd like prior to placing it in the code cache
 * If mangle_calls is false, ignores calls
 * If record_translation is true, records translation target for each
 * inserted instr -- but this slows down encoding in current implementation
 */
void
mangle(dcontext_t *dcontext, instrlist_t *ilist, uint *flags INOUT,
       bool mangle_calls, bool record_translation)
{
    instr_t *instr, *next_instr;
#ifdef WINDOWS
    bool ignorable_sysenter = DYNAMO_OPTION(ignore_syscalls) &&
        DYNAMO_OPTION(ignore_syscalls_follow_sysenter) &&
        (get_syscall_method() == SYSCALL_METHOD_SYSENTER) &&
        TEST(FRAG_HAS_SYSCALL, *flags);
#endif

    /* Walk through instr list:
     * -- convert exit branches to use near_rel form;
     * -- convert direct calls into 'push %eip', aka return address;
     * -- convert returns into 'pop %xcx (; add $imm, %xsp)';
     * -- convert indirect branches into 'save %xcx; lea EA, %xcx';
     * -- convert indirect calls as a combination of direct call and
     *    indirect branch conversion;
     * -- ifdef STEAL_REGISTER, steal edi for our own use.
     * -- ifdef UNIX, mangle seg ref and mov_seg
     */

    KSTART(mangling);
    instrlist_set_our_mangling(ilist, true); /* PR 267260 */
    for (instr = instrlist_first(ilist);
         instr != NULL;
         instr = next_instr) {

        /* don't mangle anything that mangle inserts! */
        next_instr = instr_get_next(instr);

        if (!instr_opcode_valid(instr))
            continue;

#ifdef ANNOTATIONS
        if (is_annotation_return_placeholder(instr)) {
            instrlist_remove(ilist, instr);
            instr_destroy(dcontext, instr);
            continue;
        }
#endif

        if (record_translation) {
            /* make sure inserted instrs translate to the original instr */
            app_pc xl8 = instr_get_translation(instr);
            if (xl8 == NULL)
                xl8 = instr_get_raw_bits(instr);
            instrlist_set_translation_target(ilist, xl8);
        }

#if defined(X86) && defined(X64)
        if (DYNAMO_OPTION(x86_to_x64) &&
            IF_WINDOWS_ELSE(is_wow64_process(NT_CURRENT_PROCESS), false) &&
            instr_get_x86_mode(instr))
            translate_x86_to_x64(dcontext, ilist, &instr);
#endif

#ifdef UNIX
        if (INTERNAL_OPTION(mangle_app_seg) && instr_is_app(instr)) {
            /* The instr might be changed by client, and we cannot rely on
             * PREFIX_SEG_FS/GS. So we simply call mangle_seg_ref on every
             * instruction and mangle it if necessary.
             */
            mangle_seg_ref(dcontext, ilist, instr, next_instr);
            if (instr_get_opcode(instr) == OP_mov_seg)
                mangle_mov_seg(dcontext, ilist, instr, next_instr);
        }
#endif

        if (instr_saves_float_pc(instr) && instr_is_app(instr)) {
            mangle_float_pc(dcontext, ilist, instr, next_instr, flags);
        }

#ifdef X64
        /* i#393: mangle_rel_addr might destroy the instr if it is a LEA,
         * which makes instr point to freed memory.
         * In such case, the control should skip later checks on the instr
         * for exit_cti and syscall.
         * skip the rest of the loop if instr is destroyed.
         */
        if (instr_has_rel_addr_reference(instr)) {
            if (mangle_rel_addr(dcontext, ilist, instr, next_instr))
                continue;
        }
#endif

        if (instr_is_exit_cti(instr)) {
            mangle_exit_cti_prefixes(dcontext, instr);

            /* to avoid reachability problems we convert all
             * 8-bit-offset jumps that exit the fragment to 32-bit.
             * Note that data16 jmps are implicitly converted via the
             * absolute target and loss of prefix info (xref PR 225937).
             */
            if (instr_is_cti_short(instr)) {
                /* convert short jumps */
                convert_to_near_rel(dcontext, instr);
            }
        }

#ifdef ANNOTATIONS
        if (is_annotation_label(instr)) {
            mangle_annotation_helper(dcontext, instr, ilist);
            continue;
        }
#endif

        /* PR 240258: wow64 call* gateway is considered is_syscall */
        if (instr_is_syscall(instr)) {
#ifdef WINDOWS
            /* For XP & 2003, which use sysenter, we process the syscall after all
             * mangling is completed, since we need to insert a reference to the
             * post-sysenter instruction. If that instruction is a 'ret', which
             * we've seen on both os's at multiple patch levels, we'd have a
             * dangling reference since it's deleted in mangle_return(). To avoid
             * that case, we defer syscall processing until mangling is completed.
             */
            if (!ignorable_sysenter)
#endif
                mangle_syscall(dcontext, ilist, *flags, instr, next_instr);
            continue;
        }
        else if (instr_is_interrupt(instr)) { /* non-syscall interrupt */
            mangle_interrupt(dcontext, ilist, instr, next_instr);
            continue;
        }
#ifdef FOOL_CPUID
        else if (instr_get_opcode(instr) == OP_cpuid) {
            mangle_cpuid(dcontext, ilist, instr, next_instr);
            continue;
        }
#endif

        if (!instr_is_cti(instr) || instr_is_meta(instr)) {
#ifdef STEAL_REGISTER
            steal_reg(dcontext, instr, ilist);
#endif
#ifdef CLIENT_INTERFACE
            if (TEST(INSTR_CLOBBER_RETADDR, instr->flags) && instr_is_label(instr)) {
                /* move the value to the note field (which the client cannot
                 * possibly use at this point) so we don't have to search for
                 * this label when we hit the ret instr
                 */
                dr_instr_label_data_t *data = instr_get_label_data_area(instr);
                instr_t *tmp;
                instr_t *ret = (instr_t *) data->data[0];
                CLIENT_ASSERT(ret != NULL,
                              "dr_clobber_retaddr_after_read()'s label is corrupted");
                /* avoid use-after-free if client removed the ret by ensuring
                 * this instr_t pointer does exist.
                 * note that we don't want to go searching based just on a flag
                 * as we want tight coupling w/ a pointer as a general way
                 * to store per-instr data outside of the instr itself.
                 */
                for (tmp = instr_get_next(instr); tmp != NULL; tmp = instr_get_next(tmp)) {
                    if (tmp == ret) {
                        tmp->note = (void *) data->data[1]; /* the value to use */
                        break;
                    }
                }
            }
#endif
            continue;
        }

#ifdef STEAL_REGISTER
        if (ilist->flags) {
            restore_state(dcontext, instr, ilist); /* end of edi calculation */
        }
#endif

        if (instr_is_call_direct(instr)) {
            /* mangle_direct_call may inline a call and remove next_instr, so
             * it passes us the updated next instr */
            next_instr = mangle_direct_call(dcontext, ilist, instr, next_instr,
                                            mangle_calls, *flags);
        } else if (instr_is_call_indirect(instr)) {
            mangle_indirect_call(dcontext, ilist, instr, next_instr, mangle_calls,
                                 *flags);
        } else if (instr_is_return(instr)) {
            mangle_return(dcontext, ilist, instr, next_instr, *flags);
        } else if (instr_is_mbr(instr)) {
            mangle_indirect_jump(dcontext, ilist, instr, next_instr, *flags);
        } else if (instr_get_opcode(instr) == OP_jmp_far) {
            mangle_far_direct_jump(dcontext, ilist, instr, next_instr, *flags);
        }
        /* else nothing to do, e.g. direct branches */
    }

#ifdef WINDOWS
    /* Do XP & 2003 ignore-syscalls processing now. */
    if (ignorable_sysenter) {
        /* Check for any syscalls and process them. */
        for (instr = instrlist_first(ilist);
             instr != NULL;
             instr = next_instr) {
            next_instr = instr_get_next(instr);
            if (instr_opcode_valid(instr) && instr_is_syscall(instr))
                mangle_syscall(dcontext, ilist, *flags, instr, next_instr);
        }
    }
#endif
    if (record_translation)
        instrlist_set_translation_target(ilist, NULL);
    instrlist_set_our_mangling(ilist, false); /* PR 267260 */

#if defined(X86) && defined(X64)
    if (!X64_CACHE_MODE_DC(dcontext)) {
        instr_t *in;
        for (in = instrlist_first(ilist); in != NULL; in = instr_get_next(in)) {
            if (instr_is_our_mangling(in)) {
                instr_set_x86_mode(in, true/*x86*/);
                instr_shrink_to_32_bits(in);
            }
        }
    }
#endif

    /* The following assertion should be guaranteed by fact that all
     * blocks end in some kind of branch, and the code above restores
     * the register state on a branch. */
    ASSERT(ilist->flags == 0);
    KSTOP(mangling);
}

/* END OF CONTROL-FLOW MANGLING ROUTINES
 *###########################################################################
 *###########################################################################
 */

/* SELF-MODIFYING-CODE SANDBOXING
 *
 * When we detect it, we take an exit that targets our own routine
 * fragment_self_write.  Dispatch checks for that target and if it finds it,
 * it calls that routine, so don't worry about building a bb for it.
 * Returns false if the bb has invalid instrs in the middle and it should
 * be rebuilt from scratch.
 */

#undef SAVE_TO_DC_OR_TLS
#undef RESTORE_FROM_DC_OR_TLS
/* PR 244737: x64 uses tls to avoid reachability issues w/ absolute addresses */
#ifdef X64
# define SAVE_TO_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_save_to_tls((dc), (reg), (tls_offs))
# define RESTORE_FROM_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_restore_from_tls((dc), (reg), (tls_offs))
#else
# define SAVE_TO_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_save_to_dcontext((dc), (reg), (dc_offs))
# define RESTORE_FROM_DC_OR_TLS(dc, reg, tls_offs, dc_offs) \
    instr_create_restore_from_dcontext((dc), (reg), (dc_offs))
#endif

static void
sandbox_rep_instr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, instr_t *next,
                  app_pc start_pc, app_pc end_pc /* end is open */)
{
    /* put checks before instr, set some reg as a flag, act on it
     * after instr (even if overwrite self will execute rep to completion)
     * want to read DF to find direction (0=inc xsi/xdi, 1=dec),
     * but only way to read is to do a pushf!
     * Solution: if cld or std right before rep instr, use that info,
     * otherwise check for BOTH directions!
     * xcx is a pre-check, xsi/xdi are inc/dec after memory op, so
     * xdi+xcx*opndsize == instr of NEXT write, so open-ended there:
     * if DF==0:
     *   if (xdi < end_pc && xdi+xcx*opndsize > start_pc) => self-write
     * if DF==1:
     *   if (xdi > start_pc && xdi-xcx*opndsize > end_pc) => self-write
     * both:
     *   if (xdi-xcx*opndsize < end_pc && xdi+xcx*opndsize > start_pc) => self-write
     * opndsize is 1,2, or 4 => use lea for mul
     *   lea (xdi,xcx,opndsize),xcx
     *
     *   save flags and xax
     *   save xbx
     *   lea (xdi,xcx,opndsize),xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): save xdx
     * if x64 && start_pc > 4GB: mov start_pc, xdx
     *   cmp xbx, IF_X64_>4GB_ELSE(xdx, start_pc)
     *   mov $0,xbx # for if ok
     *   jle ok # open b/c address of next rep write
     *   lea (,xcx,opndsize),xbx
     *   neg xbx # sub does dst - src
     *   add xdi,xbx
     * if x64 && end_pc > 4GB: mov end_pc, xdx
     *   cmp xbx, IF_X64_>4GB_ELSE(xdx, end_pc)
     *   mov $0,xbx # for if ok
     *   jge ok    # end is open
     *   mov $1,xbx
     * ok:
     *   restore flags and xax (xax used by stos)
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xdx
     *   <rep instr> # doesn't use xbx
     *     (PR 267764/i#398: we special-case restore xbx on cxt xl8 if this instr faults)
     *   mov xbx,xcx # we can use xcx, it's dead since 0 after rep
     *   restore xbx
     *   jecxz ok2  # if xbx was 1 we'll fall through and exit
     *   mov $0,xcx
     *   jmp <instr after write, flag as INSTR_BRANCH_SPECIAL_EXIT>
     * ok2:
     *   <label> # ok2 can't == next, b/c next may be ind br -> mangled w/ instrs
     *           # inserted before it, so jecxz would target too far
     */
    instr_t *ok = INSTR_CREATE_label(dcontext);
    instr_t *ok2 = INSTR_CREATE_label(dcontext);
    instr_t *jmp;
    app_pc after_write;
    uint opndsize = opnd_size_in_bytes(opnd_get_size(instr_get_dst(instr,0)));
    uint flags =
        instr_eflags_to_fragment_eflags(forward_eflags_analysis(dcontext, ilist, next));
    bool use_tls = IF_X64_ELSE(true, false);
    IF_X64(bool x86_to_x64_ibl_opt = DYNAMO_OPTION(x86_to_x64_ibl_opt);)
    instr_t *next_app = next;
    DOLOG(3, LOG_INTERP, { loginst(dcontext, 3, instr, "writes memory"); });

    ASSERT(!instr_is_call_indirect(instr)); /* FIXME: can you have REP on on CALL's */

    /* skip meta instrs to find next app instr (xref PR 472190) */
    while (next_app != NULL && instr_is_meta(next_app))
        next_app = instr_get_next(next_app);

    if (next_app != NULL) {
        /* client may have inserted non-meta instrs, so use translation first
         * (xref PR 472190)
         */
        if (instr_get_app_pc(next_app) != NULL)
            after_write = instr_get_app_pc(next_app);
        else if (!instr_raw_bits_valid(next_app)) {
            /* next must be the final jmp! */
            ASSERT(instr_is_ubr(next_app) && instr_get_next(next_app) == NULL);
            after_write = opnd_get_pc(instr_get_target(next_app));
        } else
            after_write = instr_get_raw_bits(next_app);
    } else {
        after_write = end_pc;
    }

    insert_save_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls
                       _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                               !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_XDI, REG_XCX, opndsize, 0, OPSZ_lea)));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
    }
    if ((ptr_uint_t)start_pc > UINT_MAX) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                 OPND_CREATE_INTPTR(start_pc)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XDX)));
    } else {
#endif
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(0)));
    PRE(ilist, instr,
        INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(ok)));
    PRE(ilist, instr,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_NULL, REG_XCX, opndsize, 0, OPSZ_lea)));
    PRE(ilist, instr,
        INSTR_CREATE_neg(dcontext, opnd_create_reg(REG_XBX)));
    PRE(ilist, instr,
        INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_reg(REG_XDI)));
#ifdef X64
    if ((ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDX),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XDX)));
    } else {
#endif
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)end_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32(0)));
    PRE(ilist, instr,
        INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(ok)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX), OPND_CREATE_INT32(1)));
    PRE(ilist, instr, ok);
    insert_restore_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls
                          _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                  !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, instr,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XDX, TLS_XDX_SLOT, XDX_OFFSET));
    }
#endif
    /* instr goes here */
    PRE(ilist, next,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX), opnd_create_reg(REG_XBX)));
    PRE(ilist, next,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    PRE(ilist, next,
        INSTR_CREATE_jecxz(dcontext, opnd_create_instr(ok2)));
    PRE(ilist, next,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                             OPND_CREATE_INT32(0))); /* on x64 top 32 bits zeroed */
    jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(after_write));
    instr_branch_set_special_exit(jmp, true);
    /* an exit cti, not a meta instr */
    instrlist_preinsert(ilist, next, jmp);
    PRE(ilist, next, ok2);
}

static void
sandbox_write(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr, instr_t *next,
              opnd_t op, app_pc start_pc, app_pc end_pc /* end is open */)
{
    /* can only test for equality w/o modifying flags, so save them
     * if (addr < end_pc && addr+opndsize > start_pc) => self-write
     *   <write memory>
     *   save xbx
     *   lea memory,xbx
     *   save flags and xax # after lea of memory in case memory includes xax
     * if x64 && (start_pc > 4GB || end_pc > 4GB): save xcx
     * if x64 && end_pc > 4GB: mov end_pc, xcx
     *   cmp xbx, IF_X64_>4GB_ELSE(xcx, end_pc)
     *   jge ok    # end is open
     *   lea opndsize(xbx),xbx
     * if x64 && start_pc > 4GB: mov start_pc, xcx
     *   cmp xbx, IF_X64_>4GB_ELSE(xcx, start_pc)
     *   jle ok    # open since added size
     *   restore flags (using xbx) and xax
     *   restore xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xcx
     *   jmp <instr after write, flag as INSTR_BRANCH_SPECIAL_EXIT>
     * ok:
     *   restore flags and xax
     *   restore xbx
     * if x64 && (start_pc > 4GB || end_pc > 4GB): restore xcx
     */
    instr_t *ok = INSTR_CREATE_label(dcontext), *jmp;
    app_pc after_write = NULL;
    uint opndsize = opnd_size_in_bytes(opnd_get_size(op));
    uint flags =
        instr_eflags_to_fragment_eflags(forward_eflags_analysis(dcontext, ilist, next));
    bool use_tls = IF_X64_ELSE(true, false);
    IF_X64(bool x86_to_x64_ibl_opt = DYNAMO_OPTION(x86_to_x64_ibl_opt);)
    instr_t *next_app = next;
    instr_t *get_addr_at = next;
    int opcode = instr_get_opcode(instr);
    DOLOG(3, LOG_INTERP, { loginst(dcontext, 3, instr, "writes memory"); });

    /* skip meta instrs to find next app instr (xref PR 472190) */
    while (next_app != NULL && instr_is_meta(next_app))
        next_app = instr_get_next(next_app);

    if (next_app != NULL) {
        /* client may have inserted non-meta instrs, so use translation first
         * (xref PR 472190)
         */
        if (instr_get_app_pc(next_app) != NULL)
            after_write = instr_get_app_pc(next_app);
        else if (!instr_raw_bits_valid(next_app)) {
            /* next must be the final artificially added jmp! */
            ASSERT(instr_is_ubr(next_app) && instr_get_next(next_app) == NULL);
            /* for sure this is the last jmp out, but it
             * doesn't have to be a direct jmp but instead
             * it could be the exit branch we add as an
             * for an indirect call - which is the only ind branch
             * that writes to memory
             * CALL* already means that we're leaving the block and it cannot be a selfmod
             * instruction even though it writes to memory
             */
            DOLOG(4, LOG_INTERP, {
                loginst(dcontext, 4, next_app, "next app instr");
            });
            after_write = opnd_get_pc(instr_get_target(next_app));
            LOG(THREAD, LOG_INTERP, 4, "after_write = "PFX" next should be final jmp\n",
                after_write);
        } else
            after_write = instr_get_raw_bits(next_app);
    } else {
        ASSERT_NOT_TESTED();
        after_write = end_pc;
    }

    if (opcode == OP_ins || opcode == OP_movs || opcode == OP_stos) {
        /* These instrs modify their own addressing register so we must
         * get the address pre-write.  None of them touch xbx.
         */
        get_addr_at = instr;
        ASSERT(!instr_writes_to_reg(instr, REG_XBX) &&
               !instr_reads_from_reg(instr, REG_XBX));
    }

    PRE(ilist, get_addr_at,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
    /* XXX: Basically reimplementing drutil_insert_get_mem_addr(). */
    /* FIXME i#986: Sandbox far writes.  Not a hypothetical problem!  NaCl uses
     * segments for its x86 sandbox, although they are 0 based with a limit.
     * qq.exe has them in sandboxed code.
     */
    ASSERT_CURIOSITY(!opnd_is_far_memory_reference(op) ||
                     /* Standard far refs */
                     opcode == OP_ins || opcode == OP_movs || opcode == OP_stos);
    if (opnd_is_base_disp(op)) {
        /* change to OPSZ_lea for lea */
        opnd_set_size(&op, OPSZ_lea);
        PRE(ilist, get_addr_at,
            INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX), op));
        if ((opcode == OP_push && opnd_is_base_disp(op) &&
             opnd_get_index(op) == DR_REG_NULL &&
             reg_to_pointer_sized(opnd_get_base(op)) == DR_REG_XSP) ||
            opcode == OP_push_imm || opcode == OP_pushf || opcode == OP_pusha ||
            opcode == OP_pop /* pop into stack slot */ ||
            opcode == OP_call || opcode == OP_call_ind || opcode == OP_call_far ||
            opcode == OP_call_far_ind) {
            /* Undo xsp adjustment made by the instruction itself.
             * We could use get_addr_at to acquire the address pre-instruction
             * for some of these, but some can read or write ebx.
             */
            PRE(ilist, next,
                INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                                 opnd_create_base_disp(REG_NULL, REG_XBX,
                                                       1, -opnd_get_disp(op), OPSZ_lea)));
        }
    } else {
        /* handle abs addr pointing within fragment */
        /* XXX: Can optimize this by doing address comparison at translation
         * time.  Might happen frequently if a JIT stores data on the same page
         * as its code.  For now we hook into existing sandboxing code.
         */
        app_pc abs_addr;
        ASSERT(opnd_is_abs_addr(op) IF_X64( || opnd_is_rel_addr(op)));
        abs_addr = opnd_get_addr(op);
        PRE(ilist, get_addr_at,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX),
                                 OPND_CREATE_INTPTR(abs_addr)));
    }
    insert_save_eflags(dcontext, ilist, next, flags, use_tls, !use_tls
                       _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                               !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
    if ((ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XCX)));
    } else {
#endif
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)end_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, next,
        INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(ok)));
    PRE(ilist, next,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XBX),
                         opnd_create_base_disp(REG_XBX, REG_NULL, 0,
                                               opndsize, OPSZ_lea)));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX) {
        PRE(ilist, next,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(start_pc)));
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             opnd_create_reg(REG_XCX)));
    } else {
#endif
        PRE(ilist, next,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XBX),
                             OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
    }
#endif
    PRE(ilist, next,
        INSTR_CREATE_jcc(dcontext, OP_jle, opnd_create_instr(ok)));
    insert_restore_eflags(dcontext, ilist, next, flags, use_tls, !use_tls
                          _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                  !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    PRE(ilist, next,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
#endif
    jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(after_write));
    instr_branch_set_special_exit(jmp, true);
    /* an exit cti, not a meta instr */
    instrlist_preinsert(ilist, next, jmp);
    PRE(ilist, next, ok);
    insert_restore_eflags(dcontext, ilist, next, flags, use_tls, !use_tls
                          _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                  !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    PRE(ilist, next,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XBX, TLS_XBX_SLOT, XBX_OFFSET));
#ifdef X64
    if ((ptr_uint_t)start_pc > UINT_MAX || (ptr_uint_t)end_pc > UINT_MAX) {
        PRE(ilist, next,
            RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
#endif
}

static bool
sandbox_top_of_bb_check_s2ro(dcontext_t *dcontext, app_pc start_pc)
{
    return (DYNAMO_OPTION(sandbox2ro_threshold) > 0 &&
            /* we can't make stack regions ro so don't put in the instrumentation */
            !is_address_on_stack(dcontext, start_pc) &&
            /* case 9098 we don't want to ever make RO untrackable driver areas */
            !is_driver_address(start_pc));
}

static void
sandbox_top_of_bb(dcontext_t *dcontext, instrlist_t *ilist,
                  bool s2ro, uint flags,
                  app_pc start_pc, app_pc end_pc, /* end is open */
                  bool for_cache,
                  /* for obtaining the two patch locations: */
                  patch_list_t *patchlist,
                  cache_pc *copy_start_loc, cache_pc *copy_end_loc)
{
    /* add check at top of ilist that compares actual app instructions versus
     * copy we saved, stored in cache right after fragment itself.  leave its
     * start address blank here, will be touched up after emitting this ilist.
     *
     * FIXME case 8165/PR 212600: optimize this: move reg restores to
     * custom fcache_return, use cmpsd instead of cmpsb, etc.
     *
     * if eflags live entering this bb:
     *   save xax
     *   lahf
     *   seto  %al
     * endif
     * if (-sandbox2ro_threshold > 0)
     *  if x64: save xcx
     *     incl  &vm_area_t->exec_count (for x64, via xcx)
     *     cmp   sandbox2ro_threshold, vm_area_t->exec_count (for x64, via xcx)
     *  if eflags live entering this bb, or x64:
     *     jl    past_threshold
     *   if x64: restore xcx
     *   if eflags live entering this bb:
     *     jmp restore_eflags_and_exit
     *   else
     *     jmp   start_pc marked as selfmod exit
     *   endif
     *   past_threshold:
     *  else
     *     jge   start_pc marked as selfmod exit
     *  endif
     * endif
     * if (-sandbox2ro_threshold == 0) && !x64)
     *   save xcx
     * endif
     *   save xsi
     *   save xdi
     * if stats:
     *   inc num_sandbox_execs stat (for x64, via xsi)
     * endif
     *   mov start_pc,xsi
     *   mov copy_start_pc,xdi  # 1 opcode byte, then offset
     *       # => patch point 1
     *   cmpsb
     * if copy_size > 1 # not an opt: for correctness: if "repe cmpsb" has xcx==0, it
     *                  # doesn't touch eflags and we treat cmp results as cmpsb results
     *     jne check_results
     *   if x64 && start_pc > 4GB
     *     mov start_pc, xcx
     *     cmp xsi, xcx
     *   else
     *     cmp xsi, start_pc
     *   endif
     *     mov copy_size-1, xcx # -1 b/c we already checked 1st byte
     *     jge forward
     *     mov copy_end_pc, xdi
     *         # => patch point 2
     *     mov end_pc, xsi
     *   forward:
     *     repe cmpsb
     * endif # copy_size > 1
     *   check_results:
     *     restore xcx
     *     restore xsi
     *     restore xdi
     * if eflags live:
     *   je start_bb
     *  restore_eflags_and_exit:
     *   add   $0x7f,%al
     *   sahf
     *   restore xax
     *   jmp start_pc marked as selfmod exit
     * else
     *   jne start_pc marked as selfmod exit
     * endif
     * start_bb:
     * if eflags live:
     *   add   $0x7f,%al
     *   sahf
     *   restore xax
     * endif
     */
    instr_t *instr, *jmp;
    instr_t *restore_eflags_and_exit = NULL;
    bool use_tls = IF_X64_ELSE(true, false);
    IF_X64(bool x86_to_x64_ibl_opt = DYNAMO_OPTION(x86_to_x64_ibl_opt);)
    bool saved_xcx = false;
    instr_t *check_results = INSTR_CREATE_label(dcontext);

    instr = instrlist_first_expanded(dcontext, ilist);

    insert_save_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls
                       _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                               !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));

    if (s2ro) {
        /* It's difficult to use lea/jecxz here as we want to use a shared
         * counter but no lock, and thus need a relative comparison, while
         * lea/jecxz can only do an exact comparison.  We could be exact by
         * having a separate counter per (private) fragment but by spilling
         * eflags we can inc memory, making the scheme here not inefficient.
         */
        uint thresh = DYNAMO_OPTION(sandbox2ro_threshold);
        uint *counter;
        if (for_cache)
            counter = get_selfmod_exec_counter(start_pc);
        else {
            /* Won't find exec area since not a real fragment (probably
             * a recreation post-flush).  Won't execute, so NULL is fine.
             */
            counter = NULL;
        }
#ifdef X64
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
        saved_xcx = true;
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(counter)));
        PRE(ilist, instr,
            INSTR_CREATE_inc(dcontext, OPND_CREATE_MEM32(REG_XCX, 0)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext, OPND_CREATE_MEM32(REG_XCX, 0),
                             OPND_CREATE_INT_32OR8((int)thresh)));
#else
        PRE(ilist, instr,
            INSTR_CREATE_inc(dcontext, OPND_CREATE_ABSMEM(counter, OPSZ_4)));
        PRE(ilist, instr,
            INSTR_CREATE_cmp(dcontext,
                             OPND_CREATE_ABSMEM(counter, OPSZ_4),
                             OPND_CREATE_INT_32OR8(thresh)));
#endif
        if (TEST(FRAG_WRITES_EFLAGS_6, flags) IF_X64(&& false)) {
            jmp = INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_pc(start_pc));
            instr_branch_set_special_exit(jmp, true);
            /* an exit cti, not a meta instr */
            instrlist_preinsert(ilist, instr, jmp);
        } else {
            instr_t *past_threshold = INSTR_CREATE_label(dcontext);
            PRE(ilist, instr,
                INSTR_CREATE_jcc_short(dcontext, OP_jl_short,
                                       opnd_create_instr(past_threshold)));
#ifdef X64
            PRE(ilist, instr,
                RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
#endif
            if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
                ASSERT(restore_eflags_and_exit == NULL);
                restore_eflags_and_exit = INSTR_CREATE_label(dcontext);
                PRE(ilist, instr, INSTR_CREATE_jmp
                    (dcontext, opnd_create_instr(restore_eflags_and_exit)));
            }
#ifdef X64
            else {
                jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(start_pc));
                instr_branch_set_special_exit(jmp, true);
                /* an exit cti, not a meta instr */
                instrlist_preinsert(ilist, instr, jmp);
            }
#endif
            PRE(ilist, instr, past_threshold);
        }
    }

    if (!saved_xcx) {
        PRE(ilist, instr,
            SAVE_TO_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    }
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XSI, TLS_XBX_SLOT, XSI_OFFSET));
    PRE(ilist, instr,
        SAVE_TO_DC_OR_TLS(dcontext, REG_XDI, TLS_XDX_SLOT, XDI_OFFSET));
    DOSTATS({
        if (GLOBAL_STATS_ON()) {
            /* We only do global inc, not bothering w/ thread-private stats.
             * We don't care about races: ballpark figure is good enough.
             * We could do a direct inc of memory for 32-bit.
             */
            PRE(ilist, instr, INSTR_CREATE_mov_imm
                (dcontext, opnd_create_reg(REG_XSI),
                 OPND_CREATE_INTPTR(GLOBAL_STAT_ADDR(num_sandbox_execs))));
            PRE(ilist, instr, INSTR_CREATE_inc
                (dcontext, opnd_create_base_disp(REG_XSI, REG_NULL, 0, 0, OPSZ_STATS)));
         }
    });
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XSI),
                             OPND_CREATE_INTPTR(start_pc)));
    PRE(ilist, instr,
        INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                             /* will become copy start */
                             OPND_CREATE_INTPTR(start_pc)));
    if (patchlist != NULL) {
        ASSERT(copy_start_loc != NULL);
        add_patch_marker(patchlist, instr_get_prev(instr), PATCH_ASSEMBLE_ABSOLUTE,
                         -(short)sizeof(cache_pc), (ptr_uint_t*)copy_start_loc);
    }
    PRE(ilist, instr, INSTR_CREATE_cmps_1(dcontext));
    /* For a 1-byte copy size we cannot use "repe cmpsb" as it won't
     * touch eflags and we'll treat the cmp results as cmpsb results, which
     * doesn't work (cmp will never be equal)
     */
    if (end_pc - start_pc > 1) {
        instr_t *forward = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_instr(check_results)));
#ifdef X64
        if ((ptr_uint_t)start_pc > UINT_MAX) {
            PRE(ilist, instr,
                INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                     OPND_CREATE_INTPTR(start_pc)));
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSI),
                                 opnd_create_reg(REG_XCX)));
        } else {
#endif
            PRE(ilist, instr,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XSI),
                                 OPND_CREATE_INT32((int)(ptr_int_t)start_pc)));
#ifdef X64
        }
#endif
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                 OPND_CREATE_INTPTR(end_pc - (start_pc + 1))));
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_jge, opnd_create_instr(forward)));
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XDI),
                                 /* will become copy end */
                                 OPND_CREATE_INTPTR(end_pc)));
        if (patchlist != NULL) {
            ASSERT(copy_end_loc != NULL);
            add_patch_marker(patchlist, instr_get_prev(instr), PATCH_ASSEMBLE_ABSOLUTE,
                             -(short)sizeof(cache_pc), (ptr_uint_t*)copy_end_loc);
        }
        PRE(ilist, instr,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XSI),
                                 OPND_CREATE_INTPTR(end_pc)));
        PRE(ilist, instr, forward);
        PRE(ilist, instr, INSTR_CREATE_rep_cmps_1(dcontext));
    }
    PRE(ilist, instr, check_results);
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XCX, TLS_XCX_SLOT, XCX_OFFSET));
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XSI, TLS_XBX_SLOT, XSI_OFFSET));
    PRE(ilist, instr,
        RESTORE_FROM_DC_OR_TLS(dcontext, REG_XDI, TLS_XDX_SLOT, XDI_OFFSET));
    if (!TEST(FRAG_WRITES_EFLAGS_6, flags)) {
        instr_t *start_bb = INSTR_CREATE_label(dcontext);
        PRE(ilist, instr,
            INSTR_CREATE_jcc(dcontext, OP_je, opnd_create_instr(start_bb)));
        if (restore_eflags_and_exit != NULL) /* somebody needs this label */
            PRE(ilist, instr, restore_eflags_and_exit);
        insert_restore_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls
                              _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                      !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
        jmp = INSTR_CREATE_jmp(dcontext, opnd_create_pc(start_pc));
        instr_branch_set_special_exit(jmp, true);
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr, jmp);
        PRE(ilist, instr, start_bb);
    } else {
        jmp = INSTR_CREATE_jcc(dcontext, OP_jne, opnd_create_pc(start_pc));
        instr_branch_set_special_exit(jmp, true);
        /* an exit cti, not a meta instr */
        instrlist_preinsert(ilist, instr, jmp);
    }
    insert_restore_eflags(dcontext, ilist, instr, flags, use_tls, !use_tls
                          _IF_X64(X64_CACHE_MODE_DC(dcontext) &&
                                  !X64_MODE_DC(dcontext) && x86_to_x64_ibl_opt));
    /* fall-through to bb start */
}

/* returns false if failed to add sandboxing b/c of a problematic ilist --
 * invalid instrs, elided ctis, etc.
 */
bool
insert_selfmod_sandbox(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
                       app_pc start_pc, app_pc end_pc, /* end is open */
                       bool record_translation, bool for_cache)
{
    instr_t *instr, *next;

    if (!INTERNAL_OPTION(cache_consistency))
        return true; /* nothing to do */

    /* this code assumes bb covers single, contiguous region */
    ASSERT((flags & FRAG_HAS_DIRECT_CTI) == 0);

    /* store first instr so loop below will skip top check */
    instr = instrlist_first_expanded(dcontext, ilist);
    instrlist_set_our_mangling(ilist, true); /* PR 267260 */
    if (record_translation) {
        /* skip client instrumentation, if any, as is done below */
        while (instr != NULL && instr_is_meta(instr))
            instr = instr_get_next_expanded(dcontext, ilist, instr);
        /* make sure inserted instrs translate to the proper original instr */
        ASSERT(instr != NULL && instr_get_translation(instr) != NULL);
        instrlist_set_translation_target(ilist, instr_get_translation(instr));
    }

    sandbox_top_of_bb(dcontext, ilist,
                      sandbox_top_of_bb_check_s2ro(dcontext, start_pc),
                      flags, start_pc, end_pc, for_cache,
                      NULL, NULL, NULL);

    if (INTERNAL_OPTION(sandbox_writes)) {
        for (; instr != NULL; instr = next) {
            int i, opcode;
            opnd_t op;

            opcode = instr_get_opcode(instr);
            if (!instr_valid(instr)) {
                /* invalid instr -- best to truncate block here, easiest way
                 * to do that and get all flags right is to re-build it,
                 * but this time we'll use full decode so we'll avoid the discrepancy
                 * between fast and full decode on invalid instr detection.
                 */
                if (record_translation)
                    instrlist_set_translation_target(ilist, NULL);
                instrlist_set_our_mangling(ilist, false); /* PR 267260 */
                return false;
            }

            /* don't mangle anything that mangle inserts! */
            next = instr_get_next_expanded(dcontext, ilist, instr);
            if (instr_is_meta(instr))
                continue;
            if (record_translation) {
                /* make sure inserted instrs translate to the proper original instr */
                ASSERT(instr_get_translation(instr) != NULL);
                instrlist_set_translation_target(ilist, instr_get_translation(instr));
            }

            if (opcode == OP_rep_ins || opcode == OP_rep_movs || opcode == OP_rep_stos) {
                sandbox_rep_instr(dcontext, ilist, instr, next, start_pc, end_pc);
                continue;
            }

            /* FIXME case 8165: optimize for multiple push/pop */
            for (i=0; i<instr_num_dsts(instr); i++) {
                op = instr_get_dst(instr, i);
                if (opnd_is_memory_reference(op)) {
                    /* ignore CALL* since last anyways */
                    if (instr_is_call_indirect(instr)) {
                        ASSERT(next != NULL && !instr_raw_bits_valid(next));
                        /* FIXME case 8165: why do we ever care about the last
                         * instruction modifying anything?
                         */
                        /* conversion of IAT calls (but not elision)
                         * transforms this into a direct CALL,
                         * in that case 'next' is a direct jmp
                         * fall through, so has no exit flags
                         */
                        ASSERT(EXIT_IS_CALL(instr_exit_branch_type(next)) ||
                               (DYNAMO_OPTION(IAT_convert) &&
                                TEST(INSTR_IND_CALL_DIRECT, instr->flags)));


                        LOG(THREAD, LOG_INTERP, 3, " ignoring CALL* at end of fragment\n");
                        /* This test could be done outside of this loop on
                         * destinations, but since it is rare it is faster
                         * to do it here.  Using continue instead of break in case
                         * it gets moved out.
                         */
                        continue;
                    }
                    if (opnd_is_abs_addr(op) IF_X64(|| opnd_is_rel_addr(op))) {
                        app_pc abs_addr = opnd_get_addr(op);
                        uint size = opnd_size_in_bytes(opnd_get_size(op));
                        if (!POINTER_OVERFLOW_ON_ADD(abs_addr, size) &&
                            (abs_addr + size < start_pc || abs_addr >= end_pc)) {
                            /* This is an absolute memory reference that points
                             * outside the current basic block and doesn't need
                             * sandboxing.
                             */
                            continue;
                        }
                    }
                    sandbox_write(dcontext, ilist, instr, next, op, start_pc, end_pc);
                }
            }
        }
    }
    if (record_translation)
        instrlist_set_translation_target(ilist, NULL);
    instrlist_set_our_mangling(ilist, false); /* PR 267260 */
    return true;
}

/* Offsets within selfmod sandbox top-of-bb code that we patch once
 * the code is emitted, as the values depend on the emitted address.
 * These vary by whether sandbox_top_of_bb_check_s2ro() and whether
 * eflags are not written, all written, or just OF is written.
 * For the copy_size == 1 variation, we simply ignore the 2nd patch point.
 */
static bool selfmod_s2ro[] = { false, true };
static uint selfmod_eflags[] = { FRAG_WRITES_EFLAGS_6, FRAG_WRITES_EFLAGS_OF, 0 };
#define SELFMOD_NUM_S2RO   (sizeof(selfmod_s2ro)/sizeof(selfmod_s2ro[0]))
#define SELFMOD_NUM_EFLAGS (sizeof(selfmod_eflags)/sizeof(selfmod_eflags[0]))
#ifdef X64 /* additional complexity: start_pc > 4GB? */
static app_pc selfmod_gt4G[] = { NULL, (app_pc)(POINTER_MAX-2)/*so end can be +2*/ };
# define SELFMOD_NUM_GT4G   (sizeof(selfmod_gt4G)/sizeof(selfmod_gt4G[0]))
#endif
uint selfmod_copy_start_offs[SELFMOD_NUM_S2RO][SELFMOD_NUM_EFLAGS]
    IF_X64([SELFMOD_NUM_GT4G]);
uint selfmod_copy_end_offs[SELFMOD_NUM_S2RO][SELFMOD_NUM_EFLAGS]
    IF_X64([SELFMOD_NUM_GT4G]);

void
set_selfmod_sandbox_offsets(dcontext_t *dcontext)
{
    int i, j;
#ifdef X64
    int k;
#endif
    instrlist_t ilist;
    patch_list_t patch;
    static byte buf[256];
    uint len;
    /* We assume this is called at init, when .data is +w and we need no
     * synch on accessing buf */
    ASSERT(!dynamo_initialized);
    for (i = 0; i < SELFMOD_NUM_S2RO; i++) {
        for (j = 0; j < SELFMOD_NUM_EFLAGS; j++) {
#ifdef X64
            for (k = 0; k < SELFMOD_NUM_GT4G; k++) {
#endif
                cache_pc start_pc, end_pc;
                app_pc app_start;
                instr_t *inst;
                instrlist_init(&ilist);
                /* sandbox_top_of_bb assumes there's an instr there */
                instrlist_append(&ilist, INSTR_CREATE_label(dcontext));
                init_patch_list(&patch, PATCH_TYPE_ABSOLUTE);
                app_start = IF_X64_ELSE(selfmod_gt4G[k], NULL);
                sandbox_top_of_bb(dcontext, &ilist,
                                  selfmod_s2ro[i], selfmod_eflags[j],
                                  /* we must have a >1-byte region to get
                                   * both patch points */
                                  app_start, app_start + 2, false,
                                  &patch, &start_pc, &end_pc);
                /* The exit cti's may not reachably encode (normally
                 * they'd be mangled away) so we munge them first
                 */
                for (inst = instrlist_first(&ilist); inst != NULL;
                     inst = instr_get_next(inst)) {
                    if (instr_is_exit_cti(inst)) {
                        instr_set_target(inst, opnd_create_pc(buf));
                    }
                }
                len = encode_with_patch_list(dcontext, &patch, &ilist, buf);
                ASSERT(len < BUFFER_SIZE_BYTES(buf));
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(start_pc - buf)));
                selfmod_copy_start_offs[i][j]IF_X64([k]) = (uint) (start_pc - buf);
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(end_pc - buf)));
                selfmod_copy_end_offs[i][j]IF_X64([k]) = (uint) (end_pc - buf);
                LOG(THREAD, LOG_EMIT, 3, "selfmod offs %d %d"IF_X64(" %d")": %u %u\n",
                    i, j, IF_X64_(k)
                    selfmod_copy_start_offs[i][j]IF_X64([k]),
                    selfmod_copy_end_offs[i][j]IF_X64([k]));
                /* free the instrlist_t elements */
                instrlist_clear(dcontext, &ilist);
#ifdef X64
            }
#endif
        }
    }
}

void
finalize_selfmod_sandbox(dcontext_t *dcontext, fragment_t *f)
{
    cache_pc copy_pc = FRAGMENT_SELFMOD_COPY_PC(f);
    byte *pc;
    int i, j;
#ifdef X64
    int k = ((ptr_uint_t)f->tag) > UINT_MAX ? 1 : 0;
#endif
    i = (sandbox_top_of_bb_check_s2ro(dcontext, f->tag)) ? 1 : 0;
    j = (TEST(FRAG_WRITES_EFLAGS_6, f->flags) ? 0 :
         (TEST(FRAG_WRITES_EFLAGS_OF, f->flags) ? 1 : 2));
    pc = FCACHE_ENTRY_PC(f) + selfmod_copy_start_offs[i][j]IF_X64([k]);
    *((cache_pc*)pc) = copy_pc;
    if (FRAGMENT_SELFMOD_COPY_CODE_SIZE(f) > 1) {
        pc = FCACHE_ENTRY_PC(f) + selfmod_copy_end_offs[i][j]IF_X64([k]);
        /* subtract the size itself, stored at the end of the copy */
        *((cache_pc*)pc) = (copy_pc + FRAGMENT_SELFMOD_COPY_CODE_SIZE(f));
    } /* else, no 2nd patch point */
}

/****************************************************************************/
/* clean call optimization code */

/* The max number of instructions try to decode from a function. */
#define MAX_NUM_FUNC_INSTRS 4096
/* the max number of instructions the callee can have for inline. */
#define MAX_NUM_INLINE_INSTRS 20

void
mangle_init(void)
{
    /* create a default func_info for:
     * 1. clean call callee that cannot be analyzed.
     * 2. variable clean_callees will not be updated during the execution
     *    and can be set write protected.
     */
#ifdef CLIENT_INTERFACE
    callee_info_init(&default_callee_info);
    callee_info_table_init();
    clean_call_info_init(&default_clean_call_info, NULL, false, 0);
#endif
}

void
mangle_exit(void)
{
#ifdef CLIENT_INTERFACE
    callee_info_table_destroy();
#endif
}

#ifdef CLIENT_INTERFACE

/* Decode instruction from callee and return the next_pc to be decoded. */
static app_pc
decode_callee_instr(dcontext_t *dcontext, callee_info_t *ci, app_pc instr_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    app_pc   next_pc = NULL;

    instr = instr_create(GLOBAL_DCONTEXT);
    instrlist_append(ilist, instr);
    ci->num_instrs++;
    TRY_EXCEPT(dcontext, {
        next_pc = decode(GLOBAL_DCONTEXT, instr_pc, instr);
    }, { /* EXCEPT */
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: crash on decoding callee instruction at: "PFX"\n",
            instr_pc);
        ASSERT_CURIOSITY(false && "crashed while decoding clean call");
        ci->bailout = true;
        return NULL;
    });
    if (!instr_valid(instr)) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: decoding invalid instruction at: "PFX"\n", instr_pc);
        ci->bailout = true;
        return NULL;
    }
    instr_set_translation(instr, instr_pc);
    DOLOG(3, LOG_CLEANCALL, {
        disassemble_with_bytes(dcontext, instr_pc, THREAD);
    });
    return next_pc;
}

/* check newly decoded instruction from callee */
static app_pc
check_callee_instr(dcontext_t *dcontext, callee_info_t *ci, app_pc next_pc)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    app_pc   cur_pc, tgt_pc;

    if (next_pc == NULL)
        return NULL;
    instr  = instrlist_last(ilist);
    cur_pc = instr_get_app_pc(instr);
    ASSERT(next_pc == cur_pc + instr_length(dcontext, instr));
    if (!instr_is_cti(instr)) {
        /* special instructions, bail out. */
        if (instr_is_syscall(instr) || instr_is_interrupt(instr)) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: bail out on syscall or interrupt at: "PFX"\n",
                cur_pc);
            ci->bailout = true;
            return NULL;
        }
        return next_pc;
    } else { /* cti instruc */
        if (instr_is_mbr(instr)) {
            /* check if instr is return, and if return is the last instr. */
            if (!instr_is_return(instr) || ci->fwd_tgt > cur_pc) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: bail out on indirect branch at: "PFX"\n",
                    cur_pc);
                ci->bailout = true;
            }
            return NULL;
        } else if (instr_is_call(instr)) {
            tgt_pc = opnd_get_pc(instr_get_target(instr));
            /* remove and destroy the call instruction */
            ci->bailout = true;
            instrlist_remove(ilist, instr);
            instr_destroy(GLOBAL_DCONTEXT, instr);
            instr = NULL;
            ci->num_instrs--;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee calls out at: "PFX" to "PFX"\n",
                cur_pc, tgt_pc);
            /* check special PIC code:
             * 1. call next_pc; pop r1;
             * or
             * 2. call pic_func;
             *    and in pic_func: mov [%xsp] %r1; ret;
             */
            if (INTERNAL_OPTION(opt_cleancall) >= 1) {
                instr_t ins;
                app_pc  tmp_pc;
                opnd_t src = OPND_CREATE_INTPTR(next_pc);
                instr_init(dcontext, &ins);
                TRY_EXCEPT(dcontext, {
                    tmp_pc = decode(dcontext, tgt_pc, &ins);
                }, {
                    ASSERT_CURIOSITY(false &&
                                     "crashed while decoding clean call");
                    instr_free(dcontext, &ins);
                    return NULL;
                });
                DOLOG(3, LOG_CLEANCALL, {
                    disassemble_with_bytes(dcontext, tgt_pc, THREAD);
                });
                /* "pop %r1" or "mov [%rsp] %r1" */
                if (!(((instr_get_opcode(&ins) == OP_pop) ||
                       (instr_get_opcode(&ins) == OP_mov_ld &&
                        opnd_same(instr_get_src(&ins, 0),
                                  OPND_CREATE_MEMPTR(REG_XSP, 0)))) &&
                      opnd_is_reg(instr_get_dst(&ins, 0)))) {
                    LOG(THREAD, LOG_CLEANCALL, 2,
                        "CLEANCALL: callee calls out is not PIC code, "
                        "bailout\n");
                    instr_free(dcontext, &ins);
                    return NULL;
                }
                /* replace with "mov next_pc r1" */
                /* XXX: the memory on top of stack will not be next_pc. */
                instr = INSTR_CREATE_mov_imm
                    (GLOBAL_DCONTEXT, instr_get_dst(&ins, 0), src);
                instr_set_translation(instr, cur_pc);
                instrlist_append(ilist, instr);
                ci->num_instrs++;
                instr_reset(dcontext, &ins);
                if (tgt_pc != next_pc) { /* a callout */
                    TRY_EXCEPT(dcontext, {
                        tmp_pc = decode(dcontext, tmp_pc, &ins);
                    }, {
                        ASSERT_CURIOSITY(false &&
                                         "crashed while decoding clean call");
                        instr_free(dcontext, &ins);
                        return NULL;
                    });
                    if (!instr_is_return(&ins)) {
                        instr_free(dcontext, &ins);
                        return NULL;
                    }
                    instr_free(dcontext, &ins);
                }
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: special PIC code at: "PFX"\n",
                    cur_pc);
                ci->bailout = false;
                instr_free(dcontext, &ins);
                if (tgt_pc == next_pc)
                    return tmp_pc;
                else
                    return next_pc;
            }
        } else { /* ubr or cbr */
            tgt_pc = opnd_get_pc(instr_get_target(instr));
            if (tgt_pc < cur_pc) { /* backward branch */
                if (tgt_pc < ci->start) {
                    LOG(THREAD, LOG_CLEANCALL, 2,
                        "CLEANCALL: bail out on out-of-range branch at: "PFX
                        "to "PFX"\n", cur_pc, tgt_pc);
                    ci->bailout = true;
                    return NULL;
                } else if (ci->bwd_tgt == NULL || tgt_pc < ci->bwd_tgt) {
                    ci->bwd_tgt = tgt_pc;
                }
            } else { /* forward branch */
                if (ci->fwd_tgt == NULL || tgt_pc > ci->fwd_tgt) {
                    ci->fwd_tgt = tgt_pc;
                }
            }
        }
    }
    return next_pc;
}

static void
check_callee_ilist(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *cti, *tgt, *ret;
    app_pc   tgt_pc;
    if (!ci->bailout) {
        /* no target pc of any branch is in a middle of an instruction,
         * replace target pc with target instr
         */
        ret = instrlist_last(ilist);
        /* must be RETURN, otherwise, bugs in decode_callee_ilist */
        ASSERT(instr_is_return(ret));
        for (cti  = instrlist_first(ilist);
             cti != ret;
             cti  = instr_get_next(cti)) {
            if (!instr_is_cti(cti))
                continue;
            ASSERT(!instr_is_mbr(cti));
            tgt_pc = opnd_get_pc(instr_get_target(cti));
            for (tgt  = instrlist_first(ilist);
                 tgt != NULL;
                 tgt  = instr_get_next(tgt)) {
                if (tgt_pc == instr_get_app_pc(tgt))
                    break;
            }
            if (tgt == NULL) {
                /* cannot find a target instruction, bail out */
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: bail out on strange internal branch at: "PFX
                    "to "PFX"\n", instr_get_app_pc(cti), tgt_pc);
                ci->bailout = true;
                break;
            }
        }
        /* remove RETURN as we do not need it any more */
        instrlist_remove(ilist, ret);
        instr_destroy(GLOBAL_DCONTEXT, ret);
    }
    if (ci->bailout) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ilist);
        ci->ilist = NULL;
    }
}

static void
decode_callee_ilist(dcontext_t *dcontext, callee_info_t *ci)
{
    app_pc cur_pc;

    ci->ilist = instrlist_create(GLOBAL_DCONTEXT);
    cur_pc = ci->start;

    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: decoding callee starting at: "PFX"\n", ci->start);
    ci->bailout = false;
    while (cur_pc != NULL) {
        cur_pc = decode_callee_instr(dcontext, ci, cur_pc);
        cur_pc = check_callee_instr(dcontext, ci, cur_pc);
    }
    check_callee_ilist(dcontext, ci);
}

static void
analyze_callee_regs_usage(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *instr;
    uint i, num_regparm;

    ci->num_xmms_used = 0;
    memset(ci->xmm_used, 0, sizeof(bool) * NUM_XMM_REGS);
    memset(ci->reg_used, 0, sizeof(bool) * NUM_GP_REGS);
    ci->write_aflags = false;
    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        /* XXX: this is not efficient as instr_uses_reg will iterate over
         * every operands, and the total would be (NUM_REGS * NUM_OPNDS)
         * for each instruction. However, since this will be only called
         * once for each clean call callee, it will have little performance
         * impact unless there are a lot of different clean call callees.
         */
        /* XMM registers usage */
        for (i = 0; i < NUM_XMM_REGS; i++) {
            if (!ci->xmm_used[i] &&
                instr_uses_reg(instr, (DR_REG_XMM0 + (reg_id_t)i))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses XMM%d at "PFX"\n",
                    ci->start, i, instr_get_app_pc(instr));
                ci->xmm_used[i] = true;
                ci->num_xmms_used++;
            }
        }
        /* General purpose registers */
        for (i = 0; i < NUM_GP_REGS; i++) {
            reg_id_t reg = DR_REG_XAX + (reg_id_t)i;
            if (!ci->reg_used[i] &&
                /* Later we'll rewrite stack accesses to not use XSP or XBP. */
                reg != DR_REG_XSP &&
                (reg != DR_REG_XBP || !ci->xbp_is_fp) &&
                instr_uses_reg(instr, reg)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" uses REG %s at "PFX"\n",
                    ci->start, reg_names[reg],
                    instr_get_app_pc(instr));
                ci->reg_used[i] = true;
                callee_info_reserve_slot(ci, SLOT_REG, reg);
            }
        }
        /* callee update aflags */
        if (!ci->write_aflags) {
            if (TESTANY(EFLAGS_WRITE_6, instr_get_arith_flags(instr))) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: callee "PFX" updates aflags\n", ci->start);
                ci->write_aflags = true;
            }
        }
    }

    /* check if callee read aflags from caller */
    /* set it false for the case of empty callee. */
    ci->read_aflags = false;
    for (instr  = instrlist_first(ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        uint flags = instr_get_arith_flags(instr);
        if (TESTANY(EFLAGS_READ_6, flags)) {
            ci->read_aflags = true;
            break;
        }
        if (TESTALL(EFLAGS_WRITE_6, flags))
            break;
        if (instr_is_return(instr))
            break;
        if (instr_is_cti(instr)) {
            ci->read_aflags = true;
            break;
        }
    }
    if (ci->read_aflags) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" reads aflags from caller\n", ci->start);
    }

    /* If we read or write aflags, we need to reserve a slot to save them.
     * We may or may not use the slot at the call site, but it needs to be
     * reserved just in case.
     */
    if (ci->read_aflags || ci->write_aflags) {
        /* XXX: We can optimize away the flags spill to memory if the callee
         * does not use xax.
         */
        callee_info_reserve_slot(ci, SLOT_FLAGS, 0);
        /* Spilling flags clobbers xax, so we need to spill the app xax first.
         * If the callee used xax, then the slot will already be reserved.
         */
        if (!ci->reg_used[DR_REG_XAX - DR_REG_XAX]) {
            callee_info_reserve_slot(ci, SLOT_REG, DR_REG_XAX);
        }
    }

    /* i#987, i#988: reg might be used for arg passing but not used in callee */
    num_regparm = MIN(ci->num_args, NUM_REGPARM);
    for (i = 0; i < num_regparm; i++) {
        reg_id_t reg = regparms[i];
        if (!ci->reg_used[reg - DR_REG_XAX]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" uses REG %s for arg passing\n",
                ci->start, reg_names[reg]);
            ci->reg_used[reg - DR_REG_XAX] = true;
            callee_info_reserve_slot(ci, SLOT_REG, reg);
        }
    }
}

/* We use push/pop pattern to detect callee saved registers,
 * and assume that the code later won't change those saved value
 * on the stack.
 */
static void
analyze_callee_save_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    instrlist_t *ilist = ci->ilist;
    instr_t *top, *bot, *push_xbp, *pop_xbp, *instr, *enter, *leave;

    ASSERT(ilist != NULL);
    ci->num_callee_save_regs = 0;
    top = instrlist_first(ilist);
    bot = instrlist_last(ilist);
    if (top == bot) {
        /* zero or one instruction only, no callee save */
        return;
    }
    /* 1. frame pointer usage analysis. */
    /* i#392-c#4: frame pointer code might be in the middle
     * 0xf771f390 <compiler_inscount>:      call   0xf7723a19 <get_pc_thunk>
     * 0xf771f395 <compiler_inscount+5>:    add    $0x6c5f,%ecx
     * 0xf771f39b <compiler_inscount+11>:   push   %ebp
     * 0xf771f39c <compiler_inscount+12>:   mov    %esp,%ebp
     * 0xf771f39e <compiler_inscount+14>:   mov    0x8(%ebp),%eax
     * 0xf771f3a1 <compiler_inscount+17>:   pop    %ebp
     * 0xf771f3a2 <compiler_inscount+18>:   add    %eax,0x494(%ecx)
     * 0xf771f3a8 <compiler_inscount+24>:   ret
     */
    /* for easy of comparison, create push xbp, pop xbp */
    push_xbp = INSTR_CREATE_push(dcontext, opnd_create_reg(DR_REG_XBP));
    pop_xbp  = INSTR_CREATE_pop(dcontext, opnd_create_reg(DR_REG_XBP));
    /* i#392-c#4: search for frame enter/leave pair */
    enter = NULL;
    leave = NULL;
    for (instr = top; instr != bot; instr = instr_get_next(instr)) {
        if (instr_get_opcode(instr) == OP_enter ||
            instr_same(push_xbp, instr)) {
            enter = instr;
            break;
        }
    }
    if (enter != NULL) {
        for (instr = bot; instr != enter; instr = instr_get_prev(instr)) {
            if (instr_get_opcode(instr) == OP_leave ||
                instr_same(pop_xbp, instr)) {
                leave = instr;
                break;
            }
        }
    }
    /* Check enter/leave pair  */
    if (enter != NULL && leave != NULL &&
        (ci->bwd_tgt == NULL || instr_get_app_pc(enter) <  ci->bwd_tgt) &&
        (ci->fwd_tgt == NULL || instr_get_app_pc(leave) >= ci->fwd_tgt)) {
        /* check if xbp is fp */
        if (instr_get_opcode(enter) == OP_enter) {
            ci->xbp_is_fp = true;
        } else {
            /* i#392-c#2: mov xsp => xbp might not be right after push_xbp */
            for (instr  = instr_get_next(enter);
                 instr != leave;
                 instr  = instr_get_next(instr)) {
                if (instr != NULL &&
                    /* we want to use instr_same to find "mov xsp => xbp",
                     * but it could be OP_mov_ld or OP_mov_st, so use opnds
                     * for comparison instead.
                     */
                    instr_num_srcs(instr) == 1 &&
                    instr_num_dsts(instr) == 1 &&
                    opnd_is_reg(instr_get_src(instr, 0)) &&
                    opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_XSP &&
                    opnd_is_reg(instr_get_dst(instr, 0)) &&
                    opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_XBP) {
                    /* found mov xsp => xbp */
                    ci->xbp_is_fp = true;
                    /* remove it */
                    instrlist_remove(ilist, instr);
                    instr_destroy(GLOBAL_DCONTEXT, instr);
                    break;
                }
            }
        }
        if (ci->xbp_is_fp) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" use XBP as frame pointer\n", ci->start);
        } else {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: callee "PFX" callee-saves reg xbp at "PFX" and "PFX"\n",
                ci->start, instr_get_app_pc(enter), instr_get_app_pc(leave));
            ci->callee_save_regs
                [DR_REG_XBP - DR_REG_XAX] = true;
            ci->num_callee_save_regs++;
        }
        /* remove enter/leave or push/pop xbp pair */
        instrlist_remove(ilist, enter);
        instrlist_remove(ilist, leave);
        instr_destroy(GLOBAL_DCONTEXT, enter);
        instr_destroy(GLOBAL_DCONTEXT, leave);
        top = instrlist_first(ilist);
        bot = instrlist_last(ilist);
    }
    instr_destroy(dcontext, push_xbp);
    instr_destroy(dcontext, pop_xbp);

    /* get the rest callee save regs */
    /* XXX: the callee save may be corrupted by memory update on the stack. */
    /* XXX: the callee save may use mov instead of push/pop */
    while (top != NULL && bot != NULL) {
        /* if not in the first/last bb, break */
        if ((ci->bwd_tgt != NULL && instr_get_app_pc(top) >= ci->bwd_tgt) ||
            (ci->fwd_tgt != NULL && instr_get_app_pc(bot) <  ci->fwd_tgt) ||
            instr_is_cti(top) || instr_is_cti(bot))
            break;
        /* XXX: I saw some compiler inserts nop, need to handle. */
        /* push/pop pair check */
        if (instr_get_opcode(top) != OP_push ||
            instr_get_opcode(bot) != OP_pop  ||
            !opnd_same(instr_get_src(top, 0), instr_get_dst(bot, 0)) ||
            !opnd_is_reg(instr_get_src(top, 0)) ||
            opnd_get_reg(instr_get_src(top, 0)) == REG_XSP)
            break;
        /* It is a callee saved reg, we will do our own save for it. */
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" callee-saves reg %s at "PFX" and "PFX"\n",
            ci->start, reg_names[opnd_get_reg(instr_get_src(top, 0))],
            instr_get_app_pc(top), instr_get_app_pc(bot));
        ci->callee_save_regs
            [opnd_get_reg(instr_get_src(top, 0)) - DR_REG_XAX] = true;
        ci->num_callee_save_regs++;
        /* remove & destroy the push/pop pairs */
        instrlist_remove(ilist, top);
        instr_destroy(GLOBAL_DCONTEXT, top);
        instrlist_remove(ilist, bot);
        instr_destroy(GLOBAL_DCONTEXT, bot);
        /* get next pair */
        top = instrlist_first(ilist);
        bot = instrlist_last(ilist);
    }
}

static void
analyze_callee_tls(dcontext_t *dcontext, callee_info_t *ci)
{
    /* access to TLS means we do need to swap/preserve TEB/PEB fields
     * for library isolation (errno, etc.)
     */
    instr_t *instr;
    int i;
    ci->tls_used = false;
    for (instr  = instrlist_first(ci->ilist);
         instr != NULL;
         instr  = instr_get_next(instr)) {
        /* we assume any access via app's tls is to app errno. */
        for (i = 0; i < instr_num_srcs(instr); i++) {
            opnd_t opnd = instr_get_src(instr, i);
            if (opnd_is_far_base_disp(opnd) &&
                opnd_get_segment(opnd) == LIB_SEG_TLS)
                ci->tls_used = true;
        }
        for (i = 0; i < instr_num_dsts(instr); i++) {
            opnd_t opnd = instr_get_dst(instr, i);
            if (opnd_is_far_base_disp(opnd) &&
                opnd_get_segment(opnd) == LIB_SEG_TLS)
                ci->tls_used = true;
        }
    }
    if (ci->tls_used) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" accesses far memory\n", ci->start);
    }
}

/* Pick a register to use as a base register pointing to our spill slots.
 * We can't use a register that is:
 * - DR_XSP (need a valid stack in case of fault)
 * - DR_XAX (could be used for args or aflags)
 * - REGPARM_0 on X64 (RDI on Lin and RCX on Win; for N>1 args, avoid REGPARM_<=N)
 * - used by the callee
 */
static void
analyze_callee_pick_spill_reg(dcontext_t *dcontext, callee_info_t *ci)
{
    uint i;
    for (i = 0; i < NUM_GP_REGS; i++) {
        reg_id_t reg = DR_REG_XAX + (reg_id_t)i;
        if (reg == DR_REG_XSP ||
            reg == DR_REG_XAX IF_X64(|| reg == REGPARM_0))
            continue;
        if (!ci->reg_used[i]) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: picking spill reg %s for callee "PFX"\n",
                reg_names[reg], ci->start);
            ci->spill_reg = reg;
            return;
        }
    }

    /* This won't happen unless someone increases CLEANCALL_NUM_INLINE_SLOTS or
     * handles calls with more arguments.  There are at least 8 GPRs, 4 spills,
     * and 3 other regs we can't touch, so one will be available.
     */
    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: failed to pick spill reg for callee "PFX"\n", ci->start);
    /* Fail to inline by setting ci->spill_reg == DR_REG_INVALID. */
    ci->spill_reg = DR_REG_INVALID;
}

static void
analyze_callee_inline(dcontext_t *dcontext, callee_info_t *ci)
{
    instr_t *instr, *next_instr;
    opnd_t opnd, mem_ref, slot;
    bool opt_inline = true;
    int i;

    mem_ref = opnd_create_null();
    /* a set of condition checks */
    if (INTERNAL_OPTION(opt_cleancall) < 2) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: opt_cleancall: %d.\n",
            ci->start, INTERNAL_OPTION(opt_cleancall));
        opt_inline = false;
    }
    if (ci->num_instrs > MAX_NUM_INLINE_INSTRS) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: num of instrs: %d.\n",
            ci->start, ci->num_instrs);
        opt_inline = false;
    }
    if (ci->bwd_tgt != NULL || ci->fwd_tgt != NULL) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: has control flow.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->num_xmms_used != 0) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: uses XMM.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->tls_used) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined: accesses TLS.\n",
            ci->start);
        opt_inline = false;
    }
    if (ci->spill_reg == DR_REG_INVALID) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined:"
            " unable to pick spill reg.\n", ci->start);
        opt_inline = false;
    }
    if (!SCRATCH_ALWAYS_TLS() || ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined:"
            " not enough scratch slots.\n", ci->start);
        opt_inline = false;
    }
    if (!opt_inline) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
        return;
    }

    /* Now we need scan instructions in the list,
     * check if possible for inline, and convert memory reference
     */
    ci->has_locals = false;
    for (instr  = instrlist_first(ci->ilist);
         instr != NULL;
         instr  = next_instr) {
        uint opc = instr_get_opcode(instr);
        next_instr = instr_get_next(instr);
        /* sanity checks on stack usage */
        if (instr_writes_to_reg(instr, DR_REG_XBP) && ci->xbp_is_fp) {
            /* xbp must not be changed if xbp is used for frame pointer */
            LOG(THREAD, LOG_CLEANCALL, 1,
                "CLEANCALL: callee "PFX" cannot be inlined: XBP is updated.\n",
                ci->start);
            opt_inline = false;
            break;
        } else if (instr_writes_to_reg(instr, DR_REG_XSP)) {
            /* stack pointer update, we only allow:
             * lea [xsp, disp] => xsp
             * xsp + imm_int => xsp
             * xsp - imm_int => xsp
             */
            if (ci->has_locals) {
                /* we do not allow stack adjustment after accessing the stack */
                opt_inline = false;
            }
            if (opc == OP_lea) {
                /* lea [xsp, disp] => xsp */
                opnd = instr_get_src(instr, 0);
                if (!opnd_is_base_disp(opnd)           ||
                    opnd_get_base(opnd)  != DR_REG_XSP ||
                    opnd_get_index(opnd) != DR_REG_NULL)
                    opt_inline = false;
            } else if (opc == OP_sub || opc == OP_add) {
                /* xsp +/- int => xsp */
                if (!opnd_is_immed_int(instr_get_src(instr, 0)))
                    opt_inline = false;
            } else {
                /* other cases like push/pop are not allowed */
                opt_inline = false;
            }
            if (opt_inline) {
                LOG(THREAD, LOG_CLEANCALL, 3,
                    "CLEANCALL: removing frame adjustment at "PFX".\n",
                    instr_get_app_pc(instr));
                instrlist_remove(ci->ilist, instr);
                instr_destroy(GLOBAL_DCONTEXT, instr);
                continue;
            } else {
                LOG(THREAD, LOG_CLEANCALL, 1,
                    "CLEANCALL: callee "PFX" cannot be inlined: "
                    "complicated stack pointer update at "PFX".\n",
                    ci->start, instr_get_app_pc(instr));
                break;
            }
        } else if (instr_reg_in_src(instr, DR_REG_XSP) ||
                   (instr_reg_in_src(instr, DR_REG_XBP) && ci->xbp_is_fp)) {
            /* Detect stack address leakage */
            /* lea [xsp/xbp] */
            if (opc == OP_lea)
                opt_inline = false;
            /* any direct use reg xsp or xbp */
            for (i = 0; i < instr_num_srcs(instr); i++) {
                opnd_t src = instr_get_src(instr, i);
                if (opnd_is_reg(src) &&
                    (reg_overlap(REG_XSP, opnd_get_reg(src))  ||
                     (reg_overlap(REG_XBP, opnd_get_reg(src)) && ci->xbp_is_fp)))
                    break;
            }
            if (i != instr_num_srcs(instr))
                opt_inline = false;
            if (!opt_inline) {
                LOG(THREAD, LOG_CLEANCALL, 1,
                    "CLEANCALL: callee "PFX" cannot be inlined: "
                    "stack pointer leaked "PFX".\n",
                    ci->start, instr_get_app_pc(instr));
                break;
            }
        }
        /* Check how many stack variables the callee has.
         * We will not inline the callee if it has more than one stack variable.
         */
        if (instr_reads_memory(instr)) {
            for (i = 0; i < instr_num_srcs(instr); i++) {
                opnd = instr_get_src(instr, i);
                if (!opnd_is_base_disp(opnd))
                    continue;
                if (opnd_get_base(opnd) != DR_REG_XSP &&
                    (opnd_get_base(opnd) != DR_REG_XBP || !ci->xbp_is_fp))
                    continue;
                if (!ci->has_locals) {
                    /* We see the first one, remember it. */
                    mem_ref = opnd;
                    callee_info_reserve_slot(ci, SLOT_LOCAL, 0);
                    if (ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
                        LOG(THREAD, LOG_CLEANCALL, 1,
                            "CLEANCALL: callee "PFX" cannot be inlined: "
                            "not enough slots for local.\n",
                            ci->start);
                        break;
                    }
                    ci->has_locals = true;
                } else if (!opnd_same(opnd, mem_ref)) {
                    /* Check if it is the same stack var as the one we saw.
                     * If different, no inline.
                     */
                    LOG(THREAD, LOG_CLEANCALL, 1,
                        "CLEANCALL: callee "PFX" cannot be inlined: "
                        "more than one stack location is accessed "PFX".\n",
                        ci->start, instr_get_app_pc(instr));
                    break;
                }
                /* replace the stack location with the scratch slot. */
                slot = callee_info_slot_opnd(ci, SLOT_LOCAL, 0);
                opnd_set_size(&slot, opnd_get_size(mem_ref));
                instr_set_src(instr, i, slot);
            }
            if (i != instr_num_srcs(instr)) {
                opt_inline = false;
                break;
            }
        }
        if (instr_writes_memory(instr)) {
            for (i = 0; i < instr_num_dsts(instr); i++) {
                opnd = instr_get_dst(instr, i);
                if (!opnd_is_base_disp(opnd))
                    continue;
                if (opnd_get_base(opnd) != DR_REG_XSP &&
                    (opnd_get_base(opnd) != DR_REG_XBP || !ci->xbp_is_fp))
                    continue;
                if (!ci->has_locals) {
                    mem_ref = opnd;
                    callee_info_reserve_slot(ci, SLOT_LOCAL, 0);
                    if (ci->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
                        LOG(THREAD, LOG_CLEANCALL, 1,
                            "CLEANCALL: callee "PFX" cannot be inlined: "
                            "not enough slots for local.\n",
                            ci->start);
                        break;
                    }
                    ci->has_locals = true;
                } else if (!opnd_same(opnd, mem_ref)) {
                    /* currently we only allows one stack refs */
                    LOG(THREAD, LOG_CLEANCALL, 1,
                        "CLEANCALL: callee "PFX" cannot be inlined: "
                        "more than one stack location is accessed "PFX".\n",
                        ci->start, instr_get_app_pc(instr));
                    break;
                }
                /* replace the stack location with the scratch slot. */
                slot = callee_info_slot_opnd(ci, SLOT_LOCAL, 0);
                opnd_set_size(&slot, opnd_get_size(mem_ref));
                instr_set_dst(instr, i, slot);
            }
            if (i != instr_num_dsts(instr)) {
                opt_inline = false;
                break;
            }
        }
    }
    if (instr == NULL && opt_inline) {
        ci->opt_inline = true;
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" can be inlined.\n", ci->start);
    } else {
        /* not inline callee, so ilist is not needed. */
        LOG(THREAD, LOG_CLEANCALL, 1,
            "CLEANCALL: callee "PFX" cannot be inlined.\n", ci->start);
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
    }
}

static void
analyze_callee_ilist(dcontext_t *dcontext, callee_info_t *ci)
{
    ASSERT(!ci->bailout && ci->ilist != NULL);
    /* Remove frame setup and reg pushes before analyzing reg usage. */
    if (INTERNAL_OPTION(opt_cleancall) >= 1) {
        analyze_callee_save_reg(dcontext, ci);
    }
    analyze_callee_regs_usage(dcontext, ci);
    if (INTERNAL_OPTION(opt_cleancall) < 1) {
        instrlist_clear_and_destroy(GLOBAL_DCONTEXT, ci->ilist);
        ci->ilist = NULL;
    } else {
        analyze_callee_tls(dcontext, ci);
        analyze_callee_pick_spill_reg(dcontext, ci);
        analyze_callee_inline(dcontext, ci);
    }
}

static void
analyze_clean_call_aflags(dcontext_t *dcontext,
                          clean_call_info_t *cci, instr_t *where)
{
    callee_info_t *ci = cci->callee_info;
    instr_t *instr;

    /* If there's a flags read, we clear the flags.  If there's a write or read,
     * we save them, because a read creates a clear which is a write. */
    cci->skip_clear_eflags = !ci->read_aflags;
    cci->skip_save_aflags  = !(ci->write_aflags || ci->read_aflags);
    /* XXX: this is a more aggressive optimization by analyzing the ilist
     * to be instrumented. The client may change the ilist, which violate
     * the analysis result. For example,
     * I do not need save the aflags now if an instruction
     * after "where" updating all aflags, but later the client can
     * insert an instruction reads the aflags before that instruction.
     */
    if (INTERNAL_OPTION(opt_cleancall) > 1 && !cci->skip_save_aflags) {
        for (instr = where; instr != NULL; instr = instr_get_next(instr)) {
            uint flags = instr_get_arith_flags(instr);
            if (TESTANY(EFLAGS_READ_6, flags) || instr_is_cti(instr))
                break;
            if (TESTALL(EFLAGS_WRITE_6, flags)) {
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: inserting clean call "PFX
                    ", skip saving aflags.\n", ci->start);
                cci->skip_save_aflags = true;
                break;
            }
        }
    }
}

static void
analyze_clean_call_regs(dcontext_t *dcontext, clean_call_info_t *cci)
{
    uint i, num_regparm;
    callee_info_t *info = cci->callee_info;

    /* 1. xmm registers */
    for (i = 0; i < NUM_XMM_REGS; i++) {
        if (info->xmm_used[i]) {
            cci->xmm_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call "PFX
                ", skip saving XMM%d.\n", info->start, i);
            cci->xmm_skip[i] = true;
            cci->num_xmms_skip++;
        }
    }
    if (INTERNAL_OPTION(opt_cleancall) > 2 && cci->num_xmms_skip != NUM_XMM_REGS)
        cci->should_align = false;
    /* 2. general purpose registers */
    /* set regs not to be saved for clean call */
    for (i = 0; i < NUM_GP_REGS; i++) {
        if (info->reg_used[i]) {
            cci->reg_skip[i] = false;
        } else {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call "PFX
                ", skip saving reg %s.\n",
                info->start, reg_names[DR_REG_XAX + (reg_id_t)i]);
            cci->reg_skip[i] = true;
            cci->num_regs_skip++;
        }
    }
    /* we need save/restore rax if save aflags because rax is used */
    if (!cci->skip_save_aflags && cci->reg_skip[0]) {
        LOG(THREAD, LOG_CLEANCALL, 3,
            "CLEANCALL: if inserting clean call "PFX
            ", cannot skip saving reg xax.\n", info->start);
        cci->reg_skip[0] = false;
        cci->num_regs_skip++;
    }
    /* i#987: args are passed via regs in 64-bit, which will clober those regs,
     * so we should not skip any regs that are used for arg passing.
     * XXX: we do not support args passing via XMMs,
     * see docs for dr_insert_clean_call
     * XXX: we can elminate the arg passing instead since it is not used
     * if marked for skip. However, we have to handle cases like some args
     * are used and some are not.
     */
    num_regparm = cci->num_args < NUM_REGPARM ? cci->num_args : NUM_REGPARM;
    for (i = 0; i < num_regparm; i++) {
        if (cci->reg_skip[regparms[i] - DR_REG_XAX]) {
            LOG(THREAD, LOG_CLEANCALL, 3,
                "CLEANCALL: if inserting clean call "PFX
                ", cannot skip saving reg %s due to param passing.\n",
                info->start, reg_names[regparms[i]]);
            cci->reg_skip[regparms[i] - DR_REG_XAX] = false;
            cci->num_regs_skip--;
            /* We cannot call callee_info_reserve_slot for reserving slot
             * on inlining the callee here, because we are in clean call
             * analysis not callee anaysis.
             * Also the reg for arg passing should be first handled in
             * analyze_callee_regs_usage on callee_info creation.
             * If we still reach here, it means the number args changes
             * for the same clean call, so we will not inline it and do not
             * need call callee_info_reserve_slot either.
             */
        }
    }
}

static void
analyze_clean_call_args(dcontext_t *dcontext,
                        clean_call_info_t *cci,
                        opnd_t *args)
{
    uint i, j, num_regparm;

    num_regparm = cci->num_args < NUM_REGPARM ? cci->num_args : NUM_REGPARM;
    /* If a param uses a reg, DR need restore register value, which assumes
     * the full context switch with priv_mcontext_t layout,
     * in which case we need keep priv_mcontext_t layout.
     */
    cci->save_all_regs = false;
    for (i = 0; i < cci->num_args; i++) {
        if (opnd_is_reg(args[i]))
            cci->save_all_regs = true;
        for (j = 0; j < num_regparm; j++) {
            if (opnd_uses_reg(args[i], regparms[j]))
                cci->save_all_regs = true;
        }
    }
    /* We only set cci->reg_skip all to false later if we fail to inline.  We
     * only need to preserve the layout if we're not inlining.
     */
}

static bool
analyze_clean_call_inline(dcontext_t *dcontext, clean_call_info_t *cci)
{
    callee_info_t *info = cci->callee_info;
    bool opt_inline = true;

    if (INTERNAL_OPTION(opt_cleancall) <= 1) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", opt_cleancall %d.\n",
            info->start, INTERNAL_OPTION(opt_cleancall));
        opt_inline = false;
    }
    if (cci->num_args > 1) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", number of args %d > 1.\n",
            info->start, cci->num_args);
        opt_inline = false;
    }
    if (cci->num_args > info->num_args) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX
            ", number of args increases.\n",
            info->start);
        opt_inline = false;
    }
    if (cci->save_fpstate) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", saving fpstate.\n",
            info->start);
        opt_inline = false;
    }
    if (!info->opt_inline) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", complex callee.\n",
            info->start);
        opt_inline = false;
    }
    if (info->slots_used > CLEANCALL_NUM_INLINE_SLOTS) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: fail inlining clean call "PFX", used %d slots, "
            "> %d available slots.\n",
            info->start, info->slots_used, CLEANCALL_NUM_INLINE_SLOTS);
        opt_inline = false;
    }
    if (!opt_inline) {
        if (cci->save_all_regs) {
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inserting clean call "PFX
                ", save all regs in priv_mcontext_t layout.\n",
                info->start);
            cci->num_regs_skip = 0;
            memset(cci->reg_skip, 0, sizeof(bool) * NUM_GP_REGS);
            cci->should_align = true;
        } else {
            uint i;
            for (i = 0; i < NUM_GP_REGS; i++) {
                if (!cci->reg_skip[i] && info->callee_save_regs[i]) {
                    cci->reg_skip[i] = true;
                    cci->num_regs_skip++;
                }
            }
        }
        if (cci->num_xmms_skip == NUM_XMM_REGS) {
            STATS_INC(cleancall_xmm_skipped);
        }
        if (cci->skip_save_aflags) {
            STATS_INC(cleancall_aflags_save_skipped);
        }
        if (cci->skip_clear_eflags) {
            STATS_INC(cleancall_aflags_clear_skipped);
        }
    } else {
        cci->ilist = instrlist_clone(dcontext, info->ilist);
    }
    return opt_inline;
}

bool
analyze_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where,
                   void *callee, bool save_fpstate, uint num_args, opnd_t *args)
{
    callee_info_t *ci;
    /* by default, no inline optimization */
    bool should_inline = false;

    CLIENT_ASSERT(callee != NULL, "Clean call target is NULL");
    /* 1. init clean_call_info */
    clean_call_info_init(cci, callee, save_fpstate, num_args);
    /* 2. check runtime optimization options */
    if (INTERNAL_OPTION(opt_cleancall) > 0) {
        /* 3. search if callee was analyzed before */
        ci = callee_info_table_lookup(callee);
        /* 4. this callee is not seen before */
        if (ci == NULL) {
            STATS_INC(cleancall_analyzed);
            LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: analyze callee "PFX"\n", callee);
            /* 4.1. create func_info */
            ci = callee_info_create((app_pc)callee, num_args);
            /* 4.2. decode the callee */
            decode_callee_ilist(dcontext, ci);
            /* 4.3. analyze the instrlist */
            if (!ci->bailout)
                analyze_callee_ilist(dcontext, ci);
            /* 4.4. add info into callee list */
            ci = callee_info_table_add(ci);
        }
        cci->callee_info = ci;
        if (ci->bailout) {
            callee_info_init(ci);
            ci->start = (app_pc)callee;
            LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: bailout "PFX"\n", callee);
        } else {
            /* 5. aflags optimization analysis */
            analyze_clean_call_aflags(dcontext, cci, where);
            /* 6. register optimization analysis */
            analyze_clean_call_regs(dcontext, cci);
            /* 7. check arguments */
            analyze_clean_call_args(dcontext, cci, args);
            /* 8. inline optimization analysis */
            should_inline = analyze_clean_call_inline(dcontext, cci);
        }
    }
    /* 9. derived fields */
    if (cci->num_xmms_skip == 0 /* save all xmms */ &&
        cci->num_regs_skip == 0 /* save all regs */ &&
        !cci->skip_save_aflags)
        cci->out_of_line_swap = true;

    return should_inline;
}

static void
insert_inline_reg_save(dcontext_t *dcontext, clean_call_info_t *cci,
                       instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    callee_info_t *ci = cci->callee_info;
    int i;

    /* Don't spill anything if we don't have to. */
    if (cci->num_regs_skip == NUM_GP_REGS && cci->skip_save_aflags &&
        !ci->has_locals) {
        return;
    }

    /* Spill a register to TLS and point it at our unprotected_context_t. */
    PRE(ilist, where, instr_create_save_to_tls
        (dcontext, ci->spill_reg, TLS_XAX_SLOT));
    insert_get_mcontext_base(dcontext, ilist, where, ci->spill_reg);

    /* Save used registers. */
    ASSERT(cci->num_xmms_skip == NUM_XMM_REGS);
    for (i = 0; i < NUM_GP_REGS; i++) {
        if (!cci->reg_skip[i]) {
            reg_id_t reg_id = DR_REG_XAX + (reg_id_t)i;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inlining clean call "PFX", saving reg %s.\n",
                ci->start, reg_names[reg_id]);
            PRE(ilist, where, INSTR_CREATE_mov_st
                (dcontext, callee_info_slot_opnd(ci, SLOT_REG, reg_id),
                 opnd_create_reg(reg_id)));
        }
    }

    /* Save aflags if necessary via XAX, which was just saved if needed. */
    if (!cci->skip_save_aflags) {
        ASSERT(!cci->reg_skip[DR_REG_XAX - DR_REG_XAX]);
        dr_save_arith_flags_to_xax(dcontext, ilist, where);
        PRE(ilist, where, INSTR_CREATE_mov_st
            (dcontext, callee_info_slot_opnd(ci, SLOT_FLAGS, 0),
             opnd_create_reg(DR_REG_XAX)));
        /* Restore app XAX here if it's needed to materialize the argument. */
        if (cci->num_args > 0 && opnd_uses_reg(args[0], DR_REG_XAX)) {
            PRE(ilist, where, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(DR_REG_XAX),
                 callee_info_slot_opnd(ci, SLOT_REG, DR_REG_XAX)));
        }
    }
}

static void
insert_inline_reg_restore(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *where)
{
    int i;
    callee_info_t *ci = cci->callee_info;

    /* Don't restore regs if we don't have to. */
    if (cci->num_regs_skip == NUM_GP_REGS && cci->skip_save_aflags &&
        !ci->has_locals) {
        return;
    }

    /* Restore aflags before regs because it uses xax. */
    if (!cci->skip_save_aflags) {
        PRE(ilist, where, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(DR_REG_XAX),
             callee_info_slot_opnd(ci, SLOT_FLAGS, 0)));
        dr_restore_arith_flags_from_xax(dcontext, ilist, where);
    }

    /* Now restore all registers. */
    for (i = NUM_GP_REGS - 1; i >= 0; i--) {
        if (!cci->reg_skip[i]) {
            reg_id_t reg_id = DR_REG_XAX + (reg_id_t)i;
            LOG(THREAD, LOG_CLEANCALL, 2,
                "CLEANCALL: inlining clean call "PFX", restoring reg %s.\n",
                ci->start, reg_names[reg_id]);
            PRE(ilist, where, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(reg_id),
                 callee_info_slot_opnd(ci, SLOT_REG, reg_id)));
        }
    }

    /* Restore reg used for unprotected_context_t pointer. */
    PRE(ilist, where, instr_create_restore_from_tls
        (dcontext, ci->spill_reg, TLS_XAX_SLOT));
}

static void
insert_inline_arg_setup(dcontext_t *dcontext, clean_call_info_t *cci,
                        instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    reg_id_t regparm;
    callee_info_t *ci = cci->callee_info;
    opnd_t arg;
    bool restored_spill_reg = false;

    if (cci->num_args == 0)
        return;

    /* If the arg is un-referenced, don't set it up.  This is actually necessary
     * for correctness because we will not have spilled regparm[0] on x64 or
     * reserved SLOT_LOCAL for x86_32.
     */
    if (IF_X64_ELSE(!ci->reg_used[regparms[0] - DR_REG_XAX],
                    !ci->has_locals)) {
        LOG(THREAD, LOG_CLEANCALL, 2,
            "CLEANCALL: callee "PFX" doesn't read arg, skipping arg setup.\n",
            ci->start);
        return;
    }

    ASSERT(cci->num_args == 1);
    arg = args[0];
    regparm = shrink_reg_for_param(IF_X64_ELSE(regparms[0], DR_REG_XAX), arg);

    if (opnd_uses_reg(arg, ci->spill_reg)) {
        if (opnd_is_reg(arg)) {
            /* Trying to pass the spill reg (or a subreg) as the arg. */
            reg_id_t arg_reg = opnd_get_reg(arg);
            arg = opnd_create_tls_slot(os_tls_offset(TLS_XAX_SLOT));
            opnd_set_size(&arg, reg_get_size(arg_reg));
            if (arg_reg == DR_REG_AH ||  /* Don't rely on ordering. */
                arg_reg == DR_REG_BH ||
                arg_reg == DR_REG_CH ||
                arg_reg == DR_REG_DH) {
                /* If it's one of the high sub-registers, add 1 to offset. */
                opnd_set_disp(&arg, opnd_get_disp(arg) + 1);
            }
        } else {
            /* Too complicated to rewrite if it's embedded in the operand.  Just
             * restore spill_reg during the arg materialization.  Hopefully this
             * doesn't happen very often.
             */
            PRE(ilist, where, instr_create_restore_from_tls
                (dcontext, ci->spill_reg, TLS_XAX_SLOT));
            DOLOG(2, LOG_CLEANCALL, {
                char disas_arg[MAX_OPND_DIS_SZ];
                opnd_disassemble_to_buffer(dcontext, arg, disas_arg,
                                           BUFFER_SIZE_ELEMENTS(disas_arg));
                LOG(THREAD, LOG_CLEANCALL, 2,
                    "CLEANCALL: passing arg %s using spill reg %s to callee "PFX" "
                    "requires extra spills, consider using a different register.\n",
                    disas_arg, reg_names[ci->spill_reg], ci->start);
            });
            restored_spill_reg = true;
        }
    }

    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: inlining clean call "PFX", passing arg via reg %s.\n",
        ci->start, reg_names[regparm]);
    if (opnd_is_immed_int(arg)) {
        PRE(ilist, where, INSTR_CREATE_mov_imm
            (dcontext, opnd_create_reg(regparm), arg));
    } else {
        PRE(ilist, where, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(regparm), arg));
    }

    /* Put the unprotected_context_t pointer back in spill_reg if we needed to
     * restore the app value.
     */
    if (restored_spill_reg) {
        insert_get_mcontext_base(dcontext, ilist, where, ci->spill_reg);
    }

#ifndef X64
    ASSERT(!cci->reg_skip[0]);
    /* Move xax to the scratch slot of the local.  We only allow at most one
     * local stack access, so the callee either does not use the argument, or
     * the local stack access is the arg.
     */
    LOG(THREAD, LOG_CLEANCALL, 2,
        "CLEANCALL: inlining clean call "PFX", passing arg via slot.\n",
        ci->start);
    PRE(ilist, where, INSTR_CREATE_mov_st
        (dcontext, callee_info_slot_opnd(ci, SLOT_LOCAL, 0),
         opnd_create_reg(DR_REG_XAX)));
#endif
}

void
insert_inline_clean_call(dcontext_t *dcontext, clean_call_info_t *cci,
                         instrlist_t *ilist, instr_t *where, opnd_t *args)
{
    instrlist_t *callee = cci->ilist;
    instr_t *instr;

    ASSERT(cci->ilist != NULL);
    ASSERT(SCRATCH_ALWAYS_TLS());
    /* 0. update stats */
    STATS_INC(cleancall_inlined);
    /* 1. save registers */
    insert_inline_reg_save(dcontext, cci, ilist, where, args);
    /* 2. setup parameters */
    insert_inline_arg_setup(dcontext, cci, ilist, where, args);
    /* 3. inline clean call ilist */
    instr = instrlist_first(callee);
    while (instr != NULL) {
        instrlist_remove(callee, instr);
        /* XXX: if client has a xl8 handler we assume it will handle any faults
         * in the callee (which should already have a translation set to the
         * callee): and if not we assume there will be no such faults.
         * We can't have a translation with no handler.
         */
        if (IF_CLIENT_INTERFACE_ELSE(!dr_xl8_hook_exists(), true))
            instr_set_translation(instr, NULL);
        instrlist_meta_preinsert(ilist, where, instr);
        instr = instrlist_first(callee);
    }
    instrlist_destroy(dcontext, callee);
    cci->ilist = NULL;
    /* 4. restore registers */
    insert_inline_reg_restore(dcontext, cci, ilist, where);
    /* XXX: the inlined code looks like this
     *   mov    %rax -> %gs:0x00
     *   mov    %rdi -> %gs:0x01
     *   mov    $0x00000003 -> %edi
     *   mov    <rel> 0x0000000072200c00 -> %rax
     *   movsxd %edi -> %rdi
     *   add    %rdi (%rax) -> (%rax)
     *   mov    %gs:0x00 -> %rax
     *   mov    %gs:0x01 -> %rdi
     *   ...
     * we can do some constant propagation optimization here,
     * leave it for higher optimization level.
     */
}

#else /* CLIENT_INTERFACE */

/* Stub implementation ifndef CLIENT_INTERFACE.  Initializes cci and returns
 * false for no inlining.  We use dr_insert_clean_call internally, but we don't
 * need it to do inlining.
 */
bool
analyze_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where,
                   void *callee, bool save_fpstate, uint num_args, opnd_t *args)
{
    CLIENT_ASSERT(callee != NULL, "Clean call target is NULL");
    /* 1. init clean_call_info */
    clean_call_info_init(cci, callee, save_fpstate, num_args);
    return false;
}

#endif /* CLIENT_INTERFACE */

#endif /* !STANDALONE_DECODER */
/***************************************************************************/

