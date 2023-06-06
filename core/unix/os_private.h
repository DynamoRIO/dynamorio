/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/*
 * os_private.h - declarations shared among os-specific files, but not
 * exported to the rest of the code
 */

#ifndef _OS_PRIVATE_H_
#define _OS_PRIVATE_H_ 1

#include <signal.h> /* for stack_t */
#include "include/siginfo.h"
#include "module.h"    /* for os_module_data_t */
#include "ksynch.h"    /* for KSYNCH_TYPE */
#include "instr.h"     /* for reg_id_t */
#include <sys/time.h>  /* struct itimerval */
#include "dr_config.h" /* for dr_platform_t */
#include "tls.h"
#include "memquery.h"
#include "../drlibc/drlibc_unix.h"

/* for inline asm */
#ifdef DR_HOST_X86
#    ifdef DR_HOST_X64
#        define ASM_XAX "rax"
#        define ASM_XCX "rcx"
#        define ASM_XDX "rdx"
#        define ASM_XBP "rbp"
#        define ASM_XSP "rsp"
#        define ASM_XSP "rsp"
#    else
#        define ASM_XAX "eax"
#        define ASM_XCX "ecx"
#        define ASM_XDX "edx"
#        define ASM_XBP "ebp"
#        define ASM_XSP "esp"
#    endif /* 64/32-bit */
#elif defined(DR_HOST_AARCH64)
#    define ASM_R0 "x0"
#    define ASM_R1 "x1"
#    define ASM_R2 "x2"
#    define ASM_R3 "x3"
#    define ASM_XSP "sp"
#    define ASM_INDJMP "br"
#elif defined(DR_HOST_ARM)
#    define ASM_R0 "r0"
#    define ASM_R1 "r1"
#    define ASM_R2 "r2"
#    define ASM_R3 "r3"
#    define ASM_XSP "sp"
#    define ASM_INDJMP "bx"
#elif defined(DR_HOST_RISCV64)
#    define ASM_R0 "a0"
#    define ASM_R1 "a1"
#    define ASM_R2 "a2"
#    define ASM_R3 "a3"
#    define ASM_XSP "sp"
#    define ASM_INDJMP "jr"
#endif /* X86/ARM */

#define MACHINE_TLS_IS_DR_TLS IF_X86_ELSE(INTERNAL_OPTION(mangle_app_seg), true)

/* The signal we use to suspend threads.
 * It may equal NUDGESIG_SIGNUM.
 */
extern int suspend_signum;
#define SUSPEND_SIGNAL suspend_signum

#ifdef MACOS
/* While there is no clone system call, we use the same clone flags to share
 * code more easily with Linux.
 */
#    define CLONE_VM 0x00000100
#    define CLONE_FS 0x00000200
#    define CLONE_FILES 0x00000400
#    define CLONE_SIGHAND 0x00000800
#    define CLONE_VFORK 0x00004000
#    define CLONE_PARENT 0x00008000
#    define CLONE_THREAD 0x00010000
#    define CLONE_SYSVSEM 0x00040000
#    define CLONE_SETTLS 0x00080000
#    define CLONE_PARENT_SETTID 0x00100000
#    define CLONE_CHILD_CLEARTID 0x00200000
#endif

/* Clone flags use by pthreads on Linux 2.6.38.  May need updating over time.
 */
#define PTHREAD_CLONE_FLAGS                                                             \
    (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | \
     CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID)

#define SYSCALL_PARAM_CLONE_STACK 1

#define SYSCALL_PARAM_CLONE3_CLONE_ARGS 0
#define SYSCALL_PARAM_CLONE3_CLONE_ARGS_SIZE 1
#define CLONE3_FLAGS_4_BYTE_MASK 0x00000000ffffffffUL

struct _os_local_state_t;

