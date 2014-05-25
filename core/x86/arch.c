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

/*
 * arch.c - x86 architecture specific routines
 */

#include "../globals.h"
#include "../link.h"
#include "../fragment.h"

#include "arch.h"
#include "instr.h"
#include "instr_create.h"
#include "decode.h"
#include "decode_fast.h"
#include "../fcache.h"
#include "proc.h"
#include "instrument.h"

#include <string.h> /* for memcpy */

#if defined(DEBUG) || defined(INTERNAL)
# include "disassemble.h"
#endif

/* in interp.c */
void interp_init(void);
void interp_exit(void);

/* Thread-shared generated routines.
 * We don't allocate the shared_code statically so that we can mark it
 * executable.
 */
generated_code_t *shared_code = NULL;
#ifdef X64
/* PR 282576: For WOW64 processes we need context switches that swap between 64-bit
 * mode and 32-bit mode when executing 32-bit code cache code, as well as
 * 32-bit-targeted IBL routines for performance.
 */
generated_code_t *shared_code_x86 = NULL;
/* In x86_to_x64 we can use the extra registers as scratch space.
 * The IBL routines are 64-bit and they use r8-r10 freely.
 */
generated_code_t *shared_code_x86_to_x64 = NULL;
#endif

static int syscall_method = SYSCALL_METHOD_UNINITIALIZED;
byte *app_sysenter_instr_addr = NULL;
#ifdef LINUX
static bool sysenter_hook_failed = false;
#endif

/* static functions forward references */
static byte *
emit_ibl_routines(dcontext_t *dcontext, generated_code_t *code,
                  byte *pc, byte *fcache_return_pc,
                  ibl_source_fragment_type_t source_fragment_type,
                  bool thread_shared,
                  bool target_trace_table,
                  ibl_code_t ibl_code[]);

static byte *
emit_syscall_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      bool thread_shared);

int
reg_spill_tls_offs(reg_id_t reg)
{
    switch (reg) {
    case REG_XAX: return TLS_XAX_SLOT;
    case REG_XBX: return TLS_XBX_SLOT;
    case REG_XCX: return TLS_XCX_SLOT;
    case REG_XDX: return TLS_XDX_SLOT;
    }
    /* don't assert if another reg passed: used on random regs looking for spills */
    return -1;
}

#ifdef INTERNAL
/* routine can be used for dumping both thread private and the thread shared routines */
static void
dump_emitted_routines(dcontext_t *dcontext, file_t file,
                      const char *code_description,
                      generated_code_t *code, byte *emitted_pc)
{
    byte *last_pc;

#ifdef X64
    if (GENCODE_IS_X86(code->gencode_mode)) {
        /* parts of x86 gencode are 64-bit but it's hard to know which here
         * so we dump all as x86
         */
        set_x86_mode(dcontext, true/*x86*/);
    }
#endif

    print_file(file, "%s routines created:\n", code_description);
    {
        last_pc = code->gen_start_pc;
        do {
            const char *ibl_brtype;
            const char *ibl_name =
                get_ibl_routine_name(dcontext, (cache_pc)last_pc, &ibl_brtype);

# ifdef WINDOWS
            /* must test first, as get_ibl_routine_name will think "bb_ibl_indjmp" */
            if (last_pc == code->unlinked_shared_syscall)
                print_file(file, "unlinked_shared_syscall:\n");
            else if (last_pc == code->shared_syscall)
                print_file(file, "shared_syscall:\n");
            else
# endif
            if (ibl_name)
                print_file(file, "%s_%s:\n", ibl_name, ibl_brtype);
            else if (last_pc == code->fcache_enter)
                print_file(file, "fcache_enter:\n");
            else if (last_pc == code->fcache_return)
                print_file(file, "fcache_return:\n");
            else if (last_pc == code->do_syscall)
                print_file(file, "do_syscall:\n");
# ifdef WINDOWS
            else if (last_pc == code->fcache_enter_indirect)
                print_file(file, "fcache_enter_indirect:\n");
            else if (last_pc == code->do_callback_return)
                print_file(file, "do_callback_return:\n");
# else
            else if (last_pc == code->do_int_syscall)
                print_file(file, "do_int_syscall:\n");
            else if (last_pc == code->do_int81_syscall)
                print_file(file, "do_int81_syscall:\n");
            else if (last_pc == code->do_int82_syscall)
                print_file(file, "do_int82_syscall:\n");
            else if (last_pc == code->do_clone_syscall)
                print_file(file, "do_clone_syscall:\n");
#  ifdef VMX86_SERVER
            else if (last_pc == code->do_vmkuw_syscall)
                print_file(file, "do_vmkuw_syscall:\n");
#  endif
# endif
# ifdef UNIX
            else if (last_pc == code->new_thread_dynamo_start)
                print_file(file, "new_thread_dynamo_start:\n");
# endif
# ifdef TRACE_HEAD_CACHE_INCR
            else if (last_pc == code->trace_head_incr)
                print_file(file, "trace_head_incr:\n");
# endif
            else if (last_pc == code->reset_exit_stub)
                print_file(file, "reset_exit_stub:\n");
            else if (last_pc == code->fcache_return_coarse)
                print_file(file, "fcache_return_coarse:\n");
            else if (last_pc == code->trace_head_return_coarse)
                print_file(file, "trace_head_return_coarse:\n");
# ifdef CLIENT_INTERFACE
            else if (last_pc == code->special_ibl_xfer[CLIENT_IBL_IDX])
                print_file(file, "client_ibl_xfer:\n");
# endif
# ifdef UNIX
            else if (last_pc == code->special_ibl_xfer[NATIVE_PLT_IBL_IDX])
                print_file(file, "native_plt_ibl_xfer:\n");
            else if (last_pc == code->special_ibl_xfer[NATIVE_RET_IBL_IDX])
                print_file(file, "native_ret_ibl_xfer:\n");
# endif
            else if (last_pc == code->clean_call_save)
                print_file(file, "clean_call_save:\n");
            else if (last_pc == code->clean_call_restore)
                print_file(file, "clean_call_restore:\n");
            last_pc = disassemble_with_bytes(dcontext, last_pc, file);
        } while (last_pc < emitted_pc);
        print_file(file, "%s routines size: "SSZFMT" / "SSZFMT"\n\n",
                   code_description, emitted_pc - code->gen_start_pc,
                   code->commit_end_pc - code->gen_start_pc);
    }

#ifdef X64
    if (GENCODE_IS_X86(code->gencode_mode))
        set_x86_mode(dcontext, false/*x64*/);
#endif
}

void
dump_emitted_routines_to_file(dcontext_t *dcontext, const char *filename,
                              const char *label, generated_code_t *code,
                              byte *stop_pc)
{
    file_t file = open_log_file(filename, NULL, 0);
    if (file != INVALID_FILE) {
        /* FIXME: we currently miss later patches for table & mask, but
         * that only changes a few immeds
         */
        dump_emitted_routines(dcontext, file, label, code, stop_pc);
        close_log_file(file);
    } else
        ASSERT_NOT_REACHED();
}
#endif /* INTERNAL */

/*** functions exported to src directory ***/

byte *
code_align_forward(byte *pc, size_t alignment)
{
    byte *new_pc = (byte *) ALIGN_FORWARD(pc, alignment);
    DOCHECK(1, {
        SET_TO_NOPS(pc, new_pc - pc);
    });
    return new_pc;
}

static byte *
move_to_start_of_cache_line(byte *pc)
{
    return code_align_forward(pc, proc_get_cache_line_size());
}

/* The real size of generated code we need varies by cache line size and
 * options like inlining of ibl code.  We also generate different routines
 * for thread-private and thread-shared.  So, we dynamically extend the size
 * as we generate.  Currently our max is under 5 pages.
 */
#define GENCODE_RESERVE_SIZE (5*PAGE_SIZE)

#define GENCODE_COMMIT_SIZE \
    ((size_t)(ALIGN_FORWARD(sizeof(generated_code_t), PAGE_SIZE) + PAGE_SIZE))

static byte *
check_size_and_cache_line(generated_code_t *code, byte *pc)
{
    /* Assumption: no single emit uses more than a page.
     * We keep an extra page at all times and release it at the end.
     */
    byte *next_pc = move_to_start_of_cache_line(pc);
    if ((byte *)ALIGN_FORWARD(pc, PAGE_SIZE) + PAGE_SIZE > code->commit_end_pc) {
        ASSERT(code->commit_end_pc + PAGE_SIZE <= ((byte *)code) + GENCODE_RESERVE_SIZE);
        heap_mmap_extend_commitment(code->commit_end_pc, PAGE_SIZE);
        code->commit_end_pc += PAGE_SIZE;
    }
    return next_pc;
}

static void
release_final_page(generated_code_t *code)
{
    /* FIXME: have heap_mmap not allocate a guard page, and use our
     * extra for that page, to use one fewer total page of address space.
     */
    size_t leftover = (ptr_uint_t)code->commit_end_pc -
        ALIGN_FORWARD(code->gen_end_pc, PAGE_SIZE);
    ASSERT(code->commit_end_pc >= (byte *) ALIGN_FORWARD(code->gen_end_pc, PAGE_SIZE));
    ASSERT(ALIGNED(code->commit_end_pc, PAGE_SIZE));
    ASSERT(ALIGNED(leftover, PAGE_SIZE));
    if (leftover > 0) {
        heap_mmap_retract_commitment(code->commit_end_pc - leftover, leftover);
        code->commit_end_pc -= leftover;
    }
    LOG(THREAD_GET, LOG_EMIT, 1,
        "Generated code "PFX": %d header, "SZFMT" gen, "SZFMT" commit/%d reserve\n",
        code, sizeof(*code), code->gen_end_pc - code->gen_start_pc,
        (ptr_uint_t)code->commit_end_pc - (ptr_uint_t)code, GENCODE_RESERVE_SIZE);
}

static void
shared_gencode_init(IF_X64_ELSE(gencode_mode_t gencode_mode, void))
{
    generated_code_t *gencode;
    ibl_branch_type_t branch_type;
    byte *pc;
#ifdef X64
    fragment_t *fragment;
    bool x86_mode = false;
    bool x86_to_x64_mode = false;
#endif

    gencode = heap_mmap_reserve(GENCODE_RESERVE_SIZE, GENCODE_COMMIT_SIZE);
    /* we would return gencode and let caller assign, but emit routines
     * that this routine calls query the shared vars so we set here
     */
#ifdef X64
    switch (gencode_mode) {
    case GENCODE_X64:
        shared_code = gencode;
        break;
    case GENCODE_X86:
        /* we do not call set_x86_mode() b/c much of the gencode may be
         * 64-bit: it's up the gencode to mark each instr that's 32-bit.
         */
        shared_code_x86 = gencode;
        x86_mode = true;
        break;
    case GENCODE_X86_TO_X64:
        shared_code_x86_to_x64 = gencode;
        x86_to_x64_mode = true;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
#else
    shared_code = gencode;
#endif
    memset(gencode, 0, sizeof(*gencode));

    gencode->thread_shared = true;
    IF_X64(gencode->gencode_mode = gencode_mode);
    /* Generated code immediately follows struct */
    gencode->gen_start_pc = ((byte *)gencode) + sizeof(*gencode);
    gencode->commit_end_pc = ((byte *)gencode) + GENCODE_COMMIT_SIZE;
    for (branch_type = IBL_BRANCH_TYPE_START;
         branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
        gencode->trace_ibl[branch_type].initialized = false;
        gencode->bb_ibl[branch_type].initialized = false;
        gencode->coarse_ibl[branch_type].initialized = false;
        /* cache the mode so we can pass just the ibl_code_t around */
        IF_X64(gencode->trace_ibl[branch_type].x86_mode = x86_mode);
        IF_X64(gencode->trace_ibl[branch_type].x86_to_x64_mode = x86_to_x64_mode);
        IF_X64(gencode->bb_ibl[branch_type].x86_mode = x86_mode);
        IF_X64(gencode->bb_ibl[branch_type].x86_to_x64_mode = x86_to_x64_mode);
        IF_X64(gencode->coarse_ibl[branch_type].x86_mode = x86_mode);
        IF_X64(gencode->coarse_ibl[branch_type].x86_to_x64_mode = x86_to_x64_mode);
    }

    pc = gencode->gen_start_pc;
    pc = check_size_and_cache_line(gencode, pc);
    gencode->fcache_enter = pc;
    pc = emit_fcache_enter_shared(GLOBAL_DCONTEXT, gencode, pc);
    pc = check_size_and_cache_line(gencode, pc);
    gencode->fcache_return = pc;
    pc = emit_fcache_return_shared(GLOBAL_DCONTEXT, gencode, pc);
    if (DYNAMO_OPTION(coarse_units)) {
        pc = check_size_and_cache_line(gencode, pc);
        gencode->fcache_return_coarse = pc;
        pc = emit_fcache_return_coarse(GLOBAL_DCONTEXT, gencode, pc);
        pc = check_size_and_cache_line(gencode, pc);
        gencode->trace_head_return_coarse = pc;
        pc = emit_trace_head_return_coarse(GLOBAL_DCONTEXT, gencode, pc);
    }
#ifdef WINDOWS_PC_SAMPLE
    gencode->fcache_enter_return_end = pc;
#endif

    /* PR 244737: thread-private uses shared gencode on x64.
     * Should we set the option instead? */
    if (USE_SHARED_TRACE_IBL()) {
        /* expected to be false for private trace IBL routine  */
        pc = emit_ibl_routines(GLOBAL_DCONTEXT, gencode,
                               pc, gencode->fcache_return,
                               DYNAMO_OPTION(shared_traces) ?
                               IBL_TRACE_SHARED : IBL_TRACE_PRIVATE, /* source_fragment_type */
                               true, /* thread_shared */
                               true, /* target_trace_table */
                               gencode->trace_ibl);
    }
    if (USE_SHARED_BB_IBL()) {
        pc = emit_ibl_routines(GLOBAL_DCONTEXT, gencode,
                               pc, gencode->fcache_return,
                               IBL_BB_SHARED, /* source_fragment_type */
                               /* thread_shared */
                               IF_X64_ELSE(true, SHARED_FRAGMENTS_ENABLED()),
                               !DYNAMO_OPTION(bb_ibl_targets), /* target_trace_table */
                               gencode->bb_ibl);
    }
    if (DYNAMO_OPTION(coarse_units)) {
        pc = emit_ibl_routines(GLOBAL_DCONTEXT, gencode, pc,
                               /* ibl routines use regular fcache_return */
                               gencode->fcache_return,
                               IBL_COARSE_SHARED, /* source_fragment_type */
                               /* thread_shared */
                               IF_X64_ELSE(true, SHARED_FRAGMENTS_ENABLED()),
                               !DYNAMO_OPTION(bb_ibl_targets), /*target_trace_table*/
                               gencode->coarse_ibl);
    }

#ifdef WINDOWS_PC_SAMPLE
    gencode->ibl_routines_end = pc;
#endif
#if defined(WINDOWS) && !defined(X64)
    /* no dispatch needed on x64 since syscall routines are thread-shared */
    if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        pc = check_size_and_cache_line(gencode, pc);
        gencode->shared_syscall = pc;
        pc = emit_shared_syscall_dispatch(GLOBAL_DCONTEXT, pc);
        pc = check_size_and_cache_line(gencode, pc);
        gencode->unlinked_shared_syscall = pc;
        pc = emit_unlinked_shared_syscall_dispatch(GLOBAL_DCONTEXT, pc);
        LOG(GLOBAL, LOG_EMIT, 3,
            "shared_syscall_dispatch: linked "PFX", unlinked "PFX"\n",
            gencode->shared_syscall, gencode->unlinked_shared_syscall);
    }
#endif

#ifdef UNIX
    /* must create before emit_do_clone_syscall() in emit_syscall_routines() */
    pc = check_size_and_cache_line(gencode, pc);
    gencode->new_thread_dynamo_start = pc;
    pc = emit_new_thread_dynamo_start(GLOBAL_DCONTEXT, pc);
#endif

#ifdef X64
# ifdef WINDOWS
    /* plain fcache_enter indirects through edi, and next_tag is in tls,
     * so we don't need a separate routine for callback return
     */
    gencode->fcache_enter_indirect = gencode->fcache_enter;
    gencode->shared_syscall_code.x86_mode = x86_mode;
    gencode->shared_syscall_code.x86_to_x64_mode = x86_to_x64_mode;
# endif
    /* i#821/PR 284029: for now we assume there are no syscalls in x86 code */
    if (IF_X64_ELSE(!x86_mode, true)) {
        /* PR 244737: syscall routines are all shared */
        pc = emit_syscall_routines(GLOBAL_DCONTEXT, gencode, pc, true/*thread-shared*/);
    }

    /* since we always have a shared fcache_return we can make reset stub shared */
    gencode->reset_exit_stub = pc;
    fragment = linkstub_fragment(GLOBAL_DCONTEXT, (linkstub_t *) get_reset_linkstub());
    if (GENCODE_IS_X86(gencode->gencode_mode))
        fragment = empty_fragment_mark_x86(fragment);
    /* reset exit stub should look just like a direct exit stub */
    pc += insert_exit_stub_other_flags
        (GLOBAL_DCONTEXT, fragment,
         (linkstub_t *) get_reset_linkstub(), pc, LINK_DIRECT);
#elif defined(UNIX) && defined(HAVE_TLS)
    /* PR 212570: we need a thread-shared do_syscall for our vsyscall hook */
    /* PR 361894: we don't support sysenter if no TLS */
    ASSERT(gencode->do_syscall == NULL);
    pc = check_size_and_cache_line(gencode, pc);
    gencode->do_syscall = pc;
    pc = emit_do_syscall(GLOBAL_DCONTEXT, gencode, pc, gencode->fcache_return,
                         true/*shared*/, 0, &gencode->do_syscall_offs);
#endif

#ifdef TRACE_HEAD_CACHE_INCR
    pc = check_size_and_cache_line(gencode, pc);
    gencode->trace_head_incr = pc;
    pc = emit_trace_head_incr_shared(GLOBAL_DCONTEXT, pc, gencode->fcache_return);
#endif

    if (!special_ibl_xfer_is_thread_private()) {
#ifdef CLIENT_INTERFACE
        gencode->special_ibl_xfer[CLIENT_IBL_IDX] = pc;
        pc = emit_client_ibl_xfer(GLOBAL_DCONTEXT, pc, gencode);
#endif
#ifdef UNIX
        /* i#1238: native exec optimization */
        if (DYNAMO_OPTION(native_exec_opt)) {
            pc = check_size_and_cache_line(gencode, pc);
            gencode->special_ibl_xfer[NATIVE_PLT_IBL_IDX] = pc;
            pc = emit_native_plt_ibl_xfer(GLOBAL_DCONTEXT, pc, gencode);
            /* native ret */
            pc = check_size_and_cache_line(gencode, pc);
            gencode->special_ibl_xfer[NATIVE_RET_IBL_IDX] = pc;
            pc = emit_native_ret_ibl_xfer(GLOBAL_DCONTEXT, pc, gencode);
        }
#endif
    }

    if (!client_clean_call_is_thread_private()) {
        pc = check_size_and_cache_line(gencode, pc);
        gencode->clean_call_save = pc;
        pc = emit_clean_call_save(GLOBAL_DCONTEXT, pc, gencode);
        pc = check_size_and_cache_line(gencode, pc);
        gencode->clean_call_restore = pc;
        pc = emit_clean_call_restore(GLOBAL_DCONTEXT, pc, gencode);
    }

    ASSERT(pc < gencode->commit_end_pc);
    gencode->gen_end_pc = pc;
    release_final_page(gencode);

    DOLOG(3, LOG_EMIT, {
        dump_emitted_routines(GLOBAL_DCONTEXT, GLOBAL,
                              IF_X64_ELSE(x86_mode ? "thread-shared x86" :
                                          "thread-shared", "thread-shared"),
                              gencode, pc);
    });
#ifdef INTERNAL
    if (INTERNAL_OPTION(gendump)) {
        dump_emitted_routines_to_file(GLOBAL_DCONTEXT, "gencode-shared",
                                      IF_X64_ELSE(x86_mode ? "thread-shared x86" :
                                                  "thread-shared", "thread-shared"),
                                      gencode, pc);
    }
#endif
#ifdef WINDOWS_PC_SAMPLE
    if (dynamo_options.profile_pcs &&
        dynamo_options.prof_pcs_gencode >= 2 &&
        dynamo_options.prof_pcs_gencode <= 32) {
        gencode->profile =
            create_profile(gencode->gen_start_pc, pc,
                           dynamo_options.prof_pcs_gencode, NULL);
        start_profile(gencode->profile);
    } else
        gencode->profile = NULL;
#endif

    gencode->writable = true;
    protect_generated_code(gencode, READONLY);
}

#ifdef X64
/* Sets other-mode ibl targets, for mixed-mode and x86_to_x64 mode */
static void
far_ibl_set_targets(ibl_code_t src_ibl[], ibl_code_t tgt_ibl[])
{
    ibl_branch_type_t branch_type;
    for (branch_type = IBL_BRANCH_TYPE_START;
         branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
        if (src_ibl[branch_type].initialized) {
            /* selector was set in emit_far_ibl (but at that point we didn't have
             * the other mode's ibl ready for the target)
             */
            ASSERT(CHECK_TRUNCATE_TYPE_uint
                   ((ptr_uint_t)tgt_ibl[branch_type].indirect_branch_lookup_routine));
            ASSERT(CHECK_TRUNCATE_TYPE_uint
                   ((ptr_uint_t)tgt_ibl[branch_type].unlinked_ibl_entry));
            src_ibl[branch_type].far_jmp_opnd.pc = (uint)(ptr_uint_t)
                tgt_ibl[branch_type].indirect_branch_lookup_routine;
            src_ibl[branch_type].far_jmp_unlinked_opnd.pc = (uint)(ptr_uint_t)
                tgt_ibl[branch_type].unlinked_ibl_entry;
        }
    }
}
#endif

