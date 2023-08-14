/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
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

/* file "arch.h" -- internal x86-specific definitions
 *
 * References:
 *   "Intel Architecture Software Developer's Manual", 1999.
 */

#ifndef ARCH_H
#define ARCH_H

#include <stddef.h>       /* for offsetof */
#include "instr.h"        /* for reg_id_t */
#include "decode.h"       /* for X64_CACHE_MODE_DC */
#include "arch_exports.h" /* for FRAG_IS_32 and FRAG_IS_X86_TO_X64 */
#include "../fragment.h"  /* IS_IBL_TARGET */
#include "ir_utils.h"

#if defined(X86) && defined(X64)
static inline bool
mixed_mode_enabled(void)
{
    /* XXX i#49: currently only supporting WOW64 and thus only
     * creating x86 versions of gencode for WOW64.  Eventually we'll
     * have to either always create for every x64 process, or lazily
     * create on first appearance of 32-bit code.
     */
#    ifdef WINDOWS
    return is_wow64_process(NT_CURRENT_PROCESS);
#    else
    return false;
#    endif
}
#endif

/* dcontext_t field offsets
 * N.B.: DO NOT USE offsetof(dcontext_t) anywhere else if passing to the
 * dcontext operand construction routines!
 * Otherwise we will have issues w/ the upcontext offset game below
 */

/* offs is not raw offset, but includes upcontext size, so we
 * can tell unprotected from normal!
 * unprotected are raw 0..sizeof(unprotected_context_t)
 * protected are raw + sizeof(unprotected_context_t)
 * (see the instr_shared.c routines for dcontext instr building)
 * FIXME: we could get rid of this hack if unprotected_context_t == priv_mcontext_t
 */
#define PROT_OFFS (sizeof(unprotected_context_t))
#define MC_OFFS (offsetof(unprotected_context_t, mcontext))

#ifdef X86
#    define XAX_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xax)))
#    define REG0_OFFSET XAX_OFFSET
#    define XBX_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xbx)))
#    define REG1_OFFSET XBX_OFFSET
#    define XCX_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xcx)))
#    define XDX_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xdx)))
#    define XSI_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xsi)))
#    define XDI_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xdi)))
#    define XBP_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xbp)))
#    ifdef X64
#        define R8_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r8)))
#        define R9_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r9)))
#        define R10_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r10)))
#        define R11_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r11)))
#        define R12_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r12)))
#        define R13_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r13)))
#        define R14_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r14)))
#        define R15_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r15)))
#    endif /* X64 */
#    define SIMD_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, simd)))
#    define OPMASK_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, opmask)))
#    define SCRATCH_REG0 DR_REG_XAX
#    define SCRATCH_REG1 DR_REG_XBX
#    define SCRATCH_REG2 DR_REG_XCX
#    define SCRATCH_REG3 DR_REG_XDX
#    define SCRATCH_REG4 DR_REG_XSI
#    define SCRATCH_REG5 DR_REG_XDI
#    define SCRATCH_REG0_OFFS XAX_OFFSET
#    define SCRATCH_REG1_OFFS XBX_OFFSET
#    define SCRATCH_REG2_OFFS XCX_OFFSET
#    define SCRATCH_REG3_OFFS XDX_OFFSET
#    define SCRATCH_REG4_OFFS XSI_OFFSET
#    define SCRATCH_REG5_OFFS XDI_OFFSET
#    define CALL_SCRATCH_REG DR_REG_R11
#    define MC_IBL_REG xcx
#    define MC_RETVAL_REG xax
#    define SS_RETVAL_REG xax
#elif defined(AARCHXX)
#    define R0_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r0)))
#    define REG0_OFFSET R0_OFFSET
#    define R1_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r1)))
#    define REG1_OFFSET R1_OFFSET
#    define R2_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r2)))
#    define R3_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r3)))
#    define R4_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r4)))
#    define R5_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r5)))
#    define R6_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r6)))
#    define R7_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r7)))
#    define R8_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r8)))
#    define R9_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r9)))
#    define R10_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r10)))
#    define R11_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r11)))
#    define R12_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r12)))
#    define R13_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r13)))
#    define R14_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, r14)))
#    define PC_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, pc)))
#    define SCRATCH_REG0 DR_REG_R0
#    define SCRATCH_REG1 DR_REG_R1
#    define SCRATCH_REG2 DR_REG_R2
#    define SCRATCH_REG3 DR_REG_R3
#    define SCRATCH_REG4 DR_REG_R4
#    define SCRATCH_REG5 DR_REG_R5
#    define SCRATCH_REG0_OFFS R0_OFFSET
#    define SCRATCH_REG1_OFFS R1_OFFSET
#    define SCRATCH_REG2_OFFS R2_OFFSET
#    define SCRATCH_REG3_OFFS R3_OFFSET
#    define SCRATCH_REG4_OFFS R4_OFFSET
#    define SCRATCH_REG5_OFFS R5_OFFSET
#    define REG_OFFSET(reg) (R0_OFFSET + ((reg)-DR_REG_R0) * sizeof(reg_t))
#    define CALL_SCRATCH_REG DR_REG_R11
#    define MC_IBL_REG r2
#    define MC_RETVAL_REG r0
#    define SS_RETVAL_REG r0
#elif defined(RISCV64)
#    define REG0_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, a0)))
#    define REG1_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, a1)))
#    define REG2_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, a2)))
#    define REG3_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, a3)))
#    define REG4_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, a4)))
#    define REG5_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, a5)))
#    define SCRATCH_REG0 DR_REG_A0
#    define SCRATCH_REG1 DR_REG_A1
#    define SCRATCH_REG2 DR_REG_A2
#    define SCRATCH_REG3 DR_REG_A3
#    define SCRATCH_REG4 DR_REG_A4
#    define SCRATCH_REG5 DR_REG_A5
#    define SCRATCH_REG0_OFFS REG0_OFFSET
#    define SCRATCH_REG1_OFFS REG1_OFFSET
#    define SCRATCH_REG2_OFFS REG2_OFFSET
#    define SCRATCH_REG3_OFFS REG3_OFFSET
#    define SCRATCH_REG4_OFFS REG4_OFFSET
#    define SCRATCH_REG5_OFFS REG5_OFFSET
/* FIXME i#3544: Check is T6 safe to use */
#    define CALL_SCRATCH_REG DR_REG_T6
#    define MC_IBL_REG a2
#    define MC_RETVAL_REG a0
#    define SS_RETVAL_REG a0
#endif /* X86/ARM/RISCV64 */
#define XSP_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xsp)))
#define XFLAGS_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, xflags)))
#define PC_OFFSET ((MC_OFFS) + (offsetof(priv_mcontext_t, pc)))

/* the register holds dcontext on fcache enter/return */
#define REG_DCXT SCRATCH_REG5
#define REG_DCXT_OFFS SCRATCH_REG5_OFFS
#define REG_DCXT_PROT SCRATCH_REG4
#define REG_DCXT_PROT_OFFS SCRATCH_REG4_OFFS

#define ERRNO_OFFSET (offsetof(unprotected_context_t, errno))
#define AT_SYSCALL_OFFSET (offsetof(unprotected_context_t, at_syscall))
#define EXIT_REASON_OFFSET (offsetof(unprotected_context_t, exit_reason))

#define NEXT_TAG_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, next_tag))
#define LAST_EXIT_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, last_exit))
#define LAST_FRAG_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, last_fragment))
#define DSTACK_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, dstack))
#define THREAD_RECORD_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, thread_record))
#define WHEREAMI_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, whereami))

#define FRAGMENT_FIELD_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, fragment_field))
#define PRIVATE_CODE_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, private_code))

#ifdef WINDOWS
#    define APP_ERRNO_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_errno))
#    define APP_FLS_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_fls_data))
#    define PRIV_FLS_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, priv_fls_data))
#    define APP_RPC_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_nt_rpc))
#    define PRIV_RPC_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, priv_nt_rpc))
#    define APP_NLS_CACHE_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_nls_cache))
#    define PRIV_NLS_CACHE_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, priv_nls_cache))
#    define APP_STATIC_TLS_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_static_tls))
#    define PRIV_STATIC_TLS_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, priv_static_tls))
#    define APP_STACK_LIMIT_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_stack_limit))
#    define APP_STACK_BASE_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, app_stack_base))
#    define NONSWAPPED_SCRATCH_OFFSET \
        ((PROT_OFFS) + offsetof(dcontext_t, nonswapped_scratch))
#else
#    define SIGPENDING_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, signals_pending))
#endif

#ifdef TRACE_HEAD_CACHE_INCR
#    define TRACE_HEAD_PC_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, trace_head_pc))
#endif

#ifdef WINDOWS
#    define SYSENTER_STORAGE_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, sysenter_storage))
#    define IGNORE_ENTEREXIT_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, ignore_enterexit))
#endif

#define CLIENT_DATA_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, client_data))

#define COARSE_IB_SRC_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, coarse_exit.src_tag))
#define COARSE_DIR_EXIT_OFFSET ((PROT_OFFS) + offsetof(dcontext_t, coarse_exit.dir_exit))

int
reg_spill_tls_offs(reg_id_t reg);

