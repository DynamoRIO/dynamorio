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

/*
 * arch.c - x86 architecture specific routines
 */

#include "../globals.h"
#include "../link.h"
#include "../fragment.h"

#include "arch.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "decode.h"
#include "decode_fast.h"
#include "../fcache.h"
#include "proc.h"
#include "instrument.h"

#if defined(DEBUG) || defined(INTERNAL)
#    include "disassemble.h"
#endif

/* in interp.c */
void
interp_init(void);
void
interp_exit(void);

/* Thread-shared generated routines.
 * We don't allocate the shared_code statically so that we can mark it
 * executable.
 */
generated_code_t *shared_code = NULL;
#if defined(X86) && defined(X64)
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

#ifdef WINDOWS
bool gencode_swaps_teb_tls;
#endif

#ifdef X86
bool *d_r_avx512_code_in_use = NULL;
bool d_r_client_avx512_code_in_use = false;
#endif

/* static functions forward references */
static byte *
emit_ibl_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                  byte *fcache_return_pc, ibl_source_fragment_type_t source_fragment_type,
                  bool thread_shared, bool target_trace_table, ibl_code_t ibl_code[]);

static byte *
emit_syscall_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      bool thread_shared);

int
reg_spill_tls_offs(reg_id_t reg)
{
    switch (reg) {
    case SCRATCH_REG0: return TLS_REG0_SLOT;
    case SCRATCH_REG1: return TLS_REG1_SLOT;
    case SCRATCH_REG2: return TLS_REG2_SLOT;
    case SCRATCH_REG3: return TLS_REG3_SLOT;
#ifdef AARCHXX
    case SCRATCH_REG4: return TLS_REG4_SLOT;
    case SCRATCH_REG5:
        return TLS_REG5_SLOT;
        /* We do not include the stolen reg slot b/c its load+stores are reversed
         * and must be special-cased vs other spills.
         */
#endif
    }
    /* don't assert if another reg passed: used on random regs looking for spills */
    return -1;
}

/* For Thumb, we store all the entry points with LSB=0 and rely on anyone
 * targeting them to use PC_AS_JMP_TGT().
 */

#ifdef INTERNAL
/* routine can be used for dumping both thread private and the thread shared routines */
static void
dump_emitted_routines(dcontext_t *dcontext, file_t file, const char *code_description,
                      generated_code_t *code, byte *emitted_pc)
{
    byte *last_pc;
    /* FIXME i#1551: merge w/ GENCODE_IS_X86 below */
#    if defined(X86) && defined(X64)
    if (GENCODE_IS_X86(code->gencode_mode)) {
        /* parts of x86 gencode are 64-bit but it's hard to know which here
         * so we dump all as x86
         */
        set_x86_mode(dcontext, true /*x86*/);
    }
#    endif

    print_file(file, "%s routines created:\n", code_description);
    {
        last_pc = code->gen_start_pc;
        do {
            const char *ibl_brtype;
            const char *ibl_name = get_ibl_routine_name(dcontext, last_pc, &ibl_brtype);

#    ifdef WINDOWS
            /* must test first, as get_ibl_routine_name will think "bb_ibl_indjmp" */
            if (last_pc == code->unlinked_shared_syscall)
                print_file(file, "unlinked_shared_syscall:\n");
            else if (last_pc == code->shared_syscall)
                print_file(file, "shared_syscall:\n");
            else
#    endif
                if (ibl_name)
                print_file(file, "%s_%s:\n", ibl_name, ibl_brtype);
            else if (last_pc == code->fcache_enter)
                print_file(file, "fcache_enter:\n");
            else if (last_pc == code->fcache_return)
                print_file(file, "fcache_return:\n");
            else if (last_pc == code->do_syscall)
                print_file(file, "do_syscall:\n");
#    ifdef AARCHXX
            else if (last_pc == code->fcache_enter_gonative)
                print_file(file, "fcache_enter_gonative:\n");
#    endif
#    ifdef WINDOWS
            else if (last_pc == code->fcache_enter_indirect)
                print_file(file, "fcache_enter_indirect:\n");
            else if (last_pc == code->do_callback_return)
                print_file(file, "do_callback_return:\n");
#    else
            else if (last_pc == code->do_int_syscall)
                print_file(file, "do_int_syscall:\n");
            else if (last_pc == code->do_int81_syscall)
                print_file(file, "do_int81_syscall:\n");
            else if (last_pc == code->do_int82_syscall)
                print_file(file, "do_int82_syscall:\n");
            else if (last_pc == code->do_clone_syscall)
                print_file(file, "do_clone_syscall:\n");
#        ifdef VMX86_SERVER
            else if (last_pc == code->do_vmkuw_syscall)
                print_file(file, "do_vmkuw_syscall:\n");
#        endif
#    endif
#    ifdef UNIX
            else if (last_pc == code->new_thread_dynamo_start)
                print_file(file, "new_thread_dynamo_start:\n");
#    endif
#    ifdef TRACE_HEAD_CACHE_INCR
            else if (last_pc == code->trace_head_incr)
                print_file(file, "trace_head_incr:\n");
#    endif
            else if (last_pc == code->reset_exit_stub)
                print_file(file, "reset_exit_stub:\n");
            else if (last_pc == code->fcache_return_coarse)
                print_file(file, "fcache_return_coarse:\n");
            else if (last_pc == code->trace_head_return_coarse)
                print_file(file, "trace_head_return_coarse:\n");
            else if (last_pc == code->special_ibl_xfer[CLIENT_IBL_IDX])
                print_file(file, "client_ibl_xfer:\n");
#    ifdef UNIX
            else if (last_pc == code->special_ibl_xfer[NATIVE_PLT_IBL_IDX])
                print_file(file, "native_plt_ibl_xfer:\n");
            else if (last_pc == code->special_ibl_xfer[NATIVE_RET_IBL_IDX])
                print_file(file, "native_ret_ibl_xfer:\n");
#    endif
            else if (last_pc == code->clean_call_save)
                print_file(file, "clean_call_save:\n");
            else if (last_pc == code->clean_call_restore)
                print_file(file, "clean_call_restore:\n");
            last_pc = disassemble_with_bytes(dcontext, last_pc, file);
        } while (last_pc < emitted_pc);
        print_file(file, "%s routines size: " SSZFMT " / " SSZFMT "\n\n",
                   code_description, emitted_pc - code->gen_start_pc,
                   code->commit_end_pc - code->gen_start_pc);
    }

#    if defined(X86) && defined(X64)
    if (GENCODE_IS_X86(code->gencode_mode))
        set_x86_mode(dcontext, false /*x64*/);
#    endif
}

void
dump_emitted_routines_to_file(dcontext_t *dcontext, const char *filename,
                              const char *label, generated_code_t *code, byte *stop_pc)
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

static byte *
code_align_forward(dr_isa_mode_t isa_mode, byte *pc, size_t alignment)
{
    byte *new_pc = (byte *)ALIGN_FORWARD(pc, alignment);
    DOCHECK(1, { SET_TO_NOPS(isa_mode, vmcode_get_writable_addr(pc), new_pc - pc); });
    return new_pc;
}

static byte *
move_to_start_of_cache_line(dr_isa_mode_t isa_mode, byte *pc)
{
    return code_align_forward(isa_mode, pc, proc_get_cache_line_size());
}

/* The real size of generated code we need varies by cache line size and
 * options like inlining of ibl code.  We also generate different routines
 * for thread-private and thread-shared.  So, we dynamically extend the size
 * as we generate.  Currently our max is under 5 pages.
 */
#define GENCODE_RESERVE_SIZE (5 * PAGE_SIZE)

#define GENCODE_COMMIT_SIZE \
    ((size_t)(ALIGN_FORWARD(sizeof(generated_code_t), PAGE_SIZE) + PAGE_SIZE))

static byte *
check_size_and_cache_line(dr_isa_mode_t isa_mode, generated_code_t *code, byte *pc)
{
    /* Assumption: no single emit uses more than a page.
     * We keep an extra page at all times and release it at the end.
     */
    byte *next_pc = move_to_start_of_cache_line(isa_mode, pc);
    if ((byte *)ALIGN_FORWARD(pc, PAGE_SIZE) + PAGE_SIZE > code->commit_end_pc) {
        ASSERT(code->commit_end_pc + PAGE_SIZE <=
               vmcode_get_executable_addr((byte *)code) + GENCODE_RESERVE_SIZE);
        heap_mmap_extend_commitment(code->commit_end_pc, PAGE_SIZE,
                                    VMM_SPECIAL_MMAP | VMM_REACHABLE);
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
    size_t leftover =
        (ptr_uint_t)code->commit_end_pc - ALIGN_FORWARD(code->gen_end_pc, PAGE_SIZE);
    ASSERT(code->commit_end_pc >= (byte *)ALIGN_FORWARD(code->gen_end_pc, PAGE_SIZE));
    ASSERT(ALIGNED(code->commit_end_pc, PAGE_SIZE));
    ASSERT(ALIGNED(leftover, PAGE_SIZE));
    if (leftover > 0) {
        heap_mmap_retract_commitment(code->commit_end_pc - leftover, leftover,
                                     VMM_SPECIAL_MMAP | VMM_REACHABLE);
        code->commit_end_pc -= leftover;
    }
    LOG(THREAD_GET, LOG_EMIT, 1,
        "Generated code " PFX ": %d header, " SZFMT " gen, " SZFMT " commit/%d reserve\n",
        code, sizeof(*code), code->gen_end_pc - code->gen_start_pc,
        (ptr_uint_t)code->commit_end_pc - (ptr_uint_t)code, GENCODE_RESERVE_SIZE);
}

static void
shared_gencode_emit(generated_code_t *gencode _IF_X86_64(bool x86_mode))
{
    byte *pc;
    /* As ARM mode switches are inexpensive, we do not need separate gencode
     * versions and stick with Thumb for all our gencode.
     */
    dr_isa_mode_t isa_mode = dr_get_isa_mode(GLOBAL_DCONTEXT);

    pc = gencode->gen_start_pc;
    /* Temporarily set this so that ibl queries work during generation */
    gencode->gen_end_pc = gencode->commit_end_pc;
    pc = check_size_and_cache_line(isa_mode, gencode, pc);
    gencode->fcache_enter = pc;
    pc = emit_fcache_enter_shared(GLOBAL_DCONTEXT, gencode, pc);
    pc = check_size_and_cache_line(isa_mode, gencode, pc);
    gencode->fcache_return = pc;
    pc = emit_fcache_return_shared(GLOBAL_DCONTEXT, gencode, pc);
    gencode->fcache_return_end = pc;
    if (DYNAMO_OPTION(coarse_units)) {
        pc = check_size_and_cache_line(isa_mode, gencode, pc);
        gencode->fcache_return_coarse = pc;
        pc = emit_fcache_return_coarse(GLOBAL_DCONTEXT, gencode, pc);
        gencode->fcache_return_coarse_end = pc;
        pc = check_size_and_cache_line(isa_mode, gencode, pc);
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
        pc = emit_ibl_routines(GLOBAL_DCONTEXT, gencode, pc, gencode->fcache_return,
                               DYNAMO_OPTION(shared_traces)
                                   ? IBL_TRACE_SHARED
                                   : IBL_TRACE_PRIVATE, /* source type */
                               true,                    /* thread_shared */
                               true,                    /* target_trace_table */
                               gencode->trace_ibl);
    }
    if (USE_SHARED_BB_IBL()) {
        pc = emit_ibl_routines(GLOBAL_DCONTEXT, gencode, pc, gencode->fcache_return,
                               IBL_BB_SHARED, /* source_fragment_type */
                               /* thread_shared */
                               IF_X86_64_ELSE(true, SHARED_FRAGMENTS_ENABLED()),
                               !DYNAMO_OPTION(bb_ibl_targets), /* target_trace_table */
                               gencode->bb_ibl);
    }
    if (DYNAMO_OPTION(coarse_units)) {
        pc = emit_ibl_routines(GLOBAL_DCONTEXT, gencode, pc,
                               /* ibl routines use regular fcache_return */
                               gencode->fcache_return,
                               IBL_COARSE_SHARED, /* source_fragment_type */
                               /* thread_shared */
                               IF_X86_64_ELSE(true, SHARED_FRAGMENTS_ENABLED()),
                               !DYNAMO_OPTION(bb_ibl_targets), /*target_trace_table*/
                               gencode->coarse_ibl);
    }

#ifdef WINDOWS_PC_SAMPLE
    gencode->ibl_routines_end = pc;
#endif
#if defined(WINDOWS) && !defined(X64)
    /* no dispatch needed on x64 since syscall routines are thread-shared */
    if (DYNAMO_OPTION(shared_fragment_shared_syscalls)) {
        pc = check_size_and_cache_line(isa_mode, gencode, pc);
        gencode->shared_syscall = pc;
        pc = emit_shared_syscall_dispatch(GLOBAL_DCONTEXT, pc);
        pc = check_size_and_cache_line(isa_mode, gencode, pc);
        gencode->unlinked_shared_syscall = pc;
        pc = emit_unlinked_shared_syscall_dispatch(GLOBAL_DCONTEXT, pc);
        LOG(GLOBAL, LOG_EMIT, 3,
            "shared_syscall_dispatch: linked " PFX ", unlinked " PFX "\n",
            gencode->shared_syscall, gencode->unlinked_shared_syscall);
    }
#endif

#ifdef UNIX
    /* must create before emit_do_clone_syscall() in emit_syscall_routines() */
    pc = check_size_and_cache_line(isa_mode, gencode, pc);
    gencode->new_thread_dynamo_start = pc;
    pc = emit_new_thread_dynamo_start(GLOBAL_DCONTEXT, pc);
#endif

#ifdef AARCHXX
    pc = check_size_and_cache_line(isa_mode, gencode, pc);
    gencode->fcache_enter_gonative = pc;
    pc = emit_fcache_enter_gonative(GLOBAL_DCONTEXT, gencode, pc);
#endif

#if defined(X86) && defined(X64)
#    ifdef WINDOWS
    /* plain fcache_enter indirects through edi, and next_tag is in tls,
     * so we don't need a separate routine for callback return
     */
    gencode->fcache_enter_indirect = gencode->fcache_enter;
#    endif
    /* i#821/PR 284029: for now we assume there are no syscalls in x86 code */
    if (IF_X64_ELSE(!x86_mode, true)) {
        /* PR 244737: syscall routines are all shared */
        pc = emit_syscall_routines(GLOBAL_DCONTEXT, gencode, pc, true /*thread-shared*/);
    }
#elif defined(UNIX) && defined(HAVE_TLS)
    /* PR 212570: we need a thread-shared do_syscall for our vsyscall hook */
    /* PR 361894: we don't support sysenter if no TLS */
    ASSERT(gencode->do_syscall == NULL || dynamo_initialized /*re-gen*/);
    pc = check_size_and_cache_line(isa_mode, gencode, pc);
    gencode->do_syscall = pc;
    pc = emit_do_syscall(GLOBAL_DCONTEXT, gencode, pc, gencode->fcache_return,
                         true /*shared*/, 0, &gencode->do_syscall_offs);
#    ifdef AARCHXX
    /* ARM has no thread-private gencode, so our clone syscall is shared */
    gencode->do_clone_syscall = pc;
    pc = emit_do_clone_syscall(GLOBAL_DCONTEXT, gencode, pc, gencode->fcache_return,
                               true /*shared*/, &gencode->do_clone_syscall_offs);
#    endif
#endif

    if (USE_SHARED_GENCODE_ALWAYS()) {
        fragment_t *fragment;
        /* make reset stub shared */
        gencode->reset_exit_stub = pc;
        fragment = linkstub_fragment(GLOBAL_DCONTEXT, (linkstub_t *)get_reset_linkstub());
#ifdef X86_64
        if (GENCODE_IS_X86(gencode->gencode_mode))
            fragment = empty_fragment_mark_x86(fragment);
#endif
        /* reset exit stub should look just like a direct exit stub */
        pc += insert_exit_stub_other_flags(GLOBAL_DCONTEXT, fragment,
                                           (linkstub_t *)get_reset_linkstub(), pc,
                                           LINK_DIRECT);
    }

#ifdef TRACE_HEAD_CACHE_INCR
    pc = check_size_and_cache_line(isa_mode, gencode, pc);
    gencode->trace_head_incr = pc;
    pc = emit_trace_head_incr_shared(GLOBAL_DCONTEXT, pc, gencode->fcache_return);
#endif

    if (!special_ibl_xfer_is_thread_private()) {
        gencode->special_ibl_xfer[CLIENT_IBL_IDX] = pc;
        pc = emit_client_ibl_xfer(GLOBAL_DCONTEXT, pc, gencode);
#ifdef UNIX
        /* i#1238: native exec optimization */
        if (DYNAMO_OPTION(native_exec_opt)) {
            pc = check_size_and_cache_line(isa_mode, gencode, pc);
            gencode->special_ibl_xfer[NATIVE_PLT_IBL_IDX] = pc;
            pc = emit_native_plt_ibl_xfer(GLOBAL_DCONTEXT, pc, gencode);
            /* native ret */
            pc = check_size_and_cache_line(isa_mode, gencode, pc);
            gencode->special_ibl_xfer[NATIVE_RET_IBL_IDX] = pc;
            pc = emit_native_ret_ibl_xfer(GLOBAL_DCONTEXT, pc, gencode);
        }
#endif
    }

    if (!client_clean_call_is_thread_private()) {
        pc = check_size_and_cache_line(isa_mode, gencode, pc);
        gencode->clean_call_save = pc;
        pc = emit_clean_call_save(GLOBAL_DCONTEXT, pc, gencode);
        pc = check_size_and_cache_line(isa_mode, gencode, pc);
        gencode->clean_call_restore = pc;
        pc = emit_clean_call_restore(GLOBAL_DCONTEXT, pc, gencode);
        gencode->clean_call_restore_end = pc;
    }

    ASSERT(pc < gencode->commit_end_pc);
    gencode->gen_end_pc = pc;

    machine_cache_sync(gencode->gen_start_pc, gencode->gen_end_pc, true);
}

static void
shared_gencode_init(IF_X86_64_ELSE(gencode_mode_t gencode_mode, void))
{
    generated_code_t *gencode;
    ibl_branch_type_t branch_type;
#if defined(X86) && defined(X64)
    bool x86_mode = false;
    bool x86_to_x64_mode = false;
#endif

    /* XXX i#5383: Audit these calls and ensure they cover all scenarios, are placed
     * at the most efficient level, and are always properly paired.
     */
    PTHREAD_JIT_WRITE();

    gencode = heap_mmap_reserve(GENCODE_RESERVE_SIZE, GENCODE_COMMIT_SIZE,
                                MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
                                VMM_SPECIAL_MMAP | VMM_REACHABLE);
    /* we would return gencode and let caller assign, but emit routines
     * that this routine calls query the shared vars so we set here
     */
#if defined(X86) && defined(X64)
    switch (gencode_mode) {
    case GENCODE_X64: shared_code = gencode; break;
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
    default: ASSERT_NOT_REACHED();
    }
#else
    shared_code = gencode;
#endif
    generated_code_t *gencode_writable =
        (generated_code_t *)vmcode_get_writable_addr((byte *)gencode);
    memset(gencode_writable, 0, sizeof(*gencode));
    /* Generated code immediately follows struct */
    gencode_writable->gen_start_pc = ((byte *)gencode) + sizeof(*gencode);
    gencode_writable->commit_end_pc = ((byte *)gencode) + GENCODE_COMMIT_SIZE;
    /* Now switch to the writable one.  We assume no further code examines the address
     * of the struct.
     */
    gencode = gencode_writable;

    gencode->thread_shared = true;
    IF_X86_64(gencode->gencode_mode = gencode_mode);
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        gencode->trace_ibl[branch_type].initialized = false;
        gencode->bb_ibl[branch_type].initialized = false;
        gencode->coarse_ibl[branch_type].initialized = false;
#if defined(X86) && defined(X64)
        /* cache the mode so we can pass just the ibl_code_t around */
        gencode->trace_ibl[branch_type].x86_mode = x86_mode;
        gencode->trace_ibl[branch_type].x86_to_x64_mode = x86_to_x64_mode;
        gencode->bb_ibl[branch_type].x86_mode = x86_mode;
        gencode->bb_ibl[branch_type].x86_to_x64_mode = x86_to_x64_mode;
        gencode->coarse_ibl[branch_type].x86_mode = x86_mode;
        gencode->coarse_ibl[branch_type].x86_to_x64_mode = x86_to_x64_mode;
#endif
    }
#if defined(X86) && defined(X64) && defined(WINDOWS)
    gencode->shared_syscall_code.x86_mode = x86_mode;
    gencode->shared_syscall_code.x86_to_x64_mode = x86_to_x64_mode;
#endif

    shared_gencode_emit(gencode _IF_X86_64(x86_mode));
    release_final_page(gencode);

#ifdef WINDOWS
    /* Ensure the swapping is known at init time and never changes. */
    gencode_swaps_teb_tls = should_swap_teb_static_tls();
#endif

    DOLOG(3, LOG_EMIT, {
        dump_emitted_routines(
            GLOBAL_DCONTEXT, GLOBAL,
            IF_X86_64_ELSE(x86_mode ? "thread-shared x86" : "thread-shared",
                           "thread-shared"),
            gencode, gencode->gen_end_pc);
    });
#ifdef INTERNAL
    if (INTERNAL_OPTION(gendump)) {
        dump_emitted_routines_to_file(
            GLOBAL_DCONTEXT, "gencode-shared",
            IF_X86_64_ELSE(x86_mode ? "thread-shared x86" : "thread-shared",
                           "thread-shared"),
            gencode, gencode->gen_end_pc);
    }
#endif
#ifdef WINDOWS_PC_SAMPLE
    if (dynamo_options.profile_pcs && dynamo_options.prof_pcs_gencode >= 2 &&
        dynamo_options.prof_pcs_gencode <= 32) {
        gencode->profile = create_profile(gencode->gen_start_pc, gencode->gen_end_pc,
                                          dynamo_options.prof_pcs_gencode, NULL);
        start_profile(gencode->profile);
    } else
        gencode->profile = NULL;
#endif

    gencode->writable = true;
    protect_generated_code(gencode, READONLY);
}