/* arch-specific initializations */
void
arch_init()
{
    ASSERT(sizeof(opnd_t) == EXPECTED_SIZEOF_OPND);
    /* ensure our flag sharing is done properly */
    ASSERT((uint)LINK_FINAL_INSTR_SHARED_FLAG <
           (uint)INSTR_FIRST_NON_LINK_SHARED_FLAG);
    ASSERT_TRUNCATE(byte, byte, OPSZ_LAST_ENUM);
    DODEBUG({ reg_check_reg_fixer(); });

    /* Verify that the structures used for a register spill area and to hold IBT
     * table addresses & masks for IBL code are laid out as expected. We expect
     * the spill area to be at offset 0 within the container struct and for the
     * table address/mask pair array to follow immediately after the spill area.
     */
    /* FIXME These can be converted into compile-time checks as follows:
     *
     *    lookup_table_access_t table[
     *       (offsetof(local_state_extended_t, spill_space) == 0 &&
     *        offsetof(local_state_extended_t, table_space) ==
     *           sizeof(spill_state_t)) ? IBL_BRANCH_TYPE_END : -1 ];
     *
     * This isn't self-descriptive, though, so it's not being used right now
     * (xref case 7097).
     */
    ASSERT(offsetof(local_state_extended_t, spill_space) == 0);
    ASSERT(offsetof(local_state_extended_t, table_space) == sizeof(spill_state_t));
#ifdef WINDOWS
    /* syscall_init() should have already set the syscall_method so go ahead
     * and create the globlal_do_syscall now */
    ASSERT(syscall_method != SYSCALL_METHOD_UNINITIALIZED);
#endif

    /* Ensure we have no unexpected padding inside structs that include
     * priv_mcontext_t (app_state_at_intercept_t and dcontext_t) */
    ASSERT(offsetof(priv_mcontext_t, pc) + sizeof(byte*) + PRE_XMM_PADDING ==
           offsetof(priv_mcontext_t, ymm));
    ASSERT(offsetof(app_state_at_intercept_t, mc) ==
           offsetof(app_state_at_intercept_t, start_pc) + sizeof(void*));
    /* Try to catch errors in x86.asm offsets for dcontext_t */
    ASSERT(sizeof(unprotected_context_t) == sizeof(priv_mcontext_t) +
           IF_WINDOWS_ELSE(IF_X64_ELSE(8, 4), 8) +
           IF_CLIENT_INTERFACE_ELSE(5 * sizeof(reg_t), 0));

    interp_init();

#ifdef CHECK_RETURNS_SSE2
    if (proc_has_feature(FEATURE_SSE2)) {
        FATAL_USAGE_ERROR(CHECK_RETURNS_SSE2_REQUIRES_SSE2, 2,
                          get_application_name(), get_application_pid());
    }
#endif

    if (USE_SHARED_GENCODE()) {
        /* thread-shared generated code */
        /* Assumption: no single emit uses more than a page.
         * We keep an extra page at all times and release it at the end.
         * FIXME: have heap_mmap not allocate a guard page, and use our
         * extra for that page, to use one fewer total page of address space.
         */
        ASSERT(GENCODE_COMMIT_SIZE < GENCODE_RESERVE_SIZE);

        shared_gencode_init(IF_X64(GENCODE_X64));
#ifdef X64
        /* FIXME i#49: usually LOL64 has only 32-bit code (kernel has 32-bit syscall
         * interface) but for mixed modes how would we know?  We'd have to make
         * this be initialized lazily on first occurrence.
         */
        if (mixed_mode_enabled()) {
            generated_code_t *shared_code_opposite_mode;

            shared_gencode_init(IF_X64(GENCODE_X86));

            if (DYNAMO_OPTION(x86_to_x64)) {
                shared_gencode_init(IF_X64(GENCODE_X86_TO_X64));
                shared_code_opposite_mode = shared_code_x86_to_x64;
            } else
                shared_code_opposite_mode = shared_code_x86;

            /* Now link the far_ibl for each type to the corresponding regular
             * ibl of the opposite mode.
             */
            far_ibl_set_targets(shared_code->trace_ibl,
                                shared_code_opposite_mode->trace_ibl);
            far_ibl_set_targets(shared_code->bb_ibl,
                                shared_code_opposite_mode->bb_ibl);
            far_ibl_set_targets(shared_code->coarse_ibl,
                                shared_code_opposite_mode->coarse_ibl);

            far_ibl_set_targets(shared_code_opposite_mode->trace_ibl,
                                shared_code->trace_ibl);
            far_ibl_set_targets(shared_code_opposite_mode->bb_ibl,
                                shared_code->bb_ibl);
            far_ibl_set_targets(shared_code_opposite_mode->coarse_ibl,
                                shared_code->coarse_ibl);
        }
#endif
    }
    mangle_init();
}

#ifdef WINDOWS_PC_SAMPLE
static void
arch_extract_profile(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *tpc = get_emitted_routines_code(dcontext _IF_X64(mode));
    thread_id_t tid = dcontext == GLOBAL_DCONTEXT ? 0 : dcontext->owning_thread;
    /* we may not have x86 gencode */
    ASSERT(tpc != NULL IF_X64(|| mode == GENCODE_X86));
    if (tpc != NULL && tpc->profile != NULL) {

        ibl_branch_type_t branch_type;
        int sum;

        protect_generated_code(tpc, WRITABLE);

        stop_profile(tpc->profile);
        mutex_lock(&profile_dump_lock);

        /* Print the thread id so even if it has no hits we can
         * count the # total threads. */
        print_file(profile_file, "Profile for thread "TIDFMT"\n", tid);
        sum = sum_profile_range(tpc->profile, tpc->fcache_enter,
                                tpc->fcache_enter_return_end);
        if (sum > 0) {
            print_file(profile_file, "\nDumping cache enter/exit code profile "
                       "(thread "TIDFMT")\n%d hits\n", tid, sum);
            dump_profile_range(profile_file, tpc->profile, tpc->fcache_enter,
                               tpc->fcache_enter_return_end);
        }

        /* Break out the IBL code by trace/BB and opcode types.
         * Not worth showing far_ibl hits since should be quite rare.
         */
        for (branch_type = IBL_BRANCH_TYPE_START;
             branch_type < IBL_BRANCH_TYPE_END; branch_type++) {

            byte *start;
            byte *end;

            if (tpc->trace_ibl[branch_type].initialized) {
                start = tpc->trace_ibl[branch_type].indirect_branch_lookup_routine;
                end = start + tpc->trace_ibl[branch_type].ibl_routine_length;
                sum = sum_profile_range(tpc->profile, start, end);
                if (sum > 0) {
                    print_file(profile_file, "\nDumping trace IBL code %s profile "
                               "(thread "TIDFMT")\n%d hits\n",
                               get_branch_type_name(branch_type), tid, sum);
                    dump_profile_range(profile_file, tpc->profile, start, end);
                }
            }
            if (tpc->bb_ibl[branch_type].initialized) {
                start = tpc->bb_ibl[branch_type].indirect_branch_lookup_routine;
                end = start + tpc->bb_ibl[branch_type].ibl_routine_length;
                sum = sum_profile_range(tpc->profile, start, end);
                if (sum > 0) {
                    print_file(profile_file, "\nDumping BB IBL code %s profile "
                               "(thread "TIDFMT")\n%d hits\n",
                               get_branch_type_name(branch_type), tid, sum);
                    dump_profile_range(profile_file, tpc->profile, start, end);
                }
            }
            if (tpc->coarse_ibl[branch_type].initialized) {
                start = tpc->coarse_ibl[branch_type].indirect_branch_lookup_routine;
                end = start + tpc->coarse_ibl[branch_type].ibl_routine_length;
                sum = sum_profile_range(tpc->profile, start, end);
                if (sum > 0) {
                    print_file(profile_file, "\nDumping coarse IBL code %s profile "
                               "(thread "TIDFMT")\n%d hits\n",
                               get_branch_type_name(branch_type), tid, sum);
                    dump_profile_range(profile_file, tpc->profile, start, end);
                }
            }
        }

        sum = sum_profile_range(tpc->profile, tpc->ibl_routines_end,
                                tpc->profile->end);
        if (sum > 0) {
            print_file(profile_file, "\nDumping generated code profile "
                       "(thread "TIDFMT")\n%d hits\n", tid, sum);
            dump_profile_range(profile_file, tpc->profile,
                               tpc->ibl_routines_end, tpc->profile->end);
        }

        mutex_unlock(&profile_dump_lock);
        free_profile(tpc->profile);
        tpc->profile = NULL;
    }
}

void
arch_profile_exit()
{
    if (USE_SHARED_GENCODE()) {
        arch_extract_profile(GLOBAL_DCONTEXT _IF_X64(GENCODE_X64));
        IF_X64(arch_extract_profile(GLOBAL_DCONTEXT _IF_X64(GENCODE_X86)));
    }
}
#endif /* WINDOWS_PC_SAMPLE */

/* arch-specific atexit cleanup */
void
arch_exit(IF_WINDOWS_ELSE_NP(bool detach_stacked_callbacks, void))
{
    /* we only need to unprotect shared_code for profile extraction
     * so we do it there to also cover the fast exit path
     */
#ifdef WINDOWS_PC_SAMPLE
    arch_profile_exit();
#endif
    /* on x64 we have syscall routines in the shared code so can't free if detaching */
    if (IF_WINDOWS(IF_X64(!detach_stacked_callbacks &&)) shared_code != NULL) {
        heap_munmap(shared_code, GENCODE_RESERVE_SIZE);
    }
#ifdef X64
    if (shared_code_x86 != NULL)
        heap_munmap(shared_code_x86, GENCODE_RESERVE_SIZE);
    if (shared_code_x86_to_x64 != NULL)
        heap_munmap(shared_code_x86_to_x64, GENCODE_RESERVE_SIZE);
#endif
    interp_exit();
    mangle_exit();
}

static byte *
emit_ibl_routine_and_template(dcontext_t *dcontext, generated_code_t *code,
                              byte *pc,
                              byte *fcache_return_pc,
                              bool target_trace_table,
                              bool inline_ibl_head,
                              bool thread_shared,
                              ibl_branch_type_t branch_type,
                              ibl_source_fragment_type_t source_type,
                              ibl_code_t *ibl_code)
{
    pc = check_size_and_cache_line(code, pc);
    ibl_code->initialized = true;
    ibl_code->indirect_branch_lookup_routine = pc;
    ibl_code->ibl_head_is_inlined = inline_ibl_head;
    ibl_code->thread_shared_routine = thread_shared;
    ibl_code->branch_type = branch_type;
    ibl_code->source_fragment_type = source_type;

    pc = emit_indirect_branch_lookup(dcontext, code, pc, fcache_return_pc,
                                     target_trace_table, inline_ibl_head,
                                     ibl_code);
    if (inline_ibl_head) {
        /* create the inlined ibl template */
        pc = check_size_and_cache_line(code, pc);
        pc = emit_inline_ibl_stub(dcontext, pc, ibl_code, target_trace_table);
    }

    ibl_code->far_ibl = pc;
    pc = emit_far_ibl(dcontext, pc, ibl_code,
                      ibl_code->indirect_branch_lookup_routine
                      _IF_X64(&ibl_code->far_jmp_opnd));
    ibl_code->far_ibl_unlinked = pc;
    pc = emit_far_ibl(dcontext, pc, ibl_code,
                      ibl_code->unlinked_ibl_entry
                      _IF_X64(&ibl_code->far_jmp_unlinked_opnd));

    return pc;
}

static byte *
emit_ibl_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                  byte *fcache_return_pc,
                  ibl_source_fragment_type_t source_fragment_type,
                  bool thread_shared,
                  bool target_trace_table,
                  ibl_code_t ibl_code_routines[])
{
    ibl_branch_type_t branch_type;
    /* emit separate routines for each branch type
       The goal is to have routines that target different fragment tables
       so that we can control for example return targets for RAC,
       or we can control inlining if some branch types have better hit ratios.

       Currently it only gives us better stats.
     */
    /*
       N.B.: shared fragments requires -atomic_inlined_linking in order to
       inline ibl lookups, but not for private since they're unlinked by another thread
       flushing but not linked by anyone but themselves.
    */

    bool inline_ibl_head =  (IS_IBL_TRACE(source_fragment_type)) ?
        DYNAMO_OPTION(inline_trace_ibl) : DYNAMO_OPTION(inline_bb_ibl);

    for (branch_type = IBL_BRANCH_TYPE_START;
         branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
#ifdef HASHTABLE_STATISTICS
        /* ugly asserts but we'll stick with uints to save space */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                      (GET_IBL_TARGET_TABLE(branch_type, target_trace_table)
                       + offsetof(ibl_table_t, unprot_stats))));
        ibl_code_routines[branch_type].unprot_stats_offset = (uint)
            GET_IBL_TARGET_TABLE(branch_type, target_trace_table)
            + offsetof(ibl_table_t, unprot_stats);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                      (GET_IBL_TARGET_TABLE(branch_type, target_trace_table)
                       + offsetof(ibl_table_t, entry_stats_to_lookup_table))));
        ibl_code_routines[branch_type].entry_stats_to_lookup_table_offset = (uint)
            GET_IBL_TARGET_TABLE(branch_type, target_trace_table)
            + offsetof(ibl_table_t, entry_stats_to_lookup_table);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                      (offsetof(unprot_ht_statistics_t, trace_ibl_stats[branch_type]))));
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                      (offsetof(unprot_ht_statistics_t, bb_ibl_stats[branch_type]))));
        ibl_code_routines[branch_type].hashtable_stats_offset = (uint)
            ((IS_IBL_TRACE(source_fragment_type)) ?
             offsetof(unprot_ht_statistics_t, trace_ibl_stats[branch_type])
             : offsetof(unprot_ht_statistics_t, bb_ibl_stats[branch_type]));
#endif
        pc = emit_ibl_routine_and_template(dcontext, code, pc,
                                           fcache_return_pc,
                                           target_trace_table,
                                           inline_ibl_head, thread_shared,
                                           branch_type, source_fragment_type,
                                           &ibl_code_routines[branch_type]);
    }
    return pc;
}

static byte *
emit_syscall_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      bool thread_shared)
{
#ifdef HASHTABLE_STATISTICS
    /* Stats for the syscall IBLs (note it is also using the trace hashtable, and it never hits!) */
# ifdef WINDOWS
    /* ugly asserts but we'll stick with uints to save space */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                  (GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true)
                   + offsetof(ibl_table_t, unprot_stats))));
    code->shared_syscall_code.unprot_stats_offset = (uint)
        GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true)
        + offsetof(ibl_table_t, unprot_stats);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                  (GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true)
                   + offsetof(ibl_table_t, entry_stats_to_lookup_table))));
    code->shared_syscall_code.entry_stats_to_lookup_table_offset = (uint)
        GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true)
        + offsetof(ibl_table_t, entry_stats_to_lookup_table);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint
                  (offsetof(unprot_ht_statistics_t, shared_syscall_hit_stats))));
    code->shared_syscall_code.hashtable_stats_offset = (uint)
        offsetof(unprot_ht_statistics_t, shared_syscall_hit_stats);
# endif /* WINDOWS */
#endif /* HASHTABLE_STATISTICS */

#ifdef WINDOWS
    pc = check_size_and_cache_line(code, pc);
    code->do_callback_return = pc;
    pc = emit_do_callback_return(dcontext, pc, code->fcache_return, thread_shared);
    if (DYNAMO_OPTION(shared_syscalls)) {

        ibl_code_t *ibl_code;

        if (DYNAMO_OPTION(disable_traces)) {
            ibl_code = DYNAMO_OPTION(shared_bbs) ?
                &SHARED_GENCODE(code->gencode_mode)->bb_ibl[IBL_SHARED_SYSCALL] :
                &code->bb_ibl[IBL_SHARED_SYSCALL];
        }
        else if (DYNAMO_OPTION(shared_traces)) {
            ibl_code = &SHARED_GENCODE(code->gencode_mode)->trace_ibl[IBL_SHARED_SYSCALL];
        }
        else {
            ibl_code = &code->trace_ibl[IBL_SHARED_SYSCALL];
        }

        pc = check_size_and_cache_line(code, pc);
        code->unlinked_shared_syscall = pc;
        pc = emit_shared_syscall(dcontext, code, pc,
                                 &code->shared_syscall_code,
                                 &code->shared_syscall_code.ibl_patch,
                                 ibl_code->indirect_branch_lookup_routine,
                                 ibl_code->unlinked_ibl_entry,
                                 !DYNAMO_OPTION(disable_traces), /* target_trace_table */
                                 /* Only a single copy of shared syscall is
                                  * emitted and afterwards it performs an IBL.
                                  * Since both traces and BBs execute shared
                                  * syscall (when trace building isn't disabled),
                                  * we can't target the trace IBT table; otherwise,
                                  * we'd miss marking secondary trace heads after
                                  * a post-trace IBL misses. More comments are
                                  * co-located with emit_shared_syscall().
                                  */
                                 DYNAMO_OPTION(disable_traces) ?
                                 DYNAMO_OPTION(inline_bb_ibl) :
                                 DYNAMO_OPTION(inline_trace_ibl), /* inline_ibl_head */
                                 ibl_code->thread_shared_routine, /* thread_shared */
                                 &code->shared_syscall);
        code->end_shared_syscall = pc;
        /* Lookup at end of shared_syscall should be able to go to bb or trace,
         * unrestricted (will never be an exit from a trace so no secondary trace
         * restrictions) -- currently only traces supported so using the trace_ibl
         * is OK.
         */
    }
    pc = check_size_and_cache_line(code, pc);
    code->do_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0, &code->do_syscall_offs);
#else /* UNIX */
    pc = check_size_and_cache_line(code, pc);
    code->do_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0, &code->do_syscall_offs);
    pc = check_size_and_cache_line(code, pc);
    code->do_int_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0x80/*force int*/, &code->do_int_syscall_offs);
    pc = check_size_and_cache_line(code, pc);
    code->do_int81_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0x81/*force int*/, &code->do_int81_syscall_offs);
    pc = check_size_and_cache_line(code, pc);
    code->do_int82_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0x82/*force int*/, &code->do_int82_syscall_offs);
    pc = check_size_and_cache_line(code, pc);
    code->do_clone_syscall = pc;
    pc = emit_do_clone_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                               &code->do_clone_syscall_offs);
# ifdef VMX86_SERVER
    pc = check_size_and_cache_line(code, pc);
    code->do_vmkuw_syscall = pc;
    pc = emit_do_vmkuw_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                               &code->do_vmkuw_syscall_offs);
# endif
#endif /* UNIX */

    return pc;
}

void
arch_thread_init(dcontext_t *dcontext)
{
    byte *pc;
    generated_code_t *code;
    ibl_branch_type_t branch_type;

    /* Simplest to have a real dcontext for emitting the selfmod code
     * and finding the patch offsets so we do it on 1st thread init */
    static bool selfmod_init = false;
    if (!selfmod_init) {
        ASSERT(!dynamo_initialized); /* .data +w */
        selfmod_init = true;
        set_selfmod_sandbox_offsets(dcontext);
    }

    ASSERT_CURIOSITY(proc_is_cache_aligned(get_local_state())
                     IF_WINDOWS(|| DYNAMO_OPTION(tls_align != 0)));

#ifdef X64
    /* PR 244737: thread-private uses only shared gencode on x64 */
    ASSERT(dcontext->private_code == NULL);
    return;
#endif

    /* For detach on windows need to use a separate mmap so we can leave this
     * memory around in case of outstanding callbacks when we detach.  Without
     * detach or on linux could just use one of our heaps (which would save
     * a little space, (would then need to coordinate with arch_thread_exit)
     */
    ASSERT(GENCODE_COMMIT_SIZE < GENCODE_RESERVE_SIZE);
    /* case 9474; share allocation unit w/ thread-private stack */
    code = heap_mmap_reserve_post_stack(dcontext,
                                        GENCODE_RESERVE_SIZE, GENCODE_COMMIT_SIZE);
    ASSERT(code != NULL);
    /* FIXME case 6493: if we split private from shared, remove this
     * memset since we will no longer have a bunch of fields we don't use
     */
    memset(code, 0, sizeof(*code));
    code->thread_shared = false;
    /* Generated code immediately follows struct */
    code->gen_start_pc = ((byte *)code) + sizeof(*code);
    code->commit_end_pc = ((byte *)code) + GENCODE_COMMIT_SIZE;
    for (branch_type = IBL_BRANCH_TYPE_START;
         branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
        code->trace_ibl[branch_type].initialized = false;
        code->bb_ibl[branch_type].initialized = false;
        code->coarse_ibl[branch_type].initialized = false;
    }

    dcontext->private_code = (void *) code;

    pc = code->gen_start_pc;
    pc = check_size_and_cache_line(code, pc);
    code->fcache_enter = pc;
    pc = emit_fcache_enter(dcontext, code, pc);
    pc = check_size_and_cache_line(code, pc);
    code->fcache_return = pc;
    pc = emit_fcache_return(dcontext, code, pc);;
#ifdef WINDOWS_PC_SAMPLE
    code->fcache_enter_return_end = pc;
#endif

    /* Currently all ibl routines target the trace hashtable
       and we don't yet support basic blocks as targets of an IBL.
       However, having separate routines at least enables finer control
       over the indirect exit stubs.
       This way we have inlined IBL stubs for trace but not in basic blocks.

       TODO: After separating the IBL routines, now we can retarget them to separate
       hashtables (or alternatively chain several IBL routines together).
       From trace ib exits we can only go to {traces}, so no change here.
         (when we exit to a basic block we need to mark as a trace head)
       From basic block ib exits we should be able to go to {traces + bbs - traceheads}
          (for the tracehead bbs we actually have to increment counters.
       From shared_syscall we should be able to go to {traces + bbs}.

       TODO: we also want to have separate routines per indirect branch types to enable
       the restricted control transfer policies to be efficiently enforced.
    */
    if (!DYNAMO_OPTION(disable_traces) && DYNAMO_OPTION(shared_trace_ibl_routine)) {
        if (!DYNAMO_OPTION(shared_traces)) {
            /* copy all bookkeeping information from shared_code into thread private
               needed by get_ibl_routine*() */
            ibl_branch_type_t branch_type;
            for (branch_type = IBL_BRANCH_TYPE_START;
                 branch_type < IBL_BRANCH_TYPE_END; branch_type++) {
                code->trace_ibl[branch_type] =
                    SHARED_GENCODE(code->gencode_mode)->trace_ibl[branch_type];
            }
        } /* FIXME: no private traces supported right now w/ -shared_traces */
    } else if (PRIVATE_TRACES_ENABLED()) {
        /* shared_trace_ibl_routine should be false for private (performance test only) */
        pc = emit_ibl_routines(dcontext, code, pc, code->fcache_return,
                               IBL_TRACE_PRIVATE, /* source_fragment_type */
                               DYNAMO_OPTION(shared_trace_ibl_routine), /* thread_shared */
                               true, /* target_trace_table */
                               code->trace_ibl);
    }
    pc = emit_ibl_routines(dcontext, code, pc, code->fcache_return,
                           IBL_BB_PRIVATE, /* source_fragment_type */
                           /* need thread-private for selfmod regardless of sharing */
                           false, /* thread_shared */
                           !DYNAMO_OPTION(bb_ibl_targets), /* target_trace_table */
                           code->bb_ibl);
#ifdef WINDOWS_PC_SAMPLE
    code->ibl_routines_end = pc;
#endif

#if defined(UNIX) && !defined(HAVE_TLS)
    /* for HAVE_TLS we use the shared version; w/o TLS we don't
     * make any shared routines (PR 361894)
     */
    /* must create before emit_do_clone_syscall() in emit_syscall_routines() */
    pc = check_size_and_cache_line(code, pc);
    code->new_thread_dynamo_start = pc;
    pc = emit_new_thread_dynamo_start(dcontext, pc);
#endif

#ifdef WINDOWS
    pc = check_size_and_cache_line(code, pc);
    code->fcache_enter_indirect = pc;
    pc = emit_fcache_enter_indirect(dcontext, code, pc, code->fcache_return);
#endif
    pc = emit_syscall_routines(dcontext, code, pc, false/*thread-private*/);
#ifdef TRACE_HEAD_CACHE_INCR
    pc = check_size_and_cache_line(code, pc);
    code->trace_head_incr = pc;
    pc = emit_trace_head_incr(dcontext, pc, code->fcache_return);
#endif
#ifdef CHECK_RETURNS_SSE2_EMIT
    /* PR 248210: unsupported feature on x64: need to move to thread-shared gencode
     * if want to support it.
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    pc = check_size_and_cache_line(code, pc);
    code->pextrw = pc;
    pc = emit_pextrw(dcontext, pc);
    pc = check_size_and_cache_line(code, pc);
    code->pinsrw = pc;
    pc = emit_pinsrw(dcontext, pc);
#endif
    code->reset_exit_stub = pc;
    /* reset exit stub should look just like a direct exit stub */
    pc += insert_exit_stub_other_flags(dcontext,
                                       linkstub_fragment(dcontext, (linkstub_t *)
                                                         get_reset_linkstub()),
                                       (linkstub_t *) get_reset_linkstub(),
                                       pc, LINK_DIRECT);

    if (special_ibl_xfer_is_thread_private()) {
#ifdef CLIENT_INTERFACE
        code->special_ibl_xfer[CLIENT_IBL_IDX] = pc;
        pc = emit_client_ibl_xfer(dcontext, pc, code);
#endif
#ifdef UNIX
        /* i#1238: native exec optimization */
        if (DYNAMO_OPTION(native_exec_opt)) {
            pc = check_size_and_cache_line(code, pc);
            code->special_ibl_xfer[NATIVE_PLT_IBL_IDX] = pc;
            pc = emit_native_plt_ibl_xfer(dcontext, pc, code);
            /* native ret */
            pc = check_size_and_cache_line(code, pc);
            code->special_ibl_xfer[NATIVE_RET_IBL_IDX] = pc;
            pc = emit_native_ret_ibl_xfer(dcontext, pc, code);
        }
#endif
    }

    /* XXX: i#1149: we should always use thread shared gencode */
    if (client_clean_call_is_thread_private()) {
        pc = check_size_and_cache_line(code, pc);
        code->clean_call_save = pc;
        pc = emit_clean_call_save(dcontext, pc, code);
        pc = check_size_and_cache_line(code, pc);
        code->clean_call_restore = pc;
        pc = emit_clean_call_restore(dcontext, pc, code);
    }

    ASSERT(pc < code->commit_end_pc);
    code->gen_end_pc = pc;
    release_final_page(code);

    DOLOG(3, LOG_EMIT, {
        dump_emitted_routines(dcontext, THREAD, "thread-private", code, pc);
    });
#ifdef INTERNAL
    if (INTERNAL_OPTION(gendump)) {
        dump_emitted_routines_to_file(dcontext, "gencode-private", "thread-private",
                                      code, pc);
    }
#endif
#ifdef WINDOWS_PC_SAMPLE
    if (dynamo_options.profile_pcs && dynamo_options.prof_pcs_gencode >= 2 &&
        dynamo_options.prof_pcs_gencode <= 32) {
        code->profile = create_profile(code->gen_start_pc, pc,
                                       dynamo_options.prof_pcs_gencode, NULL);
        start_profile(code->profile);
    } else
        code->profile = NULL;
#endif

    code->writable = true;
    /* For SELFPROT_GENCODE we don't make unwritable until after we patch,
     * though for hotp_only we don't patch.
     */
#ifdef HOT_PATCHING_INTERFACE
    if (DYNAMO_OPTION(hotp_only))
#endif
        protect_generated_code(code, READONLY);
}