#define OPSZ_SAVED_XMM (YMM_ENABLED() ? OPSZ_32 : OPSZ_16)
#define OPSZ_SAVED_ZMM OPSZ_64
#define REG_SAVED_XMM0 (YMM_ENABLED() ? REG_YMM0 : REG_XMM0)
#define OPSZ_SAVED_OPMASK (proc_has_feature(FEATURE_AVX512BW) ? OPSZ_8 : OPSZ_2)

#ifdef X86
/* Xref the partially overlapping CONTEXT_PRESERVE_XMM */
/* This routine also determines whether ymm registers should be saved. */
static inline bool
preserve_xmm_caller_saved(void)
{
    /* PR 264138: we must preserve xmm0-5 if on a 64-bit Windows kernel.
     * PR 302107: we must preserve xmm0-15 for 64-bit Linux apps.
     * i#139: we save xmm0-7 in 32-bit Linux and Windows b/c DR and client
     * code on modern compilers ends up using xmm regs w/o any flags to easily
     * disable w/o giving up perf.  (Xref PR 306394 where we originally did
     * not preserve xmm0-7 on a 32-bit kernel b/c DR didn't contain any xmm
     * reg usage).
     */
    return proc_has_feature(FEATURE_SSE) /* do xmm registers exist? */;
}

/* This is used for AVX-512 context switching and indicates whether AVX-512 has been seen
 * during decode. The variable is allocated on reachable heap during initialization.
 */
extern bool *d_r_avx512_code_in_use;

/* This flag indicates a client that had been compiled with AVX-512. In all other than
 * "earliest" inject methods, the initial value of d_r_is_avx512_code_in_use() will be
 * set to true, to prevent a client from clobbering potential application state.
 */
extern bool d_r_client_avx512_code_in_use;

/* This routine determines whether zmm registers should be saved. */
static inline bool
d_r_is_avx512_code_in_use()
{
    return *d_r_avx512_code_in_use;
}

static inline void
d_r_set_avx512_code_in_use(bool in_use, app_pc pc)
{
#    if !defined(UNIX) || !defined(X64)
    /* FIXME i#1312: we warn about unsupported AVX-512 present in the app. */
    DO_ONCE({
        if (pc != NULL) {
            char pc_addr[IF_X64_ELSE(20, 12)];
            snprintf(pc_addr, BUFFER_SIZE_ELEMENTS(pc_addr), PFX, pc);
            NULL_TERMINATE_BUFFER(pc_addr);
            SYSLOG(SYSLOG_ERROR, AVX_512_SUPPORT_INCOMPLETE, 2, get_application_name(),
                   get_application_pid(), pc_addr);
        }
    });
#    endif
#    if !defined(UNIX)
    /* All non-UNIX builds are completely unsupported. 32-bit UNIX builds are
     * partially supported, see comment in proc.c.
     */
    return;
#    endif
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    ATOMIC_1BYTE_WRITE(d_r_avx512_code_in_use, in_use, false);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

static inline bool
d_r_is_client_avx512_code_in_use()
{
    return d_r_client_avx512_code_in_use;
}

static inline void
d_r_set_client_avx512_code_in_use()
{
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    ATOMIC_1BYTE_WRITE(&d_r_client_avx512_code_in_use, (bool)true, false);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}
#endif

typedef enum {
    IBL_UNLINKED,
    IBL_DELETE,
    /* Pre-ibl routines for far ctis */
    IBL_FAR,
    IBL_FAR_UNLINKED,
#if defined(X86) && defined(X64)
    /* PR 257963: trace inline cmp has separate entries b/c it saves flags */
    IBL_TRACE_CMP,
    IBL_TRACE_CMP_UNLINKED,
#endif
    IBL_LINKED,

    IBL_TEMPLATE, /* a template is presumed to be always linked */
    IBL_LINK_STATE_END
} ibl_entry_point_type_t;

/* we should allow for all {{bb,trace} x {ret,ind call, ind jmp} x {shared, private}} */
/* combinations of routines which are in turn  x {unlinked, linked} */
typedef enum {
    /* FIXME: have a separate flag for private vs shared */
    IBL_BB_SHARED,
    IBL_SOURCE_TYPE_START = IBL_BB_SHARED,
    IBL_TRACE_SHARED,
    IBL_BB_PRIVATE,
    IBL_TRACE_PRIVATE,
    IBL_COARSE_SHARED, /* no coarse private, for now */
    IBL_SOURCE_TYPE_END
} ibl_source_fragment_type_t;

#define DEFAULT_IBL_BB() (DYNAMO_OPTION(shared_bbs) ? IBL_BB_SHARED : IBL_BB_PRIVATE)
#define DEFAULT_IBL_TRACE() \
    (DYNAMO_OPTION(shared_traces) ? IBL_TRACE_SHARED : IBL_TRACE_PRIVATE)
#define IS_IBL_BB(ibltype) ((ibltype) == IBL_BB_PRIVATE || (ibltype) == IBL_BB_SHARED)
#define IS_IBL_TRACE(ibltype) \
    ((ibltype) == IBL_TRACE_PRIVATE || (ibltype) == IBL_TRACE_SHARED)
#define IS_IBL_LINKED(ibltype)  \
    ((ibltype) == IBL_LINKED || \
     (ibltype) == IBL_FAR IF_X86_64(|| (ibltype) == IBL_TRACE_CMP))
#define IS_IBL_UNLINKED(ibltype)  \
    ((ibltype) == IBL_UNLINKED || \
     (ibltype) == IBL_FAR_UNLINKED IF_X86_64(|| (ibltype) == IBL_TRACE_CMP_UNLINKED))

#define IBL_FRAG_FLAGS(ibl_code) \
    (IS_IBL_TRACE((ibl_code)->source_fragment_type) ? FRAG_IS_TRACE : 0)

static inline ibl_entry_point_type_t
get_ibl_entry_type(uint link_or_instr_flags)
{
#if defined(X86) && defined(X64)
    if (TEST(LINK_TRACE_CMP, link_or_instr_flags))
        return IBL_TRACE_CMP;
#endif
    if (TEST(LINK_FAR, link_or_instr_flags))
        return IBL_FAR;
    else
        return IBL_LINKED;
}

typedef struct {
    /* these could be bit fields, if needed */
    ibl_entry_point_type_t link_state;
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;
} ibl_type_t;

#if defined(X86) && defined(X64)
/* PR 282576: With shared_code_x86, GLOBAL_DCONTEXT no longer specifies
 * a unique generated_code_t.  Rather than add GLOBAL_DCONTEXT_X86 everywhere,
 * we add mode parameters to a handful of routines that take in GLOBAL_DCONTEXT.
 */
/* FIXME i#1551: do we want separate Thumb vs ARM gencode, or we'll always
 * transition?  For fcache exit that's reasonable, but for ibl it would
 * require two mode transitions.
 */
typedef enum {
    GENCODE_X64 = 0,
    GENCODE_X86,
    GENCODE_X86_TO_X64,
    GENCODE_FROM_DCONTEXT,
} gencode_mode_t;
#    define FRAGMENT_GENCODE_MODE(fragment_flags) \
        (FRAG_IS_32(fragment_flags)               \
             ? GENCODE_X86                        \
             : (FRAG_IS_X86_TO_X64(fragment_flags) ? GENCODE_X86_TO_X64 : GENCODE_X64))
#    define SHARED_GENCODE(gencode_mode) get_shared_gencode(GLOBAL_DCONTEXT, gencode_mode)
#    define SHARED_GENCODE_MATCH_THREAD(dc) get_shared_gencode(dc, GENCODE_FROM_DCONTEXT)
#    define THREAD_GENCODE(dc) get_emitted_routines_code(dc, GENCODE_FROM_DCONTEXT)
#    define GENCODE_IS_X64(gencode_mode) ((gencode_mode) == GENCODE_X64)
#    define GENCODE_IS_X86(gencode_mode) ((gencode_mode) == GENCODE_X86)
#    define GENCODE_IS_X86_TO_X64(gencode_mode) ((gencode_mode) == GENCODE_X86_TO_X64)
#else
#    define SHARED_GENCODE(b) get_shared_gencode(GLOBAL_DCONTEXT)
#    define THREAD_GENCODE(dc) get_emitted_routines_code(dc)
#    define SHARED_GENCODE_MATCH_THREAD(dc) get_shared_gencode(dc)
#endif

/* Information about each individual clean call invocation site.
 * The whole struct is set to 0 at init time.
 */
typedef struct _clean_call_info_t {
    void *callee;
    uint num_args;
    bool save_fpstate;
    bool opt_inline;
    bool should_align;
    bool save_all_regs;
    bool skip_save_flags;
    bool skip_clear_flags;
    int num_simd_skip;
    bool simd_skip[MCXT_NUM_SIMD_SLOTS];
#ifdef X86
    int num_opmask_skip;
    bool opmask_skip[MCXT_NUM_OPMASK_SLOTS];
#endif
    uint num_regs_skip;
    bool reg_skip[DR_NUM_GPR_REGS];
    bool preserve_mcontext; /* even if skip reg save, preserve mcontext shape */
    bool out_of_line_swap;  /* whether we use clean_call_{save,restore} gencode */
    void *callee_info;      /* callee information */
    instrlist_t *ilist;     /* instruction list for inline optimization */
} clean_call_info_t;

/* flags for insert_meta_call_vargs, to indicate various properties about the call */
typedef enum {
    META_CALL_CLEAN = 0x0001,
    META_CALL_RETURNS = 0x0002,
    /* alias of DR_CLEANCALL_RETURNS_TO_NATIVE */
    META_CALL_RETURNS_TO_NATIVE = 0x0004,
} meta_call_flags_t;

cache_pc
get_ibl_routine_ex(dcontext_t *dcontext, ibl_entry_point_type_t entry_type,
                   ibl_source_fragment_type_t source_fragment_type,
                   ibl_branch_type_t branch_type _IF_X86_64(gencode_mode_t mode));
cache_pc
get_ibl_routine(dcontext_t *dcontext, ibl_entry_point_type_t entry_type,
                ibl_source_fragment_type_t source_fragment_type,
                ibl_branch_type_t branch_type);
cache_pc
get_ibl_routine_template(dcontext_t *dcontext,
                         ibl_source_fragment_type_t source_fragment_type,
                         ibl_branch_type_t branch_type _IF_X86_64(gencode_mode_t mode));
bool
get_ibl_routine_type(dcontext_t *dcontext, cache_pc target, ibl_type_t *type);
bool
get_ibl_routine_type_ex(dcontext_t *dcontext, cache_pc target,
                        ibl_type_t *type _IF_X86_64(gencode_mode_t *mode_out));
const char *
get_ibl_routine_name(dcontext_t *dcontext, cache_pc target, const char **ibl_brtype_name);
cache_pc
get_trace_ibl_routine(dcontext_t *dcontext, cache_pc current_entry);
cache_pc
get_private_ibl_routine(dcontext_t *dcontext, cache_pc current_entry);
cache_pc
get_shared_ibl_routine(dcontext_t *dcontext, cache_pc current_entry);
cache_pc
get_alternate_ibl_routine(dcontext_t *dcontext, cache_pc current_entry, uint flags);

ibl_source_fragment_type_t
get_source_fragment_type(dcontext_t *dcontext, uint fragment_flags);

const char *
get_target_delete_entry_name(dcontext_t *dcontext, cache_pc target,
                             const char **ibl_brtype_name);

#define GET_IBL_TARGET_TABLE(branch_type, target_trace_table)                \
    ((target_trace_table) ? offsetof(per_thread_t, trace_ibt[(branch_type)]) \
                          : offsetof(per_thread_t, bb_ibt[(branch_type)]))

#ifdef WINDOWS
/* PR 282576: These separate routines are ugly, but less ugly than adding param to
 * after_shared_syscall_code(), which is called in many places and usually passed a
 * non-global dcontext; also less ugly than adding GLOBAL_DCONTEXT_X86.
 */
cache_pc
shared_syscall_routine_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));
cache_pc
unlinked_shared_syscall_routine_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));
cache_pc
shared_syscall_routine(dcontext_t *dcontext);
cache_pc
unlinked_shared_syscall_routine(dcontext_t *dcontext);
#endif
#ifdef TRACE_HEAD_CACHE_INCR
cache_pc
trace_head_incr_routine(dcontext_t *dcontext);
cache_pc trace_head_incr_shared_routine(IF_X86_64(gencode_mode_t mode));
#endif

