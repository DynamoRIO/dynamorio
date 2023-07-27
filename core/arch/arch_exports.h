/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

/* file "arch_exports.h" -- arch-specific exported declarations
 *
 * References:
 *   "Intel Architecture Software Developer's Manual", 1999.
 *   "ARM Architecture Manual", 2014.
 */

#ifndef _ARCH_EXPORTS_H_
#define _ARCH_EXPORTS_H_ 1

/* We export all of opnd.h for reg_id_t, DR_NUM_GPR_REGS, etc. */
#include "opnd.h"

#include "atomic_exports.h"

#ifdef X86
/* PR 264138: we must preserve xmm0-5 if on a 64-bit kernel.
 * On Linux we must preserve all xmm registers.
 * If AVX is enabled we save ymm.
 * i#437: YMM is an extension of XMM from 128-bit to 256-bit without
 * adding new ones, so code operating on XMM often also operates on YMM,
 * and thus some *XMM* macros also apply to *YMM*.
 */
#    define XMM_REG_SIZE 16
#    define YMM_REG_SIZE 32
#    define ZMM_REG_SIZE 64
#    define OPMASK_AVX512F_REG_SIZE 2
#    define OPMASK_AVX512BW_REG_SIZE 8
#    define MCXT_SIMD_SLOT_SIZE ZMM_REG_SIZE
#    define MCXT_TOTAL_SIMD_SLOTS_SIZE (MCXT_NUM_SIMD_SLOTS * MCXT_SIMD_SLOT_SIZE)
#    define MCXT_TOTAL_OPMASK_SLOTS_SIZE \
        (MCXT_NUM_OPMASK_SLOTS * OPMASK_AVX512BW_REG_SIZE)
#    define MCXT_TOTAL_SIMD_SSE_AVX_SLOTS_SIZE \
        (MCXT_NUM_SIMD_SSE_AVX_SLOTS * MCXT_SIMD_SLOT_SIZE)
/* Indicates OS support, not just processor support (xref i#1278) */
#    define YMM_ENABLED() (proc_avx_enabled())
#    define ZMM_ENABLED() (proc_avx512_enabled())
#    define YMMH_REG_SIZE (YMM_REG_SIZE / 2) /* upper half */
#    define ZMMH_REG_SIZE (ZMM_REG_SIZE / 2) /* upper half */
#    define MCXT_YMMH_SLOTS_SIZE (MCXT_NUM_SIMD_SSE_AVX_SLOTS * YMMH_REG_SIZE)
#endif /* X86 */

/* Number of slots for spills from inlined clean calls. */
#define CLEANCALL_NUM_INLINE_SLOTS 5

typedef enum {
    IBL_NONE = -1,
    /* N.B.: order determines which table is on 2nd cache line in local_state_t */
    IBL_RETURN = 0, /* returns lookup routine has stricter requirements */
    IBL_BRANCH_TYPE_START = IBL_RETURN,
    IBL_INDCALL,
    IBL_INDJMP,
    IBL_GENERIC = IBL_INDJMP, /* currently least restrictive */
    /* can double if a generic lookup is needed
       FIXME: remove this and add names for specific needs */
    IBL_SHARED_SYSCALL = IBL_GENERIC,
    IBL_BRANCH_TYPE_END
} ibl_branch_type_t;

#define IBL_HASH_FUNC_OFFSET_MAX IF_X64_ELSE(4, 3)

struct _fragment_entry_t; /* in fragment.h */
struct _ibl_table_t;      /* in fragment.h */

/* Scratch space and state required to be easily accessible from
 * in-cache indirect branch lookup routines, store in thread-local storage.
 * Goal is to get it all in one cache line: currently, though, it is
 * 2 lines on 32-byte-line machines, with the call* and jmp* tables and the
 * hashtable stats spilling onto the 2nd line.
 * Even all on one line, shared ibl has a load vs private ibl's hardcoded immed...
 *
 * FIXME: to avoid splitting the mcontext for now these scratch
 * fs:slots are used in fcache, but copied to the
 * mcontext on transitions.  see case 3701
 */
typedef struct _lookup_table_access_t {
    ptr_uint_t hash_mask;
    struct _fragment_entry_t *lookuptable;
} lookup_table_access_t;

typedef struct _table_stat_state_t {
    /* Organized in mask-table pairs to get both fields for a particular table
     * on the same cache line.
     */
    /* FIXME We can play w/ordering these fields differently or if TLS space is
     * crunched keeping a subset of them in TLS.
     * For example, the ret_trace & indcall_trace tables could be heavily used
     * but if the indjmp table isn't, it might make sense to put the ret_bb
     * table's fields into TLS since ret_bb is likely to be the most heavily
     * used for BB2BB IBL.
     */
    lookup_table_access_t table[IBL_BRANCH_TYPE_END];
    /* FIXME: should allocate this separately so that release and
     * DEBUG builds have the same layout especially when backward
     * aligned entry */
#ifdef HASHTABLE_STATISTICS
    uint stats;
#endif
} table_stat_state_t;

#if defined(AARCHXX) || defined(RISCV64)
typedef struct _ibl_entry_pc_t {
    byte *ibl;
    byte *unlinked;
} ibl_entry_pc_t;
#endif

/* All spill slots are grouped in a separate struct because with
 * -no_ibl_table_in_tls, only these slots are mapped to TLS (and the
 * table address/mask pairs are not).
 */
typedef struct _spill_state_t {
    /* Four registers are used in the indirect branch lookup routines */
#ifdef X86
    reg_t xax, xbx, xcx, xdx; /* general-purpose registers */
#elif defined(AARCHXX)
    reg_t r0, r1, r2, r3;
    /* These are needed for ldex/stex mangling and A64 icache_op_ic_ivau_asm. */
    reg_t r4, r5;
    reg_t reg_stolen; /* slot for the stolen register */
#elif defined(RISCV64)
    reg_t a0, a1, a2, a3;
    /* Slot for the stolen register, which is tp.
     * Note that on RISC-V, tp is the thread pointer, and it is also a general-purpose
     * register, so we steal tp to store DR's tls.
     */
    reg_t reg_stolen;
#endif
    /* XXX: move this below the tables to fit more on cache line */
    dcontext_t *dcontext;
#if defined(RISCV64) || defined(AARCHXX)
    /* We store addresses here so we can load pointer-sized addresses into
     * registers with a single instruction in our exit stubs and gencode.
     */
    /* FIXME i#1551: add Thumb vs ARM: may need two entry points here */
    byte *fcache_return;
    ibl_entry_pc_t trace_ibl[IBL_BRANCH_TYPE_END];
    ibl_entry_pc_t bb_ibl[IBL_BRANCH_TYPE_END];
#endif
#ifdef AARCHXX
    /* State for converting exclusive monitors into compare-and-swap (-ldstex2cas). */
    ptr_uint_t ldstex_addr;
    ptr_uint_t ldstex_value;
    ptr_uint_t ldstex_value2; /* For 2nd value of a pair. */
    ptr_uint_t ldstex_size;
#    ifdef ARM
    /* In A32 mode we have no OP_cbnz so we have to save the flags. */
    reg_t ldstex_flags;
#    endif
    /* TODO i#1575: coarse-grain NYI on ARM */
#endif
} spill_state_t;

typedef struct _local_state_t {
    spill_state_t spill_space;
} local_state_t;

typedef struct _local_state_extended_t {
    spill_state_t spill_space;
    table_stat_state_t table_space;
} local_state_extended_t;

/* local_state_[extended_]t is allocated in os-specific thread-local storage (TLS),
 * accessible off of fs:.  But, the actual segment offset varies, so
 * os_tls_offset() must be used to obtain an fs: offset from a slot.
 */
#ifdef X86
#    define TLS_XAX_SLOT ((ushort)offsetof(spill_state_t, xax))
#    define TLS_XBX_SLOT ((ushort)offsetof(spill_state_t, xbx))
#    define TLS_XCX_SLOT ((ushort)offsetof(spill_state_t, xcx))
#    define TLS_XDX_SLOT ((ushort)offsetof(spill_state_t, xdx))
#    define TLS_REG0_SLOT TLS_XAX_SLOT
#    define TLS_REG1_SLOT TLS_XBX_SLOT
#    define TLS_REG2_SLOT TLS_XCX_SLOT
#    define TLS_REG3_SLOT TLS_XDX_SLOT
#    define SCRATCH_REG0 DR_REG_XAX
#    define SCRATCH_REG1 DR_REG_XBX
#    define SCRATCH_REG2 DR_REG_XCX
#    define SCRATCH_REG3 DR_REG_XDX
#elif defined(AARCHXX)
#    define TLS_REG0_SLOT ((ushort)offsetof(spill_state_t, r0))
#    define TLS_REG1_SLOT ((ushort)offsetof(spill_state_t, r1))
#    define TLS_REG2_SLOT ((ushort)offsetof(spill_state_t, r2))
#    define TLS_REG3_SLOT ((ushort)offsetof(spill_state_t, r3))
#    define TLS_REG4_SLOT ((ushort)offsetof(spill_state_t, r4))
#    define TLS_REG5_SLOT ((ushort)offsetof(spill_state_t, r5))
#    define TLS_REG_STOLEN_SLOT ((ushort)offsetof(spill_state_t, reg_stolen))
#    define SCRATCH_REG0 DR_REG_R0
#    define SCRATCH_REG1 DR_REG_R1
#    define SCRATCH_REG2 DR_REG_R2
#    define SCRATCH_REG3 DR_REG_R3
#    define SCRATCH_REG4 DR_REG_R4
#    define SCRATCH_REG5 DR_REG_R5
#    define SCRATCH_REG_LAST SCRATCH_REG5
#elif defined(RISCV64)
#    define TLS_REG0_SLOT ((ushort)offsetof(spill_state_t, a0))
#    define TLS_REG1_SLOT ((ushort)offsetof(spill_state_t, a1))
#    define TLS_REG2_SLOT ((ushort)offsetof(spill_state_t, a2))
#    define TLS_REG3_SLOT ((ushort)offsetof(spill_state_t, a3))
#    define TLS_REG_STOLEN_SLOT ((ushort)offsetof(spill_state_t, reg_stolen))
#    define SCRATCH_REG0 DR_REG_A0
#    define SCRATCH_REG1 DR_REG_A1
#    define SCRATCH_REG2 DR_REG_A2
#    define SCRATCH_REG3 DR_REG_A3
#endif /* X86/ARM */
#define IBL_TARGET_REG SCRATCH_REG2
#define IBL_TARGET_SLOT TLS_REG2_SLOT
#define TLS_DCONTEXT_SLOT ((ushort)offsetof(spill_state_t, dcontext))
#ifdef AARCHXX
#    define TLS_FCACHE_RETURN_SLOT ((ushort)offsetof(spill_state_t, fcache_return))
#    define TLS_LDSTEX_ADDR_SLOT ((ushort)offsetof(spill_state_t, ldstex_addr))
#    define TLS_LDSTEX_VALUE_SLOT ((ushort)offsetof(spill_state_t, ldstex_value))
#    define TLS_LDSTEX_VALUE2_SLOT ((ushort)offsetof(spill_state_t, ldstex_value2))
#    define TLS_LDSTEX_SIZE_SLOT ((ushort)offsetof(spill_state_t, ldstex_size))
#    ifdef ARM
#        define TLS_LDSTEX_FLAGS_SLOT ((ushort)offsetof(spill_state_t, ldstex_flags))
#    endif
#elif defined(RISCV64)
#    define TLS_FCACHE_RETURN_SLOT ((ushort)offsetof(spill_state_t, fcache_return))
#endif