#ifdef AARCHXX
/* Called during a reset when all threads are suspended */
void
arch_reset_stolen_reg(void)
{
    /* We have no per-thread gencode.  We simply re-emit on top of the existing
     * shared_code, which means we do not need to update each thread's pointers
     * to gencode stored in TLS.
     */
#    ifdef ARM
    dr_isa_mode_t old_mode;
#    endif
    if (DR_REG_R0 + INTERNAL_OPTION(steal_reg_at_reset) == dr_reg_stolen)
        return;
    SYSLOG_INTERNAL_INFO("swapping stolen reg from %s to %s", reg_names[dr_reg_stolen],
                         reg_names[DR_REG_R0 + INTERNAL_OPTION(steal_reg_at_reset)]);
#    ifdef ARM
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    dr_set_isa_mode(dcontext, DR_ISA_ARM_THUMB, &old_mode);
#    endif

    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    dr_reg_stolen = DR_REG_R0 + INTERNAL_OPTION(steal_reg_at_reset);
    ASSERT(dr_reg_stolen >= DR_REG_STOLEN_MIN && dr_reg_stolen <= DR_REG_STOLEN_MAX);
    protect_generated_code(shared_code, WRITABLE);
    shared_gencode_emit(shared_code);
    protect_generated_code(shared_code, READONLY);
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

#    ifdef ARM
    dr_set_isa_mode(dcontext, old_mode, NULL);
#    endif
    DOLOG(3, LOG_EMIT, {
        dump_emitted_routines(GLOBAL_DCONTEXT, GLOBAL, "swap stolen reg", shared_code,
                              shared_code->gen_end_pc);
    });
}

void
arch_mcontext_reset_stolen_reg(dcontext_t *dcontext, priv_mcontext_t *mc)
{
    /* Put the app value in the old stolen reg */
    *(reg_t *)(((byte *)mc) +
               opnd_get_reg_dcontext_offs(DR_REG_R0 + INTERNAL_OPTION(steal_reg))) =
        dcontext->local_state->spill_space.reg_stolen;
    /* Put the TLs base into the new stolen reg */
    set_stolen_reg_val(mc, (reg_t)os_get_dr_tls_base(dcontext));
}
#endif /* AARCHXX */

#if defined(X86) && defined(X64)
/* Sets other-mode ibl targets, for mixed-mode and x86_to_x64 mode */
static void
far_ibl_set_targets(ibl_code_t src_ibl[], ibl_code_t tgt_ibl[])
{
    ibl_branch_type_t branch_type;
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        if (src_ibl[branch_type].initialized) {
            /* selector was set in emit_far_ibl (but at that point we didn't have
             * the other mode's ibl ready for the target)
             */
            ASSERT(CHECK_TRUNCATE_TYPE_uint(
                (ptr_uint_t)tgt_ibl[branch_type].indirect_branch_lookup_routine));
            ASSERT(CHECK_TRUNCATE_TYPE_uint(
                (ptr_uint_t)tgt_ibl[branch_type].unlinked_ibl_entry));
            src_ibl[branch_type].far_jmp_opnd.pc =
                (uint)(ptr_uint_t)tgt_ibl[branch_type].indirect_branch_lookup_routine;
            src_ibl[branch_type].far_jmp_unlinked_opnd.pc =
                (uint)(ptr_uint_t)tgt_ibl[branch_type].unlinked_ibl_entry;
        }
    }
}
#endif

/* arch-specific initializations */
void
d_r_arch_init(void)
{
    ASSERT(sizeof(opnd_t) == EXPECTED_SIZEOF_OPND);
    IF_X86(ASSERT(CHECK_TRUNCATE_TYPE_byte(OPSZ_LAST)));
    /* This ensures that DR_REG_ enums that may be used as opnd_size_t fit its size.
     * Only DR_REG_ enums covered by types listed in template_optype_is_reg can fall
     * into this category.
     */
    IF_X86(ASSERT(CHECK_TRUNCATE_TYPE_byte(DR_REG_MAX_AS_OPSZ)));
    /* ensure our flag sharing is done properly */
    ASSERT((uint)LINK_FINAL_INSTR_SHARED_FLAG < (uint)INSTR_FIRST_NON_LINK_SHARED_FLAG);
    ASSERT_TRUNCATE(byte, byte, OPSZ_LAST_ENUM);
    ASSERT(DR_ISA_ARM_A32 + 1 == DR_ISA_ARM_THUMB); /* ibl relies on this */
#ifdef X86_64
    /* We rely on contiguous ranges when computing AVX-512 registers. */
    ASSERT(DR_REG_XMM16 == DR_REG_XMM15 + 1);
    ASSERT(DR_REG_YMM16 == DR_REG_YMM15 + 1);
    ASSERT(DR_REG_ZMM16 == DR_REG_ZMM15 + 1);
#endif
    /* We rely on the dr_opmask_t register type to be able to store AVX512BW wide 64-bit
     * masks. Also priv_mcontext_t.opmask slots are AVX512BW wide.
     */
    IF_X86(ASSERT(sizeof(dr_opmask_t) == OPMASK_AVX512BW_REG_SIZE));

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
    /* syscalls_init() should have already set the syscall_method so go ahead
     * and create the globlal_do_syscall now */
    ASSERT(syscall_method != SYSCALL_METHOD_UNINITIALIZED);
#endif

#ifdef AARCHXX
    dr_reg_stolen = DR_REG_R0 + DYNAMO_OPTION(steal_reg);
    ASSERT(dr_reg_stolen >= DR_REG_STOLEN_MIN && dr_reg_stolen <= DR_REG_STOLEN_MAX)
#endif

    /* Ensure we have no unexpected padding inside structs that include
     * priv_mcontext_t (app_state_at_intercept_t and dcontext_t) */
    IF_X86(ASSERT(offsetof(priv_mcontext_t, pc) + sizeof(byte *) + PRE_XMM_PADDING ==
                  offsetof(priv_mcontext_t, simd)));
    ASSERT(offsetof(app_state_at_intercept_t, mc) ==
           offsetof(app_state_at_intercept_t, start_pc) + sizeof(void *));
    /* Try to catch errors in x86.asm offsets for dcontext_t */
    ASSERT(sizeof(unprotected_context_t) ==
           sizeof(priv_mcontext_t) + IF_WINDOWS_ELSE(IF_X64_ELSE(8, 4), 8) +
               5 * sizeof(reg_t));

    interp_init();

#ifdef X86
    /* We're allocating a reachable heap variable in order to be able to use a more
     * compact rip-rel load in SIMD restore/save gencode.
     */
    d_r_avx512_code_in_use = heap_reachable_alloc(
        GLOBAL_DCONTEXT, sizeof(*d_r_avx512_code_in_use) HEAPACCT(ACCT_OTHER));
    *d_r_avx512_code_in_use = false;
#endif

#ifdef CHECK_RETURNS_SSE2
    if (proc_has_feature(FEATURE_SSE2)) {
        FATAL_USAGE_ERROR(CHECK_RETURNS_SSE2_REQUIRES_SSE2, 2, get_application_name(),
                          get_application_pid());
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

        shared_gencode_init(IF_X86_64(GENCODE_X64));
#if defined(X86) && defined(X64)
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
            far_ibl_set_targets(shared_code->bb_ibl, shared_code_opposite_mode->bb_ibl);
            far_ibl_set_targets(shared_code->coarse_ibl,
                                shared_code_opposite_mode->coarse_ibl);

            far_ibl_set_targets(shared_code_opposite_mode->trace_ibl,
                                shared_code->trace_ibl);
            far_ibl_set_targets(shared_code_opposite_mode->bb_ibl, shared_code->bb_ibl);
            far_ibl_set_targets(shared_code_opposite_mode->coarse_ibl,
                                shared_code->coarse_ibl);
        }
#endif
    }

    /* Ensure addressing registers fit into base+disp operand base and index fields. */
    IF_AARCHXX(ASSERT_BITFIELD_TRUNCATE(REG_SPECIFIER_BITS, DR_REG_MAX_ADDRESSING_REG));

    mangle_init();
}