/* in mangle_shared.c */
/* What prepare_for_clean_call() adds to xsp beyond sizeof(priv_mcontext_t) */
static inline int
clean_call_beyond_mcontext(void)
{
    return 0; /* no longer adding anything */
}
void
clean_call_info_init(clean_call_info_t *cci, void *callee, bool save_fpstate,
                     uint num_args);
void
d_r_mangle(dcontext_t *dcontext, instrlist_t *ilist, uint *flags INOUT, bool mangle_calls,
           bool record_translation);
bool
parameters_stack_padded(void);
/* Inserts a complete call to callee with the passed-in arguments */
bool
insert_meta_call_vargs(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       meta_call_flags_t flags, byte *encode_pc, void *callee,
                       uint num_args, opnd_t *args);
void
mangle_init(void);
void
mangle_exit(void);
void
patch_mov_immed_ptrsz(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                      instr_t *last);
/* in mangle.c arch-specific implementation */
#ifdef ARM
int
reinstate_it_blocks(dcontext_t *dcontext, instrlist_t *ilist, instr_t *start,
                    instr_t *end);
#endif

void
mangle_arch_init(void);

reg_id_t
shrink_reg_for_param(reg_id_t regular, opnd_t arg);
uint
insert_parameter_preparation(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool clean_call, uint num_args, opnd_t *args);
void
patch_mov_immed_arch(dcontext_t *dcontext, ptr_int_t val, byte *pc, instr_t *first,
                     instr_t *last);
instr_t *
convert_to_near_rel_arch(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);
void
mangle_syscall_arch(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr,
                    instr_t *next_instr);
void
mangle_interrupt(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr);
#ifdef X86
void
mangle_possible_single_step(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);
void
mangle_single_step(dcontext_t *dcontext, instrlist_t *ilist, uint flags, instr_t *instr);

#endif
instr_t *
mangle_direct_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                   instr_t *next_instr, bool mangle_calls, uint flags);
instr_t *
mangle_indirect_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, bool mangle_calls, uint flags);
void
mangle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
              instr_t *next_instr, uint flags);
instr_t *
mangle_indirect_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                     instr_t *next_instr, uint flags);
#if defined(X64) || defined(ARM)
instr_t *
mangle_rel_addr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr);
#endif
#ifdef AARCHXX
/* mangle instructions that use pc or dr_reg_stolen */
instr_t *
mangle_special_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                         instr_t *next_instr);
instr_t *
mangle_exclusive_monitor_op(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                            instr_t *next_instr);
#endif
void
mangle_insert_clone_code(dcontext_t *dcontext, instrlist_t *ilist,
                         instr_t *instr _IF_X86_64(gencode_mode_t mode));

#ifdef X86
#    if defined(X64) || defined(UNIX)
/* See i#847, i#3966 for discussion of stack alignment on 32-bit Linux. */
#        define ABI_STACK_ALIGNMENT 16
#    else
/* We follow the Windows (MSVC-based) 32-bit ABI which requires only 4-byte
 * stack alignment.
 * XXX i#4267: Gcc/clang through MinGW/Cygwin use 16-byte by default, but
 * for interoperating with Windows system libraries (callbacks, e.g.) they
 * have to hande 4-byte and we expect them to use -mstackrealign or something.
 * Thus for now we stick with just 4-byte even for them.
 */
#        define ABI_STACK_ALIGNMENT 4
#    endif
#elif defined(AARCH64)
#    define ABI_STACK_ALIGNMENT 16
#elif defined(ARM)
#    define ABI_STACK_ALIGNMENT 8
#elif defined(RISCV64)
#    define ABI_STACK_ALIGNMENT 8
#endif

/* Returns the number of bytes the stack pointer has to be aligned to. */
static inline uint
get_ABI_stack_alignment()
{
    return ABI_STACK_ALIGNMENT;
}

/* the stack size of a full context switch for clean call */
int
get_clean_call_switch_stack_size(void);
/* extra temporarily-used stack usage beyond
 * get_clean_call_switch_stack_size()
 */
int
get_clean_call_temp_stack_size(void);
void
insert_clear_eflags(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                    instr_t *instr);
uint
insert_push_all_registers(dcontext_t *dcontext, clean_call_info_t *cci,
                          instrlist_t *ilist, instr_t *instr, uint alignment,
                          opnd_t push_pc,
                          reg_id_t scratch /*optional*/
                              _IF_AARCH64(bool out_of_line));
void
insert_pop_all_registers(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, uint alignment _IF_AARCH64(bool out_of_line));
bool
insert_reachable_cti(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                     byte *encode_pc, byte *target, bool jmp, bool returns, bool precise,
                     reg_id_t scratch, instr_t **inlined_tgt_instr);
void
insert_get_mcontext_base(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg);
uint
prepare_for_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                       instr_t *instr, byte *encode_pc);
void
cleanup_after_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *instr, byte *encode_pc);
void
convert_to_near_rel(dcontext_t *dcontext, instr_t *instr);
instr_t *
convert_to_near_rel_meta(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

#ifdef AARCH64
typedef enum { GPR_REG_TYPE, SIMD_REG_TYPE, SVE_ZREG_TYPE, SVE_PREG_TYPE } reg_type_t;

void
insert_save_inline_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             bool *reg_skip, reg_id_t first_reg, reg_type_t rtype,
                             void *ci);

void
insert_restore_inline_registers(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                                bool *reg_skip, reg_id_t first_reg, reg_type_t rtype,
                                void *ci);

#endif

