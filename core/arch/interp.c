/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * interp.c - interpreter used for native trace selection
 */

#include "../globals.h"
#include "../link.h"
#include "../fragment.h"
#include "../emit.h"
#include "../dispatch.h"
#include "../fcache.h"
#include "../monitor.h" /* for trace_abort and monitor_data_t */
#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "decode.h"
#include "decode_fast.h"
#include "disassemble.h"
#include "instrument.h"
#include "../hotpatch.h"
#ifdef RETURN_AFTER_CALL
#    include "../rct.h"
#endif
#ifdef WINDOWS
#    include "ntdll.h"    /* for EXCEPTION_REGISTRATION */
#    include "../nudge.h" /* for generic_nudge_target() address */
#endif
#include "../perscache.h"
#include "../native_exec.h"
#include "../jit_opt.h"

#ifdef CHECK_RETURNS_SSE2
#    include <setjmp.h> /* for warning when see libc setjmp */
#endif

#ifdef VMX86_SERVER
#    include "vmkuw.h" /* VMKUW_SYSCALL_GATEWAY */
#endif

#ifdef ANNOTATIONS
#    include "../annotations.h"
#endif

#ifdef AARCH64
#    include "build_ldstex.h"
#endif

enum { DIRECT_XFER_LENGTH = 5 };

/* forward declarations */
static void
process_nops_for_trace(dcontext_t *dcontext, instrlist_t *ilist,
                       uint flags _IF_DEBUG(bool recreating));
static int
fixup_last_cti(dcontext_t *dcontext, instrlist_t *trace, app_pc next_tag, uint next_flags,
               uint trace_flags, fragment_t *prev_f, linkstub_t *prev_l,
               bool record_translation, uint *num_exits_deleted /*OUT*/,
               /* If non-NULL, only looks inside trace between these two */
               instr_t *start_instr, instr_t *end_instr);
bool
mangle_trace(dcontext_t *dcontext, instrlist_t *ilist, monitor_data_t *md);

/* we use a branch limit of 1 to make it easier for the trace
 * creation mechanism to stitch basic blocks together
 */
#define BRANCH_LIMIT 1

/* we limit total bb size to handle cases like infinite loop or sequence
 * of calls.
 * also, we have a limit on fragment body sizes, which should be impossible
 * to break since x86 instrs are max 17 bytes and we only modify ctis.
 * Although...selfmod mangling does really expand fragments!
 * -selfmod_max_writes helps for selfmod bbs (case 7893/7909).
 * System call mangling is also large, for degenerate cases like tests/linux/infinite.
 * PR 215217: also client additions: we document and assert.
 * FIXME: need better way to know how big will get, b/c we can construct
 * cases that will trigger the size assertion!
 */
/* define replaced by -max_bb_instrs option */

/* exported so micro routines can assert whether held */
DECLARE_CXTSWPROT_VAR(mutex_t bb_building_lock, INIT_LOCK_FREE(bb_building_lock));

/* i#1111: we do not use the lock until the 2nd thread is created */
volatile bool bb_lock_start;

static file_t bbdump_file = INVALID_FILE;

#ifdef DEBUG
DECLARE_NEVERPROT_VAR(uint debug_bb_count, 0);
#endif

/* initialization */
void
interp_init()
{
    if (INTERNAL_OPTION(bbdump_tags)) {
        bbdump_file = open_log_file("bbs", NULL, 0);
        ASSERT(bbdump_file != INVALID_FILE);
    }
}

#ifdef CUSTOM_TRACES_RET_REMOVAL
#    ifdef DEBUG
/* don't bother with adding lock */
static int num_rets_removed;
#    endif
#endif

/* cleanup */
void
interp_exit()
{
    if (INTERNAL_OPTION(bbdump_tags)) {
        close_log_file(bbdump_file);
    }
    DELETE_LOCK(bb_building_lock);

    LOG(GLOBAL, LOG_INTERP | LOG_STATS, 1, "Total application code seen: %d KB\n",
        GLOBAL_STAT(app_code_seen) / 1024);
#ifdef CUSTOM_TRACES_RET_REMOVAL
#    ifdef DEBUG
    LOG(GLOBAL, LOG_INTERP | LOG_STATS, 1, "Total rets removed: %d\n", num_rets_removed);
#    endif
#endif
}

/****************************************************************************
 ****************************************************************************
 *
 * B A S I C   B L O C K   B U I L D I N G
 */

/* we have a lot of data to pass around so we package it in this struct
 * so we can have separate routines for readability
 */
typedef struct {
    /* in */
    app_pc start_pc;
    bool app_interp;           /* building bb to interp app, as opposed to for pc
                                * translation or figuring out what pages a bb touches? */
    bool for_cache;            /* normal to-be-executed build? */
    bool record_vmlist;        /* should vmareas be updated? */
    bool mangle_ilist;         /* should bb ilist be mangled? */
    bool record_translation;   /* store translation info for each instr_t? */
    bool has_bb_building_lock; /* usually ==for_cache; used for aborting bb building */
    bool checked_start_vmarea; /* caller called check_new_page_start() on start_pc */
    file_t outf;               /* send disassembly and notes to a file?
                                * we use this mainly for dumping trace origins */
    app_pc stop_pc;            /* Optional: NULL for normal termination rules.
                                * Only checked for full_decode.
                                */
    bool pass_to_client;       /* pass to client, if a bb hook exists;
                                * we store this up front to avoid race conditions
                                * between full_decode setting and hook calling time.
                                */
    bool post_client;          /* has the client already processed the bb? */
    bool for_trace;            /* PR 299808: we tell client if building a trace */

    /* in and out */
    overlap_info_t *overlap_info; /* if non-null, records overlap information here;
                                   * caller must initialize region_start and region_end */

    /* out */
    instrlist_t *ilist;
    uint flags;
    void *vmlist;
    app_pc end_pc;
    bool native_exec;              /* replace cur ilist with a native_exec version */
    bool native_call;              /* the gateway is a call */
    instrlist_t **unmangled_ilist; /* PR 299808: clone ilist pre-mangling */

    /* internal usage only */
    bool full_decode;   /* decode every instruction into a separate instr_t? */
    bool follow_direct; /* elide unconditional branches? */
    bool check_vm_area; /* whether to call check_thread_vm_area() */
    uint num_elide_jmp;
    uint num_elide_call;
    app_pc last_page;
    app_pc cur_pc;
    app_pc instr_start;
    app_pc checked_end;                /* end of current vmarea checked */
    cache_pc exit_target;              /* fall-through target of final instr */
    uint exit_type;                    /* indirect branch type  */
    ibl_branch_type_t ibl_branch_type; /* indirect branch type as an IBL selector */
    instr_t *instr;                    /* the current instr */
    int eflags;
    app_pc pretend_pc; /* selfmod only: decode from separate pc */
#ifdef ARM
    dr_pred_type_t svc_pred; /* predicate for conditional svc */
#endif
    DEBUG_DECLARE(bool initialized;)
} build_bb_t;

/* forward decl */
static inline bool
bb_process_syscall(dcontext_t *dcontext, build_bb_t *bb);

static void
init_build_bb(build_bb_t *bb, app_pc start_pc, bool app_interp, bool for_cache,
              bool mangle_ilist, bool record_translation, file_t outf, uint known_flags,
              overlap_info_t *overlap_info)
{
    memset(bb, 0, sizeof(*bb));
#if defined(LINUX) && defined(X86_32)
    /* With SA_RESTART (i#2659) we end up interpreting the int 0x80 in vsyscall,
     * whose fall-through hits our hook.  We avoid interpreting our own hook
     * by shifting it to the displaced pc.
     */
    if (DYNAMO_OPTION(hook_vsyscall) && start_pc == vsyscall_sysenter_return_pc) {
        if (vsyscall_sysenter_displaced_pc != NULL)
            start_pc = vsyscall_sysenter_displaced_pc;
        else {
            /* Our hook must have failed. */
            ASSERT(should_syscall_method_be_sysenter());
        }
    }
#endif
    bb->check_vm_area = true;
    bb->start_pc = start_pc;
    bb->app_interp = app_interp;
    bb->for_cache = for_cache;
    if (bb->for_cache)
        bb->record_vmlist = true;
    bb->mangle_ilist = mangle_ilist;
    bb->record_translation = record_translation;
    bb->outf = outf;
    bb->overlap_info = overlap_info;
    bb->follow_direct = !TEST(FRAG_SELFMOD_SANDBOXED, known_flags);
    bb->flags = known_flags;
    bb->ibl_branch_type = IBL_GENERIC; /* initialization only */
#ifdef ARM
    bb->svc_pred = DR_PRED_NONE;
#endif
    DODEBUG(bb->initialized = true;);
}

static void
reset_overlap_info(dcontext_t *dcontext, build_bb_t *bb)
{
    bb->overlap_info->start_pc = bb->start_pc;
    bb->overlap_info->min_pc = bb->start_pc;
    bb->overlap_info->max_pc = bb->start_pc;
    bb->overlap_info->contiguous = true;
    bb->overlap_info->overlap = false;
}

static void
update_overlap_info(dcontext_t *dcontext, build_bb_t *bb, app_pc new_pc, bool jmp)
{
    if (new_pc < bb->overlap_info->min_pc)
        bb->overlap_info->min_pc = new_pc;
    if (new_pc > bb->overlap_info->max_pc)
        bb->overlap_info->max_pc = new_pc;
    /* we get called at end of all contiguous intervals, so ignore jmps */
    LOG(THREAD, LOG_ALL, 5, "\t    app_bb_overlaps " PFX ".." PFX " %s\n", bb->last_page,
        new_pc, jmp ? "jmp" : "");
    if (!bb->overlap_info->overlap && !jmp) {
        /* contiguous interval: prev_pc..new_pc (open-ended) */
        if (bb->last_page < bb->overlap_info->region_end &&
            new_pc > bb->overlap_info->region_start) {
            LOG(THREAD_GET, LOG_ALL, 5, "\t    it overlaps!\n");
            bb->overlap_info->overlap = true;
        }
    }
    if (bb->overlap_info->contiguous && jmp)
        bb->overlap_info->contiguous = false;
}

#ifdef DEBUG
#    define BBPRINT(bb, level, ...)                               \
        do {                                                      \
            LOG(THREAD, LOG_INTERP, level, __VA_ARGS__);          \
            if (bb->outf != INVALID_FILE && bb->outf != (THREAD)) \
                print_file(bb->outf, __VA_ARGS__);                \
        } while (0);
#else
#    ifdef INTERNAL
#        define BBPRINT(bb, level, ...)                \
            do {                                       \
                if (bb->outf != INVALID_FILE)          \
                    print_file(bb->outf, __VA_ARGS__); \
            } while (0);
#    else
#        define BBPRINT(bb, level, ...) /* nothing */
#    endif
#endif

#ifdef WINDOWS
extern void
intercept_load_dll(void);
extern void
intercept_unload_dll(void);
#    ifdef INTERNAL
extern void
DllMainThreadAttach(void);
#    endif
#endif

/* forward declarations */
static bool
mangle_bb_ilist(dcontext_t *dcontext, build_bb_t *bb);

static void
build_native_exec_bb(dcontext_t *dcontext, build_bb_t *bb);

static bool
at_native_exec_gateway(dcontext_t *dcontext, app_pc start,
                       bool *is_call _IF_DEBUG(bool xfer_target));

#ifdef DEBUG
static void
report_native_module(dcontext_t *dcontext, app_pc modpc);
#endif

/***************************************************************************
 * Image entry
 */

static bool reached_image_entry = false;

static INLINE_FORCED bool
check_for_image_entry(app_pc bb_start)
{
    if (!reached_image_entry && bb_start == get_image_entry()) {
        LOG(THREAD_GET, LOG_ALL, 1, "Reached image entry point " PFX "\n", bb_start);
        set_reached_image_entry();
        return true;
    }
    return false;
}

void
set_reached_image_entry()
{
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    reached_image_entry = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

bool
reached_image_entry_yet()
{
    return reached_image_entry;
}

/***************************************************************************
 * Whether to inline or elide callees
 */

/* Return true if pc is a call target that should NOT be entered but should
 * still be mangled.
 */
static inline bool
must_not_be_entered(app_pc pc)
{
    return false
#ifdef DR_APP_EXPORTS
        /* i#1237: DR will change dr_app_running_under_dynamorio return value
         * on seeing a bb starting at dr_app_running_under_dynamorio.
         */
        || pc == (app_pc)dr_app_running_under_dynamorio
#endif
        ;
}

/* Return true if pc is a call target that should NOT be inlined and left native. */
static inline bool
leave_call_native(app_pc pc)
{
    return (
#ifdef INTERNAL
        !dynamo_options.inline_calls
#else
        0
#endif
#ifdef WINDOWS
        || pc == (app_pc)intercept_load_dll || pc == (app_pc)intercept_unload_dll
    /* we're guaranteed to have direct calls to the next routine since our
     * own DllMain calls it! */
#    ifdef INTERNAL
        || pc == (app_pc)DllMainThreadAttach
#    endif
        /* check for nudge handling escape from cache */
        || (pc == (app_pc)generic_nudge_handler)
#else
        /* PR 200203: long-term we want to control loading of client
         * libs, but for now we have to let the loader call _fini()
         * in the client, which may end up calling __wrap_free().
         * It's simpler to let those be interpreted and make a native
         * call to the real heap routine here as this is a direct
         * call whereas we'd need native_exec for the others:
         */
        || pc == (app_pc)global_heap_free
#endif
    );
}

/* return true if pc is a direct jmp target that should NOT be elided and followed */
static inline bool
must_not_be_elided(app_pc pc)
{
#ifdef WINDOWS
    /* Allow only the return jump in the landing pad to be elided, as we
     * interpret the return path from trampolines.  The forward jump leads to
     * the trampoline and shouldn't be elided. */
    if (is_on_interception_initial_route(pc))
        return true;
#endif
    return (0
#ifdef WINDOWS
            /* we insert trampolines by adding direct jmps to our interception code buffer
             * we don't want to interpret the code in that buffer, as it may swap to the
             * dstack and mess up a return-from-fcache.
             * N.B.: if use this routine anywhere else, pay attention to the
             * hack for is_syscall_trampoline() in the use here!
             */
            || (is_in_interception_buffer(pc))
#else /* UNIX */
#endif
    );
}

#ifdef DR_APP_EXPORTS
/* This function allows automatically injected dynamo to ignore
 * dynamo API routines that would really mess things up
 */
static inline bool
must_escape_from(app_pc pc)
{
    /* if ever find ourselves at top of one of these, immediately issue
     * a ret instruction...haven't set up frame yet so stack fine, only
     * problem is return value, go ahead and overwrite xax, it's caller-saved
     * FIXME: is this ok?
     */
    /* Note that we can't just look for direct calls to these functions
     * because of stubs, etc. that end up doing indirect jumps to them!
     */
    bool res = false
#    ifdef DR_APP_EXPORTS
        || (automatic_startup &&
            (pc == (app_pc)dynamorio_app_init || pc == (app_pc)dr_app_start ||
             pc == (app_pc)dynamo_thread_init || pc == (app_pc)dynamorio_app_exit ||
             /* dr_app_stop is a nop already */
             pc == (app_pc)dynamo_thread_exit))
#    endif
        ;
#    ifdef DEBUG
    if (res) {
#        ifdef DR_APP_EXPORTS
        LOG(THREAD_GET, LOG_INTERP, 3, "must_escape_from: found ");
        if (pc == (app_pc)dynamorio_app_init)
            LOG(THREAD_GET, LOG_INTERP, 3, "dynamorio_app_init\n");
        else if (pc == (app_pc)dr_app_start)
            LOG(THREAD_GET, LOG_INTERP, 3, "dr_app_start\n");
        /* FIXME: are dynamo_thread_* still needed hered? */
        else if (pc == (app_pc)dynamo_thread_init)
            LOG(THREAD_GET, LOG_INTERP, 3, "dynamo_thread_init\n");
        else if (pc == (app_pc)dynamorio_app_exit)
            LOG(THREAD_GET, LOG_INTERP, 3, "dynamorio_app_exit\n");
        else if (pc == (app_pc)dynamo_thread_exit)
            LOG(THREAD_GET, LOG_INTERP, 3, "dynamo_thread_exit\n");
#        endif
    }
#    endif

    return res;
}
#endif /* DR_APP_EXPORTS */

/* Adds bb->instr, which must be a direct call or jmp, to bb->ilist for native
 * execution.  Makes sure its target is reachable from the code cache, which
 * is critical for jmps b/c they're native for our hooks of app code which may
 * not be reachable from the code cache.  Also needed for calls b/c in the future
 * (i#774) the DR lib (and thus our leave_call_native() calls) won't be reachable
 * from the cache.
 */
static void
bb_add_native_direct_xfer(dcontext_t *dcontext, build_bb_t *bb, bool appended)
{
#if defined(X86) && defined(X64)
    /* i#922: we're going to run this jmp from our code cache so we have to
     * make sure it still reaches its target.  We could try to check
     * reachability from the likely code cache slot, but these should be
     * rare enough that making them indirect won't matter and then we have
     * fewer reachability dependences.
     * We do this here rather than in d_r_mangle() b/c we'd have a hard time
     * distinguishing native jmp/call due to DR's own operations from a
     * client's inserted meta jmp/call.
     */
    /* Strategy: write target into xax (DR-reserved) slot and jmp through it.
     * Alternative would be to embed the target into the code stream.
     * We don't need to set translation b/c these are meta instrs and they
     * won't fault.
     */
    ptr_uint_t tgt = (ptr_uint_t)opnd_get_pc(instr_get_target(bb->instr));
    opnd_t tls_slot = opnd_create_sized_tls_slot(os_tls_offset(TLS_XAX_SLOT), OPSZ_4);
    instrlist_meta_append(
        bb->ilist, INSTR_CREATE_mov_imm(dcontext, tls_slot, OPND_CREATE_INT32((int)tgt)));
    opnd_set_disp(&tls_slot, opnd_get_disp(tls_slot) + 4);
    instrlist_meta_append(
        bb->ilist,
        INSTR_CREATE_mov_imm(dcontext, tls_slot, OPND_CREATE_INT32((int)(tgt >> 32))));
    if (instr_is_ubr(bb->instr)) {
        instrlist_meta_append(
            bb->ilist,
            INSTR_CREATE_jmp_ind(dcontext,
                                 opnd_create_tls_slot(os_tls_offset(TLS_XAX_SLOT))));
        bb->exit_type |= instr_branch_type(bb->instr);
    } else {
        ASSERT(instr_is_call_direct(bb->instr));
        instrlist_meta_append(
            bb->ilist,
            INSTR_CREATE_call_ind(dcontext,
                                  opnd_create_tls_slot(os_tls_offset(TLS_XAX_SLOT))));
    }
    if (appended)
        instrlist_remove(bb->ilist, bb->instr);
    instr_destroy(dcontext, bb->instr);
    bb->instr = NULL;
#elif defined(ARM)
    ASSERT_NOT_IMPLEMENTED(false); /* i#1582 */
#else
    if (appended) {
        /* avoid assert about meta w/ translation but no restore_state callback */
        instr_set_translation(bb->instr, NULL);
    } else
        instrlist_append(bb->ilist, bb->instr);
    /* Indicate that relative target must be
     * re-encoded, and that it is not an exit cti.
     * However, we must mangle this to ensure it reaches (i#992)
     * which we special-case in d_r_mangle().
     */
    instr_set_meta(bb->instr);
    instr_set_raw_bits_valid(bb->instr, false);
#endif
}

/* Perform checks such as looking for dynamo stopping points and bad places
 * to be.  We assume we only have to check after control transfer instructions,
 * i.e., we assume that all of these conditions are procedures that are only
 * entered by calling or jumping, never falling through.
 */
static inline bool
check_for_stopping_point(dcontext_t *dcontext, build_bb_t *bb)
{
#ifdef DR_APP_EXPORTS
    if (must_escape_from(bb->cur_pc)) {
        /* x64 will zero-extend to rax, so we use eax here */
        reg_id_t reg = IF_X86_ELSE(REG_EAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0));
        BBPRINT(bb, 3, "interp: emergency exit from " PFX "\n", bb->cur_pc);
        /* if ever find ourselves at top of one of these, immediately issue
         * a ret instruction...haven't set up frame yet so stack fine, only
         * problem is return value, go ahead and overwrite xax, it's
         * caller-saved.
         * FIXME: is this ok?
         */
        /* move 0 into xax/r0 -- our functions return 0 to indicate success */
        instrlist_append(
            bb->ilist,
            XINST_CREATE_load_int(dcontext, opnd_create_reg(reg), OPND_CREATE_INT32(0)));
        /* insert a ret instruction */
        instrlist_append(bb->ilist, XINST_CREATE_return(dcontext));
        /* should this be treated as a real return? */
        bb->exit_type |= LINK_INDIRECT | LINK_RETURN;
        bb->exit_target =
            get_ibl_routine(dcontext, IBL_LINKED, DEFAULT_IBL_BB(), IBL_RETURN);
        return true;
    }
#endif /* DR_APP_EXPORTS */

#ifdef CHECK_RETURNS_SSE2
    if (bb->cur_pc == (app_pc)longjmp) {
        SYSLOG_INTERNAL_WARNING("encountered longjmp, which will cause ret mismatch!");
    }
#endif

    return is_stopping_point(dcontext, bb->cur_pc);
}

/* Arithmetic eflags analysis to see if sequence of instrs reads an
 * arithmetic flag prior to writing it.
 * Usage: first initialize status to 0 and eflags_6 to 0.
 * Then call this routine for each instr in sequence, assigning result to status.
 * eflags_6 holds flags written and read so far.
 * Uses these flags, defined in instr.h, as status values:
 *   EFLAGS_WRITE_ARITH = writes all arith flags before reading any
 *   EFLAGS_WRITE_OF    = writes OF before reading it (x86-onlY)
 *   EFLAGS_READ_ARITH  = reads some of arith flags before writing
 *   EFLAGS_READ_OF     = reads OF before writing OF (x86-only)
 *   0                  = no information yet
 * On ARM, Q and GE flags are ignored.
 */
static inline int
eflags_analysis(instr_t *instr, int status, uint *eflags_6)
{
    uint e6 = *eflags_6; /* local copy */
    uint e6_w2r = EFLAGS_WRITE_TO_READ(e6);
    uint instr_eflags = instr_get_arith_flags(instr, DR_QUERY_DEFAULT);

    /* Keep going until result is non-zero, also keep going if
     * result is writes to OF to see if later writes to rest of flags
     * before reading any, and keep going if reads one of the 6 to see
     * if later writes to OF before reading it.
     */
    if (instr_eflags == 0 ||
        status == EFLAGS_WRITE_ARITH IF_X86(|| status == EFLAGS_READ_OF))
        return status;
    /* we ignore interrupts */
    if ((instr_eflags & EFLAGS_READ_ARITH) != 0 &&
        (!instr_opcode_valid(instr) || !instr_is_interrupt(instr))) {
        /* store the flags we're reading */
        e6 |= (instr_eflags & EFLAGS_READ_ARITH);
        *eflags_6 = e6;
        if ((e6_w2r | (instr_eflags & EFLAGS_READ_ARITH)) != e6_w2r) {
            /* we're reading a flag that has not been written yet */
            status = EFLAGS_READ_ARITH; /* some read before all written */
            LOG(THREAD_GET, LOG_INTERP, 4, "\treads flag before writing it!\n");
#ifdef X86
            if ((instr_eflags & EFLAGS_READ_OF) != 0 && (e6 & EFLAGS_WRITE_OF) == 0) {
                status = EFLAGS_READ_OF; /* reads OF before writing! */
                LOG(THREAD_GET, LOG_INTERP, 4, "\t  reads OF prior to writing it!\n");
            }
#endif
        }
    } else if ((instr_eflags & EFLAGS_WRITE_ARITH) != 0) {
        /* store the flags we're writing */
        e6 |= (instr_eflags & EFLAGS_WRITE_ARITH);
        *eflags_6 = e6;
        /* check if all written but none read yet */
        if ((e6 & EFLAGS_WRITE_ARITH) == EFLAGS_WRITE_ARITH &&
            (e6 & EFLAGS_READ_ARITH) == 0) {
            status = EFLAGS_WRITE_ARITH; /* all written before read */
            LOG(THREAD_GET, LOG_INTERP, 4, "\twrote all 6 flags now!\n");
        }
#ifdef X86
        /* check if at least OF was written but not read */
        else if ((e6 & EFLAGS_WRITE_OF) != 0 && (e6 & EFLAGS_READ_OF) == 0) {
            status = EFLAGS_WRITE_OF; /* OF written before read */
            LOG(THREAD_GET, LOG_INTERP, 4, "\twrote overflow flag before reading it!\n");
        }
#endif
    }
    return status;
}

/* check origins of code for several purposes:
 * 1) we need list of areas where this thread's fragments come
 *    from, for faster flushing on munmaps
 * 2) also for faster flushing, each vmarea has a list of fragments
 * 3) we need to mark as read-only any writable region that
 *    has a fragment come from it, to handle self-modifying code
 * 4) for PROGRAM_SHEPHERDING restricted code origins for security
 * 5) for restricted execution environments: not letting bb cross regions
 */

/*
  FIXME CASE 7380:
  since report security violation before execute off bad page, can be
  false positive due to:
  - a faulting instruction in middle of bb would have prevented
    getting there
  - ignorable syscall in middle
  - self-mod code would have ended bb sooner than bad page

  One solution is to have check_thread_vm_area() return false and have
  bb building stop at checked_end if a violation will occur when we
  get there.  Then we only raise the violation once building a bb
  starting there.
 */

static inline void
check_new_page_start(dcontext_t *dcontext, build_bb_t *bb)
{
    DEBUG_DECLARE(bool ok;)
    if (!bb->check_vm_area)
        return;
    DEBUG_DECLARE(ok =)
    check_thread_vm_area(dcontext, bb->start_pc, bb->start_pc,
                         (bb->record_vmlist ? &bb->vmlist : NULL), &bb->flags,
                         &bb->checked_end, false /*!xfer*/);
    ASSERT(ok); /* cannot return false on non-xfer */
    bb->last_page = bb->start_pc;
    if (bb->overlap_info != NULL)
        reset_overlap_info(dcontext, bb);
}

/* Walk forward in straight line from prev_pc to new_pc.
 * FIXME: with checked_end we don't need to call this on every contig end
 * while bb building like we used to.  Should revisit the overlap info and
 * walk_app_bb reasons for keeping those contig() calls and see if we can
 * optimize them away for bb building at least.
 * i#993: new_pc points to the last byte of the current instruction and is not
 * an open-ended endpoint.
 */
static inline bool
check_new_page_contig(dcontext_t *dcontext, build_bb_t *bb, app_pc new_pc)
{
    bool is_first_instr = (bb->instr_start == bb->start_pc);
    if (!bb->check_vm_area)
        return true;
    if (bb->checked_end == NULL) {
        ASSERT(new_pc == bb->start_pc);
    } else if (new_pc >= bb->checked_end) {
        if (!check_thread_vm_area(dcontext, new_pc, bb->start_pc,
                                  (bb->record_vmlist ? &bb->vmlist : NULL), &bb->flags,
                                  &bb->checked_end,
                                  /* i#989: We don't want to fall through to an
                                   * incompatible vmarea, so we treat fall
                                   * through like a transfer.  We can't end the
                                   * bb before the first instruction, so we pass
                                   * false to forcibly merge in the vmarea
                                   * flags.
                                   */
                                  !is_first_instr /*xfer*/)) {
            return false;
        }
    }
    if (bb->overlap_info != NULL)
        update_overlap_info(dcontext, bb, new_pc, false /*not jmp*/);
    DOLOG(4, LOG_INTERP, {
        if (PAGE_START(bb->last_page) != PAGE_START(new_pc))
            LOG(THREAD, LOG_INTERP, 4, "page boundary crossed\n");
    });
    bb->last_page = new_pc; /* update even if not new page, for walk_app_bb */
    return true;
}

/* Direct cti from prev_pc to new_pc */
static bool
check_new_page_jmp(dcontext_t *dcontext, build_bb_t *bb, app_pc new_pc)
{
    /* For tracking purposes, check the last byte of the cti. */
    bool ok = check_new_page_contig(dcontext, bb, bb->cur_pc - 1);
    ASSERT(ok && "should have checked cur_pc-1 in decode loop");
    if (!ok) /* Don't follow the jmp in release build. */
        return false;
    /* cur sandboxing doesn't handle direct cti
     * not good enough to only check this at top of interp -- could walk contig
     * from non-selfmod to selfmod page, and then do a direct cti, which
     * check_thread_vm_area would allow (no flag changes on direct cti)!
     * also not good enough to put this check in check_thread_vm_area, as that
     * only checks across pages.
     */
    if ((bb->flags & FRAG_SELFMOD_SANDBOXED) != 0)
        return false;
    if (PAGE_START(bb->last_page) != PAGE_START(new_pc))
        LOG(THREAD, LOG_INTERP, 4, "page boundary crossed\n");
    /* do not walk into a native exec dll (we assume not currently there,
     * though could happen if bypass a gateway -- even then this is a feature
     * to allow getting back to native ASAP)
     * FIXME: we could assume that such direct calls only
     * occur from DGC, and rely on check_thread_vm_area to disallow,
     * as an (unsafe) optimization
     */
    if (DYNAMO_OPTION(native_exec) && DYNAMO_OPTION(native_exec_dircalls) &&
        !vmvector_empty(native_exec_areas) && is_native_pc(new_pc))
        return false;
    /* i#805: If we're crossing a module boundary between two modules that are
     * and aren't on null_instrument_list, don't elide the jmp.
     * XXX i#884: if we haven't yet executed from the 2nd module, the client
     * won't receive the module load event yet and we might include code
     * from it here.  It would be tricky to solve that, and it should only happen
     * if the client turns on elision, so we leave it.
     */
    if ((!!os_module_get_flag(bb->cur_pc, MODULE_NULL_INSTRUMENT)) !=
        (!!os_module_get_flag(new_pc, MODULE_NULL_INSTRUMENT)))
        return false;
    if (!bb->check_vm_area)
        return true;
    /* need to check this even if an intra-page jmp b/c we allow sub-page vm regions */
    if (!check_thread_vm_area(dcontext, new_pc, bb->start_pc,
                              (bb->record_vmlist ? &bb->vmlist : NULL), &bb->flags,
                              &bb->checked_end, true /*xfer*/))
        return false;
    if (bb->overlap_info != NULL)
        update_overlap_info(dcontext, bb, new_pc, true /*jmp*/);
    bb->flags |= FRAG_HAS_DIRECT_CTI;
    bb->last_page = new_pc; /* update even if not new page, for walk_app_bb */
    return true;
}

static inline void
bb_process_single_step(dcontext_t *dcontext, build_bb_t *bb)
{
    LOG(THREAD, LOG_INTERP, 2, "interp: single step exception bb at " PFX "\n",
        bb->instr_start);
    /* FIXME i#2144 : handling a rep string operation.
     * In this case, we should test if only one iteration is done
     * before the single step exception.
     */
    instrlist_append(bb->ilist, bb->instr);
    instr_set_translation(bb->instr, bb->instr_start);

    /* Mark instruction as special exit. */
    instr_branch_set_special_exit(bb->instr, true);
    bb->exit_type |= LINK_SPECIAL_EXIT;

    /* Make this bb thread-private and a trace barrier. */
    bb->flags &= ~FRAG_SHARED;
    bb->flags |= FRAG_CANNOT_BE_TRACE;
}

static inline void
bb_process_invalid_instr(dcontext_t *dcontext, build_bb_t *bb)
{
    /* invalid instr: end bb BEFORE the instr, we'll throw exception if we
     * reach the instr itself
     */
    LOG(THREAD, LOG_INTERP, 2, "interp: invalid instr at " PFX "\n", bb->instr_start);
    /* This routine is called by more than just bb builder, also used
     * for recreating state, so check bb->app_interp parameter to find out
     * if building a real app bb to be executed
     */
    if (bb->app_interp && bb->instr_start == bb->start_pc) {
        /* This is first instr in bb so it will be executed for sure and
         * we need to generate an invalid instruction exception.
         * A benefit of being first instr is that the state is easy
         * to translate.
         */
        /* Copying the invalid bytes and having the processor generate the exception
         * would help on Windows where the kernel splits invalid instructions into
         * different cases (an invalid lock prefix and other distinctions, when the
         * underlying processor has a single interrupt 6), and it is hard to
         * duplicate Windows' behavior in our forged exception.  However, we are not
         * certain that this instruction will raise a fault on the processor.  It
         * might not if our decoder has a bug, or a new processor has added new
         * opcodes, or just due to processor variations in undefined gray areas.
         * Trying to copy without knowing the length of the instruction is a recipe
         * for disaster: it can lead to executing junk and even missing our exit cti
         * (i#3939).
         */
        /* TODO i#1000: Give clients a chance to see this instruction for analysis,
         * and to change it.  That's not easy to do though when we don't know what
         * it is.  But it's confusing for the client to get the illegal instr fault
         * having never seen the problematic instr in a bb event.
         */
        /* XXX i#57: provide a runtime option to specify new instruction formats to
         * avoid this app exception for new opcodes.
         */
        ASSERT(dcontext->bb_build_info == bb);
        bb_build_abort(dcontext, true /*clean vm area*/, true /*unlock*/);
        /* XXX: we use illegal instruction here, even though we
         * know windows uses different exception codes for different
         * types of invalid instructions (for ex. STATUS_INVALID_LOCK
         * _SEQUENCE for lock prefix on a jmp instruction).
         */
        if (TEST(DUMPCORE_FORGE_ILLEGAL_INST, DYNAMO_OPTION(dumpcore_mask)))
            os_dump_core("Warning: Encountered Illegal Instruction");
        os_forge_exception(bb->instr_start, ILLEGAL_INSTRUCTION_EXCEPTION);
        ASSERT_NOT_REACHED();
    } else {
        instr_destroy(dcontext, bb->instr);
        bb->instr = NULL;
    }
}

/* FIXME i#1668, i#2974: NYI on ARM/AArch64 */
#ifdef X86
/* returns true to indicate "elide and continue" and false to indicate "end bb now"
 * should be used both for converted indirect jumps and
 * FIXME: for direct jumps by bb_process_ubr
 */
static inline bool
follow_direct_jump(dcontext_t *dcontext, build_bb_t *bb, app_pc target)
{
    if (bb->follow_direct && !must_not_be_entered(target) &&
        bb->num_elide_jmp < DYNAMO_OPTION(max_elide_jmp) &&
        (DYNAMO_OPTION(elide_back_jmps) || bb->cur_pc <= target)) {
        if (check_new_page_jmp(dcontext, bb, target)) {
            /* Elide unconditional branch and follow target */
            bb->num_elide_jmp++;
            STATS_INC(total_elided_jmps);
            STATS_TRACK_MAX(max_elided_jmps, bb->num_elide_jmp);
            bb->cur_pc = target;
            BBPRINT(bb, 4, "        continuing at target " PFX "\n", bb->cur_pc);

            return true; /* keep bb going */
        } else {
            BBPRINT(bb, 3, "        NOT following jmp from " PFX " to " PFX "\n",
                    bb->instr_start, target);
        }
    } else {
        BBPRINT(bb, 3, "   NOT attempting to follow jump from " PFX " to " PFX "\n",
                bb->instr_start, target);
    }
    return false; /* stop bb */
}
#endif /* X86 */

/* returns true to indicate "elide and continue" and false to indicate "end bb now" */
static inline bool
bb_process_ubr(dcontext_t *dcontext, build_bb_t *bb)
{
    app_pc tgt = (byte *)opnd_get_pc(instr_get_target(bb->instr));
    BBPRINT(bb, 4, "interp: direct jump at " PFX "\n", bb->instr_start);
    if (must_not_be_elided(tgt)) {
#ifdef WINDOWS
        byte *wrapper_start;
        if (is_syscall_trampoline(tgt, &wrapper_start)) {
            /* HACK to avoid entering the syscall trampoline that is meant
             * only for native syscalls -- we replace the jmp with the
             * original app mov immed that it replaced
             */
            BBPRINT(bb, 3,
                    "interp: replacing syscall trampoline @" PFX " w/ orig mov @" PFX
                    "\n",
                    bb->instr_start, wrapper_start);
            instr_reset(dcontext, bb->instr);

            /* leave bb->cur_pc unchanged */
            decode(dcontext, wrapper_start, bb->instr);
            /* ASSUMPTION: syscall trampoline puts hooked instruction
             * (usually mov_imm but can be lea if hooked_deeper) here */
            ASSERT(instr_get_opcode(bb->instr) == OP_mov_imm ||
                   (instr_get_opcode(bb->instr) == OP_lea &&
                    DYNAMO_OPTION(native_exec_hook_conflict) ==
                        HOOKED_TRAMPOLINE_HOOK_DEEPER));
            instrlist_append(bb->ilist, bb->instr);
            /* translation should point to the trampoline at the
             * original application address
             */
            if (bb->record_translation)
                instr_set_translation(bb->instr, bb->instr_start);
            if (instr_get_opcode(bb->instr) == OP_lea) {
                app_pc translation = bb->instr_start + instr_length(dcontext, bb->instr);
                ASSERT_CURIOSITY(instr_length(dcontext, bb->instr) == 4);
                /* we hooked deep need to add the int 2e instruction */
                /* can't use create_syscall_instr because of case 5217 hack */
                ASSERT(get_syscall_method() == SYSCALL_METHOD_INT);
                bb->instr = INSTR_CREATE_int(dcontext,
                                             opnd_create_immed_int((sbyte)0x2e, OPSZ_1));
                if (bb->record_translation)
                    instr_set_translation(bb->instr, translation);
                ASSERT(instr_is_syscall(bb->instr) &&
                       instr_get_opcode(bb->instr) == OP_int);
                instrlist_append(bb->ilist, bb->instr);
                return bb_process_syscall(dcontext, bb);
            }
            return true; /* keep bb going */
        }
#endif
        BBPRINT(bb, 3, "interp: NOT following jmp to " PFX "\n", tgt);
        /* add instruction to instruction list */
        bb_add_native_direct_xfer(dcontext, bb, false /*!appended*/);
        /* Case 8711: coarse-grain can't handle non-exit cti */
        bb->flags &= ~FRAG_COARSE_GRAIN;
        STATS_INC(coarse_prevent_cti);
        return false; /* end bb now */
    } else {
        if (bb->follow_direct && !must_not_be_entered(tgt) &&
            bb->num_elide_jmp < DYNAMO_OPTION(max_elide_jmp) &&
            (DYNAMO_OPTION(elide_back_jmps) || bb->cur_pc <= tgt)) {
            if (check_new_page_jmp(dcontext, bb, tgt)) {
                /* Elide unconditional branch and follow target */
                bb->num_elide_jmp++;
                STATS_INC(total_elided_jmps);
                STATS_TRACK_MAX(max_elided_jmps, bb->num_elide_jmp);
                bb->cur_pc = tgt;
                BBPRINT(bb, 4, "        continuing at target " PFX "\n", bb->cur_pc);
                /* pretend never saw this ubr: delete instr, then continue */
                instr_destroy(dcontext, bb->instr);
                bb->instr = NULL;
                return true; /* keep bb going */
            } else {
                BBPRINT(bb, 3,
                        "        NOT following direct jmp from " PFX " to " PFX "\n",
                        bb->instr_start, tgt);
            }
        }
        /* End this bb now */
        bb->exit_target = opnd_get_pc(instr_get_target(bb->instr));
        instrlist_append(bb->ilist, bb->instr);
        return false; /* end bb */
    }
    return true; /* keep bb going */
}

#ifdef X86
/* returns true if call is elided,
 * and false if not following due to hitting a limit or other reason */
static bool
follow_direct_call(dcontext_t *dcontext, build_bb_t *bb, app_pc callee)
{
    /* FIXME: This code should be reused in bb_process_convertible_indcall()
     * and in bb_process_call_direct()
     */
    if (bb->follow_direct && !must_not_be_entered(callee) &&
        bb->num_elide_call < DYNAMO_OPTION(max_elide_call) &&
        (DYNAMO_OPTION(elide_back_calls) || bb->cur_pc <= callee)) {
        if (check_new_page_jmp(dcontext, bb, callee)) {
            bb->num_elide_call++;
            STATS_INC(total_elided_calls);
            STATS_TRACK_MAX(max_elided_calls, bb->num_elide_call);
            bb->cur_pc = callee;

            BBPRINT(bb, 4, "   continuing in callee at " PFX "\n", bb->cur_pc);
            return true; /* keep bb going in callee */
        } else {
            BBPRINT(bb, 3,
                    "   NOT following direct (or converted) call from " PFX " to " PFX
                    "\n",
                    bb->instr_start, callee);
        }
    } else {
        BBPRINT(bb, 3, "   NOT attempting to follow call from " PFX " to " PFX "\n",
                bb->instr_start, callee);
    }
    return false; /* stop bb */
}
#endif /* X86 */

static inline void
bb_stop_prior_to_instr(dcontext_t *dcontext, build_bb_t *bb, bool appended)
{
    if (appended)
        instrlist_remove(bb->ilist, bb->instr);
    instr_destroy(dcontext, bb->instr);
    bb->instr = NULL;
    bb->cur_pc = bb->instr_start;
}

/* returns true to indicate "elide and continue" and false to indicate "end bb now" */
static inline bool
bb_process_call_direct(dcontext_t *dcontext, build_bb_t *bb)
{
    byte *callee = (byte *)opnd_get_pc(instr_get_target(bb->instr));
#ifdef CUSTOM_TRACES_RET_REMOVAL
    if (callee == bb->instr_start + 5) {
        LOG(THREAD, LOG_INTERP, 4, "found call to next instruction\n");
    } else
        dcontext->num_calls++;
#endif
    STATS_INC(num_all_calls);
    BBPRINT(bb, 4, "interp: direct call at " PFX "\n", bb->instr_start);
    if (leave_call_native(callee)) {
        BBPRINT(bb, 3, "interp: NOT inlining or mangling call to " PFX "\n", callee);
        /* Case 8711: coarse-grain can't handle non-exit cti.
         * If we allow this fragment to be coarse we must kill the freeze
         * nudge thread!
         */
        bb->flags &= ~FRAG_COARSE_GRAIN;
        STATS_INC(coarse_prevent_cti);
        bb_add_native_direct_xfer(dcontext, bb, true /*appended*/);
        return true; /* keep bb going, w/o inlining call */
    } else {
        if (DYNAMO_OPTION(coarse_split_calls) && DYNAMO_OPTION(coarse_units) &&
            TEST(FRAG_COARSE_GRAIN, bb->flags)) {
            if (instrlist_first(bb->ilist) != bb->instr) {
                /* have call be in its own bb */
                bb_stop_prior_to_instr(dcontext, bb, true /*appended already*/);
                return false; /* stop bb */
            } else {
                /* single-call fine-grained bb */
                bb->flags &= ~FRAG_COARSE_GRAIN;
                STATS_INC(coarse_prevent_cti);
            }
        }
        /* FIXME: use follow_direct_call() */
        if (bb->follow_direct && !must_not_be_entered(callee) &&
            bb->num_elide_call < DYNAMO_OPTION(max_elide_call) &&
            (DYNAMO_OPTION(elide_back_calls) || bb->cur_pc <= callee)) {
            if (check_new_page_jmp(dcontext, bb, callee)) {
                bb->num_elide_call++;
                STATS_INC(total_elided_calls);
                STATS_TRACK_MAX(max_elided_calls, bb->num_elide_call);
                bb->cur_pc = callee;
                BBPRINT(bb, 4, "      continuing in callee at " PFX "\n", bb->cur_pc);
                return true; /* keep bb going */
            }
        }
        BBPRINT(bb, 3, "        NOT following direct call from " PFX " to " PFX "\n",
                bb->instr_start, callee);
        /* End this bb now */
        if (instr_is_cbr(bb->instr)) {
            /* Treat as cbr, not call */
            instr_exit_branch_set_type(bb->instr, instr_branch_type(bb->instr));
        } else {
            bb->exit_target = callee;
        }
        return false; /* end bb now */
    }
    return true; /* keep bb going */
}

#ifdef WINDOWS

/* We check if the instrs call, mov, and sysenter are
 * "call (%xdx); mov %xsp -> %xdx" or "call %xdx; mov %xsp -> %xdx"
 * and "sysenter".
 */
bool
instr_is_call_sysenter_pattern(instr_t *call, instr_t *mov, instr_t *sysenter)
{
    instr_t *instr;
    if (call == NULL || mov == NULL || sysenter == NULL)
        return false;
    if (instr_is_meta(call) || instr_is_meta(mov) || instr_is_meta(sysenter))
        return false;
    if (instr_get_next(call) != mov || instr_get_next(mov) != sysenter)
        return false;
    /* check sysenter */
    if (instr_get_opcode(sysenter) != OP_sysenter)
        return false;

    /* FIXME Relax the pattern matching on the "mov; call" pair so that small
     * changes in the register dataflow and call construct are tolerated. */

    /* Did we find a "mov %xsp -> %xdx"? */
    instr = mov;
    if (!(instr != NULL && instr_get_opcode(instr) == OP_mov_ld &&
          instr_num_srcs(instr) == 1 && instr_num_dsts(instr) == 1 &&
          opnd_is_reg(instr_get_dst(instr, 0)) &&
          opnd_get_reg(instr_get_dst(instr, 0)) == REG_XDX &&
          opnd_is_reg(instr_get_src(instr, 0)) &&
          opnd_get_reg(instr_get_src(instr, 0)) == REG_XSP)) {
        return false;
    }

    /* Did we find a "call (%xdx) or "call %xdx" that's already marked
     * for ind->direct call conversion? */
    instr = call;
    if (!(instr != NULL && TEST(INSTR_IND_CALL_DIRECT, instr->flags) &&
          instr_is_call_indirect(instr) &&
          /* The 2nd src operand should always be %xsp. */
          opnd_is_reg(instr_get_src(instr, 1)) &&
          opnd_get_reg(instr_get_src(instr, 1)) == REG_XSP &&
          /* Match 'call (%xdx)' for post-SP2. */
          ((opnd_is_near_base_disp(instr_get_src(instr, 0)) &&
            opnd_get_base(instr_get_src(instr, 0)) == REG_XDX &&
            opnd_get_disp(instr_get_src(instr, 0)) == 0) ||
           /* Match 'call %xdx' for pre-SP2. */
           (opnd_is_reg(instr_get_src(instr, 0)) &&
            opnd_get_reg(instr_get_src(instr, 0)) == REG_XDX)))) {
        return false;
    }

    return true;
}

/* Walk up from the bb->instr and verify that the preceding instructions
 * match the pattern that we expect to precede a sysenter. */
static instr_t *
bb_verify_sysenter_pattern(dcontext_t *dcontext, build_bb_t *bb)
{
    /* Walk back up 2 instructions and verify that there's a
     * "call (%xdx); mov %xsp -> %xdx" or "call %xdx; mov %xsp -> %xdx"
     * just prior to the sysenter.
     * We use "xsp" and "xdx" to be ready for x64 sysenter though we don't
     * expect to see it.
     */
    instr_t *mov, *call;
    mov = instr_get_prev_expanded(dcontext, bb->ilist, bb->instr);
    if (mov == NULL)
        return NULL;
    call = instr_get_prev_expanded(dcontext, bb->ilist, mov);
    if (call == NULL)
        return NULL;
    if (!instr_is_call_sysenter_pattern(call, mov, bb->instr)) {
        BBPRINT(bb, 3, "bb_verify_sysenter_pattern -- pattern didn't match\n");
        return NULL;
    }
    return call;
}

/* Only used for the Borland SEH exemption. */
/* FIXME - we can't really tell a push from a pop since both are typically a
 * mov to fs:[0], but double processing doesn't hurt. */
/* NOTE we don't see dynamic SEH frame pushes, we only see the first SEH push
 * per mov -> fs:[0] instruction in the app.  So we don't see modified in place
 * handler addresses (see at_Borland_SEH_rct_exemption()) or handler addresses
 * that are passed into a shared routine that sets up the frame (not yet seen,
 * note that MS dlls that have a _SEH_prolog hardcode the handler address in
 * the _SEH_prolog routine, only the data is passed in).
 */
static void
bb_process_SEH_push(dcontext_t *dcontext, build_bb_t *bb, void *value)
{
    if (value == NULL || value == (void *)PTR_UINT_MINUS_1) {
        /* could be popping off the last frame (leaving -1) of the SEH stack */
        STATS_INC(num_endlist_SEH_write);
        ASSERT_CURIOSITY(value != NULL);
        return;
    }
    LOG(THREAD, LOG_INTERP, 3, "App moving " PFX " to fs:[0]\n", value);
#    ifdef RETURN_AFTER_CALL
    if (DYNAMO_OPTION(borland_SEH_rct)) {
        /* xref case 5752, the Borland compiler SEH implementation uses a push
         * imm ret motif for fall through to the finally of a try finally block
         * (very similar to what the Microsoft NT at_SEH_rct_exception() is
         * doing). The layout will always look like this :
         *     push e:  (imm32)  (e should be in the .E/.F table)
         *  a:
         *     ...
         *  b: ret
         *  c: jmp rel32         (c should be in the .E/.F table)
         *  d: jmp a:  (rel8/32)
         *     ... (usually nothing)
         *  e:
         * (where ret at b is targeting e, or a valid after call).  The
         * exception dispatcher calls c (the SEH frame has c as the handler)
         * which jmps to the exception handler which, in turn, calls d to
         * execute the finally block.  Fall through is as shown above. So,
         * we see a .E violation for the handlers call to d and a .C violation
         * for the fall trough case of the ret @ b targeting e.  We may also
         * see a .E violation for a call to a as sometimes the handler computes
         * the target of the jmp @ d an passes that to a different exception
         * handler.
         *
         * For try-except we see the following layout :
         *           I've only seen jmp ind in the case that led to needing
         *           at_Borland_SEH_rct_exemption() to be added, not that
         *           it makes any difference.
         *    [ jmp z:  (rel8/32) || (rarely) ret || (very rarely) jmp ind]
         * x: jmp rel32          (x should be in the .E/.F table)
         * y:
         *    ...
         *    call rel32
         * [z: ... || ret ]
         * Though there may be other optimized layouts (the ret instead of the
         * jmp z: is one such) so we may not want to rely on anything other
         * then x y.  The exception dispatcher calls x (the SEH frame has x as
         * the handler) which jmps to the exception handler which, in turn,
         * jmps to y to execute the except block.  We see a .F violation from
         * the handler's jmp to y.  at_Borland_SEH_rct_exemption() covers a
         * case where the address of x (and thus y) in an existing SEH frame
         * is changed in place instead of popping and pushing a new frame.
         *
         * All addresses (rel and otherwise) should be in the same module.  So
         * we need to recognize the patter and add d:/y: to the .E/.F table
         * as well as a: (sometimes the handler calculates the target of d and
         * passes that up to a higher level routine, though I don't see the
         * point) and add e: to the .C table.
         *
         * It would be preferable to handle these exemptions reactively at
         * the violation point, but unfortunately, by the time we get to the
         * violation the SEH frame information has been popped off the stack
         * and is lost, so we have to do it pre-emptively here (pattern
         * matching at violation time has proven to difficult in the face of
         * certain compiler optimizations). See at_Borland_SEH_rct_exemption()
         * in callback.c, that could handle all ind branches to y and ind calls
         * to d (see below) at an acceptable level of security if we desired.
         * Handling the ret @ b to e reactively would require the ability to
         * recreate the exact src cti (so we can use the addr of the ret to
         * pattern match) at the violation point (something that can't always
         * currently be done, reset flushing etc.).  Handling the ind call to
         * a (which I've never acutally seen, though I've seen the address
         * computed and it looks like it could likely be hit) reactively is
         * more tricky. Prob. the only way to handle that is to allow .E/.F
         * transistions to any address after a push imm32 of an address in the
         * same module, but that might be too permissive.  FIXME - should still
         * revisit doing the exemptions reactively at some point, esp. once we
         * can reliably get the src cti.
         */

        extern bool seen_Borland_SEH; /* set for callback.c */
        /* First read in the SEH frame, this is the observed structure and
         * the first two fields (which are all that we use) are constrained by
         * ntdll exception dispatcher (see EXCEPTION_REGISTRATION decleration
         * in ntdll.h). */
        /* FIXME - could just use EXCEPTION_REGISTRATION period since all we
         * need is the handler address and it would allow simpler curiosity
         * [see 8181] below.  If, as is expected, other options make use of
         * this routine we'll probably have one shared get of the SEH frame
         * anyways. */
        typedef struct _borland_seh_frame_t {
            EXCEPTION_REGISTRATION reg;
            reg_t xbp; /* not used by us */
        } borland_seh_frame_t;
        borland_seh_frame_t frame;
        /* will hold [b,e] or [x-1,y] */
        byte target_buf[RET_0_LENGTH + 2 * JMP_LONG_LENGTH];
        app_pc handler_jmp_target = NULL;

        if (!d_r_safe_read(value, sizeof(frame), &frame)) {
            /* We already checked for NULL and -1 above so this should be
             * a valid SEH frame. Xref 8181, borland_seh_frame_t struct is
             * bigger then EXCEPTION_REGISTRATION (which is all that is
             * required) so verify smaller size is readable. */
            ASSERT_CURIOSITY(
                sizeof(EXCEPTION_REGISTRATION) < sizeof(frame) &&
                d_r_safe_read(value, sizeof(EXCEPTION_REGISTRATION), &frame));
            goto post_borland;
        }
        /* frame.reg.handler is c or y, read extra prior bytes to look for b */
        if (!d_r_safe_read((app_pc)frame.reg.handler - RET_0_LENGTH, sizeof(target_buf),
                           target_buf)) {
            goto post_borland;
        }
        if (is_jmp_rel32(&target_buf[RET_0_LENGTH], (app_pc)frame.reg.handler,
                         &handler_jmp_target)) {
            /* we have a possible match, now do the more expensive checking */
            app_pc base;
            LOG(THREAD, LOG_INTERP, 3,
                "Read possible borland SEH frame @" PFX "\n\t"
                "next=" PFX " handler=" PFX " xbp=" PFX "\n\t",
                value, frame.reg.prev, frame.reg.handler, frame.xbp);
            DOLOG(3, LOG_INTERP,
                  { dump_buffer_as_bytes(THREAD, target_buf, sizeof(target_buf), 0); });
            /* optimize check if we've already processed this frame once */
            if ((DYNAMO_OPTION(rct_ind_jump) != OPTION_DISABLED ||
                 DYNAMO_OPTION(rct_ind_call) != OPTION_DISABLED) &&
                rct_ind_branch_target_lookup(
                    dcontext, (app_pc)frame.reg.handler + JMP_LONG_LENGTH)) {
                /* we already processed this SEH frame once, this is prob. a
                 * frame pop, no need to continue */
                STATS_INC(num_borland_SEH_dup_frame);
                LOG(THREAD, LOG_INTERP, 3, "Processing duplicate Borland SEH frame\n");
                goto post_borland;
            }
            base = get_module_base((app_pc)frame.reg.handler);
            STATS_INC(num_borland_SEH_initial_match);
            /* Perf opt, we use the cheaper get_allocation_base() below instead
             * of get_module_base().  We are checking the result against a
             * known module base (base) so no need to duplicate the is module
             * check.  FIXME - the checks prob. aren't even necessary given the
             * later is_in_code_section checks. Xref case 8171. */
            /* FIXME - (perf) we could cache the region from the first
             * is_in_code_section() call and check against that before falling
             * back on is_in_code_section in case of multiple code sections. */
            if (base != NULL && get_allocation_base(handler_jmp_target) == base &&
                get_allocation_base(bb->instr_start) == base &&
                /* FIXME - with -rct_analyze_at_load we should be able to
                 * verify that frame->handler (x: c:) is on the .E/.F
                 * table already. We could also try to match known pre x:
                 * post y: patterns. */
                is_in_code_section(base, bb->instr_start, NULL, NULL) &&
                is_in_code_section(base, handler_jmp_target, NULL, NULL) &&
                is_range_in_code_section(base, (app_pc)frame.reg.handler,
                                         (app_pc)frame.reg.handler + JMP_LONG_LENGTH + 1,
                                         NULL, NULL)) {
                app_pc finally_target;
                byte push_imm_buf[PUSH_IMM32_LENGTH];
                DEBUG_DECLARE(bool ok;)
                /* we have a match, add handler+JMP_LONG_LENGTH (y: d:)
                 * to .E/.F table */
                STATS_INC(num_borland_SEH_try_match);
                LOG(THREAD, LOG_INTERP, 2,
                    "Found Borland SEH frame adding " PFX " to .E/.F table\n",
                    (app_pc)frame.reg.handler + JMP_LONG_LENGTH);
                if ((DYNAMO_OPTION(rct_ind_jump) != OPTION_DISABLED ||
                     DYNAMO_OPTION(rct_ind_call) != OPTION_DISABLED)) {
                    d_r_mutex_lock(&rct_module_lock);
                    rct_add_valid_ind_branch_target(
                        dcontext, (app_pc)frame.reg.handler + JMP_LONG_LENGTH);
                    d_r_mutex_unlock(&rct_module_lock);
                }
                /* we set this as an enabler for another exemption in
                 * callback .C, see notes there */
                if (!seen_Borland_SEH) {
                    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
                    seen_Borland_SEH = true;
                    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
                }
                /* case 8648: used to decide which RCT entries to persist */
                DEBUG_DECLARE(ok =) os_module_set_flag(base, MODULE_HAS_BORLAND_SEH);
                ASSERT(ok);
                /* look for .C addresses for try finally */
                if (target_buf[0] == RAW_OPCODE_ret &&
                    (is_jmp_rel32(&target_buf[RET_0_LENGTH + JMP_LONG_LENGTH],
                                  (app_pc)frame.reg.handler + JMP_LONG_LENGTH,
                                  &finally_target) ||
                     is_jmp_rel8(&target_buf[RET_0_LENGTH + JMP_LONG_LENGTH],
                                 (app_pc)frame.reg.handler + JMP_LONG_LENGTH,
                                 &finally_target)) &&
                    d_r_safe_read(finally_target - sizeof(push_imm_buf),
                                  sizeof(push_imm_buf), push_imm_buf) &&
                    push_imm_buf[0] == RAW_OPCODE_push_imm32) {
                    app_pc push_val = *(app_pc *)&push_imm_buf[1];
                    /* do a few more, expensive, sanity checks */
                    /* FIXME - (perf) see earlier note on get_allocation_base()
                     * and is_in_code_section() usage. */
                    if (get_allocation_base(finally_target) == base &&
                        is_in_code_section(base, finally_target, NULL, NULL) &&
                        get_allocation_base(push_val) == base &&
                        /* FIXME - could also check that push_val is in
                         * .E/.F table, at least for -rct_analyze_at_load */
                        is_in_code_section(base, push_val, NULL, NULL)) {
                        /* Full match, add push_val (e:) to the .C table
                         * and finally_target (a:) to the .E/.F table */
                        STATS_INC(num_borland_SEH_finally_match);
                        LOG(THREAD, LOG_INTERP, 2,
                            "Found Borland SEH finally frame adding " PFX " to"
                            " .C table and " PFX " to .E/.F table\n",
                            push_val, finally_target);
                        if ((DYNAMO_OPTION(rct_ind_jump) != OPTION_DISABLED ||
                             DYNAMO_OPTION(rct_ind_call) != OPTION_DISABLED)) {
                            d_r_mutex_lock(&rct_module_lock);
                            rct_add_valid_ind_branch_target(dcontext, finally_target);
                            d_r_mutex_unlock(&rct_module_lock);
                        }
                        if (DYNAMO_OPTION(ret_after_call)) {
                            fragment_add_after_call(dcontext, push_val);
                        }
                    } else {
                        ASSERT_CURIOSITY(false && "partial borland seh finally match");
                    }
                }
            }
        }
    }
post_borland:
#    endif /* RETURN_AFTER_CALL */
    return;
}

/* helper routine for bb_process_fs_ref
 * return true if bb should be continued, false if it shouldn't  */
static bool
bb_process_fs_ref_opnd(dcontext_t *dcontext, build_bb_t *bb, opnd_t dst, bool *is_to_fs0)
{
    ASSERT(is_to_fs0 != NULL);
    *is_to_fs0 = false;
    if (opnd_is_far_base_disp(dst) && /* FIXME - check size? */
        opnd_get_segment(dst) == SEG_FS) {
        /* is a write to fs:[*] */
        if (bb->instr_start != bb->start_pc) {
            /* Not first instruction in the bb, end bb before this
             * instruction, so we can see it as the first instruction of a
             * new bb where we can use the register state. */
            /* As is, always ending the bb here has a mixed effect on mem usage
             * with default options.  We do end up with slightly more bb's
             * (and associated bookeeping costs), but frequently with MS dlls
             * we reduce code cache dupliaction from jmp/call ellision
             * (_SEH_[Pro,Epi]log otherwise ends up frequently duplicated for
             * instance). */
            /* FIXME - we must stop the bb here even if there's already
             * a bb built for the next instruction, as we have to have
             * reproducible bb building for recreate app state.  We should
             * only get here through code duplication (typically jmp/call
             * inlining, though can also be through multiple entry points into
             * the same block of non cti instructions). */
            bb_stop_prior_to_instr(dcontext, bb, false /*not appended yet*/);
            return false; /* stop bb */
        }
        /* Only process the push if building a new bb for cache, can't check
         * this any earlier since have to preserve bb building/ending behavior
         * even when not for cache (for recreation etc.). */
        if (bb->app_interp) {
            /* check is write to fs:[0] */
            /* XXX: this won't identify all memory references (need to switch to
             * instr_compute_address_ex_priv() in order to handle VSIB) but the
             * current usage is just to identify the Borland pattern so that's ok.
             */
            if (opnd_compute_address_priv(dst, get_mcontext(dcontext)) == NULL) {
                /* we have new mov to fs:[0] */
                *is_to_fs0 = true;
            }
        }
    }
    return true;
}

/* While currently only used for Borland SEH exemptions, this analysis could
 * also be helpful for other SEH tasks (xref case 5824). */
static bool
bb_process_fs_ref(dcontext_t *dcontext, build_bb_t *bb)
{
    ASSERT(DYNAMO_OPTION(process_SEH_push) &&
           instr_get_prefix_flag(bb->instr, PREFIX_SEG_FS));

    /* If this is the first instruction of a bb for the cache we
     * want to fully decode it, check if it's pushing an SEH frame
     * and, if so, pass it to the SEH checking routines (currently
     * just used for the Borland SEH rct handling).  If this is not
     * the first instruction of the bb then we want to stop the bb
     * just before this instruction so that when we do process this
     * instruction it will be the first in the bb (allowing us to
     * use the register state). */
    if (!bb->full_decode) {
        instr_decode(dcontext, bb->instr);
        /* is possible this is an invalid instr that made it through the fast
         * decode, FIXME is there a better way to handle this? */
        if (!instr_valid(bb->instr)) {
            ASSERT_NOT_TESTED();
            if (bb->cur_pc == NULL)
                bb->cur_pc = bb->instr_start;
            bb_process_invalid_instr(dcontext, bb);
            return false; /* stop bb */
        }
        ASSERT(instr_get_prefix_flag(bb->instr, PREFIX_SEG_FS));
    }
    /* expect to see only simple mov's to fs:[0] for new SEH frames
     * FIXME - might we see other types we'd want to intercept?
     * do we want to proccess pop instructions (usually just for removing
     * a frame)? */
    if (instr_get_opcode(bb->instr) == OP_mov_st) {
        bool is_to_fs0;
        opnd_t dst = instr_get_dst(bb->instr, 0);
        if (!bb_process_fs_ref_opnd(dcontext, bb, dst, &is_to_fs0))
            return false; /* end bb */
        /* Only process the push if building a new bb for cache, can't check
         * this any earlier since have to preserve bb building/ending behavior
         * even when not for cache (for recreation etc.). */
        if (bb->app_interp) {
            if (is_to_fs0) {
                ptr_int_t value = 0;
                opnd_t src = instr_get_src(bb->instr, 0);
                if (opnd_is_immed_int(src)) {
                    value = opnd_get_immed_int(src);
                } else if (opnd_is_reg(src)) {
                    value = reg_get_value_priv(opnd_get_reg(src), get_mcontext(dcontext));
                } else {
                    ASSERT_NOT_REACHED();
                }
                STATS_INC(num_SEH_pushes_processed);
                LOG(THREAD, LOG_INTERP, 3, "found mov to fs:[0] @ " PFX "\n",
                    bb->instr_start);
                bb_process_SEH_push(dcontext, bb, (void *)value);
            } else {
                STATS_INC(num_fs_movs_not_SEH);
            }
        }
    }
#    if defined(DEBUG) && defined(INTERNAL)
    else if (INTERNAL_OPTION(check_for_SEH_push)) {
        /* Debug build Sanity check that we aren't missing SEH frame pushes */
        int i;
        int num_dsts = instr_num_dsts(bb->instr);
        for (i = 0; i < num_dsts; i++) {
            bool is_to_fs0;
            opnd_t dst = instr_get_dst(bb->instr, i);
            if (!bb_process_fs_ref_opnd(dcontext, bb, dst, &is_to_fs0)) {
                STATS_INC(num_process_SEH_bb_early_terminate_debug);
                return false; /* end bb */
            }
            /* common case is pop instructions to fs:[0] when popping an
             * SEH frame stored on tos */
            if (is_to_fs0) {
                if (instr_get_opcode(bb->instr) == OP_pop) {
                    LOG(THREAD, LOG_INTERP, 4, "found pop to fs:[0] @ " PFX "\n",
                        bb->instr_start);
                    STATS_INC(num_process_SEH_pop_fs0);
                } else {
                    /* an unexpected SEH frame push */
                    LOG(THREAD, LOG_INTERP, 1,
                        "found unexpected write to fs:[0] @" PFX "\n", bb->instr_start);
                    DOLOG(1, LOG_INTERP, { d_r_loginst(dcontext, 1, bb->instr, ""); });
                    ASSERT_CURIOSITY(!is_to_fs0);
                }
            }
        }
    }
#    endif
    return true; /* continue bb */
}
#endif /* win32 */

#if defined(UNIX) && !defined(DGC_DIAGNOSTICS) && defined(X86)
/* The basic strategy for mangling mov_seg instruction is:
 * For mov fs/gs => reg/[mem], simply mangle it to write
 * the app's fs/gs selector value into dst.
 * For mov reg/mem => fs/gs, we make it as the first instruction
 * of bb, and mark that bb not linked and has mov_seg instr,
 * and change that instruction to be a nop.
 * Then whenever before entering code cache, we check if that's the bb
 * has mov_seg. If yes, we will update the information we maintained
 * about the app's fs/gs.
 */
/* check if the basic block building should continue on a mov_seg instr. */
static bool
bb_process_mov_seg(dcontext_t *dcontext, build_bb_t *bb)
{
    reg_id_t seg;

    if (!INTERNAL_OPTION(mangle_app_seg))
        return true; /* continue bb */

    /* if it is a read, we only need mangle the instruction. */
    ASSERT(instr_num_srcs(bb->instr) == 1);
    if (opnd_is_reg(instr_get_src(bb->instr, 0)) &&
        reg_is_segment(opnd_get_reg(instr_get_src(bb->instr, 0))))
        return true; /* continue bb */

    /* it is an update, we need set to be the first instr of bb */
    ASSERT(instr_num_dsts(bb->instr) == 1);
    ASSERT(opnd_is_reg(instr_get_dst(bb->instr, 0)));
    seg = opnd_get_reg(instr_get_dst(bb->instr, 0));
    ASSERT(reg_is_segment(seg));
    /* we only need handle fs/gs */
    if (seg != SEG_GS && seg != SEG_FS)
        return true; /* continue bb */
    /* if no private loader, we only need mangle the non-tls seg */
    if (seg == IF_X64_ELSE(SEG_FS, SEG_FS) && !INTERNAL_OPTION(private_loader))
        return true; /* continue bb */

    if (bb->instr_start == bb->start_pc) {
        /* the first instruction, we can continue build bb. */
        /* this bb cannot be part of trace! */
        bb->flags |= FRAG_CANNOT_BE_TRACE;
        bb->flags |= FRAG_HAS_MOV_SEG;
        return true; /* continue bb */
    }

    LOG(THREAD, LOG_INTERP, 3, "ending bb before mov_seg\n");
    /* Set cur_pc back to the start of this instruction and delete this
     * instruction from the bb ilist.
     */
    bb->cur_pc = instr_get_raw_bits(bb->instr);
    instrlist_remove(bb->ilist, bb->instr);
    instr_destroy(dcontext, bb->instr);
    /* Set instr to NULL in order to get translation of exit cti correct. */
    bb->instr = NULL;
    /* this block must be the last one in a trace
     * breaking traces here shouldn't be a perf issue b/c this is so rare,
     * it should happen only once per thread on setting up tls.
     */
    bb->flags |= FRAG_MUST_END_TRACE;
    return false; /* stop bb here */
}
#endif /* UNIX && X86 */

/* Returns true to indicate that ignorable syscall processing is completed
 * with *continue_bb indicating if the bb should be continued or not.
 * When returning false, continue_bb isn't pertinent.
 */
static bool
bb_process_ignorable_syscall(dcontext_t *dcontext, build_bb_t *bb, int sysnum,
                             bool *continue_bb)
{
    STATS_INC(ignorable_syscalls);
    BBPRINT(bb, 3, "found ignorable system call 0x%04x\n", sysnum);
#ifdef WINDOWS
    if (get_syscall_method() != SYSCALL_METHOD_SYSENTER) {
        DOCHECK(1, {
            if (get_syscall_method() == SYSCALL_METHOD_WOW64)
                ASSERT_NOT_TESTED();
        });
        if (continue_bb != NULL)
            *continue_bb = true;
        return true;
    } else {
        /* Can we continue interp after the sysenter at the instruction
         * after the call to sysenter? */
        instr_t *call = bb_verify_sysenter_pattern(dcontext, bb);

        if (call != NULL) {
            /* If we're continuing code discovery at the after-call address,
             * change the cur_pc to continue at the after-call addr. This is
             * safe since the preceding call is in the fragment and
             * %xsp/(%xsp) hasn't changed since the call. Obviously, we assume
             * that the sysenter breaks control flow in fashion such any
             * instruction that follows it isn't reached by DR.
             */
            if (DYNAMO_OPTION(ignore_syscalls_follow_sysenter)) {
                bb->cur_pc = instr_get_raw_bits(call) + instr_length(dcontext, call);
                if (continue_bb != NULL)
                    *continue_bb = true;
                return true;
            } else {
                /* End this bb now. We set the exit target so that control
                 * skips the vsyscall 'ret' that's executed natively after the
                 * syscall and ends up at the correct place.
                 */
                /* FIXME Assigning exit_target causes the fragment to end
                 * with a direct exit stub to the after-call address, which
                 * is fine. If bb->exit_target < bb->start_pc, the future
                 * fragment for exit_target is marked as a trace head which
                 * isn't intended. A potentially undesirable side effect
                 * is that exit_target's fragment can't be included in
                 * trace for start_pc.
                 */
                bb->exit_target = instr_get_raw_bits(call) + instr_length(dcontext, call);
                if (continue_bb != NULL)
                    *continue_bb = false;
                return true;
            }
        }
        STATS_INC(ignorable_syscalls_failed_sysenter_pattern);
        /* Pattern match failed but the syscall is ignorable so maybe we
         * can try shared syscall? */
        /* Decrement the stat to prevent double counting. We rarely expect to hit
         * this case. */
        STATS_DEC(ignorable_syscalls);
        return false;
    }
#elif defined(MACOS) && defined(X86)
    if (instr_get_opcode(bb->instr) == OP_sysenter) {
        /* To continue after the sysenter we need to go to the ret ibl, as user-mode
         * sysenter wrappers put the retaddr into edx as the post-kernel continuation.
         */
        bb->exit_type |= LINK_INDIRECT | LINK_RETURN;
        bb->ibl_branch_type = IBL_RETURN;
        bb->exit_target = get_ibl_routine(dcontext, get_ibl_entry_type(bb->exit_type),
                                          DEFAULT_IBL_BB(), bb->ibl_branch_type);
        LOG(THREAD, LOG_INTERP, 4, "sysenter exit target = " PFX "\n", bb->exit_target);
        if (continue_bb != NULL)
            *continue_bb = false;
    } else if (continue_bb != NULL)
        *continue_bb = true;
    return true;
#else
    if (continue_bb != NULL)
        *continue_bb = true;
    return true;
#endif
}

#ifdef WINDOWS
/* Process a syscall that is executed via shared syscall. */
static void
bb_process_shared_syscall(dcontext_t *dcontext, build_bb_t *bb, int sysnum)
{
    ASSERT(DYNAMO_OPTION(shared_syscalls));
    DODEBUG({
        if (ignorable_system_call(sysnum, bb->instr, NULL))
            STATS_INC(ignorable_syscalls);
        else
            STATS_INC(optimizable_syscalls);
    });
    BBPRINT(bb, 3, "found %soptimizable system call 0x%04x\n",
            INTERNAL_OPTION(shared_eq_ignore) ? "ignorable-" : "", sysnum);

    LOG(THREAD, LOG_INTERP, 3,
        "ending bb at syscall & NOT removing the interrupt itself\n");

    /* Mark the instruction as pointing to shared syscall */
    bb->instr->flags |= INSTR_SHARED_SYSCALL;
    /* this block must be the last one in a trace */
    bb->flags |= FRAG_MUST_END_TRACE;
    /* we redirect all optimizable syscalls to a single shared piece of code.
     * Once a fragment reaches the shared syscall code, it can be safely
     * deleted, for example, if the thread is interrupted for a callback and
     * DR needs to delete fragments for cache management.
     *
     * Note that w/shared syscall, syscalls can be executed from TWO
     * places -- shared_syscall and do_syscall.
     */
    bb->exit_target = shared_syscall_routine(dcontext);
    /* make sure translation for ending jmp ends up right, mangle will
     * remove this instruction, so set to NULL so translation does the
     * right thing */
    bb->instr = NULL;
}
#endif /* WINDOWS */

#ifdef ARM
/* This routine walks back to find the IT instr for the current IT block
 * and the position of instr in the current IT block, and returns whether
 * instr is the last instruction in the block.
 */
static bool
instr_is_last_in_it_block(instr_t *instr, instr_t **it_out, uint *pos_out)
{
    instr_t *it;
    int num_instrs;
    ASSERT(instr != NULL && instr_get_isa_mode(instr) == DR_ISA_ARM_THUMB &&
           instr_is_predicated(instr) && instr_is_app(instr));
    /* walk backward to find the IT instruction */
    for (it = instr_get_prev(instr), num_instrs = 1;
         /* meta and app instrs are treated identically here */
         it != NULL && num_instrs <= 4 /* max 4 instr in an IT block */;
         it = instr_get_prev(it)) {
        if (instr_is_label(it))
            continue;
        if (instr_get_opcode(it) == OP_it)
            break;
        num_instrs++;
    }
    ASSERT(it != NULL && instr_get_opcode(it) == OP_it);
    ASSERT(num_instrs <= instr_it_block_get_count(it));
    if (it_out != NULL)
        *it_out = it;
    if (pos_out != NULL)
        *pos_out = num_instrs - 1; /* pos starts from 0 */
    if (num_instrs == instr_it_block_get_count(it))
        return true;
    return false;
}

static void
adjust_it_instr_for_split(dcontext_t *dcontext, instr_t *it, uint pos)
{
    dr_pred_type_t block_pred[IT_BLOCK_MAX_INSTRS];
    uint i, block_count = instr_it_block_get_count(it);
    byte firstcond[2], mask[2];
    DEBUG_DECLARE(bool ok;)
    ASSERT(pos < instr_it_block_get_count(it) - 1);
    for (i = 0; i < block_count; i++)
        block_pred[i] = instr_it_block_get_pred(it, i);
    DOCHECK(CHKLVL_ASSERTS, {
        instr_t *instr;
        for (instr = instr_get_next_app(it), i = 0; instr != NULL;
             instr = instr_get_next_app(instr)) {
            ASSERT(instr_is_predicated(instr) && i <= pos);
            ASSERT(block_pred[i++] == instr_get_predicate(instr));
        }
    });
    DEBUG_DECLARE(ok =)
    instr_it_block_compute_immediates(
        block_pred[0], (pos > 0) ? block_pred[1] : DR_PRED_NONE,
        (pos > 1) ? block_pred[2] : DR_PRED_NONE, DR_PRED_NONE, /* at most 3 preds */
        &firstcond[0], &mask[0]);
    ASSERT(ok);
    DOCHECK(CHKLVL_ASSERTS, {
        DEBUG_DECLARE(ok =)
        instr_it_block_compute_immediates(
            block_pred[pos + 1],
            (block_count > pos + 2) ? block_pred[pos + 2] : DR_PRED_NONE,
            (block_count > pos + 3) ? block_pred[pos + 3] : DR_PRED_NONE,
            DR_PRED_NONE, /* at most 3 preds */
            &firstcond[1], &mask[1]);
        ASSERT(ok);
    });
    /* firstcond should be unchanged */
    ASSERT(opnd_get_immed_int(instr_get_src(it, 0)) == firstcond[0]);
    instr_set_src(it, 1, OPND_CREATE_INT(mask[0]));
    LOG(THREAD, LOG_INTERP, 3,
        "ending bb in an IT block & adjusting the IT instruction\n");
    /* FIXME i#1669: NYI on passing split it block info to next bb */
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* ARM */

static bool
bb_process_non_ignorable_syscall(dcontext_t *dcontext, build_bb_t *bb, int sysnum)
{
    BBPRINT(bb, 3, "found non-ignorable system call 0x%04x\n", sysnum);
    STATS_INC(non_ignorable_syscalls);
    bb->exit_type |= LINK_NI_SYSCALL;
    /* destroy the interrupt instruction */
    LOG(THREAD, LOG_INTERP, 3, "ending bb at syscall & removing the interrupt itself\n");
    /* Indicate that this is a non-ignorable syscall so mangle will remove */
    /* FIXME i#1551: maybe we should union int80 and svc as both are inline syscall? */
#ifdef UNIX
    if (instr_get_opcode(bb->instr) ==
        IF_X86_ELSE(OP_int, IF_RISCV64_ELSE(OP_ecall, OP_svc))) {
#    if defined(MACOS) && defined(X86)
        int num = instr_get_interrupt_number(bb->instr);
        if (num == 0x81 || num == 0x82) {
            bb->exit_type |= LINK_SPECIAL_EXIT;
            bb->instr->flags |= INSTR_BRANCH_SPECIAL_EXIT;
        } else {
            ASSERT(num == 0x80);
#    endif /* MACOS && X86 */
            bb->exit_type |= LINK_NI_SYSCALL_INT;
            bb->instr->flags |= INSTR_NI_SYSCALL_INT;
#    if defined(MACOS) && defined(X86)
        }
#    endif
    } else
#endif
        bb->instr->flags |= INSTR_NI_SYSCALL;
#ifdef ARM
    /* we assume all conditional syscalls are treated as non-ignorable */
    if (instr_is_predicated(bb->instr)) {
        instr_t *it;
        uint pos;
        ASSERT(instr_is_syscall(bb->instr));
        bb->svc_pred = instr_get_predicate(bb->instr);
        if (instr_get_isa_mode(bb->instr) == DR_ISA_ARM_THUMB &&
            !instr_is_last_in_it_block(bb->instr, &it, &pos)) {
            /* FIXME i#1669: we violate the transparency and clients will see
             * modified IT instr.  We should adjust the IT instr at mangling
             * stage after client instrumentation, but that is complex.
             */
            adjust_it_instr_for_split(dcontext, it, pos);
        }
    }
#endif
    /* Set instr to NULL in order to get translation of exit cti correct. */
    bb->instr = NULL;
    /* this block must be the last one in a trace */
    bb->flags |= FRAG_MUST_END_TRACE;
    return false; /* end bb now */
}

/* returns true to indicate "continue bb" and false to indicate "end bb now" */
static inline bool
bb_process_syscall(dcontext_t *dcontext, build_bb_t *bb)
{
    int sysnum;
    /* PR 307284: for simplicity do syscall/int processing post-client.
     * We give up on inlining but we can still use ignorable/shared syscalls
     * and trace continuation.
     */
    if (bb->pass_to_client && !bb->post_client)
        return false;
#ifdef DGC_DIAGNOSTICS
    if (TEST(FRAG_DYNGEN, bb->flags) && !is_dyngen_vsyscall(bb->instr_start)) {
        LOG(THREAD, LOG_INTERP, 1, "WARNING: syscall @ " PFX " in dyngen code!\n",
            bb->instr_start);
    }
#endif
    BBPRINT(bb, 4, "interp: syscall @ " PFX "\n", bb->instr_start);
    check_syscall_method(dcontext, bb->instr);
    bb->flags |= FRAG_HAS_SYSCALL;
    /* if we can identify syscall number and it is an ignorable syscall,
     * we let bb keep going, else we end bb and flag it
     */
    sysnum = find_syscall_num(dcontext, bb->ilist, bb->instr);
#ifdef VMX86_SERVER
    DOSTATS({
        if (instr_get_opcode(bb->instr) == OP_int &&
            instr_get_interrupt_number(bb->instr) == VMKUW_SYSCALL_GATEWAY) {
            STATS_INC(vmkuw_syscall_sites);
            LOG(THREAD, LOG_SYSCALLS, 2, "vmkuw system call site: #=%d\n", sysnum);
        }
    });
#endif
    BBPRINT(bb, 3, "syscall # is %d\n", sysnum);
    if (sysnum != -1 && instrument_filter_syscall(dcontext, sysnum)) {
        BBPRINT(bb, 3, "client asking to intercept => pretending syscall # %d is -1\n",
                sysnum);
        sysnum = -1;
    }
#ifdef ARM
    if (sysnum != -1 && instr_is_predicated(bb->instr)) {
        BBPRINT(bb, 3,
                "conditional system calls cannot be inlined => "
                "pretending syscall # %d is -1\n",
                sysnum);
        sysnum = -1;
    }
#endif
    if (sysnum != -1 && DYNAMO_OPTION(ignore_syscalls) &&
        ignorable_system_call(sysnum, bb->instr, NULL)
#ifdef X86
        /* PR 288101: On Linux we do not yet support inlined sysenter instrs as we
         * do not have in-cache support for the post-sysenter continuation: we rely
         * for now on very simple sysenter handling where d_r_dispatch uses asynch_target
         * to know where to go next.
         */
        IF_LINUX(&&instr_get_opcode(bb->instr) != OP_sysenter)
#endif /* X86 */
    ) {

        bool continue_bb;

        if (bb_process_ignorable_syscall(dcontext, bb, sysnum, &continue_bb)) {
            if (!DYNAMO_OPTION(inline_ignored_syscalls))
                continue_bb = false;
            return continue_bb;
        }
    }
#ifdef WINDOWS
    if (sysnum != -1 && DYNAMO_OPTION(shared_syscalls) &&
        optimizable_system_call(sysnum)) {
        bb_process_shared_syscall(dcontext, bb, sysnum);
        return false;
    }
#endif

    /* Fall thru and handle as a non-ignorable syscall. */
    return bb_process_non_ignorable_syscall(dcontext, bb, sysnum);
}

/* Case 3922: for wow64 we treat "call *fs:0xc0" as a system call.
 * Only sets continue_bb if it returns true.
 */
static bool
bb_process_indcall_syscall(dcontext_t *dcontext, build_bb_t *bb, bool *continue_bb)
{
    ASSERT(continue_bb != NULL);
#ifdef WINDOWS
    if (instr_is_wow64_syscall(bb->instr)) {
        /* we could check the preceding instrs but we don't bother */
        *continue_bb = bb_process_syscall(dcontext, bb);
        return true;
    }
#endif
    return false;
}

/* returns true to indicate "continue bb" and false to indicate "end bb now" */
static inline bool
bb_process_interrupt(dcontext_t *dcontext, build_bb_t *bb)
{
#if defined(DEBUG) || defined(INTERNAL) || defined(WINDOWS)
    int num = instr_get_interrupt_number(bb->instr);
#endif
    /* PR 307284: for simplicity do syscall/int processing post-client.
     * We give up on inlining but we can still use ignorable/shared syscalls
     * and trace continuation.
     * PR 550752: we cannot end at int 0x2d: we live w/ client consequences
     */
    if (bb->pass_to_client && !bb->post_client IF_WINDOWS(&&num != 0x2d))
        return false;
    BBPRINT(bb, 3, "int 0x%x @ " PFX "\n", num, bb->instr_start);
#ifdef WINDOWS
    if (num == 0x2b) {
        /* interrupt 0x2B signals return from callback */
        /* end block here and come back to dynamo to perform interrupt */
        bb->exit_type |= LINK_CALLBACK_RETURN;
        BBPRINT(bb, 3, "ending bb at cb ret & removing the interrupt itself\n");
        /* Set instr to NULL in order to get translation of exit cti
         * correct.  mangle will destroy the instruction */
        bb->instr = NULL;
        bb->flags |= FRAG_MUST_END_TRACE;
        STATS_INC(num_int2b);
        return false;
    } else {
        SYSLOG_INTERNAL_INFO_ONCE("non-syscall, non-int2b 0x%x @ " PFX " from " PFX, num,
                                  bb->instr_start, bb->start_pc);
    }
#endif /* WINDOWS */
    return true;
}

/* If the current instr in the BB is an indirect call that can be converted into a
 * direct call, process it and return true, else, return false.
 * FIXME PR 288327: put in linux call* to vsyscall page
 */
static bool
bb_process_convertible_indcall(dcontext_t *dcontext, build_bb_t *bb)
{
#ifdef X86
    /* We perform several levels of checking, each increasingly more stringent
     * and expensive, with a false return should any fail.
     */
    instr_t *instr;
    opnd_t src0;
    instr_t *call_instr;
    int call_src_reg;
    app_pc callee;
    bool vsyscall = false;

    /* Check if this BB can be extended and the instr is a (near) indirect call */
    if (instr_get_opcode(bb->instr) != OP_call_ind)
        return false;

    /* Check if we have a "mov <imm> -> %reg; call %reg" or a
     * "mov <imm> -> %reg; call (%reg)" pair. First check for the call.
     */
    /* The 'if' conditions are broken up to make the code more readable
     * while #ifdef-ing the WINDOWS case. It's still ugly though.
     */
    instr = bb->instr;
    if (!(
#    ifdef WINDOWS
            /* Match 'call (%xdx)' for a post-SP2 indirect call to sysenter. */
            (opnd_is_near_base_disp(instr_get_src(instr, 0)) &&
             opnd_get_base(instr_get_src(instr, 0)) == REG_XDX &&
             opnd_get_disp(instr_get_src(instr, 0)) == 0) ||
#    endif
            /* Match 'call %reg'. */
            opnd_is_reg(instr_get_src(instr, 0))))
        return false;

    /* If there's no CTI in the BB, we can check if there are 5+ preceding
     * bytes and if they could hold a "mov" instruction.
     */
    if (!TEST(FRAG_HAS_DIRECT_CTI, bb->flags) && bb->instr_start - 5 >= bb->start_pc) {

        byte opcode = *((byte *)bb->instr_start - 5);

        /* Check the opcode. Do we see a "mov ... -> %reg"?  Valid opcodes are in
         * the 0xb8-0xbf range (Intel IA-32 ISA ref, v.2) and specify the
         * destination register, i.e., 0xb8 means that %xax is the destination.
         */
        if (opcode < 0xb8 || opcode > 0xbf)
            return false;
    }

    /* Check the previous instruction -- is it really a "mov"? */
    src0 = instr_get_src(instr, 0);
    call_instr = instr;
    instr = instr_get_prev_expanded(dcontext, bb->ilist, bb->instr);
    call_src_reg =
        opnd_is_near_base_disp(src0) ? opnd_get_base(src0) : opnd_get_reg(src0);
    if (instr == NULL || instr_get_opcode(instr) != OP_mov_imm ||
        opnd_get_reg(instr_get_dst(instr, 0)) != call_src_reg)
        return false;

    /* For the general case, we don't try to optimize a call
     * thru memory -- just check that the call uses a register.
     */
    callee = NULL;
    if (opnd_is_reg(src0)) {
        /* Extract the target address. */
        callee = (app_pc)opnd_get_immed_int(instr_get_src(instr, 0));
#    ifdef WINDOWS
#        ifdef PROGRAM_SHEPHERDING
        /* FIXME - is checking for on vsyscall page better or is checking == to
         * VSYSCALL_BOOTSTRAP_ADDR? Both are hacky. */
        if (is_dyngen_vsyscall((app_pc)opnd_get_immed_int(instr_get_src(instr, 0)))) {
            LOG(THREAD, LOG_INTERP, 4,
                "Pre-SP2 style indirect call "
                "to sysenter found at " PFX "\n",
                bb->instr_start);
            STATS_INC(num_sysenter_indcalls);
            vsyscall = true;
            ASSERT(opnd_get_immed_int(instr_get_src(instr, 0)) ==
                   (ptr_int_t)VSYSCALL_BOOTSTRAP_ADDR);
            ASSERT(!use_ki_syscall_routines()); /* double check our determination */
        } else
#        endif
#    endif
            STATS_INC(num_convertible_indcalls);
    }
#    ifdef WINDOWS
    /* Match the "call (%xdx)" to sysenter case for SP2-patched os's. Memory at
     * address VSYSCALL_BOOTSTRAP_ADDR (0x7ffe0300) holds the address of
     * KiFastSystemCall or (FIXME - not handled) on older platforms KiIntSystemCall.
     * FIXME It's unsavory to hard-code 0x7ffe0300, but the constant has little
     * context in an SP2 os. It's a hold-over from pre-SP2.
     */
    else if (get_syscall_method() == SYSCALL_METHOD_SYSENTER && call_src_reg == REG_XDX &&
             opnd_get_immed_int(instr_get_src(instr, 0)) ==
                 (ptr_int_t)VSYSCALL_BOOTSTRAP_ADDR) {
        /* Extract the target address. We expect that the memory read using the
         * value in the immediate field is ok as it's the vsyscall page
         * which 1) cannot be made unreadable and 2) cannot be made writable so
         * the stored value will not change. Of course, it's possible that the
         * os could change the page contents.
         */
        callee = (app_pc) * ((ptr_uint_t *)opnd_get_immed_int(instr_get_src(instr, 0)));
        if (get_app_sysenter_addr() == NULL) {
            /* For the first call* we've yet to decode an app syscall, yet we
             * cannot have later recreations have differing behavior, so we must
             * handle that case (even though it doesn't matter performance-wise
             * as the first call* is usually in runtime init code that's
             * executed once).  So we do a raw byte compare to:
             *   ntdll!KiFastSystemCall:
             *   7c82ed50 8bd4             mov     xdx,xsp
             *   7c82ed52 0f34             sysenter
             */
            uint raw;
            if (!d_r_safe_read(callee, sizeof(raw), &raw) || raw != 0x340fd48b)
                callee = NULL;
        } else {
            /* The callee should be a 2 byte "mov %xsp -> %xdx" followed by the
             * sysenter -- check the sysenter's address as 2 bytes past the callee.
             */
            if (callee + 2 != get_app_sysenter_addr())
                callee = NULL;
        }
        vsyscall = (callee != NULL);
        ASSERT(use_ki_syscall_routines()); /* double check our determination */
        DODEBUG({
            if (callee == NULL)
                ASSERT_CURIOSITY(false && "call* to vsyscall unexpected mismatch");
            else {
                LOG(THREAD, LOG_INTERP, 4,
                    "Post-SP2 style indirect call "
                    "to sysenter found at " PFX "\n",
                    bb->instr_start);
                STATS_INC(num_sysenter_indcalls);
            }
        });
    }
#    endif

    /* Check if register dataflow matched and we were able to extract
     * the callee address.
     */
    if (callee == NULL)
        return false;

    if (vsyscall) {
        /* Case 8917: abandon coarse-grainness in favor of performance */
        bb->flags &= ~FRAG_COARSE_GRAIN;
        STATS_INC(coarse_prevent_indcall);
    }

    LOG(THREAD, LOG_INTERP, 4,
        "interp: possible convertible"
        " indirect call from " PFX " to " PFX "\n",
        bb->instr_start, callee);

    if (leave_call_native(callee) || must_not_be_entered(callee)) {
        BBPRINT(bb, 3, "   NOT inlining indirect call to " PFX "\n", callee);
        /* Case 8711: coarse-grain can't handle non-exit cti */
        bb->flags &= ~FRAG_COARSE_GRAIN;
        STATS_INC(coarse_prevent_cti);
        ASSERT_CURIOSITY_ONCE(!vsyscall && "leaving call* to vsyscall");
        /* no need for bb_add_native_direct_xfer() b/c it's already indirect */
        return true; /* keep bb going, w/o inlining call */
    }

    if (bb->follow_direct && !must_not_be_entered(callee) &&
        bb->num_elide_call < DYNAMO_OPTION(max_elide_call) &&
        (DYNAMO_OPTION(elide_back_calls) || bb->cur_pc <= callee)) {
        /* FIXME This is identical to the code for evaluating a
         * direct call's callee. If such code appears in another
         * (3rd) place, we should outline it.
         * FIXME: use follow_direct_call()
         */
        if (vsyscall) {
            /* As a flag to allow our xfer from now-non-coarse to coarse
             * (for vsyscall-in-ntdll) we pre-emptively mark as has-syscall.
             */
            ASSERT(!TEST(FRAG_HAS_SYSCALL, bb->flags));
            bb->flags |= FRAG_HAS_SYSCALL;
        }
        if (check_new_page_jmp(dcontext, bb, callee)) {
            if (vsyscall) /* Restore */
                bb->flags &= ~FRAG_HAS_SYSCALL;
            bb->num_elide_call++;
            STATS_INC(total_elided_calls);
            STATS_TRACK_MAX(max_elided_calls, bb->num_elide_call);
            bb->cur_pc = callee;
            /* FIXME: when using follow_direct_call don't forget to set this */
            call_instr->flags |= INSTR_IND_CALL_DIRECT;
            BBPRINT(bb, 4, "   continuing in callee at " PFX "\n", bb->cur_pc);
            return true; /* keep bb going */
        }
        if (vsyscall) {
            /* Case 8917: Restore, just in case, though we certainly expect to have
             * this flag set as soon as we decode a few more instrs and hit the
             * syscall itself -- but for pre-sp2 we currently could be elsewhere on
             * the same page, so let's be safe here.
             */
            bb->flags &= ~FRAG_HAS_SYSCALL;
        }
    }
    /* FIXME: we're also not converting to a direct call - was this intended? */
    BBPRINT(bb, 3, "   NOT following indirect call from " PFX " to " PFX "\n",
            bb->instr_start, callee);
    DODEBUG({
        if (vsyscall) {
            DO_ONCE({
                /* Case 9095: don't complain so loudly if user asked for no elision */
                if (DYNAMO_OPTION(max_elide_call) <= 2)
                    SYSLOG_INTERNAL_WARNING("leaving call* to vsyscall");
                else
                    ASSERT_CURIOSITY(false && "leaving call* to vsyscall");
            });
        }
    });
    ;
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif            /* X86 */
    return false; /* stop bb */
}

/* FIXME i#1668, i#2974: NYI on ARM/AArch64 */
#ifdef X86
/* if we make the IAT sections unreadable we will need to map to proper location */
static inline app_pc
read_from_IAT(app_pc iat_reference)
{
    /* FIXME: we should have looked up where the real IAT should be at
     * the time of checking whether is_in_IAT
     */
    return *(app_pc *)iat_reference;
}

/* returns whether target is an IAT of a module that we convert.  Note
 * users still have to check the referred to value to verify targeting
 * a native module.
 */
static bool
is_targeting_convertible_IAT(dcontext_t *dcontext, instr_t *instr,
                             app_pc *iat_reference /* OUT */)
{
    /* FIXME: we could give up on optimizing a particular module,
     * if too many writes to its IAT are found,
     * even 1 may be too much to handle!
     */

    /* We only allow constant address,
     * any registers used for effective address calculation
     * can not be guaranteed to be constant dynamically.
     */
    /* FIXME: yet a 'call %reg' if that value is an export would be a
     * good sign that we should go backwards and look for a possible
     * mov IAT[func] -> %reg and then optimize that as well - case 1948
     */

    app_pc memory_reference = NULL;
    opnd_t opnd = instr_get_target(instr);

    LOG(THREAD, LOG_INTERP, 4, "is_targeting_convertible_IAT: ");

    /* A typical example of a proper call
     * ff 15 8810807c     call    dword ptr [kernel32+0x1088 (7c801088)]
     * where
     * [7c801088] =  7c90f04c ntdll!RtlAnsiStringToUnicodeString
     *
     * The ModR/M byte for a displacement only with no SIB should be
     * 15 for CALL, 25 for JMP, (no far versions for IAT)
     */
    if (opnd_is_near_base_disp(opnd)) {
        /* FIXME PR 253930: pattern-match x64 IAT calls */
        IF_X64(ASSERT_NOT_IMPLEMENTED(false));
        memory_reference = (app_pc)(ptr_uint_t)opnd_get_disp(opnd);

        /* now should check all other fields */
        if (opnd_get_base(opnd) != REG_NULL || opnd_get_index(opnd) != REG_NULL) {
            /* this is not a pure memory reference, can't be IAT */
            return false;
        }
        ASSERT(opnd_get_scale(opnd) == 0);
    } else {
        return false;
    }

    LOG(THREAD, LOG_INTERP, 3, "is_targeting_convertible_IAT: memory_reference " PFX "\n",
        memory_reference);

    /* FIXME: if we'd need some more additional structures those can
     * be looked up in a separate hashtable based on the IAT base, or
     * we'd have to extend the vmareas with custom fields
     */
    ASSERT(DYNAMO_OPTION(IAT_convert));
    if (vmvector_overlap(IAT_areas, memory_reference, memory_reference + 1)) {
        /* IAT has to be in the same module as current instruction,
         * but even in the unlikely reference by address from another
         * module there is really no problem, so not worth checking
         */
        ASSERT_CURIOSITY(get_module_base(instr->bytes) ==
                         get_module_base(memory_reference));

        /* FIXME: now that we know it is in IAT/GOT,
         * we have to READ the contents and return that
         * safely to the caller so they can convert accordingly
         */

        /* FIXME: we would want to add the IAT section to the vmareas
         * of a region that has a converted block.  Then on a write to
         * IAT we can flush efficiently only blocks affected by a
         * particular module, for a first hack though flushing
         * everything on a hooker will do.
         */
        *iat_reference = memory_reference;
        return true;
    } else {
        /* plain global function
         * e.g. ntdll!RtlUnicodeStringToAnsiString+0x4c:
         * ff15c009917c call dword ptr [ntdll!RtlAllocateStringRoutine (7c9109c0)]
         */
        return false;
    }
}
#endif /* X86 */

/* If the current instr in the BB is an indirect call through IAT that
 * can be converted into a direct call, process it and return true,
 * else, return false.
 */
static bool
bb_process_IAT_convertible_indjmp(dcontext_t *dcontext, build_bb_t *bb,
                                  bool *elide_continue)
{
#ifdef X86
    app_pc iat_reference;
    app_pc target;
    ASSERT(DYNAMO_OPTION(IAT_convert));

    /* Check if the instr is a (near) indirect jump */
    if (instr_get_opcode(bb->instr) != OP_jmp_ind) {
        ASSERT_CURIOSITY(false && "far ind jump");
        return false; /* not matching, stop bb */
    }

    if (!is_targeting_convertible_IAT(dcontext, bb->instr, &iat_reference)) {
        DOSTATS({
            if (EXIT_IS_IND_JMP_PLT(bb->exit_type)) {
                /* see how often we mark as likely a PLT a JMP which in
                 * fact is not going through IAT
                 */
                STATS_INC(num_indirect_jumps_PLT_not_IAT);
                LOG(THREAD, LOG_INTERP, 3,
                    "bb_process_IAT_convertible_indjmp: indirect jmp not PLT instr=" PFX
                    "\n",
                    bb->instr->bytes);
            }
        });

        return false; /* not matching, stop bb */
    }

    target = read_from_IAT(iat_reference);

    DOLOG(4, LOG_INTERP, {
        char name[MAXIMUM_SYMBOL_LENGTH];
        print_symbolic_address(target, name, sizeof(name), false);
        LOG(THREAD, LOG_INTERP, 4,
            "bb_process_IAT_convertible_indjmp: target=" PFX " %s\n", target, name);
    });

    STATS_INC(num_indirect_jumps_IAT);
    DOSTATS({
        if (!EXIT_IS_IND_JMP_PLT(bb->exit_type)) {
            /* count any other known uses for an indirect jump to go
             * through the IAT other than PLT uses, although a block
             * reaching max_elide_call would prevent the above
             * match */
            STATS_INC(num_indirect_jumps_IAT_not_PLT);
            /* FIXME: case 6459 for further inquiry */
            LOG(THREAD, LOG_INTERP, 4,
                "bb_process_IAT_convertible_indjmp: indirect jmp not PLT target=" PFX
                "\n",
                target);
        }
    });

    if (must_not_be_elided(target)) {
        ASSERT_NOT_TESTED();
        BBPRINT(bb, 3, "   NOT inlining indirect jmp to must_not_be_elided " PFX "\n",
                target);
        return false; /* do not convert indirect jump, will stop bb */
    }

    /* Verify not targeting native exec DLLs, note that the IATs of
     * any module may have imported a native DLL.  Note it may be
     * possible to optimize with a range check on IAT subregions, but
     * this check isn't much slower.
     */

    /* IAT_elide should definitely not touch native_exec modules.
     *
     * FIXME: we also prevent IAT_convert from optimizing imports in
     * native_exec_list DLLs, although we could let that convert to a
     * direct jump and require native_exec_dircalls to be always on to
     * intercept those jmps.
     */
    if (DYNAMO_OPTION(native_exec) && is_native_pc(target)) {
        BBPRINT(bb, 3, "   NOT inlining indirect jump to native exec module " PFX "\n",
                target);
        STATS_INC(num_indirect_jumps_IAT_native);
        return false; /* do not convert indirect jump, stop bb */
    }

    /* mangle mostly as such as direct jumps would be mangled in
     * bb_process_ubr(dcontext, bb) but note bb->instr has already
     * been appended so has to reverse some of its actions
     */

    /* pretend never saw an indirect JMP, we'll either add a new
       direct JMP or we'll just continue in target */
    instrlist_remove(bb->ilist, bb->instr); /* bb->instr has been appended already */
    instr_destroy(dcontext, bb->instr);
    bb->instr = NULL;

    if (DYNAMO_OPTION(IAT_elide)) {
        /* try to elide just as a direct jmp would have been elided */

        /* We could have used follow_direct_call instead since
         * commonly this really is a disguised CALL*. Yet for PLT use
         * of the form of CALL PLT[foo]; JMP* IAT[foo] we would have
         * already counted the CALL.  If we have tail call elimination
         * that converts a CALL* into a JMP* it is also OK to treat as
         * a JMP instead of a CALL just as if sharing tails.
         */
        if (follow_direct_jump(dcontext, bb, target)) {
            LOG(THREAD, LOG_INTERP, 4,
                "bb_process_IAT_convertible_indjmp: eliding jmp* target=" PFX "\n",
                target);

            STATS_INC(num_indirect_jumps_IAT_elided);
            *elide_continue = true; /* do not stop bb */
            return true;            /* converted indirect to direct */
        }
    }
    /* otherwise convert to direct jump without eliding */

    /* we set bb->instr to NULL so unlike bb_process_ubr
     * we get the final exit_target added by build_bb_ilist
     * FIXME: case 85: which will work only when we're using bb->mangle_ilist
     * FIXME: what are callers supposed to see when we do NOT mangle?
     */

    LOG(THREAD, LOG_INTERP, 4,
        "bb_process_IAT_convertible_indjmp: converting jmp* target=" PFX "\n", target);

    STATS_INC(num_indirect_jumps_IAT_converted);
    /* end basic block with a direct JMP to target */
    bb->exit_target = target;
    *elide_continue = false; /* matching, but should stop bb */
    return true;             /* matching */
#elif defined(AARCHXX)
    /* FIXME i#1551, i#1569: NYI on ARM/AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#endif /* X86/ARM/RISCV64 */
}

/* Returns true if the current instr in the BB is an indirect call
 * through IAT that can be converted into a direct call, process it
 * and sets elide_continue.  Otherwise function return false.
 * OUT elide_continue is set when bb building should continue in target,
 * and not set when bb building should be stopped.
 */
static bool
bb_process_IAT_convertible_indcall(dcontext_t *dcontext, build_bb_t *bb,
                                   bool *elide_continue)
{
#ifdef X86
    app_pc iat_reference;
    app_pc target;
    ASSERT(DYNAMO_OPTION(IAT_convert));

    /* FIXME: the code structure is the same as
     * bb_process_IAT_convertible_indjmp, could fuse the two
     */

    /* We perform several levels of checking, each increasingly more stringent
     * and expensive, with a false return should any fail.
     */

    /* Check if the instr is a (near) indirect call */
    if (instr_get_opcode(bb->instr) != OP_call_ind) {
        ASSERT_CURIOSITY(false && "far call");
        return false; /* not matching, stop bb */
    }

    if (!is_targeting_convertible_IAT(dcontext, bb->instr, &iat_reference)) {
        return false; /* not matching, stop bb */
    }
    target = read_from_IAT(iat_reference);
    DOLOG(4, LOG_INTERP, {
        char name[MAXIMUM_SYMBOL_LENGTH];
        print_symbolic_address(target, name, sizeof(name), false);
        LOG(THREAD, LOG_INTERP, 4,
            "bb_process_IAT_convertible_indcall: target=" PFX " %s\n", target, name);
    });
    STATS_INC(num_indirect_calls_IAT);

    /* mangle mostly as such as direct calls are mangled with
     * bb_process_call_direct(dcontext, bb)
     */

    if (leave_call_native(target) || must_not_be_entered(target)) {
        ASSERT_NOT_TESTED();
        BBPRINT(bb, 3, "   NOT inlining indirect call to leave_call_native " PFX "\n",
                target);
        return false; /* do not convert indirect call, stop bb */
    }

    /* Verify not targeting native exec DLLs, note that the IATs of
     * any module may have imported a native DLL.  Note it may be
     * possible to optimize with a range check on IAT subregions, but
     * this check isn't much slower.
     */
    if (DYNAMO_OPTION(native_exec) && is_native_pc(target)) {
        BBPRINT(bb, 3, "   NOT inlining indirect call to native exec module " PFX "\n",
                target);
        STATS_INC(num_indirect_calls_IAT_native);
        return false; /* do not convert indirect call, stop bb */
    }

    /* mangle_indirect_call and calculate return address as of
     * bb->instr and will remove bb->instr
     * FIXME: it would have been
     * better to replace in instrlist with a direct call and have
     * mangle_{in,}direct_call use other than the raw bytes, but this for now does the
     * job.
     */
    bb->instr->flags |= INSTR_IND_CALL_DIRECT;

    if (DYNAMO_OPTION(IAT_elide)) {
        /* try to elide just as a direct call would have been elided */
        if (follow_direct_call(dcontext, bb, target)) {
            LOG(THREAD, LOG_INTERP, 4,
                "bb_process_IAT_convertible_indcall: eliding call* flags=0x%08x "
                "target=" PFX "\n",
                bb->instr->flags, target);

            STATS_INC(num_indirect_calls_IAT_elided);
            *elide_continue = true; /* do not stop bb */
            return true;            /* converted indirect to direct */
        }
    }
    /* otherwise convert to direct call without eliding */

    LOG(THREAD, LOG_INTERP, 4,
        "bb_process_IAT_convertible_indcall: converting call* flags=0x%08x target=" PFX
        "\n",
        bb->instr->flags, target);

    STATS_INC(num_indirect_calls_IAT_converted);
    /* bb->instr has been appended already, and will get removed by
     * mangle_indirect_call.  We don't need to set to NULL, since this
     * instr is a CTI and the final jump's translation target should
     * still be the original indirect call.
     */
    bb->exit_target = target;
    /* end basic block with a direct CALL to target.  With default
     * options it should get mangled to a PUSH; JMP
     */
    *elide_continue = false; /* matching, but should stop bb */
    return true;             /* converted indirect to direct */
#elif defined(AARCHXX)
    /* FIXME i#1551, i#1569: NYI on ARM/AArch64 */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#endif /* X86/ARM/RISCV64 */
}

/* Called on instructions that save the FPU state */
static void
bb_process_float_pc(dcontext_t *dcontext, build_bb_t *bb)
{
    /* i#698: for instructions that save the floating-point state
     * (e.g., fxsave), we go back to d_r_dispatch to translate the fp pc.
     * We rule out being in a trace (and thus a potential alternative
     * would be to use a FRAG_ flag).  These are rare instructions so that
     * shouldn't have a significant perf impact: except we've been hitting
     * libm code that uses fnstenv and is not rare, so we have non-inlined
     * translation under an option for now.
     */
    if (DYNAMO_OPTION(translate_fpu_pc)) {
        bb->exit_type |= LINK_SPECIAL_EXIT;
        bb->flags |= FRAG_CANNOT_BE_TRACE;
    }
    /* If we inline the pc update, we can't persist.  Simplest to keep fine-grained. */
    bb->flags &= ~FRAG_COARSE_GRAIN;
}

static bool
instr_will_be_exit_cti(instr_t *inst)
{
    /* can't use instr_is_exit_cti() on pre-mangled instrs */
    return (instr_is_app(inst) && instr_is_cti(inst) &&
            (!instr_is_near_call_direct(inst) ||
             !leave_call_native(instr_get_branch_target_pc(inst)))
            /* PR 239470: ignore wow64 syscall, which is an ind call */
            IF_WINDOWS(&&!instr_is_wow64_syscall(inst)));
}

/* PR 215217: check syscall restrictions */
static bool
client_check_syscall(instrlist_t *ilist, instr_t *inst, bool *found_syscall,
                     bool *found_int)
{
    int op_int = IF_X86_ELSE(OP_int, IF_RISCV64_ELSE(OP_ecall, OP_svc));
    /* We do consider the wow64 call* a syscall here (it is both
     * a syscall and a call*: PR 240258).
     */
    if (instr_is_syscall(inst) || instr_get_opcode(inst) == op_int) {
        if (instr_is_syscall(inst) && found_syscall != NULL)
            *found_syscall = true;
        /* Xref PR 313869 - we should be ignoring int 3 here. */
        if (instr_get_opcode(inst) == op_int && found_int != NULL)
            *found_int = true;
        /* For linux an ignorable syscall is not a problem.  Our
         * pre-syscall-exit jmp is added post client mangling so should
         * be robust.
         * FIXME: now that we have -no_inline_ignored_syscalls should
         * we assert on ignorable also?  Probably we'd have to have
         * an exception for the middle of a trace?
         */
        if (IF_UNIX(TEST(INSTR_NI_SYSCALL, inst->flags))
            /* PR 243391: only block-ending interrupt 2b matters */
            IF_WINDOWS(instr_is_syscall(inst) ||
                       ((instr_get_opcode(inst) == OP_int &&
                         instr_get_interrupt_number(inst) == 0x2b)))) {
            /* This check means we shouldn't hit the exit_type flags
             * check below but we leave it in place in case we add
             * other flags in future
             */
            if (inst != instrlist_last(ilist)) {
                CLIENT_ASSERT(false, "a syscall or interrupt must terminate the block");
                return false;
            }
            /* should we forcibly delete the subsequent instrs?
             * or the client has to deal w/ bad behavior in release build?
             */
        }
    }
    return true;
}

/* Pass bb to client, and afterward check for criteria we require and rescan for
 * eflags and other flags that might have changed.
 * Returns true normally; returns false to indicate "go native".
 */
static bool
client_process_bb(dcontext_t *dcontext, build_bb_t *bb)
{
    dr_emit_flags_t emitflags = DR_EMIT_DEFAULT;
    instr_t *inst;
    bool found_exit_cti = false;
    bool found_syscall = false;
    bool found_int = false;
#ifdef ANNOTATIONS
    app_pc trailing_annotation_pc = NULL, instrumentation_pc = NULL;
    bool found_instrumentation_pc = false;
    instr_t *annotation_label = NULL;
#endif
    instr_t *last_app_instr = NULL;

    /* This routine is called by more than just bb builder, also used
     * for recreating state, so only call if caller requested it
     * (usually that coincides w/ bb->app_interp being set, but not
     * when recreating state on a fault (PR 214962)).
     * FIXME: hot patches shouldn't be injected during state recreations;
     *        does predicating on bb->app_interp take care of this issue?
     */
    if (!bb->pass_to_client)
        return true;

    /* i#995: DR may build a bb with one invalid instruction, which won't be
     * passed to cliennt.
     * FIXME: i#1000, we should present the bb to the client.
     * i#1000-c#1: the bb->ilist could be empty.
     */
    if (instrlist_first(bb->ilist) == NULL)
        return true;
    if (!instr_opcode_valid(instrlist_first(bb->ilist)) &&
        /* For -fast_client_decode we can have level 0 instrs so check
         * to ensure this is a single-instr bb that was built just to
         * raise the fault for us.
         * XXX i#1000: shouldn't we pass this to the client?  It might not handle an
         * invalid instr properly though.
         */
        instrlist_first(bb->ilist) == instrlist_last(bb->ilist)) {
        return true;
    }

    /* DrMem#1735: pass app pc, not selfmod copy pc */
    app_pc tag = bb->pretend_pc == NULL ? bb->start_pc : bb->pretend_pc;

#ifdef LINUX
    if (TEST(FRAG_STARTS_RSEQ_REGION, bb->flags)) {
        rseq_insert_start_label(dcontext, tag, bb->ilist);
        /* This is a temporary flag, as it overlaps with another used later. */
        bb->flags &= ~FRAG_STARTS_RSEQ_REGION;
    }
#endif

    /* Call the bb creation callback(s) */
    if (!instrument_basic_block(dcontext, tag, bb->ilist, bb->for_trace, !bb->app_interp,
                                &emitflags)) {
        /* although no callback was called we must process syscalls/ints (PR 307284) */
    }
    if (bb->for_cache && TEST(DR_EMIT_GO_NATIVE, emitflags)) {
        LOG(THREAD, LOG_INTERP, 2, "client requested that we go native\n");
        SYSLOG_INTERNAL_INFO("thread " TIDFMT " is going native at client request",
                             d_r_get_thread_id());
        /* we leverage the existing native_exec mechanism */
        dcontext->native_exec_postsyscall = bb->start_pc;
        dcontext->next_tag = BACK_TO_NATIVE_AFTER_SYSCALL;
        /* dynamo_thread_not_under_dynamo() will be called in dispatch_enter_native(). */
        return false;
    }

    bb->post_client = true;

    /* FIXME: instrumentor may totally mess us up -- our flags
     * or syscall info might be wrong.  xref PR 215217
     */

    /* PR 215217, PR 240265:
     * We need to check for client changes that require a new exit
     * target.  We can't practically analyze the instrlist to decipher
     * the exit, so we'll search backwards and require that the last
     * cti is the exit cti.  Typically, the last instruction in the
     * block should be the exit.  Post-mbr and post-syscall positions
     * are particularly fragile, as our mangling code sets state up for
     * the exit that could be messed up by instrs inserted after the
     * mbr/syscall.  We thus disallow such instrs (except for
     * dr_insert_mbr_instrumentation()).  xref cases 10503, 10782, 10784
     *
     * Here's what we support:
     * - more than one exit cti; all but the last must be a ubr
     * - an exit cbr or call must be the final instr in the block
     * - only one mbr; must be the final instr in the block and the exit target
     * - clients can't change the exit of blocks ending in a syscall
     *   (or int), and the syscall must be the final instr in the block;
     *   client can, however, remove the syscall and then add a different exit
     * - client can't add a translation target that's outside of the original
     *   source code bounds, or else our cache consistency breaks down
     *   (the one exception to this is that a jump can translate to its target)
     */

    /* we set to NULL to have a default of fall-through */
    bb->exit_target = NULL;
    bb->exit_type = 0;

    /* N.B.: we're walking backward */
    for (inst = instrlist_last(bb->ilist); inst != NULL; inst = instr_get_prev(inst)) {

        if (!instr_opcode_valid(inst))
            continue;

        if (instr_is_cti(inst) && inst != instrlist_last(bb->ilist)) {
            /* PR 213005: coarse_units can't handle added ctis (meta or not)
             * since decode_fragment(), used for state recreation, can't
             * distinguish from exit cti.
             * i#665: we now support intra-fragment meta ctis
             * to make persistence usable for clients
             */
            if (!opnd_is_instr(instr_get_target(inst)) || instr_is_app(inst)) {
                bb->flags &= ~FRAG_COARSE_GRAIN;
                STATS_INC(coarse_prevent_client);
            }
        }

        if (instr_is_meta(inst)) {
#ifdef ANNOTATIONS
            /* Save the trailing_annotation_pc in case a client truncated the bb there. */
            if (is_annotation_label(inst) && last_app_instr == NULL) {
                dr_instr_label_data_t *label_data = instr_get_label_data_area(inst);
                trailing_annotation_pc = GET_ANNOTATION_APP_PC(label_data);
                instrumentation_pc = GET_ANNOTATION_INSTRUMENTATION_PC(label_data);
                annotation_label = inst;
            }
#endif

            continue;
        }
#ifdef X86
        if (!d_r_is_avx512_code_in_use()) {
            if (ZMM_ENABLED()) {
                if (instr_may_write_zmm_or_opmask_register(inst)) {
                    LOG(THREAD, LOG_INTERP, 2, "Detected AVX-512 code in use\n");
                    d_r_set_avx512_code_in_use(true, NULL);
                    proc_set_num_simd_saved(MCXT_NUM_SIMD_SLOTS);
                }
            }
        }
#endif

#ifdef ANNOTATIONS
        if (instrumentation_pc != NULL && !found_instrumentation_pc &&
            instr_get_translation(inst) == instrumentation_pc)
            found_instrumentation_pc = true;
#endif

        /* in case bb was truncated, find last non-meta fall-through */
        if (last_app_instr == NULL)
            last_app_instr = inst;

        /* PR 215217: client should not add new source code regions, else our
         * cache consistency (both page prot and selfmod) will fail
         */
        ASSERT(!bb->for_cache || bb->vmlist != NULL);
        /* For selfmod recreation we don't check vmareas so we don't have vmlist.
         * We live w/o the checks there.
         */
        CLIENT_ASSERT(
            !bb->for_cache ||
                vm_list_overlaps(dcontext, bb->vmlist, instr_get_translation(inst),
                                 instr_get_translation(inst) + 1) ||
                (instr_is_ubr(inst) && opnd_is_pc(instr_get_target(inst)) &&
                 instr_get_translation(inst) == opnd_get_pc(instr_get_target(inst)))
                /* the displaced code and jmp return from intercept buffer
                 * has translation fields set to hooked app routine */
                IF_WINDOWS(|| dr_fragment_app_pc(bb->start_pc) != bb->start_pc),
            "block's app sources (instr_set_translation() targets) "
            "must remain within original bounds");

#ifdef AARCH64
        if (instr_get_opcode(inst) == OP_isb) {
            CLIENT_ASSERT(inst == instrlist_last(bb->ilist),
                          "OP_isb must be last instruction in block");
        }
#endif

        /* PR 307284: we didn't process syscalls and ints pre-client
         * so do so now to get bb->flags and bb->exit_type set
         */
        if (instr_is_syscall(inst) ||
            instr_get_opcode(inst) ==
                IF_X86_ELSE(OP_int, IF_RISCV64_ELSE(OP_ecall, OP_svc))) {
            instr_t *tmp = bb->instr;
            bb->instr = inst;
            if (instr_is_syscall(bb->instr))
                bb_process_syscall(dcontext, bb);
            else if (instr_get_opcode(bb->instr) ==
                     IF_X86_ELSE(OP_int, IF_RISCV64_ELSE(OP_ecall, OP_svc))) {
                /* non-syscall int */
                bb_process_interrupt(dcontext, bb);
            }
            if (inst != instrlist_last(bb->ilist))
                bb->instr = tmp;
        }

        /* ensure syscall/int2b terminates block */
        client_check_syscall(bb->ilist, inst, &found_syscall, &found_int);

        if (instr_will_be_exit_cti(inst)) {

            if (!found_exit_cti) {
                /* We're about to clobber the exit_type and could lose any
                 * special flags set above, even if the client doesn't change
                 * the exit target.  We undo such flags after this ilist walk
                 * to support client removal of syscalls/ints.
                 * EXIT_IS_IND_JMP_PLT() is used for -IAT_{convert,elide}, which
                 * is off by default for CI; it's also used for native_exec,
                 * but we're not sure if we want to support that with CI.
                 * xref case 10846 and i#198
                 */
                CLIENT_ASSERT(
                    !TEST(~(LINK_DIRECT | LINK_INDIRECT | LINK_CALL | LINK_RETURN |
                            LINK_JMP | LINK_NI_SYSCALL_ALL |
                            LINK_SPECIAL_EXIT IF_WINDOWS(| LINK_CALLBACK_RETURN)),
                          bb->exit_type) &&
                        !EXIT_IS_IND_JMP_PLT(bb->exit_type),
                    "client unsupported block exit type internal error");

                found_exit_cti = true;
                bb->instr = inst;

                if ((instr_is_near_ubr(inst) || instr_is_near_call_direct(inst))
                    /* conditional OP_bl needs the cbr code below */
                    IF_ARM(&&!instr_is_cbr(inst))) {
                    CLIENT_ASSERT(instr_is_near_ubr(inst) ||
                                      inst == instrlist_last(bb->ilist) ||
                                      /* for elision we assume calls are followed
                                       * by their callee target code
                                       */
                                      DYNAMO_OPTION(max_elide_call) > 0,
                                  "an exit call must terminate the block");
                    /* a ubr need not be the final instr */
                    if (inst == last_app_instr) {
                        bb->exit_target = instr_get_branch_target_pc(inst);
                        bb->exit_type = instr_branch_type(inst);
                    }
                } else if (instr_is_mbr(inst) ||
                           instr_is_far_cti(inst)
                               IF_ARM(/* mode-switch direct is treated as indirect */
                                      || instr_get_opcode(inst) == OP_blx)) {
                    CLIENT_ASSERT(inst == instrlist_last(bb->ilist),
                                  "an exit mbr or far cti must terminate the block");
                    bb->exit_type = instr_branch_type(inst);
#ifdef ARM
                    if (instr_get_opcode(inst) == OP_blx)
                        bb->ibl_branch_type = IBL_INDCALL;
                    else
#endif
                        bb->ibl_branch_type = get_ibl_branch_type(inst);
                    bb->exit_target =
                        get_ibl_routine(dcontext, get_ibl_entry_type(bb->exit_type),
                                        DEFAULT_IBL_BB(), bb->ibl_branch_type);
                } else {
                    ASSERT(instr_is_cbr(inst));
                    CLIENT_ASSERT(inst == instrlist_last(bb->ilist),
                                  "an exit cbr must terminate the block");
                    /* A null exit target specifies a cbr (see below). */
                    bb->exit_target = NULL;
                    bb->exit_type = 0;
                    instr_exit_branch_set_type(bb->instr, instr_branch_type(inst));
                }

                /* since we're walking backward, at the first exit cti
                 * we can check for post-cti code
                 */
                if (inst != instrlist_last(bb->ilist)) {
                    if (TEST(FRAG_COARSE_GRAIN, bb->flags)) {
                        /* PR 213005: coarse can't handle code beyond ctis */
                        bb->flags &= ~FRAG_COARSE_GRAIN;
                        STATS_INC(coarse_prevent_client);
                    }
                    /* decode_fragment can't handle code beyond ctis */
                    if (!instr_is_near_call_direct(inst) ||
                        DYNAMO_OPTION(max_elide_call) == 0)
                        bb->flags |= FRAG_CANNOT_BE_TRACE;
                }

            }

            /* Case 10784: Clients can confound trace building when they
             * introduce more than one exit cti; we'll just disable traces
             * for these fragments.
             */
            else {
                CLIENT_ASSERT(instr_is_near_ubr(inst) ||
                                  (instr_is_near_call_direct(inst) &&
                                   /* for elision we assume calls are followed
                                    * by their callee target code
                                    */
                                   DYNAMO_OPTION(max_elide_call) > 0),
                              "a second exit cti must be a ubr");
                if (!instr_is_near_call_direct(inst) ||
                    DYNAMO_OPTION(max_elide_call) == 0)
                    bb->flags |= FRAG_CANNOT_BE_TRACE;
                /* our cti check above should have already turned off coarse */
                ASSERT(!TEST(FRAG_COARSE_GRAIN, bb->flags));
            }
        }
    }

    /* To handle the client modifying syscall numbers we cannot inline
     * syscalls in the middle of a bb.
     */
    ASSERT(!DYNAMO_OPTION(inline_ignored_syscalls));

    ASSERT((TEST(FRAG_HAS_SYSCALL, bb->flags) && found_syscall) ||
           (!TEST(FRAG_HAS_SYSCALL, bb->flags) && !found_syscall));
    IF_WINDOWS(ASSERT(!TEST(LINK_CALLBACK_RETURN, bb->exit_type) || found_int));

    /* Note that we do NOT remove, or set, FRAG_HAS_DIRECT_CTI based on
     * client modifications: setting it for a selfmod fragment could
     * result in an infinite loop, and it is mainly used for elision, which we
     * are not doing for client ctis.  Clients are not supposed add new
     * app source regions (PR 215217).
     */

    /* Client might have truncated: re-set fall-through, accounting for annotations. */
    if (last_app_instr != NULL) {
        bool adjusted_cur_pc = false;
        app_pc xl8 = instr_get_translation(last_app_instr);
#ifdef ANNOTATIONS
        if (annotation_label != NULL) {
            if (found_instrumentation_pc) {
                /* i#1613: if the last app instruction precedes an annotation, extend the
                 * translation footprint of `bb` to include the annotation (such that
                 * the next bb starts after the annotation, avoiding duplication).
                 */
                bb->cur_pc = trailing_annotation_pc;
                adjusted_cur_pc = true;
                LOG(THREAD, LOG_INTERP, 3,
                    "BB ends immediately prior to an annotation. "
                    "Setting `bb->cur_pc` (for fall-through) to " PFX " so that the "
                    "annotation will be included.\n",
                    bb->cur_pc);
            } else {
                /* i#1613: the client removed the app instruction prior to an annotation.
                 * We infer that the client wants to skip the annotation. Remove it now.
                 */
                instr_t *annotation_next = instr_get_next(annotation_label);
                instrlist_remove(bb->ilist, annotation_label);
                instr_destroy(dcontext, annotation_label);
                if (is_annotation_return_placeholder(annotation_next)) {
                    instrlist_remove(bb->ilist, annotation_next);
                    instr_destroy(dcontext, annotation_next);
                }
            }
        }
#endif
#if defined(WINDOWS) && !defined(STANDALONE_DECODER)
        /* i#1632: if the last app instruction was taken from an intercept because it was
         * occluded by the corresponding hook, `bb->cur_pc` should point to the original
         * app pc (where that instruction was copied from). Cannot use `decode_next_pc()`
         * on the original app pc because it is now in the middle of the hook.
         */
        if (!adjusted_cur_pc && could_be_hook_occluded_pc(xl8)) {
            app_pc intercept_pc = get_intercept_pc_from_app_pc(
                xl8, true /* occlusions only */, false /* exclude start */);
            if (intercept_pc != NULL) {
                app_pc next_intercept_pc = decode_next_pc(dcontext, intercept_pc);
                bb->cur_pc = xl8 + (next_intercept_pc - intercept_pc);
                adjusted_cur_pc = true;
                LOG(THREAD, LOG_INTERP, 3,
                    "BB ends in the middle of an intercept. "
                    "Offsetting `bb->cur_pc` (for fall-through) to " PFX " in parallel "
                    "to intercept instr at " PFX "\n",
                    intercept_pc, bb->cur_pc);
            }
        }
#endif
        /* We do not take instr_length of what the client put in, but rather
         * the length of the translation target
         */
        if (!adjusted_cur_pc) {
            bb->cur_pc = decode_next_pc(dcontext, xl8);
            LOG(THREAD, LOG_INTERP, 3, "setting cur_pc (for fall-through) to " PFX "\n",
                bb->cur_pc);
        }

        /* don't set bb->instr if last instr is still syscall/int.
         * FIXME: I'm not 100% convinced the logic here covers everything
         * build_bb_ilist does.
         * FIXME: what about if last instr was invalid, or if client adds
         * some invalid instrs: xref bb_process_invalid_instr()
         */
        if (bb->instr != NULL || (!found_int && !found_syscall))
            bb->instr = last_app_instr;
    } else
        bb->instr = NULL; /* no app instrs left */

    /* PR 215217: re-scan for accurate eflags.
     * FIXME: should we not do eflags tracking while decoding, then, and always
     * do it afterward?
     */
    /* for -fast_client_decode, we don't support the client changing the app code */
    if (!INTERNAL_OPTION(fast_client_decode)) {
        bb->eflags =
            forward_eflags_analysis(dcontext, bb->ilist, instrlist_first(bb->ilist));
    }

    if (TEST(DR_EMIT_STORE_TRANSLATIONS, emitflags)) {
        /* PR 214962: let client request storage instead of recreation */
        bb->flags |= FRAG_HAS_TRANSLATION_INFO;
        /* if we didn't have record on from start, can't store translation info */
        CLIENT_ASSERT(!INTERNAL_OPTION(fast_client_decode),
                      "-fast_client_decode not compatible with "
                      "DR_EMIT_STORE_TRANSLATIONS");
        ASSERT(bb->record_translation && bb->full_decode);
    }

    if (DYNAMO_OPTION(coarse_enable_freeze)) {
        /* If we're not persisting, ignore the presence or absence of the flag
         * so we avoid undoing savings from -opt_memory with a tool that
         * doesn't support persistence.
         */
        if (!TEST(DR_EMIT_PERSISTABLE, emitflags)) {
            bb->flags &= ~FRAG_COARSE_GRAIN;
            STATS_INC(coarse_prevent_client);
        }
    }

    if (TEST(DR_EMIT_MUST_END_TRACE, emitflags)) {
        /* i#848: let client terminate traces */
        bb->flags |= FRAG_MUST_END_TRACE;
    }
    return true;
}

#ifdef DR_APP_EXPORTS
static void
mangle_pre_client(dcontext_t *dcontext, build_bb_t *bb)
{
    if (bb->start_pc == (app_pc)dr_app_running_under_dynamorio) {
        /* i#1237: set return value to be true in dr_app_running_under_dynamorio */
        instr_t *ret = instrlist_last(bb->ilist);
        instr_t *mov = instr_get_prev(ret);
        LOG(THREAD, LOG_INTERP, 3, "Found dr_app_running_under_dynamorio\n");
        ASSERT(ret != NULL && instr_is_return(ret) && mov != NULL &&
               IF_X86(instr_get_opcode(mov) == OP_mov_imm &&)
                   IF_ARM(instr_get_opcode(mov) == OP_mov &&
                          OPND_IS_IMMED_INT(instr_get_src(mov, 0)) &&)
                       IF_AARCH64(instr_get_opcode(mov) == OP_movz &&)
                           IF_RISCV64(instr_get_opcode(mov) == OP_addi &&)(
                               bb->start_pc == instr_get_raw_bits(mov) ||
                               /* the translation field might be NULL */
                               bb->start_pc == instr_get_translation(mov)));
        /* i#1998: ensure the instr is Level 3+ */
        instr_decode(dcontext, mov);
        instr_set_src(mov, 0, OPND_CREATE_INT32(1));
    }
}
#endif /* DR_APP_EXPORTS */

/* This routine is called from build_bb_ilist when the number of instructions reaches or
 * exceeds max_bb_instr.  It checks if bb is safe to stop after instruction stop_after.
 * On ARM, we do not stop bb building in the middle of an IT block unless there is a
 * conditional syscall.
 */
static bool
bb_safe_to_stop(dcontext_t *dcontext, instrlist_t *ilist, instr_t *stop_after)
{
#ifdef ARM
    ASSERT(ilist != NULL && instrlist_last(ilist) != NULL);
    /* only thumb mode could have IT blocks */
    if (dr_get_isa_mode(dcontext) != DR_ISA_ARM_THUMB)
        return true;
    if (stop_after == NULL)
        stop_after = instrlist_last_app(ilist);
    if (instr_get_opcode(stop_after) == OP_it)
        return false;
    if (!instr_is_predicated(stop_after))
        return true;
    if (instr_is_cti(stop_after) /* must be the last instr if in IT block */ ||
        /* we do not stop in the middle of an IT block unless it is a syscall */
        instr_is_syscall(stop_after) || instr_is_interrupt(stop_after))
        return true;
    return instr_is_last_in_it_block(stop_after, NULL, NULL);
#endif /* ARM */
    return true;
}

/* Interprets the application's instructions until the end of a basic
 * block is found, and prepares the resulting instrlist for creation of
 * a fragment, but does not create the fragment, just returns the instrlist.
 * Caller is responsible for freeing the list and its instrs!
 *
 * Input parameters in bb control aspects of creation:
 *   If app_interp is true, this is considered real app code.
 *   If pass_to_client is true,
 *     calls instrument routine on bb->ilist before mangling
 *   If mangle_ilist is true, mangles the ilist, else leaves it in app form
 *   If record_vmlist is true, updates the vmareas data structures
 *   If for_cache is true, bb building lock is assumed to be held.
 *     record_vmlist should also be true.
 *     Caller must set and later clear dcontext->bb_build_info.
 *     For !for_cache, build_bb_ilist() sets and clears it, making the
 *     assumption that the caller is doing no other reading from the region.
 *   If record_translation is true, records translation for inserted instrs
 *   If outf != NULL, does full disassembly with comments to outf
 *   If overlap_info != NULL, records overlap information for the block in
 *     the overlap_info (caller must fill in region_start and region_end).
 *
 * FIXME: now that we have better control over following direct ctis,
 * should we have adaptive mechanism to decided whether to follow direct
 * ctis, since some bmarks are better doing so (gap, vortex, wupwise)
 * and others are worse (apsi, perlbmk)?
 */
DISABLE_NULL_SANITIZER
static void
build_bb_ilist(dcontext_t *dcontext, build_bb_t *bb)
{
    /* Design decision: we will not try to identify branches that target
     * instructions in this basic block, when we take those branches we will
     * just make a new basic block and duplicate part of this one
     */
    int total_branches = 0;
    uint total_instrs = 0;
    /* maximum number of instructions for current basic block */
    uint cur_max_bb_instrs = DYNAMO_OPTION(max_bb_instrs);
    uint total_writes = 0;  /* only used for selfmod */
    instr_t *non_cti;       /* used if !full_decode */
    byte *non_cti_start_pc; /* used if !full_decode */
    uint eflags_6 = 0;      /* holds arith eflags written so far (in read slots) */
#ifdef HOT_PATCHING_INTERFACE
    bool hotp_should_inject = false, hotp_injected = false;
#endif
    app_pc page_start_pc = (app_pc)NULL;
    bool bb_build_nested = false;
    /* Caller will free objects allocated here so we must use the passed-in
     * dcontext for allocation; we need separate var for non-global dcontext.
     */
    dcontext_t *my_dcontext = get_thread_private_dcontext();
    DEBUG_DECLARE(bool regenerated = false;)
    bool stop_bb_on_fallthrough = false;

    ASSERT(bb->initialized);
    /* note that it's ok for bb->start_pc to be NULL as our check_new_page_start
     * will catch it
     */
    /* vmlist must start out empty (or N/A) */
    ASSERT(bb->vmlist == NULL || !bb->record_vmlist || bb->checked_start_vmarea);
    ASSERT(!bb->for_cache || bb->record_vmlist); /* for_cache assumes record_vmlist */

#ifdef CUSTOM_TRACES_RET_REMOVAL
    my_dcontext->num_calls = 0;
    my_dcontext->num_rets = 0;
#endif

    /* Support bb abort on decode fault */
    if (my_dcontext != NULL) {
        if (bb->for_cache) {
            /* Caller should have set! */
            ASSERT(bb == (build_bb_t *)my_dcontext->bb_build_info);
        } else if (my_dcontext->bb_build_info == NULL) {
            my_dcontext->bb_build_info = (void *)bb;
        } else {
            /* For nested we leave the original, which should be the only vmlist,
             * and we give up on freeing dangling instr_t and instrlist_t from this
             * decode.
             * We need the original's for_cache so we know to free the bb_building_lock.
             * FIXME: use TRY to handle decode exceptions locally?  Shouldn't have
             * violation remediations on a !for_cache build.
             */
            ASSERT(bb->vmlist == NULL && !bb->for_cache &&
                   ((build_bb_t *)my_dcontext->bb_build_info)->for_cache);
            /* FIXME: add nested as a field so we can have stat on nested faults */
            bb_build_nested = true;
        }
    } else
        ASSERT(dynamo_exited);

    if ((bb->record_translation && !INTERNAL_OPTION(fast_client_decode)) ||
        !bb->for_cache
             /* to split riprel, need to decode every instr */
             /* in x86_to_x64, need to translate every x86 instr */
             IF_X64(|| DYNAMO_OPTION(coarse_split_riprel) || DYNAMO_OPTION(x86_to_x64)) ||
        INTERNAL_OPTION(full_decode)
        /* We separate rseq regions into their own blocks to make this check easier. */
        IF_LINUX(||
                 (!vmvector_empty(d_r_rseq_areas) &&
                  vmvector_overlap(d_r_rseq_areas, bb->start_pc, bb->start_pc + 1))))
        bb->full_decode = true;
    else {
#ifdef CHECK_RETURNS_SSE2
        bb->full_decode = true;
#endif
    }

    LOG(THREAD, LOG_INTERP, 3,
        "\ninterp%s: ", IF_X86_64_ELSE(X64_MODE_DC(dcontext) ? "" : " (x86 mode)", ""));
    BBPRINT(bb, 3, "start_pc = " PFX "\n", bb->start_pc);

    DOSTATS({
        if (bb->app_interp) {
            if (fragment_lookup_deleted(dcontext, bb->start_pc)) {
                /* this will look up private 1st, so yes we will get
                 * dup stats if multiple threads have regnerated the
                 * same private tag, or if a shared tag is deleted and
                 * multiple privates created
                 */
                regenerated = true;
                STATS_INC(num_fragments_deja_vu);
            }
        }
    });

    /* start converting instructions into IR */
    if (!bb->checked_start_vmarea)
        check_new_page_start(dcontext, bb);

#if defined(WINDOWS) && !defined(STANDALONE_DECODER)
    /* i#1632: if `bb->start_pc` points into the middle of a DR intercept hook, change
     * it so instructions are taken from the intercept instead (note that
     * `instr_set_translation` will hide this adjustment from the client). N.B.: this
     * must follow `check_new_page_start()` (above) or `bb.vmlist` will be wrong.
     */
    if (could_be_hook_occluded_pc(bb->start_pc)) {
        app_pc intercept_pc = get_intercept_pc_from_app_pc(
            bb->start_pc, true /* occlusions only */, true /* exclude start pc */);
        if (intercept_pc != NULL) {
            LOG(THREAD, LOG_INTERP, 3,
                "Changing start_pc from hook-occluded app pc " PFX " to intercept pc " PFX
                "\n",
                bb->start_pc, intercept_pc);
            bb->start_pc = intercept_pc;
        }
    }
#endif

    bb->cur_pc = bb->start_pc;

    /* for translation in case we break out of loop before decoding any
     * instructions, (i.e. check_for_stopping_point()) */
    bb->instr_start = bb->cur_pc;

    /* create instrlist after check_new_page_start to avoid memory leak
     * on unreadable memory -- though we now properly clean up and won't leak
     * on unreadable on any check_thread_vm_area call
     */
    bb->ilist = instrlist_create(dcontext);
    bb->instr = NULL;

    /* avoid discrepancy in finding invalid instructions between fast decode
     * and the full decode of sandboxing by doing full decode up front
     */
    if (TEST(FRAG_SELFMOD_SANDBOXED, bb->flags)) {
        bb->full_decode = true;
        bb->follow_direct = false;
    }
    if (TEST(FRAG_HAS_TRANSLATION_INFO, bb->flags)) {
        bb->full_decode = true;
        bb->record_translation = true;
    }
    if (my_dcontext != NULL && my_dcontext->single_step_addr == bb->start_pc) {
        /* Decodes only one instruction because of single step exception. */
        cur_max_bb_instrs = 1;
    }

    KSTART(bb_decoding);
    while (true) {
        if (check_for_stopping_point(dcontext, bb)) {
            BBPRINT(bb, 3, "interp: found DynamoRIO stopping point at " PFX "\n",
                    bb->cur_pc);
            break;
        }

        /* fill in a new instr structure and update bb->cur_pc */
        bb->instr = instr_create(dcontext);
        /* if !full_decode:
         * All we need to decode are control-transfer instructions
         * For efficiency, put all non-cti into a single instr_t structure
         */
        non_cti_start_pc = bb->cur_pc;
        do {
            /* If the thread's vmareas aren't being added to, indicate the
             * page that's being decoded. */
            if (!bb->record_vmlist && page_start_pc != (app_pc)PAGE_START(bb->cur_pc)) {
                page_start_pc = (app_pc)PAGE_START(bb->cur_pc);
                set_thread_decode_page_start(my_dcontext == NULL ? dcontext : my_dcontext,
                                             page_start_pc);
            }

            bb->instr_start = bb->cur_pc;
            if (bb->full_decode) {
                /* only going through this do loop once! */
                bb->cur_pc = IF_AARCH64_ELSE(decode_with_ldstex,
                                             decode)(dcontext, bb->cur_pc, bb->instr);
                if (bb->record_translation)
                    instr_set_translation(bb->instr, bb->instr_start);
            } else {
                /* must reset, may go through loop multiple times */
                instr_reset(dcontext, bb->instr);
                bb->cur_pc = IF_AARCH64_ELSE(decode_cti_with_ldstex,
                                             decode_cti)(dcontext, bb->cur_pc, bb->instr);

#if defined(ANNOTATIONS) && !(defined(X64) && defined(WINDOWS))
                /* Quickly check whether this may be a Valgrind annotation. */
                if (is_encoded_valgrind_annotation_tail(bb->instr_start)) {
                    /* Might be an annotation, so try the (slower) full check. */
                    if (is_encoded_valgrind_annotation(bb->instr_start, bb->start_pc,
                                                       (app_pc)PAGE_START(bb->cur_pc))) {
                        /* Valgrind annotation needs full decode; clean up and repeat. */
                        KSTOP(bb_decoding);
                        instr_destroy(dcontext, bb->instr);
                        instrlist_clear_and_destroy(dcontext, bb->ilist);
                        if (bb->vmlist != NULL) {
                            vm_area_destroy_list(dcontext, bb->vmlist);
                            bb->vmlist = NULL;
                        }
                        bb->full_decode = true;
                        build_bb_ilist(dcontext, bb);
                        return;
                    }
                }
#endif
            }

            ASSERT(!bb->check_vm_area || bb->checked_end != NULL);
            if (bb->check_vm_area && bb->cur_pc != NULL &&
                bb->cur_pc - 1 >= bb->checked_end) {
                /* We're beyond the vmarea allowed -- so check again.
                 * Ideally we'd want to check BEFORE we decode from the
                 * subsequent page, as it could be inaccessible, but not worth
                 * the time estimating the size from a variable number of bytes
                 * before the page boundary.  Instead we rely on other
                 * mechanisms to handle faults while decoding, which we need
                 * anyway to handle racy unmaps by the app.
                 */
                uint old_flags = bb->flags;
                DEBUG_DECLARE(bool is_first_instr = (bb->instr_start == bb->start_pc));
                if (!check_new_page_contig(dcontext, bb, bb->cur_pc - 1)) {
                    /* i#989: Stop bb building before falling through to an
                     * incompatible vmarea.
                     */
                    ASSERT(!is_first_instr);
                    bb->cur_pc = NULL;
                    stop_bb_on_fallthrough = true;
                    break;
                }
                if (!TEST(FRAG_SELFMOD_SANDBOXED, old_flags) &&
                    TEST(FRAG_SELFMOD_SANDBOXED, bb->flags)) {
                    /* Restart the decode loop with full_decode and
                     * !follow_direct, which are needed for sandboxing.  This
                     * can't happen more than once because sandboxing is now on.
                     */
                    ASSERT(is_first_instr);
                    bb->full_decode = true;
                    bb->follow_direct = false;
                    bb->cur_pc = bb->instr_start;
                    instr_reset(dcontext, bb->instr);
                    continue;
                }
            }

            total_instrs++;
            DOELOG(3, LOG_INTERP,
                   { disassemble_with_bytes(dcontext, bb->instr_start, THREAD); });

            if (bb->outf != INVALID_FILE)
                disassemble_with_bytes(dcontext, bb->instr_start, bb->outf);

            if (!instr_valid(bb->instr))
                break; /* before eflags analysis! */

#ifdef X86
            /* If the next instruction at bb->cur_pc fires a debug register,
             * then we should stop this basic block before getting to it.
             */
            if (my_dcontext != NULL && debug_register_fire_on_addr(bb->instr_start)) {
                stop_bb_on_fallthrough = true;
                break;
            }
            if (!d_r_is_avx512_code_in_use()) {
                if (ZMM_ENABLED()) {
                    if (instr_get_prefix_flag(bb->instr, PREFIX_EVEX)) {
                        /* For AVX-512 detection in bb builder, we're checking only
                         * for the prefix flag, which for example can be set by
                         * decode_cti. In client_process_bb, post-client instructions
                         * are checked with instr_may_write_zmm_register.
                         */
                        LOG(THREAD, LOG_INTERP, 2, "Detected AVX-512 code in use\n");
                        d_r_set_avx512_code_in_use(true, instr_get_app_pc(bb->instr));
                        proc_set_num_simd_saved(MCXT_NUM_SIMD_SLOTS);
                    }
                }
            }
#endif
            /* Eflags analysis:
             * We do this even if -unsafe_ignore_eflags_prefix b/c it doesn't cost that
             * much and we can use the analysis to detect any bb that reads a flag
             * prior to writing it.
             */
            if (bb->eflags != EFLAGS_WRITE_ARITH IF_X86(&&bb->eflags != EFLAGS_READ_OF))
                bb->eflags = eflags_analysis(bb->instr, bb->eflags, &eflags_6);

                /* stop decoding at an invalid instr (tested above) or a cti
                 *(== opcode valid) or a possible SEH frame push (if
                 * -process_SEH_push). */
#ifdef WINDOWS
            if (DYNAMO_OPTION(process_SEH_push) &&
                instr_get_prefix_flag(bb->instr, PREFIX_SEG_FS)) {
                STATS_INC(num_bb_build_fs);
                break;
            }
#endif

#ifdef X64
            if (instr_has_rel_addr_reference(bb->instr)) {
                /* PR 215397: we need to split these out for re-relativization */
                break;
            }
#endif
#if defined(UNIX) && defined(X86)
            if (INTERNAL_OPTION(mangle_app_seg) &&
                instr_get_prefix_flag(bb->instr, PREFIX_SEG_FS | PREFIX_SEG_GS)) {
                /* These segment prefix flags are not persistent and are
                 * only used as hints just after decoding.
                 * They are not accurate later and can be misleading.
                 * This can only be used right after decoding for quick check,
                 * and a walk of operands should be performed to look for
                 * actual far mem refs.
                 */
                /* i#107, mangle reference with segment register */
                /* we up-decode the instr when !full_decode to make sure it will
                 * pass the instr_opcode_valid check in mangle and be mangled.
                 */
                instr_get_opcode(bb->instr);
                break;
            }
#endif
            /* i#107, opcode mov_seg will be set in decode_cti,
             * so instr_opcode_valid(bb->instr) is true, and terminates the loop.
             */
        } while (!instr_opcode_valid(bb->instr) && total_instrs <= cur_max_bb_instrs);

        if (bb->cur_pc == NULL) {
            /* invalid instr or vmarea change: reset bb->cur_pc, will end bb
             * after updating stats
             */
            bb->cur_pc = bb->instr_start;
        }

        /* We need the translation when mangling calls and jecxz/loop*.
         * May as well set it for all cti's since there's
         * really no extra overhead in doing so.  Note that we go
         * through the above loop only once for cti's, so it's safe
         * to set the translation here.
         */
        if (instr_opcode_valid(bb->instr) &&
            (instr_is_cti(bb->instr) || bb->record_translation))
            instr_set_translation(bb->instr, bb->instr_start);

#ifdef HOT_PATCHING_INTERFACE
        /* If this lookup succeeds then the current bb needs to be patched.
         * In hotp_inject(), address lookup will be done for each instruction
         * pc in this bb and patching will be done if an exact match is found.
         *
         * Hot patching should be done only for app interp and recreating
         * pc, not for reproducing app code.  Hence we use mangle_ilist.
         * See case 5981.
         *
         * FIXME: this lookup can further be reduced by determining whether or
         *        not the current bb's module needs patching via check_new_page*
         */
        if (DYNAMO_OPTION(hot_patching) && bb->mangle_ilist && !hotp_should_inject) {
            /* case 8780: we may hold the lock; FIXME: figure out if this can
             * be avoided - messy to hold hotp_vul_table lock like this for
             * unnecessary operations. */
            bool owns_hotp_lock = self_owns_write_lock(hotp_get_lock());
            if (hotp_does_region_need_patch(non_cti_start_pc, bb->cur_pc,
                                            owns_hotp_lock)) {
                BBPRINT(bb, 2, "hotpatch match in " PFX ": " PFX "-" PFX "\n",
                        bb->start_pc, non_cti_start_pc, bb->cur_pc);
                hotp_should_inject = true;
                /* Don't elide if we are going to hot patch this bb because
                 * the patch point can be a direct cti; eliding would result
                 * in the patch not being applied.  See case 5901.
                 * FIXME: we could make this more efficient by only turning
                 * off follow_direct if the instr is direct cti.
                 */
                bb->follow_direct = false;
                DOSTATS({
                    if (TEST(FRAG_HAS_DIRECT_CTI, bb->flags))
                        STATS_INC(hotp_num_frag_direct_cti);
                });
            }
        }
#endif

        if (bb->full_decode) {
            if (TEST(FRAG_SELFMOD_SANDBOXED, bb->flags) && instr_valid(bb->instr) &&
                instr_writes_memory(bb->instr)) {
                /* to allow tailing non-writes, end prior to the write beyond the max */
                total_writes++;
                if (total_writes > DYNAMO_OPTION(selfmod_max_writes)) {
                    BBPRINT(bb, 3, "reached selfmod write limit %d, stopping\n",
                            DYNAMO_OPTION(selfmod_max_writes));
                    STATS_INC(num_max_selfmod_writes_enforced);
                    bb_stop_prior_to_instr(dcontext, bb,
                                           false /*not added to bb->ilist*/);
                    break;
                }
            }
        } else if (bb->instr_start != non_cti_start_pc) {
            /* instr now holds the cti, so create an instr_t for the non-cti */
            non_cti = instr_create(dcontext);
            IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(bb->instr_start - non_cti_start_pc)));
            instr_set_raw_bits(non_cti, non_cti_start_pc,
                               (uint)(bb->instr_start - non_cti_start_pc));
            if (bb->record_translation)
                instr_set_translation(non_cti, non_cti_start_pc);
            /* add non-cti instructions to instruction list */
            instrlist_append(bb->ilist, non_cti);
        }

        DOSTATS({
            /* This routine is also called for recreating state, we only want
             * to count app code when we build new bbs, which is indicated by
             * the bb->app_interp parameter
             */
            if (bb->app_interp && !regenerated) {
                /* avoid double-counting for adaptive working set */
                /* FIXME - ubr ellision leads to double couting.  We also
                 * double count when we have multiple entry points into the
                 * same block of cti free instructinos. */
                STATS_ADD(app_code_seen, (bb->cur_pc - non_cti_start_pc));
                LOG(THREAD, LOG_INTERP, 5, "adding %d bytes to total app code seen\n",
                    bb->cur_pc - non_cti_start_pc);
            }
        });

        if (!instr_valid(bb->instr)) {
            bb_process_invalid_instr(dcontext, bb);
            break;
        }

        if (stop_bb_on_fallthrough) {
            bb_stop_prior_to_instr(dcontext, bb, false /*not appended*/);
            break;
        }

#ifdef ANNOTATIONS
#    if !(defined(X64) && defined(WINDOWS))
        /* Quickly check whether this may be a Valgrind annotation. */
        if (is_decoded_valgrind_annotation_tail(bb->instr)) {
            /* Might be an annotation, so try the (slower) full check. */
            if (is_encoded_valgrind_annotation(bb->instr_start, bb->start_pc,
                                               (app_pc)PAGE_START(bb->cur_pc))) {
                instrument_valgrind_annotation(dcontext, bb->ilist, bb->instr,
                                               bb->instr_start, bb->cur_pc, total_instrs);
                continue;
            }
        } else /* Top-level annotation recognition is unambiguous (xchg vs. jmp). */
#    endif
            if (is_annotation_jump_over_dead_code(bb->instr)) {
            instr_t *substitution = NULL;
            if (instrument_annotation(
                    dcontext, &bb->cur_pc,
                    &substitution _IF_WINDOWS_X64(bb->cur_pc < bb->checked_end))) {
                instr_destroy(dcontext, bb->instr);
                if (substitution == NULL)
                    continue; /* ignore annotation if no handlers are registered */
                else
                    bb->instr = substitution;
            }
        }
#endif

#ifdef WINDOWS
        if (DYNAMO_OPTION(process_SEH_push) &&
            instr_get_prefix_flag(bb->instr, PREFIX_SEG_FS)) {
            DEBUG_DECLARE(ssize_t dbl_count = bb->cur_pc - bb->instr_start);
            if (!bb_process_fs_ref(dcontext, bb)) {
                DOSTATS({
                    if (bb->app_interp) {
                        LOG(THREAD, LOG_INTERP, 3,
                            "stopping bb at fs-using instr @ " PFX "\n", bb->instr_start);
                        STATS_INC(num_process_SEH_bb_early_terminate);
                        /* don't double count the fs instruction itself
                         * since we removed it from this bb */
                        if (!regenerated)
                            STATS_ADD(app_code_seen, -dbl_count);
                    }
                });
                break;
            }
        }
#else
#    if defined(X86) && defined(LINUX)
        if (instr_get_prefix_flag(bb->instr,
                                  (SEG_TLS == SEG_GS) ? PREFIX_SEG_GS : PREFIX_SEG_FS)
            /* __errno_location is interpreted when global, though it's hidden in TOT */
            IF_UNIX(&&!is_in_dynamo_dll(bb->instr_start)) &&
            /* i#107 allows DR/APP using the same segment register. */
            !INTERNAL_OPTION(mangle_app_seg)) {
            CLIENT_ASSERT(false,
                          "no support for app using DR's segment w/o -mangle_app_seg");
            ASSERT_BUG_NUM(205276, false);
        }
#    endif /* X86 */
#endif     /* WINDOWS */

        if (my_dcontext != NULL && my_dcontext->single_step_addr == bb->instr_start) {
            bb_process_single_step(dcontext, bb);
            /* Stops basic block right now. */
            break;
        }

        /* far direct is treated as indirect (i#823) */
        if (instr_is_near_ubr(bb->instr)) {
            if (bb_process_ubr(dcontext, bb))
                continue;
            else {
                if (bb->instr != NULL) /* else, bb_process_ubr() set exit_type */
                    bb->exit_type |= instr_branch_type(bb->instr);
                break;
            }
        } else
            instrlist_append(bb->ilist, bb->instr);

#ifdef RETURN_AFTER_CALL
        if (bb->app_interp && dynamo_options.ret_after_call) {
            if (instr_is_call(bb->instr)) {
                /* add after call instruction to valid return targets */
                add_return_target(dcontext, bb->instr_start, bb->instr);
            }
        }
#endif /* RETURN_AFTER_CALL */

#ifdef X64
        /* must be prior to mbr check since mbr location could be rip-rel */
        if (DYNAMO_OPTION(coarse_split_riprel) && DYNAMO_OPTION(coarse_units) &&
            TEST(FRAG_COARSE_GRAIN, bb->flags) &&
            instr_has_rel_addr_reference(bb->instr)) {
            if (instrlist_first(bb->ilist) != bb->instr) {
                /* have ref be in its own bb */
                bb_stop_prior_to_instr(dcontext, bb, true /*appended already*/);
                break; /* stop bb */
            } else {
                /* single-instr fine-grained bb */
                bb->flags &= ~FRAG_COARSE_GRAIN;
                STATS_INC(coarse_prevent_riprel);
            }
        }
#endif

        if (instr_is_near_call_direct(bb->instr)) {
            if (!bb_process_call_direct(dcontext, bb)) {
                if (bb->instr != NULL)
                    bb->exit_type |= instr_branch_type(bb->instr);
                break;
            }
        } else if (instr_is_mbr(bb->instr) /* including indirect calls */
                   IF_X86(                 /* far direct is treated as indirect (i#823) */
                          || instr_get_opcode(bb->instr) == OP_jmp_far ||
                          instr_get_opcode(bb->instr) == OP_call_far)
                       IF_ARM(/* mode-switch direct is treated as indirect */
                              || instr_get_opcode(bb->instr) == OP_blx)) {

            /* Manage the case where we don't need to perform 'normal'
             * indirect branch processing.
             */
            bool normal_indirect_processing = true;
            bool elide_and_continue_if_converted = true;

            if (instr_is_return(bb->instr)) {
                bb->ibl_branch_type = IBL_RETURN;
                STATS_INC(num_returns);
            } else if (instr_is_call_indirect(bb->instr)) {
                STATS_INC(num_all_calls);
                STATS_INC(num_indirect_calls);

                if (DYNAMO_OPTION(coarse_split_calls) && DYNAMO_OPTION(coarse_units) &&
                    TEST(FRAG_COARSE_GRAIN, bb->flags)) {
                    if (instrlist_first(bb->ilist) != bb->instr) {
                        /* have call be in its own bb */
                        bb_stop_prior_to_instr(dcontext, bb, true /*appended already*/);
                        break; /* stop bb */
                    } else {
                        /* single-call fine-grained bb */
                        bb->flags &= ~FRAG_COARSE_GRAIN;
                        STATS_INC(coarse_prevent_cti);
                    }
                }

                /* If the indirect call can be converted into a direct one,
                 * bypass normal indirect call processing.
                 * First, check for a call* that we treat as a syscall.
                 */
                if (bb_process_indcall_syscall(dcontext, bb,
                                               &elide_and_continue_if_converted)) {
                    normal_indirect_processing = false;
                } else if (DYNAMO_OPTION(indcall2direct) &&
                           bb_process_convertible_indcall(dcontext, bb)) {
                    normal_indirect_processing = false;
                    elide_and_continue_if_converted = true;
                } else if (DYNAMO_OPTION(IAT_convert) &&
                           bb_process_IAT_convertible_indcall(
                               dcontext, bb, &elide_and_continue_if_converted)) {
                    normal_indirect_processing = false;
                } else
                    bb->ibl_branch_type = IBL_INDCALL;
#ifdef X86
            } else if (instr_get_opcode(bb->instr) == OP_jmp_far) {
                /* far direct is treated as indirect (i#823) */
                bb->ibl_branch_type = IBL_INDJMP;
            } else if (instr_get_opcode(bb->instr) == OP_call_far) {
                /* far direct is treated as indirect (i#823) */
                bb->ibl_branch_type = IBL_INDCALL;
#elif defined(ARM)
            } else if (instr_get_opcode(bb->instr) == OP_blx) {
                /* mode-changing direct call is treated as indirect */
                bb->ibl_branch_type = IBL_INDCALL;
#endif /* X86 */
            } else {
                /* indirect jump */
                /* was prev instr a direct call? if so, this is a PLT-style ind call */
                instr_t *prev = instr_get_prev(bb->instr);
                if (prev != NULL && instr_opcode_valid(prev) &&
                    instr_is_call_direct(prev)) {
                    bb->exit_type |= INSTR_IND_JMP_PLT_EXIT;
                    /* just because we have a CALL to JMP* makes it
                       only a _likely_ PLT call, we still have to make
                       sure it goes through IAT - see case 4269
                    */
                    STATS_INC(num_indirect_jumps_likely_PLT);
                }

                elide_and_continue_if_converted = true;

                if (DYNAMO_OPTION(IAT_convert) &&
                    bb_process_IAT_convertible_indjmp(dcontext, bb,
                                                      &elide_and_continue_if_converted)) {
                    /* Clear the IND_JMP_PLT_EXIT flag since we've converted
                     * the PLT to a direct transition (and possibly elided).
                     * Xref case 7867 for why leaving this flag in the eliding
                     * case can cause later failures. */
                    bb->exit_type &= ~INSTR_CALL_EXIT; /* leave just JMP */
                    normal_indirect_processing = false;
                } else /* FIXME: this can always be set */
                    bb->ibl_branch_type = IBL_INDJMP;
                STATS_INC(num_indirect_jumps);
            }
#ifdef CUSTOM_TRACES_RET_REMOVAL
            if (instr_is_return(bb->instr))
                my_dcontext->num_rets++;
            else if (instr_is_call_indirect(bb->instr))
                my_dcontext->num_calls++;
#endif
            /* set exit type since this instruction will get mangled */
            if (normal_indirect_processing) {
                bb->exit_type |= instr_branch_type(bb->instr);
                bb->exit_target =
                    get_ibl_routine(dcontext, get_ibl_entry_type(bb->exit_type),
                                    DEFAULT_IBL_BB(), bb->ibl_branch_type);
                LOG(THREAD, LOG_INTERP, 4, "mbr exit target = " PFX "\n",
                    bb->exit_target);
                break;
            } else {
                /* decide whether to stop bb here */
                if (!elide_and_continue_if_converted)
                    break;
                /* fall through for -max_bb_instrs check */
            }
        } else if (instr_is_cti(bb->instr) &&
                   (!instr_is_call(bb->instr) || instr_is_cbr(bb->instr))) {
            total_branches++;
            if (total_branches >= BRANCH_LIMIT) {
                /* set type of 1st exit cti for cbr (bb->exit_type is for fall-through) */
                instr_exit_branch_set_type(bb->instr, instr_branch_type(bb->instr));
                break;
            }
        } else if (instr_is_syscall(bb->instr)) {
            if (!bb_process_syscall(dcontext, bb))
                break;
        } /* end syscall */
        else if (instr_get_opcode(bb->instr) ==
                 IF_X86_ELSE(OP_int, IF_RISCV64_ELSE(OP_ecall, OP_svc))) {
            /* non-syscall int */
            if (!bb_process_interrupt(dcontext, bb))
                break;
        }
#ifdef AARCH64
        /* OP_isb, when mangled, has a potential side exit. */
        else if (instr_get_opcode(bb->instr) == OP_isb)
            break;
#endif
#if 0 /*i#1313, i#1314*/
        else if (instr_get_opcode(bb->instr) == OP_getsec) {
            /* XXX i#1313: if we support CPL0 in the future we'll need to
             * dynamically handle the leaf functions here, which can change eip
             * and other state.  We'll need OP_getsec in decode_cti().
             */
        }
        else if (instr_get_opcode(bb->instr) == OP_xend ||
                 instr_get_opcode(bb->instr) == OP_xabort) {
            /* XXX i#1314: support OP_xend failing and setting eip to the
             * fallback pc recorded by OP_xbegin.  We'll need both in decode_cti().
             */
        }
#endif
#ifdef CHECK_RETURNS_SSE2
        /* There are SSE and SSE2 instrs that operate on MMX instead of XMM, but
         * we perform a simple coarse-grain check here.
         */
        else if (instr_is_sse_or_sse2(bb->instr)) {
            FATAL_USAGE_ERROR(CHECK_RETURNS_SSE2_XMM_USED, 2, get_application_name(),
                              get_application_pid());
        }
#endif
#if defined(UNIX) && !defined(DGC_DIAGNOSTICS) && defined(X86)
        else if (instr_get_opcode(bb->instr) == OP_mov_seg) {
            if (!bb_process_mov_seg(dcontext, bb))
                break;
        }
#endif
        else if (instr_saves_float_pc(bb->instr)) {
            bb_process_float_pc(dcontext, bb);
            break;
        }

        if (bb->cur_pc == bb->stop_pc) {
            /* We only check stop_pc for full_decode, so not in inner loop. */
            BBPRINT(bb, 3, "reached end pc " PFX ", stopping\n", bb->stop_pc);
            break;
        }
        if (total_instrs > DYNAMO_OPTION(max_bb_instrs)) {
            /* this could be an enormous basic block, or it could
             * be some degenerate infinite-loop case like a call
             * to a function that calls exit() and then calls itself,
             * so just end it here, we'll pick up where we left off
             * if it's legit
             */
            BBPRINT(bb, 3, "reached -max_bb_instrs(%d): %d, ",
                    DYNAMO_OPTION(max_bb_instrs), total_instrs);
            if (bb_safe_to_stop(dcontext, bb->ilist, NULL)) {
                BBPRINT(bb, 3, "stopping\n");
                STATS_INC(num_max_bb_instrs_enforced);
                break;
            } else {
                /* XXX i#1669: cannot stop bb now, what's the best way to handle?
                 * We can either roll-back and find previous safe stop point, or
                 * simply extend the bb with a few more instructions.
                 * We can always lower the -max_bb_instrs to offset the additional
                 * instructions.  In contrast, roll-back seems complex and
                 * potentially problematic.
                 */
                BBPRINT(bb, 3, "cannot stop, continuing\n");
            }
        }

    } /* end of while (true) */
    KSTOP(bb_decoding);

#ifdef DEBUG_MEMORY
    /* make sure anyone who destroyed also set to NULL */
    ASSERT(bb->instr == NULL ||
           (bb->instr->bytes != (byte *)HEAP_UNALLOCATED_PTR_UINT &&
            bb->instr->bytes != (byte *)HEAP_ALLOCATED_PTR_UINT &&
            bb->instr->bytes != (byte *)HEAP_PAD_PTR_UINT));
#endif

    if (!check_new_page_contig(dcontext, bb, bb->cur_pc - 1)) {
        ASSERT(false && "Should have checked cur_pc-1 in decode loop");
    }
    bb->end_pc = bb->cur_pc;
    BBPRINT(bb, 3, "end_pc = " PFX "\n\n", bb->end_pc);

#ifdef LINUX
    if (TEST(FRAG_HAS_RSEQ_ENDPOINT, bb->flags)) {
        instr_t *label = INSTR_CREATE_label(dcontext);
        instr_set_note(label, (void *)DR_NOTE_REG_BARRIER);
        /* We want the label after the final rseq instruction.  We've
         * truncated the block after that instruction so bb->instr may
         * be NULL so we append.
         */
        instrlist_meta_append(bb->ilist, label);
    }
#endif

    /* We could put this in check_new_page_jmp where it already checks
     * for native_exec overlap, but selfmod ubrs don't even call that routine
     */
    if (DYNAMO_OPTION(native_exec) && DYNAMO_OPTION(native_exec_callcall) &&
        !vmvector_empty(native_exec_areas) && bb->app_interp && bb->instr != NULL &&
        (instr_is_near_ubr(bb->instr) || instr_is_near_call_direct(bb->instr)) &&
        instrlist_first(bb->ilist) == instrlist_last(bb->ilist)) {
        /* Case 4564/3558: handle .NET COM method table where a call* targets
         * a call to a native_exec dll -- we need to put the gateway at the
         * call* to avoid retaddr mangling of the method table call.
         * As a side effect we can also handle call*, jmp.
         * We don't actually verify or care that it was specifically a call*,
         * whatever at_native_exec_gateway() requires to assure itself that we're
         * at a return-address-clobberable point.
         */
        app_pc tgt = opnd_get_pc(instr_get_target(bb->instr));
        if (is_native_pc(tgt) &&
            at_native_exec_gateway(dcontext, tgt,
                                   &bb->native_call _IF_DEBUG(true /*xfer tgt*/))) {
            /* replace this ilist w/ a native exec one */
            LOG(THREAD, LOG_INTERP, 2,
                "direct xfer @gateway @" PFX " to native_exec module " PFX "\n",
                bb->start_pc, tgt);
            bb->native_exec = true;
            /* add this ubr/call to the native_exec_list, both as an optimization
             * for future entrances and b/c .NET changes its method table call
             * from targeting a native_exec image to instead target DGC directly,
             * thwarting our gateway!
             * FIXME: if heap region de-allocated, we'll remove, but what if re-used
             * w/o going through syscalls?  Just written over w/ something else?
             * We'll keep it on native_exec_list...
             */
            ASSERT(bb->end_pc == bb->start_pc + DIRECT_XFER_LENGTH);
            vmvector_add(native_exec_areas, bb->start_pc, bb->end_pc, NULL);
            DODEBUG({ report_native_module(dcontext, tgt); });
            STATS_INC(num_native_module_entrances_callcall);
            return;
        }
    }
#ifdef UNIX
    /* XXX: i#1247: After a call to a native module throught plt, DR
     * loses control of the app b/c of _dl_runtime_resolve
     */
    int ret_imm;
    if (DYNAMO_OPTION(native_exec) && DYNAMO_OPTION(native_exec_opt) && bb->app_interp &&
        bb->instr != NULL && instr_is_return(bb->instr) &&
        at_dl_runtime_resolve_ret(dcontext, bb->start_pc, &ret_imm)) {
        dr_insert_clean_call(dcontext, bb->ilist, bb->instr,
                             (void *)native_module_at_runtime_resolve_ret, false, 2,
                             opnd_create_reg(REG_XSP), OPND_CREATE_INT32(ret_imm));
    }
#endif

    STATS_TRACK_MAX(max_instrs_in_a_bb, total_instrs);

    if (stop_bb_on_fallthrough && TEST(FRAG_HAS_DIRECT_CTI, bb->flags)) {
        /* If we followed a direct cti to an instruction straddling a vmarea
         * boundary, we can't actually do the elision.  See the
         * sandbox_last_byte() test case in security-common/sandbox.c.  Restart
         * bb building without follow_direct.  Alternatively, we could check the
         * vmareas of the targeted instruction before performing elision.
         */
        /* FIXME: a better assert is needed because this can trigger if
         * hot patching turns off follow_direct, the current bb was elided
         * earlier and is marked as selfmod.  hotp_num_frag_direct_cti will
         * track this for now.
         */
        ASSERT(bb->follow_direct); /* else, infinite loop possible */
        BBPRINT(bb, 2,
                "*** must rebuild bb to avoid following direct cti to "
                "incompatible vmarea\n");
        STATS_INC(num_bb_end_early);
        instrlist_clear_and_destroy(dcontext, bb->ilist);
        if (bb->vmlist != NULL) {
            vm_area_destroy_list(dcontext, bb->vmlist);
            bb->vmlist = NULL;
        }
        /* Remove FRAG_HAS_DIRECT_CTI, since we're turning off follow_direct.
         * Try to keep the known flags.  We stopped the bb before merging in any
         * incompatible flags.
         */
        bb->flags &= ~FRAG_HAS_DIRECT_CTI;
        bb->follow_direct = false;
        bb->exit_type = 0;      /* i#577 */
        bb->exit_target = NULL; /* i#928 */
        /* overlap info will be reset by check_new_page_start */
        build_bb_ilist(dcontext, bb);
        return;
    }

    if (TEST(FRAG_SELFMOD_SANDBOXED, bb->flags)) {
        ASSERT(bb->full_decode);
        ASSERT(!bb->follow_direct);
        ASSERT(!TEST(FRAG_HAS_DIRECT_CTI, bb->flags));
    }

#ifdef HOT_PATCHING_INTERFACE
    /* CAUTION: This can't be moved below client interface as the basic block
     *          can be changed by the client.  This will mess up hot patching.
     *          The same is true for mangling.
     */
    if (hotp_should_inject) {
        ASSERT(DYNAMO_OPTION(hot_patching));
        hotp_injected = hotp_inject(dcontext, bb->ilist);

        /* Fix for 5272.  Hot patch injection uses dr clean call api which
         * accesses dcontext fields directly, so the injected bbs can't be
         * shared until that is changed or the clean call mechanism is replaced
         * with bb termination to execute hot patchces.
         * Case 9995 assumes that hotp fragments are fine-grained, which we
         * achieve today by being private; if we make shared we must explicitly
         * prevent from being coarse-grained.
         */
        if (hotp_injected) {
            bb->flags &= ~FRAG_SHARED;
            bb->flags |= FRAG_CANNOT_BE_TRACE;
        }
    }
#endif

    /* Until we're more confident in our decoder/encoder consistency this is
     * at the default debug build -checklevel 2.
     */
    IF_ARM(DOCHECK(2, check_encode_decode_consistency(dcontext, bb->ilist);));

#ifdef DR_APP_EXPORTS
    /* changes by DR that are visible to clients */
    mangle_pre_client(dcontext, bb);
#endif /* DR_APP_EXPORTS */

#ifdef DEBUG
    /* This is a special debugging feature */
    if (bb->for_cache && INTERNAL_OPTION(go_native_at_bb_count) > 0 &&
        debug_bb_count++ >= INTERNAL_OPTION(go_native_at_bb_count)) {
        SYSLOG_INTERNAL_INFO("thread " TIDFMT " is going native @%d bbs to " PFX,
                             d_r_get_thread_id(), debug_bb_count - 1, bb->start_pc);
        /* we leverage the existing native_exec mechanism */
        dcontext->native_exec_postsyscall = bb->start_pc;
        dcontext->next_tag = BACK_TO_NATIVE_AFTER_SYSCALL;
        dynamo_thread_not_under_dynamo(dcontext);
        IF_UNIX(os_swap_context(dcontext, true /*to app*/, DR_STATE_GO_NATIVE));
        /* i#1921: for now we do not support re-attach, so remove handlers */
        os_process_not_under_dynamorio(dcontext);
        bb_build_abort(dcontext, true /*free vmlist*/, false /*don't unlock*/);
        return;
    }
#endif
    if (!client_process_bb(dcontext, bb)) {
        bb_build_abort(dcontext, true /*free vmlist*/, false /*don't unlock*/);
        return;
    }
    /* i#620: provide API to set fall-through and retaddr targets at end of bb */
    if (instrlist_get_return_target(bb->ilist) != NULL ||
        instrlist_get_fall_through_target(bb->ilist) != NULL) {
        CLIENT_ASSERT(instr_is_cbr(instrlist_last(bb->ilist)) ||
                          instr_is_call(instrlist_last(bb->ilist)),
                      "instr_set_return_target/instr_set_fall_through_target"
                      " can only be used in a bb ending with call/cbr");
        /* the bb cannot be added to a trace */
        bb->flags |= FRAG_CANNOT_BE_TRACE;
    }
    if (bb->unmangled_ilist != NULL)
        *bb->unmangled_ilist = instrlist_clone(dcontext, bb->ilist);

    if (bb->instr != NULL && instr_opcode_valid(bb->instr) &&
        instr_is_far_cti(bb->instr)) {
        /* Simplify far_ibl (i#823) vs trace_cmp ibl as well as
         * cross-mode direct stubs varying in a trace by disallowing
         * far cti in middle of trace
         */
        bb->flags |= FRAG_MUST_END_TRACE;
        /* Simplify coarse by not requiring extra prefix stubs */
        bb->flags &= ~FRAG_COARSE_GRAIN;
    }

    /* create a final instruction that will jump to the exit stub
     * corresponding to the fall-through of the conditional branch or
     * the target of the final indirect branch (the indirect branch itself
     * will get mangled into a non-cti)
     */
    if (bb->exit_target == NULL) { /* not set by ind branch, etc. */
                                   /* fall-through pc */
        /* i#620: provide API to set fall-through target at end of bb */
        bb->exit_target = instrlist_get_fall_through_target(bb->ilist);
        if (bb->exit_target == NULL)
            bb->exit_target = (cache_pc)bb->cur_pc;
        else {
            LOG(THREAD, LOG_INTERP, 3, "set fall-throught target " PFX " by client\n",
                bb->exit_target);
        }
        if (bb->instr != NULL && instr_opcode_valid(bb->instr) &&
            instr_is_cbr(bb->instr) &&
            (int)(bb->exit_target - bb->start_pc) <= SHRT_MAX &&
            (int)(bb->exit_target - bb->start_pc) >= SHRT_MIN &&
            /* rule out jecxz, etc. */
            !instr_is_cti_loop(bb->instr))
            bb->flags |= FRAG_CBR_FALLTHROUGH_SHORT;
    }
    /* we share all basic blocks except selfmod (since want no-synch quick deletion)
     * or syscall-containing ones (to bound delay on threads exiting shared cache,
     * for cache management, both consistency and capacity)
     * bbs injected with hot patches are also not shared (see case 5272).
     */
    if (DYNAMO_OPTION(shared_bbs) && !TEST(FRAG_SELFMOD_SANDBOXED, bb->flags) &&
        !TEST(FRAG_TEMP_PRIVATE, bb->flags)
#ifdef HOT_PATCHING_INTERFACE
        && !hotp_injected
#endif
        && (my_dcontext == NULL || my_dcontext->single_step_addr != bb->instr_start)) {
        /* If the fragment doesn't have a syscall or contains a
         * non-ignorable one -- meaning that the frag will exit the cache
         * to execute the syscall -- it can be shared.
         * We don't support ignorable syscalls in shared fragments, as they
         * don't set at_syscall and so are incompatible w/ -syscalls_synch_flush.
         */
        if (!TEST(FRAG_HAS_SYSCALL, bb->flags) ||
            TESTANY(LINK_NI_SYSCALL_ALL, bb->exit_type) ||
            TEST(LINK_SPECIAL_EXIT, bb->exit_type))
            bb->flags |= FRAG_SHARED;
#ifdef WINDOWS
        /* A fragment can be shared if it contains a syscall that will be
         * executed via the version of shared syscall that can be targetted by
         * shared frags.
         */
        else if (TEST(FRAG_HAS_SYSCALL, bb->flags) &&
                 DYNAMO_OPTION(shared_fragment_shared_syscalls) &&
                 bb->exit_target == shared_syscall_routine(dcontext))
            bb->flags |= FRAG_SHARED;
        else {
            ASSERT((TEST(FRAG_HAS_SYSCALL, bb->flags) &&
                    (DYNAMO_OPTION(ignore_syscalls) ||
                     (!DYNAMO_OPTION(shared_fragment_shared_syscalls) &&
                      bb->exit_target == shared_syscall_routine(dcontext)))) &&
                   "BB not shared for unknown reason");
        }
#endif
    } else if (my_dcontext != NULL && my_dcontext->single_step_addr == bb->instr_start) {
        /* Field exit_type might have been cleared by client_process_bb. */
        bb->exit_type |= LINK_SPECIAL_EXIT;
    }

    if (TEST(FRAG_COARSE_GRAIN, bb->flags) &&
        (!TEST(FRAG_SHARED, bb->flags) ||
         /* Ignorable syscalls on linux are mangled w/ intra-fragment jmps, which
          * decode_fragment() cannot handle -- and on win32 this overlaps w/
          * FRAG_MUST_END_TRACE and LINK_NI_SYSCALL
          */
         TEST(FRAG_HAS_SYSCALL, bb->flags) || TEST(FRAG_MUST_END_TRACE, bb->flags) ||
         TEST(FRAG_CANNOT_BE_TRACE, bb->flags) ||
         TEST(FRAG_SELFMOD_SANDBOXED, bb->flags) ||
         /* PR 214142: coarse units does not support storing translations */
         TEST(FRAG_HAS_TRANSLATION_INFO, bb->flags) ||
    /* FRAG_HAS_DIRECT_CTI: we never elide (assert is below);
     * not-inlined call/jmp: we turn off FRAG_COARSE_GRAIN up above
     */
#ifdef WINDOWS
         TEST(LINK_CALLBACK_RETURN, bb->exit_type) ||
#endif
         TESTANY(LINK_NI_SYSCALL_ALL, bb->exit_type))) {
        /* Currently not supported in a coarse unit */
        STATS_INC(num_fine_in_coarse);
        DOSTATS({
            if (!TEST(FRAG_SHARED, bb->flags))
                STATS_INC(coarse_prevent_private);
            else if (TEST(FRAG_HAS_SYSCALL, bb->flags))
                STATS_INC(coarse_prevent_syscall);
            else if (TEST(FRAG_MUST_END_TRACE, bb->flags))
                STATS_INC(coarse_prevent_end_trace);
            else if (TEST(FRAG_CANNOT_BE_TRACE, bb->flags))
                STATS_INC(coarse_prevent_no_trace);
            else if (TEST(FRAG_SELFMOD_SANDBOXED, bb->flags))
                STATS_INC(coarse_prevent_selfmod);
            else if (TEST(FRAG_HAS_TRANSLATION_INFO, bb->flags))
                STATS_INC(coarse_prevent_translation);
            else if (IF_WINDOWS_ELSE_0(TEST(LINK_CALLBACK_RETURN, bb->exit_type)))
                STATS_INC(coarse_prevent_cbret);
            else if (TESTANY(LINK_NI_SYSCALL_ALL, bb->exit_type))
                STATS_INC(coarse_prevent_syscall);
            else
                ASSERT_NOT_REACHED();
        });
        bb->flags &= ~FRAG_COARSE_GRAIN;
    }
    ASSERT(!TEST(FRAG_COARSE_GRAIN, bb->flags) || !TEST(FRAG_HAS_DIRECT_CTI, bb->flags));

    /* now that we know whether shared, ensure we have the right ibl routine */
    if (!TEST(FRAG_SHARED, bb->flags) && TEST(LINK_INDIRECT, bb->exit_type)) {
        ASSERT(bb->exit_target ==
               get_ibl_routine(dcontext, get_ibl_entry_type(bb->exit_type),
                               DEFAULT_IBL_BB(), bb->ibl_branch_type));
        bb->exit_target = get_ibl_routine(dcontext, get_ibl_entry_type(bb->exit_type),
                                          IBL_BB_PRIVATE, bb->ibl_branch_type);
    }

    if (bb->mangle_ilist &&
        (bb->instr == NULL || !instr_opcode_valid(bb->instr) ||
         !instr_is_near_ubr(bb->instr) || instr_is_meta(bb->instr))) {
        instr_t *exit_instr =
            XINST_CREATE_jump(dcontext, opnd_create_pc(bb->exit_target));
        if (bb->record_translation) {
            app_pc translation = NULL;
            if (bb->instr == NULL || !instr_opcode_valid(bb->instr)) {
                /* we removed (or mangle will remove) the last instruction
                 * for special handling (invalid/syscall/int 2b) or there were
                 * no instructions added (i.e. check_stopping_point in which
                 * case instr_start == cur_pc), use last instruction's start
                 * address for the translation */
                translation = bb->instr_start;
            } else if (instr_is_cti(bb->instr)) {
                /* last instruction is a cti, consider the exit jmp part of
                 * the mangling of the cti (since we might not know the target
                 * if, for ex., its indirect) */
                translation = instr_get_translation(bb->instr);
            } else {
                /* target is the instr after the last instr in the list */
                translation = bb->cur_pc;
                ASSERT(bb->cur_pc == bb->exit_target);
            }
            ASSERT(translation != NULL);
            instr_set_translation(exit_instr, translation);
        }
        /* PR 214962: we need this jmp to be marked as "our mangling" so that
         * we won't relocate a thread there and re-do a ret pop or call push
         */
        instr_set_our_mangling(exit_instr, true);
        /* here we need to set exit_type */
        LOG(THREAD, LOG_EMIT, 3, "exit_branch_type=0x%x bb->exit_target=" PFX "\n",
            bb->exit_type, bb->exit_target);
        instr_exit_branch_set_type(exit_instr, bb->exit_type);

        instrlist_append(bb->ilist, exit_instr);
#ifdef ARM
        if (bb->svc_pred != DR_PRED_NONE) {
            /* we have a conditional syscall, add predicate to current exit */
            instr_set_predicate(exit_instr, bb->svc_pred);
            /* add another ubr exit as the fall-through */
            exit_instr = XINST_CREATE_jump(dcontext, opnd_create_pc(bb->exit_target));
            if (bb->record_translation)
                instr_set_translation(exit_instr, bb->cur_pc);
            instr_set_our_mangling(exit_instr, true);
            instr_exit_branch_set_type(exit_instr, LINK_DIRECT | LINK_JMP);
            instrlist_append(bb->ilist, exit_instr);
            /* XXX i#1734: instr svc.cc will be deleted later in mangle_syscall,
             * so we need reset encode state to avoid holding a dangling pointer.
             */
            encode_reset_it_block(dcontext);
        }
#endif
    }

    /* set flags */
#ifdef DGC_DIAGNOSTICS
    /* no traces in dyngen code, that would mess up our exit tracking */
    if (TEST(FRAG_DYNGEN, bb->flags))
        bb->flags |= FRAG_CANNOT_BE_TRACE;
#endif
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_prefix)
            IF_X64(|| !INTERNAL_OPTION(unsafe_ignore_eflags_trace))) {
        bb->flags |= instr_eflags_to_fragment_eflags(bb->eflags);
        if (TEST(FRAG_WRITES_EFLAGS_OF, bb->flags)) {
            LOG(THREAD, LOG_INTERP, 4, "fragment writes OF prior to reading it!\n");
            STATS_INC(bbs_eflags_writes_of);
        } else if (TEST(FRAG_WRITES_EFLAGS_6, bb->flags)) {
            IF_X86(ASSERT(TEST(FRAG_WRITES_EFLAGS_OF, bb->flags)));
            LOG(THREAD, LOG_INTERP, 4,
                "fragment writes all 6 flags prior to reading any\n");
            STATS_INC(bbs_eflags_writes_6);
        } else {
            DOSTATS({
                if (bb->eflags == EFLAGS_READ_ARITH) {
                    /* Reads a flag before writing any.  Won't get here if
                     * reads one flag and later writes OF, or writes OF and
                     * later reads one flag before writing that flag.
                     */
                    STATS_INC(bbs_eflags_reads);
                } else {
                    STATS_INC(bbs_eflags_writes_none);
                    if (TEST(LINK_INDIRECT, bb->exit_type))
                        STATS_INC(bbs_eflags_writes_none_ind);
                }
            });
        }
    }

    /* can only have proactive translation info if flag was set from the beginning */
    if (TEST(FRAG_HAS_TRANSLATION_INFO, bb->flags) &&
        (!bb->record_translation || !bb->full_decode))
        bb->flags &= ~FRAG_HAS_TRANSLATION_INFO;

    /* if for_cache, caller must clear once done emitting (emitting can deref
     * app memory so we wait until all done)
     */
    if (!bb_build_nested && !bb->for_cache && my_dcontext != NULL) {
        ASSERT(my_dcontext->bb_build_info == (void *)bb);
        my_dcontext->bb_build_info = NULL;
    }
    bb->instr = NULL;

    /* mangle the instruction list */

    if (!bb->mangle_ilist) {
        /* do not mangle!
         * caller must use full_decode to find invalid instrs and avoid
         * a discrepancy w/ for_cache case that aborts b/c of selfmod sandbox
         * returning false (in code below)
         */
        return;
    }

    if (!mangle_bb_ilist(dcontext, bb)) {
        /* have to rebuild bb w/ new bb flags set by mangle_bb_ilist */
        build_bb_ilist(dcontext, bb);
        return;
    }
}

/* Call when about to throw exception or other drastic action in the
 * middle of bb building, in order to free resources
 */
void
bb_build_abort(dcontext_t *dcontext, bool clean_vmarea, bool unlock)
{
    ASSERT(dcontext->bb_build_info != NULL); /* caller should check */
    if (dcontext->bb_build_info != NULL) {
        build_bb_t *bb = (build_bb_t *)dcontext->bb_build_info;
        /* free instr memory */
        if (bb->instr != NULL && bb->ilist != NULL &&
            instrlist_last(bb->ilist) != bb->instr)
            instr_destroy(dcontext, bb->instr); /* not added to bb->ilist yet */
        DODEBUG({ bb->instr = NULL; });
        if (bb->ilist != NULL) {
            instrlist_clear_and_destroy(dcontext, bb->ilist);
            DODEBUG({ bb->ilist = NULL; });
        }
        if (clean_vmarea) {
            /* Free the vmlist and any locks held (we could have been in
             * the middle of check_thread_vm_area and had a decode fault
             * during code origins checking!)
             */
            check_thread_vm_area_abort(dcontext, &bb->vmlist, bb->flags);
        } /* else we were presumably called from vmarea so caller does cleanup */
        if (unlock) {
            /* Assumption: bb building lock is held iff bb->for_cache,
             * and on a nested app bb build where !bb->for_cache we do keep the
             * original bb info in dcontext (see build_bb_ilist()).
             */
            if (bb->has_bb_building_lock) {
                ASSERT_OWN_MUTEX(USE_BB_BUILDING_LOCK(), &bb_building_lock);
                SHARED_BB_UNLOCK();
                KSTOP_REWIND(bb_building);
            } else
                ASSERT_DO_NOT_OWN_MUTEX(USE_BB_BUILDING_LOCK(), &bb_building_lock);
        }
        dcontext->bb_build_info = NULL;
    }
}

bool
expand_should_set_translation(dcontext_t *dcontext)
{
    if (dcontext->bb_build_info != NULL) {
        build_bb_t *bb = (build_bb_t *)dcontext->bb_build_info;
        /* Expanding to a higher level should set the translation to
         * the raw bytes if we're building a bb where we can assume
         * the raw byte pointer is the app pc.
         */
        return bb->record_translation;
    }
    return false;
}

/* returns false if need to rebuild bb: in that case this routine will
 * set the bb flags needed to ensure successful mangling 2nd time around
 */
static bool
mangle_bb_ilist(dcontext_t *dcontext, build_bb_t *bb)
{
#ifdef X86
    if (TEST(FRAG_SELFMOD_SANDBOXED, bb->flags)) {
        byte *selfmod_start, *selfmod_end;
        /* sandbox requires that bb have no direct cti followings!
         * check_thread_vm_area should have ensured this for us
         */
        ASSERT(!TEST(FRAG_HAS_DIRECT_CTI, bb->flags));
        LOG(THREAD, LOG_INTERP, 2,
            "fragment overlaps selfmod area, inserting sandboxing\n");
        /* only reason can't be trace is don't have mechanism set up
         * to store app code for each trace bb and update sandbox code
         * to point there
         */
        bb->flags |= FRAG_CANNOT_BE_TRACE;
        if (bb->pretend_pc != NULL) {
            selfmod_start = bb->pretend_pc;
            selfmod_end = bb->pretend_pc + (bb->cur_pc - bb->start_pc);
        } else {
            selfmod_start = bb->start_pc;
            selfmod_end = bb->cur_pc;
        }
        if (!insert_selfmod_sandbox(dcontext, bb->ilist, bb->flags, selfmod_start,
                                    selfmod_end, bb->record_translation, bb->for_cache)) {
            /* have to rebuild bb using full decode -- it has invalid instrs
             * in middle, which we don't want to deal w/ for sandboxing!
             */
            ASSERT(!bb->full_decode); /* else, how did we get here??? */
            LOG(THREAD, LOG_INTERP, 2,
                "*** must rebuild bb to avoid invalid instr in middle ***\n");
            STATS_INC(num_bb_end_early);
            instrlist_clear_and_destroy(dcontext, bb->ilist);
            if (bb->vmlist != NULL) {
                vm_area_destroy_list(dcontext, bb->vmlist);
                bb->vmlist = NULL;
            }
            bb->flags = FRAG_SELFMOD_SANDBOXED; /* lose all other flags */
            bb->full_decode = true;             /* full decode this time! */
            bb->follow_direct = false;
            bb->exit_type = 0;      /* i#577 */
            bb->exit_target = NULL; /* i#928 */
            /* overlap info will be reset by check_new_page_start */
            return false;
        }
        STATS_INC(num_sandboxed_fragments);
    }
#endif /* X86 */

    /* We make "before mangling" level 5 b/c there's not much there (just final jmp)
     * beyond "after instrumentation".
     */
    DOLOG(5, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 5, "bb ilist before mangling:\n");
        instrlist_disassemble(dcontext, bb->start_pc, bb->ilist, THREAD);
    });
    d_r_mangle(dcontext, bb->ilist, &bb->flags, true, bb->record_translation);
    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "bb ilist after mangling:\n");
        instrlist_disassemble(dcontext, bb->start_pc, bb->ilist, THREAD);
    });
    return true;
}

/* Interprets the application's instructions until the end of a basic
 * block is found, following all the rules that build_bb_ilist follows
 * with regard to terminating the block.  Does no mangling or anything of
 * the app code, though -- this routine is intended only for building the
 * original code!
 * Caller is responsible for freeing the list and its instrs!
 * If outf != INVALID_FILE, does full disassembly with comments to outf.
 */
instrlist_t *
build_app_bb_ilist(dcontext_t *dcontext, byte *start_pc, file_t outf)
{
    build_bb_t bb;
    init_build_bb(&bb, start_pc, false /*not interp*/, false /*not for cache*/,
                  false /*do not mangle*/, false /*no translation*/, outf,
                  0 /*no pre flags*/, NULL /*no overlap*/);
    build_bb_ilist(dcontext, &bb);
    return bb.ilist;
}

/* Client routine to decode instructions at an arbitrary app address,
 * following all the rules that DynamoRIO follows internally for
 * terminating basic blocks.  Note that DynamoRIO does not validate
 * that start_pc is actually the first instruction of a basic block.
 * \note Caller is reponsible for freeing the list and its instrs!
 */
instrlist_t *
decode_as_bb(void *drcontext, byte *start_pc)
{
    build_bb_t bb;

    /* Case 10009: When we hook ntdll functions, we hide the jump to
     * the interception buffer from the client BB callback.  If the
     * client asks to decode that address here, we need to decode the
     * instructions in the interception buffer instead so that we
     * again hide our hooking.
     * We will have the jmp from the buffer back to after the hooked
     * app code visible to the client (just like it is for the
     * real bb built there, so at least we're consistent).
     */
#ifdef WINDOWS
    byte *real_pc;
    if (is_intercepted_app_pc((app_pc)start_pc, &real_pc))
        start_pc = real_pc;
#endif

    init_build_bb(&bb, start_pc, false /*not interp*/, false /*not for cache*/,
                  false /*do not mangle*/,
                  true, /* translation; xref case 10070 where this
                         * currently turns on full decode; today we
                         * provide no way to turn that off, as IR
                         * expansion routines are not exported (PR 200409). */
                  INVALID_FILE, 0 /*no pre flags*/, NULL /*no overlap*/);
    build_bb_ilist((dcontext_t *)drcontext, &bb);
    return bb.ilist;
}

/* Client routine to decode a trace.  We return the instructions in
 * the original app code, i.e., no client modifications.
 */
instrlist_t *
decode_trace(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *frag = fragment_lookup(dcontext, tag);
    /* We don't support asking about other threads, for synch purposes
     * (see recreate_fragment_ilist() synch notes)
     */
    if (get_thread_private_dcontext() != dcontext)
        return NULL;

    if (frag != NULL && TEST(FRAG_IS_TRACE, frag->flags)) {
        instrlist_t *ilist;
        bool alloc_res;
        /* Support being called from bb/trace hook (couldbelinking) or
         * from cache clean call (nolinking).  We disallow asking about
         * another thread's private traces.
         */
        if (!is_couldbelinking(dcontext))
            d_r_mutex_lock(&thread_initexit_lock);
        ilist = recreate_fragment_ilist(dcontext, NULL, &frag, &alloc_res,
                                        false /*no mangling*/,
                                        false /*do not re-call client*/);
        ASSERT(!alloc_res);
        if (!is_couldbelinking(dcontext))
            d_r_mutex_unlock(&thread_initexit_lock);

        return ilist;
    }

    return NULL;
}

app_pc
find_app_bb_end(dcontext_t *dcontext, byte *start_pc, uint flags)
{
    build_bb_t bb;
    init_build_bb(&bb, start_pc, false /*not interp*/, false /*not for cache*/,
                  false /*do not mangle*/, false /*no translation*/, INVALID_FILE, flags,
                  NULL /*no overlap*/);
    build_bb_ilist(dcontext, &bb);
    instrlist_clear_and_destroy(dcontext, bb.ilist);
    return bb.end_pc;
}

bool
app_bb_overlaps(dcontext_t *dcontext, byte *start_pc, uint flags, byte *region_start,
                byte *region_end, overlap_info_t *info_res)
{
    build_bb_t bb;
    overlap_info_t info;
    info.region_start = region_start;
    info.region_end = region_end;
    init_build_bb(&bb, start_pc, false /*not interp*/, false /*not for cache*/,
                  false /*do not mangle*/, false /*no translation*/, INVALID_FILE, flags,
                  &info);
    build_bb_ilist(dcontext, &bb);
    instrlist_clear_and_destroy(dcontext, bb.ilist);
    info.bb_end = bb.end_pc;
    if (info_res != NULL)
        *info_res = info;
    return info.overlap;
}

#ifdef DEBUG
static void
report_native_module(dcontext_t *dcontext, app_pc modpc)
{
    char name[MAX_MODNAME_INTERNAL];
    const char *modname = name;
    if (os_get_module_name_buf(modpc, name, BUFFER_SIZE_ELEMENTS(name)) == 0) {
        /* for native_exec_callcall we do end up putting DGC on native_exec_list */
        ASSERT(DYNAMO_OPTION(native_exec_callcall));
        modname = "<DGC>";
    }
    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
        "module %s is on native list, executing natively\n", modname);
    STATS_INC(num_native_module_entrances);
    SYSLOG_INTERNAL_WARNING_ONCE("module %s set up for native execution", modname);
}
#endif

/* WARNING: breaks all kinds of rules, like ret addr transparency and
 * assuming app stack and not doing calls out of the cache and not having
 * control during dll loads, etc...
 */
static void
build_native_exec_bb(dcontext_t *dcontext, build_bb_t *bb)
{
    instr_t *in;
    opnd_t jmp_tgt IF_AARCH64(UNUSED);
#if defined(X86) && defined(X64)
    bool reachable = rel32_reachable_from_vmcode(bb->start_pc);
#endif
    DEBUG_DECLARE(bool ok;)
    /* if we ever protect from simultaneous thread attacks then this will
     * be a hole -- for now should work, all protected while native until
     * another thread goes into DR
     */
    /* Create a bb that changes the return address on the app stack such that we
     * will take control when coming back, and then goes native.
     * N.B.: we ASSUME we reached this moduled via a call --
     * build_basic_block_fragment needs to make sure, since we can't verify here
     * w/o trying to decode backward from retaddr, and if we're wrong we'll
     * clobber the stack and never regain control!
     * We also assume this bb is never reached later through a non-call.
     */
    ASSERT(bb->initialized);
    ASSERT(bb->app_interp);
    ASSERT(!bb->record_translation);
    ASSERT(bb->start_pc != NULL);
    /* vmlist must start out empty (or N/A).  For clients it may have started early. */
    ASSERT(bb->vmlist == NULL || !bb->record_vmlist || bb->checked_start_vmarea);
    if (TEST(FRAG_HAS_TRANSLATION_INFO, bb->flags))
        bb->flags &= ~FRAG_HAS_TRANSLATION_INFO;
    bb->native_exec = true;

    BBPRINT(bb, IF_DGCDIAG_ELSE(1, 2), "build_native_exec_bb @" PFX "\n", bb->start_pc);
    DOLOG(2, LOG_INTERP,
          { dump_mcontext(get_mcontext(dcontext), THREAD, DUMP_NOT_XML); });
    if (!bb->checked_start_vmarea)
        check_new_page_start(dcontext, bb);
    /* create instrlist after check_new_page_start to avoid memory leak
     * on unreadable memory
     * WARNING: do not add any app instructions to this ilist!
     * If you do you must enable selfmod below.
     */
    bb->ilist = instrlist_create(dcontext);
    /* FIXME PR 303413: we won't properly translate a fault in our app
     * stack references here.  We mark as our own mangling so we'll at
     * least return failure from our translate routine.
     */
    instrlist_set_our_mangling(bb->ilist, true);

    /* get dcontext to xdi, for prot-dcontext, xsi holds upcontext too */
    insert_shared_get_dcontext(dcontext, bb->ilist, NULL, true /*save xdi*/);
    instrlist_append(bb->ilist,
                     instr_create_save_to_dc_via_reg(dcontext, REG_NULL /*default*/,
                                                     SCRATCH_REG0, SCRATCH_REG0_OFFS));

    /* need some cleanup prior to native: turn off asynch, clobber trace, etc.
     * Now that we have a stack of native retaddrs, we save the app retaddr in C
     * code.
     */
    if (bb->native_call) {
        dr_insert_clean_call_ex(dcontext, bb->ilist, NULL, (void *)call_to_native,
                                DR_CLEANCALL_RETURNS_TO_NATIVE, 1,
                                opnd_create_reg(REG_XSP));
    } else {
        if (DYNAMO_OPTION(native_exec_opt)) {
            insert_return_to_native(dcontext, bb->ilist, NULL, REG_NULL /* default */,
                                    SCRATCH_REG0);
        } else {
            dr_insert_clean_call_ex(dcontext, bb->ilist, NULL, (void *)return_to_native,
                                    DR_CLEANCALL_RETURNS_TO_NATIVE, 0);
        }
    }

#if defined(X86) && defined(X64)
    if (!reachable) {
        /* best to store the target at the end of the bb, to keep it readonly,
         * but that requires a post-pass to patch its value: since native_exec
         * is already hacky we just go through TLS and ignore multi-thread selfmod.
         */
        instrlist_append(
            bb->ilist,
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(SCRATCH_REG0),
                                 OPND_CREATE_INTPTR((ptr_int_t)bb->start_pc)));
        if (X64_CACHE_MODE_DC(dcontext) && !X64_MODE_DC(dcontext) &&
            DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            jmp_tgt = opnd_create_reg(REG_R9);
        } else {
            jmp_tgt = opnd_create_tls_slot(os_tls_offset(MANGLE_XCX_SPILL_SLOT));
        }
        instrlist_append(
            bb->ilist, INSTR_CREATE_mov_st(dcontext, jmp_tgt, opnd_create_reg(REG_XAX)));
    } else
#endif
    {
        jmp_tgt = opnd_create_pc(bb->start_pc);
    }

    instrlist_append(bb->ilist,
                     instr_create_restore_from_dc_via_reg(dcontext, REG_NULL /*default*/,
                                                          SCRATCH_REG0,
                                                          SCRATCH_REG0_OFFS));
    insert_shared_restore_dcontext_reg(dcontext, bb->ilist, NULL);

#ifdef AARCH64
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#1569 */
#else
    /* this is the jump to native code */
    instrlist_append(bb->ilist,
                     opnd_is_pc(jmp_tgt) ? XINST_CREATE_jump(dcontext, jmp_tgt)
                                         : XINST_CREATE_jump_mem(dcontext, jmp_tgt));
#endif

    /* mark all as do-not-mangle, so selfmod, etc. will leave alone (in absence
     * of selfmod only really needed for the jmp to native code)
     */
    for (in = instrlist_first(bb->ilist); in != NULL; in = instr_get_next(in))
        instr_set_meta(in);

    /* this is a jump for a dummy exit cti */
    instrlist_append(bb->ilist,
                     XINST_CREATE_jump(dcontext, opnd_create_pc(bb->start_pc)));

    if (DYNAMO_OPTION(shared_bbs) && !TEST(FRAG_TEMP_PRIVATE, bb->flags))
        bb->flags |= FRAG_SHARED;

    /* Can't be coarse-grain since has non-exit cti */
    bb->flags &= ~FRAG_COARSE_GRAIN;
    STATS_INC(coarse_prevent_native_exec);

    /* We exclude the bb from trace to avoid going native in the process of
     * building a trace for simplicity.
     * XXX i#1239: DR needs to be able to unlink native exec gateway bbs for
     * proper cache consistency and signal handling, in which case we could
     * use FRAG_MUST_END_TRACE here instead.
     */
    bb->flags |= FRAG_CANNOT_BE_TRACE;

    /* We support mangling here, though currently we don't need it as we don't
     * include any app code (although we mark this bb as belonging to the start
     * pc, so we'll get flushed if this region does), and even if target is
     * selfmod we're running it natively no matter how it modifies itself.  We
     * only care that transition to target is via a call or call* so we can
     * clobber the retaddr and regain control, and that no retaddr mangling
     * happens while native before coming back out.  While the former does not
     * depend on the target at all, unfortunately we cannot verify the latter.
     */
    if (TEST(FRAG_SELFMOD_SANDBOXED, bb->flags))
        bb->flags &= ~FRAG_SELFMOD_SANDBOXED;
    DEBUG_DECLARE(ok =) mangle_bb_ilist(dcontext, bb);
    ASSERT(ok);
#ifdef DEBUG
    DOLOG(3, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 3, "native_exec_bb @" PFX "\n", bb->start_pc);
        instrlist_disassemble(dcontext, bb->start_pc, bb->ilist, THREAD);
    });
#endif
}

static bool
at_native_exec_gateway(dcontext_t *dcontext, app_pc start,
                       bool *is_call _IF_DEBUG(bool xfer_target))
{
    /* ASSUMPTION: transfer to another module will always be by indirect call
     * or non-inlined direct call from a fragment that will not be flushed.
     * For now we will only go native if last_exit was
     * a call, a true call*, or a PLT-style call,jmp* (and we detect the latter only
     * if the call is inlined, so if the jmp* table is in a DGC-marked region
     * or if -no_inline_calls we will miss these: FIXME).
     * FIXME: what if have PLT-style but no GOT indirection: call,jmp ?!?
     *
     * We try to identify funky call* constructions (like
     * call*,...,jmp* in case 4269) by examining TOS to see whether it's a
     * retaddr -- we do this if last_exit is a jmp* or is unknown (for the
     * target_delete ibl path).
     *
     * FIXME: we will fail to identify a delay-loaded indirect xfer!
     * Need to know dynamic link patchup code to look for.
     *
     * FIXME: we will fail to take over w/ non-call entrances to a dll, like
     * NtContinue or direct jmp from DGC.
     * we could try to take the top-of-stack value and see if it's a retaddr by
     * decoding the prev instr to see if it's a call.  decode backwards may have
     * issues, and if really want everything will have to do this on every bb,
     * not just if lastexit is ind xfer.
     *
     * We count up easy-to-identify cases we've missed in the DOSTATS below.
     */
    bool native_exec_bb = false;

    /* We can get here if we start interpreting native modules. */
    ASSERT(start != (app_pc)back_from_native && start != (app_pc)native_module_callout &&
           "interpreting return from native module?");
    ASSERT(is_call != NULL);
    *is_call = false;

    if (DYNAMO_OPTION(native_exec) && !vmvector_empty(native_exec_areas)) {
        /* do we KNOW that we came from an indirect call? */
        if (TEST(LINK_CALL /*includes IND_JMP_PLT*/, dcontext->last_exit->flags) &&
            /* only check direct calls if native_exec_dircalls is on */
            (DYNAMO_OPTION(native_exec_dircalls) ||
             LINKSTUB_INDIRECT(dcontext->last_exit->flags))) {
            STATS_INC(num_native_entrance_checks);
            /* we do the overlap check last since it's more costly */
            if (is_native_pc(start)) {
                native_exec_bb = true;
                *is_call = true;
                DOSTATS({
                    if (EXIT_IS_CALL(dcontext->last_exit->flags)) {
                        if (LINKSTUB_INDIRECT(dcontext->last_exit->flags))
                            STATS_INC(num_native_module_entrances_indcall);
                        else
                            STATS_INC(num_native_module_entrances_call);
                    } else
                        STATS_INC(num_native_module_entrances_plt);
                });
            }
        }
        /* can we GUESS that we came from an indirect call? */
        else if (DYNAMO_OPTION(native_exec_guess_calls) &&
                 (/* FIXME: require jmp* be in separate module? */
                  (LINKSTUB_INDIRECT(dcontext->last_exit->flags) &&
                   EXIT_IS_JMP(dcontext->last_exit->flags)) ||
                  LINKSTUB_FAKE(dcontext->last_exit))) {
            /* if unknown last exit, or last exit was jmp*, examine TOS and guess
             * whether it's a retaddr
             */
            app_pc *tos = (app_pc *)get_mcontext(dcontext)->xsp;
            STATS_INC(num_native_entrance_TOS_checks);
            /* vector check cheaper than is_readable syscall, etc. so do it before them,
             * but after last_exit checks above since overlap is more costly
             */
            if (is_native_pc(start) &&
                is_readable_without_exception((app_pc)tos, sizeof(app_pc))) {
                enum { MAX_CALL_CONSIDER = 6 /* ignore prefixes */ };
                app_pc retaddr = *tos;
                LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                    "at native_exec target: checking TOS " PFX " => " PFX
                    " for retaddr\n",
                    tos, retaddr);
#ifdef RETURN_AFTER_CALL
                if (DYNAMO_OPTION(ret_after_call)) {
                    native_exec_bb = is_observed_call_site(dcontext, retaddr);
                    *is_call = true;
                    LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                        "native_exec: *TOS is %sa call site in ret-after-call table\n",
                        native_exec_bb ? "" : "NOT ");
                } else {
#endif
                    /* try to decode backward -- make sure readable for decoding */
                    if (is_readable_without_exception(retaddr - MAX_CALL_CONSIDER,
                                                      MAX_CALL_CONSIDER +
                                                          MAX_INSTR_LENGTH)) {
                        /* ind calls have variable length and form so we decode
                         * each byte rather than searching for ff and guessing length
                         */
                        app_pc pc, next_pc;
                        instr_t instr;
                        instr_init(dcontext, &instr);
                        for (pc = retaddr - MAX_CALL_CONSIDER; pc < retaddr; pc++) {
                            LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 3,
                                "native_exec: decoding @" PFX " looking for call\n", pc);
                            instr_reset(dcontext, &instr);
                            next_pc = IF_AARCH64_ELSE(decode_cti_with_ldstex,
                                                      decode_cti)(dcontext, pc, &instr);
                            STATS_INC(num_native_entrance_TOS_decodes);
                            if (next_pc == retaddr && instr_is_call(&instr)) {
                                native_exec_bb = true;
                                *is_call = true;
                                LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                                    "native_exec: found call @ pre-*TOS " PFX "\n", pc);
                                break;
                            }
                        }
                        instr_free(dcontext, &instr);
                    }
#ifdef RETURN_AFTER_CALL
                }
#endif
                DOSTATS({
                    if (native_exec_bb) {
                        if (LINKSTUB_FAKE(dcontext->last_exit))
                            STATS_INC(num_native_module_entrances_TOS_unknown);
                        else
                            STATS_INC(num_native_module_entrances_TOS_jmp);
                    }
                });
            }
        }
        /* i#2381: Only now can we check things that might preempt the
         * "guess" code above.
         */
        /* Is this a return from a non-native module into a native module? */
        if (!native_exec_bb && DYNAMO_OPTION(native_exec_retakeover) &&
            LINKSTUB_INDIRECT(dcontext->last_exit->flags) &&
            TEST(LINK_RETURN, dcontext->last_exit->flags)) {
            if (is_native_pc(start)) {
                /* XXX: check that this is the return address of a known native
                 * callsite where we took over on a module transition.
                 */
                STATS_INC(num_native_module_entrances_ret);
                native_exec_bb = true;
                *is_call = false;
            }
        }
#ifdef UNIX
        /* Is this the entry point of a native ELF executable?  The entry point
         * (usually _start) cannot return as there is no retaddr.
         */
        else if (!native_exec_bb && DYNAMO_OPTION(native_exec_retakeover) &&
                 LINKSTUB_INDIRECT(dcontext->last_exit->flags) &&
                 start == get_image_entry()) {
            if (is_native_pc(start)) {
                native_exec_bb = true;
                *is_call = false;
            }
        }
#endif

        DOSTATS({
            /* did we reach a native dll w/o going through an ind call caught above? */
            if (!xfer_target /* else we'll re-check at the target itself */ &&
                !native_exec_bb && is_native_pc(start)) {
                LOG(THREAD, LOG_INTERP | LOG_VMAREAS, 2,
                    "WARNING: pc " PFX " is on native list but reached bypassing "
                    "gateway!\n",
                    start);
                STATS_INC(num_native_entrance_miss);
                /* do-once since once get into dll past gateway may xfer
                 * through a bunch of lastexit-null or indjmp to same dll
                 */
                ASSERT_CURIOSITY_ONCE(false && "inside native_exec dll");
            }
        });
    }

    return native_exec_bb;
}

/* Use when calling build_bb_ilist with for_cache = true.
 * Must hold bb_building_lock.
 */
static inline void
init_interp_build_bb(dcontext_t *dcontext, build_bb_t *bb, app_pc start,
                     uint initial_flags, bool for_trace, instrlist_t **unmangled_ilist)
{
    ASSERT_OWN_MUTEX(USE_BB_BUILDING_LOCK() && !TEST(FRAG_TEMP_PRIVATE, initial_flags),
                     &bb_building_lock);
    /* We need to set up for abort prior to native exec and other checks
     * that can crash */
    ASSERT(dcontext->bb_build_info == NULL);
    /* This won't make us be nested b/c for bb.for_cache caller is supposed
     * to set this up */
    dcontext->bb_build_info = (void *)bb;

    init_build_bb(
        bb, start, true /*real interp*/, true /*for cache*/, true /*mangle*/,
        false /* translation: set below for clients */, INVALID_FILE,
        initial_flags |
            (INTERNAL_OPTION(store_translations) ? FRAG_HAS_TRANSLATION_INFO : 0),
        NULL /*no overlap*/);
    if (!TEST(FRAG_TEMP_PRIVATE, initial_flags))
        bb->has_bb_building_lock = true;
    /* We avoid races where there is no hook when we start building a
     * bb (and hence we don't record translation or do full decode) yet
     * a hook when we're ready to call one by storing whether there is a
     * hook at translation/decode decision time: now.
     */
    if (dr_bb_hook_exists()) {
        /* i#805: Don't instrument code on the null instru list.
         * Because the module load event is now on 1st exec, we need to trigger
         * it now so the client can adjust the null instru list:
         */
        check_new_page_start(dcontext, bb);
        bb->checked_start_vmarea = true;
        if (!os_module_get_flag(bb->start_pc, MODULE_NULL_INSTRUMENT))
            bb->pass_to_client = true;
    }
    /* PR 299808: even if no bb hook, for a trace hook we need to
     * record translation and do full decode.  It's racy to check
     * dr_trace_hook_exists() here so we rely on trace building having
     * set unmangled_ilist.
     */
    if (bb->pass_to_client || unmangled_ilist != NULL) {
        /* case 10009/214444: For client interface builds, store the translation.
         * by default.  This ensures clients can get the correct app address
         * of any instruction.  We also rely on this for allowing the client
         * to return DR_EMIT_STORE_TRANSLATIONS and setting the
         * FRAG_HAS_TRANSLATION_INFO flag after decoding the app code.
         *
         * FIXME: xref case 10070/214505.  Currently this means that all
         * instructions are fully decoded for client interface builds.
         */
        bb->record_translation = true;
        /* PR 200409: If a bb hook exists, we always do a full decode.
         * Note that we currently do this anyway to get
         * translation fields, but once we fix case 10070 it
         * won't be that way.
         * We do not let the client turn this off (the runtime
         * option is not dynamic, and off by default anyway), as we
         * do not export level-handling instr_t routines like *_expand
         * for walking instrlists and instr_decode().
         */
        bb->full_decode = !INTERNAL_OPTION(fast_client_decode);
        /* PR 299808: we give client chance to re-add instrumentation */
        bb->for_trace = for_trace;
    }
    /* we need to clone the ilist pre-mangling */
    bb->unmangled_ilist = unmangled_ilist;
}

static inline void
exit_interp_build_bb(dcontext_t *dcontext, build_bb_t *bb)
{
    ASSERT(dcontext->bb_build_info == (void *)bb);
    /* Caller's responsibility to clean up since bb.for_cache */
    dcontext->bb_build_info = NULL;

    /* free the instrlist_t elements */
    instrlist_clear_and_destroy(dcontext, bb->ilist);
}

/* Interprets the application's instructions until the end of a basic
 * block is found, and then creates a fragment for the basic block.
 * DOES NOT look in the hashtable to see if such a fragment already exists!
 */
fragment_t *
build_basic_block_fragment(dcontext_t *dcontext, app_pc start, uint initial_flags,
                           bool link, bool visible, bool for_trace,
                           instrlist_t **unmangled_ilist)
{
    fragment_t *f;
    build_bb_t bb;
    dr_where_am_i_t wherewasi = dcontext->whereami;
    bool image_entry;
    KSTART(bb_building);
    dcontext->whereami = DR_WHERE_INTERP;

    /* Neither thin_client nor hotp_only should be building any bbs. */
    ASSERT(!RUNNING_WITHOUT_CODE_CACHE());

    /* ASSUMPTION: image entry is reached via indirect transfer and
     * so will be the start of a bb
     */
    image_entry = check_for_image_entry(start);

    init_interp_build_bb(dcontext, &bb, start, initial_flags, for_trace, unmangled_ilist);
    if (at_native_exec_gateway(dcontext, start,
                               &bb.native_call _IF_DEBUG(false /*not xfer tgt*/))) {
        DODEBUG({ report_native_module(dcontext, bb.start_pc); });
        /* PR 232617 - build_native_exec_bb doesn't support setting translation
         * info, but it also doesn't pass the built bb to the client (it
         * contains no app code) so we don't need it. */
        bb.record_translation = false;
        build_native_exec_bb(dcontext, &bb);
    } else {
        build_bb_ilist(dcontext, &bb);
        if (dcontext->bb_build_info == NULL) { /* going native */
            f = NULL;
            goto build_basic_block_fragment_done;
        }
        if (bb.native_exec) {
            /* change bb to be a native_exec gateway */
            bool is_call = bb.native_call;
            LOG(THREAD, LOG_INTERP, 2, "replacing built bb with native_exec bb\n");
            instrlist_clear_and_destroy(dcontext, bb.ilist);
            vm_area_destroy_list(dcontext, bb.vmlist);
            dcontext->bb_build_info = NULL;
            init_interp_build_bb(dcontext, &bb, start, initial_flags, for_trace,
                                 unmangled_ilist);
            /* PR 232617 - build_native_exec_bb doesn't support setting
             * translation info, but it also doesn't pass the built bb to the
             * client (it contains no app code) so we don't need it. */
            bb.record_translation = false;
            bb.native_call = is_call;
            build_native_exec_bb(dcontext, &bb);
        }
    }
    /* case 9652: we do not want to persist the image entry point, so we keep
     * it fine-grained
     */
    if (image_entry)
        bb.flags &= ~FRAG_COARSE_GRAIN;

    if (DYNAMO_OPTION(opt_jit) && visible && is_jit_managed_area(bb.start_pc)) {
        ASSERT(bb.overlap_info == NULL || bb.overlap_info->contiguous);
        jitopt_add_dgc_bb(bb.start_pc, bb.end_pc, TEST(FRAG_IS_TRACE_HEAD, bb.flags));
    }

    /* emit fragment into fcache */
    KSTART(bb_emit);
    f = emit_fragment_ex(dcontext, start, bb.ilist, bb.flags, bb.vmlist, link, visible);
    KSTOP(bb_emit);

#ifdef CUSTOM_TRACES_RET_REMOVAL
    f->num_calls = dcontext->num_calls;
    f->num_rets = dcontext->num_rets;
#endif

#ifdef DGC_DIAGNOSTICS
    if ((f->flags & FRAG_DYNGEN)) {
        LOG(THREAD, LOG_INTERP, 1, "new bb is DGC:\n");
        DOLOG(1, LOG_INTERP, { disassemble_app_bb(dcontext, start, THREAD); });
        DOLOG(3, LOG_INTERP, { disassemble_fragment(dcontext, f, false); });
    }
#endif
    DOLOG(2, LOG_INTERP,
          { disassemble_fragment(dcontext, f, d_r_stats->loglevel <= 3); });
    DOLOG(4, LOG_INTERP, {
        if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
            LOG(THREAD, LOG_INTERP, 4, "\nXXXX sandboxed fragment!  original code:\n");
            disassemble_app_bb(dcontext, f->tag, THREAD);
            LOG(THREAD, LOG_INTERP, 4, "code cache code:\n");
            disassemble_fragment(dcontext, f, false);
        }
    });
    if (INTERNAL_OPTION(bbdump_tags)) {
        disassemble_fragment_header(dcontext, f, bbdump_file);
    }

#ifdef INTERNAL
    DODEBUG({
        if (INTERNAL_OPTION(stress_recreate_pc)) {
            /* verify recreation */
            stress_test_recreate(dcontext, f, bb.ilist);
        }
    });
#endif

    exit_interp_build_bb(dcontext, &bb);
build_basic_block_fragment_done:
    dcontext->whereami = wherewasi;
    KSTOP(bb_building);
    return f;
}

/* Builds an instrlist_t as though building a bb from pretend_pc, but decodes
 * from pc.
 * Use recreate_fragment_ilist() for building an instrlist_t for a fragment.
 * If check_vm_area is false, Does NOT call check_thread_vm_area()!
 *   Make sure you know it will terminate at the right spot.  It does
 *   check selfmod and native_exec for elision, but otherwise will
 *   follow ubrs to the limit.  Currently used for
 *   record_translation_info() (case 3559).
 * If vmlist!=NULL and check_vm_area, returns the vmlist, which the
 * caller must free by calling vm_area_destroy_list.
 */
instrlist_t *
recreate_bb_ilist(dcontext_t *dcontext, byte *pc, byte *pretend_pc, app_pc stop_pc,
                  uint flags, uint *res_flags OUT, uint *res_exit_type OUT,
                  bool check_vm_area, bool mangle, void **vmlist_out OUT,
                  bool call_client, bool for_trace)
{
    build_bb_t bb;

    /* don't know full range -- just do simple check now */
    if (!is_readable_without_exception(pc, 4)) {
        LOG(THREAD, LOG_INTERP, 3, "recreate_bb_ilist: cannot read memory at " PFX "\n",
            pc);
        return NULL;
    }

    LOG(THREAD, LOG_INTERP, 3, "\nbuilding bb instrlist now *********************\n");
    init_build_bb(&bb, pc, false /*not interp*/, false /*not for cache*/, mangle,
                  true /*translation*/, INVALID_FILE, flags, NULL /*no overlap*/);
    /* We support a stop pc to ensure selfmod matches how it was originally built,
     * w/o having to include the next instr which might have triggered the bb
     * termination but not been included in the bb (i#1441).
     * It only applies to full_decode.
     */
    bb.stop_pc = stop_pc;
    bb.check_vm_area = check_vm_area;
    if (check_vm_area && vmlist_out != NULL)
        bb.record_vmlist = true;
    if (check_vm_area && !bb.record_vmlist)
        bb.record_vmlist = true; /* for xl8 region checks */
    /* PR 214962: we call bb hook again, unless the client told us
     * DR_EMIT_STORE_TRANSLATIONS, in which case we shouldn't come here,
     * except for traces (see below):
     */
    bb.pass_to_client = (DYNAMO_OPTION(code_api) && call_client &&
                         /* i#843: This flag cannot be changed dynamically, so
                          * its current value should match the value used at
                          * ilist building time.  Alternatively, we could store
                          * bb->pass_to_client in the fragment.
                          */
                         !os_module_get_flag(pc, MODULE_NULL_INSTRUMENT));
    /* PR 299808: we call bb hook again when translating a trace that
     * didn't have DR_EMIT_STORE_TRANSLATIONS on itself (or on any
     * for_trace bb if there was no trace hook).
     */
    bb.for_trace = for_trace;
    /* instrument_basic_block, called by build_bb_ilist, verifies that all
     * non-meta instrs have translation fields */
    if (pretend_pc != pc)
        bb.pretend_pc = pretend_pc;

    build_bb_ilist(dcontext, &bb);

    LOG(THREAD, LOG_INTERP, 3, "\ndone building bb instrlist *********************\n\n");
    if (res_flags != NULL)
        *res_flags = bb.flags;
    if (res_exit_type != NULL)
        *res_exit_type = bb.exit_type;
    if (check_vm_area && vmlist_out != NULL)
        *vmlist_out = bb.vmlist;
    else if (bb.record_vmlist)
        vm_area_destroy_list(dcontext, bb.vmlist);
    return bb.ilist;
}

/* Re-creates an ilist of the fragment that currently contains the
 * passed-in code cache pc, also returns the fragment.
 *
 * Exactly one of pc and (f_res or *f_res) must be NULL:
 * If pc==NULL, assumes that *f_res is the fragment to use;
 * else, looks up the fragment, allocating it if necessary.
 *   If f_res!=NULL, the fragment is returned and whether it was allocated
 *   is returned in the alloc_res param.
 *   If f_res==NULL, if the fragment was allocated it is freed here.
 *
 * NOTE : does not add prefix instructions to the created ilist, if we change
 * this to add them be sure to check recreate_app_* for compatibility (for ex.
 * adding them and setting their translation to pc would break current
 * implementation, also setting translation to NULL would trigger an assert)
 *
 * Returns NULL if unable to recreate the fragment ilist (fragment not found
 * or fragment is pending deletion and app memory might have changed).
 * In that case f_res is still pointed at the fragment if it was found, and
 * alloc is valid.
 *
 * For proper synchronization :
 * If caller is the dcontext's owner then needs to be couldbelinking, otherwise
 * the dcontext's owner should be suspended and the callers should own the
 * thread_initexit_lock
 */
instrlist_t *
recreate_fragment_ilist(dcontext_t *dcontext, byte *pc,
                        /*IN/OUT*/ fragment_t **f_res, /*OUT*/ bool *alloc_res,
                        bool mangle, bool call_client)
{
    fragment_t *f;
    uint flags = 0;
    instrlist_t *ilist;
    bool alloc = false;
    monitor_data_t md = {
        0,
    };
    dr_isa_mode_t old_mode = DEFAULT_ISA_MODE;
    /* check synchronization, we need to make sure no one flushes the
     * fragment we just looked up while we are recreating it, if it's the
     * caller's dcontext then just need to be couldbelinking, otherwise need
     * the thread_initexit_lock since then we are looking up in someone else's
     * table (the dcontext's owning thread would also need to be suspended)
     */
    ASSERT((dcontext != GLOBAL_DCONTEXT &&
            d_r_get_thread_id() == dcontext->owning_thread &&
            is_couldbelinking(dcontext)) ||
           (ASSERT_OWN_MUTEX(true, &thread_initexit_lock), true));
    STATS_INC(num_recreated_fragments);
    if (pc == NULL) {
        ASSERT(f_res != NULL && *f_res != NULL);
        f = *f_res;
    } else {
        /* Ensure callers don't give us both valid f and valid pc */
        ASSERT(f_res == NULL || *f_res == NULL);
        LOG(THREAD, LOG_INTERP, 3, "recreate_fragment_ilist: looking up pc " PFX "\n",
            pc);
        f = fragment_pclookup_with_linkstubs(dcontext, pc, &alloc);
        LOG(THREAD, LOG_INTERP, 3, "\tfound F%d\n", f == NULL ? -1 : f->id);
        if (f_res != NULL)
            *f_res = f;
        /* ref case 3559, others, we won't be able to reliably recreate if
         * target is pending flush, original memory might no longer be there or
         * the memory might have changed.  caller should use the stored
         * translation info instead.
         */
        if (f == NULL || TEST(FRAG_WAS_DELETED, f->flags)) {
            ASSERT(f != NULL || !alloc); /* alloc shouldn't be set if no f */
            ilist = NULL;
            goto recreate_fragment_done;
        }
    }

    /* Recreate in same mode as original fragment */
    DEBUG_DECLARE(bool ok =)
    dr_set_isa_mode(dcontext, FRAG_ISA_MODE(f->flags), &old_mode);
    ASSERT(ok);

    if ((f->flags & FRAG_IS_TRACE) == 0) {
        /* easy case: just a bb */
        ilist = recreate_bb_ilist(dcontext, (byte *)f->tag, (byte *)f->tag,
                                  NULL /*default stop*/, 0 /*no pre flags*/, &flags, NULL,
                                  true /*check vm area*/, mangle, NULL, call_client,
                                  false /*not for_trace*/);
        ASSERT(ilist != NULL);
        if (ilist == NULL) /* a race */
            goto recreate_fragment_done;
        if (PAD_FRAGMENT_JMPS(f->flags))
            nop_pad_ilist(dcontext, f, ilist, false /* set translation */);
        goto recreate_fragment_done;
    } else {
        /* build trace up one bb at a time */
        instrlist_t *bb;
        byte *apc;
        trace_only_t *t = TRACE_FIELDS(f);
        uint i;
        instr_t *last;
        bool mangle_at_end = mangle_trace_at_end();

        if (mangle_at_end) {
            /* we need an md for mangle_trace */
            md.trace_tag = f->tag;
            /* be sure we ask for translation fields */
            md.trace_flags = f->flags | FRAG_HAS_TRANSLATION_INFO;
            md.num_blks = t->num_bbs;
            md.blk_info = (trace_bb_build_t *)HEAP_ARRAY_ALLOC(
                dcontext, trace_bb_build_t, md.num_blks, ACCT_TRACE, true);
            md.pass_to_client = true;
        }

        ilist = instrlist_create(dcontext);
        STATS_INC(num_recreated_traces);
        ASSERT(t->bbs != NULL);
        for (i = 0; i < t->num_bbs; i++) {
            void *vmlist = NULL;
            apc = (byte *)t->bbs[i].tag;
            bb = recreate_bb_ilist(
                dcontext, apc, apc, NULL /*default stop*/, 0 /*no pre flags*/, &flags,
                &md.final_exit_flags, true /*check vm area*/, !mangle_at_end,
                (mangle_at_end ? &vmlist : NULL), call_client, true /*for_trace*/);
            ASSERT(bb != NULL);
            if (bb == NULL) {
                instrlist_clear_and_destroy(dcontext, ilist);
                vm_area_destroy_list(dcontext, vmlist);
                ilist = NULL;
                goto recreate_fragment_done;
            }
            if (mangle_at_end)
                md.blk_info[i].info = t->bbs[i];
            last = instrlist_last(bb);
            ASSERT(last != NULL);
            if (mangle_at_end) {
                md.blk_info[i].vmlist = vmlist;
                md.blk_info[i].final_cti = instr_is_cti(instrlist_last(bb));
            }

            /* PR 299808: we need to duplicate what we did when we built the trace.
             * While if there's no client trace hook we could mangle and fixup as we
             * go, for simplicity we mangle at the end either way (in either case our
             * code here is not exactly what we did when we made it anyway)
             * PR 333597: we can't use mangle_trace if we have elision on.
             */
            if (mangle && !mangle_at_end) {
                /* To duplicate the trace-building logic:
                 * - call fixup_last_cti()
                 * - retarget the ibl routine just like extend_trace() does
                 */
                app_pc target = (last != NULL) ? opnd_get_pc(instr_get_target(last))
                                               : NULL; /* FIXME: is it always safe */
                /* convert a basic block IBL, and retarget it to IBL_TRACE* */
                if (target != NULL &&
                    is_indirect_branch_lookup_routine(dcontext, target)) {
                    target = get_alternate_ibl_routine(dcontext, target, f->flags);
                    ASSERT(target != NULL);
                    LOG(THREAD, LOG_MONITOR, 3,
                        "recreate_fragment_ilist: replacing ibl_routine to target=" PFX
                        "\n",
                        target);
                    instr_set_target(last, opnd_create_pc(target));
                    instr_set_our_mangling(last, true); /* undone by target set */
                }
                if (DYNAMO_OPTION(pad_jmps) && !INTERNAL_OPTION(pad_jmps_shift_bb)) {
                    /* FIXME - hack, but pad_jmps_shift_bb will be on by
                     * default. Synchronize changes here with recreate_fragment_ilist.
                     * This hack is protected by asserts in nop_pad_ilist() (that
                     * we never add nops to a bb if -pad_jmps_shift_bb) and in
                     * extend_trace_pad_bytes (that we only add bbs to traces). */
                    /* FIXME - on linux the signal fence exit can trigger the
                     * protective assert in nop_pad_ilist() */
                    remove_nops_from_ilist(dcontext, bb _IF_DEBUG(true));
                }
                if (instrlist_last(ilist) != NULL) {
                    fixup_last_cti(dcontext, ilist, (app_pc)apc, flags, f->flags, NULL,
                                   NULL, true /* record translation */, NULL, NULL, NULL);
                }
            }

            instrlist_append(ilist, instrlist_first(bb));
            instrlist_init(bb); /* to clear fields to make destroy happy */
            instrlist_destroy(dcontext, bb);
        }

        /* XXX i#5062 In the future this call should be placed inside mangle_trace() */
        IF_AARCH64(fixup_indirect_trace_exit(dcontext, ilist));

        /* PR 214962: re-apply client changes, this time storing translation
         * info for modified instrs
         */
        if (call_client) /* else it's decode_trace() who is not really recreating */
            instrument_trace(dcontext, f->tag, ilist, true);
        /* instrument_trace checks that all non-meta instrs have translation fields */

        if (mangle) {
            if (mangle_at_end) {
                if (!mangle_trace(dcontext, ilist, &md)) {
                    instrlist_clear_and_destroy(dcontext, ilist);
                    ilist = NULL;
                    goto recreate_fragment_done;
                }
            } /* else we mangled one bb at a time up above */

#ifdef INTERNAL
            /* we only optimize traces */
            if (dynamo_options.optimize) {
                /* re-apply all optimizations to ilist
                 * assumption: all optimizations are deterministic and stateless,
                 * so we can exactly replicate their results
                 */
                LOG(THREAD_GET, LOG_INTERP, 2, "\tre-applying optimizations to F%d\n",
                    f->id);
#    ifdef SIDELINE
                if (dynamo_options.sideline) {
                    if (!TEST(FRAG_DO_NOT_SIDELINE, f->flags))
                        optimize_trace(dcontext, f->tag, ilist);
                    /* else, never optimized */
                } else
#    endif
                    optimize_trace(dcontext, f->tag, ilist);
            }
#endif

            /* FIXME: case 4718 append_trace_speculate_last_ibl(true)
             * should be called as well
             */
            if (PAD_FRAGMENT_JMPS(f->flags))
                nop_pad_ilist(dcontext, f, ilist, false /* set translation */);
        }
    }

recreate_fragment_done:
    if (md.blk_info != NULL) {
        uint i;
        for (i = 0; i < md.num_blks; i++) {
            vm_area_destroy_list(dcontext, md.blk_info[i].vmlist);
            md.blk_info[i].vmlist = NULL;
        }
        HEAP_ARRAY_FREE(dcontext, md.blk_info, trace_bb_build_t, md.num_blks, ACCT_TRACE,
                        true);
    }
    if (alloc_res != NULL)
        *alloc_res = alloc;
    if (f_res == NULL && alloc)
        fragment_free(dcontext, f);
    DEBUG_DECLARE(ok =) dr_set_isa_mode(dcontext, old_mode, NULL);
    ASSERT(ok);
    return ilist;
}

/*** TRACE BUILDING ROUTINES *****************************************************/

static void
process_nops_for_trace(dcontext_t *dcontext, instrlist_t *ilist,
                       uint flags _IF_DEBUG(bool recreating))
{
    if (PAD_FRAGMENT_JMPS(flags) && !INTERNAL_OPTION(pad_jmps_shift_bb)) {
        /* FIXME - hack, but pad_jmps_shift_bb will be on by
         * default. Synchronize changes here with recreate_fragment_ilist.
         * This hack is protected by asserts in nop_pad_ilist() (that
         * we never add nops to a bb if -pad_jmps_shift_bb) and in
         * extend_trace_pad_bytes (that we only add bbs to traces). */
        /* FIXME - on linux the signal fence exit can trigger the
         * protective assert in nop_pad_ilist() */
        remove_nops_from_ilist(dcontext, ilist _IF_DEBUG(recreating));
    }
}

/* Combines instrlist_preinsert to ilist and the size calculation of the addition */
static inline int
tracelist_add(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    /* when we emit the trace we're going to call instr_length() on all instrs
     * anyway, and we'll re-use any memory allocated here for an encoding
     */
    int size;
#if defined(X86) && defined(X64)
    if (!X64_CACHE_MODE_DC(dcontext)) {
        instr_set_x86_mode(inst, true /*x86*/);
        instr_shrink_to_32_bits(inst);
    }
#endif
    size = instr_length(dcontext, inst);
    instrlist_preinsert(ilist, where, inst);
    return size;
}

/* FIXME i#1668, i#2974: NYI on ARM/AArch64 */
#ifdef X86
/* Combines instrlist_postinsert to ilist and the size calculation of the addition */
static inline int
tracelist_add_after(dcontext_t *dcontext, instrlist_t *ilist, instr_t *where,
                    instr_t *inst)
{
    /* when we emit the trace we're going to call instr_length() on all instrs
     * anyway, and we'll re-use any memory allocated here for an encoding
     */
    int size;
#    ifdef X64
    if (!X64_CACHE_MODE_DC(dcontext)) {
        instr_set_x86_mode(inst, true /*x86*/);
        instr_shrink_to_32_bits(inst);
    }
#    endif
    size = instr_length(dcontext, inst);
    instrlist_postinsert(ilist, where, inst);
    return size;
}
#endif /* X86 */

#ifdef HASHTABLE_STATISTICS
/* increments a given counter - assuming XCX/R2 is dead */
int
insert_increment_stat_counter(dcontext_t *dcontext, instrlist_t *trace, instr_t *next,
                              uint *counter_address)
{
    int added_size = 0;
    /* incrementing a branch-type specific thread private counter */
    opnd_t private_branchtype_counter = OPND_CREATE_ABSMEM(counter_address, OPSZ_4);

    /* using LEA to avoid clobbering eflags in a simple load-increment-store */
    /*>>>    movl    counter, %ecx */
    /*>>>    lea     1(%ecx), %ecx    */
    /*>>>    movl    %ecx, counter */
    /* x64: the counter is still 32 bits */
    added_size += tracelist_add(dcontext, trace, next,
                                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG2),
                                                  private_branchtype_counter));
    added_size += tracelist_add(
        dcontext, trace, next,
        XINST_CREATE_add(dcontext, opnd_create_reg(SCRATCH_REG2), OPND_CREATE_INT8(1)));
    added_size += tracelist_add(dcontext, trace, next,
                                XINST_CREATE_store(dcontext, private_branchtype_counter,
                                                   opnd_create_reg(SCRATCH_REG2)));
    return added_size;
}
#endif /* HASHTABLE_STATISTICS */

/* inserts proper instruction(s) to restore XCX spilled on indirect branch mangling
 *    assumes target instrlist is a trace!
 * returns size to be added to trace
 */
static inline int
insert_restore_spilled_xcx(dcontext_t *dcontext, instrlist_t *trace, instr_t *next)
{
    int added_size = 0;

    if (DYNAMO_OPTION(private_ib_in_tls)) {
#ifdef X86
        if (X64_CACHE_MODE_DC(dcontext) && !X64_MODE_DC(dcontext) &&
            IF_X64_ELSE(DYNAMO_OPTION(x86_to_x64_ibl_opt), false)) {
            added_size +=
                tracelist_add(dcontext, trace, next,
                              INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XCX),
                                                  opnd_create_reg(REG_R9)));
        } else
#endif
        {
            added_size += tracelist_add(
                dcontext, trace, next,
                XINST_CREATE_load(
                    dcontext, opnd_create_reg(SCRATCH_REG2),
                    opnd_create_tls_slot(os_tls_offset(MANGLE_XCX_SPILL_SLOT))));
        }
    } else {
        /* We need to restore XCX from TLS for shared fragments, but from
         * mcontext for private fragments, and all traces are private
         */
        added_size += tracelist_add(dcontext, trace, next,
                                    instr_create_restore_from_dcontext(
                                        dcontext, SCRATCH_REG2, SCRATCH_REG2_OFFS));
    }

    return added_size;
}

bool
instr_is_trace_cmp(dcontext_t *dcontext, instr_t *inst)
{
    if (!instr_is_our_mangling(inst))
        return false;
#ifdef X86
    return
#    ifdef X64
        instr_get_opcode(inst) == OP_mov_imm ||
        /* mov %rax -> xbx-tls-spill-slot */
        instr_get_opcode(inst) == OP_mov_st || instr_get_opcode(inst) == OP_lahf ||
        instr_get_opcode(inst) == OP_seto || instr_get_opcode(inst) == OP_cmp ||
        instr_get_opcode(inst) == OP_jnz || instr_get_opcode(inst) == OP_add ||
        instr_get_opcode(inst) == OP_sahf
#    else
        instr_get_opcode(inst) == OP_lea || instr_get_opcode(inst) == OP_jecxz ||
        instr_get_opcode(inst) == OP_jmp
#    endif
        ;
#elif defined(AARCH64)
    return instr_get_opcode(inst) == OP_movz || instr_get_opcode(inst) == OP_movk ||
        instr_get_opcode(inst) == OP_eor || instr_get_opcode(inst) == OP_cbnz;
#elif defined(ARM)
    /* FIXME i#1668: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(disable_traces));
    return false;
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(DYNAMO_OPTION(disable_traces));
    return false;
#endif
}

/* 32-bit only: inserts a comparison to speculative_tag with no side effect and
 * if value is matched continue target is assumed to be immediately
 * after targeter (which must be < 127 bytes away).
 * returns size to be added to trace
 */
static int
insert_transparent_comparison(dcontext_t *dcontext, instrlist_t *trace,
                              instr_t *targeter, /* exit CTI */
                              app_pc speculative_tag)
{
    int added_size = 0;
#ifdef X86
    instr_t *jecxz;
    instr_t *continue_label = INSTR_CREATE_label(dcontext);
    /* instead of:
     *   cmp ecx,const
     * we use:
     *   lea -const(ecx) -> ecx
     *   jecxz continue
     *   lea const(ecx) -> ecx
     *   jmp exit   # usual targeter for stay on trace comparison
     * continue:    # if match, we target post-targeter
     *
     * we have to use the landing pad b/c we don't know whether the
     * stub will be <128 away
     */
    /* lea requires OPSZ_lea operand */
    added_size += tracelist_add(
        dcontext, trace, targeter,
        INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                         opnd_create_base_disp(REG_ECX, REG_NULL, 0,
                                               -((int)(ptr_int_t)speculative_tag),
                                               OPSZ_lea)));
    jecxz = INSTR_CREATE_jecxz(dcontext, opnd_create_instr(continue_label));
    /* do not treat jecxz as exit cti! */
    instr_set_meta(jecxz);
    added_size += tracelist_add(dcontext, trace, targeter, jecxz);
    /* need to recover address in ecx */
    IF_X64(ASSERT_NOT_IMPLEMENTED(!X64_MODE_DC(dcontext)));
    added_size +=
        tracelist_add(dcontext, trace, targeter,
                      INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ECX),
                                       opnd_create_base_disp(
                                           REG_ECX, REG_NULL, 0,
                                           ((int)(ptr_int_t)speculative_tag), OPSZ_lea)));
    added_size += tracelist_add_after(dcontext, trace, targeter, continue_label);
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    return added_size;
}

#if defined(X86) && defined(X64)
static int
mangle_x64_ib_in_trace(dcontext_t *dcontext, instrlist_t *trace, instr_t *targeter,
                       app_pc next_tag)
{
    int added_size = 0;
    if (X64_MODE_DC(dcontext) || !DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
        added_size += tracelist_add(
            dcontext, trace, targeter,
            INSTR_CREATE_mov_st(
                dcontext, opnd_create_tls_slot(os_tls_offset(PREFIX_XAX_SPILL_SLOT)),
                opnd_create_reg(REG_XAX)));
        added_size +=
            tracelist_add(dcontext, trace, targeter,
                          INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_XAX),
                                               OPND_CREATE_INTPTR((ptr_int_t)next_tag)));
    } else {
        ASSERT(X64_CACHE_MODE_DC(dcontext));
        added_size += tracelist_add(dcontext, trace, targeter,
                                    INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_R8),
                                                        opnd_create_reg(REG_XAX)));
        added_size +=
            tracelist_add(dcontext, trace, targeter,
                          INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_R10),
                                               OPND_CREATE_INTPTR((ptr_int_t)next_tag)));
    }
    /* saving in the trace and restoring in ibl means that
     * -unsafe_ignore_eflags_{trace,ibl} must be equivalent
     */
    if (!INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
        if (X64_MODE_DC(dcontext) || !DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            added_size += tracelist_add(
                dcontext, trace, targeter,
                INSTR_CREATE_mov_st(
                    dcontext,
                    opnd_create_tls_slot(os_tls_offset(INDIRECT_STUB_SPILL_SLOT)),
                    opnd_create_reg(REG_XAX)));
        }
        /* FIXME: share w/ insert_save_eflags() */
        added_size +=
            tracelist_add(dcontext, trace, targeter, INSTR_CREATE_lahf(dcontext));
        if (!INTERNAL_OPTION(unsafe_ignore_overflow)) { /* OF needs saving */
            /* Move OF flags into the OF flag spill slot. */
            added_size += tracelist_add(
                dcontext, trace, targeter,
                INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
        }
        if (X64_MODE_DC(dcontext) || !DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            added_size += tracelist_add(
                dcontext, trace, targeter,
                INSTR_CREATE_cmp(
                    dcontext, opnd_create_reg(REG_XCX),
                    opnd_create_tls_slot(os_tls_offset(INDIRECT_STUB_SPILL_SLOT))));
        } else {
            added_size +=
                tracelist_add(dcontext, trace, targeter,
                              INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XCX),
                                               opnd_create_reg(REG_R10)));
        }
    } else {
        added_size += tracelist_add(
            dcontext, trace, targeter,
            INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_XCX),
                             (X64_MODE_DC(dcontext) || !DYNAMO_OPTION(x86_to_x64_ibl_opt))
                                 ? opnd_create_reg(REG_XAX)
                                 : opnd_create_reg(REG_R10)));
    }
    /* change jmp into jne to trace cmp entry of ibl routine (special entry
     * that is after the eflags save) */
    instr_set_opcode(targeter, OP_jnz);
    added_size++; /* jcc is 6 bytes, jmp is 5 bytes */
    ASSERT(opnd_is_pc(instr_get_target(targeter)));
    instr_set_target(targeter,
                     opnd_create_pc(get_trace_cmp_entry(
                         dcontext, opnd_get_pc(instr_get_target(targeter)))));
    /* since the target gets lost we need to OR in this flag */
    instr_exit_branch_set_type(targeter,
                               instr_exit_branch_type(targeter) | INSTR_TRACE_CMP_EXIT);
    return added_size;
}
#endif

#ifdef AARCH64
/* Prior to indirect branch trace mangling, we check previous byte blocks
 * before each indirect branch if they were mangled properly.
 * An indirect branch:
 *    br jump_target_reg
 * is mangled into (see mangle_indirect_jump in mangle.c):
 *    str IBL_TARGET_REG, TLS_REG2_SLOT
 *    mov IBL_TARGET_REG, jump_target_reg
 *    b ibl_routine or indirect_stub
 * This function is used by mangle_indirect_branch_in_trace;
 * it removes the two mangled instructions and returns the jump_target_reg id.
 *
 * This function is an optimisation in that we can avoid the spill of
 * IBL_TARGET_REG as we know that the actual target is always in a register
 * (or the stolen register slot, see below) on AArch64 and we can compare this
 * value in the register directly with the actual target.
 * And we delay the loading of the target into IBL_TARGET_REG
 * (done in fixup_indirect_trace_exit) until we are on the miss path.
 *
 * However, there is a special case where there isn't a str/mov being patched
 * rather there is an str/ldr which could happen when the jump target is stored
 * in the stolen register. For example:
 *
 *  ....
 *  blr    stolen_reg -> %x30
 *  b      ibl_routine
 *
 *  is mangled into
 *
 *  ...
 *  str tgt_reg, TLS_REG2_SLOT
 *  ldr tgt_reg, TLS_REG_STOLEN_SLOT
 *  b ibl_routine
 *
 * This means that we should not remove the str/ldr, rather we need to compare the
 * trace_next_exit with tgt_reg directly and remember to restore the value of tgt_reg
 * in the case when the branch is not taken.
 *
 */

static reg_id_t
check_patched_ibl(dcontext_t *dcontext, instrlist_t *trace, instr_t *targeter,
                  int *added_size, bool *tgt_in_stolen_reg)
{
    instr_t *prev = instr_get_prev(targeter);

    for (prev = instr_get_prev_expanded(dcontext, trace, targeter); prev;
         prev = instr_get_prev(prev)) {

        instr_t *prev_prev = instr_get_prev(prev);
        if (prev_prev == NULL)
            break;

        /* we expect
         * prev_prev   str IBL_TARGET_REG, TLS_REG2_SLOT
         * prev        mov IBL_TARGET_REG, jump_target_reg
         */

        if (instr_get_opcode(prev_prev) == OP_str && instr_get_opcode(prev) == OP_orr &&
            opnd_get_reg(instr_get_src(prev_prev, 0)) == IBL_TARGET_REG &&
            opnd_get_base(instr_get_dst(prev_prev, 0)) == dr_reg_stolen &&
            opnd_get_reg(instr_get_dst(prev, 0)) == IBL_TARGET_REG) {

            reg_id_t jp_tg_reg = opnd_get_reg(instr_get_src(prev, 1));

            instrlist_remove(trace, prev_prev);
            instr_destroy(dcontext, prev_prev);
            instrlist_remove(trace, prev);
            instr_destroy(dcontext, prev);

            LOG(THREAD, LOG_INTERP, 4, "found and removed str/mov\n");
            *added_size -= 2 * AARCH64_INSTR_SIZE;
            return jp_tg_reg;

            /* Here we expect
             * prev_prev  str   IBL_TARGET_REG, TLS_REG2_SLOT
             * prev       ldr   IBL_TARGET_REG, TLS_REG_STOLEN_SLOT
             */
        } else if (instr_get_opcode(prev_prev) == OP_str &&
                   instr_get_opcode(prev) == OP_ldr &&
                   opnd_get_reg(instr_get_src(prev_prev, 0)) == IBL_TARGET_REG &&
                   opnd_get_base(instr_get_src(prev, 0)) == dr_reg_stolen &&
                   opnd_get_reg(instr_get_dst(prev, 0)) == IBL_TARGET_REG) {
            *tgt_in_stolen_reg = true;
            LOG(THREAD, LOG_INTERP, 4, "jump target is in stolen register slot\n");
            return IBL_TARGET_REG;
        }
    }
    return DR_REG_NULL;
}

/* For AArch64 we have a special case if we cannot remove all code after
 * the direct branch, which is mangled by cbz/cbnz stolen register.
 * For example:
 *    cbz x28, target
 * would be mangled (see mangle_cbr_stolen_reg() in aarchxx/mangle.c) into:
 *    str x0, [x28]
 *    ldr x0, [x28, #32]
 *    cbnz x0, fall       <- meta instr, not treated as exit cti
 *    ldr x0, [x28]
 *    b target            <- delete after
 * fall:
 *    ldr x0, [x28]
 *    b fall_target
 *    ...
 * If we delete all code after "b target", then the "fall" path would
 * be lost. Therefore we need to append the fall path at the end of
 * the trace as a fake exit stub. Swapping them might be dangerous since
 * a stub trace may be created on both paths.
 *
 * XXX i#5062 This special case is not needed when we elimiate decoding from code cache
 */

static bool
instr_is_cbr_stolen(instr_t *instr)
{
    if (!instr)
        return false;
    else {
        instr_get_opcode(instr);
        return instr->opcode == OP_cbz || instr->opcode == OP_cbnz ||
            instr->opcode == OP_tbz || instr->opcode == OP_tbnz;
    }
}

static bool
instr_is_load_tls(instr_t *instr)
{
    if (!instr || !instr_raw_bits_valid(instr))
        return false;
    else {
        return instr_get_opcode(instr) == OP_ldr &&
            opnd_get_base(instr_get_src(instr, 0)) == dr_reg_stolen;
    }
}

static instr_t *
fixup_cbr_on_stolen_reg(dcontext_t *dcontext, instrlist_t *trace, instr_t *targeter)
{
    /* We check prior to the targeter whether there is a pattern such as
     *    cbz/cbnz ...
     *    ldr reg, [x28, #SLOT]
     * Otherwise, just return the previous instruction.
     */
    instr_t *prev = instr_get_prev_expanded(dcontext, trace, targeter);
    if (!instr_is_load_tls(prev))
        return prev;
    instr_t *prev_prev = instr_get_prev_expanded(dcontext, trace, prev);
    if (!instr_is_cbr_stolen(prev_prev))
        return prev;
    /* Now we confirm that this cbr stolen_reg was properly mangled. */
    instr_t *next = instr_get_next_expanded(dcontext, trace, targeter);
    if (!next)
        return prev;
    /* Next instruction must be a LDR TLS slot. */
    ASSERT_CURIOSITY(instr_is_load_tls(next));
    instr_t *next_next = instr_get_next_expanded(dcontext, trace, next);
    if (!next_next)
        return prev;
    /* Next next instruction must be direct branch. */
    ASSERT_CURIOSITY(instr_is_ubr(next_next));
    /* Set the cbz/cbnz target to "fall" path. */
    instr_set_target(prev_prev, instr_get_target(next_next));
    /* Then we can safely remove the "fall" path. */
    return prev;
}
#endif

/* Mangles an indirect branch in a trace where a basic block with tag "tag"
 * is being added as the next block beyond the indirect branch.
 * Returns the size of instructions added to trace.
 */
static int
mangle_indirect_branch_in_trace(dcontext_t *dcontext, instrlist_t *trace,
                                instr_t *targeter, app_pc next_tag, uint next_flags,
                                instr_t **delete_after /*OUT*/, instr_t *end_instr)
{
    int added_size = 0;
    instr_t *next = instr_get_next(targeter);
    /* all indirect branches should be ubrs */
    ASSERT(instr_is_ubr(targeter));
    /* expecting basic blocks only */
    ASSERT((end_instr != NULL && targeter == end_instr) ||
           targeter == instrlist_last(trace));

    ASSERT(delete_after != NULL);
    *delete_after = (next == NULL || (end_instr != NULL && targeter == end_instr))
        ? NULL
        : instr_get_prev(next);

    STATS_INC(trace_ib_cmp);
#if defined(X86)
    /* Change jump to indirect_branch_lookup to a conditional jump
     * based on indirect target not equaling next block in trace
     *
     * the bb has already done:
     *   spill xcx to xcx-tls-spill-slot
     *   mov curtarget, xcx
     *   <any other side effects of ind branch, like ret xsp adjust>
     *
     * and we now want to accomplish:
     *       cmp ecx,const
     *
     * on 32-bit we use:
     *       lea -const(ecx) -> ecx
     *       jecxz continue
     *       lea const(ecx) -> ecx
     *       jmp exit   # usual targeter for stay on trace comparison
     *     continue:    # if match, we target post-targeter
     *       restore ecx
     * we have to use the landing pad b/c we don't know whether the
     * stub will be <128 away
     *
     * on 64-bit we use (PR 245832):
     *       mov xax, xax-tls-spill-slot
     *       mov $staytarget, xax
     *       if !INTERNAL_OPTION(unsafe_ignore_eflags_{trace,ibl})
     *         mov xax, xbx-tls-spill-slot
     *         lahf
     *         seto al
     *         cmp xcx, xbx-tls-spill-slot
     *       else
     *         cmp xcx, xax
     *       jne exit
     *       if xcx live:
     *         mov xcx-tls-spill-slot, xcx
     *       if flags live && unsafe options not on:
     *         add 7f, al
     *         sahf
     *       if xax live:
     *         mov xax-tls-spill-slot, xax
     */

#    ifdef CUSTOM_TRACES_RET_REMOVAL
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /* try to remove ret
     * FIXME: also handle ret imm => prev instr is add
     */
    inst = instr_get_prev(targeter);
    if (dcontext->call_depth >= 0 && instr_raw_bits_valid(inst)) {
        byte *b = inst->bytes + inst->length - 1;
        /*
  0x40538115   89 0d ec 68 06 40    mov    %ecx -> 0x400668ec
  0x4053811b   59                   pop    %esp (%esp) -> %ecx %esp
  0x4053811c   83 c4 04             add    $0x04 %esp -> %esp
        */
        LOG(THREAD, LOG_MONITOR, 4,
            "ret removal: *b=0x%x, prev=" PFX ", dcontext=" PFX ", 0x%x\n", *b,
            *((int *)(b - 4)), dcontext, XCX_OFFSET);
        if ((*b == 0x59 && *((int *)(b - 4)) == ((uint)dcontext) + XCX_OFFSET) ||
            (*(b - 3) == 0x59 && *((int *)(b - 7)) == ((uint)dcontext) + XCX_OFFSET &&
             *(b - 2) == 0x83 && *(b - 1) == 0xc4)) {
            uint esp_add;
            /* already added calls & rets to call depth
             * if not negative, the call for this ret is earlier in this trace!
             */
            LOG(THREAD, LOG_MONITOR, 4, "fixup_last_cti: removing ret!\n");
            /* delete save ecx and pop */
            if (*b == 0x59) {
                instr_set_raw_bits(inst, inst->bytes, inst->length - 7);
                esp_add = 4;
            } else { /* delete add too */
                instr_set_raw_bits(inst, inst->bytes, inst->length - 10);
                esp_add = 4 + (uint)(*b);
                LOG(THREAD, LOG_MONITOR, 4, "*b=0x%x, esp_add=%d\n", *b, esp_add);
            }
#        ifdef DEBUG
            num_rets_removed++;
#        endif
            removed_ret = true;
            added_size +=
                tracelist_add(dcontext, trace, targeter,
                              INSTR_CREATE_lea(dcontext, opnd_create_reg(REG_ESP),
                                               opnd_create_base_disp(REG_ESP, REG_NULL, 0,
                                                                     esp_add, OPSZ_lea)));
        }
    }
    if (removed_ret) {
        *delete_after = instr_get_prev(targeter);
        return added_size;
    }
#    endif /* CUSTOM_TRACES_RET_REMOVAL */

#    ifdef X64
    if (X64_CACHE_MODE_DC(dcontext)) {
        added_size += mangle_x64_ib_in_trace(dcontext, trace, targeter, next_tag);
    } else {
#    endif
        if (!INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
            /* if equal follow to the next instruction after the exit CTI */
            added_size +=
                insert_transparent_comparison(dcontext, trace, targeter, next_tag);
            /* leave jmp as it is, a jmp to exit stub (thence to ind br
             * lookup) */
        } else {
            /* assume eflags don't need to be saved across ind branches,
             * so go ahead and use cmp, jne
             */
            /* FIXME: no way to cmp w/ 64-bit immed */
            IF_X64(ASSERT_NOT_IMPLEMENTED(!X64_MODE_DC(dcontext)));
            added_size += tracelist_add(
                dcontext, trace, targeter,
                INSTR_CREATE_cmp(dcontext, opnd_create_reg(REG_ECX),
                                 OPND_CREATE_INT32((int)(ptr_int_t)next_tag)));

            /* Change jmp into jne indirect_branch_lookup */
            /* CHECK: is that also going to exit stub */
            instr_set_opcode(targeter, OP_jnz);
            added_size++; /* jcc is 6 bytes, jmp is 5 bytes */
        }
#    ifdef X64
    }
#    endif /* X64 */
    /* PR 214962: our spill restoration needs this whole sequence marked mangle */
    instr_set_our_mangling(targeter, true);

    LOG(THREAD, LOG_MONITOR, 3, "fixup_last_cti: added cmp vs. " PFX " for ind br\n",
        next_tag);

#    ifdef HASHTABLE_STATISTICS
    /* If we do stay on the trace, increment a counter using dead XCX */
    if (INTERNAL_OPTION(stay_on_trace_stats)) {
        ibl_type_t ibl_type;
        /* FIXME: see if can test the instr flags instead */
        DEBUG_DECLARE(bool ok =)
        get_ibl_routine_type(dcontext, opnd_get_pc(instr_get_target(targeter)),
                             &ibl_type);
        ASSERT(ok);
        added_size += insert_increment_stat_counter(
            dcontext, trace, next,
            &get_ibl_per_type_statistics(dcontext, ibl_type.branch_type)
                 ->ib_stay_on_trace_stat);
    }
#    endif /* HASHTABLE_STATISTICS */

    /* If we do stay on the trace, must restore xcx
     * TODO optimization: check if xcx is live or not in next bb */
    added_size += insert_restore_spilled_xcx(dcontext, trace, next);

#    ifdef X64
    if (X64_CACHE_MODE_DC(dcontext)) {
        LOG(THREAD, LOG_INTERP, 4, "next_flags for post-ibl-cmp: 0x%x\n", next_flags);
        if (!TEST(FRAG_WRITES_EFLAGS_6, next_flags) &&
            !INTERNAL_OPTION(unsafe_ignore_eflags_trace)) {
            if (!TEST(FRAG_WRITES_EFLAGS_OF, next_flags) && /* OF was saved */
                !INTERNAL_OPTION(unsafe_ignore_overflow)) {
                /* restore OF using add that overflows if OF was on when we did seto */
                added_size +=
                    tracelist_add(dcontext, trace, next,
                                  INSTR_CREATE_add(dcontext, opnd_create_reg(REG_AL),
                                                   OPND_CREATE_INT8(0x7f)));
            }
            added_size +=
                tracelist_add(dcontext, trace, next, INSTR_CREATE_sahf(dcontext));
        } else
            STATS_INC(trace_ib_no_flag_restore);
        /* TODO optimization: check if xax is live or not in next bb */
        if (X64_MODE_DC(dcontext) || !DYNAMO_OPTION(x86_to_x64_ibl_opt)) {
            added_size += tracelist_add(
                dcontext, trace, next,
                INSTR_CREATE_mov_ld(
                    dcontext, opnd_create_reg(REG_XAX),
                    opnd_create_tls_slot(os_tls_offset(PREFIX_XAX_SPILL_SLOT))));
        } else {
            added_size +=
                tracelist_add(dcontext, trace, next,
                              INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(REG_XAX),
                                                  opnd_create_reg(REG_R8)));
        }
    }
#    endif
#elif defined(AARCH64)
    instr_t *instr;
    reg_id_t jump_target_reg;
    reg_id_t scratch;
    bool tgt_in_stolen_reg;
    /* Mangle jump to indirect branch lookup routine,
     * based on indirect target not being equal to next block in trace.
     * Original ibl lookup:
     *      str tgt_reg, TLS_REG2_SLOT
     *      mov tgt_reg, jump_target
     *      b ibl_routine
     * Now we rewrite it into:
     *      str x0, TLS_REG0_SLOT
     *      mov x0, #trace_next_target
     *      eor x0, x0, jump_target
     *      cbnz x0, trace_exit (ibl routine)
     *      ldr x0, TLS_REG0_SLOT
     */

    /* Check and remove previous two patched instructions if we have. */
    tgt_in_stolen_reg = false;
    jump_target_reg =
        check_patched_ibl(dcontext, trace, targeter, &added_size, &tgt_in_stolen_reg);
    if (jump_target_reg == DR_REG_NULL) {
        ASSERT_MESSAGE(2, "Failed to get branch target register in creating trace",
                       false);
        return added_size;
    }
    LOG(THREAD, LOG_MONITOR, 4, "fixup_last_cti: jump target reg is %s\n",
        reg_names[jump_target_reg]);

    /* Choose any scratch register except the target reg. */
    scratch = (jump_target_reg == DR_REG_X0) ? DR_REG_X1 : DR_REG_X0;
    /* str scratch, TLS_REG0_SLOT */
    added_size +=
        tracelist_add(dcontext, trace, next,
                      instr_create_save_to_tls(dcontext, scratch, TLS_REG0_SLOT));
    /* mov scratch, #trace_next_target */
    instr_t *first = NULL;
    instr_t *end = NULL;
    instrlist_insert_mov_immed_ptrsz(dcontext, (ptr_int_t)next_tag,
                                     opnd_create_reg(scratch), trace, next, &first, &end);
    /* Get the acutal number of mov imm instructions added. */
    instr = first;
    while (instr != end) {
        added_size += AARCH64_INSTR_SIZE;
        instr = instr_get_next(instr);
    }
    added_size += AARCH64_INSTR_SIZE;
    /* eor scratch, scratch, jump_target */
    added_size += tracelist_add(dcontext, trace, next,
                                INSTR_CREATE_eor(dcontext, opnd_create_reg(scratch),
                                                 opnd_create_reg(jump_target_reg)));
    /* cbnz scratch, trace_exit
     * branch to original ibl lookup routine */
    instr =
        INSTR_CREATE_cbnz(dcontext, instr_get_target(targeter), opnd_create_reg(scratch));
    /* Set the exit type from the targeter. */
    instr_exit_branch_set_type(instr, instr_exit_branch_type(targeter));
    added_size += tracelist_add(dcontext, trace, next, instr);
    /* If we do stay on the trace, restore x0 and x1. */
    /* ldr scratch, TLS_REG0_SLOT */
    ASSERT(TLS_REG0_SLOT != IBL_TARGET_SLOT);
    added_size +=
        tracelist_add(dcontext, trace, next,
                      instr_create_restore_from_tls(dcontext, scratch, TLS_REG0_SLOT));
    if (tgt_in_stolen_reg) {
        /* we need to restore app's x2 in case the branch to trace_exit is not taken.
         * This is not needed in the str/mov case as we removed the instruction to
         * spill the value of IBL_TARGET_REG (str tgt_reg, TLS_REG2_SLOT)
         *
         * ldr x2 TLS_REG2_SLOT
         */
        added_size += tracelist_add(
            dcontext, trace, next,
            instr_create_restore_from_tls(dcontext, IBL_TARGET_REG, IBL_TARGET_SLOT));
    }
    /* Remove the branch. */
    instrlist_remove(trace, targeter);
    instr_destroy(dcontext, targeter);
    added_size -= AARCH64_INSTR_SIZE;

#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
    return added_size;
}

/* This routine handles the mangling of the cti at the end of the
 * previous block when adding a new block (f) to the trace fragment.
 * If prev_l is not NULL, matches the ordinal of prev_l to the nth
 * exit cti in the trace instrlist_t.
 *
 * If prev_l is NULL: WARNING: this routine assumes that the previous
 * block can only have a single indirect branch -- otherwise there is
 * no way to determine which indirect exit targeted the new block!  No
 * assumptions are made about direct exits -- we can walk through them
 * all to find the one that targeted the new block.
 *
 * Returns an upper bound on the size added to the trace with inserted
 * instructions.
 * If we change this to add a substantial # of instrs, should update
 * TRACE_CTI_MANGLE_SIZE_UPPER_BOUND (assert at bottom should notify us)
 *
 * If you want to re-add the ability to add the front end of a trace,
 * revive the now-removed CUSTOM_TRACES_ADD_TRACE define from the attic.
 */
static int
fixup_last_cti(dcontext_t *dcontext, instrlist_t *trace, app_pc next_tag, uint next_flags,
               uint trace_flags, fragment_t *prev_f, linkstub_t *prev_l,
               bool record_translation, uint *num_exits_deleted /*OUT*/,
               /* If non-NULL, only looks inside trace between these two */
               instr_t *start_instr, instr_t *end_instr)
{
    app_pc target_tag;
    instr_t *inst, *targeter = NULL;
    /* at end of routine we will delete all instrs after this one: */
    instr_t *delete_after = NULL;
    bool is_indirect = false;
    /* Added size for transformations done here.
     * Use tracelist_add to automate adding inserted instr sizes.
     */
    int added_size = 0;
    uint exits_deleted = 0;

    /* count exit stubs to get the ordinal of the exit that targeted us
     * start at prev_l, and count up extraneous exits and blks until end
     */
    uint nth_exit = 0, cur_exit;
#ifdef CUSTOM_TRACES_RET_REMOVAL
    bool removed_ret = false;
#endif
    bool have_ordinal = false;
    if (prev_l != NULL && prev_l == get_deleted_linkstub(dcontext)) {
        int last_ordinal = get_last_linkstub_ordinal(dcontext);
        if (last_ordinal != -1) {
            nth_exit = last_ordinal;
            have_ordinal = true;
        }
    }
    if (!have_ordinal && prev_l != NULL && !LINKSTUB_FAKE(prev_l)) {
        linkstub_t *stub = FRAGMENT_EXIT_STUBS(prev_f);
        while (stub != prev_l)
            stub = LINKSTUB_NEXT_EXIT(stub);
        /* if prev_l is cbr followed by ubr, we'll get 1 for ubr,
         * but we want 0, so we count prev_l itself, then decrement
         */
        stub = LINKSTUB_NEXT_EXIT(stub);
        while (stub != NULL) {
            nth_exit++;
            stub = LINKSTUB_NEXT_EXIT(stub);
        }
    } /* else, we assume it's the final exit */

    LOG(THREAD, LOG_MONITOR, 4,
        "fixup_last_cti: looking for %d-th exit cti from bottom\n", nth_exit);

    if (start_instr != NULL) {
        ASSERT(end_instr != NULL);
    } else {
        start_instr = instrlist_first(trace);
        end_instr = instrlist_last(trace);
    }
    start_instr = instr_get_prev(start_instr); /* get open-ended bound */

    cur_exit = nth_exit;
    /* now match the ordinal to the instrs.
     * we don't have any way to find boundary with previous-previous block
     * to make sure we didn't go backwards too far -- does it matter?
     */
    for (inst = end_instr; inst != NULL && inst != start_instr;
         inst = instr_get_prev(inst)) {
        if (instr_is_exit_cti(inst)) {
            if (cur_exit == 0) {
                ibl_type_t ibl_type;
                /* exit cti is guaranteed to have pc target */
                target_tag = opnd_get_pc(instr_get_target(inst));

                is_indirect = get_ibl_routine_type(dcontext, target_tag, &ibl_type);

                if (is_indirect) {
                    /* this should be a trace exit stub therefore it cannot be IBL_BB* */
                    ASSERT(IS_IBL_TRACE(ibl_type.source_fragment_type));
                    targeter = inst;
                    break;
                } else {
                    if (prev_l != NULL) {
                        /* direct jmp, better point to us */
                        ASSERT(target_tag == next_tag);
                        targeter = inst;
                        break;
                    } else {
                        /* need to search for targeting jmp */
                        DOLOG(4, LOG_MONITOR,
                              { d_r_loginst(dcontext, 4, inst, "exit==targeter?"); });
                        LOG(THREAD, LOG_MONITOR, 4,
                            "target_tag = " PFX ", next_tag = " PFX "\n", target_tag,
                            next_tag);

                        if (target_tag == next_tag) {
                            targeter = inst;
                            break;
                        }
                    }
                }
            } else if (prev_l != NULL) {
                LOG(THREAD, LOG_MONITOR, 4,
                    "counting backwards: %d == target_tag = " PFX "\n", cur_exit,
                    opnd_get_pc(instr_get_target(inst)));
                cur_exit--;
            }
        } /* is exit cti */
    }
    ASSERT(targeter != NULL);
    if (record_translation)
        instrlist_set_translation_target(trace, instr_get_translation(targeter));
    instrlist_set_our_mangling(trace, true); /* PR 267260 */
    DOLOG(4, LOG_MONITOR, { d_r_loginst(dcontext, 4, targeter, "\ttargeter"); });
    if (is_indirect) {
        added_size += mangle_indirect_branch_in_trace(
            dcontext, trace, targeter, next_tag, next_flags, &delete_after, end_instr);
    } else {
        /* direct jump or conditional branch */
        instr_t *next = targeter->next;
        if (instr_is_cbr(targeter)) {
            LOG(THREAD, LOG_MONITOR, 4, "fixup_last_cti: inverted logic of cbr\n");
            if (next != NULL && instr_is_ubr(next)) {
                /* cbr followed by ubr: if cbr got us here, reverse cbr and
                 * remove ubr
                 */
                instr_invert_cbr(targeter);
                instr_set_target(targeter, instr_get_target(next));
                ASSERT(next == end_instr);
                delete_after = targeter;
                LOG(THREAD, LOG_MONITOR, 4, "\tremoved ubr following cbr\n");
            } else {
                ASSERT_NOT_REACHED();
            }
        } else if (instr_is_ubr(targeter)) {
            /* remove unnecessary ubr at end of block */
#ifdef AARCH64
            delete_after = fixup_cbr_on_stolen_reg(dcontext, trace, targeter);
#else
            delete_after = instr_get_prev(targeter);
#endif
            if (delete_after != NULL) {
                LOG(THREAD, LOG_MONITOR, 4, "fixup_last_cti: removed ubr\n");
            }
        } else
            ASSERT_NOT_REACHED();
    }
    /* remove all instrs after this cti -- but what if internal
     * control flow jumps ahead and then comes back?
     * too expensive to check for such all the time.
     * XXX: what to do?
     *
     * XXX: rather than adding entire trace on and then chopping off where
     * we exited, why not add after we know where to stop?
     */
    if (delete_after != NULL) {
        ASSERT(delete_after != end_instr);
        delete_after = instr_get_next(delete_after);
        while (delete_after != NULL) {
            inst = delete_after;
            if (delete_after == end_instr)
                delete_after = NULL;
            else
                delete_after = instr_get_next(delete_after);
            if (instr_is_exit_cti(inst)) {
                /* assumption: passing in cache target to exit_stub_size works
                 * just as well as linkstub_t target, since only cares whether
                 * targeting ibl
                 */
                app_pc target = opnd_get_pc(instr_get_target(inst));
                /* we already added all the stub size differences to the trace,
                 * so we subtract the trace size of the stub here
                 */
                added_size -= local_exit_stub_size(dcontext, target, trace_flags);
                exits_deleted++;
            } else if (instr_opcode_valid(inst) && instr_is_cti(inst)) {
                LOG(THREAD, LOG_MONITOR, 3,
                    "WARNING: deleting non-exit cti in unused tail of frag added to "
                    "trace\n");
            }
            d_r_loginst(dcontext, 4, inst, "\tdeleting");
            instrlist_remove(trace, inst);
            added_size -= instr_length(dcontext, inst);
            instr_destroy(dcontext, inst);
        }
    }

    if (num_exits_deleted != NULL)
        *num_exits_deleted = exits_deleted;

    if (record_translation)
        instrlist_set_translation_target(trace, NULL);
    instrlist_set_our_mangling(trace, false); /* PR 267260 */

#if defined(X86) && defined(X64)
    DOCHECK(1, {
        if (FRAG_IS_32(trace_flags)) {
            instr_t *in;
            /* in case we missed any in tracelist_add() */
            for (in = instrlist_first(trace); in != NULL; in = instr_get_next(in)) {
                if (instr_is_our_mangling(in))
                    ASSERT(instr_get_x86_mode(in));
            }
        }
    });
#endif

    ASSERT(added_size < TRACE_CTI_MANGLE_SIZE_UPPER_BOUND);
    return added_size;
}

/* Add a speculative counter on last IBL exit
 * Returns additional size to add to trace estimate.
 */
int
append_trace_speculate_last_ibl(dcontext_t *dcontext, instrlist_t *trace,
                                app_pc speculate_next_tag, bool record_translation)
{
    /* unlike fixup_last_cti() here we are about to go directly to the IBL routine */
    /* spill XCX in a scratch slot - note always using TLS */
    int added_size = 0;
    ibl_type_t ibl_type;

    instr_t *inst = instrlist_last(trace); /* currently only relevant to last CTI */
    instr_t *where = inst;                 /* preinsert before last CTI */

    instr_t *next = instr_get_next(inst);
    DEBUG_DECLARE(bool ok;)

    ASSERT(speculate_next_tag != NULL);
    ASSERT(inst != NULL);
    ASSERT(instr_is_exit_cti(inst));

    /* FIXME: see if can test the instr flags instead */
    DEBUG_DECLARE(ok =)
    get_ibl_routine_type(dcontext, opnd_get_pc(instr_get_target(inst)), &ibl_type);
    ASSERT(ok);

    if (record_translation)
        instrlist_set_translation_target(trace, instr_get_translation(inst));
    instrlist_set_our_mangling(trace, true); /* PR 267260 */

    STATS_INC(num_traces_end_at_ibl_speculative_link);

#ifdef HASHTABLE_STATISTICS
    DOSTATS({
        if (INTERNAL_OPTION(speculate_last_exit_stats)) {
            int tls_stat_scratch_slot = os_tls_offset(HTABLE_STATS_SPILL_SLOT);

            added_size += tracelist_add(
                dcontext, trace, where,
                XINST_CREATE_store(dcontext, opnd_create_tls_slot(tls_stat_scratch_slot),
                                   opnd_create_reg(SCRATCH_REG2)));
            added_size += insert_increment_stat_counter(
                dcontext, trace, where,
                &get_ibl_per_type_statistics(dcontext, ibl_type.branch_type)
                     ->ib_trace_last_ibl_exit);
            added_size += tracelist_add(
                dcontext, trace, where,
                XINST_CREATE_load(dcontext, opnd_create_reg(SCRATCH_REG2),
                                  opnd_create_tls_slot(tls_stat_scratch_slot)));
        }
    });
#endif
    /* preinsert comparison before exit CTI, but increment of success
     * statistics after it
     */

    /* we need to compare to speculate_next_tag now */
    /* XCX holds value to match */

    /* should use similar eflags-clobbering scheme to inline cmp */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    /*
     *    8d 89 76 9b bf ff    lea    -tag(%ecx) -> %ecx
     *    e3 0b                jecxz  continue
     *    8d 89 8a 64 40 00    lea    tag(%ecx) -> %ecx
     *    e9 17 00 00 00       jmp    <exit stub 1: IBL>
     *
     * continue:
     *                        <increment stats>
     *                        # see FIXME whether to go to prefix or do here
     *                        <restore app ecx>
     *    e9 cc aa dd 00       jmp speculate_next_tag
     *
     */

    /* leave jmp as it is, a jmp to exit stub (thence to ind br lookup) */
    added_size +=
        insert_transparent_comparison(dcontext, trace, where, speculate_next_tag);

#ifdef HASHTABLE_STATISTICS
    DOSTATS({
        reg_id_t reg = SCRATCH_REG2;
        if (INTERNAL_OPTION(speculate_last_exit_stats)) {
            int tls_stat_scratch_slot = os_tls_offset(HTABLE_STATS_SPILL_SLOT);
            /* XCX already saved */

            added_size += insert_increment_stat_counter(
                dcontext, trace, next,
                &get_ibl_per_type_statistics(dcontext, ibl_type.branch_type)
                     ->ib_trace_last_ibl_speculate_success);
            /* restore XCX to app IB target*/
            added_size += tracelist_add(
                dcontext, trace, next,
                XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                                  opnd_create_tls_slot(tls_stat_scratch_slot)));
        }
    });
#endif
    /* adding a new CTI for speculative target that is a pseudo
     * direct exit.  Although we could have used the indirect stub
     * to be the unlinked path, with a new CTI way we can unlink a
     * speculated fragment without affecting any other targets
     * reached by the IBL.  Also in general we could decide to add
     * multiple speculative comparisons and to chain them we'd
     * need new CTIs for them.
     */

    /* Ensure all register state is properly preserved on both linked
     * and unlinked paths - currently only XCX is in use.
     *
     *
     * Preferably we should be targeting prefix of target to
     * save some space for recovering XCX from hot path.  We'd
     * restore XCX in the exit stub when unlinked.
     * So it would act like a direct CTI when linked and like indirect
     * when unlinked.  It could just be an unlinked indirect stub, if
     * we haven't modified any other registers or flags.
     *
     * For simplicity, we currently restore XCX here and use a plain
     * direct exit stub that goes to target start_pc instead of
     * prefixes.
     *
     * FIXME: (case 5085) the problem with the current scheme is that
     * when we exit unlinked the source will be marked as a DIRECT
     * exit - therefore no security policies will be enforced.
     *
     * FIXME: (case 4718) should add speculated target to current list
     * in case of RCT policy that needs to be invalidated if target is
     * flushed
     */

    /* must restore xcx to app value, FIXME: see above for doing this in prefix+stub */
    added_size += insert_restore_spilled_xcx(dcontext, trace, next);

    /* add a new direct exit stub */
    added_size +=
        tracelist_add(dcontext, trace, next,
                      XINST_CREATE_jump(dcontext, opnd_create_pc(speculate_next_tag)));
    LOG(THREAD, LOG_INTERP, 3,
        "append_trace_speculate_last_ibl: added cmp vs. " PFX " for ind br\n",
        speculate_next_tag);

    if (record_translation)
        instrlist_set_translation_target(trace, NULL);
    instrlist_set_our_mangling(trace, false); /* PR 267260 */

    return added_size;
}

#ifdef HASHTABLE_STATISTICS
/* Add a counter on last IBL exit
 * if speculate_next_tag is not NULL then check case 4817's possible success
 */
/* FIXME: remove this routine once append_trace_speculate_last_ibl()
 * currently useful only to see statistics without side effects of
 * adding exit stub
 */
int
append_ib_trace_last_ibl_exit_stat(dcontext_t *dcontext, instrlist_t *trace,
                                   app_pc speculate_next_tag)
{
    /* unlike fixup_last_cti() here we are about to go directly to the IBL routine */
    /* spill XCX in a scratch slot - note always using TLS */
    int tls_stat_scratch_slot = os_tls_offset(HTABLE_STATS_SPILL_SLOT);
    int added_size = 0;
    ibl_type_t ibl_type;

    instr_t *inst = instrlist_last(trace); /* currently only relevant to last CTI */
    instr_t *where = inst;                 /* preinsert before exit CTI */
    reg_id_t reg = SCRATCH_REG2;
    DEBUG_DECLARE(bool ok;)

    /* should use similar eflags-clobbering scheme to inline cmp */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

    ASSERT(inst != NULL);
    ASSERT(instr_is_exit_cti(inst));

    /* FIXME: see if can test the instr flags instead */
    ok = get_ibl_routine_type(dcontext, opnd_get_pc(instr_get_target(inst)), &ibl_type);

    ASSERT(ok);
    added_size += tracelist_add(
        dcontext, trace, where,
        XINST_CREATE_store(dcontext, opnd_create_tls_slot(tls_stat_scratch_slot),
                           opnd_create_reg(reg)));
    added_size += insert_increment_stat_counter(
        dcontext, trace, where,
        &get_ibl_per_type_statistics(dcontext, ibl_type.branch_type)
             ->ib_trace_last_ibl_exit);
    added_size +=
        tracelist_add(dcontext, trace, where,
                      XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                                        opnd_create_tls_slot(tls_stat_scratch_slot)));

    if (speculate_next_tag != NULL) {
        instr_t *next = instr_get_next(inst);
        /* preinsert comparison before exit CTI, but increment goes after it */

        /* we need to compare to speculate_next_tag now - just like
         * fixup_last_cti() would do later.
         */
        /* ECX holds value to match here */

        /* leave jmp as it is, a jmp to exit stub (thence to ind br lookup) */

        /* continue:
         *    increment success counter
         *    jmp targeter
         *
         *   FIXME: now the last instruction is no longer the exit_cti - see if that
         *   breaks any assumptions, using a short jump to see if anyone erroneously
         *   uses this
         */
        added_size +=
            insert_transparent_comparison(dcontext, trace, where, speculate_next_tag);

        /* we'll kill again although ECX restored unnecessarily by comparison routine */
        added_size += insert_increment_stat_counter(
            dcontext, trace, next,
            &get_ibl_per_type_statistics(dcontext, ibl_type.branch_type)
                 ->ib_trace_last_ibl_speculate_success);
        /* restore ECX */
        added_size +=
            tracelist_add(dcontext, trace, next,
                          XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                                            opnd_create_tls_slot(tls_stat_scratch_slot)));
        /* jmp where */
        added_size +=
            tracelist_add(dcontext, trace, next,
                          IF_X86_ELSE(INSTR_CREATE_jmp_short, XINST_CREATE_jump)(
                              dcontext, opnd_create_instr(where)));
    }

    return added_size;
}
#endif /* HASHTABLE_STATISTICS */

/* Add the fragment f to the end of the trace instrlist_t kept in dcontext
 *
 * Note that recreate_fragment_ilist() is making assumptions about its operation
 * synchronize changes
 *
 * Returns the size change in the trace from mangling the previous block
 * (assumes the caller has already calculated the size from adding the new block)
 */
uint
extend_trace(dcontext_t *dcontext, fragment_t *f, linkstub_t *prev_l)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    fragment_t *prev_f = NULL;
    instrlist_t *trace = &(md->trace);
    instrlist_t *ilist;
    uint size;
    uint prev_mangle_size = 0;
    uint num_exits_deleted = 0;
    uint new_exits_dir = 0, new_exits_indir = 0;

#ifdef X64
    ASSERT((!!FRAG_IS_32(md->trace_flags) == !X64_MODE_DC(dcontext)) ||
           (!FRAG_IS_32(md->trace_flags) && !X64_MODE_DC(dcontext) &&
            DYNAMO_OPTION(x86_to_x64)));
#endif

    STATS_INC(num_traces_extended);

    /* if you want to re-add the ability to add traces, revive
     * CUSTOM_TRACES_ADD_TRACE from the attic
     */
    ASSERT(!TEST(FRAG_IS_TRACE, f->flags)); /* expecting block fragments */

    if (prev_l != NULL) {
        ASSERT(!LINKSTUB_FAKE(prev_l) ||
               /* we track the ordinal of the del linkstub so it's ok */
               prev_l == get_deleted_linkstub(dcontext));
        prev_f = linkstub_fragment(dcontext, prev_l);
        LOG(THREAD, LOG_MONITOR, 4, "prev_l = owned by F%d, branch pc " PFX "\n",
            prev_f->id, EXIT_CTI_PC(prev_f, prev_l));
    } else {
        LOG(THREAD, LOG_MONITOR, 4, "prev_l is NULL\n");
    }

    /* insert code to optimize last branch based on new fragment */
    if (instrlist_last(trace) != NULL) {
        prev_mangle_size =
            fixup_last_cti(dcontext, trace, f->tag, f->flags, md->trace_flags, prev_f,
                           prev_l, false, &num_exits_deleted, NULL, NULL);
    }

#ifdef CUSTOM_TRACES_RET_REMOVAL
    /* add now, want fixup to operate on depth before adding new blk */
    dcontext->call_depth += f->num_calls;
    dcontext->call_depth -= f->num_rets;
#endif

    LOG(THREAD, LOG_MONITOR, 4, "\tadding block %d == " PFX "\n", md->num_blks, f->tag);

    size = md->trace_buf_size - md->trace_buf_top;
    LOG(THREAD, LOG_MONITOR, 4, "decoding F%d into trace buf @" PFX " + 0x%x = " PFX "\n",
        f->id, md->trace_buf, md->trace_buf_top, md->trace_buf + md->trace_buf_top);
    /* FIXME: PR 307388: if md->pass_to_client, much of this is a waste of time as
     * we're going to re-mangle and re-fixup after passing our unmangled list to the
     * client.  We do want to keep the size estimate, which requires having the last
     * cti at least, so for now we keep all the work.  Of course the size estimate is
     * less valuable when the client may add a ton of instrumentation.
     */
    /* decode_fragment will convert f's ibl routines into those appropriate for
     * our trace, whether f and the trace are shared or private
     */
    ilist = decode_fragment(dcontext, f, md->trace_buf + md->trace_buf_top, &size,
                            md->trace_flags, &new_exits_dir, &new_exits_indir);

    md->blk_info[md->num_blks].info.tag = f->tag;
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
    if (md->num_blks > 0)
        md->blk_info[md->num_blks - 1].info.num_exits -= num_exits_deleted;
    md->blk_info[md->num_blks].info.num_exits = new_exits_dir + new_exits_indir;
#endif
    md->num_blks++;

    /* We need to remove any nops we added for -pad_jmps (we don't expect there
     * to be any in a bb if -pad_jmps_shift_bb) to avoid screwing up
     * fixup_last_cti etc. */
    process_nops_for_trace(dcontext, ilist, f->flags _IF_DEBUG(false /*!recreating*/));

    DOLOG(5, LOG_MONITOR, {
        LOG(THREAD, LOG_MONITOR, 5, "post-trace-ibl-fixup, ilist is:\n");
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
    });

    ASSERT(!instrlist_get_our_mangling(ilist));
    instrlist_append(trace, instrlist_first(ilist));
    instrlist_init(ilist); /* clear fields so destroy won't kill instrs on trace list */
    instrlist_destroy(dcontext, ilist);

    md->trace_buf_top += size;
    ASSERT(md->trace_buf_top < md->trace_buf_size);
    LOG(THREAD, LOG_MONITOR, 4, "post-extend_trace, trace buf + 0x%x => " PFX "\n",
        md->trace_buf_top, md->trace_buf);

    DOLOG(4, LOG_MONITOR, {
        LOG(THREAD, LOG_MONITOR, 4, "\nafter extending trace:\n");
        instrlist_disassemble(dcontext, md->trace_tag, trace, THREAD);
    });
    return prev_mangle_size;
}

/* If branch_type is 0, sets it to the type of a ubr */
static instr_t *
create_exit_jmp(dcontext_t *dcontext, app_pc target, app_pc translation, uint branch_type)
{
    instr_t *jmp = XINST_CREATE_jump(dcontext, opnd_create_pc(target));
    instr_set_translation(jmp, translation);
    if (branch_type == 0)
        instr_exit_branch_set_type(jmp, instr_branch_type(jmp));
    else
        instr_exit_branch_set_type(jmp, branch_type);
    instr_set_our_mangling(jmp, true);
    return jmp;
}

/* Given an ilist with no mangling or stitching together, this routine does those
 * things.  This is used both for clients and for recreating traces
 * for state translation.
 * It assumes the ilist abides by client rules: single-mbr bbs, no
 * changes in source app code.  Else, it returns false.
 * Elision is supported.
 *
 * Our docs disallow removal of an entire block, changing inter-block ctis, and
 * changing the ordering of the blocks, which is what allows us to correctly
 * mangle the inter-block ctis here.
 *
 * Reads the following fields from md:
 * - trace_tag
 * - trace_flags
 * - num_blks
 * - blk_info
 * - final_exit_flags
 */
bool
mangle_trace(dcontext_t *dcontext, instrlist_t *ilist, monitor_data_t *md)
{
    instr_t *inst, *next_inst, *start_instr, *jmp;
    uint blk, num_exits_deleted;
    app_pc fallthrough = NULL;
    bool found_syscall = false, found_int = false;

    /* We don't assert that mangle_trace_at_end() is true b/c the client
     * can unregister its bb and trace hooks if it really wants to,
     * though we discourage it.
     */
    ASSERT(md->pass_to_client);

    LOG(THREAD, LOG_MONITOR, 2, "mangle_trace " PFX "\n", md->trace_tag);
    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "ilist passed to mangle_trace:\n");
        instrlist_disassemble(dcontext, md->trace_tag, ilist, THREAD);
    });

    /* We make 3 passes.
     * 1st walk: find bb boundaries
     */
    blk = 0;
    for (inst = instrlist_first(ilist); inst != NULL; inst = next_inst) {
        app_pc xl8 = instr_get_translation(inst);
        next_inst = instr_get_next(inst);

        if (instr_is_meta(inst))
            continue;

        DOLOG(5, LOG_INTERP, {
            LOG(THREAD, LOG_MONITOR, 4, "transl " PFX " ", xl8);
            d_r_loginst(dcontext, 4, inst, "considering non-meta");
        });

        /* Skip blocks that don't end in ctis (except final) */
        while (blk < md->num_blks - 1 && !md->blk_info[blk].final_cti) {
            LOG(THREAD, LOG_MONITOR, 4, "skipping fall-through bb #%d\n", blk);
            md->blk_info[blk].end_instr = NULL;
            blk++;
        }

        /* Ensure non-ignorable syscall/int2b terminates trace */
        if (md->pass_to_client &&
            !client_check_syscall(ilist, inst, &found_syscall, &found_int))
            return false;

        /* Clients should not add new source code regions, which would mess us up
         * here, as well as mess up our cache consistency (both page prot and
         * selfmod).
         */
        if (md->pass_to_client &&
            (!vm_list_overlaps(dcontext, md->blk_info[blk].vmlist, xl8, xl8 + 1) &&
             !(instr_is_ubr(inst) && opnd_is_pc(instr_get_target(inst)) &&
               xl8 == opnd_get_pc(instr_get_target(inst))))
                IF_WINDOWS(&&!vmvector_overlap(landing_pad_areas,
                                               md->blk_info[blk].info.tag,
                                               md->blk_info[blk].info.tag + 1))) {
            LOG(THREAD, LOG_MONITOR, 2,
                "trace error: out-of-bounds transl " PFX " vs block w/ start " PFX "\n",
                xl8, md->blk_info[blk].info.tag);
            CLIENT_ASSERT(false,
                          "trace's app sources (instr_set_translation() targets) "
                          "must remain within original bounds");
            return false;
        }

        /* in case no exit ctis in last block, find last non-meta fall-through */
        if (blk == md->num_blks - 1) {
            /* Do not call instr_length() on this inst: use length
             * of translation! (i#509)
             */
            fallthrough = decode_next_pc(dcontext, xl8);
        }

        /* PR 299808: identify bb boundaries.  We can't go by translations alone, as
         * ubrs can point at their targets and theoretically the entire trace could
         * be ubrs: so we have to go by exits, and limit what the client can do.  We
         * can assume that each bb should not violate the bb callback rules (PR
         * 215217): if has cbr or mbr, that must end bb.  If it has a call, that
         * could be elided; if not, its target should match the start of the next
         * block.   We also want to
         * impose the can't-be-trace rules (PR 215219), which are not documented for
         * bbs: if more than one exit cti or if code beyond last exit cti then can't
         * be in a trace.  We can soften a little and allow extra ubrs if they do not
         * target the subsequent block.  FIXME: we could have stricter translation
         * reqts for ubrs: make them point at corresponding app ubr (but what if
         * really correspond to app cbr?): then can handle code past exit ubr.
         */
        if (instr_will_be_exit_cti(inst) &&
            ((!instr_is_ubr(inst) && !instr_is_near_call_direct(inst)) ||
             (inst == instrlist_last(ilist) ||
              (blk + 1 < md->num_blks &&
               /* client is disallowed from changing bb exits and sequencing in trace
                * hook; if they change in bb for_trace, will be reflected here.
                */
               opnd_get_pc(instr_get_target(inst)) == md->blk_info[blk + 1].info.tag)))) {

            DOLOG(4, LOG_INTERP, { d_r_loginst(dcontext, 4, inst, "end of bb"); });

            /* Add jump that fixup_last_cti expects */
            if (!instr_is_ubr(inst) IF_X86(|| instr_get_opcode(inst) == OP_jmp_far)) {
                app_pc target;
                if (instr_is_mbr(inst) IF_X86(|| instr_get_opcode(inst) == OP_jmp_far)) {
                    target = get_ibl_routine(
                        dcontext, get_ibl_entry_type(instr_branch_type(inst)),
                        DEFAULT_IBL_TRACE(), get_ibl_branch_type(inst));
                } else if (instr_is_cbr(inst)) {
                    /* Do not call instr_length() on this inst: use length
                     * of translation! (i#509)
                     */
                    target = decode_next_pc(dcontext, xl8);
                } else {
                    target = opnd_get_pc(instr_get_target(inst));
                }
                ASSERT(target != NULL);
                jmp = create_exit_jmp(dcontext, target, xl8, instr_branch_type(inst));
                instrlist_postinsert(ilist, inst, jmp);
                /* we're now done w/ vmlist: switch to end instr.
                 * d_r_mangle() shouldn't remove the exit cti.
                 */
                vm_area_destroy_list(dcontext, md->blk_info[blk].vmlist);
                md->blk_info[blk].vmlist = NULL;
                md->blk_info[blk].end_instr = jmp;
            } else
                md->blk_info[blk].end_instr = inst;

            blk++;
            DOLOG(4, LOG_INTERP, {
                if (blk < md->num_blks) {
                    LOG(THREAD, LOG_MONITOR, 4, "starting next bb " PFX "\n",
                        md->blk_info[blk].info.tag);
                }
            });
            if (blk >= md->num_blks && next_inst != NULL) {
                CLIENT_ASSERT(false, "unsupported trace modification: too many exits");
                return false;
            }
        }
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
        /* PR 306761: we need to re-calculate md->blk_info[blk].info.num_exits,
         * and then adjust after fixup_last_cti.
         */
        if (instr_will_be_exit_cti(inst))
            md->blk_info[blk].info.num_exits++;
#endif
    }
    if (blk < md->num_blks) {
        ASSERT(!instr_is_ubr(instrlist_last(ilist)));
        if (blk + 1 < md->num_blks) {
            CLIENT_ASSERT(false, "unsupported trace modification: too few exits");
            return false;
        }
        /* must have been no final exit cti: add final fall-through jmp */
        jmp = create_exit_jmp(dcontext, fallthrough, fallthrough, 0);
        /* FIXME PR 307284: support client modifying, replacing, or adding
         * syscalls and ints: need to re-analyze.  Then we wouldn't
         * need the md->final_exit_flags field anymore.
         * For now we disallow.
         */
        if (found_syscall || found_int) {
            instr_exit_branch_set_type(jmp, md->final_exit_flags);
#ifdef WINDOWS
            /* For INSTR_SHARED_SYSCALL, we set it pre-mangling, and it
             * survives to here if the instr is not clobbered,
             * and does not come from md->final_exit_flags
             */
            if (TEST(INSTR_SHARED_SYSCALL, instrlist_last(ilist)->flags)) {
                instr_set_target(jmp, opnd_create_pc(shared_syscall_routine(dcontext)));
                instr_set_our_mangling(jmp, true); /* undone by target set */
            }
            /* FIXME: test for linux too, but allowing ignorable syscalls */
            if (!TESTANY(LINK_NI_SYSCALL_ALL IF_WINDOWS(| LINK_CALLBACK_RETURN),
                         md->final_exit_flags) &&
                !TEST(INSTR_SHARED_SYSCALL, instrlist_last(ilist)->flags)) {
                CLIENT_ASSERT(false,
                              "client modified or added a syscall or int: unsupported");
                return false;
            }
#endif
        }
        instrlist_append(ilist, jmp);
        md->blk_info[blk].end_instr = jmp;
    } else {
        CLIENT_ASSERT((!found_syscall && !found_int)
                      /* On linux we allow ignorable syscalls in middle.
                       * FIXME PR 307284: see notes above. */
                      IF_UNIX(|| !TEST(LINK_NI_SYSCALL, md->final_exit_flags)),
                      "client changed exit target where unsupported\n"
                      "check if trace ends in a syscall or int");
    }
    ASSERT(instr_is_ubr(instrlist_last(ilist)));
    if (found_syscall)
        md->trace_flags |= FRAG_HAS_SYSCALL;
    else
        md->trace_flags &= ~FRAG_HAS_SYSCALL;

    /* 2nd walk: mangle */
    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "trace ilist before mangling:\n");
        instrlist_disassemble(dcontext, md->trace_tag, ilist, THREAD);
    });
    /* We do not need to remove nops since we never emitted */
    d_r_mangle(dcontext, ilist, &md->trace_flags, true /*mangle calls*/,
               /* we're post-client so we don't need translations unless storing */
               TEST(FRAG_HAS_TRANSLATION_INFO, md->trace_flags));
    DOLOG(4, LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, 4, "trace ilist after mangling:\n");
        instrlist_disassemble(dcontext, md->trace_tag, ilist, THREAD);
    });

    /* 3rd walk: stitch together delineated bbs */
    for (blk = 0; blk < md->num_blks && md->blk_info[blk].end_instr == NULL; blk++)
        ; /* nothing */
    start_instr = instrlist_first(ilist);
    for (inst = instrlist_first(ilist); inst != NULL; inst = next_inst) {
        next_inst = instr_get_next(inst);

        if (inst == md->blk_info[blk].end_instr) {
            /* Chain exit to point to next bb */
            if (blk + 1 < md->num_blks) {
                /* We must do proper analysis so that state translation matches
                 * created traces in whether eflags are restored post-cmp
                 */
                uint next_flags =
                    forward_eflags_analysis(dcontext, ilist, instr_get_next(inst));
                next_flags = instr_eflags_to_fragment_eflags(next_flags);
                LOG(THREAD, LOG_INTERP, 4, "next_flags for fixup_last_cti: 0x%x\n",
                    next_flags);
                fixup_last_cti(dcontext, ilist, md->blk_info[blk + 1].info.tag,
                               next_flags, md->trace_flags, NULL, NULL,
                               TEST(FRAG_HAS_TRANSLATION_INFO, md->trace_flags),
                               &num_exits_deleted,
                               /* Only walk ilist between these instrs */
                               start_instr, inst);
#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
                md->blk_info[blk].info.num_exits -= num_exits_deleted;
#endif
            }
            blk++;
            /* skip fall-throughs */
            while (blk < md->num_blks && md->blk_info[blk].end_instr == NULL)
                blk++;
            if (blk >= md->num_blks && next_inst != NULL) {
                CLIENT_ASSERT(false, "unsupported trace modification: exits modified");
                return false;
            }
            start_instr = next_inst;
        }
    }
    if (blk < md->num_blks) {
        CLIENT_ASSERT(false, "unsupported trace modification: cannot find all exits");
        return false;
    }
    return true;
}

/****************************************************************************
 * UTILITIES
 */

/* Converts instr_t EFLAGS_ flags to corresponding fragment_t FRAG_ flags,
 * assuming that the instr_t flags correspond to the start of the fragment_t.
 * Assumes instr_eflags has already accounted for predication.
 */
uint
instr_eflags_to_fragment_eflags(uint instr_eflags)
{
    uint frag_eflags = 0;
#ifdef X86
    if (instr_eflags == EFLAGS_WRITE_OF) {
        /* this fragment writes OF before reading it
         * May still read other flags before writing them.
         */
        frag_eflags |= FRAG_WRITES_EFLAGS_OF;
        return frag_eflags;
    }
#endif
    if (instr_eflags == EFLAGS_WRITE_ARITH) {
        /* fragment writes all 6 prior to reading */
        frag_eflags |= FRAG_WRITES_EFLAGS_ARITH;
#ifdef X86
        frag_eflags |= FRAG_WRITES_EFLAGS_OF;
#endif
    }
    return frag_eflags;
}

/* Returns one of these flags, defined in instr.h:
 *   EFLAGS_WRITE_ARITH = writes all arith flags before reading any
 *   EFLAGS_WRITE_OF    = writes OF before reading it (x86-only)
 *   EFLAGS_READ_ARITH  = reads some of arith flags before writing
 *   EFLAGS_READ_OF     = reads OF before writing OF (x86-only)
 *   0                  = no information before 1st cti
 */
uint
forward_eflags_analysis(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr)
{
    instr_t *in;
    uint eflags_6 = 0; /* holds flags written so far (in read slots) */
    int eflags_result = 0;
    for (in = instr; in != NULL; in = instr_get_next_expanded(dcontext, ilist, in)) {
        if (!instr_valid(in) || instr_is_cti(in)) {
            /* give up */
            break;
        }
        if (eflags_result != EFLAGS_WRITE_ARITH IF_X86(&&eflags_result != EFLAGS_READ_OF))
            eflags_result = eflags_analysis(in, eflags_result, &eflags_6);
        DOLOG(4, LOG_INTERP, {
            d_r_loginst(dcontext, 4, in, "forward_eflags_analysis");
            LOG(THREAD, LOG_INTERP, 4, "\tinstr %x => %x\n",
                instr_get_eflags(in, DR_QUERY_DEFAULT), eflags_result);
        });
    }
    return eflags_result;
}

/* This translates f's code into an instrlist_t and returns it.
 * If buf is NULL:
 *   The Instrs returned point into f's raw bits, so encode them
 *   before you delete f!
 * Else, f's raw bits are copied into buf, and *bufsz is modified to
 *   contain the total bytes copied
 *   FIXME: should have release build checks and not just asserts where
 *   we rely on caller to have big-enough buffer?
 * If target_flags differ from f->flags in sharing and/or in trace-ness,
 *   converts ibl and tls usage in f to match the desired target_flags.
 *   FIXME: converting from private to shared tls is not yet
 *   implemented: we rely on -private_ib_in_tls for adding normal
 *   private bbs to shared traces, and disallow any extensive mangling
 *   (native_exec, selfmod) from becoming shared traces.
 * The caller is responsible for destroying the instrlist and its instrs.
 * If the fragment ends in an elided jmp, a new jmp instr is created, though
 *   its bits field is NULL, allowing the caller to set it to do-not-emit if
 *   trying to exactly duplicate or calculate the size, though most callers
 *   will want to emit that jmp.  See decode_fragment_exact().
 */

static void
instr_set_raw_bits_trace_buf(instr_t *instr, byte *buf_writable_addr, uint length)
{
    /* The trace buffer is a writable address, so we need to translate to an
     * executable address for pointing at bits.
     */
    instr_set_raw_bits(instr, vmcode_get_executable_addr(buf_writable_addr), length);
}

/* We want to avoid low-loglevel disassembly when we're in the middle of disassembly */
#define DF_LOGLEVEL(dc) (((dc) != GLOBAL_DCONTEXT && (dc)->in_opnd_disassemble) ? 6U : 4U)

instrlist_t *
decode_fragment(dcontext_t *dcontext, fragment_t *f, byte *buf, /*IN/OUT*/ uint *bufsz,
                uint target_flags, /*OUT*/ uint *dir_exits, /*OUT*/ uint *indir_exits)
{
    linkstub_t *l;
    cache_pc start_pc, stop_pc, pc, prev_pc = NULL, raw_start_pc;
    instr_t *instr, *cti = NULL, *raw_instr;
    instrlist_t *ilist = instrlist_create(dcontext);
    byte *top_buf = NULL, *cur_buf = NULL;
    app_pc target_tag;
    uint num_bytes, offset;
    uint num_dir = 0, num_indir = 0;
    bool tls_to_dc;
    bool shared_to_private =
        TEST(FRAG_SHARED, f->flags) && !TEST(FRAG_SHARED, target_flags);
#ifdef WINDOWS
    /* The fragment could contain an ignorable sysenter instruction if
     * the following conditions are satisfied. */
    bool possible_ignorable_sysenter = DYNAMO_OPTION(ignore_syscalls) &&
        (get_syscall_method() == SYSCALL_METHOD_SYSENTER) &&
        TEST(FRAG_HAS_SYSCALL, f->flags);
#endif
    instrlist_t intra_ctis;
    coarse_info_t *info = NULL;
    bool coarse_elided_ubrs = false;
    dr_isa_mode_t old_mode;
    /* for decoding and get_ibl routines we need the dcontext mode set */
    DEBUG_DECLARE(bool ok =)
    dr_set_isa_mode(dcontext, FRAG_ISA_MODE(f->flags), &old_mode);
    ASSERT(ok);
    /* i#1494: Decoding a code fragment from code cache, decode_fragment
     * may mess up the 32-bit/64-bit mode in -x86_to_x64 because 32-bit
     * application code is encoded as 64-bit code fragments into the code cache.
     * Thus we currently do not support using decode_fragment with -x86_to_x64,
     * including trace and coarse_units (coarse-grain code cache management)
     */
    IF_X86_64(ASSERT(!DYNAMO_OPTION(x86_to_x64)));

    instrlist_init(&intra_ctis);

    /* Now we need to go through f and make cti's for each of its exit cti's and
     * non-exit cti's with off-fragment targets that need to be re-pc-relativized.
     * The rest of the instructions can be lumped into raw instructions.
     */
    start_pc = FCACHE_ENTRY_PC(f);
    pc = start_pc;
    raw_start_pc = start_pc;
    if (buf != NULL) {
        cur_buf = buf;
        top_buf = cur_buf;
        ASSERT(bufsz != NULL);
    }
    /* Handle code after last exit but before stubs by allowing l to be NULL.
     * Handle coarse-grain fake fragment_t by discovering exits as we go, with
     * l being NULL the whole time.
     */
    if (TEST(FRAG_FAKE, f->flags)) {
        ASSERT(TEST(FRAG_COARSE_GRAIN, f->flags));
        info = get_fragment_coarse_info(f);
        ASSERT(info != NULL);
        coarse_elided_ubrs =
            (info->persisted && TEST(PERSCACHE_ELIDED_UBR, info->flags)) ||
            (!info->persisted && DYNAMO_OPTION(coarse_freeze_elide_ubr));
        /* Assumption: coarse-grain fragments have no ctis w/ off-fragment targets
         * that are not exit ctis
         */
        l = NULL;
    } else
        l = FRAGMENT_EXIT_STUBS(f);
    while (true) {
        uint l_flags;
        cti = NULL;
        if (l != NULL) {
            stop_pc = EXIT_CTI_PC(f, l);
        } else if (TEST(FRAG_FAKE, f->flags)) {
            /* we don't know the size of f */
            stop_pc = (cache_pc)UNIVERSAL_REGION_END;
        } else {
            /* fake fragment_t, or code between last exit but before stubs or padding */
            stop_pc = fragment_body_end_pc(dcontext, f);
            if (PAD_FRAGMENT_JMPS(f->flags) && stop_pc != raw_start_pc) {
                /* We need to adjust stop_pc to account for any padding, only
                 * way any code could get here is via client interface,
                 * and there really is no nice way to distinguish it
                 * from any padding we added.
                 * PR 213005: we do not support decode_fragment() for bbs
                 * that have code added beyond the last exit cti (we turn
                 * off FRAG_COARSE_GRAIN and set FRAG_CANNOT_BE_TRACE).
                 * Sanity check, make sure it at least looks like there is no
                 * code here */
                ASSERT(IS_SET_TO_DEBUG(raw_start_pc, stop_pc - raw_start_pc));
                stop_pc = raw_start_pc;
            }
        }
        IF_X64(ASSERT(TEST(FRAG_FAKE, f->flags) /* no copy made */ ||
                      CHECK_TRUNCATE_TYPE_uint((stop_pc - raw_start_pc))));
        num_bytes = (uint)(stop_pc - raw_start_pc);
        LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext),
            "decoding fragment from " PFX " to " PFX "\n", raw_start_pc, stop_pc);
        if (num_bytes > 0) {
            if (buf != NULL) {
                if (TEST(FRAG_FAKE, f->flags)) {
                    /* we don't know the size of f, so we copy later, though
                     * we do point instrs into buf before we copy!
                     */
                } else {
                    /* first copy entire sequence up to exit cti into buf
                     * so we don't have to copy it in pieces if we find cti's, if we don't
                     * find any we want one giant piece anyway
                     */
                    ASSERT(cur_buf + num_bytes < buf + *bufsz);
                    memcpy(cur_buf, raw_start_pc, num_bytes);
                    top_buf = cur_buf + num_bytes;
                    LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext),
                        "decode_fragment: copied " PFX "-" PFX " to " PFX "-" PFX "\n",
                        raw_start_pc, raw_start_pc + num_bytes, cur_buf,
                        cur_buf + num_bytes);
                    /* cur_buf is incremented later -- it always points to start
                     * of raw bytes for next-to-add-to-ilist instr, while
                     * top_buf points to top of copied-to-buf data
                     */
                }
            } else {
                /* point at bits in code cache */
                cur_buf = raw_start_pc;
            }
            /* now, we can't make a single raw instr for all that, there may
             * be calls with off-fragment targets in there that need to be
             * re-pc-relativized (instrumentation, etc. insert calls), or
             * we may not even know where the exit ctis are (coarse-grain fragments),
             * so walk through (original bytes!) and decode, looking for cti's
             */
            instr = instr_create(dcontext);
            pc = raw_start_pc;
            /* do we have to translate the store of xcx from tls to dcontext?
             * be careful -- there can be private bbs w/ indirect branches, so
             * must see if this is a shared fragment we're adding
             */
            tls_to_dc = (shared_to_private && !DYNAMO_OPTION(private_ib_in_tls) &&
                         /* if l==NULL (coarse src) we'll check for xcx every time */
                         (l == NULL || LINKSTUB_INDIRECT(l->flags)));
            do {
#ifdef WINDOWS
                cache_pc prev_decode_pc = prev_pc; /* store the address of the
                                                    * previous decode, the instr
                                                    * before the one 'pc'
                                                    * currently points to *before*
                                                    * the call to decode() just
                                                    * below */
#endif
                /* For frozen coarse fragments, ubr eliding forces us to check
                 * every instr for a potential next fragment start.  This is
                 * expensive so users are advised to decode from app code if
                 * possible (case 9325 -- need exact re-mangle + re-instrument),
                 * though -coarse_pclookup_table helps.
                 */
                if (info != NULL && info->frozen && coarse_elided_ubrs &&
                    pc != start_pc) {
                    /* case 6532: check for ib stubs as we elide the jmp there too */
                    bool stop = false;
                    if (coarse_is_indirect_stub(pc)) {
                        stop = true;
                        LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                            "\thit ib stub @" PFX "\n", pc);
                    } else {
                        app_pc tag = fragment_coarse_entry_pclookup(dcontext, info, pc);
                        if (tag != NULL) {
                            stop = true;
                            LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                                "\thit frozen tgt: " PFX "." PFX "\n", tag, pc);
                        }
                    }
                    if (stop) {
                        /* Add the ubr ourselves */
                        ASSERT(cti == NULL);
                        cti = XINST_CREATE_jump(dcontext, opnd_create_pc(pc));
                        /* It's up to the caller to decide whether to mark this
                         * as do-not-emit or not */
                        /* Process as an exit cti */
                        stop_pc = pc;
                        pc = stop_pc;
                        break;
                    }
                }
                instr_reset(dcontext, instr);
                prev_pc = pc;
                pc = IF_AARCH64_ELSE(decode_cti_with_ldstex, decode_cti)(dcontext, pc,
                                                                         instr);
                DOLOG(DF_LOGLEVEL(dcontext), LOG_INTERP,
                      { disassemble_with_info(dcontext, prev_pc, THREAD, true, true); });
#ifdef WINDOWS
                /* Perform fixups for ignorable syscalls on XP & 2003. */
                if (possible_ignorable_sysenter && instr_opcode_valid(instr) &&
                    instr_is_syscall(instr)) {

                    /* We want to find the instr preceding the sysenter and have
                     * it point to the post-sysenter instr in the trace, rather than
                     * remain pointing to the post-sysenter instr in the BB.
                     */
                    instr_t *sysenter_prev;
                    instr_t *sysenter_post;

                    ASSERT(prev_decode_pc != NULL);
                    LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext),
                        "decode_fragment: sysenter found @" PFX "\n",
                        instr_get_raw_bits(instr));

                    /* create single raw instr for instructions up to the
                     * sysenter EXCEPT for the immediately preceding instruction
                     */
                    offset = (int)(prev_decode_pc - raw_start_pc);
                    ASSERT(offset > 0);
                    raw_instr = instr_create(dcontext);
                    /* point to buffer bits */
                    instr_set_raw_bits_trace_buf(raw_instr, cur_buf, offset);
                    instrlist_append(ilist, raw_instr);
                    cur_buf += offset;

                    /* Get the "mov" instr just before the sysenter. We know that
                     * it's there because mangle put it there, so we can safely
                     * decode at prev_decode_pc.
                     */
                    sysenter_prev = instr_create(dcontext);
                    decode(dcontext, prev_decode_pc, sysenter_prev);
                    ASSERT(instr_valid(instr) && instr_is_mov_imm_to_tos(sysenter_prev));
                    instrlist_append(ilist, sysenter_prev);
                    cur_buf += instr_length(dcontext, sysenter_prev);

                    /* Append the sysenter. */
                    instr_set_raw_bits_trace_buf(instr, cur_buf, (int)(pc - prev_pc));
                    instrlist_append(ilist, instr);
                    instr_set_meta(instr);

                    /* skip current instr -- the sysenter */
                    cur_buf += (int)(pc - prev_pc);

                    /* Decode the next instr -- the one after the sysenter. */
                    sysenter_post = instr_create(dcontext);
                    prev_decode_pc = pc;
                    prev_pc = pc;
                    pc = decode(dcontext, pc, sysenter_post);
                    if (DYNAMO_OPTION(ignore_syscalls_follow_sysenter))
                        ASSERT(!instr_is_cti(sysenter_post));
                    raw_start_pc = pc;

                    /* skip the post-sysenter instr */
                    cur_buf += (int)(pc - prev_pc);

                    instrlist_append(ilist, sysenter_post);
                    /* Point the pre-sysenter mov to the post-sysenter instr. */
                    instr_set_src(sysenter_prev, 0, opnd_create_instr(sysenter_post));
                    instr_set_meta(sysenter_prev);
                    instr_set_meta(sysenter_post);

                    DOLOG(DF_LOGLEVEL(dcontext), LOG_INTERP, {
                        LOG(THREAD, LOG_INTERP, DF_LOGLEVEL(dcontext),
                            "Post-sysenter -- F%d (" PFX ") into:\n", f->id, f->tag);
                        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
                    });

                    /* Set all local state so that we can fall-thru and correctly
                     * process the post-sysenter instruction. Point instr to the
                     * already decoded instruction, sysenter_post. At this point,
                     * pc and raw_start_pc point to just after sysenter_post,
                     * prev_pc points to sysenter_post, prev_decode_pc points to
                     * the sysenter itself, and cur_buf points to post_sysenter.
                     */
                    instr = sysenter_post;
                }
#endif
                /* look for a cti with an off-fragment target */
                if (instr_opcode_valid(instr) && instr_is_cti(instr)) {
                    bool separate_cti = false;
                    bool re_relativize = false;
                    bool intra_target = true;
                    DOLOG(DF_LOGLEVEL(dcontext), LOG_MONITOR, {
                        d_r_loginst(dcontext, 4, instr,
                                    "decode_fragment: found non-exit cti");
                    });
                    if (TEST(FRAG_FAKE, f->flags)) {
                        /* Case 8711: we don't know the size so we can't even
                         * distinguish off-fragment from intra-fragment targets.
                         * Thus we have to assume that any cti is an exit cti, and
                         * make all fragments for which that is not true into
                         * fine-grained.
                         * Except that we want to support intra-fragment ctis for
                         * clients (i#665), so we use some heuristics.
                         */
                        if (instr_is_cti_short_rewrite(instr, prev_pc)) {
                            /* Pull in the two short jmps for a "short-rewrite" instr.
                             * We must do this before asking whether it's an
                             * intra-fragment so we don't just look at the
                             * first part of the sequence.
                             */
                            pc = remangle_short_rewrite(dcontext, instr, prev_pc,
                                                        0 /*same target*/);
                        }
                        if (!coarse_cti_is_intra_fragment(dcontext, info, instr,
                                                          start_pc)) {
                            /* Process this cti as an exit cti.  FIXME: we will then
                             * re-copy the raw bytes from this cti to the end of the
                             * fragment at the top of the next loop iter, but for
                             * coarse-grain bbs that should be just one instr for cbr bbs
                             * or none for others, so not worth doing anything about.
                             */
                            DOLOG(DF_LOGLEVEL(dcontext), LOG_MONITOR, {
                                d_r_loginst(dcontext, DF_LOGLEVEL(dcontext), instr,
                                            "\tcoarse exit cti");
                            });
                            intra_target = false;
                            stop_pc = prev_pc;
                            pc = stop_pc;
                            break;
                        } else {
                            /* we'll make it to intra_target if() below */
                            DOLOG(DF_LOGLEVEL(dcontext), LOG_MONITOR, {
                                d_r_loginst(dcontext, DF_LOGLEVEL(dcontext), instr,
                                            "\tcoarse intra-fragment cti");
                            });
                        }
                    } else if (instr_is_return(instr) ||
                               !opnd_is_near_pc(instr_get_target(instr))) {
                        /* just leave it undecoded */
                        intra_target = false;
                    } else if (instr_is_cti_short_rewrite(instr, prev_pc)) {
                        /* Cti-short should only occur as exit ctis, which are
                         * separated out unless we're decoding a fake fragment.  We
                         * include this case for future use, as otherwise we'll
                         * decode just the short cti and think it is an
                         * intra-fragment cti.
                         */
                        ASSERT_NOT_REACHED();
                        separate_cti = true;
                        re_relativize = true;
                        intra_target = false;
                    } else if (opnd_get_pc(instr_get_target(instr)) < start_pc ||
                               opnd_get_pc(instr_get_target(instr)) >
                                   start_pc + f->size) {
                        separate_cti = true;
                        re_relativize = true;
                        intra_target = false;
                        DOLOG(DF_LOGLEVEL(dcontext), LOG_MONITOR, {
                            d_r_loginst(dcontext, 4, instr,
                                        "\tcti has off-fragment target");
                        });
                    }
                    if (intra_target) {
                        /* intra-fragment target: we'll change its target operand
                         * from pc to instr_t in second pass, so remember it here
                         */
                        instr_t *clone = instr_clone(dcontext, instr);
                        /* HACK: use note field! */
                        instr_set_note(clone, (void *)instr);
                        /* we leave the clone pointing at valid original raw bits */
                        instrlist_append(&intra_ctis, clone);
                        /* intra-fragment target */
                        DOLOG(DF_LOGLEVEL(dcontext), LOG_MONITOR, {
                            d_r_loginst(dcontext, 4, instr,
                                        "\tcti has intra-fragment target");
                        });
                        /* since the resulting instrlist could be manipulated,
                         * we need to change the target operand from pc to instr_t.
                         * that requires having this instr separated out now so
                         * our clone-in-note-field hack above works.
                         */
                        separate_cti = true;
                        re_relativize = false;
                    }
                    if (separate_cti) {
                        /* create single raw instr for instructions up to the cti */
                        offset = (int)(prev_pc - raw_start_pc);
                        if (offset > 0) {
                            raw_instr = instr_create(dcontext);
                            /* point to buffer bits */
                            instr_set_raw_bits_trace_buf(raw_instr, cur_buf, offset);
                            instrlist_append(ilist, raw_instr);
                            cur_buf += offset;
                            raw_start_pc = prev_pc;
                        }
                        /* now append cti, indicating that relative target must be
                         * re-encoded, and that it is not an exit cti
                         */
                        instr_set_meta(instr);
                        if (re_relativize)
                            instr_set_raw_bits_valid(instr, false);
                        else if (!instr_is_cti_short_rewrite(instr, NULL)) {
                            instr_set_raw_bits_trace_buf(instr, cur_buf,
                                                         (int)(pc - prev_pc));
                        }
                        instrlist_append(ilist, instr);
                        /* include buf for off-fragment cti, to simplify assert below */
                        cur_buf += (int)(pc - prev_pc);
                        raw_start_pc = pc;
                        /* create new instr for future fast decodes */
                        instr = instr_create(dcontext);
                    }
                } /* is cti */
                /* instr_is_tls_xcx_spill won't upgrade from level 1 */
                else if (tls_to_dc && instr_is_tls_xcx_spill(instr)) {
                    /* shouldn't get here for x64, where everything uses tls */
                    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
                    LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext),
                        "mangling xcx save from tls to dcontext\n");
                    /* create single raw instr for instructions up to the xcx save */
                    offset = (int)(prev_pc - raw_start_pc);
                    if (offset > 0) {
                        raw_instr = instr_create(dcontext);
                        /* point to buffer bits */
                        instr_set_raw_bits_trace_buf(raw_instr, cur_buf, offset);
                        instrlist_append(ilist, raw_instr);
                        cur_buf += offset;
                        raw_start_pc = prev_pc;
                    }
                    /* now append our new xcx save */
                    instrlist_append(ilist,
                                     instr_create_save_to_dcontext(dcontext, SCRATCH_REG2,
                                                                   SCRATCH_REG2_OFFS));
                    /* make sure skip current instr */
                    cur_buf += (int)(pc - prev_pc);
                    raw_start_pc = pc;
                }
#if defined(X86) && defined(X64)
                else if (instr_has_rel_addr_reference(instr)) {
                    /* We need to re-relativize, which is done automatically only for
                     * level 1 instrs (PR 251479), and only when raw bits point to
                     * their original location.  We assume that all the if statements
                     * above end up creating a high-level instr, so a cti w/ a
                     * rip-rel operand is already covered.
                     */
                    /* create single raw instr for instructions up to this one */
                    offset = (int)(prev_pc - raw_start_pc);
                    if (offset > 0) {
                        raw_instr = instr_create(dcontext);
                        /* point to buffer bits */
                        instr_set_raw_bits_trace_buf(raw_instr, cur_buf, offset);
                        instrlist_append(ilist, raw_instr);
                        cur_buf += offset;
                        raw_start_pc = prev_pc;
                    }
                    /* should be valid right now since pointing at original bits */
                    ASSERT(instr_rip_rel_valid(instr));
                    if (buf != NULL) {
                        /* re-relativize into the new buffer */
                        DEBUG_DECLARE(byte *nxt =)
                        instr_encode_to_copy(dcontext, instr, cur_buf,
                                             vmcode_get_executable_addr(cur_buf));
                        instr_set_raw_bits_trace_buf(instr,
                                                     vmcode_get_executable_addr(cur_buf),
                                                     (int)(pc - prev_pc));
                        instr_set_rip_rel_valid(instr, true);
                        ASSERT(nxt != NULL);
                    }
                    instrlist_append(ilist, instr);
                    cur_buf += (int)(pc - prev_pc);
                    raw_start_pc = pc;
                    /* create new instr for future fast decodes */
                    instr = instr_create(dcontext);
                }
#endif
            } while (pc < stop_pc);
            DODEBUG({
                if (pc != stop_pc) {
                    LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext),
                        "PC " PFX ", stop_pc " PFX "\n", pc, stop_pc);
                }
            });
            ASSERT(pc == stop_pc);
            cache_pc next_pc = pc;
            if (l != NULL && TEST(LINK_PADDED, l->flags) && instr_is_nop(instr)) {
                /* Throw away our padding nop. */
                LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                    "%s: removing padding nop @" PFX "\n", __FUNCTION__, prev_pc);
                pc = prev_pc;
                if (buf != NULL)
                    top_buf -= instr_length(dcontext, instr);
            }
            /* create single raw instr for rest of instructions up to exit cti */
            if (pc > raw_start_pc) {
                instr_reset(dcontext, instr);
                /* point to buffer bits */
                offset = (int)(pc - raw_start_pc);
                if (offset > 0) {
                    instr_set_raw_bits_trace_buf(instr, cur_buf, offset);
                    instrlist_append(ilist, instr);
                    cur_buf += offset;
                }
                if (buf != NULL && TEST(FRAG_FAKE, f->flags)) {
                    /* Now that we know the size we can copy into buf.
                     * We have been incrementing cur_buf all along, though
                     * we didn't have contents there.
                     */
                    ASSERT(top_buf < cur_buf);
                    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((cur_buf - top_buf))));
                    num_bytes = (uint)(cur_buf - top_buf);
                    ASSERT(cur_buf + num_bytes < buf + *bufsz);
                    memcpy(cur_buf, raw_start_pc, num_bytes);
                    top_buf = cur_buf + num_bytes;
                    LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext),
                        "decode_fragment: copied " PFX "-" PFX " to " PFX "-" PFX "\n",
                        raw_start_pc, raw_start_pc + num_bytes, cur_buf,
                        cur_buf + num_bytes);
                }
                ASSERT(buf == NULL || cur_buf == top_buf);
            } else {
                /* will reach here if had a processed instr (off-fragment target, etc.)
                 * immediately prior to exit cti, so now don't need instr -- an
                 * example (in absence of clients) is trampoline to interception code
                 */
                instr_destroy(dcontext, instr);
            }
            pc = next_pc;
        }

        if (l == NULL && !TEST(FRAG_FAKE, f->flags))
            break;

        /* decode the exit branch */
        if (cti != NULL) {
            /* already created */
            instr = cti;
            ASSERT(info != NULL && info->frozen && instr_is_ubr(instr));
            raw_start_pc = pc;
        } else {
            instr = instr_create(dcontext);
            raw_start_pc = decode(dcontext, stop_pc, instr);
            ASSERT(raw_start_pc != NULL); /* our own code! */
            /* pc now points into fragment! */
        }
        ASSERT(instr_is_ubr(instr) || instr_is_cbr(instr));
        /* replace fcache target with target_tag and add to fragment */
        if (l == NULL) {
            app_pc instr_tgt;
            /* Ensure we get proper target for short cti sequence */
            if (instr_is_cti_short_rewrite(instr, stop_pc))
                remangle_short_rewrite(dcontext, instr, stop_pc, 0 /*same target*/);
            instr_tgt = opnd_get_pc(instr_get_target(instr));
            ASSERT(TEST(FRAG_COARSE_GRAIN, f->flags));
            if (cti == NULL && coarse_is_entrance_stub(instr_tgt)) {
                target_tag = entrance_stub_target_tag(instr_tgt, info);
                l_flags = LINK_DIRECT;
                /* FIXME; try to get LINK_JMP vs LINK_CALL vs fall-through? */
                LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                    "\tstub tgt: " PFX " => " PFX "\n", instr_tgt, target_tag);
            } else if (instr_tgt == raw_start_pc /*target next instr*/
                       /* could optimize by not checking for stub if
                        * coarse_elided_ubrs but we need to know whether ALL
                        * ubrs were elided, which we don't know as normally
                        * entire-bb-ubrs are not elided (case 9677).
                        * plus now that we elide jmp-to-ib-stub we must check.
                        */
                       && coarse_is_indirect_stub(instr_tgt)) {
                ibl_type_t ibl_type;
                DEBUG_DECLARE(bool is_ibl;)
                target_tag = coarse_indirect_stub_jmp_target(instr_tgt);
                l_flags = LINK_INDIRECT;
                DEBUG_DECLARE(is_ibl =)
                get_ibl_routine_type_ex(dcontext, target_tag, &ibl_type _IF_X86_64(NULL));
                ASSERT(is_ibl);
                l_flags |= ibltype_to_linktype(ibl_type.branch_type);
                LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                    "\tind stub tgt: " PFX " => " PFX "\n", instr_tgt, target_tag);
            } else {
                target_tag = fragment_coarse_entry_pclookup(dcontext, info, instr_tgt);
                /* Only frozen units don't jump through stubs */
                ASSERT(info != NULL && info->frozen);
                ASSERT(target_tag != NULL);
                l_flags = LINK_DIRECT;
                LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                    "\tfrozen tgt: " PFX "." PFX "\n", target_tag, instr_tgt);
            }
        } else {
            target_tag = EXIT_TARGET_TAG(dcontext, f, l);
            l_flags = l->flags;
        }
        if (LINKSTUB_DIRECT(l_flags))
            num_dir++;
        else
            num_indir++;
        ASSERT(target_tag != NULL);
        if (instr_is_cti_short_rewrite(instr, stop_pc)) {
            raw_start_pc = remangle_short_rewrite(dcontext, instr, stop_pc, target_tag);
        } else {
            app_pc new_target = target_tag;
            /* don't point to fcache bits */
            instr_set_raw_bits_valid(instr, false);
            LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                "decode_fragment exit_cti: pc=" PFX " l->target_tag=" PFX
                " l->flags=0x%x\n",
                stop_pc, target_tag, l_flags);

            /* need to propagate exit branch type flags,
             * instr_t flag copied from old fragment linkstub
             * TODO: when ibl targets are different this won't be necessary
             */
            instr_exit_branch_set_type(instr, linkstub_propagatable_flags(l_flags));

            /* convert to proper ibl */
            if (is_indirect_branch_lookup_routine(dcontext, target_tag)) {
                DEBUG_DECLARE(app_pc old_target = new_target;)
                new_target =
                    get_alternate_ibl_routine(dcontext, target_tag, target_flags);
                ASSERT(new_target != NULL);

                /* for stats on traces, we assume if target_flags contains
                 * FRAG_IS_TRACE then we are extending a trace
                 */
                DODEBUG({
                    LOG(THREAD, LOG_MONITOR, DF_LOGLEVEL(dcontext) - 1,
                        "%s: %s ibl_routine " PFX " with %s_target=" PFX "\n",
                        TEST(FRAG_IS_TRACE, target_flags) ? "extend_trace"
                                                          : "decode_fragment",
                        new_target == old_target ? "maintaining" : "replacing",
                        old_target, new_target == old_target ? "old" : "new", new_target);
                    STATS_INC(num_traces_ibl_extended);
                });
#ifdef WINDOWS
                DOSTATS({
                    if (TEST(FRAG_IS_TRACE, target_flags) &&
                        old_target == shared_syscall_routine(dcontext))
                        STATS_INC(num_traces_shared_syscall_extended);
                });
#endif
            }

            instr_set_target(instr, opnd_create_pc(new_target));

            if (instr_is_cti_short(instr)) {
                /* make sure non-mangled short ctis, which are generated by
                 * us and never left there from apps, are not marked as exit ctis
                 */
                instr_set_meta(instr);
            }
        }
        instrlist_append(ilist, instr);

        if (TEST(FRAG_FAKE, f->flags)) {
            /* Assumption: coarse-grain bbs have 1 ind exit or 2 direct,
             * and no code beyond the last exit!  Of course frozen bbs
             * can have their final jmp elided, which we handle above.
             */
            if (instr_is_ubr(instr)) {
                break;
            }
        }
        if (l != NULL) /* if NULL keep going: discovering exits as we go */
            l = LINKSTUB_NEXT_EXIT(l);
    } /* end while(true) loop through exit stubs */

    /* now fix up intra-trace cti targets */
    if (instrlist_first(&intra_ctis) != NULL) {
        /* We have to undo all of our level 0 blocks by expanding.
         * Any instrs that need re-relativization should already be
         * separate, so this should not affect rip-rel instrs.
         */
        int offs = 0;
        for (instr = instrlist_first_expanded(dcontext, ilist); instr != NULL;
             instr = instr_get_next_expanded(dcontext, ilist, instr)) {
            for (cti = instrlist_first(&intra_ctis); cti != NULL;
                 cti = instr_get_next(cti)) {
                /* The clone we put in intra_ctis has raw bits equal to the
                 * original bits, so its target will be in original fragment body.
                 * We can't rely on the raw bits of the new instrs (since the
                 * non-level-0 ones may have allocated raw bits) so we
                 * calculate a running offset as we go.
                 */
                if (opnd_get_pc(instr_get_target(cti)) - start_pc == offs) {
                    /* cti targets this instr */
                    instr_t *real_cti = (instr_t *)instr_get_note(cti);
                    /* PR 333691: do not preserve raw bits of real_cti, since
                     * instrlist may change (e.g., inserted nops).  Must re-encode
                     * once instrlist is finalized.
                     */
                    instr_set_target(real_cti, opnd_create_instr(instr));
                    DOLOG(DF_LOGLEVEL(dcontext), LOG_MONITOR, {
                        d_r_loginst(dcontext, 4, real_cti,
                                    "\tre-set intra-fragment target");
                    });
                    break;
                }
            }
            offs += instr_length(dcontext, instr);
        }
    }

    instrlist_clear(dcontext, &intra_ctis);
    DOLOG(DF_LOGLEVEL(dcontext), LOG_INTERP, {
        LOG(THREAD, LOG_INTERP, DF_LOGLEVEL(dcontext),
            "Decoded F%d (" PFX "." PFX ") into:\n", f->id, f->tag, FCACHE_ENTRY_PC(f));
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
    });

    DEBUG_DECLARE(ok =) dr_set_isa_mode(dcontext, old_mode, NULL);
    ASSERT(ok);

    if (dir_exits != NULL)
        *dir_exits = num_dir;
    if (indir_exits != NULL)
        *indir_exits = num_indir;
    if (buf != NULL) {
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((top_buf - buf))));
        *bufsz = (uint)(top_buf - buf);
    }
    return ilist;
}
#undef DF_LOGLEVEL

/* Just like decode_fragment() but marks any instrs missing in the cache
 * as do-not-emit
 */
instrlist_t *
decode_fragment_exact(dcontext_t *dcontext, fragment_t *f, byte *buf,
                      /*IN/OUT*/ uint *bufsz, uint target_flags,
                      /*OUT*/ uint *dir_exits, /*OUT*/ uint *indir_exits)
{
    instrlist_t *ilist =
        decode_fragment(dcontext, f, buf, bufsz, target_flags, dir_exits, indir_exits);
    /* If the final jmp was elided we do NOT want to count it in the size! */
    if (instr_get_raw_bits(instrlist_last(ilist)) == NULL) {
        instr_set_ok_to_emit(instrlist_last(ilist), false);
    }
    return ilist;
}

/* Makes a new copy of fragment f
 * If replace is true,
 *   removes f from the fcache and adds the new copy in its place
 * Else
 *   creates f as an invisible fragment (caller is responsible for linking
 *   the new fragment!)
 */
fragment_t *
copy_fragment(dcontext_t *dcontext, fragment_t *f, bool replace)
{
    instrlist_t *trace = instrlist_create(dcontext);
    instr_t *instr;
    uint *trace_buf;
    int trace_buf_top; /* index of next free location in trace_buf */
    linkstub_t *l;
    byte *p;
    cache_pc start_pc;
    int num_bytes;
    fragment_t *new_f;
    void *vmlist = NULL;
    app_pc target_tag;
    DEBUG_DECLARE(bool ok;)

    trace_buf = heap_alloc(dcontext, f->size * 2 HEAPACCT(ACCT_FRAGMENT));

    start_pc = FCACHE_ENTRY_PC(f);
    trace_buf_top = 0;
    p = ((byte *)trace_buf) + trace_buf_top;

    IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* must re-relativize when copying! */

    for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
        /* Copy the instruction bytes up to (but not including) the first
         * control-transfer instruction.  ***WARNING*** This code assumes
         * that the first link stub corresponds to the first exit branch
         * in the body. */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((EXIT_CTI_PC(f, l) - start_pc))));
        num_bytes = (uint)(EXIT_CTI_PC(f, l) - start_pc);
        if (num_bytes > 0) {
            memcpy(p, (byte *)start_pc, num_bytes);
            trace_buf_top += num_bytes;
            start_pc += num_bytes;

            /* build a mongo instruction corresponding to the copied instructions */
            instr = instr_create(dcontext);
            instr_set_raw_bits(instr, p, num_bytes);
            instrlist_append(trace, instr);
        }

        /* decode the exit branch */
        instr = instr_create(dcontext);
        p = decode(dcontext, (byte *)EXIT_CTI_PC(f, l), instr);
        ASSERT(p != NULL); /* our own code! */
        /* p now points into fragment! */
        ASSERT(instr_is_ubr(instr) || instr_is_cbr(instr));
        /* Replace cache_pc target with target_tag and add to trace.  For
         * an indirect branch, the target_tag is zero. */
        target_tag = EXIT_TARGET_TAG(dcontext, f, l);
        ASSERT(target_tag);
        if (instr_is_cti_short_rewrite(instr, EXIT_CTI_PC(f, l))) {
            p = remangle_short_rewrite(dcontext, instr, EXIT_CTI_PC(f, l), target_tag);
        } else {
            /* no short ctis that aren't mangled should be exit ctis */
            ASSERT(!instr_is_cti_short(instr));
            instr_set_target(instr, opnd_create_pc(target_tag));
        }
        instrlist_append(trace, instr);
        start_pc += (p - (byte *)EXIT_CTI_PC(f, l));
    }

    /* emit as invisible fragment */
    /* We don't support shared fragments, where vm_area_add_to_list can fail */
    ASSERT_NOT_IMPLEMENTED(!TEST(FRAG_SHARED, f->flags));
    DEBUG_DECLARE(ok =)
    vm_area_add_to_list(dcontext, f->tag, &vmlist, f->flags, f, false /*no locks*/);
    ASSERT(ok); /* should never fail for private fragments */
    new_f = emit_invisible_fragment(dcontext, f->tag, trace, f->flags, vmlist);
    if (replace) {
        /* link and replace old fragment */
        shift_links_to_new_fragment(dcontext, f, new_f);
        fragment_replace(dcontext, f, new_f);
    } else {
        /* caller is responsible for linking new fragment */
    }

    ASSERT(new_f->flags == f->flags);

    fragment_copy_data_fields(dcontext, f, new_f);

#ifdef DEBUG
    if (d_r_stats->loglevel > 1) {
        LOG(THREAD, LOG_ALL, 2, "Copying F%d to F%d\n", f->id, new_f->id);
        disassemble_fragment(dcontext, f, d_r_stats->loglevel < 3);
        disassemble_fragment(dcontext, new_f, d_r_stats->loglevel < 3);
    }
#endif /* DEBUG */

    heap_free(dcontext, trace_buf, f->size * 2 HEAPACCT(ACCT_FRAGMENT));
    /* free the instrlist_t elements */
    instrlist_clear_and_destroy(dcontext, trace);
    if (replace) {
        fragment_delete(dcontext, f,
                        FRAGDEL_NO_OUTPUT | FRAGDEL_NO_UNLINK | FRAGDEL_NO_HTABLE);
        STATS_INC(num_fragments_deleted_copy_and_replace);
    }
    return new_f;
}

/* Used when the code cache is enlarged by copying to a larger space,
 * and all of the relative ctis that target outside the cache need
 * to be shifted. Additionally, sysenter-related patching for ignore-syscalls
 * on XP/2003 is performed here, as the absolute code cache address pushed
 * onto the stack must be updated.
 * Assumption: old code cache has been copied to TOP of new cache, so to
 * detect for ctis targeting outside of old cache can look at new cache
 * start plus old cache size.
 */
void
shift_ctis_in_fragment(dcontext_t *dcontext, fragment_t *f, ssize_t shift,
                       cache_pc fcache_start, cache_pc fcache_end, size_t old_size)
{
    cache_pc pc, prev_pc = NULL;
    cache_pc start_pc = FCACHE_ENTRY_PC(f);
    cache_pc stop_pc = fragment_stubs_end_pc(f);
    /* get what would have been end of cache if just shifted not resized */
    cache_pc fcache_old_end = fcache_start + old_size;
#ifdef WINDOWS
    /* The fragment could contain an ignorable sysenter instruction if
     * the following conditions are satisfied. */
    bool possible_ignorable_sysenter = DYNAMO_OPTION(ignore_syscalls) &&
        (get_syscall_method() == SYSCALL_METHOD_SYSENTER) &&
        /* FIXME Traces don't have FRAG_HAS_SYSCALL set so we can't filter on
         * that flag for all fragments. */
        (TEST(FRAG_HAS_SYSCALL, f->flags) || TEST(FRAG_IS_TRACE, f->flags));
#endif
    instr_t instr;
    instr_init(dcontext, &instr);

    pc = start_pc;
    while (pc < stop_pc) {
#ifdef WINDOWS
        cache_pc prev_decode_pc = prev_pc; /* store the address of the
                                            * previous decode, the instr
                                            * before the one 'pc'
                                            * currently points to *before*
                                            * the call to decode_cti() just
                                            * below */
#endif
        prev_pc = pc;
        instr_reset(dcontext, &instr);
        pc = (cache_pc)decode_cti(dcontext, (byte *)pc, &instr);
#ifdef WINDOWS
        /* Perform fixups for sysenter instrs when ignorable syscalls is used on
         * XP & 2003. These are not cache-external fixups, but it's convenient &
         * efficient to perform them here since decode_cti() is called on every
         * instruction, allowing identification of sysenters without additional
         * decoding.
         */
        if (possible_ignorable_sysenter && instr_opcode_valid(&instr) &&
            instr_is_syscall(&instr)) {

            cache_pc next_pc;
            app_pc target;
            DEBUG_DECLARE(app_pc old_target;)
            DEBUG_DECLARE(cache_pc encode_nxt;)

            /* Peek up to find the "mov $post-sysenter -> (%xsp)" */
            instr_reset(dcontext, &instr);
            next_pc = decode(dcontext, prev_decode_pc, &instr);
            ASSERT(next_pc == prev_pc);
            LOG(THREAD, LOG_MONITOR, 4,
                "shift_ctis_in_fragment: pre-sysenter mov found @" PFX "\n",
                instr_get_raw_bits(&instr));
            ASSERT(instr_is_mov_imm_to_tos(&instr));
            target = instr_get_raw_bits(&instr) + instr_length(dcontext, &instr) +
                (pc - prev_pc);
            DODEBUG(old_target = (app_pc)opnd_get_immed_int(instr_get_src(&instr, 0)););
            /* PR 253943: we don't support sysenter in x64 */
            IF_X64(ASSERT_NOT_IMPLEMENTED(false)); /* can't have 8-byte imm-to-mem */
            instr_set_src(&instr, 0, opnd_create_immed_int((ptr_int_t)target, OPSZ_4));
            ASSERT(old_target + shift == target);
            LOG(THREAD, LOG_MONITOR, 4,
                "shift_ctis_in_fragment: pre-sysenter mov now pts to @" PFX "\n", target);
            DEBUG_DECLARE(encode_nxt =)
            instr_encode_to_copy(dcontext, &instr,
                                 vmcode_get_writable_addr(prev_decode_pc),
                                 prev_decode_pc);
            /* must not change size! */
            ASSERT(encode_nxt != NULL &&
                   vmcode_get_executable_addr(encode_nxt) == next_pc);
        }
        /* The following 'if' won't get executed since a sysenter isn't
         * a CTI instr, so we don't need an else. We do need to take care
         * that any 'else' clauses are added after the 'if' won't trigger
         * on a sysenter either.
         */
#endif
        /* look for a pc-relative cti (including exit ctis) w/ out-of-cache
         * target (anything in-cache is fine, the whole cache was moved)
         */
        if (instr_is_cti(&instr) &&
            /* only ret, ret_far, and iret don't have targets, and
             * we really shouldn't see them, except possibly if they
             * are inserted through instrumentation, so go ahead and
             * check num srcs
             */
            instr_num_srcs(&instr) > 0 && opnd_is_near_pc(instr_get_target(&instr))) {
            app_pc target = opnd_get_pc(instr_get_target(&instr));
            if (target < fcache_start || target > fcache_old_end) {
                DEBUG_DECLARE(byte * nxt_pc;)
                /* re-encode instr w/ new pc-relative target */
                instr_set_raw_bits_valid(&instr, false);
                instr_set_target(&instr, opnd_create_pc(target - shift));
                DEBUG_DECLARE(nxt_pc =)
                instr_encode_to_copy(dcontext, &instr, vmcode_get_writable_addr(prev_pc),
                                     prev_pc);
                /* must not change size! */
                ASSERT(nxt_pc != NULL && vmcode_get_executable_addr(nxt_pc) == pc);
#ifdef DEBUG
                if ((d_r_stats->logmask & LOG_CACHE) != 0) {
                    d_r_loginst(
                        dcontext, 5, &instr,
                        "shift_ctis_in_fragment: found cti w/ out-of-cache target");
                }
#endif
            }
        }
    }
    instr_free(dcontext, &instr);
}

#ifdef PROFILE_RDTSC
/* Add profile call to front of the trace in dc
 * Must call finalize_profile_call and pass it the fragment_t*
 * once the trace is turned into a fragment to fix up a few profile
 * call instructions.
 */
void
add_profile_call(dcontext_t *dcontext)
{
    monitor_data_t *md = (monitor_data_t *)dcontext->monitor_field;
    instrlist_t *trace = &(md->trace);
    byte *p = ((byte *)md->trace_buf) + md->trace_buf_top;
    instr_t *instr;
    uint num_bytes = profile_call_size();
    ASSERT(num_bytes + md->trace_buf_top < md->trace_buf_size);

    insert_profile_call((cache_pc)p);

    /* use one giant BINARY instruction to hold everything,
     * to keep dynamo from interpreting the cti instructions as real ones
     */
    instr = instr_create(dcontext);
    instr_set_raw_bits(instr, p, num_bytes);
    instrlist_prepend(trace, instr);

    md->trace_buf_top += num_bytes;
}
#endif

/* emulates the effects of the instruction at pc with the state in mcontext
 * limited right now to only mov instructions
 * returns NULL if failed or not yet implemented, else returns the pc of the next instr.
 */
app_pc
d_r_emulate(dcontext_t *dcontext, app_pc pc, priv_mcontext_t *mc)
{
    instr_t instr;
    app_pc next_pc = NULL;
    uint opc;
    instr_init(dcontext, &instr);
    next_pc = decode(dcontext, pc, &instr);
    if (!instr_valid(&instr)) {
        next_pc = NULL;
        goto emulate_failure;
    }
    DOLOG(2, LOG_INTERP, { d_r_loginst(dcontext, 2, &instr, "emulating"); });
    opc = instr_get_opcode(&instr);
    if (opc == OP_store) {
        opnd_t src = instr_get_src(&instr, 0);
        opnd_t dst = instr_get_dst(&instr, 0);
        reg_t *target;
        reg_t val;
        uint sz = opnd_size_in_bytes(opnd_get_size(dst));
        ASSERT(opnd_is_memory_reference(dst));
        if (sz != 4 IF_X64(&&sz != 8)) {
            next_pc = NULL;
            goto emulate_failure;
        }
        target = (reg_t *)opnd_compute_address_priv(dst, mc);
        if (opnd_is_reg(src)) {
            val = reg_get_value_priv(opnd_get_reg(src), mc);
        } else if (opnd_is_immed_int(src)) {
            val = (reg_t)opnd_get_immed_int(src);
        } else {
            next_pc = NULL;
            goto emulate_failure;
        }
        DOCHECK(1, {
            uint prot = 0;
            ASSERT(get_memory_info((app_pc)target, NULL, NULL, &prot));
            ASSERT(TEST(MEMPROT_WRITE, prot));
        });
        LOG(THREAD, LOG_INTERP, 2, "\temulating store by writing " PFX " to " PFX "\n",
            val, target);
        if (sz == 4)
            *((int *)target) = (int)val;
#ifdef X64
        else if (sz == 8)
            *target = val;
#endif
    } else if (opc == IF_X86_ELSE(OP_inc, OP_add) || opc == IF_X86_ELSE(OP_dec, OP_sub)) {
        opnd_t src = instr_get_src(&instr, 0);
        reg_t *target;
        uint sz = opnd_size_in_bytes(opnd_get_size(src));
        if (sz != 4 IF_X64(&&sz != 8)) {
            next_pc = NULL;
            goto emulate_failure;
        }
        /* FIXME: handle changing register value */
        ASSERT(opnd_is_memory_reference(src));
        /* FIXME: change these to take in priv_mcontext_t* ? */
        target = (reg_t *)opnd_compute_address_priv(src, mc);
        DOCHECK(1, {
            uint prot = 0;
            ASSERT(get_memory_info((app_pc)target, NULL, NULL, &prot));
            ASSERT(TEST(MEMPROT_WRITE, prot));
        });
        LOG(THREAD, LOG_INTERP, 2, "\temulating %s to " PFX "\n",
            opc == IF_X86_ELSE(OP_inc, OP_add) ? "inc" : "dec", target);
        if (sz == 4) {
            if (opc == IF_X86_ELSE(OP_inc, OP_add))
                (*((int *)target))++;
            else
                (*((int *)target))--;
        }
#ifdef X64
        else if (sz == 8) {
            if (opc == IF_X86_ELSE(OP_inc, OP_add))
                (*target)++;
            else
                (*target)--;
        }
#endif
    }
emulate_failure:
    instr_free(dcontext, &instr);
    return next_pc;
}

#ifdef AARCH64
/* Emit addtional code to fix up indirect trace exit for AArch64.
 * For each indirect branch in trace we have the following code:
 *      str x0, TLS_REG0_SLOT
 *      mov x0, #trace_next_target
 *      eor x0, x0, jump_target
 *      cbnz x0, trace_exit (ibl_routine)
 *      ldr x0, TLS_REG0_SLOT
 * For the trace_exit (ibl_routine), it needs to conform to the
 * protocol specified in emit_indirect_branch_lookup in
 * aarch64/emit_utils.c.
 * The ibl routine requires:
 *     x2: contains indirect branch target
 *     TLS_REG2_SLOT: contains app's x2
 * Therefore we need to add addtional spill instructions
 * before we actually jump to the ibl routine.
 * We want the indirect hit path to have minimum instructions
 * and also conform to the protocol of ibl routine
 * Therefore we append the restore at the end of the trace
 * after the backward jump to trace head.
 * For example, the code will be fixed to:
 *      eor x0, x0, jump_target
 *      cbnz x0, trace_exit_label
 *      ...
 *      b trace_head
 * trace_exit_label:
 *      ldr x0, TLS_REG0_SLOT
 *      str x2, TLS_REG2_SLOT
 *      mov x2, jump_target
 *      b ibl_routine
 *
 * XXX i#2974 This way of having a trace_exit_label at the end of a trace
 * breaks the linear requirement which is assumed by a lot of code, including
 * translation. Currently recreation of instruction list is fixed by including
 * a special call to this function. We might need to consider add special
 * support in translate.c or use an alternative linear control flow.
 *
 */
int
fixup_indirect_trace_exit(dcontext_t *dcontext, instrlist_t *trace)
{
    instr_t *instr, *prev, *branch;
    instr_t *trace_exit_label;
    app_pc target = 0;
    app_pc ind_target = 0;
    app_pc instr_trans;
    reg_id_t scratch;
    reg_id_t jump_target_reg = DR_REG_NULL;
    uint indirect_type = 0;
    int added_size = 0;
    trace_exit_label = NULL;
    /* We record the original trace end */
    instr_t *trace_end = instrlist_last(trace);

    LOG(THREAD, LOG_MONITOR, 4, "fixup the indirect trace exit\n");

    /* It is possible that we have multiple indirect trace exits to fix up
     * when more than one basic blocks are added as the trace.
     * And so we iterate over the entire trace to look for indirect exits.
     */
    for (instr = instrlist_first(trace); instr != trace_end;
         instr = instr_get_next(instr)) {
        if (instr_is_exit_cti(instr)) {
            target = instr_get_branch_target_pc(instr);
            /* Check for indirect exit. */
            if (is_indirect_branch_lookup_routine(dcontext, (cache_pc)target)) {
                /* This branch must be a cbnz, or the last_cti was not fixed up. */
                ASSERT(instr->opcode == OP_cbnz);

                trace_exit_label = INSTR_CREATE_label(dcontext);
                ind_target = target;
                /* Modify the target of the cbnz. */
                instr_set_target(instr, opnd_create_instr(trace_exit_label));
                indirect_type = instr_exit_branch_type(instr);
                /* unset exit type */
                instr->flags &= ~EXIT_CTI_TYPES;
                instr_set_our_mangling(instr, true);

                /* Retrieve jump target reg from the xor instruction. */
                prev = instr_get_prev(instr);
                ASSERT(prev->opcode == OP_eor);

                ASSERT(instr_num_srcs(prev) == 4 && opnd_is_reg(instr_get_src(prev, 1)));
                jump_target_reg = opnd_get_reg(instr_get_src(prev, 1));

                ASSERT(ind_target && jump_target_reg != DR_REG_NULL);

                /* Choose any scratch register except the target reg. */
                scratch = (jump_target_reg == DR_REG_X0) ? DR_REG_X1 : DR_REG_X0;
                /* Add the trace exit label. */
                instrlist_append(trace, trace_exit_label);
                instr_trans = instr_get_translation(instr);
                /* ldr x0, TLS_REG0_SLOT */
                instrlist_append(trace,
                                 INSTR_XL8(instr_create_restore_from_tls(
                                               dcontext, scratch, TLS_REG0_SLOT),
                                           instr_trans));
                added_size += AARCH64_INSTR_SIZE;
                /* if x2 alerady contains the jump_target, then there is no need to store
                 * it away and load value of jump target into it
                 */
                if (jump_target_reg != IBL_TARGET_REG) {
                    /* str x2, TLS_REG2_SLOT */
                    instrlist_append(
                        trace,
                        INSTR_XL8(instr_create_save_to_tls(dcontext, IBL_TARGET_REG,
                                                           TLS_REG2_SLOT),
                                  instr_trans));
                    added_size += AARCH64_INSTR_SIZE;
                    /* mov IBL_TARGET_REG, jump_target */
                    ASSERT(jump_target_reg != DR_REG_NULL);
                    instrlist_append(
                        trace,
                        INSTR_XL8(XINST_CREATE_move(dcontext,
                                                    opnd_create_reg(IBL_TARGET_REG),
                                                    opnd_create_reg(jump_target_reg)),
                                  instr_trans));
                    added_size += AARCH64_INSTR_SIZE;
                }
                /* b ibl_target */
                branch = XINST_CREATE_jump(dcontext, opnd_create_pc(ind_target));
                instr_exit_branch_set_type(branch, indirect_type);
                instr_set_translation(branch, instr_trans);
                instrlist_append(trace, branch);
                added_size += AARCH64_INSTR_SIZE;
            }
        } else if ((instr->opcode == OP_cbz || instr->opcode == OP_cbnz ||
                    instr->opcode == OP_tbz || instr->opcode == OP_tbnz) &&
                   instr_is_load_tls(instr_get_next(instr))) {
            /* Don't invoke the decoder;
             * only mangled instruction (by mangle_cbr_stolen_reg) reached here.
             */

            instr_t *next = instr_get_next(instr);

            /* Get the actual target of the cbz/cbnz. */
            opnd_t fall_target = instr_get_target(instr);
            /* Create new label. */
            trace_exit_label = INSTR_CREATE_label(dcontext);
            instr_set_target(instr, opnd_create_instr(trace_exit_label));
            /* Insert restore at end of trace. */
            instrlist_append(trace, trace_exit_label);
            instr_trans = instr_get_translation(instr);
            /* ldr cbz_reg, TLS_REG0_SLOT */
            reg_id_t mangled_reg = ((*(uint *)next->bytes) & 31) + DR_REG_START_GPR;
            instrlist_append(trace,
                             INSTR_XL8(instr_create_restore_from_tls(
                                           dcontext, mangled_reg, TLS_REG0_SLOT),
                                       instr_trans));
            added_size += AARCH64_INSTR_SIZE;
            /* b fall_target */
            branch = XINST_CREATE_jump(dcontext, fall_target);
            instr_set_translation(branch, instr_trans);
            instrlist_append(trace, branch);
            added_size += AARCH64_INSTR_SIZE;
            /* Because of the jump, a new stub was created.
             * and it is possible that this jump is leaving the fragment
             * in which case we should not increase the size of the fragment
             */
            if (instr_is_exit_cti(branch)) {
                added_size += DIRECT_EXIT_STUB_SIZE(0);
            }
        }
    }
    return added_size;
}
#endif