#ifdef WINDOWS_PC_SAMPLE
static void
arch_extract_profile(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *tpc = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    thread_id_t tid = dcontext == GLOBAL_DCONTEXT ? 0 : dcontext->owning_thread;
    /* we may not have x86 gencode */
    ASSERT(tpc != NULL IF_X86_64(|| mode == GENCODE_X86));
    if (tpc != NULL && tpc->profile != NULL) {

        ibl_branch_type_t branch_type;
        int sum;

        protect_generated_code(tpc, WRITABLE);

        stop_profile(tpc->profile);
        d_r_mutex_lock(&profile_dump_lock);

        /* Print the thread id so even if it has no hits we can
         * count the # total threads. */
        print_file(profile_file, "Profile for thread " TIDFMT "\n", tid);
        sum = sum_profile_range(tpc->profile, tpc->fcache_enter,
                                tpc->fcache_enter_return_end);
        if (sum > 0) {
            print_file(profile_file,
                       "\nDumping cache enter/exit code profile "
                       "(thread " TIDFMT ")\n%d hits\n",
                       tid, sum);
            dump_profile_range(profile_file, tpc->profile, tpc->fcache_enter,
                               tpc->fcache_enter_return_end);
        }

        /* Break out the IBL code by trace/BB and opcode types.
         * Not worth showing far_ibl hits since should be quite rare.
         */
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {

            byte *start;
            byte *end;

            if (tpc->trace_ibl[branch_type].initialized) {
                start = tpc->trace_ibl[branch_type].indirect_branch_lookup_routine;
                end = start + tpc->trace_ibl[branch_type].ibl_routine_length;
                sum = sum_profile_range(tpc->profile, start, end);
                if (sum > 0) {
                    print_file(profile_file,
                               "\nDumping trace IBL code %s profile "
                               "(thread " TIDFMT ")\n%d hits\n",
                               get_branch_type_name(branch_type), tid, sum);
                    dump_profile_range(profile_file, tpc->profile, start, end);
                }
            }
            if (tpc->bb_ibl[branch_type].initialized) {
                start = tpc->bb_ibl[branch_type].indirect_branch_lookup_routine;
                end = start + tpc->bb_ibl[branch_type].ibl_routine_length;
                sum = sum_profile_range(tpc->profile, start, end);
                if (sum > 0) {
                    print_file(profile_file,
                               "\nDumping BB IBL code %s profile "
                               "(thread " TIDFMT ")\n%d hits\n",
                               get_branch_type_name(branch_type), tid, sum);
                    dump_profile_range(profile_file, tpc->profile, start, end);
                }
            }
            if (tpc->coarse_ibl[branch_type].initialized) {
                start = tpc->coarse_ibl[branch_type].indirect_branch_lookup_routine;
                end = start + tpc->coarse_ibl[branch_type].ibl_routine_length;
                sum = sum_profile_range(tpc->profile, start, end);
                if (sum > 0) {
                    print_file(profile_file,
                               "\nDumping coarse IBL code %s profile "
                               "(thread " TIDFMT ")\n%d hits\n",
                               get_branch_type_name(branch_type), tid, sum);
                    dump_profile_range(profile_file, tpc->profile, start, end);
                }
            }
        }

        sum = sum_profile_range(tpc->profile, tpc->ibl_routines_end, tpc->profile->end);
        if (sum > 0) {
            print_file(profile_file,
                       "\nDumping generated code profile "
                       "(thread " TIDFMT ")\n%d hits\n",
                       tid, sum);
            dump_profile_range(profile_file, tpc->profile, tpc->ibl_routines_end,
                               tpc->profile->end);
        }

        d_r_mutex_unlock(&profile_dump_lock);
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
d_r_arch_exit(IF_WINDOWS_ELSE_NP(bool detach_stacked_callbacks, void))
{
    /* we only need to unprotect shared_code for profile extraction
     * so we do it there to also cover the fast exit path
     */
#ifdef WINDOWS_PC_SAMPLE
    arch_profile_exit();
#endif
    /* on x64 we have syscall routines in the shared code so can't free if detaching */
    if (IF_WINDOWS(IF_X64(!detach_stacked_callbacks &&)) shared_code != NULL) {
        heap_munmap(shared_code, GENCODE_RESERVE_SIZE, VMM_SPECIAL_MMAP | VMM_REACHABLE);
    }
#if defined(X86) && defined(X64)
    if (shared_code_x86 != NULL) {
        heap_munmap(shared_code_x86, GENCODE_RESERVE_SIZE,
                    VMM_SPECIAL_MMAP | VMM_REACHABLE);
    }
    if (shared_code_x86_to_x64 != NULL) {
        heap_munmap(shared_code_x86_to_x64, GENCODE_RESERVE_SIZE,
                    VMM_SPECIAL_MMAP | VMM_REACHABLE);
    }
#endif

#ifdef X86
    heap_reachable_free(GLOBAL_DCONTEXT, d_r_avx512_code_in_use,
                        sizeof(*d_r_avx512_code_in_use) HEAPACCT(ACCT_OTHER));
#endif

    interp_exit();
    mangle_exit();

    if (doing_detach) {
        /* Clear for possible re-attach. */
        shared_code = NULL;
#if defined(X86) && defined(X64)
        shared_code_x86 = NULL;
        shared_code_x86_to_x64 = NULL;
#endif
        app_sysenter_instr_addr = NULL;
#ifdef LINUX
        /* If we don't clear this we get asserts on vsyscall hook on re-attach on
         * some Linux variants.  We don't want to clear on Windows 8+ as that causes
         * asserts on re-attach (i#2145).
         */
        syscall_method = SYSCALL_METHOD_UNINITIALIZED;
        sysenter_hook_failed = false;
#endif
    }
}

static byte *
emit_ibl_routine_and_template(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                              byte *fcache_return_pc, bool target_trace_table,
                              bool inline_ibl_head, bool thread_shared,
                              ibl_branch_type_t branch_type,
                              ibl_source_fragment_type_t source_type,
                              ibl_code_t *ibl_code)
{
    /* FIXME i#1551: pass in or store mode in generated_code_t */
    dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    ibl_code->initialized = true;
    ibl_code->indirect_branch_lookup_routine = pc;
    ibl_code->ibl_head_is_inlined = inline_ibl_head;
    ibl_code->thread_shared_routine = thread_shared;
    ibl_code->branch_type = branch_type;
    ibl_code->source_fragment_type = source_type;

    pc = emit_indirect_branch_lookup(dcontext, code, pc, fcache_return_pc,
                                     target_trace_table, inline_ibl_head, ibl_code);
    if (inline_ibl_head) {
        /* create the inlined ibl template */
        pc = check_size_and_cache_line(isa_mode, code, pc);
        pc = emit_inline_ibl_stub(dcontext, pc, ibl_code, target_trace_table);
    }

    ibl_code->far_ibl = pc;
    pc = emit_far_ibl(
        dcontext, pc, ibl_code,
        ibl_code->indirect_branch_lookup_routine _IF_X86_64(&ibl_code->far_jmp_opnd));
    ibl_code->far_ibl_unlinked = pc;
    pc = emit_far_ibl(
        dcontext, pc, ibl_code,
        ibl_code->unlinked_ibl_entry _IF_X86_64(&ibl_code->far_jmp_unlinked_opnd));

    return pc;
}

static byte *
emit_ibl_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                  byte *fcache_return_pc, ibl_source_fragment_type_t source_fragment_type,
                  bool thread_shared, bool target_trace_table,
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

    bool inline_ibl_head = (IS_IBL_TRACE(source_fragment_type))
        ? DYNAMO_OPTION(inline_trace_ibl)
        : DYNAMO_OPTION(inline_bb_ibl);

    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
#ifdef HASHTABLE_STATISTICS
        /* ugly asserts but we'll stick with uints to save space */
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
            GET_IBL_TARGET_TABLE(branch_type, target_trace_table) +
            offsetof(ibl_table_t, unprot_stats))));
        ibl_code_routines[branch_type].unprot_stats_offset =
            (uint)GET_IBL_TARGET_TABLE(branch_type, target_trace_table) +
            offsetof(ibl_table_t, unprot_stats);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
            GET_IBL_TARGET_TABLE(branch_type, target_trace_table) +
            offsetof(ibl_table_t, entry_stats_to_lookup_table))));
        ibl_code_routines[branch_type].entry_stats_to_lookup_table_offset =
            (uint)GET_IBL_TARGET_TABLE(branch_type, target_trace_table) +
            offsetof(ibl_table_t, entry_stats_to_lookup_table);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
            offsetof(unprot_ht_statistics_t, trace_ibl_stats[branch_type]))));
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
            offsetof(unprot_ht_statistics_t, bb_ibl_stats[branch_type]))));
        ibl_code_routines[branch_type].hashtable_stats_offset =
            (uint)((IS_IBL_TRACE(source_fragment_type))
                       ? offsetof(unprot_ht_statistics_t, trace_ibl_stats[branch_type])
                       : offsetof(unprot_ht_statistics_t, bb_ibl_stats[branch_type]));
#endif
        pc = emit_ibl_routine_and_template(
            dcontext, code, pc, fcache_return_pc, target_trace_table, inline_ibl_head,
            thread_shared, branch_type, source_fragment_type,
            &ibl_code_routines[branch_type]);
    }
    return pc;
}

static byte *
emit_syscall_routines(dcontext_t *dcontext, generated_code_t *code, byte *pc,
                      bool thread_shared)
{
    /* FIXME i#1551: pass in or store mode in generated_code_t */
    dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext);
#ifdef HASHTABLE_STATISTICS
    /* Stats for the syscall IBLs (note it is also using the trace
     * hashtable, and it never hits!)
     */
#    ifdef WINDOWS
    /* ugly asserts but we'll stick with uints to save space */
    IF_X64(
        ASSERT(CHECK_TRUNCATE_TYPE_uint(GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true) +
                                        offsetof(ibl_table_t, unprot_stats))));
    code->shared_syscall_code.unprot_stats_offset =
        (uint)GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true) +
        offsetof(ibl_table_t, unprot_stats);
    IF_X64(ASSERT(
        CHECK_TRUNCATE_TYPE_uint(GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true) +
                                 offsetof(ibl_table_t, entry_stats_to_lookup_table))));
    code->shared_syscall_code.entry_stats_to_lookup_table_offset =
        (uint)GET_IBL_TARGET_TABLE(IBL_SHARED_SYSCALL, true) +
        offsetof(ibl_table_t, entry_stats_to_lookup_table);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(
        offsetof(unprot_ht_statistics_t, shared_syscall_hit_stats))));
    code->shared_syscall_code.hashtable_stats_offset =
        (uint)offsetof(unprot_ht_statistics_t, shared_syscall_hit_stats);
#    endif /* WINDOWS */
#endif     /* HASHTABLE_STATISTICS */

#ifdef WINDOWS
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_callback_return = pc;
    pc = emit_do_callback_return(dcontext, pc, code->fcache_return, thread_shared);
    if (DYNAMO_OPTION(shared_syscalls)) {

        ibl_code_t *ibl_code;

        if (DYNAMO_OPTION(disable_traces)) {
            ibl_code = DYNAMO_OPTION(shared_bbs)
                ? &SHARED_GENCODE(code->gencode_mode)->bb_ibl[IBL_SHARED_SYSCALL]
                : &code->bb_ibl[IBL_SHARED_SYSCALL];
        } else if (DYNAMO_OPTION(shared_traces)) {
            ibl_code = &SHARED_GENCODE(code->gencode_mode)->trace_ibl[IBL_SHARED_SYSCALL];
        } else {
            ibl_code = &code->trace_ibl[IBL_SHARED_SYSCALL];
        }

        pc = check_size_and_cache_line(isa_mode, code, pc);
        code->unlinked_shared_syscall = pc;
        pc = emit_shared_syscall(
            dcontext, code, pc, &code->shared_syscall_code,
            &code->shared_syscall_code.ibl_patch,
            ibl_code->indirect_branch_lookup_routine, ibl_code->unlinked_ibl_entry,
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
            DYNAMO_OPTION(disable_traces)
                ? DYNAMO_OPTION(inline_bb_ibl)
                : DYNAMO_OPTION(inline_trace_ibl), /* inline_ibl_head */
            ibl_code->thread_shared_routine,       /* thread_shared */
            &code->shared_syscall);
        code->end_shared_syscall = pc;
        /* Lookup at end of shared_syscall should be able to go to bb or trace,
         * unrestricted (will never be an exit from a trace so no secondary trace
         * restrictions) -- currently only traces supported so using the trace_ibl
         * is OK.
         */
    }
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared, 0,
                         &code->do_syscall_offs);
#else /* UNIX */
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared, 0,
                         &code->do_syscall_offs);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_int_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0x80 /*force int*/, &code->do_int_syscall_offs);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_int81_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0x81 /*force int*/, &code->do_int81_syscall_offs);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_int82_syscall = pc;
    pc = emit_do_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                         0x82 /*force int*/, &code->do_int82_syscall_offs);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_clone_syscall = pc;
    pc = emit_do_clone_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                               &code->do_clone_syscall_offs);
#    ifdef VMX86_SERVER
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->do_vmkuw_syscall = pc;
    pc = emit_do_vmkuw_syscall(dcontext, code, pc, code->fcache_return, thread_shared,
                               &code->do_vmkuw_syscall_offs);
#    endif
#endif /* UNIX */

    return pc;
}

void
arch_thread_init(dcontext_t *dcontext)
{
    byte *pc;
    generated_code_t *code;
    ibl_branch_type_t branch_type;
    dr_isa_mode_t isa_mode = dr_get_isa_mode(dcontext);

#ifdef X86
    /* Simplest to have a real dcontext for emitting the selfmod code
     * and finding the patch offsets so we do it on 1st thread init */
    static bool selfmod_init = false;
    if (!selfmod_init) {
        ASSERT(!dynamo_initialized); /* .data +w */
        selfmod_init = true;
        set_selfmod_sandbox_offsets(dcontext);
    }
#endif

    ASSERT_CURIOSITY(proc_is_cache_aligned(get_local_state())
                         IF_WINDOWS(|| DYNAMO_OPTION(tls_align != 0)));

#ifdef WINDOWS
    /* Ensure the swapping is known at init time and never changes. */
    ASSERT(gencode_swaps_teb_tls == should_swap_teb_static_tls());
#endif

#if defined(X86) && defined(X64)
    /* PR 244737: thread-private uses only shared gencode on x64 */
    ASSERT(dcontext->private_code == NULL);
    return;
#endif

#ifdef AARCHXX
    /* Store addresses we access via TLS from exit stubs and gencode. */
    get_local_state_extended()->spill_space.fcache_return =
        PC_AS_JMP_TGT(isa_mode, fcache_return_shared_routine());
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        get_local_state_extended()->spill_space.trace_ibl[branch_type].ibl =
            PC_AS_JMP_TGT(
                isa_mode,
                get_ibl_routine(dcontext, IBL_LINKED, IBL_TRACE_SHARED, branch_type));
        get_local_state_extended()->spill_space.trace_ibl[branch_type].unlinked =
            PC_AS_JMP_TGT(
                isa_mode,
                get_ibl_routine(dcontext, IBL_UNLINKED, IBL_TRACE_SHARED, branch_type));
        get_local_state_extended()->spill_space.bb_ibl[branch_type].ibl = PC_AS_JMP_TGT(
            isa_mode, get_ibl_routine(dcontext, IBL_LINKED, IBL_BB_SHARED, branch_type));
        get_local_state_extended()->spill_space.bb_ibl[branch_type].unlinked =
            PC_AS_JMP_TGT(
                isa_mode,
                get_ibl_routine(dcontext, IBL_UNLINKED, IBL_BB_SHARED, branch_type));
    }
    /* Because absolute addresses are impractical on ARM, thread-private uses
     * only shared gencode, just like for 64-bit.
     */
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
    code =
        heap_mmap_reserve_post_stack(dcontext, GENCODE_RESERVE_SIZE, GENCODE_COMMIT_SIZE,
                                     MEMPROT_EXEC | MEMPROT_READ | MEMPROT_WRITE,
                                     /* We pass VMM_PER_THREAD here, but not on the
                                      * incremental commits: it's only needed on the
                                      * reserve + unreserve.
                                      */
                                     VMM_SPECIAL_MMAP | VMM_REACHABLE | VMM_PER_THREAD);
    ASSERT(code != NULL);
    dcontext->private_code = (void *)code;

    generated_code_t *code_writable =
        (generated_code_t *)vmcode_get_writable_addr((byte *)code);
    /* FIXME case 6493: if we split private from shared, remove this
     * memset since we will no longer have a bunch of fields we don't use
     */
    memset(code_writable, 0, sizeof(*code));
    /* Generated code immediately follows struct */
    code_writable->gen_start_pc = ((byte *)code) + sizeof(*code);
    code_writable->commit_end_pc = ((byte *)code) + GENCODE_COMMIT_SIZE;
    /* Now switch to the writable one.  We assume no further code examines the address
     * of the struct.
     */
    code = code_writable;

    code->thread_shared = false;
    for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
         branch_type++) {
        code->trace_ibl[branch_type].initialized = false;
        code->bb_ibl[branch_type].initialized = false;
        code->coarse_ibl[branch_type].initialized = false;
    }

    pc = code->gen_start_pc;
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->fcache_enter = pc;
    pc = emit_fcache_enter(dcontext, code, pc);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->fcache_return = pc;
    pc = emit_fcache_return(dcontext, code, pc);

    code->fcache_return_end = pc;
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
            ibl_branch_type_t ibl_branch_type;
            for (ibl_branch_type = IBL_BRANCH_TYPE_START;
                 ibl_branch_type < IBL_BRANCH_TYPE_END; ibl_branch_type++) {
                code->trace_ibl[ibl_branch_type] =
                    SHARED_GENCODE(code->gencode_mode)->trace_ibl[ibl_branch_type];
            }
        } /* FIXME: no private traces supported right now w/ -shared_traces */
    } else if (PRIVATE_TRACES_ENABLED()) {
        /* shared_trace_ibl_routine should be false for private (performance test only) */
        pc = emit_ibl_routines(dcontext, code, pc, code->fcache_return,
                               IBL_TRACE_PRIVATE, /* source_fragment_type */
                               DYNAMO_OPTION(shared_trace_ibl_routine), /* shared */
                               true, /* target_trace_table */
                               code->trace_ibl);
    }
    pc = emit_ibl_routines(dcontext, code, pc, code->fcache_return,
                           IBL_BB_PRIVATE, /* source_fragment_type */
                           /* need thread-private for selfmod regardless of sharing */
                           false,                          /* thread_shared */
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
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->new_thread_dynamo_start = pc;
    pc = emit_new_thread_dynamo_start(dcontext, pc);
#endif

#ifdef WINDOWS
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->fcache_enter_indirect = pc;
    pc = emit_fcache_enter_indirect(dcontext, code, pc, code->fcache_return);
#endif
    pc = emit_syscall_routines(dcontext, code, pc, false /*thread-private*/);
#ifdef TRACE_HEAD_CACHE_INCR
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->trace_head_incr = pc;
    pc = emit_trace_head_incr(dcontext, pc, code->fcache_return);
#endif
#ifdef CHECK_RETURNS_SSE2_EMIT
    /* PR 248210: unsupported feature on x64: need to move to thread-shared gencode
     * if want to support it.
     */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->pextrw = pc;
    pc = emit_pextrw(dcontext, pc);
    pc = check_size_and_cache_line(isa_mode, code, pc);
    code->pinsrw = pc;
    pc = emit_pinsrw(dcontext, pc);
#endif
    code->reset_exit_stub = pc;
    /* reset exit stub should look just like a direct exit stub */
    pc += insert_exit_stub_other_flags(
        dcontext, linkstub_fragment(dcontext, (linkstub_t *)get_reset_linkstub()),
        (linkstub_t *)get_reset_linkstub(), pc, LINK_DIRECT);

    if (special_ibl_xfer_is_thread_private()) {
        code->special_ibl_xfer[CLIENT_IBL_IDX] = pc;
        pc = emit_client_ibl_xfer(dcontext, pc, code);
#ifdef UNIX
        /* i#1238: native exec optimization */
        if (DYNAMO_OPTION(native_exec_opt)) {
            pc = check_size_and_cache_line(isa_mode, code, pc);
            code->special_ibl_xfer[NATIVE_PLT_IBL_IDX] = pc;
            pc = emit_native_plt_ibl_xfer(dcontext, pc, code);
            /* native ret */
            pc = check_size_and_cache_line(isa_mode, code, pc);
            code->special_ibl_xfer[NATIVE_RET_IBL_IDX] = pc;
            pc = emit_native_ret_ibl_xfer(dcontext, pc, code);
        }
#endif
    }

    /* XXX: i#1149: we should always use thread shared gencode */
    if (client_clean_call_is_thread_private()) {
        pc = check_size_and_cache_line(isa_mode, code, pc);
        code->clean_call_save = pc;
        pc = emit_clean_call_save(dcontext, pc, code);
        pc = check_size_and_cache_line(isa_mode, code, pc);
        code->clean_call_restore = pc;
        pc = emit_clean_call_restore(dcontext, pc, code);
        code->clean_call_restore_end = pc;
    }

    ASSERT(pc < code->commit_end_pc);
    code->gen_end_pc = pc;
    release_final_page(code);

    DOLOG(3, LOG_EMIT,
          { dump_emitted_routines(dcontext, THREAD, "thread-private", code, pc); });
#ifdef INTERNAL
    if (INTERNAL_OPTION(gendump)) {
        dump_emitted_routines_to_file(dcontext, "gencode-private", "thread-private", code,
                                      pc);
    }
#endif
#ifdef WINDOWS_PC_SAMPLE
    if (dynamo_options.profile_pcs && dynamo_options.prof_pcs_gencode >= 2 &&
        dynamo_options.prof_pcs_gencode <= 32) {
        code->profile =
            create_profile(code->gen_start_pc, pc, dynamo_options.prof_pcs_gencode, NULL);
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
#if defined(X64) || defined(ARM)
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
        heap_munmap_post_stack(dcontext, dcontext->private_code, GENCODE_RESERVE_SIZE,
                               VMM_SPECIAL_MMAP | VMM_REACHABLE | VMM_PER_THREAD);
}

#ifdef WINDOWS
/* Patch syscall routines for detach */
static void
arch_patch_syscall_common(dcontext_t *dcontext, byte *target _IF_X64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X86_64(mode));
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
    volatile generated_code_t *code =
        (generated_code_t *)vmcode_get_writable_addr((byte *)code_in);
    if (TEST(SELFPROT_GENCODE, DYNAMO_OPTION(protect_mask)) &&
        code->writable != writable) {
        byte *genstart = (byte *)PAGE_START(code->gen_start_pc);
        if (!writable) {
            ASSERT(code->writable);
            code->writable = writable;
        }
        STATS_INC(gencode_prot_changes);
        change_protection(vmcode_get_writable_addr(genstart),
                          code->commit_end_pc - genstart, writable);
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
        return (pc == (cache_pc)shared_code->shared_syscall ||
                pc == (cache_pc)shared_code->unlinked_shared_syscall)
            IF_X64(||
                   (shared_code_x86 != NULL &&
                    (pc == (cache_pc)shared_code_x86->shared_syscall ||
                     pc == (cache_pc)shared_code_x86->unlinked_shared_syscall)) ||
                   (shared_code_x86_to_x64 != NULL &&
                    (pc == (cache_pc)shared_code_x86_to_x64->shared_syscall ||
                     pc == (cache_pc)shared_code_x86_to_x64->unlinked_shared_syscall)));
    } else {
        generated_code_t *code = THREAD_GENCODE(dcontext);
        return (code != NULL &&
                (pc == (cache_pc)code->shared_syscall ||
                 pc == (cache_pc)code->unlinked_shared_syscall));
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
    return get_ibl_routine_type_ex(dcontext, pc, NULL _IF_X86_64(NULL));
}

/* Promotes the current ibl routine from IBL_BB* to IBL_TRACE*
 * preserving other properties.  There seems to be no need for the
 * opposite transformation.
 */
cache_pc
get_trace_ibl_routine(dcontext_t *dcontext, cache_pc current_entry)
{
    ibl_type_t ibl_type = { 0 };
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type(dcontext, current_entry, &ibl_type);

    ASSERT(is_ibl);
    ASSERT(IS_IBL_BB(ibl_type.source_fragment_type));

    return
#ifdef WINDOWS
        DYNAMO_OPTION(shared_syscalls) &&
            is_shared_syscall_routine(dcontext, current_entry)
        ? current_entry
        :
#endif
        get_ibl_routine(dcontext, ibl_type.link_state,
                        (ibl_type.source_fragment_type == IBL_BB_PRIVATE)
                            ? IBL_TRACE_PRIVATE
                            : IBL_TRACE_SHARED,
                        ibl_type.branch_type);
}

/* Shifts the current ibl routine from IBL_BB_SHARED to IBL_BB_PRIVATE,
 * preserving other properties.
 * There seems to be no need for the opposite transformation
 */
cache_pc
get_private_ibl_routine(dcontext_t *dcontext, cache_pc current_entry)
{
    ibl_type_t ibl_type = { 0 };
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type(dcontext, current_entry, &ibl_type);

    ASSERT(is_ibl);
    ASSERT(IS_IBL_BB(ibl_type.source_fragment_type));

    return get_ibl_routine(dcontext, ibl_type.link_state, IBL_BB_PRIVATE,
                           ibl_type.branch_type);
}

/* Shifts the current ibl routine from IBL_BB_PRIVATE to IBL_BB_SHARED,
 * preserving other properties.
 * There seems to be no need for the opposite transformation
 */
cache_pc
get_shared_ibl_routine(dcontext_t *dcontext, cache_pc current_entry)
{
    ibl_type_t ibl_type = { 0 };
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type(dcontext, current_entry, &ibl_type);

    ASSERT(is_ibl);
    ASSERT(IS_IBL_BB(ibl_type.source_fragment_type));

    return get_ibl_routine(dcontext, ibl_type.link_state, IBL_BB_SHARED,
                           ibl_type.branch_type);
}

/* gets the corresponding routine to current_entry but matching whether
 * FRAG_IS_TRACE and FRAG_SHARED are set in flags
 */
cache_pc
get_alternate_ibl_routine(dcontext_t *dcontext, cache_pc current_entry, uint flags)
{
    ibl_type_t ibl_type = { 0 };
    IF_X86_64(gencode_mode_t mode = GENCODE_FROM_DCONTEXT;)
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type_ex(dcontext, current_entry, &ibl_type _IF_X86_64(&mode));
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
                              ibl_type.branch_type _IF_X86_64(mode));
}

