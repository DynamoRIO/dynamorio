/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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
 * os_exports.h - UNIX-specific exported declarations
 */

#ifndef _OS_EXPORTS_H_
#define _OS_EXPORTS_H_ 1

#include <stdarg.h>
#include "../os_shared.h"
#include "os_public.h"
/* arch_exports.h exports opnd.h, but relies on kernel_sigset_t from this header.
 * We thus directly include opnd.h here for reg_id_t to resolve the circular
 * dependence (cleaner than having to use ushort instead of reg_id_t).
 */
#include "../ir/opnd.h"

#ifndef NOT_DYNAMORIO_CORE_PROPER
#    define getpid getpid_forbidden_use_get_process_id
#endif

#ifdef MACOS
/* We end up de-referencing the symlink so we rely on a prefix match */
#    define DYNAMORIO_PRELOAD_NAME "libdrpreload.dylib"
#else
#    define DYNAMORIO_PRELOAD_NAME "libdrpreload.so"
#endif

/* The smallest granularity the OS supports */
#define OS_ALLOC_GRANULARITY (4 * 1024)
#define MAP_FILE_VIEW_ALIGNMENT (4 * 1024)

/* We steal a segment register, and so use fs for x86 (where pthreads
 * uses gs) and gs for x64 (where pthreads uses fs) (presumably to
 * avoid conflicts w/ wine).
 * Keep this consistent w/ the TLS_SEG_OPCODE define in ir/instr.h
 * and TLS_SEG in arch/asm_defines.asm
 *
 * PR 205276 covers transparently stealing our segment selector.
 */
#ifdef X86
#    if defined(MACOS64)
#        define SEG_TLS SEG_GS     /* DR is sharing the app's segment. */
#        define LIB_SEG_TLS SEG_GS /* libc+loader tls */
#        define STR_SEG "gs"
#        define STR_LIB_SEG "gs"
#    elif defined(X64)
#        define SEG_TLS SEG_GS
#        define ASM_SEG "%gs"
#        define LIB_SEG_TLS SEG_FS /* libc+loader tls */
#        define LIB_ASM_SEG "%fs"
#        define STR_SEG "gs"
#        define STR_LIB_SEG "fs"
#    else
#        define SEG_TLS SEG_FS
#        define ASM_SEG "%fs"
#        define LIB_SEG_TLS SEG_GS /* libc+loader tls */
#        define LIB_ASM_SEG "%gs"
#        define STR_SEG "fs"
#        define STR_LIB_SEG "gs"
#    endif
#elif defined(AARCHXX)
/* The SEG_TLS is not preserved by all kernels (older 32-bit, or all 64-bit), so we
 * end up having to steal the app library TPID register for priv lib use.
 * When in DR state, we steal a field inside the priv lib TLS to store the DR base.
 * When in app state in the code cache, we steal a GPR (r10 by default) to store
 * the DR base.
 */
#    ifdef X64
#        ifdef MACOS
#            define SEG_TLS DR_REG_TPIDR_EL0       /* cpu number */
#            define LIB_SEG_TLS DR_REG_TPIDRRO_EL0 /* loader tls */
#            define STR_SEG "tpidrurw"
#            define STR_LIB_SEG "tpidruro"
#        else
#            define SEG_TLS DR_REG_TPIDRRO_EL0   /* DR_REG_TPIDRURO, but can't use it */
#            define LIB_SEG_TLS DR_REG_TPIDR_EL0 /* DR_REG_TPIDRURW, libc+loader tls */
#            define STR_SEG "tpidruro"
#            define STR_LIB_SEG "tpidrurw"
#        endif
#    else
#        define SEG_TLS                                                     \
            DR_REG_TPIDRURW /* not restored by older kernel => we can't use \
                             */
#        define LIB_SEG_TLS DR_REG_TPIDRURO /* libc+loader tls */
#        define STR_SEG "tpidrurw"
#        define STR_LIB_SEG "tpidruro"
#    endif /* 64/32-bit */
#elif defined(RISCV64)
/* FIXME i#3544: Not used on RISC-V, so set to invalid. Check if this is true. */
#    define SEG_TLS DR_REG_INVALID
#    define LIB_SEG_TLS DR_REG_TP
#    define STR_SEG "<none>"
#    define STR_LIB_SEG "tp"
#endif /* X86/ARM */

