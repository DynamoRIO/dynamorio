/* **********************************************************
 * Copyright (c) 2000-2009 VMware, Inc.  All rights reserved.
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
 * os_exports.h - Linux specific exported declarations
 */

#ifndef _OS_EXPORTS_H_
#define _OS_EXPORTS_H_ 1

#include <stdarg.h>
#include "os_shared.h"

#define getpid getpid_forbidden_use_get_process_id

#define DYNAMORIO_LIBRARY_NAME "libdynamorio.so"
#define DYNAMORIO_PRELOAD_NAME "libdrpreload.so"

/* FIXME: I made these up */
#define OS_ALLOC_GRANULARITY     (ASSERT_NOT_IMPLEMENTED(false), 4*1024)
#define MAP_FILE_VIEW_ALIGNMENT  (ASSERT_NOT_IMPLEMENTED(false), 4*1024)

/* We steal a segment register, and so use fs for x86 (where pthreads
 * uses gs) and gs for x64 (where pthreads uses fs) (presumably to
 * avoid conflicts w/ wine).
 * Keep this consistent w/ the TLS_SEG_OPCODE define in x86/instr.h
 * and TLS_SEG in x86/asm_defines.asm
 *
 * PR 205276 covers transparently stealing our segment selector.
 */
#ifdef X64
# define SEG_TLS SEG_GS
# define ASM_SEG "%gs"
#else
# define SEG_TLS SEG_FS
# define ASM_SEG "%fs"
#endif

void *get_tls(ushort tls_offs);
void set_tls(ushort tls_offs, void *value);

/* in os.c */
void os_fork_init(dcontext_t *dcontext);
void os_thread_stack_store(dcontext_t *dcontext);
app_pc get_dynamorio_dll_end(void);
thread_id_t get_tls_thread_id(void);

/* We do NOT want our libc routines wrapped by pthreads, so we use
 * our own syscall wrappers.
 */
int open_syscall(const char *file, int flags, int mode);
int close_syscall(int fd);
int dup_syscall(int fd);
ssize_t read_syscall(int fd, void *buf, size_t nbytes);
ssize_t write_syscall(int fd, const void *buf, size_t nbytes);
void exit_syscall(long status);
process_id_t get_parent_id(void);

/* to avoid pthreads problems we must have our own vnsprintf */
#include <stdarg.h> /* for va_list */
int our_snprintf(char *s, size_t max, const char *fmt, ...);
int our_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);
#define snprintf our_snprintf
#define vsnprintf our_vsnprintf

/* to avoid unsetenv problems we have our own unsetenv */
#define unsetenv our_unsetenv
int our_unsetenv(const char *name);


/* new segment support
 * name is a string
 * wx should contain one of the strings "w", "wx", "x", or ""
 */
/* FIXME: also want control over where in rw region or ro region this
 * section goes -- for cl, order linked seems to do it, but for linux 
 * will need a linker script (see linux/os.c for the nspdata problem)
 */
#define DECLARE_DATA_SECTION(name, wx) \
     asm(".section "name", \"a"wx"\", @progbits"); \
     asm(".align 0x1000");

#define END_DATA_SECTION_DECLARATIONS() \
     asm(".section .data"); \
     asm(".align 0x1000");

/* the VAR_IN_SECTION macro change where each var goes */
#define START_DATA_SECTION(name, wx) /* nothing */
#define END_DATA_SECTION() /* nothing */

/* Any assignment, even to 0, puts vars in current .data and not .bss for cl,
 * but for gcc we need to explicitly declare which section.  We still need
 * the .section asm above to give section attributes and alignment.
 */
#define VAR_IN_SECTION(name) __attribute__ ((section (name)))

/* location of vsyscall "vdso" page */
extern app_pc vsyscall_page_start;
/* pc of the end of the syscall instr itself */
extern app_pc vsyscall_syscall_end_pc;
/* pc where kernel returns control after sysenter vsyscall */
extern app_pc vsyscall_sysenter_return_pc;
#define VSYSCALL_PAGE_MAPS_NAME "[vdso]"

/* We need a place to store the continuation pc for the child thread.
 * We pick a register not used for SYS_clone parameters.
 * FIXME PR 286194: currently we do not restore this in the child at all: better to
 * keep a stack in parent, or use CLONE_SETTLS (PR 285898)?
 */
#define CLONE_SCRATCH_REG_MC  xbp
#define CLONE_SCRATCH_REG_REG REG_XBP

bool is_clone_thread_syscall(dcontext_t *dcontext);
bool was_clone_thread_syscall(dcontext_t *dcontext);
bool is_sigreturn_syscall(dcontext_t *dcontext);
bool was_sigreturn_syscall(dcontext_t *dcontext);
bool ignorable_system_call(int num);

bool kernel_is_64bit(void);

/* in arch.c */
bool unhook_vsyscall(void);

/***************************************************************************/
/* in signal.c */

/* defines and typedefs exported for dr_jmp_buf_t */

#define NUM_NONRT   32 /* include 0 to make offsets simple */
#define NUM_RT      32
#define OFFS_RT     32
/* FIXME: PR 362835: actually 64, not 63, is the highest valid signum */
#define MAX_SIGNUM  ((OFFS_RT) + (NUM_RT))

/* size of long */
#ifdef X64
# define _NSIG_BPW 64
#else
# define _NSIG_BPW 32
#endif

#define _NSIG_WORDS (MAX_SIGNUM / _NSIG_BPW)

/* kernel's sigset_t packs info into bits, while glibc's uses a short for
 * each (-> 8 bytes vs. 128 bytes)
 */
typedef struct _kernel_sigset_t {
    unsigned long sig[_NSIG_WORDS];
} kernel_sigset_t;

void receive_pending_signal(dcontext_t *dcontext);
void start_itimer(dcontext_t *dcontext);
void stop_itimer(void);
void start_PAPI_timer(void);
void stop_PAPI_timer(void);
bool is_signal_restorer_code(byte *pc, size_t *len);

#define CONTEXT_HEAP_SIZE(sc) (sizeof(sc))
#define CONTEXT_HEAP_SIZE_OPAQUE (CONTEXT_HEAP_SIZE(struct sigcontext))

/* struct sigcontext field name changes */
#ifdef X64
# define SC_XIP rip
# define SC_XAX rax
# define SC_XCX rcx
# define SC_XDX rdx
# define SC_XBX rbx
# define SC_XSP rsp
# define SC_XBP rbp
# define SC_XSI rsi
# define SC_XDI rdi
# define SC_XFLAGS eflags
#else
# define SC_XIP eip
# define SC_XAX eax
# define SC_XCX ecx
# define SC_XDX edx
# define SC_XBX ebx
# define SC_XSP esp
# define SC_XBP ebp
# define SC_XSI esi
# define SC_XDI edi
# define SC_XFLAGS eflags
#endif

void *create_clone_record(dcontext_t *dcontext, app_pc continuation_pc);
app_pc signal_thread_inherit(dcontext_t *dcontext, void *clone_record);

/***************************************************************************/

/* in pcprofile.c */
void pcprofile_fragment_deleted(dcontext_t *dcontext, fragment_t *f);
void pcprofile_thread_exit(dcontext_t *dcontext);

/* in stackdump.c */
/* fork, dump core, and use gdb for complete stack trace */
void stackdump(void);
/* use backtrace feature of glibc for quick but sometimes incomplete trace */
void glibc_stackdump(int fd);

#endif /* _OS_EXPORTS_H_ */