#ifdef WINDOWS_PC_SAMPLE
void
arch_thread_profile_exit(dcontext_t *dcontext)
{
    arch_extract_profile(dcontext _IF_X64(GENCODE_FROM_DCONTEXT));
}
#endif

void
arch_thread_exit(dcontext_t *dcontext _IF_WINDOWS(bool detach_stacked_callbacks))
{
#ifdef X64
    /* PR 244737: thread-private uses only shared gencode on x64 */
    ASSERT(dcontext->private_code == NULL);
    return;
#endif
    /* We only need to unprotect private_code for profile extraction
     * so we do it there to also cover the fast exit path.
     * Also note that for detach w/ stacked callbacks arch_patch_syscall()
     * will have already unprotected.
     */
#ifdef WINDOWS
    if (!detach_stacked_callbacks && !DYNAMO_OPTION(thin_client)) {
#endif
        /* ensure we didn't miss the init patch and leave it writable! */
        ASSERT(!TEST(SELFPROT_GENCODE, DYNAMO_OPTION(protect_mask)) ||
               !((generated_code_t *)dcontext->private_code)->writable);
#ifdef WINDOWS
    }
#endif
#ifdef WINDOWS_PC_SAMPLE
    arch_thread_profile_exit(dcontext);
#endif
#ifdef WINDOWS
    if (!detach_stacked_callbacks)
#endif
        heap_munmap_post_stack(dcontext, dcontext->private_code, GENCODE_RESERVE_SIZE);
}

#ifdef WINDOWS
/* Patch syscall routines for detach */
static void
arch_patch_syscall_common(dcontext_t *dcontext, byte *target _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X64(mode));
    if (code != NULL && (!is_shared_gencode(code) || dcontext == GLOBAL_DCONTEXT)) {
        /* ensure we didn't miss the init patch and leave it writable! */
        ASSERT(!TEST(SELFPROT_GENCODE, DYNAMO_OPTION(protect_mask)) || !code->writable);
        /* this is only done for detach, so no need to re-protect */
        protect_generated_code(code, WRITABLE);
        emit_patch_syscall(dcontext, target _IF_X64(mode));
    }
}
void
arch_patch_syscall(dcontext_t *dcontext, byte *target)
{
    if (dcontext == GLOBAL_DCONTEXT) {
        arch_patch_syscall_common(GLOBAL_DCONTEXT, target _IF_X64(GENCODE_X64));
        IF_X64(arch_patch_syscall_common(GLOBAL_DCONTEXT, target _IF_X64(GENCODE_X86)));
    } else
        arch_patch_syscall_common(GLOBAL_DCONTEXT, target _IF_X64(GENCODE_FROM_DCONTEXT));
}
#endif

void
update_generated_hashtable_access(dcontext_t *dcontext)
{
    update_indirect_branch_lookup(dcontext);
}

void
protect_generated_code(generated_code_t *code_in, bool writable)
{
    /* i#936: prevent cl v16 (VS2010) from combining the two code->writable
     * stores into one prior to the change_protection() call and from
     * changing the conditionally-executed stores into always-executed
     * stores of conditionally-determined values.
     */
    volatile generated_code_t *code = code_in;
    if (TEST(SELFPROT_GENCODE, DYNAMO_OPTION(protect_mask)) &&
        code->writable != writable) {
        byte *genstart = (byte *)PAGE_START(code->gen_start_pc);
        if (!writable) {
            ASSERT(code->writable);
            code->writable = writable;
        }
        STATS_INC(gencode_prot_changes);
        change_protection(genstart, code->commit_end_pc - genstart,
                          writable);
        if (writable) {
            ASSERT(!code->writable);
            code->writable = writable;
        }
    }
}

ibl_source_fragment_type_t
get_source_fragment_type(dcontext_t *dcontext, uint fragment_flags)
{
    if (TEST(FRAG_IS_TRACE, fragment_flags)) {
        return (TEST(FRAG_SHARED, fragment_flags)) ? IBL_TRACE_SHARED : IBL_TRACE_PRIVATE;
    } else if (TEST(FRAG_COARSE_GRAIN, fragment_flags)) {
        ASSERT(TEST(FRAG_SHARED, fragment_flags));
        return IBL_COARSE_SHARED;
    } else {
        return (TEST(FRAG_SHARED, fragment_flags)) ? IBL_BB_SHARED : IBL_BB_PRIVATE;
    }
}

#ifdef WINDOWS
bool
is_shared_syscall_routine(dcontext_t *dcontext, cache_pc pc)
{
    if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        return (pc == (cache_pc) shared_code->shared_syscall
                || pc == (cache_pc) shared_code->unlinked_shared_syscall)
            IF_X64(|| (shared_code_x86 != NULL &&
                       (pc == (cache_pc) shared_code_x86->shared_syscall
                        || pc == (cache_pc) shared_code_x86->unlinked_shared_syscall))
                   || (shared_code_x86_to_x64 != NULL &&
                       (pc == (cache_pc) shared_code_x86_to_x64->shared_syscall
                        || pc == (cache_pc) shared_code_x86_to_x64
                                            ->unlinked_shared_syscall)));
    }
    else {
        generated_code_t *code = THREAD_GENCODE(dcontext);
        return (code != NULL && (pc == (cache_pc) code->shared_syscall
                                 || pc == (cache_pc) code->unlinked_shared_syscall));
    }
}
#endif

bool
is_indirect_branch_lookup_routine(dcontext_t *dcontext, cache_pc pc)
{
#ifdef WINDOWS
    if (is_shared_syscall_routine(dcontext, pc))
        return true;
#endif
    /* we only care if it is found */
    return get_ibl_routine_type_ex(dcontext, pc, NULL _IF_X64(NULL));
}

/* Promotes the current ibl routine from IBL_BB* to IBL_TRACE* preserving other properties */
/* There seems to be no need for the opposite transformation */
cache_pc
get_trace_ibl_routine(dcontext_t *dcontext, cache_pc current_entry)
{
    ibl_type_t ibl_type = {0};
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type(dcontext, current_entry, &ibl_type);

    ASSERT(is_ibl);
    ASSERT(IS_IBL_BB(ibl_type.source_fragment_type));

    return
#ifdef WINDOWS
        DYNAMO_OPTION(shared_syscalls) &&
        is_shared_syscall_routine(dcontext, current_entry) ? current_entry :
#endif
        get_ibl_routine(dcontext, ibl_type.link_state,
                        (ibl_type.source_fragment_type == IBL_BB_PRIVATE) ?
                        IBL_TRACE_PRIVATE : IBL_TRACE_SHARED,
                        ibl_type.branch_type);
}

/* Shifts the current ibl routine from IBL_BB_SHARED to IBL_BB_PRIVATE,
 * preserving other properties.
 * There seems to be no need for the opposite transformation
 */
cache_pc
get_private_ibl_routine(dcontext_t *dcontext, cache_pc current_entry)
{
    ibl_type_t ibl_type = {0};
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type(dcontext, current_entry, &ibl_type);

    ASSERT(is_ibl);
    ASSERT(IS_IBL_BB(ibl_type.source_fragment_type));

    return get_ibl_routine(dcontext, ibl_type.link_state,
                           IBL_BB_PRIVATE, ibl_type.branch_type);
}

/* Shifts the current ibl routine from IBL_BB_PRIVATE to IBL_BB_SHARED,
 * preserving other properties.
 * There seems to be no need for the opposite transformation
 */
cache_pc
get_shared_ibl_routine(dcontext_t *dcontext, cache_pc current_entry)
{
    ibl_type_t ibl_type = {0};
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type(dcontext, current_entry, &ibl_type);

    ASSERT(is_ibl);
    ASSERT(IS_IBL_BB(ibl_type.source_fragment_type));

    return get_ibl_routine(dcontext, ibl_type.link_state,
                           IBL_BB_SHARED, ibl_type.branch_type);
}

/* gets the corresponding routine to current_entry but matching whether
 * FRAG_IS_TRACE and FRAG_SHARED are set in flags
 */
cache_pc
get_alternate_ibl_routine(dcontext_t *dcontext, cache_pc current_entry,
                          uint flags)
{
    ibl_type_t ibl_type = {0};
    IF_X64(gencode_mode_t mode = GENCODE_FROM_DCONTEXT;)
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type_ex(dcontext, current_entry, &ibl_type _IF_X64(&mode));
    ASSERT(is_ibl);
#ifdef WINDOWS
    /* shared_syscalls does not change currently
     * FIXME: once we support targeting both private and shared syscall
     * we will need to change sharing here
     */
    if (DYNAMO_OPTION(shared_syscalls) &&
        is_shared_syscall_routine(dcontext, current_entry))
        return current_entry;
#endif
    return get_ibl_routine_ex(dcontext, ibl_type.link_state,
                              get_source_fragment_type(dcontext, flags),
                              ibl_type.branch_type _IF_X64(mode));
}

static ibl_entry_point_type_t
get_unlinked_type(ibl_entry_point_type_t link_state)
{
#ifdef X64
    if (link_state == IBL_TRACE_CMP)
        return IBL_TRACE_CMP_UNLINKED;
#endif
    if (link_state == IBL_FAR)
        return IBL_FAR_UNLINKED;
    else
        return IBL_UNLINKED;
}

static ibl_entry_point_type_t
get_linked_type(ibl_entry_point_type_t unlink_state)
{
#ifdef X64
    if (unlink_state == IBL_TRACE_CMP_UNLINKED)
        return IBL_TRACE_CMP;
#endif
    if (unlink_state == IBL_FAR_UNLINKED)
        return IBL_FAR;
    else
        return IBL_LINKED;
}

cache_pc
get_linked_entry(dcontext_t *dcontext, cache_pc unlinked_entry)
{
    ibl_type_t ibl_type = {0};
    IF_X64(gencode_mode_t mode = GENCODE_FROM_DCONTEXT;)
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type_ex(dcontext, unlinked_entry, &ibl_type _IF_X64(&mode));
    ASSERT(is_ibl && IS_IBL_UNLINKED(ibl_type.link_state));

#ifdef WINDOWS
    if (unlinked_entry == unlinked_shared_syscall_routine_ex(dcontext _IF_X64(mode))) {
        return shared_syscall_routine_ex(dcontext _IF_X64(mode));
    }
#endif

    return get_ibl_routine_ex(dcontext,
                              /* for -unsafe_ignore_eflags_{ibl,trace} the trace cmp
                               * entry and unlink are both identical, so we may mix
                               * them up but will have no problems */
                              get_linked_type(ibl_type.link_state),
                              ibl_type.source_fragment_type, ibl_type.branch_type
                              _IF_X64(mode));
}

#ifdef X64
cache_pc
get_trace_cmp_entry(dcontext_t *dcontext, cache_pc linked_entry)
{
    ibl_type_t ibl_type = {0};
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type(dcontext, linked_entry, &ibl_type);
    IF_WINDOWS(ASSERT(linked_entry != shared_syscall_routine(dcontext)));
    ASSERT(is_ibl && ibl_type.link_state == IBL_LINKED);
    return get_ibl_routine(dcontext, IBL_TRACE_CMP,
                           ibl_type.source_fragment_type, ibl_type.branch_type);
}
#endif

cache_pc
get_unlinked_entry(dcontext_t *dcontext, cache_pc linked_entry)
{
    ibl_type_t ibl_type = {0};
    IF_X64(gencode_mode_t mode = GENCODE_FROM_DCONTEXT;)
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type_ex(dcontext, linked_entry, &ibl_type _IF_X64(&mode));
    ASSERT(is_ibl && IS_IBL_LINKED(ibl_type.link_state));

#ifdef WINDOWS
    if (linked_entry == shared_syscall_routine_ex(dcontext _IF_X64(mode)))
        return unlinked_shared_syscall_routine_ex(dcontext _IF_X64(mode));
#endif
    return get_ibl_routine_ex(dcontext, get_unlinked_type(ibl_type.link_state),
                              ibl_type.source_fragment_type, ibl_type.branch_type
                              _IF_X64(mode));
}

static bool
in_generated_shared_routine(dcontext_t *dcontext, cache_pc pc)
{
    if (USE_SHARED_GENCODE()) {
        return (pc >= (cache_pc)(shared_code->gen_start_pc) &&
                pc < (cache_pc)(shared_code->commit_end_pc))
            IF_X64(|| (shared_code_x86 != NULL &&
                       pc >= (cache_pc)(shared_code_x86->gen_start_pc) &&
                       pc < (cache_pc)(shared_code_x86->commit_end_pc))
                   || (shared_code_x86_to_x64 != NULL &&
                       pc >= (cache_pc)(shared_code_x86_to_x64->gen_start_pc) &&
                       pc < (cache_pc)(shared_code_x86_to_x64->commit_end_pc)))
            ;
    }
    return false;
}

bool
in_generated_routine(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);

    return ((pc >= (cache_pc)(code->gen_start_pc) &&
            pc < (cache_pc)(code->commit_end_pc))
            || in_generated_shared_routine(dcontext, pc));
    /* FIXME: what about inlined IBL stubs */
}

bool
in_context_switch_code(dcontext_t *dcontext, cache_pc pc)
{
    return (pc >= (cache_pc)fcache_enter_routine(dcontext) &&
            /* get last emitted routine */
            pc <= get_ibl_routine(dcontext, IBL_LINKED, IBL_SOURCE_TYPE_END-1,
                                  IBL_BRANCH_TYPE_START));
    /* FIXME: too hacky, should have an extra field for PC profiling */
}

bool
in_indirect_branch_lookup_code(dcontext_t *dcontext, cache_pc pc)
{
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;

    for (source_fragment_type = IBL_SOURCE_TYPE_START;
         source_fragment_type < IBL_SOURCE_TYPE_END;
         source_fragment_type++) {
        for (branch_type = IBL_BRANCH_TYPE_START;
             branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            if (pc >= get_ibl_routine(dcontext, IBL_LINKED, source_fragment_type, branch_type) &&
                pc <  get_ibl_routine(dcontext, IBL_UNLINKED, source_fragment_type, branch_type))
                return true;
        }
    }
    return false;                /* not an IBL */
    /* FIXME: what about inlined IBL stubs */
}

fcache_enter_func_t
fcache_enter_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (fcache_enter_func_t) convert_data_to_function(code->fcache_enter);
}

/* exported to dispatch.c */
fcache_enter_func_t
get_fcache_enter_private_routine(dcontext_t *dcontext)
{
    return fcache_enter_routine(dcontext);
}

cache_pc
get_reset_exit_stub(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->reset_exit_stub;
}

cache_pc
get_do_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_syscall;
}

#ifdef WINDOWS
fcache_enter_func_t
get_fcache_enter_indirect_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (fcache_enter_func_t) convert_data_to_function(code->fcache_enter_indirect);
}
cache_pc
get_do_callback_return_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_callback_return;
}
#else
/* PR 286922: we need an int syscall even when vsyscall is sys{call,enter} */
cache_pc
get_do_int_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_int_syscall;
}

cache_pc
get_do_int81_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_int81_syscall;
}

cache_pc
get_do_int82_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_int82_syscall;
}

cache_pc
get_do_clone_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_clone_syscall;
}
# ifdef VMX86_SERVER
cache_pc
get_do_vmkuw_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->do_vmkuw_syscall;
}
# endif
#endif

cache_pc
fcache_return_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->fcache_return;
}

cache_pc
fcache_return_routine_ex(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X64(mode));
    return (cache_pc) code->fcache_return;
}

cache_pc
fcache_return_coarse_routine(IF_X64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X64(mode));
    ASSERT(DYNAMO_OPTION(coarse_units));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc) code->fcache_return_coarse;
}

cache_pc
trace_head_return_coarse_routine(IF_X64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X64(mode));
    ASSERT(DYNAMO_OPTION(coarse_units));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc) code->trace_head_return_coarse;
}

cache_pc
get_clean_call_save(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code;
    if (client_clean_call_is_thread_private())
        code = get_emitted_routines_code(dcontext _IF_X64(mode));
    else
        code = get_emitted_routines_code(GLOBAL_DCONTEXT _IF_X64(mode));
    ASSERT(code != NULL);
    return (cache_pc) code->clean_call_save;
}

cache_pc
get_clean_call_restore(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code;
    if (client_clean_call_is_thread_private())
        code = get_emitted_routines_code(dcontext _IF_X64(mode));
    else
        code = get_emitted_routines_code(GLOBAL_DCONTEXT _IF_X64(mode));
    ASSERT(code != NULL);
    return (cache_pc) code->clean_call_restore;
}

static inline cache_pc
get_special_ibl_xfer_entry(dcontext_t *dcontext, int index)
{
    generated_code_t *code;
    if (special_ibl_xfer_is_thread_private()) {
        ASSERT(dcontext != GLOBAL_DCONTEXT);
        code = THREAD_GENCODE(dcontext);
    } else
        code = SHARED_GENCODE_MATCH_THREAD(dcontext);
    ASSERT(index >= 0 && index < NUM_SPECIAL_IBL_XFERS);
    return code->special_ibl_xfer[index];
}

#ifdef CLIENT_INTERFACE
cache_pc
get_client_ibl_xfer_entry(dcontext_t *dcontext)
{
    return get_special_ibl_xfer_entry(dcontext, CLIENT_IBL_IDX);
}
#endif

#ifdef UNIX
cache_pc
get_native_plt_ibl_xfer_entry(dcontext_t *dcontext)
{
    return get_special_ibl_xfer_entry(dcontext, NATIVE_PLT_IBL_IDX);
}

cache_pc
get_native_ret_ibl_xfer_entry(dcontext_t *dcontext)
{
    return get_special_ibl_xfer_entry(dcontext, NATIVE_RET_IBL_IDX);
}
#endif

/* returns false if target is not an IBL routine.
 * if type is not NULL it is set to the type of the found routine.
 * if mode_out is NULL, dcontext cannot be GLOBAL_DCONTEXT.
 * if mode_out is not NULL, it is set to which mode the found routine is in.
 */
bool
get_ibl_routine_type_ex(dcontext_t *dcontext, cache_pc target, ibl_type_t *type
                        _IF_X64(gencode_mode_t *mode_out))
{
    ibl_entry_point_type_t link_state;
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;
#ifdef X64
    gencode_mode_t mode;
#endif

    /* An up-front range check. Many calls into this routine are with addresses
     * outside of the IBL code or the generated_code_t in which IBL resides.
     * For all of those cases, this quick up-front check saves the expense of
     * examining all of the different IBL entry points.
     */
    if ((shared_code == NULL ||
         target < shared_code->gen_start_pc ||
         target >= shared_code->gen_end_pc)
        IF_X64(&& (shared_code_x86 == NULL ||
                   target < shared_code_x86->gen_start_pc ||
                   target >= shared_code_x86->gen_end_pc)
               && (shared_code_x86_to_x64 == NULL ||
                   target < shared_code_x86_to_x64->gen_start_pc ||
                   target >= shared_code_x86_to_x64->gen_end_pc))) {
        if (dcontext == GLOBAL_DCONTEXT ||
            /* PR 244737: thread-private uses shared gencode on x64 */
            IF_X64(true ||)
            target < ((generated_code_t *)dcontext->private_code)->gen_start_pc ||
            target >= ((generated_code_t *)dcontext->private_code)->gen_end_pc)
            return false;
    }

    /* a decent compiler should inline these nested loops */
    /* iterate in order <linked, unlinked> */
    for (link_state = IBL_LINKED;
         /* keep in mind we need a signed comparison when going downwards */
         (int)link_state >= (int)IBL_UNLINKED; link_state-- ) {
        /* it is OK to compare to IBL_BB_PRIVATE even when !SHARED_FRAGMENTS_ENABLED() */
        for (source_fragment_type = IBL_SOURCE_TYPE_START;
             source_fragment_type < IBL_SOURCE_TYPE_END;
             source_fragment_type++) {
            for (branch_type = IBL_BRANCH_TYPE_START;
                 branch_type < IBL_BRANCH_TYPE_END;
                 branch_type++) {
#ifdef X64
                for (mode = GENCODE_X64; mode <= GENCODE_X86_TO_X64; mode++) {
#endif
                    if (target == get_ibl_routine_ex(dcontext, link_state,
                                                     source_fragment_type,
                                                     branch_type _IF_X64(mode))) {
                        if (type) {
                            type->link_state = link_state;
                            type->source_fragment_type = source_fragment_type;
                            type->branch_type = branch_type;
                        }
#ifdef X64
                        if (mode_out != NULL)
                            *mode_out = mode;
#endif
                        return true;
                    }
#ifdef X64
                }
#endif
            }
        }
    }
#ifdef WINDOWS
    if (is_shared_syscall_routine(dcontext, target)) {
        if (type != NULL) {
            type->branch_type = IBL_SHARED_SYSCALL;
            type->source_fragment_type = DEFAULT_IBL_BB();
#ifdef X64
            for (mode = GENCODE_X64; mode <= GENCODE_X86_TO_X64; mode++) {
#endif
                if (target == unlinked_shared_syscall_routine_ex(dcontext _IF_X64(mode)))
                    type->link_state = IBL_UNLINKED;
                else IF_X64(if (target ==
                                shared_syscall_routine_ex(dcontext _IF_X64(mode))))
                    type->link_state = IBL_LINKED;
#ifdef X64
                else
                    continue;
                if (mode_out != NULL)
                    *mode_out = mode;
                break;
            }
#endif
        }
        return true;
    }
#endif

    return false;                /* not an IBL */
}