#define TLS_REG_LIB LIB_SEG_TLS /* TLS reg commonly used by libraries in Linux */
#define TLS_REG_ALT SEG_TLS     /* spare TLS reg, used by DR in X86 Linux */

#ifdef X86
#    define DR_REG_SYSNUM REG_EAX /* not XAX */
#elif defined(ARM)
#    define DR_REG_SYSNUM DR_REG_R7
#elif defined(AARCH64)
#    define DR_REG_SYSNUM DR_REG_X8
#elif defined(RISCV64)
#    define DR_REG_SYSNUM DR_REG_A7
#else
#    error NYI
#endif

#ifdef MACOS64
/* FIXME i#1568: current pthread_t struct has the first TLS entry at offset 28. We should
 * provide a dynamic method to determine the first entry for forward compatability.
 * Starting w/ libpthread-218.1.3 they now leave slots 6 and 11 unused to allow
 * limited interoperability w/ code targeting the Windows x64 ABI. We steal slot 6
 * for our own use.
 */
/* XXX i#5383: This is used as *8 so it's really a slot not a byte offset. */
#    define SEG_TLS_BASE_SLOT 28 /* offset from pthread_t struct to segment base */
#    define DR_TLS_BASE_SLOT 6   /* the TLS slot for DR's TLS base */
/* offset from pthread_t struct to slot 6 */
#    define DR_TLS_BASE_OFFSET (sizeof(void *) * (SEG_TLS_BASE_SLOT + DR_TLS_BASE_SLOT))
#endif

#if defined(AARCHXX) && !defined(MACOS64)
#    ifdef ANDROID
/* We have our own slot at the end of our instance of Android's
 * pthread_internal_t. However, its offset varies by Android version, requiring
 * indirection through a variable.
 */
#        ifdef AARCH64
#            error NYI
#        else
extern uint android_tls_base_offs;
#            define DR_TLS_BASE_OFFSET android_tls_base_offs
#        endif
#    else
/* The TLS slot for DR's TLS base.
 * On ARM, we use the 'private' field of the tcbhead_t to store DR TLS base,
 * as we can't use the alternate TLS register b/c the kernel doesn't preserve it.
 *   typedef struct
 *   {
 *     dtv_t *dtv;
 *     void *private;
 *   } tcb_head_t;
 * When using the private loader, we control all the TLS allocation and
 * should be able to avoid using that field.
 * This is also used in asm code, so we use literal instead of sizeof.
 */
#        define DR_TLS_BASE_OFFSET IF_X64_ELSE(8, 4) /* skip dtv */
#    endif
/* opcode for reading usr mode TLS base (user-read-only-thread-ID-register)
 * mrc p15, 0, reg_app, c13, c0, 3
 */
#    define USR_TLS_REG_OPCODE 3
#    define USR_TLS_COPROC_15 15
#endif

#ifdef RISCV64
/* Re-using ARM's approach and store DR TLS in tcb_head_t::private,
 * with the only difference being tp register points at the end of TCB.
 *
 * typedef struct
 *   {
 *     dtv_t *dtv;
 *     void *private;
 *   } tcb_head_t;
 */
#    define DR_TLS_BASE_OFFSET IF_X64_ELSE(-8, -4) /* tcb->private, skip dtv */
#endif

#ifdef LINUX
#    include "include/clone3.h"
#endif

void *
d_r_get_tls(ushort tls_offs);
void
d_r_set_tls(ushort tls_offs, void *value);
byte *
os_get_dr_tls_base(dcontext_t *dcontext);

/* in os.c */
void
os_file_init(void);
void
os_fork_init(dcontext_t *dcontext);
void
os_thread_stack_store(dcontext_t *dcontext);
app_pc
get_dynamorio_dll_end(void);
thread_id_t
get_tls_thread_id(void);
thread_id_t
get_sys_thread_id(void);
bool
is_thread_terminated(dcontext_t *dcontext);
void
os_wait_thread_terminated(dcontext_t *dcontext);
void
os_wait_thread_detached(dcontext_t *dcontext);
void
os_signal_thread_detach(dcontext_t *dcontext);
void
os_tls_pre_init(int gdt_index);
ushort
os_get_app_tls_base_offset(reg_id_t seg);
ushort
os_get_app_tls_reg_offset(reg_id_t seg);
void *
os_get_app_tls_base(dcontext_t *dcontext, reg_id_t seg);