#define TABLE_OFFSET (offsetof(local_state_extended_t, table_space))
#define TLS_MASK_SLOT(btype)                                              \
    ((ushort)(TABLE_OFFSET + offsetof(table_stat_state_t, table[btype]) + \
              offsetof(lookup_table_access_t, hash_mask)))
#define TLS_TABLE_SLOT(btype)                                             \
    ((ushort)(TABLE_OFFSET + offsetof(table_stat_state_t, table[btype]) + \
              offsetof(lookup_table_access_t, lookuptable)))

#ifdef HASHTABLE_STATISTICS
#    define TLS_HTABLE_STATS_SLOT                                 \
        ((ushort)(offsetof(local_state_extended_t, table_space) + \
                  offsetof(table_stat_state_t, stats)))
#endif

#define TLS_NUM_SLOTS                                                                  \
    (DYNAMO_OPTION(ibl_table_in_tls) ? sizeof(local_state_extended_t) / sizeof(void *) \
                                     : sizeof(local_state_t) / sizeof(void *))

#ifdef WINDOWS
#    define DETACH_CALLBACK_CODE_SIZE 256
#    define DETACH_CALLBACK_FINAL_JMP_SIZE 32

/* For detach - stores callback continuation pcs and is used to d_r_dispatch to them after
 * we detach. We have one per a thread (with stacked callbacks) stored in an array. */
typedef struct _detach_callback_stack_t {
    thread_id_t tid;        /* thread tid */
    ptr_uint_t count;       /* number of saved post-syscall continuation pcs */
    app_pc *callback_addrs; /* location of array of saved continuation pcs */
    reg_t xax_save;         /* spill slot for post-syscall code */
    reg_t xbx_save;         /* spill slot for post-syscall code */
    reg_t xcx_save;         /* spill slot for post-syscall code */
    app_pc target;          /* temp slot for post-syscall code */
    /* we need some private code to do the actual jmp */
    byte code_buf[DETACH_CALLBACK_FINAL_JMP_SIZE];
} detach_callback_stack_t;

void
arch_patch_syscall(dcontext_t *dcontext, byte *target);
byte *
emit_detach_callback_code(dcontext_t *dcontext, byte *buf,
                          detach_callback_stack_t *callback_state);
void
emit_detach_callback_final_jmp(dcontext_t *dcontext,
                               detach_callback_stack_t *callback_state);
#endif

uint64
proc_get_timestamp(void);

#ifdef WINDOWS
#    define RDTSC_LL(var) (var = __rdtsc())
/* no intrinsic available, and no inline asm support, so we have x86.asm routine */
byte *
get_frame_ptr(void);
byte *
get_stack_ptr(void);
#    define GET_FRAME_PTR(var) (var = get_frame_ptr())
#    define GET_STACK_PTR(var) (var = get_stack_ptr())
#else /* UNIX */
#    ifdef DR_HOST_X86
#        ifdef X64
#            define RDTSC_LL(llval)                                        \
                do {                                                       \
                    uint low, high;                                        \
                    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high)); \
                    (llval) = ((uint64)high << 32) | low;                  \
                } while (0)
#            define GET_CUR_PC(var)                                         \
                __asm__ __volatile__("call 1f; 1: pop %%rax; mov %%rax, %0" \
                                     : "=m"(var)                            \
                                     :                                      \
                                     : "rax", "memory")
#        else
/* We define RDTSC_LL differently on 32-bit for better performance */
#            define RDTSC_LL(llval) __asm__ __volatile__("rdtsc" : "=A"(llval))
#            define GET_CUR_PC(var)                                         \
                __asm__ __volatile__("call 1f; 1: pop %%eax; mov %%eax, %0" \
                                     : "=m"(var)                            \
                                     :                                      \
                                     : "eax", "memory")
#        endif /* 64/32 */
#        define GET_FRAME_PTR(var) \
            asm("mov %%" IF_X64_ELSE("rbp", "ebp") ", %0" : "=m"(var))
#        define GET_STACK_PTR(var) \
            asm("mov %%" IF_X64_ELSE("rsp", "esp") ", %0" : "=m"(var))
#    elif defined(DR_HOST_AARCH64)
#        define RDTSC_LL(llval)                 \
            do {                                \
                (llval) = proc_get_timestamp(); \
            } while (0)

#        define GET_FRAME_PTR(var) __asm__ __volatile__("mov %0, x29" : "=r"(var))
#        define GET_STACK_PTR(var) __asm__ __volatile__("mov %0, sp" : "=r"(var))
#        define GET_CUR_PC(var) \
            __asm__ __volatile__("bl 1f\n1: str x30, %0" : "=m"(var) : : "x30")
#    elif defined(DR_HOST_ARM)
#        define RDTSC_LL(llval) (llval) = proc_get_timestamp()
/* FIXME i#1551: frame pointer is r7 in thumb mode */
#        define GET_FRAME_PTR(var) \
            __asm__ __volatile__("str " IF_X64_ELSE("x29", "r11") ", %0" : "=m"(var))
#        define GET_STACK_PTR(var) __asm__ __volatile__("str sp, %0" : "=m"(var))
#        define GET_CUR_PC(var) \
            __asm__ __volatile__("bl 1f\n1: str lr, %0" : "=m"(var) : : "lr")
#    elif defined(DR_HOST_RISCV64)
#        define RDTSC_LL(llval)                                      \
            do {                                                     \
                __asm__ __volatile__("csrr %0, time" : "=r"(llval)); \
            } while (0)

#        define GET_FRAME_PTR(var) __asm__ __volatile__("mv %0, fp" : "=r"(var))
#        define GET_STACK_PTR(var) __asm__ __volatile__("mv %0, sp" : "=r"(var))
#        define GET_CUR_PC(var) \
            __asm__ __volatile__("jal t0, 1f; 1: sd t0, %0" : "=m"(var) : : "t0")
#    endif /* X86/ARM/RISCV64 */
#endif     /* UNIX */

#define DEBUGGER_INTERRUPT_BYTE 0xcc

/* if hot_patch is true:
 *   The write that inserts the relative target is done atomically so this
 *   function is safe with respect to a thread executing the code containing
 *   this target, presuming that the code in both the before and after states
 *   is valid
 */
byte *
insert_relative_target(byte *pc, cache_pc target, bool hot_patch);

byte *
insert_relative_jump(byte *pc, cache_pc target, bool hot_patch);

/* in arch.c */

#ifdef PROFILE_RDTSC
#    ifdef UNIX
/* This only works on Pentium I or later */
__inline__ uint64
get_time();
#    else /* WINDOWS */
/* This only works on Pentium I or later */
uint64
get_time(void);
#    endif
#endif

void
d_r_arch_init(void);
void d_r_arch_exit(IF_WINDOWS_ELSE_NP(bool detach_stacked_callbacks, void));
void
arch_thread_init(dcontext_t *dcontext);
void
arch_thread_exit(dcontext_t *dcontext _IF_WINDOWS(bool detach_stacked_callbacks));
#if defined(WINDOWS_PC_SAMPLE) && !defined(DEBUG)
/* for sampling fast exit path */
void
arch_thread_profile_exit(dcontext_t *dcontext);
void
arch_profile_exit(void);
#endif
#ifdef AARCHXX
void
arch_reset_stolen_reg(void);
void
arch_mcontext_reset_stolen_reg(dcontext_t *dcontext, priv_mcontext_t *mc);
#endif