bool
get_ibl_routine_type(dcontext_t *dcontext, cache_pc target, ibl_type_t *type)
{
    IF_X64(ASSERT(dcontext != GLOBAL_DCONTEXT)); /* should call get_ibl_routine_type_ex */
    return get_ibl_routine_type_ex(dcontext, target, type _IF_X64(NULL));
}

/* returns false if target is not an IBL template
   if type is not NULL it is set to the type of the found routine
*/
static bool
get_ibl_routine_template_type(dcontext_t *dcontext, cache_pc target, ibl_type_t *type
                              _IF_X64(gencode_mode_t *mode_out))
{
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;
#ifdef X64
    gencode_mode_t mode;
#endif

    for (source_fragment_type = IBL_SOURCE_TYPE_START;
         source_fragment_type < IBL_SOURCE_TYPE_END;
         source_fragment_type++) {
        for (branch_type = IBL_BRANCH_TYPE_START;
             branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
#ifdef X64
            for (mode = GENCODE_X64; mode <= GENCODE_X86_TO_X64; mode++) {
#endif
                if (target == get_ibl_routine_template(dcontext, source_fragment_type,
                                                       branch_type _IF_X64(mode))) {
                    if (type) {
                        type->link_state = IBL_TEMPLATE;
                        type->source_fragment_type = source_fragment_type;
                        type->branch_type = branch_type;
#ifdef X64
                        if (mode_out != NULL)
                            *mode_out = mode;
#endif
                    }
                    return true;
#ifdef X64
                }
#endif
            }
        }
    }
    return false;                /* not an IBL template */
}

const char *
get_branch_type_name(ibl_branch_type_t branch_type)
{
    static const char *const ibl_brtype_names[IBL_BRANCH_TYPE_END] =
        {"ret", "indcall", "indjmp"};
    return ibl_brtype_names[branch_type];
}

ibl_branch_type_t
get_ibl_branch_type(instr_t *instr)
{
    ASSERT(instr_is_mbr(instr) ||
           instr_get_opcode(instr) == OP_jmp_far ||
           instr_get_opcode(instr) == OP_call_far);

    if (instr_is_return(instr))
        return IBL_RETURN;
    else if (instr_is_call_indirect(instr))
        return IBL_INDCALL;
    else
        return IBL_INDJMP;
}


/* returns a symbolic name if target is an IBL routine or an IBL template,
 * otherwise returns NULL
 */
const char *
get_ibl_routine_name(dcontext_t *dcontext, cache_pc target, const char **ibl_brtype_name)
{
    static const char *const
        ibl_routine_names IF_X64([3]) [IBL_SOURCE_TYPE_END][IBL_LINK_STATE_END] = {
        IF_X64({)
        {"shared_unlinked_bb_ibl", "shared_delete_bb_ibl",
         "shared_bb_far", "shared_bb_far_unlinked",
         IF_X64_("shared_bb_cmp") IF_X64_("shared_bb_cmp_unlinked")
         "shared_bb_ibl", "shared_bb_ibl_template"},
        {"shared_unlinked_trace_ibl", "shared_delete_trace_ibl",
         "shared_trace_far", "shared_trace_far_unlinked",
         IF_X64_("shared_trace_cmp") IF_X64_("shared_trace_cmp_unlinked")
         "shared_trace_ibl", "shared_trace_ibl_template"},
        {"private_unlinked_bb_ibl", "private_delete_bb_ibl",
         "private_bb_far", "private_bb_far_unlinked",
         IF_X64_("private_bb_cmp") IF_X64_("private_bb_cmp_unlinked")
         "private_bb_ibl", "private_bb_ibl_template"},
        {"private_unlinked_trace_ibl", "private_delete_trace_ibl",
         "private_trace_far", "private_trace_far_unlinked",
         IF_X64_("private_trace_cmp") IF_X64_("private_trace_cmp_unlinked")
         "private_trace_ibl", "private_trace_ibl_template"},
        {"shared_unlinked_coarse_ibl", "shared_delete_coarse_ibl",
         "shared_coarse_trace_far", "shared_coarse_trace_far_unlinked",
         IF_X64_("shared_coarse_trace_cmp") IF_X64_("shared_coarse_trace_cmp_unlinked")
         "shared_coarse_ibl", "shared_coarse_ibl_template"},
#ifdef X64
        /* PR 282576: for WOW64 processes we have separate x86 routines */
        }, {
        {"x86_shared_unlinked_bb_ibl", "x86_shared_delete_bb_ibl",
         "x86_shared_bb_far", "x86_shared_bb_far_unlinked",
         IF_X64_("x86_shared_bb_cmp") IF_X64_("x86_shared_bb_cmp_unlinked")
         "x86_shared_bb_ibl", "x86_shared_bb_ibl_template"},
        {"x86_shared_unlinked_trace_ibl", "x86_shared_delete_trace_ibl",
         "x86_shared_trace_far", "x86_shared_trace_far_unlinked",
         IF_X64_("x86_shared_trace_cmp") IF_X64_("x86_shared_trace_cmp_unlinked")
         "x86_shared_trace_ibl", "x86_shared_trace_ibl_template"},
        {"x86_private_unlinked_bb_ibl", "x86_private_delete_bb_ibl",
         "x86_private_bb_far", "x86_private_bb_far_unlinked",
         IF_X64_("x86_private_bb_cmp") IF_X64_("x86_private_bb_cmp_unlinked")
         "x86_private_bb_ibl", "x86_private_bb_ibl_template"},
        {"x86_private_unlinked_trace_ibl", "x86_private_delete_trace_ibl",
         "x86_private_trace_far", "x86_private_trace_far_unlinked",
         IF_X64_("x86_private_trace_cmp") IF_X64_("x86_private_trace_cmp_unlinked")
         "x86_private_trace_ibl", "x86_private_trace_ibl_template"},
        {"x86_shared_unlinked_coarse_ibl", "x86_shared_delete_coarse_ibl",
         "x86_shared_coarse_trace_far",
         "x86_shared_coarse_trace_far_unlinked",
         IF_X64_("x86_shared_coarse_trace_cmp")
         IF_X64_("x86_shared_coarse_trace_cmp_unlinked")
         "x86_shared_coarse_ibl", "x86_shared_coarse_ibl_template"},
        }, {
        {"x86_to_x64_shared_unlinked_bb_ibl", "x86_to_x64_shared_delete_bb_ibl",
         "x86_to_x64_shared_bb_far", "x86_to_x64_shared_bb_far_unlinked",
         "x86_to_x64_shared_bb_cmp", "x86_to_x64_shared_bb_cmp_unlinked",
         "x86_to_x64_shared_bb_ibl", "x86_to_x64_shared_bb_ibl_template"},
        {"x86_to_x64_shared_unlinked_trace_ibl", "x86_to_x64_shared_delete_trace_ibl",
         "x86_to_x64_shared_trace_far", "x86_to_x64_shared_trace_far_unlinked",
         "x86_to_x64_shared_trace_cmp", "x86_to_x64_shared_trace_cmp_unlinked",
         "x86_to_x64_shared_trace_ibl", "x86_to_x64_shared_trace_ibl_template"},
        {"x86_to_x64_private_unlinked_bb_ibl", "x86_to_x64_private_delete_bb_ibl",
         "x86_to_x64_private_bb_far", "x86_to_x64_private_bb_far_unlinked",
         "x86_to_x64_private_bb_cmp", "x86_to_x64_private_bb_cmp_unlinked",
         "x86_to_x64_private_bb_ibl", "x86_to_x64_private_bb_ibl_template"},
        {"x86_to_x64_private_unlinked_trace_ibl", "x86_to_x64_private_delete_trace_ibl",
         "x86_to_x64_private_trace_far", "x86_to_x64_private_trace_far_unlinked",
         "x86_to_x64_private_trace_cmp", "x86_to_x64_private_trace_cmp_unlinked",
         "x86_to_x64_private_trace_ibl", "x86_to_x64_private_trace_ibl_template"},
        {"x86_to_x64_shared_unlinked_coarse_ibl", "x86_to_x64_shared_delete_coarse_ibl",
         "x86_to_x64_shared_coarse_trace_far",
         "x86_to_x64_shared_coarse_trace_far_unlinked",
         "x86_to_x64_shared_coarse_trace_cmp",
         "x86_to_x64_shared_coarse_trace_cmp_unlinked",
         "x86_to_x64_shared_coarse_ibl", "x86_to_x64_shared_coarse_ibl_template"},
        }
#endif
    };
    ibl_type_t ibl_type;
#ifdef X64
    gencode_mode_t mode;
#endif
    if (!get_ibl_routine_type_ex(dcontext, target, &ibl_type _IF_X64(&mode))) {
        /* not an IBL routine */
        if (!get_ibl_routine_template_type(dcontext, target, &ibl_type _IF_X64(&mode))) {
            return NULL;                /* not an IBL template either */
        }
    }
    /* ibl_type is valid and will give routine or template name, and qualifier */

    *ibl_brtype_name = get_branch_type_name(ibl_type.branch_type);
    return ibl_routine_names IF_X64([mode])
        [ibl_type.source_fragment_type][ibl_type.link_state];
}

static inline
ibl_code_t*
get_ibl_routine_code_internal(dcontext_t *dcontext,
                              ibl_source_fragment_type_t source_fragment_type,
                              ibl_branch_type_t branch_type
                              _IF_X64(gencode_mode_t mode))
{
#ifdef X64
    if (((mode == GENCODE_X86 ||
          (mode == GENCODE_FROM_DCONTEXT && dcontext != GLOBAL_DCONTEXT &&
           dcontext->x86_mode && !X64_CACHE_MODE_DC(dcontext))) &&
         shared_code_x86 == NULL) ||
        ((mode == GENCODE_X86_TO_X64 ||
          (mode == GENCODE_FROM_DCONTEXT && dcontext != GLOBAL_DCONTEXT &&
           dcontext->x86_mode && X64_CACHE_MODE_DC(dcontext))) &&
         shared_code_x86_to_x64 == NULL))
        return NULL;
#endif
    switch (source_fragment_type) {
    case IBL_BB_SHARED:
        if (!USE_SHARED_BB_IBL())
            return NULL;
        return &(get_shared_gencode(dcontext _IF_X64(mode))->bb_ibl[branch_type]);
    case IBL_BB_PRIVATE:
        return &(get_emitted_routines_code(dcontext _IF_X64(mode))->bb_ibl[branch_type]);
    case IBL_TRACE_SHARED:
        if (!USE_SHARED_TRACE_IBL())
            return NULL;
        return &(get_shared_gencode(dcontext _IF_X64(mode))->trace_ibl[branch_type]);
    case IBL_TRACE_PRIVATE:
        return &(get_emitted_routines_code(dcontext _IF_X64(mode))
                 ->trace_ibl[branch_type]);
    case IBL_COARSE_SHARED:
        if (!DYNAMO_OPTION(coarse_units))
            return NULL;
        return &(get_shared_gencode(dcontext _IF_X64(mode))->coarse_ibl[branch_type]);
    default:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return NULL;
}


cache_pc
get_ibl_routine_ex(dcontext_t *dcontext, ibl_entry_point_type_t entry_type,
                   ibl_source_fragment_type_t source_fragment_type,
                   ibl_branch_type_t branch_type _IF_X64(gencode_mode_t mode))
{
    ibl_code_t *ibl_code =
        get_ibl_routine_code_internal(dcontext,
                                      source_fragment_type, branch_type _IF_X64(mode));
    if (ibl_code == NULL || !ibl_code->initialized)
        return NULL;
    switch (entry_type) {
    case IBL_LINKED:
        return (cache_pc) ibl_code->indirect_branch_lookup_routine;
    case IBL_UNLINKED:
        return (cache_pc) ibl_code->unlinked_ibl_entry;
    case IBL_DELETE:
        return (cache_pc) ibl_code->target_delete_entry;
    case IBL_FAR:
        return (cache_pc) ibl_code->far_ibl;
    case IBL_FAR_UNLINKED:
        return (cache_pc) ibl_code->far_ibl_unlinked;
#ifdef X64
    case IBL_TRACE_CMP:
        return (cache_pc) ibl_code->trace_cmp_entry;
    case IBL_TRACE_CMP_UNLINKED:
        return (cache_pc) ibl_code->trace_cmp_unlinked;
#endif
    default:
        ASSERT_NOT_REACHED();
    }
    return NULL;
}

cache_pc
get_ibl_routine(dcontext_t *dcontext, ibl_entry_point_type_t entry_type,
                ibl_source_fragment_type_t source_fragment_type,
                ibl_branch_type_t branch_type)
{
    return get_ibl_routine_ex(dcontext, entry_type, source_fragment_type,
                              branch_type _IF_X64(GENCODE_FROM_DCONTEXT));
}

cache_pc
get_ibl_routine_template(dcontext_t *dcontext,
                         ibl_source_fragment_type_t source_fragment_type,
                         ibl_branch_type_t branch_type
                         _IF_X64(gencode_mode_t mode))
{
    ibl_code_t *ibl_code = get_ibl_routine_code_internal
        (dcontext, source_fragment_type, branch_type _IF_X64(mode));
    if (ibl_code == NULL || !ibl_code->initialized)
        return NULL;
    return ibl_code->inline_ibl_stub_template;
}

/* Convert FRAG_TABLE_* flags to FRAG_* flags */
/* FIXME This seems more appropriate in fragment.c but since there's no
 * need for the functionality there, we place it here and inline it. We
 * can move it if other pieces need the functionality later.
 */
static inline uint
table_flags_to_frag_flags(dcontext_t *dcontext, ibl_table_t *table)
{
    uint flags = 0;
    if (TEST(FRAG_TABLE_TARGET_SHARED, table->table_flags))
        flags |= FRAG_SHARED;
    if (TEST(FRAG_TABLE_TRACE, table->table_flags))
        flags |= FRAG_IS_TRACE;
    /* We want to make sure that any updates to FRAG_TABLE_* flags
     * are reflected in this routine. */
    ASSERT_NOT_IMPLEMENTED(!TESTANY(~(FRAG_TABLE_INCLUSIVE_HIERARCHY |
                                      FRAG_TABLE_IBL_TARGETED |
                                      FRAG_TABLE_TARGET_SHARED |
                                      FRAG_TABLE_SHARED |
                                      FRAG_TABLE_TRACE |
                                      FRAG_TABLE_PERSISTENT |
                                      HASHTABLE_USE_ENTRY_STATS |
                                      HASHTABLE_ALIGN_TABLE),
                                    table->table_flags));
    return flags;
}

/* Derive the PC of an entry point that aids in atomic hashtable deletion.
 * FIXME: Once we can correlate from what table the fragment is being
 * deleted and therefore type of the corresponding IBL routine, we can
 * widen the interface and be more precise about which entry point
 * is returned, i.e., specify something other than IBL_GENERIC.
 */
cache_pc
get_target_delete_entry_pc(dcontext_t *dcontext, ibl_table_t *table)
{
    /*
     * A shared IBL routine makes sure any registers restored on the
     * miss path are all saved in the current dcontext - as well as
     * copying the ECX in both TLS scratch and dcontext, so it is OK
     * to simply return the thread private routine.  We have
     * proven that they are functionally equivalent (all data in the
     * shared lookup is fs indirected to the private dcontext)
     *
     * FIXME: we can in fact use a global delete_pc entry point that
     * is the unlinked path of a shared_ibl_not_found, just like we
     * could share all routines. Since it doesn't matter much for now
     * we can also return the slightly more efficient private
     * ibl_not_found path.
     */
    uint frag_flags = table_flags_to_frag_flags(dcontext, table);

    ASSERT(dcontext != GLOBAL_DCONTEXT);

    return (cache_pc) get_ibl_routine(dcontext, IBL_DELETE,
                                      get_source_fragment_type(dcontext,
                                                               frag_flags),
                                      table->branch_type);
}

ibl_code_t *
get_ibl_routine_code_ex(dcontext_t *dcontext, ibl_branch_type_t branch_type,
                        uint fragment_flags _IF_X64(gencode_mode_t mode))
{
    ibl_source_fragment_type_t source_fragment_type =
        get_source_fragment_type(dcontext, fragment_flags);

    ibl_code_t *ibl_code =
        get_ibl_routine_code_internal(dcontext, source_fragment_type, branch_type
                                      _IF_X64(mode));
    ASSERT(ibl_code != NULL);
    return ibl_code;
}

ibl_code_t *
get_ibl_routine_code(dcontext_t *dcontext, ibl_branch_type_t branch_type,
                     uint fragment_flags)
{
    return get_ibl_routine_code_ex(dcontext, branch_type, fragment_flags
                                   _IF_X64(dcontext == GLOBAL_DCONTEXT ?
                                           FRAGMENT_GENCODE_MODE(fragment_flags) :
                                           GENCODE_FROM_DCONTEXT));
}


#ifdef WINDOWS
/* FIXME We support a private and shared fragments simultaneously targeting
 * shared syscall -- -shared_fragment_shared_syscalls must be on and both
 * fragment types target the entry point in shared_code. We could optimize
 * the private fragment->shared syscall path (case 8025).
 */
/* PR 282576: These separate routines are ugly, but less ugly than adding param to
 * the main routines, which are called in many places and usually passed a
 * non-global dcontext; also less ugly than adding GLOBAL_DCONTEXT_X86.
 */
cache_pc
shared_syscall_routine_ex(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = DYNAMO_OPTION(shared_fragment_shared_syscalls) ?
        get_shared_gencode(dcontext _IF_X64(mode)) :
        get_emitted_routines_code(dcontext _IF_X64(mode));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc) code->shared_syscall;
}

cache_pc
shared_syscall_routine(dcontext_t *dcontext)
{
    return shared_syscall_routine_ex(dcontext _IF_X64(GENCODE_FROM_DCONTEXT));
}

cache_pc
unlinked_shared_syscall_routine_ex(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = DYNAMO_OPTION(shared_fragment_shared_syscalls) ?
        get_shared_gencode(dcontext _IF_X64(mode)) :
        get_emitted_routines_code(dcontext _IF_X64(mode));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc) code->unlinked_shared_syscall;
}

cache_pc
unlinked_shared_syscall_routine(dcontext_t *dcontext)
{
    return unlinked_shared_syscall_routine_ex(dcontext _IF_X64(GENCODE_FROM_DCONTEXT));
}

cache_pc
after_shared_syscall_code(dcontext_t *dcontext)
{
    return after_shared_syscall_code_ex(dcontext  _IF_X64(GENCODE_FROM_DCONTEXT));
}

cache_pc
after_shared_syscall_code_ex(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X64(mode));
    ASSERT(code != NULL);
    return (cache_pc) (code->unlinked_shared_syscall + code->sys_syscall_offs);
}

cache_pc
after_shared_syscall_addr(dcontext_t *dcontext)
{
    ASSERT(get_syscall_method() != SYSCALL_METHOD_UNINITIALIZED);
    if (DYNAMO_OPTION(sygate_int) &&
        get_syscall_method() == SYSCALL_METHOD_INT)
        return (int_syscall_address + INT_LENGTH /* sizeof int 2e */);
    else
        return after_shared_syscall_code(dcontext);
}

/* These are Windows-only since Linux needs to disambiguate its two
 * versions of do_syscall
 */
cache_pc
after_do_syscall_code(dcontext_t *dcontext)
{
    return after_do_syscall_code_ex(dcontext  _IF_X64(GENCODE_FROM_DCONTEXT));
}

cache_pc
after_do_syscall_code_ex(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X64(mode));
    ASSERT(code != NULL);
    return (cache_pc) (code->do_syscall + code->do_syscall_offs);
}

cache_pc
after_do_syscall_addr(dcontext_t *dcontext)
{
    ASSERT(get_syscall_method() != SYSCALL_METHOD_UNINITIALIZED);
    if (DYNAMO_OPTION(sygate_int) &&
        get_syscall_method() == SYSCALL_METHOD_INT)
        return (int_syscall_address + INT_LENGTH /* sizeof int 2e */);
    else
        return after_do_syscall_code(dcontext);
}
#else
cache_pc
after_do_shared_syscall_addr(dcontext_t *dcontext)
{
    /* PR 212570: return the thread-shared do_syscall used for vsyscall hook */
    generated_code_t *code = get_emitted_routines_code(GLOBAL_DCONTEXT
                                                       _IF_X64(GENCODE_X64));
    IF_X64(ASSERT_NOT_REACHED()); /* else have to worry about GENCODE_X86 */
    ASSERT(code != NULL);
    ASSERT(code->do_syscall != NULL);
    return (cache_pc) (code->do_syscall + code->do_syscall_offs);
}

cache_pc
after_do_syscall_addr(dcontext_t *dcontext)
{
    /* PR 212570: return the thread-shared do_syscall used for vsyscall hook */
    generated_code_t *code = get_emitted_routines_code(dcontext
                                                       _IF_X64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    ASSERT(code->do_syscall != NULL);
    return (cache_pc) (code->do_syscall + code->do_syscall_offs);
}

static bool
is_after_main_do_syscall_addr(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = get_emitted_routines_code(dcontext
                                                       _IF_X64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    return (pc == (cache_pc) (code->do_syscall + code->do_syscall_offs));
}

bool
is_after_do_syscall_addr(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = get_emitted_routines_code(dcontext
                                                       _IF_X64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    return (pc == (cache_pc) (code->do_syscall + code->do_syscall_offs) ||
            pc == (cache_pc) (code->do_int_syscall + code->do_int_syscall_offs)
            IF_VMX86(|| pc == (cache_pc) (code->do_vmkuw_syscall +
                                          code->do_vmkuw_syscall_offs)));
}
#endif

bool
is_after_syscall_address(dcontext_t *dcontext, cache_pc pc)
{
#ifdef WINDOWS
    if (pc == after_shared_syscall_addr(dcontext))
        return true;
    if (pc == after_do_syscall_addr(dcontext))
        return true;
    return false;
#else
    return is_after_do_syscall_addr(dcontext, pc);
#endif
    /* NOTE - we ignore global_do_syscall since that's only used in special
     * circumstances and is not something the callers (recreate_app_state)
     * really know how to handle. */
}

/* needed b/c linux can have sysenter as main syscall method but also
 * has generated int syscall routines
 */
static bool
is_after_syscall_that_rets(dcontext_t *dcontext, cache_pc pc)
{
#ifdef WINDOWS
    return (is_after_syscall_address(dcontext, pc) &&
            does_syscall_ret_to_callsite());
#else
    generated_code_t *code = get_emitted_routines_code(dcontext
                                                       _IF_X64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    return ((pc == (cache_pc) (code->do_syscall + code->do_syscall_offs) &&
             does_syscall_ret_to_callsite()) ||
            pc == (cache_pc) (code->do_int_syscall + code->do_int_syscall_offs)
            IF_VMX86(|| pc == (cache_pc) (code->do_vmkuw_syscall +
                                          code->do_vmkuw_syscall_offs)));
#endif
}

#ifdef UNIX
/* PR 212290: can't be static code in x86.asm since it can't be PIC */
cache_pc
get_new_thread_start(dcontext_t *dcontext _IF_X64(gencode_mode_t mode))
{
#ifdef HAVE_TLS
    /* for HAVE_TLS we use the shared version; w/o TLS we don't
     * make any shared routines (PR 361894)
     */
    dcontext = GLOBAL_DCONTEXT;
#endif
    generated_code_t *gen = get_emitted_routines_code(dcontext _IF_X64(mode));
    return gen->new_thread_dynamo_start;
}
#endif

#ifdef TRACE_HEAD_CACHE_INCR
cache_pc
trace_head_incr_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->trace_head_incr;
}
#endif

#ifdef CHECK_RETURNS_SSE2_EMIT
cache_pc
get_pextrw_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->pextrw;
}

cache_pc
get_pinsrw_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc) code->pinsrw;
}
#endif

