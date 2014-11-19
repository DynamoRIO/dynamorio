/* **********************************************************
 * Copyright (c) 2010-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* file "emit_utils_shared.c" */
/* The Pentium processors maintain cache consistency in hardware, so we don't
 * worry about getting stale cache entries.
 */
/* FIXME i#1551: flush code cache after update it on ARM because the hardware
 * does not maintain cache consistency in hardware.
 */

#include "../globals.h"
#include "../link.h"
#include "../fragment.h"
#include "../fcache.h"
#include "../emit.h"

#include "arch.h"
#include "instr.h"
#include "instr_create.h"
#include "instrlist.h"
#include "instrument.h" /* for dr_insert_call() */
#include "proc.h"
#include <string.h> /* for memcpy */
#include "decode.h"
#include "decode_fast.h"
#include "x86/decode_private.h"
#ifdef DEBUG
# include "disassemble.h"
#endif
#include <limits.h> /* for UCHAR_MAX */
#include "../perscache.h"

#ifdef VMX86_SERVER
# include "vmkuw.h"
#endif

/* fragment_t fields */
#define FRAGMENT_TAG_OFFS        (offsetof(fragment_t, tag))
/* CAUTION: if TAG_OFFS changes from 0, must change indirect exit stub! */
#define FRAGMENT_START_PC_OFFS   (offsetof(fragment_t, start_pc))
#define FRAGMENT_COUNTER_OFFS    (offsetof(fragment_t, hot_counter))
#define FRAGMENT_PREFIX_SIZE_OFFS (offsetof(fragment_t, prefix_size))

#define HASHLOOKUP_TAG_OFFS       (offsetof(fragment_entry_t, tag_fragment))
#define HASHLOOKUP_START_PC_OFFS       (offsetof(fragment_entry_t, start_pc_fragment))

#ifdef HASHTABLE_STATISTICS
#  define HASHLOOKUP_STAT_OFFS(event) (offsetof(hashtable_statistics_t, event##_stat))
#endif

#ifdef TRACE_HEAD_CACHE_INCR
/* linkstub_t field */
#  define LINKSTUB_TARGET_FRAG_OFFS  (offsetof(direct_linkstub_t, target_fragment))
#endif

#ifdef PROFILE_LINKCOUNT
#  define LINKSTUB_COUNT_OFFS        (offsetof(linkstub_t, count))
#endif

/* macro to make it easier to get offsets of fields that are in
 * a different memory space with self-protection
 */
#define UNPROT_OFFS(dcontext, offs)                         \
    (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask) ? \
     (((ptr_uint_t)((dcontext)->upcontext.separate_upcontext)) + (offs)) :  \
     (((ptr_uint_t)(dcontext)) + (offs)))

/* N.B.: I decided to not keep supporting DCONTEXT_IN_EDI
 * If we really want it later we can add it, it's a pain to keep
 * maintaining it with every change here
 */
#ifdef DCONTEXT_IN_EDI
#error DCONTEXT_IN_EDI Not Implemented
#endif

/* make code more readable by shortening long lines
 * we mark all as meta to avoid client interface asserts
 */
#define POST instrlist_meta_postinsert
#define PRE  instrlist_meta_preinsert
#define APP  instrlist_meta_append

/* we are sharing bbs w/o ibs -- we assume that a bb
 * w/ a direct branch cannot have an ib and thus is shared
 */
#ifdef TRACE_HEAD_CACHE_INCR
/* incr routine can't tell whether coming from shared bb
 * or non-shared fragment (such as a trace) so must always
 * use shared stubs
 */
#  define FRAG_DB_SHARED(flags) true
#else
#  define FRAG_DB_SHARED(flags) (TEST(FRAG_SHARED, (flags)))
#endif

/**
 ** CAUTION!
 **
 ** The following definitions and routines are highly dependent upon
 ** definitions made in x86.asm.  Do NOT change any constants or code
 ** without first consulting that file.
 **
 **/

/***************************************************************************
 ***************************************************************************
 ** EXIT STUB
 **
 ** WARNING: all exit stubs must support atomic linking and unlinking,
 ** meaning a link/unlink operation must involve a single store!
 ** There is an exception: a first-time link (detected using a sentinel
 ** LINKCOUNT_NEVER_LINKED_SENTINEL placed where the unlinked entry
 ** code will go once linked) does not need to be atomic.
 **/

/*
direct branch exit_stub:
   5x8  mov   %xax, xax_offs(&dcontext) or tls
  #if defined(PROFILE_LINKCOUNT)  (PR 248210: x64 not supported)
  | 1   lahf
  | 3   seto  %al
  |#if !defined(LINKCOUNT_64_BITS)
  | 6   inc   l->count
  |#else
  | 7   add   $1,l->count
  | 7   adc   $0,l->count+4
  |#endif
  | 2   add   $0x7f,%al
  | 1   sahf
  #endif
   5x10 mov   &linkstub, %xax
    5   jmp   target addr
#if defined(PROFILE_LINKCOUNT)  (PR 248210: x64 not supported)
|unlinked entry point:
|    5   movl  %eax, eax_offs(&dcontext)
|    5   movl  &linkstub, %eax
|    5   jmp   fcache_return
|
|  Notes: we link/unlink by modifying the 1st jmp to either target unlinked
|  entry point or the target fragment.  When we link for the first time
|  we try to remove the eflags save/restore, shifting the 1st jmp up (the
|  space between it and unlinked entry just becomes junk).
#endif

indirect branch exit_stub (only used if -indirect_stubs):
   6x9  mov   %xbx, xbx_offs(&dcontext) or tls
   5x11 mov   &linkstub, %xbx
    5   jmp   indirect_branch_lookup

indirect branches use xbx so that the flags can be saved into xax using
the lahf instruction!
xref PR 249775 on lahf support on x64.

for PROFILE_LINKCOUNT, the count increment is performed inside the
hashtable lookup (in both linked and unlinked paths) both since the flags
are saved there for the linked path and to save space in stubs

also see emit_inline_ibl_stub() below

*/

/* DIRECT_EXIT_STUB_SIZE is in arch_exports.h */
#define STUB_DIRECT_SIZE(flags) DIRECT_EXIT_STUB_SIZE(flags)

/* for -thread_private, we're relying on the fact that
 * SIZE32_MOV_XBX_TO_TLS == SIZE32_MOV_XBX_TO_ABS, and that
 * x64 always uses tls
 */
#define STUB_INDIRECT_SIZE32 \
    (SIZE32_MOV_XBX_TO_TLS + SIZE32_MOV_PTR_IMM_TO_XAX + JMP_LONG_LENGTH)
#define STUB_INDIRECT_SIZE64 \
    (SIZE64_MOV_XBX_TO_TLS + SIZE64_MOV_PTR_IMM_TO_XAX + JMP_LONG_LENGTH)
#define STUB_INDIRECT_SIZE(flags) \
    (FRAG_IS_32(flags) ? STUB_INDIRECT_SIZE32 : STUB_INDIRECT_SIZE64)

/* STUB_COARSE_DIRECT_SIZE is in arch_exports.h */
#define STUB_COARSE_INDIRECT_SIZE(flags) (STUB_INDIRECT_SIZE(flags))

#ifndef LINKCOUNT_64_BITS
#  define LINKCOUNT_INCSIZE (6)
#else
#  define LINKCOUNT_INCSIZE (7+7)
#endif
#define LINKCOUNT_EFLAGS_SAVE (3+1)
#define LINKCOUNT_EFLAGS_RESTORE (2+1)
#define LINKCOUNT_FLAGSIZE (LINKCOUNT_EFLAGS_SAVE + LINKCOUNT_EFLAGS_RESTORE)

#define LINKCOUNT_DIRECT_EXTRA(flags) \
    (LINKCOUNT_INCSIZE + LINKCOUNT_FLAGSIZE + STUB_DIRECT_SIZE(flags))
#define LINKCOUNT_UNLINKED_ENTRY(flags) \
    (LINKCOUNT_INCSIZE + LINKCOUNT_FLAGSIZE + STUB_DIRECT_SIZE(flags))

/* used to distinguish a never-linked direct exit -- once linked this
 * will be replaced by the beginning of the unlink entry point, which is
 * a save of xax, which will never look like this.  we choose nops to
 * avoid complicating our disassembly routines.
 */
#define LINKCOUNT_NEVER_LINKED_SENTINEL 0x90909090

/* Return size in bytes required for an exit stub with specified
 * target and FRAG_ flags
 */
int
exit_stub_size(dcontext_t *dcontext, cache_pc target, uint flags)
{
    if (TEST(FRAG_COARSE_GRAIN, flags)) {
        /* For coarse: bb building points at bb ibl, and then insert_exit_stub
         * changes that to the appropriate coarse prefix.  So the emit() calls to
         * this routine pass in a real ibl.  But any later calls, e.g. for
         * disassembly, that ask linkstub_size() will call EXIT_TARGET_TAG() which
         * calls indirect_linkstub_target() which returns get_coarse_ibl_prefix():
         * which then is not recognized as indirect by this routine!
         * Note that coarse_indirect_stub_jmp_target() derefs the prefix:
         * should we require callers who have stub pc to call that instead of us
         * de-referencing?
         */
        target = coarse_deref_ibl_prefix(dcontext, target);
    }
    if (is_indirect_branch_lookup_routine(dcontext, target)) {
        /* indirect branch */

        /* FIXME: Since we don't have the stub flags we'll lookup the
         * target routine's template in a very roundabout fashion here
         * by dispatching on the ibl_routine entry point
         */
        ibl_code_t *ibl_code;

        ibl_type_t ibl_type;
        IF_X64(gencode_mode_t mode;)
        DEBUG_DECLARE(bool is_ibl = )
            get_ibl_routine_type_ex(dcontext, target, &ibl_type _IF_X64(&mode));
        ASSERT(is_ibl);
        IF_X64(ASSERT(mode == FRAGMENT_GENCODE_MODE(flags) ||
                      (DYNAMO_OPTION(x86_to_x64) && mode == GENCODE_X86_TO_X64)));
        ibl_code = get_ibl_routine_code_ex(dcontext, ibl_type.branch_type, flags
                                           _IF_X64(mode));

        if (!EXIT_HAS_STUB(ibltype_to_linktype(ibl_code->branch_type),
                           IBL_FRAG_FLAGS(ibl_code)))
            return 0;

        if (TEST(FRAG_COARSE_GRAIN, flags)) {
            IF_WINDOWS(ASSERT(!is_shared_syscall_routine(dcontext, target)));
            /* keep in synch w/ coarse_indirect_stub_size() */
            return (STUB_COARSE_INDIRECT_SIZE(flags));
        }

#ifdef WINDOWS
        if (is_shared_syscall_routine(dcontext, target)) {
            return INTERNAL_OPTION(shared_syscalls_fastpath) ? 5 :
                STUB_INDIRECT_SIZE(flags);
        }
#endif

        if (ibl_code->ibl_head_is_inlined)
            return ibl_code->inline_stub_length;
        else
            return (STUB_INDIRECT_SIZE(flags));
    } else {
        /* direct branch */
        if (TEST(FRAG_COARSE_GRAIN, flags))
            return (STUB_COARSE_DIRECT_SIZE(flags));
#ifdef PROFILE_LINKCOUNT
        if (dynamo_options.profile_counts && (flags & FRAG_IS_TRACE) != 0)
            return (STUB_DIRECT_SIZE(flags) + LINKCOUNT_DIRECT_EXTRA(flags));
        else {
#endif
            return (STUB_DIRECT_SIZE(flags));
#ifdef PROFILE_LINKCOUNT
        }
#endif
    }
}

/* The write that inserts the relative target is done atomically so this
 * function is safe with respect to a thread executing the code containing
 * this target, presuming that the code in both the before and after states
 * is valid, and that [pc, pc+4) does not cross a cache line.
 * For x64 this routine only works for 32-bit reachability.  If further
 * reach is needed the caller must use indirection.  Xref PR 215395.
 */
byte *
insert_relative_target(byte *pc, cache_pc target, bool hot_patch)
{
    /* insert 4-byte pc-relative offset from the beginning of the next instruction
     */
    int value = (int)(ptr_int_t)(target - pc - 4);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(target - pc - 4)));
    ATOMIC_4BYTE_WRITE(pc, value, hot_patch);
    pc += 4;
    return pc;
}

byte *
insert_relative_jump(byte *pc, cache_pc target, bool hot_patch)
{
    int value;
    ASSERT(pc != NULL);
    *pc = JMP_OPCODE;
    pc++;

    /* test that we aren't crossing a cache line boundary */
    CHECK_JMP_TARGET_ALIGNMENT(pc, 4, hot_patch);
    /* We don't need to be atomic, so don't use insert_relative_target. */
    value = (int)(ptr_int_t)(target - pc - 4);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(target - pc - 4)));
    *(int *)pc = value;
    pc += 4;
    return pc;
}

/* make sure to keep in sync w/ instr_raw_is_tls_spill() */
static cache_pc
insert_spill_or_restore(dcontext_t *dcontext, cache_pc pc, uint flags, bool spill,
                        bool shared, reg_id_t reg, ushort tls_offs,
                        uint dc_offs, bool require_addr16)
{
    DEBUG_DECLARE(cache_pc start_pc = pc;)
    byte opcode = ((reg == REG_XAX) ?
                   (spill ? MOV_XAX2MEM_OPCODE : MOV_MEM2XAX_OPCODE) :
                   (spill ? MOV_REG2MEM_OPCODE : MOV_MEM2REG_OPCODE));
#ifdef X64
    /* for x64, shared and private fragments all use tls, even for 32-bit code */
    shared = true;
    if (!FRAG_IS_32(flags)) {
        /* mov %rbx, gs:os_tls_offset(tls_offs) */
        if (reg == REG_XAX) {
            /* shorter to use 0xa3 w/ addr32 prefix than 0x89/0x8b w/ sib byte */
            /* FIXME: PR 209709: test perf and remove if outweighs space */
            *pc = ADDR_PREFIX_OPCODE; pc++;
        }
        *pc = TLS_SEG_OPCODE; pc++;
        *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG; pc++;
        *pc = opcode; pc++;
        if (reg != REG_XAX) {
            /* 0x1c for rbx, 0x0c for rcx, 0x04 for rax */
            *pc = MODRM_BYTE(0/*mod*/, reg_get_bits(reg), 4/*rm*/); pc++;
            *pc = SIB_DISP32; pc++;   /* sib byte: abs addr */
        }
        *((uint *)pc) = (uint) os_tls_offset(tls_offs); pc+=4;
    } else
#endif /* X64 */
    if (shared) {
        /* mov %ebx, fs:os_tls_offset(tls_offs) */
        /* trying hard to keep the size of the stub 5 for eax, 6 else */
        /* FIXME: case 5231 when staying on trace space is better,
         * when going through this to the IBL routine speed asks for
         * not adding the prefix.
         */
        bool addr16 = (require_addr16 || use_addr_prefix_on_short_disp());
        if (addr16) {
            *pc = ADDR_PREFIX_OPCODE;
            pc++;
        }
        *pc = TLS_SEG_OPCODE; pc++;
        *pc = opcode; pc++;
        if (reg != REG_XAX) {
            /* 0x1e for ebx, 0x0e for ecx, 0x06 for eax
             * w/o addr16 those are 0x1d, 0x0d, 0x05
             */
            *pc = MODRM_BYTE(0/*mod*/, reg_get_bits(reg), addr16 ? 6 : 5/*rm*/);
            pc++;
        }
        if (addr16) {
            *((ushort *)pc) = os_tls_offset(tls_offs);
            pc+=2;
        } else {
            *((uint *)pc) = os_tls_offset(tls_offs);
            pc+=4;
        }
    } else {
        /* mov %ebx,((int)&dcontext)+dc_offs */
        *pc = opcode; pc++;
        if (reg != REG_XAX) {
            /* 0x1d for ebx, 0x0d for ecx, 0x05 for eax */
            *pc = MODRM_BYTE(0/*mod*/, reg_get_bits(reg), 5/*rm*/); pc++;
        }
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        *((uint *)pc) = (uint)(ptr_uint_t) UNPROT_OFFS(dcontext, dc_offs); pc+=4;
    }
    ASSERT(IF_X64_ELSE(false, !shared) || (pc - start_pc) ==
           (reg == REG_XAX ? SIZE_MOV_XAX_TO_TLS(flags, require_addr16) :
            SIZE_MOV_XBX_TO_TLS(flags, require_addr16)));
    ASSERT(IF_X64_ELSE(false, !shared) || !spill || reg == REG_XAX ||
           instr_raw_is_tls_spill(start_pc, reg, tls_offs));
    return pc;
}

/* instr_raw_is_tls_spill() matches the exact sequence of bytes inserted here */
static byte *
insert_jmp_to_ibl(byte *pc, fragment_t *f, linkstub_t *l, cache_pc exit_target,
                  dcontext_t *dcontext)
{
#ifdef WINDOWS
    bool spill_xbx_to_fs =
        FRAG_DB_SHARED(f->flags) ||
        (is_shared_syscall_routine(dcontext, exit_target) &&
         DYNAMO_OPTION(shared_fragment_shared_syscalls));
#else
    bool spill_xbx_to_fs = FRAG_DB_SHARED(f->flags);
#endif
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    /* we use XBX to hold the linkstub pointer for IBL routines,
       note that direct stubs use XAX for linkstub pointer */
#ifdef WINDOWS
    if (INTERNAL_OPTION(shared_syscalls_fastpath)
        && is_shared_syscall_routine(dcontext, exit_target)) {
        /* jmp <exit_target> */
        pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
        return pc;
    } else
#endif
        pc = insert_spill_or_restore(dcontext, pc, f->flags, true/*spill*/,
                                     spill_xbx_to_fs, REG_XBX,
                                     INDIRECT_STUB_SPILL_SLOT, XBX_OFFSET,
                                     true);

    /* mov $linkstub_ptr,%xbx */
#ifdef X64
    if (!FRAG_IS_32(f->flags))
        *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG; pc++;
#endif
    *pc = MOV_IMM2XBX_OPCODE; pc++;
#ifdef WINDOWS
    if (DYNAMO_OPTION(shared_syscalls)
        && is_shared_syscall_routine(dcontext, exit_target)) {
        /* FIXME We could reduce mem usage by not allocating a linkstub for
         * this exit since it's never referenced.
         */
        LOG(THREAD, LOG_LINKS, 4,
            "\tF%d using %s shared syscalls link stub\n",
            f->id, TEST(FRAG_IS_TRACE, f->flags) ? "trace" :
            "bb");
        l = (linkstub_t *) (TEST(FRAG_IS_TRACE, f->flags) ?
                          get_shared_syscalls_trace_linkstub() :
                          get_shared_syscalls_bb_linkstub());
    }
#endif
    if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* FIXME: once we separate these we should switch to 15-byte w/
         * store-to-mem instead of in a spilled xbx, to use same
         * slots as coarse direct stubs
         */
        /* There is no linkstub_t so we store source tag instead */
        *((ptr_uint_t *)pc) = (ptr_uint_t)f->tag; pc += sizeof(f->tag);
        /* FIXME: once we separate the indirect stubs out we will need
         * a 15-byte stub .  For that we should simply store the
         * source cti directly into a tls slot.  For now though we inline
         * the stubs and spill xbx.
         */
    } else {
        *((ptr_uint_t *)pc) = (ptr_uint_t)l; pc += sizeof(l);
    }
    /* jmp <exit_target> */
    pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
    return pc;
}

#ifdef PROFILE_LINKCOUNT /**************************************/
static byte *
insert_linkcount_inc(byte *pc, linkstub_t *l)
{
    /* increment counter l->count: */
#ifndef LINKCOUNT_64_BITS
    /* incl &(l->count) */
    *pc = 0xff; pc++;
    *pc = 0x05; pc++;
    /* PR 248210: PROFILE_LINKCOUNT is unsupported on x64 */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    *((uint *)pc) = (uint) &(l->count); pc+=4;
#else /* 64-bit */
    /* addl $0x1,&(l->count) */
    *pc = 0x83; pc++;
    *pc = 0x05; pc++;
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    *((uint *)pc) = (uint) &(l->count); pc+=4;
    *pc = 0x01; pc++;
    /* adcl $0x0,&(l->count)+4 */
    *pc = 0x83; pc++;
    *pc = 0x15; pc++;
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    *((uint *)pc) = 4 + (uint)(&(l->count)); pc+=4;
    *pc = 0x00; pc++;
#endif /* 64-bit */
    return pc;
}

/* FIXME: keep in synch w/ insert_save_eflags */
static byte *
insert_linkcount_saveflags(byte *pc, dcontext_t *dcontext, uint flags)
{
    DEBUG_DECLARE(cache_pc original_pc = pc;)
    /* lahf */
    *pc = LAHF_OPCODE; pc++;
    /* 0f 90 c0   seto %al */
    *pc = SETO_OPCODE_1; pc++;
    *pc = SETO_OPCODE_2; pc++;
    *pc = 0xc0; pc++;
    ASSERT(pc - original_pc == LINKCOUNT_EFLAGS_SAVE);
    return pc;
}

/* FIXME: keep in synch w/ insert_restore_eflags */
static byte *
insert_linkcount_restoreflags(byte *pc, dcontext_t *dcontext, uint flags)
{
    DEBUG_DECLARE(cache_pc original_pc = pc;)
    /* 04 7f   add $0x7f,%al */
    *pc = ADD_AL_OPCODE; pc++;
    *pc = 0x7f; pc++;
    /* sahf */
    *pc = SAHF_OPCODE; pc++;
    ASSERT(pc - original_pc == LINKCOUNT_EFLAGS_RESTORE);
    return pc;
}
#endif /* PROFILE_LINKCOUNT **************************************/

static bool
is_patchable_exit_stub_helper(dcontext_t *dcontext, cache_pc ltarget,
                              ushort lflags, uint fflags)
{
    if (LINKSTUB_INDIRECT(lflags)) {
        /*indirect */
        if (!DYNAMO_OPTION(indirect_stubs))
            return false;
        if (
#ifdef WINDOWS
            !is_shared_syscall_routine(dcontext, ltarget) &&
#endif
            get_ibl_routine_code(dcontext, extract_branchtype(lflags), fflags)
                ->ibl_head_is_inlined) {
            return !DYNAMO_OPTION(atomic_inlined_linking);
        } else {
            return true;
        }
    } else {
        /* direct */
        ASSERT(LINKSTUB_DIRECT(lflags));
#if defined(PROFILE_LINKCOUNT) || defined(TRACE_HEAD_CACHE_INCR)
        return true;
#else
        return false;
#endif
    }
}

bool
is_patchable_exit_stub(dcontext_t *dcontext, linkstub_t *l, fragment_t *f)
{
    return is_patchable_exit_stub_helper(dcontext, EXIT_TARGET_TAG(dcontext, f, l),
                                         l->flags, f->flags);
}

bool
is_exit_cti_stub_patchable(dcontext_t *dcontext, instr_t *inst, uint frag_flags)
{
    app_pc target;
    /* we figure out what the linkstub flags should be
     * N.B.: we have to be careful to match the LINKSTUB_ macros
     */
    ushort lflags = (ushort) instr_exit_branch_type(inst);
    ASSERT_TRUNCATE(lflags, ushort, instr_exit_branch_type(inst));
    ASSERT(instr_is_exit_cti(inst));
    target = instr_get_branch_target_pc(inst);
    if (is_indirect_branch_lookup_routine(dcontext, target)) {
        lflags |= LINK_INDIRECT;
    } else {
        lflags |= LINK_DIRECT;
    }
    return is_patchable_exit_stub_helper(dcontext, target, lflags, frag_flags);
}

uint
bytes_for_exitstub_alignment(dcontext_t *dcontext, linkstub_t *l, fragment_t *f, byte *startpc)
{
    if (is_patchable_exit_stub(dcontext, l, f)) {
        /* assumption - we only hot patch the ending jmp of the exit stub
         * (and that exit stub size returns the right values) */
        ptr_uint_t shift = ALIGN_SHIFT_SIZE
            (startpc +
             exit_stub_size(dcontext, EXIT_TARGET_TAG(dcontext, f, l), f->flags) -
             EXIT_STUB_PATCH_OFFSET,
             EXIT_STUB_PATCH_SIZE, PAD_JMPS_ALIGNMENT);
#ifdef PROFILE_LINKCOUNT
        /* assumption doesn't hold because of the optimize ... */
        /* FIXME : once this is implemented re-enable the ifdefed out stats
         * in emit_fragment_common */
        ASSERT_NOT_IMPLEMENTED(false);
#endif
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(shift)));
        return (uint) shift;
    }
    return 0;
}

/* Returns an upper bound on the number of bytes that will be needed to add
 * this fragment to a trace */
uint
extend_trace_pad_bytes(fragment_t *add_frag)
{
    /* FIXME : this is a poor estimate, we could do better by looking at the
     * linkstubs and checking if we are inlining ibl, but since this is just
     * used by monitor.c for a max size check should be fine to overestimate
     * we'll just end up with slightly shorter max size traces */
    /* we don't trace through traces in normal builds, so don't worry about
     * number of exits (FIXME this also assumes bbs don't trace through
     * conditional or indirect branches) */
    ASSERT_NOT_IMPLEMENTED(!TEST(FRAG_IS_TRACE, add_frag->flags));
    /* Also, if -pad_jmps_shift_bb we assume that we don't need to remove
     * any nops from fragments added to traces since there shouldn't be any if
     * we only add bbs (nop_pad_ilist has an assert that verifies we don't add
     * any nops to bbs when -pad_jmps_shift_bb without marking as CANNOT_BE_TRACE,
     * so here we also verify that we only add bbs) - Xref PR 215179, UNIX syscall
     * fence exits and CLIENT_INTERFACE added/moved exits can lead to bbs with
     * additional hot_patchable locations.  We mark such bb fragments as CANNOT_BE_TRACE
     * in nop_pad_ilist() if -pad_jmps_mark_no_trace is set or assert otherwise to avoid
     * various difficulties so should not see them here. */
    /* A standard bb has at most 2 patchable locations (ends in conditional or ends
     * in indirect that is promoted to inlined). */
    return 2*MAX_PAD_SIZE;
}

/* return startpc shifted by the necessary bytes to pad patchable jmps of the
 * exit stub to proper alignment */
byte *
pad_for_exitstub_alignment(dcontext_t *dcontext, linkstub_t *l, fragment_t *f, byte *startpc)
{
    uint shift;
    ASSERT(PAD_FRAGMENT_JMPS(f->flags)); /* shouldn't call this otherwise */

    shift = bytes_for_exitstub_alignment(dcontext, l, f, startpc);
    if (shift > 0) {
        /* Pad with 1 byte instructions so looks nice in debuggers.
         * decode_fragment also checks for this as a sanity check.  Note,
         * while these instructions can never be reached, they will be decoded
         * by shift fcache pointers so must put something valid here. */
        SET_TO_DEBUG(startpc, shift);
        startpc += shift;
        STATS_PAD_JMPS_ADD(f->flags, num_shifted_stubs, 1);
        STATS_PAD_JMPS_ADD(f->flags, shifted_stub_bytes, shift);
    } else {
        STATS_PAD_JMPS_ADD(f->flags, num_stubs_no_shift, 1);
    }
    return startpc;
}

/* Only used if -no_pad_jmps_shift_{bb,trace}. FIXME this routine is expensive (the
 * instr_expand) and we may end up removing app nops (an optimizations but
 * not really what we're after here). */
void
remove_nops_from_ilist(dcontext_t *dcontext, instrlist_t *ilist _IF_DEBUG(bool recreating))
{
    instr_t *inst, *next_inst;

    for (inst = instrlist_first(ilist); inst != NULL; inst = next_inst) {
        /* FIXME : expensive, just expand instr before cti, function not used
         * if -no_pad_jmps_shift_{bb,trace} */
        inst = instr_expand(dcontext, ilist, inst);
        next_inst = instr_get_next(inst);
        if (instr_is_nop(inst)) {
            instrlist_remove(ilist, inst);
            DOSTATS({
                if (!recreating) {
                    STATS_INC(num_nops_removed);
                    STATS_ADD(num_nop_bytes_removed, instr_length(dcontext, inst));
                }
            });
            instr_destroy(dcontext, inst);
        }
    }
}


/* inserts any nop padding needed to ensure patchable branch offsets don't
 * cross cache line boundaries.  If emitting sets the offset field of all
 * instructions, else sets the translation for the added nops (for
 * recreating). If emitting and -pad_jmps_shift_{bb,trace} returns the number
 * of bytes to shift the start_pc by (this avoids putting a nop before the
 * first exit cti) else returns 0. */
uint
nop_pad_ilist(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist, bool emitting)
{
    instr_t *inst;
    uint offset = 0;
    int first_patch_offset = -1;
    uint start_shift = 0;
    /* if emitting prefix_size isn't set up yet */
    cache_pc starting_pc = f->start_pc + fragment_prefix_size(f->flags);
    ASSERT(emitting || f->prefix_size == fragment_prefix_size(f->flags));

    ASSERT(PAD_FRAGMENT_JMPS(f->flags)); /* shouldn't call this otherwise */

    for (inst = instrlist_first(ilist); inst != NULL; inst = instr_get_next(inst)) {
        /* don't support non exit cti patchable instructions yet */
        ASSERT_NOT_IMPLEMENTED(!TEST(INSTR_HOT_PATCHABLE, inst->flags));
        if (instr_is_exit_cti(inst)) {
            /* see if we need to be able to patch this instruction */
            if (is_exit_cti_patchable(dcontext, inst, f->flags)) {
                /* see if we are crossing a cache line, offset is the start of
                 * the next instr here so need to see if the dword ending at
                 * offset crosses a cache line */
                uint nop_length =
                    patchable_exit_cti_align_offs(dcontext, inst,
                                                  starting_pc+offset);
                if (first_patch_offset < 0)
                    first_patch_offset = offset;
                if (nop_length > 0) {
                    /* crosses cache line, nop pad */
                    /* instead of inserting a nop, shift the starting pc if
                     * we are within 1 cache line of the first patchable offset
                     * (this covers the case of a conditional branch which
                     * mangles to two patchable exits and is still safe since
                     * they are less then a cache line apart) */
                    if (PAD_JMPS_SHIFT_START(f->flags) &&
                        offset + instr_length(dcontext, inst) - first_patch_offset <
                        PAD_JMPS_ALIGNMENT) {
                        ASSERT(start_shift == 0); /* should only shift once */
                        start_shift = nop_length;
                        /* adjust the starting_pc, all previously checked
                         * instructions should be fine since we are still
                         * within the same cache line as the first patchable
                         * offset */
                        starting_pc += nop_length;
                    } else {
                        instr_t *nop_inst = INSTR_CREATE_nopNbyte(dcontext, nop_length);
#ifdef X64
                        if (FRAG_IS_32(f->flags)) {
                            instr_set_x86_mode(nop_inst, true/*x86*/);
                            instr_shrink_to_32_bits(nop_inst);
                        }
#endif
                        /* We expect bbs to never need this if
                         * -pad_jmps_shift_bb except on UNIX (signal fence exit) and
                         * CLIENT_INTERFACE (client can re-arrange fragment) where we
                         * -pad_jmps_mark_no_trace.  We rely on having not
                         * inserted any nops into traceable bbs in interp (so that we
                         * don't have to remove them when we build the trace).
                         * FIXME add tracing support so we don't have to mark
                         * CANNOT_BE_TRACE (see PR 215179). */
                        if (emitting && DYNAMO_OPTION(pad_jmps_mark_no_trace) &&
                            !TEST(FRAG_IS_TRACE, f->flags)) {
                            f->flags |= FRAG_CANNOT_BE_TRACE;
                            STATS_INC(pad_jmps_mark_no_trace);
                        } else {
                            ASSERT(TEST(FRAG_IS_TRACE, f->flags) ||
                                   TEST(FRAG_CANNOT_BE_TRACE, f->flags) ||
                                   !INTERNAL_OPTION(pad_jmps_shift_bb));
                        }
                        instrlist_preinsert(ilist, inst, nop_inst);
                        /* sanity check */
                        ASSERT((int)nop_length == instr_length(dcontext, nop_inst));
                        if (emitting) {
                            /* fixup offsets */
                            instr_set_note(nop_inst, (void *)(ptr_uint_t)offset);
                            /* only inc stats for emitting, not for recreating */
                            STATS_PAD_JMPS_ADD(f->flags, num_nops, 1);
                            STATS_PAD_JMPS_ADD(f->flags, nop_bytes, nop_length);
                        }
                        /* set translation whether emitting or not */
                        instr_set_translation(nop_inst, instr_get_translation(inst));
                        instr_set_our_mangling(nop_inst, true);
                        offset += nop_length;
                    }
                    /* sanity check that we fixed the alignment */
                    ASSERT(patchable_exit_cti_align_offs(dcontext, inst,
                                                         starting_pc+offset) == 0);
                } else {
                    DOSTATS({
                        /* only inc stats for emitting, not for recreating */
                        if (emitting)
                            STATS_PAD_JMPS_ADD(f->flags,
                                               num_no_pad_exits, 1);
                    });
                }
            }
        }
        if (emitting)
            instr_set_note(inst, (void *)(ptr_uint_t)offset); /* used by instr_encode */
        offset += instr_length(dcontext, inst);
    }
#ifdef CUSTOM_EXIT_STUBS
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    return start_shift;
}

/* for the hashtable lookup inlined into exit stubs, the
 * lookup routine is encoded earlier into a template,
 * (in the routine emit_inline_ibl_stub(), below)
 * which we copy here and fix up the linkstub ptr for.
 * when the hashtable changes, the mask and table are
 * updated in update_indirect_exit_stub(), below.
 */
static byte *
insert_inlined_ibl(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, byte *pc,
                   cache_pc unlinked_exit_target, uint flags)
{
    ibl_code_t *ibl_code =
        get_ibl_routine_code(dcontext,
                             extract_branchtype(l->flags), f->flags);
    byte *start_pc = pc;
    cache_pc linked_exit_target =
        get_linked_entry(dcontext, unlinked_exit_target);

    /* PR 248207: haven't updated the inlining to be x64-compliant yet */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(ibl_code->ibl_head_is_inlined);
    ASSERT(EXIT_HAS_STUB(l->flags, f->flags));
    memcpy(start_pc, ibl_code->inline_ibl_stub_template, ibl_code->inline_stub_length);

    /* exit should be unlinked initially */
    patch_branch(EXIT_CTI_PC(f, l), start_pc + ibl_code->inline_unlink_offs,
                 NOT_HOT_PATCHABLE);

    if (DYNAMO_OPTION(indirect_stubs)) {
        /* fixup linked/unlinked targets */
        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            insert_relative_target(start_pc + ibl_code->inline_linkedjmp_offs,
                                   linked_exit_target, NOT_HOT_PATCHABLE);
            insert_relative_target(start_pc + ibl_code->inline_unlinkedjmp_offs,
                                   unlinked_exit_target, NOT_HOT_PATCHABLE);
        } else {
            insert_relative_target(start_pc + ibl_code->inline_linkedjmp_offs,
                                   unlinked_exit_target, NOT_HOT_PATCHABLE);
        }
        /* set the linkstub ptr */
        pc = start_pc + ibl_code->inline_linkstub_first_offs;
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        *((uint *)pc) = (uint)(ptr_uint_t)l;
        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            pc = start_pc + ibl_code->inline_linkstub_second_offs;
            IF_X64(ASSERT_NOT_IMPLEMENTED(false));
            *((uint *)pc) = (uint)(ptr_uint_t)l;
        }
    } else {
        insert_relative_target(start_pc + ibl_code->inline_linkedjmp_offs,
                               linked_exit_target, NOT_HOT_PATCHABLE);
        insert_relative_target(start_pc + ibl_code->inline_unlink_offs
                               + 1 /* skip jmp opcode: see emit_inline_ibl_stub FIXME */,
                               unlinked_exit_target, NOT_HOT_PATCHABLE);
    }

    return start_pc + ibl_code->inline_stub_length;
}

static cache_pc
get_direct_exit_target(dcontext_t *dcontext, uint flags)
{
    if (FRAG_DB_SHARED(flags)) {
        if (TEST(FRAG_COARSE_GRAIN, flags)) {
            /* note that entrance stubs should target their unit's prefix,
             * who will then target this routine
             */
            return fcache_return_coarse_routine(IF_X64(FRAGMENT_GENCODE_MODE(flags)));
        } else
            return fcache_return_shared_routine(IF_X64(FRAGMENT_GENCODE_MODE(flags)));
    } else {
        return fcache_return_routine_ex(dcontext _IF_X64(FRAGMENT_GENCODE_MODE(flags)));
    }
}

static cache_pc
insert_save_xax(dcontext_t *dcontext, cache_pc pc, uint flags,
                bool shared, ushort tls_offs, bool require_addr16)
{
    return insert_spill_or_restore(dcontext, pc, flags, true/*spill*/,
                                   shared, REG_XAX, tls_offs, XAX_OFFSET,
                                   require_addr16);
}

/* restore xax in a stub or a fragment prefix */
static cache_pc
insert_restore_xax(dcontext_t *dcontext, cache_pc pc, uint flags,
                   bool shared, ushort tls_offs, bool require_addr16)
{
    return insert_spill_or_restore(dcontext, pc, flags, false/*restore*/,
                                   shared, REG_XAX, tls_offs, XAX_OFFSET,
                                   require_addr16);
}

/* Emit code for the exit stub at stub_pc.  Return the size of the
 * emitted code in bytes.  This routine assumes that the caller will
 * take care of any cache synchronization necessary (though none is
 * necessary on the Pentium).
 * The stub is unlinked initially, except coarse grain indirect exits,
 * which are always linked.
 */
int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f,
                             linkstub_t *l, cache_pc stub_pc, ushort l_flags)
{
    byte *pc = (byte *)stub_pc;
    cache_pc exit_target;
    bool indirect = false;
#ifdef PROFILE_LINKCOUNT
    bool linkcount = (dynamo_options.profile_counts && (f->flags & FRAG_IS_TRACE) != 0);
#endif
    bool can_inline = true;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));

    /* select the correct exit target */
    if (LINKSTUB_DIRECT(l_flags)) {
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* need to target the fcache return prefix */
            exit_target = fcache_return_coarse_prefix(stub_pc, NULL);
            ASSERT(exit_target != NULL);
        } else
            exit_target = get_direct_exit_target(dcontext, f->flags);
    } else {
        ASSERT(LINKSTUB_INDIRECT(l_flags));
        /* caller shouldn't call us if no stub */
        ASSERT(EXIT_HAS_STUB(l_flags, f->flags));
        if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
            /* need to target the ibl prefix */
            exit_target = get_coarse_ibl_prefix(dcontext, stub_pc,
                                                extract_branchtype(l_flags));
            ASSERT(exit_target != NULL);
        } else {
            /* initially, stub should be unlinked */
            exit_target = get_unlinked_entry(dcontext,
                                             EXIT_TARGET_TAG(dcontext, f, l));
        }
        indirect = true;
# ifdef WINDOWS
        can_inline = (exit_target != unlinked_shared_syscall_routine(dcontext));
# endif
        if (can_inline) {
            ibl_code_t *ibl_code =
                get_ibl_routine_code(dcontext,
                                     extract_branchtype(l_flags), f->flags);
            if (!ibl_code->ibl_head_is_inlined)
                can_inline = false;
        }
    }

    if (indirect && can_inline) {
        pc = insert_inlined_ibl(dcontext, f, l, pc, exit_target, f->flags);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(pc - stub_pc)));
        return (int) (pc - stub_pc);
    }

    if (indirect) {
        pc = insert_jmp_to_ibl(pc, f, l, exit_target, dcontext);
    } else if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* This is an entrance stub.  It may be executed even when linked,
         * so we store target info to memory instead of a register.
         * The exact bytes used here are assumed by entrance_stub_target_tag().
         */
#ifdef X64
        if (!FRAG_IS_32(f->flags)) {
            app_pc tgt = EXIT_TARGET_TAG(dcontext, f, l);
            /* Both entrance_stub_target_tag() and coarse_indirect_stub_jmp_target()
             * assume that the addr prefix is present for 32-bit but not 64-bit.
             */
            /* since we have no 8-byte-immed-to-memory, we split into two pieces */
            *pc = TLS_SEG_OPCODE; pc++;
            *pc = MOV_IMM2MEM_OPCODE; pc++;
            *pc = MODRM_BYTE(0/*mod*/, 0/*reg*/, 4/*rm*/); pc++; /* => no base, w/ sib */
            *pc = SIB_DISP32; pc++; /* just disp32 */
            /* low 32 bits */
            *((uint *)pc) = (uint) os_tls_offset(DIRECT_STUB_SPILL_SLOT); pc+=4;
            *((uint *)pc) = (uint)(ptr_uint_t) tgt; pc += 4;

            *pc = TLS_SEG_OPCODE; pc++;
            *pc = MOV_IMM2MEM_OPCODE; pc++;
            *pc = MODRM_BYTE(0/*mod*/, 0/*reg*/, 4/*rm*/); pc++; /* => no base, w/ sib */
            *pc = SIB_DISP32; pc++; /* just disp32 */
            /* high 32 bits */
            *((uint *)pc) = 4 + (uint) os_tls_offset(DIRECT_STUB_SPILL_SLOT); pc+=4;
            *((uint *)pc) = (uint)(((ptr_uint_t)tgt) >> 32); pc += 4;
        } else {
#endif /* X64 */
            /* We must be at or below 15 bytes so we require addr16.
             * Both entrance_stub_target_tag() and coarse_indirect_stub_jmp_target()
             * assume that the addr prefix is present for 32-bit but not 64-bit.
             */
            /* addr16 mov <target>, fs:<dir-stub-spill> */
            /* FIXME: PR 209709: test perf and remove if outweighs space */
            *pc = ADDR_PREFIX_OPCODE; pc++;
            *pc = TLS_SEG_OPCODE; pc++;
            *pc = MOV_IMM2MEM_OPCODE; pc++;
            *pc = MODRM16_DISP16; pc++;
            *((ushort *)pc) = os_tls_offset(DIRECT_STUB_SPILL_SLOT); pc+=2;
            *((uint *)pc) = (uint)(ptr_uint_t) EXIT_TARGET_TAG(dcontext, f, l); pc += 4;
#ifdef X64
        }
#endif /* X64 */
        /* jmp to exit target */
        pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);
    } else {
        /* direct branch */

        /* we use XAX to hold the linkstub pointer before we get to fcache_return,
           note that indirect stubs use XBX for linkstub pointer */
        pc = insert_save_xax(dcontext, pc, f->flags, FRAG_DB_SHARED(f->flags),
                             DIRECT_STUB_SPILL_SLOT, true);

#ifdef PROFILE_LINKCOUNT
        if (linkcount) {
            pc = insert_linkcount_saveflags(pc, dcontext, f->flags);
            pc = insert_linkcount_inc(pc, l);
            pc = insert_linkcount_restoreflags(pc, dcontext, f->flags);
        }
#endif
        /* mov $linkstub_ptr,%xax */
#ifdef X64
        if (FRAG_IS_32(f->flags)) {
            /* XXX i#829: we only support stubs in the low 4GB which is ok for
             * WOW64 mixed-mode but long-term for 64-bit flexibility (i#774) we
             * may need to store the other half of the pointer somewhere
             */
            uint l_uint;
            ASSERT_TRUNCATE(l_uint, uint, (ptr_uint_t)l);
            l_uint = (uint)(ptr_uint_t) l;
            *pc = MOV_IMM2XAX_OPCODE; pc++;
            *((uint *)pc) = l_uint; pc += sizeof(l_uint);
        } else {
            *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG; pc++;
#endif
            /* shared w/ 32-bit and 64-bit !FRAG_IS_32 */
            *pc = MOV_IMM2XAX_OPCODE; pc++;
            *((ptr_uint_t *)pc) = (ptr_uint_t)l; pc += sizeof(l);
#ifdef X64
        }
#endif
        /* jmp to exit target */
        pc = insert_relative_jump(pc, exit_target, NOT_HOT_PATCHABLE);

#ifdef PROFILE_LINKCOUNT
        if (linkcount) {
            /* this is where the unlinked entry point goes, but to
             * distinguish a never-linked stub we place a sentinel here
             */
            ASSERT(pc == stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags));
            *((uint *)pc) = LINKCOUNT_NEVER_LINKED_SENTINEL;
            pc += sizeof(uint);
            /* nop out rest of mem to play nice with the disassembler
             * (we're profile linkcount so presumably going to output) */
            memset(pc, 0x90 /* NOP */, STUB_DIRECT_SIZE(f->flags) - sizeof(uint));
            pc += STUB_DIRECT_SIZE(f->flags) - sizeof(uint);
        }
#endif
    }

    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(pc - stub_pc)));
    return (int) (pc - stub_pc);
}

int
insert_exit_stub(dcontext_t *dcontext, fragment_t *f,
                 linkstub_t *l, cache_pc stub_pc)
{
    return insert_exit_stub_other_flags(dcontext, f, l, stub_pc, l->flags);
}

static cache_pc
exit_cti_disp_pc(cache_pc branch_pc)
{
    cache_pc byte_ptr = branch_pc;
    byte opcode = *byte_ptr;
    uint length = 0;

    if (opcode == RAW_PREFIX_jcc_taken || opcode == RAW_PREFIX_jcc_not_taken) {
        length++;
        byte_ptr++;
        opcode = *byte_ptr;
        /* branch hints are only valid with jcc instrs, and if present on
         * other ctis we strip them out during mangling (i#435)
         */
        ASSERT(opcode == RAW_OPCODE_jcc_byte1);
    }
    if (opcode == ADDR_PREFIX_OPCODE) { /* used w/ jecxz/loop* */
        length++;
        byte_ptr++;
        opcode = *byte_ptr;
    }

    if (opcode >= RAW_OPCODE_loop_start && opcode <= RAW_OPCODE_loop_end) {
        /* assume that this is a mangled jcxz/loop*
         * target pc is in last 4 bytes of "9-byte instruction"
         */
        length += CTI_SHORT_REWRITE_LENGTH;
    } else if (opcode == RAW_OPCODE_jcc_byte1) {
        /* 2-byte opcode, 6-byte instruction, except for branch hint */
        ASSERT(*(byte_ptr+1) >= RAW_OPCODE_jcc_byte2_start &&
               *(byte_ptr+1) <= RAW_OPCODE_jcc_byte2_end);
        length += CBR_LONG_LENGTH;
    } else {
        /* 1-byte opcode, 5-byte instruction */
#ifdef HOT_PATCHING_INTERFACE
        ASSERT(opcode == RAW_OPCODE_jmp || opcode == RAW_OPCODE_call);
#else
        ASSERT(opcode == RAW_OPCODE_jmp);
#endif
        length += JMP_LONG_LENGTH;
    }
    return branch_pc + length - 4; /* disp is 4 even on x64 */
}

/* Patch the (direct) branch at branch_pc so it branches to target_pc
 * The write that actually patches the branch is done atomically so this
 * function is safe with respect to a thread executing this branch presuming
 * that both the before and after targets are valid and that [pc, pc+4) does
 * not cross a cache line.
 */
void
patch_branch(cache_pc branch_pc, cache_pc target_pc, bool hot_patch)
{
    cache_pc byte_ptr = exit_cti_disp_pc(branch_pc);
    insert_relative_target(byte_ptr, target_pc, hot_patch);
}

#ifdef PROFILE_LINKCOUNT
static byte *
change_linkcount_target(byte *pc, app_pc target)
{
    /* Once we've linked once, we modify the jmp at the end of the
     * link code in the stub to either jmp to the unlinked entry
     * (which has no counter inc code of its own, that's why the exit
     * jmp doesn't go straight there) or to the target.
     * To find the jmp, watch first opcode to determine which state
     * stub is in (depending on whether had to save eflags or not).
     */
    if (*pc == 0xff || *pc == 0x83) { /* inc/add is 1st instr */
        pc += LINKCOUNT_INCSIZE + 1;
    } else {
        IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* need to pass in flags */
        pc += LINKCOUNT_INCSIZE + LINKCOUNT_FLAGSIZE + STUB_DIRECT_SIZE(FRAG_32_BIT) - 4;
    }
    pc = insert_relative_target(pc, target, HOT_PATCHABLE);
    return pc;
}

static void
optimize_linkcount_stub(dcontext_t *dcontext, fragment_t *f,
                        linkstub_t *l, fragment_t *targetf)
{
    /* first-time link: try to remove eflags save/restore */
# ifdef CUSTOM_EXIT_STUBS
    byte *stub_pc = (byte *) EXIT_FIXED_STUB_PC(dcontext, f, l);
# else
    byte *stub_pc = (byte *) EXIT_STUB_PC(dcontext, f, l);
# endif
    byte *pc = stub_pc;
    bool remove_eflags_save = false;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_DIRECT(l->flags));

    if (!INTERNAL_OPTION(unsafe_ignore_eflags_prefix)) {
        remove_eflags_save = TEST(FRAG_WRITES_EFLAGS_6, targetf->flags);
    }
    else {
        /* scan through code at target fragment, stop scanning at 1st branch */
        uint eflags = 0;
        cache_pc end_pc = EXIT_CTI_PC(f, FRAGMENT_EXIT_STUBS(targetf));
        byte *fpc = (byte *) FCACHE_ENTRY_PC(targetf);
        /* for simplicity, stop at first instr that touches the flags */
        while (eflags == 0 && fpc != NULL && ((cache_pc)fpc) < end_pc) {
            fpc = decode_eflags_usage(dcontext, fpc, &eflags);
        }
        remove_eflags_save =
            (eflags & (EFLAGS_WRITE_6|EFLAGS_READ_6)) == EFLAGS_WRITE_6;
    }
    if (remove_eflags_save) {
        /* the 6 flags modified by add and adc are written before
         * they're read -> don't need to save eflags!
         *
         * I tried replacing lahf & sahf w/ nops, it's noticeably
         * faster to not have the nops, so redo the increment:
         */
        pc = insert_linkcount_inc(pc, l);
        pc = insert_relative_jump(pc, FCACHE_ENTRY_PC(targetf),
                                  NOT_HOT_PATCHABLE);
        /* Fill out with nops till the unlinked entry point so disassembles
         * nicely for logfile (we're profile linkcount so presumably going
         * to dump this). */
        while (pc < (stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags))) {
            *pc = 0x90; pc++;  /* nop */
        }
    } else {
        /* keep eflags save & restore -- need to keep save of eax
         * so skip all that now, go to right before store of &l into eax
         */
        pc += LINKCOUNT_DIRECT_EXTRA(f->flags) - 5 - 5;
        /* need to insert a restore of eax -- luckily it perfectly
         * overwrites the store of &l into eax, FIXME - dangerous
         * though, if we ever drop the addr16 flag on a shared restore the
         * instruction will be 6 bytes and our hardcoded 5 above will
         * lead to a crash (should trigger assert below at least).
         */
        pc = insert_restore_xax(dcontext, pc, f->flags, FRAG_DB_SHARED(f->flags),
                                DIRECT_STUB_SPILL_SLOT, true);
        ASSERT(pc == stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags) - 5);
        /* now add jmp */
        pc = insert_relative_jump(pc, FCACHE_ENTRY_PC(targetf),
                                  NOT_HOT_PATCHABLE);
    }

    /* we need to replace our never-linked sentinel w/ the real
     * unlinked entry point.
     */
    ASSERT(pc == stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags));
    pc = stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags);
    ASSERT(*((uint *)pc) == LINKCOUNT_NEVER_LINKED_SENTINEL);
    pc = insert_save_xax(dcontext, pc, f->flags, FRAG_DB_SHARED(f->flags),
                         DIRECT_STUB_SPILL_SLOT, true);
    /* mov $linkstub_ptr,%xax */
    *pc = MOV_IMM2XAX_OPCODE; pc++;
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    *((uint *)pc) = (uint)l; pc += 4;
    /* jmp to target */
    pc = insert_relative_jump(pc, get_direct_exit_target(dcontext, f->flags),
                              NOT_HOT_PATCHABLE);
}
#endif /* PROFILE_LINKCOUNT */

/* Checks patchable exit cti for proper alignment for patching. If it's
 * properly aligned returns 0, else returns the number of bytes it would
 * need to be forward shifted to be properly aligned */
uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc)
{
    /* all our exit cti's currently use 4 byte offsets */
    /* FIXME : would be better to use a instr_is_cti_long or some such
     * also should check for addr16 flag (we shouldn't have any prefixes) */
    ASSERT((instr_is_cti(inst) && !instr_is_cti_short(inst) &&
            !TESTANY(~(PREFIX_JCC_TAKEN|PREFIX_JCC_NOT_TAKEN), instr_get_prefixes(inst)))
            || instr_is_cti_short_rewrite(inst, NULL));
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                  (ALIGN_SHIFT_SIZE(pc + instr_length(dcontext, inst) - CTI_PATCH_SIZE,
                                    CTI_PATCH_SIZE, PAD_JMPS_ALIGNMENT))));
    return (uint) ALIGN_SHIFT_SIZE(pc + instr_length(dcontext, inst) - CTI_PATCH_SIZE,
                                   CTI_PATCH_SIZE, PAD_JMPS_ALIGNMENT);
}

/* Returns true if the exit cti is ever dynamically modified */
bool
is_exit_cti_patchable(dcontext_t *dcontext, instr_t *inst, uint frag_flags)
{
    app_pc target;
    if (TEST(FRAG_COARSE_GRAIN, frag_flags)) {
        /* Case 8647: coarse grain fragment bodies always link through stubs
         * until frozen, so their ctis are never patched except at freeze time
         * when we suspend the world.
         */
        ASSERT(!TEST(FRAG_IS_TRACE, frag_flags));
        return false;
    }
    ASSERT(instr_is_exit_cti(inst));
    target = instr_get_branch_target_pc(inst);
    if (is_indirect_branch_lookup_routine(dcontext, target)) {
        /* whether has an inline stub or not, cti is always
         * patched if -no_indirect_stubs
         */
        if (!DYNAMO_OPTION(indirect_stubs))
            return true;
#ifdef WINDOWS
        if (target != shared_syscall_routine(dcontext)) {
#endif
            return get_ibl_routine_code(dcontext,
                     extract_branchtype((ushort)instr_exit_branch_type(inst)),
                                        frag_flags)->ibl_head_is_inlined;
#ifdef WINDOWS
        }
        return false;
#endif
    } else {
        /* direct exit */
#ifdef PROFILE_LINKCOUNT
        if (DYNAMO_OPTION(profile_counts) && TEST(FRAG_IS_TRACE, frag_flags)) {
# ifdef CUSTOM_EXIT_STUBS
            return true;
# else
            return false;
# endif
        }
#endif
        if (instr_branch_special_exit(inst))
            return false;
        return true;
    }
}

/* returns true if exit cti no longer points at stub
 * (certain situations, like profiling or TRACE_HEAD_CACHE_INCR, go
 * through the stub even when linked)
 */
bool
link_direct_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, fragment_t *targetf,
                 bool hot_patch)
{
#if defined(PROFILE_LINKCOUNT) || defined(TRACE_HEAD_CACHE_INCR)
# ifdef CUSTOM_EXIT_STUBS
    byte *stub_pc = (byte *) (EXIT_FIXED_STUB_PC(dcontext, f, l));
# else
    byte *stub_pc = (byte *) (EXIT_STUB_PC(dcontext, f, l));
# endif
#endif
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_DIRECT(l->flags));
    STATS_INC(num_direct_links);

#ifdef PROFILE_LINKCOUNT
    if (dynamo_options.profile_counts && TEST(FRAG_IS_TRACE, f->flags)) {
        /* do not change the exit jmp, instead change the stub itself */
        if (*((uint *)(stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags))) ==
            LINKCOUNT_NEVER_LINKED_SENTINEL) {
            /* this is not atomic, but that's ok, it's first-time only */
            /* FIXME - this assumption is so not safe with shared cache
             * since we add to table and link incoming before linking outgoing
             */
            optimize_linkcount_stub(dcontext, f, l, targetf);
# ifdef CUSTOM_EXIT_STUBS
            /* FIXME: want flag that says whether should go through custom
             * only when unlinked, or always!
             * For now we assume only when unlinked:
             */
            /* skip custom code */
            patch_branch(EXIT_CTI_PC(f, l), stub_pc,
                         TEST(FRAG_SHARED, f->flags) ? hot_patch : NOT_HOT_PATCHABLE);
# endif
        } else {
# ifdef CUSTOM_EXIT_STUBS
            /* FIXME: want flag that says whether should go through custom
             * only when unlinked, or always!
             * For now we assume only when unlinked:
             */
            /* skip custom code */
            patch_branch(EXIT_CTI_PC(f, l), stub_pc, hot_patch);
# endif
            change_linkcount_target(stub_pc, FCACHE_ENTRY_PC(targetf));
        }
# ifdef TRACE_HEAD_CACHE_INCR
        /* yes, we wait for linkcount to do its thing and then we change it --
         * but to make it more efficient will make this already ungainly
         * code even harder to read
         */
        /* FIXME - atomicity issues? */
        if ((targetf->flags & FRAG_IS_TRACE_HEAD) != 0) {
            /* after optimized inc, jmp to unlinked code, but change its final
             * jmp to go to incr routine
             */
            change_linkcount_target(stub_pc, stub_pc + LINKCOUNT_UNLINKED_ENTRY(f->flags));
            LOG(THREAD, LOG_LINKS, 4,
                "\tlinking F%d."PFX" to incr routine b/c F%d is trace head\n",
                f->id, EXIT_CTI_PC(f, l), targetf->id);
            patch_branch(stub_pc + LINKCOUNT_UNLINKED_ENTRY(f->flags) + 10,
                         trace_head_incr_routine(dcontext), hot_patch);
        }
# endif
        return false; /* going through stub */
    }
#endif /* PROFILE_LINKCOUNT */

#ifdef TRACE_HEAD_CACHE_INCR
    if ((targetf->flags & FRAG_IS_TRACE_HEAD) != 0) {
        LOG(THREAD, LOG_LINKS, 4,
            "\tlinking F%d."PFX" to incr routine b/c F%d is trace head\n",
            f->id, EXIT_CTI_PC(f, l), targetf->id);
        /* FIXME: more efficient way than multiple calls to get size-5? */
        ASSERT(linkstub_size(dcontext, f, l) == DIRECT_EXIT_STUB_SIZE(f->flags));
        patch_branch(stub_pc + DIRECT_EXIT_STUB_SIZE(f->flags) - 5,
                     trace_head_incr_routine(dcontext), hot_patch);
        return false; /* going through stub */
    }
#endif

    /* change jmp target to point to the passed-in target */
#ifdef UNSUPPORTED_API
    if ((l->flags & LINK_TARGET_PREFIX) != 0) {
        /* want to target just the xcx restore, not the eflags restore
         * (only ibl targets eflags restore)
         */
        patch_branch(EXIT_CTI_PC(f, l), FCACHE_PREFIX_ENTRY_PC(targetf),
                     hot_patch);
    } else
#endif
        patch_branch(EXIT_CTI_PC(f, l), FCACHE_ENTRY_PC(targetf), hot_patch);
    return true; /* do not need stub anymore */
}

void
unlink_direct_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    cache_pc stub_pc = (cache_pc) EXIT_STUB_PC(dcontext, f, l);
#ifdef TRACE_HEAD_CACHE_INCR
    direct_linkstub_t *dl = (direct_linkstub_t *) l;
#endif
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_DIRECT(l->flags));

#ifdef PROFILE_LINKCOUNT
    if (dynamo_options.profile_counts && TEST(FRAG_IS_TRACE, f->flags)) {
        byte *pc;
        if (*((uint *)(stub_pc + LINKCOUNT_DIRECT_EXTRA(f->flags))) ==
            LINKCOUNT_NEVER_LINKED_SENTINEL) {
            /* never been linked, don't go pointing at the uninitialized
             * unlink entry point -- just return, initial state is fine
             */
            return;
        }
# ifdef CUSTOM_EXIT_STUBS
        pc = (byte *) (EXIT_FIXED_STUB_PC(dcontext, f, l));
        stub_pc = (cache_pc) pc;
        /* FIXME: want flag that says whether should go through custom
         * only when unlinked, or always! Also is racy with 2nd branch patch.
         * For now we assume only when unlinked.
         */
        /* go through custom code again */
        patch_branch(EXIT_CTI_PC(f, l), stub_pc, HOT_PATCHABLE);
# else
        pc = (byte *) stub_pc;
# endif
# ifdef TRACE_HEAD_CACHE_INCR
        if (dl->target_fragment != NULL) { /* HACK to tell if targeted trace head */
            /* make unlinked jmp go back to fcache_return */
            patch_branch(pc + LINKCOUNT_UNLINKED_ENTRY(f->flags) + 10,
                         get_direct_exit_target(dcontext, f->flags),
                         HOT_PATCHABLE);
        } else
# endif
            /* make jmp after incr go to unlinked entry */
            change_linkcount_target(pc, stub_pc + LINKCOUNT_UNLINKED_ENTRY(f->flags));
        return;
    }
#endif

#ifdef TRACE_HEAD_CACHE_INCR
    if (dl->target_fragment != NULL) { /* HACK to tell if targeted trace head */
# ifdef CUSTOM_EXIT_STUBS
        byte *pc = (byte *) (EXIT_FIXED_STUB_PC(dcontext, f, l));
# else
        byte *pc = (byte *) (EXIT_STUB_PC(dcontext, f, l));
# endif
        /* FIXME: more efficient way than multiple calls to get size-5? */
        ASSERT(linkstub_size(dcontext, f, l) == DIRECT_EXIT_STUB_SIZE(f->flags));
        patch_branch(pc + DIRECT_EXIT_STUB_SIZE(f->flags) - 5,
                     get_direct_exit_target(dcontext, f->flags),
                     HOT_PATCHABLE);
    }
#endif

    /* change jmp target to point to top of exit stub */
    patch_branch(EXIT_CTI_PC(f, l), stub_pc, HOT_PATCHABLE);
}

/* NOTE : for inlined indirect branches linking is !NOT! atomic with respect
 * to a thread executing in the cache unless using the atomic_inlined_linking
 * option (unlike unlinking)
 */
void
link_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, bool hot_patch)
{
    uint stub_size;
    app_pc exit_target, cur_target;
    app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
    byte *pc;
    /* w/ indirect exits now having their stub pcs computed based
     * on the cti targets, we must calculate them at a consistent
     * state (we do have multi-stage modifications for inlined stubs)
     */
    byte *stub_pc = (byte *) EXIT_STUB_PC(dcontext, f, l);
#ifdef CUSTOM_EXIT_STUBS
    byte *fixed_stub_pc = (byte *) EXIT_FIXED_STUB_PC(dcontext, f, l);
#endif

    ASSERT(!TEST(FRAG_COARSE_GRAIN, f->flags));

    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    /* target is always the same, so if it's already linked, this is a nop */
    if ((l->flags & LINK_LINKED) != 0) {
        STATS_INC(num_indirect_already_linked);
        return;
    }
    STATS_INC(num_indirect_links);

# ifdef WINDOWS
    if (!is_shared_syscall_routine(dcontext, target_tag))
# endif
    {
        ibl_code_t *ibl_code =
            get_ibl_routine_code(dcontext,
                                 extract_branchtype(l->flags), f->flags);

        if (ibl_code->ibl_head_is_inlined) {
            /* need to make branch target the top of the exit stub */
            patch_branch(EXIT_CTI_PC(f, l), stub_pc, hot_patch);
            if (DYNAMO_OPTION(atomic_inlined_linking)) {
                return;
            }
        }
    }

    if (DYNAMO_OPTION(indirect_stubs)) {
        /* go to start of 5-byte jump instruction at end of exit stub */
        stub_size = exit_stub_size(dcontext, target_tag, f->flags);
# ifdef CUSTOM_EXIT_STUBS
        pc = fixed_stub_pc + stub_size - 5;
# else
        pc = stub_pc + stub_size - 5;
# endif
    } else {
        /* cti goes straight to ibl, and must be a jmp, not jcc,
         * except for -unsafe_ignore_eflags_trace stay-on-trace cmp,jne
         */
        pc = EXIT_CTI_PC(f, l);
        /* for x64, or -unsafe_ignore_eflags_trace, a trace may have a jne to the stub */
        if (*pc == JNE_OPCODE_1) {
            ASSERT(TEST(FRAG_IS_TRACE, f->flags));
#ifndef X64
            ASSERT(INTERNAL_OPTION(unsafe_ignore_eflags_trace));
#endif
            /* FIXME: share this code w/ common path below: 1 opcode byte vs 2 */
            /* get absolute address of target */
            cur_target = (app_pc) PC_RELATIVE_TARGET(pc+2);
            exit_target = get_linked_entry(dcontext, cur_target);
            pc += 2; /* skip jne opcode */
            pc = insert_relative_target(pc, exit_target, hot_patch);
            return;
        } else {
            ASSERT(*pc == JMP_OPCODE);
        }
    }
    /* get absolute address of target */
    cur_target = (app_pc) PC_RELATIVE_TARGET(pc+1);
    exit_target = get_linked_entry(dcontext, cur_target);
    pc++; /* skip jmp opcode */
    pc = insert_relative_target(pc, exit_target, hot_patch);
}

int
linkstub_unlink_entry_offset(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    ibl_code_t *ibl_code;
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    if (!LINKSTUB_INDIRECT(l->flags))
        return 0;
#ifdef WINDOWS
    if (is_shared_syscall_routine(dcontext, EXIT_TARGET_TAG(dcontext, f, l)))
        return 0;
#endif
    ibl_code = get_ibl_routine_code(dcontext, extract_branchtype(l->flags), f->flags);
    if (ibl_code->ibl_head_is_inlined)
        return ibl_code->inline_unlink_offs;
    else
        return 0;
}

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    cache_pc cti = EXIT_CTI_PC(f, l);
    /* decode the cti: it should be a relative jmp to the stub */
    cache_pc stub;
    if (!EXIT_HAS_STUB(l->flags, f->flags))
        return NULL;
    /* for x64, or -unsafe_ignore_eflags_trace, a trace may have a jne to the stub */
    if (*cti == JNE_OPCODE_1) {
        ASSERT(TEST(FRAG_IS_TRACE, f->flags));
#ifndef X64
        ASSERT(INTERNAL_OPTION(unsafe_ignore_eflags_trace));
#endif
        stub = (cache_pc) PC_RELATIVE_TARGET(cti+2/*opcode bytes*/);
    } else {
        if (*cti == JMP_OPCODE) {
            stub = (cache_pc) PC_RELATIVE_TARGET(cti+1/*opcode byte*/);
        } else {
            /* case 6532/10987: frozen coarse has no jmp to stub */
            ASSERT(TEST(FRAG_COARSE_GRAIN, f->flags));
            ASSERT(coarse_is_indirect_stub(cti));
            stub = cti;
        }
    }
    ASSERT(stub >= cti && (stub - cti) <= MAX_FRAGMENT_SIZE);
    if (!TEST(LINK_LINKED, l->flags)) {
        /* the unlink target is not always the start of the stub */
        stub -= linkstub_unlink_entry_offset(dcontext, f, l);
        /* FIXME: for -no_indirect_stubs we could point exit cti directly
         * at unlink ibl routine if we could find the stub target for
         * linking here...should consider storing stub pc for ind exits
         * for that case to save 5 bytes in the inlined stub
         */
    }
    return stub;
}

cache_pc
indirect_linkstub_target(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    ASSERT(!TESTANY(LINK_NI_SYSCALL_ALL, l->flags));
#ifdef WINDOWS
    if (EXIT_TARGETS_SHARED_SYSCALL(l->flags)) {
        /* currently this is the only way to distinguish shared_syscall
         * exit from other indirect exits and from other exits in
         * a fragment containing ignorable or non-ignorable syscalls
         */
        ASSERT(TEST(FRAG_HAS_SYSCALL, f->flags));
        return shared_syscall_routine_ex(dcontext
                                         _IF_X64(FRAGMENT_GENCODE_MODE(f->flags)));
    }
#endif
    if (TEST(FRAG_COARSE_GRAIN, f->flags)) {
        /* Need to target the ibl prefix.  Passing in cti works as well as stub,
         * and avoids a circular dependence where linkstub_unlink_entry_offset()
         * call this routine to get the target and then this routine asks for
         * the stub which calls linkstub_unlink_entry_offset()...
         */
        return get_coarse_ibl_prefix(dcontext, EXIT_CTI_PC(f, l),
                                     extract_branchtype(l->flags));
    } else {
        return get_ibl_routine_ex(dcontext, get_ibl_entry_type(l->flags),
                                  get_source_fragment_type(dcontext, f->flags),
                                  extract_branchtype(l->flags)
                                  _IF_X64(FRAGMENT_GENCODE_MODE(f->flags)));
    }
}

/* based on machine state, returns which of cbr l1 and fall-through l2
 * must have been taken
 */
linkstub_t *
linkstub_cbr_disambiguate(dcontext_t *dcontext, fragment_t *f,
                          linkstub_t *l1, linkstub_t *l2)
{
    instr_t instr;
    linkstub_t *taken;
    instr_init(dcontext, &instr);
    decode(dcontext, EXIT_CTI_PC(f, l1), &instr);
    ASSERT(instr_is_cbr(&instr));
    if (instr_cbr_taken(&instr, get_mcontext(dcontext), false/*post-state*/))
        taken = l1;
    else
        taken = l2;
    instr_free(dcontext, &instr);
    return taken;
}

/* since we now support branch hints on long cbrs, we need to do a little
 * decoding to find their length
 */
cache_pc
cbr_fallthrough_exit_cti(cache_pc prev_cti_pc)
{
    if (*prev_cti_pc == RAW_PREFIX_jcc_taken ||
        *prev_cti_pc == RAW_PREFIX_jcc_not_taken)
        prev_cti_pc++;
    return (prev_cti_pc + CBR_LONG_LENGTH);
}

/* This is an atomic operation with respect to a thread executing in the
 * cache (barring ifdef NATIVE_RETURN, which is now removed), for
 * inlined indirect exits the
 * unlinked path of the ibl routine detects the race condition between the
 * two patching writes and handles it appropriately unless using the
 * atomic_inlined_linking option in which case there is only one patching
 * write (since tail is duplicated) */
void
unlink_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    uint stub_size;
    cache_pc exit_target, cur_target;
    app_pc target_tag = EXIT_TARGET_TAG(dcontext, f, l);
    byte *pc;
    ibl_code_t *ibl_code = NULL;
    /* w/ indirect exits now having their stub pcs computed based
     * on the cti targets, we must calculate them at a consistent
     * state (we do have multi-stage modifications for inlined stubs)
     */
#ifdef CUSTOM_EXIT_STUBS
    byte *fixed_stub_pc = (byte *) EXIT_FIXED_STUB_PC(dcontext, f, l);
#else
    byte *stub_pc = (byte *) EXIT_STUB_PC(dcontext, f, l);
#endif
    ASSERT(!TEST(FRAG_COARSE_GRAIN, f->flags));
    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    /* target is always the same, so if it's already unlinked, this is a nop */
    if (!TEST(LINK_LINKED, l->flags))
        return;

# ifdef WINDOWS
    if (!is_shared_syscall_routine(dcontext, target_tag))
# endif
        ibl_code = get_ibl_routine_code(dcontext,
                                        extract_branchtype(l->flags), f->flags);

    if ((!DYNAMO_OPTION(atomic_inlined_linking) && DYNAMO_OPTION(indirect_stubs)) ||
# ifdef WINDOWS
        target_tag == shared_syscall_routine_ex
        (dcontext _IF_X64(FRAGMENT_GENCODE_MODE(f->flags))) ||
# endif
        /* FIXME: for -no_indirect_stubs and inlined ibl, we'd like to directly
         * target the unlinked ibl entry but we don't yet -- see FIXME in
         * emit_inline_ibl_stub()
         */
        !ibl_code->ibl_head_is_inlined) {

        if (DYNAMO_OPTION(indirect_stubs)) {
            /* go to start of 5-byte jump instruction at end of exit stub */
            stub_size = exit_stub_size(dcontext, target_tag, f->flags);
# ifdef CUSTOM_EXIT_STUBS
            pc = fixed_stub_pc + stub_size - 5;
# else
            pc = stub_pc + stub_size - 5;
# endif
        } else {
            /* cti goes straight to ibl, and must be a jmp, not jcc */
            pc = EXIT_CTI_PC(f, l);
            /* for x64, or -unsafe_ignore_eflags_trace, a trace may have a jne */
            if (*pc == JNE_OPCODE_1) {
                ASSERT(TEST(FRAG_IS_TRACE, f->flags));
#ifndef X64
                ASSERT(INTERNAL_OPTION(unsafe_ignore_eflags_trace));
#endif
                pc++; /* 2-byte opcode, skip 1st here */
            } else
                ASSERT (*pc == JMP_OPCODE);
        }
        cur_target = (cache_pc) PC_RELATIVE_TARGET(pc+1);
        exit_target = get_unlinked_entry(dcontext, cur_target);
        pc++; /* skip jmp opcode */
        pc = insert_relative_target(pc, exit_target, HOT_PATCHABLE);
    }

    /* To maintain atomicity with respect to executing thread, must unlink
     * the ending jmp (above) first so that the unlinked path can detect the
     * race condition case */
# ifdef WINDOWS
    /* faster than is_shared_syscall_routine() since only linked target can get here
     * yet inconsistent
     */
    if (target_tag != shared_syscall_routine_ex
        (dcontext _IF_X64(FRAGMENT_GENCODE_MODE(f->flags))))
# endif
    {
        /* need to make branch target the unlink entry point inside exit stub */
        if (ibl_code->ibl_head_is_inlined) {
# ifdef CUSTOM_EXIT_STUBS
            /* FIXME: now custom code will be skipped!!! */
            cache_pc target = fixed_stub_pc;
# else
            cache_pc target = stub_pc;
# endif

            /* now add offset of unlinked entry */
            target += ibl_code->inline_unlink_offs;
            patch_branch(EXIT_CTI_PC(f, l), target, HOT_PATCHABLE);
        }
    }
}

/*******************************************************************************
 * COARSE-GRAIN FRAGMENT SUPPORT
 */

cache_pc
entrance_stub_jmp(cache_pc stub)
{
#ifdef X64
    if (*stub == 0x65)
        return (stub + STUB_COARSE_DIRECT_SIZE64 - JMP_LONG_LENGTH);
    /* else, 32-bit stub */
#endif
    return (stub + STUB_COARSE_DIRECT_SIZE32 - JMP_LONG_LENGTH);
}

/* Returns whether stub is an entrance stub as opposed to a fragment
 * or a coarse indirect stub.  FIXME: if we separate coarse indirect
 * stubs from bodies we'll need to put them somewhere else, or fix up
 * decode_fragment() to be able to distinguish them in some other way
 * like first instruction tls slot.
 */
bool
coarse_is_entrance_stub(cache_pc stub)
{
    bool res = false;
    /* FIXME: case 10334: pass in info and if non-NULL avoid lookup here? */
    coarse_info_t *info = get_stub_coarse_info(stub);
    if (info != NULL) {
        res = ALIGNED(stub, coarse_stub_alignment(info)) &&
            *entrance_stub_jmp(stub) == JMP_OPCODE;
        DOCHECK(1, {
            if (res) {
                cache_pc tgt = entrance_stub_jmp_target(stub);
                ASSERT(!in_fcache(stub));
                ASSERT(tgt == trace_head_return_coarse_prefix(stub, info) ||
                       tgt == fcache_return_coarse_prefix(stub, info) ||
                       /* another fragment */
                       in_fcache(tgt));
            }
        });
    }
    return res;
}

/* FIXME: case 10334: pass in info? */
bool
coarse_is_trace_head(cache_pc stub)
{
    if (coarse_is_entrance_stub(stub)) {
        cache_pc tgt = entrance_stub_jmp_target(stub);
        /* FIXME: could see if tgt is a jmp and deref and cmp to
         * trace_head_return_coarse_routine() to avoid the vmvector
         * lookup required to find the prefix
         */
        return tgt == trace_head_return_coarse_prefix(stub, NULL);
    }
    return false;
}

cache_pc
entrance_stub_jmp_target(cache_pc stub)
{
    cache_pc jmp = entrance_stub_jmp(stub);
    cache_pc tgt;
    ASSERT(jmp != NULL);
    tgt = (cache_pc) PC_RELATIVE_TARGET(jmp+1);
    ASSERT(*jmp == JMP_OPCODE);
    return tgt;
}

app_pc
entrance_stub_target_tag(cache_pc stub, coarse_info_t *info)
{
    cache_pc jmp = entrance_stub_jmp(stub);
    app_pc tag;
    /* find the immed that is put into tls: at end of pre-jmp instr */
#ifdef X64
    /* To identify whether 32-bit: we could look up the coarse_info_t
     * this is part of but that's expensive so we check whether the
     * tls offset has 2 high byte 0's (we always use addr16 for 32-bit).
     * 32-bit:
     *   67 64 c7 06 e0 0e 02 99 4e 7d  addr16 mov $0x7d4e9902 -> %fs:0x0ee0
     * 64-bit is split into high and low dwords:
     *   65 c7 04 25 20 16 00 00 02 99 4e 7d  mov $0x7d4e9902 -> %gs:0x1620
     *   65 c7 04 25 24 16 00 00 00 00 00 00  mov $0x00000000 -> %gs:0x1624
     * both are followed by a direct jmp.
     */
    if (*((ushort *)(jmp-6)) == 0) { /* 64-bit has 2 0's for high 2 bytes of tls offs */
        ptr_uint_t high32 = (ptr_uint_t) *((uint *)(jmp-4));
        ptr_uint_t low32 = (ptr_uint_t)
            *((uint *)(jmp - (SIZE64_MOV_PTR_IMM_TO_TLS/2) - 4));
        tag = (cache_pc) ((high32 << 32) | low32);
    } else { /* else fall-through to 32-bit case */
#endif
        tag = *((cache_pc *)(jmp-4));
#ifdef X64
    }
#endif
    /* if frozen, this could be a persist-time app pc (i#670).
     * we take in info so we can know mod_shift (we can decode to find it
     * for unlinked but not for linked)
     */
    if (info == NULL)
        info = get_stub_coarse_info(stub);
    if (info->mod_shift != 0 &&
        tag >= info->persist_base &&
        tag < info->persist_base + (info->end_pc - info->base_pc))
        tag -= info->mod_shift;
    return tag;
}

bool
coarse_is_indirect_stub(cache_pc pc)
{
    /* match insert_jmp_to_ibl */
    return instr_raw_is_tls_spill(pc, REG_XBX, INDIRECT_STUB_SPILL_SLOT);
}

/* caller should call fragment_coarse_entry_pclookup() ahead of time
 * to avoid deadlock if caller holds info->lock
 */
bool
coarse_cti_is_intra_fragment(dcontext_t *dcontext, coarse_info_t *info,
                             instr_t *inst, cache_pc start_pc)
{
    /* We don't know the size of the fragment but we want to support
     * intra-fragment ctis for clients (i#665) so we use some
     * heuristics.  A real cti is either linked to a target within the
     * same coarse unit (where its target will be an entry point) or
     * points at a stub of some kind (frozen exit prefix or separate
     * entrance stub or inlined indirect stub).
     */
    cache_pc tgt = opnd_get_pc(instr_get_target(inst));
    if (tgt < start_pc ||
        tgt >= start_pc + MAX_FRAGMENT_SIZE ||
        /* if tgt is an entry, then it's a linked exit cti
         * XXX: this may acquire info->lock if it's never been called before
         */
        fragment_coarse_entry_pclookup(dcontext, info, tgt) != NULL ||
        /* these lookups can get expensive but should only hit them
         * when have clients adding intra-fragment ctis.
         * XXX: is there a min distance we could use to rule out
         * being in stubs?  for frozen though prefixes are
         * right after cache.
         */
        coarse_is_indirect_stub(tgt) ||
        in_coarse_stubs(tgt) ||
        in_coarse_stub_prefixes(tgt)) {
        return false;
    } else
        return true;
}

cache_pc
coarse_indirect_stub_jmp_target(cache_pc stub)
{
    cache_pc prefix_tgt, tgt;
    cache_pc jmp;
    size_t stub_size;
#ifdef X64
    /* See the stub sequences in entrance_stub_target_tag(): 32-bit always has
     * an addr prefix while 64-bit does not
     */
    /* FIXME: PR 209709: test perf and remove if outweighs space */
    if (*stub == ADDR_PREFIX_OPCODE)
        stub_size = STUB_COARSE_INDIRECT_SIZE(FRAG_32_BIT);
    else /* default */
#endif
        stub_size = STUB_COARSE_INDIRECT_SIZE(0);
    jmp = stub + stub_size - JMP_LONG_LENGTH;
    ASSERT(*jmp == JMP_OPCODE);
    prefix_tgt = (cache_pc) PC_RELATIVE_TARGET(jmp+1);
    ASSERT(*prefix_tgt == JMP_OPCODE);
    tgt = (cache_pc) PC_RELATIVE_TARGET(prefix_tgt+1);
    return tgt;
}

uint
coarse_indirect_stub_size(coarse_info_t *info)
{
    /* Keep in synch w/ exit_stub_size().  We export this separately since
     * it's difficult to get the target to pass to exit_stub_size().
     */
    return STUB_COARSE_INDIRECT_SIZE(COARSE_32_FLAG(info));
}

/* Passing in stub's info avoids a vmvector lookup */
bool
entrance_stub_linked(cache_pc stub, coarse_info_t *info /*OPTIONAL*/)
{
    /* entrance stubs are of two types:
     * - targeting trace heads: always point to trace_head_return_coarse,
     *   whether target exists or not, so are always unlinked;
     * - targeting non-trace-heads: if linked, point to fragment; if unlinked,
     *   point to fcache_return_coarse
     */
    cache_pc tgt = entrance_stub_jmp_target(stub);
    /* FIXME: do vmvector just once instead of for each call */
    return (tgt != trace_head_return_coarse_prefix(stub, info) &&
            tgt != fcache_return_coarse_prefix(stub, info));
}

/* Returns whether it had to change page protections */
static bool
patch_coarse_branch(cache_pc stub, cache_pc tgt, bool hot_patch,
                    coarse_info_t *info /*OPTIONAL*/)
{
    bool stubs_readonly = false;
    bool stubs_restore = false;
    if (DYNAMO_OPTION(persist_protect_stubs)) {
        if (info == NULL)
            info = get_stub_coarse_info(stub);
        ASSERT(info != NULL);
        if (info->stubs_readonly) {
            stubs_readonly = true;
            stubs_restore = true;
            /* if we don't preserve mapped-in COW state the protection change
             * will fail (case 10570)
             */
            make_copy_on_writable((byte *)PAGE_START(entrance_stub_jmp(stub)),
                                  /* stub jmp can't cross page boundary (can't
                                   * cross cache line in fact) */
                                  PAGE_SIZE);
            if (DYNAMO_OPTION(persist_protect_stubs_limit) > 0) {
                info->stubs_write_count++;
                if (info->stubs_write_count >
                    DYNAMO_OPTION(persist_protect_stubs_limit)) {
                    SYSLOG_INTERNAL_WARNING_ONCE("pcache stubs over write limit");
                    STATS_INC(pcache_unprot_over_limit);
                    stubs_restore = false;
                    info->stubs_readonly = false;
                }
            }
        }
    }
    patch_branch(entrance_stub_jmp(stub), tgt, HOT_PATCHABLE);
    if (stubs_restore)
        make_unwritable((byte *)PAGE_START(entrance_stub_jmp(stub)), PAGE_SIZE);
    return stubs_readonly;
}

/* Passing in stub's info avoids a vmvector lookup */
void
link_entrance_stub(dcontext_t *dcontext, cache_pc stub, cache_pc tgt,
                   bool hot_patch, coarse_info_t *info /*OPTIONAL*/)
{
    ASSERT(DYNAMO_OPTION(coarse_units));
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    LOG(THREAD, LOG_LINKS, 5, "link_entrance_stub "PFX"\n", stub);
    if (patch_coarse_branch(stub, tgt, hot_patch, info))
        STATS_INC(pcache_unprot_link);
    /* We check this afterward since this link may be what makes it consistent
     * FIXME: pass in arg to not check target?  Then call before and after */
    ASSERT(coarse_is_entrance_stub(stub));
}

/* Passing in stub's info avoids a vmvector lookup */
void
unlink_entrance_stub(dcontext_t *dcontext, cache_pc stub, uint flags,
                     coarse_info_t *info /*OPTIONAL*/)
{
    cache_pc tgt;
    ASSERT(DYNAMO_OPTION(coarse_units));
    ASSERT(coarse_is_entrance_stub(stub));
    ASSERT(self_owns_recursive_lock(&change_linking_lock));
    LOG(THREAD, LOG_LINKS, 5,
        "unlink_entrance_stub "PFX"\n", stub);
    if (TESTANY(FRAG_IS_TRACE_HEAD|FRAG_IS_TRACE, flags))
        tgt = trace_head_return_coarse_prefix(stub, info);
    else
        tgt = fcache_return_coarse_prefix(stub, info);
    if (patch_coarse_branch(stub, tgt, HOT_PATCHABLE, info))
        STATS_INC(pcache_unprot_unlink);
}

cache_pc
entrance_stub_from_cti(cache_pc cti)
{
    cache_pc disp = exit_cti_disp_pc(cti);
    cache_pc tgt = (cache_pc) PC_RELATIVE_TARGET(disp);
    return tgt;
}

/*******************************************************************************/

/* Patch list support routines */
void
init_patch_list(patch_list_t *patch, patch_list_type_t type)
{
    patch->num_relocations = 0;
    /* Cast to int to avoid a tautological comparison warning from clang. */
    ASSERT_TRUNCATE(patch->type, ushort, (int)type);
    patch->type = (ushort) type;
}

/* add an instruction to patch list and address of location for future updates */
/* Use the type checked wrappers add_patch_entry or add_patch_marker */
static void
add_patch_entry_internal(patch_list_t *patch, instr_t *instr, ushort patch_flags,
                         short instruction_offset,
                         ptr_uint_t value_location_offset)
{
    uint i = patch->num_relocations;

    ASSERT(patch->num_relocations < MAX_PATCH_ENTRIES);
    /* Since in debug build we have the extra slots for stats, it's important
     * to provide a useful release build message
     */
    if (patch->num_relocations >= MAX_PATCH_ENTRIES) {
        SYSLOG_CUSTOM_NOTIFY(SYSLOG_CRITICAL, MSG_EXCEPTION, 4,
                             "Maximum patch entries exceeded",
                             get_application_name(), get_application_pid(),
                             "<maxpatch>", "Maximum patch entries exceeded");
        os_terminate(get_thread_private_dcontext(), TERMINATE_PROCESS);
        ASSERT_NOT_REACHED();
    }

    LOG(THREAD_GET, LOG_EMIT, 4,
        "add_patch_entry[%d] value_location_offset="PFX"\n", i,
        value_location_offset);

    patch->entry[i].where.instr = instr;
    patch->entry[i].patch_flags = patch_flags;
    patch->entry[i].value_location_offset = value_location_offset;
    patch->entry[i].instr_offset = instruction_offset;

    patch->num_relocations++;
}

/* add an instruction to patch list and address of location for future updates */
static inline void
add_patch_entry(patch_list_t *patch, instr_t *instr, ushort patch_flags,
                ptr_uint_t value_location_offset)
{
    add_patch_entry_internal(patch, instr, patch_flags, -4 /* offset of imm32 argument */,
                             value_location_offset);
}

/* add an instruction to patch list to retrieve its offset later.
   Takes an instruction and an offset within the instruction.
   Result: The offset within an encoded instruction stream will
   be stored in target_offset by encode_with_patch_list
*/
void
add_patch_marker(patch_list_t *patch, instr_t *instr, ushort patch_flags,
                 short instr_offset, ptr_uint_t *target_offset /* OUT */)
{
    add_patch_entry_internal(patch, instr, (ushort) (patch_flags | PATCH_MARKER),
                             instr_offset, (ptr_uint_t) target_offset);
}

/* remove PATCH_MARKER entries since not needed for dynamic updates */
static INLINE_ONCE void
remove_assembled_patch_markers(dcontext_t *dcontext, patch_list_t *patch)
{
    ushort i=0, j=0;

    /* we can remove the PATCH_MARKER entries after encoding,
       and so patch_emitted_code won't even need to check for PATCH_MARKER
    */

    while (j < patch->num_relocations) {
        if (TEST(PATCH_MARKER, patch->entry[j].patch_flags)) {
            LOG(THREAD, LOG_EMIT, 4,
                "remove_assembled_patch_markers: removing marker %d\n", j);
        } else {
            patch->entry[i] = patch->entry[j];
            i++;
        }

        j++;
    }

    LOG(THREAD, LOG_EMIT, 3, "remove_assembled_patch_markers: relocations %d, left only %d\n",
        patch->num_relocations, i);
    patch->num_relocations = i;
}


/* Indirect all instructions instead of later patching */
static void
relocate_patch_list(dcontext_t *dcontext, patch_list_t *patch,
                    instrlist_t *ilist)
{
    instr_t *inst;
    uint cur = 0;
    LOG(THREAD, LOG_EMIT, 3, "relocate_patch_list ["PFX"]\n", patch);

    /* go through the instructions and "relocate" by indirectly using XDI */
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        if (cur < patch->num_relocations &&
            inst == patch->entry[cur].where.instr) {
            ASSERT(!TEST(PATCH_OFFSET_VALID, patch->entry[cur].patch_flags));

            if (!TEST(PATCH_MARKER, patch->entry[cur].patch_flags)) {
                opnd_t opnd;
                ASSERT(instr_num_srcs(inst) > 0);
                opnd = instr_get_src(inst, 0);

                DOLOG(4, LOG_EMIT, {
                    LOG(THREAD, LOG_EMIT, 2,
                        "encode_with_patch_list: patch_entry_t[%d] before update \n");
                    instr_disassemble(dcontext, inst, THREAD);
                    LOG(THREAD, LOG_EMIT, 2, "\n");
                });
                /* we assume that per_thread_t will be in XDI,
                   and the displacement is in value_location_offset */
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int
                              (patch->entry[cur].value_location_offset)));
                if (opnd_is_near_base_disp(opnd)) {
                    /* indirect through XDI and update displacement */
                    opnd_set_disp(&opnd, (int) patch->entry[cur].value_location_offset);
                    opnd_replace_reg(&opnd, REG_NULL, REG_XDI);
                } else if (opnd_is_immed_int(opnd)) {
                    /* indirect through XDI and set displacement */
                    /* converting AND $0x00003fff, %xcx -> %xcx
                       into       AND  mask(%xdi), %xcx -> %xcx
                    */
                    opnd = opnd_create_base_disp
                        (REG_XDI, REG_NULL, 0,
                         (int) patch->entry[cur].value_location_offset, OPSZ_4);
                }

                instr_set_src(inst, 0, opnd);
                DOLOG(3, LOG_EMIT, {
                    LOG(THREAD, LOG_EMIT, 2,
                        "encode_with_patch_list: patch_entry_t[%d] after update \n");
                    instr_disassemble(dcontext, inst, THREAD);
                    LOG(THREAD, LOG_EMIT, 2, "\n");
                });
            }
            cur++;
        }
    }
}

/* Updates patch list with offsets in assembled instruction list */
/* Cf: instrlist_encode which does not support a patch list */
/* Returns length of emitted code */
int
encode_with_patch_list(dcontext_t *dcontext, patch_list_t *patch,
                       instrlist_t *ilist, cache_pc start_pc)
{
    instr_t *inst;
    uint len;
    uint cur;
    cache_pc pc = start_pc;

    ASSERT(patch->num_relocations < MAX_PATCH_ENTRIES);

    if (patch->type == PATCH_TYPE_INDIRECT_XDI) {
        relocate_patch_list(dcontext, patch, ilist);
    }

    /* now encode the instructions */
    /* must set note fields first with offset */
    len = 0;
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        instr_set_note(inst, (void *)(ptr_uint_t)len);
        len += instr_length(dcontext, inst);
    }

    cur = 0;
    /* after instruction list is assembled we collect the offsets */
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        short offset_in_instr = patch->entry[cur].instr_offset;
        byte *nxt_pc = instr_encode(dcontext, inst, pc);
        ASSERT(nxt_pc != NULL);
        len = (int) (nxt_pc - pc);
        pc = nxt_pc;

        if (cur < patch->num_relocations &&
            inst == patch->entry[cur].where.instr) {
            ASSERT(!TEST(PATCH_OFFSET_VALID, patch->entry[cur].patch_flags));

            /* support positive offsets from beginning and negative - from end of instruction */
            if (offset_in_instr < 0) {
                /* grab offset offset_in_instr bytes from the end of instruction */
                /* most commonly -4 for a 32bit immediate  */
                patch->entry[cur].where.offset =
                    ((pc + offset_in_instr) - start_pc);
            } else {
                /* grab offset after skipping offset_in_instr from beginning of instruction */
                patch->entry[cur].where.offset =
                    ((pc - len + offset_in_instr) - start_pc);
            }
            patch->entry[cur].patch_flags |= PATCH_OFFSET_VALID;

            LOG(THREAD, LOG_EMIT, 4,
                "encode_with_patch_list: patch_entry_t[%d] offset="PFX"\n",
                cur, patch->entry[cur].where.offset);

            if (TEST(PATCH_MARKER, patch->entry[cur].patch_flags)) {
                /* treat value_location_offset as an output argument
                   and store there the computed offset,
                */
                ptr_uint_t *output_value = (ptr_uint_t *)
                    patch->entry[cur].value_location_offset;
                ptr_uint_t output_offset = patch->entry[cur].where.offset;
                if (TEST(PATCH_ASSEMBLE_ABSOLUTE, patch->entry[cur].patch_flags)) {
                    ASSERT(!TEST(PATCH_UINT_SIZED, patch->entry[cur].patch_flags));
                    output_offset += (ptr_uint_t)start_pc;
                }
                if (TEST(PATCH_UINT_SIZED, patch->entry[cur].patch_flags)) {
                    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(output_offset)));
                    *((uint *)output_value) = (uint) output_offset;
                } else
                    *output_value = output_offset;
            }

            LOG(THREAD, LOG_EMIT, 4,
                "encode_with_patch_list [%d] extras patch_flags=0x%x value_offset="
                PFX"\n", cur, patch->entry[cur].patch_flags,
                patch->entry[cur].value_location_offset);
            cur++;
        }
    }

    /* assuming patchlist is in the same order as ilist, we should have seen all */
    LOG(THREAD, LOG_EMIT, 4, "cur %d, num %d", cur, patch->num_relocations);
    ASSERT(cur == patch->num_relocations);

    remove_assembled_patch_markers(dcontext, patch);
    ASSERT(CHECK_TRUNCATE_TYPE_int(pc - start_pc));
    return (int)(pc - start_pc);
}

#ifdef DEBUG
void
print_patch_list(patch_list_t *patch)
{
    uint i;
    LOG(THREAD_GET, LOG_EMIT, 4, "patch="PFX" num_relocations=%d\n",
        patch, patch->num_relocations);

    for(i=0; i<patch->num_relocations; i++) {
        ASSERT(TEST(PATCH_OFFSET_VALID, patch->entry[i].patch_flags));
        LOG(THREAD_GET, LOG_EMIT, 4,
            "patch_list [%d] offset="PFX" patch_flags=%d value_offset="PFX"\n", i,
            patch->entry[i].where.offset,
            patch->entry[i].patch_flags,
            patch->entry[i].value_location_offset);
    }
}

# ifdef INTERNAL
/* disassembles code adding patch list labels */
static void
disassemble_with_annotations(dcontext_t *dcontext, patch_list_t *patch,
                             byte *start_pc, byte *end_pc)
{
    byte *pc = start_pc;
    uint cur = 0;

    do {
        if (cur < patch->num_relocations &&
            pc >= start_pc + patch->entry[cur].where.offset) {
            ASSERT(TEST(PATCH_OFFSET_VALID, patch->entry[cur].patch_flags));
            /* this is slightly off - we'll mark next instruction,
               but is good enough for this purpose */
            LOG(THREAD, LOG_EMIT, 2, "%d:", cur);
            cur++;
        } else {
            LOG(THREAD, LOG_EMIT, 2, "  ");
        }

        pc = disassemble_with_bytes(dcontext, pc, THREAD);
    } while (pc < end_pc);
    LOG(THREAD, LOG_EMIT, 2, "\n");
}
# endif
#endif

/* updates emitted code according to patch list */
static void
patch_emitted_code(dcontext_t *dcontext, patch_list_t *patch, byte *start_pc)
{
    uint i;
    /* FIXME: can get this as a patch list entry through indirection */
    per_thread_t *pt = (per_thread_t *) dcontext->fragment_field;
    ASSERT(dcontext != GLOBAL_DCONTEXT && dcontext != NULL);

    LOG(THREAD, LOG_EMIT, 2, "patch_emitted_code start_pc="PFX" pt="PFX"\n",
        start_pc);
    if (patch->type != PATCH_TYPE_ABSOLUTE) {
        LOG(THREAD, LOG_EMIT, 2,
            "patch_emitted_code type=%d indirected, nothing to patch\n", patch->type);
        /* FIXME: propagate the check earlier to save the extraneous calls
           to update_indirect_exit_stub and update_indirect_branch_lookup
        */
        return;
    }
    DOLOG(4, LOG_EMIT, {
        print_patch_list(patch);
    });
    for(i=0; i<patch->num_relocations; i++) {
        byte *pc = start_pc + patch->entry[i].where.offset;
        /* value address, (think for example of pt->trace.hash_mask) */
        ptr_uint_t value;
        char *vaddr = NULL;
        if (TEST(PATCH_PER_THREAD, patch->entry[i].patch_flags)) {
            vaddr = (char *)pt + patch->entry[i].value_location_offset;
        } else if (TEST(PATCH_UNPROT_STAT, patch->entry[i].patch_flags)) {
            /* separate the two parts of the stat */
            uint unprot_offs = (uint) (patch->entry[i].value_location_offset) >> 16;
            uint field_offs = (uint) (patch->entry[i].value_location_offset) & 0xffff;
            IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                          (patch->entry[i].value_location_offset)));
            vaddr = (*((char **)((char *)pt + unprot_offs))) + field_offs;
            LOG(THREAD, LOG_EMIT, 4,
                "patch_emitted_code [%d] value "PFX" => 0x%x 0x%x => "PFX"\n",
                i, patch->entry[i].value_location_offset, unprot_offs, field_offs, vaddr);
        }
        else
            ASSERT_NOT_REACHED();
        ASSERT(TEST(PATCH_OFFSET_VALID, patch->entry[i].patch_flags));
        ASSERT(!TEST(PATCH_MARKER, patch->entry[i].patch_flags));

        if (!TEST(PATCH_TAKE_ADDRESS, patch->entry[i].patch_flags)) {
            /* use value pointed by computed address */
            if (TEST(PATCH_UINT_SIZED, patch->entry[i].patch_flags))
                value = (ptr_uint_t) *((uint *)vaddr);
            else
                value = *(ptr_uint_t*)vaddr;
        } else {
            ASSERT(!TEST(PATCH_UINT_SIZED, patch->entry[i].patch_flags));
            value = (ptr_uint_t)vaddr;   /* use computed address */
        }

        LOG(THREAD, LOG_EMIT, 4,
            "patch_emitted_code [%d] offset="PFX" patch_flags=%d value_offset="PFX
            " vaddr="PFX" value="PFX"\n", i,
            patch->entry[i].where.offset, patch->entry[i].patch_flags,
            patch->entry[i].value_location_offset, vaddr, value);
        if (TEST(PATCH_UINT_SIZED, patch->entry[i].patch_flags)) {
            IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(value)));
            *((uint*)pc) = (uint) value;
        } else
            *((ptr_uint_t *)pc) = value;
        LOG(THREAD, LOG_EMIT, 4,
            "patch_emitted_code: updated pc *"PFX" = "PFX"\n", pc, value);
    }

    STATS_INC(emit_patched_fragments);
    DOSTATS({
        /* PR 217008: avoid gcc warning from truncation assert in XSTATS_ADD_DC */
        int tmp_num = patch->num_relocations;
        STATS_ADD(emit_patched_relocations, tmp_num);
    });
    LOG(THREAD, LOG_EMIT, 4, "patch_emitted_code done\n");
}

#define GET_IB_FTABLE(ibl_code,target_trace_table,field)                   \
  (GET_IBL_TARGET_TABLE((ibl_code)->branch_type, (target_trace_table))     \
   + offsetof(ibl_table_t, field))

/* Updates an indirect branch exit stub with the latest hashtable mask
 * and hashtable address
 * See also update_indirect_branch_lookup
 */
void
update_indirect_exit_stub(dcontext_t *dcontext, fragment_t *f, linkstub_t *l)
{
    generated_code_t *code = get_emitted_routines_code
        (dcontext _IF_X64(FRAGMENT_GENCODE_MODE(f->flags)));
# ifdef CUSTOM_EXIT_STUBS
    byte *start_pc = (byte *) EXIT_FIXED_STUB_PC(dcontext, f, l);
# else
    byte *start_pc = (byte *) EXIT_STUB_PC(dcontext, f, l);
# endif
    ibl_branch_type_t branch_type;

    ASSERT(linkstub_owned_by_fragment(dcontext, f, l));
    ASSERT(LINKSTUB_INDIRECT(l->flags));
    ASSERT(EXIT_HAS_STUB(l->flags, f->flags));
    /* Shared use indirection so no patching needed -- caller should check */
    ASSERT(!TEST(FRAG_SHARED, f->flags));
#ifdef WINDOWS
    /* Do not touch shared_syscall */
    if (EXIT_TARGET_TAG(dcontext, f, l) ==
        shared_syscall_routine_ex(dcontext _IF_X64(FRAGMENT_GENCODE_MODE(f->flags))))
        return;
#endif
    branch_type = extract_branchtype(l->flags);

    LOG(THREAD, LOG_EMIT, 4, "update_indirect_exit_stub: f->tag="PFX"\n",
        f->tag);

    if (DYNAMO_OPTION(disable_traces) && !code->bb_ibl[branch_type].ibl_head_is_inlined) {
        return;
    }

    if (TEST(FRAG_IS_TRACE, f->flags)) {
        ASSERT(code->trace_ibl[branch_type].ibl_head_is_inlined);
        patch_emitted_code(dcontext, &code->trace_ibl[branch_type].ibl_stub_patch, start_pc);
    } else {
        ASSERT(code->bb_ibl[branch_type].ibl_head_is_inlined);
        patch_emitted_code(dcontext, &code->bb_ibl[branch_type].ibl_stub_patch, start_pc);
    }
}

/*###########################################################################
 *
 * fragment_t Prefixes
 *
 * Two types: indirect branch target, which restores eflags and xcx, and
 * normal prefix, which just restores xcx
 */

/* Indirect Branch Target Prefix
 * We have 3 different prefixes: one if we don't need to restore eflags, one
 * if we need to restore just using sahf, and one if we also need to restore
 * the overflow flag OF.
 *
 * FIXME: currently we cache-align the prefix, not the normal
 * entry point...if prefix gets much longer, might want to add
 * nops to get normal entry cache-aligned?
 */

/* for now all ibl targets must use same scratch locations: tls or not, no mixture */
/* PR 244737: x64 always uses tls even if all-private */
#define IBL_EFLAGS_IN_TLS() (IF_X64_ELSE(true, SHARED_IB_TARGETS()))

#define RESTORE_XAX_PREFIX(flags) \
    ((FRAG_IS_X86_TO_X64(flags) && \
      IF_X64_ELSE(DYNAMO_OPTION(x86_to_x64_ibl_opt), false)) ? \
     SIZE64_MOV_R8_TO_XAX : \
     (IBL_EFLAGS_IN_TLS() ? SIZE_MOV_XAX_TO_TLS(flags, false) : SIZE32_MOV_XAX_TO_ABS))
#define PREFIX_BASE(flags) \
    (RESTORE_XAX_PREFIX(flags) + FRAGMENT_BASE_PREFIX_SIZE(flags))

enum {PREFIX_SIZE_RESTORE_OF = 2, /* add $0x7f, %al  */
      PREFIX_SIZE_FIVE_EFLAGS = 1,    /* SAHF */
};

/* use indirect branch target prefix? */
static inline bool
use_ibt_prefix(uint flags)
{
    /* when no traces, all bbs use IBT prefix */
    /* FIXME: currently to allow bb2bb we simply have a prefix on all BB's
     * should experiment with a shorter prefix for targetting BBs
     * by restoring the flags in the IBL routine,
     * or even jump through memory to avoid having the register restore prefix
     * Alternatively, we can reemit a fragment only once it is known to be an IBL target,
     * assuming the majority will be reached with an IB when they are first built.
     * (Simplest counterexample is of a return from a function with no arguments
     * called within a conditional, but the cache compaction of not having
     * prefixes on all bb's may offset this double emit).
     * All of these are covered by case 147.
     */
    return (IS_IBL_TARGET(flags) &&
            /* coarse bbs (and fine in presence of coarse) do not support prefixes */
            !(DYNAMO_OPTION(coarse_units) &&
              !TEST(FRAG_IS_TRACE, flags) &&
              DYNAMO_OPTION(bb_ibl_targets)));
}

static inline bool
ibl_use_target_prefix(ibl_code_t *ibl_code)
{
   return !(DYNAMO_OPTION(coarse_units) &&
            /* If coarse units are enabled we need to have no prefix
             * for both fine and coarse bbs
             */
            ((ibl_code->source_fragment_type == IBL_COARSE_SHARED &&
              DYNAMO_OPTION(bb_ibl_targets)) ||
             (IS_IBL_BB(ibl_code->source_fragment_type) &&
              /* FIXME case 147/9636: if -coarse_units -bb_ibl_targets
               * but traces are enabled, we won't put prefixes on regular
               * bbs but will assume we have them here!  We don't support
               * that combination yet.  When we do this routine should return
               * another bit of info: whether to do two separate lookups.
               */
              DYNAMO_OPTION(disable_traces) && DYNAMO_OPTION(bb_ibl_targets))));
}

int
fragment_prefix_size(uint flags)
{
    if (use_ibt_prefix(flags)) {
        bool use_eflags_restore = TEST(FRAG_IS_TRACE, flags) ?
            !DYNAMO_OPTION(trace_single_restore_prefix) :
            !DYNAMO_OPTION(bb_single_restore_prefix);
        /* The common case is !INTERNAL_OPTION(unsafe_ignore_eflags*) so
         * PREFIX_BASE(flags) is defined accordingly, and we subtract from it to
         * get the correct value when the option is on.
         */
        if (INTERNAL_OPTION(unsafe_ignore_eflags_prefix)) {
            if (INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
                ASSERT(PREFIX_BASE(flags) - RESTORE_XAX_PREFIX(flags) >= 0);
                return PREFIX_BASE(flags) - RESTORE_XAX_PREFIX(flags);
            } else {
                /* still need to restore xax, just don't restore eflags */
                return PREFIX_BASE(flags);
            }
        }
        if (!use_eflags_restore)
            return PREFIX_BASE(flags) - RESTORE_XAX_PREFIX(flags);
        if (TEST(FRAG_WRITES_EFLAGS_6, flags)) /* no flag restoration needed */
            return PREFIX_BASE(flags);
        else if (TEST(FRAG_WRITES_EFLAGS_OF, flags)) /* no OF restoration needed */
            return (PREFIX_BASE(flags) + PREFIX_SIZE_FIVE_EFLAGS);
        else /* must restore all 6 flags */
            if (INTERNAL_OPTION(unsafe_ignore_overflow)) {
                /* do not restore OF */
                return (PREFIX_BASE(flags) + PREFIX_SIZE_FIVE_EFLAGS);
            } else {
                return (PREFIX_BASE(flags) + PREFIX_SIZE_RESTORE_OF +
                        PREFIX_SIZE_FIVE_EFLAGS);
            }
    } else {
#ifdef CLIENT_INTERFACE
        if (dynamo_options.bb_prefixes)
            return FRAGMENT_BASE_PREFIX_SIZE(flags);
        else
#endif
            return 0;
    }
}

/* See SAVE_TO_DC_OR_TLS() in mangle.c for the save xcx code */
static cache_pc
insert_restore_xcx(dcontext_t *dcontext, cache_pc pc, uint flags, bool require_addr16)
{
    /* shared fragment prefixes use tls, private use mcontext
     * this works b/c the shared ibl copies the app xcx to both places!
     * private_ib_in_tls option makes all prefixes use tls
     */
    return insert_spill_or_restore(dcontext, pc, flags, false/*restore*/,
                                   XCX_IN_TLS(flags), REG_XCX,
                                   MANGLE_XCX_SPILL_SLOT, XCX_OFFSET,
                                   require_addr16);
}

static cache_pc
insert_restore_register(dcontext_t *dcontext, fragment_t *f, cache_pc pc, reg_id_t reg)
{
    ASSERT(reg == REG_XAX || reg == REG_XCX);
#ifdef X64
    if (FRAG_IS_X86_TO_X64(f->flags) && DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
        /* in x86_to_x64 mode, rax was spilled to r8 and rcx was spilled to r9
         * to restore rax:  49 8b c0   mov %r8 -> %rax
         * to restore rcx:  49 8b c9   mov %r9 -> %rcx
         */
        *pc = REX_PREFIX_BASE_OPCODE | REX_PREFIX_W_OPFLAG | REX_PREFIX_B_OPFLAG; pc++;
        *pc = MOV_MEM2REG_OPCODE; pc++;
        *pc = MODRM_BYTE(3/*mod*/, reg_get_bits(reg),
                         reg_get_bits((reg == REG_XAX) ? REG_R8 : REG_R9));
        pc++;
    } else {
#endif
        pc = (reg == REG_XAX) ? insert_restore_xax(dcontext, pc, f->flags,
                                                   IBL_EFLAGS_IN_TLS(),
                                                   PREFIX_XAX_SPILL_SLOT, false)
                              : insert_restore_xcx(dcontext, pc, f->flags, false);
#ifdef X64
    }
#endif
    return pc;
}

void
insert_fragment_prefix(dcontext_t *dcontext, fragment_t *f)
{
    byte *pc = (byte *) f->start_pc;
    bool insert_eflags_xax_restore = TEST(FRAG_IS_TRACE, f->flags) ?
        !DYNAMO_OPTION(trace_single_restore_prefix) :
        !DYNAMO_OPTION(bb_single_restore_prefix);
    ASSERT(f->prefix_size == 0); /* shouldn't be any prefixes yet */
    if (use_ibt_prefix(f->flags)) {
        if ((!INTERNAL_OPTION(unsafe_ignore_eflags_prefix) ||
             !INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) &&
            insert_eflags_xax_restore) {
            if (!INTERNAL_OPTION(unsafe_ignore_eflags_prefix) &&
                !TEST(FRAG_WRITES_EFLAGS_6, f->flags)) {
                if (!TEST(FRAG_WRITES_EFLAGS_OF, f->flags)
                    && !INTERNAL_OPTION(unsafe_ignore_overflow)) {
                    DEBUG_DECLARE(byte *restore_of_prefix_pc = pc;)
                    /* must restore OF
                     * we did a seto on %al, so we restore OF by adding 0x7f to
                     * %al (7f not ff b/c add only sets OF for signed operands,
                     * sets CF for uint)
                     */
                    STATS_INC(num_oflag_prefix_restore);

                    /* 04 7f   add $0x7f,%al */
                    *pc = ADD_AL_OPCODE; pc++;
                    *pc = 0x7f; pc++;

                    ASSERT(pc - restore_of_prefix_pc == PREFIX_SIZE_RESTORE_OF);
                }

                /* restore other 5 flags w/ sahf */
                *pc = SAHF_OPCODE; pc++;
                ASSERT(PREFIX_SIZE_FIVE_EFLAGS == 1);
            }
            /* restore xax */
            pc = insert_restore_register(dcontext, f, pc, REG_XAX);
        }

        pc = insert_restore_register(dcontext, f, pc, REG_XCX);

        /* set normal entry point to be next pc */
        ASSERT_TRUNCATE(f->prefix_size, byte, ((cache_pc) pc) - f->start_pc);
        f->prefix_size = (byte)(((cache_pc) pc) - f->start_pc);
    } else {
#ifdef CLIENT_INTERFACE
        if (dynamo_options.bb_prefixes) {
            pc = insert_restore_register(dcontext, f, pc, REG_XCX);

            /* set normal entry point to be next pc */
            ASSERT_TRUNCATE(f->prefix_size, byte, ((cache_pc) pc) - f->start_pc);
            f->prefix_size = (byte)(((cache_pc) pc) - f->start_pc);
        } /* else, no prefix */
#endif
    }
    /* make sure emitted size matches size we requested */
    ASSERT(f->prefix_size == fragment_prefix_size(f->flags));
}

#ifdef PROFILE_RDTSC
/***************************************************************************
 ***************************************************************************
 ** PROFILING USING RDTSC
 **
 **/
/*
We want the profile code to not count towards fragment times.
So we stop time as quickly as possible, in assembly here instead of
in the profile_fragment_enter function, and start time again as late
as possible:
    mov %eax, eax_offset(dcontext) # save eax
    mov %edx, edx_offset(dcontext) # save edx
    rdtsc                          # stop time
    switch to dynamo stack
    pushfl                         # save eflags (call will clobber)
    mov %ecx, ecx_offset(dcontext) # save ecx
    pushl %edx                     # pass time as arg
    pushl %eax
    pushil &fragment_address       # pass &frag as arg
    call profile_fragment_enter    #
    addl $0xc, %esp                # clean up args
    popl %ecx                      # restore ecx
    popfl                          # restore eflags
    restore app stack
    rdtsc                          # start time
    movl %eax, start_time_OFFS(dcontext)   # store time value
    movl %edx, 4+start_time_OFFS(dcontext) # store time value
    mov eax_offset(dcontext), %eax   # restore eax
    mov edx_offset(dcontext), %edx   # restore edx
    mov ecx_offset(dcontext), %ecx   # restore ecx
*/

static uint profile_call_length = 0;
static int profile_call_fragment_offset = 0;
static int profile_call_call_offset = 0;
static byte profile_call_buf[128];
static dcontext_t *buffer_dcontext;
static void build_profile_call_buffer(void);

uint
profile_call_size()
{
    if (profile_call_length == 0)
        build_profile_call_buffer();
    return profile_call_length;
}

/* if insert_profile_call emits its code into the trace buffer, this
 * routine must be called once the fragment is created and the code is
 * in the fcache
 */
void
finalize_profile_call(dcontext_t *dcontext, fragment_t *f)
{
    byte *start_pc = (byte *) FCACHE_ENTRY_PC(f);
    byte *pc;
    byte *prev_pc;
    instr_t instr;
    instr_init(dcontext, &instr);

    /* fill in address of owning fragment now that that fragment exists */
    pc = start_pc + profile_call_fragment_offset;
    /* PR 248210: unsupported feature on x64 */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    *((int *)pc) = (uint)f;

    /* fill in call's proper pc-relative offset now that code is
     * in its final location in fcache
     */
    pc = start_pc + profile_call_call_offset;
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    *((int *)pc) = (int)&profile_fragment_enter - (int)pc - 4;

    /* must fix up all dcontext references to point to the right dcontext */
    pc = start_pc;
    do {
        prev_pc = pc;
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        ASSERT(instr_valid(&instr)); /* our own code! */
        /* look for loads and stores that reference buffer_dcontext */
        if (instr_get_opcode(&instr) == OP_mov_ld &&
            opnd_is_near_base_disp(instr_get_src(&instr, 0)) &&
            opnd_get_base(instr_get_src(&instr, 0)) == REG_NULL &&
            opnd_get_index(instr_get_src(&instr, 0)) == REG_NULL) {
            /* if not really dcontext value, update_ will return old value */
            instr_set_src(&instr, 0,
                          update_dcontext_address(instr_get_src(&instr, 0),
                                                  buffer_dcontext, dcontext));
        }
        else if (instr_get_opcode(&instr) == OP_mov_st &&
                 opnd_is_near_base_disp(instr_get_dst(&instr, 0)) &&
                 opnd_get_base(instr_get_dst(&instr, 0)) == REG_NULL &&
                 opnd_get_index(instr_get_dst(&instr, 0)) == REG_NULL) {
            /* if not really dcontext value, update_ will return old value */
            instr_set_dst(&instr, 0,
                          update_dcontext_address(instr_get_dst(&instr, 0),
                                                  buffer_dcontext, dcontext));
        }
        if (!instr_raw_bits_valid(&instr)) {
            DEBUG_DECLARE(byte *nxt_pc;)
            DEBUG_DECLARE(nxt_pc = ) instr_encode(dcontext, &instr, prev_pc);
            ASSERT(nxt_pc != NULL);
        }
    } while (pc < start_pc + profile_call_length);
    instr_free(dcontext, &instr);
}


void
insert_profile_call(cache_pc start_pc)
{
    if (profile_call_length == 0)
        build_profile_call_buffer();
    memcpy((void *)start_pc, profile_call_buf, profile_call_length);
    /* if thread-private, we change to proper dcontext when finalizing */
}


/* This routine builds the profile call code using the instr_t
 * abstraction, then emits it into a buffer to be saved.
 * The code can then be directly copied whenever needed.
 * Assumption: this thread's dcontext must have been created
 * before calling this function.
 */
static void
build_profile_call_buffer()
{
    byte *pc, *nxt_pc;
    instrlist_t ilist;
    instr_t *inst;
    int start_time_offs;
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    /* remember dcontext for easy replacement when finalizing: */
    buffer_dcontext = dcontext;

    /* we require a dcontext to find this offset because it may
     * or may not be pushed to a quadword boundary, making it
     * hard to hardcode it
     */
    start_time_offs = (int)(&(dcontext->start_time)) - (int)dcontext;

    /* initialize the ilist */
    instrlist_init(&ilist);

    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EDX, XDX_OFFSET));

    /* get time = rdtsc */
    APP(&ilist, INSTR_CREATE_rdtsc(dcontext));

    /* swap to dstack */
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_ESP, XSP_OFFSET));
    APP(&ilist, instr_create_restore_dynamo_stack(dcontext));

    /* finish saving caller-saved registers
     * The profile_fragment_enter function will save the callee-saved
     * regs (ebx, ebp, esi, edi) and will restore ebp and esp, but we need
     * to explicitly save eax, ecx, and edx
     */
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_ECX, XCX_OFFSET));

    /* save eflags (call will clobber) */
    APP(&ilist, INSTR_CREATE_RAW_pushf(dcontext));

#ifdef WINDOWS
    /* must preserve the LastErrorCode (if the profile procedure
     * calls a Win32 API routine it could overwrite the app's error code)
     * currently this is done in the profile routine itself --
     * if you want to move it here, look at the code in profile.c
     */
#endif

    /* push time as 2nd argument for call */
    APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_EDX)));
    APP(&ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_EAX)));

    /* push fragment address as 1st argument for call
     * fragment isn't built yet, we fill it in in finalize_profile_call
     */
    APP(&ilist, INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0)));

    /* call near rel: 4-byte pc-relative offset from start of next instr
     * we don't have that offset now so we fill it in later (in
     * finalize_profile_call)
     */
    APP(&ilist, INSTR_CREATE_call(dcontext, opnd_create_pc(NULL)));

    /* pop arguments: addl $0xc, %esp */
    APP(&ilist,
        INSTR_CREATE_add(dcontext, opnd_create_reg(REG_ESP), OPND_CREATE_INT8(0xc)));

    /* restore eflags */
    APP(&ilist, INSTR_CREATE_RAW_popf(dcontext));

    /* restore caller-saved registers */
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_ECX, XCX_OFFSET));

    /* restore app stack */
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_ESP, XSP_OFFSET));

    /* get start time = rdtsc */
    APP(&ilist, INSTR_CREATE_rdtsc(dcontext));

    /* copy start time into dcontext */
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EAX, start_time_offs));
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EDX, start_time_offs+4));

    /* finish restoring caller-saved registers */
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_EDX, XDX_OFFSET));
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));

    /* now encode the instructions */
    pc = profile_call_buf;
    for (inst = instrlist_first(&ilist); inst; inst = instr_get_next(inst)) {
        if (instr_is_call_direct(inst)) {
            /* push_immed was just before us, so fragment address
             * starts 4 bytes before us:
             */
            profile_call_fragment_offset = (int) (pc - 4 - profile_call_buf);
            /* call opcode is 1 byte, offset is next: */
            profile_call_call_offset = (int) (pc + 1 - profile_call_buf);
        }
        /* we have no jumps with instr_t targets so we don't need to set note
         * field in order to use instr_encode
         */
        nxt_pc = instr_encode(dcontext, inst, (void*)pc);
        ASSERT(nxt_pc != NULL);
        profile_call_length += nxt_pc - pc;
        pc = nxt_pc;
        ASSERT(profile_call_length < 128);
    }

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
}

#endif /* PROFILE_RDTSC */

#ifdef WINDOWS
# ifdef CLIENT_INTERFACE

/* Leaving in place old notes on LastError preservation: */
    /* inlined versions of save/restore last error by reading of TIB */
    /* If our inlined version fails on a later version of windows
       should verify [GS]etLastError matches the disassembly below.
    */
    /* Win2000: kernel32!SetLastError: */
    /*   77E87671: 55                 push        ebp */
    /*   77E87672: 8B EC              mov         ebp,esp */
    /*   77E87674: 64 A1 18 00 00 00  mov         eax,fs:[00000018] */
    /*   77E8767A: 8B 4D 08           mov         ecx,dword ptr [ebp+8] */
    /*   77E8767D: 89 48 34           mov         dword ptr [eax+34h],ecx */
    /*   77E87680: 5D                 pop         ebp */
    /*   77E87681: C2 04 00           ret         4 */

    /* Win2003: ntdll!RtlSetLastWin32Error: optimized to */
    /*   77F45BB4: 64 A1 18 00 00 00  mov         eax,fs:[00000018] */
    /*   77F45BBA: 8B 4C 24 04        mov         ecx,dword ptr [esp+4] */
    /*   77F45BBE: 89 48 34           mov         dword ptr [eax+34h],ecx */
    /*   77F45BC1: C2 04 00           ret         4 */

    /* See InsideWin2k, p. 329 SelfAddr fs:[18h] simply has the linear address of the TIB
       while we're interested only in LastError which is at fs:[34h] */
    /* Therefore all we need is a single instruction! */
    /* 64 a1 34 00 00 00  mov         dword ptr fs:[34h],errno_register */
    /* Overall savings: 7 instructions, 5 data words */

    /*kernel32!GetLastError:*/
    /*   77E87684: 64 A1 18 00 00 00  mov         eax,fs:[00000018] */
    /*   77E8768A: 8B 40 34           mov         eax,dword ptr [eax+34h] */
    /*   77E8768D: C3                 ret */

    /* All we need is a single instruction: */
    /*  77F45BBE: 89 48 34           mov         reg_result, dword ptr fs:[34h] */

/* i#249: isolate app's PEB+TEB by keeping our own copy and swapping on cxt switch
 * For clean calls we share this in clean_call_{save,restore} (i#171, i#1349).
 */
void
preinsert_swap_peb(dcontext_t *dcontext, instrlist_t *ilist, instr_t *next,
                   bool absolute, reg_id_t reg_dr, reg_id_t reg_scratch, bool to_priv)
{
    /* We assume PEB is globally constant and we don't need per-thread pointers
     * and can use use absolute pointers known at init time
     */
    PEB *tgt_peb = to_priv ? get_private_peb() : get_own_peb();
    reg_id_t scratch32 = IF_X64_ELSE(reg_64_to_32(reg_scratch), reg_scratch);
    ASSERT(INTERNAL_OPTION(private_peb) && should_swap_peb_pointer());
    ASSERT(reg_dr != REG_NULL && reg_scratch != REG_NULL);
    /* can't store 64-bit immed, so we use scratch reg, for 32-bit too since
     * long 32-bit-immed-store instr to fs:offs is slow to decode
     */
    PRE(ilist, next, INSTR_CREATE_mov_imm
        (dcontext, opnd_create_reg(reg_scratch), OPND_CREATE_INTPTR((ptr_int_t)tgt_peb)));
    PRE(ilist, next, INSTR_CREATE_mov_st
        (dcontext, opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, PEB_TIB_OFFSET, OPSZ_PTR),
         opnd_create_reg(reg_scratch)));

    /* Preserve app's TEB->LastErrorValue.  We used to do this separately b/c
     * DR at one point long ago made some win32 API calls: now we only have to
     * do this when loading private libraries.  We assume no private library
     * code needs to preserve LastErrorCode across app execution.
     */
    if (to_priv) {
        /* yes errno is 32 bits even on x64 */
        PRE(ilist, next, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(scratch32), opnd_create_far_base_disp
             (SEG_TLS, REG_NULL, REG_NULL, 0, ERRNO_TIB_OFFSET, OPSZ_4)));
        PRE(ilist, next, SAVE_TO_DC_VIA_REG
            (absolute, dcontext, reg_dr, scratch32, APP_ERRNO_OFFSET));
    } else {
        PRE(ilist, next, RESTORE_FROM_DC_VIA_REG
            (absolute, dcontext, reg_dr, scratch32, APP_ERRNO_OFFSET));
        PRE(ilist, next, INSTR_CREATE_mov_st
            (dcontext, opnd_create_far_base_disp
             (SEG_TLS, REG_NULL, REG_NULL, 0, ERRNO_TIB_OFFSET, OPSZ_4),
             opnd_create_reg(scratch32)));
    }

#ifdef X64
    /* We have to swap TEB->StackLimit (i#1102).  For now I'm only doing this
     * on X64, though it seems possible for 32-bit stacks to be up high too?
     * We have never seen that.
     */
    if (to_priv) {
        PRE(ilist, next, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(reg_scratch), opnd_create_far_base_disp
             (SEG_TLS, REG_NULL, REG_NULL, 0, BASE_STACK_TIB_OFFSET, OPSZ_PTR)));
        PRE(ilist, next, SAVE_TO_DC_VIA_REG
            (absolute, dcontext, reg_dr, reg_scratch, APP_STACK_LIMIT_OFFSET));
        PRE(ilist, next, RESTORE_FROM_DC_VIA_REG
            (absolute, dcontext, reg_dr, reg_scratch, DSTACK_OFFSET));
        PRE(ilist, next, INSTR_CREATE_lea
            (dcontext, opnd_create_reg(reg_scratch),
             opnd_create_base_disp(reg_scratch, REG_NULL, 0,
                                   -(int)DYNAMORIO_STACK_SIZE, OPSZ_lea)));
        PRE(ilist, next, INSTR_CREATE_mov_st
            (dcontext, opnd_create_far_base_disp
             (SEG_TLS, REG_NULL, REG_NULL, 0, BASE_STACK_TIB_OFFSET, OPSZ_PTR),
             opnd_create_reg(reg_scratch)));
    } else {
        PRE(ilist, next, RESTORE_FROM_DC_VIA_REG
            (absolute, dcontext, reg_dr, reg_scratch, APP_STACK_LIMIT_OFFSET));
        PRE(ilist, next, INSTR_CREATE_mov_st
            (dcontext, opnd_create_far_base_disp
             (SEG_TLS, REG_NULL, REG_NULL, 0, BASE_STACK_TIB_OFFSET, OPSZ_PTR),
             opnd_create_reg(reg_scratch)));
    }
#endif

    /* We also swap TEB->NlsCache.  Unlike TEB->ProcessEnvironmentBlock, which is
     * constant, and TEB->LastErrorCode, which is not peristent, we have to maintain
     * both values and swap between them which is expensive.
     */
    PRE(ilist, next, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(reg_scratch), opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, NLS_CACHE_TIB_OFFSET, OPSZ_PTR)));
    PRE(ilist, next, SAVE_TO_DC_VIA_REG
        (absolute, dcontext, reg_dr, reg_scratch,
         to_priv ? APP_NLS_CACHE_OFFSET : PRIV_NLS_CACHE_OFFSET));
    PRE(ilist, next, RESTORE_FROM_DC_VIA_REG
        (absolute, dcontext, reg_dr, reg_scratch,
         to_priv ? PRIV_NLS_CACHE_OFFSET : APP_NLS_CACHE_OFFSET));
    PRE(ilist, next, INSTR_CREATE_mov_st
        (dcontext, opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, NLS_CACHE_TIB_OFFSET, OPSZ_PTR),
         opnd_create_reg(reg_scratch)));
    /* We also swap TEB->FlsData.  Unlike TEB->ProcessEnvironmentBlock, which is
     * constant, and TEB->LastErrorCode, which is not peristent, we have to maintain
     * both values and swap between them which is expensive.
     */
    PRE(ilist, next, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(reg_scratch), opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, FLS_DATA_TIB_OFFSET, OPSZ_PTR)));
    PRE(ilist, next, SAVE_TO_DC_VIA_REG
        (absolute, dcontext, reg_dr, reg_scratch,
         to_priv ? APP_FLS_OFFSET : PRIV_FLS_OFFSET));
    PRE(ilist, next, RESTORE_FROM_DC_VIA_REG
        (absolute, dcontext, reg_dr, reg_scratch,
         to_priv ? PRIV_FLS_OFFSET : APP_FLS_OFFSET));
    PRE(ilist, next, INSTR_CREATE_mov_st
        (dcontext, opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, FLS_DATA_TIB_OFFSET, OPSZ_PTR),
         opnd_create_reg(reg_scratch)));
    /* We swap TEB->ReservedForNtRpc as well.  Hopefully there won't be many
     * more we'll have to swap.
     */
    PRE(ilist, next, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(reg_scratch), opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, NT_RPC_TIB_OFFSET, OPSZ_PTR)));
    PRE(ilist, next, SAVE_TO_DC_VIA_REG
        (absolute, dcontext, reg_dr, reg_scratch,
         to_priv ? APP_RPC_OFFSET : PRIV_RPC_OFFSET));
    PRE(ilist, next, RESTORE_FROM_DC_VIA_REG
        (absolute, dcontext, reg_dr, reg_scratch,
         to_priv ? PRIV_RPC_OFFSET : APP_RPC_OFFSET));
    PRE(ilist, next, INSTR_CREATE_mov_st
        (dcontext, opnd_create_far_base_disp
         (SEG_TLS, REG_NULL, REG_NULL, 0, NT_RPC_TIB_OFFSET, OPSZ_PTR),
         opnd_create_reg(reg_scratch)));
}
# endif /* CLIENT_INTERFACE */

#endif /* WINDOWS */

/***************************************************************************/
/*             THREAD-PRIVATE/SHARED ROUTINE GENERATION                    */
/***************************************************************************/

/* Export this in instr.h if it becomes useful elsewhere */
#ifdef X86
# ifdef X64
#  ifdef WINDOWS
#   define OPND_ARG1  opnd_create_reg(REG_RCX)
#  else
#   define OPND_ARG1  opnd_create_reg(REG_RDI)
#  endif /* Win/Unix */
# else
#  define OPND_ARG1   OPND_CREATE_MEM32(REG_ESP, 4)
# endif /* 64/32-bit */
#elif defined(ARM)
# define OPND_ARG1    opnd_create_reg(DR_REG_R0)
#endif /* X86/ARM */

/* register for holding dcontext on fcache enter/return */
#define REG_DCTXT SCRATCH_REG5

/* append instructions to setup fcache target
 *   if (!absolute)
 *     # put target somewhere we can be absolute about
 *     RESTORE_FROM_UPCONTEXT next_tag_OFFSET,%xax
 *     if (shared)
 *       mov  %xax,fs:xax_OFFSET
 *     endif
 *   endif
 */
static void
append_setup_fcache_target(dcontext_t *dcontext, instrlist_t *ilist,
                           bool absolute, bool shared)
{
    if (absolute)
        return;

    /* put target into special slot that we can be absolute about */
    APP(ilist, RESTORE_FROM_DC(dcontext, SCRATCH_REG0, NEXT_TAG_OFFSET));
    if (shared) {
        APP(ilist, SAVE_TO_TLS(dcontext, SCRATCH_REG0, FCACHE_ENTER_TARGET_SLOT));
    } else {
#ifdef WINDOWS
        /* absolute into main dcontext (not one in REG_DCTXT) */
        APP(ilist, instr_create_save_to_dcontext(dcontext, SCRATCH_REG0,
                                                 NONSWAPPED_SCRATCH_OFFSET));
#else
        /* no special scratch slot! */
        ASSERT_NOT_IMPLEMENTED(false);
#endif /* !WINDOWS */
    }
}

/* append instructions to jump to target in code cache
 *  ifdef X64 and (target is x86 mode)
 *    # we can't indirect through a register since we couldn't restore
 *    # the high bits (PR 283152)
 *    mov gencode-jmp86-value, fs:xbx_OFFSET
 *    far jmp to next instr, stored w/ 32-bit cs selector in fs:xbx_OFFSET
 *  endif
 *
 *  # jump indirect through dcontext->next_tag, set by dispatch()
 *  if (absolute)
 *    JUMP_VIA_DCONTEXT next_tag_OFFSET
 *  else
 *    if (shared)
 *      jmp *fs:xax_OFFSET
 *    else
 *      JUMP_VIA_DCONTEXT nonswapped_scratch_OFFSET
 *    endif
 *  endif
 */
static void
append_jmp_to_fcache_target(dcontext_t *dcontext, instrlist_t *ilist,
                            generated_code_t *code,
                            bool absolute, bool shared, patch_list_t *patch
                            _IF_X64(byte **jmp86_store_addr)
                            _IF_X64(byte **jmp86_target_addr))
{
#ifdef X86_64
    if (GENCODE_IS_X86(code->gencode_mode)) {
        instr_t *label = INSTR_CREATE_label(dcontext);
        instr_t *store;
        /* We must use an indirect jmp (far direct are illegal in x64) and
         * we can't indirect through a register since we couldn't restore the
         * high bits (PR 283152) so we write the 6-byte far address to TLS.
         */
        /* AMD only supports 32-bit address for far jmp */
        store = INSTR_CREATE_mov_st(dcontext,
                                    OPND_TLS_FIELD_SZ(TLS_XBX_SLOT, OPSZ_4),
                                    OPND_CREATE_INT32(0/*placeholder*/));
        APP(ilist, store);
        APP(ilist, INSTR_CREATE_mov_st(dcontext,
                                       OPND_TLS_FIELD_SZ(TLS_XBX_SLOT+4, OPSZ_2),
                                       OPND_CREATE_INT16((ushort)CS32_SELECTOR)));
        APP(ilist, INSTR_CREATE_jmp_far_ind(dcontext,
                                            OPND_TLS_FIELD_SZ(TLS_XBX_SLOT, OPSZ_6)));
        APP(ilist, label);
        /* We need a patch that involves two instrs, which is not supported,
         * so we get both addresses involved into local vars and do the patch
         * by hand after emitting.
         */
        add_patch_marker(patch, store, PATCH_ASSEMBLE_ABSOLUTE,
                         -4 /* 4 bytes from end */, (ptr_uint_t*)jmp86_store_addr);
        add_patch_marker(patch, label, PATCH_ASSEMBLE_ABSOLUTE,
                         0 /* start of label */, (ptr_uint_t*)jmp86_target_addr);
    }
#endif /* X64 */

    /* Jump indirect through next_tag.  Dispatch set this value with
     * where we want to go next in the fcache_t.
     */
    if (absolute) {
        APP(ilist, instr_create_jump_via_dcontext(dcontext, NEXT_TAG_OFFSET));
    } else {
        if (shared) {
            /* next_tag placed into tls slot earlier in this routine */
            APP(ilist,
                XINST_CREATE_jump_mem(dcontext,
                                      OPND_TLS_FIELD(FCACHE_ENTER_TARGET_SLOT)));

        } else {
#ifdef WINDOWS
            /* FIXME: we could just use tls, right?  no real need for the "shared"
             * parameter?
             */
            /* need one absolute ref using main dcontext (not one in edi):
             * it's the final jmp, using the special slot we set up earlier
             */
            APP(ilist, instr_create_jump_via_dcontext(dcontext,
                                                      NONSWAPPED_SCRATCH_OFFSET));
#else /* !WINDOWS */
            /* no special scratch slot! */
            ASSERT_NOT_IMPLEMENTED(false);
#endif /* !WINDOWS */
        }
    }
}

/* Our context switch to and from the fragment cache are arranged such
 * that there is no persistent state kept on the dstack, allowing us to
 * start with a clean slate on exiting the cache.  This eliminates the
 * need to protect our dstack from inadvertent or malicious writes.
 *
 * We do not bother to save any DynamoRIO state, even the eflags.  We clear
 * them in fcache_return, assuming that a cleared state is always the
 * proper value (df is never set across the cache, etc.)
 *
 * The code is split into several helper functions.
 *
 * # Used by dispatch to begin execution in fcache at dcontext->next_tag
 * fcache_enter(dcontext_t *dcontext)
 *
 *  if (!absolute)
 *      mov    ARG1, SCRATCH_REG5 # dcontext param
 *    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *      RESTORE_FROM_UPCONTEXT PROT_OFFSET, %xsi
 *    endif
 *  endif
 *
 *  # append_setup_fcache_target
 *  if (!absolute)
 *      # put target somewhere we can be absolute about
 *      RESTORE_FROM_UPCONTEXT next_tag_OFFSET, SCRATCH_REG0
 *    if (shared)
 *      mov  SCRATCH_REG0, fs:xax_OFFSET
 *    endif
 *  endif
 *
 *  # append_call_exit_dr_hook
 *  if (EXIT_DR_HOOK != NULL && !dcontext->ignore_enterexit)
 *    if (!absolute)
 *      push    %xdi
 *      push    %xsi
 *    else
 *      # support for skipping the hook
 *      RESTORE_FROM_UPCONTEXT ignore_enterexit_OFFSET,%edi
 *      cmpl    %edi,0
 *      jnz     post_hook
 *    endif
 *      call    EXIT_DR_HOOK # for x64 windows, reserve 32 bytes stack space for call
 *    if (!absolute)
 *      pop    %xsi
 *      pop    %xdi
 *    endif
 *  endif
 *
 *  post_hook:
 *
 *  # restore the original register state
 *
 *  # append_restore_xflags
 *  RESTORE_FROM_UPCONTEXT xflags_OFFSET,%xax
 *  push    %xax
 *  popf            # restore eflags temporarily using dstack
 *
 *  # append_restore_simd_reg
 *  if preserve_xmm_caller_saved
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+0*16,%xmm0
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+1*16,%xmm1
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+2*16,%xmm2
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+3*16,%xmm3
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+4*16,%xmm4
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+5*16,%xmm5
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+6*16,%xmm6  # 32-bit Linux
 *    RESTORE_FROM_UPCONTEXT xmm_OFFSET+7*16,%xmm7  # 32-bit Linux
 *  endif
 *
 *  # append_restore_gpr
 *  ifdef X64
 *    RESTORE_FROM_UPCONTEXT r8_OFFSET,%r8
 *    RESTORE_FROM_UPCONTEXT r9_OFFSET,%r9
 *    RESTORE_FROM_UPCONTEXT r10_OFFSET,%r10
 *    RESTORE_FROM_UPCONTEXT r11_OFFSET,%r11
 *    RESTORE_FROM_UPCONTEXT r12_OFFSET,%r12
 *    RESTORE_FROM_UPCONTEXT r13_OFFSET,%r13
 *    RESTORE_FROM_UPCONTEXT r14_OFFSET,%r14
 *    RESTORE_FROM_UPCONTEXT r15_OFFSET,%r15
 *  endif
 *    RESTORE_FROM_UPCONTEXT xax_OFFSET,%xax
 *    RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx
 *    RESTORE_FROM_UPCONTEXT xcx_OFFSET,%xcx
 *    RESTORE_FROM_UPCONTEXT xdx_OFFSET,%xdx
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xsi
 *  endif
 *  if (absolute || TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xdi
 *  endif
 *    RESTORE_FROM_UPCONTEXT xbp_OFFSET,%xbp
 *    RESTORE_FROM_UPCONTEXT xsp_OFFSET,%xsp
 *  if (!absolute)
 *    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *      RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xsi
 *    else
 *      RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xdi
 *    endif
 *  endif
 *
 *  # append_jmp_to_fcache_target
 *  ifdef X64 and (target is x86 mode)
 *    # we can't indirect through a register since we couldn't restore
 *    # the high bits (PR 283152)
 *    mov gencode-jmp86-value, fs:xbx_OFFSET
 *    far jmp to next instr, stored w/ 32-bit cs selector in fs:xbx_OFFSET
 *  endif
 *
 *  # jump indirect through dcontext->next_tag, set by dispatch()
 *  if (absolute)
 *    JUMP_VIA_DCONTEXT next_tag_OFFSET
 *  else
 *    if (shared)
 *      jmp *fs:xax_OFFSET
 *    else
 *      JUMP_VIA_DCONTEXT nonswapped_scratch_OFFSET
 *    endif
 *  endif
 *
 *  # now executing in fcache
 */
static byte *
emit_fcache_enter_common(dcontext_t *dcontext, generated_code_t *code,
                         byte *pc, bool absolute, bool shared)
{
    int len;
    instrlist_t ilist;
    patch_list_t patch;
#ifdef X64
    byte *jmp86_store_addr = NULL;
    byte *jmp86_target_addr = NULL;
#endif /* X64 */

    init_patch_list(&patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_XDI);
    instrlist_init(&ilist);

    /* no support for absolute addresses on x64/ARM: we always use tls */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute && shared));
    IF_ARM(ASSERT_NOT_IMPLEMENTED(!absolute && shared));

    if (!absolute) {
        /* grab gen routine's parameter dcontext and put it into edi */
        APP(&ilist,
            IF_X86_ELSE(INSTR_CREATE_mov_ld, XINST_CREATE_move)
            (dcontext, opnd_create_reg(SCRATCH_REG5), OPND_ARG1));
        if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
            IF_X86_ELSE({
                APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XSI, PROT_OFFS));
            }, {
                /* FIXME i#1551: SELFPROT is not supported on ARM */
                ASSERT_NOT_REACHED();
            });
        }
    }

    append_setup_fcache_target(dcontext, &ilist, absolute, shared);
    append_call_exit_dr_hook(dcontext, &ilist, absolute, shared);

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
    /* i#249: isolate the PEB */
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
        preinsert_swap_peb(dcontext, &ilist, NULL, absolute, REG_XDI,
                           REG_XAX/*scratch*/, false/*to app*/);
    }
#endif

    /* restore the original register state */
    append_restore_xflags(dcontext, &ilist, absolute);
    append_restore_simd_reg(dcontext, &ilist, absolute);
    append_restore_gpr(dcontext, &ilist, absolute);
    append_jmp_to_fcache_target(dcontext, &ilist, code, absolute, shared, &patch
                                _IF_X64(&jmp86_store_addr)
                                _IF_X64(&jmp86_target_addr));

    /* now encode the instructions */
    len = encode_with_patch_list(dcontext, &patch, &ilist, pc);
    ASSERT(len != 0);

#ifdef X64
    if (GENCODE_IS_X86(code->gencode_mode)) {
        /* Put the absolute address in place */
        ASSERT(jmp86_target_addr != NULL && jmp86_store_addr != NULL);
        ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)jmp86_target_addr));
        *((uint *)jmp86_store_addr) = (uint)(ptr_uint_t)jmp86_target_addr;
    }
#endif

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc + len;
}

byte *
emit_fcache_enter(dcontext_t *dcontext, generated_code_t *code, byte *pc)
{
    return emit_fcache_enter_common(dcontext, code, pc,
                                    true/*absolute*/, false/*!shared*/);
}

/* Generate a shared prologue for grabbing the dcontext into XDI

   TODO: Should be used by fcache_return and shared IBL routines,
   but for now some assumptions are not quite the same.

   Only assumption is that xcx cannot be touched (IBL expects looked up address)
       if save_xdi we assume DCONTEXT_BASE_SPILL_SLOT can be clobbered

   OUTPUT: xdi contains dcontext
       if save_xdi DCONTEXT_BASE_SPILL_SLOT will contain saved value
   FIXME: xdx is the spill slot -- switch over to xdx as base reg?
       Have to measure perf effect first (case 5239)

    00:   mov xdi, tls_slot_scratch2   64 89 3d 0c 0f 00 00 mov    %edi -> %fs:0xf0c
    07:   mov tls_slot_dcontext, xdi   64 8b 3d 14 0f 00 00 mov    %fs:0xf14 -> %edi
  if TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)
     ASSERT_NOT_TESTED
  endif
*/
void
insert_shared_get_dcontext(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                           bool save_xdi)
{
    /* needed to support grabbing the dcontext w/ shared cache */
    if (save_xdi) {
        PRE(ilist, where, SAVE_TO_TLS(dcontext, REG_XDI, DCONTEXT_BASE_SPILL_SLOT));
    }
    PRE(ilist, where, RESTORE_FROM_TLS(dcontext, REG_XDI, TLS_DCONTEXT_SLOT));
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        bool absolute = false;
        /* PR 224798: we could avoid extra indirection by storing
         * unprotected_context_t in TLS_DCONTEXT_SLOT instead of dcontext_t
         */
        ASSERT_NOT_TESTED();
        /* we'd need a 3rd slot in order to nicely get unprot ptr into esi
         * we can do it w/ only 2 slots by clobbering dcontext ptr
         * (we could add base reg info to RESTORE_FROM_DC/SAVE_TO_DC and go
         * straight through esi to begin w/ and subtract one instr (xchg)
         */
        PRE(ilist, where, RESTORE_FROM_DC(dcontext, REG_XDI, PROT_OFFS));
        PRE(ilist, where, INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XSI),
                                     opnd_create_reg(REG_XDI)));
        PRE(ilist, where, SAVE_TO_DC(dcontext, REG_XDI, XSI_OFFSET));
        PRE(ilist, where, RESTORE_FROM_TLS(dcontext, REG_XDI, TLS_DCONTEXT_SLOT));
    }
}


/* restore XDI through TLS */
void
insert_shared_restore_dcontext_reg(dcontext_t *dcontext, instrlist_t *ilist,
                                   instr_t *where)
{
    PRE(ilist, where, RESTORE_FROM_TLS(dcontext, REG_XDI, DCONTEXT_BASE_SPILL_SLOT));
}


/*  append instructions to prepare for fcache return:
 *  i.e., far jump to switch mode, load dcontext, etc.
 *
 *  # on X86
 *  ifdef X64 and (source is x86 mode)
 *    far direct jmp to next instr w/ 64-bit switch
 *  endif
 *
 *  if (!absolute)
 *    mov  %xdi,fs:xdx_OFFSET
 *    mov  fs:dcontext,%xdi
 *    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *      RESTORE_FROM_DCONTEXT PROT_OFFSET,%xdi
 *      xchg   %xsi,%xdi
 *      SAVE_TO_UPCONTEXT %xdi,xsi_OFFSET
 *      mov    fs:dcontext,%xdi
 *    endif
 *    # get xax and xdi into their real slots, via xbx
 *    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *    mov    fs:xax_OFFSET,%xbx
 *    SAVE_TO_UPCONTEXT %xbx,xax_OFFSET
 *    mov    fs:xdx_OFFSET,%xbx
 *    SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET
 *  endif
 */
static void
append_prepare_fcache_return(dcontext_t *dcontext, instrlist_t *ilist,
                             bool absolute, bool shared)
{
#ifdef X86_64
    if (GENCODE_IS_X86(code->gencode_mode)) {
        instr_t *label = INSTR_CREATE_label(dcontext);
        instr_t *ljmp = INSTR_CREATE_jmp_far
            (dcontext, opnd_create_far_instr(CS64_SELECTOR, label));
        instr_set_x86_mode(ljmp, true/*x86*/);
        APP(ilist, ljmp);
        APP(ilist, label);
    }
#endif /* X86_64 */

    if (absolute)
        return;

    /* only support non-absolute w/ shared cache */
    ASSERT_NOT_IMPLEMENTED(shared);
    /* xax is in 1 scratch slot, so we have to use a 2nd scratch
     * slot in order to get dcontext into xdi
     */
    APP(ilist, SAVE_TO_TLS(dcontext, REG_DCTXT, DCONTEXT_BASE_SPILL_SLOT));
    APP(ilist, RESTORE_FROM_TLS(dcontext, REG_DCTXT, TLS_DCONTEXT_SLOT));
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
#ifdef X86
        /* we'd need a 3rd slot in order to nicely get unprot ptr into xsi
         * we can do it w/ only 2 slots by clobbering dcontext ptr
         * (we could add base reg info to RESTORE_FROM_DC/SAVE_TO_DC and go
         * straight through xsi to begin w/ and subtract one instr (xchg)
         */
        ASSERT_NOT_TESTED();
        APP(ilist, RESTORE_FROM_DC(dcontext, REG_XDI, PROT_OFFS));
        APP(ilist, INSTR_CREATE_xchg(dcontext, opnd_create_reg(REG_XSI),
                                     opnd_create_reg(REG_XDI)));
        APP(ilist, SAVE_TO_DC(dcontext, REG_XDI, XSI_OFFSET));
        APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XDI, TLS_DCONTEXT_SLOT));
#elif defined(ARM)
        /* FIXME i#1551: NYI on ARM */
        ASSERT_NOT_REACHED();
#endif /* X86/ARM */
    }
}

static void
append_call_dispatch(dcontext_t *dcontext, instrlist_t *ilist, bool absolute)
{
    /* call central dispatch routine */
    /* for x64 linux we could optimize and avoid the "mov rdi, rdi" */
    dr_insert_call((void *)dcontext, ilist, NULL/*append*/,
                   (void *)dispatch, 1,
                   absolute ?
                   OPND_CREATE_INTPTR((ptr_int_t)dcontext) : opnd_create_reg(REG_DCTXT));

    /* dispatch() shouldn't return! */
    insert_reachable_cti(dcontext, ilist, NULL, vmcode_get_start(),
                         (byte *)unexpected_return, true/*jmp*/, false/*!precise*/,
                         DR_REG_R11/*scratch*/, NULL);
}

/*
 * # fcache_return: context switch back to DynamoRIO.
 * # Invoked via
 * #     a) from the fcache via a fragment exit stub,
 * #     b) from indirect_branch_lookup().
 * # Invokes dispatch() with a clean dstack.
 * # Assumptions:
 * #     1) app's value in xax/r0 already saved in dcontext.
 * #     2) xax/r0 holds the linkstub ptr
 * #
 *
 * fcache_return:
 *  # append_prepare_fcache_return
 *  ifdef X64 and (source is x86 mode)
 *      far direct jmp to next instr w/ 64-bit switch
 *  endif
 *
 *  if (!absolute)
 *    mov  %xdi,fs:xdx_OFFSET
 *    mov  fs:dcontext,%xdi
 *    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *      RESTORE_FROM_DCONTEXT PROT_OFFSET,%xdi
 *      xchg   %xsi,%xdi
 *      SAVE_TO_UPCONTEXT %xdi,xsi_OFFSET
 *      mov    fs:dcontext,%xdi
 *    endif
 *  endif
 *
 *  # append_save_gpr
 *  if (!absolute)
 *    # get xax and xdi into their real slots, via xbx
 *    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *    mov    fs:xax_OFFSET,%xbx
 *    SAVE_TO_UPCONTEXT %xbx,xax_OFFSET
 *    mov    fs:xdx_OFFSET,%xbx
 *    SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET
 *  endif
 *
 *  # save the current register state to context->regs
 *  # xax already in context
 *
 *  if (absolute)
 *    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
 *  endif
 *    SAVE_TO_UPCONTEXT %xcx,xcx_OFFSET
 *    SAVE_TO_UPCONTEXT %xdx,xdx_OFFSET
 *  if (absolute || !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
 *    SAVE_TO_UPCONTEXT %xsi,xsi_OFFSET
 *  endif
 *  if (absolute)
 *    SAVE_TO_UPCONTEXT %xdi,xdi_OFFSET
 *  endif
 *    SAVE_TO_UPCONTEXT %xbp,xbp_OFFSET
 *    SAVE_TO_UPCONTEXT %xsp,xsp_OFFSET
 *  ifdef X64
 *    SAVE_TO_UPCONTEXT %r8,r8_OFFSET
 *    SAVE_TO_UPCONTEXT %r9,r9_OFFSET
 *    SAVE_TO_UPCONTEXT %r10,r10_OFFSET
 *    SAVE_TO_UPCONTEXT %r11,r11_OFFSET
 *    SAVE_TO_UPCONTEXT %r12,r12_OFFSET
 *    SAVE_TO_UPCONTEXT %r13,r13_OFFSET
 *    SAVE_TO_UPCONTEXT %r14,r14_OFFSET
 *    SAVE_TO_UPCONTEXT %r15,r15_OFFSET
 *  endif
 *
 *  # append_save_simd_reg
 *  if preserve_xmm_caller_saved
 *    SAVE_TO_UPCONTEXT %xmm0,xmm_OFFSET+0*16
 *    SAVE_TO_UPCONTEXT %xmm1,xmm_OFFSET+1*16
 *    SAVE_TO_UPCONTEXT %xmm2,xmm_OFFSET+2*16
 *    SAVE_TO_UPCONTEXT %xmm3,xmm_OFFSET+3*16
 *    SAVE_TO_UPCONTEXT %xmm4,xmm_OFFSET+4*16
 *    SAVE_TO_UPCONTEXT %xmm5,xmm_OFFSET+5*16
 *    SAVE_TO_UPCONTEXT %xmm6,xmm_OFFSET+6*16  # 32-bit Linux
 *    SAVE_TO_UPCONTEXT %xmm7,xmm_OFFSET+7*16  # 32-bit Linux
 *  endif
 *
 *  # switch to clean dstack
 *  RESTORE_FROM_DCONTEXT dstack_OFFSET,%xsp
 *
 *  # append_save_clear_xflags
 *  # now save eflags -- too hard to do without a stack!
 *  pushf           # push eflags on stack
 *  pop     %xbx    # grab eflags value
 *  SAVE_TO_UPCONTEXT %xbx,xflags_OFFSET # save eflags value
 *
 *  # clear eflags now to avoid app's eflags messing up our ENTER_DR_HOOK
 *  # FIXME: this won't work at CPL0 if we ever run there!
 *  push  0
 *  popf
 *
 *  # append_call_enter_dr_hook
 *  if (ENTER_DR_HOOK != NULL && !dcontext->ignore_enterexit)
 *    # don't bother to save any registers around call except for xax
 *    # and xcx, which holds next_tag
 *    push    %xcx
 *    if (!absolute)
 *      push    %xdi
 *      push    %xsi
 *    endif
 *      push    %xax
 *    if (absolute)
 *      # support for skipping the hook (note: 32-bits even on x64)
 *      RESTORE_FROM_UPCONTEXT ignore_enterexit_OFFSET,%edi
 *      cmp     %edi,0
 *      jnz     post_hook
 *    endif
 *    # for x64 windows, reserve 32 bytes stack space for call prior to call
 *    call    ENTER_DR_HOOK
 *
 *   post_hook:
 *    pop     %xax
 *    if (!absolute)
 *      pop     %xsi
 *      pop     %xdi
 *    endif
 *      pop     %xcx
 *  endif
 *
 *  # save last_exit, currently in eax, into dcontext->last_exit
 *  SAVE_TO_DCONTEXT %xax,last_exit_OFFSET
 *
 *  .ifdef WINDOWS && CLIENT_INTERFACE
 *    swap_peb
 *  .endif
 *
 *  .ifdef SIDELINE
 *    # clear cur-trace field so we don't think cur trace is still running
 *    movl    $0, _sideline_trace
 *  .endif
 *
 *  # call central dispatch routine w/ dcontext as an argument
 *  if (absolute)
 *    push    <dcontext>
 *  else
 *    push     %xdi  # for x64, mov %xdi, ARG1
 *  endif
 *  call    dispatch # for x64 windows, reserve 32 bytes stack space for call
 *  # dispatch() shouldn't return!
 *  jmp     unexpected_return
 */

/* N.B.: this routine is used to generate both the regular fcache_return
 * and a slightly different copy that is used for the miss/unlinked paths
 * for indirect_branch_lookup for self-protection.
 * ibl_end should be true only for that end of the lookup routine.
 *
 * If linkstub != NULL, used for coarse fragments, this routine assumes that:
 * - app xax is still in %xax
 * - next target pc is in DIRECT_STUB_SPILL_SLOT tls
 * - linkstub is the linkstub_t to pass back to dispatch
 * - if coarse_info:
 *   - app xcx is in MANGLE_XCX_SPILL_SLOT
 *   - source coarse info is in %xcx
 *
 * We assume this routine does not use TLS slot FLOAT_PC_STATE_SLOT (TLS_XBX_SLOT).
 */
static bool
append_fcache_return_common(dcontext_t *dcontext, generated_code_t *code,
                            instrlist_t *ilist, bool ibl_end,
                            bool absolute, bool shared, linkstub_t *linkstub,
                            bool coarse_info)
{
    bool instr_targets;

    /* no support for absolute addresses on x64: we always use tls */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute && shared));

    /* currently linkstub is only used for coarse-grain exits */
    ASSERT(linkstub == NULL || !absolute);

    append_prepare_fcache_return(dcontext, ilist, absolute, shared);
    append_save_gpr(dcontext, ilist, ibl_end, absolute, code, linkstub, coarse_info);
    append_save_simd_reg(dcontext, ilist, absolute);

    /* Switch to a clean dstack as part of our scheme to avoid state kept
     * unprotected across cache executions.
     * FIXME: this isn't perfect: we switch to the dstack BEFORE we call
     * the entrance hook that will be used to coordinate other threads,
     * so if our hook suspends all other threads to protect vs cross-thread
     * attacks, the dstack is not perfectly protected.
     */
    APP(ilist, RESTORE_FROM_DC(dcontext, REG_XSP, DSTACK_OFFSET));

    append_save_clear_xflags(dcontext, ilist, absolute);
    instr_targets = append_call_enter_dr_hook(dcontext, ilist, ibl_end, absolute);

    /* save last_exit, currently in scratch_reg0 into dcontext->last_exit */
    APP(ilist, SAVE_TO_DC(dcontext, SCRATCH_REG0, LAST_EXIT_OFFSET));

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
    /* i#249: isolate the PEB */
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
        preinsert_swap_peb(dcontext, ilist, NULL, absolute, REG_XDI,
                           REG_XAX/*scratch*/, true/*to priv*/);
    }
#endif /* WINDOWS && CLIENT_INTERFACE */

#ifdef SIDELINE
    if (dynamo_options.sideline) {
        /* clear cur-trace field so we don't think cur trace is still running */
        /* PR 248210: unsupported feature on x64 */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* PR 244737: fix abs address */
        APP(ilist,
            INSTR_CREATE_mov_st(dcontext,
                                OPND_CREATE_MEM32(REG_NULL, (int)&sideline_trace),
                                OPND_CREATE_INT32(0)));
    }
#endif

    append_call_dispatch(dcontext, ilist, absolute);
    return instr_targets;
}

byte *
emit_fcache_return(dcontext_t *dcontext, generated_code_t *code, byte *pc)
{
    bool instr_targets;
    instrlist_t ilist;
    instrlist_init(&ilist);
    instr_targets = append_fcache_return_common(dcontext, code, &ilist,
                                                false/*!ibl_end*/,
                                                true/*absolute*/, false/*!shared*/,
                                                NULL, false/*not coarse*/);
    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, instr_targets);
    ASSERT(pc != NULL);
    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    return pc;
}

byte *
emit_fcache_enter_shared(dcontext_t *dcontext, generated_code_t *code, byte *pc)
{
    return emit_fcache_enter_common(dcontext, code, pc,
                                    false/*through xdi*/, true/*shared*/);
}

byte *
emit_fcache_return_shared(dcontext_t *dcontext, generated_code_t *code, byte *pc)
{
    bool instr_targets;
    instrlist_t ilist;
    instrlist_init(&ilist);
    instr_targets = append_fcache_return_common(dcontext, code, &ilist, false/*!ibl_end*/,
                                                false/*through xdi*/, true/*shared*/,
                                                NULL, false/*not coarse*/);
    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, instr_targets);
    ASSERT(pc != NULL);
    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    return pc;
}

byte *
emit_fcache_return_coarse(dcontext_t *dcontext, generated_code_t *code, byte *pc)
{
    bool instr_targets;
    linkstub_t *linkstub = (linkstub_t *) get_coarse_exit_linkstub();
    instrlist_t ilist;
    instrlist_init(&ilist);
    instr_targets = append_fcache_return_common(dcontext, code, &ilist, false/*!ibl_end*/,
                                                false/*through xdi*/, true/*shared*/,
                                                linkstub, true/*coarse info in xcx*/);
    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, instr_targets);
    ASSERT(pc != NULL);
    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    return pc;
}

byte *
emit_trace_head_return_coarse(dcontext_t *dcontext, generated_code_t *code, byte *pc)
{
    /* Could share tail end of coarse_fcache_return instead of duplicating */
    bool instr_targets;
    linkstub_t *linkstub = (linkstub_t *) get_coarse_trace_head_exit_linkstub();
    instrlist_t ilist;
    instrlist_init(&ilist);
    instr_targets = append_fcache_return_common(dcontext, code, &ilist, false/*!ibl_end*/,
                                                false/*through xdi*/, true/*shared*/,
                                                linkstub, false/*no coarse info*/);
    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, instr_targets);
    ASSERT(pc != NULL);
    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    return pc;
}

/* Our coarse entrance stubs have several advantages, such as eliminating
 * future fragments, but their accompanying lazy linking does need source
 * information that is not available in each stub.  We instead have an
 * unlinked entrance stub target a per-unit prefix that records the source
 * unit.  We can then search within the unit to identify the actual source
 * entrance stub, which is enough for lazy linking (but does not find the
 * unique source tag: case 8565).  This also gives us a single indirection
 * point in the form of the prefix at which to patch the fcache_return target.
 * We also place in the prefix indirection points for trace head cache exit and
 * the 3 coarse ibl targets, to keep the cache read-only and (again) make it
 * easier to patch when persisting/sharing.
 */
uint
coarse_exit_prefix_size(coarse_info_t *info)
{
#ifdef X64
    uint flags = COARSE_32_FLAG(info);
#endif
    /* FIXME: would be nice to use size calculated in emit_coarse_exit_prefix(),
     * but we need to know size before we emit and would have to do a throwaway
     * emit, or else set up a template to be patched w/ specific info field.
     * Also we'd have to unprot .data as we don't access this until post-init.
     */
    /* We don't need to require addr16: in fact it might be better to force
     * not using it, so if we persist on P4 but run on Core we don't lose
     * performance.  We have enough space.
     */
    return SIZE_MOV_XBX_TO_TLS(flags, false) + SIZE_MOV_PTR_IMM_TO_XAX(flags)
        + 5*JMP_LONG_LENGTH;
}

byte *
emit_coarse_exit_prefix(dcontext_t *dcontext, byte *pc, coarse_info_t *info)
{
    byte *ibl;
    DEBUG_DECLARE(byte *start_pc = pc;)
    instrlist_t ilist;
    patch_list_t patch;
    instr_t *fcache_ret_prefix;
#ifdef X64
    gencode_mode_t mode = FRAGMENT_GENCODE_MODE(COARSE_32_FLAG(info));
#endif

    instrlist_init(&ilist);
    init_patch_list(&patch, PATCH_TYPE_INDIRECT_FS);

    /* prefix looks like this, using xcx instead of xbx just to make
     * the fcache_return code simpler (as it already uses xbx early),
     * and using the info as we're doing per-cache and not per-unit:
     *
     *   fcache_return_coarse_prefix:
     *   6/9 mov  %xcx, MANGLE_XCX_SPILL_SLOT
     *  5/10 mov  <info ptr>, %xcx
     *     5 jmp fcache_return_coarse
     *   trace_head_return_coarse_prefix:
     *     5 jmp trace_head_return_coarse
     *       (if -disable_traces, it jmps to fcache_return_coarse_prefix instead)
     *   coarse_ibl_ret_prefix:
     *     5 jmp coarse_ibl_ret
     *   coarse_ibl_call_prefix:
     *     5 jmp coarse_ibl_call
     *   coarse_ibl_jmp_prefix:
     *     5 jmp coarse_ibl_jmp
     *
     * We assume that info ptr is at
     *   trace_head_return_prefix - JMP_LONG_LENGTH - 4
     * in patch_coarse_exit_prefix().
     * We assume that the ibl prefixes are nothing but jmps in
     * coarse_indirect_stub_jmp_target() so we can recover the ibl type.
     *
     * FIXME case 9647: on P4 our jmp->jmp sequence will be
     * elided, but on Core we may want to switch to a jmp*, though
     * since we have no register for a base ptr we'd need a reloc
     * entry for every single stub
     */
    /* entrance stub has put target_tag into xax-slot so we use xcx-slot */
    ASSERT(DIRECT_STUB_SPILL_SLOT != MANGLE_XCX_SPILL_SLOT);

    fcache_ret_prefix = INSTR_CREATE_label(dcontext);
    APP(&ilist, fcache_ret_prefix);

#ifdef X64
    if (TEST(PERSCACHE_X86_32, info->flags)) {
        /* XXX: this won't work b/c opnd size will be wrong */
        ASSERT_NOT_IMPLEMENTED(false && "must pass opnd size to SAVE_TO_TLS");
        APP(&ilist, SAVE_TO_TLS(dcontext, REG_ECX, MANGLE_XCX_SPILL_SLOT));
        /* We assume all our data structures are <4GB which is guaranteed for
         * WOW64 processes.
         */
        ASSERT(CHECK_TRUNCATE_TYPE_int((ptr_int_t)info));
        APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_ECX),
                                         OPND_CREATE_INT32((int)(ptr_int_t)info)));
    } else { /* default code */
        if (GENCODE_IS_X86_TO_X64(mode) && DYNAMO_OPTION(x86_to_x64_ibl_opt))
            APP(&ilist, SAVE_TO_REG(dcontext, REG_XCX, REG_R9));
        else
#endif
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
        APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                         OPND_CREATE_INTPTR((ptr_int_t)info)));
#ifdef X64
    }
#endif
    APP(&ilist, INSTR_CREATE_jmp(dcontext,
        opnd_create_pc(get_direct_exit_target(dcontext,
                                              FRAG_SHARED | FRAG_COARSE_GRAIN |
                                              COARSE_32_FLAG(info)))));

    APP(&ilist, INSTR_CREATE_label(dcontext));
    add_patch_marker(&patch, instrlist_last(&ilist), PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* start of instr */,
                     (ptr_uint_t*)&info->trace_head_return_prefix);
    if (DYNAMO_OPTION(disable_traces) ||
        /* i#670: the stub stored the abs addr at persist time.  we need
         * to adjust to the use-time mod base which we do in dispatch
         * but we need to set the dcontext->coarse_exit so we go through
         * the fcache return
         */
        (info->frozen && info->mod_shift != 0)) {
        /* trace_t heads need to store the info ptr for lazy linking */
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(fcache_ret_prefix)));
    } else {
        APP(&ilist, INSTR_CREATE_jmp
            (dcontext, opnd_create_pc(trace_head_return_coarse_routine(IF_X64(mode)))));
    }

    /* coarse does not support IBL_FAR so we don't bother with get_ibl_entry_type() */
    ibl = get_ibl_routine_ex(dcontext, IBL_LINKED,
                             get_source_fragment_type(dcontext,
                                                      FRAG_SHARED | FRAG_COARSE_GRAIN),
                             IBL_RETURN _IF_X64(mode));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(ibl)));
    add_patch_marker(&patch, instrlist_last(&ilist), PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* start of instr */, (ptr_uint_t*)&info->ibl_ret_prefix);

    ibl = get_ibl_routine_ex(dcontext, IBL_LINKED,
                             get_source_fragment_type(dcontext,
                                                      FRAG_SHARED | FRAG_COARSE_GRAIN),
                             IBL_INDCALL _IF_X64(mode));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(ibl)));
    add_patch_marker(&patch, instrlist_last(&ilist), PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* start of instr */, (ptr_uint_t*)&info->ibl_call_prefix);

    ibl = get_ibl_routine_ex(dcontext, IBL_LINKED,
                             get_source_fragment_type(dcontext,
                                                      FRAG_SHARED | FRAG_COARSE_GRAIN),
                             IBL_INDJMP _IF_X64(mode));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(ibl)));
    add_patch_marker(&patch, instrlist_last(&ilist), PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* start of instr */, (ptr_uint_t*)&info->ibl_jmp_prefix);

    /* now encode the instructions */
    pc += encode_with_patch_list(dcontext, &patch, &ilist, pc);
    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    ASSERT((size_t)(pc - start_pc) == coarse_exit_prefix_size(info));

    DOLOG(3, LOG_EMIT, {
        byte *dpc = start_pc;
        LOG(GLOBAL, LOG_EMIT, 3, "\nprefixes for coarse unit %s:\n", info->module);
        do {
            if (dpc == info->fcache_return_prefix)
                LOG(GLOBAL, LOG_EMIT, 3, "fcache_return_coarse_prefix:\n");
            else if (dpc == info->trace_head_return_prefix)
                LOG(GLOBAL, LOG_EMIT, 3, "trace_head_return_coarse_prefix:\n");
            else if (dpc == info->ibl_ret_prefix)
                LOG(GLOBAL, LOG_EMIT, 3, "ibl_coarse_ret_prefix:\n");
            else if (dpc == info->ibl_call_prefix)
                LOG(GLOBAL, LOG_EMIT, 3, "ibl_coarse_call_prefix:\n");
            else if (dpc == info->ibl_jmp_prefix)
                LOG(GLOBAL, LOG_EMIT, 3, "ibl_coarse_jmp_prefix:\n");
            dpc = disassemble_with_bytes(dcontext, dpc, GLOBAL);
        } while (dpc < pc);
        LOG(GLOBAL, LOG_EMIT, 3, "\n");
    });

    return pc;
}

/* Update info pointer in exit prefixes */
void
patch_coarse_exit_prefix(dcontext_t *dcontext, coarse_info_t *info)
{
    ptr_uint_t *pc = (ptr_uint_t *)
        (info->trace_head_return_prefix - JMP_LONG_LENGTH - sizeof(info));
    *pc = (ptr_uint_t) info;
}


/* saves the eflags
 * uses the xax slot, either in TLS
 * memory if tls is true; else using mcontext accessed using absolute address if
 * absolute is true, else off xdi.
 * MUST NOT clobber xax between this call and the restore call!
 */
void
insert_save_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                   uint flags, bool tls, bool absolute _IF_X64(bool x86_to_x64_ibl_opt))
{
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (TEST(FRAG_WRITES_EFLAGS_6, flags)) /* no flag save needed */
        return;
    /* save the flags */
    /*>>>    SAVE_TO_TLS/UPCONTEXT %xax,xax_tls_slot/xax_OFFSET  */
    /*>>>    lahf                                                */
    /*>>>    seto        %al                                     */
    /* for shared ibl targets we put eflags in tls -- else, we use mcontext,
     * either absolute address or indirected via xdi as specified by absolute param
     */
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        /* in x86_to_x64, spill rax to r8 */
        PRE(ilist, where, SAVE_TO_REG(dcontext, REG_XAX, REG_R8));
    } else if (tls) {
        /* We need to save this in an easy location for the prefixes
         * to restore from.  FIXME: This can be much more streamlined
         * if TLS_SLOT_SCRATCH1 was the XAX spill slot for everyone
         */
        /* FIXME: since the prefixes are trying to be smart now
         * based on shared/privateness of the fragment, we also need
         * to know what would the target do if shared.
         */
        /*>>>    SAVE_TO_TLS %xax,xax_tls_slot                       */
        PRE(ilist, where, SAVE_TO_TLS(dcontext, REG_XAX, PREFIX_XAX_SPILL_SLOT));
    } else {
        /*>>>    SAVE_TO_UPCONTEXT %xax,xax_OFFSET                       */
        PRE(ilist, where, SAVE_TO_DC(dcontext, REG_XAX, XAX_OFFSET));
    }
    PRE(ilist, where, INSTR_CREATE_lahf(dcontext));
    if (!TEST(FRAG_WRITES_EFLAGS_OF, flags)
        && !INTERNAL_OPTION(unsafe_ignore_overflow)) { /* OF needs saving */
        /* Move OF flags into the OF flag spill slot. */
        PRE(ilist, where,
            INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
    }
}

/* restores eflags from xax and the xax app value from the xax slot, either in
 * TLS memory if tls is true; else using mcontext accessed using absolute
 * address if absolute is true, else off xdi.
 * also restores xax
 */
void
insert_restore_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                      uint flags, bool tls, bool absolute
                      _IF_X64(bool x86_to_x64_ibl_opt))
{
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (TEST(FRAG_WRITES_EFLAGS_6, flags)) /* no flag save was done */
        return;
    if (!TEST(FRAG_WRITES_EFLAGS_OF, flags) &&  /* OF was saved */
        !INTERNAL_OPTION(unsafe_ignore_overflow)) {
        /* restore OF using add that overflows and sets OF if OF was on when we did seto */
        PRE(ilist, where,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    }
    /* restore other 5 flags (still in xax at this point) */
    /*>>>    sahf                                                    */
    PRE(ilist, where, INSTR_CREATE_sahf(dcontext));
    /* restore xax */
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        PRE(ilist, where, RESTORE_FROM_REG(dcontext, REG_XAX, REG_R8));
    } else if (tls) {
        PRE(ilist, where, RESTORE_FROM_TLS(dcontext, REG_XAX, PREFIX_XAX_SPILL_SLOT));
    } else {
        /*>>>    RESTORE_FROM_UPCONTEXT xax_OFFSET,%xax                  */
        PRE(ilist, where, RESTORE_FROM_DC(dcontext, REG_XAX, XAX_OFFSET));
    }
}

#ifdef HASHTABLE_STATISTICS
/* note that arch_thread_init is called before fragment_thread_init, so these need to be updated */
/* When used in a thread-shared routine, this routine clobbers XDI. The
 * caller should spill & restore it or rematerialize it as needed. */
/* NOTE - this routine does NOT save the eflags, which will be clobbered by the
 * inc */
static void
append_increment_counter(dcontext_t *dcontext, instrlist_t *ilist,
                         ibl_code_t *ibl_code, patch_list_t *patch,
                         reg_id_t entry_register, /* register indirect (XCX) or NULL */
                         uint counter_offset, /* adjusted to unprot_ht_statistics_t if no entry_register */
                         reg_id_t scratch_register)
{
    instr_t *counter;
    bool absolute = !ibl_code->thread_shared_routine;
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (!INTERNAL_OPTION(hashtable_ibl_stats))
        return;

    LOG(THREAD, LOG_EMIT, 3, "append_increment_counter: hashtable_stats_offset=0x%x counter_offset=0x%x\n",
        ibl_code->hashtable_stats_offset, counter_offset);

    if (entry_register == REG_NULL) {
        /* adjust offset within a unprot_ht_statistics_t structure */
        counter_offset += ibl_code->hashtable_stats_offset;
    }

    if (!absolute) {
        opnd_t counter_opnd;

        /* get dcontext in register (xdi) */
        insert_shared_get_dcontext(dcontext, ilist, NULL, false/* dead register */);
        /* XDI now has dcontext */
        APP(ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XDI),
                                       OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR,
                                                     FRAGMENT_FIELD_OFFSET)));

        /* XDI now has per_thread_t structure */
        /* an extra step here: find the unprot_stats field in the fragment_table_t
         * could avoid for protect_mask==0 if we always had a copy
         * in the per_thread_t struct -- see fragment.h, not worth it
         */
        if (entry_register != REG_NULL) {
            APP(ilist, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(REG_XDI),
                 OPND_CREATE_MEMPTR(REG_XDI,
                                    ibl_code->entry_stats_to_lookup_table_offset)));
            /* XDI should now have (entry_stats - lookup_table) value,
             * so we need [xdi+xcx] to get an entry reference
             */
            counter_opnd = opnd_create_base_disp(REG_XDI, entry_register, 1,
                                                 counter_offset, OPSZ_4);
        } else {
            APP(ilist, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(REG_XDI),
                 OPND_CREATE_MEMPTR(REG_XDI, ibl_code->unprot_stats_offset)));
            /* XDI now has unprot_stats structure */
            counter_opnd = OPND_CREATE_MEM32(REG_XDI, counter_offset);
        }

        counter = INSTR_CREATE_inc(dcontext, counter_opnd);
        APP(ilist, counter);
    } else {
        /* TAKE_ADDRESS will in fact add the necessary base to the statistics structure,
           hence no explicit indirection needed here */
        opnd_t counter_opnd = OPND_CREATE_MEMPTR(entry_register, counter_offset);
        counter = INSTR_CREATE_inc(dcontext, counter_opnd);
        /* hack to get both this table's unprot offset and the specific stat's offs */
        ASSERT(counter_offset < USHRT_MAX);
        if (entry_register != REG_NULL) {
            /* although we currently don't use counter_offset, it doesn't hurt to support as well */
            ASSERT(ibl_code->entry_stats_to_lookup_table_offset < USHRT_MAX);
            add_patch_entry(patch, counter, PATCH_UNPROT_STAT|PATCH_TAKE_ADDRESS,
                            (ibl_code->entry_stats_to_lookup_table_offset << 16) | counter_offset);
        } else {
            ASSERT(ibl_code->unprot_stats_offset < USHRT_MAX);
            add_patch_entry(patch, counter, PATCH_UNPROT_STAT|PATCH_TAKE_ADDRESS,
                            (ibl_code->unprot_stats_offset << 16) | counter_offset);
        }
        APP(ilist, counter);
    }
}
#endif /* HASHTABLE_STATISTICS */

#ifdef INTERNAL
/* add a slowdown loop to measure if a routine is likely to be on a critical path */
/* note that FLAGS are clobbered */
static void
append_empty_loop(dcontext_t *dcontext, instrlist_t *ilist,
                  uint iterations, reg_id_t scratch_register)
{
    instr_t *initloop;
    instr_t *loop;
    /*       mov     ebx, iterations */
    /* loop: dec     ebx */
    /*       jnz loop */
    ASSERT(REG_NULL != scratch_register);

    initloop = INSTR_CREATE_mov_imm(dcontext,
                                    opnd_create_reg(scratch_register),
                                    OPND_CREATE_INT32(iterations));
    loop = INSTR_CREATE_dec(dcontext, opnd_create_reg(scratch_register));
    APP(ilist, initloop);
    APP(ilist, loop);
    APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jnz_short, opnd_create_instr(loop)));
}
#endif /* INTERNAL */

#ifdef X64
static void
instrlist_convert_to_x86(instrlist_t *ilist)
{
    instr_t *in;
    for (in = instrlist_first(ilist); in != NULL; in = instr_get_next(in)) {
        instr_set_x86_mode(in, true/*x86*/);
        instr_shrink_to_32_bits(in);
    }
}
#endif

/* what we do on a hit in the hashtable */
/* Restore XBX saved from the indirect exit stub insert_jmp_to_ibl() */
/* Indirect jump through hashtable entry pointed to by XCX */
static inline void
append_ibl_found(dcontext_t *dcontext, instrlist_t *ilist,
                 ibl_code_t *ibl_code, patch_list_t *patch,
                 uint start_pc_offset,
                 bool collision,
                 bool only_spill_state_in_tls, /* if true, no table info in TLS;
                                                * indirection off of XDI is used */
                 bool restore_eflags,
                 instr_t **fragment_found)
{
    bool absolute = !ibl_code->thread_shared_routine;
    bool target_prefix = true;
    /* eflags and xcx are restored in the target's prefix */
    /* if thread private routine */
    /*>>>    RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx                  */
    /*>>>    jmp     *FRAGMENT_START_PC_OFFS(%xcx)                   */
    instr_t *inst = NULL;
    IF_X64(bool x86_to_x64_ibl_opt = (ibl_code->x86_to_x64_mode &&
                                      DYNAMO_OPTION(x86_to_x64_ibl_opt));)

    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (absolute) {
        inst = RESTORE_FROM_DC(dcontext, REG_XBX, XBX_OFFSET);
    }

    if (!ibl_use_target_prefix(ibl_code)) {
        target_prefix = false;
        restore_eflags = true;
    }

#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats) ||
        INTERNAL_OPTION(hashtable_ibl_entry_stats)) {
        if (!absolute && !only_spill_state_in_tls) {
            /* XDI holds app state, not a ptr to dcontext+<some offset> */
            APP(ilist, SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
        }
        append_increment_counter(dcontext, ilist, ibl_code, patch,
                                 REG_NULL,
                                 HASHLOOKUP_STAT_OFFS(hit),
                                 REG_XBX);
        if (collision) {
            append_increment_counter(dcontext, ilist, ibl_code, patch,
                                     REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(collision_hit),
                                     REG_XBX);
        }
        if (INTERNAL_OPTION(hashtable_ibl_entry_stats)) {
            /* &lookup_table[i] - should allow access to &entry_stats[i] */
            append_increment_counter(dcontext, ilist, ibl_code, patch,
                                     REG_XCX,
                                     offsetof(fragment_stat_entry_t, hits),
                                     REG_XBX);
        }
        if (!absolute && !only_spill_state_in_tls)
            APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
    }
#endif /* HASHTABLE_STATISTICS */

#ifdef INTERNAL
    if (INTERNAL_OPTION(slowdown_ibl_found)) {
        /* add a loop here */
        append_empty_loop(dcontext, ilist, INTERNAL_OPTION(slowdown_ibl_found),
                          REG_XBX /* dead */);
    }
#endif /* INTERNAL */

    if (restore_eflags) {
        insert_restore_eflags(dcontext, ilist, NULL, 0,
                              IBL_EFLAGS_IN_TLS(), absolute _IF_X64(x86_to_x64_ibl_opt));
    }
    if (!target_prefix) {
        /* We're going to clobber the xax slot */
        ASSERT(restore_eflags);
        /* For target_delete support with no prefix, since we're
         * clobbering all the registers here, we must save something;
         * We save the tag, rather than the table entry, to avoid an
         * extra load to get the tag in target_delete:
         *     <save    %xbx to xax slot>  # put tag in xax slot for target_delete
         */
        if (absolute) {
            APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XAX_OFFSET));
        } else {
            APP(ilist, SAVE_TO_TLS(dcontext, REG_XBX, DIRECT_STUB_SPILL_SLOT));
        }
    }
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        APP(ilist, RESTORE_FROM_REG(dcontext, REG_XBX, REG_R10));
    } else if (absolute) {
        /* restore XBX through dcontext */
        APP(ilist, inst);
    } else {
        /* restore XBX through INDIRECT_STUB_SPILL_SLOT */
        APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, INDIRECT_STUB_SPILL_SLOT));
        DOCHECK(1, {
            if (!SHARED_IB_TARGETS())
                ASSERT(only_spill_state_in_tls);
        });
    }
    if (only_spill_state_in_tls) {
        /* If TLS doesn't hold table info, XDI was used for indirection.
         * Restore XDI through DCONTEXT_BASE_SPILL_SLOT */
        insert_shared_restore_dcontext_reg(dcontext, ilist, NULL);
    }

    if (target_prefix) {
        /* FIXME: do we want this?  seems to be a problem, I'm disabling:
         * ASSERT(!collision || start_pc_offset == FRAGMENT_START_PC_OFFS);  // c \imply FRAGMENT
         */
        APP(ilist, INSTR_CREATE_jmp_ind(dcontext,
                                        OPND_CREATE_MEMPTR(REG_XCX, start_pc_offset)));
    } else {
        /* There is no prefix so we must restore all and jmp through memory:
         *     mov      start_pc_offset(%xcx), %xcx
         *     <save    %xcx to xbx slot>  # put target in xbx slot for later jmp
         *     <restore %xcx from xcx slot>
         *     jmp*     <xbx slot>
         */
        APP(ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                       OPND_CREATE_MEMPTR(REG_XCX, start_pc_offset)));
        if (absolute) {
            APP(ilist, SAVE_TO_DC(dcontext, REG_XCX, XBX_OFFSET));
            if (IF_X64_ELSE(x86_to_x64_ibl_opt, false))
                APP(ilist, RESTORE_FROM_REG(dcontext, REG_XCX, REG_R9));
            else if (XCX_IN_TLS(0/*!FRAG_SHARED*/))
                APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
            else
                APP(ilist, RESTORE_FROM_DC(dcontext, REG_XCX, XCX_OFFSET));
            APP(ilist, INSTR_CREATE_jmp_ind(dcontext,
                                            OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR,
                                                          XBX_OFFSET)));
        } else {
            APP(ilist, SAVE_TO_TLS(dcontext, REG_XCX, INDIRECT_STUB_SPILL_SLOT));
#ifdef X64
            if (x86_to_x64_ibl_opt)
                APP(ilist, RESTORE_FROM_REG(dcontext, REG_XCX, REG_R9));
            else
#endif
                APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
            APP(ilist, INSTR_CREATE_jmp_ind(dcontext,
                                            OPND_TLS_FIELD(INDIRECT_STUB_SPILL_SLOT)));
        }
    }

    if (fragment_found != NULL)
        *fragment_found = inst;
}

#ifdef PROFILE_LINKCOUNT
/* assumes linkstub in ebx, can handle NULL linkstub pointer
 * if clobber_eflags is true, assumes can kill eflags
 * else, ASSUMES CAN KILL ECX
 * both require a non null next instruction
 */
void
append_linkcount_incr(dcontext_t *dcontext, instrlist_t *ilist, bool clobber_eflags,
                      instr_t *next)
{
    /* PR 248210: unsupported feature on x64 */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    ASSERT(next != NULL);
    if (clobber_eflags) {
        /*>>>    testl   ebx,ebx */
        /*>>>    je      next:  */
        /* P4 opt guide says to use test to cmp reg with 0: shorter instr */
        APP(ilist, INSTR_CREATE_test(dcontext, opnd_create_reg(REG_EBX),
                                     opnd_create_reg(REG_EBX)));
        APP(ilist, INSTR_CREATE_jcc_short(dcontext, OP_je,
                                          opnd_create_instr(next)));
# ifdef LINKCOUNT_64_BITS
        /*>>>    addl    $1,count_offs(ebx == &linkstub)                 */
        /*>>>    adcl    $0,count_offs+4(ebx == &linkstub)               */
        APP(ilist, INSTR_CREATE_add(dcontext,
                                    OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS),
                                    OPND_CREATE_INT8(1)));
        APP(ilist, INSTR_CREATE_adc(dcontext,
                                    OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS+4),
                                    OPND_CREATE_INT8(0)));
# else
        /*>>>    incl    count_offs(ebx == &linkstub)                    */
        APP(ilist, INSTR_CREATE_inc(dcontext,
                                    OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS)));
# endif
    } else {
# ifdef LINKCOUNT_64_BITS
        instr_t *carry;
# endif
        /*>>>    movl    %ebx, %ecx */
        /*>>>    jecxz   next:      */
        APP(ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                                       opnd_create_reg(REG_EBX)));
        APP(ilist, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(next)));
        /*>>>    movl    count_offs(%ebx == &linkstub), %ecx              */
        /*>>>    lea     1(%ecx), %ecx                                    */
        /*>>>    movl    %ecx, count_offs(%ebx == &linkstub), %ecx        */
        APP(ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                               OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS)));
        APP(ilist, INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                            opnd_create_base_disp(REG_ECX, REG_NULL, 0, 1, OPSZ_lea)));
        APP(ilist, INSTR_CREATE_mov_st(dcontext,
                               OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS),
                               opnd_create_reg(REG_ECX)));
# ifdef LINKCOUNT_64_BITS
        /*>>>    jecxz   carry                                            */
        /*>>>    jmp     nocarry                                          */
        /*>>>  carry:                                                     */
        /*>>>    movl    count_offs+4(%ebx == &linkstub), %ecx            */
        /*>>>    lea     1(%ecx), %ecx                                    */
        /*>>>    movl    %ecx, count_offs+4(%ebx == &linkstub), %ecx      */
        /*>>>  nocarry:                                                   */
        carry = INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                                    OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS));
        APP(ilist, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(carry)));
        APP(ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(next)));
        APP(ilist, carry);
        APP(ilist, INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                            opnd_create_base_disp(REG_ECX, REG_NULL, 0, 1, OPSZ_lea)));
        APP(ilist, INSTR_CREATE_mov_st(dcontext,
                               OPND_CREATE_MEM32(REG_EBX, LINKSTUB_COUNT_OFFS),
                               opnd_create_reg(REG_ECX)));
# endif
    }
}
#endif

/* When inline_ibl_head, this emits the inlined lookup for the exit stub.
 *   Only assumption is that xcx = effective address of indirect branch
 * Else, this emits the top of the shared lookup routine, which assumes:
 *   1) xbx = &linkstub
 *   2) xcx = effective address of indirect branch
 * Assumes that a jne_short is sufficient to reach miss_tgt.
 * Returns pointers to three instructions, for use in calculating offsets
 * and in pointing jmps inside the ibl head.
 * It's fine to pass NULL if you're not interested in them.
 */
static void
append_ibl_head(dcontext_t *dcontext, instrlist_t *ilist,
                ibl_code_t *ibl_code, patch_list_t *patch,
                instr_t **fragment_found, instr_t **compare_tag_inst,
                instr_t **post_eflags_save,
                opnd_t miss_tgt, bool miss_8bit,
                bool target_trace_table,
                bool inline_ibl_head)
{
    instr_t *mask, *table = NULL, *compare_tag = NULL, *after_linkcount;
    opnd_t mask_opnd;
    bool absolute = !ibl_code->thread_shared_routine;
    bool table_in_tls = SHARED_IB_TARGETS() &&
        (target_trace_table || SHARED_BB_ONLY_IB_TARGETS()) &&
        DYNAMO_OPTION(ibl_table_in_tls);
    uint hash_to_address_factor;
    /* Use TLS only for spilling app state -- registers & flags */
    bool only_spill_state_in_tls = !absolute && !table_in_tls;
    IF_X64(bool x86_to_x64_ibl_opt = ibl_code->x86_to_x64_mode &&
        DYNAMO_OPTION(x86_to_x64_ibl_opt);)

    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

#ifndef X64
    /* For x64 we need this after the cmp post-eflags entry; for x86, it's
     * needed before for thread-private eflags save.
     */
    if (only_spill_state_in_tls) {
        /* grab dcontext in XDI for thread shared routine */
        insert_shared_get_dcontext(dcontext, ilist, NULL, true/* save xdi to scratch */);
    }
#endif
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
        /* There are ways to generate IBL that doesn't touch the EFLAGS -- see
         * case 7169. We're not using any of those techniques, so we save the
         * flags.
         */
         insert_save_eflags(dcontext, ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                            absolute _IF_X64(x86_to_x64_ibl_opt));
    }
    /* PR 245832: x64 trace cmp saves flags so we need an entry point post-flags-save */
    if (post_eflags_save != NULL) {
        *post_eflags_save = INSTR_CREATE_label(dcontext);
        APP(ilist, *post_eflags_save);
    }
#ifdef X64
    /* See comments above */
    if (only_spill_state_in_tls) {
        /* grab dcontext in XDI for thread shared routine */
        insert_shared_get_dcontext(dcontext, ilist, NULL, true/* save xdi to scratch */);
    }
#endif
    if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
        after_linkcount = SAVE_TO_REG(dcontext, REG_XBX, REG_R10);
    } else if (inline_ibl_head || !DYNAMO_OPTION(indirect_stubs)) {
        /*>>>    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET                       */
        if (absolute)
            after_linkcount = SAVE_TO_DC(dcontext, REG_XBX, XBX_OFFSET);
        else
            after_linkcount = SAVE_TO_TLS(dcontext, REG_XBX, TLS_XBX_SLOT);
    } else {
        /* create scratch register: re-use xbx, it holds linkstub ptr,
         * don't need to restore it on hit!  save to **xdi** slot so as
         * to not overwrite linkstub ptr */
        /*>>>    SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET                       */
        if (absolute)
            after_linkcount = SAVE_TO_DC(dcontext, REG_XBX, XDI_OFFSET);
        else if (table_in_tls) /* xdx is the free slot for tls */
            after_linkcount = SAVE_TO_TLS(dcontext, REG_XBX, TLS_XDX_SLOT);
        else /* the xdx slot already holds %xdi so use the mcontext */
            after_linkcount = SAVE_TO_DC(dcontext, REG_XBX, XDI_OFFSET);
    }
#ifdef PROFILE_LINKCOUNT
    ASSERT_NOT_IMPLEMENTED(!inline_ibl_head);
    /* not supported for inline_ibl_head, so we can assume xbx==&linkstub
     * FIXME: this ignores whether fragment is trace or not --
     * everybody has the count field, we just end up incr-ing it for bbs that
     * nobody looks at
     */
    if (dynamo_options.profile_counts &&
        IS_IBL_TRACE(ibl_code->source_fragment_type)) {
        append_linkcount_incr(dcontext, ilist, true /* can clobber eflags */,
                              after_linkcount);
    }
#endif /* PROFILE_LINKCOUNT */
    APP(ilist, after_linkcount);
    if (ibl_code->thread_shared_routine && !DYNAMO_OPTION(private_ib_in_tls)) {
        /* copy app xcx currently in tls slot into mcontext slot, so that we
         * can work with both tls and mcontext prefixes
         * do not need this if using all-tls (private_ib_in_tls option)
         */
        /* xbx is now dead, just saved it */
#ifdef X64
        if (x86_to_x64_ibl_opt)
            APP(ilist, RESTORE_FROM_REG(dcontext, REG_XBX, REG_R9));
        else
#endif
            APP(ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, MANGLE_XCX_SPILL_SLOT));
        APP(ilist, SAVE_TO_DC(dcontext, REG_XBX, XCX_OFFSET));
    }
    /* make a copy of the tag for hashing
     * keep original in xbx, hash will be in xcx
     *>>>    mov     %xcx,%xbx                                       */
    APP(ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XBX),
                                   opnd_create_reg(REG_XCX)));

    if (only_spill_state_in_tls) {
        /* grab the per_thread_t into XDI - can't use SAVE_TO_DC after this */
        /* >>> mov  %xdi, fragment_field(%xdi) */
        /*   TODO:  make this an 8bit offset, currently it is a 32bit one
             8b bf 94 00 00 00    mov    0x94(%xdi) -> %xdi
        */
        APP(ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XDI),
                                       OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR,
                                                     FRAGMENT_FIELD_OFFSET)));
        /* TODO: should have a flag that SAVE_TO_DC can ASSERT(valid_DC_in_reg) */
    }
    /* hash function = (tag & mask) */
    if (!absolute && table_in_tls) {
        /* mask is in tls */
        mask_opnd = OPND_TLS_FIELD(TLS_MASK_SLOT(ibl_code->branch_type));
    } else if (!absolute) {
        ASSERT(only_spill_state_in_tls);
        /* this is an offset in per_thread_t so should fit in 32 bits */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int
                      (GET_IB_FTABLE(ibl_code, target_trace_table, hash_mask))));
        mask_opnd =
            opnd_create_base_disp(REG_XDI, REG_NULL, 0,
                                  (int) GET_IB_FTABLE(ibl_code, target_trace_table,
                                                      hash_mask), OPSZ_PTR);
    } else {
        /* mask not created yet, use 0x3fff for now
         * if we did need to support an immediate for x64, we could
         * just use the lower 32 bits and let them be sign-extended.
         *>>>    andl    $0x3fff,%xcx                                    */
        mask_opnd = opnd_create_immed_int(0x3fff, OPSZ_4);
    }
    mask = INSTR_CREATE_and(dcontext, opnd_create_reg(REG_XCX), mask_opnd);
    APP(ilist, mask);
    if (absolute) {
        add_patch_entry(patch, mask, PATCH_PER_THREAD,
                        (ptr_uint_t) GET_IB_FTABLE(ibl_code, target_trace_table,
                                                   hash_mask));
    }

    /* load from lookup hash table tag and start_pc */
    /* simply   cmp     BOGUS_HASH_TABLE(,%xcx,8),%xcx   # tag */
    /*          jne     next_fragment    */
    /*          jmp     *FRAGMENT_START_PC_OFFS(4,%xdx,3)# pc */
    /* or better yet */
    /*  lea     BOGUS_HASH_TABLE(,%xcx,8),%xcx   # xcx  = &lookuptable[hash]    */
    /*  cmp     HASHLOOKUP_TAG_OFFS(%xcx),%xbx   # tag           _cache line 1_ */
    /*  jne     next_fragment    */
    /*  jmp     *HASHLOOKUP_START_PC_OFFS(%xcx)  # pc            _cache line 1_ */

    /* *>>>    lea    BOGUS_HASH_TABLE(,%xcx,8),%xcx
       not created yet, use 0 */

    if (only_spill_state_in_tls) {
        /* grab the corresponding table or lookuptable for trace into XDI */
        /* >>> mov  %xdi, lookuptable(%xdi) */
        /* 8b 7f 40             mov    0x40(%xdi) -> %xdi
        */

        instr_t *table_in_xdi;
        /* this is an offset in per_thread_t so should fit in 32 bits */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int
                      (GET_IB_FTABLE(ibl_code, target_trace_table, table))));
        table_in_xdi =
            INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XDI),
                                opnd_create_base_disp(REG_XDI, REG_NULL, 0,
                                                      (int)
                                                      GET_IB_FTABLE(ibl_code,
                                                                    target_trace_table,
                                                                    table),
                                                      OPSZ_PTR));
        /* lookuptable can still be reloaded from XDI later at sentinel_check */
        APP(ilist, table_in_xdi);
    }

    if (absolute) {
        ASSERT(sizeof(fragment_entry_t) == 8); /* x64 not supported */
        if (HASHTABLE_IBL_OFFSET(ibl_code->branch_type) <= IBL_HASH_FUNC_OFFSET_MAX) {
            /* multiply by 16,8,4,2 or 1 respectively when we offset 0,1,2,3,4 bits */
            IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                          (sizeof(fragment_entry_t) /
                           (size_t)(1 << HASHTABLE_IBL_OFFSET(ibl_code->branch_type)))));
            hash_to_address_factor = (uint) sizeof(fragment_entry_t) /
                (1 << HASHTABLE_IBL_OFFSET(ibl_code->branch_type));
        } else {
            /* FIXME: we'll need to shift right a few more bits */
            /* >>>   shrl  factor-3, %xcx */
            ASSERT_NOT_IMPLEMENTED(false);
            hash_to_address_factor = 1;
        }
        /* FIXME: there is no good way to ASSERT that the table we're looking up
         * is using the correct hash_mask_offset
         */

        /* FIXME: case 4893: three ADD's are faster than one LEA - if IBL
         * head is not inlined we may want to try that advice
         */
        /* FIXME: case 4893 when hash_mask_offset==3 we can use a better encoding
         * since we don't need an index register we can switch to a non-SIB encoding
         * so that instead of 7 bytes we have 6 byte encoding going through the fast decoder
         * 8d 0c 0d 5039721c   lea     xcx,[1c723950+xcx]   ; currently
         * 8d 89 __ 5039721c   lea     xcx,[xcx+0x1c723950] ; shorter
         */
        table = INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XCX),
                                 opnd_create_base_disp(REG_NULL, REG_XCX,
                                                       hash_to_address_factor, 0, OPSZ_lea));
        add_patch_entry(patch, table, PATCH_PER_THREAD,
                        GET_IB_FTABLE(ibl_code, target_trace_table, table));
        APP(ilist, table);
    } else { /* !absolute && (table_in_tls  || only_spill_state_in_tls) */
        /* we have the base added separately already, so we skip the lea and
         * use faster and smaller add sequences for our shift
         */
        uint i;
        opnd_t table_opnd;

        ASSERT_NOT_IMPLEMENTED(HASHTABLE_IBL_OFFSET(ibl_code->branch_type) <=
                               IBL_HASH_FUNC_OFFSET_MAX);
        /* are 4 adds faster than 1 lea, for x64? */
        for (i = IBL_HASH_FUNC_OFFSET_MAX;
             i > HASHTABLE_IBL_OFFSET(ibl_code->branch_type); i--) {
            APP(ilist, INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XCX),
                                        opnd_create_reg(REG_XCX)));
        }
        /* we separately add the lookuptable base since we'd need an extra register
         * to do it in combination with the shift:
         *    add   fs:lookuptable,%xcx -> %xcx
         * or if the table addr is in %xdi:
         *    add   %xdi,%xcx -> %xcx
         */
        table_opnd = table_in_tls ?
            OPND_TLS_FIELD(TLS_TABLE_SLOT(ibl_code->branch_type)) /* addr in TLS */:
            opnd_create_reg(REG_XDI) /* addr in %xdi */;
        APP(ilist,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XCX), table_opnd));
    }

    /* compare tags, empty slot is not 0, instead is a constant frag w/ tag 0
     *>>>    cmp     HASHLOOKUP_TAG_OFFS(%xcx),%xbx                    */
    compare_tag = INSTR_CREATE_cmp(dcontext,
                                   OPND_CREATE_MEMPTR(REG_XCX, HASHLOOKUP_TAG_OFFS),
                                   opnd_create_reg(REG_XBX));
    APP(ilist, compare_tag);

    /*>>>    jne     next_fragment                                   */
    if (miss_8bit)
        APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne_short, miss_tgt));
    else
        APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne, miss_tgt));

#ifdef X64
    if (ibl_code->x86_mode) {
        /* Currently we're using the x64 table, so we have to ensure the top
         * bits are 0 before we declare it a match (xref PR 283895).
         */
        APP(ilist, INSTR_CREATE_cmp(dcontext,
                                    OPND_CREATE_MEM32(REG_XCX, HASHLOOKUP_TAG_OFFS+4),
                                    OPND_CREATE_INT32(0)));
        if (miss_8bit)
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne_short, miss_tgt));
        else
            APP(ilist, INSTR_CREATE_jcc(dcontext, OP_jne, miss_tgt));
    }
#endif

#define HEAD_START_PC_OFFS HASHLOOKUP_START_PC_OFFS
    append_ibl_found(dcontext, ilist, ibl_code, patch, HEAD_START_PC_OFFS,
                     false, only_spill_state_in_tls,
                     target_trace_table ?
                     DYNAMO_OPTION(trace_single_restore_prefix) :
                     DYNAMO_OPTION(bb_single_restore_prefix),
                     fragment_found);
#undef HEAD_START_PC_OFFS

    if (compare_tag_inst != NULL)
        *compare_tag_inst = compare_tag;
}

/* create the inlined ibl exit stub template
 *
hit path (shared_syscall remains as before):
  if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
  | 5   movl  %eax,eax_OFFSET
  | 1   lahf
  | 3   seto  %al
  }
    6   movl  %ebx, ebx_offs(&dcontext)
    2   movl  %ecx,%ebx                 # tag in ecx, hash will be in ebx
    6   andl  $0x3fff,%ecx              # hash the tag
    7   movl  ftable(,%ecx,4),%ecx      # ecx = ftable[hash]
        # empty slot is not 0, instead is a constant frag w/ tag 0
    2   cmpl  FRAGMENT_TAG_OFFS(%ecx),%ebx
    2   jne   miss # if !DYNAMO_OPTION(indirect_stubs), jne ibl
    6   movl  ebx_offs(&dcontext),%ebx
    3   jmp   *FRAGMENT_START_PC_OFFS(%ecx)
unlinked entry point into stub:
 if (!DYNAMO_OPTION(indirect_stubs)) {
     5  jmp   unlinked_ib_lookup  # we can eliminate this if we store stub pc
 } else {
  if (DYNAMO_OPTION(atomic_inlined_linking)) {
        # duplicate miss path so linking can be atomic
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   unlinked_ib_lookup
  } else {
        # set flag in ecx (bottom byte = 0x1) so that unlinked path can
        # detect race condition during unlinking
    6   movl  %ecx, ebx_offs(&dcontext)
    2   movb  $0x1, %ecx
  }
miss:
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   indirect_branch_lookup/(if !atomic_inlined_linking)unlinked_ib_lookup
 }
*/
byte *
emit_inline_ibl_stub(dcontext_t *dcontext, byte *pc,
                     ibl_code_t *ibl_code, bool target_trace_table)
{
    /* careful -- we're called in middle of setting up code fields, so don't go
     * reading any without making sure they're initialized first
     */
    instrlist_t ilist;
    instr_t *miss = NULL, *unlink, *after_unlink = NULL;

    patch_list_t *patch =  &ibl_code->ibl_stub_patch;
    byte *unlinked_ibl_pc = ibl_code->unlinked_ibl_entry;
    byte *linked_ibl_pc = ibl_code->indirect_branch_lookup_routine;

    bool absolute = !ibl_code->thread_shared_routine;

    /* PR 248207: haven't updated the inlining to be x64-compliant yet.
     * Keep in mind PR 257963: trace inline cmp needs separate entry. */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    ibl_code->inline_ibl_stub_template = pc;
    ibl_code->ibl_head_is_inlined = true;

    /* initialize the ilist and the patch list */
    instrlist_init(&ilist);
    /* FIXME: for !absolute need to optimize to PATCH_TYPE_INDIRECT_FS */
    init_patch_list(patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_XDI);

    /*
        <head>
unlinked entry point into stub:
  if (DYNAMO_OPTION(atomic_inlined_linking)) {
        # duplicate miss path so linking can be atomic
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   unlinked_ib_lookup
  } else {
        # set flag in ecx (bottom byte = 0x1) so that unlinked path can
        # detect race condition during unlinking
    6   movl  %ecx, ebx_offs(&dcontext)
    2   movb  $0x1, %ecx
  }
miss:
    10  movl  &linkstub, edi_offs(&dcontext)
    5   jmp   indirect_branch_lookup/(if !atomic_inlined_linking)unlinked_ib_lookup
    */

    if (DYNAMO_OPTION(indirect_stubs)) {
        if (absolute) {
            miss = INSTR_CREATE_mov_st(dcontext,
                                       opnd_create_dcontext_field(dcontext, XDI_OFFSET),
                                       OPND_CREATE_INT32(0));
        } else {
            miss = INSTR_CREATE_mov_st(dcontext,
                                       OPND_TLS_FIELD(TLS_XDX_SLOT),
                                       OPND_CREATE_INT32(0));
        }
        append_ibl_head(dcontext, &ilist, ibl_code, patch, NULL, NULL, NULL,
                        opnd_create_instr(miss), true/*miss can have 8-bit offs*/,
                        target_trace_table, true /* inline of course */ );

        /*>>>    SAVE_TO_UPCONTEXT %ebx,ebx_OFFSET                       */
        /*>>>    SAVE_TO_UPCONTEXT &linkstub,edx_OFFSET                  */
        /*>>>    jmp     unlinked_ib_lookup                              */
        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            if (absolute) {
                unlink = SAVE_TO_DC(dcontext, REG_EBX, XBX_OFFSET);
                after_unlink =
                    INSTR_CREATE_mov_st(dcontext,
                                        opnd_create_dcontext_field(dcontext,
                                                                   XDI_OFFSET),
                                        OPND_CREATE_INT32(0));
            } else {
                unlink = SAVE_TO_TLS(dcontext, REG_EBX, TLS_XBX_SLOT);
                after_unlink = INSTR_CREATE_mov_st(dcontext,
                                                   OPND_TLS_FIELD(TLS_XDX_SLOT),
                                                   OPND_CREATE_INT32(0));
            }
            APP(&ilist, unlink);
            APP(&ilist, after_unlink);
            APP(&ilist, INSTR_CREATE_jmp(dcontext,
                                         opnd_create_pc(unlinked_ibl_pc)));
        } else {
            if (absolute)
                unlink = SAVE_TO_DC(dcontext, REG_ECX, XBX_OFFSET);
            else
                unlink = SAVE_TO_TLS(dcontext, REG_ECX, TLS_XBX_SLOT);
            APP(&ilist, unlink);
            APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_CL),
                                             OPND_CREATE_INT8(1)));
        }
        APP(&ilist, miss);
        APP(&ilist,
            INSTR_CREATE_jmp(dcontext,
                             opnd_create_pc(DYNAMO_OPTION(atomic_inlined_linking) ?
                                            linked_ibl_pc : unlinked_ibl_pc)));

        add_patch_marker(patch, unlink, PATCH_UINT_SIZED /* pc relative */,
                         0 /* beginning of instruction */,
                         (ptr_uint_t*)&ibl_code->inline_unlink_offs);

        if (DYNAMO_OPTION(atomic_inlined_linking)) {
            add_patch_marker(patch, after_unlink, PATCH_UINT_SIZED /* pc relative */,
                             -4 /* grab last 4 bytes of instructions */,
                             (ptr_uint_t*)&ibl_code->inline_linkstub_second_offs);
            add_patch_marker(patch, instr_get_prev(miss),
                             PATCH_UINT_SIZED /* pc relative */,
                             1 /* skip jmp opcode */,
                             (ptr_uint_t*)&ibl_code->inline_unlinkedjmp_offs);
        }
        add_patch_marker(patch, miss, PATCH_UINT_SIZED /* pc relative */,
                         -4 /* grab offsets that are last 4 bytes of instructions */,
                         (ptr_uint_t*)&ibl_code->inline_linkstub_first_offs);
        add_patch_marker(patch, instrlist_last(&ilist),
                         PATCH_UINT_SIZED /* pc relative */,
                         1 /* skip jmp opcode */,
                         (ptr_uint_t*)&ibl_code->inline_linkedjmp_offs);
    } else {
        instr_t *cmp;
        append_ibl_head(dcontext, &ilist, ibl_code, patch, NULL, &cmp, NULL,
                        opnd_create_pc(linked_ibl_pc), false/*miss needs 32-bit offs*/,
                        target_trace_table, true /* inline of course */ );
        /* FIXME: we'd like to not have this jmp at all and instead have the cti
         * go to the inlined stub when linked and straight to the unlinked ibl
         * entry when unlinked but we haven't put in the support in the link
         * routines (they all assume they can find the unlinked from the current
         * target in a certain manner).
         */
        unlink = INSTR_CREATE_jmp(dcontext, opnd_create_pc(unlinked_ibl_pc));
        APP(&ilist, unlink);
        /* FIXME: w/ private traces and htable stats we have a patch entry
         * inserted inside app_ibl_head (in append_ibl_found) at a later instr
         * than the miss instr.  To fix, we must either put the miss patch point
         * in the middle of the array and shift it over to keep it sorted, or
         * enable patch-encode to handle out-of-order entries (we could mark
         * this with a flag).
         */
#ifdef HASHTABLE_STATISTICS
        ASSERT_NOT_IMPLEMENTED(!absolute || !INTERNAL_OPTION(hashtable_ibl_stats));
#endif
        /* FIXME: cleaner to have append_ibl_head pass back miss instr */
        add_patch_marker(patch, instr_get_next(cmp), PATCH_UINT_SIZED /* pc relative */,
                         2 /* skip jne opcode */,
                         (ptr_uint_t*)&ibl_code->inline_linkedjmp_offs);
        /* FIXME: we would add a patch for inline_unlinkedjmp_offs at unlink+1,
         * but encode_with_patch_list asserts, wanting 1 patch per instr, in order
         */
        add_patch_marker(patch, unlink, PATCH_UINT_SIZED /* pc relative */,
                         0 /* beginning of instruction */,
                         (ptr_uint_t*)&ibl_code->inline_unlink_offs);
    }

    ibl_code->inline_stub_length = encode_with_patch_list(dcontext, patch, &ilist, pc);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
    return pc + ibl_code->inline_stub_length;
}


/* FIXME: case 5232 where this should really be smart -
 * for now always using jmp rel32 with statistics
 *
 * Use with caution where jmp_short would really work in release - no
 * ASSERTs to help you.
 */
#ifdef HASHTABLE_STATISTICS
#  define INSTR_CREATE_jmp_smart INSTR_CREATE_jmp
#else
#  define INSTR_CREATE_jmp_smart INSTR_CREATE_jmp_short
#endif

/*
# indirect_branch_lookup
#.If the lookup succeeds, control jumps to the fcache target; otherwise
# it sets up for and jumps to fcache_return.

# when we unlink an indirect branch we go through the cleanup part of
# this lookup routine that takes us straight to fcache_return.

# We assume dynamo is NOT in trace creation mode (which would require
# going back to dynamo here).  We assume that when a fragment is
# unlinked its indirect branch exit stubs are redirected to the
# unlinked_* labels below.  Note that even if you did come in here in
# trace creation mode, and we didn't go back to dynamo here, the
# current trace would have ended now (b/c next fragment is a trace),
# so we'd end up possibly adding erroneous fragments to the end of
# the trace but the indirect branch check would ensure they were never
# executed.

# N.B.: a number of optimizations of the miss path are possible by making
# it separate from the unlink path
*/
/* Must have a valid fcache return pc prior to calling this function! */
byte *
emit_indirect_branch_lookup(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc,
                            bool target_trace_table,
                            bool inline_ibl_head,
                            ibl_code_t *ibl_code /* IN/OUT */)
{
    instrlist_t ilist;
    instr_t *fragment_not_found, *unlinked = INSTR_CREATE_label(dcontext);
    instr_t *target_delete_entry;
    instr_t *post_linkcount;
    patch_list_t *patch = &ibl_code->ibl_patch;
    bool absolute = !ibl_code->thread_shared_routine;
    bool table_in_tls = SHARED_IB_TARGETS() &&
        (target_trace_table || SHARED_BB_ONLY_IB_TARGETS()) &&
        DYNAMO_OPTION(ibl_table_in_tls);
    /* Use TLS only for spilling app state -- registers & flags */
    bool only_spill_state_in_tls = !absolute && !table_in_tls;
#ifdef HASHTABLE_STATISTICS
    /* save app XDI since inc routine uses it */
    bool save_xdi = !absolute && table_in_tls;
#endif
    instr_t *fragment_found;
    instr_t *compare_tag = NULL;
    instr_t *sentinel_check;
    /* for IBL_COARSE_SHARED and !DYNAMO_OPTION(indirect_stubs) */
    const linkstub_t *linkstub = NULL;
    IF_X64(bool x86_to_x64_ibl_opt = ibl_code->x86_to_x64_mode &&
        DYNAMO_OPTION(x86_to_x64_ibl_opt);)

    instr_t *next_fragment_nochasing =
        INSTR_CREATE_cmp(dcontext,
                         OPND_CREATE_MEMPTR(REG_XCX, HASHLOOKUP_TAG_OFFS),
                         OPND_CREATE_INT8(0));

    /* no support for absolute addresses on x64: we always use tls/reg */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));

    if (ibl_code->source_fragment_type == IBL_COARSE_SHARED ||
        !DYNAMO_OPTION(indirect_stubs)) {
        linkstub =
            get_ibl_sourceless_linkstub(ibltype_to_linktype(ibl_code->branch_type),
                                        IBL_FRAG_FLAGS(ibl_code));
    }

    /* When the target_delete_entry is reached, all registers contain
     * app state, except for those restored in a prefix. We need to massage
     * the state so that it looks like the fragment_not_found -- IBL miss -
     * path, so we need to restore %xbx. See more on the target_delete_entry
     * below, where the instr is added to the ilist.
     */
#ifdef X64
    if (x86_to_x64_ibl_opt) {
        target_delete_entry = SAVE_TO_REG(dcontext, REG_XBX, REG_R10);
    } else {
#endif
        target_delete_entry = absolute ?
            instr_create_save_to_dcontext(dcontext, REG_XBX, XBX_OFFSET) :
            SAVE_TO_TLS(dcontext, REG_XBX, INDIRECT_STUB_SPILL_SLOT);
#ifdef X64
    }
#endif

    fragment_not_found = INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                             opnd_create_reg(REG_XBX));

    /* initialize the ilist */
    instrlist_init(&ilist);
    init_patch_list(patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_XDI);

    LOG(THREAD, LOG_EMIT, 3,
        "emit_indirect_branch_lookup: pc="PFX" fcache_return_pc="PFX"\n"
        "target_trace_table=%d inline_ibl_head=%d absolute=%d\n",
        pc, fcache_return_pc, target_trace_table, inline_ibl_head, absolute);

    if (inline_ibl_head) {
        /* entry point is next_fragment, expects
         * 1) xbx = effective address of indirect branch
         * 2) xcx = &fragment
         * 3) xdx_slot = &linkstub
         */
    } else {
        /* entry point: expects
         * 1) xbx = &linkstub if DYNAMO_OPTION(indirect_stubs),
         *          or src tag if DYNAMO_OPTION(coarse_units)
         * 2) xcx = effective address of indirect branch
         */
#ifdef X64
        instr_t *trace_cmp_entry;
#endif
        append_ibl_head(dcontext, &ilist, ibl_code, patch, &fragment_found, &compare_tag,
                        IF_X64_ELSE(&trace_cmp_entry, NULL),
                        opnd_create_instr(next_fragment_nochasing),
                        true/*miss can have 8-bit offs*/, target_trace_table,
                        inline_ibl_head);
#ifdef X64
        if (IS_IBL_TRACE(ibl_code->source_fragment_type) &&
            !GENCODE_IS_X86(code->gencode_mode)) {
            /* if -unsafe_ignore_eflags_ibl this will equal regular entry */
            add_patch_marker(patch, trace_cmp_entry, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t*)&ibl_code->trace_cmp_entry);
        }
#endif
    }
    /* next_fragment_nochasing: */
    /*>>>    cmp     $0, HASHLOOKUP_TAG_OFFS(%xcx)                   */
    APP(&ilist, next_fragment_nochasing);

    /* forward reference to sentinel_check */
    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* sentinel_check: */
        /* check if at table end sentinel */
        /* One solution would be to compare xcx to
         * &lookuptable[ftable->capacity-1] (sentinel) while it would
         * work great for thread private IBL routines where we can
         * hardcode the address.
         *  >>>a)   cmp     %xcx, HASHLOOKUP_SENTINEL_ADDR
                                  ;; &lookuptable[ftable->capacity-1] (sentinel)
         * For shared routines currently we'd need to walk a few
         * pointers - we could just put that one TLS to avoid pointer
         * chasing.  Yet if we are to have even one extra memory load
         * anyways, it is easier to just store a special start_pc to
         * compare instead
         *  >>>b)   cmp     4x8(%xcx), HASHLOOKUP_SENTINEL_PC
         * Where the expectation is that null_fragment=(0,0) while
         * sentinel_fragment=(0,1) For simplicity we just use b) even
         * in private IBL routines.
         */
        /* we can use 8-bit immed, will be sign-expanded before cmp */
        ASSERT((int)(ptr_int_t)HASHLOOKUP_SENTINEL_START_PC <= INT8_MAX &&
               (int)(ptr_int_t)HASHLOOKUP_SENTINEL_START_PC >= INT8_MIN);
        sentinel_check = INSTR_CREATE_cmp(dcontext,
                                          OPND_CREATE_MEMPTR(REG_XCX, HASHLOOKUP_START_PC_OFFS),
                                          OPND_CREATE_INT8((int)(ptr_int_t)
                                                           HASHLOOKUP_SENTINEL_START_PC));
    } else {
        /* sentinel handled in C code
         * just exit back to dispatch
         */
        sentinel_check = fragment_not_found;
    }

    /*>>>    je      sentinel_check                                  */
    /* FIXME: je_short ends up not reaching target for shared inline! */
    APP(&ilist,
        INSTR_CREATE_jcc(dcontext, OP_je_short, opnd_create_instr(sentinel_check)));

    /* For open address hashing xcx = &lookuptable[h]; to get &lt[h+1] just add 8x16
     *   add xcx, 8x16  # no wrap around check, instead rely on a nulltag sentinel entry
     * alternative method of rehashing xbx+4x8 or without checks is also not efficient
     */
# ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats)) {
        if (save_xdi)
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
        append_increment_counter(dcontext, &ilist, ibl_code, patch,
                                 REG_NULL,
                                 HASHLOOKUP_STAT_OFFS(collision),
                                 REG_NULL); /* no registers dead */
        if (save_xdi)
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
    }
# endif
    APP(&ilist,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_XCX),
                         opnd_create_base_disp(REG_XCX, REG_NULL, 0,
                                               sizeof(fragment_entry_t), OPSZ_lea)));

    if (inline_ibl_head) {
        compare_tag = INSTR_CREATE_cmp(dcontext,
                                       OPND_CREATE_MEMPTR(REG_XCX, HASHLOOKUP_TAG_OFFS),
                                       opnd_create_reg(REG_XBX));
        APP(&ilist, compare_tag);

        /*  TODO: check whether the static predictor can help here */
        /*  P4OG:2-18 "Use prefix 3E (DS) for taken and 2E (CS) for not taken cbr" (DS == PREFIX_DATA) */
        APP(&ilist,
            INSTR_CREATE_jcc(dcontext, OP_jne_short, opnd_create_instr(next_fragment_nochasing)));

        append_ibl_found(dcontext, &ilist, ibl_code, patch,
                         HASHLOOKUP_START_PC_OFFS, true, only_spill_state_in_tls,
                         target_trace_table ?
                         DYNAMO_OPTION(trace_single_restore_prefix) :
                         DYNAMO_OPTION(bb_single_restore_prefix),
                         NULL);
    } else {
        /* case 5232: use INSTR_CREATE_jmp_smart, since release builds can use a short jump */
        APP(&ilist, INSTR_CREATE_jmp_smart(dcontext, opnd_create_instr(compare_tag)));
    }

    if (INTERNAL_OPTION(ibl_sentinel_check)) {
        /* check if at table end sentinel */
        APP(&ilist, sentinel_check);

        /* not found, if not at end of table sentinel fragment  */
        /*>>>    jne      fragment_not_found                        */
        APP(&ilist,
            INSTR_CREATE_jcc(dcontext, OP_jne_short, opnd_create_instr(fragment_not_found)));

# ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            if (save_xdi)
                APP(&ilist, SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            append_increment_counter(dcontext, &ilist, ibl_code, patch,
                                     REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(overwrap),
                                     REG_NULL); /* no registers dead */
            if (save_xdi) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            }
        }
# endif

        /* should overwrap to beginning of table, if at end of table sentinel */
        /* for private table */
        /*>>>    mov     BOGUS_HASH_TABLE -> %xcx  ; &lookuptable[0]   */

        /* for shared table - table address should still be preserved in XDI
         *        mov    %xdi -> %xcx        ; xdi == &lookuptable[0]
         */
        if (absolute) {
            /* lookuptable is a patchable immediate */
            enum {BOGUS_HASH_TABLE = 0xabcdabcd};
            instr_t *table = INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XCX),
                                                OPND_CREATE_INT32(BOGUS_HASH_TABLE));
            add_patch_entry(patch, table, PATCH_PER_THREAD,
                            GET_IB_FTABLE(ibl_code, target_trace_table, table));
            APP(&ilist, table);
        } else if (table_in_tls) {
            /* grab lookuptable from tls */
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XCX,
                                         TLS_TABLE_SLOT(ibl_code->branch_type)));
        } else {
# ifdef HASHTABLE_STATISTICS
            if (INTERNAL_OPTION(hashtable_ibl_stats)) {
                /* The hash stats inc routine clobbers XDI so we need to reload
                 * it and then reload per_thread_t* and then the table*. */
                insert_shared_get_dcontext(dcontext, &ilist, NULL, false);
                APP(&ilist,
                    INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XDI),
                                        OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR,
                                                      FRAGMENT_FIELD_OFFSET)));
                /* We could load directly into XCX but since hash stats are on,
                 * we assume that this isn't a performance-sensitive run and
                 * opt for code simplicity by rematerializing XDI.
                 */
                /* this is an offset in per_thread_t so should fit in 32 bits */
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int
                              (GET_IB_FTABLE(ibl_code, target_trace_table, table))));
                APP(&ilist,
                    INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XDI),
                                        opnd_create_base_disp
                                        (REG_XDI, REG_NULL, 0,
                                         (int) GET_IB_FTABLE(ibl_code,
                                                             target_trace_table, table),
                                         OPSZ_PTR)));
            }
# endif
            /* XDI should still point to lookuptable[0] */
            APP(&ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                            opnd_create_reg(REG_XDI)));
        }

        /*>>>    jmp    compare_tag                                 */
        /* FIXME: should fit in a jmp_short for release builds */
        /* case 5232: use INSTR_CREATE_jmp_smart here */
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(compare_tag)));
    }

    /* there is no fall-through through here: we insert separate entry points here */

#ifdef X64
    if (IS_IBL_TRACE(ibl_code->source_fragment_type) &&
        !GENCODE_IS_X86(code->gencode_mode)) {
        if (DYNAMO_OPTION(unsafe_ignore_eflags_trace)) { /*==unsafe_ignore_eflags_ibl */
            /* trace_cmp link and unlink entries are identical to regular */
            add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t*)&ibl_code->trace_cmp_unlinked);
        } else if (inline_ibl_head) {
            /* For inlining we can't reuse the eflags restore below, so
             * we insert our own
             */
            instr_t *trace_cmp_unlinked = INSTR_CREATE_label(dcontext);
            APP(&ilist, trace_cmp_unlinked);
            add_patch_marker(patch, trace_cmp_unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t*)&ibl_code->trace_cmp_unlinked);
            insert_restore_eflags(dcontext, &ilist, NULL, 0, true/*tls*/, false/*!abs*/,
                                  x86_to_x64_ibl_opt);
            APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(unlinked)));
        }
    }
#endif

    /****************************************************************************
     * target_delete_entry
     */

    /*>>>  target_delete_entry:                                     */
    /* This entry point aids in atomic hashtable deletion. A        */
    /* hashtable entry's start_pc_fragment is redirected to here    */
    /* when the entry's fragment is being deleted. It's a prefix to */
    /* the fragment_not_found path and so leads to a cache exit.    */
    /* The regular not_found path skips over these instructions,    */
    /* directly to the fragment_not_found entry.                    */
    /*                                                              */
    /* If coming from a shared ibl, xbx is NOT in the mcontext, which our
     * miss path restore assumes -- so we put it there now.  If coming from
     * a private ibl or a no-prefix-target ibl, this is simply a redundant
     * store.  Xref case 4649.
     */
    /*>>>    SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET                       */
    APP(&ilist, target_delete_entry);

    if (linkstub == NULL) {
        /* If coming from an inlined ibl, the linkstub was not stored, so
         * we use a special linkstub_t in the last_exit "slot" (xdi / tls xdx) for any
         * source (xref case 4635).
         * Rare enough that should be ok, and everyone, including trace building,
         * can handle it.  Although w/ an unknown last_exit the trace builder has to
         * assume the final exit was taken, that's only bad when ending in a cbr,
         * and when that's the case won't end up here (have to have -inline_bb_ibl
         * to get here, since we only add bbs to traces).
         */
#ifdef X64
        /* xbx is now dead so we can use it */
        APP(&ilist, INSTR_CREATE_mov_imm
            (dcontext, opnd_create_reg(REG_XBX),
             OPND_CREATE_INTPTR((ptr_int_t)get_ibl_deleted_linkstub())));
#endif
        if (absolute) {
            APP(&ilist, instr_create_save_immed_to_dcontext
                (dcontext, (int)(ptr_int_t) get_ibl_deleted_linkstub(), XDI_OFFSET));
        } else if (table_in_tls) {
            APP(&ilist, INSTR_CREATE_mov_st
                (dcontext, OPND_TLS_FIELD(TLS_XDX_SLOT),
                 IF_X64_ELSE(opnd_create_reg(REG_XBX),
                             OPND_CREATE_INTPTR((ptr_int_t)get_ibl_deleted_linkstub()))));
        } else {
            /* doesn't touch xbx */
            insert_shared_get_dcontext(dcontext, &ilist, NULL, true);
            APP(&ilist, INSTR_CREATE_mov_st
                (dcontext, OPND_DC_FIELD(absolute, dcontext, OPSZ_PTR, XDI_OFFSET),
                 IF_X64_ELSE(opnd_create_reg(REG_XBX),
                             OPND_CREATE_INTPTR((ptr_int_t)get_ibl_deleted_linkstub()))));
            insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
        }
    } /* else later will fill in fake linkstub anyway.
       * FIXME: for -no_indirect_stubs, is this source of add_ibl curiosities on IIS?
       * but one at least was a post-syscall!
       */

    /* load the tag value from the table ptr in xcx into xbx, so    */
    /* that it gets shuffled into xcx by the following instruction  */
    /*>>>    mov     (%xcx), %xbx                                   */
    if (!ibl_use_target_prefix(ibl_code)) {
        /* case 9688: for no-prefix-target ibl we stored the tag in xax slot
         * in the hit path.  we also restored eflags already.
         */
        if (absolute) {
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XAX_OFFSET));
        } else {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, DIRECT_STUB_SPILL_SLOT));
        }
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            /* save flags so we can re-use miss path below */
            insert_save_eflags(dcontext, &ilist, NULL, 0,
                               IBL_EFLAGS_IN_TLS(), absolute _IF_X64(x86_to_x64_ibl_opt));
        }
    } else {
        APP(&ilist,
            INSTR_CREATE_mov_ld(dcontext,
                                opnd_create_reg(REG_XBX),
                                OPND_CREATE_MEMPTR(REG_XCX, FRAGMENT_TAG_OFFS)));
    }

    /* Add to the patch list right away; hashtable stats could be added
     * further later, so if we don't add now the patch ordering becomes
     * confused.
     */
    add_patch_marker(patch, target_delete_entry, PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */,
                     (ptr_uint_t*)&ibl_code->target_delete_entry);

    /****************************************************************************
     * fragment_not_found
     */

    /* put target back in xcx to match regular unlinked path,
     * the unlinked inlined indirect branch race condition case
     * also comes here (if !atomic_inlined_linking) */
    /*>>>  fragment_not_found:
     *>>>    mov     %xbx, %xcx                                      */
    APP(&ilist, fragment_not_found);

    /* This counter will also get the unlink inline indirect branch race
     * condition cases (if !atomic_inlined_linking), but that should almost
     * never happen so don't worry about it screwing up the count */
#ifdef HASHTABLE_STATISTICS
    if (INTERNAL_OPTION(hashtable_ibl_stats)) {
        if (save_xdi)
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
        append_increment_counter(dcontext, &ilist, ibl_code, patch,
                                 REG_NULL,
                                 HASHLOOKUP_STAT_OFFS(miss),
                                 REG_XBX); /* xbx dead */
        if (save_xdi)
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
    }
#endif
    if (only_spill_state_in_tls) {
        /* get dcontext in register (xdi) */
        insert_shared_get_dcontext(dcontext, &ilist, NULL, false /* xdi is dead */);
    }

    /* for inlining we must restore flags prior to xbx restore: but when not
     * inlining we reverse them so that trace_cmp entry can come in at the restore */
    if (inline_ibl_head) {
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            /* restore flags + xax (which we only need so save below works) */
            insert_restore_eflags(dcontext, &ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                                  absolute _IF_X64(x86_to_x64_ibl_opt));
        }
        APP(&ilist, unlinked);
    }
    if (DYNAMO_OPTION(indirect_stubs)) {
        /* restore scratch xbx from **xdi / tls xdx ** offset */
        /*>>>    RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xbx                  */
        if (absolute) {
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XDI_OFFSET));
        } else if (table_in_tls) {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, TLS_XDX_SLOT));
        } else {
            ASSERT(only_spill_state_in_tls);
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XDI_OFFSET));
        }
    } else {
        /* restore xbx */
        if (IF_X64_ELSE(x86_to_x64_ibl_opt, false))
            APP(&ilist, RESTORE_FROM_REG(dcontext, REG_XBX, REG_R10));
        else if (absolute)
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XBX_OFFSET));
        else
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XBX, INDIRECT_STUB_SPILL_SLOT));
    }

    if (only_spill_state_in_tls) {
        insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    }

    if (!inline_ibl_head) {
        /* see above: when not inlining we do eflags restore after xbx restore */
#ifdef X64
        if (IS_IBL_TRACE(ibl_code->source_fragment_type) &&
            !GENCODE_IS_X86(code->gencode_mode) &&
            !DYNAMO_OPTION(unsafe_ignore_eflags_trace)) {
            instr_t *trace_cmp_unlinked = INSTR_CREATE_label(dcontext);
            APP(&ilist, trace_cmp_unlinked);
            add_patch_marker(patch, trace_cmp_unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t*)&ibl_code->trace_cmp_unlinked);
        }
#endif
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_ibl)) {
            /* restore flags + xax (which we only need so save below works) */
            insert_restore_eflags(dcontext, &ilist, NULL, 0, IBL_EFLAGS_IN_TLS(),
                                  absolute _IF_X64(x86_to_x64_ibl_opt));
        }
        APP(&ilist, unlinked);
    }

    if (!absolute) {
        insert_shared_get_dcontext(dcontext, &ilist, NULL, true /* save register */);
    }
    /* note we are now saving XAX to the dcontext - no matter where it
     * was saved before for saving and restoring eflags.  FIXME: in
     * some incarnations of this routine it is redundant, yet this is
     * the slow path anyways
     */
    APP(&ilist, SAVE_TO_DC(dcontext, REG_XAX, XAX_OFFSET));

    /* Indirect exit stub: we have app XBX in slot1, and linkstub in XBX */
    /*   fcache_return however is geared for direct exit stubs which uses XAX */
    /*   app XBX is properly restored  */
    /*   XAX gets the linkstub_ptr */
    /*   app XAX is saved in slot1 */
    /* FIXME: this all can be cleaned up at the cost of an extra byte in
       direct exit stubs to use XBX */

    if (ibl_code->source_fragment_type == IBL_COARSE_SHARED) {
        /* Coarse-grain uses the src tag plus sourceless but type-containing
         * fake linkstubs.  Here we put the src from xbx into its special slot.
         */
        ASSERT(DYNAMO_OPTION(coarse_units));
        APP(&ilist, SAVE_TO_DC(dcontext, REG_XBX, COARSE_IB_SRC_OFFSET));
        ASSERT(linkstub != NULL);
    }

    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        /* in order to write next_tag we must unprotect -- and we don't
         * have safe stack yet!  so we duplicate fcache_return code here,
         * but we keep xcx w/ next tag around until we can store it as next_tag.
         */
        /* need to save xax (was never saved before) */
        /*>>>    SAVE_TO_UPCONTEXT %xax,xax_OFFSET                  */
        /* put &linkstub where dispatch expects it */
        /*>>>    mov     %xbx,%xax                                       */
        if (linkstub == NULL) {
            APP(&ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                            opnd_create_reg(REG_XBX)));
        } else {
            APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                             OPND_CREATE_INTPTR((ptr_int_t)linkstub)));
        }
        append_fcache_return_common(dcontext, code, &ilist, true/*ibl end*/, absolute,
                                    false/*!shared*/, NULL, false/*no coarse info*/);
    } else {
        /* set up for fcache_return: save xax, put xcx in next_tag, &linkstub in xax */
        /*>>>    SAVE_TO_UPCONTEXT %xax,xax_OFFSET                       */
        /*>>>    SAVE_TO_DCONTEXT %xcx,next_tag_OFFSET                   */
        /*>>>    mov     %xbx,%xax                                       */
        /*>>>    RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx                  */
        /*>>>    RESTORE_FROM_UPCONTEXT xcx_OFFSET,%xcx                  */
        /*>>>    jmp     _fcache_return                                  */
        APP(&ilist, SAVE_TO_DC(dcontext, REG_XCX, NEXT_TAG_OFFSET));

        if (linkstub == NULL) {
            post_linkcount = INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                                 opnd_create_reg(REG_XBX));
        } else {
            /* there is no exit-specific stub -- we use a generic one here */
            post_linkcount = INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                                  OPND_CREATE_INTPTR((ptr_int_t)linkstub));
        }
#ifdef PROFILE_LINKCOUNT
        /* not supported for inline_ibl_head, so we can assume xbx==&linkstub
         * assumes xcx is free */
        if (dynamo_options.profile_counts &&
            IS_IBL_TRACE(ibl_code->source_fragment_type)) {
            append_linkcount_incr(dcontext, &ilist, false/*preserve eflags*/,
                                  post_linkcount);
        }
#endif
        APP(&ilist, post_linkcount);

        if (absolute) {
            if (DYNAMO_OPTION(indirect_stubs))
                APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XBX, XBX_OFFSET));
            /* else, xbx never spilled */
        } else /* table_in_tls || only_spill_state_in_tls */ {
            if (DYNAMO_OPTION(indirect_stubs)) {
                /* restore XBX from EXIT_STUB_SPILL_SLOT */
                APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XBX,
                                             INDIRECT_STUB_SPILL_SLOT));
            } /* else, xbx never spilled */
            /* now need to juggle with app XAX to be in DIRECT_STUB_SPILL_SLOT, using XCX */
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XCX, XAX_OFFSET));
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XCX, DIRECT_STUB_SPILL_SLOT));
            /* DIRECT_STUB_SPILL_SLOT has XAX value as needed for fcache_return_shared */
        }

        /* We need to restore XCX from TLS for shared IBL routines, but from
         * mcontext for private IBL routines (unless private_ib_in_tls is set).
         * For x86_to_x64, we restore XCX from R9.
         */
        if (IF_X64_ELSE(x86_to_x64_ibl_opt, false)) {
            APP(&ilist, RESTORE_FROM_REG(dcontext, REG_XCX, REG_R9));
        } else if (ibl_code->thread_shared_routine || DYNAMO_OPTION(private_ib_in_tls)) {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
        } else {
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XCX, XCX_OFFSET));
        }

        if (!absolute) {
            /* restore from scratch the XDI register even if fcache_return will
             * save it again */
            insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
        }
        /* pretending we came from a direct exit stub - linkstub in XAX, all
         * other app registers restored */
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(fcache_return_pc)));
    }

    if (inline_ibl_head && !DYNAMO_OPTION(atomic_inlined_linking)) {
        /* #ifdef HASHTABLE_STATISTICS
         * >>> race_condition_inc:
         * >>>   #note that eflags are already saved in this path
         * >>>   <inc_stat>
         * >>>   jmp fragment_not_found
         * #endif
         * >>>   #detect unlink path flag to check for unlinking race condition
         * >>>   #must be eflags safe, they are prob. not saved yet
         * >>> unlinked:
         * >>>   movzx %cl, %xcx
         * >>>   # xcx now holds 1 in the unlink case, and the zero extended
         * >>>   # lower byte of a pointer into the hashtable in the race
         * >>>   # condition case (since our pointers into the hashtable are
         * >>>   # aligned this can't be 1), the loop will jmp if xcx != 1
         * #ifdef HASHTABLE_STATISTICS
         * >>>   loop race_condition_inc #race condition handling path
         * #else
         * >>>   loop fragment_not_found  #race condition handling path
         * #endif
         * >>>   #normal unlink path
         * >>>   RESTORE_FROM_UPCONTEXT xbx_offset, %xcx
         * >>>   SAVE_TO_UPCONTEXT %xbx, xbx_offset
         * >>>   jmp old_unlinked
         */
        instr_t *old_unlinked_target = instr_get_next(unlinked);
        instr_t *race_target = fragment_not_found;
#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            race_target = instrlist_last(&ilist);
            if (save_xdi)
                APP(&ilist, SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            append_increment_counter(dcontext, &ilist, ibl_code, patch,
                                     REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(race_condition),
                                     REG_XCX);
            if (save_xdi) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            }
            APP(&ilist,
                INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(fragment_not_found)));
        }
        race_target = instr_get_next(race_target);
#endif
        instrlist_remove(&ilist, unlinked);
        APP(&ilist, unlinked);
        APP(&ilist, INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_XCX),
                                       opnd_create_reg(REG_CL)));
        add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                         0 /* beginning of instruction */,
                         (ptr_uint_t*)&ibl_code->unlinked_ibl_entry);
        /* subtract 1 from xcx and jmp if !=0 (race condition case) */
        APP(&ilist, INSTR_CREATE_loop(dcontext,
                                      opnd_create_instr(race_target)));
#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            if (save_xdi)
                APP(&ilist, SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            append_increment_counter(dcontext, &ilist, ibl_code, patch,
                                     REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(unlinked_count),
                                     REG_XCX);
            if (save_xdi) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            }
            else if (only_spill_state_in_tls) /* restore app %xdi */
                insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
        }
#endif
        if (absolute) {
            APP(&ilist, RESTORE_FROM_DC(dcontext, REG_XCX, XBX_OFFSET));
            APP(&ilist, SAVE_TO_DC(dcontext, REG_XBX, XBX_OFFSET));
        } else {
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_XCX, XBX_OFFSET));
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XBX, XBX_OFFSET));
        }
        APP(&ilist, INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(old_unlinked_target)));
    } else {
        /* get a patch marker at the instruction the label is at: */
#ifdef HASHTABLE_STATISTICS
        if (INTERNAL_OPTION(hashtable_ibl_stats)) {
            instr_t *old_unlinked = instr_get_next(unlinked);
            instrlist_remove(&ilist, unlinked);
            APP(&ilist, unlinked);
            add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t*)&ibl_code->unlinked_ibl_entry);
            /* FIXME: for x64 -thread_private we enter here from trace,
             * and not from top of ibl, so we must save xdi.  Is this true
             * for all cases of only_spill_state_in_tls with !save_xdi?
             * Maybe should be saved in append_increment_counter's call to
             * insert_shared_get_dcontext() instead.
             */
            if (IF_X64_ELSE(true, save_xdi)) {
                APP(&ilist,
                    SAVE_TO_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            }
            /* have to save eflags before increment, saving eflags clobbers
             * xax slot, but that should be dead here */
            insert_save_eflags(dcontext, &ilist, NULL, 0, !absolute, absolute
                               _IF_X64(x86_to_x64_ibl_opt));
            append_increment_counter(dcontext, &ilist, ibl_code, patch,
                                     REG_NULL,
                                     HASHLOOKUP_STAT_OFFS(unlinked_count),
                                     REG_XBX);
            insert_restore_eflags(dcontext, &ilist, NULL, 0, !absolute, absolute
                                  _IF_X64(x86_to_x64_ibl_opt));
            if (IF_X64_ELSE(true, save_xdi)) {
                APP(&ilist,
                    RESTORE_FROM_TLS(dcontext, REG_XDI, HTABLE_STATS_SPILL_SLOT));
            }
            else if (only_spill_state_in_tls) {
                /* we didn't care that XDI got clobbered since it was spilled
                 * at the entry point into the IBL routine but we do need to
                 * restore app state now.
                 */
                insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
            }
            /* In a PROFILE_LINKCOUNT LINKCOUNT_64_BITS build with
             * -prof_counts on, this jmp is too far for a jmp_short on a shared
             * ibl.  We just use a regular jmp since this code isn't even in a
             * release build. FIXME - would be a good place for a real
             * jmp_smart opcode xref case 5232. */
            APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(old_unlinked)));
        } else
#endif
        {
            add_patch_marker(patch, unlinked, PATCH_ASSEMBLE_ABSOLUTE,
                             0 /* beginning of instruction */,
                             (ptr_uint_t*)&ibl_code->unlinked_ibl_entry);
        }
    }

#ifdef X64
    if (GENCODE_IS_X86(code->gencode_mode)) {
        /* We currently have x86 code parsing the regular x64 table (PR 283895 covers
         * using an x86 table, for both full correctness and performance: for now
         * we have no way to detect a source in one mode jumping to a target built in
         * another mode w/o a mode switch, but that would be an app error anyway).
         * Rather than complicating the REG_X* defines used above we have a post-pass
         * that shrinks all the registers and all the INTPTR immeds.
         * The other two changes we need are performed up above:
         *   1) cmp top bits to 0 for match
         *   2) no trace_cmp entry points
         * Note that we're punting on PR 283152: we go ahead and clobber the top bits
         * of all our scratch registers.
         */
        instrlist_convert_to_x86(&ilist);
    }
#endif

    ibl_code->ibl_routine_length = encode_with_patch_list(dcontext, patch, &ilist, pc);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc + ibl_code->ibl_routine_length;
}

static inline void
update_ibl_routine(dcontext_t *dcontext, ibl_code_t *ibl_code)
{
    if (!ibl_code->initialized)
        return;
    patch_emitted_code(dcontext, &ibl_code->ibl_patch,
                       ibl_code->indirect_branch_lookup_routine);
    DOLOG(2, LOG_EMIT, {
        const char *ibl_name;
        const char *ibl_brtype;
        ibl_name = get_ibl_routine_name(dcontext,
                                        ibl_code->indirect_branch_lookup_routine,
                                        &ibl_brtype);
        LOG(THREAD, LOG_EMIT, 2, "Just updated indirect branch lookup\n%s_%s:\n",
            ibl_name, ibl_brtype);
        disassemble_with_annotations(dcontext, &ibl_code->ibl_patch,
                                     ibl_code->indirect_branch_lookup_routine,
                                     ibl_code->indirect_branch_lookup_routine + ibl_code->ibl_routine_length);
    });

    if (ibl_code->ibl_head_is_inlined) {
        patch_emitted_code(dcontext, &ibl_code->ibl_stub_patch, ibl_code->inline_ibl_stub_template);
        DOLOG(2, LOG_EMIT, {
            const char *ibl_name;
            const char *ibl_brtype;
            ibl_name = get_ibl_routine_name(dcontext,
                                            ibl_code->indirect_branch_lookup_routine,
                                            &ibl_brtype);
            LOG(THREAD, LOG_EMIT, 2, "Just updated inlined stub indirect branch lookup\n%s_template_%s:\n",
                ibl_name, ibl_brtype);
            disassemble_with_annotations(dcontext, &ibl_code->ibl_stub_patch, ibl_code->inline_ibl_stub_template,
                                         ibl_code->inline_ibl_stub_template + ibl_code->inline_stub_length);
        });
    }
}

void
update_indirect_branch_lookup(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);

    ibl_branch_type_t branch_type;
#ifdef X64
    ASSERT(is_shared_gencode(code));
    return; /* nothing to do: routines are all thread-shared */
#endif
    protect_generated_code(code, WRITABLE);
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
        update_ibl_routine(dcontext, &code->bb_ibl[branch_type]);
        if (PRIVATE_TRACES_ENABLED() && !DYNAMO_OPTION(shared_trace_ibl_routine))
            update_ibl_routine(dcontext, &code->trace_ibl[branch_type]);
    }
#ifdef WINDOWS
    /* update mask and table in inlined ibl at end of syscall routine */
    if (DYNAMO_OPTION(shared_syscalls)) {
        patch_emitted_code(dcontext, &code->shared_syscall_code.ibl_patch,
                           code->unlinked_shared_syscall);
        DOLOG(2, LOG_EMIT, {
            LOG(THREAD, LOG_EMIT, 2, "Just updated shared syscall routine:\n");
            disassemble_with_annotations(dcontext,
                                         &code->shared_syscall_code.ibl_patch,
                                         code->unlinked_shared_syscall,
                                         code->end_shared_syscall);
        });
    }
#endif
    protect_generated_code(code, READONLY);
}

/* i#823: handle far cti transitions.  For now only handling known cs values
 * for WOW64 when using x64 DR, but we still use this far ibl so that in
 * the future we can add general cs change handling outside of the
 * fragment (which is much simpler: see below).
 *
 * One approach is to have the mode change happen in the fragment itself via
 * ind branch mangling.  But then we have the check for known cs there and
 * thus multiple exits some of which are 32-bit and some of which are 64-bit
 * which is messy.  Instead, we spill another reg, put the selector in it,
 * and jump to this ibl prefix routine.  One drawback is that by not doing
 * the mode transition in the fragment we give up on traces extending through
 * it and we must make a far cti a trace barrier.
 *
 *   fragment:
 *     spill xbx
 *     movzx selector -> xbx
 *     spill xcx
 *     mov target -> xcx
 *     jmp far_ibl
 *
 *   far_ibl:
 *     clear top 32 bits of xcx slot
 *     xchg xcx, xbx
 *     lea xcx -32_bit_cs -> xcx
 *     jecxz to_32
 *   64: (punting on handling cs o/w)
 *     xchg xcx, xbx
 *     restore xbx
 *     jmp 64-bit ibl
 *   to-32:
 *     dcontext -> ecx
 *     mov $1 -> x86_mode_offs(ecx)
 *     xchg xcx, xbx
 *     restore xbx
 *     far ind jmp through const mem that targets 32-bit ibl
 *
 * This is much simpler for state xl8: shouldn't need any added support.
 * For unlinking: have two versions of the gencode, so the unlink
 * is the standard fragment exit cti change only.
 *
 * For non-mixed-mode, we just jmp straight to ibl.  It's simpler to
 * generate and always go through this far_ibl though rather than
 * having interp up front figure out whether a mode change for direct
 * and then have far direct sometimes be direct and sometimes use
 * indirect faar-Ibl.
 *
 * For -x86_to_x64, we assume no 32-bit un-translated code entering here.
 *
 * FIXME i#865: for mixed-mode (including -x86_to_x64), far ibl must
 * preserve the app's r8-r15 during 32-bit execution.
 */
byte *
emit_far_ibl(dcontext_t *dcontext, byte *pc, ibl_code_t *ibl_code,
             cache_pc ibl_same_mode_tgt _IF_X64(far_ref_t *far_jmp_opnd))
{
    instrlist_t ilist;
    instrlist_init(&ilist);

#ifdef X64
    if (mixed_mode_enabled()) {
        instr_t *change_mode = INSTR_CREATE_label(dcontext);
        bool source_is_x86 = DYNAMO_OPTION(x86_to_x64) ? ibl_code->x86_to_x64_mode
                                                       : ibl_code->x86_mode;
        short selector = source_is_x86 ? CS64_SELECTOR : CS32_SELECTOR;

        /* all scratch space should be in TLS only */
        ASSERT(ibl_code->thread_shared_routine || DYNAMO_OPTION(private_ib_in_tls));

        if (ibl_code->x86_mode) {
            /* we're going to look up rcx in ibl table but we only saved the
             * bottom half so zero top half now
             */
            APP(&ilist, INSTR_CREATE_mov_imm
                (dcontext, opnd_create_tls_slot(os_tls_offset(MANGLE_XCX_SPILL_SLOT) + 4),
                 OPND_CREATE_INT32(0)));
        }

        APP(&ilist, INSTR_CREATE_xchg
            (dcontext, opnd_create_reg(REG_XBX), opnd_create_reg(REG_XCX)));
        /* segment is just 2 bytes but need addr prefix if don't have rex prefix */
        APP(&ilist, INSTR_CREATE_lea
            (dcontext, opnd_create_reg(REG_XCX),
             opnd_create_base_disp(REG_XCX, REG_NULL, 0, -selector, OPSZ_lea)));
        APP(&ilist, INSTR_CREATE_jecxz
            (dcontext, opnd_create_instr(change_mode)));

        APP(&ilist, INSTR_CREATE_xchg
            (dcontext, opnd_create_reg(REG_XBX), opnd_create_reg(REG_XCX)));
        if (ibl_code->x86_to_x64_mode && DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            APP(&ilist, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(REG_XBX), opnd_create_reg(REG_R10)));
        } else {
            APP(&ilist,
                RESTORE_FROM_TLS(dcontext, REG_XBX, MANGLE_FAR_SPILL_SLOT));
        }
        APP(&ilist, INSTR_CREATE_jmp
            (dcontext, opnd_create_pc(ibl_same_mode_tgt)));

        APP(&ilist, change_mode);
        APP(&ilist, instr_create_restore_from_tls
            (dcontext, REG_XCX, TLS_DCONTEXT_SLOT));
        /* FIXME: for SELFPROT_DCONTEXT we'll need to exit to dispatch every time
         * and add logic there to set x86_mode based on LINK_FAR.
         * We do not want x86_mode sitting in unprotected_context_t.
         */
        ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)));
        APP(&ilist, INSTR_CREATE_mov_st
            (dcontext, OPND_CREATE_MEM8(REG_XCX, (int)offsetof(dcontext_t, isa_mode)),
             OPND_CREATE_INT8(source_is_x86 ? DR_ISA_AMD64 : DR_ISA_IA32)));
        APP(&ilist, INSTR_CREATE_xchg
            (dcontext, opnd_create_reg(REG_XBX), opnd_create_reg(REG_XCX)));
        if (ibl_code->x86_to_x64_mode && DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            APP(&ilist, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(REG_XBX), opnd_create_reg(REG_R10)));
        } else {
            APP(&ilist,
                RESTORE_FROM_TLS(dcontext, REG_XBX, MANGLE_FAR_SPILL_SLOT));
        }
        if (ibl_code->x86_mode) {
            /* FIXME i#865: restore 64-bit regs here */
        } else if (ibl_code->x86_to_x64_mode && DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            /* In the current mode, XCX is spilled into R9.
             * After mode switch, will use MANGLE_XCX_SPILL_SLOT for spilling XCX.
             */
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_R9, MANGLE_XCX_SPILL_SLOT));
            /* FIXME i#865: restore 64-bit regs here */
        } else {
            /* FIXME i#865: save 64-bit regs here */
            /* In the current mode, XCX is spilled into MANGLE_XCX_SPILL_SLOT.
             * After mode switch, will use R9 for spilling XCX.
             */
            APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_R9, MANGLE_XCX_SPILL_SLOT));
        }
        /* For now we assume we're WOW64 and thus in low 4GB.  For general mixed-mode
         * and reachability (xref i#774) we will need a trampoline in low 4GB.
         * Note that targeting the tail of the not-taken jecxz above doesn't help
         * b/c then that needs to be 32-bit reachable.
         */
        ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)far_jmp_opnd));
        APP(&ilist, INSTR_CREATE_jmp_far_ind
            (dcontext, opnd_create_base_disp(REG_NULL, REG_NULL, 0,
                                             (uint)(ptr_uint_t)far_jmp_opnd, OPSZ_6)));
        /* For -x86_to_x64, we can disallow 32-bit fragments from having
         * indirect branches or far branches or system calls, and thus ibl
         * is always 64-bit.
         * Even if we allow 32-bit indirection, here we have to pick one
         * lookup method, and we'd go w/ the most common, which would assume
         * a 32-bit target has been translated: so even for a same-mode far
         * cti in a 32-bit (untranslated) fragment, we'd want to do a mode
         * change here.
         */
        /* caller will set target: we just set selector */
        far_jmp_opnd->selector = DYNAMO_OPTION(x86_to_x64) ? CS64_SELECTOR
                                                           : (ushort) selector;

        if (ibl_code->x86_mode) {
            instrlist_convert_to_x86(&ilist);
        }
    } else {
#endif
        /* We didn't spill or store into xbx when mangling so just jmp to ibl.
         * Note that originally I had the existence of far_ibl, and LINK_FAR,
         * as X64 only, and only emitted far_ibl for mixed-mode.  But given that
         * it's simpler to have far direct as indirect all the time, I decided
         * to also go through a far ibl all the time.  Eventually to fully
         * handle any cs change we'll want it this way.
         *
         * XXX i#823: store cs into xbx when mangling, and then do cs
         * change here.
         */
        APP(&ilist, INSTR_CREATE_jmp
            (dcontext, opnd_create_pc(ibl_same_mode_tgt)));
#ifdef X64
    }
#endif

    pc = instrlist_encode(dcontext, &ilist, pc, true/*instr targets*/);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}


static instr_t *
create_int_syscall_instr(dcontext_t *dcontext)
{
#ifdef WINDOWS
    /* On windows should already be initialized by syscalls_init() */
    ASSERT(get_syscall_method() != SYSCALL_METHOD_UNINITIALIZED);
    /* int $0x2e */
    if (DYNAMO_OPTION(sygate_int)) {
        /* ref case 5217, we call to an existing int in NtYieldExecution
         * to avoid tripping up Sygate. */
        return INSTR_CREATE_call(dcontext, opnd_create_pc(int_syscall_address));
    } else {
        return INSTR_CREATE_int(dcontext, opnd_create_immed_int((char)0x2e, OPSZ_1));
    }
#else
    /* if uninitialized just guess int, we'll patch up later */
    /* int $0x80 */
    return INSTR_CREATE_int(dcontext, opnd_create_immed_int((char)0x80, OPSZ_1));
#endif
}

instr_t *
create_syscall_instr(dcontext_t *dcontext)
{
    int method = get_syscall_method();
    if (method == SYSCALL_METHOD_INT || method == SYSCALL_METHOD_UNINITIALIZED) {
        return create_int_syscall_instr(dcontext);
    } else if (method == SYSCALL_METHOD_SYSENTER) {
        return INSTR_CREATE_sysenter(dcontext);
    } else if (method == SYSCALL_METHOD_SYSCALL) {
        return INSTR_CREATE_syscall(dcontext);
    }
#ifdef WINDOWS
    else if (method == SYSCALL_METHOD_WOW64) {
        /* call *fs:0xc0 */
        return INSTR_CREATE_call_ind(dcontext,
                                     opnd_create_far_base_disp(SEG_FS, REG_NULL,
                                                               REG_NULL, 0,
                                                               WOW64_TIB_OFFSET,
                                                               OPSZ_4_short2));
    }
#endif
    else {
        ASSERT_NOT_REACHED();
        return NULL;
    }
}

#ifdef WINDOWS

/* Insert instructions after the syscall instruction (e.g., sysenter) to
 * restore the next tag target from dcontext XSI slot to %xcx register
 * for continue execution.
 * See the comment below for emit_shared_syscall about shared syscall
 * handling.
 */
static void
insert_restore_target_from_dc(dcontext_t *dcontext,
                              instrlist_t *ilist,
                              bool all_shared)
{
    ASSERT(IF_X64_ELSE(all_shared, true)); /* PR 244737 */
    if (all_shared) {
        APP(ilist,
            instr_create_restore_from_dc_via_reg
            (dcontext, REG_NULL/*default*/, REG_XCX, XSI_OFFSET));
    } else {
        APP(ilist,
            instr_create_restore_from_dcontext(dcontext, REG_XCX, XSI_OFFSET));
    }
# ifdef CLIENT_INTERFACE
    /* i#537: we push KiFastSystemCallRet on to the stack and adjust the
     * next code to be executed at KiFastSystemCallRet.
     */
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
        KiFastSystemCallRet_address != NULL) {
        /* push adjusted ecx onto stack */
        APP(ilist, INSTR_CREATE_push(dcontext, opnd_create_reg(REG_XCX)));
        APP(ilist, INSTR_CREATE_mov_imm(dcontext,
                                        opnd_create_reg(REG_XCX),
                                        OPND_CREATE_INT32
                                        (KiFastSystemCallRet_address)));
    }
# endif /* CLIENT_INTERFACE */
}

/* All system call instructions turn into a jump to an exit stub that
 * jumps here, with the xsi slot in dcontext (or the mangle-next-tag tls slot
 * for -shared_fragment_shared_syscalls) containing the return address
 * after the original system call instr, and xbx containing the linkstub ptr.
 *
 * Unlinked version of shared_syscall is needed, even though syscalls are
 * not part of traces (we unlink for other reasons, like flushing or
 * in-trace replacement).
 * To make unlinked entry point, have to make completely separate routine
 * that calls unlinked_ibl instead of indirect_branch_lookup, or else
 * common linked case needs an extra conditional.  I chose the latter
 * approach.  I figure an extra load and jecxz won't be noticeable.
 * Another reason is that this approach means there is a single system
 * call instruction to check for suspended threads at, instead of two.
 * To make the jecxz match forward-not-taken I actually add another store
 * on the linked path.
 * FIXME: is this a perf hit that makes it worth the code complexity
 * of two syscall routines?
 * FIXME: The 'target_trace_table' indicates whether the trace or BB IBT
 * table should be targetted. If BB2BB IBL is used (when trace building is
 * not disabled), then both traces and BBs use the same shared syscall.
 * (We emit only one.) So we can't target the BB table since that would
 * result in missed opportunities to mark secondary trace heads (trace->BB
 * IB transitions after shared syscall). So for BB2BB IBL this could be
 * a perf hit, but not a regression compared to not using BB2BB IBL. More
 * comments below in the routine.
 *
_unlinked_shared_syscall:
        SAVE_TO_UPCONTEXT $0,xax_OFFSET # flag: use unlinked ibl; xcx tls if all_shared
        jmp skip_linked
_shared_syscall:
        SAVE_TO_UPCONTEXT $1,xax_OFFSET # flag: use regular ibl; xcx tls if all_shared
skip_linked:
        .ifdef SIDELINE
        # clear cur-trace field so we don't think cur trace is still running
        mov     $0, _sideline_trace
        .endif

        .if all_shared
        SAVE_TO_TLS xdi, xdi_offset
        RESTORE_FROM_TLS xdi, dcontext_offset
        .endif

        .if !all_shared && DYNAMO_OPTION(shared_fragment_shared_syscalls)
          .if !sysenter_syscall_method
              LOAD_FROM_TLS MANGLE_NEXT_TAG_SLOT,%xdi
              SAVE_TO_UPCONTEXT %xdi,xsi_OFFSET
          .endif
        RESTORE_FROM_TLS xdi_OFFSET
        .endif

        # make registers have app values for interrupt
        .if !INTERNAL_OPTION(shared_syscalls_fastpath)
        SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET # save linkstub ptr
           .if all_shared
           # get next_tag (from xcx tls slot) into upcontext, for callback dcontext swap
           RESTORE_FROM_TLS xbx, mangle_next_tag_slot
           SAVE_TO_UPCONTEXT xbx, xsi_OFFSET
           .endif
           # %xbx is stored in TLS if shared fragments can target shared syscall
           .if DYNAMO_OPTION(shared_fragment_shared_syscalls)
           LOAD_FROM_TLS INDIRECT_STUB_SPILL_SLOT,%xbx # restore app's xbx
           .else
           RESTORE_FROM_UPCONTEXT xbx_OFFSET,%xbx # restore app's xbx
           .endif
        .endif

        .if sysenter_syscall_method
        pop     xsi_OFFSET
        push    <after-syscall-address>
        .endif

        # even if !DYNAMO_OPTION(syscalls_synch_flush) must set for reset
        movl 1, at_syscall_OFFSET # indicate to flusher we're in a syscall

        .if all_shared
        SAVE_TO_UPCONTEXT  xdi, xdi_offset
        RESTORE_FROM_TLS xdi, xdi_offset
        .endif

        # system call itself
        int     $0x2e
        # kernel may decide to run a callback here...but when we come
        #   back we can't tell the difference

        .if all_shared
        RESTORE_FROM_TLS xdi, dcontext_offset
        .endif

        # even if !DYNAMO_OPTION(syscalls_synch_flush) must clear for cbret
        movl 0, at_syscall_OFFSET # indicate to flusher/dispatch we're done w/ syscall

        # assume interrupt could have changed register values
        .if !inline_ibl_head # else, saved inside inlined ibl
           # for shared_fragment_shared_syscalls = true, absolute != true
           .if !DYNAMO_OPTION(shared_fragment_shared_syscalls)
           SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET
           .endif
           .if !absolute
           SAVE_TO_TLS %xbx,INDIRECT_STUB_SPILL_SLOT
           .endif
           .if !INTERNAL_OPTION(shared_syscalls_fastpath)
           RESTORE_FROM_UPCONTEXT xdi_OFFSET,%xbx # bring back linkstub ptr
           .endif
        .endif

        # now set up for indirect_branch_lookup
        .if !DYNAMO_OPTION(shared_fragment_shared_syscalls)
        SAVE_TO_UPCONTEXT %xcx,xcx_OFFSET
        .endif
        .if !absolute && !all_shared
        SAVE_TO_TLS %xcx,MANGLE_XCX_SPILL_SLOT
        .endif

        .if all_shared
        xchg  xcx-tls, xcx # get link/unlink flag, and save app xcx, at once
          .if x64
           mov   ecx,ecx # clear top 32 bits of flag
           .endif
        .else
        RESTORE_FROM_UPCONTEXT xax_OFFSET,%xcx # get link/unlink flag
        .endif

        # patch point: jecxz -> jmp for shared_syscall unlink
        jecxz unlink

        .if INTERNAL_OPTION(shared_syscalls_fastpath)
        mov     shared-syscalls-bb-linkstub, %xbx # set linkstub ptr
           .if inline_ibl_head
           SAVE_TO_UPCONTEXT %xbx,xdi_OFFSET # save linkstub ptr
           .endif
        .endif

        # linked code
        RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xcx # bring back return address
        .if !inline_ibl_head
        jmp     _indirect_branch_lookup
        .else
        # inline ibl lookup head here! (don't need unlink/miss, already did
        #   that work, miss goes straight to ibl routine)
        .endif

unlink:
        # unlinked code
        RESTORE_FROM_UPCONTEXT xsi_OFFSET,%xcx # bring back return address
        .if !inline_ibl_head
        mov  @shared_syscall_unlinked_linkstub,%xbx
        .else
           .if absolute
           SAVE_TO_UPCONTEXT @shared_syscall_unlinked_linkstub,xdi_OFFSET
           .else
           SAVE_TO_TLS @shared_syscall_unlinked_linkstub,INDIRECT_STUB_SPILL_SLOT
           .endif
           .if !DYNAMO_OPTION(atomic_inlined_linking)
           SAVE_TO_UPCONTEXT %xcx,xbx_offset
           movb  $0x1, %cl
           .else
           SAVE_TO_UPCONTEXT %xbx,xbx_OFFSET # could have changed in kernel
           .endif
        .endif

        jmp     _unlinked_ib_lookup
 */
byte *
emit_shared_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                    ibl_code_t *ibl_code,
                    patch_list_t *patch,
                    byte *ind_br_lookup_pc, byte *unlinked_ib_lookup_pc,
                    bool target_trace_table,
                    bool inline_ibl_head,
                    bool thread_shared,
                    byte **shared_syscall_pc)
{
    instrlist_t ilist;
    byte *start_pc = pc;
    instr_t *syscall; /* remember after-syscall pc b/c often suspended there */
    /* relative labels */
    instr_t *linked, *jecxz, *unlink, *skip_syscall = NULL;
    bool absolute = !thread_shared;
    uint after_syscall_ptr = 0;
    uint syscall_method = get_syscall_method();
    instr_t *adjust_tos;
    /* thread_shared indicates whether ibl is thread-shared: this bool indicates
     * whether this routine itself is all thread-shared */
    bool all_shared = IF_X64_ELSE(true, false); /* PR 244737 */
    IF_X64(bool x86_to_x64_ibl_opt = ibl_code->x86_to_x64_mode &&
        DYNAMO_OPTION(x86_to_x64_ibl_opt);)

    /* no support for absolute addresses on x64: we always use tls */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!absolute));
    /* x64 always shares shared_syscall fragments */
    IF_X64(ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(shared_fragment_shared_syscalls)));
    /* PR 248207: haven't updated the inlining to be x64-compliant yet */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!inline_ibl_head));

    /* i#821/PR 284029: for now we assume there are no syscalls in x86 code.
     * To support them we need to update this routine, emit_do_syscall*,
     * and emit_detach_callback_code().
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!ibl_code->x86_mode));

    /* ibl_code was not initialized by caller */
    ibl_code->thread_shared_routine = thread_shared;
    ibl_code->branch_type = IBL_SHARED_SYSCALL;

    /* initialize the ilist */
    instrlist_init(&ilist);
    init_patch_list(patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_XDI);
    /* We should generate some thread-shared code when
     * shared_fragment_shared_syscalls=true. */
    DOCHECK(1, {
        if (DYNAMO_OPTION(shared_fragment_shared_syscalls))
            ASSERT(!absolute);
    });
    LOG(THREAD, LOG_EMIT, 3,
        "emit_shared_syscall: pc="PFX" patch="PFX
        " inline_ibl_head=%d thread shared=%d\n",
        pc, patch, inline_ibl_head, thread_shared);

    /* FIXME: could save space by storing a single byte, and using movzx into ecx
     * below before the jecxz
     */
    if (all_shared) {
        /* xax and xbx tls slots are taken so we use xcx */
# ifdef X64
        if (x86_to_x64_ibl_opt) {
            linked = INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_R9D),
                                          OPND_CREATE_INT32(1));
        } else {
# endif
            linked = INSTR_CREATE_mov_st(dcontext,
                                         OPND_TLS_FIELD_SZ(MANGLE_XCX_SPILL_SLOT, OPSZ_4),
                                         OPND_CREATE_INT32(1));
# ifdef X64
        }
# endif
    } else
        linked = instr_create_save_immed_to_dcontext(dcontext, 1, XAX_OFFSET);
    APP(&ilist, linked);
    add_patch_marker(patch, instrlist_first(&ilist), PATCH_ASSEMBLE_ABSOLUTE,
                     0 /* beginning of instruction */, (ptr_uint_t*)shared_syscall_pc);

# ifdef SIDELINE
    if (dynamo_options.sideline) {
        /* clear cur-trace field so we don't think cur trace is still running */
        APP(&ilist, INSTR_CREATE_mov_st
            (dcontext, OPND_CREATE_ABSMEM((void *)&sideline_trace, OPSZ_4),
             OPND_CREATE_INT32(0)));
    }
# endif

    if (all_shared) {
        /* load %xdi w/ dcontext */
        insert_shared_get_dcontext(dcontext, &ilist, NULL, true/*save xdi*/);
    }

    /* for all-shared we move next tag from tls down below once xbx is dead */
    if (!all_shared && DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        if (syscall_method != SYSCALL_METHOD_SYSENTER) {
            /* Move the next tag field from TLS into the proper slot. */
            APP(&ilist,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XDI),
                                    opnd_create_tls_slot(os_tls_offset(MANGLE_NEXT_TAG_SLOT))));
            APP(&ilist,
                instr_create_save_to_dcontext(dcontext, REG_XDI, XSI_OFFSET));
        }
        /* restore app %xdi */
        insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    }

    /* put linkstub ptr in slot such that when inlined it will be
     * in the right place in case of a miss */
    if (!INTERNAL_OPTION(shared_syscalls_fastpath) && DYNAMO_OPTION(indirect_stubs)) {
        /* even if inline_ibl_head and !absolute, we must put into mcontext
         * here since tls is not saved on callback stack
         */
        if (all_shared) {
            APP(&ilist,
                instr_create_save_to_dc_via_reg(dcontext, REG_NULL/*default*/,
                                                REG_XBX, XDI_OFFSET));
        } else {
            APP(&ilist,
                instr_create_save_to_dcontext(dcontext, REG_XBX, XDI_OFFSET));
        }
    } else {
        /* FIXME: for -no_indirect_stubs, we need our own complete ibl
         * here in order to use our own linkstub_t.  For now we just use
         * a trace jmp* linkstub_t from the ibl we target, making every
         * post-non-ignorable-syscall fragment a trace head.
         */
    }

    if (all_shared) {
        /* move next_tag from tls into dcontext, for callback dcontext swap,
         * using dead xbx */
        if (!DYNAMO_OPTION(indirect_stubs)) {
            /* xbx isn't dead */
            APP(&ilist, instr_create_save_to_tls
                (dcontext, REG_XBX, INDIRECT_STUB_SPILL_SLOT));
        }
        APP(&ilist,
            instr_create_restore_from_tls(dcontext, REG_XBX, MANGLE_NEXT_TAG_SLOT));
        APP(&ilist,
            instr_create_save_to_dc_via_reg(dcontext, REG_NULL/*default*/,
                                            REG_XBX, XSI_OFFSET));
        if (!DYNAMO_OPTION(indirect_stubs)) {
            /* restore xbx */
            APP(&ilist, instr_create_restore_from_tls
                (dcontext, REG_XBX, INDIRECT_STUB_SPILL_SLOT));
        }
    }

    /* make registers have app values for the interrupt */
    /* restore app's xbx (if we went through a stub to get here) */
    if (!INTERNAL_OPTION(shared_syscalls_fastpath) && DYNAMO_OPTION(indirect_stubs)) {
        if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
            APP(&ilist,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XBX),
                                    opnd_create_tls_slot(os_tls_offset(INDIRECT_STUB_SPILL_SLOT))));
        }
        else {
            APP(&ilist,
                instr_create_restore_from_dcontext(dcontext, REG_XBX, XBX_OFFSET));
        }
    }
    if (syscall_method == SYSCALL_METHOD_SYSENTER) {
        /* PR 248210: not bothering to make x64-ready: if we do, be sure to pop into
         * next-tag tls */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        /* For sysenter, mangle pushed the next tag onto the stack,
         * so we pop it into the xsi slot and push the [to-be-patched]
         * after-syscall address.
         */
        /* We have to save xsp in case a callback is delivered and we later detach
         * (since detach expects the callback dcontext xsp to be correct). xref 9889 */
        APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_XSP, XSP_OFFSET));
        APP(&ilist,
            INSTR_CREATE_pop(dcontext,
                             opnd_create_dcontext_field(dcontext, XSI_OFFSET)));
        adjust_tos = INSTR_CREATE_push_imm(dcontext, OPND_CREATE_INT32(0));
        APP(&ilist, adjust_tos);
        add_patch_marker(patch, adjust_tos, PATCH_ASSEMBLE_ABSOLUTE,
                         1 /* offset of imm field */,
                         (ptr_uint_t *)&after_syscall_ptr);
    }
    /* even if !DYNAMO_OPTION(syscalls_synch_flush) must set for reset */
    ASSERT(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)));
    if (all_shared) {
        /* readers of at_syscall are ok w/ us not quite having xdi restored yet */
        APP(&ilist, INSTR_CREATE_mov_st
            (dcontext,
             opnd_create_dcontext_field_via_reg_sz(dcontext, REG_NULL/*default*/,
                                                   AT_SYSCALL_OFFSET, OPSZ_4),
             OPND_CREATE_INT32(1)));
        /* restore app %xdi */
        insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    } else
        APP(&ilist, instr_create_save_immed_to_dcontext(dcontext, 1, AT_SYSCALL_OFFSET));

    if (DYNAMO_OPTION(sygate_sysenter) &&
        get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* PR 248210: not bothering to make x64-ready */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        /* case 5441 hack - set up stack so first return address points to ntdll
         * Won't worry about arithmetic eflags since no one should care about
         * those at a syscall, will preserve other regs though. */
        /* FIXME - what is the perf impact of these extra 5 instructions, we can
         * prob. do better. */
        /* note we assume xsp == xdx (if doesn't we already have prob. ref
         * case 5461) */
        /* current state
         *     xsi_slot = next_pc
         *     xsp -> after_shared_syscall
         *      +4 -> app value1
         * desired state
         *     sysenter_storage_slot = app_value1
         *     xsp -> sysenter_ret_address (ntdll ret)
         *      +4 -> after_shared_syscall
         */
        /* NOTE - the stack mangling must match that of handle_system_call()
         * and intercept_nt_continue() as not all routines looking at the stack
         * differentiate. */
        /* pop stack leaving old value (after_shared_syscall) in place */
        APP(&ilist, INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                                     OPND_CREATE_INT8(4)));
        APP(&ilist,
            INSTR_CREATE_pop(dcontext,
                             opnd_create_dcontext_field(dcontext,
                                                        SYSENTER_STORAGE_OFFSET)));
        /* instead of pulling in the existing stack value we could just patch in
         * the after syscall imm */
        /* see intel docs, source calculated before xsp dec'ed so we're pushing two
         * stack slots up into the next slot up */
        APP(&ilist, INSTR_CREATE_push(dcontext, OPND_CREATE_MEM32(REG_XSP, -8)));
        APP(&ilist, INSTR_CREATE_push_imm
            (dcontext, OPND_CREATE_INTPTR((ptr_int_t)sysenter_ret_address)));
    }

    /* syscall itself */
    APP(&ilist, create_syscall_instr(dcontext));
    syscall = instrlist_last(&ilist);

    if (DYNAMO_OPTION(sygate_sysenter) &&
        get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        /* PR 248210: not bothering to make x64-ready */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        /* case 5441 hack - we popped an extra stack slot, need to fill with saved
         * app value */
        APP(&ilist,
            INSTR_CREATE_push(dcontext,
                              opnd_create_dcontext_field(dcontext,
                                                         SYSENTER_STORAGE_OFFSET)));
    }

    /* Now that all instructions from the linked entry point up to and
     * including the syscall have been added, prepend the unlinked path
     * instructions. We wait until the syscall has been added because when
     * shared_syscalls_fastpath = true and "int 2e" syscalls are used, the
     * target of the unlinked path's jmp is the syscall itself.
     */
    /* these two in reverse order since prepended */
    instrlist_prepend(&ilist, INSTR_CREATE_jmp
                      (dcontext, opnd_create_instr(instr_get_next(linked))));
    if (all_shared) {
        /* xax and xbx tls slots are taken so we use xcx */
# ifdef X64
        if (x86_to_x64_ibl_opt) {
            instrlist_prepend(&ilist, INSTR_CREATE_mov_imm
                              (dcontext, opnd_create_reg(REG_R9D), OPND_CREATE_INT32(0)));
        } else {
# endif
            instrlist_prepend(&ilist, INSTR_CREATE_mov_st
                              (dcontext,
                               /* simpler to do 4 bytes even on x64 */
                               OPND_TLS_FIELD_SZ(MANGLE_XCX_SPILL_SLOT, OPSZ_4),
                               OPND_CREATE_INT32(0)));
# ifdef X64
        }
# endif
    } else {
        instrlist_prepend(&ilist,
                          instr_create_save_immed_to_dcontext(dcontext, 0, XAX_OFFSET));
    }

    /* even if !DYNAMO_OPTION(syscalls_synch_flush) must clear for cbret */
    if (all_shared) {
        /* readers of at_syscall are ok w/ us spilling xdi first */
        insert_shared_get_dcontext(dcontext, &ilist, NULL, true/*save xdi*/);
        APP(&ilist, INSTR_CREATE_mov_st
            (dcontext,
             opnd_create_dcontext_field_via_reg_sz(dcontext, REG_NULL/*default*/,
                                                   AT_SYSCALL_OFFSET, OPSZ_4),
             OPND_CREATE_INT32(0)));
    } else
        APP(&ilist, instr_create_save_immed_to_dcontext(dcontext, 0, AT_SYSCALL_OFFSET));

    if (!inline_ibl_head && DYNAMO_OPTION(indirect_stubs)) {
        /* FIXME Can we remove the write to the mcontext for the !absolute
         * case? Initial tests w/notepad crashed when doing so -- we should
         * look deeper.
         */
        /* save app's xbx (assume interrupt could have changed it) */
        /* Remember, shared_fragment_shared_syscalls=true means absolute=false,
         * so for shared_fragment_shared_syscalls=true %xbx is saved in
         * the !absolute "if" that follows.
         */
        if (!DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
            APP(&ilist,
                instr_create_save_to_dcontext(dcontext, REG_XBX, XBX_OFFSET));
        }
        if (!absolute) {
            /* save xbx in TLS so that downstream code can find it */
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XBX, INDIRECT_STUB_SPILL_SLOT));
        }
        if (!INTERNAL_OPTION(shared_syscalls_fastpath)) {
            if (all_shared) {
                APP(&ilist,
                    instr_create_restore_from_dc_via_reg(dcontext, REG_NULL/*default*/,
                                                         REG_XBX, XDI_OFFSET));
            } else {
                APP(&ilist,
                    instr_create_restore_from_dcontext(dcontext, REG_XBX, XDI_OFFSET));
            }
        }
    } /* if inlined, xbx will be saved inside inlined ibl; if no indirect stubs,
       * xbx will be saved in the ibl routine, or not at all if unlinked
       */

    /* set up for indirect_branch_lookup */
    /* save app's xcx */
    if (!DYNAMO_OPTION(shared_fragment_shared_syscalls))
        APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_XCX, XCX_OFFSET));
    /* FIXME Can we remove the write to the mcontext for the !absolute
     * case, as suggested above? */
    if (!absolute && !all_shared/*done later*/) {
        /* save xcx in TLS */
# ifdef X64
        if (x86_to_x64_ibl_opt)
            APP(&ilist, SAVE_TO_REG(dcontext, REG_XCX, REG_R9));
        else
# endif
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
    }

    if (!INTERNAL_OPTION(shared_syscalls_fastpath)) {
        if (inline_ibl_head && DYNAMO_OPTION(indirect_stubs)) {
            /* Need to move linkstub ptr from mcontext->xdi into tls.
             * We couldn't put it directly there pre-syscall b/c tls
             * is not saved on callback stack!
             * We do this now to take advantage of xcx being dead.
             */
            APP(&ilist,
                instr_create_restore_from_dcontext(dcontext, REG_XCX, XDI_OFFSET));
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XCX, TLS_XDX_SLOT));
        }
    }

    /* get link flag */
    unlink = INSTR_CREATE_label(dcontext);
    if (all_shared) {
        /* we stored 4 bytes so get 4 bytes back; save app xcx at same time */
# ifdef X64
        if (x86_to_x64_ibl_opt) {
            APP(&ilist, INSTR_CREATE_xchg
                (dcontext, opnd_create_reg(REG_R9), opnd_create_reg(REG_XCX)));
        } else {
# endif
            APP(&ilist, INSTR_CREATE_xchg
                (dcontext, OPND_TLS_FIELD(MANGLE_XCX_SPILL_SLOT), opnd_create_reg(REG_XCX)));
# ifdef X64
        }
        /* clear top 32 bits */
        APP(&ilist, INSTR_CREATE_mov_st
            (dcontext, opnd_create_reg(REG_ECX), opnd_create_reg(REG_ECX)));
# endif
        /* app xdi is restored later after we've restored next_tag from xsi slot */
    } else {
        APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_XCX, XAX_OFFSET));
    }
    jecxz = INSTR_CREATE_jecxz(dcontext, opnd_create_instr(unlink));
    APP(&ilist, jecxz);
    /* put linkstub ptr in xbx */
    if (INTERNAL_OPTION(shared_syscalls_fastpath) && DYNAMO_OPTION(indirect_stubs)) {
        APP(&ilist, INSTR_CREATE_mov_imm
            (dcontext, opnd_create_reg(REG_XBX),
             OPND_CREATE_INTPTR((ptr_int_t)get_shared_syscalls_bb_linkstub())));
        /* put linkstub ptr in slot such that when inlined it will be
         * in the right place in case of a miss */
        if (inline_ibl_head) {
            if (absolute) {
                APP(&ilist,
                    instr_create_save_to_dcontext(dcontext, REG_XBX, XDI_OFFSET));
            } else {
                APP(&ilist, SAVE_TO_TLS(dcontext, REG_XBX, TLS_XDX_SLOT));
            }
        }
    } /* else case is up above to use dead xcx reg */

    /* Add a patch marker once we know that there's an instr in the ilist
     * after the syscall. */
    add_patch_marker(patch, instr_get_next(syscall) /* take addr of next instr */,
                     PATCH_UINT_SIZED /* pc relative */,
                     0 /* beginning of instruction */,
                     (ptr_uint_t*)&code->sys_syscall_offs);
    add_patch_marker(patch, jecxz, PATCH_UINT_SIZED /* pc relative */,
                     0 /* point at opcode of jecxz */,
                     (ptr_uint_t*)&code->sys_unlink_offs);

    /* put return address in xcx (was put in xsi slot by mangle.c, or in tls
     * by mangle.c and into xsi slot before syscall for all_shared) */
    /* we duplicate the restore from dc and restore of xdi on the link
     * and unlink paths, rather than putting next_tag back into tls here
     * (can't rely on that tls slot persisting over syscall w/ callbacks)
     */
    insert_restore_target_from_dc(dcontext, &ilist, all_shared);
    if (all_shared) {
        /* restore app %xdi */
        insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    }

    /* FIXME As noted in the routine's header comments, shared syscall targets
     * the trace [IBT] table when both traces and BBs could be using it (when
     * trace building is not disabled). Ideally, we want traces to target the
     * trace table and BBs to target the BB table (when BB2BB IBL is on, that is).
     * Since the BB IBT table usually holds non-trace head BBs as well as traces
     * (including traces is option controlled), using it will doubtless lead to
     * higher IBL hit rate, though it's unclear if there would be a visible
     * impact on performance. Since BBs and traces use different fake linkstubs
     * when executing thru shared syscall, we can detect what the last fragment
     * was and conditionally jump to the ideal IBL routine.
     *
     * Since the EFLAGS at this point hold app state, we'd need to save/restore
     * them prior to executing the IBL code if we used a 'cmp' followed by cond.
     * branch. Or we could save the EFLAGS and jump to a new entry point in the
     * IBL, one just after the 'seto'. (We'd have to move any load of %xdi
     * with the dcontext to just below the 'seto'.)
     *
     * We could avoid conditional code altogether if both inline_trace_ibl
     * and inline_bb_ibl are false. Instead of passing fake linkstub addresses
     * from a fragment exit stub through shared syscall, we could pass the
     * address of the IBL routine to jump to -- BB IBL for BBs and trace IBL
     * for traces. Shared syscall would do an indirect jump to reach the proper
     * routine. On an IBL miss, the address is passed through to dispatch, which
     * can convert the address into the appropriate fake linkstub address (check
     * if the address is within emitted code and equals either BB or trace IBL.)
     * Since an address is being passed around and saved to the dcontext during
     * syscalls, some of which could be relatively long, this is a security
     * hole.
     */
    if (!inline_ibl_head) {
        APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(ind_br_lookup_pc)));
    } else {
        append_ibl_head(dcontext, &ilist, ibl_code, patch, NULL, NULL, NULL,
                        opnd_create_pc(ind_br_lookup_pc),
                        false/*miss cannot have 8-bit offs*/,
                        target_trace_table,
                        inline_ibl_head);
    }

    /* unlink path (there can be no fall-through) */
    APP(&ilist, unlink);
    /* we duplicate the restore from dc and restore of xdi on the link
     * and unlink paths: see note above */
    insert_restore_target_from_dc(dcontext, &ilist, all_shared);
    if (all_shared) {
        /* restore app %xdi */
        insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    }
    /* When traversing the unlinked entry path, since IBL is bypassed
     * control reaches dispatch, and the target is (usually) added to the IBT
     * table. But since the unlinked path was used, the target may already be
     * present in the table so the add attempt is unnecessary and triggers an
     * ASSERT in fragment_add_ibl_target().
     *
     * The add attempt is bypassed by moving an unlinked linkstub ptr into the
     * correct place -- for inlined IBL, the %xdi slot, otherwise, %xbx. This will
     * identify exits from the unlinked path. The stub's flags are set to 0
     * to bypass the add IBL target attempt.
     */
    if (!inline_ibl_head) {
        if (DYNAMO_OPTION(indirect_stubs)) {
            APP(&ilist, INSTR_CREATE_mov_imm
                (dcontext, opnd_create_reg(REG_XBX),
                 OPND_CREATE_INTPTR((ptr_int_t)get_shared_syscalls_unlinked_linkstub())));
        }
    }
    else {
        if (absolute) {
            APP(&ilist, instr_create_save_immed_to_dcontext
                (dcontext, (int)(ptr_int_t)get_shared_syscalls_unlinked_linkstub(),
                 XDI_OFFSET));
        }
        else {
            APP(&ilist, INSTR_CREATE_mov_st
                (dcontext, OPND_TLS_FIELD(TLS_XDX_SLOT),
                 OPND_CREATE_INTPTR((ptr_int_t)get_shared_syscalls_unlinked_linkstub())));
        }
        if (!DYNAMO_OPTION(atomic_inlined_linking)) {
            /* we need to duplicate the emit_inline_ibl_stub unlinking race
             * condition detection code here, before we jump to unlink
             */
            /*
             * # set flag in xcx (bottom byte = 0x1) so that unlinked path can
             * # detect race condition during unlinking
             * 2   movb  $0x1, %cl
            */
            /* we expect target saved in xbx_offset */
            if (absolute) {
                APP(&ilist,
                    instr_create_save_to_dcontext(dcontext, REG_XCX, XBX_OFFSET));
            } else
                APP(&ilist, SAVE_TO_TLS(dcontext, REG_XCX, TLS_XBX_SLOT));
            APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_CL),
                                             OPND_CREATE_INT8(1)));
        } else {
            /* xbx could have changed in kernel, unlink expects it saved */
            if (absolute) {
                APP(&ilist,
                    instr_create_save_to_dcontext(dcontext, REG_XBX, XBX_OFFSET));
            } else
                APP(&ilist, SAVE_TO_TLS(dcontext, REG_XBX, TLS_XBX_SLOT));
        }
    }
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(unlinked_ib_lookup_pc)));

    pc += encode_with_patch_list(dcontext, patch, &ilist, pc);
    if (syscall_method == SYSCALL_METHOD_SYSENTER) {
        ASSERT(after_syscall_ptr != 0);
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        *((uint *)(ptr_uint_t) after_syscall_ptr) =
            (uint)(ptr_uint_t) (code->unlinked_shared_syscall + code->sys_syscall_offs);
    }
    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}


static byte *
emit_dispatch_template(dcontext_t *dcontext, byte *pc, uint offset)
{
    instrlist_t ilist;

    /* PR 244737: we don't use this for x64 b/c syscall routines are thread-shared */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* load %edi w/the dcontext */
    insert_shared_get_dcontext(dcontext, &ilist, NULL, true);

    /* load the generated_code_t address */
    APP(&ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EDI),
                                    OPND_DC_FIELD(false, dcontext, OPSZ_PTR,
                                                  PRIVATE_CODE_OFFSET)));

    /* jump thru the address in the offset */
    APP(&ilist, INSTR_CREATE_jmp_ind(dcontext, OPND_CREATE_MEM32(REG_EDI, offset)));

    pc = instrlist_encode(dcontext, &ilist, pc, false /* no instr targets */);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}

byte *
emit_shared_syscall_dispatch(dcontext_t *dcontext, byte *pc)
{
    return emit_dispatch_template(dcontext, pc,
                                  offsetof(generated_code_t, shared_syscall));
}

byte *
emit_unlinked_shared_syscall_dispatch(dcontext_t *dcontext, byte *pc)
{
    return emit_dispatch_template(dcontext, pc, offsetof(generated_code_t,
                                                         unlinked_shared_syscall));
}

/* Links the shared_syscall routine to go directly to the indirect branch
 * lookup routine.
 * If it is already linked, does nothing.
 * Assumes caller takes care of any synchronization if this is called
 * from other than the owning thread!
 */
/* NOTE the link/unlink of shared syscall is atomic w/respect to threads in the
 * cache since is only single byte write (always atomic). */
static void
link_shared_syscall_common(generated_code_t *code)
{
    /* strategy: change "jmp unlink" back to "jecxz unlink" */
    cache_pc pc;
    if (code == NULL) /* shared_code_x86 */
        return;
    pc = code->unlinked_shared_syscall + code->sys_unlink_offs;
    if (*pc != JECXZ_OPCODE) {
        protect_generated_code(code, WRITABLE);
        ASSERT(*pc == JMP_SHORT_OPCODE);
        *pc = JECXZ_OPCODE;
        protect_generated_code(code, READONLY);
    }
}

void
link_shared_syscall(dcontext_t *dcontext)
{
    ASSERT(IS_SHARED_SYSCALL_THREAD_SHARED || dcontext != GLOBAL_DCONTEXT);
    if (dcontext == GLOBAL_DCONTEXT) {
        link_shared_syscall_common(SHARED_GENCODE(GENCODE_X64));
#ifdef X64
        /* N.B.: there are no 32-bit syscalls for WOW64 with 64-bit DR (i#821) */
        if (DYNAMO_OPTION(x86_to_x64))
            link_shared_syscall_common(SHARED_GENCODE(GENCODE_X86_TO_X64));
#endif
    } else
        link_shared_syscall_common(THREAD_GENCODE(dcontext));
}

/* Unlinks the shared_syscall routine so it goes back to dispatch after
 * the system call itself.
 * If it is already unlinked, does nothing.
 * Assumes caller takes care of any synchronization if this is called
 * from other than the owning thread!
 */
static void
unlink_shared_syscall_common(generated_code_t *code)
{
    /* strategy: change "jecxz unlink" to "jmp unlink" */
    cache_pc pc;
    if (code == NULL) /* shared_code_x86 */
        return;
    pc = code->unlinked_shared_syscall + code->sys_unlink_offs;
    if (*pc != JMP_SHORT_OPCODE) {
        protect_generated_code(code, WRITABLE);
        ASSERT(*pc == JECXZ_OPCODE);
        *pc = JMP_SHORT_OPCODE;
        protect_generated_code(code, READONLY);
    }
}

void
unlink_shared_syscall(dcontext_t *dcontext)
{
    ASSERT(IS_SHARED_SYSCALL_THREAD_SHARED || dcontext != GLOBAL_DCONTEXT);
    if (dcontext == GLOBAL_DCONTEXT) {
        unlink_shared_syscall_common(SHARED_GENCODE(GENCODE_X64));
#ifdef X64
        /* N.B.: there are no 32-bit syscalls for WOW64 with 64-bit DR (i#821) */
        if (DYNAMO_OPTION(x86_to_x64))
            unlink_shared_syscall_common(SHARED_GENCODE(GENCODE_X86_TO_X64));
#endif
    } else
        unlink_shared_syscall_common(THREAD_GENCODE(dcontext));
}

#endif /* defined(WINDOWS) ****************************/

#ifdef WINDOWS
/* used by detach, this inlines the callback stack so that we can detach
 *
 * we spill xax and xbx to the PID and TID (respectively) TLS slots until we find
 * the thread private state at which point we switch to using it for spilling.  We
 * use the TID slot (as opposed to the PEB slot that callback.c uses) because we need
 * to get the TID anyways.
 *
 * note the counter walks backwards through the array of saved address (they are
 * stored in reverse order)
 *
 * FIXME - we clobber eflags, but those should be dead after a system call anyways.
 *
 * From emit_patch_syscall()
 * after_shared_syscall:
 *   jmp _after_do_syscall
 *
 * after_do_syscall:
 *   mov xax -> PID in TEB
 *   mov &callback_buf -> xax
 *   jmp xax
 *
 *
 * From emit_detach_callback_code()
 * // xax is currently saved in PID slot of TEB
 *  callback_buf:
 *   xchg xbx, TID in TEB  // store xbx and get TID
 *   mov &callback_state -> xax  //the array of detach_callback_stack_t
 *  match_tid:
 *   cmp xbx, thread_id_offset(xax)
 *   je match_found
 *   add xax, sizeof(detach_callback_stack_t)
 *   jmp match_tid  // Note - infinite loop till find or crash (not clear what else to do)
 *  match_found:  // xax now holds ptr to the detach_callback_stack_t for this thread
 *   xchg xbx, TID in TEB  // restore tid & xbx
 *   mov xbx -> xbx_save_offset(xax)
 *   mov PID -> xbx
 *   xchg xbx, PID in TEB  // restore pid, saved xax now in xbx
 *   mov xbx -> xax_save_offset(xax)
 *   mov xcx -> xcx_save_offset(xax)
 *   mov count_offset(xax) -> xbx  // need count in register for addr calculation below
 *   sub xbx, 1
 *   mov xbx -> count_offset(xax)
 *   mov callback_addrs_offset(xax) -> xcx
 *   mov (xcx + xbx*sizeof(app_pc)) -> xcx // xcx now holds the xip we need to go to
 *   mov xcx -> target_offset(xax)
 *   mov xcx_save_offset(xax) -> xcx
 *   mov xbx_save_offset(xax) -> xbx
 *   lea code_buf_offset(xax) -> xax
 *   jmp xax
 *
 214f1000 6764871e2400     xchg    fs:[0024],ebx
 214f1006 b800114f21       mov     eax,0x214f1100
 214f100b 3b18             cmp     ebx,[eax]
 214f100d 0f8408000000     je      214f101b
 214f1013 83c03c           add     eax,0x3c
 214f1016 e9f0ffffff       jmp     214f100b
 214f101b 6764871e2400     xchg    fs:[0024],ebx
 214f1021 895810           mov     [eax+0x10],ebx
 214f1024 bb5c040000       mov     ebx,0x45c
 214f1029 6764871e2000     xchg    fs:[0020],ebx
 214f102f 89580c           mov     [eax+0xc],ebx
 214f1032 894814           mov     [eax+0x14],ecx
 214f1035 8b5804           mov     ebx,[eax+0x4]
 214f1038 83eb01           sub     ebx,0x1
 214f103b 895804           mov     [eax+0x4],ebx
 214f103e 8b4808           mov     ecx,[eax+0x8]
 214f1041 8b0c99           mov     ecx,[ecx+ebx*4]
 214f1044 894818           mov     [eax+0x18],ecx
 214f1047 8b4814           mov     ecx,[eax+0x14]
 214f104a 8b5810           mov     ebx,[eax+0x10]
 214f104d 8d401c           lea     eax,[eax+0x1c]
 214f1050 ffe0             jmp     eax
 *
 *
 * From emit_detach_callback_final_jmp()
 * _detach_callback_stack_t.code_buf (thread private)
 *   mov (xax_save_offset) -> xax
 *   jmp *target
 *
 214f111c a10c114f21       mov     eax,[214f110c]
 214f1121 ff2518114f21     jmp     dword ptr [214f1118]
 */
byte *
emit_detach_callback_code(dcontext_t *dcontext, byte *buf,
                          detach_callback_stack_t *callback_state)
{
    byte *pc = buf;
    instrlist_t ilist;
    instr_t *match_tid = INSTR_CREATE_label(dcontext),
        *match_found = INSTR_CREATE_label(dcontext);

    /* i#821/PR 284029: for now we assume there are no syscalls in x86 code, so
     * we do not need to generate an x86 version
     */

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* create instructions */
    APP(&ilist, INSTR_CREATE_xchg(dcontext, opnd_create_tls_slot(TID_TIB_OFFSET),
                                  opnd_create_reg(REG_XBX)));
    APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                     OPND_CREATE_INTPTR((ptr_uint_t)callback_state)));
    APP(&ilist, match_tid);
    /* FIXME - we clobber eflags.  We don't anticipate that being a problem on callback
     * returns since syscalls clobber eflags too. */
    APP(&ilist, INSTR_CREATE_cmp
        (dcontext, opnd_create_reg(REG_XBX),
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, tid))));
    APP(&ilist, INSTR_CREATE_jcc_short(dcontext, OP_je, opnd_create_instr(match_found)));
    APP(&ilist, INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XAX),
                                 OPND_CREATE_INT_32OR8(sizeof(detach_callback_stack_t))));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_instr(match_tid)));
    APP(&ilist, match_found);
    /* found matching tid ptr is in xax
     * spill registers into local slots and restore TEB fields */
    APP(&ilist, INSTR_CREATE_xchg(dcontext, opnd_create_tls_slot(TID_TIB_OFFSET),
                                  opnd_create_reg(REG_XBX)));
    APP(&ilist, INSTR_CREATE_mov_st
        (dcontext,
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, xbx_save)),
         opnd_create_reg(REG_XBX)));
    APP(&ilist, INSTR_CREATE_mov_imm
        (dcontext, opnd_create_reg(REG_XBX),
         OPND_CREATE_INTPTR((ptr_uint_t)get_process_id())));
    APP(&ilist, INSTR_CREATE_xchg(dcontext, opnd_create_tls_slot(PID_TIB_OFFSET),
                                  opnd_create_reg(REG_XBX)));
    APP(&ilist, INSTR_CREATE_mov_st
        (dcontext,
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, xax_save)),
         opnd_create_reg(REG_XBX)));
    APP(&ilist, INSTR_CREATE_mov_st
        (dcontext,
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, xcx_save)),
         opnd_create_reg(REG_XCX)));
    /* now find the right address and move it into target while updating the
     * thread private count */
    APP(&ilist, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(REG_XBX),
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, count))));
    /* see earlier comment on clobbering eflags */
    APP(&ilist, INSTR_CREATE_sub(dcontext, opnd_create_reg(REG_XBX),
                                 OPND_CREATE_INT8(1)));
    APP(&ilist, INSTR_CREATE_mov_st
        (dcontext,
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, count)),
         opnd_create_reg(REG_XBX)));
    APP(&ilist, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(REG_XCX),
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, callback_addrs))));
    APP(&ilist, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(REG_XCX),
         opnd_create_base_disp(REG_XCX, REG_XBX, sizeof(app_pc), 0, OPSZ_PTR)));
    APP(&ilist, INSTR_CREATE_mov_st
        (dcontext,
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, target)),
         opnd_create_reg(REG_XCX)));
    APP(&ilist, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(REG_XCX),
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, xcx_save))));
    APP(&ilist, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(REG_XBX),
         OPND_CREATE_MEMPTR(REG_XAX, offsetof(detach_callback_stack_t, xbx_save))));
    APP(&ilist, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XAX),
         OPND_CREATE_MEM_lea(REG_XAX, REG_NULL, 0,
                             offsetof(detach_callback_stack_t, code_buf))));
    APP(&ilist, INSTR_CREATE_jmp_ind(dcontext, opnd_create_reg(REG_XAX)));

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, true /* instr targets */);
    ASSERT(pc != NULL);
    ASSERT(pc - buf < DETACH_CALLBACK_CODE_SIZE);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}

void
emit_detach_callback_final_jmp(dcontext_t *dcontext,
                               detach_callback_stack_t *callback_state)
{
    byte *pc = callback_state->code_buf;
    instrlist_t ilist;

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* restore eax and jmp target */
    APP(&ilist, INSTR_CREATE_mov_ld
        (dcontext, opnd_create_reg(REG_XAX),
         OPND_CREATE_ABSMEM(&(callback_state->xax_save), OPSZ_PTR)));
    APP(&ilist, INSTR_CREATE_jmp_ind
        (dcontext, OPND_CREATE_ABSMEM(&(callback_state->target), OPSZ_PTR)));

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, true /* instr targets */);
    ASSERT(pc != NULL);
    ASSERT(pc - callback_state->code_buf < DETACH_CALLBACK_FINAL_JMP_SIZE);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
}


void
emit_patch_syscall(dcontext_t *dcontext, byte *target _IF_X64(gencode_mode_t mode))
{
    byte *pc = after_do_syscall_code_ex(dcontext _IF_X64(mode));
    instrlist_t ilist;

    if (DYNAMO_OPTION(shared_syscalls)) {
        /* Simply patch shared_syscall to jump to after_do_syscall. Only
         * one array of callback stack addresses is needed -- a return from
         * a callback entered from shared_syscall will jump to the patched
         * after_do_syscall and fetch the correct address off of our
         * callback stack copy. It "just works".
         */
        instr_t *instr = INSTR_CREATE_jmp(dcontext, opnd_create_pc(pc));
        DEBUG_DECLARE(byte *nxt_pc =)
            instr_encode(dcontext, instr,
                         after_shared_syscall_code_ex(dcontext _IF_X64(mode)));
        ASSERT(nxt_pc != NULL);
        /* check that there was room - shared_syscall should be before do_syscall
         * anything between them is dead at this point */
        ASSERT(after_shared_syscall_code_ex(dcontext _IF_X64(mode)) < pc && nxt_pc < pc);
        instr_destroy(dcontext, instr);
        LOG(THREAD, LOG_EMIT, 2,
            "Finished patching shared syscall routine for detach -- patch "PFX
            " to jump to "PFX"\n", after_shared_syscall_code(dcontext), pc);
    }

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* patch do_syscall to jmp to target */
    /* Note that on 64-bit target may not be reachable in which case we need to inline
     * the first register spill here so we can jmp reg. We go ahead and the spill here
     * and jmp through reg for 32-bit as well for consistency. */
    APP(&ilist, INSTR_CREATE_mov_st(dcontext, opnd_create_tls_slot(PID_TIB_OFFSET),
                                    opnd_create_reg(REG_XAX)));
    APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                     OPND_CREATE_INTPTR((ptr_uint_t)target)));
    APP(&ilist, INSTR_CREATE_jmp_ind(dcontext, opnd_create_reg(REG_XAX)));

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, true /* instr targets */);
    ASSERT(pc != NULL);
    /* ASSERT that there was enough space after the system call (everything after
     * do_syscall should be dead at this point). */
    ASSERT(pc <= get_emitted_routines_code(dcontext _IF_X64(mode))->commit_end_pc);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);
}
#endif /* WINDOWS */

/* this routine performs a single system call instruction and then returns
 * to dynamo via fcache_return
 */
static byte *
emit_do_syscall_common(dcontext_t *dcontext, generated_code_t *code,
                       byte *pc, byte *fcache_return_pc,
                       bool handle_clone, bool thread_shared, int interrupt,
                       instr_t *syscall_instr, uint *syscall_offs /*OUT*/)
{
    instrlist_t ilist;
    instr_t *syscall;
#ifdef UNIX
    instr_t *post_syscall;
#endif

#if defined(UNIX) && !defined(X64)
    /* PR 286922: 32-bit clone syscall cannot use vsyscall: must be int */
    if (handle_clone) {
        ASSERT(interrupt == 0 || interrupt == 0x80);
        interrupt = 0x80;
    }
#endif
    if (syscall_instr != NULL)
        syscall = syscall_instr;
    else {
        if (interrupt != 0) {
            syscall = INSTR_CREATE_int(dcontext,
                                       opnd_create_immed_int((char)interrupt, OPSZ_1));
        } else
            syscall = create_syscall_instr(dcontext);
    }

    /* i#821/PR 284029: for now we assume there are no syscalls in x86 code.
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!GENCODE_IS_X86(code->gencode_mode)));

    ASSERT(syscall_offs != NULL);
    *syscall_offs = instr_length(dcontext, syscall);

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* system call itself -- using same method we've observed OS using */
    APP(&ilist, syscall);
#ifdef UNIX
    if (get_syscall_method() == SYSCALL_METHOD_UNINITIALIZED) {
        /* Since we lazily find out the method, but emit these routines
         * up front, we have to leave room for the longest syscall method.
         * This used to the 6-byte LOL64 call* but we now walk into that
         * call* (PR 286922).  Not much of a perf worry, but if we
         * ever have proactive syscall determination on linux we should
         * remove these nops.
         */
        ASSERT(instr_length(dcontext, instrlist_last(&ilist)) == 2);
        if (SYSCALL_METHOD_LONGEST_INSTR == 6) {
            /* we could add 4-byte nop support but I'm too lazy */
            APP(&ilist, INSTR_CREATE_nop3byte(dcontext));
            APP(&ilist, INSTR_CREATE_nop1byte(dcontext));
        } else
            ASSERT_NOT_IMPLEMENTED(instr_length(dcontext, instrlist_last(&ilist)) ==
                                   SYSCALL_METHOD_LONGEST_INSTR);
    }
    post_syscall = instrlist_last(&ilist);
#endif

    /* go to fcache return -- use special syscall linkstub */
    /* in case it returns: go to fcache return -- use 0 as &linkstub */
    if (thread_shared)
        APP(&ilist, instr_create_save_to_tls(dcontext, REG_XAX, TLS_XAX_SLOT));
    else
        APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    APP(&ilist, INSTR_CREATE_mov_imm
        (dcontext, opnd_create_reg(REG_XAX),
         OPND_CREATE_INTPTR((ptr_int_t)get_syscall_linkstub())));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(fcache_return_pc)));

#ifdef UNIX
    if (handle_clone) {
        /* put in clone code, and make sure to target it.
         * do it here since it assumes an instr after the syscall exists.
         */
        mangle_insert_clone_code(dcontext, &ilist, post_syscall,
                                 false /*do not skip*/
                                 _IF_X64(code->gencode_mode));
    }
#endif

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc,
#ifdef UNIX
                          handle_clone /* instr targets */
#else
                          false /* no instr targets */
#endif
                          );
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}

#ifdef WINDOWS
/* like fcache_enter but indirects the dcontext passed in through edi */
byte *
emit_fcache_enter_indirect(dcontext_t *dcontext, generated_code_t *code,
                           byte *pc, byte *fcache_return_pc)
{
    return emit_fcache_enter_common(dcontext, code, pc,
                                    false/*indirect*/, false/*!shared*/);
}

/* This routine performs an int 2b, which maps to NtCallbackReturn, and then returns
 * to dynamo via fcache_return (though it won't reach there)
 */
byte *
emit_do_callback_return(dcontext_t *dcontext, byte *pc, byte *fcache_return_pc,
                        bool thread_shared)
{
    instrlist_t ilist;

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* interrupt 2b */
    APP(&ilist, INSTR_CREATE_int(dcontext, opnd_create_immed_int(0x2b, OPSZ_1)));

    /* in case it returns: go to fcache return -- use 0 as &linkstub */
    if (thread_shared)
        APP(&ilist, instr_create_save_to_tls(dcontext, REG_XAX, TLS_XAX_SLOT));
    else
        APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    /* for x64 we rely on sign-extension to fill out rax */
    APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EAX),
                                     OPND_CREATE_INT32(0)));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(fcache_return_pc)));

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, false /* no instr targets */);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}
#else /* !WINDOWS => UNIX */
byte *
emit_do_clone_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      byte *fcache_return_pc, bool thread_shared,
                      uint *syscall_offs /*OUT*/)
{
    return emit_do_syscall_common(dcontext, code, pc, fcache_return_pc,
                                  true, thread_shared, false, NULL, syscall_offs);
}
# ifdef VMX86_SERVER
byte *
emit_do_vmkuw_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      byte *fcache_return_pc, bool thread_shared,
                      uint *syscall_offs /*OUT*/)
{
    instr_t *gateway = INSTR_CREATE_int
        (dcontext, opnd_create_immed_int((char)VMKUW_SYSCALL_GATEWAY, OPSZ_1));
    return emit_do_syscall_common(dcontext, code, pc, fcache_return_pc,
                                  false, thread_shared, false, gateway, syscall_offs);
}
# endif
#endif /* UNIX */

byte *
emit_do_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                byte *fcache_return_pc, bool thread_shared, int interrupt,
                uint *syscall_offs /*OUT*/)
{
    pc = emit_do_syscall_common(dcontext, code, pc, fcache_return_pc,
                                false, thread_shared, interrupt, NULL, syscall_offs);
    return pc;
}

#ifndef WINDOWS
/* updates first syscall instr it finds with the new method of syscall */
static void
update_syscall(dcontext_t *dcontext, byte *pc)
{
    LOG_DECLARE(byte *start_pc = pc;)
    byte *prev_pc;
    instr_t instr;
    instr_init(dcontext, &instr);

    do {
        prev_pc = pc;
        instr_reset(dcontext, &instr);
        pc = decode_cti(dcontext, pc, &instr);
        ASSERT(pc != NULL); /* this our own code we're decoding, should be valid */
        if (instr_is_syscall(&instr)) {
            instr_t *newinst = create_syscall_instr(dcontext);
            byte *nxt_pc = instr_encode(dcontext, newinst, prev_pc);
            /* instruction must not change size! */
            ASSERT(nxt_pc != NULL);
            if (nxt_pc != pc) {
                pc = nxt_pc;
                byte *stop_pc = prev_pc + SYSCALL_METHOD_LONGEST_INSTR;
                ASSERT(nxt_pc <= stop_pc);
                while (pc < stop_pc) {
                    /* we could add >3-byte nop support but I'm too lazy */
                    int noplen = MIN(stop_pc - pc, 3);
                    instr_t *nop = instr_create_nbyte_nop(dcontext, noplen, true);
                    pc = instr_encode(dcontext, nop, pc);
                    ASSERT(pc != NULL);
                    instr_destroy(dcontext, nop);
                }
            }
            instr_destroy(dcontext, newinst);
            break;
        }
        ASSERT(pc - prev_pc < 128);
    } while (1);

    instr_free(dcontext, &instr);

    DOLOG(3, LOG_EMIT, {
        LOG(THREAD, LOG_EMIT, 3, "Just updated syscall routine:\n");
        prev_pc = pc;
        pc = start_pc;
        do {
            pc = disassemble_with_bytes(dcontext, pc, THREAD);
        } while (pc < prev_pc + 1); /* +1 to get next instr */
        LOG(THREAD, LOG_EMIT, 3, "  ...\n");
    });
}

void
update_syscalls(dcontext_t *dcontext)
{
    byte *pc;
    pc = get_do_syscall_entry(dcontext);
    update_syscall(dcontext, pc);
#ifdef X64
    /* PR 286922: for 32-bit, we do NOT update the clone syscall as it
     * always uses int (since can't use call to vsyscall when swapping
     * stacks!)
     */
    pc = get_do_clone_syscall_entry(dcontext);
    update_syscall(dcontext, pc);
#endif
}
#endif /* !WINDOWS */

/* Returns -1 on failure */
int
decode_syscall_num(dcontext_t *dcontext, byte *entry)
{
    byte *pc;
    int syscall = -1;
    instr_t instr;
    ASSERT(entry != NULL);
    instr_init(dcontext, &instr);
    pc = entry;
    LOG(GLOBAL, LOG_EMIT, 3, "decode_syscall_num "PFX"\n", entry);
    while (true) {
        DOLOG(3, LOG_EMIT, { disassemble_with_bytes(dcontext, pc, GLOBAL); });
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        if (pc == NULL)
            break; /* give up gracefully */
        /* we do not handle control transfer instructions! */
        if (instr_is_cti(&instr)) {
#ifdef WINDOWS /* since no interception code buffer to check on linux */
            if (DYNAMO_OPTION(native_exec_syscalls) && instr_is_ubr(&instr)) {
                /* probably our own trampoline, follow it
                 * ASSUMPTION: mov eax is the instr that jmp targets: i.e.,
                 * we don't handle deep hooks here.
                 */
                if (!is_syscall_trampoline(opnd_get_pc(instr_get_target(&instr)), &pc)) {
                    break;  /* give up gracefully */
                } /* else, carry on at pc */
            } else
#endif
                break; /* give up gracefully */
        }
        if (instr_num_dsts(&instr) > 0 &&
            opnd_is_reg(instr_get_dst(&instr, 0)) &&
            opnd_get_reg(instr_get_dst(&instr, 0)) == REG_XAX) {
            if (instr_get_opcode(&instr) == OP_mov_imm) {
                IF_X64(ASSERT_TRUNCATE(int, int,
                                       opnd_get_immed_int(instr_get_src(&instr, 0))));
                syscall = (int) opnd_get_immed_int(instr_get_src(&instr, 0));
                LOG(GLOBAL, LOG_EMIT, 3, "\tfound syscall num: 0x%x\n", syscall);
                break;
            } else
                break; /* give up gracefully */
        }
    }
    instr_free(dcontext, &instr);
    return syscall;
}

/* If code_buf points to a jmp rel32 returns true and returns the target of
 * the jmp in jmp_target as if was located at app_loc. */
bool
is_jmp_rel32(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */)
{
    if (*code_buf == JMP_OPCODE) {
        if (jmp_target != NULL) {
            *jmp_target = app_loc + JMP_LONG_LENGTH +
                *(int *)(code_buf+1);
        }
        return true;
    }
    return false;
}

/* If code_buf points to a jmp rel8 returns true and returns the target of
 * the jmp in jmp_target as if was located at app_loc. */
bool
is_jmp_rel8(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */)
{
    if (*code_buf == JMP_SHORT_OPCODE) {
        if (jmp_target != NULL) {
            *jmp_target = app_loc + JMP_SHORT_LENGTH +
                *(char *)(code_buf+1);
        }
        return true;
    }
    return false;
}

#ifdef UNIX
/* PR 212290: can't be static code in x86.asm since it can't be PIC */
/*
 * new_thread_dynamo_start - for initializing a new thread created
 * via the clone system call.
 * assumptions:
 *   1) app's xcx is in xax.
 *   2) xcx contains clone_record_t, which is not 0.
 *   3) app's xax should contain 0.
 *   4) this thread holds initstack_mutex
 */
byte *
emit_new_thread_dynamo_start(dcontext_t *dcontext, byte *pc)
{
    instrlist_t ilist;
    uint offset;

    /* initialize the ilist */
    instrlist_init(&ilist);

    /* Since we don't have TLS available here (we could use CLONE_SETTLS
     * for kernel 2.5.32+: PR 285898) we can't non-racily acquire
     * initstack_mutex as we can't spill or spare a register
     * (xref i#101/PR 207903).
     */

    /* Restore app xcx and xax, which were swapped post-syscall to distinguish
     * parent from child.
     */
    APP(&ilist, INSTR_CREATE_xchg
        (dcontext, opnd_create_reg(REG_XAX), opnd_create_reg(REG_XCX)));

    /* grab exec state and pass as param in a priv_mcontext_t struct
     * new_thread_setup will restore real app xsp and xax
     * we emulate x86.asm's PUSH_DR_MCONTEXT(REG_XAX) (for priv_mcontext_t.pc)
     */
    offset = insert_push_all_registers(dcontext, NULL, &ilist, NULL,
                                       IF_X64_ELSE(16, 4),
                                       INSTR_CREATE_push(dcontext,
                                                         opnd_create_reg(REG_XAX)));
    /* put pre-push xsp into priv_mcontext_t.xsp slot */
    ASSERT(offset == sizeof(priv_mcontext_t));
    APP(&ilist, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XAX),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, sizeof(priv_mcontext_t))));
    APP(&ilist, INSTR_CREATE_mov_st
        (dcontext, OPND_CREATE_MEMPTR(REG_XSP, offsetof(priv_mcontext_t, xsp)),
         opnd_create_reg(REG_XAX)));

    /* We avoid get_thread_id syscall in get_thread_private_dcontext()
     * by clearing the segment register here (cheaper check than syscall)
     * (xref PR 192231).  If we crash prior to this point though, the
     * signal handler will get the wrong dcontext, but that's a small window.
     * See comments in get_thread_private_dcontext() for alternatives.
     */
    APP(&ilist, INSTR_CREATE_mov_imm
        (dcontext, opnd_create_reg(REG_AX), OPND_CREATE_INT16(0)));
    APP(&ilist, INSTR_CREATE_mov_seg
        (dcontext, opnd_create_reg(SEG_TLS), opnd_create_reg(REG_AX)));
    /* stack grew down, so priv_mcontext_t at tos */
    APP(&ilist, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(REG_XAX),
         OPND_CREATE_MEM_lea(REG_XSP, REG_NULL, 0, 0)));

    dr_insert_call(dcontext, &ilist, NULL, (void *)new_thread_setup,
                   1, opnd_create_reg(REG_XAX));

    /* should not return */
    insert_reachable_cti(dcontext, &ilist, NULL, vmcode_get_start(),
                         (byte *)unexpected_return, true/*jmp*/, false/*!precise*/,
                         DR_REG_R11/*scratch*/, NULL);

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, true /* instr targets */);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}
#endif /* UNIX */

#ifdef TRACE_HEAD_CACHE_INCR
/* trace_t heads come here instead of back to dynamo to have their counters
 * incremented.
 */
byte *
emit_trace_head_incr(dcontext_t *dcontext, byte *pc, byte *fcache_return_pc)
{
    /*  save ecx
        save eax->xbx slot
        mov target_fragment_offs(eax), eax
        movzx counter_offs(eax), ecx
        lea 1(ecx), ecx                 # increment counter
        mov data16 cx, counter_offs(eax)
        lea -hot_threshold(ecx), ecx    # compare to hot_threshold
        jecxz is_hot
        mov start_pc_offs(eax), ecx
        movzx prefix_size_offs(eax), eax
        lea (ecx,eax,1), ecx
        mov ecx, trace_head_pc_offs + dcontext   # special slot to avoid target prefix
        restore ecx
        restore eax
        jmp * trace_head_pc_offs + dcontext
      is_hot:
        restore ebx slot to eax         # put &l into eax
        restore ecx
        jmp fcache_return
     */
    instrlist_t ilist;
    instr_t *is_hot =
        instr_create_restore_from_dcontext(dcontext, REG_EAX, XBX_OFFSET);
    instr_t *in;

    /* PR 248210: unsupported feature on x64 */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    instrlist_init(&ilist);
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_ECX, XCX_OFFSET));
    if (DYNAMO_OPTION(shared_bbs)) {
        /* HACK to get shared exit stub, which puts eax into fs:scratch1, to work
         * w/ thread-private THCI: we pull eax out of the tls slot and into mcontext.
         * This requires that all direct stubs for cti that can link to trace
         * heads use the shared stub -- so if traces can link to trace heads, their
         * exits must use the shared stubs, even if the traces are thread-private.
         */
        APP(&ilist, RESTORE_FROM_TLS(dcontext, REG_ECX, EXIT_STUB_SPILL_SLOT));
        APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_ECX, XAX_OFFSET));
    }
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_EAX, XBX_OFFSET));
    APP(&ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_EAX),
                            OPND_CREATE_MEM32(REG_EAX, LINKSTUB_TARGET_FRAG_OFFS)));
    ASSERT_NOT_IMPLEMENTED(false && "must handle LINKSTUB_CBR_FALLTHROUGH case by calculating target tag")
    APP(&ilist, INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_ECX),
                                   opnd_create_base_disp(REG_EAX, REG_NULL, 0,
                                                         FRAGMENT_COUNTER_OFFS, OPSZ_2)));
    APP(&ilist, INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                              opnd_create_base_disp(REG_ECX, REG_NULL, 0, 1, OPSZ_lea)));
    /* data16 prefix is set auto-magically */
    APP(&ilist, INSTR_CREATE_mov_st(dcontext,
                                    opnd_create_base_disp(REG_EAX, REG_NULL, 0,
                                                          FRAGMENT_COUNTER_OFFS, OPSZ_2),
                                    opnd_create_reg(REG_CX)));
    APP(&ilist, INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                              opnd_create_base_disp(REG_ECX, REG_NULL, 0,
                                            -((int)INTERNAL_OPTION(trace_threshold)), OPSZ_lea)));
    APP(&ilist, INSTR_CREATE_jecxz(dcontext, opnd_create_instr(is_hot)));
    APP(&ilist, INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_ECX),
                         OPND_CREATE_MEM32(REG_EAX, FRAGMENT_START_PC_OFFS)));
    APP(&ilist, INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EAX),
                                   opnd_create_base_disp(REG_EAX, REG_NULL, 0,
                                                         FRAGMENT_PREFIX_SIZE_OFFS, OPSZ_1)));
    APP(&ilist, INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                                 opnd_create_base_disp(REG_ECX, REG_EAX, 1, 0, OPSZ_lea)));
    APP(&ilist, instr_create_save_to_dcontext(dcontext, REG_ECX, TRACE_HEAD_PC_OFFSET));
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_ECX, XCX_OFFSET));
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_EAX, XAX_OFFSET));
    APP(&ilist, INSTR_CREATE_jmp_ind(dcontext,
                          opnd_create_dcontext_field(dcontext, TRACE_HEAD_PC_OFFSET)));
    APP(&ilist, is_hot);
    APP(&ilist, instr_create_restore_from_dcontext(dcontext, REG_ECX, XCX_OFFSET));
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(fcache_return_pc)));

    /* now encode the instructions */
    pc = instrlist_encode(dcontext, &ilist, pc, true /* instr targets */);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}

byte *
emit_trace_head_incr_shared(dcontext_t *dcontext, byte *pc, byte *fcache_return_pc)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

#endif /* TRACE_HEAD_CACHE_INCR */

/***************************************************************************
 * SPECIAL IBL XFER ROUTINES
 */

static inline byte *
special_ibl_xfer_tgt(dcontext_t *dcontext, generated_code_t *code,
                     ibl_entry_point_type_t entry_type,
                     ibl_branch_type_t ibl_type)
{
    /* We use the trace ibl so that the target will be a trace head,
     * avoiding a trace disruption.
     * We request that bbs doing this xfer are marked DR_EMIT_MUST_END_TRACE.
     * We use the ret ibt b/c we figure most uses will involve rets and there's
     * no reason to fill up the jmp ibt.
     * This feature is unavail for prog shep b/c of the cross-type pollution.
     */
    return get_ibl_routine_ex(dcontext, entry_type,
                              DYNAMO_OPTION(disable_traces) ?
                              (code->thread_shared ? IBL_BB_SHARED : IBL_BB_PRIVATE) :
                              (code->thread_shared ? IBL_TRACE_SHARED : IBL_TRACE_PRIVATE),
                              ibl_type
                              _IF_X64(code->gencode_mode));
}

/* We only need a thread-private version if our ibl target is thread-private */
bool
special_ibl_xfer_is_thread_private(void)
{
#ifdef X64
    return false; /* all gencode is shared */
#else
    return (DYNAMO_OPTION(disable_traces) ?
            !DYNAMO_OPTION(shared_bbs) : !DYNAMO_OPTION(shared_traces));
#endif
}

/* emit the special_ibl trampoline code for transferring the control flow to
 * ibl lookup
 * - index: the index of special_ibl array to be emitted to
 * - ibl_type: the branch type (IBL_RETURN or IBL_INDCALL)
 * - custom_ilist: the custom instructions added by caller, which are added at
 *   the end of trampoline and right before jump to the ibl routine
 * - tgt: the opnd holding the target, which will be moved into XCX for ibl.
 */
static byte *
emit_special_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code,
                      uint index,
                      ibl_branch_type_t ibl_type,
                      instrlist_t *custom_ilist, opnd_t tgt)
{
    instrlist_t ilist;
    patch_list_t patch;
    instr_t *in;
    size_t len;
    byte *ibl_tgt = special_ibl_xfer_tgt(dcontext, code, IBL_LINKED, ibl_type);
    bool absolute = !code->thread_shared;

    ASSERT(ibl_tgt != NULL);
    instrlist_init(&ilist);
    init_patch_list(&patch, absolute ? PATCH_TYPE_ABSOLUTE : PATCH_TYPE_INDIRECT_FS);

    if (DYNAMO_OPTION(indirect_stubs)) {
        const linkstub_t *linkstub =
            get_special_ibl_linkstub(ibl_type,
                                     DYNAMO_OPTION(disable_traces) ? false : true);
        APP(&ilist, SAVE_TO_TLS(dcontext, REG_XBX, TLS_XBX_SLOT));
        APP(&ilist, INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XBX),
                                         OPND_CREATE_INTPTR((ptr_int_t)linkstub)));
    }

    if (code->thread_shared || DYNAMO_OPTION(private_ib_in_tls)) {
#ifdef X64
        if (GENCODE_IS_X86_TO_X64(code->gencode_mode) &&
            DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            APP(&ilist, SAVE_TO_REG(dcontext, REG_XCX, REG_R9));
        } else
#endif
            APP(&ilist, SAVE_TO_TLS(dcontext, REG_XCX, MANGLE_XCX_SPILL_SLOT));
    } else {
        APP(&ilist, SAVE_TO_DC(dcontext, REG_XCX, XCX_OFFSET));
    }

    APP(&ilist,
        INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX), tgt));

    /* insert customized instructions right before xfer to ibl */
    if (custom_ilist != NULL)
        in = instrlist_first(custom_ilist);
    else
        in = NULL;
    while (in != NULL) {
        instrlist_remove(custom_ilist, in);
        APP(&ilist, in);
        in = instrlist_first(custom_ilist);
    }

#ifdef X64
    if (GENCODE_IS_X86(code->gencode_mode))
        instrlist_convert_to_x86(&ilist);
#endif
    /* do not add new instrs that need conversion to x86 below here! */

    /* to support patching the 4-byte pc-rel tgt we must ensure it doesn't
     * cross a cache line
     */
    for (len = 0, in = instrlist_first(&ilist); in != NULL; in = instr_get_next(in)) {
        len += instr_length(dcontext, in);
    }
    if (CROSSES_ALIGNMENT(pc + len + 1/*opcode*/, 4, PAD_JMPS_ALIGNMENT)) {
        instr_t *nop_inst;
        len = ALIGN_FORWARD(pc + len + 1, 4) - (ptr_uint_t)(pc + len + 1);
        nop_inst = INSTR_CREATE_nopNbyte(dcontext, (uint)len);
#ifdef X64
        if (GENCODE_IS_X86(code->gencode_mode)) {
            instr_set_x86_mode(nop_inst, true/*x86*/);
            instr_shrink_to_32_bits(nop_inst);
        }
#endif
        /* XXX: better to put prior to entry point but then need to change model
         * of who assigns entry point
         */
        APP(&ilist, nop_inst);
    }
    APP(&ilist, INSTR_CREATE_jmp(dcontext, opnd_create_pc(ibl_tgt)));
    add_patch_marker(&patch, instrlist_last(&ilist),
                     PATCH_UINT_SIZED /* pc relative */,
                     0 /* point at opcode of jecxz */,
                     (ptr_uint_t*)&code->special_ibl_unlink_offs[index]);

    /* now encode the instructions */
    pc += encode_with_patch_list(dcontext, &patch, &ilist, pc);
    ASSERT(pc != NULL);

    /* free the instrlist_t elements */
    instrlist_clear(dcontext, &ilist);

    return pc;
}

static void
relink_special_ibl_xfer(dcontext_t *dcontext, int index,
                        ibl_entry_point_type_t entry_type,
                        ibl_branch_type_t ibl_type)
{
    generated_code_t *code;
    byte *pc, *ibl_tgt;
    if (dcontext == GLOBAL_DCONTEXT) {
        ASSERT(!special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = SHARED_GENCODE_MATCH_THREAD(get_thread_private_dcontext());
    } else {
#ifdef X64
        code = SHARED_GENCODE_MATCH_THREAD(dcontext);
#else
        ASSERT(special_ibl_xfer_is_thread_private()); /* else shouldn't be called */
        code = THREAD_GENCODE(dcontext);
#endif
    }
    if (code == NULL) /* shared_code_x86, or thread private that we don't need */
        return;
    ibl_tgt = special_ibl_xfer_tgt(dcontext, code, entry_type, ibl_type);
    ASSERT(code->special_ibl_xfer[index] != NULL);
    pc = (code->special_ibl_xfer[index] +
          code->special_ibl_unlink_offs[index] + 1/*jmp opcode*/);

    protect_generated_code(code, WRITABLE);
    insert_relative_target(pc, ibl_tgt, code->thread_shared/*hot patch*/);
    protect_generated_code(code, READONLY);
}

void
link_special_ibl_xfer(dcontext_t *dcontext)
{
    IF_CLIENT_INTERFACE(relink_special_ibl_xfer(dcontext, CLIENT_IBL_IDX,
                                                IBL_LINKED, IBL_RETURN);)
#ifdef UNIX
    if (DYNAMO_OPTION(native_exec_opt)) {
        relink_special_ibl_xfer(dcontext, NATIVE_PLT_IBL_IDX,
                                IBL_LINKED, IBL_INDCALL);
        relink_special_ibl_xfer(dcontext, NATIVE_RET_IBL_IDX,
                                IBL_LINKED, IBL_RETURN);
    }
#endif
}

void
unlink_special_ibl_xfer(dcontext_t *dcontext)
{
    IF_CLIENT_INTERFACE(relink_special_ibl_xfer(dcontext, CLIENT_IBL_IDX,
                                                IBL_UNLINKED, IBL_RETURN);)
#ifdef UNIX
    if (DYNAMO_OPTION(native_exec_opt)) {
        relink_special_ibl_xfer(dcontext, NATIVE_PLT_IBL_IDX,
                                IBL_UNLINKED, IBL_INDCALL);
        relink_special_ibl_xfer(dcontext, NATIVE_RET_IBL_IDX,
                                IBL_UNLINKED, IBL_RETURN);
    }
#endif
}


#ifdef CLIENT_INTERFACE
/* i#849: low-overhead xfer for clients */
byte *
emit_client_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code)
{
    /* The client puts the target in SPILL_SLOT_REDIRECT_NATIVE_TGT. */
    return emit_special_ibl_xfer(dcontext, pc, code, CLIENT_IBL_IDX,
                                 IBL_RETURN, NULL,
                                 reg_spill_slot_opnd
                                 (dcontext, SPILL_SLOT_REDIRECT_NATIVE_TGT));
}

#endif /* CLIENT_INTERFACE */

/* i#171: out-of-line clean call */
/* XXX: i#1149 the clean call context switch should be shared among all threads */
bool
client_clean_call_is_thread_private(void)
{
#ifdef X64
    return false; /* all gencode is shared */
#else
    return !USE_SHARED_GENCODE();
#endif
}

byte *
emit_clean_call_save(dcontext_t *dcontext, byte *pc, generated_code_t *code)
{
    instrlist_t ilist;

    instrlist_init(&ilist);
    /* xref insert_out_of_line_context_switch @ x86/mangle.c,
     * stack was adjusted beyond what we place there to get retaddr
     * in right spot, adjust the stack back to save context
     */
    /* XXX: this LEA can be optimized away by using the LEA
     * in insert_push_all_registers
     */
    APP(&ilist, INSTR_CREATE_lea
        (dcontext,
         opnd_create_reg(DR_REG_XSP),
         opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                               (int)(get_clean_call_switch_stack_size() +
                                     get_clean_call_temp_stack_size() +
                                     XSP_SZ /* return addr */),
                               OPSZ_lea)));
    /* save all registers */
    insert_push_all_registers(dcontext, NULL, &ilist, NULL, PAGE_SIZE,
                              INSTR_CREATE_push_imm(dcontext,
                                                    OPND_CREATE_INT32(0)));

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
    /* i#249: isolate the PEB */
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
        /* We pay the cost of this extra load of dcontext in order to get
         * this code shared (when not shared we place this where we already
         * have the dcontext in a register: see prepare_for_clean_call()).
         */
        if (SCRATCH_ALWAYS_TLS())
            insert_get_mcontext_base(dcontext, &ilist, NULL, REG_XAX);
        preinsert_swap_peb(dcontext, &ilist, NULL, !SCRATCH_ALWAYS_TLS(),
                           REG_XAX/*dc*/, REG_XCX/*scratch*/, true/*to priv*/);
        /* We also need 2 extra loads to restore the 2 regs, in case the
         * clean call passes them as args.
         */
        APP(&ilist, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(REG_XAX),
             OPND_CREATE_MEMPTR(REG_XSP, offsetof(priv_mcontext_t, xax))));
        APP(&ilist, INSTR_CREATE_mov_ld
            (dcontext, opnd_create_reg(REG_XCX),
             OPND_CREATE_MEMPTR(REG_XSP, offsetof(priv_mcontext_t, xcx))));
    }
#endif

    /* clear eflags */
    insert_clear_eflags(dcontext, NULL, &ilist, NULL);
    /* return back */
    APP(&ilist, INSTR_CREATE_lea
        (dcontext, opnd_create_reg(DR_REG_XSP),
         opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                               -(get_clean_call_temp_stack_size() +
                                 (int)XSP_SZ /* return stack */),
                               OPSZ_lea)));
    APP(&ilist, INSTR_CREATE_ret_imm
        (dcontext, OPND_CREATE_INT16(get_clean_call_temp_stack_size())));
    /* emti code */
    pc = instrlist_encode(dcontext, &ilist, pc, false);
    ASSERT(pc != NULL);
    instrlist_clear(dcontext, &ilist);
    return pc;
}

byte *
emit_clean_call_restore(dcontext_t *dcontext, byte *pc, generated_code_t *code)
{
    instrlist_t ilist;

    instrlist_init(&ilist);

#if defined(WINDOWS) && defined(CLIENT_INTERFACE)
    /* i#249: isolate the PEB */
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer()) {
        /* We pay the cost of this extra load of dcontext in order to get
         * this code shared (when not shared we place this where we already
         * have the dcontext in a register: see cleanup_after_clean_call()).
         * The 2 regs are dead as the popa will restore.
         */
        if (SCRATCH_ALWAYS_TLS())
            insert_get_mcontext_base(dcontext, &ilist, NULL, REG_XAX);
        preinsert_swap_peb(dcontext, &ilist, NULL, !SCRATCH_ALWAYS_TLS(),
                           REG_XAX/*dc*/, REG_XCX/*scratch*/, false/*to app*/);
    }
#endif

    /* adjust the stack for the return target */
    APP(&ilist, INSTR_CREATE_lea
        (dcontext,
         opnd_create_reg(DR_REG_XSP),
         opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                               (int)XSP_SZ, OPSZ_lea)));
    /* restore all registers */
    insert_pop_all_registers(dcontext, NULL, &ilist, NULL, PAGE_SIZE);
    /* return back */
    /* we adjust lea + ret_imm instead of ind jmp to take advantage of RSB */
    APP(&ilist, INSTR_CREATE_lea
        (dcontext,
         opnd_create_reg(DR_REG_XSP),
         opnd_create_base_disp(DR_REG_XSP, DR_REG_NULL, 0,
                               -(get_clean_call_switch_stack_size() +
                                 (int)XSP_SZ /* return address */),
                               OPSZ_lea)));
    APP(&ilist, INSTR_CREATE_ret_imm
        (dcontext,
         OPND_CREATE_INT16(get_clean_call_switch_stack_size())));
    /* emit code */
    pc = instrlist_encode(dcontext, &ilist, pc, false);
    ASSERT(pc != NULL);
    instrlist_clear(dcontext, &ilist);
    return pc;
}

/* mirrored inline implementation of set_last_exit() */
void
insert_set_last_exit(dcontext_t *dcontext, linkstub_t *l,
                     instrlist_t *ilist, instr_t *where, reg_id_t reg_dc)
{
    ASSERT(l != NULL);

    /* C equivalent:
     *   dcontext->last_exit = l
     */
    insert_mov_immed_ptrsz
        (dcontext, (ptr_int_t) l,
         opnd_create_dcontext_field_via_reg(dcontext, reg_dc, LAST_EXIT_OFFSET),
         ilist, where, NULL, NULL);

    /* C equivalent:
     *   dcontext->last_fragment = linkstub_fragment()
     */
    insert_mov_immed_ptrsz
        (dcontext, (ptr_int_t) linkstub_fragment(dcontext, l),
         opnd_create_dcontext_field_via_reg(dcontext, reg_dc, LAST_FRAG_OFFSET),
         ilist, where, NULL, NULL);

    /* C equivalent:
     *   dcontext->coarse_exit.dir_exit = NULL
     */
    insert_mov_immed_ptrsz
        (dcontext, (ptr_int_t) NULL,
         opnd_create_dcontext_field_via_reg(dcontext, reg_dc,
                                            COARSE_DIR_EXIT_OFFSET),
         ilist, where, NULL, NULL);
}

/* mirrored inline implementation of return_to_native() */
static void
insert_entering_native(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t reg_dc, reg_id_t reg_scratch)
{
#ifdef WINDOWS
    /* FIXME i#1238-c#1: we did not turn off asynch interception in windows */
    /* skip C equivalent:
     * set_asynch_interception(dcontext->owning_thread, false)
     */
    ASSERT_BUG_NUM(1238, false && "set_asynch_interception is not inlined");
#endif

    /* C equivalent:
     *   dcontext->thread_record->under_dynamo_control = false
     */
    PRE(ilist, where,
        instr_create_restore_from_dc_via_reg(dcontext, reg_dc, reg_scratch,
                                             THREAD_RECORD_OFFSET));
    PRE(ilist, where,
        INSTR_CREATE_mov_st(dcontext,
                            OPND_CREATE_MEM8(reg_scratch,
                                             offsetof(thread_record_t,
                                                      under_dynamo_control)),
                            OPND_CREATE_INT8(false)));

    /* C equivalent:
     *   set_last_exit(dcontext, (linkstub_t *) get_native_exec_linkstub())
     */
    insert_set_last_exit(dcontext,
                         (linkstub_t *) get_native_exec_linkstub(),
                         ilist, where, reg_dc);

    /* XXX i#1238-c#4 -native_exec_opt does not support -kstats
     * skip C equivalent:
     *   KSTOP_NOT_MATCHING(dispatch_num_exits)
     */

    /* skip C equivalent:
     *   SYSLOG_INTERNAL_WARNING_ONCE("entered at least one module natively")
     */

    /* C equivalent:
     *   whereami = WHERE_APP
     */
    PRE(ilist, where,
        instr_create_save_immed_to_dc_via_reg(dcontext, reg_dc, WHEREAMI_OFFSET,
                                              (ptr_int_t) WHERE_APP, OPSZ_4));

    /* skip C equivalent:
     *   STATS_INC(num_native_module_enter)
     */
}

/* mirrored inline implementation of return_to_native()
 * two registers are needed:
 * - reg_dc holds the dcontext
 * - reg_scratch is the scratch register.
 */
void
insert_return_to_native(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t reg_dc, reg_id_t reg_scratch)
{
    /* skip C equivalent:
     * ENTERING_DR()
     */
    ASSERT(dcontext != NULL);

    /* C equivalent:
     * entering_native(dcontext)
     */
    insert_entering_native(dcontext, ilist, where, reg_dc, reg_scratch);

    /* skip C equivalent:
     * EXITING_DR()
     */
}

#if defined(UNIX) && defined(X86)
static void
insert_entering_non_native(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                           reg_id_t reg_dc, reg_id_t reg_scratch)
{
    /* C equivalent:
     *   dcontext->thread_record->under_dynamo_control = true
     */
    PRE(ilist, where,
        instr_create_restore_from_dc_via_reg(dcontext, reg_dc, reg_scratch,
                                             THREAD_RECORD_OFFSET));
    PRE(ilist, where,
        INSTR_CREATE_mov_st(dcontext,
                            OPND_CREATE_MEM8(reg_scratch,
                                             offsetof(thread_record_t,
                                                      under_dynamo_control)),
                            OPND_CREATE_INT8(true)));

    /* C equivalent:
     *   set_last_exit(dcontext, (linkstub_t *) get_native_exec_linkstub())
     */
    insert_set_last_exit(dcontext,
                         (linkstub_t *) get_native_exec_linkstub(),
                         ilist, where, reg_dc);

    /* C equivalent:
     *   whereami = WHERE_FCACHE
     */
    PRE(ilist, where,
        instr_create_save_immed_to_dc_via_reg(dcontext, reg_dc, WHEREAMI_OFFSET,
                                              (ptr_int_t) WHERE_FCACHE, OPSZ_4));
}

/* Emit code to transfer execution from native module to code cache of non-native
 * module via plt calls.
 * The emitted code update some fields of dcontext like whereami and last_exit,
 * and jump to ibl looking for target code fragment.
 * We assume %XAX holds the target and can be clobbered.
 */
byte *
emit_native_plt_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code)
{
    instrlist_t ilist;
    opnd_t tgt = opnd_create_reg(REG_XAX);

    ASSERT(DYNAMO_OPTION(native_exec_opt));
    instrlist_init(&ilist);
    insert_shared_get_dcontext(dcontext, &ilist, NULL, true);
    insert_entering_non_native(dcontext, &ilist, NULL, REG_NULL, REG_XAX);
    insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    return emit_special_ibl_xfer(dcontext, pc, code, NATIVE_PLT_IBL_IDX,
                                 IBL_INDCALL, &ilist, tgt);
}

/* Emit code to transfer execution from native module to code cache of non-native
 * module via return.
 * The emitted code update some fields of dcontext like whereami and last_exit,
 * and jump to ibl looking for target code fragment.
 * We assume %XAX holds the target and must be restored from TLS_XAX_SLOT before
 * jumpping to ibl.
 */
byte *
emit_native_ret_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code)
{
    instrlist_t ilist;
    opnd_t tgt = opnd_create_reg(REG_XAX);

    ASSERT(DYNAMO_OPTION(native_exec_opt));
    instrlist_init(&ilist);
    insert_shared_get_dcontext(dcontext, &ilist, NULL, true);
    insert_entering_non_native(dcontext, &ilist, NULL, REG_NULL, REG_XAX);
    insert_shared_restore_dcontext_reg(dcontext, &ilist, NULL);
    /* restore xax */
    APP(&ilist, instr_create_restore_from_tls(dcontext, REG_XAX, TLS_XAX_SLOT));
    return emit_special_ibl_xfer(dcontext, pc, code, NATIVE_RET_IBL_IDX,
                                 IBL_RETURN, &ilist, tgt);
}
#endif /* UNIX && X86 */