bool
is_indirect_branch_lookup_routine(dcontext_t *dcontext, cache_pc pc);
bool
in_generated_routine(dcontext_t *dcontext, cache_pc pc);
bool
in_fcache_return(dcontext_t *dcontext, cache_pc pc);
bool
in_clean_call_save(dcontext_t *dcontext, cache_pc pc);
bool
in_clean_call_restore(dcontext_t *dcontext, cache_pc pc);
bool
in_indirect_branch_lookup_code(dcontext_t *dcontext, cache_pc pc);
cache_pc
get_fcache_target(dcontext_t *dcontext);
void
set_fcache_target(dcontext_t *dcontext, cache_pc value);
void
copy_mcontext(priv_mcontext_t *src, priv_mcontext_t *dst);
bool
dr_mcontext_to_priv_mcontext(priv_mcontext_t *dst, dr_mcontext_t *src);
bool
priv_mcontext_to_dr_mcontext(dr_mcontext_t *dst, priv_mcontext_t *src);
priv_mcontext_t *
dr_mcontext_as_priv_mcontext(dr_mcontext_t *mc);
priv_mcontext_t *
get_priv_mcontext_from_dstack(dcontext_t *dcontext);
void
dr_mcontext_init(dr_mcontext_t *mc);
void
dump_mcontext(priv_mcontext_t *context, file_t f, bool dump_xml);
#ifdef AARCHXX
reg_t
get_stolen_reg_val(priv_mcontext_t *context);
void
set_stolen_reg_val(priv_mcontext_t *mc, reg_t newval);
#endif
const char *
get_branch_type_name(ibl_branch_type_t branch_type);
ibl_branch_type_t
get_ibl_branch_type(instr_t *instr);

/* Return the entry point for a routine with which an
 * atomic hashtable delete can be performed for the given fragment.
 */
cache_pc
get_target_delete_entry_pc(dcontext_t *dcontext, struct _ibl_table_t *table);
cache_pc
get_reset_exit_stub(dcontext_t *dcontext);

typedef linkstub_t *(*fcache_enter_func_t)(dcontext_t *dcontext);
fcache_enter_func_t
get_fcache_enter_private_routine(dcontext_t *dcontext);
fcache_enter_func_t
get_fcache_enter_gonative_routine(dcontext_t *dcontext);

cache_pc
get_unlinked_entry(dcontext_t *dcontext, cache_pc linked_entry);
cache_pc
get_linked_entry(dcontext_t *dcontext, cache_pc unlinked_entry);
#ifdef X64
cache_pc
get_trace_cmp_entry(dcontext_t *dcontext, cache_pc linked_entry);
#endif

cache_pc
get_do_syscall_entry(dcontext_t *dcontext);
#ifdef WINDOWS
fcache_enter_func_t
get_fcache_enter_indirect_routine(dcontext_t *dcontext);
cache_pc
get_do_callback_return_entry(dcontext_t *dcontext);
#else
cache_pc
get_do_int_syscall_entry(dcontext_t *dcontext);
cache_pc
get_do_int81_syscall_entry(dcontext_t *dcontext);
cache_pc
get_do_int82_syscall_entry(dcontext_t *dcontext);
cache_pc
get_do_clone_syscall_entry(dcontext_t *dcontext);
#    ifdef VMX86_SERVER
cache_pc
get_do_vmkuw_syscall_entry(dcontext_t *dcontext);
#    endif
#endif
byte *
get_global_do_syscall_entry(void);

/* NOTE - because of the sygate int 2e hack, after_do_syscall_addr and
 * after_shared_syscall_addr in fact both check for the same address with
 * int system calls, so can't use them to disambiguate between the two. */
#ifdef WINDOWS
/* For int system calls
 *  - addr is the instruction immediately after the int 2e
 *  - code is the addr of our code that handles post int 2e
 * For non int system calls addr and code will be the same (ret addr), this
 * is just for working around our Sygate int 2e in ntdll hack (5217) */
cache_pc
after_shared_syscall_addr(dcontext_t *dcontext);
cache_pc
after_shared_syscall_code(dcontext_t *dcontext);
bool
is_shared_syscall_routine(dcontext_t *dcontext, cache_pc pc);
/* For int system calls
 *  - addr is the instruction immediately after the int 2e
 *  - code is the addr of our code that handles post int 2e
 * For non int system calls addr and code will be the same (ret addr), this
 * is just for working around our Sygate int 2e in ntdll hack (5217) */
cache_pc
after_do_syscall_addr(dcontext_t *dcontext);
cache_pc
after_do_syscall_code(dcontext_t *dcontext);
#else
cache_pc
after_do_shared_syscall_addr(dcontext_t *dcontext);
cache_pc
after_do_syscall_addr(dcontext_t *dcontext);
bool
is_after_main_do_syscall_addr(dcontext_t *dcontext, cache_pc pc);
bool
is_after_do_syscall_addr(dcontext_t *dcontext, cache_pc pc);
#endif

bool
is_after_syscall_address(dcontext_t *dcontext, cache_pc pc);
bool
is_after_syscall_that_rets(dcontext_t *dcontext, cache_pc pc);

void
update_generated_hashtable_access(dcontext_t *dcontext);
fcache_enter_func_t
get_fcache_enter_shared_routine(dcontext_t *dcontext);

/* Method of performing system call.
 * We assume that only one method is in use, except for 32-bit applications
 * on 64-bit x86 linux kernels, which use both sys{enter,call} on the vsyscall
 * page and inlined int (PR 286922).
 * For these apps, DR itself and global_do_syscall use int, but we
 * have both a do_syscall for the vsyscall and a separate do_int_syscall
 * (we can't use the vsyscall for some system calls like clone: we could
 * potentially use int for everything if we fixed up the syscall args).
 * The method set in that case is the vsyscall method.
 */
enum {
    SYSCALL_METHOD_UNINITIALIZED,
    SYSCALL_METHOD_INT,
    SYSCALL_METHOD_SYSENTER,
    SYSCALL_METHOD_SYSCALL,
#ifdef WINDOWS
    SYSCALL_METHOD_WOW64,
#endif
    SYSCALL_METHOD_SVC,   /* ARM */
    SYSCALL_METHOD_ECALL, /* RISCV64 */
};
#ifdef UNIX
enum { SYSCALL_METHOD_LONGEST_INSTR = 2 }; /* to ensure safe patching */
#endif
void
check_syscall_method(dcontext_t *dcontext, instr_t *instr);
int
get_syscall_method(void);
/* Does the syscall instruction always return to the invocation point? */
bool
does_syscall_ret_to_callsite(void);
void
set_syscall_method(int method);
#ifdef LINUX
bool
should_syscall_method_be_sysenter(void);
#endif
bool
hook_vsyscall(dcontext_t *dcontext, bool method_changing);
bool
unhook_vsyscall(void);
/* returns the address of the first app syscall instruction we saw (see hack
 * in win32/os.c that uses this for PRE_SYSCALL_PC, not for general use */
byte *
get_app_sysenter_addr(void);

/* in [x86/arm].asm */
/* Calls the specified function 'func' after switching to the stack 'stack'.  If we're
 * currently on the d_r_initstack, 'mutex_to_free' should be passed so we release the
 * initstack_mutex.  The supplied 'func_arg' will be passed as an argument to 'func'.
 * If 'func' returns then 'return_on_return' is checked. If set we swap back stacks and
 * return to the caller.  If not set then it's assumed that func wasn't supposed to
 * return and we go to an error routine unexpected_return() below.
 */
void
call_switch_stack(void *func_arg, byte *stack, void (*func)(void *arg),
                  void *mutex_to_free, bool return_on_return);
#if defined(WINDOWS) && !defined(X64)
DYNAMORIO_EXPORT int64
dr_invoke_x64_routine(dr_auxlib64_routine_ptr_t func64, uint num_params, ...);
#endif
void
unexpected_return(void);
void
clone_and_swap_stack(byte *stack, byte *tos);
void
go_native(dcontext_t *dcontext);

/* Calls dynamo_exit_process if exitproc is true, else calls dynamo_exit_thread.
 * Uses the current dstack, but instructs the cleanup routines not to
 * de-allocate it, does a custom de-allocate after swapping to d_r_initstack (don't
 * want to use d_r_initstack the whole time, that's too long to hold the mutex).
 * Then calls system call sysnum with parameter base param_base, which is presumed
 * to be either NtTerminateThread or NtTerminateProcess or exit.
 *
 * Note that the caller is responsible for placing the actual syscall arguments
 * at the correct offset from edx (or ebx).  See SYSCALL_PARAM_OFFSET in
 * win32 os.c for more info.
 */
void
cleanup_and_terminate(dcontext_t *dcontext, int sysnum, ptr_uint_t sys_arg1,
                      ptr_uint_t sys_arg2, bool exitproc,
                      /* these 2 args are only used for Mac thread exit */
                      ptr_uint_t sys_arg3, ptr_uint_t sys_arg4);

bool
cpuid_supported(void);
void
our_cpuid(int res[4], int eax, int ecx);
#ifdef WINDOWS
int
dynamorio_syscall_int2e(int sysnum, ...);
int
dynamorio_syscall_sysenter(int sysnum, ...);
int
dynamorio_syscall_sygate_int2e(int sysnum, ...);
int
dynamorio_syscall_sygate_sysenter(int sysnum, ...);
#    ifdef X64
int
dynamorio_syscall_syscall(int sysnum, ...);
#    endif
int
dynamorio_syscall_wow64(int sysnum, ...);
/* Use this version if !syscall_uses_edx_param_base() */
int
dynamorio_syscall_wow64_noedx(int sysnum, ...);
void
get_segments_cs_ss(cxt_seg_t *cs, cxt_seg_t *ss);
void
get_segments_defg(cxt_seg_t *ds, cxt_seg_t *es, cxt_seg_t *fs, cxt_seg_t *gs);
void
get_own_context_helper(CONTEXT *cxt);
/* PR203701: If the dstack is exhausted we'll use this function to
 * call internal_exception_info() with a separate exception stack.
 */
void
call_intr_excpt_alt_stack(dcontext_t *dcontext, EXCEPTION_RECORD *pExcptRec, CONTEXT *cxt,
                          byte *stack);
void
dynamorio_earliest_init_takeover(void);
#else /* UNIX */
void
client_int_syscall(void);
void
dynamorio_sigreturn(void);
void
dynamorio_sys_exit(void);
void
dynamorio_condvar_wake_and_jmp(KSYNCH_TYPE *ksynch /*in xax/r0*/,
                               byte *jmp_tgt /*in xcx/r1*/);