/* thread-local data that's os-private, for modularity */
typedef struct _os_thread_data_t {
    /* store stack info at thread startup, since stack can get fragmented in
     * /proc/self/maps w/ later mprotects and it can be hard to piece together later
     */
    app_pc stack_base;
    app_pc stack_top;

#ifdef RETURN_AFTER_CALL
    app_pc stack_bottom_pc; /* return target in the loader at program startup */
#endif

    /* PR 2120990: for thread suspension */
    /* This lock synchronizes suspension and resumption and controls
     * access to suspend_count and the bools below in os_thread_suspend
     * and suspend_resume.  handle_suspend_signal() does not use the
     * mutex as it is not safe to do so, but our suspend and resume
     * synch avoids any need for it there.
     */
    mutex_t suspend_lock;
    int suspend_count;

    /* Thread synchronization data held across a fork. */
    thread_record_t **fork_threads;
    int fork_num_threads;

    /* We would use event_t's here except we can't use mutexes in
     * our signal handler
     */
    /* Any function that sets these flags must also notify possibly waiting
     * thread(s). See i#96/PR 295561.
     */
    KSYNCH_TYPE suspended;
    KSYNCH_TYPE wakeup;
    KSYNCH_TYPE resumed;
    sig_full_cxt_t *suspended_sigcxt;

    /* PR 297902: for thread termination */
    bool terminate;
    /* Any function that sets this flag must also notify possibly waiting
     * thread(s). See i#96/PR 295561.
     */
    KSYNCH_TYPE terminated;

    KSYNCH_TYPE detached;
    volatile bool do_detach;

    volatile bool retakeover; /* for re-attach */

    /* PR 450670: for re-entrant suspend signals */
    int processing_signal;

    /* i#107: If -mangle_app_seg is on, these hold the bases for both SEG_TLS
     * and LIB_SEG_TLS.  If -mangle_app_seg is off, the base for LIB_SEG_TLS
     * will be NULL, but the base for SEG_TLS will still be present.
     */
    void *priv_lib_tls_base;
    void *priv_alt_tls_base;
    void *dr_tls_base;
#ifdef X86
    void *app_thread_areas;              /* data structure for app's thread area info */
    struct _os_local_state_t *clone_tls; /* i#2089: a copy for children to inherit */
#endif
} os_thread_data_t;

enum { ARGC_PTRACE_SENTINEL = -1 };

/* This data is pushed on the stack by the ptrace injection code. */
typedef struct ptrace_stack_args_t {
    ptr_int_t argc;              /* Set to ARGC_PTRACE_SENTINEL */
    priv_mcontext_t mc;          /* Registers at attach time */
    char home_dir[MAXIMUM_PATH]; /* In case the user of the injectee is not us. */
} ptrace_stack_args_t;

/* in drlibc_os.c */
byte *
mmap_syscall(byte *addr, size_t len, ulong prot, ulong flags, ulong fd, ulong offs);

long
munmap_syscall(byte *addr, size_t len);

/* in os.c */
bool
os_thread_take_over(priv_mcontext_t *mc, kernel_sigset_t *sigset);

void *
os_get_priv_tls_base(dcontext_t *dcontext, reg_id_t seg);

void
os_tls_thread_exit(local_state_t *local_state);

#ifdef AARCHXX
bool
os_set_app_tls_base(dcontext_t *dcontext, reg_id_t reg, void *base);
#endif

#ifdef MACOS
/* xref i#1404: we should expose these via the dr_get_os_version() API */
#    define MACOS_VERSION_MOJAVE 18
#    define MACOS_VERSION_HIGH_SIERRA 17
#    define MACOS_VERSION_SIERRA 16
#    define MACOS_VERSION_EL_CAPITAN 15
#    define MACOS_VERSION_YOSEMITE 14
#    define MACOS_VERSION_MAVERICKS 13
#    define MACOS_VERSION_MOUNTAIN_LION 12
#    define MACOS_VERSION_LION 11
int
os_get_version(void);
#endif

void
set_executable_path(const char *);

void
set_app_args(int *, char **);

uint
memprot_to_osprot(uint prot);

bool
mmap_syscall_succeeded(byte *retval);

bool
os_files_same(const char *path1, const char *path2);

extern const reg_id_t syscall_regparms[MAX_SYSCALL_ARGS];