#ifdef DEBUG
void
os_enter_dynamorio(void);
#endif

/* We do NOT want our libc routines wrapped by pthreads, so we use
 * our own syscall wrappers.
 */
int
open_syscall(const char *file, int flags, int mode);
int
close_syscall(int fd);
int
dup_syscall(int fd);
ssize_t
read_syscall(int fd, void *buf, size_t nbytes);
ssize_t
write_syscall(int fd, const void *buf, size_t nbytes);
void
exit_process_syscall(long status);
void
exit_thread_syscall(long status);
process_id_t
get_parent_id(void);

/* i#238/PR 499179: our __errno_location isn't affecting libc so until
 * we have libc independence or our own private isolated libc we need
 * to preserve the app's libc's errno
 */
int
get_libc_errno(void);
void
set_libc_errno(int val);

/* i#46: Our env manipulation routines. */
#ifdef STATIC_LIBRARY
/* For STATIC_LIBRARY, we want to support the app setting DYNAMORIO_OPTIONS
 * after our constructor ran: thus we do not want to cache the environ pointer.
 */
extern char **environ;
#    define our_environ environ
#else
extern char **our_environ;
#endif
bool
is_our_environ_followed_by_auxv(void);
void
dynamorio_set_envp(char **envp);
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* drinjectlib wants the libc version while the core wants the private version */
#    define getenv our_getenv
#endif
char *
our_getenv(const char *name);

/* to avoid unsetenv problems we have our own unsetenv */
#define unsetenv our_unsetenv
/* XXX: unsetenv is unsafe to call during init, as it messes up access to auxv!
 * Use disable_env instead.  Xref i#909.
 */
int
our_unsetenv(const char *name);
bool
disable_env(const char *name);

/* new segment support
 * name is a string
 * wx should contain one of the strings "w", "wx", "x", or ""
 */
/* FIXME: also want control over where in rw region or ro region this
 * section goes -- for cl, order linked seems to do it, but for linux
 * will need a linker script (see unix/os.c for the nspdata problem)
 */
/* XXX i#5565: Sections are aligned to page-size because DR can enable memory
 * protection per-page (currently only on Windows). Hard-coded 4K alignment will
 * lead to issues on systems with larger base pages.
 */
#ifdef MACOS
/* XXX: currently assuming all custom sections are writable and non-executable! */
#    define DECLARE_DATA_SECTION(name, wx) \
        asm(".section __DATA," name);      \
        asm(".align 12"); /* 2^12 */
#else
#    ifdef DR_HOST_X86
#        define DECLARE_DATA_SECTION(name, wx)                \
            asm(".section " name ", \"a" wx "\", @progbits"); \
            asm(".align 0x1000");
#    elif defined(DR_HOST_AARCHXX)
#        define DECLARE_DATA_SECTION(name, wx)     \
            asm(".section " name ", \"a" wx "\""); \
            asm(".align 12"); /* 2^12 */
#    elif defined(DR_HOST_RISCV64)
#        define DECLARE_DATA_SECTION(name, wx)     \
            asm(".section " name ", \"a" wx "\""); \
            asm(".align 12"); /* 2^12 */
#    endif                    /* X86/ARM/RISCV64 */
#endif                        /* MACOS/UNIX */

/* XXX i#465: It's unclear what section we should switch to after our section
 * declarations.  gcc 4.3 seems to switch back to text at the start of every
 * function, while gcc >= 4.6 seems to emit all code together without extra
 * section switches.  Since earlier versions of gcc do their own switching and
 * the latest versions expect .text, we choose to switch to the text section.
 */
#ifdef MACOS
#    define END_DATA_SECTION_DECLARATIONS() \
        asm(".section __DATA,.data");       \
        asm(".align 12");                   \
        asm(".text");
#else
#    ifdef DR_HOST_X86
#        define END_DATA_SECTION_DECLARATIONS() \
            asm(".section .data");              \
            asm(".align 0x1000");               \
            asm(".text");
#    elif defined(DR_HOST_AARCHXX)
#        define END_DATA_SECTION_DECLARATIONS() \
            asm(".section .data");              \
            asm(".align 12");                   \
            asm(".text");
#    elif defined(DR_HOST_RISCV64)
#        define END_DATA_SECTION_DECLARATIONS() \
            asm(".section .data");              \
            asm(".align 12");                   \
            asm(".text");
