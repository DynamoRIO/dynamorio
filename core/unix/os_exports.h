/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
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

#ifndef NOT_DYNAMORIO_CORE_PROPER
# define getpid getpid_forbidden_use_get_process_id
#endif

#ifdef MACOS
/* We end up de-referencing the symlink so we rely on a prefix match */
# define DYNAMORIO_LIBRARY_NAME "libdynamorio."
# define DYNAMORIO_PRELOAD_NAME "libdrpreload.dylib"
#else
# define DYNAMORIO_LIBRARY_NAME "libdynamorio.so"
# define DYNAMORIO_PRELOAD_NAME "libdrpreload.so"
#endif

/* The smallest granularity the OS supports */
#define OS_ALLOC_GRANULARITY     (4*1024)
#define MAP_FILE_VIEW_ALIGNMENT  (4*1024)

/* We steal a segment register, and so use fs for x86 (where pthreads
 * uses gs) and gs for x64 (where pthreads uses fs) (presumably to
 * avoid conflicts w/ wine).
 * Keep this consistent w/ the TLS_SEG_OPCODE define in arch/instr.h
 * and TLS_SEG in arch/asm_defines.asm
 *
 * PR 205276 covers transparently stealing our segment selector.
 */
#ifdef X86
# ifdef X64
#  define SEG_TLS SEG_GS
#  define ASM_SEG "%gs"
#  define LIB_SEG_TLS SEG_FS /* libc+loader tls */
#  define LIB_ASM_SEG "%fs"
# else
#  define SEG_TLS SEG_FS
#  define ASM_SEG "%fs"
#  define LIB_SEG_TLS SEG_GS /* libc+loader tls */
#  define LIB_ASM_SEG "%gs"
# endif
#elif defined(AARCHXX)
/* The SEG_TLS is not preserved by all kernels (older 32-bit, or all 64-bit), so we
 * end up having to steal the app library TPID register for priv lib use.
 * When in DR state, we steal a field inside the priv lib TLS to store the DR base.
 * When in app state in the code cache, we steal a GPR (r10 by default) to store
 * the DR base.
 */
# ifdef X64
#  define SEG_TLS      DR_REG_TPIDRRO_EL0 /* DR_REG_TPIDRURO, but we can't use it */
#  define LIB_SEG_TLS  DR_REG_TPIDR_EL0   /* DR_REG_TPIDRURW, libc+loader tls */
# else
#  define SEG_TLS      DR_REG_TPIDRURW /* not restored by older kernel => we can't use */
#  define LIB_SEG_TLS  DR_REG_TPIDRURO /* libc+loader tls */
# endif /* 64/32-bit */
#endif /* X86/ARM */

#define TLS_REG_LIB  LIB_SEG_TLS  /* TLS reg commonly used by libraries in Linux */
#define TLS_REG_ALT  SEG_TLS      /* spare TLS reg, used by DR in X86 Linux */

#ifdef X86
# define DR_REG_SYSNUM REG_EAX /* not XAX */
#elif defined(ARM)
# define DR_REG_SYSNUM DR_REG_R7
#elif defined(AARCH64)
# define DR_REG_SYSNUM DR_REG_X8
#else
# error NYI
#endif

#ifdef AARCHXX
# ifdef ANDROID
/* We have our own slot at the end of our instance of Android's pthread_internal_t.
 * However, its offset varies by Android version, requiring indirection through
 * a variable.
 */
#  ifdef AARCH64
#   error NYI
#  else
extern uint android_tls_base_offs;
#   define DR_TLS_BASE_OFFSET  android_tls_base_offs
#  endif
# else
/* The TLS slot for DR's TLS base.
 * On ARM, we use the 'private' field of the tcbhead_t to store DR TLS base,
 * as we can't use the alternate TLS register b/c the kernel doesn't preserve it.
 *   typedef struct
 *   {
 *     dtv_t *dtv;
 *     void *private;
 *   } tcbhead_t;
 * When using the private loader, we control all the TLS allocation and
 * should be able to avoid using that field.
 * This is also used in asm code, so we use literal instead of sizeof.
 */
#  define DR_TLS_BASE_OFFSET   IF_X64_ELSE(8, 4) /* skip dtv */
# endif
/* opcode for reading usr mode TLS base (user-read-only-thread-ID-register)
 * mrc p15, 0, reg_app, c13, c0, 3
 */
# define USR_TLS_REG_OPCODE 3
# define USR_TLS_COPROC_15 15
#endif