/* exported beyond x86/ */
fcache_enter_func_t
get_fcache_enter_shared_routine(dcontext_t *dcontext)
{
    return fcache_enter_shared_routine(dcontext);
}

fcache_enter_func_t
fcache_enter_shared_routine(dcontext_t *dcontext)
{
    ASSERT(USE_SHARED_GENCODE());
    return (fcache_enter_func_t)
        convert_data_to_function(SHARED_GENCODE_MATCH_THREAD(dcontext)->fcache_enter);
}

cache_pc
fcache_return_shared_routine(IF_X64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X64(mode));
    ASSERT(USE_SHARED_GENCODE());
    if (code == NULL)
        return NULL;
    else
        return code->fcache_return;
}

#ifdef TRACE_HEAD_CACHE_INCR
cache_pc
trace_head_incr_shared_routine(IF_X64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X64(mode));
    ASSERT(USE_SHARED_GENCODE());
    if (code == NULL)
        return NULL;
    else
        return code->trace_head_incr;
}
#endif

/* get the fcache target for the next code cache entry */
cache_pc
get_fcache_target(dcontext_t *dcontext)
{
    /* we used to use mcontext.pc, but that's in the writable
     * portion of the dcontext, and so for self-protection we use the
     * next_tag slot, which is protected
     */
    return dcontext->next_tag;
}

/* set the fcache target for the next code cache entry */
void
set_fcache_target(dcontext_t *dcontext, cache_pc value)
{
    /* we used to use mcontext.pc, but that's in the writable
     * portion of the dcontext, and so for self-protection we use the
     * next_tag slot, which is protected
     */
    dcontext->next_tag = value;
    /* set eip as well to complete mcontext state */
    get_mcontext(dcontext)->pc = value;
}

/***************************************************************************
 * FAULT TRANSLATION
 *
 * Current status:
 * After PR 214962, PR 267260, PR 263407, PR 268372, and PR 267764/i398, we
 * properly translate indirect branch mangling and client modifications.
 * FIXME: However, we still do not properly translate for:
 * - PR 303413: properly translate native_exec and windows sysenter mangling faults
 * - PR 208037/i#399: flushed fragments (need -safe_translate_flushed)
 * - PR 213251: hot patch fragments (b/c nudge can change whether patched =>
 *     should store translations for all hot patch fragments)
 * - PR 372021: restore eflags if within window of ibl or trace-cmp eflags-are-dead
 * - i#751: fault translation has not been tested for x86_to_x64
 */

typedef struct _translate_walk_t {
    /* The context we're translating */
    priv_mcontext_t *mc;
    /* The code cache span of the containing fragment */
    byte *start_cache;
    byte *end_cache;
    /* PR 263407: Track registers spilled since the last cti, for
     * restoring indirect branch and rip-rel spills
     */
    bool reg_spilled[REG_SPILL_NUM];
    bool reg_tls[REG_SPILL_NUM];
    /* PR 267260: Track our own mangle-inserted pushes and pops, for
     * restoring state in the middle of our indirect branch mangling.
     * This is the adjustment in the forward direction.
     */
    int xsp_adjust;
    /* Track whether we've seen an instr for which we can't relocate */
    bool unsupported_mangle;
    /* Are we currently in a mangle region */
    bool in_mangle_region;
    /* What is the translation target of the current mangle region */
    app_pc translation;
} translate_walk_t;

static void
translate_walk_init(translate_walk_t *walk, byte *start_cache, byte *end_cache,
                    priv_mcontext_t *mc)
{
    memset(walk, 0, sizeof(*walk));
    walk->mc = mc;
    walk->start_cache = start_cache;
    walk->end_cache = end_cache;
}

static inline bool
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

static inline bool
instr_is_trace_cmp(dcontext_t *dcontext, instr_t *inst)
{
    /* We don't support restoring a fault in the middle, but we
     * identify here to avoid "unsupported mangle instr" message
     */
    if (!instr_is_our_mangling(inst))
        return false;
    return
#ifdef X64
        instr_get_opcode(inst) == OP_mov_imm ||
        /* mov %rax -> xbx-tls-spill-slot */
        instr_get_opcode(inst) == OP_mov_st ||
        instr_get_opcode(inst) == OP_lahf ||
        instr_get_opcode(inst) == OP_seto ||
        instr_get_opcode(inst) == OP_cmp ||
        instr_get_opcode(inst) == OP_jnz ||
        instr_get_opcode(inst) == OP_add ||
        instr_get_opcode(inst) == OP_sahf
#else
        instr_get_opcode(inst) == OP_lea ||
        instr_get_opcode(inst) == OP_jecxz ||
        instr_get_opcode(inst) == OP_jmp
#endif
        ;
}

#ifdef UNIX
static inline bool
instr_is_inline_syscall_jmp(dcontext_t *dcontext, instr_t *inst)
{
    if (!instr_is_our_mangling(inst))
        return false;
    /* Not bothering to check whether there's a nearby syscall instr:
     * any label-targeting short jump should be fine to ignore.
     */
    return (instr_get_opcode(inst) == OP_jmp_short &&
            opnd_is_instr(instr_get_target(inst)));
}

static inline bool
instr_is_seg_ref_load(dcontext_t *dcontext, instr_t *inst)
{
    /* This won't fault but we don't want "unsupported mangle instr" message. */
    if (!instr_is_our_mangling(inst))
        return false;
    /* Look for the load of either segment base */
    if (instr_is_tls_restore(inst, REG_NULL/*don't care*/,
                             os_tls_offset(os_get_app_seg_base_offset(SEG_FS))) ||
        instr_is_tls_restore(inst, REG_NULL/*don't care*/,
                             os_tls_offset(os_get_app_seg_base_offset(SEG_GS))))
        return true;
    /* Look for the lea */
    if (instr_get_opcode(inst) == OP_lea) {
        opnd_t mem = instr_get_src(inst, 0);
        if (opnd_get_scale(mem) == 1 &&
            opnd_get_index(mem) == opnd_get_reg(instr_get_dst(inst, 0)))
            return true;
    }
    return false;
}
#endif

static void
translate_walk_track(dcontext_t *tdcontext, instr_t *inst, translate_walk_t *walk)
{
    reg_id_t reg, r;
    bool spill, spill_tls;

    /* Two mangle regions can be adjacent: distinguish by translation field */
    if (walk->in_mangle_region &&
        (!instr_is_our_mangling(inst) ||
         instr_get_translation(inst) != walk->translation)) {
        /* We assume our manglings are local and contiguous: once out of a
         * mangling region, we're good to go again */
        walk->in_mangle_region = false;
        walk->unsupported_mangle = false;
        walk->xsp_adjust = 0;
        for (r = 0; r < REG_SPILL_NUM; r++) {
            /* we should have seen a restore for every spill, unless at
             * fragment-ending jump to ibl, which shouldn't come here
             */
            ASSERT(!walk->reg_spilled[r]);
            walk->reg_spilled[r] = false; /* be paranoid */
        }
    }

    if (instr_is_our_mangling(inst)) {
        if (!walk->in_mangle_region) {
            walk->in_mangle_region = true;
            walk->translation = instr_get_translation(inst);
        } else
            ASSERT(walk->translation == instr_get_translation(inst));
        /* PR 302951: we recognize a clean call by its NULL translation.
         * We do not track any stack or spills: we assume we will only
         * fault on an argument that references app memory, in which case
         * we restore to the priv_mcontext_t on the stack.
         */
        if (walk->translation == NULL) {
            DOLOG(4, LOG_INTERP, {
                loginst(get_thread_private_dcontext(), 4, inst,
                        "\tin clean call arg region");
            });
            return;
        }
        /* PR 263407: track register values that we've spilled.  We assume
         * that spilling to non-canonical slots only happens in ibl or
         * context switch code: never in app code mangling.  Since a client
         * might add ctis (non-linear code) and its own spills, we track
         * register spills only within our own mangling code (for
         * post-mangling traces (PR 306163) we require that the client
         * handle all translation if it modifies our mangling regions:
         * we'll provide a query routine instr_is_DR_mangling()): our
         * spills are all local anyway, except
         * for selfmod, which we hardcode rep-string support for (non-linear code
         * isn't handled by general reg scan).  Our trace cmp is the only
         * instance (besides selfmod) where we have a cti in our mangling,
         * but it doesn't affect our linearity assumption.  We assume we
         * have no entry points in between a spill and a restore.  Our
         * mangling goes in last (for regular bbs and traces; see
         * comment above for post-mangling traces), and so for local
         * spills like rip-rel and ind branches this is fine.
         */
        if (instr_is_cti(inst) &&
            /* Do not reset for a trace-cmp jecxz or jmp (32-bit) or
             * jne (64-bit), since ecx needs to be restored (won't
             * fault, but for thread relocation)
             */
            ((instr_get_opcode(inst) != OP_jecxz &&
              instr_get_opcode(inst) != OP_jmp &&
              /* x64 trace cmp uses jne for exit */
              instr_get_opcode(inst) != OP_jne) ||
             /* Rather than check for trace, just ignore exit jumps, which
              * won't mess up linearity here. For stored translation info we
              * don't have meta-flags so we can't use instr_is_exit_cti(). */
             ((instr_get_opcode(inst) == OP_jmp ||
               /* x64 trace cmp uses jne for exit */
               instr_get_opcode(inst) == OP_jne) &&
              (!opnd_is_pc(instr_get_target(inst)) ||
               (opnd_get_pc(instr_get_target(inst)) >= walk->start_cache &&
                opnd_get_pc(instr_get_target(inst)) < walk->end_cache))))) {
            /* reset for non-exit non-trace-jecxz cti (i.e., selfmod cti) */
            for (r = 0; r < REG_SPILL_NUM; r++)
                walk->reg_spilled[r] = false;
        }
        if (instr_is_reg_spill_or_restore(tdcontext, inst, &spill_tls, &spill, &reg)) {
            r = reg - REG_START_SPILL;
            /* if a restore whose spill was before a cti, ignore */
            if (spill || walk->reg_spilled[r]) {
                /* ensure restores and spills are properly paired up */
                ASSERT((spill && !walk->reg_spilled[r]) ||
                       (!spill && walk->reg_spilled[r]));
                ASSERT(spill || walk->reg_tls[r] == spill_tls);
                walk->reg_spilled[r] = spill;
                walk->reg_tls[r] = spill_tls;
                LOG(THREAD_GET, LOG_INTERP, 5,
                    "\tspill update: %s %s %s\n", spill ? "spill" : "restore",
                    spill_tls ? "tls" : "mcontext", reg_names[reg]);
            }
        }
        /* PR 267260: Track our own mangle-inserted pushes and pops, for
         * restoring state on an app fault in the middle of our indirect
         * branch mangling.  We only need to support instrs added up until
         * the last one that could have an app fault, as we can fail when
         * called to translate for thread relocation: thus we ignore
         * syscall mangling.
         *
         * The main scenarios are:
         *
         * 1) call*: "spill ecx; mov->ecx; push retaddr":
         *    ecx restore handled above
         * 2) far direct call: "push cs; push retaddr"
         *    if fail on 2nd push need to undo 1st push
         * 3) far call*: "spill ecx; tgt->ecx; push cs; push retaddr"
         *    if fail on 1st push, restore ecx (above); 2nd push, also undo 1st push
         * 4) iret: "pop eip; pop cs; pop eflags; (pop rsp; pop ss)"
         *    if fail on non-initial pop, undo earlier pops
         * 5) lret: "pop eip; pop cs"
         *    if fail on non-initial pop, undo earlier pops
         *
         * FIXME: some of these push/pops are simulated (we simply adjust
         * esp or do nothing), so we're not truly fault-transparent.
         */
        else if (instr_check_xsp_mangling(tdcontext, inst, &walk->xsp_adjust)) {
            /* walk->xsp_adjust is now adjusted */
        }
        else if (instr_is_trace_cmp(tdcontext, inst)) {
            /* nothing to do */
        }
#ifdef UNIX
        else if (instr_is_inline_syscall_jmp(tdcontext, inst)) {
            /* nothing to do */
        }
        else if (instr_is_seg_ref_load(tdcontext, inst)) {
            /* nothing to do */
        }
#endif
        else if (instr_ok_to_mangle(inst)) {
            /* To have reg spill+restore in the same mangle region, we mark
             * the (modified) app instr for rip-rel and for segment mangling as
             * "our mangling".  There's nothing specific to do for it.
             */
        }
        /* We do not support restoring state at arbitrary points for thread
         * relocation (a performance issue, not a correctness one): if not a
         * spill, restore, push, or pop, we will not properly translate.
         * For an exit jmp for a simple ret we could relocate: but better not to
         * for a call, since we've modified the stack w/ a push, so we fail on
         * all exit jmps.
         */
        else {
            DOLOG(4, LOG_INTERP,
                  loginst(get_thread_private_dcontext(), 4,
                          inst, "unsupported mangle instr"););
            walk->unsupported_mangle = true;
        }
    }
}

static bool
translate_walk_good_state(dcontext_t *tdcontext, translate_walk_t *walk,
                          app_pc translate_pc)
{
    return (!walk->unsupported_mangle ||
            /* If we're at the instr AFTER the mangle region, we're ok */
            (walk->in_mangle_region && translate_pc != walk->translation));
}

static void
translate_walk_restore(dcontext_t *tdcontext, translate_walk_t *walk,
                       app_pc translate_pc)
{
    reg_id_t r;

    if (translate_pc != walk->translation) {
        /* When we walk we update only each instr we pass.  If we're
         * now sitting at the instr AFTER the mangle region, we do
         * NOT want to adjust xsp, since we're not translating to
         * before that instr.  We should not have any outstanding spills.
         */
        LOG(THREAD_GET, LOG_INTERP, 2,
            "\ttranslation "PFX" is post-walk "PFX" so not fixing xsp\n",
            translate_pc, walk->translation);
        DOCHECK(1, {
            for (r = 0; r < REG_SPILL_NUM; r++)
                ASSERT(!walk->reg_spilled[r]);
        });
        return;
    }

    /* PR 263407: restore register values that are currently in spill slots
     * for ind branches or rip-rel mangling.
     * FIXME: for rip-rel loads, we may have clobbered the destination
     * already, and won't be able to restore it: but that's a minor issue.
     */
    for (r = 0; r < REG_SPILL_NUM; r++) {
        if (walk->reg_spilled[r]) {
            reg_id_t reg = r + REG_START_SPILL;
            reg_t value;
            if (walk->reg_tls[r]) {
                value = *(reg_t *)(((byte*)&tdcontext->local_state->spill_space) +
                                   reg_spill_tls_offs(reg));
            } else {
                value = reg_get_value_priv(reg, get_mcontext(tdcontext));
            }
            LOG(THREAD_GET, LOG_INTERP, 2,
                "\trestoring spilled %s to "PFX"\n", reg_names[reg], value);
            STATS_INC(recreate_spill_restores);
            reg_set_value_priv(reg, walk->mc, value);
        }
    }
    /* PR 267260: Restore stack-adjust mangling of ctis.
     * FIXME: we do NOT undo writes to the stack, so we're not completely
     * transparent.  If we ever do restore memory, we'll want to pass in
     * the restore_memory param.
     */
    if (walk->xsp_adjust != 0) {
        walk->mc->xsp -= walk->xsp_adjust; /* negate to undo */
        LOG(THREAD_GET, LOG_INTERP, 2,
            "\tundoing push/pop by %d: xsp now "PFX"\n",
            walk->xsp_adjust, walk->mc->xsp);
    }
}

static void
translate_restore_clean_call(dcontext_t *tdcontext, translate_walk_t *walk)
{
    /* PR 302951: we recognize a clean call by its combination of
     * our-mangling and NULL translation.
     * We restore to the priv_mcontext_t that was pushed on the stack.
     */
    LOG(THREAD_GET, LOG_INTERP, 2, "\ttranslating clean call arg crash\n");
    dr_get_mcontext_priv(tdcontext, NULL, walk->mc);
    /* walk->mc->pc will be fixed up by caller */

    /* PR 306410: up to caller to shift signal or SEH frame from dstack
     * to app stack.  We naturally do that already for linux b/c we always
     * have an alternate signal handling stack, but for Windows it takes
     * extra work.
     */
}

/* Returns a success code, but makes a best effort regardless.
 * If just_pc is true, only recreates pc.
 * Modifies mc with the recreated state.
 * The caller must ensure tdcontext remains valid.
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
static recreate_success_t
recreate_app_state_from_info(dcontext_t *tdcontext, const translation_info_t *info,
                             byte *start_cache, byte *end_cache,
                             priv_mcontext_t *mc, bool just_pc _IF_DEBUG(uint flags))
{
    byte *answer = NULL;
    byte *cpc, *prev_cpc;
    cache_pc target_cache = mc->pc;
    uint i;
    bool contig = true, ours = false;
    recreate_success_t res = (just_pc ? RECREATE_SUCCESS_PC : RECREATE_SUCCESS_STATE);
    instr_t instr;
    translate_walk_t walk;
    translate_walk_init(&walk, start_cache, end_cache, mc);
    instr_init(tdcontext, &instr);

    ASSERT(info != NULL);
    ASSERT(end_cache >= start_cache);

    LOG(THREAD_GET, LOG_INTERP, 3,
        "recreate_app : looking for "PFX" in frag @ "PFX" (tag "PFX")\n",
        target_cache, start_cache, info->translation[0].app);
    DOLOG(3, LOG_INTERP, {
        translation_info_print(info, start_cache, THREAD_GET);
    });

    /* Strategy: walk through cache instrs, updating current app translation
     * as we go along from the info table.  The table records only
     * translations at change points and must interpolate between them, using
     * either a stride of 0 if the previous translation entry is marked
     * "identical" or a stride equal to the instruction length as we decode
     * from the cache if the previous entry is !identical=="contiguous".
     */

    cpc = start_cache;
    ASSERT(cpc - start_cache == info->translation[0].cache_offs);
    i = 0;
    while (cpc < end_cache) {
        /* we can go beyond the end of the table: then use the last point */
        if (i < info->num_entries &&
            cpc - start_cache >= info->translation[i].cache_offs) {
            /* We hit a change point: new app translation target */
            answer = info->translation[i].app;
            contig = !TEST(TRANSLATE_IDENTICAL, info->translation[i].flags);
            ours = TEST(TRANSLATE_OUR_MANGLING, info->translation[i].flags);
            i++;
        }

        if (cpc >= target_cache) {
            /* we found the target to translate */
            ASSERT(cpc == target_cache);
            if (cpc > target_cache) { /* in debug will hit assert 1st */
                LOG(THREAD_GET, LOG_INTERP, 2,
                    "recreate_app -- WARNING: cache pc "PFX" != "PFX"\n",
                    cpc, target_cache);
                res = RECREATE_FAILURE; /* try to restore, but return false */
            }
            break;
        }

        /* PR 263407/PR 268372: we need to decode to instr level to track register
         * values that we've spilled, and watch for ctis.  So far we don't need
         * enough to justify a full decode_fragment().
         */
        instr_reset(tdcontext, &instr);
        prev_cpc = cpc;
        cpc = decode(tdcontext, cpc, &instr);
        instr_set_our_mangling(&instr, ours);
        translate_walk_track(tdcontext, &instr, &walk);

        /* advance translation by the stride: either instr length or 0 */
        if (contig)
            answer += (cpc - prev_cpc);
        /* else, answer stays put */
    }
    /* should always find xlation */
    ASSERT(cpc < end_cache);
    instr_free(tdcontext, &instr);

    if (answer == NULL || !translate_walk_good_state(tdcontext, &walk, answer)) {
        /* PR 214962: we're either in client meta-code (NULL translation) or
         * post-app-fault in our own manglings: we shouldn't get an app
         * fault in either case, so it's ok to fail, and neither is a safe
         * spot for thread relocation.  For client meta-code we could split
         * synch view (since we can get the app state consistent, just not
         * the client state) from synch relocate, but that would require
         * synchall re-architecting and may not be a noticeable perf win
         * (should spend enough time at syscalls that will hit safe spot in
         * reasonable time).
         */
        /* PR 302951: our clean calls do show up here and have full state */
        if (answer == NULL && ours)
            translate_restore_clean_call(tdcontext, &walk);
        else
            res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
        /* should only happen for thread synch, not a fault */
        DOCHECK(1, {
            if (!(res == RECREATE_SUCCESS_STATE /* clean call */ ||
                  tdcontext != get_thread_private_dcontext() ||
                  INTERNAL_OPTION(stress_recreate_pc) ||
                  /* we can currently fail for flushed code (PR 208037/i#399)
                   * (and hotpatch, native_exec, and sysenter: but too rare to check) */
                  TEST(FRAG_SELFMOD_SANDBOXED, flags) ||
                  TEST(FRAG_WAS_DELETED, flags))) {
                CLIENT_ASSERT(false, "meta-instr faulted?  must set translation"
                              " field and handle fault!");
            }
        });
        if (answer == NULL) {
            /* use next instr's translation.  skip any further meta-instrs regions. */
            for (; i < info->num_entries; i++) {
                if (info->translation[i].app != NULL)
                    break;
            }
            ASSERT(i < info->num_entries);
            if (i < info->num_entries)
                answer = info->translation[i].app;;
            ASSERT(answer != NULL);
        }
    }

    if (!just_pc)
        translate_walk_restore(tdcontext, &walk, answer);
    LOG(THREAD_GET, LOG_INTERP, 2,
        "recreate_app -- found ok pc "PFX"\n", answer);
    mc->pc = answer;
    return res;
}

/* Returns a success code, but makes a best effort regardless.
 * If just_pc is true, only recreates pc.
 * Modifies mc with the recreated state.
 * The caller must ensure tdcontext remains valid.
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
static recreate_success_t
recreate_app_state_from_ilist(dcontext_t *tdcontext, instrlist_t *ilist,
                              byte *start_app, byte *start_cache, byte *end_cache,
                              priv_mcontext_t *mc, bool just_pc, uint flags)
{
    byte *answer = NULL;
    byte *cpc, *prev_bytes;
    instr_t *inst, *prev_ok;
    cache_pc target_cache = mc->pc;
    recreate_success_t res = (just_pc ? RECREATE_SUCCESS_PC : RECREATE_SUCCESS_STATE);
    translate_walk_t walk;

    LOG(THREAD_GET, LOG_INTERP, 3,
        "recreate_app : looking for "PFX" in frag @ "PFX" (tag "PFX")\n",
        target_cache, start_cache, start_app);

    DOLOG(5, LOG_INTERP, {
        instrlist_disassemble(tdcontext, 0, ilist, THREAD_GET);
    });

    /* walk ilist, incrementing cache pc by each instr's length until
     * cache pc equals target, then look at original address of
     * current instr, which is set by routines in mangle except for
     * cti_short_rewrite.
     */
    cpc = start_cache;
    /* since asking for the length will encode to a buffer, we cannot
     * walk backwards at all.  thus we keep track of the previous instr
     * with valid original bytes.
     */
    prev_ok = NULL;
    prev_bytes = NULL;

    translate_walk_init(&walk, start_cache, end_cache, mc);

    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        int len = instr_length(tdcontext, inst);

        /* All we care about is that we are not going to skip over a
         * bundle of app instructions.
         */
        ASSERT(!instr_is_level_0(inst));

        /* Case 4531, 4344: raw instructions being up-decoded can have
         * their translation fields clobbered so we don't want any of those.
         * (We used to have raw jecxz and nop instrs.)
         * FIXME: if bb associated with this instr was hot patched, then
         * the inserted raw instructions can trigger this assert.  Part of
         * fix for case 5981.  In that case, this would be harmless.
         */
        ASSERT_CURIOSITY(instr_operands_valid(inst));

        /* PR 332437: skip label instrs.  Nobody should expect setting
         * a label's translation field to have any effect, and we
         * don't need to explicitly split our mangling regions at
         * labels so no reason to call translate_walk_track().
         *
         * We also skip all other length 0 instrs.  That would
         * include un-encodable instrs, which we wouldn't have output,
         * and so we should skip here in case the very next instr that we
         * did encode had the real fault.
         */
        if (len == 0)
            continue;

        /* note this will be exercised for all instructions up to the answer */