#ifdef WINDOWS
bool
instr_is_call_sysenter_pattern(instr_t *call, instr_t *mov, instr_t *sysenter);
#endif
int
find_syscall_num(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

/* in mangle.c  but not exported to non-arch files */
int
insert_out_of_line_context_switch(dcontext_t *dcontext, instrlist_t *ilist,
                                  instr_t *instr, bool save, byte *encode_pc);
#ifdef X86
#    ifdef UNIX
/* Mangle reference of fs/gs semgents, reg must not be used in the oldop. */
opnd_t
mangle_seg_ref_opnd(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                    opnd_t oldop, reg_id_t reg);
#    endif
/* mangle the instruction that reference memory via segment register */
void
mangle_seg_ref(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr);
#    ifdef ANNOTATIONS
void
mangle_annotation_helper(dcontext_t *dcontext, instr_t *label, instrlist_t *ilist);
#    endif
/* mangle the instruction OP_mov_seg, i.e. the instruction that
 * read/update the segment register.
 */
void
mangle_mov_seg(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
               instr_t *next_instr);
void
mangle_float_pc(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                instr_t *next_instr, uint *flags INOUT);
void
mangle_exit_cti_prefixes(dcontext_t *dcontext, instr_t *instr);
void
mangle_far_direct_jump(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                       instr_t *next_instr, uint flags);
void
set_selfmod_sandbox_offsets(dcontext_t *dcontext);
bool
insert_selfmod_sandbox(dcontext_t *dcontext, instrlist_t *ilist, uint flags,
                       app_pc start_pc, app_pc end_pc, /* end is open */
                       bool record_translation, bool for_cache);
#endif /* X86 */

#ifdef ARM
/* mangle the instruction that reads thread register */
instr_t *
mangle_reads_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             instr_t *next_instr);
#endif /* ARM */

#ifdef AARCH64
instr_t *
mangle_icache_op(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                 instr_t *next_instr, app_pc pc);
instr_t *
mangle_reads_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                             instr_t *next_instr);
instr_t *
mangle_writes_thread_register(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                              instr_t *next_instr);
#endif /* AARCH64 */

/* offsets within local_state_t used for specific scratch purposes */
enum {
    /* ok for this guy to overlap w/ others since he is pre-cache.
     * Also, note that we cannot use either TLS_REG0_SLOT or
     * TLS_REG1_SLOT for this because those are used in fragment prefix.
     */
    FCACHE_ENTER_TARGET_SLOT = TLS_REG2_SLOT,
    /* FIXME: put register name in each enum name to avoid conflicts
     * when mixed with raw slot names?
     */
    /* ok for the next_tag and direct_stub to overlap as next_tag is
     * used for sysenter shared syscall mangling, which uses an
     * indirect stub.
     */
    MANGLE_NEXT_TAG_SLOT = TLS_REG0_SLOT,
    DIRECT_STUB_SPILL_SLOT = TLS_REG0_SLOT,
    MANGLE_RIPREL_SPILL_SLOT = TLS_REG0_SLOT,
    /* ok for far cti mangling/far ibl and stub/ibl xbx slot usage to overlap */
    DIRECT_STUB_SPILL_SLOT2 = TLS_REG1_SLOT, /* used on AArch64 */
    INDIRECT_STUB_SPILL_SLOT = TLS_REG1_SLOT,
    MANGLE_FAR_SPILL_SLOT = TLS_REG1_SLOT,
    /* i#698: float_pc handling stores the mem addr of the float state here.  We
     * assume this slot is not touched on the fcache_return path.
     */
    FLOAT_PC_STATE_SLOT = TLS_REG1_SLOT,
    MANGLE_XCX_SPILL_SLOT = TLS_REG2_SLOT,
/* FIXME: edi is used as the base, yet I labeled this slot for edx
 * since it's next in the progression -- change one or the other?
 * (this is case 5239)
 */
#ifdef AARCH64
    DCONTEXT_BASE_SPILL_SLOT = TLS_REG5_SLOT,
#else
    DCONTEXT_BASE_SPILL_SLOT = TLS_REG3_SLOT,
#endif
    PREFIX_XAX_SPILL_SLOT = TLS_REG0_SLOT,
#ifdef HASHTABLE_STATISTICS
    HTABLE_STATS_SPILL_SLOT = TLS_HTABLE_STATS_SLOT,
#endif
};

/* A simple linker to give us indirection for patching after relocating structures */
typedef struct patch_entry_t {
    union {
        instr_t *instr; /* used before instructions are encoded */
        size_t offset;  /* offset in instruction stream */
    } where;
    ptr_uint_t value_location_offset; /* location containing value to be updated */
    /* offset from dcontext->fragment_field (usually pt->trace.field),
     * or an absolute address */
    ushort patch_flags; /* whether to use the address of location or its value */
    short instr_offset; /* desired offset within instruction,
                           negative offsets are from end of instruction */
} patch_entry_t;

enum {
    MAX_PATCH_ENTRIES =
#ifdef HASHTABLE_STATISTICS
        6 + /* will need more only for statistics */
#endif
        7, /* we use 5 normally, 7 w/ -atomic_inlined_linking and inlining */
    /* Patch entry flags */
    /* Patch offset entries for dynamic updates from input variables */
    /* use computed address if set, value at address otherwise */
    PATCH_TAKE_ADDRESS = 0x01,
    /* address is relative to the per_thread_t thread local field */
    PATCH_PER_THREAD = 0x02,
    /* address is (unprot_ht_statistics_t offs << 16) | (stats offs) */
    PATCH_UNPROT_STAT = 0x04,
    /* Patch offset markers update an output variable in encode_with_patch_list */
    PATCH_MARKER = 0x08,            /* if set use only as a static marker */
    PATCH_ASSEMBLE_ABSOLUTE = 0x10, /* if set retrieve an absolute pc into given target
                                     * address, otherwise relative to start pc */
    PATCH_OFFSET_VALID = 0x20,      /* if set use patch_entry_t.where.offset;
                                     * else patch_entry_t.where.instr */
    PATCH_UINT_SIZED = 0x40,        /* if set value is uint-sized; else pointer-sized */
};

typedef enum {
    PATCH_TYPE_ABSOLUTE = 0x0,     /* link with absolute address, updated dynamically */
    PATCH_TYPE_INDIRECT_XDI = 0x1, /* linked with indirection through EDI, no updates */
    PATCH_TYPE_INDIRECT_FS = 0x2,  /* linked with indirection through FS, no updates */
    PATCH_TYPE_INDIRECT_TLS = 0x3, /* multi-step TLS indirection (ARM), no updates */
} patch_list_type_t;

typedef struct patch_list_t {
    ushort num_relocations;
    ushort /* patch_list_type_t */ type;
    patch_entry_t entry[MAX_PATCH_ENTRIES];
} patch_list_t;

void
init_patch_list(patch_list_t *patch, patch_list_type_t type);

void
add_patch_marker(patch_list_t *patch, instr_t *instr, ushort patch_flags,
                 short instr_offset, ptr_uint_t *target_offset /* OUT */);

int
encode_with_patch_list(dcontext_t *dcontext, patch_list_t *patch, instrlist_t *ilist,
                       cache_pc start_pc);

#if defined(X86) && defined(X64)
/* Shouldn't need to mark as packed.  We order for 6-byte little-endian selector:pc. */
typedef struct _far_ref_t {
    /* We target WOW64 and cross-plaform so no 8-byte Intel-only pc */
    uint pc;
    ushort selector;
} far_ref_t;
#endif

/* Defines book-keeping structures needed for an indirect branch lookup routine */
typedef struct ibl_code_t {
    bool initialized : 1; /* currently only used for ibl routines */
    bool thread_shared_routine : 1;
    bool ibl_head_is_inlined : 1;
    byte *indirect_branch_lookup_routine;
    /* for far ctis (i#823) */
    byte *far_ibl;
    byte *far_ibl_unlinked;
#if defined(X86) && defined(X64)
    /* PR 257963: trace inline cmp has already saved eflags */
    byte *trace_cmp_entry;
    byte *trace_cmp_unlinked;
    bool x86_mode;        /* Is this code for 32-bit (x86 mode)? */
    bool x86_to_x64_mode; /* Does this code use r8-r10 as scratch (for x86_to_x64)? */
    /* for far ctis (i#823) in mixed-mode (i#49) and x86_to_x64 mode (i#751) */
    far_ref_t far_jmp_opnd;
    far_ref_t far_jmp_unlinked_opnd;
#endif
    byte *unlinked_ibl_entry;
    byte *target_delete_entry;
    uint ibl_routine_length;
    /* offsets into ibl routine */
    patch_list_t ibl_patch;
    ibl_branch_type_t branch_type;
    ibl_source_fragment_type_t source_fragment_type;

    /* bookkeeping for the inlined ibl stub template, if inlining */
    byte *inline_ibl_stub_template;
    patch_list_t ibl_stub_patch;
    uint inline_stub_length;
    /* for atomic_inlined_linking we store the linkstub twice so need to update
     * two offsets */
    uint inline_linkstub_first_offs;
    uint inline_linkstub_second_offs;
    uint inline_unlink_offs;
    uint inline_linkedjmp_offs;
    uint inline_unlinkedjmp_offs;

#ifdef HASHTABLE_STATISTICS
    /* need two offsets to get to stats, since in unprotected memory */
    uint unprot_stats_offset;
    uint hashtable_stats_offset;
    /* e.g. offsetof(per_thread_t, trace) + offsetof(ibl_table_t, bb_ibl_stats) */
    /* Note hashtable statistics are associated with the hashtable for easier use
     * when sharing IBL routines
     */
    uint entry_stats_to_lookup_table_offset; /* offset to (entry_stats - lookup_table)  */
#endif
} ibl_code_t;