#    endif /* X86/ARM */
#endif

/* the VAR_IN_SECTION macro change where each var goes */
#define START_DATA_SECTION(name, wx) /* nothing */
#define END_DATA_SECTION()           /* nothing */

/* Any assignment, even to 0, puts vars in current .data and not .bss for cl,
 * but for gcc we need to explicitly declare which section.  We still need
 * the .section asm above to give section attributes and alignment.
 */
#ifdef MACOS
#    define VAR_IN_SECTION(name) __attribute__((section("__DATA," name)))
#else
#    define VAR_IN_SECTION(name) __attribute__((section(name)))
#endif

/* Location of "vdso" page(s), or on systems pre-vdso, equals the vsyscall page. */
extern app_pc vdso_page_start;
extern size_t vdso_size;
/* Location of vsyscall page (within vdso if vdso exists). */
extern app_pc vsyscall_page_start;
/* pc of the end of the syscall instr itself */
extern app_pc vsyscall_syscall_end_pc;
/* pc where kernel returns control after sysenter vsyscall */
extern app_pc vsyscall_sysenter_return_pc;
/* pc where our hook-displaced code was copied */
extern app_pc vsyscall_sysenter_displaced_pc;
#define VSYSCALL_PAGE_MAPS_NAME "[vdso]"

bool
was_thread_create_syscall(dcontext_t *dcontext);
bool
is_sigreturn_syscall(dcontext_t *dcontext);
bool
was_sigreturn_syscall(dcontext_t *dcontext);
bool
ignorable_system_call(int num, instr_t *gateway, dcontext_t *dcontext_live);

bool
kernel_is_64bit(void);

void
os_handle_mov_seg(dcontext_t *dcontext, byte *pc);

void
init_emulated_brk(app_pc exe_end);

bool
is_DR_segment_reader_entry(app_pc pc);

/***************************************************************************/
/* in signal.c */

/* defines and typedefs exported for dr_jmp_buf_t */

#define NUM_NONRT 32 /* includes 0 */
#define OFFS_RT 32
#ifdef LINUX
#    define NUM_RT 33 /* RT signals are [32..64] inclusive, hence 33. */
#else
#    define NUM_RT 0 /* no RT signals */
#endif
/* MAX_SIGNUM is the highest valid signum. */
#define MAX_SIGNUM ((OFFS_RT) + (NUM_RT)-1)
/* i#336: MAX_SIGNUM is a valid signal, so we must allocate space for it.
 */
#define SIGARRAY_SIZE (MAX_SIGNUM + 1)

/* size of long */
#ifdef X64
#    define _NSIG_BPW 64
#else
#    define _NSIG_BPW 32
#endif

#ifdef LINUX
#    define _NSIG_WORDS (MAX_SIGNUM / _NSIG_BPW)
#else
#    define _NSIG_WORDS 1 /* avoid 0 */
#endif

/* kernel's sigset_t packs info into bits, while glibc's uses a short for
 * each (-> 8 bytes vs. 128 bytes)
 */
typedef struct _kernel_sigset_t {
#ifdef LINUX
    unsigned long sig[_NSIG_WORDS];
#elif defined(MACOS)
    unsigned int sig[_NSIG_WORDS];
#endif
} kernel_sigset_t;

void
receive_pending_signal(dcontext_t *dcontext);
bool
is_signal_restorer_code(byte *pc, size_t *len);
bool
is_currently_on_sigaltstack(dcontext_t *dcontext);

#define CONTEXT_HEAP_SIZE(sc) (sizeof(sc))
#define CONTEXT_HEAP_SIZE_OPAQUE (CONTEXT_HEAP_SIZE(sigcontext_t))

/* Points at both general-purpose regs and floating-point/SIMD state.
 * The storage for the pointed-at structs must be valid across the whole use of
 * this container struct, of course, so be careful where it's used.
 */
typedef struct _sig_full_cxt_t {
    sigcontext_t *sc;
    void *fp_simd_state;
} sig_full_cxt_t;

typedef sig_full_cxt_t os_cxt_ptr_t;

extern os_cxt_ptr_t osc_empty;

static inline bool
is_os_cxt_ptr_null(os_cxt_ptr_t osc)
{
    return osc.sc == NULL;
}