void
set_syscall_param(dcontext_t *dcontext, int param_num, reg_t new_value);

file_t
fd_priv_dup(file_t curfd);

bool
fd_mark_close_on_exec(file_t fd);

void
fd_table_add(file_t fd, uint flags);

uint
permstr_to_memprot(const char *const perm);

/* The caller needs to bracket this with memquery_iterator_{start,stop}.
 * Returns the number of executable regions found in the address space.
 */
int
os_walk_address_space(memquery_iter_t *iter, bool add_modules);

bool
is_sigreturn_syscall_number(int sysnum);

bool
is_sigqueue_supported(void);

/* in signal.c */
struct _kernel_sigaction_t;
typedef struct _kernel_sigaction_t kernel_sigaction_t;
struct _old_sigaction_t;
typedef struct _old_sigaction_t old_sigaction_t;
#ifdef MACOS
/* i#2105: on Mac the old action for SYS_sigaction is a different type. */
struct _prev_sigaction_t;
typedef struct _prev_sigaction_t prev_sigaction_t;
#else
typedef kernel_sigaction_t prev_sigaction_t;
#endif

void
d_r_signal_init(void);
void
d_r_signal_exit(void);
void
signal_thread_init(dcontext_t *dcontext, void *os_data);
void
signal_thread_exit(dcontext_t *dcontext, bool other_thread);
/* In addition to the list, does not block SIGSEGV or SIGBUS. */
void
block_all_noncrash_signals_except(kernel_sigset_t *oset, int num_signals, ...);
void
block_cleanup_and_terminate(dcontext_t *dcontext, int sysnum, ptr_uint_t sys_arg1,
                            ptr_uint_t sys_arg2, bool exitproc,
                            /* these 2 args are only used for Mac thread exit */
                            ptr_uint_t sys_arg3, ptr_uint_t sys_arg4);
bool
is_thread_signal_info_initialized(dcontext_t *dcontext);
void
signal_swap_mask(dcontext_t *dcontext, bool to_app);
void
signal_remove_handlers(dcontext_t *dcontext);
void
signal_reinstate_handlers(dcontext_t *dcontext, bool ignore_alarm);
void
signal_reinstate_alarm_handlers(dcontext_t *dcontext);
void
handle_clone(dcontext_t *dcontext, uint64 flags);
/* If returns false to skip the syscall, the result is in "result". */
bool
handle_sigaction(dcontext_t *dcontext, int sig, const kernel_sigaction_t *act,
                 prev_sigaction_t *oact, size_t sigsetsize, OUT uint *result);
/* Returns the desired app return value (caller will negate if nec) */
uint
handle_post_sigaction(dcontext_t *dcontext, bool success, int sig,
                      const kernel_sigaction_t *act, prev_sigaction_t *oact,
                      size_t sigsetsize);
#ifdef LINUX
/* If returns false to skip the syscall, the result is in "result". */
bool
handle_old_sigaction(dcontext_t *dcontext, int sig, const old_sigaction_t *act,
                     old_sigaction_t *oact, OUT uint *result);
/* Returns the desired app return value (caller will negate if nec) */
uint
handle_post_old_sigaction(dcontext_t *dcontext, bool success, int sig,
                          const old_sigaction_t *act, old_sigaction_t *oact);
bool
handle_sigreturn(dcontext_t *dcontext, bool rt);
#else
bool
handle_sigreturn(dcontext_t *dcontext, void *ucxt, int style);
#endif

#ifdef LINUX
bool
handle_pre_extended_syscall_sigmasks(dcontext_t *dcontext, kernel_sigset_t *sigmask,
                                     size_t sizemask, bool *pending);

void
handle_post_extended_syscall_sigmasks(dcontext_t *dcontext, bool success);
#endif

bool
handle_sigaltstack(dcontext_t *dcontext, const stack_t *stack, stack_t *old_stack,
                   reg_t cur_xsp, OUT uint *result);