static ibl_entry_point_type_t
get_unlinked_type(ibl_entry_point_type_t link_state)
{
#if defined(X86) && defined(X64)
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
#if defined(X86) && defined(X64)
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
    ibl_type_t ibl_type = { 0 };
    IF_X86_64(gencode_mode_t mode = GENCODE_FROM_DCONTEXT;)
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type_ex(dcontext, unlinked_entry, &ibl_type _IF_X86_64(&mode));
    ASSERT(is_ibl && IS_IBL_UNLINKED(ibl_type.link_state));

#ifdef WINDOWS
    if (unlinked_entry == unlinked_shared_syscall_routine_ex(dcontext _IF_X86_64(mode))) {
        return shared_syscall_routine_ex(dcontext _IF_X86_64(mode));
    }
#endif

    return get_ibl_routine_ex(dcontext,
                              /* for -unsafe_ignore_eflags_{ibl,trace} the trace cmp
                               * entry and unlink are both identical, so we may mix
                               * them up but will have no problems */
                              get_linked_type(ibl_type.link_state),
                              ibl_type.source_fragment_type,
                              ibl_type.branch_type _IF_X86_64(mode));
}

#if defined(X86) && defined(X64)
cache_pc
get_trace_cmp_entry(dcontext_t *dcontext, cache_pc linked_entry)
{
    ibl_type_t ibl_type = { 0 };
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type(dcontext, linked_entry, &ibl_type);
    IF_WINDOWS(ASSERT(linked_entry != shared_syscall_routine(dcontext)));
    ASSERT(is_ibl && ibl_type.link_state == IBL_LINKED);
    return get_ibl_routine(dcontext, IBL_TRACE_CMP, ibl_type.source_fragment_type,
                           ibl_type.branch_type);
}
#endif

cache_pc
get_unlinked_entry(dcontext_t *dcontext, cache_pc linked_entry)
{
    ibl_type_t ibl_type = { 0 };
    IF_X86_64(gencode_mode_t mode = GENCODE_FROM_DCONTEXT;)
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type_ex(dcontext, linked_entry, &ibl_type _IF_X86_64(&mode));
    ASSERT(is_ibl && IS_IBL_LINKED(ibl_type.link_state));

#ifdef WINDOWS
    if (linked_entry == shared_syscall_routine_ex(dcontext _IF_X86_64(mode)))
        return unlinked_shared_syscall_routine_ex(dcontext _IF_X86_64(mode));
#endif
    return get_ibl_routine_ex(dcontext, get_unlinked_type(ibl_type.link_state),
                              ibl_type.source_fragment_type,
                              ibl_type.branch_type _IF_X86_64(mode));
}

static bool
in_generated_shared_routine(dcontext_t *dcontext, cache_pc pc)
{
    if (USE_SHARED_GENCODE()) {
        return (pc >= (cache_pc)(shared_code->gen_start_pc) &&
                pc < (cache_pc)(shared_code->commit_end_pc))
            IF_X86_64(||
                      (shared_code_x86 != NULL &&
                       pc >= (cache_pc)(shared_code_x86->gen_start_pc) &&
                       pc < (cache_pc)(shared_code_x86->commit_end_pc)) ||
                      (shared_code_x86_to_x64 != NULL &&
                       pc >= (cache_pc)(shared_code_x86_to_x64->gen_start_pc) &&
                       pc < (cache_pc)(shared_code_x86_to_x64->commit_end_pc)));
    }
    return false;
}

bool
in_generated_routine(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);

    return (
        (pc >= (cache_pc)(code->gen_start_pc) && pc < (cache_pc)(code->commit_end_pc)) ||
        in_generated_shared_routine(dcontext, pc));
    /* FIXME: what about inlined IBL stubs */
}

static bool
in_fcache_return_for_gencode(generated_code_t *code, cache_pc pc)
{
    return pc != NULL &&
        ((pc >= code->fcache_return && pc < code->fcache_return_end) ||
         (pc >= code->fcache_return_coarse && pc < code->fcache_return_coarse_end));
}

bool
in_fcache_return(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    if (in_fcache_return_for_gencode(code, pc))
        return true;
    if (USE_SHARED_GENCODE()) {
        if (in_fcache_return_for_gencode(shared_code, pc))
            return true;
#if defined(X86) && defined(X64)
        if (shared_code_x86 != NULL && in_fcache_return_for_gencode(shared_code_x86, pc))
            return true;
        if (shared_code_x86_to_x64 != NULL &&
            in_fcache_return_for_gencode(shared_code_x86_to_x64, pc))
            return true;
#endif
    }
    return false;
}

static bool
in_clean_call_save_for_gencode(generated_code_t *code, cache_pc pc)
{
    return pc != NULL && pc >= code->clean_call_save && pc < code->clean_call_restore;
}

static bool
in_clean_call_restore_for_gencode(generated_code_t *code, cache_pc pc)
{
    return pc != NULL && pc >= code->clean_call_restore &&
        pc < code->clean_call_restore_end;
}

bool
in_clean_call_save(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    if (in_clean_call_save_for_gencode(code, pc))
        return true;
    if (USE_SHARED_GENCODE()) {
        if (in_clean_call_save_for_gencode(shared_code, pc))
            return true;
#if defined(X86) && defined(X64)
        if (shared_code_x86 != NULL &&
            in_clean_call_save_for_gencode(shared_code_x86, pc))
            return true;
        if (shared_code_x86_to_x64 != NULL &&
            in_clean_call_save_for_gencode(shared_code_x86_to_x64, pc))
            return true;
#endif
    }
    return false;
}

bool
in_clean_call_restore(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    if (in_clean_call_restore_for_gencode(code, pc))
        return true;
    if (USE_SHARED_GENCODE()) {
        if (in_clean_call_restore_for_gencode(shared_code, pc))
            return true;
#if defined(X86) && defined(X64)
        if (shared_code_x86 != NULL &&
            in_clean_call_restore_for_gencode(shared_code_x86, pc))
            return true;
        if (shared_code_x86_to_x64 != NULL &&
            in_clean_call_restore_for_gencode(shared_code_x86_to_x64, pc))
            return true;
#endif
    }
    return false;
}

bool
in_indirect_branch_lookup_code(dcontext_t *dcontext, cache_pc pc)
{
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;

    for (source_fragment_type = IBL_SOURCE_TYPE_START;
         source_fragment_type < IBL_SOURCE_TYPE_END; source_fragment_type++) {
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
            if (pc >= get_ibl_routine(dcontext, IBL_LINKED, source_fragment_type,
                                      branch_type) &&
                pc < get_ibl_routine(dcontext, IBL_UNLINKED, source_fragment_type,
                                     branch_type))
                return true;
        }
    }
    return false; /* not an IBL */
    /* FIXME: what about inlined IBL stubs */
}

fcache_enter_func_t
fcache_enter_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (fcache_enter_func_t)convert_data_to_function(code->fcache_enter);
}

/* exported to dispatch.c */
fcache_enter_func_t
get_fcache_enter_private_routine(dcontext_t *dcontext)
{
    return fcache_enter_routine(dcontext);
}

fcache_enter_func_t
get_fcache_enter_gonative_routine(dcontext_t *dcontext)
{
#ifdef AARCHXX
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (fcache_enter_func_t)convert_data_to_function(code->fcache_enter_gonative);
#else
    return fcache_enter_routine(dcontext);
#endif
}

cache_pc
get_reset_exit_stub(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->reset_exit_stub;
}

cache_pc
get_do_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_syscall;
}

#ifdef WINDOWS
fcache_enter_func_t
get_fcache_enter_indirect_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (fcache_enter_func_t)convert_data_to_function(code->fcache_enter_indirect);
}
cache_pc
get_do_callback_return_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_callback_return;
}
#else
/* PR 286922: we need an int syscall even when vsyscall is sys{call,enter} */
cache_pc
get_do_int_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_int_syscall;
}

cache_pc
get_do_int81_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_int81_syscall;
}

cache_pc
get_do_int82_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_int82_syscall;
}

cache_pc
get_do_clone_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_clone_syscall;
}
#    ifdef VMX86_SERVER
cache_pc
get_do_vmkuw_syscall_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->do_vmkuw_syscall;
}
#    endif
#endif

cache_pc
fcache_return_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->fcache_return;
}

cache_pc
fcache_return_routine_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    return (cache_pc)code->fcache_return;
}

cache_pc
fcache_return_coarse_routine(IF_X86_64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X86_64(mode));
    ASSERT(DYNAMO_OPTION(coarse_units));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc)code->fcache_return_coarse;
}

cache_pc
trace_head_return_coarse_routine(IF_X86_64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X86_64(mode));
    ASSERT(DYNAMO_OPTION(coarse_units));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc)code->trace_head_return_coarse;
}

cache_pc
get_clean_call_save(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code;
    if (client_clean_call_is_thread_private())
        code = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    else
        code = get_emitted_routines_code(GLOBAL_DCONTEXT _IF_X86_64(mode));
    ASSERT(code != NULL);
    /* FIXME i#1551: NYI on ARM (we need emit_clean_call_save()) */
    IF_ARM(ASSERT_NOT_IMPLEMENTED(false));
    return (cache_pc)code->clean_call_save;
}