#    ifdef LINUX
#        ifndef X64
void
dynamorio_nonrt_sigreturn(void);
#        endif
thread_id_t
dynamorio_clone(uint flags, byte *newsp, void *ptid, void *tls, void *ctid,
                void (*func)(void));
void
xfer_to_new_libdr(app_pc entry, void **init_sp, byte *cur_dr_map, size_t cur_dr_size);
#    endif
#    ifdef MACOS
void
new_bsdthread_intercept(void);
#    endif
#endif
void
back_from_native(void);
/* The _end is a label, not a function. */
void
back_from_native_retstubs(void);
void
back_from_native_retstubs_end(void);
/* Each stub should be 4 bytes: push imm8 + jmp rel8 */
enum { BACK_FROM_NATIVE_RETSTUB_SIZE = 4 };
#ifdef UNIX
void
native_plt_call(void);
#endif
DEBUG_DECLARE(void debug_infinite_loop(void); /* handy cpu eating infinite loop */)
void
hashlookup_null_handler(void);
void
dr_stmxcsr(uint *val);
void
dr_xgetbv(uint *high, uint *low);
void
dr_fxsave(byte *buf_aligned);
void
dr_fnsave(byte *buf_aligned);
void
dr_fxrstor(byte *buf_aligned);
void
dr_frstor(byte *buf_aligned);
#ifdef X64
void
dr_fxsave32(byte *buf_aligned);
void
dr_fxrstor32(byte *buf_aligned);
#endif

/* Keep in synch with x86.asm.  This is the difference between the SP saved in
 * the mcontext and the SP of the caller of dr_app_start() and
 * dynamorio_app_take_over().
 */
#ifdef X86
#    define DYNAMO_START_XSP_ADJUST 16
#else
#    define DYNAMO_START_XSP_ADJUST 0
#endif

/* x86_code.c */
void
dynamo_start(priv_mcontext_t *mc);

/* Gets the retstack index saved in x86.asm and restores the mcontext to the
 * original app state.
 */
int
native_get_retstack_idx(priv_mcontext_t *mc);

/* in proc.c -- everything in proc.h is exported so just include it here */
#include "proc.h"

/* in disassemble.c */
#ifdef DEBUG /* because uses logfile */
void
disassemble_fragment(dcontext_t *dcontext, fragment_t *f, bool just_header);
void
disassemble_app_bb(dcontext_t *dcontext, app_pc tag, file_t outfile);
/* dumps callstack for ebp stored in mcontext */
void
dump_mcontext_callstack(dcontext_t *dcontext);
#endif

/* flags for dump_callstack_to_buffer */
enum {
    CALLSTACK_USE_XML = 0x00000001,
    CALLSTACK_ADD_HEADER = 0x00000002,
    CALLSTACK_MODULE_INFO = 0x00000004,
    CALLSTACK_MODULE_PATH = 0x00000008,
    CALLSTACK_FRAME_PTR = 0x00000010,
};

/* dumps callstack for current pc and ebp */
void
dump_dr_callstack(file_t outfile);

/* user-specified ebp */
void
dump_callstack(app_pc pc, app_pc ebp, file_t outfile, bool dump_xml);

void
dump_callstack_to_buffer(char *buf, size_t bufsz, size_t *sofar, app_pc pc, app_pc ebp,
                         uint flags /*CALLSTACK_ bitmask*/);

void
disassemble_fragment_header(dcontext_t *dcontext, fragment_t *f, file_t outfile);
void
disassemble_fragment_body(dcontext_t *dcontext, fragment_t *f, file_t outfile);
void
disassemble_app_bb(dcontext_t *dcontext, app_pc tag, file_t outfile);

/* in emit_utils.c */

static inline bool
use_addr_prefix_on_short_disp(void)
{
#ifdef STANDALONE_DECODER
    /* Not worth providing control over this.  Go w/ most likely best choice. */
    return false;
#else
    /* -ibl_addr_prefix => addr prefix everywhere */
    return (
        DYNAMO_OPTION(ibl_addr_prefix) ||
        /* PR 212807, PR 209709: addr prefix is noticeably worse
         * on Pentium M, Core, and Core2.
         * It's better on Pentium 4 and Pentium D.
         *
         * Note that this variation by processor type does not need to
         * be stored in pcaches b/c either way works and the size is
         * not assumed (except for prefixes: but coarse_units doesn't
         * support prefixes in general).
         */
        /* P4 and PD */
        (proc_get_family() == FAMILY_PENTIUM_4 ||
         /* PPro, P2, P3, but not PM */
         (proc_get_family() == FAMILY_PENTIUM_3 &&
          (proc_get_model() <= 8 || proc_get_model() == 10 || proc_get_model() == 11))));
    /* FIXME: should similarly remove addr prefixes from hardcoded
     * emits in emit_utils.c, except in cases where space is more
     * important than speed.
     * FIXME: case 5231 long term solution should properly choose
     * - ibl - speed
     * - prefixes - speed/space?
     * - app code - preserverd since we normally don't need to reencode,
     *              unless it is a CTI that goes through FS - should be preserved too
     * - direct stubs - space
     * - indirect stubs - speed/space?
     * - enter/exit - speed?
     * - interception routines - speed?
     */
#endif /* STANDALONE_DECODER */
}

#include "encode_api.h"

/* static version for drdecodelib */
#define DEFAULT_ISA_MODE_STATIC                                                \
    IF_X86_ELSE(IF_X64_ELSE(DR_ISA_AMD64, DR_ISA_IA32),                        \
                IF_AARCHXX_ELSE(IF_X64_ELSE(DR_ISA_ARM_A64, DR_ISA_ARM_THUMB), \
                                DR_ISA_RV64IMAFDC))

/* Use this one in DR proper.
 * This one is now static as well after we removed the runtime option that
 * used to be here: but I'm leaving the split to make it easier to add
 * an option in the future.
 */
#define DEFAULT_ISA_MODE                                                       \
    IF_X86_ELSE(IF_X64_ELSE(DR_ISA_AMD64, DR_ISA_IA32),                        \
                IF_AARCHXX_ELSE(IF_X64_ELSE(DR_ISA_ARM_A64, DR_ISA_ARM_THUMB), \
                                DR_ISA_RV64IMAFDC))

/* For converting back from PC_AS_JMP_TGT on Thumb */
#ifdef ARM
#    define ENTRY_PC_TO_DECODE_PC(pc) \
        ((app_pc)(ALIGN_BACKWARD(pc, THUMB_SHORT_INSTR_SIZE)))
#else
#    define ENTRY_PC_TO_DECODE_PC(pc) ((app_pc)(pc))
#endif

/* Switches the ISA mode, if necessary, and returns the (potentially modified) pc */
app_pc
canonicalize_pc_target(dcontext_t *dcontext, app_pc pc);

void
d_r_decode_init(void);

bool
fill_with_nops(dr_isa_mode_t isa_mode, byte *addr, size_t size);

/***************************************************************************
 * Arch-specific defines
 */
#ifdef X86

/* Merge w/ _LENGTH enum below? */
/* not ifdef X64 to simplify code */
#    define SIZE64_MOV_XAX_TO_TLS 8
#    define SIZE64_MOV_XBX_TO_TLS 9
#    define SIZE64_MOV_PTR_IMM_TO_XAX 10
#    define SIZE64_MOV_PTR_IMM_TO_TLS (12 * 2) /* high and low 32 bits separately */
#    define SIZE64_MOV_R8_TO_XAX 3
#    define SIZE64_MOV_R9_TO_XCX 3
#    define SIZE32_MOV_XAX_TO_TLS 5
#    define SIZE32_MOV_XBX_TO_TLS 6
#    define SIZE32_MOV_XAX_TO_TLS_DISP32 6
#    define SIZE32_MOV_XBX_TO_TLS_DISP32 7
#    define SIZE32_MOV_XAX_TO_ABS 5
#    define SIZE32_MOV_XBX_TO_ABS 6
#    define SIZE32_MOV_PTR_IMM_TO_XAX 5
#    define SIZE32_MOV_PTR_IMM_TO_TLS 10

#    ifdef X64
#        define FRAG_IS_32(flags) (TEST(FRAG_32_BIT, (flags)))
#        define FRAG_IS_X86_TO_X64(flags) (TEST(FRAG_X86_TO_X64, (flags)))
#    else
#        define FRAG_IS_32(flags) true
#        define FRAG_IS_X86_TO_X64(flags) false
#    endif

#    define PC_AS_JMP_TGT(isa_mode, pc) pc
#    define PC_AS_LOAD_TGT(isa_mode, pc) pc

#    define SIZE_MOV_XAX_TO_TLS(flags, require_addr16)                            \
        (FRAG_IS_32(flags) ? ((require_addr16 || use_addr_prefix_on_short_disp()) \
                                  ? SIZE32_MOV_XAX_TO_TLS                         \
                                  : SIZE32_MOV_XAX_TO_TLS_DISP32)                 \
                           : SIZE64_MOV_XAX_TO_TLS)
#    define SIZE_MOV_XBX_TO_TLS(flags, require_addr16)                            \
        (FRAG_IS_32(flags) ? ((require_addr16 || use_addr_prefix_on_short_disp()) \
                                  ? SIZE32_MOV_XBX_TO_TLS                         \
                                  : SIZE32_MOV_XBX_TO_TLS_DISP32)                 \
                           : SIZE64_MOV_XBX_TO_TLS)
#    define SIZE_MOV_PTR_IMM_TO_XAX(flags) \
        (FRAG_IS_32(flags) ? SIZE32_MOV_PTR_IMM_TO_XAX : SIZE64_MOV_PTR_IMM_TO_XAX)

/* size of restore ecx prefix */
#    define XCX_IN_TLS(flags) \
        (DYNAMO_OPTION(private_ib_in_tls) || TEST(FRAG_SHARED, (flags)))
