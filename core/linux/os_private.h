/* **********************************************************
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
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
    /* We would use event_t's here except we can't use mutexes in
     * our signal handler 
     */
    bool suspended;
    bool wakeup;
    bool resumed;
    struct sigcontext *suspended_sigcxt;

    /* PR 297902: for thread termination */
    bool terminate;
    bool terminated;
} os_thread_data_t;

/* in signal.c */
struct _kernel_sigaction_t;
typedef struct _kernel_sigaction_t kernel_sigaction_t;

void signal_init(void);
void signal_exit(void);
void signal_thread_init(dcontext_t *dcontext);
void signal_thread_exit(dcontext_t *dcontext);
void handle_clone(dcontext_t *dcontext, uint flags);
bool handle_sigaction(dcontext_t *dcontext, int sig,
                      const kernel_sigaction_t *act, 
                      kernel_sigaction_t *oact, size_t sigsetsize);
void handle_post_sigaction(dcontext_t *dcontext, int sig,
                           const kernel_sigaction_t *act, 
                           kernel_sigaction_t *oact, size_t sigsetsize);
void handle_sigreturn(dcontext_t *dcontext, bool rt);
bool handle_sigaltstack(dcontext_t *dcontext, const stack_t *stack,
                        stack_t *old_stack);
void handle_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                        kernel_sigset_t *oset, size_t sigsetsize);
void handle_post_sigprocmask(dcontext_t *dcontext, int how, kernel_sigset_t *set,
                             kernel_sigset_t *oset, size_t sigsetsize);
void handle_sigsuspend(dcontext_t *dcontext, kernel_sigset_t *set,
                       size_t sigsetsize);
void
sigcontext_to_mcontext(dr_mcontext_t *mc, struct sigcontext *sc);

void
mcontext_to_sigcontext(struct sigcontext *sc, dr_mcontext_t *mc);

bool
set_default_signal_action(int sig);

/* in pcprofile.c */
void pcprofile_thread_init(dcontext_t *dcontext);
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

#ifdef VMX86_SERVER
#  include "vmkuw.h"
#endif

#endif /* _OS_PRIVATE_H_ */