/* special ibls */
#define NUM_SPECIAL_IBL_XFERS 3 /* client_ibl and native_plt/ret__ibl */
#define CLIENT_IBL_IDX 0
#define NATIVE_PLT_IBL_IDX 1
#define NATIVE_RET_IBL_IDX 2

/* Each thread needs its own copy of these routines, but not all
 * routines here are created in a thread-private: we could save space
 * by splitting into two separate structs.
 *
 * On x64, we only have thread-shared generated routines,
 * including do_syscall and shared_syscall and detach's post-syscall
 * continuation (PR 244737).
 */
typedef struct _generated_code_t {
    byte *fcache_enter;
    byte *fcache_return;
    byte *fcache_return_end;
#ifdef WINDOWS_PC_SAMPLE
    byte *fcache_enter_return_end;
#endif

    ibl_code_t trace_ibl[IBL_BRANCH_TYPE_END];
    ibl_code_t bb_ibl[IBL_BRANCH_TYPE_END];
    ibl_code_t coarse_ibl[IBL_BRANCH_TYPE_END];
#ifdef WINDOWS_PC_SAMPLE
    byte *ibl_routines_end;
#endif

#ifdef WINDOWS
    /* for the shared_syscalls option */
    ibl_code_t shared_syscall_code;
    byte *shared_syscall;
    byte *unlinked_shared_syscall;
    byte *end_shared_syscall; /* just marks end */
    /* N.B.: these offsets are from the start of unlinked_shared_syscall,
     * not from shared_syscall (which is later)!!!
     */
    /* offsets into shared_syscall routine */
    uint sys_syscall_offs;
    /* where to patch to unlink end of syscall thread-wide */
    uint sys_unlink_offs;
#endif
    byte *do_syscall;
    uint do_syscall_offs; /* offs of pc after actual syscall instr */
#ifdef AARCHXX
    byte *fcache_enter_gonative;
#endif
#ifdef WINDOWS
    byte *fcache_enter_indirect;
    byte *do_callback_return;
#else
    /* PR 286922: we both need an int and a sys{call,enter} do-syscall for
     * 32-bit apps on 64-bit kernels.  do_syscall is whatever is in
     * vsyscall, while do_int_syscall is hardcoded to use OP_int.
     */
    byte *do_int_syscall;
    uint do_int_syscall_offs; /* offs of pc after actual syscall instr */
    /* These are for Mac but we avoid ifdefs for simplicity */
    byte *do_int81_syscall;
    uint do_int81_syscall_offs; /* offs of pc after actual syscall instr */
    byte *do_int82_syscall;
    uint do_int82_syscall_offs; /* offs of pc after actual syscall instr */
    byte *do_clone_syscall;
    uint do_clone_syscall_offs; /* offs of pc after actual syscall instr */
#    ifdef VMX86_SERVER
    byte *do_vmkuw_syscall;
    uint do_vmkuw_syscall_offs; /* offs of pc after actual syscall instr */
#    endif
#endif
#ifdef UNIX
    /* PR 212290: can't be static code in x86.asm since it can't be PIC */
    byte *new_thread_dynamo_start;
#endif
#ifdef TRACE_HEAD_CACHE_INCR
    byte *trace_head_incr;
#endif
#ifdef CHECK_RETURNS_SSE2
    byte *pextrw;
    byte *pinsrw;
#endif
#ifdef WINDOWS_PC_SAMPLE
    profile_t *profile;
#endif
    /* For control redirection from a syscall.
     * We could make this shared-only and save some space, if we
     * generated a shared fcache_return in all-private-fragment configs.
     */
    byte *reset_exit_stub;

    /* Coarse-grain fragments don't have linkstubs and need custom routines.
     * Direct exits use entrance stubs that record the target app pc,
     * while coarse indirect stubs record the source cache cti.
     */
    /* FIXME: these two return routines are only needed in the global struct */
    byte *fcache_return_coarse;
    byte *fcache_return_coarse_end;
    byte *trace_head_return_coarse;
    /* special ibl xfer */
    byte *special_ibl_xfer[NUM_SPECIAL_IBL_XFERS];
    uint special_ibl_unlink_offs[NUM_SPECIAL_IBL_XFERS];
    /* i#171: out-of-line clean call context switch */
    byte *clean_call_save;
    byte *clean_call_restore;
    byte *clean_call_restore_end;

    bool thread_shared;
    bool writable;
#if defined(X86) && defined(X64)
    gencode_mode_t gencode_mode; /* mode of this code (x64, x86, x86_to_x64) */
#endif

    /* We store the start of the generated code for simplicity even
     * though it is always right after this struct; if we really need
     * to shrink 4 bytes we can remove this field and replace w/
     * ((char *)TPC_ptr) + sizeof(generated_code_t)
     */
    byte *gen_start_pc;  /* start of generated code */
    byte *gen_end_pc;    /* end of generated code */
    byte *commit_end_pc; /* end of committed region */
    /* generated code follows, ends at gen_end_pc < commit_end_pc */
} generated_code_t;

/* thread-private generated code */
fcache_enter_func_t
fcache_enter_routine(dcontext_t *dcontext);
cache_pc
fcache_return_routine(dcontext_t *dcontext);
cache_pc
fcache_return_routine_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));

/* thread-shared generated code */
byte *
emit_fcache_enter_shared(dcontext_t *dcontext, generated_code_t *code, byte *pc);
byte *
emit_fcache_return_shared(dcontext_t *dcontext, generated_code_t *code, byte *pc);
fcache_enter_func_t
fcache_enter_shared_routine(dcontext_t *dcontext);
/* the fcache_return routines are queried by get_direct_exit_target and need more
 * direct control than the dcontext
 */
cache_pc fcache_return_shared_routine(IF_X86_64(gencode_mode_t mode));

/* coarse-grain generated code */
byte *
emit_fcache_return_coarse(dcontext_t *dcontext, generated_code_t *code, byte *pc);
byte *
emit_trace_head_return_coarse(dcontext_t *dcontext, generated_code_t *code, byte *pc);
cache_pc fcache_return_coarse_routine(IF_X86_64(gencode_mode_t mode));
cache_pc trace_head_return_coarse_routine(IF_X86_64(gencode_mode_t mode));

/* shared clean call context switch */
bool
client_clean_call_is_thread_private();
cache_pc
get_clean_call_save(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));
cache_pc
get_clean_call_restore(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));

void
protect_generated_code(generated_code_t *code, bool writable);

extern generated_code_t *shared_code;
#if defined(X86) && defined(X64)
extern generated_code_t *shared_code_x86;
extern generated_code_t *shared_code_x86_to_x64;
#endif

static inline bool
is_shared_gencode(generated_code_t *code)
{
    if (code == NULL) /* since shared_code_x86 in particular can be NULL */
        return false;
#if defined(X86) && defined(X64)
    return code == shared_code_x86 || code == shared_code ||
        code == shared_code_x86_to_x64;
#else
    return code == shared_code;
#endif
}

static inline generated_code_t *
get_shared_gencode(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
#if defined(X86) && defined(X64)
    ASSERT(mode != GENCODE_FROM_DCONTEXT ||
           dcontext != GLOBAL_DCONTEXT IF_INTERNAL(|| dynamo_exited));
    /* PR 302344: this is here only for tracedump_origins */
    if (dynamo_exited && mode == GENCODE_FROM_DCONTEXT && dcontext == GLOBAL_DCONTEXT) {
        if (get_x86_mode(dcontext))
            return X64_CACHE_MODE_DC(dcontext) ? shared_code_x86_to_x64 : shared_code_x86;
        else
            return shared_code;
    }
    if (mode == GENCODE_X86)
        return shared_code_x86;
    else if (mode == GENCODE_X86_TO_X64)
        return shared_code_x86_to_x64;
    else if (mode == GENCODE_FROM_DCONTEXT && !X64_MODE_DC(dcontext))
        return X64_CACHE_MODE_DC(dcontext) ? shared_code_x86_to_x64 : shared_code_x86;
    else
        return shared_code;
#else
    return shared_code;
#endif
}

/* PR 244737: thread-private uses shared gencode on x64, b/c absolute addresses
 * are impractical.  The same goes for ARM.
 */
#define USE_SHARED_GENCODE_ALWAYS() IF_ARM_ELSE(true, IF_X64_ELSE(true, false))
/* PR 212570: on linux we need a thread-shared do_syscall for our vsyscall hook,
 * if we have TLS and support sysenter (PR 361894)
 */
#define USE_SHARED_GENCODE()                                                 \
    (USE_SHARED_GENCODE_ALWAYS() ||                                          \
     IF_UNIX(IF_HAVE_TLS_ELSE(true, false) ||) SHARED_FRAGMENTS_ENABLED() || \
     DYNAMO_OPTION(shared_trace_ibl_routine))

#define USE_SHARED_BB_IBL() (USE_SHARED_GENCODE_ALWAYS() || DYNAMO_OPTION(shared_bbs))