bool
handle_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                   kernel_sigset_t *oset, size_t sigsetsize, uint *error_code);
int
handle_post_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                        kernel_sigset_t *oset, size_t sigsetsize);
void
handle_sigsuspend(dcontext_t *dcontext, kernel_sigset_t *set, size_t sigsetsize);

#ifdef LINUX
ptr_int_t
handle_pre_signalfd(dcontext_t *dcontext, int fd, kernel_sigset_t *mask, size_t sizemask,
                    int flags);

void
signal_handle_dup(dcontext_t *dcontext, file_t src, file_t dst);

void
signal_handle_close(dcontext_t *dcontext, file_t fd);
#endif

void
sigcontext_to_mcontext(priv_mcontext_t *mc, sig_full_cxt_t *sc_full,
                       dr_mcontext_flags_t flags);

void
mcontext_to_sigcontext(sig_full_cxt_t *sc_full, priv_mcontext_t *mc,
                       dr_mcontext_flags_t flags);

bool
set_default_signal_action(int sig);

void
signal_thread_inherit(dcontext_t *dcontext, void *clone_record);

dcontext_t *
init_thread_with_shared_siginfo(priv_mcontext_t *mc, dcontext_t *takeover_dc);

void
signal_set_mask(dcontext_t *dcontext, kernel_sigset_t *sigset);

void
os_terminate_via_signal(dcontext_t *dcontext, terminate_flags_t flags, int sig);

bool
thread_signal(process_id_t pid, thread_id_t tid, int signum);

bool
thread_signal_queue(process_id_t pid, thread_id_t tid, int signum, void *value);

void
start_itimer(dcontext_t *dcontext);
void
stop_itimer(dcontext_t *dcontext);

/* handle app itimer syscalls */
void
handle_pre_setitimer(dcontext_t *dcontext, int which, const struct itimerval *new_timer,
                     struct itimerval *prev_timer);
void
handle_post_setitimer(dcontext_t *dcontext, bool success, int which,
                      const struct itimerval *new_timer, struct itimerval *prev_timer);
void
handle_post_getitimer(dcontext_t *dcontext, bool success, int which,
                      struct itimerval *cur_timer);

void
handle_pre_alarm(dcontext_t *dcontext, unsigned int sec);

void
handle_post_alarm(dcontext_t *dcontext, bool success, unsigned int sec);

/* not exported beyond unix/ unlike rest of clone record routines */
void
set_clone_record_fields(void *record, reg_t app_thread_xsp, app_pc continuation_pc,
                        uint clone_sysnum, uint clone_flags);

#ifdef ARM
dr_isa_mode_t
get_sigcontext_isa_mode(sig_full_cxt_t *sc_full);
void
set_sigcontext_isa_mode(sig_full_cxt_t *sc_full, dr_isa_mode_t isa_mode);
#endif

/* in pcprofile.c */
void
pcprofile_thread_init(dcontext_t *dcontext, bool shared_itimer, void *parent_info);
void
pcprofile_fork_init(dcontext_t *dcontext);

void
os_request_live_coredump(const char *msg);

#ifdef VMX86_SERVER
#    include "vmkuw.h"
#endif

/* in loader.c */
void *
privload_tls_init(void *app_tp);
void
privload_tls_exit(void *dr_tp);
#ifdef ANDROID
bool
get_kernel_args(int *argc OUT, char ***argv OUT, char ***envp OUT);
void
init_android_version(void);
#endif

/* in nudgesig.c */
bool
create_nudge_signal_payload(kernel_siginfo_t *info OUT, uint action_mask, uint flags,
                            client_id_t client_id, uint64 client_arg);

#ifdef X86
/* In x86.asm */
uint
safe_read_tls_magic(void);
void
safe_read_tls_magic_recover(void);
byte *
safe_read_tls_self(void);
void
safe_read_tls_self_recover(void);
byte *
safe_read_tls_app_self(void);
void
safe_read_tls_app_self_recover(void);
#endif

/* In module.c */
#ifdef LINUX
void
module_locate_rseq_regions(void);
bool
rseq_is_registered_for_current_thread(void);
#endif

#endif /* _OS_PRIVATE_H_ */