static inline void
set_os_cxt_ptr_null(os_cxt_ptr_t *osc)
{
    *osc = osc_empty;
}

/* Only one of mc and dmc can be non-NULL. */
bool
os_context_to_mcontext(dr_mcontext_t *dmc, priv_mcontext_t *mc, os_cxt_ptr_t osc);
/* Only one of mc and dmc can be non-NULL. */
bool
mcontext_to_os_context(os_cxt_ptr_t osc, dr_mcontext_t *dmc, priv_mcontext_t *mc);

void *
#ifdef MACOS
create_clone_record(dcontext_t *dcontext, reg_t *app_thread_xsp, app_pc thread_func,
                    void *func_arg);
#elif defined(LINUX)
create_clone_record(dcontext_t *dcontext, reg_t *app_thread_xsp,
                    clone3_syscall_args_t *dr_clone_args,
                    clone3_syscall_args_t *app_clone_args);
#else
create_clone_record(dcontext_t *dcontext, reg_t *app_thread_xsp);
#endif

#ifdef MACOS
void *
get_clone_record_thread_arg(void *record);
#endif

void *
get_clone_record(reg_t xsp);
reg_t
get_clone_record_app_xsp(void *record);
byte *
get_clone_record_dstack(void *record);

#ifdef AARCHXX
reg_t
get_clone_record_stolen_value(void *record);

#    ifndef AARCH64
uint /* dr_isa_mode_t but we have a header ordering problem */
get_clone_record_isa_mode(void *record);
#    endif

void
set_thread_register_from_clone_record(void *record);

void
set_app_lib_tls_base_from_clone_record(dcontext_t *dcontext, void *record);
#endif

void
restore_clone_param_from_clone_record(dcontext_t *dcontext, void *record);

void
os_clone_post(dcontext_t *dcontext);

void
signal_fork_init(dcontext_t *dcontext);

void
signal_remove_alarm_handlers(dcontext_t *dcontext);

bool
set_itimer_callback(dcontext_t *dcontext, int which, uint millisec,
                    void (*func)(dcontext_t *, priv_mcontext_t *),
                    void (*func_api)(dcontext_t *, dr_mcontext_t *));

uint
get_itimer_frequency(dcontext_t *dcontext, int which);

bool
sysnum_is_not_restartable(int sysnum);

/***************************************************************************/

/* in pcprofile.c */
void
pcprofile_fragment_deleted(dcontext_t *dcontext, fragment_t *f);
void
pcprofile_thread_exit(dcontext_t *dcontext);

/* in stackdump.c */
/* fork, dump core, and use gdb for complete stack trace */
void
d_r_stackdump(void);
/* use backtrace feature of glibc for quick but sometimes incomplete trace */
void
glibc_stackdump(int fd);

/* nudgesig.c */
bool
send_nudge_signal(process_id_t pid, uint action_mask, client_id_t client_id,
                  uint64 client_arg);

/* module.c */
/* source_fragment is the start pc of the fragment to be run under DR */
bool
at_dl_runtime_resolve_ret(dcontext_t *dcontext, app_pc source_fragment, int *ret_imm);

/* rseq_linux.c */
#ifdef LINUX
extern vm_area_vector_t *d_r_rseq_areas;

bool
rseq_get_region_info(app_pc pc, app_pc *start OUT, app_pc *end OUT, app_pc *handler OUT,
                     bool **reg_written OUT, int *reg_written_size OUT);

bool
rseq_set_final_instr_pc(app_pc start, app_pc final_instr_pc);

int
rseq_get_tls_ptr_offset(void);

int
rseq_get_signature(void);

int
rseq_get_rseq_cs_alignment(void);

byte *
rseq_get_rseq_cs_alloc(byte **rseq_cs_aligned OUT);

/* The first parameter is the value returned by rseq_get_rseq_cs_alloc(). */
void
rseq_record_rseq_cs(byte *rseq_cs_alloc, fragment_t *f, cache_pc start, cache_pc end,
                    cache_pc abort);
void
rseq_remove_fragment(dcontext_t *dcontext, fragment_t *f);

void
rseq_shared_fragment_flushtime_update(dcontext_t *dcontext);

void
rseq_process_native_abort(dcontext_t *dcontext);

void
rseq_insert_start_label(dcontext_t *dcontext, app_pc tag, instrlist_t *ilist);

#endif

#endif /* _OS_EXPORTS_H_ */