void *get_tls(ushort tls_offs);
void set_tls(ushort tls_offs, void *value);
byte *os_get_dr_tls_base(dcontext_t *dcontext);

/* in os.c */
void os_file_init(void);
void os_fork_init(dcontext_t *dcontext);
void os_thread_stack_store(dcontext_t *dcontext);
app_pc get_dynamorio_dll_end(void);
thread_id_t get_tls_thread_id(void);
thread_id_t get_sys_thread_id(void);
bool is_thread_terminated(dcontext_t *dcontext);
void os_wait_thread_terminated(dcontext_t *dcontext);
void os_wait_thread_detached(dcontext_t *dcontext);
void os_signal_thread_detach(dcontext_t *dcontext);
void os_tls_pre_init(int gdt_index);
/* XXX: reg_id_t is not defined here, use ushort instead */
ushort os_get_app_tls_base_offset(ushort/*reg_id_t*/ seg);
ushort os_get_app_tls_reg_offset(ushort/*reg_id_t*/ seg);
void *os_get_app_tls_base(dcontext_t *dcontext, ushort/*reg_id_t*/ seg);
void os_swap_context_go_native(dcontext_t *dcontext, dr_state_flags_t flags);

#ifdef DEBUG
void os_enter_dynamorio(void);
#endif

/* We do NOT want our libc routines wrapped by pthreads, so we use
 * our own syscall wrappers.
 */
int open_syscall(const char *file, int flags, int mode);
int close_syscall(int fd);
int dup_syscall(int fd);
ssize_t read_syscall(int fd, void *buf, size_t nbytes);
ssize_t write_syscall(int fd, const void *buf, size_t nbytes);
void exit_process_syscall(long status);
void exit_thread_syscall(long status);
process_id_t get_parent_id(void);

/* i#238/PR 499179: our __errno_location isn't affecting libc so until
 * we have libc independence or our own private isolated libc we need
 * to preserve the app's libc's errno
 */
int get_libc_errno(void);
void set_libc_errno(int val);

/* i#46: Our env manipulation routines. */
#ifdef STATIC_LIBRARY
/* For STATIC_LIBRARY, we want to support the app setting DYNAMORIO_OPTIONS
 * after our constructor ran: thus we do not want to cache the environ pointer.
 */
extern char **environ;
# define our_environ environ
#else
extern char **our_environ;
#endif
void dynamorio_set_envp(char **envp);
#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
/* drinjectlib wants the libc version while the core wants the private version */
# define getenv our_getenv
#endif
char *our_getenv(const char *name);

/* to avoid unsetenv problems we have our own unsetenv */
#define unsetenv our_unsetenv
/* XXX: unsetenv is unsafe to call during init, as it messes up access to auxv!
 * Use disable_env instead.  Xref i#909.
 */
int our_unsetenv(const char *name);
bool disable_env(const char *name);

/* new segment support
 * name is a string
 * wx should contain one of the strings "w", "wx", "x", or ""
 */
/* FIXME: also want control over where in rw region or ro region this
 * section goes -- for cl, order linked seems to do it, but for linux
 * will need a linker script (see unix/os.c for the nspdata problem)
 */
#ifdef MACOS
/* XXX: currently assuming all custom sections are writable and non-executable! */
# define DECLARE_DATA_SECTION(name, wx) \
     asm(".section __DATA,"name); \
     asm(".align 12"); /* 2^12 */
#else
# ifdef X86
#  define DECLARE_DATA_SECTION(name, wx) \
     asm(".section "name", \"a"wx"\", @progbits"); \
     asm(".align 0x1000");
# elif defined(AARCHXX)
#  define DECLARE_DATA_SECTION(name, wx) \
     asm(".section "name", \"a"wx"\""); \
     asm(".align 12"); /* 2^12 */
# endif /* X86/ARM */
#endif /* MACOS/UNIX */

/* XXX i#465: It's unclear what section we should switch to after our section
 * declarations.  gcc 4.3 seems to switch back to text at the start of every
 * function, while gcc >= 4.6 seems to emit all code together without extra
 * section switches.  Since earlier versions of gcc do their own switching and
 * the latest versions expect .text, we choose to switch to the text section.
 */
#ifdef MACOS
# define END_DATA_SECTION_DECLARATIONS() \
     asm(".section __DATA,.data"); \
     asm(".align 12"); \
     asm(".text");
#else
# ifdef X86
#  define END_DATA_SECTION_DECLARATIONS() \
     asm(".section .data"); \
     asm(".align 0x1000"); \
     asm(".text");