#ifndef CLIENT_INTERFACE
# ifdef INTERNAL
        ASSERT(instr_get_translation(inst) != NULL || DYNAMO_OPTION(optimize));
# else
        ASSERT(instr_get_translation(inst) != NULL);
# endif
#endif

        LOG(THREAD_GET, LOG_INTERP, 5, "cache pc "PFX" vs "PFX"\n",
            cpc, target_cache);
        if (cpc >= target_cache) {
            if (cpc > target_cache) {
                if (cpc == start_cache) {
                    /* Prefix instructions are not added to recreate_fragment_ilist()
                     * FIXME: we should do so, and then we can at least restore
                     * our spills, just in case.
                     */
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- cache pc "PFX" != "PFX", "
                        "assuming a prefix instruction\n", cpc, target_cache);
                    res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                    /* should only happen for thread synch, not a fault */
                    ASSERT(tdcontext != get_thread_private_dcontext() ||
                           INTERNAL_OPTION(stress_recreate_pc));
                } else {
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- WARNING: cache pc "PFX" != "PFX", "
                        "probably prefix instruction\n", cpc, target_cache);
                    res = RECREATE_FAILURE; /* try to restore, but return false */
                }
            }
            if (instr_get_translation(inst) == NULL) {
                /* Clients are supposed to leave their meta instrs with
                 * NULL translations.  (DR may hit this assert for
                 * -optimize but we need to fix that by setting translation
                 * for all our optimizations.)  We assume we will never
                 * get an app fault here, so we fail if asked for full state
                 * since although we can get full app state we can't relocate
                 * in the middle of client meta code.
                 */
                ASSERT(!instr_ok_to_mangle(inst));
                /* PR 302951: our clean calls do show up here and have full state */
                if (instr_is_our_mangling(inst))
                    translate_restore_clean_call(tdcontext, &walk);
                else
                    res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                /* should only happen for thread synch, not a fault */
                DOCHECK(1, {
                    if (!(instr_is_our_mangling(inst) /* PR 302951 */ ||
                          tdcontext != get_thread_private_dcontext() ||
                          INTERNAL_OPTION(stress_recreate_pc)
                          IF_CLIENT_INTERFACE(||
                                              tdcontext->client_data->is_translating))) {
                        CLIENT_ASSERT(false, "meta-instr faulted?  must set translation "
                                      "field and handle fault!");
                    }
                });
                if (prev_ok == NULL) {
                    answer = start_app;
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- WARNING: guessing start pc "PFX"\n", answer);
                } else {
                    answer = prev_bytes;
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- WARNING: guessing after prev "
                        "translation (pc "PFX")\n", answer);
                    DOLOG(2, LOG_INTERP, loginst(get_thread_private_dcontext(),
                                                 2, prev_ok, "\tprev instr"););
                }
            } else {
                answer = instr_get_translation(inst);
                if (translate_walk_good_state(tdcontext, &walk, answer)) {
                    LOG(THREAD_GET, LOG_INTERP, 2,
                        "recreate_app -- found valid state pc "PFX"\n", answer);
                } else {
                    int op = instr_get_opcode(inst);
                    if (TEST(FRAG_SELFMOD_SANDBOXED, flags) &&
                        (op == OP_rep_ins || op == OP_rep_movs || op == OP_rep_stos)) {
                        /* i#398: xl8 selfmod: rep string instrs have xbx spilled in
                         * thread-private slot.  We assume no other selfmod mangling
                         * has a reg spilled at time of app instr execution.
                         */
                        if (!just_pc) {
                            walk.mc->xbx = get_mcontext(tdcontext)->xbx;
                            LOG(THREAD_GET, LOG_INTERP, 2,
                                "\trestoring spilled xbx to "PFX"\n", walk.mc->xbx);
                            STATS_INC(recreate_spill_restores);
                        }
                        LOG(THREAD_GET, LOG_INTERP, 2,
                            "recreate_app -- found valid state pc "PFX"\n", answer);
                    } else {
                        res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                        /* should only happen for thread synch, not a fault */
                        ASSERT(tdcontext != get_thread_private_dcontext() ||
                               INTERNAL_OPTION(stress_recreate_pc) ||
                               /* we can currently fail for flushed code (PR 208037)
                                * (and hotpatch, native_exec, and sysenter: but too
                                * rare to check) */
                               TEST(FRAG_SELFMOD_SANDBOXED, flags) ||
                               TEST(FRAG_WAS_DELETED, flags));
                        LOG(THREAD_GET, LOG_INTERP, 2,
                            "recreate_app -- not able to fully recreate "
                            "context, pc is in added instruction from mangling\n");
                    }
                }
            }
            if (!just_pc)
                translate_walk_restore(tdcontext, &walk, answer);
            LOG(THREAD_GET, LOG_INTERP, 2,
                "recreate_app -- found ok pc "PFX"\n", answer);
            mc->pc = answer;
            return res;
        }
        /* we only use translation pointers, never just raw bit pointers */
        if (instr_get_translation(inst) != NULL) {
            prev_ok = inst;
            DOLOG(5, LOG_INTERP, loginst(get_thread_private_dcontext(),
                                         5, prev_ok, "\tok instr"););
            prev_bytes = instr_get_translation(inst);
            if (instr_ok_to_mangle(inst)) {
                /* we really want the pc after the translation target since we'll
                 * use this if we pass up the target without hitting it:
                 * unless this is a meta instr in which case we assume the
                 * real instr is ahead (FIXME: there could be cases where
                 * we want the opposite: how know?)
                 */
                /* FIXME: do we need to check for readability first?
                 * in normal usage all translation targets should have been decoded
                 * already while building the bb ilist
                 */
                prev_bytes = decode_next_pc(tdcontext, prev_bytes);
            }
        }

        translate_walk_track(tdcontext, inst, &walk);

        cpc += len;
    }

    /* ERROR! */
    LOG(THREAD_GET, LOG_INTERP, 1,
        "ERROR: recreate_app : looking for "PFX" in frag @ "PFX" "
        "(tag "PFX")\n", target_cache, start_cache, start_app);
    DOLOG(1, LOG_INTERP, {
        instrlist_disassemble(tdcontext, 0, ilist, THREAD_GET);
    });
    ASSERT_NOT_REACHED();
    if (just_pc) {
        /* just guess */
        mc->pc = answer;
    }
    return RECREATE_FAILURE;
}

static instrlist_t *
recreate_selfmod_ilist(dcontext_t *dcontext, fragment_t *f)
{
    cache_pc selfmod_copy;
    instrlist_t *ilist;
    instr_t *inst;
    ASSERT(TEST(FRAG_SELFMOD_SANDBOXED, f->flags));
    /* If f is selfmod, app code may have changed (we see this w/ code
     * on the stack later flushed w/ os_thread_stack_exit(), though in that
     * case we don't expect it to be executed again), so we do a special
     * recreate from the selfmod copy.
     * Since selfmod is straight-line code we can rebuild from cache and
     * offset each translation entry
     */
    selfmod_copy = FRAGMENT_SELFMOD_COPY_PC(f);
    ASSERT(!TEST(FRAG_IS_TRACE, f->flags));
    ASSERT(!TEST(FRAG_HAS_DIRECT_CTI, f->flags));
    /* We must build our ilist w/o calling check_thread_vm_area(), as it will
     * freak out that we are decoding DR memory.
     */
    /* Be sure to "pretend" the bb is for f->tag, b/c selfmod instru is
     * different based on whether pc's are in low 2GB or not.
     */
    ilist = recreate_bb_ilist(dcontext, selfmod_copy, (byte *) f->tag,
                              /* Be sure to limit the size (i#1441) */
                              selfmod_copy + FRAGMENT_SELFMOD_COPY_CODE_SIZE(f),
                              FRAG_SELFMOD_SANDBOXED, NULL, NULL,
                              false/*don't check vm areas!*/, true/*mangle*/, NULL
                              _IF_CLIENT(true/*call client*/)
                              _IF_CLIENT(false/*!for_trace*/));
    ASSERT(ilist != NULL); /* shouldn't fail: our own code is always readable! */
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        app_pc app = instr_get_translation(inst);
        if (app != NULL)
            instr_set_translation(inst, app - selfmod_copy + f->tag);
    }
    return ilist;
}

/* The esp in mcontext must either be valid or NULL (if null will be unable to
 * recreate on XP and 03 at vsyscall_after_syscall and on sygate 2k at after syscall).
 * Returns true if successful.  Whether successful or not, attempts to modify
 * mcontext with recreated state. If just_pc only translates the pc
 * (this is more likely to succeed)
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
/* Also see NOTEs at recreate_app_state() about lock usage, and lack of full stack
 * translation. */
static recreate_success_t
recreate_app_state_internal(dcontext_t *tdcontext, priv_mcontext_t *mcontext,
                            bool just_pc, fragment_t *owning_f, bool restore_memory)
{
    recreate_success_t res = (just_pc ? RECREATE_SUCCESS_PC : RECREATE_SUCCESS_STATE);
#ifdef WINDOWS
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
        mcontext->pc == vsyscall_after_syscall &&
        mcontext->xsp != 0) {
        ASSERT(get_os_version() >= WINDOWS_VERSION_XP);
        /* case 5441 sygate hack means ret addr to after_syscall will be at
         * esp+4 (esp will point to ret in ntdll.dll) for sysenter */
        /* FIXME - should we check that esp is readable? */
        if (is_after_syscall_address(tdcontext, *(cache_pc *)
                                     (mcontext->xsp+
                                      (DYNAMO_OPTION(sygate_sysenter) ? 4 : 0)))) {
            /* no translation needed, ignoring sysenter stack hacks */
            LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                "recreate_app no translation needed (at vsyscall)\n");
            return res;
        } else {
            /* this is a dynamo system call! */
            LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                "recreate_app at dynamo system call\n");
            return RECREATE_FAILURE;
        }
    }
#else
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER &&
        /* Even when the main syscall method is sysenter, we also have a
         * do_int_syscall and do_clone_syscall that use int, so check only
         * the main syscall routine.
         * Note that we don't modify the stack, so once we do sysenter syscalls
         * inlined in the cache (PR 288101) we'll need some mechanism to
         * distinguish those: but for now if a sysenter instruction is used it
         * has to be do_syscall since DR's own syscalls are ints.
         */
        (mcontext->pc == vsyscall_sysenter_return_pc ||
         is_after_main_do_syscall_addr(tdcontext, mcontext->pc) ||
         /* Check for pointing right at sysenter, for i#1145 */
         mcontext->pc + SYSENTER_LENGTH == vsyscall_syscall_end_pc ||
         is_after_main_do_syscall_addr(tdcontext, mcontext->pc + SYSENTER_LENGTH))) {
# ifdef MACOS
        if (!just_pc) {
            LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                "recreate_app: restoring xdx (at sysenter)\n");
            mcontext->xdx = tdcontext->app_xdx;
        }
# else
        LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
            "recreate_app no translation needed (at syscall)\n");
# endif
        return res;
    }
#endif
    else if (is_after_syscall_that_rets(tdcontext, mcontext->pc)
             /* Check for pointing right at sysenter, for i#1145 */
             IF_UNIX(|| is_after_syscall_that_rets(tdcontext,
                                                   mcontext->pc + INT_LENGTH))) {
            /* suspended inside kernel at syscall
             * all registers have app values for syscall */
            LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                "recreate_app pc = after_syscall, translating\n");
#ifdef WINDOWS
            if (DYNAMO_OPTION(sygate_int) &&
                get_syscall_method() == SYSCALL_METHOD_INT) {
                if ((app_pc)mcontext->xsp == NULL)
                    return RECREATE_FAILURE;
                /* dr system calls will have the same after_syscall address when
                 * sygate hack are in effect so need to check top of stack to see
                 * if we are returning to dr or do/share syscall (generated
                 * routines) */
                if (!in_generated_routine(tdcontext, *(app_pc *)mcontext->xsp)) {
                    /* this must be a dynamo system call! */
                    LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                        "recreate_app at dynamo system call\n");
                    return RECREATE_FAILURE;
                }
                ASSERT(*(app_pc *)mcontext->xsp ==
                       after_do_syscall_code(tdcontext) ||
                       *(app_pc *)mcontext->xsp ==
                       after_shared_syscall_code(tdcontext));
                if (!just_pc) {
                    /* This is an int system call and since for sygate
                     * compatibility we redirect those with a call to an ntdll.dll
                     * int 2e ret 0 we need to pop the stack once to match app. */
                    mcontext->xsp += XSP_SZ; /* pop the stack */
                }
            }
#else
            if (is_after_syscall_that_rets(tdcontext, mcontext->pc + INT_LENGTH)) {
                /* i#1145: preserve syscall re-start point */
                mcontext->pc = POST_SYSCALL_PC(tdcontext) - INT_LENGTH;
            } else
#endif
                mcontext->pc = POST_SYSCALL_PC(tdcontext);
            return res;
    } else if (mcontext->pc == get_reset_exit_stub(tdcontext)) {
        LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
            "recreate_app at reset exit stub => using next_tag "PFX"\n",
            tdcontext->next_tag);
        /* context is completely native except the pc */
        mcontext->pc = tdcontext->next_tag;
        return res;
    } else if (in_generated_routine(tdcontext, mcontext->pc)) {
        LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
            "recreate_app state at untranslatable address in "
            "generated routines for thread "TIDFMT"\n", tdcontext->owning_thread);
        return RECREATE_FAILURE;
    } else if (in_fcache(mcontext->pc)) {
        /* FIXME: what if pc is in separate direct stub???
         * do we have to read the &l from the stub to find linkstub_t and thus
         * fragment_t owner?
         */
        /* NOTE - only at this point is it safe to grab locks other then the
         * fcache_unit_areas.lock */
        linkstub_t *l;
        cache_pc cti_pc;
        instrlist_t *ilist = NULL;
        fragment_t *f = owning_f;
        bool alloc = false;
        IF_X64(bool old_mode;)
#ifdef CLIENT_INTERFACE
        dr_restore_state_info_t client_info;
        dr_mcontext_t xl8_mcontext;
        dr_mcontext_t raw_mcontext;
        dr_mcontext_init(&xl8_mcontext);
        dr_mcontext_init(&raw_mcontext);
#endif

        /* Rather than storing a mapping table, we re-build the fragment
         * containing the code cache pc whenever we can.  For pending-deletion
         * fragments we can't do that and have to store the info, due to our
         * weak consistency flushing where the app code may have changed
         * before we get here (case 3559).
         */

        /* Check whether we have a fragment w/ stored translations before
         * asking to recreate the ilist
         */
        if (f == NULL)
            f = fragment_pclookup_with_linkstubs(tdcontext, mcontext->pc, &alloc);

        /* If the passed-in fragment is fake, we need to get the linkstubs */
        if (f != NULL && TEST(FRAG_FAKE, f->flags)) {
            ASSERT(TEST(FRAG_COARSE_GRAIN, f->flags));
            f = fragment_recreate_with_linkstubs(tdcontext, f);
            alloc = true;
        }

        /* Whether a bb or trace, this routine will recreate the entire ilist. */
        if (f == NULL) {
            ilist = recreate_fragment_ilist(tdcontext, mcontext->pc, &f, &alloc,
                                            true/*mangle*/ _IF_CLIENT(true/*client*/));
        } else if (FRAGMENT_TRANSLATION_INFO(f) == NULL) {
            if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
                ilist = recreate_selfmod_ilist(tdcontext, f);
            } else {
                /* NULL for pc indicates that f is valid */
                bool new_alloc;
                DEBUG_DECLARE(fragment_t *pre_f = f;)
                ilist = recreate_fragment_ilist(tdcontext, NULL, &f, &new_alloc,
                                                true/*mangle*/ _IF_CLIENT(true/*client*/));
                ASSERT(owning_f == NULL || f == owning_f ||
                       (TEST(FRAG_COARSE_GRAIN, owning_f->flags) && f == pre_f));
                ASSERT(!new_alloc);
            }
        }
        if (ilist == NULL && (f == NULL || FRAGMENT_TRANSLATION_INFO(f) == NULL)) {
            /* It is problematic if this routine fails.  Many places assume that
             * recreate_app_pc() will work.
             */
            ASSERT(!INTERNAL_OPTION(safe_translate_flushed));
            res = RECREATE_FAILURE;
            goto recreate_app_state_done;
        }

        LOG(THREAD_GET, LOG_INTERP, 2,
            "recreate_app : pc is in F%d("PFX")%s\n", f->id, f->tag,
            ((f->flags & FRAG_IS_TRACE) != 0)?" (trace)":"");

        DOLOG(2, LOG_SYNCH, {
            if (ilist != NULL) {
                LOG(THREAD_GET, LOG_SYNCH, 2, "ilist for recreation:\n");
                instrlist_disassemble(tdcontext, f->tag, ilist, THREAD_GET);
            }
        });

        /* if pc is in an exit stub, we find the corresponding exit instr */
        cti_pc = NULL;
        for (l = FRAGMENT_EXIT_STUBS(f); l; l = LINKSTUB_NEXT_EXIT(l)) {
            if (EXIT_HAS_LOCAL_STUB(l->flags, f->flags)) {
                /* FIXME: as computing the stub pc becomes more expensive,
                 * should perhaps check fragment_body_end_pc() or something
                 * that only does one stub check up front, and then find the
                 * exact stub if pc is beyond the end of the body.
                 */
                if (mcontext->pc < EXIT_STUB_PC(tdcontext, f, l))
                    break;
                cti_pc = EXIT_CTI_PC(f, l);
            }
        }
        if (cti_pc != NULL) {
            /* target is inside an exit stub!
             * new target: the exit cti, not its stub
             */
            if (!just_pc) {
                /* FIXME : translate from exit stub */
                LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                    "recreate_app_helper -- can't full recreate state, pc "PFX" "
                    "is in exit stub\n", mcontext->pc);
                res = RECREATE_SUCCESS_PC; /* failed on full state, but pc good */
                goto recreate_app_state_done;
            }
            LOG(THREAD_GET, LOG_INTERP|LOG_SYNCH, 2,
                "\ttarget "PFX" is inside an exit stub, looking for its cti "
                " "PFX"\n", mcontext->pc, cti_pc);
            mcontext->pc = cti_pc;
        }

        /* Recreate in same mode as original fragment */
        IF_X64(old_mode = set_x86_mode(tdcontext, FRAG_IS_32(f->flags) ||
                                                  FRAG_IS_X86_TO_X64(f->flags)));

        /* now recreate the state */
#ifdef CLIENT_INTERFACE
        /* keep a copy of the pre-translation state */
        priv_mcontext_to_dr_mcontext(&raw_mcontext, mcontext);
        client_info.raw_mcontext = &raw_mcontext;
        client_info.raw_mcontext_valid = true;
#endif
        if (ilist == NULL) {
            ASSERT(f != NULL && FRAGMENT_TRANSLATION_INFO(f) != NULL);
            ASSERT(!TEST(FRAG_WAS_DELETED, f->flags) ||
                   INTERNAL_OPTION(safe_translate_flushed));
            res = recreate_app_state_from_info(tdcontext, FRAGMENT_TRANSLATION_INFO(f),
                                               (byte *) f->start_pc,
                                               (byte *) f->start_pc + f->size,
                                               mcontext, just_pc _IF_DEBUG(f->flags));
            STATS_INC(recreate_via_stored_info);
        } else {
            res = recreate_app_state_from_ilist(tdcontext, ilist, (byte *) f->tag,
                                                (byte *) FCACHE_ENTRY_PC(f),
                                                (byte *) f->start_pc + f->size,
                                                mcontext, just_pc, f->flags);
            STATS_INC(recreate_via_app_ilist);
        }
        IF_X64(set_x86_mode(tdcontext, old_mode));

#ifdef STEAL_REGISTER
        /* FIXME: conflicts w/ PR 263407 reg spill tracking */
        ASSERT_NOT_IMPLEMENTED(false && "conflicts w/ reg spill tracking");
        if (!just_pc) {
            /* get app's value of edi */
            mc->xdi = get_mcontext(tdcontext)->xdi;
        }
#endif
#ifdef CLIENT_INTERFACE
        if (res != RECREATE_FAILURE) {
            /* PR 214962: if the client has a restore callback, invoke it to
             * fix up the state (and pc).
             */
            priv_mcontext_to_dr_mcontext(&xl8_mcontext, mcontext);
            client_info.mcontext = &xl8_mcontext;
            client_info.fragment_info.tag = (void *) f->tag;
            client_info.fragment_info.cache_start_pc = FCACHE_ENTRY_PC(f);
            client_info.fragment_info.is_trace = TEST(FRAG_IS_TRACE, f->flags);
            client_info.fragment_info.app_code_consistent =
                !TESTANY(FRAG_WAS_DELETED|FRAG_SELFMOD_SANDBOXED, f->flags);
            /* i#220/PR 480565: client has option of failing the translation */
            if (!instrument_restore_state(tdcontext, restore_memory, &client_info))
                res = RECREATE_FAILURE;
            dr_mcontext_to_priv_mcontext(mcontext, &xl8_mcontext);
        }
#endif

    recreate_app_state_done:
        /* free the instrlist_t elements */
        if (ilist != NULL)
            instrlist_clear_and_destroy(tdcontext, ilist);
        if (alloc) {
            ASSERT(f != NULL);
            fragment_free(tdcontext, f);
        }
        return res;
    } else {
        /* handle any other cases, in DR etc. */
        return RECREATE_FAILURE;
    }
}

/* Assumes that pc is a pc_recreatable place (i.e. in_fcache(), though could do
 * syscalls with esp, also see FIXME about separate stubs in
 * recreate_app_state_internal()), ASSERTs otherwise.
 * If caller knows which fragment pc belongs to, caller should pass in f
 * to avoid work and avoid lock rank issues as pclookup acquires shared_cache_lock;
 * else, pass in NULL for f.
 * NOTE - If called by a thread other than the tdcontext owner, caller must
 * ensure tdcontext remains valid.  Caller also must ensure that it is safe to
 * allocate memory from tdcontext (for instr routines), i.e. caller owns
 * tdcontext or the owner of tdcontext is suspended.  Also if tdcontext is
 * !couldbelinking then caller must own the thread_initexit_lock in case
 * recreate_fragment_ilist() is called.
 * NOTE - If this function is unable to translate the pc, but the pc is
 * in_fcache() then there is an assert curiosity and the function returns NULL.
 * This can happen only from the pc being in a fragment that is pending
 * deletion (ref case 3559 others).  Most callers don't check the returned
 * value and wouldn't have a way to recover even if they did. FIXME
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
app_pc
recreate_app_pc(dcontext_t *tdcontext, cache_pc pc, fragment_t *f)
{
    priv_mcontext_t mc;
    recreate_success_t res;

#if defined(CLIENT_INTERFACE) && defined(WINDOWS)
    bool swap_peb = false;
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer() &&
        dr_using_app_state(tdcontext)) {
        swap_peb_pointer(tdcontext, true/*to priv*/);
        swap_peb = true;
    }