#define USE_SHARED_TRACE_IBL()                                      \
    (USE_SHARED_GENCODE_ALWAYS() || DYNAMO_OPTION(shared_traces) || \
     DYNAMO_OPTION(shared_trace_ibl_routine))

/* returns the thread private code or GLOBAL thread shared code */
static inline generated_code_t *
get_emitted_routines_code(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code;
    /* This routine exists only because GLOBAL_DCONTEXT is not a real dcontext
     * structure. Still, useful to wrap all references to private_code. */
    /* PR 244737: thread-private uses only shared gencode on x64 */
    /* PR 253431: to distinguish shared x86 gencode from x64 gencode, a dcontext
     * must be passed in; use get_shared_gencode() for x64 builds */
    IF_X86_64(ASSERT(mode != GENCODE_FROM_DCONTEXT || dcontext != GLOBAL_DCONTEXT));
    if (USE_SHARED_GENCODE_ALWAYS() ||
        (USE_SHARED_GENCODE() && dcontext == GLOBAL_DCONTEXT)) {
        code = get_shared_gencode(dcontext _IF_X86_64(mode));
    } else {
        ASSERT(dcontext != GLOBAL_DCONTEXT);
        /* NOTE thread private code entry points may also refer to shared
         * routines */
        code = (generated_code_t *)dcontext->private_code;
    }
    return code;
}

ibl_code_t *
get_ibl_routine_code(dcontext_t *dcontext, ibl_branch_type_t branch_type,
                     uint fragment_flags);
ibl_code_t *
get_ibl_routine_code_ex(dcontext_t *dcontext, ibl_branch_type_t branch_type,
                        uint fragment_flags _IF_X86_64(gencode_mode_t mode));

/* in emit_utils.c but not exported to non-x86 files */
int
insert_exit_stub_other_flags(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                             cache_pc stub_pc, ushort l_flags);
bool
exit_cti_reaches_target(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        cache_pc target_pc);
void
patch_stub(fragment_t *f, cache_pc stub_pc, cache_pc target_pc, cache_pc target_prefix_pc,
           bool hot_patch);
bool
stub_is_patched(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc);
void
unpatch_stub(dcontext_t *dcontext, fragment_t *f, cache_pc stub_pc, bool hot_patch);

byte *
emit_inline_ibl_stub(dcontext_t *dcontext, byte *pc, ibl_code_t *ibl_code,
                     bool target_trace_table);
byte *
emit_fcache_enter(dcontext_t *dcontext, generated_code_t *code, byte *pc);
byte *
emit_fcache_return(dcontext_t *dcontext, generated_code_t *code, byte *pc);
byte *
emit_indirect_branch_lookup(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                            byte *fcache_return_pc, bool target_trace_table,
                            bool inline_ibl_head, ibl_code_t *ibl_code);
void
update_indirect_branch_lookup(dcontext_t *dcontext);
bool
instr_is_ibl_hit_jump(instr_t *instr);

byte *
emit_far_ibl(dcontext_t *dcontext, byte *pc, ibl_code_t *ibl_code,
             cache_pc ibl_tgt _IF_X86_64(far_ref_t *far_jmp_opnd));

#ifndef WINDOWS
void
update_syscalls(dcontext_t *dcontext);
#endif

#ifdef WINDOWS
/* FIXME If we widen the interface any further, do we want to use an options
 * struct or OR-ed flags to replace the bool args? */
byte *
emit_shared_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                    ibl_code_t *ibl_code, patch_list_t *patch, byte *ind_br_lookup_pc,
                    byte *unlinked_ib_lookup_pc, bool target_trace_table,
                    bool inline_ibl_head, bool thread_shared, byte **shared_syscall_pc);

byte *
emit_shared_syscall_dispatch(dcontext_t *dcontext, byte *pc);

byte *
emit_unlinked_shared_syscall_dispatch(dcontext_t *dcontext, byte *pc);

/* i#249: isolate app's PEB by keeping our own copy and swapping on cxt switch */
void
preinsert_swap_peb(dcontext_t *dcontext, instrlist_t *ilist, instr_t *next, bool absolute,
                   reg_id_t reg_dr, reg_id_t reg_scratch, bool to_priv);

void
emit_patch_syscall(dcontext_t *dcontext, byte *target _IF_X86_64(gencode_mode_t mode));
#endif /* WINDOWS */

byte *
emit_do_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                byte *fcache_return_pc, bool thread_shared, int interrupt,
                uint *syscall_offs /*OUT*/);

#ifdef AARCH64
/* Generate move (immediate) of a 64-bit value using at most 4 instructions.
 * pc must be a writable (vmcode) pc.
 */
uint *
insert_mov_imm(uint *pc, reg_id_t dst, ptr_int_t val);
#endif

#ifdef AARCHXX
byte *
emit_fcache_enter_gonative(dcontext_t *dcontext, generated_code_t *code, byte *pc);
#endif

#ifdef WINDOWS
/* PR 282576: These separate routines are ugly, but less ugly than adding param to
 * the main routines, which are called in many places and usually passed a
 * non-global dcontext; also less ugly than adding GLOBAL_DCONTEXT_X86.
 */
cache_pc
after_shared_syscall_code_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));
cache_pc
after_do_syscall_code_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));

byte *
emit_fcache_enter_indirect(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                           byte *fcache_return_pc);
byte *
emit_do_callback_return(dcontext_t *dcontext, byte *pc, byte *fcache_return_pc,
                        bool thread_shared);
#else
byte *
emit_do_clone_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      byte *fcache_return_pc, bool thread_shared,
                      uint *syscall_offs /*OUT*/);
#    ifdef VMX86_SERVER
byte *
emit_do_vmkuw_syscall(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      byte *fcache_return_pc, bool thread_shared,
                      uint *syscall_offs /*OUT*/);
#    endif
#endif

#ifdef UNIX
byte *
emit_new_thread_dynamo_start(dcontext_t *dcontext, byte *pc);

cache_pc
get_new_thread_start(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode));
#endif

#ifdef TRACE_HEAD_CACHE_INCR
byte *
emit_trace_head_incr(dcontext_t *dcontext, byte *pc, byte *fcache_return_pc);
byte *
emit_trace_head_incr_shared(dcontext_t *dcontext, byte *pc, byte *fcache_return_pc);
#endif

byte *
emit_client_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code);

#ifdef UNIX
byte *
emit_native_plt_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code);

byte *
emit_native_ret_ibl_xfer(dcontext_t *dcontext, byte *pc, generated_code_t *code);
#endif

byte *
emit_clean_call_save(dcontext_t *dcontext, byte *pc, generated_code_t *code);

byte *
emit_clean_call_restore(dcontext_t *dcontext, byte *pc, generated_code_t *code);

void
insert_save_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, uint flags,
                   bool tls, bool absolute _IF_X86_64(bool x86_to_x64_ibl_opt));
void
insert_restore_eflags(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                      uint flags, bool tls,
                      bool absolute _IF_X86_64(bool x86_to_x64_ibl_opt));

instr_t *
create_syscall_instr(dcontext_t *dcontext);

void
insert_shared_get_dcontext(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                           bool save_xdi);

void
insert_shared_restore_dcontext_reg(dcontext_t *dcontext, instrlist_t *ilist,
                                   instr_t *where);

