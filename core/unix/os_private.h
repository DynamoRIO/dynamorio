/* **********************************************************
 * Copyright (c) 2011-2016 Google, Inc.  All rights reserved.
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
#include "module.h" /* for os_module_data_t */
#include "ksynch.h" /* for KSYNCH_TYPE */
#include "instr.h" /* for reg_id_t */
#include <sys/time.h> /* struct itimerval */
#include "dr_config.h" /* for dr_platform_t */

/* for inline asm */
#ifdef X86
# ifdef X64
#  define ASM_XAX "rax"
#  define ASM_XCX "rcx"
#  define ASM_XDX "rdx"
#  define ASM_XBP "rbp"
#  define ASM_XSP "rsp"
#  define ASM_XSP "rsp"
# else
#  define ASM_XAX "eax"
#  define ASM_XCX "ecx"
#  define ASM_XDX "edx"
#  define ASM_XBP "ebp"
#  define ASM_XSP "esp"
# endif /* 64/32-bit */
#elif defined(AARCH64)
# define ASM_R0 "x0"
# define ASM_R1 "x1"
# define ASM_XSP "sp"
# define ASM_XSP "sp"
# define ASM_INDJMP "br"
#elif defined(ARM)
# define ASM_R0 "r0"
# define ASM_R1 "r1"
# define ASM_XSP "sp"
# define ASM_INDJMP "bx"
#endif /* X86/ARM */

#define MACHINE_TLS_IS_DR_TLS IF_X86_ELSE(INTERNAL_OPTION(mangle_app_seg), true)


/* PR 212090: the signal we use to suspend threads */
#define SUSPEND_SIGNAL SIGUSR2

#ifdef MACOS
/* While there is no clone system call, we use the same clone flags to share
 * code more easily with Linux.
 */
#  define CLONE_VM             0x00000100
#  define CLONE_FS             0x00000200
#  define CLONE_FILES          0x00000400
#  define CLONE_SIGHAND        0x00000800
#  define CLONE_VFORK          0x00004000
#  define CLONE_PARENT         0x00008000
#  define CLONE_THREAD         0x00010000
#  define CLONE_SYSVSEM        0x00040000
#  define CLONE_SETTLS         0x00080000
#  define CLONE_PARENT_SETTID  0x00100000
#  define CLONE_CHILD_CLEARTID 0x00200000
#endif

/* Clone flags use by pthreads on Linux 2.6.38.  May need updating over time.
 */
#define PTHREAD_CLONE_FLAGS (CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND| \
                             CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS| \
                             CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID)

/* On Mac, we use the _nocancel variant to defer app-initiated thread termination */
#ifdef MACOS
# define SYSNUM_NO_CANCEL(num) num##_nocancel
#else
# define SYSNUM_NO_CANCEL(num) num
#endif

/* Maximum number of arguments to Linux syscalls. */
enum { MAX_SYSCALL_ARGS = 6 };

/* thread-local data that's os-private, for modularity */
typedef struct _os_thread_data_t {
    /* store stack info at thread startup, since stack can get fragmented in
     * /proc/self/maps w/ later mprotects and it can be hard to piece together later
     */
    app_pc stack_base;
    app_pc stack_top;

#ifdef RETURN_AFTER_CALL
    app_pc stack_bottom_pc;     /* return target in the loader at program startup */
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
    void *app_thread_areas; /* data structure for app's thread area info */
#endif
} os_thread_data_t;

enum { ARGC_PTRACE_SENTINEL = -1 };

/* This data is pushed on the stack by the ptrace injection code. */
typedef struct ptrace_stack_args_t {
    ptr_int_t argc;              /* Set to ARGC_PTRACE_SENTINEL */
    priv_mcontext_t mc;          /* Registers at attach time */
    char home_dir[MAXIMUM_PATH]; /* In case the user of the injectee is not us. */
} ptrace_stack_args_t;

/* in os.c */
void os_thread_take_over(priv_mcontext_t *mc, kernel_sigset_t *sigset);

void *os_get_priv_tls_base(dcontext_t *dcontext, reg_id_t seg);

void
os_tls_thread_exit(local_state_t *local_state);

#ifdef AARCHXX
bool
os_set_app_tls_base(dcontext_t *dcontext, reg_id_t reg, void *base);
#endif

void
set_executable_path(const char *);

uint
memprot_to_osprot(uint prot);

bool
mmap_syscall_succeeded(byte *retval);

bool
os_files_same(const char *path1, const char *path2);

extern const reg_id_t syscall_regparms[MAX_SYSCALL_ARGS];

file_t
fd_priv_dup(file_t curfd);

bool
fd_mark_close_on_exec(file_t fd);

void
fd_table_add(file_t fd, uint flags);