#    define FRAGMENT_BASE_PREFIX_SIZE(flags)                          \
        ((FRAG_IS_X86_TO_X64(flags) &&                                \
          IF_X64_ELSE(DYNAMO_OPTION(x86_to_x64_ibl_opt), false))      \
             ? SIZE64_MOV_R9_TO_XCX                                   \
             : (XCX_IN_TLS(flags) ? SIZE_MOV_XBX_TO_TLS(flags, false) \
                                  : SIZE32_MOV_XBX_TO_ABS))

/* exported for DYNAMO_OPTION(separate_private_stubs)
 * FIXME: find better way to export -- would use global var accessed
 * by macro, but easiest to have as static initializer for heap bucket
 */
/* for -thread_private, we're relying on the fact that
 * SIZE32_MOV_XAX_TO_TLS == SIZE32_MOV_XAX_TO_ABS, and that
 * x64 always uses tls
 */
#    define DIRECT_EXIT_STUB_SIZE32 \
        (SIZE32_MOV_XAX_TO_TLS + SIZE32_MOV_PTR_IMM_TO_XAX + JMP_LONG_LENGTH)
#    define DIRECT_EXIT_STUB_SIZE64 \
        (SIZE64_MOV_XAX_TO_TLS + SIZE64_MOV_PTR_IMM_TO_XAX + JMP_LONG_LENGTH)
#    define DIRECT_EXIT_STUB_SIZE(flags) \
        (FRAG_IS_32(flags) ? DIRECT_EXIT_STUB_SIZE32 : DIRECT_EXIT_STUB_SIZE64)
#    define DIRECT_EXIT_STUB_DATA_SZ 0

/* coarse-grain stubs use a store directly to memory so they can
 * link through the stub and not mess up app state.
 * 1st instr example:
 *   67 64 c7 06 e0 0e 02 99 4e 7d  addr16 mov $0x7d4e9902 -> %fs:0x0ee0
 * 64-bit is split into high and low dwords:
 *   65 c7 04 25 20 16 00 00 02 99 4e 7d  mov $0x7d4e9902 -> %gs:0x1620
 *   65 c7 04 25 24 16 00 00 00 00 00 00  mov $0x00000000 -> %gs:0x1624
 * both of these exact sequences are assumed in entrance_stub_target_tag()
 * and coarse_indirect_stub_jmp_target().
 */
#    define STUB_COARSE_DIRECT_SIZE32 (SIZE32_MOV_PTR_IMM_TO_TLS + JMP_LONG_LENGTH)
#    define STUB_COARSE_DIRECT_SIZE64 (SIZE64_MOV_PTR_IMM_TO_TLS + JMP_LONG_LENGTH)
#    define STUB_COARSE_DIRECT_SIZE(flags) \
        (FRAG_IS_32(flags) ? STUB_COARSE_DIRECT_SIZE32 : STUB_COARSE_DIRECT_SIZE64)

/* Writes nops into the address range. */
#    define SET_TO_NOPS(isa_mode, addr, size) fill_with_nops(isa_mode, addr, size)
/* writes debugbreaks into the address range */
#    define SET_TO_DEBUG(addr, size) memset(addr, 0xcc, size)
/* check if region is SET_TO_NOP */
#    define IS_SET_TO_NOP(addr, size) is_region_memset_to_char(addr, size, 0x90)
/* check if region is SET_TO_DEBUG */
#    define IS_SET_TO_DEBUG(addr, size) is_region_memset_to_char(addr, size, 0xcc)

/* offset of the patchable region from the end of a cti */
#    define CTI_PATCH_OFFSET 4
/* size of the patch to a cti */
#    define CTI_PATCH_SIZE 4

/* offset of the patchable region from the end of a stub */
#    define EXIT_STUB_PATCH_OFFSET 4
/* size of the patch to a stub */
#    define EXIT_STUB_PATCH_SIZE 4

/* the most bytes we'll need to shift a patchable location for -pad_jmps */
#    define MAX_PAD_SIZE 3

/****************************************************************************/
#elif defined(AARCHXX)

#    ifdef X64
#        define FRAG_IS_THUMB(flags) false
#        define FRAG_IS_32(flags) false
#    else
#        define FRAG_IS_THUMB(flags) (TEST(FRAG_THUMB, (flags)))
#        define FRAG_IS_32(flags) true
#    endif

#    define PC_AS_JMP_TGT(isa_mode, pc) \
        ((isa_mode) == DR_ISA_ARM_THUMB ? (app_pc)(((ptr_uint_t)pc) | 1) : pc)
#    define PC_AS_LOAD_TGT(isa_mode, pc) \
        ((isa_mode) == DR_ISA_ARM_THUMB ? (app_pc)(((ptr_uint_t)pc) & ~0x1) : pc)

#    ifdef AARCH64
#        define AARCH64_INSTR_SIZE 4
#        define FRAGMENT_BASE_PREFIX_SIZE(flags) AARCH64_INSTR_SIZE
#        define DIRECT_EXIT_STUB_SIZE(flags) \
            (10 * AARCH64_INSTR_SIZE) /* see insert_exit_stub_other_flags */
/* Size of data slot used to store address of linked fragment or fcache return routine.
 * We reserve 12 bytes for the 8 byte address, so that we can store it in an 8-byte
 * aligned address. This is required for atomicity of write operations.
 */
#        define DIRECT_EXIT_STUB_DATA_SLOT_ALIGNMENT_PADDING 4
#        define DIRECT_EXIT_STUB_DATA_SZ \
            (sizeof(app_pc) + DIRECT_EXIT_STUB_DATA_SLOT_ALIGNMENT_PADDING)
#    else
#        define FRAGMENT_BASE_PREFIX_SIZE(flags) \
            (FRAG_IS_THUMB(flags) ? THUMB_LONG_INSTR_SIZE : ARM_INSTR_SIZE)
/* exported for DYNAMO_OPTION(separate_private_stubs) */
#        define ARM_INSTR_SIZE 4
#        define THUMB_SHORT_INSTR_SIZE 2
#        define THUMB_LONG_INSTR_SIZE 4
#        define DIRECT_EXIT_STUB_INSTR_COUNT 4
/* for far linking we need a target stored in the stub */
#        define DIRECT_EXIT_STUB_DATA_SZ sizeof(app_pc)
/* all instrs are wide in the Thumb version */
#        define DIRECT_EXIT_STUB_SIZE(flags)                               \
            ((FRAG_IS_THUMB(flags)                                         \
                  ? (DIRECT_EXIT_STUB_INSTR_COUNT * THUMB_LONG_INSTR_SIZE) \
                  : (DIRECT_EXIT_STUB_INSTR_COUNT * ARM_INSTR_SIZE)) +     \
             DIRECT_EXIT_STUB_DATA_SZ)
#    endif

/* FIXME i#1575: implement coarse-grain support */
#    define STUB_COARSE_DIRECT_SIZE(flags) (ASSERT_NOT_IMPLEMENTED(false), 0)

/* FIXME i#1551: we need these to all take in the dr_isa_mode_t */
#    define ARM_NOP 0xe320f000
#    define THUMB_NOP 0xbf00
#    define ARM_BKPT 0xe1200070
#    define THUMB_BKPT 0xbe00
/* writes nops into the address range */
#    define SET_TO_NOPS(isa_mode, addr, size) fill_with_nops(isa_mode, addr, size)
/* writes debugbreaks into the address range */
#    define SET_TO_DEBUG(addr, size) ASSERT_NOT_IMPLEMENTED(false)
/* check if region is SET_TO_DEBUG */
#    define IS_SET_TO_DEBUG(addr, size) (ASSERT_NOT_IMPLEMENTED(false), false)

/* offset of the patchable region from the end of a cti */
#    define CTI_PATCH_OFFSET 4
/* size of the patch to a cti */
#    define CTI_PATCH_SIZE 4

/* offset of the patchable region from the end of a stub */
#    define EXIT_STUB_PATCH_OFFSET 4
/* size of the patch to a stub */
#    define EXIT_STUB_PATCH_SIZE 4

/* the most bytes we'll need to shift a patchable location for -pad_jmps */
#    define MAX_PAD_SIZE 0

/* i#1906: alignment needed for the source address of data to load into the PC */
#    define PC_LOAD_ADDR_ALIGN 4

#elif defined(RISCV64)
/* Instructions from C extensions are 2 bytes in size and we will never use this macro on
 * these instructions. */
#    define RISCV64_INSTR_SIZE 4
#    define RISCV64_INSTR_COMPRESSED_SIZE 2
#    define FRAGMENT_BASE_PREFIX_SIZE(flags) RISCV64_INSTR_SIZE * 2
#    define DIRECT_EXIT_STUB_SIZE(flags) \
        (16 * RISCV64_INSTR_SIZE) /* see insert_exit_stub_other_flags() */
#    define FRAG_IS_32(flags) false
#    define PC_AS_JMP_TGT(isa_mode, pc) pc
#    define PC_AS_LOAD_TGT(isa_mode, pc) pc
#    define DIRECT_EXIT_STUB_DATA_SLOT_ALIGNMENT_PADDING 4
#    define DIRECT_EXIT_STUB_DATA_SZ \
        (sizeof(app_pc) + DIRECT_EXIT_STUB_DATA_SLOT_ALIGNMENT_PADDING)
#    define STUB_COARSE_DIRECT_SIZE(flags) (ASSERT_NOT_IMPLEMENTED(false), 0)
#    define SET_TO_NOPS(isa_mode, addr, size) fill_with_nops(isa_mode, addr, size)
#    define SET_TO_DEBUG(addr, size) ASSERT_NOT_IMPLEMENTED(false)
#    define IS_SET_TO_DEBUG(addr, size) (ASSERT_NOT_IMPLEMENTED(false), false)