# elif defined(AARCHXX)
#  define END_DATA_SECTION_DECLARATIONS() \
     asm(".section .data"); \
     asm(".align 12"); \
     asm(".text");
# endif /* X86/ARM */
#endif

/* the VAR_IN_SECTION macro change where each var goes */
#define START_DATA_SECTION(name, wx) /* nothing */
#define END_DATA_SECTION() /* nothing */

/* Any assignment, even to 0, puts vars in current .data and not .bss for cl,
 * but for gcc we need to explicitly declare which section.  We still need
 * the .section asm above to give section attributes and alignment.
 */
#ifdef MACOS
# define VAR_IN_SECTION(name) __attribute__ ((section ("__DATA,"name)))
#else
# define VAR_IN_SECTION(name) __attribute__ ((section (name)))
#endif

/* Location of vsyscall "vdso" page.  Even when vdso is 2 pages we assume the
 * vsyscall is on the 1st page (i#1583).
 */
extern app_pc vsyscall_page_start;
/* pc of the end of the syscall instr itself */
extern app_pc vsyscall_syscall_end_pc;
/* pc where kernel returns control after sysenter vsyscall */
extern app_pc vsyscall_sysenter_return_pc;
/* pc where our hook-displaced code was copied */
extern app_pc vsyscall_sysenter_displaced_pc;
#define VSYSCALL_PAGE_MAPS_NAME "[vdso]"

bool is_thread_create_syscall(dcontext_t *dcontext);
bool was_thread_create_syscall(dcontext_t *dcontext);
bool is_sigreturn_syscall(dcontext_t *dcontext);
bool was_sigreturn_syscall(dcontext_t *dcontext);
bool ignorable_system_call(int num, instr_t *gateway, dcontext_t *dcontext_live);

bool kernel_is_64bit(void);

void
os_handle_mov_seg(dcontext_t *dcontext, byte *pc);

void init_emulated_brk(app_pc exe_end);

/***************************************************************************/
/* in signal.c */

/* defines and typedefs exported for dr_jmp_buf_t */

#define NUM_NONRT   32 /* includes 0 */
#define OFFS_RT     32
#ifdef LINUX
#  define NUM_RT    33 /* RT signals are [32..64] inclusive, hence 33. */
#else
#  define NUM_RT     0 /* no RT signals */
#endif
/* MAX_SIGNUM is the highest valid signum. */
#define MAX_SIGNUM  ((OFFS_RT) + (NUM_RT) - 1)
/* i#336: MAX_SIGNUM is a valid signal, so we must allocate space for it.
 */
#define SIGARRAY_SIZE (MAX_SIGNUM + 1)

/* size of long */
#ifdef X64
# define _NSIG_BPW 64
#else
# define _NSIG_BPW 32
#endif

#ifdef LINUX
# define _NSIG_WORDS (MAX_SIGNUM / _NSIG_BPW)
#else
# define _NSIG_WORDS 1 /* avoid 0 */
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

void receive_pending_signal(dcontext_t *dcontext);
bool is_signal_restorer_code(byte *pc, size_t *len);
bool is_currently_on_sigaltstack(dcontext_t *dcontext);

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

void *
#ifdef MACOS
create_clone_record(dcontext_t *dcontext, reg_t *app_xsp,
                    app_pc thread_func, void *func_arg);
#else
create_clone_record(dcontext_t *dcontext, reg_t *app_xsp);
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

# ifndef AARCH64
uint /* dr_isa_mode_t but we have a header ordering problem */
get_clone_record_isa_mode(void *record);
# endif

void
set_thread_register_from_clone_record(void *record);

void
set_app_lib_tls_base_from_clone_record(dcontext_t *dcontext, void *record);
#endif

void
os_clone_post(dcontext_t *dcontext);

app_pc
signal_thread_inherit(dcontext_t *dcontext, void *clone_record);

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
void pcprofile_fragment_deleted(dcontext_t *dcontext, fragment_t *f);
void pcprofile_thread_exit(dcontext_t *dcontext);

/* in stackdump.c */
/* fork, dump core, and use gdb for complete stack trace */
void stackdump(void);
/* use backtrace feature of glibc for quick but sometimes incomplete trace */
void glibc_stackdump(int fd);

/* nudgesig.c */
bool
send_nudge_signal(process_id_t pid, uint action_mask,
                  client_id_t client_id, uint64 client_arg);

/* module.c */
/* source_fragment is the start pc of the fragment to be run under DR */
bool
at_dl_runtime_resolve_ret(dcontext_t *dcontext, app_pc source_fragment, int *ret_imm);
#endif /* _OS_EXPORTS_H_ */