/* in optimize.c */
instr_t *
find_next_self_loop(dcontext_t *dcontext, app_pc tag, instr_t *instr);
void
replace_inst(dcontext_t *dcontext, instrlist_t *ilist, instr_t *old, instr_t *new);
void
remove_redundant_loads(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
void
remove_dead_code(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);

#ifdef CHECK_RETURNS_SSE2
/* in retcheck.c */
void
check_return_handle_call(dcontext_t *dcontext, instrlist_t *ilist, instr_t *next);
void
check_return_handle_return(dcontext_t *dcontext, instrlist_t *ilist, instr_t *next);
void
check_return_too_deep(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                      volatile reg_t reg_edi, volatile reg_t reg_esi,
                      volatile reg_t reg_ebp, volatile reg_t reg_esp,
                      volatile reg_t reg_ebx, volatile reg_t reg_edx,
                      volatile reg_t reg_ecx, volatile reg_t reg_eax);
void
check_return_too_shallow(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                         volatile reg_t reg_edi, volatile reg_t reg_esi,
                         volatile reg_t reg_ebp, volatile reg_t reg_esp,
                         volatile reg_t reg_ebx, volatile reg_t reg_edx,
                         volatile reg_t reg_ecx, volatile reg_t reg_eax);
void
check_return_ra_mangled(dcontext_t *dcontext, volatile int errno, volatile reg_t eflags,
                        volatile reg_t reg_edi, volatile reg_t reg_esi,
                        volatile reg_t reg_ebp, volatile reg_t reg_esp,
                        volatile reg_t reg_ebx, volatile reg_t reg_edx,
                        volatile reg_t reg_ecx, volatile reg_t reg_eax);
#endif

#ifdef UNIX
void
new_thread_setup(priv_mcontext_t *mc);
#    ifdef MACOS
void
new_bsdthread_setup(priv_mcontext_t *mc);
/* Enable writing to MAP_JIT pages.
 * This is for the local thread only and not process-wide.
 */

#        define PTHREAD_JIT_WRITE() pthread_jit_write_protect_np(false)
/* Enable writing to MAP_JIT pages.
 * This is for the local thread only and not process-wide.
 */
#        define PTHREAD_JIT_READ() pthread_jit_write_protect_np(true)
void
pthread_jit_write_protect_np(int);
#    endif
#endif

#ifndef PTHREAD_JIT_WRITE
#    define PTHREAD_JIT_WRITE()
#    define PTHREAD_JIT_READ()
#endif

void
get_simd_vals(priv_mcontext_t *mc);

/* i#350: Fast safe_read without dcontext.  On success or failure, returns the
 * current source pointer.  Requires fault handling to be set up.
 */
void *
safe_read_asm(void *dst, const void *src, size_t size);
/* These are labels, not function pointers.  We declare them as functions to
 * prevent loads and stores to these globals from compiling.
 */
void
safe_read_asm_pre(void);
void
safe_read_asm_mid(void);
void
safe_read_asm_post(void);
void
safe_read_asm_recover(void);

/* from x86.asm */
/* Note these have specialized calling conventions and shouldn't be called from
 * C code (see comments in x86.asm). */
void
global_do_syscall_sysenter(void);
void
global_do_syscall_int(void);
void
global_do_syscall_sygate_int(void);
void
global_do_syscall_sygate_sysenter(void);
#ifdef WINDOWS
void
global_do_syscall_wow64(void);
void
global_do_syscall_wow64_index0(void);
#endif
#ifdef X64
void
global_do_syscall_syscall(void);
#endif
void
get_xmm_caller_saved(dr_zmm_t *xmm_caller_saved_buf);
void
get_ymm_caller_saved(dr_zmm_t *ymm_caller_saved_buf);
void
get_zmm_caller_saved(dr_zmm_t *zmm_caller_saved_buf);
void
get_opmask_caller_saved(dr_opmask_t *opmask_caller_saved_buf);

/* in encode.c */
byte *
instr_encode_ignore_reachability(dcontext_t *dcontext_t, instr_t *instr, byte *pc);
byte *
instr_encode_check_reachability(dcontext_t *dcontext_t, instr_t *instr, byte *pc,
                                bool *has_instr_opnds /*OUT OPTIONAL*/);
byte *
copy_and_re_relativize_raw_instr(dcontext_t *dcontext, instr_t *instr, byte *dst_pc,
                                 byte *final_pc);
#ifdef ARM
byte *
encode_raw_jmp(dr_isa_mode_t isa_mode, byte *target_pc, byte *dst_pc, byte *final_pc);
void
encode_track_it_block(dcontext_t *dcontext, instr_t *instr);
#endif

/* In instr_shared.c. */
uint
move_mm_reg_opcode(bool aligned16, bool aligned32);

/* In instr_shared.c. We have a separate function for AVX-512, because we do not want to
 * introduce AVX-512 code if not explicitly requested, due to DynamoRIO's lazy AVX-512
 * context switching.
 */
uint
move_mm_avx512_reg_opcode(bool aligned64);

bool
clean_call_needs_simd(clean_call_info_t *cci);

/* clean call optimization */
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
    bool bailout;      /* if we bail out on function analysis */
    uint num_args;     /* number of args that will passed in */
    int num_instrs;    /* total number of instructions of a function */
    app_pc start;      /* entry point of a function  */
    app_pc bwd_tgt;    /* earliest backward branch target */
    app_pc fwd_tgt;    /* last forward branch target */
    int num_simd_used; /* number of SIMD registers (xmms) used by callee */
    /* SIMD ([xyz]mm) registers usage. Part of the array might be left
     * uninitialized if proc_num_simd_registers() < MCXT_NUM_SIMD_SLOTS.
     */
    bool simd_used[MCXT_NUM_SIMD_SLOTS];
#ifdef X86
    int num_opmask_used; /* number of mask registers used by callee */
    /* AVX-512 mask register usage. */
    bool opmask_used[MCXT_NUM_OPMASK_SLOTS];
#endif
    bool reg_used[DR_NUM_GPR_REGS];         /* general purpose registers usage */
    int num_callee_save_regs;               /* number of regs callee saved */
    bool callee_save_regs[DR_NUM_GPR_REGS]; /* callee-save registers */
    bool has_locals;                        /* if reference local via stack */
    bool standard_fp;   /* if standard reg (xbp/x29) is used as frame pointer */
    bool opt_inline;    /* can be inlined or not */
    bool write_flags;   /* if the function changes flags */
    bool read_flags;    /* if the function reads flags from caller */
    bool tls_used;      /* application accesses TLS (errno, etc.) */
    reg_id_t spill_reg; /* base register for spill slots */
    uint slots_used;    /* scratch slots needed after analysis */
    slot_t scratch_slots[CLEANCALL_NUM_INLINE_SLOTS]; /* scratch slot allocation */
    instrlist_t *ilist; /* instruction list of function for inline. */
} callee_info_t;
extern callee_info_t default_callee_info;
extern clean_call_info_t default_clean_call_info;

/* in clean_call_opt_shared.c */
void
clean_call_opt_init(void);
void
clean_call_opt_exit(void);
bool
analyze_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instr_t *where,
                   void *callee, bool save_fpstate, bool always_out_of_line,
                   uint num_args, opnd_t *args);
void
insert_inline_clean_call(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                         instr_t *where, opnd_t *args);

/* in mangle.c */
void
insert_push_retaddr(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr,
                    ptr_int_t retaddr, opnd_size_t opsize);