cache_pc
get_clean_call_restore(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code;
    if (client_clean_call_is_thread_private())
        code = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    else
        code = get_emitted_routines_code(GLOBAL_DCONTEXT _IF_X86_64(mode));
    ASSERT(code != NULL);
    /* FIXME i#1551: NYI on ARM (we need emit_clean_call_restore()) */
    IF_ARM(ASSERT_NOT_IMPLEMENTED(false));
    return (cache_pc)code->clean_call_restore;
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

cache_pc
get_client_ibl_xfer_entry(dcontext_t *dcontext)
{
    return get_special_ibl_xfer_entry(dcontext, CLIENT_IBL_IDX);
}

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
get_ibl_routine_type_ex(dcontext_t *dcontext, cache_pc target,
                        ibl_type_t *type _IF_X86_64(gencode_mode_t *mode_out))
{
    /* This variable is int instead of ibl_entry_point_type_t.  This is because
     * below we use it as loop index variable which can take negative values.
     * It is possible that ibl_entry_point_type_t, which is an enum, has an
     * underlying unsigned type which can cause problems due to wrap around.
     */
    int link_state;
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;
#if defined(X86) && defined(X64)
    gencode_mode_t mode;
#endif

    /* An up-front range check. Many calls into this routine are with addresses
     * outside of the IBL code or the generated_code_t in which IBL resides.
     * For all of those cases, this quick up-front check saves the expense of
     * examining all of the different IBL entry points.
     */
    if ((shared_code == NULL || target < shared_code->gen_start_pc ||
         target >= shared_code->gen_end_pc)
            IF_X86_64(&&(shared_code_x86 == NULL ||
                         target < shared_code_x86->gen_start_pc ||
                         target >= shared_code_x86->gen_end_pc) &&
                      (shared_code_x86_to_x64 == NULL ||
                       target < shared_code_x86_to_x64->gen_start_pc ||
                       target >= shared_code_x86_to_x64->gen_end_pc))) {
        if (dcontext == GLOBAL_DCONTEXT || USE_SHARED_GENCODE_ALWAYS() ||
            target < ((generated_code_t *)dcontext->private_code)->gen_start_pc ||
            target >= ((generated_code_t *)dcontext->private_code)->gen_end_pc)
            return false;
    }

    /* a decent compiler should inline these nested loops */
    /* iterate in order <linked, unlinked> */
    for (link_state = IBL_LINKED;
         /* keep in mind we need a signed comparison when going downwards */
         link_state >= (int)IBL_UNLINKED; link_state--) {
        /* it is OK to compare to IBL_BB_PRIVATE even when !SHARED_FRAGMENTS_ENABLED() */
        for (source_fragment_type = IBL_SOURCE_TYPE_START;
             source_fragment_type < IBL_SOURCE_TYPE_END; source_fragment_type++) {
            for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
                 branch_type++) {
#if defined(X86) && defined(X64)
                for (mode = GENCODE_X64; mode <= GENCODE_X86_TO_X64; mode++) {
#endif
                    if (target ==
                        get_ibl_routine_ex(dcontext, link_state, source_fragment_type,
                                           branch_type _IF_X86_64(mode))) {
                        if (type) {
                            type->link_state = link_state;
                            type->source_fragment_type = source_fragment_type;
                            type->branch_type = branch_type;
                        }
#if defined(X86) && defined(X64)
                        if (mode_out != NULL)
                            *mode_out = mode;
#endif
                        return true;
                    }
#if defined(X86) && defined(X64)
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
#    if defined(X86) && defined(X64)
            for (mode = GENCODE_X64; mode <= GENCODE_X86_TO_X64; mode++) {
#    endif
                if (target ==
                    unlinked_shared_syscall_routine_ex(dcontext _IF_X86_64(mode)))
                    type->link_state = IBL_UNLINKED;
                else
                    IF_X64(if (target ==
                               shared_syscall_routine_ex(dcontext _IF_X86_64(mode))))
                type->link_state = IBL_LINKED;
#    if defined(X86) && defined(X64)
                else continue;
                if (mode_out != NULL)
                    *mode_out = mode;
                break;
            }
#    endif
        }
        return true;
    }
#endif

    return false; /* not an IBL */
}

bool
get_ibl_routine_type(dcontext_t *dcontext, cache_pc target, ibl_type_t *type)
{
    IF_X64(ASSERT(dcontext != GLOBAL_DCONTEXT)); /* should call get_ibl_routine_type_ex */
    return get_ibl_routine_type_ex(dcontext, target, type _IF_X86_64(NULL));
}

/* returns false if target is not an IBL template
   if type is not NULL it is set to the type of the found routine
*/
static bool
get_ibl_routine_template_type(dcontext_t *dcontext, cache_pc target,
                              ibl_type_t *type _IF_X86_64(gencode_mode_t *mode_out))
{
    ibl_source_fragment_type_t source_fragment_type;
    ibl_branch_type_t branch_type;
#if defined(X86) && defined(X64)
    gencode_mode_t mode;
#endif

    for (source_fragment_type = IBL_SOURCE_TYPE_START;
         source_fragment_type < IBL_SOURCE_TYPE_END; source_fragment_type++) {
        for (branch_type = IBL_BRANCH_TYPE_START; branch_type < IBL_BRANCH_TYPE_END;
             branch_type++) {
#if defined(X86) && defined(X64)
            for (mode = GENCODE_X64; mode <= GENCODE_X86_TO_X64; mode++) {
#endif
                if (target ==
                    get_ibl_routine_template(dcontext, source_fragment_type,
                                             branch_type _IF_X86_64(mode))) {
                    if (type) {
                        type->link_state = IBL_TEMPLATE;
                        type->source_fragment_type = source_fragment_type;
                        type->branch_type = branch_type;
#if defined(X86) && defined(X64)
                        if (mode_out != NULL)
                            *mode_out = mode;
#endif
                    }
                    return true;
#if defined(X86) && defined(X64)
                }
#endif
            }
        }
    }
    return false; /* not an IBL template */
}

const char *
get_branch_type_name(ibl_branch_type_t branch_type)
{
    static const char *const ibl_brtype_names[IBL_BRANCH_TYPE_END] = { "ret", "indcall",
                                                                       "indjmp" };
    return ibl_brtype_names[branch_type];
}

ibl_branch_type_t
get_ibl_branch_type(instr_t *instr)
{
    ASSERT(instr_is_mbr(instr) IF_X86(|| instr_get_opcode(instr) == OP_jmp_far ||
                                      instr_get_opcode(instr) == OP_call_far));

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
#if defined(X86) && defined(X64)
    static const char
        *const ibl_routine_names[3][IBL_SOURCE_TYPE_END][IBL_LINK_STATE_END] = {
            {
                { "shared_unlinked_bb_ibl", "shared_delete_bb_ibl", "shared_bb_far",
                  "shared_bb_far_unlinked", "shared_bb_cmp", "shared_bb_cmp_unlinked",
                  "shared_bb_ibl", "shared_bb_ibl_template" },
                { "shared_unlinked_trace_ibl", "shared_delete_trace_ibl",
                  "shared_trace_far", "shared_trace_far_unlinked", "shared_trace_cmp",
                  "shared_trace_cmp_unlinked", "shared_trace_ibl",
                  "shared_trace_ibl_template" },
                { "private_unlinked_bb_ibl", "private_delete_bb_ibl", "private_bb_far",
                  "private_bb_far_unlinked", "private_bb_cmp", "private_bb_cmp_unlinked",
                  "private_bb_ibl", "private_bb_ibl_template" },
                { "private_unlinked_trace_ibl", "private_delete_trace_ibl",
                  "private_trace_far", "private_trace_far_unlinked", "private_trace_cmp",
                  "private_trace_cmp_unlinked", "private_trace_ibl",
                  "private_trace_ibl_template" },
                { "shared_unlinked_coarse_ibl", "shared_delete_coarse_ibl",
                  "shared_coarse_trace_far", "shared_coarse_trace_far_unlinked",
                  "shared_coarse_trace_cmp", "shared_coarse_trace_cmp_unlinked",
                  "shared_coarse_ibl", "shared_coarse_ibl_template" },
                /* PR 282576: for WOW64 processes we have separate x86 routines */
            },
            {
                { "x86_shared_unlinked_bb_ibl", "x86_shared_delete_bb_ibl",
                  "x86_shared_bb_far", "x86_shared_bb_far_unlinked",
                  IF_X64_("x86_shared_bb_cmp")
                      IF_X64_("x86_shared_bb_cmp_unlinked") "x86_shared_bb_ibl",
                  "x86_shared_bb_ibl_template" },
                { "x86_shared_unlinked_trace_ibl", "x86_shared_delete_trace_ibl",
                  "x86_shared_trace_far", "x86_shared_trace_far_unlinked",
                  IF_X64_("x86_shared_trace_cmp")
                      IF_X64_("x86_shared_trace_cmp_unlinked") "x86_shared_trace_ibl",
                  "x86_shared_trace_ibl_template" },
                { "x86_private_unlinked_bb_ibl", "x86_private_delete_bb_ibl",
                  "x86_private_bb_far", "x86_private_bb_far_unlinked",
                  IF_X64_("x86_private_bb_cmp")
                      IF_X64_("x86_private_bb_cmp_unlinked") "x86_private_bb_ibl",
                  "x86_private_bb_ibl_template" },
                { "x86_private_unlinked_trace_ibl", "x86_private_delete_trace_ibl",
                  "x86_private_trace_far", "x86_private_trace_far_unlinked",
                  IF_X64_("x86_private_trace_cmp")
                      IF_X64_("x86_private_trace_cmp_unlinked") "x86_private_trace_ibl",
                  "x86_private_trace_ibl_template" },
                { "x86_shared_unlinked_coarse_ibl", "x86_shared_delete_coarse_ibl",
                  "x86_shared_coarse_trace_far", "x86_shared_coarse_trace_far_unlinked",
                  IF_X64_("x86_shared_coarse_trace_cmp") IF_X64_(
                      "x86_shared_coarse_trace_cmp_unlinked") "x86_shared_coarse_ibl",
                  "x86_shared_coarse_ibl_template" },
            },
            {
                { "x86_to_x64_shared_unlinked_bb_ibl", "x86_to_x64_shared_delete_bb_ibl",
                  "x86_to_x64_shared_bb_far", "x86_to_x64_shared_bb_far_unlinked",
                  "x86_to_x64_shared_bb_cmp", "x86_to_x64_shared_bb_cmp_unlinked",
                  "x86_to_x64_shared_bb_ibl", "x86_to_x64_shared_bb_ibl_template" },
                { "x86_to_x64_shared_unlinked_trace_ibl",
                  "x86_to_x64_shared_delete_trace_ibl", "x86_to_x64_shared_trace_far",
                  "x86_to_x64_shared_trace_far_unlinked", "x86_to_x64_shared_trace_cmp",
                  "x86_to_x64_shared_trace_cmp_unlinked", "x86_to_x64_shared_trace_ibl",
                  "x86_to_x64_shared_trace_ibl_template" },
                { "x86_to_x64_private_unlinked_bb_ibl",
                  "x86_to_x64_private_delete_bb_ibl", "x86_to_x64_private_bb_far",
                  "x86_to_x64_private_bb_far_unlinked", "x86_to_x64_private_bb_cmp",
                  "x86_to_x64_private_bb_cmp_unlinked",
                  /* clang-format off */
              "x86_to_x64_private_bb_ibl",
              "x86_to_x64_private_bb_ibl_template" },
                /* clang-format on */
                { "x86_to_x64_private_unlinked_trace_ibl",
                  "x86_to_x64_private_delete_trace_ibl", "x86_to_x64_private_trace_far",
                  "x86_to_x64_private_trace_far_unlinked", "x86_to_x64_private_trace_cmp",
                  "x86_to_x64_private_trace_cmp_unlinked", "x86_to_x64_private_trace_ibl",
                  "x86_to_x64_private_trace_ibl_template" },
                { "x86_to_x64_shared_unlinked_coarse_ibl",
                  "x86_to_x64_shared_delete_coarse_ibl",
                  "x86_to_x64_shared_coarse_trace_far",
                  "x86_to_x64_shared_coarse_trace_far_unlinked",
                  "x86_to_x64_shared_coarse_trace_cmp",
                  "x86_to_x64_shared_coarse_trace_cmp_unlinked",
                  "x86_to_x64_shared_coarse_ibl",
                  "x86_to_x64_shared_coarse_ibl_template" },
            }
        };
#else
    static const char
        *const ibl_routine_names[IBL_SOURCE_TYPE_END][IBL_LINK_STATE_END] = {
            { "shared_unlinked_bb_ibl", "shared_delete_bb_ibl", "shared_bb_far",
              "shared_bb_far_unlinked", "shared_bb_ibl", "shared_bb_ibl_template" },
            { "shared_unlinked_trace_ibl", "shared_delete_trace_ibl", "shared_trace_far",
              "shared_trace_far_unlinked", "shared_trace_ibl",
              "shared_trace_ibl_template" },
            { "private_unlinked_bb_ibl", "private_delete_bb_ibl", "private_bb_far",
              "private_bb_far_unlinked", "private_bb_ibl", "private_bb_ibl_template" },
            { "private_unlinked_trace_ibl", "private_delete_trace_ibl",
              "private_trace_far", "private_trace_far_unlinked", "private_trace_ibl",
              "private_trace_ibl_template" },
            { "shared_unlinked_coarse_ibl", "shared_delete_coarse_ibl",
              "shared_coarse_trace_far", "shared_coarse_trace_far_unlinked",
              "shared_coarse_ibl", "shared_coarse_ibl_template" },
        };
#endif
    ibl_type_t ibl_type;
#if defined(X86) && defined(X64)
    gencode_mode_t mode;
#endif
    if (!get_ibl_routine_type_ex(dcontext, target, &ibl_type _IF_X86_64(&mode))) {
        /* not an IBL routine */
        if (!get_ibl_routine_template_type(dcontext, target,
                                           &ibl_type _IF_X86_64(&mode))) {
            return NULL; /* not an IBL template either */
        }
    }
    /* ibl_type is valid and will give routine or template name, and qualifier */

    *ibl_brtype_name = get_branch_type_name(ibl_type.branch_type);
    return ibl_routine_names IF_X86_64([mode])[ibl_type.source_fragment_type]
                                              [ibl_type.link_state];
}

static inline ibl_code_t *
get_ibl_routine_code_internal(
    dcontext_t *dcontext, ibl_source_fragment_type_t source_fragment_type,
    ibl_branch_type_t branch_type _IF_X86_64(gencode_mode_t mode))
{
#if defined(X86) && defined(X64)
    if (((mode == GENCODE_X86 ||
          (mode == GENCODE_FROM_DCONTEXT && dcontext != GLOBAL_DCONTEXT &&
           dcontext->isa_mode == DR_ISA_IA32 && !X64_CACHE_MODE_DC(dcontext))) &&
         shared_code_x86 == NULL) ||
        ((mode == GENCODE_X86_TO_X64 ||
          (mode == GENCODE_FROM_DCONTEXT && dcontext != GLOBAL_DCONTEXT &&
           dcontext->isa_mode == DR_ISA_IA32 && X64_CACHE_MODE_DC(dcontext))) &&
         shared_code_x86_to_x64 == NULL))
        return NULL;
#endif
    switch (source_fragment_type) {
    case IBL_BB_SHARED:
        if (!USE_SHARED_BB_IBL())
            return NULL;
        return &(get_shared_gencode(dcontext _IF_X86_64(mode))->bb_ibl[branch_type]);
    case IBL_BB_PRIVATE:
        return &(
            get_emitted_routines_code(dcontext _IF_X86_64(mode))->bb_ibl[branch_type]);
    case IBL_TRACE_SHARED:
        if (!USE_SHARED_TRACE_IBL())
            return NULL;
        return &(get_shared_gencode(dcontext _IF_X86_64(mode))->trace_ibl[branch_type]);
    case IBL_TRACE_PRIVATE:
        return &(
            get_emitted_routines_code(dcontext _IF_X86_64(mode))->trace_ibl[branch_type]);
    case IBL_COARSE_SHARED:
        if (!DYNAMO_OPTION(coarse_units))
            return NULL;
        return &(get_shared_gencode(dcontext _IF_X86_64(mode))->coarse_ibl[branch_type]);
    default: ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return NULL;
}

cache_pc
get_ibl_routine_ex(dcontext_t *dcontext, ibl_entry_point_type_t entry_type,
                   ibl_source_fragment_type_t source_fragment_type,
                   ibl_branch_type_t branch_type _IF_X86_64(gencode_mode_t mode))
{
    ibl_code_t *ibl_code = get_ibl_routine_code_internal(dcontext, source_fragment_type,
                                                         branch_type _IF_X86_64(mode));
    if (ibl_code == NULL || !ibl_code->initialized)
        return NULL;
    switch (entry_type) {
    case IBL_LINKED: return (cache_pc)ibl_code->indirect_branch_lookup_routine;
    case IBL_UNLINKED: return (cache_pc)ibl_code->unlinked_ibl_entry;
    case IBL_DELETE: return (cache_pc)ibl_code->target_delete_entry;
    case IBL_FAR: return (cache_pc)ibl_code->far_ibl;
    case IBL_FAR_UNLINKED: return (cache_pc)ibl_code->far_ibl_unlinked;
#if defined(X86) && defined(X64)
    case IBL_TRACE_CMP: return (cache_pc)ibl_code->trace_cmp_entry;
    case IBL_TRACE_CMP_UNLINKED: return (cache_pc)ibl_code->trace_cmp_unlinked;
#endif
    default: ASSERT_NOT_REACHED();
    }
    return NULL;
}

cache_pc
get_ibl_routine(dcontext_t *dcontext, ibl_entry_point_type_t entry_type,
                ibl_source_fragment_type_t source_fragment_type,
                ibl_branch_type_t branch_type)
{
    return get_ibl_routine_ex(dcontext, entry_type, source_fragment_type,
                              branch_type _IF_X86_64(GENCODE_FROM_DCONTEXT));
}

cache_pc
get_ibl_routine_template(dcontext_t *dcontext,
                         ibl_source_fragment_type_t source_fragment_type,
                         ibl_branch_type_t branch_type _IF_X86_64(gencode_mode_t mode))
{
    ibl_code_t *ibl_code = get_ibl_routine_code_internal(dcontext, source_fragment_type,
                                                         branch_type _IF_X86_64(mode));
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
    ASSERT_NOT_IMPLEMENTED(!TESTANY(
        ~(FRAG_TABLE_INCLUSIVE_HIERARCHY | FRAG_TABLE_IBL_TARGETED |
          FRAG_TABLE_TARGET_SHARED | FRAG_TABLE_SHARED | FRAG_TABLE_TRACE |
          FRAG_TABLE_PERSISTENT | HASHTABLE_USE_ENTRY_STATS | HASHTABLE_ALIGN_TABLE),
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

    return (cache_pc)get_ibl_routine(dcontext, IBL_DELETE,
                                     get_source_fragment_type(dcontext, frag_flags),
                                     table->branch_type);
}

ibl_code_t *
get_ibl_routine_code_ex(dcontext_t *dcontext, ibl_branch_type_t branch_type,
                        uint fragment_flags _IF_X86_64(gencode_mode_t mode))
{
    ibl_source_fragment_type_t source_fragment_type =
        get_source_fragment_type(dcontext, fragment_flags);

    ibl_code_t *ibl_code = get_ibl_routine_code_internal(dcontext, source_fragment_type,
                                                         branch_type _IF_X86_64(mode));
    ASSERT(ibl_code != NULL);
    return ibl_code;
}

ibl_code_t *
get_ibl_routine_code(dcontext_t *dcontext, ibl_branch_type_t branch_type,
                     uint fragment_flags)
{
    return get_ibl_routine_code_ex(
        dcontext, branch_type,
        fragment_flags _IF_X86_64(dcontext == GLOBAL_DCONTEXT
                                      ? FRAGMENT_GENCODE_MODE(fragment_flags)
                                      : GENCODE_FROM_DCONTEXT));
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
shared_syscall_routine_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code = DYNAMO_OPTION(shared_fragment_shared_syscalls)
        ? get_shared_gencode(dcontext _IF_X86_64(mode))
        : get_emitted_routines_code(dcontext _IF_X86_64(mode));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc)code->shared_syscall;
}

cache_pc
shared_syscall_routine(dcontext_t *dcontext)
{
    return shared_syscall_routine_ex(dcontext _IF_X64(GENCODE_FROM_DCONTEXT));
}

cache_pc
unlinked_shared_syscall_routine_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code = DYNAMO_OPTION(shared_fragment_shared_syscalls)
        ? get_shared_gencode(dcontext _IF_X86_64(mode))
        : get_emitted_routines_code(dcontext _IF_X86_64(mode));
    if (code == NULL)
        return NULL;
    else
        return (cache_pc)code->unlinked_shared_syscall;
}

cache_pc
unlinked_shared_syscall_routine(dcontext_t *dcontext)
{
    return unlinked_shared_syscall_routine_ex(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
}

cache_pc
after_shared_syscall_code(dcontext_t *dcontext)
{
    return after_shared_syscall_code_ex(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
}

cache_pc
after_shared_syscall_code_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    ASSERT(code != NULL);
    return (cache_pc)(code->unlinked_shared_syscall + code->sys_syscall_offs);
}

cache_pc
after_shared_syscall_addr(dcontext_t *dcontext)
{
    ASSERT(get_syscall_method() != SYSCALL_METHOD_UNINITIALIZED);
    if (DYNAMO_OPTION(sygate_int) && get_syscall_method() == SYSCALL_METHOD_INT)
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
    return after_do_syscall_code_ex(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
}

cache_pc
after_do_syscall_code_ex(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
    generated_code_t *code = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    ASSERT(code != NULL);
    return (cache_pc)(code->do_syscall + code->do_syscall_offs);
}

cache_pc
after_do_syscall_addr(dcontext_t *dcontext)
{
    ASSERT(get_syscall_method() != SYSCALL_METHOD_UNINITIALIZED);
    if (DYNAMO_OPTION(sygate_int) && get_syscall_method() == SYSCALL_METHOD_INT)
        return (int_syscall_address + INT_LENGTH /* sizeof int 2e */);
    else
        return after_do_syscall_code(dcontext);
}
#else
cache_pc
after_do_shared_syscall_addr(dcontext_t *dcontext)
{
    /* PR 212570: return the thread-shared do_syscall used for vsyscall hook */
    generated_code_t *code =
        get_emitted_routines_code(GLOBAL_DCONTEXT _IF_X86_64(GENCODE_X64));
    IF_X86_64(ASSERT_NOT_REACHED()); /* else have to worry about GENCODE_X86 */
    ASSERT(code != NULL);
    ASSERT(code->do_syscall != NULL);
    return (cache_pc)(code->do_syscall + code->do_syscall_offs);
}

cache_pc
after_do_syscall_addr(dcontext_t *dcontext)
{
    /* PR 212570: return the thread-shared do_syscall used for vsyscall hook */
    generated_code_t *code =
        get_emitted_routines_code(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    ASSERT(code->do_syscall != NULL);
    return (cache_pc)(code->do_syscall + code->do_syscall_offs);
}

bool
is_after_main_do_syscall_addr(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code =
        get_emitted_routines_code(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    return (pc == (cache_pc)(code->do_syscall + code->do_syscall_offs));
}

bool
is_after_do_syscall_addr(dcontext_t *dcontext, cache_pc pc)
{
    generated_code_t *code =
        get_emitted_routines_code(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    return (
        pc == (cache_pc)(code->do_syscall + code->do_syscall_offs) ||
        pc ==
            (cache_pc)(code->do_int_syscall + code->do_int_syscall_offs) IF_VMX86(
                ||
                pc == (cache_pc)(code->do_vmkuw_syscall + code->do_vmkuw_syscall_offs)));
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
bool
is_after_syscall_that_rets(dcontext_t *dcontext, cache_pc pc)
{
#ifdef WINDOWS
    return (is_after_syscall_address(dcontext, pc) && does_syscall_ret_to_callsite());
#else
    generated_code_t *code =
        get_emitted_routines_code(dcontext _IF_X86_64(GENCODE_FROM_DCONTEXT));
    ASSERT(code != NULL);
    return (
        (pc == (cache_pc)(code->do_syscall + code->do_syscall_offs) &&
         does_syscall_ret_to_callsite()) ||
        pc ==
            (cache_pc)(code->do_int_syscall + code->do_int_syscall_offs) IF_VMX86(
                ||
                pc == (cache_pc)(code->do_vmkuw_syscall + code->do_vmkuw_syscall_offs)));
#endif
}

#ifdef UNIX
/* PR 212290: can't be static code in x86.asm since it can't be PIC */
cache_pc
get_new_thread_start(dcontext_t *dcontext _IF_X86_64(gencode_mode_t mode))
{
#    ifdef HAVE_TLS
    /* for HAVE_TLS we use the shared version; w/o TLS we don't
     * make any shared routines (PR 361894)
     */
    dcontext = GLOBAL_DCONTEXT;
#    endif
    generated_code_t *gen = get_emitted_routines_code(dcontext _IF_X86_64(mode));
    return gen->new_thread_dynamo_start;
}
#endif

#ifdef TRACE_HEAD_CACHE_INCR
cache_pc
trace_head_incr_routine(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->trace_head_incr;
}
#endif

#ifdef CHECK_RETURNS_SSE2_EMIT
cache_pc
get_pextrw_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->pextrw;
}

cache_pc
get_pinsrw_entry(dcontext_t *dcontext)
{
    generated_code_t *code = THREAD_GENCODE(dcontext);
    return (cache_pc)code->pinsrw;
}
#endif

/* exported beyond arch/ */
fcache_enter_func_t
get_fcache_enter_shared_routine(dcontext_t *dcontext)
{
    return fcache_enter_shared_routine(dcontext);
}

fcache_enter_func_t
fcache_enter_shared_routine(dcontext_t *dcontext)
{
    ASSERT(USE_SHARED_GENCODE());
    return (fcache_enter_func_t)convert_data_to_function(
        SHARED_GENCODE_MATCH_THREAD(dcontext)->fcache_enter);
}

cache_pc
fcache_return_shared_routine(IF_X86_64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X86_64(mode));
    ASSERT(USE_SHARED_GENCODE());
    if (code == NULL)
        return NULL;
    else
        return code->fcache_return;
}

#ifdef TRACE_HEAD_CACHE_INCR
cache_pc
trace_head_incr_shared_routine(IF_X86_64_ELSE(gencode_mode_t mode, void))
{
    generated_code_t *code = get_shared_gencode(GLOBAL_DCONTEXT _IF_X86_64(mode));
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
#if defined(X86) && defined(X64)
        return (byte *)global_do_syscall_syscall;
#else
#    ifdef WINDOWS
        ASSERT_NOT_IMPLEMENTED(false && "PR 205898: 32-bit syscall on Windows NYI");
#    else
        return (byte *)global_do_syscall_int;
#    endif
#endif
    } else {
#ifdef UNIX
        /* PR 205310: we sometimes have to execute syscalls before we
         * see an app syscall: for a signal default action, e.g.
         */
        return (byte *)IF_X86_64_ELSE(global_do_syscall_syscall, global_do_syscall_int);
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
#if defined(WINDOWS) || (defined(X86) && defined(X64))
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER)
        return (byte *)global_do_syscall_sysenter;
    else
#endif
#ifdef WINDOWS
        if (get_syscall_method() == SYSCALL_METHOD_WOW64 && syscall_uses_wow64_index())
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
hook_vsyscall(dcontext_t *dcontext, bool method_changing)
{
    return false;
}
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
 * to do any extra work in d_r_dispatch, which will naturally go to the
 * post-system-call-instr pc.
 * Unfortunately the 4.4.8 kernel removed the nops (i#1939) so for
 * recent kernels we instead copy into the padding area:
 *     0xf77c6be0:  push   %ecx
 *     0xf77c6be1:  push   %edx
 *     0xf77c6be2:  push   %ebp
 *     0xf77c6be3:  mov    %esp,%ebp
 *     0xf77c6be5:  sysenter
 *     0xf77c6be7:  int    $0x80
 *   normal return point:
 *     0xf77c6be9:  pop    %ebp
 *     0xf77c6bea:  pop    %edx
 *     0xf77c6beb:  pop    %ecx
 *     0xf77c6bec:  ret
 *     0xf77c6bed+:  <padding>
 *
 * Using a hook is much simpler than clobbering the retaddr, which is what
 * Windows does and then has to spend a lot of effort juggling transparency
 * and control on asynch in/out events.
 *
 * XXX i#2694: We can't handle threads that had never been taken over. Such
 * native threads w/o TLS will follow the hook and will crash when spilling
 * to TLS post-syscall before the jump to the linkstub. More synchronization
 * or no-TLS handling is needed.
 */

#    define VSYS_DISPLACED_LEN 4

bool
hook_vsyscall(dcontext_t *dcontext, bool method_changing)
{
#    ifdef X86
    bool res = true;
    instr_t instr;
    byte *pc;
    uint num_nops = 0;
    uint prot;

    /* On a call on a method change the method is not yet finalized so we always try
     */
    if (get_syscall_method() != SYSCALL_METHOD_SYSENTER && !method_changing)
        return false;

    ASSERT(DATASEC_WRITABLE(DATASEC_RARELY_PROT));
    ASSERT(vsyscall_page_start != NULL && vsyscall_syscall_end_pc != NULL &&
           vsyscall_page_start == (app_pc)PAGE_START(vsyscall_syscall_end_pc));

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
#        define CHECK(x)                                          \
            do {                                                  \
                if (!(x)) {                                       \
                    ASSERT(false && "vsyscall pattern mismatch"); \
                    res = false;                                  \
                    goto hook_vsyscall_return;                    \
                }                                                 \
            } while (0);

    /* Only now that we've set vsyscall_sysenter_return_pc do we check writability */
    if (!DYNAMO_OPTION(hook_vsyscall)) {
        res = false;
        goto hook_vsyscall_return;
    }
    byte *base_pc;
    size_t vsyscall_size;
    get_memory_info(vsyscall_page_start, &base_pc, &vsyscall_size, &prot);
    if (base_pc != vsyscall_page_start) {
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "vsyscall page %p is not the base of its area %p\n",
            vsyscall_sysenter_return_pc, base_pc);
    }
    if (!TEST(MEMPROT_WRITE, prot)) {
        res = set_protection(vsyscall_page_start, vsyscall_size, prot | MEMPROT_WRITE);
        if (!res) {
            LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                "failed to mark vsyscall page %p writable\n",
                vsyscall_sysenter_return_pc);
            goto hook_vsyscall_return;
        }
    }

    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1, "Hooking vsyscall page @ " PFX "\n",
        vsyscall_sysenter_return_pc);

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
    /* We don't know what the 5th byte is but we assume that it is junk */

    /* FIXME: at some point we should pull out all the hook code from
     * callback.c into an os-neutral location.  For now, this hook
     * is very special-case and simple.
     */

    /* For thread synch, the datasec prot lock will serialize us (FIXME: do this at
     * init time instead, when see [vdso] page in maps file?)
     */

    CHECK(pc - vsyscall_sysenter_return_pc == VSYS_DISPLACED_LEN);
    ASSERT(pc + 1 /*nop*/ - vsyscall_sysenter_return_pc == JMP_LONG_LENGTH);
    if (num_nops >= VSYS_DISPLACED_LEN) {
        CHECK(num_nops >= pc - vsyscall_sysenter_return_pc);
        memcpy(vmcode_get_writable_addr(vsyscall_syscall_end_pc),
               vsyscall_sysenter_return_pc,
               /* we don't copy the 5th byte to preserve nop for nice disassembly */
               pc - vsyscall_sysenter_return_pc);
        vsyscall_sysenter_displaced_pc = vsyscall_syscall_end_pc;
    } else {
        /* i#1939: the 4.4.8 kernel removed the nops.  It might be safer
         * to place the bytes in our own memory somewhere but that requires
         * extra logic to mark it as executable and to map the PC for
         * dr_fragment_app_pc() and dr_app_pc_for_decoding(), so we go for the
         * easier-to-implement route and clobber the padding garbage after the ret.
         * We assume it is large enough for the 1 byte from the jmp32 and the
         * 4 bytes of displacement.  Technically we should map the PC back
         * here as well but it's close enough.
         */
        pc += 1; /* skip 5th byte of to-be-inserted jmp */
        CHECK(PAGE_START(pc) == PAGE_START(pc + VSYS_DISPLACED_LEN));
        memcpy(vmcode_get_writable_addr(pc), vsyscall_sysenter_return_pc,
               VSYS_DISPLACED_LEN);
        vsyscall_sysenter_displaced_pc = pc;
    }
    insert_relative_jump(vsyscall_sysenter_return_pc,
                         /* we require a thread-shared fcache_return */
                         after_do_shared_syscall_addr(dcontext), NOT_HOT_PATCHABLE);

    if (!TEST(MEMPROT_WRITE, prot)) {
        /* we don't override res here since not much point in not using the
         * hook once its in if we failed to re-protect: we're going to have to
         * trust the app code here anyway */
        DEBUG_DECLARE(bool ok =)
        set_protection(vsyscall_page_start, vsyscall_size, prot);
        ASSERT(ok);
    }
hook_vsyscall_return:
    instr_free(dcontext, &instr);
    return res;
#        undef CHECK
#    elif defined(AARCHXX)
    /* No vsyscall support needed for our ARM targets -- still called on
     * os_process_under_dynamorio().
     */
    ASSERT(!method_changing);
    return false;
#    elif defined(RISCV64)
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    endif /* X86/ARM/RISCV64 */
}

bool
unhook_vsyscall(void)
{
#    ifdef X86
    uint prot;
    bool res;
    uint len = VSYS_DISPLACED_LEN;
    if (get_syscall_method() != SYSCALL_METHOD_SYSENTER)
        return false;
    ASSERT(!sysenter_hook_failed);
    ASSERT(vsyscall_sysenter_return_pc != NULL);
    ASSERT(vsyscall_syscall_end_pc != NULL);
    byte *base_pc;
    size_t vsyscall_size;
    get_memory_info(vsyscall_page_start, &base_pc, &vsyscall_size, &prot);
    if (base_pc != vsyscall_page_start) {
        LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
            "vsyscall page %p is not the base of its area %p\n",
            vsyscall_sysenter_return_pc, base_pc);
        return false;
    }
    if (!TEST(MEMPROT_WRITE, prot)) {
        res = set_protection(vsyscall_page_start, vsyscall_size, prot | MEMPROT_WRITE);
        if (!res)
            return false;
    }
    memcpy(vsyscall_sysenter_return_pc, vsyscall_sysenter_displaced_pc, len);
    /* we do not restore the 5th (junk/nop) byte (we never copied it) */
    if (vsyscall_sysenter_displaced_pc == vsyscall_syscall_end_pc) /* <4.4.8 */
        memset(vmcode_get_writable_addr(vsyscall_syscall_end_pc), RAW_OPCODE_nop, len);
    if (!TEST(MEMPROT_WRITE, prot)) {
        res = set_protection(vsyscall_page_start, vsyscall_size, prot);
        ASSERT(res);
    }
    return true;
#    elif defined(AARCHXX)
    ASSERT_NOT_IMPLEMENTED(get_syscall_method() != SYSCALL_METHOD_SYSENTER);
    return false;
#    elif defined(RISCV64)
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#    endif /* X86/ARM/RISCV64 */
}
#endif     /* LINUX */

void
check_syscall_method(dcontext_t *dcontext, instr_t *instr)
{
    int new_method = SYSCALL_METHOD_UNINITIALIZED;
#ifdef X86
    if (instr_get_opcode(instr) == OP_int)
        new_method = SYSCALL_METHOD_INT;
    else if (instr_get_opcode(instr) == OP_sysenter)
        new_method = SYSCALL_METHOD_SYSENTER;
    else if (instr_get_opcode(instr) == OP_syscall)
        new_method = SYSCALL_METHOD_SYSCALL;
#    ifdef WINDOWS
    else if (instr_get_opcode(instr) == OP_call_ind)
        new_method = SYSCALL_METHOD_WOW64;
#    endif
#elif defined(AARCHXX)
    if (instr_get_opcode(instr) == OP_svc)
        new_method = SYSCALL_METHOD_SVC;
#elif defined(RISCV64)
    if (instr_get_opcode(instr) == OP_ecall)
        new_method = SYSCALL_METHOD_ECALL;
#endif /* X86/ARM */
    else
        ASSERT_NOT_REACHED();

    if (new_method == SYSCALL_METHOD_SYSENTER ||
        IF_X64_ELSE(false, new_method == SYSCALL_METHOD_SYSCALL)) {
        DO_ONCE({
            /* FIXME: DO_ONCE will unprot and reprot, and here we unprot again */
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            /* FIXME : using the raw-bits as the app pc for the instr is
             * not really supported, but places in monitor assume it as well */
            ASSERT(instr_raw_bits_valid(instr) && !instr_has_allocated_bits(instr));
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
            vsyscall_syscall_end_pc =
                instr_get_raw_bits(instr) + instr_length(dcontext, instr);
            IF_WINDOWS({
                /* for XP sp0,1 (but not sp2) and 03 fixup boostrap values */
                if (vsyscall_page_start == VSYSCALL_PAGE_START_BOOTSTRAP_VALUE) {
                    vsyscall_page_start = (app_pc)PAGE_START(instr_get_raw_bits(instr));
                    ASSERT(vsyscall_page_start == VSYSCALL_PAGE_START_BOOTSTRAP_VALUE);
                }
                if (vsyscall_after_syscall == VSYSCALL_AFTER_SYSCALL_BOOTSTRAP_VALUE) {
                    /* for XP sp0,1 and 03 the ret is immediately after the
                     * sysenter instruction */
                    vsyscall_after_syscall =
                        instr_get_raw_bits(instr) + instr_length(dcontext, instr);
                    ASSERT(vsyscall_after_syscall ==
                           VSYSCALL_AFTER_SYSCALL_BOOTSTRAP_VALUE);
                }
            });
            /* For linux, we should have found "[vdso]" in the maps file, but vsyscall
             * is not always on the first vdso page (i#2945).
             */
            IF_LINUX({
                if (vsyscall_page_start !=
                    (app_pc)PAGE_START(instr_get_raw_bits(instr))) {
                    LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
                        "Found vsyscall " PFX " not on 1st vdso page " PFX
                        ", shifting it\n",
                        instr_get_raw_bits(instr), vsyscall_page_start);
                    vsyscall_page_start = (app_pc)PAGE_START(instr_get_raw_bits(instr));
                }
            });
            LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "Found vsyscall @ " PFX " => page " PFX ", post " PFX "\n",
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
            LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 2,
                "Closing vsyscall page hole (int @ " PFX ") => page " PFX ", post " PFX
                "\n",
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
         * inlined int and vsyscall sysenter system calls. We handle fixing up for
         * that in the next ifdef. */
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
#    ifdef LINUX

        /* i#4407: An OP_syscall instruction on 32-bit AMD returns to a hardcoded vsyscall
         * PC no matter where it is. Thus we must hook the vsyscall just like we do for
         * OP_sysenter.
         */
        if (new_method ==
            SYSCALL_METHOD_SYSENTER IF_X86_32(||
                                              (new_method == SYSCALL_METHOD_SYSCALL &&
                                               cpu_info.vendor == VENDOR_AMD))) {
#        ifndef HAVE_TLS
            if (DYNAMO_OPTION(hook_vsyscall)) {
                /* PR 361894: we use TLS for our vsyscall hook (PR 212570) */
                FATAL_USAGE_ERROR(SYSENTER_NOT_SUPPORTED, 2, get_application_name(),
                                  get_application_pid());
            }
#        endif
            /* Hook the sysenter continuation point so we don't lose control */
            if (!sysenter_hook_failed && !hook_vsyscall(dcontext, true /*force*/)) {
                /* PR 212570: for now we bail out to using int;
                 * for performance we should clobber the retaddr and
                 * keep the sysenters.
                 */
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
                sysenter_hook_failed = true;
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
                LOG(GLOBAL, LOG_SYSCALLS | LOG_VMAREAS, 1,
                    "Unable to hook vsyscall page; falling back on int\n");
            }
            if (sysenter_hook_failed)
                new_method = SYSCALL_METHOD_INT;
        }
#    endif /* LINUX */
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
    /* We hook vsyscall page in AMD 32-bit (LOL64) */
    if (syscall_method == SYSCALL_METHOD_SYSCALL && cpu_info.vendor == VENDOR_AMD)
        return IF_X86_64_ELSE(true, false);

    return (syscall_method == SYSCALL_METHOD_INT ||
            syscall_method == SYSCALL_METHOD_SYSCALL ||
            syscall_method == SYSCALL_METHOD_SVC ||
            syscall_method ==
                SYSCALL_METHOD_ECALL IF_WINDOWS(|| syscall_method == SYSCALL_METHOD_WOW64)
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
    ASSERT(syscall_method == SYSCALL_METHOD_UNINITIALIZED ||
           /* on re-attach this happens */
           syscall_method ==
               method IF_UNIX(|| syscall_method == SYSCALL_METHOD_INT /*PR 286922*/));
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

size_t
syscall_instr_length(dr_isa_mode_t mode)
{
    size_t syslen;
#if defined(X86)
    ASSERT(INT_LENGTH == SYSCALL_LENGTH);
    ASSERT(SYSENTER_LENGTH == SYSCALL_LENGTH);
    syslen = SYSCALL_LENGTH;
#elif defined(RISCV64)
    syslen = SYSCALL_LENGTH;
#elif defined(ARM)
    syslen = mode == DR_ISA_ARM_THUMB ? SVC_THUMB_LENGTH : SVC_ARM_LENGTH;
#else
    syslen = SVC_LENGTH;
#endif
    return syslen;
}

bool
is_syscall_at_pc(dcontext_t *dcontext, app_pc pc)
{
    instr_t instr;
    bool res = false;
    instr_init(dcontext, &instr);
    TRY_EXCEPT(dcontext,
               {
                   pc = decode(dcontext, pc, &instr);
                   res = (pc != NULL && instr_valid(&instr) && instr_is_syscall(&instr));
               },
               {});
    instr_free(dcontext, &instr);
    return res;
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
    /* We assume fields from xdi onward are identical. */
    if (src->size > sizeof(dr_mcontext_t))
        return false;
    if (TESTALL(DR_MC_ALL, src->flags) && src->size == sizeof(dr_mcontext_t)) {
        *dst = *(priv_mcontext_t *)(&MCXT_FIRST_REG_FIELD(src));
    } else {
        if (TEST(DR_MC_INTEGER, src->flags)) {
            /* xsp is in the middle of the mcxt, so we save dst->xsp here and
             * restore it later so we can use one memcpy for DR_MC_INTEGER.
             */
            reg_t save_xsp = dst->xsp;
            if (src->size >= offsetof(dr_mcontext_t, IF_X86_ELSE(xflags, pc))) {
                memcpy(&MCXT_FIRST_REG_FIELD(dst), &MCXT_FIRST_REG_FIELD(src),
                       /* end of the mcxt integer gpr */
                       offsetof(priv_mcontext_t, IF_X86_ELSE(xflags, pc)));
            } else
                return false;
            dst->xsp = save_xsp;
        }
        if (TEST(DR_MC_CONTROL, src->flags)) {
            /* XXX i#2710: mc->lr should be under DR_MC_CONTROL */
            dst->xsp = src->xsp;
#if defined(RISCV64)
            if (src->size > offsetof(dr_mcontext_t, fcsr))
                dst->fcsr = src->fcsr;
#else
            /* XXX i#5595: AArch64 should handle fpcr and fpsr here. */
            if (src->size > offsetof(dr_mcontext_t, xflags))
                dst->xflags = src->xflags;
#endif
            else
                return false;
            if (src->size > offsetof(dr_mcontext_t, pc))
                dst->pc = src->pc;
            else
                return false;
        }
        if (TEST(DR_MC_MULTIMEDIA, src->flags)) {
#ifdef X86
            if (src->size > offsetof(dr_mcontext_t, simd)) {
                if (MCXT_NUM_SIMD_SLOTS > MCXT_NUM_SIMD_SSE_AVX_SLOTS &&
                    src->size > offsetof(dr_mcontext_t, simd) +
                            MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE) {
                    if (src->size < offsetof(dr_mcontext_t, simd) + sizeof(dst->simd))
                        return false;
                    /* UNIX 64-bit case, up-to-date copy. XXX i#1312: We don't support
                     * AVX-512 extended number of registers in 64-bit Windows yet.
                     */
                    memcpy(&dst->simd, &src->simd, sizeof(dst->simd));
                } else if (MCXT_NUM_SIMD_SLOTS > MCXT_NUM_SIMD_SSE_AVX_SLOTS &&
                           src->size > offsetof(dr_mcontext_t, simd) +
                                   MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE) {
                    if (src->size < offsetof(dr_mcontext_t, simd) +
                            MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE)
                        return false;
                    /* UNIX 64-bit case, backwards compatibility copy from old
                     * ZMM_REG_SIZE format w/o AVX-512. XXX i#1312: We don't support
                     * AVX-512 extended number of registers in 64-bit Windows yet.
                     */
                    memcpy(&dst->simd, &src->simd,
                           MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE);
                } else if (MCXT_NUM_SIMD_SLOTS == MCXT_NUM_SIMD_SSE_AVX_SLOTS &&
                           src->size > offsetof(dr_mcontext_t, simd) +
                                   MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE) {
                    if (src->size < offsetof(dr_mcontext_t, simd) + sizeof(dst->simd))
                        return false;
                    /* Every other build other than UNIX 64-bit case, up-to-date copy. */
                    memcpy(&dst->simd, &src->simd, sizeof(dst->simd));
                } else {
                    if (src->size < offsetof(dr_mcontext_t, simd) +
                            MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE)
                        return false;
                    /* Any build, backwards compatibility copy from old YMM_REG_SIZE
                     * format w/o AVX-512, all builds.
                     */
                    dr_ymm_t *src_simd_compat = (dr_ymm_t *)src->simd;
                    for (int i = 0; i < MCXT_NUM_SIMD_SSE_AVX_SLOTS; ++i) {
                        dst->simd[i] = *(dr_zmm_t *)&src_simd_compat[i];
                    }
                }
            } else
                return false;
            if (src->size > offsetof(dr_mcontext_t, opmask)) {
                if (src->size < offsetof(dr_mcontext_t, opmask) + sizeof(dst->opmask))
                    return false;
                memcpy(&dst->opmask, &src->opmask, sizeof(dst->opmask));
            }
#else
            /* FIXME i#1551: NYI on ARM */
            ASSERT_NOT_IMPLEMENTED(false);
#endif
        }
    }
    return true;
}

bool
priv_mcontext_to_dr_mcontext(dr_mcontext_t *dst, priv_mcontext_t *src)
{
    /* We assume fields from xdi onward are identical. DynamoRIO's mcontext's size has
     * been appended for AVX-512, and the additional structure's size is checked here.
     */
    if (dst->size > sizeof(dr_mcontext_t))
        return false;
#if defined(AARCH64)
    /* We could support binary compatibility for clients built before the
     * addition of AArch64's SVE support, by evaluating the machine context's
     * user set-size field. But currently do not, preferring to detect
     * incompatibility and asserting or returning false.
     */
    if (TEST(DR_MC_MULTIMEDIA, dst->flags) && dst->size != sizeof(dr_mcontext_t)) {
        CLIENT_ASSERT(
            false, "A pre-SVE client is running on an Arm AArch64 SVE DynamoRIO build!");
        return false;
    }
#endif
    if (TESTALL(DR_MC_ALL, dst->flags) && dst->size == sizeof(dr_mcontext_t)) {
        *(priv_mcontext_t *)(&MCXT_FIRST_REG_FIELD(dst)) = *src;
    } else {
        if (TEST(DR_MC_INTEGER, dst->flags)) {
            /* xsp is in the middle of the mcxt, so we save dst->xsp here and
             * restore it later so we can use one memcpy for DR_MC_INTEGER.
             */
            reg_t save_xsp = dst->xsp;
            if (dst->size >= offsetof(dr_mcontext_t, IF_X86_ELSE(xflags, pc))) {
                memcpy(&MCXT_FIRST_REG_FIELD(dst), &MCXT_FIRST_REG_FIELD(src),
                       /* end of the mcxt integer gpr */
                       offsetof(priv_mcontext_t, IF_X86_ELSE(xflags, pc)));
            } else
                return false;
            dst->xsp = save_xsp;
        }
        if (TEST(DR_MC_CONTROL, dst->flags)) {
            dst->xsp = src->xsp;
#if defined(RISCV64)
            if (dst->size > offsetof(dr_mcontext_t, fcsr))
                dst->fcsr = src->fcsr;
#else
            /* XXX i#5595: AArch64 should handle fpcr and fpsr here. */
            if (dst->size > offsetof(dr_mcontext_t, xflags))
                dst->xflags = src->xflags;
#endif
            else
                return false;
            if (dst->size > offsetof(dr_mcontext_t, pc))
                dst->pc = src->pc;
            else
                return false;
        }
        if (TEST(DR_MC_MULTIMEDIA, dst->flags)) {
#ifdef X86
            if (dst->size > offsetof(dr_mcontext_t, simd)) {
                if (MCXT_NUM_SIMD_SLOTS > MCXT_NUM_SIMD_SSE_AVX_SLOTS &&
                    dst->size > offsetof(dr_mcontext_t, simd) +
                            MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE) {
                    if (dst->size < offsetof(dr_mcontext_t, simd) + sizeof(dst->simd))
                        return false;
                    /* UNIX 64-bit case, up-to-date copy. XXX i#1312: We don't support
                     * AVX-512 extended number of registers in 64-bit Windows yet.
                     */
                    memcpy(&dst->simd, &src->simd, sizeof(dst->simd));
                } else if (MCXT_NUM_SIMD_SLOTS > MCXT_NUM_SIMD_SSE_AVX_SLOTS &&
                           dst->size > offsetof(dr_mcontext_t, simd) +
                                   MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE) {
                    if (dst->size < offsetof(dr_mcontext_t, simd) +
                            MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE)
                        return false;
                    /* UNIX 64-bit case, backwards compatibility copy from old
                     * ZMM_REG_SIZE format w/o AVX-512. XXX i#1312: We don't support
                     * AVX-512 extended number of registers in 64-bit Windows yet.
                     */
                    memcpy(&dst->simd, &src->simd,
                           MCXT_NUM_SIMD_SSE_AVX_SLOTS * ZMM_REG_SIZE);
                } else if (MCXT_NUM_SIMD_SLOTS == MCXT_NUM_SIMD_SSE_AVX_SLOTS &&
                           dst->size > offsetof(dr_mcontext_t, simd) +
                                   MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE) {
                    if (dst->size < offsetof(dr_mcontext_t, simd) + sizeof(dst->simd))
                        return false;
                    /* Every other build other than UNIX 64-bit case, up-to-date copy. */
                    memcpy(&dst->simd, &src->simd, sizeof(dst->simd));
                } else {
                    if (dst->size < offsetof(dr_mcontext_t, simd) +
                            MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMM_REG_SIZE)
                        return false;
                    /* Any build, backwards compatibility copy from old YMM_REG_SIZE
                     * format w/o AVX-512, all builds.
                     */
                    dr_ymm_t *dst_simd_compat = (dr_ymm_t *)dst->simd;
                    for (int i = 0; i < MCXT_NUM_SIMD_SSE_AVX_SLOTS; ++i) {
                        dst_simd_compat[i] = *(dr_ymm_t *)&src->simd[i];
                    }
                }
            } else
                return false;
            if (dst->size > offsetof(dr_mcontext_t, opmask)) {
                if (dst->size < offsetof(dr_mcontext_t, opmask) + sizeof(dst->opmask))
                    return false;
                memcpy(&dst->opmask, &src->opmask, sizeof(dst->opmask));
            }
#elif defined(AARCHXX)
            /* FIXME i#1551: NYI on ARM */
            ASSERT_NOT_IMPLEMENTED(false);
#endif
        }
    }
    return true;
}

priv_mcontext_t *
dr_mcontext_as_priv_mcontext(dr_mcontext_t *mc)
{
    /* It's up to the caller to ensure the proper DR_MC_ flags are set (i#1848) */
    return (priv_mcontext_t *)(&MCXT_FIRST_REG_FIELD(mc));
}

priv_mcontext_t *
get_priv_mcontext_from_dstack(dcontext_t *dcontext)
{
    return (priv_mcontext_t *)((char *)dcontext->dstack - sizeof(priv_mcontext_t));
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
    print_file(
        f,
        dump_xml ? "\t<priv_mcontext_t value=\"@" PFX "\""
#ifdef X86
                   "\n\t\txax=\"" PFX "\"\n\t\txbx=\"" PFX "\""
                   "\n\t\txcx=\"" PFX "\"\n\t\txdx=\"" PFX "\""
                   "\n\t\txsi=\"" PFX "\"\n\t\txdi=\"" PFX "\""
                   "\n\t\txbp=\"" PFX "\"\n\t\txsp=\"" PFX "\""
#    ifdef X64
                   "\n\t\tr8=\"" PFX "\"\n\t\tr9=\"" PFX "\""
                   "\n\t\tr10=\"" PFX "\"\n\t\tr11=\"" PFX "\""
                   "\n\t\tr12=\"" PFX "\"\n\t\tr13=\"" PFX "\""
                   "\n\t\tr14=\"" PFX "\"\n\t\tr15=\"" PFX "\""
#    endif /* X64 */
#elif defined(AARCHXX)
                   "\n\t\tr0=\"" PFX "\"\n\t\tr1=\"" PFX "\""
                   "\n\t\tr2=\"" PFX "\"\n\t\tr3=\"" PFX "\""
                   "\n\t\tr4=\"" PFX "\"\n\t\tr5=\"" PFX "\""
                   "\n\t\tr6=\"" PFX "\"\n\t\tr7=\"" PFX "\""
                   "\n\t\tr8=\"" PFX "\"\n\t\tr9=\"" PFX "\""
                   "\n\t\tr10=\"" PFX "\"\n\t\tr11=\"" PFX "\""
                   "\n\t\tr12=\"" PFX "\"\n\t\tr13=\"" PFX "\""
                   "\n\t\tr14=\"" PFX "\"\n\t\tr15=\"" PFX "\""
#    ifdef X64
                   "\n\t\tr16=\"" PFX "\"\n\t\tr17=\"" PFX "\""
                   "\n\t\tr18=\"" PFX "\"\n\t\tr19=\"" PFX "\""
                   "\n\t\tr20=\"" PFX "\"\n\t\tr21=\"" PFX "\""
                   "\n\t\tr22=\"" PFX "\"\n\t\tr23=\"" PFX "\""
                   "\n\t\tr24=\"" PFX "\"\n\t\tr25=\"" PFX "\""
                   "\n\t\tr26=\"" PFX "\"\n\t\tr27=\"" PFX "\""
                   "\n\t\tr28=\"" PFX "\"\n\t\tr29=\"" PFX "\""
                   "\n\t\tr30=\"" PFX "\"\n\t\tr31=\"" PFX "\""
#    endif /* X64 */
#elif defined(RISCV64)
                   "\n\t\tx0=\"" PFX "\"\n\t\tx1=\"" PFX "\""
                   "\n\t\tx2=\"" PFX "\"\n\t\tx3=\"" PFX "\""
                   "\n\t\tx4=\"" PFX "\"\n\t\tx5=\"" PFX "\""
                   "\n\t\tx6=\"" PFX "\"\n\t\tx7=\"" PFX "\""
                   "\n\t\tx8=\"" PFX "\"\n\t\tx9=\"" PFX "\""
                   "\n\t\tx10=\"" PFX "\"\n\t\tx11=\"" PFX "\""
                   "\n\t\tx12=\"" PFX "\"\n\t\tx13=\"" PFX "\""
                   "\n\t\tx14=\"" PFX "\"\n\t\tx15=\"" PFX "\""
                   "\n\t\tx16=\"" PFX "\"\n\t\tx17=\"" PFX "\""
                   "\n\t\tx18=\"" PFX "\"\n\t\tx19=\"" PFX "\""
                   "\n\t\tx20=\"" PFX "\"\n\t\tx21=\"" PFX "\""
                   "\n\t\tx22=\"" PFX "\"\n\t\tx23=\"" PFX "\""
                   "\n\t\tx24=\"" PFX "\"\n\t\tx25=\"" PFX "\""
                   "\n\t\tx26=\"" PFX "\"\n\t\tx27=\"" PFX "\""
                   "\n\t\tx28=\"" PFX "\"\n\t\tx29=\"" PFX "\""
                   "\n\t\tx30=\"" PFX "\"\n\t\tx31=\"" PFX "\""
#endif /* X86/ARM/RISCV64 */
                 : "priv_mcontext_t @" PFX "\n"
#ifdef X86
                   "\txax = " PFX "\n\txbx = " PFX "\n\txcx = " PFX "\n\txdx = " PFX "\n"
                   "\txsi = " PFX "\n\txdi = " PFX "\n\txbp = " PFX "\n\txsp = " PFX "\n"
#    ifdef X64
                   "\tr8  = " PFX "\n\tr9  = " PFX "\n\tr10 = " PFX "\n\tr11 = " PFX "\n"
                   "\tr12 = " PFX "\n\tr13 = " PFX "\n\tr14 = " PFX "\n\tr15 = " PFX "\n"
#    endif /* X64 */
#elif defined(AARCHXX)
                   "\tr0  = " PFX "\n\tr1  = " PFX "\n\tr2  = " PFX "\n\tr3  = " PFX "\n"
                   "\tr4  = " PFX "\n\tr5  = " PFX "\n\tr6  = " PFX "\n\tr7  = " PFX "\n"
                   "\tr8  = " PFX "\n\tr9  = " PFX "\n\tr10 = " PFX "\n\tr11 = " PFX "\n"
                   "\tr12 = " PFX "\n\tr13 = " PFX "\n\tr14 = " PFX "\n\tr15 = " PFX "\n"
#    ifdef X64
                   "\tr16 = " PFX "\n\tr17 = " PFX "\n\tr18 = " PFX "\n\tr19 = " PFX "\n"
                   "\tr20 = " PFX "\n\tr21 = " PFX "\n\tr22 = " PFX "\n\tr23 = " PFX "\n"
                   "\tr24 = " PFX "\n\tr25 = " PFX "\n\tr26 = " PFX "\n\tr27 = " PFX "\n"
                   "\tr28 = " PFX "\n\tr29 = " PFX "\n\tr30 = " PFX "\n\tr31 = " PFX "\n"
#    endif /* X64 */
#elif defined(RISCV64)
                   "\tx0  = " PFX "\n\tx1  = " PFX "\n\tx2  = " PFX "\n\tx3  = " PFX "\n"
                   "\tx4  = " PFX "\n\tx5  = " PFX "\n\tx6  = " PFX "\n\tx7  = " PFX "\n"
                   "\tx8  = " PFX "\n\tx9  = " PFX "\n\tx10 = " PFX "\n\tx11 = " PFX "\n"
                   "\tx12 = " PFX "\n\tx13 = " PFX "\n\tx14 = " PFX "\n\tx15 = " PFX "\n"
                   "\tx16 = " PFX "\n\tx17 = " PFX "\n\tx18 = " PFX "\n\tx19 = " PFX "\n"
                   "\tx20 = " PFX "\n\tx21 = " PFX "\n\tx22 = " PFX "\n\tx23 = " PFX "\n"
                   "\tx24 = " PFX "\n\tx25 = " PFX "\n\tx26 = " PFX "\n\tx27 = " PFX "\n"
                   "\tx28 = " PFX "\n\tx29 = " PFX "\n\tx30 = " PFX "\n\tx31 = " PFX "\n"
#endif /* X86/ARM/RISCV64 */
        ,
        context,
#ifdef X86
        context->xax, context->xbx, context->xcx, context->xdx, context->xsi,
        context->xdi, context->xbp, context->xsp
#    ifdef X64
        ,
        context->r8, context->r9, context->r10, context->r11, context->r12, context->r13,
        context->r14, context->r15
#    endif /* X64 */
#elif defined(AARCHXX)
        context->r0, context->r1, context->r2, context->r3, context->r4, context->r5,
        context->r6, context->r7, context->r8, context->r9, context->r10, context->r11,
        context->r12, context->r13, context->r14, context->r15
#    ifdef X64
        ,
        context->r16, context->r17, context->r18, context->r19, context->r20,
        context->r21, context->r22, context->r23, context->r24, context->r25,
        context->r26, context->r27, context->r28, context->r29, context->r30, context->r31
#    endif /* X64 */
#elif defined(RISCV64)
        context->x0, context->x1, context->x2, context->x3, context->x4, context->x5,
        context->x6, context->x7, context->x8, context->x9, context->x10, context->x11,
        context->x12, context->x13, context->x14, context->x15, context->x16,
        context->x17, context->x18, context->x19, context->x20, context->x21,
        context->x22, context->x23, context->x24, context->x25, context->x26,
        context->x27, context->x28, context->x29, context->x30, context->x31
#endif /* X86/ARM/RISCV64 */
    );

#ifdef X86
    if (preserve_xmm_caller_saved()) {
        int i, j;
        for (i = 0; i < proc_num_simd_saved(); i++) {
            if (ZMM_ENABLED()) {
                print_file(f, dump_xml ? "\t\tzmm%d= \"0x" : "\tzmm%d= 0x", i);
                for (j = 0; j < 16; j++) {
                    print_file(f, "%08x", context->simd[i].u32[j]);
                }
            } else if (YMM_ENABLED()) {
                print_file(f, dump_xml ? "\t\tymm%d= \"0x" : "\tymm%d= 0x", i);
                for (j = 0; j < 8; j++) {
                    print_file(f, "%08x", context->simd[i].u32[j]);
                }
            } else {
                print_file(f, dump_xml ? "\t\txmm%d= \"0x" : "\txmm%d= 0x", i);
                /* This would be simpler if we had uint64 fields in dr_xmm_t but
                 * that complicates our struct layouts */
                for (j = 0; j < 4; j++) {
                    print_file(f, "%08x", context->simd[i].u32[j]);
                }
            }
            print_file(f, dump_xml ? "\"\n" : "\n");
        }
        for (i = 0; i < MCXT_NUM_OPMASK_SLOTS; i++) {
            print_file(f, dump_xml ? "\t\tk%d= \"" PFX "\"\n" : "\tk%d= " PFX "\n", i,
                       context->opmask[i]);
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
#elif defined(AARCHXX)
    {
        int i, j;
#    ifdef AARCH64
        int words = proc_has_feature(FEATURE_SVE) ? 16 : 4;
#    else
        int words = 4;
#    endif
        /* XXX: should be proc_num_simd_saved(). */
        for (i = 0; i < proc_num_simd_registers(); i++) {
            print_file(f, dump_xml ? "\t\tqd= \"0x" : "\tq%-3d= 0x", i);
            for (j = 0; j < words; j++) {
                print_file(f, "%08x ", context->simd[i].u32[j]);
            }
            print_file(f, dump_xml ? "\"\n" : "\n");
        }
        /* TODO i#5365: SVE predicate registers and FFR dump. */
    }
#endif

#if defined(RISCV64)
    print_file(f, dump_xml ? "\n\t\tpc=\"" PFX "\" />\n" : "\tpc     = " PFX "\n",
               context->pc);
#else
    print_file(f,
               dump_xml ? "\n\t\teflags=\"" PFX "\"\n\t\tpc=\"" PFX "\" />\n"
                        : "\teflags = " PFX "\n\tpc     = " PFX "\n",
               context->xflags, context->pc);
#endif
}

#ifdef AARCHXX
reg_t
get_stolen_reg_val(priv_mcontext_t *mc)
{
    return *(reg_t *)(((byte *)mc) + opnd_get_reg_dcontext_offs(dr_reg_stolen));
}

void
set_stolen_reg_val(priv_mcontext_t *mc, reg_t newval)
{
    *(reg_t *)(((byte *)mc) + opnd_get_reg_dcontext_offs(dr_reg_stolen)) = newval;
}
#endif

#ifdef PROFILE_RDTSC
/* This only works on Pentium I or later */
#    ifdef UNIX
__inline__ uint64
get_time()
{
    uint64 res;
    RDTSC_LL(res);
    return res;
}
#    else /* WINDOWS */
uint64
get_time()
{
    return __rdtsc(); /* compiler intrinsic */
}
#    endif
#endif /* PROFILE_RDTSC */

#ifdef DEBUG
bool
is_ibl_routine_type(dcontext_t *dcontext, cache_pc target, ibl_branch_type_t branch_type)
{
    ibl_type_t ibl_type;
    DEBUG_DECLARE(bool is_ibl =)
    get_ibl_routine_type_ex(dcontext, target, &ibl_type _IF_X86_64(NULL));
    ASSERT(is_ibl);
    return (branch_type == ibl_type.branch_type);
}
#endif /* DEBUG */

/***************************************************************************
 * UNIT TEST
 */
#ifdef STANDALONE_UNIT_TEST

#    ifdef UNIX
#        include <pthread.h>
#    endif

#    define MAX_NUM_THREADS 3
#    define LOOP_COUNT 10000
volatile static int count1 = 0;
volatile static int count2 = 0;
#    ifdef X64
volatile static ptr_int_t count3 = 0;
#    endif

IF_UNIX_ELSE(void *, DWORD WINAPI)
test_thread_func(void *arg)
{
    int i;
    /* We first incrment "count" LOOP_COUNT times, then decrement it (LOOP_COUNT-1)
     * times, so each thread will increment "count" by 1.
     */
    for (i = 0; i < LOOP_COUNT; i++)
        ATOMIC_INC(int, count1);
    for (i = 0; i < (LOOP_COUNT - 1); i++)
        ATOMIC_DEC(int, count1);
    for (i = 0; i < LOOP_COUNT; i++)
        ATOMIC_ADD(int, count2, 1);
    for (i = 0; i < (LOOP_COUNT - 1); i++)
        ATOMIC_ADD(int, count2, -1);

    return 0;
}

static void
do_parallel_updates()
{
    int i;
#    ifdef UNIX
    pthread_t threads[MAX_NUM_THREADS];
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, test_thread_func, NULL);
    }
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
#    else  /* WINDOWS */
    HANDLE threads[MAX_NUM_THREADS];
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        threads[i] =
            CreateThread(NULL,                   /* use default security attributes */
                         0,                      /* use defautl stack size */
                         test_thread_func, NULL, /* argument to thread function */
                         0,                      /* use default creation flags */
                         NULL /* thread id */);
    }
    WaitForMultipleObjects(MAX_NUM_THREADS, threads, TRUE, INFINITE);
#    endif /* UNIX/WINDOWS */
}

/* some tests for inline asm for atomic ops */
void
unit_test_atomic_ops(void)
{
    int value = -1;
#    ifdef X64
    int64 value64 = -1;
#    endif
    print_file(STDERR, "test inline asm atomic ops\n");
    ATOMIC_4BYTE_WRITE(&count1, value, false);
    EXPECT(count1, -1);
#    ifdef X64
    ATOMIC_8BYTE_WRITE(&count3, value64, false);
    EXPECT(count3, -1);
#    endif
    EXPECT(atomic_inc_and_test(&count1), true);      /* result is 0 */
    EXPECT(atomic_inc_and_test(&count1), false);     /* result is 1 */
    EXPECT(atomic_dec_and_test(&count1), false);     /* init value is 1, result is 0 */
    EXPECT(atomic_dec_and_test(&count1), true);      /* init value is 0, result is -1 */
    EXPECT(atomic_dec_becomes_zero(&count1), false); /* result is -2 */
    EXPECT(atomic_compare_exchange_int(&count1, -3, 1), false); /* no exchange */
    EXPECT(count1, -2);
    EXPECT(atomic_compare_exchange_int(&count1, -2, 1), true); /* exchange */
    EXPECT(atomic_dec_becomes_zero(&count1), true);            /* result is 0 */
    do_parallel_updates();
    EXPECT(count1, MAX_NUM_THREADS);
    EXPECT(count2, MAX_NUM_THREADS);
}

#endif /* STANDALONE_UNIT_TEST */