/* offset of the patchable region from the end of a cti */
/* FIXME i#3544: Not implemented */
#    define CTI_PATCH_OFFSET 4
/* offset of the patchable region from the end of a stub */
/* FIXME i#3544: Not implemented */
#    define EXIT_STUB_PATCH_OFFSET 4
/* size of the patch to a stub */
/* FIXME i#3544: Not implemented */
#    define EXIT_STUB_PATCH_SIZE 4
/* the most bytes we'll need to shift a patchable location for -pad_jmps */
/* FIXME i#3544: Not implemented */
#    define MAX_PAD_SIZE 0
#endif /* RISCV64 */
/****************************************************************************/

/* evaluates to true if region crosses at most 1 padding boundary */
#define WITHIN_PAD_REGION(lower, upper) ((upper) - (lower) <= PAD_JMPS_ALIGNMENT)

#define STATS_PAD_JMPS_ADD(flags, stat, val)                  \
    DOSTATS({                                                 \
        if (TEST(FRAG_SHARED, (flags))) {                     \
            if (TEST(FRAG_IS_TRACE, (flags)))                 \
                STATS_ADD(pad_jmps_shared_trace_##stat, val); \
            else                                              \
                STATS_ADD(pad_jmps_shared_bb_##stat, val);    \
        } else if (TEST(FRAG_IS_TRACE, (flags)))              \
            STATS_ADD(pad_jmps_trace_##stat, val);            \
        else if (TEST(FRAG_TEMP_PRIVATE, (flags)))            \
            STATS_ADD(pad_jmps_temp_##stat, val);             \
        else                                                  \
            STATS_ADD(pad_jmps_bb_##stat, val);               \
    })

bool
is_exit_cti_stub_patchable(dcontext_t *dcontext, instr_t *inst, uint frag_flags);

uint
extend_trace_pad_bytes(fragment_t *add_frag);

uint
patchable_exit_cti_align_offs(dcontext_t *dcontext, instr_t *inst, cache_pc pc);

bool
is_patchable_exit_stub(dcontext_t *dcontext, linkstub_t *l, fragment_t *f);

uint
bytes_for_exitstub_alignment(dcontext_t *dcontext, linkstub_t *l, fragment_t *f,
                             byte *startpc);

byte *
pad_for_exitstub_alignment(dcontext_t *dcontext, linkstub_t *l, fragment_t *f,
                           byte *startpc);

void
remove_nops_from_ilist(dcontext_t *dcontext,
                       instrlist_t *ilist _IF_DEBUG(bool recreating));

uint
nop_pad_ilist(dcontext_t *dcontext, fragment_t *f, instrlist_t *ilist, bool emitting);

bool
is_exit_cti_patchable(dcontext_t *dcontext, instr_t *inst, uint frag_flags);

int
exit_stub_size(dcontext_t *dcontext, cache_pc target, uint flags);

int
insert_exit_stub(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, cache_pc stub_pc);

int
linkstub_unlink_entry_offset(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);

cache_pc
indirect_linkstub_stub_pc(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);

cache_pc
indirect_linkstub_target(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);

/* based on machine state, returns which of l1 and l2 must have been taken */
linkstub_t *
linkstub_cbr_disambiguate(dcontext_t *dcontext, fragment_t *f, linkstub_t *l1,
                          linkstub_t *l2);

cache_pc
cbr_fallthrough_exit_cti(cache_pc prev_cti_pc);

/* for use with patch_branch and insert_relative target */
enum { NOT_HOT_PATCHABLE = false, HOT_PATCHABLE = true };
void
patch_branch(dr_isa_mode_t isa_mode, cache_pc branch_pc, cache_pc target_pc,
             bool hot_patch);
bool
link_direct_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, fragment_t *targetf,
                 bool hot_patch);
void
unlink_direct_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);
void
link_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l, bool hot_patch);
void
unlink_indirect_exit(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);
void
insert_fragment_prefix(dcontext_t *dcontext, fragment_t *f);
int
fragment_prefix_size(uint flags);
void
update_indirect_exit_stub(dcontext_t *dcontext, fragment_t *f, linkstub_t *l);
#ifdef PROFILE_RDTSC
uint
profile_call_size(void);
void
insert_profile_call(cache_pc start_pc);
void
finalize_profile_call(dcontext_t *dcontext, fragment_t *f);
#endif
int
decode_syscall_num(dcontext_t *dcontext, byte *entry);
#ifdef WINDOWS
void
link_shared_syscall(dcontext_t *dcontext);
void
unlink_shared_syscall(dcontext_t *dcontext);
#endif
size_t
syscall_instr_length(dr_isa_mode_t mode);
bool
is_syscall_at_pc(dcontext_t *dcontext, app_pc pc);

/* Coarse-grain fragment support */
cache_pc
entrance_stub_jmp(cache_pc stub);
cache_pc
entrance_stub_jmp_target(cache_pc stub);
app_pc
entrance_stub_target_tag(cache_pc stub, coarse_info_t *info);
bool
coarse_is_indirect_stub(cache_pc stub);
bool
coarse_cti_is_intra_fragment(dcontext_t *dcontext, coarse_info_t *info, instr_t *inst,
                             cache_pc start_pc);
cache_pc
coarse_indirect_stub_jmp_target(cache_pc stub);
uint
coarse_indirect_stub_size(coarse_info_t *info);
bool
coarse_is_entrance_stub(cache_pc stub);
bool
coarse_is_trace_head(cache_pc stub);
bool
entrance_stub_linked(cache_pc stub, coarse_info_t *info /*OPTIONAL*/);
void
link_entrance_stub(dcontext_t *dcontext, cache_pc stub, cache_pc tgt, bool hot_patch,
                   coarse_info_t *info /*OPTIONAL*/);
void
unlink_entrance_stub(dcontext_t *dcontext, cache_pc stub, uint flags,
                     coarse_info_t *info /*OPTIONAL*/);
cache_pc
entrance_stub_from_cti(cache_pc cti);

uint
coarse_exit_prefix_size(coarse_info_t *info);

byte *
emit_coarse_exit_prefix(dcontext_t *dcontext, byte *pc, coarse_info_t *info);

/* Update info pointer in exit prefixes */
void
patch_coarse_exit_prefix(dcontext_t *dcontext, coarse_info_t *info);

bool
special_ibl_xfer_is_thread_private(void);

void
link_special_ibl_xfer(dcontext_t *dcontext);

void
unlink_special_ibl_xfer(dcontext_t *dcontext);

cache_pc
get_client_ibl_xfer_entry(dcontext_t *dcontext);

#ifdef UNIX
cache_pc
get_native_plt_ibl_xfer_entry(dcontext_t *dcontext);

cache_pc
get_native_ret_ibl_xfer_entry(dcontext_t *dcontext);
#endif

enum {
#ifdef X86
    /* size of 32-bit-offset jcc instr, assuming it has no
     * jcc branch hint!
     */
    CBR_LONG_LENGTH = 6,
    JMP_LONG_LENGTH = 5,
    JMP_SHORT_LENGTH = 2,
    CBR_SHORT_REWRITE_LENGTH = 9, /* FIXME: use this in mangle.c */
    RET_0_LENGTH = 1,
    PUSH_IMM32_LENGTH = 5,
    POPF_LENGTH = 1,

    /* size of 32-bit call and jmp instructions w/o prefixes. */
    CTI_IND1_LENGTH = 2,    /* FF D6             call esi                      */
    CTI_IND2_LENGTH = 3,    /* FF 14 9E          call dword ptr [esi+ebx*4]    */
    CTI_IND3_LENGTH = 4,    /* FF 54 B3 08       call dword ptr [ebx+esi*4+8]  */
    CTI_DIRECT_LENGTH = 5,  /* E8 9A 0E 00 00    call 7C8024CB                 */
    CTI_IAT_LENGTH = 6,     /* FF 15 38 10 80 7C call dword ptr ds:[7C801038h] */
    CTI_FAR_ABS_LENGTH = 7, /* 9A 1B 07 00 34 39 call 0739:3400071B            */
                            /* 07                                              */
#elif defined(AARCH64)
    CBR_LONG_LENGTH = 4,
    JMP_LONG_LENGTH = 4,
    JMP_SHORT_LENGTH = 4,
    CBR_SHORT_REWRITE_LENGTH = 4,
    SVC_LENGTH = 4,
#elif defined(ARM)
    CBR_LONG_LENGTH = ARM_INSTR_SIZE,
    JMP_LONG_LENGTH = ARM_INSTR_SIZE,
    JMP_SHORT_LENGTH = THUMB_SHORT_INSTR_SIZE,
    CBR_SHORT_REWRITE_LENGTH = 6,
    SVC_THUMB_LENGTH = THUMB_SHORT_INSTR_SIZE, /* Thumb syscall instr */
    SVC_ARM_LENGTH = ARM_INSTR_SIZE,           /* ARM syscall instr */
#elif defined(RISCV64)
    CBR_LONG_LENGTH = 4,
    JMP_LONG_LENGTH = 4,
    JMP_SHORT_LENGTH = 4,
    CBR_SHORT_REWRITE_LENGTH = 4,
#endif

/* Not under defines so we can have code that is less cluttered */
/* FIXME i#3544: With Compressed ext ecall can be 2 */
#if defined(AARCH64) || defined(RISCV64)
    INT_LENGTH = 4,
    SYSCALL_LENGTH = 4,
    SYSENTER_LENGTH = 4,
#else
    INT_LENGTH = 2,
    SYSCALL_LENGTH = 2,
    SYSENTER_LENGTH = 2,
#endif
};

#define REL32_REACHABLE_OFFS(offs) ((offs) <= INT_MAX && (offs) >= INT_MIN)
/* source should be the end of a rip-relative-referencing instr */
#define REL32_REACHABLE(source, target) REL32_REACHABLE_OFFS((target) - (source))

/* If code_buf points to a jmp rel32 returns true and returns the target of
 * the jmp in jmp_target as if was located at app_loc. */
bool
is_jmp_rel32(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */);

/* If code_buf points to a jmp rel8 returns true and returns the target of
 * the jmp in jmp_target as if was located at app_loc. */
bool
is_jmp_rel8(byte *code_buf, app_pc app_loc, app_pc *jmp_target /* OUT */);

/* in interp.c */

/* An upper bound on instructions added to a bb when added to a trace,
 * which is of course highest for the case of indirect branch mangling.
 * Normal lea, jecxz, lea is 14, NATIVE_RETURN (now removed) could get above 20,
 * but this should cover everything, fine to be well above, this is
 * only used to keep below the maximum trace size for the next bb,
 * we calculate the exact size in fixup_last_cti().
 *
 * For x64 we have to increase this (PR 333576 hit this):
 *    +19   L4  65 48 89 0c 25 10 00 mov    %rcx -> %gs:0x10
 *              00 00
 *    +28   L4  48 8b c8             mov    %rax -> %rcx
 *    +31   L4  e9 1b e2 f6 ff       jmp    $0x00000000406536e0 <shared_bb_ibl_indjmp>
 *    (+36)
 *   =>
 *    +120  L0  65 48 89 0c 25 10 00 mov    %rcx -> %gs:0x10
 *              00 00
 *    +129  L0  48 8b c8             mov    %rax -> %rcx
 *    +132  L3  65 48 a3 00 00 00 00 mov    %rax -> %gs:0x00
 *              00 00 00 00
 *    +143  L3  48 b8 23 24 93 28 00 mov    $0x0000000028932423 -> %rax
 *              00 00 00
 *    +153  L3  65 48 a3 08 00 00 00 mov    %rax -> %gs:0x08
 *              00 00 00 00
 *    +164  L3  9f                   lahf   -> %ah
 *    +165  L3  0f 90 c0             seto   -> %al
 *    +168  L3  65 48 3b 0c 25 08 00 cmp    %rcx %gs:0x08
 *              00 00
 *    +177  L4  0f 85 a9 d7 f6 ff    jnz    $0x000000004065312f <shared_trace_cmp_indjmp>
 *    +183  L3  65 48 8b 0c 25 10 00 mov    %gs:0x10 -> %rcx
 *              00 00
 *    +192  L3  04 7f                add    $0x7f %al -> %al
 *    +194  L3  9e                   sahf   %ah
 *    +195  L3  65 48 a1 00 00 00 00 mov    %gs:0x00 -> %rax
 *              00 00 00 00
 *    +206
 *
 *    (36-19)=17 vs (206-120)=86 => 69 bytes.  was 65 bytes prior to PR 209709!
 *    usually 3 bytes smaller since don't need to restore eflags.
 */
#define TRACE_CTI_MANGLE_SIZE_UPPER_BOUND 72

fragment_t *
build_basic_block_fragment(dcontext_t *dcontext, app_pc start_pc, uint initial_flags,
                           bool linked, bool visible, bool for_trace,
                           instrlist_t **unmangled_ilist);

void
interp(dcontext_t *dcontext);
uint
extend_trace(dcontext_t *dcontext, fragment_t *f, linkstub_t *prev_l);
int
append_trace_speculate_last_ibl(dcontext_t *dcontext, instrlist_t *trace,
                                app_pc speculate_next_tag, bool record_translation);

/* XXX i#5062 In the long term we should have this only called in mangle_trace()
 * and this function would be removed from end_and_emit_trace and
 * recreate_fragment_ilist, which are where it is currently called.
 */
#ifdef AARCH64
int
fixup_indirect_trace_exit(dcontext_t *dcontext, instrlist_t *trace);
#endif

uint
forward_eflags_analysis(dcontext_t *dcontext, instrlist_t *ilist, instr_t *instr);

/* Converts instr_t EFLAGS_ flags to corresponding fragment_t FRAG_ flags,
 * assuming that the instr_t flags correspond to the start of the fragment_t
 */
uint
instr_eflags_to_fragment_eflags(uint instr_eflags);

instrlist_t *
decode_fragment(dcontext_t *dcontext, fragment_t *f, byte *buf, /*OUT*/ uint *bufsz,
                uint target_flags, /*OUT*/ uint *dir_exits, /*OUT*/ uint *indir_exits);

instrlist_t *
decode_fragment_exact(dcontext_t *dcontext, fragment_t *f, byte *buf,
                      /*IN/OUT*/ uint *bufsz, uint target_flags,
                      /*OUT*/ uint *dir_exits, /*OUT*/ uint *indir_exits);

fragment_t *
copy_fragment(dcontext_t *dcontext, fragment_t *f, bool replace);
void
shift_ctis_in_fragment(dcontext_t *dcontext, fragment_t *f, ssize_t shift, cache_pc start,
                       cache_pc end, size_t old_size);
#ifdef PROFILE_RDTSC
void
add_profile_call(dcontext_t *dcontext);
#endif
app_pc
d_r_emulate(dcontext_t *dcontext, app_pc pc, priv_mcontext_t *mc);

bool
instr_is_trace_cmp(dcontext_t *dcontext, instr_t *inst);

typedef struct {
    app_pc region_start;
    app_pc region_end;
    app_pc start_pc;
    app_pc min_pc;
    app_pc max_pc;
    app_pc bb_end;
    bool contiguous;
    bool overlap;
} overlap_info_t;

instrlist_t *
build_app_bb_ilist(dcontext_t *dcontext, byte *start_pc, file_t outf);

void
bb_build_abort(dcontext_t *dcontext, bool clean_vmarea, bool unlock);

bool
expand_should_set_translation(dcontext_t *dcontext);

/* Builds an instrlist_t as though building a bb from pc.
 * Use recreate_fragment_ilist() for building an instrlist_t for a fragment.
 * If check_vm_area is false, Does NOT call check_thread_vm_area()!
 *   Make sure you know it will terminate at the right spot.  It does
 *   check selfmod and native_exec for elision, but otherwise will
 *   follow ubrs to the limit.  Currently used for
 *   record_translation_info() (case 3559).
 */
instrlist_t *
recreate_bb_ilist(dcontext_t *dcontext, byte *pc, byte *pretend_pc,
                  app_pc stop_pc /*optional, only for full_decode*/, uint flags,
                  uint *res_flags, uint *res_exit_type, bool check_vm_area, bool mangle,
                  void **vmlist, bool call_client, bool for_trace);

instrlist_t *
recreate_fragment_ilist(dcontext_t *dcontext, byte *pc,
                        /*IN/OUT*/ fragment_t **f_res, /*OUT*/ bool *alloc, bool mangle,
                        bool call_client);

app_pc
find_app_bb_end(dcontext_t *dcontext, byte *start_pc, uint flags);
bool
app_bb_overlaps(dcontext_t *dcontext, byte *start_pc, uint flags, byte *region_start,
                byte *region_end, overlap_info_t *info_res);

bool
reached_image_entry_yet(void);

void
set_reached_image_entry(void);

/* in mangle.c */
void
insert_clean_call_with_arg_jmp_if_ret_true(dcontext_t *dcontext, instrlist_t *ilist,
                                           instr_t *instr, void *callee, int arg,
                                           app_pc jmp_tag, instr_t *jmp_instr);

#ifdef UNIX
bool
mangle_syscall_code(dcontext_t *dcontext, fragment_t *f, byte *pc, bool skip);
#endif
void
finalize_selfmod_sandbox(dcontext_t *dcontext, fragment_t *f);

bool
instr_check_xsp_mangling(dcontext_t *dcontext, instr_t *inst, int *xsp_adjust);

bool
instr_supports_simple_mangling_epilogue(dcontext_t *dcontext, instr_t *inst);

void
float_pc_update(dcontext_t *dcontext);

void
mangle_finalize(dcontext_t *dcontext, instrlist_t *ilist, fragment_t *f);

/* in retcheck.c */
#ifdef CHECK_RETURNS_SSE2
void
finalize_return_check(dcontext_t *dcontext, fragment_t *f);
#endif
#ifdef RETURN_AFTER_CALL
void
add_return_target(dcontext_t *dcontext, app_pc instr_pc, instr_t *instr);
int
ret_after_call_check(dcontext_t *dcontext, app_pc target_addr, app_pc src_addr);
bool
is_observed_call_site(dcontext_t *dcontext, app_pc retaddr);
#endif

/* in optimize.c */
void
optimize_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace);
#ifdef DEBUG
void
print_optimization_stats(void);
#endif

#ifdef SIDELINE
/* exact overlap with sideline.h */
#    include "sideline.h"
#endif

#include "../link.h"
/* convert link flags to ibl_branch_type_t */
static inline ibl_branch_type_t
extract_branchtype(ushort linkstub_flags)
{
    if (TEST(LINK_RETURN, linkstub_flags))
        return IBL_RETURN;
    if (EXIT_IS_CALL(linkstub_flags))
        return IBL_INDCALL;
    if (TEST(LINK_JMP, linkstub_flags)) /* plain JMP or IND_JMP_PLT */
        return IBL_INDJMP;
    ASSERT_NOT_REACHED();
    return IBL_GENERIC;
}

/* convert ibl_branch_type_t to LINK_ flags */
static inline uint
ibltype_to_linktype(ibl_branch_type_t ibltype)
{
    if (ibltype == IBL_RETURN)
        return LINK_INDIRECT | LINK_RETURN;
    if (ibltype == IBL_INDCALL)
        return LINK_INDIRECT | LINK_CALL;
    if (ibltype == IBL_INDJMP)
        return LINK_INDIRECT | LINK_JMP;
    ASSERT_NOT_REACHED();
    return 0;
}

#ifdef DEBUG
bool
is_ibl_routine_type(dcontext_t *dcontext, cache_pc target, ibl_branch_type_t branch_type);
#endif /* DEBUG */

/* This completely optimizable routine is the only place where we
 * allow a data pointer to be converted to a function pointer to allow
 * better type-checking for the rest of our C code
 */
/* on x86 function pointers and data pointers are interchangeable */

static inline generic_func_t
convert_data_to_function(void *data_ptr)
{
#ifdef WINDOWS
#    pragma warning(push)
#    pragma warning(disable : 4055)
#endif
    return (generic_func_t)data_ptr;
#ifdef WINDOWS
#    pragma warning(pop)
#endif
}

/* Our version of setjmp & longjmp.  Currently used only for handling hot patch
 * exceptions and to implement an internal generic try-catch mechanism
 * later on (case 1800).
 * We could use a priv_mcontext_t here, but that has 4 extra fields that aren't
 * used here.  TODO: we could use it and live with the wastage?
 * Espcially in light of the merging from PR 218131.
 *
 * edx & eax need not be saved because they are scratch registers in a
 * call, i.e., caller-save;  they are used to return values from functions.
 * As longjmp is implemented as return from setjmp, eax & edx need not be saved.
 */
typedef struct dr_jmp_buf_t {
#ifdef X86 /* for x86.asm */
    reg_t xbx;
    reg_t xcx;
    reg_t xdi;
    reg_t xsi;
    reg_t xbp;
    reg_t xsp;
    reg_t xip;
#    ifdef X64
    /* optimization: can we trust callee-saved regs r8,r9,r10,r11 and not save them? */
    reg_t r8, r9, r10, r11, r12, r13, r14, r15;
#    endif
#elif defined(ARM)             /* for arm.asm */
#    define REGS_IN_JMP_BUF 26 /* See dr_setjmp and dr_longjmp. */
    reg_t regs[REGS_IN_JMP_BUF];
#elif defined(AARCH64)         /* for aarch64.asm */
#    define REGS_IN_JMP_BUF 22 /* See dr_setjmp and dr_longjmp. */
    reg_t regs[REGS_IN_JMP_BUF];
#elif defined(RISCV64)         /* For riscv64.asm. */
#    define REGS_IN_JMP_BUF 26 /* See dr_setjmp and dr_longjmp. */
    reg_t regs[REGS_IN_JMP_BUF];
#endif                         /* X86/AARCH64/ARM/RISCV64 */
#if defined(UNIX) && defined(DEBUG)
    /* i#226/PR 492568: we avoid the cost of storing this by using the
     * mask in the fault's signal frame, but we do record it in debug
     * build to verify our assumptions
     */
    kernel_sigset_t sigmask;
#endif
} dr_jmp_buf_t;
int
dr_longjmp(dr_jmp_buf_t *buf, int val);
int
dr_setjmp(dr_jmp_buf_t *buf);

/* Fast asm-based safe read, but requires fault handling to be set up */
bool
safe_read_fast(const void *base, size_t size, void *out_buf, size_t *bytes_read);
/* For os-specific fault recovery */
bool
is_safe_read_pc(app_pc pc);
app_pc
safe_read_resume_pc(void);

#ifdef UNIX
/* i#46: Private string routines for libc isolation. */
#    ifdef memcpy
#        undef memcpy
#    endif
#    ifdef memset
#        undef memset
#    endif
void *
memcpy(void *dst, const void *src, size_t n);
void *
memset(void *dst, int val, size_t n);
void *
memmove(void *dst, const void *src, size_t n);
#endif /* UNIX */

#ifdef UNIX
/* Private replacement for _dl_runtime_resolve() for native_exec. */
void *
_dynamorio_runtime_resolve(void);
#endif

#define DR_SETJMP(buf) (dr_setjmp(buf))

#define DR_LONGJMP(buf, val)  \
    do {                      \
        ASSERT(val != 0);     \
        dr_longjmp(buf, val); \
    } while (0)

/* Macros to access application function parameters.
 * These assume that we're at function entry, (i.e., mc->xsp points at the
 * return address on X86, or mc->sp points at the first on-stack arg on ARM).
 * Compare the SYS_PARAM* macros and REGPARM* enum: some duplication there.
 *
 * Note that, in X64, if a param is 32 bits we must ignore the top 32 bits
 * of its stack slot (Since passed via "mov dword" instead of "push", top
 * bits are garbage.)
 */
#ifdef X86
#    ifdef X64
#        ifdef WINDOWS
#            define APP_PARAM_0(mc) (mc)->xcx
#            define APP_PARAM_1(mc) (mc)->xdx
#            define APP_PARAM_2(mc) (mc)->r8
#            define APP_PARAM_3(mc) (mc)->r9
#            define APP_PARAM_4(mc) (*(((reg_t *)((mc)->xsp)) + 5))
#            define APP_PARAM_5(mc) (*(((reg_t *)((mc)->xsp)) + 6))
#            define APP_PARAM_6(mc) (*(((reg_t *)((mc)->xsp)) + 7))
#            define APP_PARAM_7(mc) (*(((reg_t *)((mc)->xsp)) + 8))
#            define APP_PARAM_8(mc) (*(((reg_t *)((mc)->xsp)) + 9))
#            define APP_PARAM_9(mc) (*(((reg_t *)((mc)->xsp)) + 10))
#            define APP_PARAM_10(mc) (*(((reg_t *)((mc)->xsp)) + 11))
#        else
#            define APP_PARAM_0(mc) (mc)->xdi
#            define APP_PARAM_1(mc) (mc)->xsi
#            define APP_PARAM_2(mc) (mc)->rdx
#            define APP_PARAM_3(mc) (mc)->rcx
#            define APP_PARAM_4(mc) (mc)->r8
#            define APP_PARAM_5(mc) (mc)->r9
#            define APP_PARAM_6(mc) (*(((reg_t *)((mc)->xsp)) + 1))
#            define APP_PARAM_7(mc) (*(((reg_t *)((mc)->xsp)) + 2))
#            define APP_PARAM_8(mc) (*(((reg_t *)((mc)->xsp)) + 3))
#            define APP_PARAM_9(mc) (*(((reg_t *)((mc)->xsp)) + 4))
#            define APP_PARAM_10(mc) (*(((reg_t *)((mc)->xsp)) + 5))
#        endif /* Win/Unix */
               /* only takes integer literals */
#        define APP_PARAM(mc, offs) APP_PARAM_##offs(mc)
#    else /* 32-bit */
/* only takes integer literals */
#        define APP_PARAM(mc, offs) (*(((reg_t *)((mc)->xsp)) + (offs) + 1))
#    endif /* 64/32-bit */
#elif defined(ARM)
#    ifdef UNIX
#        define APP_PARAM_0(mc) (mc)->r0
#        define APP_PARAM_1(mc) (mc)->r1
#        define APP_PARAM_2(mc) (mc)->r2
#        define APP_PARAM_3(mc) (mc)->r3
#        ifdef X64
#            define APP_PARAM_4(mc) (mc)->r4
#            define APP_PARAM_5(mc) (mc)->r5
#            define APP_PARAM_6(mc) (mc)->r6
#            define APP_PARAM_7(mc) (mc)->r7
#            define APP_PARAM_8(mc) (*(((reg_t *)((mc)->xsp)) + 0))
#            define APP_PARAM_9(mc) (*(((reg_t *)((mc)->xsp)) + 1))
#            define APP_PARAM_10(mc) (*(((reg_t *)((mc)->xsp)) + 2))
#        else
#            define APP_PARAM_4(mc) (*(((reg_t *)((mc)->xsp)) + 0))
#            define APP_PARAM_5(mc) (*(((reg_t *)((mc)->xsp)) + 1))
#            define APP_PARAM_6(mc) (*(((reg_t *)((mc)->xsp)) + 2))
#            define APP_PARAM_7(mc) (*(((reg_t *)((mc)->xsp)) + 3))
#            define APP_PARAM_8(mc) (*(((reg_t *)((mc)->xsp)) + 4))
#            define APP_PARAM_9(mc) (*(((reg_t *)((mc)->xsp)) + 5))
#            define APP_PARAM_10(mc) (*(((reg_t *)((mc)->xsp)) + 6))
#        endif /* 64/32-bit */
#    else      /* Windows */
#        error Windows is not supported
#    endif /* UNIX/Win */
#    define APP_PARAM(mc, offs) APP_PARAM_##offs(mc)
#endif /* X86/ARM */

#define MCXT_SYSNUM_REG(mc) ((mc)->MCXT_FLD_SYSNUM_REG)
#define MCXT_FIRST_REG_FIELD(mc) ((mc)->MCXT_FLD_FIRST_REG)

static inline reg_t
get_mcontext_frame_ptr(dcontext_t *dcontext, priv_mcontext_t *mc)
{
    reg_t reg;
    switch (dr_get_isa_mode(dcontext)) {
#ifdef X86
    case DR_ISA_IA32:
    case DR_ISA_AMD64: reg = mc->xbp; break;
#elif defined(ARM)
    case DR_ISA_ARM_THUMB: reg = mc->r7; break;
    case DR_ISA_ARM_A32: reg = mc->r11; break;
#elif defined(AARCH64)
    case DR_ISA_ARM_A64: reg = mc->r29; break;
#elif defined(RISCV64)
    case DR_ISA_RV64IMAFDC: reg = mc->x8; break;
#endif /* X86/ARM/AARCH64 */
    default: ASSERT_NOT_REACHED(); reg = 0;
    }
    return reg;
}

/* FIXME: check on all platforms: these are for Fedora 8 and XP SP2
 * Keep in synch w/ defines in x86.asm
 */
#define CS32_SELECTOR 0x23
#define CS64_SELECTOR 0x33

#ifdef ARM
/* reset the encode state stored in dcontext used by A32 Thumb mode */
void
encode_reset_it_block(dcontext_t *dcontext);
/* Reset the encode state stored in dcontext used by A32 Thumb mode if the
 * being-freed instr is involved.
 */
void
encode_instr_freed_event(dcontext_t *dcontext, instr_t *instr);
#endif

#ifdef LINUX
/* Register state preserved on input to restartable sequences ("rseq"). */
typedef struct _rseq_entry_state_t {
    reg_t gpr[DR_NUM_GPR_REGS];
} rseq_entry_state_t;
#endif

#endif /* _ARCH_EXPORTS_H_ */
