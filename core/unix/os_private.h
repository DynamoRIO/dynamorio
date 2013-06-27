/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
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
#include "instr.h" /* for reg_id_t */
#include <sys/time.h> /* struct itimerval */
#include "dr_config.h" /* for dr_platform_t */

/* for inline asm */
#ifdef X64
# define ASM_XAX "rax"
# define ASM_XDX "rdx"
# define ASM_XBP "rbp"
# define ASM_XSP "rsp"
#else
# define ASM_XAX "eax"
# define ASM_XDX "edx"
# define ASM_XBP "ebp"
# define ASM_XSP "esp"
#endif

/* PR 212090: the signal we use to suspend threads */
#define SUSPEND_SIGNAL SIGUSR2

/* Clone flags use by pthreads on Linux 2.6.38.  May need updating over time.
 */
#define PTHREAD_CLONE_FLAGS (CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND| \
                             CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS| \
                             CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID)

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
     * access to suspend_count and the bools below in thread_suspend
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
    /* volatile int rather than bool since these are used as futexes.
     * 0 is unset, 1 is set, and no other value is used.
     * Any function that sets these flags must also notify possibly waiting
     * thread(s). See i#96/PR 295561.
     */
    volatile int suspended;
    volatile int wakeup;
    volatile int resumed;
    struct sigcontext *suspended_sigcxt;

    /* PR 297902: for thread termination */
    bool terminate;
    /* volatile int rather than bool since this is used as a futex.
     * 0 is unset, 1 is set, and no other value is used.
     * Any function that sets this flag must also notify possibly waiting
     * thread(s). See i#96/PR 295561.
     */
    volatile int terminated;

    volatile bool retakeover; /* for re-attach */

    /* PR 450670: for re-entrant suspend signals */
    int processing_signal;

    /* i#107: If -mangle_app_seg is on, these hold the bases for both SEG_TLS
     * and LIB_SEG_TLS.  If -mangle_app_seg is off, the base for LIB_SEG_TLS
     * will be NULL, but the base for SEG_TLS will still be present.
     */
    void *dr_fs_base;
    void *dr_gs_base;
    void *app_thread_areas; /* data structure for app's thread area info */
} os_thread_data_t;

enum { ARGC_PTRACE_SENTINEL = -1 };

/* This data is pushed on the stack by the ptrace injection code. */
typedef struct ptrace_stack_args_t {
    ptr_int_t argc;              /* Set to ARGC_PTRACE_SENTINEL */
    priv_mcontext_t mc;          /* Registers at attach time */
    char home_dir[MAXIMUM_PATH]; /* In case the user of the injectee is not us. */
} ptrace_stack_args_t;


/* in os.c */
void os_thread_take_over(priv_mcontext_t *mc);

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

/* in signal.c */
struct _kernel_sigaction_t;
typedef struct _kernel_sigaction_t kernel_sigaction_t;

void signal_init(void);
void signal_exit(void);
void signal_thread_init(dcontext_t *dcontext);
void signal_thread_exit(dcontext_t *dcontext, bool other_thread);
void handle_clone(dcontext_t *dcontext, uint flags);
bool handle_sigaction(dcontext_t *dcontext, int sig,
                      const kernel_sigaction_t *act, 
                      kernel_sigaction_t *oact, size_t sigsetsize);
void handle_post_sigaction(dcontext_t *dcontext, int sig,
                           const kernel_sigaction_t *act, 
                           kernel_sigaction_t *oact, size_t sigsetsize);
bool handle_sigreturn(dcontext_t *dcontext, bool rt);
bool handle_sigaltstack(dcontext_t *dcontext, const stack_t *stack,
                        stack_t *old_stack);
bool handle_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                        kernel_sigset_t *oset, size_t sigsetsize);
void handle_post_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                             kernel_sigset_t *oset, size_t sigsetsize);
void handle_sigsuspend(dcontext_t *dcontext, kernel_sigset_t *set,
                       size_t sigsetsize);

ptr_int_t
handle_pre_signalfd(dcontext_t *dcontext, int fd, kernel_sigset_t *mask,
                    size_t sizemask, int flags);

void
signal_handle_dup(dcontext_t *dcontext, file_t src, file_t dst);

void
signal_handle_close(dcontext_t *dcontext, file_t fd);

void
sigcontext_to_mcontext(priv_mcontext_t *mc, struct sigcontext *sc);

void
mcontext_to_sigcontext(struct sigcontext *sc, priv_mcontext_t *mc);

bool
set_default_signal_action(int sig);

void
share_siginfo_after_take_over(dcontext_t *dcontext, dcontext_t *takeover_dc);

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

/* in module.c */
bool is_elf_so_header(app_pc base, size_t size);
bool module_walk_program_headers(app_pc base, size_t view_size, bool at_map,
                                 app_pc *out_base, app_pc *out_end, char **out_soname,
                                 os_module_data_t *out_data);

uint module_num_program_headers(app_pc base);

app_pc module_vaddr_from_prog_header(app_pc prog_header, uint num_segments,
                                     OUT app_pc *mod_end);

bool module_read_program_header(app_pc base,
                                uint segment_num,
                                OUT app_pc *segment_base,
                                OUT app_pc *segment_end,
                                OUT uint *segment_prot,
                                OUT size_t *segment_align);

void os_request_live_coredump(const char *msg);
bool file_is_elf64(file_t f);
bool get_elf_platform(file_t f, dr_platform_t *platform);

/* helper routines for using futex(2). See i#96/PR 295561. in os.c */
ptr_int_t 
futex_wait(volatile int *futex, int mustbe);
ptr_int_t 
futex_wake(volatile int *futex);
ptr_int_t 
futex_wake_all(volatile int *futex);

extern bool kernel_futex_support;

#ifdef VMX86_SERVER
#  include "vmkuw.h"
#endif

/* in nudgesig.c */
bool
create_nudge_signal_payload(siginfo_t *info OUT, uint action_mask,
                            client_id_t client_id, uint64 client_arg);

#endif /* _OS_PRIVATE_H_ */