#endif
    LOG(THREAD_GET, LOG_INTERP, 2,
        "recreate_app_pc -- translating from pc="PFX"\n", pc);

    memset(&mc, 0, sizeof(mc)); /* ensures esp is NULL */
    mc.pc = pc;

    res = recreate_app_state_internal(tdcontext, &mc, true, f, false);
    if (res != RECREATE_SUCCESS_PC) {
        ASSERT(res != RECREATE_SUCCESS_STATE); /* shouldn't return that for just_pc */
        ASSERT(in_fcache(pc)); /* Make sure caller didn't screw up */
        /* We were unable to translate the pc, most likely because the
         * pc is in a fragment that is pending deletion FIXME, most callers
         * aren't able to recover! */
        ASSERT_CURIOSITY(res && "Unable to translate pc");
        mc.pc = NULL;
    }

    LOG(THREAD_GET, LOG_INTERP, 2,
        "recreate_app_pc -- translation is "PFX"\n", mc.pc);

#if defined(CLIENT_INTERFACE) && defined(WINDOWS)
    if (swap_peb)
        swap_peb_pointer(tdcontext, false/*to app*/);
#endif
    return mc.pc;
}

/* Translates the code cache state in mcontext into what it would look like
 * in the original application.
 * If it fails altogether, returns RECREATE_FAILURE, but still provides
 * a best-effort translation.
 * If it fails to restore the full machine state, but does restore the pc,
 * returns RECREATE_SUCCESS_PC.
 * If it successfully restores the full machine state,
 * returns RECREATE_SUCCESS_STATE.  Only for full success does it
 * consider the restore_memory parameter, which, if true, requests restoration
 * of any memory values that were shifted (primarily due to clients) (otherwise,
 * only the passed-in mcontext is modified). If restore_memory is
 * true, the caller should always relocate the translated thread, as
 * it may not execute properly if left at its current location (it
 * could be in the middle of client code in the cache).
 *
 * If caller knows which fragment pc belongs to, caller should pass in f
 * to avoid work and avoid lock rank issues as pclookup acquires shared_cache_lock;
 * else, pass in NULL for f.
 *
 * FIXME: does not undo stack mangling for sysenter
 */
/* NOTE - Can be called with a thread suspended at an arbitrary place by synch
 * routines so must not call mutex_lock (or call a function that does) unless
 * the synch routines have checked that lock.  Currently only fcache_unit_areas.lock
 * is used (for in_fcache in recreate_app_state_internal())
 * (if in_fcache succeeds then assumes other locks won't be a problem).
 * NOTE - If called by a thread other than the tdcontext owner, caller must
 * ensure tdcontext remains valid.  Caller also must ensure that it is safe to
 * allocate memory from tdcontext (for instr routines), i.e. caller owns
 * tdcontext or the owner of tdcontext is suspended.  Also if tdcontext is
 * !couldbelinking then caller must own the thread_initexit_lock in case
 * recreate_fragment_ilist() is called.  We assume that when tdcontext is
 * not the calling thread, this is a thread synch request, and is NOT from
 * an app fault (PR 267260)!
 */
/* Use THREAD_GET instead of THREAD so log messages go to calling thread */
recreate_success_t
recreate_app_state(dcontext_t *tdcontext, priv_mcontext_t *mcontext,
                   bool restore_memory, fragment_t *f)
{
    recreate_success_t res;
#if defined(CLIENT_INTERFACE) && defined(WINDOWS)
    bool swap_peb = false;
    if (INTERNAL_OPTION(private_peb) && should_swap_peb_pointer() &&
        dr_using_app_state(tdcontext)) {
        swap_peb_pointer(tdcontext, true/*to priv*/);
        swap_peb = true;
    }
#endif
#ifdef DEBUG
    if (stats->loglevel >= 2 && (stats->logmask & LOG_SYNCH) != 0) {
        LOG(THREAD_GET, LOG_SYNCH, 2,
            "recreate_app_state -- translating from:\n");
        dump_mcontext(mcontext, THREAD_GET, DUMP_NOT_XML);
    }
#endif

    res = recreate_app_state_internal(tdcontext, mcontext, false, f, restore_memory);

#ifdef DEBUG
    if (res) {
        if (stats->loglevel >= 2 && (stats->logmask & LOG_SYNCH) != 0) {
            LOG(THREAD_GET, LOG_SYNCH, 2,
                "recreate_app_state -- translation is:\n");
            dump_mcontext(mcontext, THREAD_GET, DUMP_NOT_XML);
        }
    } else {
        LOG(THREAD_GET, LOG_SYNCH, 2,
            "recreate_app_state -- unable to translate\n");
    }
#endif

#if defined(CLIENT_INTERFACE) && defined(WINDOWS)
    if (swap_peb)
        swap_peb_pointer(tdcontext, false/*to app*/);
#endif
    return res;
}

static inline uint
translation_info_alloc_size(uint num_entries)
{
    return (sizeof(translation_info_t) + sizeof(translation_entry_t)*num_entries);
}

/* we save space by inlining the array with the struct holding the length */
static translation_info_t *
translation_info_alloc(dcontext_t *dcontext, uint num_entries)
{
    /* we need to use global heap since pending-delete fragments become
     * shared entities
     */
    translation_info_t *info =
        global_heap_alloc(translation_info_alloc_size(num_entries) HEAPACCT(ACCT_OTHER));
    info->num_entries = num_entries;
    return info;
}

void
translation_info_free(dcontext_t *dcontext, translation_info_t *info)
{
    global_heap_free(info, translation_info_alloc_size(info->num_entries)
                     HEAPACCT(ACCT_OTHER));
}

static inline void
set_translation(dcontext_t *dcontext, translation_entry_t **entries,
                uint *num_entries, uint entry,
                ushort cache_offs, app_pc app, bool identical, bool our_mangling)
{
    if (entry >= *num_entries) {
        /* alloc new arrays 2x as big */
        *entries = global_heap_realloc(*entries, *num_entries,
                                       *num_entries*2, sizeof(translation_entry_t)
                                       HEAPACCT(ACCT_OTHER));
        *num_entries *= 2;
    }
    ASSERT(entry < *num_entries);
    (*entries)[entry].cache_offs = cache_offs;
    (*entries)[entry].app = app;
    (*entries)[entry].flags = 0;
    if (identical)
        (*entries)[entry].flags |= TRANSLATE_IDENTICAL;
    if (our_mangling)
        (*entries)[entry].flags |= TRANSLATE_OUR_MANGLING;
    LOG(THREAD, LOG_FRAGMENT, 4, "\tset_translation: %d +%5d => "PFX" %s%s\n",
        entry, cache_offs, app, identical ? "identical" : "contiguous",
        our_mangling ? " ours" : "");
}

void
translation_info_print(const translation_info_t *info, cache_pc start, file_t file)
{
    uint i;
    ASSERT(info != NULL);
    ASSERT(file != INVALID_FILE);
    print_file(file, "translation info "PFX"\n", info);
    for (i=0; i<info->num_entries; i++) {
        print_file(file, "\t%d +%5d == "PFX" => "PFX" %s%s\n",
                   i, info->translation[i].cache_offs,
                   start + info->translation[i].cache_offs,
                   info->translation[i].app,
                   TEST(TRANSLATE_IDENTICAL, info->translation[i].flags) ?
                   "identical" : "contiguous",
                   TEST(TRANSLATE_OUR_MANGLING, info->translation[i].flags) ?
                   " ours" : "");
    }
}

/* With our weak flushing consistency we must store translation info
 * for any fragment that may outlive its original app code (case
 * 3559).  Here we store actual translation info.  An alternative is
 * to store elided jmp information and a copy of the source memory,
 * but that takes more memory for all but the smallest fragments.  A
 * better alternative is to reliably de-mangle, which would require
 * only elided jmp information.
 */
translation_info_t *
record_translation_info(dcontext_t *dcontext, fragment_t *f, instrlist_t *existing_ilist)
{
    translation_entry_t *entries;
    uint num_entries;
    translation_info_t *info = NULL;
    instrlist_t *ilist;
    instr_t *inst;
    uint i;
    uint last_len = 0;
    bool last_contig;
    app_pc last_translation = NULL;
    cache_pc cpc;

    LOG(THREAD, LOG_FRAGMENT, 3, "record_translation_info: F%d("PFX")."PFX"\n",
        f->id, f->tag, f->start_pc);

    if (existing_ilist != NULL)
        ilist = existing_ilist;
    else if (TEST(FRAG_SELFMOD_SANDBOXED, f->flags)) {
        ilist = recreate_selfmod_ilist(dcontext, f);
    } else {
        /* Must re-build fragment and record translation info for each instr.
         * Whether a bb or trace, this routine will recreate the entire ilist.
         */
        ilist = recreate_fragment_ilist(dcontext, NULL, &f, NULL,
                                        true/*mangle*/ _IF_CLIENT(true/*client*/));
    }
    ASSERT(ilist != NULL);
    DOLOG(3, LOG_FRAGMENT, {
        LOG(THREAD, LOG_FRAGMENT, 3, "ilist for recreation:\n");
        instrlist_disassemble(dcontext, f->tag, ilist, THREAD);
    });

    /* To avoid two passes we do one pass and store into a large-enough array.
     * We then copy the results into a just-right-sized array.  A typical bb
     * requires 2 entries, one for its body of straight-line code and one for
     * the inserted jmp at the end, so we start w/ that to avoid copying in
     * the common case.  FIXME: optimization: instead of every bb requiring a
     * final entry for the inserted jmp, have recreate_ know about it and cut
     * in half the typical storage reqts.
     */
#   define NUM_INITIAL_TRANSLATIONS 2
    num_entries = NUM_INITIAL_TRANSLATIONS;
    entries = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, translation_entry_t,
                               NUM_INITIAL_TRANSLATIONS, ACCT_OTHER, PROTECTED);

    i = 0;
    cpc = (byte *) FCACHE_ENTRY_PC(f);
    if (fragment_prefix_size(f->flags) > 0) {
        ASSERT(f->start_pc < cpc);
        set_translation(dcontext, &entries, &num_entries, i, 0,
                        f->tag, true/*identical*/, true/*our mangling*/);
        last_translation = f->tag;
        last_contig = false;
        i++;
    } else {
        ASSERT(f->start_pc == cpc);
        last_contig = true; /* we create 1st entry on 1st loop iter */
    }
    for (inst = instrlist_first(ilist); inst; inst = instr_get_next(inst)) {
        app_pc app = instr_get_translation(inst);
        uint prev_i = i;
#ifndef CLIENT_INTERFACE
# ifdef INTERNAL
        ASSERT(app != NULL || DYNAMO_OPTION(optimize));
# else
        ASSERT(app != NULL);
# endif
#endif
        /* Should only be NULL for meta-code added by a client.
         * We preserve the NULL so our translation routines know to not
         * let this be a thread relocation point
         */
        /* i#739, skip label instr */
        if (instr_is_label(inst))
            continue;
        /* PR 302951: clean call args are instr_is_our_mangling so no assert for that */
        ASSERT(app != NULL || !instr_ok_to_mangle(inst));
        /* see whether we need a new entry, or the current stride (contig
         * or identical) holds
         */
        if (last_contig) {
            if ((i == 0 && (app == NULL || instr_is_our_mangling(inst)))
                || app == last_translation) {
                /* we are now in an identical region */
                /* Our incremental discovery can cause us to add a new
                 * entry of one type that on the next instr we discover
                 * can optimally be recorded as the other type.  Here we
                 * hit an app pc shift whose target needs an identical
                 * entry: so rather than a contig followed by identical,
                 * we can get away with a single identical.
                 * Example: "x x+1 y y", where we use an identical for the
                 * first y instead of the contig that we initially guessed
                 * at b/c we assumed it was an elision.
                 */
                if (i > 0 && entries[i-1].cache_offs == cpc - last_len - f->start_pc) {
                    /* convert prev contig into identical */
                    ASSERT(!TEST(TRANSLATE_IDENTICAL, entries[i-1].flags));
                    entries[i-1].flags |= TRANSLATE_IDENTICAL;
                    LOG(THREAD, LOG_FRAGMENT, 3,
                        "\tchanging %d to identical\n", i-1);
                } else {
                    set_translation(dcontext, &entries, &num_entries, i,
                                    (ushort) (cpc - f->start_pc),
                                    app, true/*identical*/, instr_is_our_mangling(inst));
                    i++;
                }
                last_contig = false;
            } else if ((i == 0 && app != NULL && !instr_is_our_mangling(inst))
                       || app != last_translation + last_len) {
                /* either 1st loop iter w/ app instr & no prefix, or else probably
                 * a follow-ubr, so create a new contig entry
                 */
                set_translation(dcontext, &entries, &num_entries, i,
                                (ushort) (cpc - f->start_pc),
                                app, false/*contig*/, instr_is_our_mangling(inst));
                last_contig = true;
                i++;
            } /* else, contig continues */
        } else {
            if (app != last_translation) {
                /* no longer in an identical region */
                ASSERT(i > 0);
                /* If we have translations "x x+1 x+1 x+2 x+3" we can more
                 * efficiently encode with a new contig entry at the 2nd
                 * x+1 rather than an identical entry there followed by a
                 * contig entry for x+2.
                 */
                if (app == last_translation + last_len &&
                    entries[i-1].cache_offs == cpc - last_len - f->start_pc) {
                    /* convert prev identical into contig */
                    ASSERT(TEST(TRANSLATE_IDENTICAL, entries[i-1].flags));
                    entries[i-1].flags &= ~TRANSLATE_IDENTICAL;
                    LOG(THREAD, LOG_FRAGMENT, 3,
                        "\tchanging %d to contig\n", i-1);
                } else {
                    /* probably a follow-ubr, so create a new contig entry */
                    set_translation(dcontext, &entries, &num_entries, i,
                                    (ushort) (cpc - f->start_pc), app, false/*contig*/,
                                    instr_is_our_mangling(inst));
                    last_contig = true;
                    i++;
                }
            }
        }
        last_translation = app;

        /* We need to make a new entry if the our-mangling flag changed */
        if (i > 0 && i == prev_i && instr_is_our_mangling(inst) !=
            TEST(TRANSLATE_OUR_MANGLING, entries[i-1].flags)) {
            /* our manglings are usually identical */
            bool identical = instr_is_our_mangling(inst);
            set_translation(dcontext, &entries, &num_entries, i,
                            (ushort) (cpc - f->start_pc),
                            app, identical, instr_is_our_mangling(inst));
            last_contig = !identical;
            i++;
        }
        last_len = instr_length(dcontext, inst);
        cpc += last_len;
        ASSERT(CHECK_TRUNCATE_TYPE_ushort(cpc - f->start_pc));
    }
    /* exit stubs can be examined after app code is gone, so we don't need
     * to store any info on them here
     */

    /* free the instrlist_t elements */
    if (existing_ilist == NULL)
        instrlist_clear_and_destroy(dcontext, ilist);

    /* now copy into right-sized array */
    info = translation_info_alloc(dcontext, i);
    memcpy(info->translation, entries, sizeof(translation_entry_t) * i);
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, entries, translation_entry_t,
                    num_entries, ACCT_OTHER, PROTECTED);

    STATS_INC(translations_computed);

    DOLOG(3, LOG_INTERP, {
        translation_info_print(info, f->start_pc, THREAD);
    });

    return info;
}

#ifdef INTERNAL
void
stress_test_recreate_state(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist)
{
    priv_mcontext_t mc;
    bool res;
    cache_pc cpc;
    instr_t *in;
    static const reg_t STRESS_XSP_INIT = 0x08000000; /* arbitrary */
    bool success_so_far = true;
    bool inside_mangle_region = false;
    bool spill_xcx_outstanding = false;
    reg_id_t reg;
    bool spill;
    int xsp_adjust = 0;
    app_pc mangle_translation = f->tag;

    LOG(THREAD, LOG_INTERP, 3, "Testing restoring state fragment #%d\n",
        GLOBAL_STAT(num_fragments));

    if (TEST(FRAG_IS_TRACE, f->flags)) {
        /* decode_fragment() does not set the our-mangling bits, nor the
         * translation fields (to distinguish back-to-back mangling
         * regions): not ideal to test using part of what we're testing but
         * better than nothing
         */
        ilist = recreate_fragment_ilist(dcontext, NULL, &f, NULL, true/*mangle*/
                                        _IF_CLIENT(true/*call client*/));
    }

    cpc = FCACHE_ENTRY_PC(f);
    for (in = instrlist_first(ilist); in != NULL;
         cpc += instr_length(dcontext, in), in = instr_get_next(in)) {
        /* PR 267260: we're only testing mangling regions.
         * FIXME: also verify rip-relative mangling translation
         */
        if (inside_mangle_region &&
            (!instr_is_our_mangling(in) ||
             /* handle adjacent mangle regions */
             (TEST(FRAG_IS_TRACE, f->flags) /* we have translation only for traces */ &&
              mangle_translation != instr_get_translation(in)))) {
            /* reset */
            LOG(THREAD, LOG_INTERP, 3, "  out of mangling region\n");
            inside_mangle_region = false;
            xsp_adjust = 0;
            success_so_far = true;
            spill_xcx_outstanding = false;
            /* go ahead and fall through and ensure we succeed w/ 0 xsp adjust */
        }
        if (instr_is_our_mangling(in)) {
            if (!inside_mangle_region) {
                inside_mangle_region = true;
                LOG(THREAD, LOG_INTERP, 3, "  entering mangling region\n");
                mangle_translation = instr_get_translation(in);
            } else {
                ASSERT(!TEST(FRAG_IS_TRACE, f->flags) ||
                       mangle_translation == instr_get_translation(in));
            }

            mc.xcx = (reg_t) get_tls(os_tls_offset
                                     ((ushort)reg_spill_tls_offs(REG_XCX))) + 1;
            mc.xsp = STRESS_XSP_INIT;
            mc.pc = cpc;
            LOG(THREAD, LOG_INTERP, 3,
                "  restoring cpc="PFX", xsp="PFX"\n", mc.pc, mc.xsp);
            res = recreate_app_state(dcontext, &mc, false/*just registers*/, NULL);
            LOG(THREAD, LOG_INTERP, 3,
                "  restored res=%d pc="PFX", xsp="PFX" vs "PFX", xcx="PFX" vs "PFX"\n",
                res, mc.pc, mc.xsp, STRESS_XSP_INIT -/*negate*/ xsp_adjust,
                mc.xcx, get_tls(os_tls_offset((ushort)reg_spill_tls_offs(REG_XCX))));
            /* We should only have failures at tail end of mangle regions.
             * No instrs after a failing instr should touch app memory.
             */
            ASSERT(success_so_far /* ok to fail */ ||
                   (!res &&
                    (instr_is_reg_spill_or_restore(dcontext, in, NULL, NULL, NULL) ||
                     (!instr_reads_memory(in) && !instr_writes_memory(in)))));

            /* check that xsp and xcx are adjusted properly */
            ASSERT(mc.xsp == STRESS_XSP_INIT -/*negate*/ xsp_adjust);
            ASSERT(!spill_xcx_outstanding ||
                   mc.xcx == (reg_t)
                   get_tls(os_tls_offset((ushort)reg_spill_tls_offs(REG_XCX))));

            if (success_so_far && !res)
                success_so_far = false;
            instr_check_xsp_mangling(dcontext, in, &xsp_adjust);
            if (xsp_adjust != 0)
                LOG(THREAD, LOG_INTERP, 3, "  xsp_adjust=%d\n", xsp_adjust);
            if (instr_is_reg_spill_or_restore(dcontext, in, NULL, &spill, &reg) &&
                reg == REG_XCX)
                spill_xcx_outstanding = spill;
        }
    }
    if (TEST(FRAG_IS_TRACE, f->flags)) {
        instrlist_clear_and_destroy(dcontext, ilist);
    }
}
#endif /* INTERNAL */

/* END TRANSLATION CODE ***********************************************************/


/* For 32-bit linux apps on 64-bit kernels we assume that all syscalls that
 * we use this for are ok w/ int (i.e., we don't need a sys{call,enter} version).
 */
byte *
get_global_do_syscall_entry()
{
    int method = get_syscall_method();
    if (method == SYSCALL_METHOD_INT) {
#ifdef WINDOWS
        if (DYNAMO_OPTION(sygate_int))
            return (byte *)global_do_syscall_sygate_int;
        else
#endif
            return (byte *)global_do_syscall_int;
    } else if (method == SYSCALL_METHOD_SYSENTER) {
#ifdef WINDOWS
        if (DYNAMO_OPTION(sygate_sysenter))
            return (byte *)global_do_syscall_sygate_sysenter;
        else
            return (byte *)global_do_syscall_sysenter;
#else
        return (byte *)global_do_syscall_int;
#endif
    }
#ifdef WINDOWS
    else if (method == SYSCALL_METHOD_WOW64)
        return (byte *)global_do_syscall_wow64;
#endif
    else if (method == SYSCALL_METHOD_SYSCALL) {
#ifdef X64
        return (byte *)global_do_syscall_syscall;
#else
# ifdef WINDOWS
        ASSERT_NOT_IMPLEMENTED(false && "PR 205898: 32-bit syscall on Windows NYI");
# else
        return (byte *)global_do_syscall_int;
# endif
#endif
    } else {
#ifdef UNIX
        /* PR 205310: we sometimes have to execute syscalls before we
         * see an app syscall: for a signal default action, e.g.
         */
        return (byte *)IF_X64_ELSE(global_do_syscall_syscall,global_do_syscall_int);
#else
        ASSERT_NOT_REACHED();
#endif
    }
    return NULL;
}

/* used only by cleanup_and_terminate to avoid the sysenter
 * sygate hack version */
byte *
get_cleanup_and_terminate_global_do_syscall_entry()
{
    /* see note above: for 32-bit linux apps we use int.
     * xref PR 332427 as well where sysenter causes a crash
     * if called from cleanup_and_terminate() where ebp is
     * left pointing to the old freed stack.
     */
#if defined(WINDOWS) || defined(X64)
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER)
        return (byte *)global_do_syscall_sysenter;
    else
#endif
#ifdef WINDOWS
    if (get_syscall_method() == SYSCALL_METHOD_WOW64 &&
        syscall_uses_wow64_index())
        return (byte *)global_do_syscall_wow64_index0;
    else
#endif
        return get_global_do_syscall_entry();
}

#ifdef MACOS
/* There is no single resumption point from sysenter: each sysenter stores
 * the caller's retaddr in edx.  Thus, there is nothing to hook.
 */