ptr_uint_t
get_call_return_address(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

/* if translation is null, uses raw bits (assumes instr was just decoded from app) */
app_pc
get_app_instr_xl8(instr_t *instr);

#ifdef X64
/* in x86_to_x64.c */
void
translate_x86_to_x64(dcontext_t *dcontext, instrlist_t *ilist, INOUT instr_t **instr);
#endif

#ifdef AARCHXX
bool
instr_is_ldstex_mangling(dcontext_t *dcontext, instr_t *inst);
#endif

/****************************************************************************
 * Platform-independent emit_utils_shared.c
 */
/* add an instruction to patch list and address of location for future updates */
/* Use the type checked wrappers add_patch_entry or add_patch_marker */
void
add_patch_entry_internal(patch_list_t *patch, instr_t *instr, ushort patch_flags,
                         short instruction_offset, ptr_uint_t value_location_offset);
cache_pc
get_direct_exit_target(dcontext_t *dcontext, uint flags);

#ifdef AARCHXX
size_t
get_fcache_return_tls_offs(dcontext_t *dcontext, uint flags);

size_t
get_ibl_entry_tls_offs(dcontext_t *dcontext, cache_pc ibl_entry);
#endif

void
link_indirect_exit_arch(dcontext_t *dcontext, fragment_t *f, linkstub_t *l,
                        bool hot_patch, app_pc target_tag);
cache_pc
exit_cti_disp_pc(cache_pc branch_pc);
void
append_ibl_found(dcontext_t *dcontext, instrlist_t *ilist, ibl_code_t *ibl_code,
                 patch_list_t *patch, uint start_pc_offset, bool collision,
                 bool only_spill_state_in_tls, /* if true, no table info in TLS;
                                                * indirection off of XDI is used */
                 bool restore_eflags, instr_t **fragment_found);
#ifdef HASHTABLE_STATISTICS
#    define HASHLOOKUP_STAT_OFFS(event) (offsetof(hashtable_statistics_t, event##_stat))
void
append_increment_counter(dcontext_t *dcontext, instrlist_t *ilist, ibl_code_t *ibl_code,
                         patch_list_t *patch,
                         reg_id_t entry_register, /* register indirect (XCX) or NULL */
                         /* adjusted to unprot_ht_statistics_t if no entry_register */
                         uint counter_offset, reg_id_t scratch_register);
#endif

void
relink_special_ibl_xfer(dcontext_t *dcontext, int index,
                        ibl_entry_point_type_t entry_type, ibl_branch_type_t ibl_type);

byte *
special_ibl_xfer_tgt(dcontext_t *dcontext, generated_code_t *code,
                     ibl_entry_point_type_t entry_type, ibl_branch_type_t ibl_type);

/* we are sharing bbs w/o ibs -- we assume that a bb
 * w/ a direct branch cannot have an ib and thus is shared
 */
#ifdef TRACE_HEAD_CACHE_INCR
/* incr routine can't tell whether coming from shared bb
 * or non-shared fragment (such as a trace) so must always
 * use shared stubs
 */
#    define FRAG_DB_SHARED(flags) true
#else
#    define FRAG_DB_SHARED(flags) (TEST(FRAG_SHARED, (flags)))
#endif

/* fragment_t fields */
#define FRAGMENT_TAG_OFFS (offsetof(fragment_t, tag))

enum {
    PREFIX_SIZE_RESTORE_OF = 2,  /* add $0x7f, %al  */
    PREFIX_SIZE_FIVE_EFLAGS = 1, /* SAHF */
};

/* PR 244737: x64 always uses tls even if all-private */
#define IBL_EFLAGS_IN_TLS() (IF_X64_ELSE(true, SHARED_IB_TARGETS()))

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
            !(DYNAMO_OPTION(coarse_units) && !TEST(FRAG_IS_TRACE, flags) &&
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

/* add an instruction to patch list and address of location for future updates */
static inline void
add_patch_entry(patch_list_t *patch, instr_t *instr, ushort patch_flags,
                ptr_uint_t value_location_offset)
{
    add_patch_entry_internal(patch, instr, patch_flags, -4 /* offset of imm32 argument */,
                             value_location_offset);
}

/****************************************************************************
 * Platform-specific {x86/arm}/emit_utils.c
 */
/* macros shared by fcache_enter and fcache_return
 * in order to generate both thread-private code that uses absolute
 * addressing and thread-shared or dcontext-shared code that uses
 * scratch_reg5(xdi/r5) (and scratch_reg4(xsi/r4)) for addressing.
 * The via_reg macros now auto-magically pick the opnd size from the
 * target register and so work with more than just pointer-sized values.
 */
/* PR 244737: even thread-private fragments use TLS on x64.  We accomplish
 * that at the caller site, so we should never see an "absolute" request.
 */
#define RESTORE_FROM_DC(dc, reg, offs) \
    RESTORE_FROM_DC_VIA_REG(absolute, dc, REG_NULL, reg, offs)
/* Note the magic absolute boolean that callers are expected to have declared */
#define SAVE_TO_DC(dc, reg, offs) SAVE_TO_DC_VIA_REG(absolute, dc, REG_NULL, reg, offs)

#define OPND_TLS_FIELD(offs) opnd_create_tls_slot(os_tls_offset(offs))

#define OPND_TLS_FIELD_SZ(offs, sz) opnd_create_sized_tls_slot(os_tls_offset(offs), sz)

#define SAVE_TO_TLS(dc, reg, offs) instr_create_save_to_tls(dc, reg, offs)
#define RESTORE_FROM_TLS(dc, reg, offs) instr_create_restore_from_tls(dc, reg, offs)

#define SAVE_TO_REG(dc, reg, spill) instr_create_save_to_reg(dc, reg, spill)
#define RESTORE_FROM_REG(dc, reg, spill) instr_create_restore_from_reg(dc, reg, spill)

#define OPND_DC_FIELD(absolute, dcontext, sz, offs)                    \
    ((absolute)                                                        \
         ? (IF_X64_(ASSERT_NOT_IMPLEMENTED(false))                     \
                opnd_create_dcontext_field_sz(dcontext, (offs), (sz))) \
         : opnd_create_dcontext_field_via_reg_sz((dcontext), REG_NULL, (offs), (sz)))

/* PR 244737: even thread-private fragments use TLS on x64.  We accomplish
 * that at the caller site, so we should never see an "absolute" request.
 */
#define RESTORE_FROM_DC_VIA_REG(absolute, dc, reg_dr, reg, offs)                \
    ((absolute) ? (IF_X64_(ASSERT_NOT_IMPLEMENTED(false))                       \
                       instr_create_restore_from_dcontext((dc), (reg), (offs))) \
                : instr_create_restore_from_dc_via_reg((dc), reg_dr, (reg), (offs)))
/* Note the magic absolute boolean that callers are expected to have declared */
#define SAVE_TO_DC_VIA_REG(absolute, dc, reg_dr, reg, offs)                \
    ((absolute) ? (IF_X64_(ASSERT_NOT_IMPLEMENTED(false))                  \
                       instr_create_save_to_dcontext((dc), (reg), (offs))) \
                : instr_create_save_to_dc_via_reg((dc), reg_dr, (reg), (offs)))

#ifdef ARM
/* Push/pop GPR helpers */
#    define DR_REG_LIST_HEAD                                          \
        opnd_create_reg(DR_REG_R0), opnd_create_reg(DR_REG_R1),       \
            opnd_create_reg(DR_REG_R2), opnd_create_reg(DR_REG_R3),   \
            opnd_create_reg(DR_REG_R4), opnd_create_reg(DR_REG_R5),   \
            opnd_create_reg(DR_REG_R6), opnd_create_reg(DR_REG_R7),   \
            opnd_create_reg(DR_REG_R8), opnd_create_reg(DR_REG_R9),   \
            opnd_create_reg(DR_REG_R10), opnd_create_reg(DR_REG_R11), \
            opnd_create_reg(DR_REG_R12)

#    ifdef X64
#        define DR_REG_LIST_LENGTH_ARM 32
#        define DR_REG_LIST_ARM                                                         \
            DR_REG_LIST_HEAD, opnd_create_reg(DR_REG_R13), opnd_create_reg(DR_REG_X14), \
                opnd_create_reg(DR_REG_X15), opnd_create_reg(DR_REG_X16),               \
                opnd_create_reg(DR_REG_X17), opnd_create_reg(DR_REG_X18),               \
                opnd_create_reg(DR_REG_X19), opnd_create_reg(DR_REG_X20),               \
                opnd_create_reg(DR_REG_X21), opnd_create_reg(DR_REG_X22),               \
                opnd_create_reg(DR_REG_X23), opnd_create_reg(DR_REG_X24),               \
                opnd_create_reg(DR_REG_X25), opnd_create_reg(DR_REG_X26),               \
                opnd_create_reg(DR_REG_X27), opnd_create_reg(DR_REG_X28),               \
                opnd_create_reg(DR_REG_X29), opnd_create_reg(DR_REG_X30),               \
                opnd_create_reg(DR_REG_X31)
#    else
#        define DR_REG_LIST_LENGTH_ARM 15 /* no R15 (pc) */
#        define DR_REG_LIST_ARM \
            DR_REG_LIST_HEAD, opnd_create_reg(DR_REG_R13), opnd_create_reg(DR_REG_R14)
#    endif
#    define DR_REG_LIST_LENGTH_T32 13 /* no R13+ (sp, lr, pc) */
#    define DR_REG_LIST_T32 DR_REG_LIST_HEAD

/* we can only push or pop 16 32-bit-sized SIMD registers at a time */
#    define SIMD_REG_LIST_LEN 16
#    define SIMD_REG_LIST_0_15                                        \
        opnd_create_reg(DR_REG_D0), opnd_create_reg(DR_REG_D1),       \
            opnd_create_reg(DR_REG_D2), opnd_create_reg(DR_REG_D3),   \
            opnd_create_reg(DR_REG_D4), opnd_create_reg(DR_REG_D5),   \
            opnd_create_reg(DR_REG_D6), opnd_create_reg(DR_REG_D7),   \
            opnd_create_reg(DR_REG_D8), opnd_create_reg(DR_REG_D9),   \
            opnd_create_reg(DR_REG_D10), opnd_create_reg(DR_REG_D11), \
            opnd_create_reg(DR_REG_D12), opnd_create_reg(DR_REG_D13), \
            opnd_create_reg(DR_REG_D14), opnd_create_reg(DR_REG_D15)
#    define SIMD_REG_LIST_16_31                                       \
        opnd_create_reg(DR_REG_D16), opnd_create_reg(DR_REG_D17),     \
            opnd_create_reg(DR_REG_D18), opnd_create_reg(DR_REG_D19), \
            opnd_create_reg(DR_REG_D20), opnd_create_reg(DR_REG_D21), \
            opnd_create_reg(DR_REG_D22), opnd_create_reg(DR_REG_D23), \
            opnd_create_reg(DR_REG_D24), opnd_create_reg(DR_REG_D25), \
            opnd_create_reg(DR_REG_D26), opnd_create_reg(DR_REG_D27), \
            opnd_create_reg(DR_REG_D28), opnd_create_reg(DR_REG_D29), \
            opnd_create_reg(DR_REG_D30), opnd_create_reg(DR_REG_D31)
#endif

int
fragment_ibt_prefix_size(uint flags);

void
append_fcache_enter_prologue(dcontext_t *dcontext, instrlist_t *ilist, bool absolute);
void
append_call_exit_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool absolute,
                         bool shared);
void
append_restore_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute);
void
append_restore_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute);
void
append_restore_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool absolute);
void
append_save_gpr(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end, bool absolute,
                generated_code_t *code, linkstub_t *linkstub, bool coarse_info);
void
append_save_simd_reg(dcontext_t *dcontext, instrlist_t *ilist, bool absolute);
void
append_save_clear_xflags(dcontext_t *dcontext, instrlist_t *ilist, bool absolute);
bool
append_call_enter_dr_hook(dcontext_t *dcontext, instrlist_t *ilist, bool ibl_end,
                          bool absolute);
bool
append_fcache_return_common(dcontext_t *dcontext, generated_code_t *code,
                            instrlist_t *ilist, bool ibl_end, bool absolute, bool shared,
                            linkstub_t *linkstub, bool coarse_info);
void
append_ibl_head(dcontext_t *dcontext, instrlist_t *ilist, ibl_code_t *ibl_code,
                patch_list_t *patch, instr_t **fragment_found, instr_t **compare_tag_inst,
                instr_t **post_eflags_save, opnd_t miss_tgt, bool miss_8bit,
                bool target_trace_table, bool inline_ibl_head);
#ifdef X64
void
instrlist_convert_to_x86(instrlist_t *ilist);
#endif
#ifdef AARCHXX
bool
mrs_id_reg_supported(void);
#endif
#endif /* ARCH_H */