uint permstr_to_memprot(const char * const perm);

/* in signal.c */
struct _kernel_sigaction_t;
typedef struct _kernel_sigaction_t kernel_sigaction_t;
struct _old_sigaction_t;
typedef struct _old_sigaction_t old_sigaction_t;

void signal_init(void);
void signal_exit(void);
void signal_thread_init(dcontext_t *dcontext);
void signal_thread_exit(dcontext_t *dcontext, bool other_thread);
bool is_thread_signal_info_initialized(dcontext_t *dcontext);
void handle_clone(dcontext_t *dcontext, uint flags);
bool handle_sigaction(dcontext_t *dcontext, int sig,
                      const kernel_sigaction_t *act,
                      kernel_sigaction_t *oact, size_t sigsetsize);
void handle_post_sigaction(dcontext_t *dcontext, int sig,
                           const kernel_sigaction_t *act,
                           kernel_sigaction_t *oact, size_t sigsetsize);
#ifdef LINUX
bool handle_old_sigaction(dcontext_t *dcontext, int sig,
                          const old_sigaction_t *act,
                          old_sigaction_t *oact);
void handle_post_old_sigaction(dcontext_t *dcontext, int sig,
                           const old_sigaction_t *act,
                           old_sigaction_t *oact);
bool handle_sigreturn(dcontext_t *dcontext, bool rt);
#else
bool handle_sigreturn(dcontext_t *dcontext, void *ucxt, int style);
#endif
bool handle_sigaltstack(dcontext_t *dcontext, const stack_t *stack,
                        stack_t *old_stack);
bool handle_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                        kernel_sigset_t *oset, size_t sigsetsize);
void handle_post_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                             kernel_sigset_t *oset, size_t sigsetsize);
void handle_sigsuspend(dcontext_t *dcontext, kernel_sigset_t *set,
                       size_t sigsetsize);

#ifdef LINUX
ptr_int_t
handle_pre_signalfd(dcontext_t *dcontext, int fd, kernel_sigset_t *mask,
                    size_t sizemask, int flags);

void
signal_handle_dup(dcontext_t *dcontext, file_t src, file_t dst);

void
signal_handle_close(dcontext_t *dcontext, file_t fd);
#endif

void
sigcontext_to_mcontext(priv_mcontext_t *mc, sig_full_cxt_t *sc_full);

void
mcontext_to_sigcontext(sig_full_cxt_t *sc_full, priv_mcontext_t *mc);

bool
set_default_signal_action(int sig);

void
share_siginfo_after_take_over(dcontext_t *dcontext, dcontext_t *takeover_dc);

void
signal_set_mask(dcontext_t *dcontext, kernel_sigset_t *sigset);

void
os_terminate_via_signal(dcontext_t *dcontext, terminate_flags_t flags, int sig);

void start_itimer(dcontext_t *dcontext);
void stop_itimer(dcontext_t *dcontext);

/* handle app itimer syscalls */
void
handle_pre_setitimer(dcontext_t *dcontext,
                     int which, const struct itimerval *new_timer,
                     struct itimerval *prev_timer);
void
handle_post_setitimer(dcontext_t *dcontext, bool success,
                      int which, const struct itimerval *new_timer,
                      struct itimerval *prev_timer);
void
handle_post_getitimer(dcontext_t *dcontext, bool success,
                      int which, struct itimerval *cur_timer);

void
handle_pre_alarm(dcontext_t *dcontext, unsigned int sec);

void
handle_post_alarm(dcontext_t *dcontext, bool success, unsigned int sec);

/* not exported beyond unix/ unlike rest of clone record routines */
void
set_clone_record_fields(void *record, reg_t app_thread_xsp, app_pc continuation_pc,
                        uint clone_sysnum, uint clone_flags);

/* in pcprofile.c */
void pcprofile_thread_init(dcontext_t *dcontext, bool shared_itimer, void *parent_info);
void pcprofile_fork_init(dcontext_t *dcontext);

void os_request_live_coredump(const char *msg);

#ifdef VMX86_SERVER
#  include "vmkuw.h"
#endif

/* in loader.c */
void *privload_tls_init(void *app_tp);
void  privload_tls_exit(void *dr_tp);
#ifdef ANDROID
bool get_kernel_args(int *argc OUT, char ***argv OUT, char ***envp OUT);
void init_android_version(void);
#endif

/* in nudgesig.c */
bool
create_nudge_signal_payload(siginfo_t *info OUT, uint action_mask,
                            client_id_t client_id, uint64 client_arg);

#ifdef X86
/* In x86.asm */
void *safe_read_tls_base(void);
void *safe_read_tls_recover(void);
#endif

#endif /* _OS_PRIVATE_H_ */