bool
unhook_vsyscall(void)
{
    return false;
}
#elif defined(LINUX)
/* PR 212570: for sysenter support we need to regain control after the
 * kernel sets eip to a hardcoded user-mode address on the vsyscall page.
 * The vsyscall code layout is as follows:
 *     0xffffe400 <__kernel_vsyscall+0>:       push   %ecx
 *     0xffffe401 <__kernel_vsyscall+1>:       push   %edx
 *     0xffffe402 <__kernel_vsyscall+2>:       push   %ebp
 *     0xffffe403 <__kernel_vsyscall+3>:       mov    %esp,%ebp
 *     0xffffe405 <__kernel_vsyscall+5>:       sysenter
 *   nops for alignment of return point:
 *     0xffffe407 <__kernel_vsyscall+7>:       nop
 *     0xffffe408 <__kernel_vsyscall+8>:       nop
 *     0xffffe409 <__kernel_vsyscall+9>:       nop
 *     0xffffe40a <__kernel_vsyscall+10>:      nop
 *     0xffffe40b <__kernel_vsyscall+11>:      nop
 *     0xffffe40c <__kernel_vsyscall+12>:      nop
 *     0xffffe40d <__kernel_vsyscall+13>:      nop
 *   system call restart point:
 *     0xffffe40e <__kernel_vsyscall+14>:      jmp    0xffffe403 <__kernel_vsyscall+3>
 *   system call normal return point:
 *     0xffffe410 <__kernel_vsyscall+16>:      pop    %ebp
 *     0xffffe411 <__kernel_vsyscall+17>:      pop    %edx
 *     0xffffe412 <__kernel_vsyscall+18>:      pop    %ecx
 *     0xffffe413 <__kernel_vsyscall+19>:      ret
 *
 * For randomized vsyscall page locations we can mark the page +w and
 * write to it.  For now, for simplicity, we focus only on that case;
 * for vsyscall page at un-reachable 0xffffe000 we bail out and use
 * ints for now (perf hit but works).  PR 288330 covers leaving
 * as sysenters.
 *
 * There are either nops or garbage after the ret, so we clobber one
 * byte past the ret to put in a rel32 jmp (an alternative is to do
 * rel8 jmp into the nop area and have a rel32 jmp there).  We
 * cleverly copy the 4 bytes of displaced code into the nop area, so
 * that 1) we don't have to allocate any memory and 2) we don't have
 * to do any extra work in dispatch, which will naturally go to the
 * post-system-call-instr pc.
 *
 * Using a hook is much simpler than clobbering the retaddr, which is what
 * Windows does and then has to spend a lot of effort juggling transparency
 * and control on asynch in/out events.
 */

#define VSYS_DISPLACED_LEN 4

static bool
hook_vsyscall(dcontext_t *dcontext)
{
    bool res = true;
    instr_t instr;
    byte *pc;
    uint num_nops = 0;
    uint prot;

    ASSERT(DATASEC_WRITABLE(DATASEC_RARELY_PROT));
    IF_X64(ASSERT_NOT_REACHED()); /* no sysenter support on x64 */
    ASSERT(vsyscall_page_start != NULL && vsyscall_syscall_end_pc != NULL);

    instr_init(dcontext, &instr);
    pc = vsyscall_syscall_end_pc;
    do {
        instr_reset(dcontext, &instr);
        pc = decode(dcontext, pc, &instr);
        if (instr_is_nop(&instr))
            num_nops++;
    } while (instr_is_nop(&instr));
    vsyscall_sysenter_return_pc = pc;
    ASSERT(instr_get_opcode(&instr) == OP_jmp_short ||
           instr_get_opcode(&instr) == OP_int /*ubuntu 11.10: i#647*/);

    /* We fail if the pattern looks different */
#define CHECK(x) do {                                 \
    if (!(x)) {                                       \
        ASSERT(false && "vsyscall pattern mismatch"); \
        res = false;                                  \
        goto hook_vsyscall_return;                    \
    }                                                 \
} while (0);

    CHECK(num_nops >= VSYS_DISPLACED_LEN);

    /* Only now that we've set vsyscall_sysenter_return_pc do we check writability */
    if (!DYNAMO_OPTION(hook_vsyscall)) {
        res = false;
        goto hook_vsyscall_return;
    }
    get_memory_info(vsyscall_page_start, NULL, NULL, &prot);
    if (!TEST(MEMPROT_WRITE, prot)) {
        res = set_protection(vsyscall_page_start, PAGE_SIZE, prot|MEMPROT_WRITE);
        if (!res)
            goto hook_vsyscall_return;
    }

    LOG(GLOBAL, LOG_SYSCALLS|LOG_VMAREAS, 1,
        "Hooking vsyscall page @ "PFX"\n", vsyscall_sysenter_return_pc);

    /* The 5 bytes we'll clobber: */
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, pc, &instr);
    CHECK(instr_get_opcode(&instr) == OP_pop);
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, pc, &instr);
    CHECK(instr_get_opcode(&instr) == OP_pop);
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, pc, &instr);
    CHECK(instr_get_opcode(&instr) == OP_pop);
    instr_reset(dcontext, &instr);
    pc = decode(dcontext, pc, &instr);
    CHECK(instr_get_opcode(&instr) == OP_ret);
    /* Sometimes the next byte is a nop, sometimes it's non-code */
    ASSERT(*pc == RAW_OPCODE_nop || *pc == 0);

    /* FIXME: at some point we should pull out all the hook code from
     * callback.c into an os-neutral location.  For now, this hook
     * is very special-case and simple.
     */

    /* For thread synch, the datasec prot lock will serialize us (FIXME: do this at
     * init time instead, when see [vdso] page in maps file?)
     */

    CHECK(pc - vsyscall_sysenter_return_pc == VSYS_DISPLACED_LEN);
    ASSERT(pc + 1/*nop*/ - vsyscall_sysenter_return_pc == JMP_LONG_LENGTH);
    CHECK(num_nops >= pc - vsyscall_sysenter_return_pc);
    memcpy(vsyscall_syscall_end_pc, vsyscall_sysenter_return_pc,
           /* we don't copy the 5th byte to preserve nop for nice disassembly */
           pc - vsyscall_sysenter_return_pc);
    insert_relative_jump(vsyscall_sysenter_return_pc,
                         /* we require a thread-shared fcache_return */
                         after_do_shared_syscall_addr(dcontext),
                         NOT_HOT_PATCHABLE);

    if (!TEST(MEMPROT_WRITE, prot)) {
        /* we don't override res here since not much point in not using the
         * hook once its in if we failed to re-protect: we're going to have to
         * trust the app code here anyway */
        DEBUG_DECLARE(bool ok =)
            set_protection(vsyscall_page_start, PAGE_SIZE, prot);
        ASSERT(ok);
    }
 hook_vsyscall_return:
    instr_free(dcontext, &instr);
    return res;
#undef CHECK
}

bool
unhook_vsyscall(void)
{
    uint prot;
    bool res;
    uint len = VSYS_DISPLACED_LEN;
    if (get_syscall_method() != SYSCALL_METHOD_SYSENTER)
        return false;
    ASSERT(!sysenter_hook_failed);
    ASSERT(vsyscall_sysenter_return_pc != NULL);
    ASSERT(vsyscall_syscall_end_pc != NULL);
    get_memory_info(vsyscall_page_start, NULL, NULL, &prot);
    if (!TEST(MEMPROT_WRITE, prot)) {
        res = set_protection(vsyscall_page_start, PAGE_SIZE, prot|MEMPROT_WRITE);
        if (!res)
            return false;
    }
    memcpy(vsyscall_sysenter_return_pc, vsyscall_syscall_end_pc, len);
    /* we do not restore the 5th (junk/nop) byte (we never copied it) */
    memset(vsyscall_syscall_end_pc, RAW_OPCODE_nop, len);
    if (!TEST(MEMPROT_WRITE, prot)) {
        res = set_protection(vsyscall_page_start, PAGE_SIZE, prot);
        ASSERT(res);
    }
    return true;
}
#endif /* LINUX */

void
check_syscall_method(dcontext_t *dcontext, instr_t *instr)
{
    int new_method = SYSCALL_METHOD_UNINITIALIZED;
    if (instr_get_opcode(instr) == OP_int)
        new_method = SYSCALL_METHOD_INT;
    else if (instr_get_opcode(instr) == OP_sysenter)
        new_method = SYSCALL_METHOD_SYSENTER;
    else if (instr_get_opcode(instr) == OP_syscall)
        new_method = SYSCALL_METHOD_SYSCALL;
#ifdef WINDOWS
    else if (instr_get_opcode(instr) == OP_call_ind)
        new_method = SYSCALL_METHOD_WOW64;
#endif
    else
        ASSERT_NOT_REACHED();

    if (new_method == SYSCALL_METHOD_SYSENTER ||
        IF_X64_ELSE(false, new_method == SYSCALL_METHOD_SYSCALL)) {
        DO_ONCE({
            /* FIXME: DO_ONCE will unprot and reprot, and here we unprot again */
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            /* FIXME : using the raw-bits as the app pc for the instr is
             * not really supported, but places in monitor assume it as well */
            ASSERT(instr_raw_bits_valid(instr) &&
                   !instr_has_allocated_bits(instr));
            /* Some places (such as clean_syscall_wrapper) assume that only int system
             * calls are used in older versions of windows. */
            IF_WINDOWS(ASSERT(get_os_version() > WINDOWS_VERSION_2000 &&
                              "Expected int syscall method on NT and 2000"));
            /* Used by SYSCALL_PC in win32/os.c for non int system calls */
            IF_WINDOWS(app_sysenter_instr_addr = instr_get_raw_bits(instr));
            /* we expect, only on XP and later or on recent linux kernels,
             * indirected syscalls through a certain page, which we record here
             * FIXME: don't allow anyone to make this region writable?
             */
            /* FIXME : we need to verify that windows lays out all of the
             * syscall stuff as expected on AMD chips: xref PR 205898.
             */
            /* FIXME: bootstrapping problem...would be nicer to read ahead and find
             * syscall before needing to know about page it's on, but for now we just
             * check if our initial assignments were correct
             */
            vsyscall_syscall_end_pc = instr_get_raw_bits(instr) +
                instr_length(dcontext, instr);
            IF_WINDOWS({
                /* for XP sp0,1 (but not sp2) and 03 fixup boostrap values */
                if (vsyscall_page_start ==  VSYSCALL_PAGE_START_BOOTSTRAP_VALUE) {
                    vsyscall_page_start = (app_pc) PAGE_START(instr_get_raw_bits(instr));
                    ASSERT(vsyscall_page_start == VSYSCALL_PAGE_START_BOOTSTRAP_VALUE);
                }
                if (vsyscall_after_syscall == VSYSCALL_AFTER_SYSCALL_BOOTSTRAP_VALUE) {
                    /* for XP sp0,1 and 03 the ret is immediately after the
                     * sysenter instruction */
                    vsyscall_after_syscall = instr_get_raw_bits(instr) +
                        instr_length(dcontext, instr);
                    ASSERT(vsyscall_after_syscall ==
                           VSYSCALL_AFTER_SYSCALL_BOOTSTRAP_VALUE);
                }
            });
            /* For linux, we should have found "[vdso]" in the maps file */
            IF_LINUX(ASSERT(vsyscall_page_start != NULL &&
                            vsyscall_page_start ==
                            (app_pc) PAGE_START(instr_get_raw_bits(instr))));
            LOG(GLOBAL, LOG_SYSCALLS|LOG_VMAREAS, 2,
                "Found vsyscall @ "PFX" => page "PFX", post "PFX"\n",
                instr_get_raw_bits(instr), vsyscall_page_start,
                IF_WINDOWS_ELSE(vsyscall_after_syscall, vsyscall_syscall_end_pc));
            /* make sure system call numbers match */
            IF_WINDOWS(DOCHECK(1, { check_syscall_numbers(dcontext); }));
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        });
    } else {
#ifdef WINDOWS
        DO_ONCE({
            /* FIXME: DO_ONCE will unprot and reprot, and here we unprot again */
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            /* Close vsyscall page hole.
             * FIXME: the vsyscall page can still be in use and contain int:
             * though I have yet to see that case where the page is not marked rx.
             * On linux the vsyscall page is reached via "call *%gs:0x10", but
             * sometimes that call ends up at /lib/ld-2.3.4.so:_dl_sysinfo_int80
             * instead (which is the case when the vsyscall page is marked with no
             * permissions).
             */
            LOG(GLOBAL, LOG_SYSCALLS|LOG_VMAREAS, 2,
                "Closing vsyscall page hole (int @ "PFX") => page "PFX", post "PFX"\n",
                instr_get_translation(instr), vsyscall_page_start,
                IF_WINDOWS_ELSE(vsyscall_after_syscall, vsyscall_syscall_end_pc));
            vsyscall_page_start = NULL;
            vsyscall_after_syscall = NULL;
            ASSERT_CURIOSITY(new_method != SYSCALL_METHOD_WOW64 ||
                             (get_os_version() > WINDOWS_VERSION_XP &&
                              is_wow64_process(NT_CURRENT_PROCESS) &&
                              "Unexpected WOW64 syscall method"));
            /* make sure system call numbers match */
            DOCHECK(1, { check_syscall_numbers(dcontext); });
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        });
#else
        /* On Linux we can't clear vsyscall_page_start as the app will often use both
         * inlined int and vsyscall sysenter system calls. We handle fixing up for that
         * in the next ifdef. */
#endif
    }

#ifdef UNIX
    if (new_method != get_syscall_method() &&
        /* PR 286922: for linux, vsyscall method trumps occasional use of int.  We
         * update do_syscall for the vsyscall method, and use do_int_syscall for any
         * int uses. */
        (new_method != SYSCALL_METHOD_INT ||
         (get_syscall_method() != SYSCALL_METHOD_SYSENTER &&
          get_syscall_method() != SYSCALL_METHOD_SYSCALL))) {
        ASSERT(get_syscall_method() == SYSCALL_METHOD_UNINITIALIZED ||
               get_syscall_method() == SYSCALL_METHOD_INT);
# ifdef LINUX
        if (new_method == SYSCALL_METHOD_SYSENTER) {
#  ifndef HAVE_TLS
            if (DYNAMO_OPTION(hook_vsyscall)) {
                /* PR 361894: we use TLS for our vsyscall hook (PR 212570) */
                FATAL_USAGE_ERROR(SYSENTER_NOT_SUPPORTED, 2,
                                  get_application_name(), get_application_pid());
            }
#  endif
            /* Hook the sysenter continuation point so we don't lose control */
            if (!sysenter_hook_failed && !hook_vsyscall(dcontext)) {
                /* PR 212570: for now we bail out to using int;
                 * for performance we should clobber the retaddr and
                 * keep the sysenters.
                 */
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
                sysenter_hook_failed = true;
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
                LOG(GLOBAL, LOG_SYSCALLS|LOG_VMAREAS, 1,
                    "Unable to hook vsyscall page; falling back on int\n");
            }
            if (sysenter_hook_failed)
                new_method = SYSCALL_METHOD_INT;
        }
# endif /* LINUX */
        if (get_syscall_method() == SYSCALL_METHOD_UNINITIALIZED ||
            new_method != get_syscall_method()) {
            set_syscall_method(new_method);
            /* update the places we have emitted syscalls: do_*syscall */
            update_syscalls(dcontext);
        }
    }
#else
    /* we assume only single method; else need multiple do_syscalls */
    ASSERT(new_method == get_syscall_method());
#endif
}

int
get_syscall_method(void)
{
    return syscall_method;
}

/* Does the syscall instruction always return to the invocation point? */
bool
does_syscall_ret_to_callsite(void)
{
    return (syscall_method == SYSCALL_METHOD_INT ||
            syscall_method == SYSCALL_METHOD_SYSCALL
            IF_WINDOWS(|| syscall_method == SYSCALL_METHOD_WOW64)
            /* The app is reported to be at whatever's in edx, so
             * for our purposes it does return to the call site
             * if we always mangle edx to point there.  Since we inline
             * Mac sysenter (well, we execute it inside fragments, even
             * if we don't continue (except maybe in a trace) we do
             * want to return true here for skipping syscalls and
             * handling interrupted syscalls.
             */
            IF_MACOS(|| syscall_method == SYSCALL_METHOD_SYSENTER));
}

void
set_syscall_method(int method)
{
    ASSERT(syscall_method == SYSCALL_METHOD_UNINITIALIZED
           IF_UNIX(|| syscall_method == SYSCALL_METHOD_INT/*PR 286922*/));
    syscall_method = method;
}

#ifdef LINUX
/* PR 313715: If we fail to hook the vsyscall page (xref PR 212570, PR 288330)
 * we fall back on int, but we have to tweak syscall param #5 (ebp)
 */
bool
should_syscall_method_be_sysenter(void)
{
    return sysenter_hook_failed;
}
#endif

/* returns the address of the first app syscall instruction we saw (see hack
 * in win32/os.c that uses this for PRE_SYSCALL_PC, not for general use */
byte *
get_app_sysenter_addr()
{
    /* FIXME : would like to assert that this has been initialized, but interp
     * bb_process_convertible_indcall() will use it before we initialize it. */
    return app_sysenter_instr_addr;
}

void
copy_mcontext(priv_mcontext_t *src, priv_mcontext_t *dst)
{
    /* FIXME: do we need this? */
    *dst = *src;
}

bool
dr_mcontext_to_priv_mcontext(priv_mcontext_t *dst, dr_mcontext_t *src)
{
    /* we assume fields from xdi onward are identical.
     * if we append to dr_mcontext_t in the future we'll need
     * to check src->size here.
     */
    if (src->size != sizeof(dr_mcontext_t))
        return false;
    if (TESTALL(DR_MC_ALL, src->flags))
        *dst = *(priv_mcontext_t*)(&src->xdi);
    else {
        if (TEST(DR_MC_INTEGER, src->flags)) {
            memcpy(&dst->xdi, &src->xdi, offsetof(priv_mcontext_t, xsp));
            memcpy(&dst->xbx, &src->xbx, offsetof(priv_mcontext_t, xflags) -
                   offsetof(priv_mcontext_t, xbx));
        }
        if (TEST(DR_MC_CONTROL, src->flags)) {
            dst->xsp = src->xsp;
            dst->xflags = src->xflags;
            dst->xip = src->xip;
        }
        if (TEST(DR_MC_MULTIMEDIA, src->flags)) {
            memcpy(&dst->ymm, &src->ymm, sizeof(dst->ymm));
        }
    }
    return true;
}

bool
priv_mcontext_to_dr_mcontext(dr_mcontext_t *dst, priv_mcontext_t *src)
{
    /* we assume fields from xdi onward are identical.
     * if we append to dr_mcontext_t in the future we'll need
     * to check dst->size here.
     */
    if (dst->size != sizeof(dr_mcontext_t))
        return false;
    if (TESTALL(DR_MC_ALL, dst->flags))
        *(priv_mcontext_t*)(&dst->xdi) = *src;
    else {
        if (TEST(DR_MC_INTEGER, dst->flags)) {
            memcpy(&dst->xdi, &src->xdi, offsetof(priv_mcontext_t, xsp));
            memcpy(&dst->xbx, &src->xbx, offsetof(priv_mcontext_t, xflags) -
                   offsetof(priv_mcontext_t, xbx));
        }
        if (TEST(DR_MC_CONTROL, dst->flags)) {
            dst->xsp = src->xsp;
            dst->xflags = src->xflags;
            dst->xip = src->xip;
        }
        if (TEST(DR_MC_MULTIMEDIA, dst->flags)) {
            memcpy(&dst->ymm, &src->ymm, sizeof(dst->ymm));
        }
    }
    return true;
}

priv_mcontext_t *
dr_mcontext_as_priv_mcontext(dr_mcontext_t *mc)
{
    /* We allow not selected xmm fields since clients may legitimately
     * emulate a memref w/ just GPRs
     */
    CLIENT_ASSERT(TESTALL(DR_MC_CONTROL|DR_MC_INTEGER, mc->flags),
                  "dr_mcontext_t.flags must include DR_MC_CONTROL and DR_MC_INTEGER");
    return (priv_mcontext_t*)(&mc->xdi);
}

void
dr_mcontext_init(dr_mcontext_t *mc)
{
    mc->size = sizeof(dr_mcontext_t);
    mc->flags = DR_MC_ALL;
}

/* dumps the context */
void
dump_mcontext(priv_mcontext_t *context, file_t f, bool dump_xml)
{
    print_file(f, dump_xml ?
               "\t<priv_mcontext_t value=\"@"PFX"\""
               "\n\t\txax=\""PFX"\"\n\t\txbx=\""PFX"\""
               "\n\t\txcx=\""PFX"\"\n\t\txdx=\""PFX"\""
               "\n\t\txsi=\""PFX"\"\n\t\txdi=\""PFX"\""
               "\n\t\txbp=\""PFX"\"\n\t\txsp=\""PFX"\""
#ifdef X64
               "\n\t\tr8=\""PFX"\"\n\t\tr9=\""PFX"\""
               "\n\t\tr10=\""PFX"\"\n\t\tr11=\""PFX"\""
               "\n\t\tr12=\""PFX"\"\n\t\tr13=\""PFX"\""
               "\n\t\tr14=\""PFX"\"\n\t\tr15=\""PFX"\""
#endif
               :
               "priv_mcontext_t @"PFX"\n"
               "\txax = "PFX"\n\txbx = "PFX"\n\txcx = "PFX"\n\txdx = "PFX"\n"
               "\txsi = "PFX"\n\txdi = "PFX"\n\txbp = "PFX"\n\txsp = "PFX"\n"
#ifdef X64
               "\tr8  = "PFX"\n\tr9  = "PFX"\n\tr10 = "PFX"\n\tr11 = "PFX"\n"
               "\tr12 = "PFX"\n\tr13 = "PFX"\n\tr14 = "PFX"\n\tr15 = "PFX"\n"
#endif
               ,
               context,
               context->xax, context->xbx, context->xcx, context->xdx,
               context->xsi, context->xdi, context->xbp, context->xsp
#ifdef X64
               , context->r8,  context->r9,  context->r10,  context->r11,
               context->r12, context->r13, context->r14,  context->r15
#endif
               );
    if (preserve_xmm_caller_saved()) {
        int i, j;
        for (i=0; i<NUM_XMM_SAVED; i++) {
            if (YMM_ENABLED()) {
                print_file(f, dump_xml ? "\t\tymm%d= \"0x" : "\tymm%d= 0x", i);
                for (j = 0; j < 8; j++) {
                    print_file(f, "%08x", context->ymm[i].u32[j]);
                }
            } else {
                print_file(f, dump_xml ? "\t\txmm%d= \"0x" : "\txmm%d= 0x", i);
                /* This would be simpler if we had uint64 fields in dr_xmm_t but
                 * that complicates our struct layouts */
                for (j = 0; j < 4; j++) {
                    print_file(f, "%08x", context->ymm[i].u32[j]);
                }
            }
            print_file(f, dump_xml ? "\"\n" : "\n");
        }
        DOLOG(2, LOG_INTERP, {
            /* Not part of mcontext but useful for tracking app behavior */
            if (!dump_xml) {
                uint mxcsr;
                dr_stmxcsr(&mxcsr);
                print_file(f, "\tmxcsr=0x%08x\n", mxcsr);
            }
        });
    }
    print_file(f, dump_xml ?
               "\n\t\teflags=\""PFX"\"\n\t\tpc=\""PFX"\" />\n"
               :
               "\teflags = "PFX"\n\tpc     = "PFX"\n",
               context->xflags, context->pc);
}


#ifdef PROFILE_RDTSC
/* This only works on Pentium I or later */
#ifdef UNIX
__inline__ uint64 get_time()
{
    uint64 x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
#else /* WINDOWS */
uint64 get_time()
{
    return __rdtsc(); /* compiler intrinsic */
}
#endif
#endif /* PROFILE_RDTSC */


#ifdef DEBUG
bool
is_ibl_routine_type(dcontext_t *dcontext, cache_pc target, ibl_branch_type_t branch_type)
{
    ibl_type_t ibl_type;
    DEBUG_DECLARE(bool is_ibl = )
        get_ibl_routine_type_ex(dcontext, target, &ibl_type _IF_X64(NULL));
    ASSERT(is_ibl);
    return (branch_type == ibl_type.branch_type);
}
#endif /* DEBUG */
